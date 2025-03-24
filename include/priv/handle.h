#pragma once
#include "ampart.h"

struct iops {
    ampart_iop snapshot_out;
    ampart_iop log_out;
};

struct ampart_handle {
    struct iops iops;
};

#define prln_with_level(handle, level, format, arg...) \
    fprintf(handle.log_out, "[%s@"__FILE__":%u] " #level ": " format "\n", __func__, __LINE__, ##arg)

#define prln_fatal(handle, format, arg...) \
    prln_with_level(handle, fatal, format, ##arg)

#define prln_error(handle, format, arg...) \
    prln_with_level(handle, error, format, ##arg)

#define prln_warn(handle, format, arg...) \
    prln_with_level(handle, warning, format, ##arg)

#define prln_info(handle, format, arg...) \
    prln_with_level(handle, info, format, ##arg)

#ifdef DEBUGGING
#define prln_debug(handle, format, arg...) \
    prln_with_level(handle, debug, format, ##arg)
#else
#define prln_debug(handle, format, arg...)
#endif
