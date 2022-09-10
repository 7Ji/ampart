#ifndef HAVE_DTB_H
#define HAVE_DTB_H
#include "common.h"

#define DTB_HEADER_HOT(x)   union { uint32_t x, hot_##x; }

#define DTB_PARTITION_SIZE          0x40000U  //256K
#define DTB_PARTITION_DATA_SIZE     DTB_PARTITION_SIZE - 4*sizeof(uint32_t)

/*
  0x000000 - 0x003fff: partition table
  0x004000 - 0x03ffff: storage key area	(16k offset & 256k size)
  0x400000 - 0x47ffff: dtb area  (4M offset & 512k size)
  0x480000 - 64MBytes: resv for other usage.

*/

// struct partitions emmc_partition_table[] = {
// 	PARTITION_ELEMENT(MMC_BOOT_NAME, MMC_BOOT_DEVICE_SIZE, 0),
// 	PARTITION_ELEMENT(MMC_RESERVED_NAME, MMC_RESERVED_SIZE, 0),
// 	/* prior partitions, same partition name with dts*/
// 	/* partition size will be overide by dts*/
// 	PARTITION_ELEMENT(MMC_CACHE_NAME, 0, 0),
// 	PARTITION_ELEMENT(MMC_ENV_NAME, MMC_ENV_SIZE, 0),
// };

struct dtb_header {
    uint32_t magic;
    DTB_HEADER_HOT(totalsize);
    uint32_t off_dt_struct;
    DTB_HEADER_HOT(off_dt_strings);
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    DTB_HEADER_HOT(size_dt_struct);
};

struct dtb_partition {
    unsigned char data[DTB_PARTITION_DATA_SIZE];
    uint32_t magic;
    uint32_t version;
    uint32_t timestamp;
    uint32_t checksum;
};

struct dts_partition_entry {
    char name[MAX_PARTITION_NAME_LENGTH];
    uint64_t size;
    uint32_t mask;
    uint32_t phandle;
};

struct dts_partitions_helper {
    struct dts_partition_entry partitions[MAX_PARTITIONS_COUNT];
    uint32_t phandle_root;
    uint32_t phandles[MAX_PARTITIONS_COUNT];
    uint32_t partitions_count;
    uint32_t record_count;
};

uint32_t dtb_checksum(struct dtb_partition *dtb);
// unsigned char *dtb_get_node_with_path_from_dts(const unsigned char *dts, const uint32_t max_offset, const char *path, const size_t len_path);
struct dts_partitions_helper *dtb_get_partitions(uint8_t *dtb, size_t len);
void dtb_report_partitions(struct dts_partitions_helper *phelper);
#endif