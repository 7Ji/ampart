#ifndef HAVE_DTB_H
#define HAVE_DTB_H
#include "common.h"

#define DTB_HEADER_HOT(x)   union { uint32_t x, hot_##x; }

#define DTB_PARTITION_SIZE          256*1024  //256K
#define DTB_PARTITION_DATA_SIZE     DTB_PARTITION_SIZE - 4*sizeof(unsigned int)


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


#endif