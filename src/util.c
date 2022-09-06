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