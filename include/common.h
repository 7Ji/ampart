#ifndef HAVE_COMMON_H
#define HAVE_COMMON_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

/* Defitition */

#define MAX_PARTITION_NAME_LENGTH 16
#define MAX_PARTITIONS_COUNT      32

/* Macro */

#define prln_with_level(level, format, arg...) \
    fprintf(stderr, "[%s@"__FILE__":%u] " #level ": " format "\n", __func__, __LINE__, ##arg)

#define prln_fatal(format, arg...) \
    prln_with_level(fatal, format, ##arg)

#define prln_error(format, arg...) \
    prln_with_level(error, format, ##arg)

#define prln_warn(format, arg...) \
    prln_with_level(warning, format, ##arg)

#define prln_info(format, arg...) \
    prln_with_level(info, format, ##arg)

#ifdef DEBUGGING
#define prln_debug(format, arg...) \
    prln_with_level(debug, format, ##arg)
#else
#define prln_debug(format, arg...)
#endif

#define prln_error_with_errno(format, arg...) \
    prln_error(format", errno: %d, error: %s", ##arg, errno, strerror(errno))

#endif