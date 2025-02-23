// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/fluid/ir_adaptor/translator/program_translator.h"

#include <unordered_map>

#include "glog/logging.h"

#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/ir_adaptor/translator/op_translator.h"
#include "paddle/fluid/ir_adaptor/translator/type_translator.h"
#include "paddle/ir/core/attribute.h"
#include "paddle/ir/core/block.h"
#include "paddle/ir/core/builtin_op.h"
#include "paddle/ir/core/builtin_type.h"
#include "paddle/ir/core/operation.h"
#include "paddle/phi/core/enforce.h"

namespace paddle {
namespace translator {

using ProgramDesc = ::paddle::framework::ProgramDesc;
using BlockDesc = ::paddle::framework::BlockDesc;

ProgramTranslator::ProgramTranslator(const ProgramDesc* legacy_program,
                                     ir::Program* program)
    : legacy_program(legacy_program), program(program) {
  ctx = ir::IrContext::Instance();
}

const std::unordered_set<std::string> ProgramTranslator::no_cast_var_names = {
    "feed",
    "fetch",
};

void ProgramTranslator::Translate() {
  PADDLE_ENFORCE_EQ(
      legacy_program->Size(),
      1u,
      platform::errors::PreconditionNotMet(
          "Not support multi block ProgramDesc translated, now has %d blocks",
          legacy_program->Size()));

  for (size_t block_idx = 0; block_idx < legacy_program->Size(); block_idx++) {
    const BlockDesc& block = legacy_program->Block(block_idx);
    ExtractParameterFromSingleBlock(block);
  }

  for (size_t block_idx = 0; block_idx < legacy_program->Size(); block_idx++) {
    const BlockDesc& block = legacy_program->Block(block_idx);
    InsertOperationToSingleBlock(block);
  }
}

void ProgramTranslator::ExtractParameterFromSingleBlock(
    const BlockDesc& block) {
  auto& type_translator = TypeTranslator::instance();

  for (auto& var : block.AllVars()) {
    if (!var->Persistable()) continue;
    if (param_map.count(var->Name()) != 0) continue;
    if (no_cast_var_names.count(var->Name()) != 0) continue;

    std::string get_parameter_op_name(ir::GetParameterOp::name());
    ir::OpInfo op_info = ctx->GetRegisteredOpInfo(get_parameter_op_name);
    std::unordered_map<std::string, ir::Attribute> op_attribute_map = {
        {"parameter_name", ir::StrAttribute::get(ctx, var->Name())},
    };
    ir::Type translated_var_type = type_translator[var->GetType()](ctx, *var);
    ir::Operation* operation = ir::Operation::Create(
        {}, op_attribute_map, {translated_var_type}, op_info);
    program->block()->push_back(operation);
    param_map[var->Name()] =
        VariableDefiningInfo(operation->GetResultByIndex(0));
    VLOG(10) << "[op translated][get parameter]" << operation;

    program->SetParameter(var->Name(), nullptr);
  }
}

void ProgramTranslator::InsertOperationToSingleBlock(const BlockDesc& block) {
  auto& op_translator = OpTranslator::instance();
  for (auto op : block.AllOps()) {
    OpTranslateFn& fn = op_translator[op->Type()];
    ir::Operation* operation = fn(ctx, &param_map, program, *op);
    VLOG(10) << "[op translated][special]" << operation;
  }
}

}  // namespace translator
}  // namespace paddle
