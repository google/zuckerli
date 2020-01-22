#ifndef ZUCKERLI_CHECKSUM_H
#define ZUCKERLI_CHECKSUM_H
#include <cstdlib>

#include "common.h"

namespace zuckerli {
ZKR_INLINE size_t Checksum(size_t chk, size_t a, size_t b) {
  return chk + (a ^ b) + (((chk << 23) | (chk >> 41)) ^ ~b);
}
}  // namespace zuckerli

#endif  // ZUCKERLI_CHECKSUM_H
