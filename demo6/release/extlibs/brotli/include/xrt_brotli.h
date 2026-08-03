#ifndef XRT_BROTLI_H
#define XRT_BROTLI_H

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
	XRT_BROTLI_ERROR_STATE = 7
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
	xrtresourcelimits tLimits;
} xrtbrotlioptions;

typedef struct xrtbrotlicompressor xrtbrotlicompressor;
typedef struct xrtbrotlidecompressor xrtbrotlidecompressor;

XXAPI void xrtBrotliOptionsInit(xrtbrotlioptions* pOptions);
XXAPI void xrtBrotliErrorInit(xrtbrotlierror* pError);

XXAPI xrtbrotlicompressor* xrtBrotliCompressorCreate(const xrtbrotlioptions* pOptions);
XXAPI bool xrtBrotliCompressorUpdate(xrtbrotlicompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtBrotliCompressorFinish(xrtbrotlicompressor* pCompressor);
XXAPI bool xrtBrotliCompressorDrain(xrtbrotlicompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtBrotliCompressorTakeBuffer(xrtbrotlicompressor* pCompressor);
XXAPI xbuffer xrtBrotliCompressorDrainBuffer(xrtbrotlicompressor* pCompressor);
XXAPI uint64 xrtBrotliCompressorInputBytes(const xrtbrotlicompressor* pCompressor);
XXAPI uint64 xrtBrotliCompressorOutputBytes(const xrtbrotlicompressor* pCompressor);
XXAPI const xrtbrotlierror* xrtBrotliCompressorError(const xrtbrotlicompressor* pCompressor);
XXAPI void xrtBrotliCompressorDestroy(xrtbrotlicompressor* pCompressor);

XXAPI xrtbrotlidecompressor* xrtBrotliDecompressorCreate(const xrtbrotlioptions* pOptions);
XXAPI bool xrtBrotliDecompressorUpdate(xrtbrotlidecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtBrotliDecompressorFinish(xrtbrotlidecompressor* pDecompressor);
XXAPI bool xrtBrotliDecompressorDrain(xrtbrotlidecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtBrotliDecompressorTakeBuffer(xrtbrotlidecompressor* pDecompressor);
XXAPI xbuffer xrtBrotliDecompressorDrainBuffer(xrtbrotlidecompressor* pDecompressor);
XXAPI uint64 xrtBrotliDecompressorInputBytes(const xrtbrotlidecompressor* pDecompressor);
XXAPI uint64 xrtBrotliDecompressorOutputBytes(const xrtbrotlidecompressor* pDecompressor);
XXAPI bool xrtBrotliDecompressorFinished(const xrtbrotlidecompressor* pDecompressor);
XXAPI uint64 xrtBrotliDecompressorMembers(const xrtbrotlidecompressor* pDecompressor);
XXAPI const xrtbrotlierror* xrtBrotliDecompressorError(const xrtbrotlidecompressor* pDecompressor);
XXAPI void xrtBrotliDecompressorDestroy(xrtbrotlidecompressor* pDecompressor);

XXAPI bool xrtBrotliCompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer* pOutput, xrtbrotlierror* pError);
XXAPI bool xrtBrotliDecompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer* pOutput, xrtbrotlierror* pError);
XXAPI bool xrtBrotliCompressStream(xreader pReader, xwriter pWriter, const xrtbrotlioptions* pOptions,
	xrtbrotlierror* pError);
XXAPI bool xrtBrotliDecompressStream(xreader pReader, xwriter pWriter, const xrtbrotlioptions* pOptions,
	xrtbrotlierror* pError);

#ifdef __cplusplus
}
#endif

#endif
