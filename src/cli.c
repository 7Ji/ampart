#include "cli_p.h"

#define CLI_ARGUMENT_ANY                0b00000000
#define CLI_ARGUMENT_DISALLOW           0b10000000
#define CLI_ARGUMENT_REQUIRED           0b00000001
#define CLI_ARGUMENT_ALLOW_ABSOLUTE     0b00000010
#define CLI_ARGUMENT_ALLOW_RELATIVE     0b00000100

struct cli_partition_definer {
    char name[16];
    bool relative_offset, relative_size;
    uint64_t offset, size;
    uint32_t masks;
};

struct cli_partition_definer *cli_parse_partition(const char *arg, uint8_t require_name, uint8_t require_offset, uint8_t require_size, uint8_t require_masks) {
    const char *c;
    const char *seperators[3];
    unsigned int seperator_id = 0;
    for (c=arg; *c; ++c) {
        if (*c == ':') {
            seperators[seperator_id++] = c;
        }
        if (seperator_id == 3) {
            break;
        }
    }
    if (seperator_id < 3) {
        fprintf(stderr, "CLI parse partition: Argument too short: %s\n", arg);
        return NULL;
    }
    bool have_name = seperators[0] != arg;
    if (have_name && require_name & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains name but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_name && require_name & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contains name but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_offset = seperators[1] != seperators[0];
    if (have_offset && require_offset & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains offset but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_offset && require_offset & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contain offset but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_size = seperators[2] != seperators[1];
    if (have_size && require_size & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains size but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_size && require_size & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contain size but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_masks = *(seperators[2] + 1);
    if (have_masks && require_masks & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains masks but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_masks && require_masks & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contains mask but it must be set: %s\n", arg);
        return NULL;
    }
    // if (!*(seperators[2]+1) && require_masks) {
    //     fprintf(stderr, "Argument does not contain masks but it's required");
    //     return;
    // }
    // if (seperators[2] - seperators[1] == 1 && require_size) {
    //     fprintf(stderr, "Argument does not contain size but it's required");
    //     return;
    // }
    // if (seperators[1] - seperators[0] == 1 && require_offset) {
    //     fprintf(stderr, "Argument does not contain offset but it's required");
    //     return;
    // }
    // if (seperators[0] == arg && require_name) {
    //     fprintf(stderr, "Argument does not contain name but it's required");
    //     return;
    // }    
}

void cli_parse_partition_safe() {

}