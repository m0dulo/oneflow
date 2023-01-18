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
#ifndef ONEFLOW_CORE_JOB_REWRITER_PASS_UTIL_H_
#define ONEFLOW_CORE_JOB_REWRITER_PASS_UTIL_H_

#include <string>
#include <map>

#include "oneflow/core/graph/op_graph.h"

namespace oneflow {
#define INSERT_CHECK(expr) CHECK(expr.second)
#define INSERT_CHECK_OR_RETURN(expr) CHECK_OR_RETURN(expr.second)

template<typename MapT, typename KeyT>
bool IsKeyFound(const MapT& m, const KeyT& k) {
  return m.find(k) != m.end();
}

bool IsNodeInList(const HashSet<std::string>& op_list, OpNode* node);

template<typename ContainerT, typename ElemT>
std::string Container2Str(const ContainerT& container,
                          std::function<std::string(const ElemT&)> elem2str) {
  std::string ret;
  bool is_first = true;
  for (const ElemT& elem : container) {
    if (is_first) {
      is_first = false;
    } else {
      ret += ",\n";
    }
    ret += elem2str(elem);
  }
  return ret;
}

std::string ReplaceSlashToDash4Lbn(std::string lbn);

void DfsTopoGraphTraversal(const OpGraph& graph, bool reversed,
                           std::function<bool(OpNode*)> IsCurNodeStartNode,
                           std::function<bool(OpNode*)> IsCurNodeSatisfied,
                           std::function<bool(OpNode*)> IsFatherNodeSatisfied,
                           std::function<void(OpNode*)> NodeHandler);

// make sure an op_conf can only be udpated once, cuz later update will override before
class OpConfCache {
  std::map<std::string, OperatorConf> _op_confs_to_update;

 public:
  OperatorConf GetLatest(const OperatorConf& op_conf) {
    if (_op_confs_to_update.find(op_conf.name()) != _op_confs_to_update.end()) {
      return _op_confs_to_update[op_conf.name()];
    }
    return op_conf;
  }
  void Put(const OperatorConf& op_conf) { _op_confs_to_update[op_conf.name()] = op_conf; }
  std::vector<OperatorConf> op_confs() {
    std::vector<OperatorConf> ret;
    for (const auto& x : _op_confs_to_update) { ret.emplace_back(x.second); }
    return ret;
  }
};

std::function<bool(const OpNode* op_node)> MakePredicatorIsSafeToDelete(const OpGraph& op_graph);
bool IsUserOpWithTypeName(const OperatorConf& op_conf, const std::string& op_type_name);

void InsertCtrlEdgeInChain(const std::vector<const OpNode*>& ordered_op_nodes,
                           std::function<bool(const std::string&, const std::string&)>& IsReachable,
                           HashMap<std::string, OperatorConf>* mut_op_name2conf);

}  // namespace oneflow

#endif  // ONEFLOW_CORE_JOB_REWRITER_PASS_UTIL_H_
