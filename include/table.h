#ifndef HAVE_TABLE_H
#define HAVE_TABLE_H
#include "common.h"

#include "dtb.h"

struct table_partition {
    char name[MAX_PARTITION_NAME_LENGTH];
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
    uint32_t partitions_count;   // 4
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
            uint32_t partitions_count;   // 4
            uint32_t checksum;   // 4
        };
    };
    union {
        struct table_partition partitions[MAX_PARTITIONS_COUNT]; // 40 * 32
        char partition_names[MAX_PARTITIONS_COUNT][sizeof(struct table_partition)];
    };
}; // 1304

int table_valid(struct table *table);
void table_report(struct table *table);
struct table *table_complete_dtb(struct dts_partitions_helper *dhelper, uint64_t capacity);
struct table *table_from_dtb(uint8_t *dtb, size_t dtb_size, uint64_t capacity);
int table_compare(struct table *table_a, struct table *table_b);
uint64_t table_get_capacity(struct table *table);
#endif 