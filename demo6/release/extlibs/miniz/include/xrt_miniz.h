#ifndef XRT_MINIZ_H
#define XRT_MINIZ_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_MINIZ_OPTIONS_VERSION 1u

typedef enum {
	XRT_MINIZ_OK = 0,
	XRT_MINIZ_ERROR_ARGUMENT = 1,
	XRT_MINIZ_ERROR_MEMORY = 2,
	XRT_MINIZ_ERROR_DATA = 3,
	XRT_MINIZ_ERROR_TRUNCATED = 4,
	XRT_MINIZ_ERROR_LIMIT = 5,
	XRT_MINIZ_ERROR_IO = 6,
	XRT_MINIZ_ERROR_STATE = 7,
	XRT_MINIZ_ERROR_CANCELLED = 8
} xrtminizstatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtminizerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	uint8 sReserved[4];
	xrtresourcelimits tLimits;
} xrtminizoptions;

typedef struct xrtminizcompressor xrtminizcompressor;
typedef struct xrtminizdecompressor xrtminizdecompressor;

XRT_API void xrtMinizOptionsInit(xrtminizoptions* pOptions);
XRT_API void xrtMinizErrorInit(xrtminizerror* pError);

XRT_API xrtminizcompressor* xrtMinizCompressorCreate(const xrtminizoptions* pOptions);
XRT_API bool xrtMinizCompressorUpdate(xrtminizcompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtMinizCompressorFinish(xrtminizcompressor* pCompressor);
XRT_API bool xrtMinizCompressorDrain(xrtminizcompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtMinizCompressorTakeBuffer(xrtminizcompressor* pCompressor);
XRT_API xbuffer* xrtMinizCompressorDrainBuffer(xrtminizcompressor* pCompressor);
XRT_API uint64 xrtMinizCompressorInputBytes(const xrtminizcompressor* pCompressor);
XRT_API uint64 xrtMinizCompressorOutputBytes(const xrtminizcompressor* pCompressor);
XRT_API const xrtminizerror* xrtMinizCompressorError(const xrtminizcompressor* pCompressor);
XRT_API void xrtMinizCompressorDestroy(xrtminizcompressor* pCompressor);

XRT_API xrtminizdecompressor* xrtMinizDecompressorCreate(const xrtminizoptions* pOptions);
XRT_API bool xrtMinizDecompressorUpdate(xrtminizdecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtMinizDecompressorFinish(xrtminizdecompressor* pDecompressor);
XRT_API bool xrtMinizDecompressorDrain(xrtminizdecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtMinizDecompressorTakeBuffer(xrtminizdecompressor* pDecompressor);
XRT_API xbuffer* xrtMinizDecompressorDrainBuffer(xrtminizdecompressor* pDecompressor);
XRT_API uint64 xrtMinizDecompressorInputBytes(const xrtminizdecompressor* pDecompressor);
XRT_API uint64 xrtMinizDecompressorOutputBytes(const xrtminizdecompressor* pDecompressor);
XRT_API bool xrtMinizDecompressorFinished(const xrtminizdecompressor* pDecompressor);
XRT_API uint64 xrtMinizDecompressorMembers(const xrtminizdecompressor* pDecompressor);
XRT_API const xrtminizerror* xrtMinizDecompressorError(const xrtminizdecompressor* pDecompressor);
XRT_API void xrtMinizDecompressorDestroy(xrtminizdecompressor* pDecompressor);

XRT_API bool xrtMinizCompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer** ppOutput, xrtminizerror* pError);
XRT_API bool xrtMinizDecompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer** ppOutput, xrtminizerror* pError);
XRT_API bool xrtMinizCompressStream(xreader* pReader, xwriter* pWriter, const xrtminizoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtminizerror* pError);
XRT_API bool xrtMinizDecompressStream(xreader* pReader, xwriter* pWriter, const xrtminizoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtminizerror* pError);

#ifdef __cplusplus
}
#endif

#endif
