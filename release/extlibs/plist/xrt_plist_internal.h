#ifndef XRT_PLIST_INTERNAL_H
#define XRT_PLIST_INTERNAL_H

/* 包清单可能已指定这些模块，避免重复定义。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
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
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/xrt_plist.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint8* pKey;
	uint64 iKeySize;
	xrtplistnode* pValue;
} xrtplistdictitem;

bool __xrtPlistAppleEpoch(xtime* pResult);
bool __xrtPlistBinarySecondsToTime(double fSeconds, xtime* pResult);
bool __xrtPlistBinaryTimeToSeconds(xtime tValue, double* pResult);

struct xrtplistnode_struct {
	xrtplistdocument* pDocument;
	int iType;
	union {
		bool bValue;
		int64 iValue;
		double fValue;
		xtime tValue;
		uint64 iUid;
		struct { uint8* pData; uint64 iSize; } Bytes;
		struct { xrtplistnode** pItems; uint32 iCount; uint32 iCapacity; } Array;
		struct { xrtplistdictitem* pItems; uint32 iCount; uint32 iCapacity; } Dict;
	} Value;
};

struct xrtplistdocument_struct {
	xrtplistnode* pRoot;
	xrtplistnode** pNodes;
	uint32 iNodeCount;
	uint32 iNodeCapacity;
};

typedef struct {
	xrtplistoptions Options;
	xrtplisterror* Error;
	uint64 iNodes;
} xrtplistcontext;

void __xrtPlistSetError(xrtplisterror* pError, int iCode,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sFormat, ...);
bool __xrtPlistResolveOptions(const xrtplistoptions* pOptions,
	xrtplistoptions* pResolved, xrtplisterror* pError);
bool __xrtPlistLimitNode(xrtplistcontext* pContext, uint32 iDepth,
	uint64 iOffset);
bool __xrtPlistLimitItem(xrtplistcontext* pContext, uint64 iSize,
	uint64 iOffset);
xrtplistnode* __xrtPlistCreateNode(xrtplistdocument* pDocument, int iType);
bool __xrtPlistSetBytes(xrtplistnode* pNode, const void* pData,
	uint64 iSize, bool bString);
xrtplistdocument* __xrtPlistParseXml(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
xrtplistdocument* __xrtPlistParseBinary(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
xbuffer* __xrtPlistWriteXml(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
xbuffer* __xrtPlistWriteBinary(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

#endif
