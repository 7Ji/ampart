#ifndef HAVE_TABLE_H
#define HAVE_TABLE_H
#include "common.h"

#include "dtb.h"

#define TABLE_PARTITION_GAP_GENERIC     0x800000U    // 8M
#define TABLE_PARTITION_GAP_RESERVED    0x2000000U   // 32M

#define TABLE_PARTITION_BOOTLOADER_SIZE 0x400000U    // 4M
#define TABLE_PARTITION_BOOTLOADER_NAME "bootloader"
#define TABLE_PARTITION_RESERVED_NAME   "reserved"
#define TABLE_PARTITION_RESERVED_SIZE   0x4000000U   // 64M
#define TABLE_PARTITION_CACHE_NAME      "cache"
#define TABLE_PARTITION_CACHE_SIZE      0U
#define TABLE_PARTITION_ENV_NAME        "env"
#define TABLE_PARTITION_ENV_SIZE        0x800000U    // 8M

#define TABLE_HEADER_MAGIC_UINT32 (uint32_t)0x0054504DU

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

int table_valid(const struct table *table);
void table_report(const struct table *table);
struct table *table_complete_dtb(const struct dts_partitions_helper *dhelper, uint64_t capacity);
struct table *table_from_dtb(const uint8_t *dtb, size_t dtb_size, uint64_t capacity);
int table_compare(const struct table *table_a, const struct table *table_b);
uint64_t table_get_capacity(const struct table *table);
#endif 