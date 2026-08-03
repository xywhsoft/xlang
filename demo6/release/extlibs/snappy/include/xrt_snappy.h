#ifndef XRT_SNAPPY_H
#define XRT_SNAPPY_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_SNAPPY_OPTIONS_VERSION 1u

typedef enum {
	XRT_SNAPPY_OK = 0,
	XRT_SNAPPY_ERROR_ARGUMENT = 1,
	XRT_SNAPPY_ERROR_MEMORY = 2,
	XRT_SNAPPY_ERROR_DATA = 3,
	XRT_SNAPPY_ERROR_TRUNCATED = 4,
	XRT_SNAPPY_ERROR_LIMIT = 5,
	XRT_SNAPPY_ERROR_IO = 6,
	XRT_SNAPPY_ERROR_STATE = 7
} xrtsnappystatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtsnappyerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint8 sReserved[4];
	xrtresourcelimits tLimits;
} xrtsnappyoptions;

typedef struct xrtsnappycompressor xrtsnappycompressor;
typedef struct xrtsnappydecompressor xrtsnappydecompressor;

XXAPI void xrtSnappyOptionsInit(xrtsnappyoptions* pOptions);
XXAPI void xrtSnappyErrorInit(xrtsnappyerror* pError);

XXAPI xrtsnappycompressor* xrtSnappyCompressorCreate(const xrtsnappyoptions* pOptions);
XXAPI bool xrtSnappyCompressorUpdate(xrtsnappycompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtSnappyCompressorFinish(xrtsnappycompressor* pCompressor);
XXAPI bool xrtSnappyCompressorDrain(xrtsnappycompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtSnappyCompressorTakeBuffer(xrtsnappycompressor* pCompressor);
XXAPI xbuffer xrtSnappyCompressorDrainBuffer(xrtsnappycompressor* pCompressor);
XXAPI uint64 xrtSnappyCompressorInputBytes(const xrtsnappycompressor* pCompressor);
XXAPI uint64 xrtSnappyCompressorOutputBytes(const xrtsnappycompressor* pCompressor);
XXAPI const xrtsnappyerror* xrtSnappyCompressorError(const xrtsnappycompressor* pCompressor);
XXAPI void xrtSnappyCompressorDestroy(xrtsnappycompressor* pCompressor);

XXAPI xrtsnappydecompressor* xrtSnappyDecompressorCreate(const xrtsnappyoptions* pOptions);
XXAPI bool xrtSnappyDecompressorUpdate(xrtsnappydecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtSnappyDecompressorFinish(xrtsnappydecompressor* pDecompressor);
XXAPI bool xrtSnappyDecompressorDrain(xrtsnappydecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtSnappyDecompressorTakeBuffer(xrtsnappydecompressor* pDecompressor);
XXAPI xbuffer xrtSnappyDecompressorDrainBuffer(xrtsnappydecompressor* pDecompressor);
XXAPI uint64 xrtSnappyDecompressorInputBytes(const xrtsnappydecompressor* pDecompressor);
XXAPI uint64 xrtSnappyDecompressorOutputBytes(const xrtsnappydecompressor* pDecompressor);
XXAPI bool xrtSnappyDecompressorFinished(const xrtsnappydecompressor* pDecompressor);
XXAPI uint64 xrtSnappyDecompressorMembers(const xrtsnappydecompressor* pDecompressor);
XXAPI const xrtsnappyerror* xrtSnappyDecompressorError(const xrtsnappydecompressor* pDecompressor);
XXAPI void xrtSnappyDecompressorDestroy(xrtsnappydecompressor* pDecompressor);

XXAPI bool xrtSnappyCompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer* pOutput, xrtsnappyerror* pError);
XXAPI bool xrtSnappyDecompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer* pOutput, xrtsnappyerror* pError);
XXAPI bool xrtSnappyCompressStream(xreader pReader, xwriter pWriter, const xrtsnappyoptions* pOptions,
	xrtsnappyerror* pError);
XXAPI bool xrtSnappyDecompressStream(xreader pReader, xwriter pWriter, const xrtsnappyoptions* pOptions,
	xrtsnappyerror* pError);

#ifdef __cplusplus
}
#endif

#endif
