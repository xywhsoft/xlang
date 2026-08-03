#ifndef XRT_ZSTD_H
#define XRT_ZSTD_H

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
	XRT_ZSTD_ERROR_STATE = 7
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
	xrtresourcelimits tLimits;
} xrtzstdoptions;

typedef struct xrtzstdcompressor xrtzstdcompressor;
typedef struct xrtzstddecompressor xrtzstddecompressor;

XXAPI void xrtZstdOptionsInit(xrtzstdoptions* pOptions);
XXAPI void xrtZstdErrorInit(xrtzstderror* pError);

XXAPI xrtzstdcompressor* xrtZstdCompressorCreate(const xrtzstdoptions* pOptions);
XXAPI bool xrtZstdCompressorUpdate(xrtzstdcompressor* pCompressor, const void* pData, size_t iSize);
XXAPI bool xrtZstdCompressorFinish(xrtzstdcompressor* pCompressor);
XXAPI bool xrtZstdCompressorDrain(xrtzstdcompressor* pCompressor, xwriter pWriter);
XXAPI xbuffer xrtZstdCompressorTakeBuffer(xrtzstdcompressor* pCompressor);
XXAPI xbuffer xrtZstdCompressorDrainBuffer(xrtzstdcompressor* pCompressor);
XXAPI uint64 xrtZstdCompressorInputBytes(const xrtzstdcompressor* pCompressor);
XXAPI uint64 xrtZstdCompressorOutputBytes(const xrtzstdcompressor* pCompressor);
XXAPI const xrtzstderror* xrtZstdCompressorError(const xrtzstdcompressor* pCompressor);
XXAPI void xrtZstdCompressorDestroy(xrtzstdcompressor* pCompressor);

XXAPI xrtzstddecompressor* xrtZstdDecompressorCreate(const xrtzstdoptions* pOptions);
XXAPI bool xrtZstdDecompressorUpdate(xrtzstddecompressor* pDecompressor, const void* pData, size_t iSize);
XXAPI bool xrtZstdDecompressorFinish(xrtzstddecompressor* pDecompressor);
XXAPI bool xrtZstdDecompressorDrain(xrtzstddecompressor* pDecompressor, xwriter pWriter);
XXAPI xbuffer xrtZstdDecompressorTakeBuffer(xrtzstddecompressor* pDecompressor);
XXAPI xbuffer xrtZstdDecompressorDrainBuffer(xrtzstddecompressor* pDecompressor);
XXAPI uint64 xrtZstdDecompressorInputBytes(const xrtzstddecompressor* pDecompressor);
XXAPI uint64 xrtZstdDecompressorOutputBytes(const xrtzstddecompressor* pDecompressor);
XXAPI bool xrtZstdDecompressorFinished(const xrtzstddecompressor* pDecompressor);
XXAPI uint64 xrtZstdDecompressorMembers(const xrtzstddecompressor* pDecompressor);
XXAPI const xrtzstderror* xrtZstdDecompressorError(const xrtzstddecompressor* pDecompressor);
XXAPI void xrtZstdDecompressorDestroy(xrtzstddecompressor* pDecompressor);

XXAPI bool xrtZstdCompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer* pOutput, xrtzstderror* pError);
XXAPI bool xrtZstdDecompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer* pOutput, xrtzstderror* pError);
XXAPI bool xrtZstdCompressStream(xreader pReader, xwriter pWriter, const xrtzstdoptions* pOptions,
	xrtzstderror* pError);
XXAPI bool xrtZstdDecompressStream(xreader pReader, xwriter pWriter, const xrtzstdoptions* pOptions,
	xrtzstderror* pError);

#ifdef __cplusplus
}
#endif

#endif
