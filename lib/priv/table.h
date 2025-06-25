#pragma once
#include <stdint.h>
#include <inttypes.h>
#include "ampart.h"

#define TABLE_NAME_MAXLEN   15
#define TBALE_PARTS_MAXLEN  32

typedef char
    table_magic_t[4],
    table_version_t[12],
    table_name_t[TABLE_NAME_MAXLEN + 1];

typedef uint32_t
    table_count_t,
    table_checksum_t,
    table_masks_t;

#define TABLE_MASKS_MAX         UINT32_MAX

#define PRI_TABLE_COUNT_U       PRIu32
#define PRI_TABLE_CHECKSUM_U    PRIu32
#define PRI_TABLE_MASKS_U       PRIu32

#define PRI_TABLE_COUNT_X       PRIX32
#define PRI_TABLE_CHECKSUM_X    PRIX32
#define PRI_TABLE_MASKS_X       PRIX32

typedef uint64_t
    table_size_t;

#define TABLE_SIZE_MAX          UINT64_MAX

#define PRI_TABLE_SIZE_U        PRIu64
#define PRI_TABLE_SIZE_X        PRIX64

struct table_header {
    table_magic_t magic;
    table_version_t version;
    table_count_t parts_count;
    table_checksum_t checksum;
};

struct table_part {
    table_name_t name;
    table_size_t size, offset;
    table_masks_t masks;
    // with 4 bytes padding at the end
};

struct table {
    struct table_header header;
    struct table_part parts[TBALE_PARTS_MAXLEN];
};

ampart_status table_size_from_human_readable(table_size_t *restrict size, char const * restrict readable);

ampart_status table_masks_from_literal(table_masks_t *restrict masks, char const * restrict literal);
