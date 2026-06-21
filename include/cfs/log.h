/**
 * \file log.h
 * \brief Internal logging utilities and debug macros.
 */
#ifndef CFS_LOG_H
#define CFS_LOG_H

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
void cfs_log_debug(const char *fmt, ...);
/** \brief Macro LOG_DEBUG */
#define LOG_DEBUG cfs_log_debug
#else
/**
 * \brief Submits a formatted message to the internal debug log.
 *
 * \param fmt Printf-style format string for the log message.
 * \param ... Variadic arguments.
 */
void cfs_log_debug(const char *fmt, ...);
/** \brief Macro LOG_DEBUG */
#define LOG_DEBUG 1 ? (void)0 : cfs_log_debug
#endif /* DEBUG */
#endif /* !LOG_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_LOG_H */
