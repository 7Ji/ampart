#ifndef HAVE_GZIP_H
#define HAVE_GZIP_H
#include "common.h"
#define GZIP_MAGIC              0x8b1fU

struct gzip_header {
    uint16_t magic;
    uint8_t method;
    uint8_t file_flags;
    uint8_t timestamps[4];
    uint8_t compress_flags;
    uint8_t os;
};

size_t gzip_unzip(uint8_t *in, const size_t in_size, uint8_t **out);
size_t gzip_zip(uint8_t *in, const size_t in_size, uint8_t **out);
#endif