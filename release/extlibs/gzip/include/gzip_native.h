#ifndef XLANG_GZIP_NATIVE_H
#define XLANG_GZIP_NATIVE_H
#include <stdbool.h>
#include <stddef.h>
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 薄绑定只使用与语言 c_* 对应的物理 C 类型；内部算法继续使用 XRT 类型。 */
void* xgzip_compress(void*, size_t, int, unsigned int, unsigned long long, unsigned long long);
void* xgzip_compress_str(const char*, size_t, int, unsigned int, unsigned long long, unsigned long long);
void* xgzip_decompress(void*, size_t, unsigned long long, unsigned long long, unsigned int);
/* text_size 保留内嵌 NUL；text_take 销毁缓冲并移交 xrtFree 所有权。 */
void* xgzip_decompress_text(void*, size_t, unsigned long long, unsigned long long, unsigned int);
size_t xgzip_text_size(void*);
void* xgzip_text_take(void*);
int xgzip_compress_file(const char*, const char*, int, unsigned int, bool, unsigned long long, unsigned long long, xvalue*);
int xgzip_decompress_file(const char*, const char*, bool, unsigned long long, unsigned long long, unsigned int, xvalue*);

void* xgzip_compressor_create(int, unsigned int, unsigned long long, unsigned long long);
void xgzip_compressor_destroy(void*);
bool xgzip_compressor_update(void*, void*, size_t);
bool xgzip_compressor_update_str(void*, const char*, size_t);
void* xgzip_compressor_finish(void*);
void* xgzip_compressor_drain(void*);
unsigned long long xgzip_compressor_input_bytes(void*);
unsigned long long xgzip_compressor_output_bytes(void*);
bool xgzip_compressor_finished(void*);

void* xgzip_decompressor_create(unsigned long long, unsigned long long, unsigned int);
void xgzip_decompressor_destroy(void*);
bool xgzip_decompressor_update(void*, void*, size_t);
void* xgzip_decompressor_finish(void*);
void* xgzip_decompressor_drain(void*);
unsigned long long xgzip_decompressor_input_bytes(void*);
unsigned long long xgzip_decompressor_output_bytes(void*);
bool xgzip_decompressor_finished(void*);

#ifdef __cplusplus
}
#endif
#endif
