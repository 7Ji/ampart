#include "table.h"

#include <unistd.h>
#include <fcntl.h>

#include "util.h"

#define TABLE_PART_MAXIMUM  32

#define TABLE_PART_MASKS_BOOT   1
#define TABLE_PART_MASKS_SYSTEM 2
#define TABLE_PART_MASKS_DATA   4

#define TABLE_HEADER_MAGIC_UINT32 (uint32_t)0x0054504D
#define TABLE_HEADER_MAGIC_STRING   "MPT"
#define TABLE_HEADER_VERSION_STRING "01.00.00"
#define TABLE_HEADER_VERSION_UINT32_0   (uint32_t)0x302E3130
#define TABLE_HEADER_VERSION_UINT32_1   (uint32_t)0x30302E30
#define TABLE_HEADER_VERSION_UINT32_2   (uint32_t)0x00000000

static const uint32_t table_header_version_uint32[] = {
    TABLE_HEADER_VERSION_UINT32_0,
    TABLE_HEADER_VERSION_UINT32_1,
    TABLE_HEADER_VERSION_UINT32_2
};
