#pragma once
#include <stdint.h>
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

typedef uint64_t
    table_size_t;

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
