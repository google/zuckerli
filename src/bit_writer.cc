#include "bit_writer.h"

#include <string.h>

#include "common.h"

void BitWriter::Write(size_t nbits, size_t bits) {
  ZKR_DASSERT(nbits >= 0);
  ZKR_DASSERT(bits >> nbits == 0);
  ZKR_DASSERT(nbits < kMaxBitsPerCall);

  uint8_t *ptr = &data_[bits_written_ / 8];
  size_t used_bits = bits_written_ % 8;
  bits <<= used_bits;
  bits |= *ptr;
  memcpy(ptr, &bits, sizeof(bits));
  bits_written_ += nbits;
}

void BitWriter::AppendAligned(const uint8_t *ptr, size_t cnt) {
  ZKR_ASSERT(bits_written_ % 8 == 0);
  data_.resize(bits_written_ / 8);
  data_.insert(data_.end(), ptr, ptr + cnt);
}

std::vector<uint8_t> BitWriter::GetData() && {
  data_.resize((bits_written_ + 7) / 8);
  return std::move(data_);
}

void BitWriter::Reserve(size_t nbits) {
  // Add padding to ensure memcpy does not write out of bounds.
  size_t required_size = (bits_written_ + nbits + 7) / 8 + sizeof(size_t);
  if (required_size > data_.size()) {
    data_.resize(required_size);
  }
}
