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
#ifndef ZUCKERLI_BIT_READER_H
#define ZUCKERLI_BIT_READER_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

namespace zuckerli {

// Simple bit reader (based on JPEG XL's bit reader and variant 4 of
// fgiesen.wordpress.com/2018/02/20/reading-bits-in-far-too-many-ways-part-2/)
// that can handle 56 bits per call. Simple implementation that only handles
// little endian systems.
// TODO: does not handle reads past the end of the stream (assumed to be
// 0 bits).
class BitReader {
public:
  static constexpr size_t kMaxBitsPerCall = 56;

  BitReader(const uint8_t *ZKR_RESTRICT data, size_t size);
  BitReader(const uint8_t *ZKR_RESTRICT data, size_t bit_offset, size_t size);

  BitReader(const BitReader &other) = delete;
  BitReader(BitReader &&other) = delete;
  BitReader &operator=(const BitReader &other) = delete;
  BitReader &operator=(BitReader &&other) = delete;

  uint64_t PeekBits(size_t nbits) { return buf_ & ((1 << nbits) - 1); }

  ZKR_INLINE uint64_t ReadBits(size_t nbits) {
    Refill();
    const uint64_t bits = PeekBits(nbits);
    Advance(nbits);
    return bits;
  }

  ZKR_INLINE void Refill() {
    if (next_byte_ > end_minus_8_) {
      BoundsCheckedRefill();
    } else {
      uint64_t bits;
      memcpy(&bits, next_byte_, sizeof(bits));
      buf_ |= bits << bits_in_buf_;
      next_byte_ += (63 - bits_in_buf_) >> 3;
      bits_in_buf_ |= 56;
    }
  }

  ZKR_INLINE void Advance(size_t nbits) {
    bits_in_buf_ -= nbits;
    buf_ >>= nbits;
  }

private:
  uint64_t buf_{0};
  size_t bits_in_buf_{0};
  const uint8_t *ZKR_RESTRICT next_byte_;
  const uint8_t *ZKR_RESTRICT end_minus_8_;
  void BoundsCheckedRefill();
};

} // namespace zuckerli

#endif // ZUCKERLI_BIT_READER_H
