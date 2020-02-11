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
                     BitReader* br, const CB& cb) {
  using IntegerCoder = zuckerli::IntegerCoder;
  // Storage for the previous up-to-NumAdjLists() lists to be used as a
  // reference.
  std::vector<std::vector<uint32_t>> graph(std::min(NumAdjLists(), N));
  std::vector<uint32_t> residuals;
  std::vector<uint32_t> blocks;
  for (size_t i = 0; i < graph.size(); i++) graph[i].clear();
  size_t rle_min =
      allow_random_access ? kRleMin : std::numeric_limits<size_t>::max();
  // The three quantities below get reset to after kDegreeReferenceChunkSize
  // adjacency lists if in random-access mode.
  //
  // Reference degree for degree delta coding.
  size_t ref = 0;
  // Last degree delta for context modeling.
  size_t last_degree_delta = 0;
  // Last reference offset for context modeling.
  size_t last_reference = 0;
  for (size_t i = 0; i < N; i++) {
    size_t i_mod = i % NumAdjLists();
    graph[i_mod].clear();
    blocks.clear();
    size_t degree;
    if ((allow_random_access && i % kDegreeReferenceChunkSize == 0) || i == 0) {
      last_degree_delta = IntegerCoder::Read(kFirstDegreeContext, br, reader);
      degree = last_degree_delta;
      last_reference = 0;
    } else {
      size_t ctx = DegreeContext(last_degree_delta);
      last_degree_delta = IntegerCoder::Read(ctx, br, reader);
      degree = ref + UnpackSigned(last_degree_delta);
    }
    ref = degree;
    if (degree >= N) return ZKR_FAILURE("Invalid degree");
    if (degree == 0) continue;

    // If this is not the first node, read the offset of the list to be used as
    // a reference.
    size_t reference = 0;
    if (i != 0) {
      reference =
          IntegerCoder::Read(ReferenceContext(last_reference), br, reader);
      last_reference = reference;
    }
    if (reference > i) return ZKR_FAILURE("Invalid reference");

    // If a reference is used, read the list of blocks of (alternating) copied
    // and skipped edges.
    size_t copied = 0;
    if (reference != 0) {
      size_t block_count = IntegerCoder::Read(kBlockCountContext, br, reader);
      size_t pos = 0;
      for (size_t j = 0; j < block_count; j++) {
        size_t ctx = j == 0
                         ? kBlockContext
                         : (j % 2 == 0 ? kBlockContextEven : kBlockContextOdd);
        size_t block = IntegerCoder::Read(ctx, br, reader);
        if (j) {
          block++;
        }
        pos += block;
        blocks.push_back(block);
      }
      if (graph[(i - reference) % NumAdjLists()].size() < pos) {
        return ZKR_FAILURE("Invalid block copy pattern");
      }
      // Last block is implicit and goes to the end of the reference list.
      blocks.push_back(graph[(i - reference) % NumAdjLists()].size() - pos);
      // Blocks in even positions are to be copied.
      for (size_t i = 0; i < blocks.size(); i += 2) {
        copied += blocks[i];
      }
    }

    // Read all the edges that are not copied.

    // Reference node for delta-coding of neighbours.
    size_t ref = i;
    // Number of edges to read.
    size_t num_residuals = degree - copied;
    // Last delta for the residual edges, used for context modeling.
    size_t last_delta = 0;
    // Current position in the reference list.
    size_t ref_pos = 0;
    // Number of nodes of the current block that should still be copied.
    size_t to_copy = blocks.empty() ? 0 : blocks[0];
    // Index of the next block.
    size_t next_block = 1;
    // If we don't need to copy anything from the first block, and we have at
    // least another even-positioned block, advance the position in the
    // reference list accordingly.
    if (to_copy == 0 && blocks.size() > 2) {
      ref_pos = blocks[1];
      to_copy = blocks[2];
      next_block = 3;
    }
    // ID of reference list.
    size_t ref_id = (i - reference) % NumAdjLists();
    // Number of consecutive zeros that have been decoded last.
    size_t zero_run = 0;
    // Number of further 0s that should not be read from the bitstream.
    size_t rle_zeros = 0;
    const auto append = [&](size_t x) {
      if (x >= N) return ZKR_FAILURE("Invalid residual");
      graph[i_mod].push_back(x);
      cb(i, x);
      return true;
    };
    for (size_t j = 0; j < num_residuals; j++) {
      size_t r;
      if (j == 0) {
        last_delta =
            IntegerCoder::Read(FirstResidualContext(num_residuals), br, reader);
        r = ref + UnpackSigned(last_delta);
      } else if (rle_zeros > 0) {  // If in a zero run, don't read anything.
        last_delta = 0;
        r = ref;
      } else {
        last_delta =
            IntegerCoder::Read(ResidualContext(last_delta), br, reader);
        r = ref + last_delta;
      }
      // Compute run of zeros if we read a zero and we are not already in one.
      if (last_delta == 0 && rle_zeros == 0) {
        zero_run++;
      } else {
        zero_run = 0;
      }
      // If we are in a run of zeros, decrease its length.
      if (rle_zeros > 0) {
        rle_zeros--;
      }
      // Merge the edges copied from the reference list with the ones read from
      // the bitstream.
      while (to_copy > 0 && graph[ref_id][ref_pos] <= r) {
        to_copy--;
        ZKR_RETURN_IF_ERROR(append(graph[ref_id][ref_pos]));
        // If our delta coding would produce an edge to r, but y with y<=r is
        // copied from the reference list, we increase r.
        if (j != 0 && graph[ref_id][ref_pos] >= ref) {
          r++;
        }
        ref_pos++;
        if (to_copy == 0 && next_block + 1 < blocks.size()) {
          ref_pos += blocks[next_block];
          to_copy = blocks[next_block + 1];
          next_block += 2;
        }
      }
      // If the current run of zeros is large enough, read how many further
      // zeros to decode from the bitstream.
      if (zero_run >= rle_min) {
        rle_zeros = IntegerCoder::Read(kRleContext, br, reader);
        zero_run = 0;
      }

      ZKR_RETURN_IF_ERROR(append(r));
      ref = r + 1;
    }
    ZKR_ASSERT(ref_pos + to_copy <= graph[ref_id].size());
    // Process the rest of the block-copy list.
    while (to_copy > 0) {
      to_copy--;
      ZKR_RETURN_IF_ERROR(append(graph[ref_id][ref_pos]));
      ref_pos++;
      if (to_copy == 0 && next_block + 1 < blocks.size()) {
        ref_pos += blocks[next_block];
        to_copy = blocks[next_block + 1];
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
                 size_t* checksum = nullptr) {
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
    ZKR_RETURN_IF_ERROR(detail::DecodeGraphImpl(
        N, allow_random_access, &huff_reader, &reader, edge_callback));
  } else {
    ANSReader ans_reader;
    ans_reader.Init(kNumContexts, &reader);
    ZKR_RETURN_IF_ERROR(detail::DecodeGraphImpl(
        N, allow_random_access, &ans_reader, &reader, edge_callback));
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
