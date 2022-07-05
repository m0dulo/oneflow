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
#include "oneflow/core/job_rewriter/job_pass.h"
#include "oneflow/core/framework/framework.h"

namespace oneflow {

namespace {

void UpdateConsumerOpConf(const OpNode* consumer, const LogicalBlobId& out,
                          const std::string& new_out_lbn,
                          HashMap<std::string, OperatorConf>* op_name2op_conf) {
  const std::string& consumer_op_name = consumer->op().op_name();
  if (op_name2op_conf->find(consumer_op_name) == op_name2op_conf->end()) {
    (*op_name2op_conf)[consumer_op_name] = consumer->op().op_conf();
  }
  for (const std::string& ibn : consumer->op().input_bns()) {
    if (consumer->op().BnInOp2Lbi(ibn) == out) {
      OperatorConf& consumer_op_conf = op_name2op_conf->at(consumer_op_name);
      const auto& new_val = new_out_lbn;
      const auto& old_val = ReplaceInputLbnInOpCustomizedConf(&consumer_op_conf, ibn, new_val);
      CHECK_EQ(GenLogicalBlobName(out), old_val);
    }
  }
}

bool IsUserOpWithTypeName(const OperatorConf& op_conf, const std::string& op_type_name) {
  return op_conf.has_user_conf() && op_conf.user_conf().op_type_name() == op_type_name;
};

class FuseEmbeddingShuffleInteractionPass final : public JobPass {
 public:
  FuseEmbeddingShuffleInteractionPass() = default;
  ~FuseEmbeddingShuffleInteractionPass() override = default;

  bool IsEnabled(const JobPassCtx& ctx) const {
    return true;
  }
  Maybe<void> Apply(const OpGraph& op_graph, JobBuilder* job_builder) const;

  Maybe<void> Apply(Job* job, JobPassCtx* ctx) const override {
    if (!IsEnabled(*ctx)) { return Maybe<void>::Ok(); }
    const OpGraph op_graph(*job);
    JobBuilder job_builder(job);
    return Apply(op_graph, &job_builder);
  }
};

Maybe<void> FuseEmbeddingShuffleInteractionPass::Apply(const OpGraph& op_graph,
                                                       JobBuilder* job_builder) const {
  HashMap<std::string, OperatorConf> op_name2op_conf;

  op_graph.ForEachNode([&](const OpNode* op_node) {
    if (!IsUserOpWithTypeName(op_node->op().op_conf(), "embedding_shuffle")) { return; }
    if (op_node->out_edges().size() > 2) { return; }
    const user_op::UserOpConfWrapper embedding_shuffle_conf(op_node->op().op_conf());
    const std::string& embeddings_lbn = embedding_shuffle_conf.output("embeddings", 0);
    const std::string& indices_lbn =
        embedding_shuffle_conf.input("inverse_unique_partition_indices", 0);
    for (const OpEdge* out_edge : op_node->out_edges()) {
      const OpNode* consumer = out_edge->dst_node();
      if (!consumer->op().op_conf().has_user_conf()) { return; }
      const user_op::UserOpConfWrapper consumer_op_conf(consumer->op().op_conf());
      if (!(consumer_op_conf.op_type_name() == "fused_dot_feature_interaction"
            || consumer_op_conf.op_type_name() == "fused_dot_feature_interaction_grad")) {
        return;
      }
      if (consumer_op_conf.attr<std::string>("pooling") != "none") { return; }
      int input_size = consumer_op_conf.input_size("features");
      CHECK_GT(input_size, 0);
      // only support embeddings as last feature
      if (consumer_op_conf.input("features", input_size - 1) != embeddings_lbn) { return; }
      user_op::UserOpConfWrapperBuilder fused_op_builder(consumer_op_conf.op_name());
      const std::string& op_type_name = consumer_op_conf.op_type_name();
      fused_op_builder.OpTypeName(op_type_name)
          .Input("sparse_feature", embeddings_lbn)
          .Input("sparse_indices", indices_lbn)
          .Attr<bool>("self_interaction", consumer_op_conf.attr<bool>("self_interaction"))
          .Attr<std::string>("pooling", consumer_op_conf.attr<std::string>("pooling"));
      for (int i = 0; i < input_size - 1; ++i) {
        fused_op_builder.Input("features", consumer_op_conf.input("features", i));
      }
      OperatorConf new_op_conf = consumer->op().op_conf();
      if (op_type_name == "fused_dot_feature_interaction") {
        if (consumer_op_conf.has_input("output_concat", 0)) {
          fused_op_builder.Input("output_concat", consumer_op_conf.input("output_concat", 0));
        }
        fused_op_builder.Output("out")
            .Attr<bool>("has_output_concat", consumer_op_conf.attr<bool>("has_output_concat"))
            .Attr<int32_t>("output_padding", consumer_op_conf.attr<int32_t>("output_padding"));
        *new_op_conf.mutable_user_conf() = fused_op_builder.Build().op_conf().user_conf();
      } else {
        // fused_dot_feature_interaction_grad
        fused_op_builder.Input("dy", consumer_op_conf.input("dy", 0))
            .Output("features_grad", input_size - 1)
            .Output("sparse_feature_grad")
            .Attr<int32_t>("output_concat_grad_dim",
                           consumer_op_conf.attr<int32_t>("output_concat_grad_dim"));
        if (consumer_op_conf.has_output("output_concat_grad", 0)) {
          fused_op_builder.Output("output_concat_grad");
        }
        user_op::UserOpConfWrapper fused_dot_feature_interaction_grad_op = fused_op_builder.Build();
        *new_op_conf.mutable_user_conf() =
            fused_dot_feature_interaction_grad_op.op_conf().user_conf();
        const LogicalBlobId last_feature_grad_lbi =
            GenLogicalBlobId(consumer_op_conf.output("features_grad", input_size - 1));
        std::string sparse_feature_grad_lbn =
            fused_dot_feature_interaction_grad_op.output("sparse_feature_grad", 0);
        for (const OpEdge* out_edge : consumer->out_edges()) {
          const OpNode* grad_out_node = out_edge->dst_node();
          if (out_edge->lbis().size() == 1 && out_edge->lbis().front() == last_feature_grad_lbi) {
            if (!IsUserOpWithTypeName(grad_out_node->op().op_conf(),
                                      "embedding_gradient_shuffle")) {
              return;
            }
            UpdateConsumerOpConf(grad_out_node, last_feature_grad_lbi, sparse_feature_grad_lbn,
                                 &op_name2op_conf);
          }
        }
      }
      LOG(ERROR) << "replace " << new_op_conf.name();
      job_builder->MutOpsOnlyOnce({new_op_conf});
    }
    for (const auto& pair : op_name2op_conf) {
      LOG(ERROR) << "replace " << pair.first;
      job_builder->MutOpsOnlyOnce({pair.second});
    }
  });

  return Maybe<void>::Ok();
}

}  // namespace

REGISTER_JOB_PASS("FuseEmbeddingShuffleInteractionPass", FuseEmbeddingShuffleInteractionPass);

}  // namespace oneflow
