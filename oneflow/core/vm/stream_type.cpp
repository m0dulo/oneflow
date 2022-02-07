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
#include "oneflow/core/vm/stream_type.h"
#include "oneflow/core/vm/instruction_type.h"
#include "oneflow/core/vm/stream.h"
#include "oneflow/core/vm/instruction.h"
#include "oneflow/core/common/util.h"
#include "oneflow/core/intrusive/intrusive.h"

namespace oneflow {
namespace vm {

namespace {

HashMap<StreamTypeId, StreamTypeId>* InferStreamTypeId4ComputeStreamTypeId() {
  static HashMap<StreamTypeId, StreamTypeId> map;
  return &map;
}

}  // namespace

HashMap<std::type_index, const StreamType*>* StreamType4TypeIndex() {
  static HashMap<std::type_index, const StreamType*> map;
  return &map;
}

const StreamTypeId& LookupInferStreamTypeId(const StreamTypeId& compute_stream_type_id) {
  return InferStreamTypeId4ComputeStreamTypeId()->at(compute_stream_type_id);
}

void StreamType::Run(Instruction* instruction) const { Compute(instruction); }

void TryRegisterInferStreamTypeId(const StreamType* infer_stream_type,
                                  const StreamType* compute_stream_type) {
  StreamTypeId compute_stream_type_id;
  compute_stream_type_id.__Init__(compute_stream_type, InterpretType::kCompute);
  StreamTypeId infer_stream_type_id;
  infer_stream_type_id.__Init__(infer_stream_type, InterpretType::kInfer);
  InferStreamTypeId4ComputeStreamTypeId()->emplace(compute_stream_type_id, infer_stream_type_id);
}

}  // namespace vm
}  // namespace oneflow
