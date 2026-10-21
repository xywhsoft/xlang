#ifndef XRT_NETRC_INTERNAL_H
#define XRT_NETRC_INTERNAL_H

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/xrt_netrc.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	xrtnetrcoptions Options;
	xrtnetrcerror* Error;
	uint64 iNodes;
	uint64 iEntries;
} xrt_netrc_context;

void __xrtNetrcSetError(xrtnetrcerror* pError, int iCode,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sMessage);
bool __xrtNetrcResolveOptions(const xrtnetrcoptions* pOptions,
	xrtnetrcoptions* pResolved, xrtnetrcerror* pError);
bool __xrtNetrcLimitNode(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset);
bool __xrtNetrcLimitEntry(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset);
bool __xrtNetrcSetValue(xvalue* pTable, const char* sKey, xvalue* pValue);
xvalue* __xrtNetrcText(const char* sText, size_t iSize);

#endif
