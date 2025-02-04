// RUN: oneflow-opt %s \
// RUN: -lower-okl-to-llvm-call \
// RUN: | FileCheck %s

//CHECK: module {
//CHECK:   llvm.func @okl_llvm_func(!llvm.ptr<i8>, i64) attributes {llvm.emit_c_interface}
//CHECK:   func.func @okl_func(%[[ARG:[a-zA-Z0-9_]+]]: !llvm.ptr<i8>) attributes {llvm.emit_c_interface} {
//CHECK:     %[[ARG0:[a-zA-Z0-9_]+]] = builtin.unrealized_conversion_cast %[[ARG]] : !llvm.ptr<i8> to !okl.launcher_ctx
//CHECK:     %[[ARG1:[a-zA-Z0-9_]+]] = llvm.mlir.constant(0 : index) : i64
//CHECK:     llvm.call @okl_llvm_func(%[[ARG]], %[[ARG1]]) : (!llvm.ptr<i8>, i64) -> ()
//CHECK:     %[[ARG2:[a-zA-Z0-9_]+]] = llvm.mlir.constant(1 : index) : i64
//CHECK:     llvm.call @okl_llvm_func(%[[ARG]], %[[ARG2]]) : (!llvm.ptr<i8>, i64) -> ()
//CHECK:     return
//CHECK:   }
//CHECK: }

module {
  func.func @okl_func(%arg0: !llvm.ptr<i8>) attributes {llvm.emit_c_interface} {
    %0 = builtin.unrealized_conversion_cast %arg0 : !llvm.ptr<i8> to !okl.launcher_ctx
    "okl.wrapper_kernel"() ({
      %1 = "okl.get_tensor_from_arg"(%0) {index = 0 : i32} : (!okl.launcher_ctx) -> tensor<2xf32>
      %2 = "oneflow.relu"(%1) {device_name = ["@0:0"], device_tag = "cpu", hierarchy = [1], op_name = "relu-0", scope_symbol_id = 12 : i64} : (tensor<2xf32>) -> tensor<2xf32>
      %3 = "okl.get_tensor_as_ret"(%0, %2) {index = 0 : i32} : (!okl.launcher_ctx, tensor<2xf32>) -> tensor<2xf32>
      okl.return
    }) {index = 0 : i32} : () -> ()
    "okl.wrapper_kernel"() ({
      %1 = "okl.get_tensor_from_ret"(%0) {index = 0 : i32} : (!okl.launcher_ctx) -> tensor<2xf32>
      %2 = "oneflow.tanh"(%1) {device_name = ["@0:0"], device_tag = "cpu", hierarchy = [1], op_name = "tanh-1", scope_symbol_id = 12 : i64} : (tensor<2xf32>) -> tensor<2xf32>
      %3 = "okl.get_tensor_as_ret"(%0, %2) {index = 1 : i32} : (!okl.launcher_ctx, tensor<2xf32>) -> tensor<2xf32>
      okl.return
    }) {index = 1 : i32} : () -> ()
    return
  }
}
