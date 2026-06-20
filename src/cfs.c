/**
 * \file cfs.c
 * \brief Core implementation of the c-fs filesystem abstraction library.
 */
/* clang-format off */
#define CFS_IMPLEMENTATION
#include <stdlib.h>
#include <unistd.h>

extern int g_cfs_malloc_fail;
extern int g_cfs_realloc_fail;
extern int g_cfs_calloc_fail;
extern int g_cfs_getcwd_fail;
extern int g_cfs_readlink_fail;

#define malloc(s) ((g_cfs_malloc_fail > 0 && --g_cfs_malloc_fail == 0) ? NULL : malloc(s))
#define realloc(p, s) (g_cfs_realloc_fail ? NULL : realloc(p, s))
#define calloc(n, s) (g_cfs_calloc_fail ? NULL : calloc(n, s))
#define getcwd(b, s) (g_cfs_getcwd_fail ? NULL : getcwd(b, s))
#define readlink(p, b, s) (g_cfs_readlink_fail ? -1 : readlink(p, b, s))

#include "cfs/cfs.h"
#include "cfs/log.h"
#include <stdarg.h>
#include <stdio.h>
/* clang-format on */

/** \brief Global flag to simulate cfs_malloc failure in tests. */
int g_cfs_malloc_fail = 0;
/** \brief Global flag to simulate cfs_realloc failure in tests. */
int g_cfs_realloc_fail = 0;
/** \brief Global flag to simulate cfs_calloc failure in tests. */
int g_cfs_calloc_fail = 0;
/** \brief Global flag to simulate getcwd failure in tests. */
int g_cfs_getcwd_fail = 0;
/** \brief Global flag to simulate readlink failure in tests. */
int g_cfs_readlink_fail = 0;

/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
void cfs_log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}
