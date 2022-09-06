#ifndef HAVE_STRINGBLOCK_H
#define HAVE_STRINGBLOCK_H
#include "common.h"

struct stringblock_helper {
    off_t length, allocated_length;
    char *stringblock;
};

off_t stringblock_find_string(struct stringblock_helper *shelper, char *string);
off_t stringblock_append_string_force(struct stringblock_helper *shelper, char *string, size_t slength);
off_t stringblock_append_string_safely(struct stringblock_helper *shelper, char *string, size_t slength);
#endif