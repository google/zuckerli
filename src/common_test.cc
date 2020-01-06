#include "common.h"

#include <gtest/gtest.h>

namespace zuckerli {
namespace {
TEST(CommonDeathTest, TestAbort) { EXPECT_DEATH(ZKR_ABORT("error"), "error"); }
TEST(CommonDeathTest, TestAssert) {
  EXPECT_DEATH(ZKR_ASSERT(0 != 0), "0 != 0");
}
} // namespace
} // namespace zuckerli
