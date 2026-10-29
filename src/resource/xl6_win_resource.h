#ifndef XL6_WIN_RESOURCE_H
#define XL6_WIN_RESOURCE_H

#include <stddef.h>

int xl6WinResourceConfigToCoffMemory(
    const char* configPath,
    void** outData,
    size_t* outSize,
    char* error,
    size_t errorSize);

#endif
