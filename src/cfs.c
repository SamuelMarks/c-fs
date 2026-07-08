/**
 * \file cfs.c
 * \brief Core implementation of the c-fs filesystem abstraction library.
 */
/* clang-format off */
#if !defined(_XOPEN_SOURCE) && !defined(_WIN32)
#define _XOPEN_SOURCE 500
#endif
/** \brief Internal Macro CFS_IMPLEMENTATION */
#define CFS_IMPLEMENTATION
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <pthread.h>
#endif
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#else
#include <direct.h>
#endif

/* Implement CFS_API for external declarations to match definitions */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#if defined(CFS_BUILD_SHARED)
#if defined(CFS_EXPORTS)
#define CFS_API_FWD __declspec(dllexport)
#else
#define CFS_API_FWD __declspec(dllimport)
#endif
#else
#define CFS_API_FWD
#endif
#else
#if defined(CFS_BUILD_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
#define CFS_API_FWD __attribute__((visibility("default")))
#else
#define CFS_API_FWD
#endif
#endif

extern CFS_API_FWD int g_cfs_malloc_fail;
extern CFS_API_FWD int g_cfs_realloc_fail;
extern CFS_API_FWD int g_cfs_calloc_fail;
extern CFS_API_FWD int g_cfs_getcwd_fail;
extern CFS_API_FWD int g_cfs_readlink_fail;

/** \brief Internal Macro malloc */
#define malloc(s) ((g_cfs_malloc_fail > 0 && --g_cfs_malloc_fail == 0) ? NULL : malloc(s))
/** \brief Internal Macro realloc */
#define realloc(p, s) ((g_cfs_realloc_fail > 0 && --g_cfs_realloc_fail == 0) ? NULL : realloc(p, s))
/** \brief Internal Macro calloc */
#define calloc(n, s) ((g_cfs_calloc_fail > 0 && --g_cfs_calloc_fail == 0) ? NULL : calloc(n, s))
/** \brief Internal Macro getcwd */
#define getcwd(b, s) (g_cfs_getcwd_fail ? NULL : getcwd(b, s))
/** \brief Internal Macro readlink */
#define readlink(p, b, s) (g_cfs_readlink_fail ? -1 : readlink(p, b, s))

#if defined(_WIN32) || defined(_WIN64)
/** \brief Internal Macro _getcwd */
#define _getcwd(b, s) (g_cfs_getcwd_fail ? NULL : _getcwd(b, s))
#endif

#include "cfs/cfs.h"
#include "cfs/log.h"
#include <stdarg.h>
#include <stdio.h>
/* clang-format on */

/** \brief Global flag to simulate cfs_malloc failure in tests. */
CFS_API int g_cfs_malloc_fail = 0;
/** \brief Global flag to simulate cfs_realloc failure in tests. */
CFS_API int g_cfs_realloc_fail = 0;
/** \brief Global flag to simulate cfs_calloc failure in tests. */
CFS_API int g_cfs_calloc_fail = 0;
/** \brief Global flag to simulate getcwd failure in tests. */
CFS_API int g_cfs_getcwd_fail = 0;
/** \brief Global flag to simulate readlink failure in tests. */
CFS_API int g_cfs_readlink_fail = 0;

/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
CFS_API void cfs_log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}
