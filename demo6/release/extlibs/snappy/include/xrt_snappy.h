#ifndef XRT_SNAPPY_H
#define XRT_SNAPPY_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
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
	XRT_SNAPPY_ERROR_STATE = 7,
	XRT_SNAPPY_ERROR_CANCELLED = 8
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
	/* iMaxEntries 限制 framed stream identifier 数量；nodes/depth/flags 不适用。 */
	xrtresourcelimits tLimits;
} xrtsnappyoptions;

typedef struct xrtsnappycompressor xrtsnappycompressor;
typedef struct xrtsnappydecompressor xrtsnappydecompressor;

XRT_API void xrtSnappyOptionsInit(xrtsnappyoptions* pOptions);
XRT_API void xrtSnappyErrorInit(xrtsnappyerror* pError);

XRT_API xrtsnappycompressor* xrtSnappyCompressorCreate(const xrtsnappyoptions* pOptions);
XRT_API bool xrtSnappyCompressorUpdate(xrtsnappycompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtSnappyCompressorFinish(xrtsnappycompressor* pCompressor);
XRT_API bool xrtSnappyCompressorDrain(xrtsnappycompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtSnappyCompressorTakeBuffer(xrtsnappycompressor* pCompressor);
XRT_API xbuffer* xrtSnappyCompressorDrainBuffer(xrtsnappycompressor* pCompressor);
XRT_API uint64 xrtSnappyCompressorInputBytes(const xrtsnappycompressor* pCompressor);
XRT_API uint64 xrtSnappyCompressorOutputBytes(const xrtsnappycompressor* pCompressor);
XRT_API const xrtsnappyerror* xrtSnappyCompressorError(const xrtsnappycompressor* pCompressor);
XRT_API void xrtSnappyCompressorDestroy(xrtsnappycompressor* pCompressor);

XRT_API xrtsnappydecompressor* xrtSnappyDecompressorCreate(const xrtsnappyoptions* pOptions);
XRT_API bool xrtSnappyDecompressorUpdate(xrtsnappydecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtSnappyDecompressorFinish(xrtsnappydecompressor* pDecompressor);
XRT_API bool xrtSnappyDecompressorDrain(xrtsnappydecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtSnappyDecompressorTakeBuffer(xrtsnappydecompressor* pDecompressor);
XRT_API xbuffer* xrtSnappyDecompressorDrainBuffer(xrtsnappydecompressor* pDecompressor);
XRT_API uint64 xrtSnappyDecompressorInputBytes(const xrtsnappydecompressor* pDecompressor);
XRT_API uint64 xrtSnappyDecompressorOutputBytes(const xrtsnappydecompressor* pDecompressor);
XRT_API bool xrtSnappyDecompressorFinished(const xrtsnappydecompressor* pDecompressor);
XRT_API uint64 xrtSnappyDecompressorMembers(const xrtsnappydecompressor* pDecompressor);
XRT_API const xrtsnappyerror* xrtSnappyDecompressorError(const xrtsnappydecompressor* pDecompressor);
XRT_API void xrtSnappyDecompressorDestroy(xrtsnappydecompressor* pDecompressor);

XRT_API bool xrtSnappyCompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer** ppOutput, xrtsnappyerror* pError);
XRT_API bool xrtSnappyDecompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer** ppOutput, xrtsnappyerror* pError);
XRT_API bool xrtSnappyCompressStream(xreader* pReader, xwriter* pWriter, const xrtsnappyoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtsnappyerror* pError);
XRT_API bool xrtSnappyDecompressStream(xreader* pReader, xwriter* pWriter, const xrtsnappyoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtsnappyerror* pError);

#ifdef __cplusplus
}
#endif

#endif
