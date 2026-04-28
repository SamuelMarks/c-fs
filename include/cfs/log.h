#ifndef CFS_LOG_H
#define CFS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef LOG_DEBUG
#ifdef DEBUG
void cfs_log_debug(const char *fmt, ...);
#define LOG_DEBUG cfs_log_debug
#else
void cfs_log_debug(const char *fmt, ...);
#define LOG_DEBUG 1 ? (void)0 : cfs_log_debug
#endif /* DEBUG */
#endif /* !LOG_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_LOG_H */
