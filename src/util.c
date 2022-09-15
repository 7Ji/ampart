/* Self */

#include "util.h"

/* System */

#include <string.h>

/* Definition */

#define UTIL_HUMAN_READABLE_SUFFIXES    "BKMGTPEZY"

/* Variable */

char const      util_human_readable_suffixes[] = UTIL_HUMAN_READABLE_SUFFIXES;
size_t const    util_human_readable_suffixes_length = strlen(UTIL_HUMAN_READABLE_SUFFIXES);

/* Function */

unsigned long 
util_nearest_upper_bound_ulong(
    unsigned long const value, 
    unsigned long const bound, 
    unsigned long const multiply
){
    return (value % bound) ? (value / bound * bound * multiply + bound) : (value * multiply);
}

long 
util_nearest_upper_bound_long(
    long const  value, 
    long const  bound, 
    long const  multiply
){
    return (value % bound) ? (value / bound * bound * multiply + bound) : (value * multiply);
}

double
util_size_to_human_readable(
    uint64_t const  size, 
    char * const    suffix
){
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

int 
util_get_base_of_integer_literal(
    char const * const  literal
){
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

size_t
util_human_readable_to_size(
    char const * const  literal
){
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
#if 0 /* These numbers are too big */
        case 'z':
        case 'Z':
            size *= 0x400000000000000000;
            break;
        case 'y':
        case 'Y':
            size *= 0x100000000000000000000;
            break;
#endif /* if 0 */
        default:
            break;
    }
    return size;
}

bool 
util_string_is_empty(
    char const * const  string
){
    if (string && string[0]) {
        return true;
    } else {
        return false;
    }
}

inline
void
util_log_title(
    const char * const  title
){
    fputs(title, stderr);
    fputs(": ", stderr);
}

/* util.c: Utility functions that do not lean to specific tasks */