#ifndef XRT_NETRC_INTERNAL_H
#define XRT_NETRC_INTERNAL_H

#include "include/xrt_netrc.h"

typedef struct {
	xrtnetrcoptions Options;
	xrtnetrcerror* Error;
	uint64 iNodes;
	uint64 iEntries;
} xrt_netrc_context;

void __xrtNetrcSetError(xrtnetrcerror* pError, int iCode,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sMessage);
bool __xrtNetrcLimitNode(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset);
bool __xrtNetrcLimitEntry(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset);
bool __xrtNetrcSetValue(xvalue pTable, const char* sKey, xvalue pValue);
xvalue __xrtNetrcText(const char* sText, size_t iSize);

#endif
