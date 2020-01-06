#include "bit_reader.h"
#include "bit_writer.h"

#include <gtest/gtest.h>
#include <random>
#include <vector>

#include "common.h"

namespace zuckerli {
namespace {
constexpr size_t kTestSize = 1 << 24;
TEST(BitsTest, TestWriteNumBits) {
  std::mt19937 rng;
  std::uniform_int_distribution<int> dist(0, BitWriter::kMaxBitsPerCall);
  size_t total_bits = 0;
  BitWriter writer;
  writer.Reserve(kTestSize * BitWriter::kMaxBitsPerCall);
  for (size_t i = 0; i < kTestSize; i++) {
    size_t nbits = dist(rng);
    size_t bits = rng() & ((1 << nbits) - 1);
    writer.Write(nbits, bits);
    total_bits += nbits;
  }
  std::vector<uint8_t> data = std::move(writer).GetData();
  EXPECT_EQ(data.size(), DivCeil(total_bits, 8));
}
TEST(BitsTest, TestWriteNibbles) {
  BitWriter writer;
  writer.Reserve(8);
  writer.Write(4, 0xf);
  writer.Write(4, 0xa);
  writer.Write(4, 0x9);
  writer.Write(4, 0x8);
  std::vector<uint8_t> data = std::move(writer).GetData();
  EXPECT_EQ(data.size(), 2);
  EXPECT_EQ(data[0], uint8_t(0xaf));
  EXPECT_EQ(data[1], uint8_t(0x89));
}
TEST(BitsTest, TestWriteRead) {
  std::mt19937 rng;
  std::uniform_int_distribution<int> dist(0, BitWriter::kMaxBitsPerCall);
  std::vector<std::pair<int, uint64_t>> all_bits;
  all_bits.reserve(kTestSize);
  BitWriter writer;
  writer.Reserve(kTestSize * BitWriter::kMaxBitsPerCall);
  for (size_t i = 0; i < kTestSize; i++) {
    size_t nbits = dist(rng);
    size_t bits = rng() & ((1 << nbits) - 1);
    writer.Write(nbits, bits);
    all_bits.emplace_back(nbits, bits);
  }
  std::vector<uint8_t> data = std::move(writer).GetData();
  BitReader reader(data.data(), data.size());
  for (size_t i = 0; i < kTestSize; i++) {
    EXPECT_EQ(reader.ReadBits(all_bits[i].first), all_bits[i].second);
  }
}
} // namespace
} // namespace zuckerli
