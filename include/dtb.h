#ifndef HAVE_DTB_H
#define HAVE_DTB_H
#include "common.h"

/* Local */

#include "dts.h"

/* Definition */

#define DTB_MAGIC_MULTI     0x5F4C4D41U
#define DTB_MAGIC_PLAIN     0xEDFE0DD0U

#define DTB_PARTITION_OFFSET        0x400000U //4M
#define DTB_PARTITION_SIZE          0x40000U  //256K
#define DTB_PARTITION_DATA_SIZE     DTB_PARTITION_SIZE - 4*sizeof(uint32_t)

/* Macro */

//#define DTB_HEADER_HOT(x)   union { uint32_t x, hot_##x; }

/* Enumerable */

enum 
    dtb_type {
        DTB_TYPE_INVALID,
        DTB_TYPE_PLAIN,
        DTB_TYPE_MULTI,
        DTB_TYPE_GZIPPED
    };

/* Structure */

struct
    dtb_buffer_entry {
        uint8_t *                       buffer;
        size_t                          size;
        struct dts_partitions_helper    phelper;
        bool                            has_partitions;
        char                            target[36];
        char                            soc[12];
        char                            platform[12];
        char                            variant[12];
    };

struct
    dtb_buffer_helper {
        struct dtb_buffer_entry *   dtbs;
        unsigned                    dtb_count;
        unsigned                    multi_version;
        enum dtb_type               type_main;
        enum dtb_type               type_sub;
    };

struct 
    dtb_header {
        uint32_t        magic;
        union {
            uint32_t    totalsize;
            uint32_t    hot_totalsize;
        };
        uint32_t        off_dt_struct;
        union {
            uint32_t    off_dt_strings;
            uint32_t    hot_off_dt_strings;
        };
        uint32_t        off_mem_rsvmap;
        uint32_t        version;
        uint32_t        last_comp_version;
        uint32_t        boot_cpuid_phys;
        uint32_t        size_dt_strings;
        union {
            uint32_t    size_dt_struct;
            uint32_t    hot_size_dt_struct;
        };
    };

struct 
    dtb_multi_entry {
        char        soc[12];
        char        platform[12];
        char        variant[12];
        char        target[36];
        uint8_t *   dtb;
        uint32_t    offset;
        uint32_t    size;
    };

struct 
    dtb_multi_entries_helper {
        uint8_t                     version;
        uint32_t                    entry_count;
        struct dtb_multi_entry *    entries;
    };

struct 
    dtb_multi_header {
        uint32_t    magic;
        uint32_t    version;
        uint32_t    entry_count;
    };

struct 
    dtb_partition {
        uint8_t     data[DTB_PARTITION_DATA_SIZE];
        uint32_t    magic;
        uint32_t    version;
        uint32_t    timestamp;
        uint32_t    checksum;
    };

/* Function */
int
    dtb_buffer_helper_implement_partitions(
        struct dtb_buffer_helper *                  new,
        struct dtb_buffer_helper const *            old,
        struct dts_partitions_helper_simple const * phelper
    );

uint32_t 
    dtb_checksum(
        struct dtb_partition const *    dtb
    );

int
    dtb_check_buffers_partitions(
        struct dtb_buffer_helper const *    bhelper
    );

int
    dtb_compose(
        uint8_t * *                                 dtb,
        size_t *                                    size,
        struct dtb_buffer_helper const *            bhelper,
        struct dts_partitions_helper_simple const * phelper
    );

void
    dtb_free_buffer_helper(
        struct dtb_buffer_helper *  bhelper
    );

int
    dtb_get_partitions(
        struct dts_partitions_helper *  phelper,
        uint8_t const *                 dtb, 
        size_t                          size
    );

struct dts_phandle_list *
    dtb_get_phandles(
        uint8_t const * dtb,
        size_t          size
    );

enum dtb_type 
    dtb_identify_type(
        uint8_t const * dtb
    );

int
    dtb_parse_multi_entries(
        struct dtb_multi_entries_helper *   mhelper,
        uint8_t const *                     dtb
    );

int
    dtb_read_into_buffer_helper(
        struct dtb_buffer_helper *  bhelper,
        int                         fd,
        size_t                      size_max,
        bool                        should_checksum
    );

int
    dtb_read_into_buffer_helper_and_report(
        struct dtb_buffer_helper *  bhelper,
        int                         fd, 
        size_t                      size_max, 
        bool                        should_checksum
    );

int
    dtb_snapshot(
        struct dtb_buffer_helper const * const  bhelper
    );

#endif