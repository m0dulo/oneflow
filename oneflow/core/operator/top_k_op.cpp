#include "oneflow/core/operator/top_k_op.h"

namespace oneflow {

void TopKOp::InitFromOpConf() {
  CHECK(op_conf().has_top_k_conf());
  EnrollInputBn("in", false);
  EnrollFwBufBn("fw_buf");
  EnrollOutputBn("out", false);
}

void TopKOp::InferBlobDescs(std::function<BlobDesc*(const std::string&)> GetBlobDesc4BnInOp,
                            const ParallelContext* parallel_ctx) const {
  const BlobDesc* in = GetBlobDesc4BnInOp("in");
  CHECK_LE(in->shape().elem_cnt(), GetMaxVal<int32_t>());
  const TopKOpConf& conf = op_conf().top_k_conf();
  // GPU version top_k op only support "k == 1" for now
  CHECK_GE(conf.k(), 1);
  CHECK_LE(conf.k(), in->shape().dim_vec().back());
  // fw_buf
  if (conf.k() > 1) {
    BlobDesc* fw_buf = GetBlobDesc4BnInOp("fw_buf");
    fw_buf->mut_shape() = Shape({in->shape()});
    fw_buf->set_data_type(DataType::kInt32);
  }
  // out
  BlobDesc* out = GetBlobDesc4BnInOp("out");
  *out = *in;
  out->mut_shape().Set(in->shape().NumAxes() - 1, conf.k());
  out->set_data_type(DataType::kInt32);
}

void TopKOp::VirtualGenKernelConf(
    std::function<const BlobDesc*(const std::string&)> GetBlobDesc4BnInOp, const ParallelContext*,
    KernelConf* kernel_conf) const {
  kernel_conf->set_data_type(GetBlobDesc4BnInOp("in")->data_type());
}

REGISTER_OP(OperatorConf::kTopKConf, TopKOp);

}  // namespace oneflow
