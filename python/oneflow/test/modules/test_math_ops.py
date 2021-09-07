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

import unittest
from collections import OrderedDict

import numpy as np
from automated_test_util import *
from test_util import GenArgList, type_name_to_flow_type, type_name_to_np_type

import oneflow as flow
import oneflow.unittest


@flow.unittest.skip_unless_1n1d()
class TestSinh(flow.unittest.TestCase):
    @autotest()
    def test_flow_sinh_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.sinh(x)
        return y


@flow.unittest.skip_unless_1n1d()
class TestSin(flow.unittest.TestCase):
    @autotest()
    def test_flow_sin_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.sin(x)
        return y


def _test_cos(test_case, shape, device):
    input = flow.tensor(
        np.random.randn(*shape), dtype=flow.float32, device=flow.device(device)
    )
    of_out = flow.cos(input)
    np_out = np.cos(input.numpy())
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-05, 1e-05))


def _test_cos_backward(test_case, shape, device):
    x = flow.tensor(
        np.random.randn(*shape),
        dtype=flow.float32,
        device=flow.device(device),
        requires_grad=True,
    )
    y = flow.cos(x)
    z = y.sum()
    z.backward()
    np_grad = -np.sin(x.numpy())
    test_case.assertTrue(np.allclose(x.grad.numpy(), np_grad, 1e-05, 1e-05))


@flow.unittest.skip_unless_1n1d()
class TestCos(flow.unittest.TestCase):
    def test_cos(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_cos, _test_cos_backward]
        arg_dict["shape"] = [(2, 3), (2, 3, 4), (2, 3, 4, 5)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])


@flow.unittest.skip_unless_1n1d()
class TestLogModule(flow.unittest.TestCase):
    @autotest()
    def test_log_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        return torch.log(x)


def _test_std(test_case, shape, device):
    np_arr = np.random.randn(*shape)
    input = flow.tensor(np_arr, dtype=flow.float32, device=flow.device(device))
    of_out = flow.std(input, dim=2)
    np_out = np.std(np_arr, axis=2)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 0.0001, 0.0001))


def _test_std_dim1(test_case, shape, device):
    np_arr = np.random.randn(*shape)
    input = flow.tensor(np_arr, dtype=flow.float32, device=flow.device(device))
    of_out = flow.std(input, dim=1)
    np_out = np.std(np_arr, axis=1)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 0.0001, 0.0001))


def _test_std_negative_dim(test_case, shape, device):
    np_arr = np.random.randn(4, 2, 3, 5)
    input = flow.tensor(np_arr, dtype=flow.float32, device=flow.device(device))
    of_out = input.std(dim=(-2, -1, -3), keepdim=False)
    np_out = np.std(np_arr, axis=(-2, -1, -3))
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 0.0001, 0.0001))


@flow.unittest.skip_unless_1n1d()
class TestStd(flow.unittest.TestCase):
    def test_std(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_std, _test_std_dim1, _test_std_negative_dim]
        arg_dict["shape"] = [(2, 3, 4), (2, 3, 4, 5)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @unittest.skip("std has bug")
    @autotest()
    def test_std_flow_with_random_data(test_case):
        device = random_device()
        all_dim = random().to(int)
        dim = random(low=0, high=all_dim).to(int)
        x = random_pytorch_tensor(ndim=all_dim).to(device)
        z = torch.std(x, dim=dim)
        return z

    @unittest.skip("std has bug")
    @autotest()
    def test_std_tensor_with_random_data(test_case):
        device = random_device()
        all_dim = random().to(int)
        dim = random(low=0, high=all_dim).to(int)
        x = random_pytorch_tensor(ndim=all_dim).to(device)
        z = x.std(dim=dim)
        return z


@flow.unittest.skip_unless_1n1d()
class TestSqrt(flow.unittest.TestCase):
    @autotest()
    def test_sqrt_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        z = torch.sqrt(x)
        return z

    @autotest()
    def test_sqrt_tensor_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        z = x.sqrt()
        return z


@flow.unittest.skip_unless_1n1d()
class TestRsqrt(flow.unittest.TestCase):
    @autotest()
    def test_rsqrt_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        z = torch.rsqrt(x)
        return z


@flow.unittest.skip_unless_1n1d()
class TestSquare(flow.unittest.TestCase):
    @autotest()
    def test_square_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        z = torch.square(x)
        return z

    @autotest()
    def test_square_tensor_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        z = x.square()
        return z


@flow.unittest.skip_unless_1n1d()
class TestPow(flow.unittest.TestCase):
    @autotest()
    def test_pow_scalar_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = random().to(float)
        return torch.pow(x, y)

    @autotest()
    def test_pow_elementwise_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        return torch.pow(x, y)

    @autotest()
    def test_pow_broadcast_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=1).to(device)
        return torch.pow(x, y)

    @autotest()
    def test_pow_broadcast_with_random_data_reverse(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=1).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        return torch.pow(x, y)


@flow.unittest.skip_unless_1n1d()
class TestAsin(flow.unittest.TestCase):
    @autotest()
    def test_flow_asin_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=-0.5, high=0.5).to(device)
        y = torch.asin(x)
        return y

    @autotest()
    def test_flow_arcsin_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=-0.5, high=0.5).to(device)
        y = torch.arcsin(x)
        return y


@flow.unittest.skip_unless_1n1d()
class TestAsinh(flow.unittest.TestCase):
    @autotest()
    def test_flow_asinh_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.asinh(x)
        return y

    @autotest()
    def test_flow_arcsinh_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.arcsinh(x)
        return y


@flow.unittest.skip_unless_1n1d()
class TestTan(flow.unittest.TestCase):
    @autotest()
    def test_flow_tan_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.tan(x)
        return y


@flow.unittest.skip_unless_1n1d()
class TestAtan(flow.unittest.TestCase):
    @autotest()
    def test_flow_atan_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.atan(x)
        return y

    @autotest()
    def test_flow_arctan_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.arctan(x)
        return y

    @autotest()
    def test_flow_atan2_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=3).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=3).to(device)
        z = torch.atan2(x, y)
        return z

    @autotest()
    def test_flow_atanh_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=-0.5, high=0.5).to(device)
        y = torch.atanh(x)
        return y

    @autotest()
    def test_flow_arctanh_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=-0.5, high=0.5).to(device)
        y = torch.arctanh(x)
        return y


@flow.unittest.skip_unless_1n1d()
class TestTopk(flow.unittest.TestCase):
    @autotest(auto_backward=False)
    def test_flow_topk_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=4, dim1=8, dim2=9, dim3=10).to(device)
        y = torch.topk(
            x,
            random(low=1, high=8).to(int),
            dim=random(low=1, high=4).to(int),
            largest=random_bool(),
            sorted=constant(True),
        )
        return y[0], y[1]


@flow.unittest.skip_unless_1n1d()
class TestPow(flow.unittest.TestCase):
    @autotest()
    def test_pow_scalar_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = random().to(float)
        return torch.pow(x, y)

    @autotest()
    def test_pow_elementwise_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        return torch.pow(x, y)

    @unittest.skip("not support for broadcast currently")
    @autotest()
    def test_pow_broadcast_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=2, dim1=2).to(device)
        y = random_pytorch_tensor(ndim=2, dim1=1).to(device)
        return torch.pow(x, y)


@unittest.skipIf(
    not flow.unittest.env.eager_execution_enabled(),
    ".numpy() doesn't work in lazy mode",
)
@flow.unittest.skip_unless_1n1d()
class TestArccosh(flow.unittest.TestCase):
    @autotest()
    def test_arccosh_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=2, high=3).to(device)
        y = torch.arccosh(x)
        return y


@unittest.skipIf(
    not flow.unittest.env.eager_execution_enabled(),
    ".numpy() doesn't work in lazy mode",
)
@flow.unittest.skip_unless_1n1d()
class TestAcosh(flow.unittest.TestCase):
    @autotest()
    def test_acosh_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(low=2, high=3).to(device)
        y = torch.acosh(x)
        return y


@unittest.skipIf(
    not flow.unittest.env.eager_execution_enabled(),
    ".numpy() doesn't work in lazy mode",
)
@flow.unittest.skip_unless_1n1d()
class TestAtan2(flow.unittest.TestCase):
    @autotest()
    def test_flow_atan2_with_random_data(test_case):
        device = random_device()
        x1 = random_pytorch_tensor(ndim=1, dim0=1).to(device)
        x2 = random_pytorch_tensor(ndim=1, dim0=1).to(device)
        y = torch.atan2(x1, x2)
        return y


@unittest.skipIf(
    not flow.unittest.env.eager_execution_enabled(),
    ".numpy() doesn't work in lazy mode",
)
@flow.unittest.skip_unless_1n1d()
class TestMinimum(flow.unittest.TestCase):
    @autotest()
    def test_flow_elementwise_minimum_with_random_data(test_case):
        k1 = random(2, 6)
        k2 = random(2, 6)
        x = random_pytorch_tensor(ndim=2, dim0=k1, dim1=k2)
        y = random_pytorch_tensor(ndim=2, dim0=k1, dim1=k2)
        return torch.minimum(x, y)

    @autotest()
    def test_flow_broadcast_minimum_with_random_data(test_case):
        k1 = random(2, 6)
        k2 = random(2, 6)
        k3 = random(2, 6)
        x = random_pytorch_tensor(ndim=3, dim0=k1, dim1=1, dim2=1)
        y = random_pytorch_tensor(ndim=3, dim0=1, dim1=k2, dim2=k3)
        return torch.minimum(x, y)


@unittest.skipIf(
    not flow.unittest.env.eager_execution_enabled(),
    ".numpy() doesn't work in lazy mode",
)
class TestMaximum(flow.unittest.TestCase):
    @autotest()
    def test_flow_elementwise_mximum_with_random_data(test_case):
        k1 = random(2, 6)
        k2 = random(2, 6)
        x = random_pytorch_tensor(ndim=2, dim0=k1, dim1=k2)
        y = random_pytorch_tensor(ndim=2, dim0=k1, dim1=k2)
        return torch.maximum(x, y)

    @autotest()
    def test_flow_broadcast_maximum_with_random_data(test_case):
        k1 = random(2, 6)
        k2 = random(2, 6)
        k3 = random(2, 6)
        x = random_pytorch_tensor(ndim=3, dim0=k1, dim1=1, dim2=1)
        y = random_pytorch_tensor(ndim=3, dim0=1, dim1=k2, dim2=k3)
        return torch.maximum(x, y)


@flow.unittest.skip_unless_1n1d()
class TestUnaryInplaceOpsModule(flow.unittest.TestCase):
    def test_inplace(test_case):
        def test_of_np_result(
            test_case, shape, device, flow_fun, np_fun, low=0, high=1
        ):
            x = (high - low) * flow.rand(
                *shape, dtype=flow.float32, device=flow.device(device)
            ) + low
            x_inplace = x + 1e-5
            np_out = np_fun(x_inplace.numpy())
            id_old = id(x_inplace)
            y_inplace = flow_fun(x_inplace)
            test_case.assertEqual(id_old, id(y_inplace))
            test_case.assertTrue(np.allclose(y_inplace.numpy(), np_out, 1e-4, 1e-4))

        def test_inplace_impl(test_case, shape, device):
            ops = [
                (flow.Tensor.sin_, np.sin),
                (flow.Tensor.abs_, np.abs),
                (flow.Tensor.exp_, np.exp),
                (flow.Tensor.cosh_, np.cosh),
                (flow.Tensor.ceil_, np.ceil),
                (flow.Tensor.expm1_, np.expm1),
                (flow.Tensor.floor_, np.floor),
                (flow.Tensor.negative_, np.negative),
                (flow.Tensor.round_, np.round),
                (flow.Tensor.square_, np.square),
                (flow.Tensor.sign_, np.sign),
                (flow.Tensor.tanh_, np.tanh),
                (flow.Tensor.sinh_, np.sinh),
                (flow.Tensor.atan_, np.arctan),
                (flow.Tensor.cos_, np.cos),
                (flow.Tensor.asinh_, np.arcsinh),
            ]
            for pair in ops:
                test_of_np_result(
                    test_case, shape, device, pair[0], pair[1], low=-10, high=10
                )

            # x > 0
            def np_rsqrt(x):
                return np.reciprocal(np.sqrt(x))

            def np_erf(x):
                import math

                y = np.array([math.erf(item) for item in x.flatten()])
                return y.reshape(x.shape)

            def np_erfc(x):
                return 1 - np_erf(x)

            ops = [
                (flow.Tensor.log_, np.log),
                (flow.Tensor.sqrt_, np.sqrt),
                (flow.Tensor.rsqrt_, np_rsqrt),
                (flow.Tensor.erf_, np_erf),
                (flow.Tensor.erfc_, np_erfc),
            ]
            for pair in ops:
                test_of_np_result(
                    test_case, shape, device, pair[0], pair[1], low=0, high=10
                )

            # x > -1
            test_of_np_result(
                test_case,
                shape,
                device,
                flow.Tensor.log1p_,
                np.log1p,
                low=-0.99,
                high=10,
            )

            # -1 < x < 1
            ops = [
                (flow.Tensor.acos_, np.arccos),
                (flow.Tensor.asin_, np.arcsin),
                (flow.Tensor.atanh_, np.arctanh),
            ]
            for pair in ops:
                test_of_np_result(
                    test_case, shape, device, pair[0], pair[1], low=-0.99, high=0.99
                )

            # x > 1
            test_of_np_result(
                test_case,
                shape,
                device,
                flow.Tensor.acosh_,
                np.arccosh,
                low=1.01,
                high=10,
            )

            # -pi/2 < x pi/2
            test_of_np_result(
                test_case,
                shape,
                device,
                flow.Tensor.tan_,
                np.tan,
                low=-3.14 / 2 + 0.01,
                high=3.14 / 2 - 0.0 - 1,
            )

            # x != 0
            test_of_np_result(
                test_case,
                shape,
                device,
                flow.Tensor.reciprocal_,
                np.reciprocal,
                low=-10,
                high=-0.01,
            )
            test_of_np_result(
                test_case,
                shape,
                device,
                flow.Tensor.reciprocal_,
                np.reciprocal,
                low=0.01,
                high=10,
            )

        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [test_inplace_impl]
        arg_dict["shape"] = [(2, 3), (2, 3, 4), (2, 3, 4, 5)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])


if __name__ == "__main__":
    unittest.main()
