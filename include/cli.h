#ifndef HAVE_CLI_H
#define HAVE_CLI_H
#include "common.h"
#include "table.h"

enum cli_partition_modify_method {
    CLI_PARTITION_MODIFY_PRESERVE,
    CLI_PARTITION_MODIFY_REPLACE,
    CLI_PARTITION_MODIFY_ADD,
    CLI_PARTITION_MODIFY_SUBSTRACT
};

struct cli_partition_modifier {
    struct table_partition *part;
    enum cli_partition_modify_method update_name, update_offset, update_size, update_masks;
    char name[16];
    uint64_t offset, size;
    uint32_t masks;
};

#endif