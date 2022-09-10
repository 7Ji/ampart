#include "util.h"
#include <string.h>

#define UTIL_HUMAN_READABLE_SUFFIXES    "BKMGTPBEZY"

const char util_human_readable_suffixes[] = UTIL_HUMAN_READABLE_SUFFIXES;
const size_t util_human_readable_suffixes_length = strlen(UTIL_HUMAN_READABLE_SUFFIXES);