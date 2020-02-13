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

#include "huffman.h"

#include <algorithm>

#include "bit_reader.h"
#include "common.h"
#include "integer_coder.h"

namespace zuckerli {
namespace {
struct HuffmanSymbolInfo {
  uint8_t present;
  uint8_t nbits;
  uint8_t bits;
};

// Reverses bit order.
static ZKR_INLINE uint8_t FlipByte(const uint8_t x) {
  static constexpr uint8_t kNibbleLut[16] = {
      0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110,
      0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111,
  };
  return (kNibbleLut[x & 0xF] << 4) | kNibbleLut[x >> 4];
}

// Very simple encoding: for each symbol, 1 bit for presence/absence, and 3 bits
// for symbol length if present.
void EncodeSymbolNBits(const HuffmanSymbolInfo* ZKR_RESTRICT info,
                       BitWriter* ZKR_RESTRICT writer) {
  for (size_t i = 0; i < kNumSymbols; i++) {
    if (info[i].present) {
      writer->Write(1, 1);
      writer->Write(3, info[i].nbits - 1);
    } else {
      writer->Write(1, 0);
    }
  }
}

void DecodeSymbolNBits(HuffmanSymbolInfo* ZKR_RESTRICT info,
                       BitReader* ZKR_RESTRICT reader) {
  for (size_t i = 0; i < kNumSymbols; i++) {
    info[i].present = reader->ReadBits(1);
    if (info[i].present) {
      info[i].nbits = reader->ReadBits(3) + 1;
    }
  }
}

// For a given array of HuffmanSymbolInfo, where only the `present` and `nbits`
// fields are set, fill up the `bits` field by building a Canonical Huffman code
// (https://en.wikipedia.org/wiki/Canonical_Huffman_code).
bool ComputeSymbolBits(HuffmanSymbolInfo* ZKR_RESTRICT info) {
  std::pair<uint8_t, uint8_t> syms[kNumSymbols];
  size_t present_symbols = 0;
  for (size_t i = 0; i < kNumSymbols; i++) {
    if (info[i].present == 0) continue;
    syms[present_symbols++] = {info[i].nbits, static_cast<uint8_t>(i)};
  }
  std::sort(syms, syms + present_symbols);
  size_t x = 0;
  for (size_t s = 0; s < present_symbols; s++) {
    info[syms[s].second].bits =
        FlipByte(x) >> (kMaxHuffmanBits - info[syms[s].second].nbits);
    x++;
    if (s + 1 != present_symbols) {
      x <<= syms[s + 1].first - syms[s].first;
    }
  }
  return true;
}

// Computes the lookup table from bitstream bits to decoded symbol for the
// decoder.
bool ComputeDecoderTable(const HuffmanSymbolInfo* sym_info,
                         HuffmanDecoderInfo* decoder_info) {
  size_t cnt = 0;
  size_t s = 0;
  for (size_t sym = 0; sym < kNumSymbols; sym++) {
    if (sym_info[sym].present == 0) continue;
    cnt++;
    s = sym;
  }
  if (cnt <= 1) {
    for (size_t i = 0; i < (1 << kMaxHuffmanBits); i++) {
      decoder_info[i].nbits = sym_info[s].nbits;
      decoder_info[i].symbol = s;
    }
    return true;
  }
  for (size_t i = 0; i < (1 << kMaxHuffmanBits); i++) {
    size_t s = kNumSymbols;
    for (size_t sym = 0; sym < kNumSymbols; sym++) {
      if (sym_info[sym].present == 0) continue;
      if ((i & ((1U << sym_info[sym].nbits) - 1)) == sym_info[sym].bits) {
        s = sym;
        break;
      }
    }
    if (s == kNumSymbols) return ZKR_FAILURE("Invalid table");
    decoder_info[i].nbits = sym_info[s].nbits;
    decoder_info[i].symbol = s;
  }
  return true;
}

// Compute the optimal number of bits for each symbol given the input
// distribution. Uses a (quadratic version) of the package-merge/coin-collector
// algorithm.
void ComputeSymbolNumBits(const std::vector<size_t>& histogram,
                          HuffmanSymbolInfo* ZKR_RESTRICT info) {
  // Mark the present/missing symbols.
  size_t nzsym = 0;
  for (size_t i = 0; i < histogram.size(); i++) {
    if (histogram[i] == 0) continue;
    info[i].present = 1;
    nzsym++;
  }
  if (nzsym <= 1) {
    for (size_t i = 0; i < kNumSymbols; i++) {
      if (info[i].present) {
        info[i].nbits = 1;
      }
    }
    return;
  }

  // Create a list of symbols for any given cost.
  std::vector<std::pair<size_t, std::vector<uint8_t>>> bags[kMaxHuffmanBits];
  for (size_t i = 0; i < kMaxHuffmanBits; i++) {
    for (size_t s = 0; s < kNumSymbols; s++) {
      if (info[s].present == 0) continue;
      std::vector<uint8_t> sym(1, s);
      bags[i].emplace_back(histogram[s], sym);
    }
  }

  // Pair up symbols (or groups of symbols) of a given bit-length to create
  // symbols of the following bit-length, creating pairs by merging (groups of)
  // symbols consecutively in increasing order of cost.
  for (size_t i = 0; i < kMaxHuffmanBits - 1; i++) {
    std::sort(bags[i].begin(), bags[i].end());
    for (size_t j = 0; j + 1 < bags[i].size(); j += 2) {
      size_t nf = bags[i][j].first + bags[i][j + 1].first;
      std::vector<uint8_t> nsym = std::move(bags[i][j].second);
      nsym.insert(nsym.end(), bags[i][j + 1].second.begin(),
                  bags[i][j + 1].second.end());
      bags[i + 1].emplace_back(nf, std::move(nsym));
    }
  }
  std::sort(bags[kMaxHuffmanBits - 1].begin(), bags[kMaxHuffmanBits - 1].end());

  // In the groups of symbols for the highest bit length we need to select the
  // last 2*num_symbols-2 groups, and assign to each symbol one bit of cost for
  // each of its occurrences in these groups.
  for (size_t i = 0; i < 2 * nzsym - 2; i++) {
    const auto& b = bags[kMaxHuffmanBits - 1][i];
    for (const uint8_t x : b.second) {
      info[x].nbits++;
    }
  }

  // In a properly-constructed set of lengths for a set of symbols, the sum
  // across the symbols of 2^-sym_length equals 1.
  size_t cost_check = 0;
  for (size_t i = 0; i < kNumSymbols; i++) {
    if (info[i].present == 0) continue;
    cost_check += 1U << (kMaxHuffmanBits - info[i].nbits);
  }
  ZKR_ASSERT(cost_check == 1 << kMaxHuffmanBits);
}

}  // namespace

std::vector<size_t> HuffmanEncode(
    const IntegerData& integers, size_t num_contexts, BitWriter* writer,
    const std::vector<size_t>& node_degree_indices) {
  std::vector<size_t> node_degree_bit_pos;
  node_degree_bit_pos.reserve(node_degree_indices.size());
  size_t current_node = 0;

  // Compute histograms.
  std::vector<std::vector<size_t>> histograms;
  histograms.resize(num_contexts);
  integers.Histograms(&histograms);

  writer->Reserve(num_contexts * kNumSymbols * 4);

  // Compute and encode symbol length and bits for each symbol.
  ZKR_ASSERT(histograms.size() == num_contexts);
  HuffmanSymbolInfo info[kMaxNumContexts][kNumSymbols] = {};
  for (size_t i = 0; i < histograms.size(); i++) {
    ComputeSymbolNumBits(histograms[i], &info[i][0]);
    ZKR_ASSERT(ComputeSymbolBits(&info[i][0]));
    EncodeSymbolNBits(&info[i][0], writer);
  }

  // Pre-compute the number of bits needed.
  size_t nbits_histo = writer->NumBitsWritten();
  size_t total_nbits = 0;
  integers.ForEach([&](size_t ctx, size_t token, size_t nextrabits,
                       size_t extrabits, size_t i) {
    if (current_node < node_degree_indices.size() &&
        i == node_degree_indices[current_node]) {
      node_degree_bit_pos.push_back(total_nbits + nbits_histo);
      ++current_node;
    }
    total_nbits += info[ctx][token].nbits;
    total_nbits += nextrabits;
  });

  writer->Reserve(total_nbits);

  // Encode the actual data.
  integers.ForEach([&](size_t ctx, size_t token, size_t nextrabits,
                       size_t extrabits, size_t i) {
    writer->Write(info[ctx][token].nbits, info[ctx][token].bits);
    writer->Write(nextrabits, extrabits);
  });

  return node_degree_bit_pos;
}

bool HuffmanReader::Init(size_t num_contexts, BitReader* ZKR_RESTRICT br) {
  ZKR_ASSERT(num_contexts <= kMaxNumContexts);
  for (size_t i = 0; i < num_contexts; i++) {
    HuffmanSymbolInfo symbol_info[kNumSymbols] = {};
    DecodeSymbolNBits(&symbol_info[0], br);
    ZKR_RETURN_IF_ERROR(ComputeSymbolBits(&symbol_info[0]));
    ZKR_RETURN_IF_ERROR(ComputeDecoderTable(&symbol_info[0], &info_[i][0]));
  }
  return true;
}

size_t HuffmanReader::Read(size_t ctx, BitReader* ZKR_RESTRICT br) {
  const uint32_t bits = br->PeekBits(kMaxHuffmanBits);
  br->Advance(info_[ctx][bits].nbits);
  return info_[ctx][bits].symbol;
}
}  // namespace zuckerli
