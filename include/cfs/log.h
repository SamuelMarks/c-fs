/**
 * \file log.h
 * \brief Internal logging utilities and debug macros.
 */
#ifndef CFS_LOG_H
#define CFS_LOG_H

#ifndef CFS_API
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#if defined(CFS_BUILD_SHARED)
#if defined(CFS_EXPORTS)
#define CFS_API __declspec(dllexport)
#else
#define CFS_API __declspec(dllimport)
#endif
#else
#define CFS_API
#endif
#else
#if defined(CFS_BUILD_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
#define CFS_API __attribute__((visibility("default")))
#else
#define CFS_API
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef LOG_DEBUG
#ifdef DEBUG
/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
CFS_API void cfs_log_debug(const char *fmt, ...);
/** \brief Macro LOG_DEBUG */
#define LOG_DEBUG cfs_log_debug
#else
/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
CFS_API void cfs_log_debug(const char *fmt, ...);
/** \brief Macro LOG_DEBUG */
#define LOG_DEBUG 1 ? (void)0 : cfs_log_debug
#endif /* DEBUG */
#endif /* !LOG_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_LOG_H */
