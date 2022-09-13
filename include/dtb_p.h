#include "dtb.h"

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "stringblock.h"
#include "gzip.h"
#include "util.h"

#define DTB_FDT_BEGIN_NODE_SPEC     0x00000001U
#define DTB_FDT_END_NODE_SPEC       0x00000002U
#define DTB_FDT_PROP_SPEC           0x00000003U
#define DTB_FDT_NOP_SPEC            0x00000004U
#define DTB_FDT_END_SPEC            0x00000009U

#define DTB_FDT_BEGIN_NODE_ACTUAL   0x01000000U
#define DTB_FDT_END_NODE_ACTUAL     0x02000000U
#define DTB_FDT_PROP_ACTUAL         0x03000000U
#define DTB_FDT_NOP_ACTUAL          0x04000000U
#define DTB_FDT_END_ACTUAL          0x09000000U

#define DTB_PARTITION_CHECKSUM_COUNT   (DTB_PARTITION_SIZE - 4U) >> 2U

#define DTB_PARTITIONS_NODE_START_LENGTH    12U

#define DTB_GET_PARTITIONS_NODE_FROM_DTS(dts, max_offset) \
    dtb_get_node_with_path_from_dts(dts, max_offset, "/partitions", 11)

static const uint8_t dtb_partitions_node_start[DTB_PARTITIONS_NODE_START_LENGTH] = "partitions";

struct dtb_stringblock_essential_offsets {
    off_t parts, pname, size, mask, phandle, linux_phandle;
};


struct dts_property {
    uint32_t len;
    const uint8_t *value;
} dts_property;