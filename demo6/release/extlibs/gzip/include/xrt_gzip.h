#ifndef XRT_GZIP_H
#define XRT_GZIP_H

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
	XRT_GZIP_ERROR_STATE = 7
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
	xrtresourcelimits tLimits;
} xrtgzipoptions;

typedef struct xrtgzipcompressor xrtgzipcompressor;
typedef struct xrtgzipdecompressor xrtgzipdecompressor;

XXAPI void xrtGzipOptionsInit(xrtgzipoptions* pOptions);
XXAPI void xrtGzipErrorInit(xrtgziperror* pError);

XXAPI xrtgzipcompressor* xrtGzipCompressorCreate(const xrtgzipoptions* pOptions);
XXAPI bool xrtGzipCompressorUpdate(xrtgzipcompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtGzipCompressorFinish(xrtgzipcompressor* pCompressor);
XXAPI bool xrtGzipCompressorDrain(xrtgzipcompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtGzipCompressorTakeBuffer(xrtgzipcompressor* pCompressor);
XXAPI const xrtgziperror* xrtGzipCompressorError(const xrtgzipcompressor* pCompressor);
XXAPI void xrtGzipCompressorDestroy(xrtgzipcompressor* pCompressor);

XXAPI xrtgzipdecompressor* xrtGzipDecompressorCreate(const xrtgzipoptions* pOptions);
XXAPI bool xrtGzipDecompressorUpdate(xrtgzipdecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtGzipDecompressorFinish(xrtgzipdecompressor* pDecompressor);
XXAPI bool xrtGzipDecompressorDrain(xrtgzipdecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtGzipDecompressorTakeBuffer(xrtgzipdecompressor* pDecompressor);
XXAPI const xrtgziperror* xrtGzipDecompressorError(const xrtgzipdecompressor* pDecompressor);
XXAPI void xrtGzipDecompressorDestroy(xrtgzipdecompressor* pDecompressor);

XXAPI bool xrtGzipCompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer* pOutput, xrtgziperror* pError);
XXAPI bool xrtGzipDecompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer* pOutput, xrtgziperror* pError);
XXAPI bool xrtGzipCompressStream(xreader pReader, xwriter pWriter, const xrtgzipoptions* pOptions,
	xrtgziperror* pError);
XXAPI bool xrtGzipDecompressStream(xreader pReader, xwriter pWriter, const xrtgzipoptions* pOptions,
	xrtgziperror* pError);

#ifdef __cplusplus
}
#endif

#endif
