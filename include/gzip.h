#ifndef HAVE_GZIP_H
#define HAVE_GZIP_H
#include "common.h"

unsigned long gzip_unzip(unsigned char *in, unsigned long in_size, unsigned char **out);
unsigned long gzip_zip(unsigned char *in, unsigned long in_size, unsigned char **out);
#endif