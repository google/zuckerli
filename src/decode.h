#ifndef ZUCKERLI_DECODE_H
#define ZUCKERLI_DECODE_H
#include <chrono>
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
namespace detail {

template <typename Reader, typename CB>
bool DecodeGraphImpl(size_t N, bool allow_random_access, Reader* reader,
                     BitReader* br, const CB& cb,
                     std::vector<size_t>* node_start_indices) {
  using IntegerCoder = zuckerli::IntegerCoder;
  // Storage for the previous up-to-MaxNodesBackwards() lists to be used as a
  // reference.
  std::vector<std::vector<uint32_t>> prev_lists(
      std::min(MaxNodesBackwards(), N));
  std::vector<uint32_t> residuals;
  std::vector<uint32_t> block_lengths;
  for (size_t i = 0; i < prev_lists.size(); i++) prev_lists[i].clear();
  size_t rle_min =
      allow_random_access ? kRleMin : std::numeric_limits<size_t>::max();
  // The three quantities below get reset to after kDegreeReferenceChunkSize
  // adjacency lists if in random-access mode.
  //
  // Reference degree for degree delta coding.
  size_t last_degree = 0;
  // Last degree delta for context modeling.
  size_t last_degree_delta = 0;
  // Last reference offset for context modeling.
  size_t last_reference_offset = 0;
  for (size_t current_node = 0; current_node < N; current_node++) {
    size_t i_mod = current_node % MaxNodesBackwards();
    prev_lists[i_mod].clear();
    block_lengths.clear();
    size_t degree;
    if (node_start_indices) node_start_indices->push_back(br->NumBitsRead());
    if ((allow_random_access &&
         current_node % kDegreeReferenceChunkSize == 0) ||
        current_node == 0) {
      degree = IntegerCoder::Read(kFirstDegreeContext, br, reader);
      last_degree_delta =
          degree;  // special case: we assume a node -1 with degree 0
      last_reference_offset = 0;
    } else {
      size_t ctx = DegreeContext(last_degree_delta);
      last_degree_delta = IntegerCoder::Read(ctx, br, reader);
      degree =
          last_degree +
          UnpackSigned(
              last_degree_delta);  // this can be negative, hence calling this
    }
    last_degree = degree;
    if (degree > N) return ZKR_FAILURE("Invalid degree");
    if (degree == 0) continue;

    // If this is not the first node, read the offset of the list to be used as
    // a reference.
    size_t reference_offset = 0;
    if (current_node != 0) {
      reference_offset = IntegerCoder::Read(
          ReferenceContext(last_reference_offset), br, reader);
      last_reference_offset = reference_offset;
    }
    if (reference_offset > current_node)
      return ZKR_FAILURE("Invalid reference_offset");

    // If a reference_offset is used, read the list of blocks of (alternating)
    // copied and skipped edges.
    size_t num_to_copy = 0;
    if (reference_offset != 0) {
      size_t block_count = IntegerCoder::Read(kBlockCountContext, br, reader);
      size_t block_end = 0;  // end of current block
      for (size_t j = 0; j < block_count; j++) {
        size_t ctx = j == 0
                         ? kBlockContext
                         : (j % 2 == 0 ? kBlockContextEven : kBlockContextOdd);
        size_t block_len;
        if (j == 0) {
          block_len = IntegerCoder::Read(ctx, br, reader);
        } else {
          block_len = IntegerCoder::Read(ctx, br, reader) + 1;
        }
        block_end += block_len;
        block_lengths.push_back(block_len);
      }
      if (prev_lists[(current_node - reference_offset) % MaxNodesBackwards()]
              .size() < block_end) {
        return ZKR_FAILURE("Invalid block copy pattern");
      }
      // Last block is implicit and goes to the end of the reference list.
      block_lengths.push_back(
          prev_lists[(current_node - reference_offset) % MaxNodesBackwards()]
              .size() -
          block_end);
      // Blocks in even positions are to be copied.
      for (size_t i = 0; i < block_lengths.size(); i += 2) {
        num_to_copy += block_lengths[i];
      }
    }

    // Read all the edges that are not copied.

    // reference_offset node for delta-coding of neighbours.
    size_t last_dest_plus_one = 0;  // will not be used
    // Number of edges to read.
    size_t num_residuals = degree - num_to_copy;
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
    // ID of reference list.
    size_t ref_id = (current_node - reference_offset) % MaxNodesBackwards();
    // Number of consecutive zeros that have been decoded last.
    // Delta encoding with -1.
    size_t contiguous_zeroes_len = 0;
    // Number of further zeros that should not be read from the bitstream.
    size_t num_zeros_to_skip = 0;
    const auto append = [&](size_t x) {
      if (x >= N) return ZKR_FAILURE("Invalid residual");
      prev_lists[i_mod].push_back(x);
      cb(current_node, x);
      return true;
    };
    for (size_t j = 0; j < num_residuals; j++) {
      size_t destination_node;
      if (j == 0) {
        last_residual_delta =
            IntegerCoder::Read(FirstResidualContext(num_residuals), br, reader);
        destination_node = current_node + UnpackSigned(last_residual_delta);
      } else if (num_zeros_to_skip >
                 0) {  // If in a zero run, don't read anything.
        last_residual_delta = 0;
        destination_node = last_dest_plus_one;
      } else {
        last_residual_delta = IntegerCoder::Read(
            ResidualContext(last_residual_delta), br, reader);
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
             prev_lists[ref_id][ref_pos] <= destination_node) {
        num_to_copy_from_current_block--;
        ZKR_RETURN_IF_ERROR(append(prev_lists[ref_id][ref_pos]));
        // If our delta coding would produce an edge to destination_node, but y
        // with y<=destination_node is copied from the reference_offset list, we
        // increase destination_node. In other words, it's delta coding with
        // respect to both lists (prev_lists and residuals).
        if (j != 0 && prev_lists[ref_id][ref_pos] >= last_dest_plus_one) {
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
      if (contiguous_zeroes_len >= rle_min) {
        num_zeros_to_skip = IntegerCoder::Read(kRleContext, br, reader);
        contiguous_zeroes_len = 0;
      }

      ZKR_RETURN_IF_ERROR(append(destination_node));
      last_dest_plus_one = destination_node + 1;
    }
    ZKR_ASSERT(ref_pos + num_to_copy_from_current_block <=
               prev_lists[ref_id].size());
    // Process the rest of the block-copy list.
    while (num_to_copy_from_current_block > 0) {
      num_to_copy_from_current_block--;
      ZKR_RETURN_IF_ERROR(append(prev_lists[ref_id][ref_pos]));
      ref_pos++;
      if (num_to_copy_from_current_block == 0 &&
          next_block + 1 < block_lengths.size()) {
        ref_pos += block_lengths[next_block];
        num_to_copy_from_current_block = block_lengths[next_block + 1];
        next_block += 2;
      }
    }
  }
  if (!reader->CheckFinalState()) {
    return ZKR_FAILURE("Invalid stream");
  }
  return true;
}

}  // namespace detail

bool DecodeGraph(const std::vector<uint8_t>& compressed,
                 size_t* checksum = nullptr,
                 std::vector<size_t>* node_start_indices = nullptr) {
  if (compressed.empty()) return ZKR_FAILURE("Empty file");
  auto start = std::chrono::high_resolution_clock::now();
  BitReader reader(compressed.data(), compressed.size());
  size_t N = reader.ReadBits(48);
  bool allow_random_access = reader.ReadBits(1);
  size_t edges = 0, chksum = 0;
  auto edge_callback = [&](size_t a, size_t b) {
    edges++;
    chksum = Checksum(chksum, a, b);
  };
  if (allow_random_access) {
    HuffmanReader huff_reader;
    huff_reader.Init(kNumContexts, &reader);
    ZKR_RETURN_IF_ERROR(
        detail::DecodeGraphImpl(N, allow_random_access, &huff_reader, &reader,
                                edge_callback, node_start_indices));
  } else {
    ANSReader ans_reader;
    ans_reader.Init(kNumContexts, &reader);
    ZKR_RETURN_IF_ERROR(
        detail::DecodeGraphImpl(N, allow_random_access, &ans_reader, &reader,
                                edge_callback, node_start_indices));
  }
  auto stop = std::chrono::high_resolution_clock::now();

  float elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
          .count();

  fprintf(stderr, "Decompressed %.2f ME/s (%zu) from %.2f BPE. Checksum: %lx\n",
          edges / elapsed, edges, 8.0 * compressed.size() / edges, chksum);
  if (checksum) *checksum = chksum;
  return true;
}
}  // namespace zuckerli

#endif  // ZUCKERLI_DECODE_H
