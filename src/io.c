/* Self */

#include "io.h"

/* System */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <linux/fs.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

/* Local */

#include "cli.h"
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
            pr_error("IO: can not retry with errno %d: %s\n", id, strerror(id));
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
    const char* const name = basename(path);
    pr_error("IO find disk: Trying to find corresponding full disk drive of '%s' (name %s) so more advanced operations (partition migration, actual table manipulation, partprobe, etc) can be performed\n", path, name);
    const size_t len_name = strlen(name);
    struct stat st;
    if (stat(path, &st)) {
        pr_error("IO find disk: Failed to get stat of '%s', errno: %d, error: %s\n", path, errno, strerror(errno));
        return NULL;
    }
    char major_minor[23];
    snprintf(major_minor, 23, "%u:%u\n", major(st.st_rdev), minor(st.st_rdev));
    DIR *dir = opendir("/sys/block");
    if (!dir) {
        return NULL;
    }
    char dev_file[17 + 2*NAME_MAX] = "/sys/block/"; // 11 for /sys/block/, 256 for disk+/, 256 for name+/, 4 for dev\0 
    char dev_content[23];
    struct dirent * dir_entry;
    int fd;
    size_t len_entry;
    while ((dir_entry = readdir(dir))) {
        if (dir_entry->d_name[0] && dir_entry->d_name[0] != '.') {
            len_entry = strlen(dir_entry->d_name);
            snprintf(dev_file + 11, 6 + len_name + len_entry, "%s/%s/dev", dir_entry->d_name, name);
            fd = open(dev_file, O_RDONLY);
            if (fd < 0) {
                continue;
            }
            memset(dev_content, 0, 23);
            read(fd, dev_content, 23);
            close(fd);
            if (!strcmp(major_minor, dev_content)) {
                char *path_real = malloc((6 + len_entry) * sizeof *path_real); // /dev/ is 5, name max 256
                if (!path_real) {
                    return NULL;
                }
                snprintf(path_real, 6 + len_entry, "/dev/%s", dir_entry->d_name);
                pr_error("IO find disk: Corresponding disk drive for '%s' is '%s'\n", path, path_real);
                closedir(dir);
                return path_real;
            }
        }
    }
    pr_error("IO find disk: Could not find corresponding disk drive for '%s'\n", path);
    closedir(dir);
    return NULL;
}

void 
io_describe_target_type(
    struct io_target_type * type,
    char const * const      path
){
    if (path) {
        pr_error("IO identify target type: '%s' is a ", path);
    } else {
        fputs("IO identify target type: target is a ", stderr);
    }
    switch (type->file) {
        case IO_TARGET_TYPE_FILE_BLOCKDEVICE:
            fputs("block device", stderr);
            break;
        case IO_TARGET_TYPE_FILE_REGULAR:
            fputs("regular file", stderr);
            break;
        case IO_TARGET_TYPE_FILE_UNSUPPORTED:
            fputs("unsupported file", stderr);
            break;
    }
    pr_error(" with a size of %zu bytes, and contains the content of ", type->size);
    switch (type->content) {
        case IO_TARGET_TYPE_CONTENT_UNSUPPORTED:
            fputs("unsupported", stderr);
            break;
        case IO_TARGET_TYPE_CONTENT_DTB:
            fputs("DTB", stderr);
            break;
        case IO_TARGET_TYPE_CONTENT_RESERVED:
            fputs("reserved partition", stderr);
            break;
        case IO_TARGET_TYPE_CONTENT_DISK:
            fputs("full disk", stderr);
            break;
    }
    fputc('\n', stderr);
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
        pr_error("IO identify target type: Failed to get stat of '%s', errno: %d, error: %s\n", path, errno, strerror(errno));
        return 1;
    }
    if (S_ISBLK(st.st_mode)) {
        pr_error("IO identify target type: '%s' is a block device, getting its size via ioctl\n", path);
        type->file = IO_TARGET_TYPE_FILE_BLOCKDEVICE;
        if (ioctl(fd, BLKGETSIZE64, &type->size)) {
            pr_error("IO identify target type: Failed to get size of '%s' via ioctl, errno: %d, error: %s\n", path, errno, strerror(errno));
            return 2;
        }
    } else if (S_ISREG(st.st_mode)) {
        pr_error("IO identify target type: '%s' is a regular file, getting its size via stat\n", path);
        type->file = IO_TARGET_TYPE_FILE_REGULAR;
        type->size = st.st_size;
    } else {
        pr_error("IO identify target type: '%s' is neither a regular file nor a block device, assuming its size as 0\n", path);
        type->file = IO_TARGET_TYPE_FILE_UNSUPPORTED;
        type->size = 0;
    }
    pr_error("IO identify target type: size of '%s' is %zu\n", path, type->size);
    return 0;
}

static inline
enum io_target_type_content
io_identify_target_type_guess_content_from_size(
    size_t const    size
){
    if (size > DTB_PARTITION_SIZE) {
        if (size > EPT_PARTITION_RESERVED_SIZE) {
            fputs("IO identify target type: Size larger than reserved partition, considering content full disk\n", stderr);
            return IO_TARGET_TYPE_CONTENT_DISK;
        } else if (size == EPT_PARTITION_RESERVED_SIZE) {
            fputs("IO identify target type: Size equals reserved partition, considering content reserved partition\n", stderr);
            return IO_TARGET_TYPE_CONTENT_RESERVED;
        } else {
            fputs("IO identify target type: Size between reserved partition and DTB partition, considering content unsupported\n", stderr);
            return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else if (size == DTB_PARTITION_SIZE) {
        fputs("IO identify target type: Size equals DTB partition, consiering content DTB\n", stderr);
        return IO_TARGET_TYPE_CONTENT_DTB;
    } else {
        fputs("IO identify target type: Size too small, considering content DTB\n", stderr);
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
                    fputs("IO identify target type: Content type full disk, as pure 0 in the header was found\n", stderr);
                    return IO_TARGET_TYPE_CONTENT_DISK;
                    break;
                case EPT_HEADER_MAGIC_UINT32:
                    fputs("IO identify target type: Content type reserved partition, as EPT magic was found\n", stderr);
                    return IO_TARGET_TYPE_CONTENT_RESERVED;
                    break;
                case DTB_MAGIC_MULTI:
                case DTB_MAGIC_PLAIN:
                    fputs("IO identify target type: Content type DTB, as DTB magic was found\n", stderr);
                    return IO_TARGET_TYPE_CONTENT_DTB;
                    break;
                default:
                    if (*(uint16_t *)buffer == GZIP_MAGIC) {
                        fputs("IO identify target type: Content type DTB, as gzip magic was found\n", stderr);
                        return IO_TARGET_TYPE_CONTENT_DTB;
                        break;
                    }
                    fputs("IO identify target type: Content type unsupported due to magic unrecognisable\n", stderr);
                    return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
                    break;
            }
        } else {
            fputs("IO identify target type: Content type unsupported due to read failure\n", stderr);
            return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else {
        fputs("IO identify target type: Content type unsupported due to size too small\n", stderr);
        return IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
    }
}

static inline
void
io_identify_target_type_guess_content(
    struct io_target_type * const   type,
    int const                       fd
){
    fputs("IO identify target type: Guessing content type by size\n", stderr);
    enum io_target_type_content ctype_size = io_identify_target_type_guess_content_from_size(type->size);
    fputs("IO identify target type: Getting content type via reading\n", stderr);
    enum io_target_type_content ctype_read = io_identify_target_type_guess_content_by_read(fd, type->size);
    if (ctype_read == ctype_size) {
        fputs("IO identify target type: Read and Size results are the same, using any\n", stderr);
        type->content = ctype_read;
    } else if (ctype_read == IO_TARGET_TYPE_CONTENT_UNSUPPORTED) {
        fputs("IO identify target type: Read result unsupported, using Size result\n", stderr);
        type->content = ctype_size;
    } else if (ctype_size == IO_TARGET_TYPE_CONTENT_UNSUPPORTED) {
        fputs("IO identify target type: Size result unsupported, using Read result\n", stderr);
        type->content = ctype_read;
    } else {
        fputs("IO identify target type: Both Read and Size results valid, using Read result\n", stderr);
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
        pr_error("IO identify target type: Failed to open '%s' as read-only\n", path);
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
            fputs("IO seek DTB: Ilegal target content type (auto), this should not happen\n", stderr);
            return -1;
    }
    pr_error("IO seek DTB: Seeking to %ld\n", offset);
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) {
        pr_error("IO seek DTB: Failed to seek for DTB, errno: %d, error: %s\n", errno, strerror(errno));
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
            pr_error("IO seek EPT: Ilegal target content type (%s), this should not happen\n", cli_mode_strings[cli_options.content]);
            return -1;
    }
    pr_error("IO seek EPT: Seeking to %ld\n", offset);
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) {
        pr_error("IO seek EPT: Failed to seek for EPT, errno: %d, error: %s\n", errno, strerror(errno));
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
        fputs("IO seek and read: Failed to seek\n", stderr);
        return 1;
    }
    if (r_seek != offset) {
        pr_error("IO seek and read: Seeked offset different from expected: Result 0x%lx, expected 0x%lx\n", r_seek, offset);
        return 2;
    }
    if (io_read_till_finish(fd, buffer, size)) {
        fputs("IO seek and read: Failed to read\n", stderr);
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
        pr_error("IO migrate recursive: Bad plan! target = source on block %lu\n", msource - mhelper->entries);
        return -1;
    }
    msource->pending = false;
    if (mtarget->pending) {
        pr_error("IO migrate recursive: Reading block %u\n", msource->target);
        memset(mhelper->buffer_sub, 0, mhelper->block * sizeof *mhelper->buffer_sub);
        if (io_seek_and_read(mhelper->fd, (off_t)mhelper->block * (off_t)msource->target, mhelper->buffer_sub, mhelper->block)) {
            pr_error("IO migrate recursive: Failed to seek and read block %u\n", msource->target);
            return 2;
        }
    }
    pr_error("IO migrate recursive: Writing block %u\n", msource->target);
    if (io_seek_and_write(mhelper->fd, (off_t)mhelper->block * (off_t)msource->target, mhelper->buffer_main, mhelper->block)) {
        pr_error("IO migrate recursive: Failed to seek and read block %u\n", msource->target);
        return 3;
    }
    if (mtarget->pending) {
        uint8_t *buffer = mhelper->buffer_main;
        mhelper->buffer_main = mhelper->buffer_sub;
        mhelper->buffer_sub = buffer;
        if (io_migrate_recursive(mhelper, mtarget)) {
            pr_error("IO migrate recursive: Failed to recursively migrate block %u\n", msource->target);
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
    pr_error("IO migrate: Start migrating, block size 0x%x, total blocks %u\n", mhelper->block, mhelper->count);
    if (!(mhelper->buffer_main = malloc(mhelper->block * sizeof *mhelper->buffer_main))) {
        fputs("IO migfate: Failed to allocate memory for main buffer\n", stderr);
        return 1;
    }
    if (!(mhelper->buffer_sub = malloc(mhelper->block * sizeof *mhelper->buffer_sub))) {
        fputs("IO migfate: Failed to allocate memory for sub buffer\n", stderr);
        free(mhelper->buffer_main);
        return 2;
    }
    struct io_migrate_entry *mentry;
    for (uint32_t i = 0; i < mhelper->count; ++i) {
        mentry = mhelper->entries + i;
        if (mentry->pending) {
            pr_error("IO migrate: Migrating block %u\n", i);
            memset(mhelper->buffer_main, 0, mhelper->block * sizeof *mhelper->buffer_main);
            pr_error("IO migrate: Reading block %u\n", i);
            if (io_seek_and_read(mhelper->fd, (off_t)mhelper->block * (off_t)i, mhelper->buffer_main, mhelper->block)) {
                pr_error("IO migrate: Failed to seek and read block %u\n", i);
                free(mhelper->buffer_main);
                free(mhelper->buffer_sub);
                return 3;
            }
            if (io_migrate_recursive(mhelper, mentry)) {
                pr_error("IO migrate: Failed to recursively migrate block %u\n", i);
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
    if (fd <= 0) {
        pr_error("IO rereadpart: File descriptor not greater than 0 (%d), give up\n", fd);
        return -1;
    }
    if (ioctl(fd, BLKRRPART) == -1) {
        pr_error("IO rereadpart: Failed to send ioctl BLKRRPART to kernel, errno: %d, error: %s\n", errno, strerror(errno));
        return 1;
    }
    fputs("IO rereadpart: success\n", stderr);
    return 0;
}

/* io.c: IO-related functions, type-recognition is also here */