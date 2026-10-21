#ifndef XRT_GZIP_H
#define XRT_GZIP_H

/* gzip 公共契约直接依赖连续缓冲和通用 Reader/Writer。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_GZIP_OPTIONS_VERSION 1u

typedef enum {
	XRT_GZIP_OK = 0,
	XRT_GZIP_ERROR_ARGUMENT = 1,
	XRT_GZIP_ERROR_MEMORY = 2,
	XRT_GZIP_ERROR_DATA = 3,
	XRT_GZIP_ERROR_TRUNCATED = 4,
	XRT_GZIP_ERROR_LIMIT = 5,
	XRT_GZIP_ERROR_IO = 6,
	XRT_GZIP_ERROR_STATE = 7,
	XRT_GZIP_ERROR_CANCELLED = 8
} xrtgzipstatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtgziperror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	int iStrategy;
	uint32 iMTime;
	uint32 iReserved;
	/* 仅使用输入、输出和解压倍率限制；其他字段必须为零。 */
	xrtresourcelimits tLimits;
} xrtgzipoptions;

typedef struct xrtgzipcompressor xrtgzipcompressor;
typedef struct xrtgzipdecompressor xrtgzipdecompressor;

XRT_API void xrtGzipOptionsInit(xrtgzipoptions* pOptions);
XRT_API void xrtGzipErrorInit(xrtgziperror* pError);

XRT_API xrtgzipcompressor* xrtGzipCompressorCreate(const xrtgzipoptions* pOptions);
XRT_API bool xrtGzipCompressorUpdate(xrtgzipcompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtGzipCompressorFinish(xrtgzipcompressor* pCompressor);
XRT_API bool xrtGzipCompressorDrain(xrtgzipcompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtGzipCompressorTakeBuffer(xrtgzipcompressor* pCompressor);
/* DrainBuffer 随时移交本次输出；TakeBuffer 只在 Finish 后移交一次最终所有权。 */
XRT_API xbuffer* xrtGzipCompressorDrainBuffer(xrtgzipcompressor* pCompressor);
XRT_API uint64 xrtGzipCompressorInputBytes(const xrtgzipcompressor* pCompressor);
XRT_API uint64 xrtGzipCompressorOutputBytes(const xrtgzipcompressor* pCompressor);
XRT_API bool xrtGzipCompressorFinished(const xrtgzipcompressor* pCompressor);
XRT_API const xrtgziperror* xrtGzipCompressorError(const xrtgzipcompressor* pCompressor);
XRT_API void xrtGzipCompressorDestroy(xrtgzipcompressor* pCompressor);

XRT_API xrtgzipdecompressor* xrtGzipDecompressorCreate(const xrtgzipoptions* pOptions);
XRT_API bool xrtGzipDecompressorUpdate(xrtgzipdecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtGzipDecompressorFinish(xrtgzipdecompressor* pDecompressor);
XRT_API bool xrtGzipDecompressorDrain(xrtgzipdecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtGzipDecompressorTakeBuffer(xrtgzipdecompressor* pDecompressor);
XRT_API xbuffer* xrtGzipDecompressorDrainBuffer(xrtgzipdecompressor* pDecompressor);
XRT_API uint64 xrtGzipDecompressorInputBytes(const xrtgzipdecompressor* pDecompressor);
XRT_API uint64 xrtGzipDecompressorOutputBytes(const xrtgzipdecompressor* pDecompressor);
XRT_API bool xrtGzipDecompressorFinished(const xrtgzipdecompressor* pDecompressor);
XRT_API const xrtgziperror* xrtGzipDecompressorError(const xrtgzipdecompressor* pDecompressor);
XRT_API void xrtGzipDecompressorDestroy(xrtgzipdecompressor* pDecompressor);

XRT_API bool xrtGzipCompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer** ppOutput, xrtgziperror* pError);
XRT_API bool xrtGzipDecompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer** ppOutput, xrtgziperror* pError);
XRT_API bool xrtGzipCompressStream(xreader* pReader, xwriter* pWriter, const xrtgzipoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtgziperror* pError);
XRT_API bool xrtGzipDecompressStream(xreader* pReader, xwriter* pWriter, const xrtgzipoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtgziperror* pError);

#ifdef __cplusplus
}
#endif

#endif
