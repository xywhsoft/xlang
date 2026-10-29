#ifndef XRT_TOML_INTERNAL_H
#define XRT_TOML_INTERNAL_H

/* 与包清单一致的真实依赖；不以全模块声明掩盖缺项。 */
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
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/xrt_toml.h"
#include "../_tomlc17/src/tomlc17.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	xrttomloptions Options;
	xrttomlerror* Error;
	uint64 Nodes;
} xrt_toml_context;

void __xrtTomlSetError(xrttomlerror* pError, int iCode, uint64 iLine,
	uint64 iColumn, uint64 iOffset, const char* sFormat, ...);
bool __xrtTomlResolveOptions(const xrttomloptions* pOptions,
	xrttomloptions* pResolved, xrttomlerror* pError);
bool __xrtTomlLimitNode(xrt_toml_context* pContext, uint32 iDepth,
	uint64 iLine, uint64 iColumn);
xvalue* __xrtTomlConvertDatum(xrt_toml_context* pContext,
	const toml_datum_t* pDatum, uint32 iDepth);
bool __xrtTomlAppend(xbuffer* pOutput, const void* pData, size_t iSize,
	xrt_toml_context* pContext);
bool __xrtTomlAppendText(xbuffer* pOutput, const char* sText,
	xrt_toml_context* pContext);
bool __xrtTomlEncodeValue(xbuffer* pOutput, const xvalue* pValue,
	xrt_toml_context* pContext, uint32 iDepth, bool bInline);

#endif
