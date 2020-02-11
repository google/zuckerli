#include <cstdio>

#include "common.h"
#include "decode.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(std::string, input_path, "", "Input file path");

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  FILE* in = fopen(absl::GetFlag(FLAGS_input_path).c_str(), "r");
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
