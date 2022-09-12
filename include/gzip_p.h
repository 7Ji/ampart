#include "gzip.h"

#include <zlib.h>
#include <time.h>

#include "util.h"

#define GZIP_FILE_FLAG_TEXT     0b00000001U
#define GZIP_FILE_FLAG_HCRC     0b00000010U
#define GZIP_FILE_FLAG_EXTRA    0b00000100U
#define GZIP_FILE_FLAG_NAME     0b00001000U
#define GZIP_FILE_FLAG_COMMENT  0b00010000U
#define GZIP_FILE_FLAG_RESERVED 0b11100000U
#define GZIP_DEFAULT_MEM_LEVEL  8U
#define GZIP_WRAPPER            16U // Add this to the window bit will cause it to wrap around and use a gzip container