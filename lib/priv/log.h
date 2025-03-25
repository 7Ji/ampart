#pragma once
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ampart.h"

typedef void (*prln_func)(char const *restrict format, ...);

extern FILE *file_log;

#define flog_with_level(file, level, format, arg...) \
    if (file_log) fprintf(file_log, "[%s@ampart/lib/"__FILE_NAME__":%u] " #level ": " format "\n", __func__, __LINE__, ##arg)

#define log_with_handle_level(handle, level, format, arg...) \
    flog_with_level(handle->log_out, level, format, ##arg)

#define log_info(handle, format, arg...) \
    log_with_handle_level(handle, info, format, ##arg)

#define log_warn(handle, format, arg...) \
    log_with_handle_level(handle, warn, format, ##arg)

#define log_error(handle, format, arg...) \
    log_with_handle_level(handle, error, format, ##arg)

#define log_error_with_errno(handle, format, arg...) \
    log_error(handle, format", errno: %d, error: %s", ##arg, errno, strerror(errno))
