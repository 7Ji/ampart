#include "cli_p.h"



void cli_parse_partition(const char *arg, bool require_name, bool require_offset, bool require_size, bool require_masks) {
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
        fprintf(stderr, "Argument too short: %s", arg);
        return;
    }
    if (!*(seperators[2]+1) && require_masks) {
        fprintf(stderr, "Argument does not contain masks but it's required");
        return;
    }
    if (seperators[2] - seperators[1] == 1 && require_size) {
        fprintf(stderr, "Argument does not contain size but it's required");
        return;
    }
    if (seperators[1] - seperators[0] == 1 && require_offset) {
        fprintf(stderr, "Argument does not contain offset but it's required");
        return;
    }
    if (seperators[0] == arg) {
        fprintf(stderr, "Argument does not contain name but it's required");
        return;
    }
    
}