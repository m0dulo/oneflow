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
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/device/nccl_util.h"
#include "oneflow/core/job/eager_nccl_comm_manager.h"
#include "oneflow/core/job/parallel_desc.h"
#include "oneflow/core/ep/cuda/cuda_stream.h"
#include "oneflow/user/kernels/gather_kernel_util.h"
#include "oneflow/user/kernels/unsorted_segment_sum_kernel_util.h"
#include "oneflow/core/cuda/atomic.cuh"
#include "oneflow/core/embedding/embedding_manager.h"
#include "oneflow/core/control/ctrl_client.h"
#include "oneflow/core/ep/include/primitive/memcpy.h"
#include "absl/strings/str_cat.h"

namespace oneflow {

namespace {

void DumpToFile(ep::Stream* stream, std::string filename, int64_t parallel_id, size_t data_size,
                const void* ptr) {
  void* host_ptr;
  OF_CUDA_CHECK(cudaMallocHost(&host_ptr, data_size));
  std::unique_ptr<ep::primitive::Memcpy> copyd2h_primitive =
      ep::primitive::NewPrimitive<ep::primitive::MemcpyFactory>(DeviceType::kCUDA,
                                                                ep::primitive::MemcpyKind::kDtoH);
  CHECK(copyd2h_primitive);
  copyd2h_primitive->Launch(stream, host_ptr, ptr, data_size);
  CHECK_JUST(stream->Sync());
  std::ofstream dx_os;
  dx_os.open(StrCat("test/" + filename + "_", parallel_id));
  dx_os.write(reinterpret_cast<char*>(host_ptr), data_size);
  dx_os.close();
  OF_CUDA_CHECK(cudaFreeHost(host_ptr));
}

template<typename T, int pack_size>
struct alignas(sizeof(T) * pack_size) Pack {
  T elem[pack_size];
};

template<typename T, typename IDX, int pack_size, int N>
struct Param {
  IDX* inverse_indices[N];
  const Pack<T, pack_size>* unique_embeddings[N];
  int32_t* is_kernel_start[N];
  const IDX* num_unique_matrix;
  Pack<T, pack_size>* embedding_ptr;
};

template<typename T, typename IDX, int pack_size, int N>
__global__ void EmbeddingShuffleCudaKernel(int64_t parallel_id, int64_t parallel_num,
                                           int64_t embedding_num_pack,
                                           Param<T, IDX, pack_size, N> param) {
#pragma unroll 1
  for (int i = 0; i < parallel_num; ++i) {
    int rank_id = (parallel_id + i) % parallel_num;
    IDX out_index_offset = 0;
    for (int k = 0; k < rank_id; ++k) {
      out_index_offset += param.num_unique_matrix[parallel_id * parallel_num + k];
    }
    IDX in_index_offset = 0;
    for (int k = 0; k < parallel_id; ++k) {
      in_index_offset += param.num_unique_matrix[k * parallel_num + rank_id];
    }
    const IDX* inverse_indices_ptr = param.inverse_indices[rank_id] + in_index_offset;
    const Pack<T, pack_size>* unique_embeddings_ptr = param.unique_embeddings[rank_id];
    Pack<T, pack_size>* embedding_ptr = param.embedding_ptr + out_index_offset * embedding_num_pack;
    CUDA_1D_KERNEL_LOOP_T(
        int, j,
        param.num_unique_matrix[parallel_id * parallel_num + rank_id] * embedding_num_pack) {
      int out_row_id = j / embedding_num_pack;
      int in_row_id = inverse_indices_ptr[out_row_id];
      int col_id = j - out_row_id * embedding_num_pack;
      embedding_ptr[j] = unique_embeddings_ptr[in_row_id * embedding_num_pack + col_id];
    }
  }
}

template<typename T, typename IDX, int pack_size, int N>
__global__ void BarrierKernel(int32_t parallel_id, int32_t parallel_num,
                              Param<T, IDX, pack_size, N> param) {
  int count = *param.is_kernel_start[parallel_id];
  volatile int32_t* start_f = param.is_kernel_start[parallel_id];
  *start_f = count + 1;
  // printf("\nparallel_id %d set to %d\n", parallel_id, *start_f);
  for (int k = 0; k < parallel_num; ++k) {
    volatile int32_t* is_kernel_start_ptr = param.is_kernel_start[k];
    while (*is_kernel_start_ptr < count + 1)
      ;
  }
}

struct IpcMemHandleOffset {
  cudaIpcMemHandle_t handle;
  int64_t offset;
};

void GetPtrs(user_op::KernelComputeContext* ctx, std::vector<void*>* unique_embeddings_ptr,
             std::vector<void*>* inverse_indices_ptr, std::vector<void*>* is_kernel_start_ptr) {
  const int64_t num_ids =
      ctx->TensorDesc4ArgNameAndIndex("inverse_unique_partition_indices", 0)->shape().elem_cnt();
  const int64_t parallel_id = ctx->parallel_ctx().parallel_id();
  const int64_t parallel_num = ctx->parallel_ctx().parallel_num();
  unique_embeddings_ptr->at(parallel_id) =
      const_cast<void*>(ctx->Tensor4ArgNameAndIndex("cur_rank_embeddings", 0)->dptr());
  inverse_indices_ptr->at(parallel_id) =
      const_cast<void*>(ctx->Tensor4ArgNameAndIndex("cur_rank_inverse_indices", 0)->dptr());
  std::string name =
      ctx->op_name()
      + std::to_string(num_ids);  // train and eval same op name. do it in pass? or use newUniqueId
  {
    std::vector<IpcMemHandleOffset> push_handle_offset;
    push_handle_offset.resize(3);
    OF_CUDA_CHECK(cudaIpcGetMemHandle(&push_handle_offset.at(0).handle,
                                      unique_embeddings_ptr->at(parallel_id)));
    OF_CUDA_CHECK(cudaIpcGetMemHandle(&push_handle_offset.at(1).handle,
                                      inverse_indices_ptr->at(parallel_id)));
    OF_CUDA_CHECK(cudaIpcGetMemHandle(&push_handle_offset.at(2).handle,
                                      is_kernel_start_ptr->at(parallel_id)));

    cudaError_t (*func)(void*, CUpointer_attribute, CUdeviceptr);
    cudaGetDriverEntryPoint("cuPointerGetAttribute", (void**)(&func), cudaEnableDefault);
    void* unique_embeddings_base;
    func(&unique_embeddings_base, CU_POINTER_ATTRIBUTE_RANGE_START_ADDR,
         (CUdeviceptr)(unique_embeddings_ptr->at(parallel_id)));
    push_handle_offset.at(0).offset =
        reinterpret_cast<char*>(unique_embeddings_ptr->at(parallel_id))
        - reinterpret_cast<char*>(unique_embeddings_base);
    void* inverse_indices_base;
    func(&inverse_indices_base, CU_POINTER_ATTRIBUTE_RANGE_START_ADDR,
         (CUdeviceptr)(inverse_indices_ptr->at(parallel_id)));
    push_handle_offset.at(1).offset = reinterpret_cast<char*>(inverse_indices_ptr->at(parallel_id))
                                      - reinterpret_cast<char*>(inverse_indices_base);
    push_handle_offset.at(2).offset = 0;
    Singleton<CtrlClient>::Get()->PushKV(
        name + std::to_string(parallel_id),
        std::string(reinterpret_cast<const char*>(push_handle_offset.data()),
                    3 * sizeof(IpcMemHandleOffset)));
  }
  for (int64_t i = 0; i < parallel_num; ++i) {
    std::string key = name + std::to_string(i);
    if (parallel_id != i) {
      std::vector<IpcMemHandleOffset> handle_offset;
      handle_offset.resize(3);
      Singleton<CtrlClient>::Get()->PullKV(key, [i, &handle_offset](const std::string& val) {
        memcpy(handle_offset.data(), val.data(), 3 * sizeof(IpcMemHandleOffset));
      });
      OF_CUDA_CHECK(cudaIpcOpenMemHandle(&unique_embeddings_ptr->at(i), handle_offset.at(0).handle,
                                         cudaIpcMemLazyEnablePeerAccess));
      unique_embeddings_ptr->at(i) =
          reinterpret_cast<char*>(unique_embeddings_ptr->at(i)) + handle_offset.at(0).offset;

      OF_CUDA_CHECK(cudaIpcOpenMemHandle(&inverse_indices_ptr->at(i), handle_offset.at(1).handle,
                                         cudaIpcMemLazyEnablePeerAccess));
      inverse_indices_ptr->at(i) =
          reinterpret_cast<char*>(inverse_indices_ptr->at(i)) + handle_offset.at(1).offset;

      OF_CUDA_CHECK(cudaIpcOpenMemHandle(&is_kernel_start_ptr->at(i), handle_offset.at(2).handle,
                                         cudaIpcMemLazyEnablePeerAccess));
      is_kernel_start_ptr->at(i) =
          reinterpret_cast<char*>(is_kernel_start_ptr->at(i)) + handle_offset.at(2).offset;
    }
  }
}

template<typename IDX>
class DataShuffleKernelState final : public user_op::OpKernelState {
 public:
  explicit DataShuffleKernelState(user_op::KernelInitContext* ctx)
      : device_index_(-1), parallel_desc_(ctx->parallel_desc()) {
    const int64_t parallel_id = ctx->parallel_ctx().parallel_id();
    int64_t parallel_num = parallel_desc_.parallel_num();
    unique_embeddings_ptr_.resize(parallel_num);
    inverse_indices_ptr_.resize(parallel_num);
    is_kernel_start_ptr_.resize(parallel_num);
    size_t is_kernel_start_size = GetCudaAlignedSize(sizeof(int32_t));
    OF_CUDA_CHECK(cudaMalloc(&is_kernel_start_ptr_.at(parallel_id), is_kernel_start_size));
    OF_CUDA_CHECK(cudaMemset(is_kernel_start_ptr_.at(parallel_id), 0, is_kernel_start_size));
  }

  ~DataShuffleKernelState() {
    // free
  }

  std::vector<void*>* UniqueEmbeddings() { return &unique_embeddings_ptr_; }

  std::vector<void*>* InverseIndices() { return &inverse_indices_ptr_; }

  std::vector<void*>* IsKernelStart() { return &is_kernel_start_ptr_; }

 private:
  int device_index_;
  ParallelDesc parallel_desc_;
  std::vector<void*> unique_embeddings_ptr_;
  std::vector<void*> inverse_indices_ptr_;
  std::vector<void*> is_kernel_start_ptr_;
};

constexpr int pack_size = 4;

}  // namespace

template<typename T, typename IDX>
class EmbeddingShuffleP2PKernel final : public user_op::OpKernel {
 public:
  EmbeddingShuffleP2PKernel() : current_iter_(0) {}
  ~EmbeddingShuffleP2PKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<DataShuffleKernelState<IDX>>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state,
               const user_op::OpKernelCache*) const override {
    CHECK(!embedding::UseDynamicMemoryAllocation());
    CHECK(ParseBooleanFromEnv("ONEFLOW_ONE_EMBEDDING_FUSE_EMBEDDING_INTERACTION",
                              false));  // only support skip last gather.
    CHECK(ParseBooleanFromEnv("ADD_IDENTITY",
                              false));  // when no identity, every time the cur_rank_inverse_indices
                                        // will change becauseof regster num=2.
    auto* kernel_state = dynamic_cast<DataShuffleKernelState<IDX>*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* num_unique_matrix = ctx->Tensor4ArgNameAndIndex("num_unique_matrix", 0);
    const user_op::Tensor* cur_rank_inverse_indices =
        ctx->Tensor4ArgNameAndIndex("cur_rank_inverse_indices", 0);
    const user_op::Tensor* inverse_unique_partition_indices =
        ctx->Tensor4ArgNameAndIndex("inverse_unique_partition_indices", 0);
    user_op::Tensor* embeddings = ctx->Tensor4ArgNameAndIndex("embeddings", 0);
    const int64_t embedding_size = ctx->Attr<int64_t>("embedding_size");
    DataType data_type = embeddings->data_type();
    const int64_t parallel_num = ctx->parallel_ctx().parallel_num();
    const int64_t parallel_id = ctx->parallel_ctx().parallel_id();
    const bool skip_last_gather = ctx->Attr<bool>("skip_last_gather");
    CHECK(skip_last_gather);
    cudaStream_t cuda_stream = ctx->stream()->As<ep::CudaStream>()->cuda_stream();
    if (current_iter_ == 0) {
      GetPtrs(ctx, kernel_state->UniqueEmbeddings(), kernel_state->InverseIndices(),
              kernel_state->IsKernelStart());
    }
    CHECK_EQ(kernel_state->UniqueEmbeddings()->at(parallel_id),
             ctx->Tensor4ArgNameAndIndex("cur_rank_embeddings", 0)->dptr())
        << parallel_id;
    CHECK_EQ(kernel_state->InverseIndices()->at(parallel_id), cur_rank_inverse_indices->dptr())
        << parallel_id;
    Param<T, IDX, pack_size, 8> param;
    CHECK_LE(parallel_num, 8);
    param.embedding_ptr = reinterpret_cast<Pack<T, pack_size>*>(embeddings->mut_dptr<T>());
    for (int i = 0; i < parallel_num; ++i) {
      param.inverse_indices[i] = reinterpret_cast<IDX*>(kernel_state->InverseIndices()->at(i));
      param.unique_embeddings[i] =
          reinterpret_cast<Pack<T, pack_size>*>(kernel_state->UniqueEmbeddings()->at(i));
      param.is_kernel_start[i] = reinterpret_cast<int32_t*>(kernel_state->IsKernelStart()->at(i));
    }
    param.num_unique_matrix = reinterpret_cast<const uint32_t*>(num_unique_matrix->dptr());
    int64_t embedding_num_pack = embedding_size / pack_size;

    BarrierKernel<<<1, 1, 0, cuda_stream>>>(parallel_id, parallel_num, param);
    EmbeddingShuffleCudaKernel<<<216, kCudaThreadsNumPerBlock, 0, cuda_stream>>>(
        parallel_id, parallel_num, embedding_num_pack, param);
    BarrierKernel<<<1, 1, 0, cuda_stream>>>(parallel_id, parallel_num, param);  // if in eval,should add last barrier.

    current_iter_++;
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
  mutable int64_t current_iter_;
};

REGISTER_USER_KERNEL("embedding_shuffle")
    .SetCreateFn<EmbeddingShuffleP2PKernel<half, uint32_t>>()
    .SetIsMatchedHob((user_op::HobDeviceType() == DeviceType::kCUDA)
                     && (user_op::HobDataType("cur_rank_embeddings", 0) == DataType::kFloat16)
                     && (user_op::HobDataType("num_unique_matrix", 0) == DataType::kUInt32)
                     && ParseBooleanFromEnv("EMBEDDING_SHUFFLE_USE_P2P_KERNEL", false));

}  // namespace oneflow
