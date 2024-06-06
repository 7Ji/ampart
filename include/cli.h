#ifndef HAVE_CLI_H
#define HAVE_CLI_H
#include "common.h"

/* System */

#include <linux/limits.h>

/* Local */

#include "ept.h"

/* Enumerable */

enum 
    cli_content_types {
        CLI_CONTENT_TYPE_AUTO,
        CLI_CONTENT_TYPE_DTB,
        CLI_CONTENT_TYPE_RESERVED,
        CLI_CONTENT_TYPE_DISK
    };

enum
    cli_migrate {
        CLI_MIGRATE_ESSENTIAL,
        CLI_MIGRATE_NONE,
        CLI_MIGRATE_ALL
    };

enum 
    cli_modes {
        CLI_MODE_INVALID,
        CLI_MODE_DTOE,
        CLI_MODE_ETOD,
        CLI_MODE_EPEDANTIC,
        CLI_MODE_DEDIT,
        CLI_MODE_EEDIT,
        CLI_MODE_DSNAPSHOT,
        CLI_MODE_ESNAPSHOT,
        CLI_MODE_WEBREPORT,
        CLI_MODE_DCLONE,
        CLI_MODE_ECLONE,
        CLI_MODE_ECREATE
    };

/* Structure */

struct 
    cli_options {
        enum cli_modes          mode;
        enum cli_content_types  content;
        enum cli_migrate        migrate;
        bool                    dry_run;
        bool                    strict_device;
        bool                    rereadpart;
        uint8_t                 write;
        uint64_t                offset_reserved;
        uint64_t                offset_dtb;
        uint64_t                gap_partition;
        uint64_t                gap_reserved;
        size_t                  size;
        char                    target[PATH_MAX];
    };

/* Variable */

extern struct cli_options cli_options;
extern char const         cli_mode_strings[][10];

/* Function */

struct cli_partition_definer *
    cli_parse_partition_raw(
        char const *    arg, 
        uint8_t         require_name, 
        uint8_t         require_offset, 
        uint8_t         require_size, 
        uint8_t         require_masks, 
        uint32_t        default_masks
    );

struct cli_partition_updater *
    cli_parse_partition_update_mode(
        char const *    arg
    );

int 
    cli_interface(
        int             argc, 
        char * const    argv[]
    );
#endif