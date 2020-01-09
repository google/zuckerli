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

#include <gtest/gtest.h>

namespace zuckerli {
namespace {

TEST(UncompressedGraphTest, TestInvalidSignature) {
  EXPECT_DEATH(UncompressedGraph g(TESTDATA "/invalid_signature"),
               "invalid fingerprint");
}

TEST(UncompressedGraphTest, TestSmallGraph) {
  UncompressedGraph g(TESTDATA "/small");

  ASSERT_EQ(g.size(), 3);

  ASSERT_EQ(g.degree(0), 2);
  ASSERT_EQ(g.degree(1), 2);
  ASSERT_EQ(g.degree(2), 1);

  EXPECT_EQ(g.neighs(0)[0], 0);
  EXPECT_EQ(g.neighs(0)[1], 1);

  EXPECT_EQ(g.neighs(1)[0], 1);
  EXPECT_EQ(g.neighs(1)[1], 2);

  EXPECT_EQ(g.neighs(2)[0], 0);
}

}  // namespace
}  // namespace zuckerli
