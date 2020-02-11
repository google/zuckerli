// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ans.h"

#include <random>

#include "bit_reader.h"
#include "integer_coder.h"
#include "gtest/gtest.h"

namespace zuckerli {
namespace {

TEST(ANSTest, TestRoundtrip) {
  constexpr size_t kNumIntegers = 1 << 24;
  constexpr size_t kNumContexts = 128;

  IntegerData data;

  std::mt19937 rng;
  std::uniform_int_distribution<uint32_t> dist(
      0, std::numeric_limits<uint32_t>::max());
  std::uniform_int_distribution<uint32_t> ctx_dist(0, kNumContexts - 1);

  for (size_t i = 0; i < kNumIntegers; i++) {
    size_t ctx = ctx_dist(rng);
    size_t integer = dist(rng);
    data.Add(ctx, integer);
  }

  BitWriter writer;
  ANSEncode(data, kNumContexts, &writer);

  std::vector<uint8_t> encoded = std::move(writer).GetData();
  BitReader reader(encoded.data(), encoded.size());
  ANSReader symbol_reader;
  ASSERT_TRUE(symbol_reader.Init(kNumContexts, &reader));

  for (size_t i = 0; i < kNumIntegers; i++) {
    EXPECT_EQ(IntegerCoder::Read(data.Context(i), &reader, &symbol_reader),
              data.Value(i));
  }
  EXPECT_TRUE(symbol_reader.CheckFinalState());
}

}  // namespace
}  // namespace zuckerli
