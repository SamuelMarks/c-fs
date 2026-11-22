/**
 * @file test_interop_before.c
 * @brief Tests including cfs/cfs.h BEFORE a foreign no_discard.h header.
 */

/* clang-format off */
#include "cfs/cfs.h"
#ifdef NO_DISCARD_H
#error "cfs/cfs.h must not define generic NO_DISCARD_H before foreign header"
#endif
#include "mock_foreign_no_discard.h"
#include "test_interop.h"
/* clang-format on */

#ifndef CFS_NO_DISCARD_H
#error "CFS_NO_DISCARD_H must be defined"
#endif

#ifndef NO_DISCARD_H
#error "Foreign NO_DISCARD_H must be defined after mock_foreign_no_discard.h"
#endif

/**
 * @brief Helper returning simulated foreign nodiscard error enum.
 * @param ok Non-zero for success, zero for failure.
 * @return `MOCK_FOREIGN_OK` or `MOCK_FOREIGN_FAIL`.
 */
static NO_DISCARD mock_foreign_err_t mock_before_helper(int ok) {
  return ok ? MOCK_FOREIGN_OK : MOCK_FOREIGN_FAIL;
}

CFS_NO_DISCARD cfs_errc cfs_test_interop_before(cfs_bool *out_empty) {
  cfs_path p;
  mock_foreign_err_t ferr;
  cfs_errc rc;

  ferr = mock_before_helper(out_empty != NULL);
  if (ferr != MOCK_FOREIGN_OK) {
    return cfs_errc_invalid_argument;
  }

  rc = cfs_path_init_str(&p, CFS_STR("interop_before"));
  if (rc != cfs_errc_success) {
    return rc;
  }
  rc = cfs_path_is_empty(&p, out_empty);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&p);
    return rc;
  }
  cfs_path_destroy(&p);
  return cfs_errc_success;
}
