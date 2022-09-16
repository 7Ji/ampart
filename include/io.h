#ifndef HAVE_IO_H
#define HAVE_IO_H
#include "common.h"

/* System */

#include <sys/types.h>

/* Local */

#include "dtb.h"

/* Enumerable */

enum 
    io_target_type_content{
        IO_TARGET_TYPE_CONTENT_UNSUPPORTED,
        IO_TARGET_TYPE_CONTENT_DTB,
        IO_TARGET_TYPE_CONTENT_RESERVED,
        IO_TARGET_TYPE_CONTENT_DISK
    };

enum 
    io_target_type_file {
        IO_TARGET_TYPE_FILE_UNSUPPORTED,
        IO_TARGET_TYPE_FILE_REGULAR,
        IO_TARGET_TYPE_FILE_BLOCKDEVICE
    };

/* Structure */

struct 
    io_target_type {
        enum io_target_type_content content;
        enum io_target_type_file    file;
        enum dtb_type               dtb;
        size_t                      size;
    };

/* Function */

void 
    io_describe_target_type(
        struct io_target_type * type,
        char const * const      path
    );

char *
    io_find_disk(
        char const *    path
    );

struct io_target_type *
    io_identify_target_type(
        char const *    path
    );
    
int 
    io_read_till_finish(
        int     fd, 
        void *  buffer, 
        size_t  size
    );

off_t
    io_seek_dtb(
        int const   fd
    );

off_t
    io_seek_ept(
        int const   fd
    );

int 
    io_write_till_finish(
        int     fd,
        void *  buffer,
        size_t  size
    );

#endif