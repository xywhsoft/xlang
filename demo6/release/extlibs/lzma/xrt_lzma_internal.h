#ifndef XRT_LZMA_INTERNAL_H
#define XRT_LZMA_INTERNAL_H

#include "include/xrt_lzma.h"
#include "third_party/xz/XzEnc.h"
#include <stdint.h>

#define XRT_LZMA_CHUNK_SIZE 65536u

extern const ISzAlloc xlzmaAllocator;

void xlzmaSetError(xrtlzmaerror* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sMessage);
int xlzmaStreamErrorCode(void);
bool xlzmaAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit);
bool xlzmaOverRatio(const xrtresourcelimits* pLimits,
	uint64 iInput, uint64 iOutput);
const char* xlzmaStatusName(SRes iResult);
xbuffer* xlzmaBufferCreate(void);
bool xlzmaPrepareOptions(const xrtlzmaoptions* pInput,
	xrtlzmaoptions* pOutput, xrtlzmaerror* pError);
bool xlzmaInitXzTables(void);

#endif
