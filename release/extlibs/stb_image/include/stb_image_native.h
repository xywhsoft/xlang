#ifndef XL_EXTLIB_STB_IMAGE_NATIVE_H
#define XL_EXTLIB_STB_IMAGE_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool xstbi_available(void);
const char* xstbi_version(void);
void* xstbi_load(const char* path, int desired_channels);
void* xstbi_loadf(const char* path, int desired_channels);
void* xstbi_load_memory(void* data, size_t size, int desired_channels, int is_float);
int xstbi_is_hdr(const char* path);
/* 由 XRT 分配，调用方以 xrtFree 释放。 */
void* xstbi_info_json(const char* path);
void* xstbi_image_adopt(void* image);
int xstbi_image_valid(void* image);
int xstbi_image_width(void* image);
int xstbi_image_height(void* image);
int xstbi_image_channels(void* image);
int xstbi_image_is_float(void* image);
size_t xstbi_image_byte_size(void* image);
void* xstbi_image_data(void* image);
void xstbi_image_release(void* image);

#ifdef __cplusplus
}
#endif

#endif
