/**
 * \file cfs.c
 * \brief Core implementation of the c-fs filesystem abstraction library.
 */
/* clang-format off */
#if !defined(_XOPEN_SOURCE) && !defined(_WIN32)
/** @brief Enables POSIX.1-2004 interfaces on non-Windows platforms. */
#define _XOPEN_SOURCE 600
#endif
/** \brief Internal Macro CFS_IMPLEMENTATION */
#define CFS_IMPLEMENTATION
#include <stdlib.h>
#if defined(__EMSCRIPTEN__) && defined(CFS_MULTITHREADED)
#include <pthread.h>
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__DOS__) ||                  \
    defined(__MSDOS__) || defined(_MSDOS)
#include <direct.h>
#else
#if defined(CFS_MULTITHREADED)
#include <pthread.h>
#endif
#if !defined(__EMSCRIPTEN__)
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <unistd.h>
#endif

#if defined(CFS_ENABLE_FAULT_INJECTION)
/* Implement CFS_API for external declarations to match definitions */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#if defined(CFS_BUILD_SHARED)
#if defined(CFS_EXPORTS)
/** @brief Forward export attribute for fault-injection globals. */
#define CFS_API_FWD __declspec(dllexport)
#else
/** @brief Forward import attribute for fault-injection globals. */
#define CFS_API_FWD __declspec(dllimport)
#endif
#else
/** @brief Forward visibility attribute for fault-injection globals. */
#define CFS_API_FWD
#endif
#else
#if defined(CFS_BUILD_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
/** @brief Forward visibility attribute for fault-injection globals. */
#define CFS_API_FWD __attribute__((visibility("default")))
#else
/** @brief Forward visibility attribute for fault-injection globals. */
#define CFS_API_FWD
#endif
#endif

/** @brief Countdown flag to simulate `malloc` failure. */
extern CFS_API_FWD int g_cfs_malloc_fail;
/** @brief Countdown flag to simulate `realloc` failure. */
extern CFS_API_FWD int g_cfs_realloc_fail;
/** @brief Countdown flag to simulate `calloc` failure. */
extern CFS_API_FWD int g_cfs_calloc_fail;
/** @brief Flag to simulate `getcwd` failure. */
extern CFS_API_FWD int g_cfs_getcwd_fail;
/** @brief Flag to simulate `readlink` failure. */
extern CFS_API_FWD int g_cfs_readlink_fail;
/** @brief Flag to simulate `pthread_create` failure. */
extern CFS_API_FWD int g_cfs_pthread_create_fail;
/** @brief Flag to simulate `pipe` failure. */
extern CFS_API_FWD int g_cfs_pipe_fail;
/** @brief Countdown flag to simulate `read`/`write` failure. */
extern CFS_API_FWD int g_cfs_rw_fail;
/** @brief Flag to simulate `posix_spawn` failure. */
extern CFS_API_FWD int g_cfs_spawn_fail;
/** @brief Flag to simulate `waitpid` failure. */
extern CFS_API_FWD int g_cfs_waitpid_fail;

/** \brief Internal Macro malloc */
#define malloc(s) ((g_cfs_malloc_fail > 0 && --g_cfs_malloc_fail == 0) ? NULL : malloc(s))
/** \brief Internal Macro realloc */
#define realloc(p, s) ((g_cfs_realloc_fail > 0 && --g_cfs_realloc_fail == 0) ? NULL : realloc(p, s))
/** \brief Internal Macro calloc */
#define calloc(n, s) ((g_cfs_calloc_fail > 0 && --g_cfs_calloc_fail == 0) ? NULL : calloc(n, s))
/** \brief Internal Macro getcwd */
#define getcwd(b, s) (g_cfs_getcwd_fail ? NULL : getcwd(b, s))
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__DOS__) &&               \
    !defined(__MSDOS__) && !defined(_MSDOS)
/** \brief Internal Macro readlink */
#define readlink(p, b, s) (g_cfs_readlink_fail ? -1 : readlink(p, b, s))
/** @brief Internal Macro pipe */
#define pipe(f) (g_cfs_pipe_fail ? -1 : pipe(f))
/** @brief Internal Macro write */
#define write(fd, b, n) ((g_cfs_rw_fail > 0 && --g_cfs_rw_fail == 0) ? (ssize_t)-1 : write((fd), (b), (n)))
/** @brief Internal Macro read */
#define read(fd, b, n) ((g_cfs_rw_fail > 0 && --g_cfs_rw_fail == 0) ? (ssize_t)-1 : read((fd), (b), (n)))
#if !defined(__EMSCRIPTEN__)
/** @brief Internal Macro posix_spawn */
#define posix_spawn(p, path, fa, sa, av, ev) (g_cfs_spawn_fail ? 2 : posix_spawn((p), (path), (fa), (sa), (av), (ev)))
/** @brief Internal Macro waitpid */
#define waitpid(p, st, o) (g_cfs_waitpid_fail ? (pid_t)-1 : waitpid((p), (st), (o)))
#endif
#if defined(CFS_MULTITHREADED)
/** @brief Internal Macro pthread_create */
#define pthread_create(t, a, f, arg) (g_cfs_pthread_create_fail ? 11 : pthread_create((t), (a), (f), (arg)))
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
/** \brief Internal Macro _getcwd */
#define _getcwd(b, s) (g_cfs_getcwd_fail ? NULL : _getcwd(b, s))
#endif
#endif /* CFS_ENABLE_FAULT_INJECTION */

#include "cfs/cfs.h"
#include "cfs/log.h"
#include <stdarg.h>
#include <stdio.h>
/* clang-format on */

#if defined(CFS_ENABLE_FAULT_INJECTION)
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
/** @brief Global flag to simulate pthread_create failure in tests. */
CFS_API int g_cfs_pthread_create_fail = 0;
/** @brief Global flag to simulate pipe failure in tests. */
CFS_API int g_cfs_pipe_fail = 0;
/** @brief Global flag to simulate read/write failure in tests. */
CFS_API int g_cfs_rw_fail = 0;
/** @brief Global flag to simulate posix_spawn failure in tests. */
CFS_API int g_cfs_spawn_fail = 0;
/** @brief Global flag to simulate waitpid failure in tests. */
CFS_API int g_cfs_waitpid_fail = 0;
#endif /* CFS_ENABLE_FAULT_INJECTION */

/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
CFS_API void cfs_log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
#if defined(_MSC_VER)
  vfprintf_s(stderr, fmt, args);
  fprintf_s(stderr, "\n");
#else
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
#endif
  va_end(args);
}
