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
	XRT_MINIZ_ERROR_STATE = 7
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

XXAPI void xrtMinizOptionsInit(xrtminizoptions* pOptions);
XXAPI void xrtMinizErrorInit(xrtminizerror* pError);

XXAPI xrtminizcompressor* xrtMinizCompressorCreate(const xrtminizoptions* pOptions);
XXAPI bool xrtMinizCompressorUpdate(xrtminizcompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtMinizCompressorFinish(xrtminizcompressor* pCompressor);
XXAPI bool xrtMinizCompressorDrain(xrtminizcompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtMinizCompressorTakeBuffer(xrtminizcompressor* pCompressor);
XXAPI xbuffer xrtMinizCompressorDrainBuffer(xrtminizcompressor* pCompressor);
XXAPI uint64 xrtMinizCompressorInputBytes(const xrtminizcompressor* pCompressor);
XXAPI uint64 xrtMinizCompressorOutputBytes(const xrtminizcompressor* pCompressor);
XXAPI const xrtminizerror* xrtMinizCompressorError(const xrtminizcompressor* pCompressor);
XXAPI void xrtMinizCompressorDestroy(xrtminizcompressor* pCompressor);

XXAPI xrtminizdecompressor* xrtMinizDecompressorCreate(const xrtminizoptions* pOptions);
XXAPI bool xrtMinizDecompressorUpdate(xrtminizdecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtMinizDecompressorFinish(xrtminizdecompressor* pDecompressor);
XXAPI bool xrtMinizDecompressorDrain(xrtminizdecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtMinizDecompressorTakeBuffer(xrtminizdecompressor* pDecompressor);
XXAPI xbuffer xrtMinizDecompressorDrainBuffer(xrtminizdecompressor* pDecompressor);
XXAPI uint64 xrtMinizDecompressorInputBytes(const xrtminizdecompressor* pDecompressor);
XXAPI uint64 xrtMinizDecompressorOutputBytes(const xrtminizdecompressor* pDecompressor);
XXAPI bool xrtMinizDecompressorFinished(const xrtminizdecompressor* pDecompressor);
XXAPI uint64 xrtMinizDecompressorMembers(const xrtminizdecompressor* pDecompressor);
XXAPI const xrtminizerror* xrtMinizDecompressorError(const xrtminizdecompressor* pDecompressor);
XXAPI void xrtMinizDecompressorDestroy(xrtminizdecompressor* pDecompressor);

XXAPI bool xrtMinizCompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer* pOutput, xrtminizerror* pError);
XXAPI bool xrtMinizDecompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer* pOutput, xrtminizerror* pError);
XXAPI bool xrtMinizCompressStream(xreader pReader, xwriter pWriter, const xrtminizoptions* pOptions,
	xrtminizerror* pError);
XXAPI bool xrtMinizDecompressStream(xreader pReader, xwriter pWriter, const xrtminizoptions* pOptions,
	xrtminizerror* pError);

#ifdef __cplusplus
}
#endif

#endif
