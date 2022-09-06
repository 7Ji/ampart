#ifndef HAVE_TABLE_H
#define HAVE_TABLE_H
#include "common.h"

struct table_partition {
    char name[16];
    uint64_t size;
    uint64_t offset;
    uint32_t mask_flags;
    uint32_t padding;
}; // 40

struct table_header {
    union { // 4
        char magic_string[4];
        uint32_t magic_uint32;
    };
    union { // 12
        char version_string[12];
        uint32_t version_uint32[3];
    };
    int partitions_count;   // 4
    uint32_t checksum;   // 4
}; // 24

struct table {
    union {
        struct table_header header; // 24
        struct {
            union { // 4
                char magic_string[4];
                uint32_t magic_uint32;
            };
            union { // 12
                char version_string[12];
                uint32_t version_uint32[3];
            };
            int partitions_count;   // 4
            uint32_t checksum;   // 4
        };
    };
    union {
        struct table_partition partitions[32]; // 40 * 32
        char partition_names[32][40];
    };
}; // 1304

#endif 