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
            fprintf(stderr, "IO: can not retry with errno %d: %s\n", id, strerror(id));
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
    fprintf(stderr, "IO find disk: Trying to find corresponding full disk drive of '%s' (name %s) so more advanced operations (partition migration, actual table manipulation, partprobe, etc) can be performed\n", path, name);
    const size_t len_name = strlen(name);
    struct stat st;
    if (stat(path, &st)) {
        fprintf(stderr, "IO find disk: Failed to get stat of '%s', errno: %d, error: %s\n", path, errno, strerror(errno));
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
                char *path_real = malloc(sizeof(char) * (6 + len_entry)); // /dev/ is 5, name max 256
                if (!path_real) {
                    return NULL;
                }
                snprintf(path_real, 6 + len_entry, "/dev/%s", dir_entry->d_name);
                fprintf(stderr, "IO find disk: Corresponding disk drive for '%s' is '%s'\n", path, path_real);
                closedir(dir);
                return path_real;
            }
        }
    }
    fprintf(stderr, "IO find disk: Could not find corresponding disk drive for '%s'\n", path);
    closedir(dir);
    return NULL;
}

void 
io_describe_target_type(
    struct io_target_type * type,
    char const * const      path
){
    if (path) {
        fprintf(stderr, "IO identify target type: '%s' is a ", path);
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
    fprintf(stderr, " with a size of %zu bytes, and contains the content of ", type->size);
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

struct io_target_type *
io_identify_target_type(
    char const * const  path
){
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "IO identify target type: Failed to open '%s' as read-only\n", path);
        return NULL;
    }
    struct io_target_type *type = malloc(sizeof(struct io_target_type));
    if (!type) {
        fputs("IO identify target type: Failed to allocate memory for type helper\n", stderr);
        close(fd);
        return NULL;
    }
    memset(type, 0, sizeof(struct io_target_type));
    struct stat st;
    if (fstat(fd, &st)) {
        fprintf(stderr, "IO identify target type: Failed to get stat of '%s', errno: %d, error: %s\n", path, errno, strerror(errno));
        free(type);
        close(fd);
        return NULL;
    }
    if (S_ISBLK(st.st_mode)) {
        fprintf(stderr, "IO identify target type: '%s' is a block device, getting its size via ioctl\n", path);
        type->file = IO_TARGET_TYPE_FILE_BLOCKDEVICE;
        if (ioctl(fd, BLKGETSIZE64, &type->size)) {
            fprintf(stderr, "IO identify target type: Failed to get size of '%s' via ioctl, errno: %d, error: %s\n", path, errno, strerror(errno));
            free(type);
            close(fd);
            return NULL;
        }
    } else if (S_ISREG(st.st_mode)) {
        fprintf(stderr, "IO identify target type: '%s' is a regular file, getting its size via stat\n", path);
        type->file = IO_TARGET_TYPE_FILE_REGULAR;
        type->size = st.st_size;
    } else {
        fprintf(stderr, "IO identify target type: '%s' is neither a regular file nor a block device, assuming its size as 0\n", path);
        type->file = IO_TARGET_TYPE_FILE_UNSUPPORTED;
        type->size = 0;
    }
    fprintf(stderr, "IO identify target type: size of '%s' is %zu\n", path, type->size);
    fputs("IO identify target type: Guessing content type by size\n", stderr);
    enum io_target_type_content ctype_size = IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
    if (type->size > DTB_PARTITION_SIZE) {
        if (type->size > EPT_PARTITION_RESERVED_SIZE) {
            fputs("IO identify target type: Size larger than reserved partition, considering content full disk\n", stderr);
            ctype_size = IO_TARGET_TYPE_CONTENT_DISK;
        } else if (type->size == EPT_PARTITION_RESERVED_SIZE) {
            fputs("IO identify target type: Size equals reserved partition, considering content reserved partition\n", stderr);
            ctype_size = IO_TARGET_TYPE_CONTENT_RESERVED;
        } else {
            fputs("IO identify target type: Size between reserved partition and DTB partition, considering content unsupported\n", stderr);
            ctype_size = IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else if (type->size == DTB_PARTITION_SIZE) {
        fputs("IO identify target type: Size equals DTB partition, consiering content DTB\n", stderr);
        ctype_size = IO_TARGET_TYPE_CONTENT_DTB;
    } else {
        fputs("IO identify target type: Size too small, considering content DTB\n", stderr);
        ctype_size = IO_TARGET_TYPE_CONTENT_DTB;
    }
    fputs("IO identify target type: Getting content type via reading\n", stderr);
    enum io_target_type_content ctype_read;
    if (type->size) {
        uint8_t buffer[4] = {0};
        if (read(fd, buffer, 4) == 4) {
            switch (*(uint32_t *)buffer) {
                case 0:
                    fputs("IO identify target type: Content type full disk, as pure 0 in the header was found\n", stderr);
                    ctype_read = IO_TARGET_TYPE_CONTENT_DISK;
                    break;
                case EPT_HEADER_MAGIC_UINT32:
                    fputs("IO identify target type: Content type reserved partition, as EPT magic was found\n", stderr);
                    ctype_read = IO_TARGET_TYPE_CONTENT_RESERVED;
                    break;
                case DTB_MAGIC_MULTI:
                case DTB_MAGIC_PLAIN:
                    fputs("IO identify target type: Content type DTB, as DTB magic was found\n", stderr);
                    ctype_read = IO_TARGET_TYPE_CONTENT_DTB;
                    break;
                default:
                    if (*(uint16_t *)buffer == GZIP_MAGIC) {
                        fputs("IO identify target type: Content type DTB, as gzip magic was found\n", stderr);
                        ctype_read = IO_TARGET_TYPE_CONTENT_DTB;
                        break;
                    }
                    fputs("IO identify target type: Content type unsupported due to magic unrecognisable\n", stderr);
                    ctype_read = IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
                    break;
            }
        } else {
            fputs("IO identify target type: Content type unsupported due to read failure\n", stderr);
            ctype_read = IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
        }
    } else {
        fputs("IO identify target type: Content type unsupported due to size too small\n", stderr);
        ctype_read = IO_TARGET_TYPE_CONTENT_UNSUPPORTED;
    }
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
    close(fd);
    return type;
}

/* io.c: IO-related functions, type-recognition is also here */