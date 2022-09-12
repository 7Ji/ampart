#include "util_p.h"

unsigned long util_nearest_upper_bound_ulong(unsigned long value, unsigned long bound, unsigned long multiply) {
    return (value % bound) ? (value / bound * bound * multiply + bound) : (value * multiply);
}

long util_nearest_upper_bound_long(long value, long bound, long multiply) {
    return (value % bound) ? (value / bound * bound * multiply + bound) : (value * multiply);
}

double util_size_to_human_readable(uint64_t size, char *suffix) {
    unsigned int i;
    double size_human = size;
    for (i=0; i<util_human_readable_suffixes_length; ++i) {
        if (size_human <= 1024) {
            break;
        }
        size_human /= 1024;
    }
    *suffix = util_human_readable_suffixes[i];
    return size_human;
}

int util_get_base_of_integer_literal(const char *literal) {
    switch (literal[0]) {
        case '0':
            switch (literal[1]) {
                case 'b':
                case 'B':
                    return 2;
                case 'x':
                case 'X':
                    return 16;
                default:
                    return 8;
            }
            break;
        default:
            return 10;
    }
}

size_t util_human_readable_to_size(const char *literal) {
    char *suffix = NULL;
    size_t size = strtoul(literal, &suffix, 0);
    switch (suffix[0]) {
        case 'k':
        case 'K':
            size *= 0x400UL;
            break;
        case 'm':
        case 'M':
            size *= 0x100000UL;
            break;
        case 'g':
        case 'G':
            size *= 0x40000000UL;
            break;
        case 't': // The fuck why would some one wants a size this big? But we support it any way
        case 'T':
            size *= 0x10000000000UL;
            break;
        case 'p':
        case 'P':
            size *= 0x4000000000000UL;
            break;
        case 'e':
        case 'E':
            size *= 0x1000000000000000UL;
            break;
#if 0
        case 'z':
        case 'Z':
            size *= 0x400000000000000000;
            break;
        case 'y':
        case 'Y':
            size *= 0x100000000000000000000;
            break;
#endif
        default:
            break;
    }
    return size;
}