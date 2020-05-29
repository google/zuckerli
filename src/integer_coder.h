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
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bit_reader.h"
#include "common.h"
#include "absl/flags/flag.h"

ABSL_DECLARE_FLAG(int32_t, log2_num_explicit);
ABSL_DECLARE_FLAG(int32_t, num_token_bits);

namespace zuckerli {

// Only entropy-coded symbols smaller than this value are supported.
static constexpr size_t kLogNumSymbols = 8;
static constexpr size_t kNumSymbols = 1 << kLogNumSymbols;

// Only context ids smaller than this value are supported.
static constexpr size_t kMaxNumContexts = 256;

// Variable integer encoding scheme that puts bits either in an entropy-coded
// symbol or as raw bits, depending on the specified configuration.
// TODO: The behavior of IntegerCoder<0, 0> is a bit weird - both 0
// and 1 get their own symbol.
class IntegerCoder {
 public:
  static ZKR_INLINE size_t Log2NumExplicit() {
#if ZKR_HONOR_FLAGS
    return absl::GetFlag(FLAGS_log2_num_explicit);
#else
    return 4;
#endif
  }
  static ZKR_INLINE size_t NumTokenMSB() {
#if ZKR_HONOR_FLAGS
    return absl::GetFlag(FLAGS_num_token_bits);
#else
    return 2;
#endif
  }
  static ZKR_INLINE size_t NumTokenLSB() {
#if ZKR_HONOR_FLAGS
    return absl::GetFlag(FLAGS_num_token_lsb);
#else
    return 1;
#endif
  }
  static ZKR_INLINE void Encode(uint64_t value, size_t *ZKR_RESTRICT token,
                                size_t *ZKR_RESTRICT nbits,
                                size_t *ZKR_RESTRICT bits) {
    uint32_t split_exponent = Log2NumExplicit();
    uint32_t split_token = 1 << split_exponent;
    uint32_t msb_in_token = NumTokenMSB();
    uint32_t lsb_in_token = NumTokenLSB();
    ZKR_ASSERT(split_exponent >= lsb_in_token + msb_in_token);
    if (value < split_token) {
      *token = value;
      *nbits = 0;
      *bits = 0;
    } else {
      uint32_t n = FloorLog2Nonzero(value);
      uint32_t m = value - (1 << n);
      *token = split_token +
               ((n - split_exponent) << (msb_in_token + lsb_in_token)) +
               ((m >> (n - msb_in_token)) << lsb_in_token) +
               (m & ((1 << lsb_in_token) - 1));
      *nbits = n - msb_in_token - lsb_in_token;
      *bits = (value >> lsb_in_token) & ((1 << *nbits) - 1);
    }
  }
  template <typename EntropyCoder>
  static ZKR_INLINE size_t Read(size_t ctx, BitReader *ZKR_RESTRICT reader,
                                EntropyCoder *ZKR_RESTRICT entropy_coder) {
    uint32_t split_exponent = Log2NumExplicit();
    uint32_t split_token = 1 << split_exponent;
    uint32_t msb_in_token = NumTokenMSB();
    uint32_t lsb_in_token = NumTokenLSB();
    reader->Refill();
    size_t token = entropy_coder->Read(ctx, reader);
    if (token < split_token) return token;
    uint32_t nbits = split_exponent - (msb_in_token + lsb_in_token) +
                     ((token - split_token) >> (msb_in_token + lsb_in_token));
    uint32_t low = token & ((1 << lsb_in_token) - 1);
    token >>= lsb_in_token;
    const size_t bits = reader->ReadBits(nbits);
    size_t ret = (((((1 << msb_in_token) | (token & ((1 << msb_in_token) - 1)))
                    << nbits) |
                   bits)
                  << lsb_in_token) |
                 low;
    return ret;
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

class IntegerData {
 public:
  size_t Size() {
    ZKR_ASSERT(ctxs_.size() == values_.size());
    return values_.size();
  }
  void Add(uint32_t ctx, uint32_t val) {
    ZKR_DASSERT(ctx < kMaxNumContexts);
    ctxs_.push_back(ctx);
    values_.push_back(val);
  }
  void RemoveLast() {
    ZKR_DASSERT(!ctxs_.empty());
    ctxs_.pop_back();
    values_.pop_back();
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
      ZKR_ASSERT(token < kNumSymbols);
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

ZKR_INLINE uint64_t PackSigned(int64_t s) { return s < 0 ? 2 * -s - 1 : 2 * s; }

ZKR_INLINE int64_t UnpackSigned(uint64_t s) {
  return s & 1 ? -((s + 1) >> 1) : s >> 1;
}

}  // namespace zuckerli

#endif  // ZUCKERLI_INTEGER_CODER_H
