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
        const char *                string,
        size_t                      slength
    );

off_t 
    stringblock_append_string_safely(
        struct stringblock_helper * shelper,
        const char *                string,
        size_t                      slength
    );
    
off_t 
    stringblock_find_string(
        const struct stringblock_helper *   shelper, 
        const char *                        string
    );
    
off_t 
    stringblock_find_string_raw(
        const char *    sblock, 
        off_t           length, 
        const char *    string
    );

#endif