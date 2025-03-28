#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/table.h"

ampart_status table_size_from_human_readable(table_size_t *restrict const size, char const *restrict const readable) {
    char *suffix = NULL;
    errno = 0;
    *size = strtoul(readable, &suffix, 0);
    if (errno) {
        log_error_with_errno("failed to parse table size/offset from human readable string '%s'", readable);
        return AMPART_BAD;
    }
    switch (suffix[0]) {
        case 'k':
        case 'K':
            *size *= 0x400UL;
            break;
        case 'm':
        case 'M':
            *size *= 0x100000UL;
            break;
        case 'g':
        case 'G':
            *size *= 0x40000000UL;
            break;
        case 't': // The fuck why would some one wants a size this big? But we support it any way
        case 'T':
            *size *= 0x10000000000UL;
            break;
        case 'p':
        case 'P':
            *size *= 0x4000000000000UL;
            break;
        case 'e':
        case 'E':
            *size *= 0x1000000000000000UL;
            break;
#if 0 /* These numbers are too big */
        case 'z':
        case 'Z':
            *size *= 0x400000000000000000;
            break;
        case 'y':
        case 'Y':
            *size *= 0x100000000000000000000;
            break;
#endif /* if 0 */
        default:
            break;
    }
    return AMPART_OK;
}

ampart_status table_masks_from_literal(table_masks_t *restrict const size, char const *restrict const literal) {
    errno = 0;
    *size = strtoul(literal, NULL, 0);
    if (errno) {
        log_error_with_errno("failed to parse table masks from human readable string '%s'", literal);
        return AMPART_BAD;
    }
    return AMPART_OK;
}
