#include "compressed_graph.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "common.h"
#include "context_model.h"
#include "decode.h"
#include "integer_coder.h"

namespace zuckerli {

CompressedGraph::CompressedGraph(const std::string& file) {
  FILE* in = std::fopen(file.c_str(), "r");
  ZKR_ASSERT(in);

  fseek(in, 0, SEEK_END);
  size_t len = ftell(in);
  fseek(in, 0, SEEK_SET);

  compressed_.resize(len);
  ZKR_ASSERT(fread(compressed_.data(), 1, len, in) == len);
  if (compressed_.empty()) ZKR_ABORT("Empty file");

  BitReader reader(compressed_.data(), compressed_.size());
  num_nodes_ = reader.ReadBits(48);
  bool allow_random_access = reader.ReadBits(1);
  if (!allow_random_access) {
    ZKR_ABORT("No random access allowed");
  }

  huff_reader_.Init(kNumContexts, &reader);

  node_start_indices_.clear();
  node_start_indices_.reserve(num_nodes_);
  if (!DecodeGraph(compressed_, nullptr, &node_start_indices_)) {
    ZKR_ABORT("Invalid graph");
  }
}

uint32_t CompressedGraph::ReadDegreeBits(uint32_t node_id, size_t context) {
  BitReader bit_reader(compressed_.data() + node_start_indices_[node_id] / 8,
                       compressed_.size());
  bit_reader.ReadBits(node_start_indices_[node_id] % 8);
  return zuckerli::IntegerCoder::Read(context, &bit_reader, &huff_reader_);
}

std::pair<uint32_t, size_t> CompressedGraph::ReadDegreeAndRefBits(
    uint32_t node_id, size_t context, size_t last_reference_offset) {
  BitReader bit_reader(compressed_.data() + node_start_indices_[node_id] / 8,
                       compressed_.size());
  bit_reader.ReadBits(node_start_indices_[node_id] % 8);
  uint32_t degree =
      zuckerli::IntegerCoder::Read(context, &bit_reader, &huff_reader_);
  // If this is not the first node, read the offset of the list to be used as
  // a reference.
  size_t reference_offset = 0;
  if (node_id != 0) {
    reference_offset = IntegerCoder::Read(
        ReferenceContext(last_reference_offset), &bit_reader, &huff_reader_);
  }
  return std::make_pair(degree, reference_offset);
}

uint32_t CompressedGraph::Degree(size_t node_id) {
  uint32_t first_node_in_chunk = node_id - node_id % kDegreeReferenceChunkSize;
  uint32_t reconstructed_degree =
      ReadDegreeBits(first_node_in_chunk, kFirstDegreeContext);
  size_t context;
  size_t last_degree_delta = reconstructed_degree;
  for (int node = first_node_in_chunk + 1; node <= node_id; ++node) {
    context = DegreeContext(last_degree_delta);
    last_degree_delta = ReadDegreeBits(node, context);
    reconstructed_degree += UnpackSigned(last_degree_delta);
  }
  if (reconstructed_degree > num_nodes_) ZKR_ABORT("Invalid degree");
  return reconstructed_degree;
}

std::vector<uint32_t> CompressedGraph::Neighbours(size_t node_id) {
  BitReader bit_reader(compressed_.data() + node_start_indices_[node_id] / 8,
                       compressed_.size());
  bit_reader.ReadBits(node_start_indices_[node_id] % 8);
  std::vector<uint32_t> neighbours;

  uint32_t first_node_in_chunk = node_id - node_id % kDegreeReferenceChunkSize;
  uint32_t reconstructed_degree;
  size_t reference_offset = 0;
  size_t last_reference_offset = 0;
  size_t last_degree_delta = 0;
  if (first_node_in_chunk != node_id) {
    size_t context;
    std::tie(reconstructed_degree, reference_offset) = ReadDegreeAndRefBits(
        first_node_in_chunk, kFirstDegreeContext, last_reference_offset);
    if (reconstructed_degree != 0) {
      last_reference_offset = reference_offset;
    }
    last_degree_delta = reconstructed_degree;
    for (int node = first_node_in_chunk + 1; node < node_id; ++node) {
      context = DegreeContext(last_degree_delta);
      std::tie(last_degree_delta, reference_offset) =
          ReadDegreeAndRefBits(node, context, last_reference_offset);
      reconstructed_degree += UnpackSigned(last_degree_delta);
      if (reconstructed_degree != 0) {
        last_reference_offset = reference_offset;
      }
    }
    context = DegreeContext(last_degree_delta);
    last_degree_delta = IntegerCoder::Read(context, &bit_reader, &huff_reader_);
    reconstructed_degree += UnpackSigned(last_degree_delta);
  } else {
    reconstructed_degree =
        IntegerCoder::Read(kFirstDegreeContext, &bit_reader, &huff_reader_);
  }

  if (reconstructed_degree == 0) return {};

  if (node_id != 0) {
    reference_offset = IntegerCoder::Read(
        ReferenceContext(last_reference_offset), &bit_reader, &huff_reader_);
  }

  if (reconstructed_degree > num_nodes_) ZKR_ABORT("Invalid degree");
  if (reference_offset > node_id) ZKR_ABORT("Invalid reference_offset");

  std::vector<uint32_t> ref_list;
  std::vector<uint32_t> block_lengths;
  // If a reference_offset is used, read the list of blocks of (alternating)
  // copied and skipped edges.
  size_t num_to_copy = 0;
  if (reference_offset != 0) {
    size_t ref_id = node_id - reference_offset;
    ref_list = Neighbours(ref_id);
    size_t block_count =
        IntegerCoder::Read(kBlockCountContext, &bit_reader, &huff_reader_);
    size_t block_end = 0;  // end of current block
    for (size_t j = 0; j < block_count; j++) {
      size_t ctx = j == 0 ? kBlockContext
                          : (j % 2 == 0 ? kBlockContextEven : kBlockContextOdd);
      size_t block_len;
      if (j == 0) {
        block_len = IntegerCoder::Read(ctx, &bit_reader, &huff_reader_);
      } else {
        block_len = IntegerCoder::Read(ctx, &bit_reader, &huff_reader_) + 1;
      }
      block_end += block_len;
      block_lengths.push_back(block_len);
    }
    if (ref_list.size() < block_end) {
      ZKR_ABORT("Invalid block copy pattern");
    }
    // Last block is implicit and goes to the end of the reference list.
    block_lengths.push_back(ref_list.size() - block_end);
    // Blocks in even positions are to be copied.
    for (size_t i = 0; i < block_lengths.size(); i += 2) {
      num_to_copy += block_lengths[i];
    }
  }

  // reference_offset node for delta-coding of neighbours.
  size_t last_dest_plus_one = 0;  // will not be used
  // Number of edges to read.
  size_t num_residuals = reconstructed_degree - num_to_copy;
  // Last delta for the residual edges, used for context modeling.
  size_t last_residual_delta = 0;
  // Current position in the reference list (because we are making a sorted
  // merged list).
  size_t ref_pos = 0;
  // Number of nodes of the current block that should still be copied.
  size_t num_to_copy_from_current_block =
      block_lengths.empty() ? 0 : block_lengths[0];
  // Index of the next block.
  size_t next_block = 1;
  // If we don't need to copy anything from the first block, and we have at
  // least another even-positioned block, advance the position in the
  // reference_offset list accordingly.
  if (num_to_copy_from_current_block == 0 && block_lengths.size() > 2) {
    ref_pos = block_lengths[1];
    num_to_copy_from_current_block = block_lengths[2];
    next_block = 3;
  }

  // Number of consecutive zeros that have been decoded last.
  // Delta encoding with -1.
  size_t contiguous_zeroes_len = 0;
  // Number of further zeros that should not be read from the bitstream.
  size_t num_zeros_to_skip = 0;
  const auto append = [&](size_t destination) {
    if (destination >= num_nodes_) return ZKR_FAILURE("Invalid residual");
    neighbours.push_back(destination);
    return true;
  };
  for (size_t j = 0; j < num_residuals; j++) {
    size_t destination_node;
    if (j == 0) {
      last_residual_delta = IntegerCoder::Read(
          FirstResidualContext(num_residuals), &bit_reader, &huff_reader_);
      destination_node = node_id + UnpackSigned(last_residual_delta);
    } else if (num_zeros_to_skip >
               0) {  // If in a zero run, don't read anything.
      last_residual_delta = 0;
      destination_node = last_dest_plus_one;
    } else {
      last_residual_delta = IntegerCoder::Read(
          ResidualContext(last_residual_delta), &bit_reader, &huff_reader_);
      destination_node = last_dest_plus_one + last_residual_delta;
    }
    // Compute run of zeros if we read a zero and we are not already in one.
    if (last_residual_delta == 0 && num_zeros_to_skip == 0) {
      contiguous_zeroes_len++;
    } else {
      contiguous_zeroes_len = 0;
    }
    // If we are in a run of zeros, decrease its length.
    if (num_zeros_to_skip > 0) {
      num_zeros_to_skip--;
    }
    // Merge the edges copied from the reference_offset list with the ones
    // read from the bitstream.
    while (num_to_copy_from_current_block > 0 &&
           ref_list[ref_pos] <= destination_node) {
      num_to_copy_from_current_block--;
      if (!append(ref_list[ref_pos])) ZKR_ABORT("Invalid residual");
      // If our delta coding would produce an edge to destination_node, but y
      // with y<=destination_node is copied from the reference_offset list, we
      // increase destination_node. In other words, it's delta coding with
      // respect to both lists (ref_list and residuals).
      if (j != 0 && ref_list[ref_pos] >= last_dest_plus_one) {
        destination_node++;
      }
      ref_pos++;
      if (num_to_copy_from_current_block == 0 &&
          next_block + 1 < block_lengths.size()) {
        ref_pos += block_lengths[next_block];
        num_to_copy_from_current_block = block_lengths[next_block + 1];
        next_block += 2;
      }
    }
    // If the current run of zeros is large enough, read how many further
    // zeros to decode from the bitstream.
    if (contiguous_zeroes_len >= kRleMin) {
      num_zeros_to_skip =
          IntegerCoder::Read(kRleContext, &bit_reader, &huff_reader_);
      contiguous_zeroes_len = 0;
    }
    if (!append(destination_node)) ZKR_ABORT("Invalid residual");
    last_dest_plus_one = destination_node + 1;
  }
  ZKR_ASSERT(ref_pos + num_to_copy_from_current_block <= ref_list.size());
  // Process the rest of the block-copy list.
  while (num_to_copy_from_current_block > 0) {
    num_to_copy_from_current_block--;
    if (!append(ref_list[ref_pos])) ZKR_ABORT("Invalid residual");
    ref_pos++;
    if (num_to_copy_from_current_block == 0 &&
        next_block + 1 < block_lengths.size()) {
      ref_pos += block_lengths[next_block];
      num_to_copy_from_current_block = block_lengths[next_block + 1];
      next_block += 2;
    }
  }
  return neighbours;
}  // namespace zuckerli

}  // namespace zuckerli
