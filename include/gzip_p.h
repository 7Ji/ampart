#include "gzip.h"

#include <zlib.h>
#include <time.h>

#include "util.h"

#define GZIP_FILE_FLAG_TEXT     0b00000001
#define GZIP_FILE_FLAG_HCRC     0b00000010
#define GZIP_FILE_FLAG_EXTRA    0b00000100
#define GZIP_FILE_FLAG_NAME     0b00001000
#define GZIP_FILE_FLAG_COMMENT  0b00010000
#define GZIP_FILE_FLAG_RESERVED 0b11100000
#define GZIP_MAGIC              0x8b1f
#define GZIP_DEFAULT_MEM_LEVEL  8
#define GZIP_WRAPPER            16 // Add this to the window bit will cause it to wrap around and use a gzip container

struct gzip_header {
    uint16_t magic;
    uint8_t method;
    uint8_t file_flags;
    uint8_t timestamps[4];
    uint8_t compress_flags;
    uint8_t os;
};