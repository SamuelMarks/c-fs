/* clang-format off */
#define CFS_IMPLEMENTATION
#include "cfs/cfs.h"
#include "cfs/log.h"
#include <stdarg.h>
#include <stdio.h>
/* clang-format on */

void cfs_log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
