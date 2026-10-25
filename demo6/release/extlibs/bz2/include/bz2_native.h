#ifndef XLANG_BZ2_NATIVE_H
#define XLANG_BZ2_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"
#ifdef __cplusplus
extern "C" {
#endif

/* point 对应 void*；C 固定宽度 typedef 不替代语言 c_* 的物理 C 类型。 */
void* xbz2_compress(void*, size_t, int, unsigned long long, unsigned long long);
void* xbz2_compress_str(const char*, size_t, int, unsigned long long, unsigned long long);
void* xbz2_decompress(void*, size_t, bool, unsigned long long, unsigned long long, unsigned int);
/* text_size 保留内嵌 NUL；text_take 移交由 xrtFree 释放的字符串。 */
void* xbz2_decompress_text(void*, size_t, bool, unsigned long long, unsigned long long, unsigned int);
size_t xbz2_text_size(void*);
void* xbz2_text_take(void*);
int xbz2_compress_file(const char*, const char*, int, bool, unsigned long long, unsigned long long, xvalue*);
int xbz2_decompress_file(const char*, const char*, bool, bool, unsigned long long, unsigned long long, unsigned int, xvalue*);
void* xbz2_compressor_create(int, int, unsigned long long, unsigned long long);
void xbz2_compressor_destroy(void*);
bool xbz2_compressor_update(void*, void*, size_t);
bool xbz2_compressor_update_str(void*, const char*, size_t);
bool xbz2_compressor_finish(void*);
void* xbz2_compressor_drain(void*);
unsigned long long xbz2_compressor_input_bytes(void*);
unsigned long long xbz2_compressor_output_bytes(void*);
bool xbz2_compressor_finished(void*);
void* xbz2_decompressor_create(bool, unsigned long long, unsigned long long, unsigned int);
void xbz2_decompressor_destroy(void*);
bool xbz2_decompressor_update(void*, void*, size_t);
bool xbz2_decompressor_finish(void*);
void* xbz2_decompressor_drain(void*);
unsigned long long xbz2_decompressor_input_bytes(void*);
unsigned long long xbz2_decompressor_output_bytes(void*);
bool xbz2_decompressor_finished(void*);
unsigned long long xbz2_decompressor_members(void*);

#ifdef __cplusplus
}
#endif
#endif
