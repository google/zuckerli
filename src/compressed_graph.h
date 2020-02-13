#ifndef THIRD_PARTY_ZUCKERLI_SRC_COMPRESSED_GRAPH_H_
#define THIRD_PARTY_ZUCKERLI_SRC_COMPRESSED_GRAPH_H_

#include <chrono>
#include <iostream>
#include <limits>
#include <vector>

#include "ans.h"
#include "bit_reader.h"
#include "checksum.h"
#include "common.h"
#include "context_model.h"
#include "huffman.h"
#include "integer_coder.h"

namespace zuckerli {

class CompressedGraph {
 public:
  CompressedGraph(const std::string &file);
  ZKR_INLINE size_t size() { return num_nodes_; }
  uint32_t Degree(size_t node_id);
  std::vector<uint32_t> Neighbours(size_t node_id);

 private:
  size_t num_nodes_;
  std::vector<uint8_t> compressed_;
  std::vector<size_t> node_start_indices_;
  HuffmanReader huff_reader_;

  uint32_t ReadDegreeBits(uint32_t node_id, size_t context);
  std::pair<uint32_t, size_t> ReadDegreeAndRefBits(
      uint32_t node_id, size_t context, size_t last_reference_offset);
};

}  // namespace zuckerli

#endif  // THIRD_PARTY_ZUCKERLI_SRC_COMPRESSED_GRAPH_H_
