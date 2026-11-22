/**
 * @file test_interop_cxx.cpp
 * @brief C++ header interoperability test verifying `<sal.h>` and `cfs` headers
 * never wrap C++ headers inside `extern "C"` linkage blocks.
 */

/* clang-format off */
#include "cfs/no_discard.h"
#include "cfs/cfs.h"
#include "mock_foreign_no_discard.h"
/* clang-format on */

/**
 * @brief C++ template verifying global C++ linkage was preserved by headers.
 * @tparam T Held value type.
 */
template <typename T> struct CfsCxxTemplateProbe {
  /** @brief Stored value. */
  T value;
};

/**
 * @brief Verifies calling cfs C APIs and foreign nodiscard types from C++.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
  CfsCxxTemplateProbe<cfs_errc> probe;
  mock_foreign_err_t ferr = MOCK_FOREIGN_OK;
  cfs_path p;
  cfs_bool empty = cfs_true;

  probe.value = cfs_path_init_str(&p, CFS_STR("cxx_interop"));
  if (probe.value != cfs_errc_success) {
    return 1;
  }
  probe.value = cfs_path_is_empty(&p, &empty);
  cfs_path_destroy(&p);
  if (probe.value != cfs_errc_success || empty != cfs_false ||
      ferr != MOCK_FOREIGN_OK) {
    return 2;
  }
  return 0;
}
