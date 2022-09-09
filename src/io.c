#include "io_p.h"
static inline bool io_can_retry(int id) {
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

int io_read_till_finish(int fd, void *buffer, size_t size) {
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

int io_write_till_finish(int fd, void *buffer, size_t size) {
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

#if 0
int main(int argc, char **argv) {
    if (argc <= 1) {
        return 1;
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        return 2;
    }
    puts("Ready to read");
    struct stat st_fd;
    fstat(fd, &st_fd);
    printf("Size is %ld, %lx\n", st_fd.st_size, st_fd.st_size);
    unsigned char *buf = malloc(st_fd.st_size);
    if (!buf) {
        close(fd);
        return 3;
    }
    if (io_read_till_finish(fd, buf, st_fd.st_size)) {
        puts("Failed to read");
        close(fd);
        return 4;
    }
    close(fd);
    fd = open("testout", O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return 5;
    }
    puts("Ready to write");
    if (io_write_till_finish(fd, buf, st_fd.st_size)) {
        puts("Failed to write");
        close(fd);
        return 6;
    }
    close (fd);

    return 0;
}
#endif