#ifndef XSTBIR_NATIVE_H
#define XSTBIR_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int xstbir_available(void);
const char* xstbir_version(void);

/* 返回 xbuffer*，调用方以 xrtBufferDestroy 释放；Linear 为 0(sRGB)/1。 */
void* xstbir_resize(void* Pixels, size_t Length, int Width, int Height,
    int Channels, int OutWidth, int OutHeight, int InputStride, int Linear);

#ifdef __cplusplus
}
#endif
#endif
