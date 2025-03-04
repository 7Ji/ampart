/* Self */

#include "stringblock.h"

/* System */

#include <string.h>

/* Local */

#include "util.h"

/* Function */

off_t
stringblock_find_string_raw(
    char const * const  sblock,
    off_t const         length,
    char const * const  string
){
    if (string[0]) {
        for (off_t i=0; i<length; ++i) {
            if (sblock[i] && !strcmp(sblock + i, string)) {
                return i;
            }
        }
    } else {
        for (off_t i=0; i<length; ++i) {
            if (!sblock[i]) {
                return i;
            }
        }
    }
    return -1;
}

off_t
stringblock_find_string(
    struct stringblock_helper const * const shelper,
    char const * const                      string
){
    return stringblock_find_string_raw(shelper->stringblock, shelper->length, string);
}

off_t
stringblock_append_string_force(
    struct stringblock_helper * const   shelper,
    char const * const                  string,
    size_t                              slength
){
    if (!slength && !(slength = strlen(string))) {
        for (off_t i=0; i<shelper->length; ++i) {
            if (!shelper->stringblock[i]) {
                return i;
            }
        }
        return shelper->length - 1; // This should not be reached, but it's a failsafe
    }
    if (shelper->length + (off_t)slength + 1 > shelper->allocated_length) {
        shelper->allocated_length *= 2;
        char *buffer = realloc(shelper->stringblock, shelper->allocated_length);
        if (buffer) {
            shelper->stringblock = buffer;
        } else {
            prln_error("failed to re-allocate string buffer");
            return -1;
        }
    }
    const off_t offset = shelper->length;
    strcpy(shelper->stringblock+shelper->length, string);
    shelper->length += slength + 1;
    return offset;
}

off_t
stringblock_append_string_safely(
    struct stringblock_helper * const   shelper,
    char const * const                  string,
    size_t const                        slength
){
    off_t const offset = stringblock_find_string(shelper, string);
    if (offset>=0) {
        return offset;
    }
    return stringblock_append_string_force(shelper, string, slength);
}

/* stringblock.c: Stingblock-related functions, used for but not only for the string block found in DTB */