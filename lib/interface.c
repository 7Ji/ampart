#define _GNU_SOURCE
#include "priv/log.h"

FILE *file_log = NULL;

ampart_status ampart_init(FILE * log_out) {
    file_log = log_out;
    return AMPART_OK;
}

ampart_status ampart_free() {
    return AMPART_OK;
}
