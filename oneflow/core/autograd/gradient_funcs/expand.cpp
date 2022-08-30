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
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/functional/functional.h"

namespace oneflow {
namespace one {

struct ExpandCaptureState : public AutoGradCaptureState {
  bool requires_grad;
  std::vector<int32_t> reduce_dims;
  int32_t squeeze;
};

class Expand : public OpExprGradFunction<ExpandCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override;
  Maybe<void> Capture(ExpandCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override;
  Maybe<void> Apply(const ExpandCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override;
};

Maybe<void> Expand::Init(const OpExpr& op) {
  const UserOpExpr* fw_op_expr = dynamic_cast<const UserOpExpr*>(&op);
  CHECK_NOTNULL_OR_RETURN(fw_op_expr);  // NOLINT(maybe-need-error-msg)
  return Maybe<void>::Ok();
}

Maybe<void> Expand::Capture(ExpandCaptureState* ctx, const TensorTuple& inputs,
                            const TensorTuple& outputs, const AttrMap& attrs) const {
  CHECK_EQ_OR_RETURN(inputs.size(), 1);   // NOLINT(maybe-need-error-msg)
  CHECK_EQ_OR_RETURN(outputs.size(), 1);  // NOLINT(maybe-need-error-msg)
  ctx->requires_grad = inputs[0]->requires_grad();
  if (!ctx->requires_grad) { return Maybe<void>::Ok(); }

  const Shape& in_shape = *inputs[0]->shape();
  const Shape& expand_shape = *outputs[0]->shape();
  ctx->squeeze = expand_shape.size() - in_shape.size();
  ctx->reduce_dims.reserve(expand_shape.size());
  for (size_t i = 0; i < expand_shape.size(); ++i) {
    const auto& t_dim = expand_shape[i];
    if (i >= ctx->squeeze) {
      const auto& dim = in_shape[i - ctx->squeeze];
      if (t_dim != dim) { ctx->reduce_dims.push_back(i); }
    } else {
      ctx->reduce_dims.push_back(i);
    }
  }

  return Maybe<void>::Ok();
}

Maybe<void> Expand::Apply(const ExpandCaptureState* ctx, const TensorTuple& out_grads,
                          TensorTuple* in_grads) const {
  if (!ctx->requires_grad) { return Maybe<void>::Ok(); }
  CHECK_EQ_OR_RETURN(out_grads.size(), 1);  // NOLINT(maybe-need-error-msg)
  in_grads->resize(1);
  auto reduced = JUST(functional::ReduceSum(out_grads[0], ctx->reduce_dims, true));
  if (ctx->squeeze > 0) {
    in_grads->at(0) = JUST(functional::Flatten(reduced, 0, ctx->squeeze));
  } else {
    in_grads->at(0) = reduced;
  }
  return Maybe<void>::Ok();
}

REGISTER_OP_EXPR_GRAD_FUNCTION("expand", Expand);

}  // namespace one
}  // namespace oneflow
