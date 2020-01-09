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
#include "uncompressed_graph.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

namespace zuckerli {

MemoryMappedFile::MemoryMappedFile(const std::string &filename) {
  struct stat st;
  int ret = stat(filename.c_str(), &st);
  ZKR_ASSERT(ret == 0);
  size_ = st.st_size;
  ZKR_ASSERT(size_ % sizeof(uint32_t) == 0);
  size_ /= sizeof(uint32_t);
  fd_ = open(filename.c_str(), O_RDONLY, 0);
  auto flags = MAP_SHARED;
  flags |= MAP_POPULATE;
  data_ = (const uint32_t *)mmap(NULL, size_ * sizeof(uint32_t), PROT_READ,
                                 flags, fd_, 0);
  ZKR_ASSERT(data_ != MAP_FAILED);
}

MemoryMappedFile::~MemoryMappedFile() {
  munmap((void *)data_, size_ * sizeof(uint32_t));
  close(fd_);
}

UncompressedGraph::UncompressedGraph(const std::string &file) : f_(file) {
  const uint32_t *data = f_.data();
  if (kFingerprint != *(uint64_t *)data) {
    fprintf(stderr, "ERROR: invalid fingerprint\n");
    exit(1);
  }
  N = data[2];
  neigh_start_ = (uint64_t *)(data + 3);
  neighs_ = data + 2 * (N + 1) + 3;
}

}  // namespace zuckerli
