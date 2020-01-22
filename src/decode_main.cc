#include <cstdio>

#include "common.h"
#include "decode.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s in.hc\n", argv[0]);
    return 0;
  }
  FILE* in = fopen(argv[1], "r");
  ZKR_ASSERT(in);

  fseek(in, 0, SEEK_END);
  size_t len = ftell(in);
  fseek(in, 0, SEEK_SET);

  std::vector<uint8_t> data(len);
  ZKR_ASSERT(fread(data.data(), 1, len, in) == len);

  if (!zuckerli::DecodeGraph(data)) {
    fprintf(stderr, "Invalid graph\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
