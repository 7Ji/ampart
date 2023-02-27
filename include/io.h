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
    io_migrate_entry{
#ifdef IO_MIGRATE_SEPERATE_CIRCLE_PLAIN
        uint8_t *   buffer;
#endif
        uint32_t    target;
        bool        pending;
    };

struct
    io_migrate_helper{
        struct io_migrate_entry *   entries;
        uint32_t                    count;
        uint32_t                    block;
#ifndef IO_MIGRATE_SEPERATE_CIRCLE_PLAIN
        uint8_t *                   buffer_main;
        uint8_t *                   buffer_sub;
        int                         fd;
#endif
    };

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
        char const *            path
    );

char *
    io_find_disk(
        char const *    path
    );

int
    io_identify_target_type(
        struct io_target_type * type,
        char const *            path
    );

int
    io_migrate(
        struct io_migrate_helper *  mhelper
    );
    
int 
    io_read_till_finish(
        int     fd, 
        void *  buffer, 
        size_t  size
    );

int
    io_rereadpart(
        int fd
    );

off_t
    io_seek_dtb(
        int fd
    );

off_t
    io_seek_ept(
        int fd
    );

int 
    io_write_till_finish(
        int     fd,
        void *  buffer,
        size_t  size
    );

#endif