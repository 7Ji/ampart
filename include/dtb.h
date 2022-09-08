#ifndef HAVE_DTB_H
#define HAVE_DTB_H
#include "common.h"

#define DTB_HEADER_HOT(x)   union { uint32_t x, hot_##x; }

#define DTB_PARTITION_SIZE          256*1024  //256K
#define DTB_PARTITION_DATA_SIZE     DTB_PARTITION_SIZE - 4*sizeof(unsigned int)

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
    unsigned int magic;
    unsigned int version;
    unsigned int timestamp;
    unsigned int checksum;
};

struct dtb_partition_node {
    char pname[16];
    uint64_t offset, size;
    
    uint32_t phandle;
};

struct dtb_partitions {
    struct dtb_partition_node partitions[32];


};


#endif