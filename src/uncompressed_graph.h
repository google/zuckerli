// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef ZUCKERLI_UNCOMPRESSED_GRAPH_H
#define ZUCKERLI_UNCOMPRESSED_GRAPH_H

#include <stdint.h>
#include <stdlib.h>

#include <string>

#include "common.h"

namespace zuckerli {

template <typename T>
class span {
 public:
  using iterator = const T *;
  span(const T *data, size_t size) : data_(data), size_(size) {}
  iterator begin() const { return data_; }
  iterator end() const { return data_ + size_; }
  const T &operator[](size_t pos) const { return data_[pos]; }
  ZKR_INLINE T &at(size_t pos) {
    ZKR_DASSERT(pos < size_);
    return data_[pos];
  }
  ZKR_INLINE const T &at(size_t pos) const {
    ZKR_DASSERT(pos < size_);
    return data_[pos];
  }
  ZKR_INLINE size_t size() const { return size_; }

 private:
  const T *data_;
  size_t size_;
};

class MemoryMappedFile {
 public:
  MemoryMappedFile(const std::string &filename);
  ~MemoryMappedFile();
  MemoryMappedFile(const MemoryMappedFile &) = delete;
  void operator=(const MemoryMappedFile &) = delete;
  MemoryMappedFile(MemoryMappedFile &&) = default;
  MemoryMappedFile &operator=(MemoryMappedFile &&) = default;
  ZKR_INLINE const uint32_t *data() const { return data_; }
  ZKR_INLINE size_t size() const { return size_; }

 private:
  size_t size_;
  const uint32_t *ZKR_RESTRICT data_;
  int fd_;
};

// Simple on-disk representation of a graph that can directly mapped into memory
// (allowing reduced memory usage).
// Format description:
// - 8 bytes of fingerprint
// - 4 bytes to represent the number of nodes N
// - N+1 8-byte integers that represent the index of the first edge of the i-th
//   adjacency list. The last of these integers is the total number of edges, M.
// - M 4-byte integers that represent the destination node of each graph edge.
class UncompressedGraph {
 public:
  // Fingerprint of the simple uncompressed graph format: number of bytes to
  // represent the number of edges followed by number of bytes to represent the
  // number of nodes.
  static constexpr uint64_t kFingerprint =
      (sizeof(uint64_t) << 4) | sizeof(uint32_t);
  UncompressedGraph(const std::string &file);
  ZKR_INLINE uint32_t size() const { return N; }
  ZKR_INLINE uint32_t Degree(size_t i) const {
    ZKR_DASSERT(i < size());
    return uint32_t(neigh_start_[i + 1] - neigh_start_[i]);
  }
  ZKR_INLINE span<const uint32_t> Neighbours(size_t i) const {
    return span<const uint32_t>(neighs_ + neigh_start_[i], Degree(i));
  }

 private:
  MemoryMappedFile f_;
  uint32_t N;
  const uint64_t *ZKR_RESTRICT neigh_start_;
  const uint32_t *ZKR_RESTRICT neighs_;
};

}  // namespace zuckerli
#endif  // ZUCKERLI_UNCOMPRESSED_GRAPH_H
