#include <assert.h>

#include "dtb.h"
#include "ept.h"
#include "gzip.h"

static_assert(sizeof(struct dtb_header) == 40, "Size of struct DTB header is not 40");

static_assert(sizeof(struct dtb_partition) == DTB_PARTITION_SIZE, "Size of dtb partition is not 256K");

static_assert(sizeof(struct gzip_header) == 10, "Size of GZIP header of not 10");

static_assert(sizeof(struct ept_partition) == 40, "Size of partition is not 40");

static_assert(sizeof(struct ept_header) == 24, "Size of partition table header is not 24");

static_assert(sizeof(struct ept_table) == 1304, "Size of partition table is not 1304");

/* size.c: Sanity check for struct size */