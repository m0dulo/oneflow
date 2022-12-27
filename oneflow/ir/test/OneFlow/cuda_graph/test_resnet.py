"""
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
"""
# RUN: python3 -m oneflow.test_utils.throttle --with-cuda=%with_cuda python3 %s

import os
import sys

sys.path.append(os.path.abspath(os.path.dirname(__file__)))
sys.path.append(os.path.abspath(os.path.dirname(__file__)) + "/..")

# TODO(peihong): extract MLIR ir env variables into a single module to control.
os.environ["ONEFLOW_MLIR_ENABLE_ROUND_TRIP"] = "1"
os.environ["ONEFLOW_MLIR_FUSE_KERNEL_LAUNCH"] = "1"
os.environ["ONEFLOW_MLIR_FUSE_FORWARD_OPS"] = "0"
# os.environ["ONEFLOW_KERNEL_ENABLE_CUDA_GRAPH"] = "1"
# os.environ["ONEFLOW_MLIR_ENABLE_IR_PRINTING"] = "1"

import unittest
import numpy as np
import oneflow as flow
import oneflow.unittest
from networks.resnet50 import resnet50
import time


def _test_okl_resnet(test_case):
    x = flow.randn(2, 3, 224, 224)
    resnet = resnet50()
    x = x.cuda()
    resnet.to("cuda")


    class GraphToRun(flow.nn.Graph):
        def __init__(self):
            super().__init__()
            self.resnet = resnet

        def build(self, x):
            return self.resnet(x)

    graph_to_run = GraphToRun()
    
    warm = 1
    batch = 10
    # epoch = 20
    for _ in range(warm):
        for i in range(batch):
            lazy_res = graph_to_run(x)

    # outcome = 0
    # for i in range(epoch):
    #     tick = time.time()
    #     for _ in range(batch):
    #         lazy_res = graph_to_run(x)
    #     tick = (time.time() - tick)/batch
    #     print(f'{i}: {tick}')
    #     outcome += tick
    # print(f'avg: {outcome/epoch}')


@flow.unittest.skip_unless_1n1d()
class TestOKLResNet(flow.unittest.TestCase):
    @unittest.skipUnless(flow.sysconfig.with_cuda(), "only test cpu cases")
    def test_okl_resnet(test_case):
        _test_okl_resnet(test_case)


if __name__ == "__main__":
    unittest.main()
