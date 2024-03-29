#ifndef HAVE_DTS_H
#define HAVE_DTS_H
#include "common.h"

/* Local */

#include "stringblock.h"

/* Definition */

#define DTS_BEGIN_NODE_SPEC     0x00000001U
#define DTS_END_NODE_SPEC       0x00000002U
#define DTS_PROP_SPEC           0x00000003U
#define DTS_NOP_SPEC            0x00000004U
#define DTS_END_SPEC            0x00000009U

#define DTS_BEGIN_NODE_ACTUAL   0x01000000U
#define DTS_END_NODE_ACTUAL     0x02000000U
#define DTS_PROP_ACTUAL         0x03000000U
#define DTS_NOP_ACTUAL          0x04000000U
#define DTS_END_ACTUAL          0x09000000U

#define DTS_NO_PHANDLE          0b00000000U    
#define DTS_HAS_PHANDLE         0b00000001U
#define DTS_HAS_LINUX_PHANDLE   0b00000010U

/* Structure */

struct 
    dts_partition_entry {
        char        name[MAX_PARTITION_NAME_LENGTH];
        uint64_t    size;
        uint32_t    mask;
        uint32_t    phandle;
        uint32_t    linux_phandle;
    };

struct 
    dts_partitions_helper {
        struct dts_partition_entry  partitions[MAX_PARTITIONS_COUNT];
        uint8_t *                   node;
        uint32_t                    phandle_root;
        uint32_t                    linux_phandle_root;
        uint32_t                    phandles[MAX_PARTITIONS_COUNT];
        uint32_t                    partitions_count;
        uint32_t                    record_count;
    };

struct 
    dts_partition_entry_simple {
        char        name[MAX_PARTITION_NAME_LENGTH];
        uint64_t    size;
        uint32_t    mask;
    };

struct 
    dts_partitions_helper_simple {
        struct dts_partition_entry_simple   partitions[MAX_PARTITIONS_COUNT];
        uint32_t                            partitions_count;
    };

struct
    dts_phandle_entry {
        uint8_t *   node;
        uint8_t     status;
    };

struct 
    dts_phandle_list {
        struct dts_phandle_entry *  entries;
        uint32_t                    min;
        uint32_t                    max;
        uint32_t                    allocated;
        uint8_t                     status;
    };

/* Function */

uint32_t 
    dts_compare_partitions_mixed(
        struct dts_partitions_helper const *        phelper_a, 
        struct dts_partitions_helper_simple const * phelper_b
    );

int
    dts_compose_partitions_node(
        uint8_t * *                                 node,
        size_t *                                    length,
        struct dts_phandle_list *                   plist,
        struct dts_partitions_helper_simple const * phelper,
        struct stringblock_helper *                 shelper,
        off_t                                       offset_phandle,
        off_t                                       offset_linux_phandle
    );

int
    dts_dclone_parse(
        struct dts_partitions_helper_simple * const dparts,
        int const                                   argc,
        char const * const * const                  argv
    );

int
    dts_dedit_parse(
        struct dts_partitions_helper_simple *   dparts,
        int                                     argc,
        char const * const *                    argv
    );

int
    dts_drop_partitions_phandles(
        struct dts_phandle_list *               plist,
        struct dts_partitions_helper const *    phelper
    );

uint8_t *
    dts_get_node_from_path(
        uint8_t const * dts, 
        uint32_t        max_offset,
        char const *    path,
        size_t          len_path
    );

size_t
    dts_get_node_full_length(
        uint8_t const * node,
        uint32_t        max_offset
    );

int
    dts_get_partitions_from_node(
        struct dts_partitions_helper *          phelper,
        struct stringblock_helper const *   shelper
    );

int
    dts_get_phandles(
        struct dts_phandle_list *   plist,
        uint8_t const *             node, 
        uint32_t                    max_offset,
        uint32_t                    offset_phandle,
        uint32_t                    offset_linux_phandle
    );

int
    dts_get_property_string(
        uint8_t const * const   node,
        uint32_t const          property_offset,
        char *                  string,
        size_t const            max_len
    );

int
    dts_partitions_helper_to_simple(
        struct dts_partitions_helper_simple *   simple,
        struct dts_partitions_helper const *    generic
    );

// int 
//     dts_phandle_list_finish(
//         struct dts_phandle_list *   plist
//     );

void
    dts_report_partitions(
        struct dts_partitions_helper const *    phelper
    );

void
    dts_report_partitions_simple(
        struct dts_partitions_helper_simple const * phelper
    );

int
    dts_sort_partitions(
        struct dts_partitions_helper *  phelper
    );

int
    dts_valid_partitions_simple(
        struct dts_partitions_helper_simple const * dparts
    );
    
#endif