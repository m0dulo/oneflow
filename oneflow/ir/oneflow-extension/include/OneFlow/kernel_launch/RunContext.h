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
#ifndef ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_KERNEL_LAUNCH_RUNCONTEXT_H_
#define ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_KERNEL_LAUNCH_RUNCONTEXT_H_

#include "mlir/IR/BuiltinAttributes.h"
#include "oneflow/ir/oneflow-extension/include/OneFlow/kernel_launch/RegContext.h"
#include "OneFlow/OKL/OKLOps.h"

namespace oneflow {
namespace okl {
class RunContext final : public user_op::KernelComputeContext {
 public:
  explicit RunContext(std::shared_ptr<RegContext> reg, user_op::KernelComputeContext* comp);
  ~RunContext() = default;

  const user_op::TensorDesc* TensorDesc4ArgNameAndIndex(const std::string& arg_name,
                                                        int32_t index) const override;
  user_op::Tensor* Tensor4ArgNameAndIndex(const std::string& arg_name, int32_t index) override;

  ep::Stream* stream() override;

  DeviceType device_type() const override;
  const ParallelContext& parallel_ctx() const override;

  const ArgVec& inputs() const override;
  const ArgVec& outputs() const override;

  const user_op::UserOpConfWrapper& user_op_conf() const override;

 private:
  const std::shared_ptr<const user_op::AttrVal>& Attr4Name(
      const std::string& attr_name) const override;
  std::shared_ptr<RegContext> reg_ctx_;
  KernelComputeContext* comp_ctx_ = nullptr;
  std::unordered_map<mlir::oneflow::user_op::ArgID, user_op::Tensor*> tensor_desc_{};
};
}  // namespace okl
}  // namespace oneflow

#endif  // ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_KERNEL_LAUNCH_RUNCONTEXT_H_
