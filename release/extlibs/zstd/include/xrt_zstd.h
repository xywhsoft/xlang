#ifndef XRT_ZSTD_H
#define XRT_ZSTD_H

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

#define XRT_ZSTD_OPTIONS_VERSION 1u

typedef enum {
	XRT_ZSTD_OK = 0,
	XRT_ZSTD_ERROR_ARGUMENT = 1,
	XRT_ZSTD_ERROR_MEMORY = 2,
	XRT_ZSTD_ERROR_DATA = 3,
	XRT_ZSTD_ERROR_TRUNCATED = 4,
	XRT_ZSTD_ERROR_LIMIT = 5,
	XRT_ZSTD_ERROR_IO = 6,
	XRT_ZSTD_ERROR_STATE = 7,
	XRT_ZSTD_ERROR_CANCELLED = 8
} xrtzstdstatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtzstderror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	uint8 sReserved[4];
	/* iMaxEntries 限制解压帧数；nodes/depth/flags 不适用。 */
	xrtresourcelimits tLimits;
} xrtzstdoptions;

typedef struct xrtzstdcompressor xrtzstdcompressor;
typedef struct xrtzstddecompressor xrtzstddecompressor;

XRT_API void xrtZstdOptionsInit(xrtzstdoptions* pOptions);
XRT_API void xrtZstdErrorInit(xrtzstderror* pError);

XRT_API xrtzstdcompressor* xrtZstdCompressorCreate(const xrtzstdoptions* pOptions);
XRT_API bool xrtZstdCompressorUpdate(xrtzstdcompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtZstdCompressorFinish(xrtzstdcompressor* pCompressor);
XRT_API bool xrtZstdCompressorDrain(xrtzstdcompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtZstdCompressorTakeBuffer(xrtzstdcompressor* pCompressor);
XRT_API xbuffer* xrtZstdCompressorDrainBuffer(xrtzstdcompressor* pCompressor);
XRT_API uint64 xrtZstdCompressorInputBytes(const xrtzstdcompressor* pCompressor);
XRT_API uint64 xrtZstdCompressorOutputBytes(const xrtzstdcompressor* pCompressor);
XRT_API const xrtzstderror* xrtZstdCompressorError(const xrtzstdcompressor* pCompressor);
XRT_API void xrtZstdCompressorDestroy(xrtzstdcompressor* pCompressor);

XRT_API xrtzstddecompressor* xrtZstdDecompressorCreate(const xrtzstdoptions* pOptions);
XRT_API bool xrtZstdDecompressorUpdate(xrtzstddecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtZstdDecompressorFinish(xrtzstddecompressor* pDecompressor);
XRT_API bool xrtZstdDecompressorDrain(xrtzstddecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtZstdDecompressorTakeBuffer(xrtzstddecompressor* pDecompressor);
XRT_API xbuffer* xrtZstdDecompressorDrainBuffer(xrtzstddecompressor* pDecompressor);
XRT_API uint64 xrtZstdDecompressorInputBytes(const xrtzstddecompressor* pDecompressor);
XRT_API uint64 xrtZstdDecompressorOutputBytes(const xrtzstddecompressor* pDecompressor);
XRT_API bool xrtZstdDecompressorFinished(const xrtzstddecompressor* pDecompressor);
XRT_API uint64 xrtZstdDecompressorMembers(const xrtzstddecompressor* pDecompressor);
XRT_API const xrtzstderror* xrtZstdDecompressorError(const xrtzstddecompressor* pDecompressor);
XRT_API void xrtZstdDecompressorDestroy(xrtzstddecompressor* pDecompressor);

XRT_API bool xrtZstdCompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer** ppOutput, xrtzstderror* pError);
XRT_API bool xrtZstdDecompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer** ppOutput, xrtzstderror* pError);
XRT_API bool xrtZstdCompressStream(xreader* pReader, xwriter* pWriter, const xrtzstdoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtzstderror* pError);
XRT_API bool xrtZstdDecompressStream(xreader* pReader, xwriter* pWriter, const xrtzstdoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtzstderror* pError);

#ifdef __cplusplus
}
#endif

#endif
