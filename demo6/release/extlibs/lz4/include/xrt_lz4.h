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
	XRT_LZ4_ERROR_STATE = 7,
	XRT_LZ4_ERROR_CANCELLED = 8
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

XRT_API void xrtLz4OptionsInit(xrtlz4options* pOptions);
XRT_API void xrtLz4ErrorInit(xrtlz4error* pError);

XRT_API xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* pOptions);
XRT_API bool xrtLz4CompressorUpdate(xrtlz4compressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtLz4CompressorFinish(xrtlz4compressor* pCompressor);
XRT_API bool xrtLz4CompressorDrain(xrtlz4compressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLz4CompressorTakeBuffer(xrtlz4compressor* pCompressor);
XRT_API xbuffer* xrtLz4CompressorDrainBuffer(xrtlz4compressor* pCompressor);
XRT_API uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* pCompressor);
XRT_API uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* pCompressor);
XRT_API const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* pCompressor);
XRT_API void xrtLz4CompressorDestroy(xrtlz4compressor* pCompressor);

XRT_API xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* pOptions);
XRT_API bool xrtLz4DecompressorUpdate(xrtlz4decompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtLz4DecompressorFinish(xrtlz4decompressor* pDecompressor);
XRT_API bool xrtLz4DecompressorDrain(xrtlz4decompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* pDecompressor);
XRT_API xbuffer* xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* pDecompressor);
XRT_API uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* pDecompressor);
XRT_API uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* pDecompressor);
XRT_API bool xrtLz4DecompressorFinished(const xrtlz4decompressor* pDecompressor);
XRT_API uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* pDecompressor);
XRT_API const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* pDecompressor);
XRT_API void xrtLz4DecompressorDestroy(xrtlz4decompressor* pDecompressor);

XRT_API bool xrtLz4Compress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError);
XRT_API bool xrtLz4Decompress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError);
XRT_API bool xrtLz4CompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError);
XRT_API bool xrtLz4DecompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError);

#ifdef __cplusplus
}
#endif

#endif
