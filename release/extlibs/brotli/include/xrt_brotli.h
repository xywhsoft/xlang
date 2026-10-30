#ifndef XRT_BROTLI_H
#define XRT_BROTLI_H

/* 公共 C 接口依赖 reader/writer/Buffer，直接包含时也必须可用。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_BROTLI_OPTIONS_VERSION 1u

typedef enum {
	XRT_BROTLI_OK = 0,
	XRT_BROTLI_ERROR_ARGUMENT = 1,
	XRT_BROTLI_ERROR_MEMORY = 2,
	XRT_BROTLI_ERROR_DATA = 3,
	XRT_BROTLI_ERROR_TRUNCATED = 4,
	XRT_BROTLI_ERROR_LIMIT = 5,
	XRT_BROTLI_ERROR_IO = 6,
	XRT_BROTLI_ERROR_STATE = 7,
	XRT_BROTLI_ERROR_CANCELLED = 8
} xrtbrotlistatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtbrotlierror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iQuality;
	int iWindow;
	int iMode;
	/* 仅使用输入、输出及解压倍率限制；其他字段必须为零。 */
	xrtresourcelimits tLimits;
} xrtbrotlioptions;

typedef struct xrtbrotlicompressor xrtbrotlicompressor;
typedef struct xrtbrotlidecompressor xrtbrotlidecompressor;

XRT_API void xrtBrotliOptionsInit(xrtbrotlioptions* pOptions);
XRT_API void xrtBrotliErrorInit(xrtbrotlierror* pError);

XRT_API xrtbrotlicompressor* xrtBrotliCompressorCreate(const xrtbrotlioptions* pOptions);
XRT_API bool xrtBrotliCompressorUpdate(xrtbrotlicompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtBrotliCompressorFinish(xrtbrotlicompressor* pCompressor);
XRT_API bool xrtBrotliCompressorDrain(xrtbrotlicompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtBrotliCompressorTakeBuffer(xrtbrotlicompressor* pCompressor);
XRT_API xbuffer* xrtBrotliCompressorDrainBuffer(xrtbrotlicompressor* pCompressor);
XRT_API uint64 xrtBrotliCompressorInputBytes(const xrtbrotlicompressor* pCompressor);
XRT_API uint64 xrtBrotliCompressorOutputBytes(const xrtbrotlicompressor* pCompressor);
XRT_API const xrtbrotlierror* xrtBrotliCompressorError(const xrtbrotlicompressor* pCompressor);
XRT_API void xrtBrotliCompressorDestroy(xrtbrotlicompressor* pCompressor);

XRT_API xrtbrotlidecompressor* xrtBrotliDecompressorCreate(const xrtbrotlioptions* pOptions);
XRT_API bool xrtBrotliDecompressorUpdate(xrtbrotlidecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtBrotliDecompressorFinish(xrtbrotlidecompressor* pDecompressor);
XRT_API bool xrtBrotliDecompressorDrain(xrtbrotlidecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtBrotliDecompressorTakeBuffer(xrtbrotlidecompressor* pDecompressor);
XRT_API xbuffer* xrtBrotliDecompressorDrainBuffer(xrtbrotlidecompressor* pDecompressor);
XRT_API uint64 xrtBrotliDecompressorInputBytes(const xrtbrotlidecompressor* pDecompressor);
XRT_API uint64 xrtBrotliDecompressorOutputBytes(const xrtbrotlidecompressor* pDecompressor);
XRT_API bool xrtBrotliDecompressorFinished(const xrtbrotlidecompressor* pDecompressor);
XRT_API uint64 xrtBrotliDecompressorMembers(const xrtbrotlidecompressor* pDecompressor);
XRT_API const xrtbrotlierror* xrtBrotliDecompressorError(const xrtbrotlidecompressor* pDecompressor);
XRT_API void xrtBrotliDecompressorDestroy(xrtbrotlidecompressor* pDecompressor);

XRT_API bool xrtBrotliCompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError);
XRT_API bool xrtBrotliDecompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError);
XRT_API bool xrtBrotliCompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError);
XRT_API bool xrtBrotliDecompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError);

/* 同目录临时文件，最终回调、刷新及关闭成功后提交；支持原地变换。
 * options 为 NULL 使用默认值。同步借用路径/回调/用户状态，失败详情见 xrtGetError。
 * 文件仍打开时按句柄回滚；已关闭后路径回滚再遇 OOM 的保证受宿主 XRT 限制。 */
XRT_API bool xrtBrotliCompressFile(const char* source, const char* destination,
    const xrtbrotlioptions* options, bool overwrite, xrtprogressproc progress, ptr user);
XRT_API bool xrtBrotliDecompressFile(const char* source, const char* destination,
    const xrtbrotlioptions* options, bool overwrite, xrtprogressproc progress, ptr user);

#ifdef __cplusplus
}
#endif

#endif
