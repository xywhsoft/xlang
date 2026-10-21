#ifndef XLANG_LZMA_NATIVE_H
#define XLANG_LZMA_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与 xlang c_* 声明一致；point 输入借用且不修改，返回缓冲转移给调用方。
 * 需要 const 输入的独立 C 调用方可使用 xrt_lzma.h 的核心接口。 */
bool xlzma_available(void);
const char* xlzma_version(void);
const char* xlzma_last_error(void);
void* xlzma_compress(void* pData, size_t iSize, int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xlzma_compress_str(const char* sText, size_t iSize, int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xlzma_decompress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
int xlzma_compress_file(const char* sSource, const char* sDestination, int iLevel, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback);
int xlzma_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback);
void* xlzma_compressor_create(int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void xlzma_compressor_destroy(void* pHandle);
bool xlzma_compressor_update(void* pHandle, void* pData, size_t iSize);
bool xlzma_compressor_update_str(void* pHandle, const char* sText, size_t iSize);
bool xlzma_compressor_finish(void* pHandle);
void* xlzma_compressor_drain(void* pHandle);
unsigned long long xlzma_compressor_input_bytes(void* pHandle);
unsigned long long xlzma_compressor_output_bytes(void* pHandle);
void* xlzma_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
void xlzma_decompressor_destroy(void* pHandle);
bool xlzma_decompressor_update(void* pHandle, void* pData, size_t iSize);
bool xlzma_decompressor_finish(void* pHandle);
void* xlzma_decompressor_drain(void* pHandle);
unsigned long long xlzma_decompressor_input_bytes(void* pHandle);
unsigned long long xlzma_decompressor_output_bytes(void* pHandle);
bool xlzma_decompressor_finished(void* pHandle);
unsigned long long xlzma_decompressor_members(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
