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

#ifdef __cplusplus
}
#endif

#endif
