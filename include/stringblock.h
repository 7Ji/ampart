#ifndef HAVE_STRINGBLOCK_H
#define HAVE_STRINGBLOCK_H
#include "common.h"

/* Structure */

struct 
    stringblock_helper {
        off_t   length;
        off_t   allocated_length;
        char *  stringblock;
    };

/* Function */

off_t 
    stringblock_append_string_force(
        struct stringblock_helper * shelper,
        char const *                string,
        size_t                      slength
    );

off_t 
    stringblock_append_string_safely(
        struct stringblock_helper * shelper,
        char const *                string,
        size_t                      slength
    );
    
off_t 
    stringblock_find_string(
        struct stringblock_helper const *   shelper, 
        char const *                        string
    );
    
off_t 
    stringblock_find_string_raw(
        char const *    sblock, 
        off_t           length, 
        char const *    string
    );

#endif