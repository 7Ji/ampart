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

struct 
    io_target_type {
        enum io_target_type_content content;
        enum io_target_type_file    file;
        enum dtb_type               dtb;
        size_t                      size;
    };


int 
    io_read_till_finish(
        int     fd, 
        void *  buffer, 
        size_t  size
    );

int 
    io_write_till_finish(
        int     fd,
        void *  buffer,
        size_t  size
    );

void io_describe_target_type(struct io_target_type *type, const char *const path);

char *
    io_find_disk(
        const char *    path
    );


struct io_target_type *io_identify_target_type(const char *const path);

#endif