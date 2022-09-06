#include "stringblock_p.h"

struct stringblock_helper {
    off_t length, allocated_length;
    char *stringblock;
};

off_t stringblock_find_string(struct stringblock_helper *shelper, char *string) {
    char *sblock = shelper->stringblock;
    if (string[0]) {
        for (off_t i=0; i<shelper->length; ++i) {
            if (sblock[i] && !strcmp(sblock + i, string)) {
                return i;
            }
        }
    } else {
        for (off_t i=0; i<shelper->length; ++i) {
            if (!sblock[i]) {
                return i;
            }
        }
    }
    return -1;
}

off_t stringblock_append_string_force(struct stringblock_helper *shelper, char *string, size_t slength) {
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
            fputs("Stringblock: Failed to re-allocate string buffer\n", stderr);
            return -1;
        }
    }
    off_t offset = shelper->length;
    strcpy(shelper->stringblock+shelper->length, string);
    shelper->length += slength + 1;
    return offset;
}

off_t stringblock_append_string_safely(struct stringblock_helper *shelper, char *string, size_t slength) {
    off_t offset = stringblock_find_string(shelper, string);
    if (offset>=0) {
        return offset;
    }
    return stringblock_append_string_force(shelper, string, slength);
}

// int main() {
//     struct stringblock_helper shelper;
//     shelper.length = 24;
//     shelper.allocated_length = util_nearest_upper_bound_ulong(24, 4096, 1);
//     shelper.stringblock = malloc(shelper.allocated_length);
//     if (shelper.stringblock) {
//         memcpy(
//             shelper.stringblock,
//             "Whatever\0Goodbyte\0What?",
//             24
//         );
//     } else {
//         puts("Failed to alloc");
//         return 1;
//     }
//     off_t offset = stringblock_append_string_safely(&shelper, "hat?", 0);
//     printf("%ld, %ld, %ld\n", offset, shelper.length, shelper.allocated_length);
//     puts(shelper.stringblock+offset);
//     return 0;
// }