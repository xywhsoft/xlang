#ifndef XRT_TOML_INTERNAL_H
#define XRT_TOML_INTERNAL_H

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
bool __xrtTomlLimitNode(xrt_toml_context* pContext, uint32 iDepth,
	uint64 iLine, uint64 iColumn);
xvalue __xrtTomlConvertDatum(xrt_toml_context* pContext,
	const toml_datum_t* pDatum, uint32 iDepth);
bool __xrtTomlAppend(xbuffer pOutput, const void* pData, size_t iSize,
	xrt_toml_context* pContext);
bool __xrtTomlAppendText(xbuffer pOutput, const char* sText,
	xrt_toml_context* pContext);
bool __xrtTomlEncodeValue(xbuffer pOutput, xvalue pValue,
	xrt_toml_context* pContext, uint32 iDepth, bool bInline);

#endif
