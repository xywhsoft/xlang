#ifndef XRT_BZ2_H
#define XRT_BZ2_H

/* bz2 公共契约直接依赖连续缓冲和通用 Reader/Writer。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif

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
	XRT_BZ2_ERROR_STATE = 7,
	XRT_BZ2_ERROR_CANCELLED = 8
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

XRT_API void xrtBz2OptionsInit(xrtbz2options* pOptions);
XRT_API void xrtBz2ErrorInit(xrtbz2error* pError);

XRT_API xrtbz2compressor* xrtBz2CompressorCreate(const xrtbz2options* pOptions);
XRT_API bool xrtBz2CompressorUpdate(xrtbz2compressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtBz2CompressorFinish(xrtbz2compressor* pCompressor);
XRT_API bool xrtBz2CompressorDrain(xrtbz2compressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtBz2CompressorTakeBuffer(xrtbz2compressor* pCompressor);
XRT_API xbuffer* xrtBz2CompressorDrainBuffer(xrtbz2compressor* pCompressor);
XRT_API uint64 xrtBz2CompressorInputBytes(const xrtbz2compressor* pCompressor);
XRT_API uint64 xrtBz2CompressorOutputBytes(const xrtbz2compressor* pCompressor);
XRT_API const xrtbz2error* xrtBz2CompressorError(const xrtbz2compressor* pCompressor);
XRT_API void xrtBz2CompressorDestroy(xrtbz2compressor* pCompressor);

XRT_API xrtbz2decompressor* xrtBz2DecompressorCreate(const xrtbz2options* pOptions);
XRT_API bool xrtBz2DecompressorUpdate(xrtbz2decompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtBz2DecompressorFinish(xrtbz2decompressor* pDecompressor);
XRT_API bool xrtBz2DecompressorDrain(xrtbz2decompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtBz2DecompressorTakeBuffer(xrtbz2decompressor* pDecompressor);
XRT_API xbuffer* xrtBz2DecompressorDrainBuffer(xrtbz2decompressor* pDecompressor);
XRT_API uint64 xrtBz2DecompressorInputBytes(const xrtbz2decompressor* pDecompressor);
XRT_API uint64 xrtBz2DecompressorOutputBytes(const xrtbz2decompressor* pDecompressor);
XRT_API bool xrtBz2DecompressorFinished(const xrtbz2decompressor* pDecompressor);
XRT_API uint64 xrtBz2DecompressorMembers(const xrtbz2decompressor* pDecompressor);
XRT_API const xrtbz2error* xrtBz2DecompressorError(const xrtbz2decompressor* pDecompressor);
XRT_API void xrtBz2DecompressorDestroy(xrtbz2decompressor* pDecompressor);

XRT_API bool xrtBz2Compress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer** ppOutput, xrtbz2error* pError);
XRT_API bool xrtBz2Decompress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer** ppOutput, xrtbz2error* pError);
XRT_API bool xrtBz2CompressStream(xreader* pReader, xwriter* pWriter, const xrtbz2options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbz2error* pError);
XRT_API bool xrtBz2DecompressStream(xreader* pReader, xwriter* pWriter, const xrtbz2options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbz2error* pError);

#ifdef __cplusplus
}
#endif

#endif
