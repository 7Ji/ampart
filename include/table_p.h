#include "table.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "cli.h"
#include "util.h"

#define TABLE_PART_MAXIMUM  32

#define TABLE_PART_MASKS_BOOT   1
#define TABLE_PART_MASKS_SYSTEM 2
#define TABLE_PART_MASKS_DATA   4

#define TABLE_HEADER_MAGIC_UINT32 (uint32_t)0x0054504DU
#define TABLE_HEADER_MAGIC_STRING   "MPT"
#define TABLE_HEADER_VERSION_STRING "01.00.00"
#define TABLE_HEADER_VERSION_UINT32_0   (uint32_t)0x302E3130U
#define TABLE_HEADER_VERSION_UINT32_1   (uint32_t)0x30302E30U
#define TABLE_HEADER_VERSION_UINT32_2   (uint32_t)0x00000000U

static const uint32_t table_header_version_uint32[] = {
    TABLE_HEADER_VERSION_UINT32_0,
    TABLE_HEADER_VERSION_UINT32_1,
    TABLE_HEADER_VERSION_UINT32_2
};

static struct table table_empty = {
    {
        {
            { .magic_uint32 = TABLE_HEADER_MAGIC_UINT32 },
            { .version_uint32 = {TABLE_HEADER_VERSION_UINT32_0, TABLE_HEADER_VERSION_UINT32_1, TABLE_HEADER_VERSION_UINT32_2} },
            4U,
            0U
        }
    },
    {
        {
            {
                TABLE_PARTITION_BOOTLOADER_NAME,
                TABLE_PARTITION_BOOTLOADER_SIZE,
                0U,
                0U,
                0U
            },
            {
                TABLE_PARTITION_RESERVED_NAME,
                TABLE_PARTITION_RESERVED_SIZE,
                0U,
                0U,
                0U
            },
            {
                TABLE_PARTITION_CACHE_NAME,
                TABLE_PARTITION_CACHE_SIZE,
                0U,
                0U,
                0U
            },
            {
                TABLE_PARTITION_ENV_NAME,
                TABLE_PARTITION_ENV_SIZE,
                0U,
                0U,
                0U
            },
            {{0U},0U,0U,0U,0U}
        }
    }
    
};