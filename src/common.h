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
#ifndef ZUCKERLI_COMMON_H
#define ZUCKERLI_COMMON_H

#include <stdint.h>

#define ZKR_ASSERT(cond)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ::zuckerli::Abort(__FILE__, __LINE__, "Assertion failure: %s\n", #cond); \
    }                                                                          \
  } while (0)

// Enable DASSERTs in debug build, and if specifically requested.
#ifndef NDEBUG
#define ZKR_ENABLE_DEBUG
#endif

#ifdef ZKR_ENABLE_DEBUG
#define ZKR_DASSERT(cond)
#else
#define ZKR_DASSERT(cond) ZKR_ASSERT(cond)
#endif

#define ZKR_ABORT(...) ::zuckerli::Abort(__FILE__, __LINE__, __VA_ARGS__)

#ifdef ZKR_CRASH_ON_ERROR
#define ZKR_FAILURE(...) \
  ::zuckerli::Abort(__FILE__, __LINE__, __VA_ARGS__), false
#else
#define ZKR_FAILURE(...) false
#endif

#define ZKR_RESTRICT __restrict__

#define ZKR_INLINE inline __attribute__((always_inline))

#define ZKR_RETURN_IF_ERROR(cond) \
  if (!(cond)) return false;

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "bit_writer and reader assumes a little endian system"
#endif

namespace zuckerli {
__attribute__((noreturn, __format__(__printf__, 3, 4))) void Abort(
    const char *file, int line, const char *format, ...);

template <typename T, typename U>
constexpr ZKR_INLINE T DivCeil(T a, U b) {
  return (a + b - 1) / b;
}

ZKR_INLINE int FloorLog2Nonzero(uint64_t value) {
  return 63 - __builtin_clzll(value);
}

}  // namespace zuckerli

#endif  // ZUCKERLI_COMMON_H
