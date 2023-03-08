/* Self */

#include "gzip.h"

/* System */

#include <zlib.h>
#include <time.h>

/* Local */

#include "util.h"

/* Definition */

#define GZIP_FILE_FLAG_TEXT     0b00000001U
#define GZIP_FILE_FLAG_HCRC     0b00000010U
#define GZIP_FILE_FLAG_EXTRA    0b00000100U
#define GZIP_FILE_FLAG_NAME     0b00001000U
#define GZIP_FILE_FLAG_COMMENT  0b00010000U
#define GZIP_FILE_FLAG_RESERVED 0b11100000U
#define GZIP_DEFAULT_MEM_LEVEL  8U
#define GZIP_WRAPPER            16U // Add this to the window bit will cause it to wrap around and use a gzip container

/* Function */

static inline 
size_t 
gzip_unzip_no_header(
    uint8_t * const     in,
    size_t const        in_size,
    uint8_t * * const   out
){
    size_t allocated_size = util_nearest_upper_bound_with_multiply_ulong(in_size, 64, 4); // 4 times the size, nearest multiply of 64
    pr_error("unzip: Decompressing raw deflated data, in size %ld, allocated %ld\n", in_size, allocated_size);
    *out = malloc(allocated_size);
    if (!*out) {
        fputs("unzip: Failed to allocate memory for decompression\n", stderr);
        return 0;
    }
    z_stream s;
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) {
        free(*out);
        *out = NULL;
        fputs("unzip: Failed to initialize Z stream for decompression\n", stderr);
        return 0;
    }
    s.next_in = in;
    s.avail_in = in_size;
    s.next_out = *out;
    s.avail_out = allocated_size;
    uint8_t *temp_buffer;
    int r;
    while (true) {
        r = inflate(&s, Z_FINISH);
        switch (r) {
            case Z_STREAM_END:
                inflateEnd(&s);
                return s.next_out - *out;
            case Z_BUF_ERROR:
                s.avail_out += allocated_size;
                allocated_size *= 2;
                temp_buffer = realloc(*out, allocated_size);
                if (temp_buffer) {
                    s.next_out = temp_buffer + (s.next_out - *out);
                    *out = temp_buffer;
                    pr_error("unzip: Re-allocated memory, now %lu\n", allocated_size);
                } else {
                    free(*out);
                    *out = NULL;
                    inflateEnd(&s);
                    fputs("unzip: Failed to reallocate memory for decompression\n", stderr);
                    return 0;
                }
                break;
            default:
                free(*out);
                *out = NULL;
                inflateEnd(&s);
                pr_error("unzip: Unknown error when decompressing, errno: %d\n", r);
                return 0;
        }
    }
}

static inline 
unsigned int 
gzip_valid_header(
    uint8_t *   data
){
    const struct gzip_header *const gh = (const struct gzip_header *)data;
    if (gh->magic != GZIP_MAGIC) {
        pr_error("GZIP header: Magic is wrong, record %"PRIx16" != expected %"PRIx16"\n", gh->magic, GZIP_MAGIC);
        return 0;
    }
    if (gh->method != Z_DEFLATED) {
        pr_error("GZIP header: Compression method is wrong, record %"PRIu8" != expected %"PRIu8"\n", gh->method, Z_DEFLATED);
        return 0;
    }
    if (gh->file_flags & GZIP_FILE_FLAG_RESERVED) {
        pr_error("GZIP header: Reserved flag is set, record %"PRIx8" contains %"PRIx8"\n", gh->file_flags, GZIP_FILE_FLAG_RESERVED);
        return 0;
    }
    unsigned int offset = sizeof(struct gzip_header);
    if (gh->file_flags & GZIP_FILE_FLAG_EXTRA) {
        offset += 2 + *(uint16_t *)(data + sizeof(struct gzip_header));
    }
    if (gh->file_flags & GZIP_FILE_FLAG_NAME) {
        while (data[offset++]);
    }
    if (gh->file_flags & GZIP_FILE_FLAG_COMMENT) {
        while (data[offset++]);
    }
    if (gh->file_flags & GZIP_FILE_FLAG_HCRC) {
        offset += 2;
    }
    return offset;
}

size_t 
gzip_unzip(
    uint8_t * const     in,
    size_t const        in_size,
    uint8_t * * const   out
){
    const int offset = gzip_valid_header(in);
    if (!offset) {
        fputs("unzip: Gzip header invalid\n", stderr);
        return 0;
    }
    return gzip_unzip_no_header(in+offset, in_size-offset, out);
}

size_t
gzip_zip(
    uint8_t * const     in,
    size_t const        in_size,
    uint8_t * * const   out
){
    z_stream s;
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + GZIP_WRAPPER, GZIP_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(*out);
        *out = NULL;
        fputs("zip: Failed to initialize deflation for compression\n", stderr);
        return 0;
    }
    size_t allocated_size = deflateBound(&s, in_size);
    pr_error("zip: Compressing data to gzip, size %ld, allocated %ld\n", in_size, allocated_size);
    *out = malloc(allocated_size);
    if (!*out) {
        free(*out);
        *out = NULL;
        deflateEnd(&s);
        fputs("zip: Failed to allocate memory for compression\n", stderr);
        return 0;
    }
    s.next_in = in;
    s.avail_in = in_size;
    s.next_out = *out;
    s.avail_out = allocated_size;
    int r = deflate(&s, Z_FINISH);
    deflateEnd(&s);
    if (r == Z_STREAM_END) {
        *(uint32_t *)(*out + 4) = time(NULL);
        return s.next_out - *out;
    } else {
        free(*out);
        *out = NULL;
        pr_error("zip: Unexpected result of deflate: %d\n", r);
        return 0;
    }
}

/* gzip.c: Compressing and decompressing GZIP format stream */