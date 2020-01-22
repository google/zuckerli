#ifndef ZUCKERLI_ENCODE_H
#define ZUCKERLI_ENCODE_H
#include <vector>

#include "uncompressed_graph.h"

namespace zuckerli {
std::vector<uint8_t> EncodeGraph(const UncompressedGraph& g,
                                 bool allow_random_access,
                                 size_t* checksum = nullptr);
}

#endif  // ZUCKERLI_ENCODE_H
