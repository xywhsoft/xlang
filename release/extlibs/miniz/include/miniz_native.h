#ifndef XLANG_MINIZ_NATIVE_H
#define XLANG_MINIZ_NATIVE_H
#include <stdbool.h>
#include <stddef.h>

/* 回调只借用公开的不透明 xvalue；不要求先选 VALUE 模块或固定包含顺序。 */
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 与 xlang c_* 声明一致；point 输入借用且不修改，返回缓冲转移给调用方。
 * 需要 const 输入的独立 C 调用方可使用 xrt_miniz.h 的核心接口。 */
bool xminiz_available(void);
const char* xminiz_version(void);
const char* xminiz_last_error(void);
void* xminiz_compress(void* pData, size_t iSize, int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xminiz_compress_str(const char* sText, size_t iSize, int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void* xminiz_decompress(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
/* text_size 保留内嵌 NUL；text_take 移交 xrtFree 所有权。 */
void* xminiz_decompress_text(void* pData, size_t iSize, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
size_t xminiz_text_size(void* pRaw);
void* xminiz_text_take(void* pRaw);
int xminiz_compress_file(const char* sSource, const char* sDestination, int iLevel, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback);
int xminiz_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback);
void* xminiz_compressor_create(int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput);
void xminiz_compressor_destroy(void* pHandle);
bool xminiz_compressor_update(void* pHandle, void* pData, size_t iSize);
bool xminiz_compressor_update_str(void* pHandle, const char* sText, size_t iSize);
bool xminiz_compressor_finish(void* pHandle);
void* xminiz_compressor_drain(void* pHandle);
unsigned long long xminiz_compressor_input_bytes(void* pHandle);
unsigned long long xminiz_compressor_output_bytes(void* pHandle);
void* xminiz_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio);
void xminiz_decompressor_destroy(void* pHandle);
bool xminiz_decompressor_update(void* pHandle, void* pData, size_t iSize);
bool xminiz_decompressor_finish(void* pHandle);
void* xminiz_decompressor_drain(void* pHandle);
unsigned long long xminiz_decompressor_input_bytes(void* pHandle);
unsigned long long xminiz_decompressor_output_bytes(void* pHandle);
bool xminiz_decompressor_finished(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
