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

#include <gtest/gtest.h>

namespace zuckerli {
namespace {

struct ByteCoder {
  size_t Read(size_t ctx, BitReader *ZKR_RESTRICT br) {
    return br->ReadBits(8);
  }
};

template <typename T> void TestIntegerCoder() {
  for (size_t i = 0; i < (1 << 14); i++) {
    BitWriter writer;
    writer.Reserve(256);
    size_t token, nbits, bits;
    T::Encode(i, &token, &nbits, &bits);
    writer.Write(8, token);
    writer.Write(nbits, bits);
    std::vector<uint8_t> data = std::move(writer).GetData();
    BitReader reader(data.data(), data.size());
    ByteCoder coder;
    EXPECT_EQ(i, T::Read(0, &reader, &coder));
  }
}

TEST(IntegerCoderTest, TestDefault) { TestIntegerCoder<IntegerCoder>(); }
TEST(IntegerCoderTest, Test00) {
  TestIntegerCoder<detail::IntegerCoder<0, 0>>();
}
TEST(IntegerCoderTest, Test40) {
  TestIntegerCoder<detail::IntegerCoder<4, 0>>();
}
TEST(IntegerCoderTest, Test42) {
  TestIntegerCoder<detail::IntegerCoder<4, 2>>();
}
TEST(IntegerCoderTest, Test43) {
  TestIntegerCoder<detail::IntegerCoder<4, 3>>();
}
TEST(IntegerCoderTest, Test44) {
  TestIntegerCoder<detail::IntegerCoder<4, 4>>();
}
} // namespace
} // namespace zuckerli
