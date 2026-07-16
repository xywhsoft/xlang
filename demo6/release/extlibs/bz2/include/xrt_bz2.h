#ifndef XRT_BZ2_H
#define XRT_BZ2_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_BZ2_OPTIONS_VERSION 1u

typedef enum {
	XRT_BZ2_OK = 0,
	XRT_BZ2_ERROR_ARGUMENT = 1,
	XRT_BZ2_ERROR_MEMORY = 2,
	XRT_BZ2_ERROR_DATA = 3,
	XRT_BZ2_ERROR_TRUNCATED = 4,
	XRT_BZ2_ERROR_LIMIT = 5,
	XRT_BZ2_ERROR_IO = 6,
	XRT_BZ2_ERROR_STATE = 7
} xrtbz2status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtbz2error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	int iWorkFactor;
	bool bSmallMemory;
	uint8 sReserved[7];
	xrtresourcelimits tLimits;
} xrtbz2options;

typedef struct xrtbz2compressor xrtbz2compressor;
typedef struct xrtbz2decompressor xrtbz2decompressor;

XXAPI void xrtBz2OptionsInit(xrtbz2options* pOptions);
XXAPI void xrtBz2ErrorInit(xrtbz2error* pError);

XXAPI xrtbz2compressor* xrtBz2CompressorCreate(const xrtbz2options* pOptions);
XXAPI bool xrtBz2CompressorUpdate(xrtbz2compressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtBz2CompressorFinish(xrtbz2compressor* pCompressor);
XXAPI bool xrtBz2CompressorDrain(xrtbz2compressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtBz2CompressorTakeBuffer(xrtbz2compressor* pCompressor);
XXAPI const xrtbz2error* xrtBz2CompressorError(const xrtbz2compressor* pCompressor);
XXAPI void xrtBz2CompressorDestroy(xrtbz2compressor* pCompressor);

XXAPI xrtbz2decompressor* xrtBz2DecompressorCreate(const xrtbz2options* pOptions);
XXAPI bool xrtBz2DecompressorUpdate(xrtbz2decompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtBz2DecompressorFinish(xrtbz2decompressor* pDecompressor);
XXAPI bool xrtBz2DecompressorDrain(xrtbz2decompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtBz2DecompressorTakeBuffer(xrtbz2decompressor* pDecompressor);
XXAPI const xrtbz2error* xrtBz2DecompressorError(const xrtbz2decompressor* pDecompressor);
XXAPI void xrtBz2DecompressorDestroy(xrtbz2decompressor* pDecompressor);

XXAPI bool xrtBz2Compress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer* pOutput, xrtbz2error* pError);
XXAPI bool xrtBz2Decompress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer* pOutput, xrtbz2error* pError);
XXAPI bool xrtBz2CompressStream(xreader pReader, xwriter pWriter, const xrtbz2options* pOptions,
	xrtbz2error* pError);
XXAPI bool xrtBz2DecompressStream(xreader pReader, xwriter pWriter, const xrtbz2options* pOptions,
	xrtbz2error* pError);

#ifdef __cplusplus
}
#endif

#endif
