#ifndef ZUCKERLI_BIT_WRITER_H
#define ZUCKERLI_BIT_WRITER_H
#include <stdint.h>
#include <vector>

// Simple bit writer that can handle up to 56 bits per call. Inspired by JPEG
// XL's bit writer. Simple implementation that can only handle little endian
// systems.
class BitWriter {
public:
  static constexpr size_t kMaxBitsPerCall = 56;

  void Write(size_t nbits, size_t bits);

  // Required before calls to write.
  void Reserve(size_t nbits);

  void AppendAligned(const uint8_t *ptr, size_t cnt);

  void ZeroPad() { bits_written_ = (bits_written_ + 7) / 8 * 8; }

  std::vector<uint8_t> GetData() &&;

private:
  std::vector<uint8_t> data_;
  size_t bits_written_ = 0;
};

#endif // ZUCKERLI_BIT_WRITER_H
