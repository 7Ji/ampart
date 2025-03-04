#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include "common.h"

/* Function */

int 
    util_get_base_of_integer_literal(
        char const *    literal
    );

size_t 
    util_human_readable_to_size(
        char const *    literal
    );

long 
    util_nearest_upper_bound_long(
        long    value,
        long    bound
    );

unsigned long 
    util_nearest_upper_bound_ulong(
        unsigned long   value,
        unsigned long   bound
    );

long 
    util_nearest_upper_bound_with_multiply_long(
        long    value, 
        long    bound, 
        long    multiply
    );

unsigned long 
    util_nearest_upper_bound_with_multiply_ulong(
        unsigned long   value,
        unsigned long   bound,
        unsigned long   multiply
    );

uint32_t
    util_safe_partitions_count(
        uint32_t const  count
    );

double 
    util_size_to_human_readable(
        uint64_t    size, 
        char *      suffix
    );

size_t
    util_size_to_human_readable_int(
        uint64_t const  size, 
        char * const    suffix
    );

bool 
    util_string_is_empty (
        char const *    string
    );
#endif