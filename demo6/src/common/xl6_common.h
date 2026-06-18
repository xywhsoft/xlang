#ifndef XL6_COMMON_H
#define XL6_COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XL6_ARRAY_COUNT
#define XL6_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static inline void* xl6Alloc(size_t size)
{
    void* p;

    p = calloc(1, size);
    return p;
}

static inline void xl6Free(void* p)
{
    free(p);
}

char* xl6StrDupRange(const char* text, size_t len);
char* xl6ReadFileText(const char* path, size_t* outLen);

#endif
