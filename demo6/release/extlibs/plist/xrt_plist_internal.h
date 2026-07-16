#ifndef XRT_PLIST_INTERNAL_H
#define XRT_PLIST_INTERNAL_H

#include "include/xrt_plist.h"

#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint8* pKey;
	uint64 iKeySize;
	xrtplistnode* pValue;
} xrtplistdictitem;

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
xrtplistoptions __xrtPlistResolveOptions(const xrtplistoptions* pOptions);
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
xbuffer __xrtPlistWriteXml(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
xbuffer __xrtPlistWriteBinary(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

#endif
