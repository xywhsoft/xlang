#ifndef XRT_LZMA_H
#define XRT_LZMA_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_LZMA_OPTIONS_VERSION 1u

typedef enum {
	XRT_LZMA_OK = 0,
	XRT_LZMA_ERROR_ARGUMENT = 1,
	XRT_LZMA_ERROR_MEMORY = 2,
	XRT_LZMA_ERROR_DATA = 3,
	XRT_LZMA_ERROR_TRUNCATED = 4,
	XRT_LZMA_ERROR_LIMIT = 5,
	XRT_LZMA_ERROR_IO = 6,
	XRT_LZMA_ERROR_STATE = 7
} xrtlzmastatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtlzmaerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	uint8 sReserved[4];
	xrtresourcelimits tLimits;
} xrtlzmaoptions;

typedef struct xrtlzmacompressor xrtlzmacompressor;
typedef struct xrtlzmadecompressor xrtlzmadecompressor;

XXAPI void xrtLzmaOptionsInit(xrtlzmaoptions* pOptions);
XXAPI void xrtLzmaErrorInit(xrtlzmaerror* pError);

XXAPI xrtlzmacompressor* xrtLzmaCompressorCreate(const xrtlzmaoptions* pOptions);
XXAPI bool xrtLzmaCompressorUpdate(xrtlzmacompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtLzmaCompressorFinish(xrtlzmacompressor* pCompressor);
XXAPI bool xrtLzmaCompressorDrain(xrtlzmacompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtLzmaCompressorTakeBuffer(xrtlzmacompressor* pCompressor);
XXAPI xbuffer xrtLzmaCompressorDrainBuffer(xrtlzmacompressor* pCompressor);
XXAPI uint64 xrtLzmaCompressorInputBytes(const xrtlzmacompressor* pCompressor);
XXAPI uint64 xrtLzmaCompressorOutputBytes(const xrtlzmacompressor* pCompressor);
XXAPI const xrtlzmaerror* xrtLzmaCompressorError(const xrtlzmacompressor* pCompressor);
XXAPI void xrtLzmaCompressorDestroy(xrtlzmacompressor* pCompressor);

XXAPI xrtlzmadecompressor* xrtLzmaDecompressorCreate(const xrtlzmaoptions* pOptions);
XXAPI bool xrtLzmaDecompressorUpdate(xrtlzmadecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtLzmaDecompressorFinish(xrtlzmadecompressor* pDecompressor);
XXAPI bool xrtLzmaDecompressorDrain(xrtlzmadecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtLzmaDecompressorTakeBuffer(xrtlzmadecompressor* pDecompressor);
XXAPI xbuffer xrtLzmaDecompressorDrainBuffer(xrtlzmadecompressor* pDecompressor);
XXAPI uint64 xrtLzmaDecompressorInputBytes(const xrtlzmadecompressor* pDecompressor);
XXAPI uint64 xrtLzmaDecompressorOutputBytes(const xrtlzmadecompressor* pDecompressor);
XXAPI bool xrtLzmaDecompressorFinished(const xrtlzmadecompressor* pDecompressor);
XXAPI uint64 xrtLzmaDecompressorMembers(const xrtlzmadecompressor* pDecompressor);
XXAPI const xrtlzmaerror* xrtLzmaDecompressorError(const xrtlzmadecompressor* pDecompressor);
XXAPI void xrtLzmaDecompressorDestroy(xrtlzmadecompressor* pDecompressor);

XXAPI bool xrtLzmaCompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer* pOutput, xrtlzmaerror* pError);
XXAPI bool xrtLzmaDecompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer* pOutput, xrtlzmaerror* pError);
XXAPI bool xrtLzmaCompressStream(xreader pReader, xwriter pWriter, const xrtlzmaoptions* pOptions,
	xrtlzmaerror* pError);
XXAPI bool xrtLzmaDecompressStream(xreader pReader, xwriter pWriter, const xrtlzmaoptions* pOptions,
	xrtlzmaerror* pError);

#ifdef __cplusplus
}
#endif

#endif
