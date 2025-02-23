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

#include <gtest/gtest.h>

#include "paddle/fluid/ir/dialect/pd_dialect.h"
#include "paddle/fluid/ir/dialect/pd_type.h"
#include "paddle/fluid/ir/dialect/utils.h"
#include "paddle/fluid/ir/interface/op_yaml_info.h"
#include "paddle/ir/core/builtin_attribute.h"
#include "paddle/ir/core/builtin_dialect.h"
#include "paddle/ir/core/builtin_op.h"
#include "paddle/ir/core/ir_context.h"
#include "paddle/ir/core/program.h"
#include "paddle/ir/core/utils.h"
#include "paddle/phi/core/meta_tensor.h"
#include "paddle/phi/infermeta/binary.h"
#include "paddle/phi/kernels/elementwise_add_kernel.h"

#include "paddle/fluid/framework/scope.h"
#include "paddle/fluid/framework/tensor.h"
#include "paddle/fluid/framework/variable.h"
#include "paddle/fluid/framework/variable_helper.h"

#include "paddle/phi/common/place.h"
#include "paddle/phi/core/kernel_context.h"
#include "paddle/phi/core/kernel_factory.h"

#include "paddle/fluid/platform/init.h"

#include "paddle/fluid/ir/dialect/pd_attribute.h"

#include "paddle/phi/core/kernel_registry.h"
#include "test/cpp/ir/core/phi_kernel_adaptor.h"

PD_DECLARE_KERNEL(uniform, CPU, ALL_LAYOUT);
PD_DECLARE_KERNEL(add, CPU, ALL_LAYOUT);

bool simple_cmp(float a, float b) { return std::abs((a - b) / a) < 1e-5; }

TEST(program_test, program) {
  ir::IrContext* ctx = ir::IrContext::Instance();
  ctx->GetOrRegisterDialect<paddle::dialect::PaddleDialect>();

  ir::Program program(ctx);
  ir::Block* block = program.block();
  ir::Type fp32_dtype = ir::Float32Type::get(ctx);

  paddle::dialect::DenseTensorTypeStorage::Dim dims = {2, 2};
  paddle::dialect::DenseTensorTypeStorage::DataLayout data_layout =
      paddle::dialect::DenseTensorTypeStorage::DataLayout::NCHW;
  paddle::dialect::DenseTensorTypeStorage::LoD lod = {};
  size_t offset = 0;
  ir::Type dense_tensor_dtype = paddle::dialect::DenseTensorType::get(
      ctx, fp32_dtype, dims, data_layout, lod, offset);

  // (1) Def a = GetParameterOp("a")
  std::string op1_name = std::string(paddle::dialect::UniformOp::name());
  ir::OpInfo op1_info = ctx->GetRegisteredOpInfo(op1_name);

  // ir::Attribute shape_1 = ir::ArrayAttribute::get(ctx, {ten} );
  ir::Attribute shape_1 = paddle::dialect::IntArrayAttribute::get(
      ctx, std::vector<int64_t>({2, 2}));
  ir::Attribute data_type =
      paddle::dialect::DataTypeAttribute::get(ctx, phi::DataType::FLOAT32);
  ir::Attribute min = ir::FloatAttribute::get(ctx, 0.0);
  ir::Attribute max = ir::FloatAttribute::get(ctx, 1.0);
  ir::Attribute seed = ir::Int32_tAttribute::get(ctx, 2);
  ir::Attribute uni_place = paddle::dialect::PlaceAttribute::get(
      ctx, phi::Place(phi::AllocationType::CPU));
  std::unordered_map<std::string, ir::Attribute> op1_attribute{
      {"shape", shape_1},
      {"dtype", data_type},
      {"min", min},
      {"max", max},
      {"seed", seed},
      {"place", uni_place}};
  ir::Operation* op1 =
      ir::Operation::Create({}, op1_attribute, {dense_tensor_dtype}, op1_info);

  block->push_back(op1);

  // (2) Def b = GetParameterOp("b")
  std::string op2_name = std::string(paddle::dialect::UniformOp::name());
  ir::OpInfo op2_info = ctx->GetRegisteredOpInfo(op2_name);
  ir::Attribute ten2 = ir::Int32_tAttribute::get(ctx, 3);
  std::unordered_map<std::string, ir::Attribute> op2_attribute{{"shape", ten2}};
  ir::Operation* op2 =
      ir::Operation::Create({}, op1_attribute, {dense_tensor_dtype}, op2_info);
  block->push_back(op2);

  // (3) Def out = AddOp(a, b)
  std::string add_op_name = std::string(paddle::dialect::AddOp::name());
  ir::OpInfo add_op_info = ctx->GetRegisteredOpInfo(add_op_name);
  ir::Operation* add_op = ir::Operation::Create(
      {op1->GetResultByIndex(0), op2->GetResultByIndex(0)},
      {},
      {dense_tensor_dtype},
      add_op_info);
  block->push_back(add_op);

  paddle::framework::Scope scope;

  PhiKernelAdaptor phi_kernel_adaptor(&scope);

  phi_kernel_adaptor.run(&program);

  auto out_tensor =
      scope.Var(phi_kernel_adaptor.out_name)->Get<phi::DenseTensor>();

  bool res0 = simple_cmp(out_tensor.data<float>()[0], 1.80721);
  bool res1 = simple_cmp(out_tensor.data<float>()[1], 1.70047);
  bool res2 = simple_cmp(out_tensor.data<float>()[2], 1.56764);
  bool res3 = simple_cmp(out_tensor.data<float>()[3], 1.85063);
  std::cerr << out_tensor.data<float>()[0] << "\t"
            << out_tensor.data<float>()[1] << "\t"
            << out_tensor.data<float>()[2] << "\t"
            << out_tensor.data<float>()[3] << std::endl;
  EXPECT_EQ(res0, true);
  EXPECT_EQ(res1, true);
  EXPECT_EQ(res2, true);
  EXPECT_EQ(res3, true);
}
