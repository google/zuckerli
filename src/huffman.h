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
#ifndef ZUCKERLI_HUFFMAN_H
#define ZUCKERLI_HUFFMAN_H

#include "bit_writer.h"
#include "integer_coder.h"

namespace zuckerli {

static constexpr size_t kMaxHuffmanBits = 8;

struct HuffmanDecoderInfo {
  uint8_t nbits;
  uint8_t symbol;
};

// Encodes the given sequence of integers into a BitWriter. The context id
// for each integer must be in the range [0, num_contexts).
void HuffmanEncode(const IntegerData& integers, size_t num_contexts,
                   BitWriter* writer);

// Class to read Huffman-encoded symbols from a stream.
class HuffmanReader {
 public:
  // Decodes the specified number of distributions from the reader and creates
  // the corresponding decoding tables.
  bool Init(size_t num_contexts, BitReader* ZKR_RESTRICT br);

  // Decodes a single symbol from the bitstream, using distribution of index
  // `ctx`.
  size_t Read(size_t ctx, BitReader* ZKR_RESTRICT br);

  // For interface compatibilty with ANS reader.
  bool CheckFinalState() const { return true; }

 private:
  // For each context, maps the next kMaxHuffmanBits in the bitstream into a
  // symbol and the number of bits that should actually be consumed from the
  // bitstream.
  HuffmanDecoderInfo info_[kMaxNumContexts][1 << kMaxHuffmanBits];
};

};  // namespace zuckerli

#endif  // ZUCKERLI_HUFFMAN_H
