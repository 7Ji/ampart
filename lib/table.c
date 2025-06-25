#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/table.h"

ampart_status table_size_from_human_readable(table_size_t *restrict const size, char const *restrict const readable) {
    char *suffix = NULL;
    errno = 0;
    unsigned long parsed = strtoul(readable, &suffix, 0);
#if TABLE_SIZE_MAX < ULONG_MAX
    if (parsed > TABLE_SIZE_MAX) {
        log_error("parsed size 0x%lx larger than TABLE_SIZE_MAX (0x%"PRI_TABLE_SIZE_X")", parsed, TABLE_SIZE_MAX);
        return AMPART_BAD;
    }
#endif
    if (errno) {
        log_error_with_errno("failed to parse table size/offset from human readable string '%s'", readable);
        return AMPART_BAD;
    }
    switch (suffix[0]) {
        case 'k':
        case 'K':
            if (parsed > TABLE_SIZE_MAX / 0x400UL) {
                log_error("parsed size would overflow with K suffix");
                return AMPART_BAD;
            }
            parsed *= 0x400UL;
            break;
        case 'm':
        case 'M':
            if (parsed > TABLE_SIZE_MAX / 0x100000UL) {
                log_error("parsed size would overflow with M suffix");
                return AMPART_BAD;
            }
            parsed *= 0x100000UL;
            break;
        case 'g':
        case 'G':
            if (parsed > TABLE_SIZE_MAX / 0x40000000UL) {
                log_error("parsed size would overflow with G suffix");
                return AMPART_BAD;
            }
            parsed *= 0x40000000UL;
            break;
        case 't': // The fuck why would some one wants a size this big? But we support it any way
        case 'T':
            if (parsed > TABLE_SIZE_MAX / 0x10000000000UL) {
                log_error("parsed size would overflow with T suffix");
                return AMPART_BAD;
            }
            parsed *= 0x10000000000UL;
            break;
        case 'p':
        case 'P':
            if (parsed > TABLE_SIZE_MAX / 0x4000000000000UL) {
                log_error("parsed size would overflow with P suffix");
                return AMPART_BAD;
            }
            parsed *= 0x4000000000000UL;
            break;
        case 'e':
        case 'E':
            if (parsed > TABLE_SIZE_MAX / 0x1000000000000000UL) {
                log_error("parsed size would overflow with E suffix");
                return AMPART_BAD;
            }
            parsed *= 0x1000000000000000UL;
            break;
#if 0 /* These numbers are too big */
        case 'z':
        case 'Z':
            parsed *= 0x400000000000000000;
            break;
        case 'y':
        case 'Y':
            parsed *= 0x100000000000000000000;
            break;
#endif /* if 0 */
        case ':':
        case ' ':
        case '\t':
        case '\b':
        case '\r':
        case '\n':
        case '\0':
            break;
        default:
            log_error("size suffix '%c' (0x%x) invalid", suffix[0], suffix[0]);
            return AMPART_BAD;
    }
    *size = parsed;
    return AMPART_OK;
}

ampart_status table_masks_from_literal(table_masks_t *restrict const size, char const *restrict const literal) {
    errno = 0;
    unsigned long parsed = strtoul(literal, NULL, 0);
    if (errno) {
        log_error_with_errno("failed to parse table masks from literal '%s'", literal);
        return AMPART_BAD;
    }
#if TABLE_MASKS_MAX < ULONG_MAX
    if (parsed > TABLE_MASKS_MAX) {
        log_error("parsed masks 0x%lx larger than TABLE_MASKS_MAX (0x%"PRI_TABLE_MASKS_X")", parsed, TABLE_MASKS_MAX);
        return AMPART_BAD;
    }
#endif
    *size = parsed;
    return AMPART_OK;
}
