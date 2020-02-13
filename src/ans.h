// Copyright 2020 Google LLC
// Copyright 2019 the JPEG XL Project
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
#ifndef ZUCKERLI_ANS_H
#define ZUCKERLI_ANS_H

#include "bit_writer.h"
#include "integer_coder.h"

namespace zuckerli {

// ANS implementation adapted from JPEG XL's.

static constexpr size_t kANSNumBits = 12;
static constexpr size_t kANSSignature = 0x13 << 16;

// An alias table implements a mapping from the [0, 1<<kANSNumBits) range into
// the [0, kNumSymbols) range, satisfying the following conditions:
// - each symbol occurs as many times as specified by any valid distribution
//   of frequencies of the symbols. A valid distribution here is an array of
//   kNumSymbols that contains numbers in the range [0, 1<<kANSNumBits],
//   and whose sum is 1<<kANSNumBits.
// - lookups can be done in constant time, and also return how many smaller
//   input values map into the same symbol, according to some well-defined order
//   of input values.
// - the space used by the alias table is given by a small constant times the
//   index of the largest symbol with nonzero probability in the distribution.
// Each of the entries in the table covers a range of `entry_size` values in the
// [0, kANSNumBits) range; consecutive entries represent consecutive
// sub-ranges. In the range covered by entry `i`, the first `cutoff` values map
// to symbol `i`, while the others map to symbol `right_value`.
struct AliasTable {
  static constexpr uint32_t kLogEntrySize = kANSNumBits - kLogNumSymbols;
  static constexpr uint32_t kEntrySizeMinus1 = (1 << kLogEntrySize) - 1;
  struct Symbol {
    size_t value;
    size_t offset;
    size_t freq;
  };

// Working set size matters here (~64 tables x 256 entries).
// offsets0 is always zero (beginning of [0] side among the same symbol).
// offsets1 is an offset of (pos >= cutoff) side decremented by cutoff.
#pragma pack(push, 1)
  struct Entry {
    uint8_t cutoff;       // < kEntrySizeMinus1 when used by ANS.
    uint8_t right_value;  // < alphabet size.
    uint16_t freq0;

    // Only used if `greater` (see Lookup)
    uint16_t offsets1;         // <= ANS_TAB_SIZE
    uint16_t freq1_xor_freq0;  // for branchless ternary in Lookup
  };
#pragma pack(pop)

  // Dividing `value` by `entry_size` determines `i`, the entry which is
  // responsible for the input. If the remainder is below `cutoff`, then the
  // mapped symbol is `i`; since `offsets[0]` stores the number of occurences of
  // `i` "before" the start of this entry, the offset of the input will be
  // `offsets[0] + remainder`. If the remainder is above cutoff, the mapped
  // symbol is `right_value`; since `offsets[1]` stores the number of occurences
  // of `right_value` "before" this entry, minus the `cutoff` value, the input
  // offset is then `remainder + offsets[1]`.
  static ZKR_INLINE Symbol Lookup(const Entry* ZKR_RESTRICT table,
                                  size_t value);
};

// Encodes the given sequence of integers into a BitWriter. The context id
// for each integer must be in the range [0, num_contexts).
void ANSEncode(const IntegerData& integers, size_t num_contexts,
               BitWriter* writer, std::vector<float>* bits_per_ctx);

// Class to read ANS-encoded symbols from a stream.
class ANSReader {
 public:
  // Decodes the specified number of distributions from the reader and creates
  // the corresponding alias tables.
  bool Init(size_t num_contexts, BitReader* ZKR_RESTRICT br);

  // Decodes a single symbol from the bitstream, using distribution of index
  // `ctx`.
  size_t Read(size_t ctx, BitReader* ZKR_RESTRICT br);

  // Checks that the final state has its expected value. To be called after
  // decoding all the symbols.
  bool CheckFinalState() const { return state_ == kANSSignature; }

 private:
  // Alias tables for decoding symbols from each context.
  AliasTable::Entry entries_[kMaxNumContexts][kNumSymbols];
  uint32_t state_ = kANSSignature;
};

};  // namespace zuckerli

#endif  // ZUCKERLI_ANS_H
