#include "common.h"
#include <stdio.h>
#include <stdlib.h>

namespace zuckerli {
__attribute__((noreturn, __format__(__printf__, 3, 4))) void
Abort(const char *file, int line, const char *format, ...) {
  //
  char buf[2000];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  fprintf(stderr, "Abort at %s:%d: %s\n", file, line, buf);
  abort();
}
} // namespace zuckerli
