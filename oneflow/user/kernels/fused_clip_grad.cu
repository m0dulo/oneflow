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
#include "oneflow/core/ep/cuda/cuda_stream.h"
#include "oneflow/user/kernels/fused_clip_grad_util.h"

namespace oneflow {

namespace {

template<typename T>
__global__ void MultiBlockScaleMulGpu(MultiScaleMulParamsPack<T> pack_params, T* scale) {
  T t = *scale;
  for (int i = 0; i < pack_params.size; ++i) {
    auto& param = pack_params.params[i];
    CUDA_1D_KERNEL_LOOP(j, param.size) {
      param.data[j] *= t;
    }
  }
}

}  // namespace

template<typename T>
struct MultiScaleMul<DeviceType::kCUDA, T> {
  void operator()(ep::Stream* stream, std::vector<MultiScaleMulParam<T>>& params, T* scale) {
    int32_t total_num_blocks = 0;
    for (size_t i = 0; i < params.size(); i += kMultiReduceScaleMulPackSize) {
      MultiScaleMulParamsPack<T> pack_params{};
      size_t max_elem_cnt = 0;
      pack_params.size = std::min<size_t>(kMultiReduceScaleMulPackSize, params.size() - i);
      for (size_t j = 0; j < pack_params.size; ++j) {
        pack_params.params[j] = params[i + j];
        max_elem_cnt = std::max<size_t>(max_elem_cnt, pack_params.params[j].size);
      }
      int32_t num_blocks = BlocksNum4ThreadsNum(max_elem_cnt);
      MultiBlockScaleMulGpu<T><<<num_blocks, kCudaThreadsNumPerBlock, 0, stream->As<ep::CudaStream>()->cuda_stream()>>>(
          pack_params, scale);
      total_num_blocks += num_blocks;
    }
  }
};

template struct MultiScaleMul<DeviceType::kCUDA, float>;

}  // namespace oneflow