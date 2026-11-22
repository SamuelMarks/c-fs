/**
 * @file test_interop_predef.c
 * @brief Tests pre-defining generic NO_DISCARD before including cfs/cfs.h.
 */

#define NO_DISCARD

/* clang-format off */
#include "cfs/cfs.h"
#include "test_interop.h"
/* clang-format on */

#ifndef CFS_NO_DISCARD_H
#error "CFS_NO_DISCARD_H must be defined by cfs/no_discard.h"
#endif

#ifdef NO_DISCARD_H
#error "cfs/no_discard.h must not define generic NO_DISCARD_H"
#endif

#ifndef NO_DISCARD_TYPE
#error "cfs/no_discard.h must provide fallback NO_DISCARD_TYPE"
#endif

#ifndef NO_DISCARD_TYPEDEF
#error "cfs/no_discard.h must provide fallback NO_DISCARD_TYPEDEF"
#endif

/** @brief Local enum verifying fallback NO_DISCARD_TYPE / TYPEDEF compile. */
typedef enum NO_DISCARD_TYPE cfs_predef_check_enum {
  /** @brief Success enumerator. */
  CFS_PREDEF_OK = 0,
  /** @brief Error enumerator. */
  CFS_PREDEF_ERR = 1
} cfs_predef_check_t NO_DISCARD_TYPEDEF;

CFS_NO_DISCARD cfs_errc cfs_test_interop_predef(cfs_bool *out_empty) {
  cfs_path p;
  cfs_predef_check_t chk = (out_empty != NULL) ? CFS_PREDEF_OK : CFS_PREDEF_ERR;
  if (chk != CFS_PREDEF_OK) {
    return cfs_errc_invalid_argument;
  }
  cfs_path_init(&p);
  return cfs_path_is_empty(&p, out_empty);
}
