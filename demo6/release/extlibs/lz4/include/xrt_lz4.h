#ifndef XRT_LZ4_H
#define XRT_LZ4_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_LZ4_OPTIONS_VERSION 1u

typedef enum {
	XRT_LZ4_OK = 0,
	XRT_LZ4_ERROR_ARGUMENT = 1,
	XRT_LZ4_ERROR_MEMORY = 2,
	XRT_LZ4_ERROR_DATA = 3,
	XRT_LZ4_ERROR_TRUNCATED = 4,
	XRT_LZ4_ERROR_LIMIT = 5,
	XRT_LZ4_ERROR_IO = 6,
	XRT_LZ4_ERROR_STATE = 7
} xrtlz4status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtlz4error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iAcceleration;
	uint8 sReserved[4];
	xrtresourcelimits tLimits;
} xrtlz4options;

typedef struct xrtlz4compressor xrtlz4compressor;
typedef struct xrtlz4decompressor xrtlz4decompressor;

XXAPI void xrtLz4OptionsInit(xrtlz4options* pOptions);
XXAPI void xrtLz4ErrorInit(xrtlz4error* pError);

XXAPI xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* pOptions);
XXAPI bool xrtLz4CompressorUpdate(xrtlz4compressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtLz4CompressorFinish(xrtlz4compressor* pCompressor);
XXAPI bool xrtLz4CompressorDrain(xrtlz4compressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtLz4CompressorTakeBuffer(xrtlz4compressor* pCompressor);
XXAPI xbuffer xrtLz4CompressorDrainBuffer(xrtlz4compressor* pCompressor);
XXAPI uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* pCompressor);
XXAPI uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* pCompressor);
XXAPI const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* pCompressor);
XXAPI void xrtLz4CompressorDestroy(xrtlz4compressor* pCompressor);

XXAPI xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* pOptions);
XXAPI bool xrtLz4DecompressorUpdate(xrtlz4decompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtLz4DecompressorFinish(xrtlz4decompressor* pDecompressor);
XXAPI bool xrtLz4DecompressorDrain(xrtlz4decompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* pDecompressor);
XXAPI xbuffer xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* pDecompressor);
XXAPI uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* pDecompressor);
XXAPI uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* pDecompressor);
XXAPI bool xrtLz4DecompressorFinished(const xrtlz4decompressor* pDecompressor);
XXAPI uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* pDecompressor);
XXAPI const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* pDecompressor);
XXAPI void xrtLz4DecompressorDestroy(xrtlz4decompressor* pDecompressor);

XXAPI bool xrtLz4Compress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer* pOutput, xrtlz4error* pError);
XXAPI bool xrtLz4Decompress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer* pOutput, xrtlz4error* pError);
XXAPI bool xrtLz4CompressStream(xreader pReader, xwriter pWriter, const xrtlz4options* pOptions,
	xrtlz4error* pError);
XXAPI bool xrtLz4DecompressStream(xreader pReader, xwriter pWriter, const xrtlz4options* pOptions,
	xrtlz4error* pError);

#ifdef __cplusplus
}
#endif

#endif
