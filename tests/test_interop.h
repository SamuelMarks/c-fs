/**
 * @file test_interop.h
 * @brief Declarations for multi-header coexistence unit tests.
 */

#ifndef CFS_TEST_INTEROP_H
#define CFS_TEST_INTEROP_H

/* clang-format off */
#include "cfs/cfs.h"
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Verifies pre-defining NO_DISCARD before including cfs/cfs.h.
 * @param out_empty Pointer to receive whether a freshly initialized path is
 * empty.
 * @return `cfs_errc_success` on success, or non-zero on failure.
 */
CFS_NO_DISCARD cfs_errc cfs_test_interop_predef(cfs_bool *out_empty);

/**
 * @brief Verifies including cfs/cfs.h before a foreign no_discard.h header.
 * @param out_empty Pointer to receive whether a populated path is empty.
 * @return `cfs_errc_success` on success, or non-zero on failure.
 */
CFS_NO_DISCARD cfs_errc cfs_test_interop_before(cfs_bool *out_empty);

/**
 * @brief Verifies including cfs/cfs.h after a foreign no_discard.h header.
 * @param out_empty Pointer to receive whether a populated path is empty.
 * @return `cfs_errc_success` on success, or non-zero on failure.
 */
CFS_NO_DISCARD cfs_errc cfs_test_interop_after(cfs_bool *out_empty);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_TEST_INTEROP_H */
