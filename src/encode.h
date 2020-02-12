#ifndef ZUCKERLI_ENCODE_H
#define ZUCKERLI_ENCODE_H
#include <vector>

#include "absl/flags/flag.h"
#include "uncompressed_graph.h"

ABSL_DECLARE_FLAG(int32_t, num_rounds);
ABSL_DECLARE_FLAG(bool, allow_random_access);
ABSL_DECLARE_FLAG(bool, greedy_random_access);

namespace zuckerli {
std::vector<uint8_t> EncodeGraph(const UncompressedGraph& g,
                                 bool allow_random_access,
                                 size_t* checksum = nullptr);
}

#endif  // ZUCKERLI_ENCODE_H
