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
#include "oneflow/core/framework/op_expr_grad_function.h"
#include "oneflow/core/functional/functional.h"

namespace oneflow {
namespace one {

struct FusedGeluMulGradCaptureState : public AutoGradCaptureState {
  bool in_requires_grad = true;
  bool multiplier_requires_grad = true;
  std::string approximate = "tanh";
};

class FusedGeluMulGrad : public OpExprGradFunction<FusedGeluMulGradCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override {
    const auto* fw_op_expr = dynamic_cast<const UserOpExpr*>(&op);
    CHECK_NOTNULL_OR_RETURN(fw_op_expr);
    base_attrs_ = MakeAttrMapFromUserOpConf(fw_op_expr->proto());
    return Maybe<void>::Ok();
  }

  Maybe<void> Capture(FusedGeluMulGradCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override {
    CHECK_EQ_OR_RETURN(inputs.size(), 2);   // (in, multiplier)
    CHECK_EQ_OR_RETURN(outputs.size(), 1);  // (out,)
    ctx->in_requires_grad = inputs.at(0)->requires_grad();
    ctx->multiplier_requires_grad = inputs.at(1)->requires_grad();

    if (ctx->in_requires_grad || ctx->multiplier_requires_grad) {
      ctx->SaveTensorForBackward(inputs.at(0));  // in
      ctx->SaveTensorForBackward(inputs.at(1));  // multiplier
    }

    if (ctx->multiplier_requires_grad) {
      ctx->SaveTensorForBackward(outputs.at(0));  // out
    }

    ComposedAttrMap composed_attrs(attrs, base_attrs_);
    ctx->approximate = JUST(composed_attrs.GetAttr<std::string>("approximate"));
    return Maybe<void>::Ok();
  }

  Maybe<void> Apply(const FusedGeluMulGradCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override {
    if (!ctx->in_requires_grad && !ctx->multiplier_requires_grad) { return Maybe<void>::Ok(); }

    const auto& saved_tensors = ctx->SavedTensors();
    CHECK_EQ_OR_RETURN(out_grads.size(), 1);
    CHECK_GE_OR_RETURN(saved_tensors.size(), 2);

    const auto& out_diff = out_grads.at(0);
    // const int64_t num_axes = out_diff->shape()->NumAxes();

    in_grads->resize(2);  // (in_diff, multiplier_diff)
    const auto& in = saved_tensors.at(0);
    const auto& multiplier = saved_tensors.at(1);

    if (ctx->in_requires_grad) {
      in_grads->at(0) =
          JUST(functional::FusedGeluMulGrad(out_diff, in, multiplier, ctx->approximate));
    }

    if (ctx->multiplier_requires_grad) {
      CHECK_EQ_OR_RETURN(saved_tensors.size(), 3);
      const auto& out = saved_tensors.at(2);
      in_grads->at(1) = JUST(functional::Mul(out_diff, out));
    }

    return Maybe<void>::Ok();
  }

 private:
  AttrMap base_attrs_;
};

REGISTER_OP_EXPR_GRAD_FUNCTION("fused_gelu_mul", FusedGeluMulGrad);

}  // namespace one
}  // namespace oneflow
