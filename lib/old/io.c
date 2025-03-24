/* Self */

#include "io.h"

/* System */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <linux/fs.h>
#include <linux/limits.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

/* Local */

#include "cli.h"
#include "common.h"
#include "gzip.h"
#include "ept.h"

/* Function */

static inline
bool 
io_can_retry(
    int const   id
){
    switch (id) {
        case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
        case EWOULDBLOCK:
#endif
        case EINTR:
            return true;
        default:
            prln_error("can not retry with errno %d: %s", id, strerror(id));
            return false;
    }

}

int
io_read_till_finish(
    int const   fd,
    void *      buffer,
    size_t      size
){
    ssize_t r;
    while (size) {
        do {
            r = read(fd, buffer, size);
        } while (r == -1 && io_can_retry(errno));
        if (r == -1) {
            return 1;
        }
        size -= r;
        buffer = (unsigned char *)buffer + r;
    }
    return 0;
}

int 
io_write_till_finish(
    int const   fd,
    void *      buffer, 
    size_t      size
){
    ssize_t r;
    while (size) {
        do {
            r = write(fd, buffer, size);
        } while (r == -1 && io_can_retry(errno));
        if (r == -1) {
            return 1;
        }
        size -= r;
        buffer = (unsigned char *)buffer + r;
    }
    return 0;
}

char *
io_find_disk(
    char const * const  path
){
    char *path_disk = NULL;

    size_t const len_path = strnlen(path, PATH_MAX);
    if (!len_path || len_path >= PATH_MAX) {
        prln_error("path to disk '%s' is too long", path);
        goto result;
    }

    char *const path_dup = malloc(len_path + 1);
    if (!path_dup) {
        prln_error_with_errno("failed to allocate memory for duplicated path");
        goto result;
    }
    memcpy(path_dup, path, len_path);
    path_dup[len_path] = '\0';

    char const *const name = basename(path_dup);
    if (!name || name[0] == '\0') {
        prln_error("could not figure out base name of disk path '%s'", path);
        goto free_path;
    }
    prln_error("trying to find corresponding full disk drive of '%s' (name %s) so more advanced operations (partition migration, actual table manipulation, partprobe, etc) can be performed", path, name);

    size_t const len_name = strnlen(name, len_path);
    if (!len_name || len_name > NAME_MAX) {
        prln_error("could not figure out length of disk name '%s'", name);
        goto free_path;
    }

    struct stat st;
    if (stat(path, &st)) {
        prln_error_with_errno("failed to get stat of '%s'", path);
        goto free_path;
    }
    char major_minor[23];
    int len_devnode = snprintf(major_minor, 23, "%u:%u", major(st.st_rdev), minor(st.st_rdev));
    if (len_devnode < 0) {
        prln_error_with_errno("failed to snprintf to get major:minor dev name");
        goto free_path;
    } else if (len_devnode >= 23) {
        prln_error("name of devnode is impossibly long");
        goto free_path;
    }

    int dir_fd = open("/sys/block", O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) {
        prln_error_with_errno("failed to open /sys/block vdir");
        goto free_path;
    }

    int dup_dir_fd = dup(dir_fd);
    if (dup_dir_fd < 0) {
        prln_error_with_errno("failed to duplicate dir fd");
        goto free_path;
    }

    DIR *dir = fdopendir(dup_dir_fd);
    if (!dir) {
        prln_error_with_errno("failed to fdopendir /sys/block vdir");
        close(dup_dir_fd);
        goto close_fd;
    }

    char dev_file[6 + 2*NAME_MAX]; // 256 for disk+/, 256 for name+/, 4 for dev\0
    char dev_content[23];
    struct dirent * dir_entry;
    int fd;
    while ((dir_entry = readdir(dir))) {
        switch (dir_entry->d_name[0]) {
        case '\0':
        case '.':
            continue;
        default:
            break;
        }
        size_t len_entry = strnlen(dir_entry->d_name, NAME_MAX);
        if (!len_entry || len_entry > NAME_MAX) {
            prln_error_with_errno("entry name is too long or empty");
            goto close_dir;
        }
        memcpy(dev_file, dir_entry->d_name, len_entry);
        dev_file[len_entry] = '/';
        memcpy(dev_file + len_entry + 1, name, len_name);
        memcpy(dev_file + len_entry + 1 + len_name, "/dev", 5);
        fd = openat(dir_fd, dev_file, O_RDONLY);
        if (fd < 0) {
            prln_error_with_errno("failed to open dev file '%s' as read-only", dev_file);
            goto close_dir;
        }
        memset(dev_content, 0, 23);
        read(fd, dev_content, 23);
        close(fd);
        if (strncmp(major_minor, dev_content, 23)) {
            continue;
        }
        path_disk = malloc(6 + len_entry); // /dev/ is 5, name max 256
        if (!path_disk) {
            prln_error_with_errno("failed to allocate memory for result path");
            goto close_dir;
        }
        memcpy(path_disk, "/dev/", 5);
        memcpy(path_disk + 5, dir_entry->d_name, len_entry);
        path_disk[5 + len_entry] = '\0';
        break;
    }
    if (path_disk) {
        prln_error("corresponding disk drive for '%s' is '%s'", path, path_disk);
    } else {
        prln_error("could not find corresponding disk drive for '%s'", path);
    }
close_dir:
    if (closedir(dir)) {
        prln_error_with_errno("failed to close dir");
    }
close_fd:
    if (close(dir_fd)) {
        prln_error_with_errno("failed to close dir fd");
    }
free_path:
    free(path_dup);
result:
    return (char *)path_disk;
}

void
io_describe_target_type(
    struct io_target_type * type,
    char const * const      path
){
    char const *type_nice, *content_nice;
    switch (type->file) {
    case IO_TARGET_TYPE_FILE_BLOCKDEVICE:
        type_nice = "block device";
        break;
    case IO_TARGET_TYPE_FILE_REGULAR:
        type_nice = "regular file";
        break;
    case IO_TARGET_TYPE_FILE_UNSUPPORTED:
        type_nice = "unsupported file";
        break;
    }
    switch (type->content) {
    case IO_TARGET_TYPE_CONTENT_UNSUPPORTED:
        content_nice = "unsupported";
        break;
    case IO_TARGET_TYPE_CONTENT_DTB:
        content_nice = "DTB";
        break;
    case IO_TARGET_TYPE_CONTENT_RESERVED:
        content_nice = "reserved partition";
        break;
    case IO_TARGET_TYPE_CONTENT_DISK:
        content_nice = "full disk";
        break;
    }
    prln_info("%s is a %s with a size of %zu bytes, and contains the content of %s", path ? path : "target", type_nice, type->size, content_nice);
}

static inline
int
io_identify_target_type_get_basic_stat(
    struct io_target_type * const   type,
    int const                       fd,
    char const * const              path
){
    struct stat st;
    if (fstat(fd, &st)) {
        prln_error_with_errno("failed to get stat of '%s'", path);
        return 1;
    }
    if (S_ISBLK(st.st_mode)) {
        prln_info("'%s' is a block device, getting its size via ioctl", path);
        type->file = IO_TARGET_TYPE_FILE_BLOCKDEVICE;
        if (ioctl(fd, BLKGETSIZE64, &type->size)) {
            prln_error_with_errno("failed to get size of '%s' via ioctl", path);
            return 2;
        }
    } else if (S_ISREG(st.st_mode)) {
        prln_info("'%s' is a regular file, getting its size via stat", path);
        type->file = IO_TARGET_TYPE_FILE_REGULAR;
        type->size = st.st_size;
    } else {
        prln_info("'%s' is neither a regular file nor a block device, assuming its size as 0", path);
        type->file = IO_TARGET_TYPE_FILE_UNSUPPORTED;
        type->size = 0;
    }
    prln_error("size of '%s' is %zu", path, type->size);
    return 0;
}

static inline
enum io_target_type_content
io_identify_target_type_guess_content_from_size(
    size_t const    size
){
    if (size > DTB_PARTITION_SIZE) {
        if (size > EPT_PARTITION_RESERVED_SIZE) {
            prln_info("size larger than reserved partition, considering content full disk");
            return IO_TARGET_TYPE_CONTENT_DISK;
        } else if (size == EPT_PARTITION_RESERVED_SIZE) {
            prln_info("size equals reserved partition, considering content reserved partition");
            return IO_TARGET_TYPE_CONTENT_RESERVED;
        } else {
            prln_info("size between reserved partition and DTB partition, considering content unsupported");
            return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else if (size == DTB_PARTITION_SIZE) {
        prln_info("size equals DTB partition, consiering content DTB");
        return IO_TARGET_TYPE_CONTENT_DTB;
    } else {
        prln_info("size too small, considering content DTB");
        return IO_TARGET_TYPE_CONTENT_DTB;
    }
}

static inline
enum io_target_type_content
io_identify_target_type_guess_content_by_read(
    int const       fd,
    size_t const    size
){
    if (size) {
        uint8_t buffer[4] = {0};
        if (read(fd, buffer, 4) == 4) {
            switch (*(uint32_t *)buffer) {
                case 0:
                    prln_info("content type full disk, as pure 0 in the header was found");
                    return IO_TARGET_TYPE_CONTENT_DISK;
                    break;
                case EPT_HEADER_MAGIC_UINT32:
                    prln_info("content type reserved partition, as EPT magic was found");
                    return IO_TARGET_TYPE_CONTENT_RESERVED;
                    break;
                case DTB_MAGIC_MULTI:
                case DTB_MAGIC_PLAIN:
                    prln_info("content type DTB, as DTB magic was found");
                    return IO_TARGET_TYPE_CONTENT_DTB;
                    break;
                default:
                    if (*(uint16_t *)buffer == GZIP_MAGIC) {
                        prln_info("content type DTB, as gzip magic was found");
                        return IO_TARGET_TYPE_CONTENT_DTB;
                        break;
                    }
                    prln_info("content type unsupported due to magic unrecognisable");
                    return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
                    break;
            }
        } else {
            prln_info("content type unsupported due to read failure");
            return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else {
        prln_info("content type unsupported due to size too small");
        return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
    }
}

static inline
void
io_identify_target_type_guess_content(
    struct io_target_type * const   type,
    int const                       fd
){
    prln_info("guessing content type by size");
    enum io_target_type_content ctype_size = io_identify_target_type_guess_content_from_size(type->size);
    prln_info("getting content type by reading");
    enum io_target_type_content ctype_read = io_identify_target_type_guess_content_by_read(fd, type->size);
    if (ctype_read == ctype_size) {
        prln_info("by-read and by-size results are the same, using any");
        type->content = ctype_read;
    } else if (ctype_read == IO_TARGET_TYPE_CONTENT_UNSUPPORTED) {
        prln_info("by-read result unsupported, using by-size result");
        type->content = ctype_size;
    } else if (ctype_size == IO_TARGET_TYPE_CONTENT_UNSUPPORTED) {
        prln_info("by-size result unsupported, using by-read result");
        type->content = ctype_read;
    } else {
        prln_info("both by-read and by-size results valid, using by-read result");
        type->content = ctype_read;
    }
}

int
io_identify_target_type(
    struct io_target_type * const   type,
    char const * const              path
){
    if (!type) {
        return 1;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        prln_error_with_errno("failed to open '%s' as read-only", path);
        return 2;
    }
    memset(type, 0, sizeof *type);
    if (io_identify_target_type_get_basic_stat(type, fd, path)) {
        close(fd);
        return 3;
    }
    io_identify_target_type_guess_content(type, fd);
    close(fd);
    return 0;
}

off_t
io_seek_dtb(
    int const   fd
){
    off_t offset;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_DTB:
            offset = 0;
            break;
        case CLI_CONTENT_TYPE_RESERVED:
            offset = cli_options.offset_dtb;
            break;
        case CLI_CONTENT_TYPE_DISK:
            offset = cli_options.offset_reserved + cli_options.offset_dtb;
            break;
        default:
            prln_error("ilegal target content type (auto), this should not happen");
            return -1;
    }
    prln_info("seeking to %ld", offset);
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) {
        prln_error_with_errno("failed to seek for DTB");
        return -1;
    }
    return offset;
}

off_t
io_seek_ept(
    int const   fd
){
    off_t offset;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_RESERVED:
            offset = 0;
            break;
        case CLI_CONTENT_TYPE_DISK:
            offset = cli_options.offset_reserved;
            break;
        default:
            prln_error("ilegal target content type (%s), this should not happen", cli_mode_strings[cli_options.content]);
            return -1;
    }
    prln_info("seeking to %ld", offset);
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) {
        prln_error_with_errno("failed to seek for EPT");
        return -1;
    }
    return offset;
}

static inline
int
io_seek_and_read(
    int const       fd,
    off_t const     offset,
    void * const    buffer,
    size_t          size

){
    off_t r_seek = lseek(fd, offset, SEEK_SET);
    if (r_seek < 0) {
        prln_error("failed to seek");
        return 1;
    }
    if (r_seek != offset) {
        prln_error("seeked offset different from expected: result 0x%lx, expected 0x%lx", r_seek, offset);
        return 2;
    }
    if (io_read_till_finish(fd, buffer, size)) {
        prln_error("failed to read");
        return 3;
    }
    return 0;
}

static inline
int
io_seek_and_write(
    int const       fd,
    off_t const     offset,
    void * const    buffer,
    size_t          size

){
    off_t r_seek = lseek(fd, offset, SEEK_SET);
    if (r_seek < 0) {
        return 1;
    }
    if (r_seek != offset) {
        return 2;
    }
    if (io_write_till_finish(fd, buffer, size)) {
        return 3;
    }
    return 0;
}

int
io_migrate_recursive(
    struct io_migrate_helper *const mhelper,
    struct io_migrate_entry *const msource
){
    struct io_migrate_entry *const mtarget = mhelper->entries + msource->target;
    if (mtarget == msource) {
        prln_error("bad plan! target = source on block %lu", msource - mhelper->entries);
        return -1;
    }
    msource->pending = false;
    if (mtarget->pending) {
        prln_info("reading block %u", msource->target);
        memset(mhelper->buffer_sub, 0, mhelper->block * sizeof *mhelper->buffer_sub);
        if (io_seek_and_read(mhelper->fd, (off_t)mhelper->block * (off_t)msource->target, mhelper->buffer_sub, mhelper->block)) {
            prln_error("failed to seek and read block %u", msource->target);
            return 2;
        }
    }
    prln_info("writing block %u", msource->target);
    if (io_seek_and_write(mhelper->fd, (off_t)mhelper->block * (off_t)msource->target, mhelper->buffer_main, mhelper->block)) {
        prln_error("failed to seek and read block %u", msource->target);
        return 3;
    }
    if (mtarget->pending) {
        uint8_t *buffer = mhelper->buffer_main;
        mhelper->buffer_main = mhelper->buffer_sub;
        mhelper->buffer_sub = buffer;
        if (io_migrate_recursive(mhelper, mtarget)) {
            prln_error("failed to recursively migrate block %u", msource->target);
            return 4;
        }
    }
    return 0;
}

int
io_migrate(
    struct io_migrate_helper *const mhelper
){
    if (!mhelper || !mhelper->count || mhelper->fd < 0) {
        return -1;
    }
    prln_warn("start migrating, block size 0x%x, total blocks %u", mhelper->block, mhelper->count);
    if (!(mhelper->buffer_main = malloc(mhelper->block * sizeof *mhelper->buffer_main))) {
        prln_error_with_errno("failed to allocate memory for main buffer");
        return 1;
    }
    if (!(mhelper->buffer_sub = malloc(mhelper->block * sizeof *mhelper->buffer_sub))) {
        prln_error_with_errno("failed to allocate memory for sub buffer");
        free(mhelper->buffer_main);
        return 2;
    }
    struct io_migrate_entry *mentry;
    for (uint32_t i = 0; i < mhelper->count; ++i) {
        mentry = mhelper->entries + i;
        if (mentry->pending) {
            prln_info("migrating block %u", i);
            memset(mhelper->buffer_main, 0, mhelper->block * sizeof *mhelper->buffer_main);
            prln_info("reading block %u", i);
            if (io_seek_and_read(mhelper->fd, (off_t)mhelper->block * (off_t)i, mhelper->buffer_main, mhelper->block)) {
                prln_error("failed to seek and read block %u", i);
                free(mhelper->buffer_main);
                free(mhelper->buffer_sub);
                return 3;
            }
            if (io_migrate_recursive(mhelper, mentry)) {
                prln_error("failed to recursively migrate block %u", i);
                free(mhelper->buffer_main);
                free(mhelper->buffer_sub);
                return 4;
            }
        }
    }
    free(mhelper->buffer_main);
    free(mhelper->buffer_sub);
    return 0;
}

int
io_rereadpart(
    int fd
){
    if (fd < 0) {
        prln_error("file descriptor not >= 0 (%d), give up", fd);
        return -1;
    }
    if (ioctl(fd, BLKRRPART) == -1) {
        prln_error_with_errno("failed to send ioctl BLKRRPART to kernel");
        return 1;
    }
    prln_info("reread partitions success");
    return 0;
}

/* io.c: IO-related functions, type-recognition is also here */