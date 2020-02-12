#include <string.h>

#include "encode.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "uncompressed_graph.h"

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "", "Output file path");

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  FILE* out = fopen(absl::GetFlag(FLAGS_output_path).c_str(), "w");
  if (out == nullptr) {
    fprintf(stderr, "Invalid output file %s\n", argv[2]);
  }

  zuckerli::UncompressedGraph g(absl::GetFlag(FLAGS_input_path));
  auto data =
      zuckerli::EncodeGraph(g, absl::GetFlag(FLAGS_allow_random_access));
  fwrite(data.data(), 1, data.size(), out);
  fclose(out);
}
