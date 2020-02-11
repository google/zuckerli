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
#include "bit_writer.h"
#include "integer_coder.h"
#include "gtest/gtest.h"
#include "absl/flags/flag.h"

namespace zuckerli {
namespace {

struct ByteCoder {
  size_t Read(size_t ctx, BitReader *ZKR_RESTRICT br) {
    return br->ReadBits(8);
  }
};

void TestIntegerCoder(int k, int h) {
  absl::SetFlag(&FLAGS_log2_num_explicit, k);
  absl::SetFlag(&FLAGS_num_token_bits, h);
  for (size_t i = 0; i < (1 << 14); i++) {
    BitWriter writer;
    writer.Reserve(256);
    size_t token, nbits, bits;
    IntegerCoder::Encode(i, &token, &nbits, &bits);
    writer.Write(8, token);
    writer.Write(nbits, bits);
    std::vector<uint8_t> data = std::move(writer).GetData();
    BitReader reader(data.data(), data.size());
    ByteCoder coder;
    EXPECT_EQ(i, IntegerCoder::Read(0, &reader, &coder));
  }
}

TEST(IntegerCoderTest, Test00) { TestIntegerCoder(0, 0); }
TEST(IntegerCoderTest, Test40) { TestIntegerCoder(4, 0); }
TEST(IntegerCoderTest, Test41) { TestIntegerCoder(4, 1); }
TEST(IntegerCoderTest, Test42) { TestIntegerCoder(4, 2); }
TEST(IntegerCoderTest, Test43) { TestIntegerCoder(4, 3); }
TEST(IntegerCoderTest, Test44) { TestIntegerCoder(4, 4); }
}  // namespace
}  // namespace zuckerli
