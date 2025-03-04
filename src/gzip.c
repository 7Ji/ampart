/* Self */

#include "gzip.h"

/* System */

#include <zlib.h>
#include <time.h>

/* Local */

#include "common.h"
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
    prln_info("decompressing raw deflated data, in size %ld, allocated %ld", in_size, allocated_size);
    *out = malloc(allocated_size);
    if (!*out) {
        prln_error_with_errno("failed to allocate memory for decompression");
        return 0;
    }
    z_stream s;
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) {
        free(*out);
        *out = NULL;
        prln_error("failed to initialize Z stream for decompression");
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
                    prln_warn("re-allocated memory, now %lu", allocated_size);
                } else {
                    free(*out);
                    *out = NULL;
                    inflateEnd(&s);
                    prln_error_with_errno("failed to reallocate memory for decompression");
                    return 0;
                }
                break;
            default:
                free(*out);
                *out = NULL;
                inflateEnd(&s);
                prln_error("unknown error when decompressing, errno: %d", r);
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
        prln_error("magic is wrong, record %"PRIx16" != expected %"PRIx16"", gh->magic, GZIP_MAGIC);
        return 0;
    }
    if (gh->method != Z_DEFLATED) {
        prln_error("compression method is wrong, record %"PRIu8" != expected %"PRIu8"", gh->method, Z_DEFLATED);
        return 0;
    }
    if (gh->file_flags & GZIP_FILE_FLAG_RESERVED) {
        prln_error("reserved flag is set, record %"PRIx8" contains %"PRIx8"", gh->file_flags, GZIP_FILE_FLAG_RESERVED);
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
        prln_error("gzip header invalid");
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
        prln_error("failed to initialize deflation for compression");
        return 0;
    }
    size_t allocated_size = deflateBound(&s, in_size);
    prln_error("compressing data to gzip, size %ld, allocated %ld", in_size, allocated_size);
    *out = malloc(allocated_size);
    if (!*out) {
        free(*out);
        *out = NULL;
        deflateEnd(&s);
        prln_error("failed to allocate memory for compression");
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
        prln_error("unexpected result of deflate: %d", r);
        return 0;
    }
}

/* gzip.c: Compressing and decompressing GZIP format stream */