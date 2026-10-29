#ifndef XLANG_SNAPPY_NATIVE_H
#define XLANG_SNAPPY_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与 xlang c_* 声明一致；point 输入借用且不修改，返回缓冲转移给调用方。
 * 需要 const 输入的独立 C 调用方可使用 xrt_snappy.h 的核心接口。 */
bool xsnappy_available(void);
const char* xsnappy_version(void);
const char* xsnappy_last_error(void);
void* xsnappy_compress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xsnappy_compress_str(const char* sText, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xsnappy_decompress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
/* text_size 保留内嵌 NUL；text_take 移交 xrtFree 所有权。 */
void* xsnappy_decompress_text(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
size_t xsnappy_text_size(void* pRaw);
void* xsnappy_text_take(void* pRaw);
int xsnappy_compress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback);
int xsnappy_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback);
void* xsnappy_compressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput);
void xsnappy_compressor_destroy(void* pHandle);
bool xsnappy_compressor_update(void* pHandle, void* pData, size_t iSize);
bool xsnappy_compressor_update_str(void* pHandle, const char* sText, size_t iSize);
bool xsnappy_compressor_finish(void* pHandle);
void* xsnappy_compressor_drain(void* pHandle);
unsigned long long xsnappy_compressor_input_bytes(void* pHandle);
unsigned long long xsnappy_compressor_output_bytes(void* pHandle);
void* xsnappy_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
void xsnappy_decompressor_destroy(void* pHandle);
bool xsnappy_decompressor_update(void* pHandle, void* pData, size_t iSize);
bool xsnappy_decompressor_finish(void* pHandle);
void* xsnappy_decompressor_drain(void* pHandle);
unsigned long long xsnappy_decompressor_input_bytes(void* pHandle);
unsigned long long xsnappy_decompressor_output_bytes(void* pHandle);
bool xsnappy_decompressor_finished(void* pHandle);
unsigned long long xsnappy_decompressor_members(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
