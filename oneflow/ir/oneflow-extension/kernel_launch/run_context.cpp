/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "mlir/IR/BuiltinAttributes.h"
#include "oneflow/ir/oneflow-extension/include/OneFlow/kernel_launch/RegContext.h"
#include "oneflow/ir/oneflow-extension/include/OneFlow/kernel_launch/RunContext.h"
#include "OneFlow/OKL/OKLOps.h"

namespace oneflow {
namespace okl {

RunContext::RunContext(std::shared_ptr<RegContext> reg, user_op::KernelComputeContext* comp)
    : reg_ctx_(std::move(reg)), comp_ctx_(comp) {}

const user_op::TensorDesc* RunContext::TensorDesc4ArgNameAndIndex(const std::string& arg_name,
                                                                  int32_t index) const {
  return reg_ctx_->TensorDesc4ArgNameAndIndex(arg_name, index);
}

user_op::Tensor* RunContext::Tensor4ArgNameAndIndex(const std::string& arg_name, int32_t index) {
  auto op = reg_ctx_->GetOp();
  if (arg_name == "x") {
    auto val = op->getOperand(index);
    auto define_op = val.getDefiningOp();
    auto index = define_op->getAttr("index").cast<mlir::IntegerAttr>().getInt();
    if (define_op->getName().getStringRef() == mlir::okl::GetTensorFromArgOp::getOperationName()) {
      return comp_ctx_->Tensor4ArgNameAndIndex("in", index);
    } else if (define_op->getName().getStringRef()
               == mlir::okl::GetTensorFromRetOp::getOperationName()) {
      return comp_ctx_->Tensor4ArgNameAndIndex("out", index);
    } else {
      LOG(FATAL) << "Signature Not supported";
      exit(1);
    }
  } else if (arg_name == "y") {
    auto val = op->getResult(index);
    for (auto use : val.getUsers()) {
      if (use->getName().getStringRef() == mlir::okl::GetTensorAsRetOp::getOperationName()) {
        auto index = use->getAttr("index").cast<mlir::IntegerAttr>().getInt();
        return comp_ctx_->Tensor4ArgNameAndIndex("out", index);
      }
    }
  } else {
    LOG(FATAL) << "Signature Not supported";
    exit(1);
  }
}

ep::Stream* RunContext::stream() { return comp_ctx_->stream(); }

DeviceType RunContext::device_type() const { return reg_ctx_->device_type(); }
const ParallelContext& RunContext::parallel_ctx() const { return comp_ctx_->parallel_ctx(); }

const ArgVec& RunContext::inputs() const { return reg_ctx_->inputs(); }
const ArgVec& RunContext::outputs() const { return reg_ctx_->outputs(); }

const user_op::UserOpConfWrapper& RunContext::user_op_conf() const {
  return reg_ctx_->user_op_conf();
}

const std::shared_ptr<const user_op::AttrVal>& RunContext::Attr4Name(
    const std::string& attr_name) const {
  return user_op_conf().Attr4Name(attr_name);
}

}  // namespace okl
}  // namespace oneflow