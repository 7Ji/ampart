#include "cli.h"

#include <string.h>
#include <getopt.h>

#include "table.h"

const char cli_mode_strings[][9] = {
    "yolo", // No check, don't update dtb
    "clone", 
    "safe",
    "fix",  // DTB to EPT
    "update",
    "snapshot"
};

const char cli_type_strings[][9] = {
    "auto",
    "dtb",
    "reserved",
    "disk"
};

#define CLI_WRITE_NOTHING   0b00000000
#define CLI_WRITE_DTB       0b00000001
#define CLI_WRITE_TABLE     0b00000010
#define CLI_WRITE_MIGRATES  0b00000100

struct cli_options cli_options = {0};



#include "util.h"