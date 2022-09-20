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
#include "oneflow/core/job/rank_compiler.h"
#include "oneflow/core/job/global_for.h"
#include "oneflow/core/job/intra_job_mem_sharing_util.h"
#include "oneflow/core/job/plan_util.h"
#include "oneflow/core/persistence/tee_persistent_log_stream.h"
#include "oneflow/core/graph/op_graph.h"
#include "oneflow/core/job_rewriter/job_completer.h"
#include "oneflow/core/thread/thread_pool.h"
#include "oneflow/core/common/blocking_counter.h"
#include "oneflow/core/rpc/include/global_process_ctx.h"

namespace oneflow {

namespace {

void CreateOpAttributeRef(Plan* plan, int64_t job_id, TaskProto* task_proto) {
  auto* job_id2op_attribute_ref_table = plan->mutable_job_id2op_attribute_ref_table();
  CHECK(task_proto->exec_sequence().exec_node_size() == 1);
  auto* exec_node = task_proto->mutable_exec_sequence()->mutable_exec_node(0);
  CHECK(exec_node->kernel_conf().has_op_attribute());
  const std::string op_name = exec_node->kernel_conf().op_attribute().op_conf().name();
  auto* op_name2op_attribute =
      (*job_id2op_attribute_ref_table)[job_id].mutable_op_name2op_attribute();
  auto find_it = op_name2op_attribute->find(op_name);
  if (find_it == op_name2op_attribute->end()) {
    op_name2op_attribute->insert(
        {op_name, task_proto->exec_sequence().exec_node(0).kernel_conf().op_attribute()});
  }
  auto* kernel_conf =
      task_proto->mutable_exec_sequence()->mutable_exec_node(0)->mutable_kernel_conf();
  kernel_conf->set_op_attribute_ref(op_name);
  // NOTE(levi): memory of op_attribute_ is released here.
  kernel_conf->set_allocated_op_attribute(nullptr);
}

}  // namespace

Maybe<void> RankCompiler::Compile(Job* job, Plan* plan) const {
  // build task_gph.
  // TODO(levi): we can rewrite this part of code in visitor pattern.
  auto task_gph =
      JUST(RankTaskGraph::New(boxing_task_graph_proto_, rank_,
                              job->job_conf().enable_straighten_algorithm_in_task_graph()));
  const int64_t machine_id = GlobalProcessCtx::GetMachineId(rank_);
  using std::placeholders::_1;
  task_gph->ForEachNode(std::bind(&TaskNode::ProduceAllRegstsAndBindEdges, _1));
  task_gph->ForEachNode([&](TaskNode* task_node) {
    auto* comp_task_node = dynamic_cast<CompTaskNode*>(task_node);
    if (task_node->machine_id() != machine_id && comp_task_node != nullptr) {
      comp_task_node->ConsumeFakeRegstsIf();
    } else {
      task_node->ConsumeAllRegsts();
    }
  });
  task_gph->ForEachNode([&](TaskNode* task_node) {
    auto* comp_task_node = dynamic_cast<CompTaskNode*>(task_node);
    if (task_node->machine_id() != machine_id && comp_task_node != nullptr) {
      // Do nothing. because all consumed registers are fake.
    } else {
      task_node->PinConsumedRegst();
    }
  });
  task_gph->TopoForEachNode(&TaskNode::Build);
  task_gph->RemoveEmptyRegsts();
  task_gph->MergeChainAndAddOrderingCtrlEdgeInSameChain();
  auto IsReachable = Singleton<OpGraph>::Get()->MakePredicatorIsOpNameDataOrCtrlReachable();
  const JobDesc& job_desc = GlobalJobDesc();
  if (job_desc.enable_inplace()) {
    task_gph->ForEachGpuDeviceNodes([&](const HashSet<TaskNode*>& dev_nodes) {
      if (dev_nodes.empty()) { return; }
      if ((*dev_nodes.begin())->machine_id() != machine_id) { return; }
      task_gph->EnableInplaceMemSharing(dev_nodes, IsReachable);
    });
  }
  task_gph->TopoForEachNode(&TaskNode::InferTimeShapeIfMeaningful);
  task_gph->ForEachEdge([&](TaskEdge* task_edge) { task_edge->CheckRegstLbiValid(); });

  // put infomation from task_gph into plan.
  task_gph->ForEachNode([&](TaskNode* task_node) {
    if (task_node->IsMeaningLess()) { return; }
    int64_t machine_id = GlobalProcessCtx::GetMachineId(rank_);
    if (machine_id != task_node->machine_id()) { return; }
    TaskProto task_proto;
    task_node->ToProto(&task_proto);
    if (task_node->GetTaskType() == kNormalForward || task_node->GetTaskType() == kRepeat
        || task_node->GetTaskType() == kAcc) {
      CreateOpAttributeRef(plan, job_desc.job_id(), &task_proto);
    }
    plan->mutable_task()->Add(std::move(task_proto));
  });
  // NOTE(levi): release task_gph here to decrise memory peak.
  task_gph.reset();

  // post-process for plan and delete Singleton<OpGraph>.
  auto* job_id2job_conf = plan->mutable_job_confs()->mutable_job_id2job_conf();
  (*job_id2job_conf)[GlobalJobDesc().job_id()] = GlobalJobDesc().job_conf();
  // NOTE(chengcheng): infer mem blob id & set inplace & add ctrl
  IntraJobMemSharingUtil::InferMemBlockId4MemReusedRegst(plan, IsReachable);
  PlanUtil::SetUniqueMemBlockId4UnreusedMemRegst(plan);
  return Maybe<void>::Ok();
}

}  // namespace oneflow
