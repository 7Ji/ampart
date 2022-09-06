#include "dtb.h"

#include <byteswap.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "stringblock.h"
#include "gzip.h"

#define DTB_MAGIC_MULTI     0x5F4C4D41
#define DTB_MAGIC_PLAIN     0xEDFE0DD0
#define DTB_MAGIC_GZIP      0x00008B1F
#define DTB_MAGIC_XIAOMI    0x000089EF
#define DTB_MAGIC_PHICOMM   0x000004DA


#define DTB_FDT_BEGIN_NODE_SPEC 1
#define DTB_FDT_END_NODE_SPEC   2
#define DTB_FDT_PROP_SPEC       3
#define DTB_FDT_NOP_SPEC        4
#define DTB_FDT_END_SPEC        9

#define DTB_FDT_BEGIN_NODE_ACTUAL   0x01000000
#define DTB_FDT_END_NODE_ACTUAL     0x02000000
#define DTB_FDT_PROP_ACTUAL         0x03000000
#define DTB_FDT_NOP_ACTUAL          0x04000000
#define DTB_FDT_END_ACTUAL          0x09000000

#define DTB_PARTITION_CHECKSUM_COUNT   (DTB_PARTITION_SIZE - 4) >> 2