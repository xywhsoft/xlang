#ifndef XLANG_ZSTD_NATIVE_H
#define XLANG_ZSTD_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与 xlang c_* 声明一致；point 输入借用且不修改，返回缓冲转移给调用方。
 * 需要 const 输入的独立 C 调用方可使用 xrt_zstd.h 的核心接口。 */
bool xzstd_available(void);
const char* xzstd_version(void);
const char* xzstd_last_error(void);
bool xzstd_last_error_is_memory(void);
bool xzstd_last_error_is_range(void);
void* xzstd_compress(void* pData, size_t iSize, int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xzstd_decompress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
int xzstd_compress_file(const char* sSource, const char* sDestination, int iLevel, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback);
int xzstd_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback);
void* xzstd_compressor_create(int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void xzstd_compressor_destroy(void* pHandle);
bool xzstd_compressor_update(void* pHandle, void* pData, size_t iSize);
bool xzstd_compressor_finish(void* pHandle);
void* xzstd_compressor_drain(void* pHandle);
unsigned long long xzstd_compressor_input_bytes(void* pHandle);
unsigned long long xzstd_compressor_output_bytes(void* pHandle);
void* xzstd_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
void xzstd_decompressor_destroy(void* pHandle);
bool xzstd_decompressor_update(void* pHandle, void* pData, size_t iSize);
bool xzstd_decompressor_finish(void* pHandle);
void* xzstd_decompressor_drain(void* pHandle);
unsigned long long xzstd_decompressor_input_bytes(void* pHandle);
unsigned long long xzstd_decompressor_output_bytes(void* pHandle);
bool xzstd_decompressor_finished(void* pHandle);
unsigned long long xzstd_decompressor_members(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
