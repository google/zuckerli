#ifndef ZUCKERLI_CONTEXT_MODEL_H
#define ZUCKERLI_CONTEXT_MODEL_H
#include <cstdlib>
#include <limits>

#include "common.h"
#include "integer_coder.h"
#include "absl/flags/flag.h"

ABSL_DECLARE_FLAG(int32_t, ref_block);

namespace zuckerli {

ZKR_INLINE size_t SearchNum() {
#if ZKR_HONOR_FLAGS
  ZKR_ASSERT(absl::GetFlag(FLAGS_ref_block) <= 64);
  return absl::GetFlag(FLAGS_ref_block);
#else
  return 32;
#endif
};
ZKR_INLINE size_t NumAdjLists() { return SearchNum() + 1; };

static constexpr size_t kFirstDegreeContext = 0;
static constexpr size_t kDegreeBaseContext = 1;
static constexpr size_t kNumDegreeContexts = 32;
static constexpr size_t kReferenceContextBase =
    kDegreeBaseContext + kNumDegreeContexts;
static constexpr size_t kNumReferenceContexts = 64;  // At most 64.
static constexpr size_t kBlockCountContext =
    kReferenceContextBase + kNumReferenceContexts;
static constexpr size_t kBlockContext = kBlockCountContext + 1;
static constexpr size_t kBlockContextEven = kBlockContext + 1;
static constexpr size_t kBlockContextOdd = kBlockContextEven + 1;
static constexpr size_t kFirstResidualBaseContext = kBlockContextOdd + 1;
static constexpr size_t kFirstResidualNumContexts = 32;
static constexpr size_t kResidualBaseContext =
    kFirstResidualBaseContext + kFirstResidualNumContexts;
static constexpr size_t kNumResidualContexts = 80;  // Slightly lax bound
// TODO: use number of remaining residuals as a ctx?
static constexpr size_t kRleContext =
    kResidualBaseContext + kNumResidualContexts;

ZKR_INLINE size_t DegreeContext(size_t last_residual) {
  uint32_t token = IntegerCoder::Token(last_residual);
  return kDegreeBaseContext + std::min<size_t>(token, kNumDegreeContexts - 1);
}

ZKR_INLINE size_t ReferenceContext(size_t last_reference) {
  return kReferenceContextBase + last_reference;
}

ZKR_INLINE size_t FirstResidualContext(size_t edges_left) {
  uint32_t token = IntegerCoder::Token(edges_left);
  size_t ctx = kFirstResidualBaseContext;
  ctx += std::min<size_t>(kFirstResidualNumContexts - 1, token);
  return ctx;
}

ZKR_INLINE size_t ResidualContext(size_t last_residual) {
  uint32_t token = IntegerCoder::Token(last_residual);
  return kResidualBaseContext +
         std::min<size_t>(token, kNumResidualContexts - 1);
}

static constexpr size_t kNumContexts = kRleContext + 1;

// Random access only parameters: minimum length for RLE and size of chunk of
// nodes for which residuals and references are delta-coded.
static constexpr size_t kDegreeReferenceChunkSize = 32;

static constexpr size_t kRleMin = 3;

}  // namespace zuckerli

#endif  // ZUCKERLI_CONTEXT_MODEL_H
