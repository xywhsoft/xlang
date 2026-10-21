#ifndef XLANG_BROTLI_NATIVE_H
#define XLANG_BROTLI_NATIVE_H

#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 薄绑定 ABI；point 为 void*，c_ulonglong 不依赖平台 uint64_t 的 typedef。 */
bool xbrotli_available(void);
const char* xbrotli_version(void);
const char* xbrotli_last_error(void);
void* xbrotli_compress(void*, size_t, int, int, int, unsigned long long, unsigned long long);
void* xbrotli_compress_str(const char*, size_t, int, int, int, unsigned long long, unsigned long long);
void* xbrotli_decompress(void*, size_t, unsigned long long, unsigned long long, unsigned int);
int xbrotli_compress_file(const char*, const char*, int, int, int, bool,
    unsigned long long, unsigned long long, xvalue*);
int xbrotli_decompress_file(const char*, const char*, bool,
    unsigned long long, unsigned long long, unsigned int, xvalue*);

void* xbrotli_compressor_create(int, int, int, unsigned long long, unsigned long long);
void xbrotli_compressor_destroy(void*);
bool xbrotli_compressor_update(void*, void*, size_t);
bool xbrotli_compressor_update_str(void*, const char*, size_t);
bool xbrotli_compressor_finish(void*);
void* xbrotli_compressor_drain(void*);
unsigned long long xbrotli_compressor_input_bytes(void*);
unsigned long long xbrotli_compressor_output_bytes(void*);

void* xbrotli_decompressor_create(unsigned long long, unsigned long long, unsigned int);
void xbrotli_decompressor_destroy(void*);
bool xbrotli_decompressor_update(void*, void*, size_t);
bool xbrotli_decompressor_finish(void*);
void* xbrotli_decompressor_drain(void*);
unsigned long long xbrotli_decompressor_input_bytes(void*);
unsigned long long xbrotli_decompressor_output_bytes(void*);
bool xbrotli_decompressor_finished(void*);
unsigned long long xbrotli_decompressor_members(void*);

#ifdef __cplusplus
}
#endif
#endif
