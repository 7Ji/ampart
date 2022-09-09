#ifndef HAVE_IO_H
#define HAVE_IO_H
#include "common.h"

int io_read_till_finish(int fd, void *buffer, size_t size);
int io_write_till_finish(int fd, void *buffer, size_t size);

#endif