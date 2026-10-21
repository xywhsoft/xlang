#ifndef XLANG_LZ4_NATIVE_H
#define XLANG_LZ4_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与 xlang c_* 声明一致；point 输入借用且不修改，返回缓冲转移给调用方。
 * 需要 const 输入的独立 C 调用方可使用 xrt_lz4.h 的核心接口。 */
bool xlz4_available(void);
const char* xlz4_version(void);
const char* xlz4_last_error(void);
void* xlz4_compress(void* pData, size_t iSize, int iAcceleration, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xlz4_compress_str(const char* sText, size_t iSize, int iAcceleration, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xlz4_decompress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
int xlz4_compress_file(const char* sSource, const char* sDestination, int iAcceleration, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback);
int xlz4_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback);
void* xlz4_compressor_create(int iAcceleration, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void xlz4_compressor_destroy(void* pHandle);
bool xlz4_compressor_update(void* pHandle, void* pData, size_t iSize);
bool xlz4_compressor_update_str(void* pHandle, const char* sText, size_t iSize);
bool xlz4_compressor_finish(void* pHandle);
void* xlz4_compressor_drain(void* pHandle);
unsigned long long xlz4_compressor_input_bytes(void* pHandle);
unsigned long long xlz4_compressor_output_bytes(void* pHandle);
void* xlz4_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
void xlz4_decompressor_destroy(void* pHandle);
bool xlz4_decompressor_update(void* pHandle, void* pData, size_t iSize);
bool xlz4_decompressor_finish(void* pHandle);
void* xlz4_decompressor_drain(void* pHandle);
unsigned long long xlz4_decompressor_input_bytes(void* pHandle);
unsigned long long xlz4_decompressor_output_bytes(void* pHandle);
bool xlz4_decompressor_finished(void* pHandle);
unsigned long long xlz4_decompressor_members(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
