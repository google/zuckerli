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
#ifndef ZUCKERLI_INTEGER_CODER_H
#define ZUCKERLI_INTEGER_CODER_H
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "bit_reader.h"
#include "common.h"

namespace zuckerli {

// Only entropy-coded symbols smaller than this value are supported.
static constexpr size_t kLogNumSymbols = 8;
static constexpr size_t kNumSymbols = 1 << kLogNumSymbols;

// Only context ids smaller than this value are supported.
static constexpr size_t kMaxNumContexts = 256;

namespace detail {
// Variable integer encoding scheme that puts bits either in an entropy-coded
// symbol or as raw bits, depending on the specified configuration.
// TODO: The behavior of IntegerCoder<0, 0> is a bit weird - both 0
// and 1 get their own symbol.
template <size_t log2_num_explicit, size_t num_token_bits>
class IntegerCoder {
  static constexpr size_t num_explicit = 1 << log2_num_explicit;

 public:
  static_assert(num_token_bits <= 4, "At most 4 bits in the token");
  static_assert(num_explicit >= 1 << num_token_bits,
                "The first non-explicit token should have at least "
                "num_token_bits+1 bits.");
  static ZKR_INLINE void Encode(uint64_t value, size_t *ZKR_RESTRICT token,
                                size_t *ZKR_RESTRICT nbits,
                                size_t *ZKR_RESTRICT bits) {
    if (value < num_explicit) {
      *token = value;
      *nbits = 0;
      *bits = 0;
    } else {
      size_t n = FloorLog2Nonzero(value);
      size_t token_bits =
          (value >> (n - num_token_bits)) & ((1 << num_token_bits) - 1);
      *token = num_explicit + ((n - log2_num_explicit) << num_token_bits) +
               token_bits;
      ZKR_DASSERT(*token < kNumSymbols);
      *nbits = n - num_token_bits;
      *bits = value & ((1 << (n - num_token_bits)) - 1);
    }
  }
  template <typename EntropyCoder>
  static ZKR_INLINE size_t Read(size_t ctx, BitReader *ZKR_RESTRICT reader,
                                EntropyCoder *ZKR_RESTRICT entropy_coder) {
    reader->Refill();
    size_t token = entropy_coder->Read(ctx, reader);
    if (token < num_explicit) {
      return token;
    }
    size_t nbits = log2_num_explicit - num_token_bits +
                   ((token - num_explicit) >> num_token_bits);
    size_t bits = reader->ReadBits(nbits);
    size_t high_bits =
        (1 << num_token_bits) | (token & ((1 << num_token_bits) - 1));
    return (high_bits << nbits) | bits;
  }
  // sym_cost is such that position `ctx*kNumSymbols+token` holds the cost of
  // encoding `token` in the context `ctx`.
  static ZKR_INLINE float Cost(size_t ctx, uint64_t value,
                               const float *ZKR_RESTRICT sym_cost) {
    size_t token, nbits, bits;
    Encode(value, &token, &nbits, &bits);
    ZKR_DASSERT(token < kNumSymbols);
    return sym_cost[ctx * kNumSymbols + token] + nbits;
  }
  static ZKR_INLINE size_t Token(uint64_t value) {
    size_t token, nbits, bits;
    Encode(value, &token, &nbits, &bits);
    return token;
  }
};
}  // namespace detail

using IntegerCoder = detail::IntegerCoder<4, 1>;

class IntegerData {
 public:
  void Add(uint32_t ctx, uint32_t val) {
    ZKR_DASSERT(ctx < kMaxNumContexts);
    ctxs_.push_back(ctx);
    values_.push_back(val);
  }

  void TotalCost(const uint8_t *ZKR_RESTRICT ctx_group,
                 const float *ZKR_RESTRICT sym_cost,
                 float *ZKR_RESTRICT group_cost) {
    for (size_t i = 0; i < values_.size(); i++) {
      group_cost[ctx_group[ctxs_[i]]] = 0;
    }
    for (size_t i = 0; i < values_.size(); i++) {
      group_cost[ctx_group[ctxs_[i]]] +=
          IntegerCoder::Cost(ctxs_[i], values_[i], sym_cost);
    }
  }

  template <typename CB>
  void ForEach(const CB &cb) const {
    for (size_t i = 0; i < values_.size(); i++) {
      size_t token, nbits, bits;
      IntegerCoder::Encode(values_[i], &token, &nbits, &bits);
      cb(ctxs_[i], token, nbits, bits, i);
    }
  }

  template <typename CB>
  void ForEachReversed(const CB &cb) const {
    for (size_t i = values_.size(); i > 0; i--) {
      size_t token, nbits, bits;
      IntegerCoder::Encode(values_[i - 1], &token, &nbits, &bits);
      cb(ctxs_[i - 1], token, nbits, bits, i - 1);
    }
  }

  void Histograms(std::vector<std::vector<size_t>> *histo) const {
    ForEach([histo](size_t ctx, size_t token, size_t nbits, size_t bits,
                    size_t idx) {
      if (histo->size() <= ctx) {
        histo->resize(ctx + 1);
      }
      (*histo)[ctx].resize(kNumSymbols);
      (*histo)[ctx][token]++;
    });
  }

  uint32_t Context(size_t i) const { return ctxs_[i]; }
  uint32_t Value(size_t i) const { return values_[i]; }

 private:
  std::vector<uint32_t> values_;
  std::vector<uint8_t> ctxs_;
};

}  // namespace zuckerli

#endif  // ZUCKERLI_INTEGER_CODER_H
