#ifndef ONEFLOW_CORE_GRAPH_EXEC_GRAPH_H_
#define ONEFLOW_CORE_GRAPH_EXEC_GRAPH_H_

#include "oneflow/core/graph/exec_sequence.pb.h"
#include "oneflow/core/operator/operator.h"
#include "oneflow/core/graph/graph.h"
#include "oneflow/core/register/register_desc.h"
#include "oneflow/core/common/protobuf.h"

namespace oneflow {

class ExecNode;

class ExecEdge final : public Edge<ExecNode, ExecEdge> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ExecEdge);
  ExecEdge() = default;
  ~ExecEdge() = default;

  // Getters
  const std::string& lbn() const { return lbn_; }
  const std::string& src_bn() const { return src_bn_; }
  const std::string& dst_bn() const { return dst_bn_; }

  // Setters
  void set_lbn(const std::string& lbn);
  std::string& mut_src_bn() { return src_bn_; }
  std::string& mut_dst_bn() { return dst_bn_; }

 private:
  // various names for one blob
  std::string lbn_;
  std::string src_bn_;
  std::string dst_bn_;

};

class ExecNode final : public Node<ExecNode, ExecEdge> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ExecNode);
  ExecNode() = default;
  ~ExecNode() = default;

  std::shared_ptr<const Operator> op() const { return op_; }
  std::shared_ptr<const Operator>& mut_op() { return op_; }

  void BindBnInOpAndRegst(const std::string& bn_in_op, std::weak_ptr<RegstDesc> regst) {
    CHECK(bn_in_op2regst_.emplace(bn_in_op, regst).second);
  }
  std::shared_ptr<RegstDesc> GetRegstFromBnInOp(const std::string& bn_in_op) const {
    return bn_in_op2regst_.at(bn_in_op).lock();
  }
  const HashMap<std::string, std::weak_ptr<RegstDesc>>& bn_in_op2regst() const {
    return bn_in_op2regst_;
  }

  std::function<Shape*(const std::string&)> GetMutShapePtr4BnInOpFunc() const;
  
  std::string VisualStr() const { return op_->op_name(); }
  
  void ToProto(ExecNodeProto* ret) const;

 private:
  std::shared_ptr<const Operator> op_;
  HashMap<std::string, std::weak_ptr<RegstDesc>> bn_in_op2regst_;

};

class ExecGraph final : public Graph<ExecNode, ExecEdge> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ExecGraph);
  ExecGraph() = default;
  ~ExecGraph() = default;
  
  void ToExecSequence(ExecSequence* ret) const;

 private:

};

} // namespace oneflow

#endif // ONEFLOW_CORE_GRAPH_EXEC_GRAPH_H_
