#ifndef TCC_UTF8_IO_H
#define TCC_UTF8_IO_H

#include <stdio.h>

int tcc_utf8_open(const char *path, int flags, ...);
FILE *tcc_utf8_fopen(const char *path, const char *mode);
int tcc_utf8_unlink(const char *path);

#endif
