#include "cli.h"

#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include "gzip.h"
#include "io.h"
#include "table.h"

#define CLI_WRITE_NOTHING   0b00000000
#define CLI_WRITE_DTB       0b00000001
#define CLI_WRITE_TABLE     0b00000010
#define CLI_WRITE_MIGRATES  0b00000100

#define CLI_PARSE_PARTITION_RAW_ALLOWANCE_CHECK(name) \
    if (have_##name && require_##name & CLI_ARGUMENT_DISALLOW) { \
        fprintf(stderr, "CLI parse partition: Argument contains "#name" but it's not allowed: %s\n", arg); \
        return NULL; \
    } \
    if (!have_##name && require_##name & CLI_ARGUMENT_REQUIRED) { \
        fprintf(stderr, "CLI parse partition: Argument does not contain "#name" but it must be set: %s\n", arg); \
        return NULL; \
    }

#define CLI_PARSE_PARTITION_YOLO_MODE(arg) \
    cli_parse_partition_raw( \
        arg, \
        CLI_ARGUMENT_REQUIRED, \
        CLI_ARGUMENT_ALLOW_ABSOLUTE | CLI_ARGUMENT_ALLOW_RELATIVE, \
        CLI_ARGUMENT_ALLOW_ABSOLUTE, \
        CLI_ARGUMENT_ANY, \
        4 \
    )

#define CLI_PARSE_PARTITION_CLONE_MODE(arg) \
    cli_parse_partition_raw( \
        arg, \
        CLI_ARGUMENT_REQUIRED, \
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE, \
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE, \
        CLI_ARGUMENT_REQUIRED, \
        0 \
    )

#define CLI_PARSE_PARTITION_SAFE_MODE(arg) \
    cli_parse_partition_raw( \
        arg, \
        CLI_ARGUMENT_REQUIRED, \
        CLI_ARGUMENT_DISALLOW, \
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE, \
        CLI_ARGUMENT_REQUIRED, \
        4 \
    )

const char cli_mode_strings[][9] = {
    "yolo", // No check, don't update dtb
    "clone", 
    "safe",
    "fix",  // DTB to EPT
    "update",
    "snapshot"
};

const char cli_content_type_strings[][9] = {
    "auto",
    "dtb",
    "reserved",
    "disk"
};

struct cli_options cli_options = {
    .mode = CLI_MODE_YOLO,
    .content = CLI_CONTENT_TYPE_AUTO,
    .dry_run = false,
    .strict_device = false,
    .write = CLI_WRITE_DTB | CLI_WRITE_TABLE | CLI_WRITE_MIGRATES,
    .offset_reserved = TABLE_PARTITION_GAP_RESERVED + TABLE_PARTITION_BOOTLOADER_SIZE,
    .offset_dtb = DTB_PARTITION_OFFSET,
    .gap_partition = TABLE_PARTITION_GAP_GENERIC,
    .gap_reserved = TABLE_PARTITION_GAP_RESERVED,
    .size = 0,
    .target = NULL
};

#include "util.h"