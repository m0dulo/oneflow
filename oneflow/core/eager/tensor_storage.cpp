#include "oneflow/core/eager/tensor_storage.h"
#include "oneflow/core/common/env_var/remat.h"
#include "oneflow/core/vm/op_call_instruction_policy.h"
#include "oneflow/core/vm/remat/disjoint_set.h"
#include "oneflow/core/vm/remat/env.h"
#include "oneflow/core/vm/remat/util.h"
#include "oneflow/core/vm/virtual_machine.h"

namespace oneflow {
namespace vm {
namespace {
int64_t unique_id() {
  static size_t id = 0;
  return id++;
}

}  // namespace

TensorStorage::TensorStorage(bool is_allocated_in_vm, Symbol<Device> device)
    : blob_bytes_(0),
      device_(device),
      non_pod_allocator_(std::make_unique<MemoryAllocator>()),
      producer_stream_(NullOpt),
      last_used_stream_(NullOpt),
      is_allocated_in_vm_(is_allocated_in_vm) {}

Symbol<Device> TensorStorage::device() const { return device_; }

void TensorStorage::_Release() {
  for (const auto& hook : storage_delete_hooks_) { hook(); }
  non_pod_allocator_.reset();
  blob_dptr_.reset();
}

void TensorStorage::Release() { return _Release(); }

Maybe<void> TensorStorage::init_producer_stream(Symbol<::oneflow::Stream> producer_stream) {
  CHECK_OR_RETURN(!producer_stream_.has_value());
  producer_stream_ = producer_stream;
  return Maybe<void>::Ok();
}

RematableTensorStorage::RematableTensorStorage(Symbol<Device> device)
    : TensorStorage(true, device),
      node(std::make_shared<remat::DisjNode>(0)),
      id_(unique_id()),
      num_pinned_(0),
      last_access_time_(0),
      compute_time_(0) {
  VLOG(1) << "create rematable storage " << id_;
}

RematableTensorStorage::~RematableTensorStorage() {
  // We must call _Release before destruction or the release will be
  // called in base class's destructor and causes segfault.
  // Time order:
  // 1. ~RematableTensorStorage destructs its members
  // 2. ~TensorStorage, Allocator::Deallocate, which uses RematableTensorStorage members
  _Release();
  if (compute_op_) { Singleton<remat::Env>::Get()->remove_compute_op(compute_op_.get()); }
  VLOG(1) << "delete storage " << id_;
}

void RematableTensorStorage::LogEviction(bool eager_eviction) const {
  Singleton<remat::Env>::Get()->add_eviction_num(eager_eviction);
  VLOG(1) << "evict storage " << id_ << ", compute op type: " << compute_op_type_name()
          << ", eager_eviction: " << eager_eviction;
}

void RematableTensorStorage::Remat() {
  if (is_in_memory()) { return; }
  auto stream = CHECK_JUST(GetDefaultStreamByDevice(device_));
  auto* vm_stream = CHECK_JUST(Singleton<VirtualMachine>::Get()->GetVmStream(stream));
  auto op = compute_op();
  CHECK_JUST(Recompute(&op, vm_stream));
}

void RematableTensorStorage::Evict(bool eager_eviction) {
  CHECK(!is_eviction_disabled());
  LogEviction(eager_eviction);
  return _Release();
}

void RematableTensorStorage::Release() {
  CHECK(device_->rematable());
  if (is_eviction_disabled()) { return; }
  return Evict(true);
}

std::vector<std::string> random_ops{"uniform", "uniform_int", "normal", "randperm"};

bool RematableTensorStorage::is_evictable() const {
  return compute_op_ != nullptr
         && std::find(random_ops.begin(), random_ops.end(), compute_op_type_name())
                == random_ops.end()
         && !eviction_disabled_;
}

OpCallInstructionPolicy RematableTensorStorage::compute_op() const {
  CHECK_NOTNULL(compute_op_);
  return OpCallInstructionPolicy(*compute_op_);
}

std::shared_ptr<DtrOpCallInstructionPolicy> RematableTensorStorage::dtr_compute_op() const {
  return compute_op_;
}

void RematableTensorStorage::Pin() {
  ++num_pinned_;
  VLOG(3) << "pin storage " << id_ << ", num_pinned: " << num_pinned_;
}

void RematableTensorStorage::Unpin() {
  CHECK_GT(num_pinned_, 0);
  --num_pinned_;
  VLOG(3) << "unpin storage " << id_ << ", num_pinned: " << num_pinned_;
}

void RematableTensorStorage::clear_compute_op() {
  if (compute_op_ == nullptr) { return; }
  VLOG(1) << "clear_compute_op: " << id_;
  Singleton<remat::Env>::Get()->remove_compute_op(compute_op_.get());
  compute_op_ = nullptr;
  compute_time_ = -1;
}

void RematableTensorStorage::set_compute_op(
    const std::shared_ptr<DtrOpCallInstructionPolicy>& compute_op, double compute_time) {
  CHECK_ISNULL(compute_op_);
  compute_op_ = compute_op;
  VLOG(1) << "set_compute_op: " << id_ << ", compute op: " << compute_op.get();
  Singleton<remat::Env>::Get()->ops.push_back(CHECK_NOTNULL(compute_op_.get()));
  compute_time_ = compute_time;
}

std::string RematableTensorStorage::compute_op_type_name() const {
  if (is_eviction_disabled()) { return "eviction_disabled"; }
  if (compute_op_) { return compute_op_->opkernel().op_type_name(); }
  return "None";
}

void RematableTensorStorage::Access() {
  last_access_time_ = Singleton<remat::Env>::Get()->time_now();
}

Maybe<double> RematableTensorStorage::cost(size_t override_size) const {
  const double time_since_last_access = Singleton<remat::Env>::Get()->time_now() - last_access_time_;
  size_t size = 1;
  if (EnvBool<ONEFLOW_REMAT_HEURISTIC_DTE>() || EnvBool<ONEFLOW_REMAT_HEURISTIC_DTR>()) {
    size = override_size == 0 ? blob_bytes_ : override_size;
  }
  return (EnvBool<ONEFLOW_REMAT_NEIGHBOR>() ? approx_neighbor_cost() : compute_time_)
         / time_since_last_access / static_cast<double>(size);
}

double RematableTensorStorage::approx_neighbor_cost() const {
  double cost = 0;
  auto compute_op = this->compute_op();
  const auto& inputs = compute_op.inputs();
  for (int i = 0; i < inputs.size(); ++i) {
    const auto& tmp = inputs[i];
    if (auto storage = std::dynamic_pointer_cast<RematableTensorStorage>(tmp->tensor_storage());
        !storage->is_in_memory()) {
      double p_cost = remat::DisjointSet::find_father(storage->node)->compute_time();
      if (p_cost < storage->compute_time()) { p_cost = storage->compute_time(); }
      cost += p_cost;
    }
  }

  const auto& outputs = compute_op.outputs();
  for (int i = 0; i < outputs.size(); ++i) {
    const auto& tmp = outputs[i];
    if (auto storage = std::dynamic_pointer_cast<RematableTensorStorage>(tmp->tensor_storage());
        !storage->is_in_memory()) {
      double c_cost = remat::DisjointSet::find_father(storage->node)->compute_time();
      if (c_cost < storage->compute_time()) { c_cost = storage->compute_time(); }
      cost += c_cost;
    }
  }

  return cost + compute_time_;
}

}  // namespace vm
}  // namespace oneflow
