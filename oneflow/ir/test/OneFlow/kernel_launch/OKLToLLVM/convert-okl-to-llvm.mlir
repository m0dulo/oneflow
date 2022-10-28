// RUN: oneflow-opt %s \
// RUN: -only-keep-compute-ops \
// RUN: -lower-launcher-to-llvm-ptr \
// RUN: -lower-okl-to-llvm-call \
// RUN: -reconcile-unrealized-casts \
// RUN: -convert-func-to-llvm \
// RUN: -mlir-print-ir-after-all \
// RUN: | FileCheck %s

// CHECK: module attributes {llvm.data_layout = ""} {
// CHECK:   llvm.func @launch(!llvm.ptr<i8>, !llvm.ptr<i8>) attributes {llvm.emit_c_interface}
// CHECK:   llvm.func @fetch_kernel(!llvm.ptr<i8>, i64) -> !llvm.ptr<i8> attributes {llvm.emit_c_interface}
// CHECK:   llvm.func @fetch_run_ctx(!llvm.ptr<i8>, i64) -> !llvm.ptr<i8> attributes {llvm.emit_c_interface}
// CHECK:   llvm.func @okl_compute(%arg0: !llvm.ptr<i8>) attributes {llvm.emit_c_interface} {
// CHECK:     %0 = llvm.mlir.constant(0 : index) : i64
// CHECK:     %1 = llvm.call @fetch_run_ctx(%arg0, %0) : (!llvm.ptr<i8>, i64) -> !llvm.ptr<i8>
// CHECK:     %2 = llvm.mlir.constant(1 : index) : i64
// CHECK:     %3 = llvm.call @fetch_run_ctx(%arg0, %2) : (!llvm.ptr<i8>, i64) -> !llvm.ptr<i8>
// CHECK:     %4 = llvm.mlir.constant(0 : index) : i64
// CHECK:     %5 = llvm.call @fetch_kernel(%arg0, %4) : (!llvm.ptr<i8>, i64) -> !llvm.ptr<i8>
// CHECK:     %6 = llvm.mlir.constant(1 : index) : i64
// CHECK:     %7 = llvm.call @fetch_kernel(%arg0, %6) : (!llvm.ptr<i8>, i64) -> !llvm.ptr<i8>
// CHECK:     llvm.call @launch(%1, %5) : (!llvm.ptr<i8>, !llvm.ptr<i8>) -> ()
// CHECK:     llvm.call @launch(%3, %7) : (!llvm.ptr<i8>, !llvm.ptr<i8>) -> ()
// CHECK:     llvm.return
// CHECK:   }
// CHECK:   llvm.func @_mlir_ciface_okl_compute(%arg0: !llvm.ptr<i8>) attributes {llvm.emit_c_interface} {
// CHECK:     llvm.call @okl_compute(%arg0) : (!llvm.ptr<i8>) -> ()
// CHECK:     llvm.return
// CHECK:   }
// CHECK: }

module {
  func.func @okl_recycle(%arg0: !okl.launcher_ctx) {
    %0 = "okl.fetch_reg_ctx"(%arg0) {index = 0 : si64} : (!okl.launcher_ctx) -> !okl.reg_ctx
    %1 = "okl.fetch_reg_ctx"(%arg0) {index = 1 : si64} : (!okl.launcher_ctx) -> !okl.reg_ctx
    %2 = "okl.fetch_run_ctx"(%arg0) {index = 0 : si64} : (!okl.launcher_ctx) -> !okl.run_ctx
    %3 = "okl.fetch_run_ctx"(%arg0) {index = 1 : si64} : (!okl.launcher_ctx) -> !okl.run_ctx
    "okl.destroy_reg_ctx"(%0) : (!okl.reg_ctx) -> ()
    "okl.destroy_reg_ctx"(%1) : (!okl.reg_ctx) -> ()
    "okl.destroy_run_ctx"(%2) : (!okl.run_ctx) -> ()
    "okl.destroy_run_ctx"(%3) : (!okl.run_ctx) -> ()
    return
  }
  func.func @okl_compute(%arg0: !okl.launcher_ctx) {
    %0 = "okl.fetch_run_ctx"(%arg0) {index = 0 : si64} : (!okl.launcher_ctx) -> !okl.run_ctx
    %1 = "okl.fetch_run_ctx"(%arg0) {index = 1 : si64} : (!okl.launcher_ctx) -> !okl.run_ctx
    %2 = "okl.fetch_kernel"(%arg0) {index = 0 : si64} : (!okl.launcher_ctx) -> !okl.kernel
    %3 = "okl.fetch_kernel"(%arg0) {index = 1 : si64} : (!okl.launcher_ctx) -> !okl.kernel
    "okl.launch"(%0, %2) : (!okl.run_ctx, !okl.kernel) -> ()
    "okl.launch"(%1, %3) : (!okl.run_ctx, !okl.kernel) -> ()
    return
  }
  func.func @okl_init_context(%arg0: !okl.launcher_ctx) {
    %0 = "okl.build_reg_ctx"() ({
      %6 = "okl.get_tensor_from_arg"(%arg0) {index = 0 : i32, tensor_type = 0 : i32} : (!okl.launcher_ctx) -> tensor<2xf32>
      %7 = "oneflow.relu"(%6) {device_name = ["@0:0"], device_tag = "cpu", hierarchy = [1], op_name = "relu-0", scope_symbol_id = 12 : i64} : (tensor<2xf32>) -> tensor<2xf32>
      %8 = "okl.get_tensor_as_ret"(%arg0, %7) {index = 0 : i32, tensor_type = 2 : i32} : (!okl.launcher_ctx, tensor<2xf32>) -> tensor<2xf32>
      okl.return
    }) {function_type = () -> ()} : () -> !okl.reg_ctx
    %1 = "okl.build_reg_ctx"() ({
      %6 = "okl.get_tensor_from_ret"(%arg0) {index = 0 : i32, tensor_type = 0 : i32} : (!okl.launcher_ctx) -> tensor<2xf32>
      %7 = "oneflow.tanh"(%6) {device_name = ["@0:0"], device_tag = "cpu", hierarchy = [1], op_name = "tanh-1", scope_symbol_id = 12 : i64} : (tensor<2xf32>) -> tensor<2xf32>
      %8 = "okl.get_tensor_as_ret"(%arg0, %7) {index = 1 : i32, tensor_type = 2 : i32} : (!okl.launcher_ctx, tensor<2xf32>) -> tensor<2xf32>
      okl.return
    }) {function_type = () -> ()} : () -> !okl.reg_ctx
    %2 = "okl.build_run_ctx"(%0) : (!okl.reg_ctx) -> !okl.run_ctx
    %3 = "okl.build_run_ctx"(%1) : (!okl.reg_ctx) -> !okl.run_ctx
    %4 = "okl.build_op_kernel"(%0) : (!okl.reg_ctx) -> !okl.kernel
    %5 = "okl.build_op_kernel"(%1) : (!okl.reg_ctx) -> !okl.kernel
    return
  }
  func.func private @get_resources_type_0(!okl.launcher_ctx) -> (!okl.reg_ctx, !okl.reg_ctx)
  func.func private @get_resources_type_1(!okl.launcher_ctx) -> (!okl.run_ctx, !okl.run_ctx)
  func.func private @get_resources_type_2(!okl.launcher_ctx) -> (!okl.kernel, !okl.kernel)
}