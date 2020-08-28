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
#include "ans.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "bit_reader.h"
#include "integer_coder.h"

namespace zuckerli {

namespace {

// Ensure that each histogram sums to 1<<kANSNumBits.
void NormalizeHistogram(std::vector<size_t>* histogram) {
  int64_t sum = std::accumulate(histogram->begin(), histogram->end(), 0);
  if (sum == 0) {
    (*histogram)[0] = 1 << kANSNumBits;
    return;
  }
  std::vector<std::pair<size_t, int>> symbols_with_freq;
  for (size_t i = 0; i < histogram->size(); i++) {
    if ((*histogram)[i] != 0) {
      symbols_with_freq.emplace_back((*histogram)[i], i);
    }
  }
  std::sort(symbols_with_freq.begin(), symbols_with_freq.end());
  for (size_t i = 0; i < symbols_with_freq.size(); i++) {
    size_t sym = symbols_with_freq[i].second;
    int64_t freq = (*histogram)[sym];
    int64_t normalized_freq = freq * (1 << kANSNumBits) / sum;
    if (normalized_freq <= 0) normalized_freq = 1;
    (*histogram)[sym] = normalized_freq;
  }

  // Adjust sum by assigning all the extra (or missing) weight to the
  // highest-weight symbol.
  int64_t new_sum = std::accumulate(histogram->begin(), histogram->end(), 0);
  (*histogram)[symbols_with_freq.back().second] += (1 << kANSNumBits) - new_sum;
  ZKR_ASSERT(std::accumulate(histogram->begin(), histogram->end(), 0) ==
             (1 << kANSNumBits));
}

// First, all trailing non-occuring symbols are removed from the distribution;
// if this leaves the distribution empty, a dummy symbol with max weight is
// added. This ensures that the resulting distribution sums to total table size.
// Then, `entry_size` is chosen to be the largest power of two so that
// `table_size` = ANS_TAB_SIZE/`entry_size` is at least as big as the
// distribution size.
// Note that each entry will only ever contain two different symbols, and
// consecutive ranges of offsets, which allows us to use a compact
// representation.
// Each entry is initialized with only the (symbol=i, offset) pairs; then
// positions for which the entry overflows (i.e. distribution[i] > entry_size)
// or is not full are computed, and put into a stack in increasing order.
// Missing symbols in the distribution are padded with 0 (because `table_size`
// >= number of symbols). The `cutoff` value for each entry is initialized to
// the number of occupied slots in that entry (i.e. `distributions[i]`). While
// the overflowing-symbol stack is not empty (which implies that the
// underflowing-symbol stack also is not), the top overfull and underfull
// positions are popped from the stack; the empty slots in the underfull entry
// are then filled with as many slots as needed from the overfull entry; such
// slots are placed after the slots in the overfull entry, and `offsets[1]` is
// computed accordingly. The formerly underfull entry is thus now neither
// underfull nor overfull, and represents exactly two symbols. The overfull
// entry might be either overfull or underfull, and is pushed into the
// corresponding stack.
void InitAliasTable(std::vector<size_t> distribution,
                    AliasTable::Entry* ZKR_RESTRICT a) {
  while (!distribution.empty() && distribution.back() == 0) {
    distribution.pop_back();
  }
  // Ensure that a valid table is always returned, even for an empty
  // alphabet. Otherwise, a specially-crafted stream might crash the
  // decoder.
  if (distribution.empty()) {
    distribution.emplace_back(1 << kANSNumBits);
  }
  const int kTableSizeLog = kLogNumSymbols;
  const int kTableSize = kNumSymbols;
  ZKR_ASSERT(distribution.size() <= (1 << kTableSizeLog));
  ZKR_ASSERT(kTableSize >= distribution.size());
  constexpr int kEntrySize = 1 << (kANSNumBits - kTableSizeLog);
  static_assert(kEntrySize <= 256,
                "kEntrySize-sized integers will be stored in uint8_t");
  std::vector<int> underfull_posn;
  std::vector<int> overfull_posn;
  size_t cutoffs[kTableSize];
  // Initialize entries.
  for (size_t i = 0; i < distribution.size(); i++) {
    cutoffs[i] = distribution[i];
    if (cutoffs[i] > kEntrySize) {
      overfull_posn.push_back(i);
    } else if (cutoffs[i] < kEntrySize) {
      underfull_posn.push_back(i);
    }
  }
  for (int i = distribution.size(); i < kTableSize; i++) {
    cutoffs[i] = 0;
    underfull_posn.push_back(i);
  }
  // Reassign overflow/underflow values.
  while (!overfull_posn.empty()) {
    int overfull_i = overfull_posn.back();
    overfull_posn.pop_back();
    ZKR_ASSERT(!underfull_posn.empty());
    int underfull_i = underfull_posn.back();
    underfull_posn.pop_back();
    int underfull_by = kEntrySize - cutoffs[underfull_i];
    cutoffs[overfull_i] -= underfull_by;
    // overfull positions have their original symbols
    a[underfull_i].right_value = overfull_i;
    a[underfull_i].offsets1 = cutoffs[overfull_i];
    // Slots in the right part of entry underfull_i were taken from the end
    // of the symbols in entry overfull_i.
    if (cutoffs[overfull_i] < kEntrySize) {
      underfull_posn.push_back(overfull_i);
    } else if (cutoffs[overfull_i] > kEntrySize) {
      overfull_posn.push_back(overfull_i);
    }
  }
  for (int i = 0; i < kTableSize; i++) {
    // cutoffs[i] is properly initialized but the clang-analyzer doesn't infer
    // it since it is partially initialized across two for-loops.
    // NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult)
    if (cutoffs[i] == kEntrySize) {
      a[i].right_value = i;
      a[i].offsets1 = 0;
      a[i].cutoff = 0;
    } else {
      // Note that, if cutoff is not equal to entry_size,
      // a[i].offsets1 was initialized with (overfull cutoff) -
      // (entry_size - a[i].cutoff). Thus, subtracting
      // a[i].cutoff cannot make it negative.
      a[i].offsets1 -= cutoffs[i];
      a[i].cutoff = cutoffs[i];
    }
    const size_t freq0 = i < distribution.size() ? distribution[i] : 0;
    const size_t i1 = a[i].right_value;
    const size_t freq1 = i1 < distribution.size() ? distribution[i1] : 0;
    a[i].freq0 = static_cast<uint16_t>(freq0);
    a[i].freq1_xor_freq0 = static_cast<uint16_t>(freq1 ^ freq0);
  }
}

// Very simple encoding: for each symbol, 1 bit for presence/absence, and
// kANSNumBits bits for symbol probability if present.
void EncodeSymbolProbabilities(const std::vector<size_t>& histogram,
                               BitWriter* ZKR_RESTRICT writer) {
  for (size_t i = 0; i < kNumSymbols; i++) {
    if (histogram.size() > i && histogram[i] != 0) {
      writer->Write(1, 1);
      writer->Write(kANSNumBits, histogram[i] - 1);
    } else {
      writer->Write(1, 0);
    }
  }
}

void DecodeSymbolProbabilities(std::vector<size_t>* histogram,
                               BitReader* ZKR_RESTRICT reader) {
  histogram->resize(kNumSymbols);
  for (size_t i = 0; i < kNumSymbols; i++) {
    if (reader->ReadBits(1)) {
      (*histogram)[i] = reader->ReadBits(kANSNumBits) + 1;
    } else {
      (*histogram)[i] = 0;
    }
  }
}

// precision must be equal to:  #bits(state_) + #bits(freq)
size_t kReciprocalPrecision = 32 + kANSNumBits;

struct ANSEncSymbolInfo {
  uint16_t freq;
  std::vector<uint16_t> reverse_map;
  // Value such that (state_ * ifreq) >> kReciprocalPrecision == state_ / freq.
  uint64_t ifreq;
};

}  // namespace

void ANSEncode(const IntegerData& integers, size_t num_contexts,
               BitWriter* writer, std::vector<double>* bits_per_ctx) {
  // Compute histograms.
  std::vector<std::vector<size_t>> histograms;
  histograms.resize(num_contexts);
  integers.Histograms(&histograms);

  writer->Reserve(num_contexts * kNumSymbols * (1 + kANSNumBits));
  bits_per_ctx->resize(num_contexts);

  // Normalize and encode histograms and compute alias tables.
  ZKR_ASSERT(histograms.size() == num_contexts);
  ANSEncSymbolInfo enc_symbol_info[kMaxNumContexts][kNumSymbols] = {};
  for (size_t i = 0; i < histograms.size(); i++) {
    AliasTable::Entry entries[1 << kANSNumBits] = {};
    // Ensure consistent size on decoder and encoder side.
    histograms[i].resize(kNumSymbols);
    NormalizeHistogram(&histograms[i]);
    EncodeSymbolProbabilities(histograms[i], writer);
    InitAliasTable(histograms[i], &entries[0]);

    // Compute encoding information.
    for (size_t sym = 0; sym < std::max<size_t>(histograms[i].size(), 1);
         sym++) {
      size_t freq =
          histograms[i].empty() ? (1 << kANSNumBits) : histograms[i][sym];
      enc_symbol_info[i][sym].freq = freq;
      if (freq != 0) {
        enc_symbol_info[i][sym].ifreq =
            ((1ull << kReciprocalPrecision) + freq - 1) / freq;
      }
      enc_symbol_info[i][sym].reverse_map.resize(freq);
    }
    for (size_t t = 0; t < (1 << kANSNumBits); t++) {
      AliasTable::Symbol s = AliasTable::Lookup(entries, t);
      if (s.freq == 0) continue;
      enc_symbol_info[i][s.value].reverse_map[s.offset] = t;
    }
  }

  float kProbBits[(1 << kANSNumBits) + 1];
  for (size_t i = 1; i <= (1 << kANSNumBits); i++) {
    kProbBits[i] = -std::log2(i * (1.0f / (1 << kANSNumBits)));
  }

  // The decoder should output ans_output_bits[i] when reaching index
  // output_idx[i].
  std::vector<uint16_t> ans_output_bits;
  std::vector<size_t> output_idx;

  size_t extra_bits = 0;

  size_t ans_state = kANSSignature;

  // Iterate through tokens **in reverse order** to compute state updates.
  integers.ForEachReversed([&](size_t ctx, size_t token, size_t nbits,
                               size_t bits, size_t i) {
    (*bits_per_ctx)[ctx] += kProbBits[enc_symbol_info[ctx][token].freq] + nbits;
    extra_bits += nbits;
    const ANSEncSymbolInfo& info = enc_symbol_info[ctx][token];
    // Flush state.
    if ((ans_state >> (32 - kANSNumBits)) >= info.freq) {
      ans_output_bits.push_back(ans_state & 0xFFFF);
      output_idx.push_back(i);
      ans_state >>= 16;
    }
    uint32_t v = (ans_state * info.ifreq) >> kReciprocalPrecision;
    uint32_t offset = info.reverse_map[ans_state - v * info.freq];
    ans_state = (v << kANSNumBits) + offset;
  });

  writer->Reserve(extra_bits + ans_output_bits.size() * 16 + 32);
  writer->Write(32, ans_state);

  size_t output_idx_pos = output_idx.size();
  // Iterate through tokens in forward order to produce output.
  integers.ForEach(
      [&](size_t ctx, size_t token, size_t nbits, size_t bits, size_t i) {
        if (output_idx_pos > 0 && i == output_idx[output_idx_pos - 1]) {
          writer->Write(16, ans_output_bits[output_idx_pos - 1]);
          output_idx_pos--;
        }
        writer->Write(nbits, bits);
      });
}

AliasTable::Symbol AliasTable::Lookup(const Entry* ZKR_RESTRICT table,
                                      size_t value) {
  const size_t i = value >> kLogEntrySize;
  const size_t pos = value & kEntrySizeMinus1;

  uint64_t entry;
  memcpy(&entry, &table[i].cutoff, sizeof(entry));
  const size_t cutoff = entry & 0xFF;              // = MOVZX
  const size_t right_value = (entry >> 8) & 0xFF;  // = MOVZX
  const size_t freq0 = (entry >> 16) & 0xFFFF;

  const bool greater = pos >= cutoff;

  const uint64_t conditional = greater ? entry : 0;  // = CMOV
  const size_t offsets1_or_0 = (conditional >> 32) & 0xFFFF;
  const size_t freq1_xor_freq0_or_0 = conditional >> 48;

  // WARNING: moving this code may interfere with CMOV heuristics.
  Symbol s;
  s.value = greater ? right_value : i;
  s.offset = offsets1_or_0 + pos;
  s.freq = freq0 ^ freq1_xor_freq0_or_0;  // = greater ? freq1 : freq0
  // XOR avoids implementation-defined conversion from unsigned to signed.
  // Alternatives considered: BEXTR is 2 cycles on HSW, SET+shift causes
  // spills, simple ternary has a long dependency chain.

  return s;
}

bool ANSReader::Init(size_t num_contexts, BitReader* ZKR_RESTRICT br) {
  ZKR_ASSERT(num_contexts <= kMaxNumContexts);
  std::vector<size_t> histogram;
  for (size_t i = 0; i < num_contexts; i++) {
    DecodeSymbolProbabilities(&histogram, br);
    size_t total_probability =
        std::accumulate(histogram.begin(), histogram.end(), 0);
    if (total_probability != 0 && total_probability != 1 << kANSNumBits) {
      return ZKR_FAILURE("Invalid histogram");
    }
    InitAliasTable(histogram, &entries_[i][0]);
  }
  state_ = br->ReadBits(32);
  return true;
}

size_t ANSReader::Read(size_t ctx, BitReader* reader) {
  const uint32_t res = state_ & ((1 << kANSNumBits) - 1);
  const AliasTable::Entry* table = &entries_[ctx][0];
  const AliasTable::Symbol symbol = AliasTable::Lookup(table, res);
  state_ = symbol.freq * (state_ >> kANSNumBits) + symbol.offset;
  const uint32_t new_state =
      (state_ << 16u) | static_cast<uint32_t>(reader->PeekBits(16));
  const bool normalize = state_ < (1u << 16u);
  state_ = normalize ? new_state : state_;
  reader->Advance(normalize ? 16 : 0);
  if (state_ < (1u << 16u)) {
    state_ = (state_ << 16u) | reader->PeekBits(16);
    reader->Advance(16);
  }
  return symbol.value;
}

}  // namespace zuckerli
