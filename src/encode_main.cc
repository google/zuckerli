#include <string.h>

#include "encode.h"
#include "uncompressed_graph.h"

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "Usage: %s in.bin out.hc [--allow_random_access]\n",
            argv[0]);
    return 1;
  }
  FILE *out = fopen(argv[2], "w");
  if (out == nullptr) {
    fprintf(stderr, "Invalid output file %s\n", argv[2]);
  }

  bool allow_random_access = false;
  if (argc == 4 && strcmp("--allow_random_access", argv[3]) == 0) {
    allow_random_access = true;
  }

  zuckerli::UncompressedGraph g(argv[1]);
  auto data = zuckerli::EncodeGraph(g, allow_random_access);
  fwrite(data.data(), 1, data.size(), out);
  fclose(out);
}
