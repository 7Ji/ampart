#pragma once
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ampart.h"

typedef void (*prln_func)(char const *restrict format, ...);

extern FILE *file_log;

#define log_with_level(level, format, arg...) \
    if (file_log) fprintf(file_log, "[%s@ampart/lib/"__FILE_NAME__":%u] " #level ": " format "\n", __func__, __LINE__, ##arg)

#define log_info(format, arg...) \
    log_with_level(info, format, ##arg)

#define log_warn(format, arg...) \
    log_with_level(warn, format, ##arg)

#define log_error(format, arg...) \
    log_with_level(error, format, ##arg)

#define log_error_with_errno(format, arg...) \
    log_error(format", errno: %d, error: %s", ##arg, errno, strerror(errno))
