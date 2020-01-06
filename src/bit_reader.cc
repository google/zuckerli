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
#include "bit_reader.h"

namespace zuckerli {
BitReader::BitReader(const uint8_t *ZKR_RESTRICT data, size_t size)
    : next_byte_(data), end_minus_8_(data + size - 8) {}

BitReader::BitReader(const uint8_t *ZKR_RESTRICT data, size_t bit_offset,
                     size_t size)
    : BitReader(data + bit_offset / 8, size - bit_offset / 8) {
  ZKR_ASSERT(bit_offset <= size * 8);
  Refill();
  Advance(bit_offset % 8);
}

void BitReader::BoundsCheckedRefill() {
  const uint8_t *end = end_minus_8_ + 8;
  for (; bits_in_buf_ < 56; bits_in_buf_ += 8) {
    if (next_byte_ >= end)
      break;
    buf_ |= static_cast<uint64_t>(*next_byte_++) << bits_in_buf_;
  }
  size_t extra_bytes = (63 - bits_in_buf_) / 8;
  bits_in_buf_ += extra_bytes * 8;
}
} // namespace zuckerli
