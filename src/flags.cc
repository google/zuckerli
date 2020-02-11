#include "context_model.h"
#include "integer_coder.h"
#include "absl/flags/flag.h"

ABSL_FLAG(int32_t, log2_num_explicit, 4,
          "Number of direct-coded tokens (pow2)");
ABSL_FLAG(int32_t, num_token_bits, 1, "Number of MSBs in token");
ABSL_FLAG(int32_t, ref_block, 32,
          "Number of previous lists to try to copy from");
