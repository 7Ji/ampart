#ifndef HAVE_CLI_H
#define HAVE_CLI_H
#include "common.h"

/* Local */
#include "ept.h"

/* Definition */

#define CLI_ARGUMENT_ANY                0b00000000
#define CLI_ARGUMENT_DISALLOW           0b10000000
#define CLI_ARGUMENT_REQUIRED           0b00000001
#define CLI_ARGUMENT_ALLOW_ABSOLUTE     0b00000010
#define CLI_ARGUMENT_ALLOW_RELATIVE     0b00000100

/* Enumerable */

enum 
    cli_content_types {
        CLI_CONTENT_TYPE_AUTO,
        CLI_CONTENT_TYPE_DTB,
        CLI_CONTENT_TYPE_RESERVED,
        CLI_CONTENT_TYPE_DISK
    };

enum 
    cli_modes {
        CLI_MODE_INVALID,
        CLI_MODE_DTOE,
        CLI_MODE_ETOD,
        CLI_MODE_PEDANTIC,
        CLI_MODE_DEDIT,
        CLI_MODE_EEDIT,
        CLI_MODE_DSNAPSHOT,
        CLI_MODE_ESNAPSHOT,
        CLI_MODE_DCLONE,
        CLI_MODE_ECLONE,
        CLI_MODE_ECREATE
    };

enum 
    cli_partition_select_method {
        CLI_PARTITION_SELECT_NAME,
        CLI_PARTITION_SELECT_RELATIVE
    };

enum 
    cli_partition_modify_detail_method {
        CLI_PARTITION_MODIFY_DETAIL_PRESERVE,
        CLI_PARTITION_MODIFY_DETAIL_SET,
        CLI_PARTITION_MODIFY_DETAIL_ADD,
        CLI_PARTITION_MODIFY_DETAIL_SUBSTRACT,
    };

enum 
    cli_partition_modify_part_method {
        CLI_PARTITION_MODIFY_PART_ADJUST,
        CLI_PARTITION_MODIFY_PART_DELETE,
        CLI_PARTITION_MODIFY_PART_CLONE,
        CLI_PARTITION_MODIFY_PART_PLACE
    };

enum 
    cli_partition_modify_place_method {
        CLI_PARTITION_MODIFY_PLACE_PRESERVE,
        CLI_PARTITION_MODIFY_PLACE_ABSOLUTE,
        CLI_PARTITION_MODIFY_PLACE_RELATIVE
    };

/* Structure */

struct 
    cli_options {
        enum cli_modes          mode;
        enum cli_content_types  content;
        bool                    dry_run;
        bool                    strict_device;
        uint8_t                 write;
        uint64_t                offset_reserved;
        uint64_t                offset_dtb;
        uint64_t                gap_partition;
        uint64_t                gap_reserved;
        size_t                  size;
        char *                  target;
    };

struct 
    cli_partition_definer {
        char        name[MAX_PARTITION_NAME_LENGTH];
        bool        relative_offset;
        bool        relative_size;
        uint64_t    offset;
        uint64_t    size;
        uint32_t    masks;
    };

struct 
    cli_partition_modifier {
        enum cli_partition_select_method        select;
        enum cli_partition_modify_part_method   modify_part;
        enum cli_partition_modify_place_method  modify_place;
        enum cli_partition_modify_detail_method modify_name;
        enum cli_partition_modify_detail_method modify_offset;
        enum cli_partition_modify_detail_method modify_size;
        enum cli_partition_modify_detail_method modify_masks;
        char                                    select_name[16];
        int                                     select_relative;
        int                                     place;
        char                                    name[16];
        uint64_t                                offset;
        uint64_t                                size;
        uint32_t                                masks;
    };

struct 
    cli_partition_updater {
        bool                                modify;
        union {
            struct cli_partition_definer    definer;
            struct cli_partition_modifier   modifier;
        };
    };

/* External */

extern struct cli_options cli_options;

/* Function */

struct cli_partition_definer *
    cli_parse_partition_raw(
        const char *    arg, 
        uint8_t         require_name, 
        uint8_t         require_offset, 
        uint8_t         require_size, 
        uint8_t         require_masks, 
        uint32_t        default_masks
    );

struct cli_partition_updater *
    cli_parse_partition_update_mode(
        const char *    arg
    );

int 
    cli_interface(
        int     argc, 
        char *  argv[]
    );
#endif