#include "xrt_plist_internal.h"



typedef struct {
	xrtplistnode* pNode;
	const uint8* pKey;
	uint64 iKeySize;
	uint64* pReferences;
	uint64 iReferenceCount;
	uint8 iState;
	bool bKey;
} xrtplistbinaryobject;

typedef struct {
	xbuffer* pOutput;
	xrtplistcontext Context;
	xrtplistbinaryobject* pObjects;
	uint64 iObjectCount;
	uint64 iObjectCapacity;
	uint8 iReferenceSize;
} xrtplistbinarywriter;

static bool __xrtPlistBinaryAppend(xrtplistbinarywriter* pWriter,
	const void* pData, uint64 iSize)
{
	uint64 iLimit = pWriter->Context.Options.tLimits.iMaxOutputBytes;
	if ( iSize == 0u ) { return true; }
	if ( pData == NULL ||
		(iLimit != 0u && (iSize > iLimit ||
		 (uint64)pWriter->pOutput->Size > iLimit - iSize)) ||
		!xrtBufferAppend(pWriter->pOutput,
			(xbytesview){ (cbytes)pData, (size_t)iSize }) ) {
		__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "binary plist output exceeds the configured limit");
		return false;
	}
	return true;
}

static bool __xrtPlistBinaryAppendUInt(xrtplistbinarywriter* pWriter,
	uint64 iValue, uint8 iSize)
{
	uint8 sBytes[8];
	uint8 i;
	for ( i = 0u; i < iSize; i++ ) {
		sBytes[iSize - i - 1u] = (uint8)(iValue & 0xffu);
		iValue >>= 8u;
	}
	return __xrtPlistBinaryAppend(pWriter, sBytes, iSize);
}

static uint8 __xrtPlistBinarySize(uint64 iValue)
{
	if ( iValue <= 0xffu ) { return 1u; }
	if ( iValue <= 0xffffu ) { return 2u; }
	if ( iValue <= 0xffffffffu ) { return 4u; }
	return 8u;
}

static bool __xrtPlistBinaryInlineInteger(xrtplistbinarywriter* pWriter,
	uint64 iValue)
{
	uint8 iSize;
	uint8 iMarker;
	if ( iValue <= 0x7fu ) { iSize = 1u; iMarker = 0x10u; }
	else if ( iValue <= 0x7fffu ) { iSize = 2u; iMarker = 0x11u; }
	else if ( iValue <= 0x7fffffffu ) { iSize = 4u; iMarker = 0x12u; }
	else { iSize = 8u; iMarker = 0x13u; }
	return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
		__xrtPlistBinaryAppendUInt(pWriter, iValue, iSize);
}

static bool __xrtPlistBinaryHeader(xrtplistbinarywriter* pWriter,
	uint8 iKind, uint64 iLength)
{
	uint8 iMarker = (uint8)(iKind << 4u);
	if ( iLength < 15u ) {
		iMarker |= (uint8)iLength;
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u);
	}
	iMarker |= 0x0fu;
	return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
		__xrtPlistBinaryInlineInteger(pWriter, iLength);
}

static int __xrtPlistBinaryKeyCompare(const xrtplistdictitem* pLeft,
	const xrtplistdictitem* pRight)
{
	uint64 iMin = pLeft->iKeySize < pRight->iKeySize ?
		pLeft->iKeySize : pRight->iKeySize;
	int iResult = iMin != 0u ? memcmp(pLeft->pKey, pRight->pKey,
		(size_t)iMin) : 0;
	if ( iResult != 0 ) { return iResult; }
	return pLeft->iKeySize < pRight->iKeySize ? -1 :
		(pLeft->iKeySize > pRight->iKeySize ? 1 : 0);
}

static uint32* __xrtPlistBinaryDictOrder(const xrtplistnode* pNode,
	bool bSort)
{
	uint32* pOrder;
	uint32 i;
	if ( pNode->Value.Dict.iCount == 0u ) { return NULL; }
	#if SIZE_MAX == UINT32_MAX
		if ( pNode->Value.Dict.iCount > SIZE_MAX / sizeof(uint32) ) { return NULL; }
	#endif
	pOrder = (uint32*)xrtMalloc(sizeof(uint32) * (size_t)pNode->Value.Dict.iCount);
	if ( pOrder == NULL ) { return NULL; }
	for ( i = 0u; i < pNode->Value.Dict.iCount; i++ ) { pOrder[i] = i; }
	if ( bSort ) {
		for ( i = 1u; i < pNode->Value.Dict.iCount; i++ ) {
			uint32 iValue = pOrder[i];
			uint32 j = i;
			while ( j != 0u && __xrtPlistBinaryKeyCompare(
				&pNode->Value.Dict.pItems[pOrder[j - 1u]],
				&pNode->Value.Dict.pItems[iValue]) > 0 ) {
				pOrder[j] = pOrder[j - 1u]; j--;
			}
			pOrder[j] = iValue;
		}
	}
	return pOrder;
}

static bool __xrtPlistBinaryGrowObjects(xrtplistbinarywriter* pWriter)
{
	xrtplistbinaryobject* pItems;
	uint64 iCapacity;
	if ( pWriter->iObjectCount < pWriter->iObjectCapacity ) { return true; }
	iCapacity = pWriter->iObjectCapacity != 0u ?
		pWriter->iObjectCapacity * 2u : 32u;
	if ( iCapacity > SIZE_MAX / sizeof(xrtplistbinaryobject) ) { return false; }
	pItems = (xrtplistbinaryobject*)xrtRealloc(pWriter->pObjects,
		(size_t)iCapacity * sizeof(xrtplistbinaryobject));
	if ( pItems == NULL ) { return false; }
	memset(pItems + pWriter->iObjectCapacity, 0,
		(size_t)(iCapacity - pWriter->iObjectCapacity) *
		sizeof(xrtplistbinaryobject));
	pWriter->pObjects = pItems;
	pWriter->iObjectCapacity = iCapacity;
	return true;
}

static int64 __xrtPlistBinaryFindNode(const xrtplistbinarywriter* pWriter,
	const xrtplistnode* pNode)
{
	uint64 i;
	for ( i = 0u; i < pWriter->iObjectCount; i++ ) {
		if ( !pWriter->pObjects[i].bKey &&
			pWriter->pObjects[i].pNode == pNode ) { return (int64)i; }
	}
	return -1;
}

static bool __xrtPlistBinaryAddKey(xrtplistbinarywriter* pWriter,
	const uint8* pKey, uint64 iKeySize, uint64* pObject)
{
	xrtplistbinaryobject* pItem;
	if ( !__xrtPlistLimitItem(&pWriter->Context, iKeySize, 0u) ||
		!__xrtPlistBinaryGrowObjects(pWriter) ) { return false; }
	pWriter->Context.iNodes++;
	if ( pWriter->Context.Options.tLimits.iMaxNodes != 0u &&
		pWriter->Context.iNodes >
		pWriter->Context.Options.tLimits.iMaxNodes ) {
		__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "binary plist object count exceeds the configured limit");
		return false;
	}
	*pObject = pWriter->iObjectCount++;
	pItem = &pWriter->pObjects[*pObject];
	pItem->bKey = true;
	pItem->pKey = pKey;
	pItem->iKeySize = iKeySize;
	pItem->iState = 2u;
	return true;
}

static bool __xrtPlistBinaryAddNode(xrtplistbinarywriter* pWriter,
	xrtplistnode* pNode, uint32 iDepth, uint64* pObject)
{
	int64 iExisting = __xrtPlistBinaryFindNode(pWriter, pNode);
	uint64 iIndex;
	uint64 i;
	uint64 iRefs;
	if ( iExisting >= 0 ) {
		if ( pWriter->pObjects[iExisting].iState == 1u ) {
			__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_FORMAT,
				0u, 0u, 0u, "plist value graph contains a cycle");
			return false;
		}
		*pObject = (uint64)iExisting;
		return true;
	}
	if ( pNode == NULL || !__xrtPlistLimitNode(&pWriter->Context,
		iDepth, 0u) || !__xrtPlistBinaryGrowObjects(pWriter) ) { return false; }
	iIndex = pWriter->iObjectCount++;
	pWriter->pObjects[iIndex].pNode = pNode;
	pWriter->pObjects[iIndex].iState = 1u;
	*pObject = iIndex;
	if ( pNode->iType == XRT_PLIST_ARRAY ) {
		iRefs = pNode->Value.Array.iCount;
		if ( pWriter->Context.Options.tLimits.iMaxEntries != 0u &&
			iRefs > pWriter->Context.Options.tLimits.iMaxEntries ) { return false; }
		if ( iRefs != 0u ) {
			pWriter->pObjects[iIndex].pReferences =
				(uint64*)xrtMalloc(sizeof(uint64) * (size_t)iRefs);
			if ( pWriter->pObjects[iIndex].pReferences == NULL ) { return false; }
		}
		pWriter->pObjects[iIndex].iReferenceCount = iRefs;
		for ( i = 0u; i < iRefs; i++ ) {
			if ( !__xrtPlistBinaryAddNode(pWriter,
				pNode->Value.Array.pItems[i], iDepth + 1u,
				&pWriter->pObjects[iIndex].pReferences[i]) ) { return false; }
		}
	} else if ( pNode->iType == XRT_PLIST_DICT ) {
		uint32* pOrder;
		iRefs = pNode->Value.Dict.iCount;
		if ( pWriter->Context.Options.tLimits.iMaxEntries != 0u &&
			iRefs > pWriter->Context.Options.tLimits.iMaxEntries ) { return false; }
		pOrder = __xrtPlistBinaryDictOrder(pNode,
			pWriter->Context.Options.bSortKeys);
		if ( iRefs != 0u && pOrder == NULL ) { return false; }
		if ( iRefs != 0u ) {
			pWriter->pObjects[iIndex].pReferences =
				(uint64*)xrtMalloc(sizeof(uint64) * (size_t)iRefs * 2u);
			if ( pWriter->pObjects[iIndex].pReferences == NULL ) {
				xrtFree(pOrder); return false;
			}
		}
		pWriter->pObjects[iIndex].iReferenceCount = iRefs * 2u;
		for ( i = 0u; i < iRefs; i++ ) {
			xrtplistdictitem* pItem = &pNode->Value.Dict.pItems[pOrder[i]];
			if ( !__xrtPlistBinaryAddKey(pWriter, pItem->pKey,
				pItem->iKeySize,
				&pWriter->pObjects[iIndex].pReferences[i]) ||
				!__xrtPlistBinaryAddNode(pWriter, pItem->pValue,
					iDepth + 1u,
					&pWriter->pObjects[iIndex].pReferences[iRefs + i]) ) {
				xrtFree(pOrder); return false;
			}
		}
		xrtFree(pOrder);
	}
	pWriter->pObjects[iIndex].iState = 2u;
	return true;
}

static bool __xrtPlistBinaryString(xrtplistbinarywriter* pWriter,
	const uint8* pData, uint64 iSize)
{
	uint64 i;
	bool bAscii = true;
	uint16* pUtf16;
	size_t iUnits = 0u;
	if ( !__xrtPlistLimitItem(&pWriter->Context, iSize, 0u) ||
		iSize > SIZE_MAX || (iSize != 0u &&
		!xrtUtf8Valid((xstrview){ (cstr)pData, (size_t)iSize }, NULL)) ) { return false; }
	for ( i = 0u; i < iSize; i++ ) {
		if ( pData[i] > 0x7fu ) { bAscii = false; break; }
	}
	if ( bAscii ) {
		return __xrtPlistBinaryHeader(pWriter, 0x5u, iSize) &&
			__xrtPlistBinaryAppend(pWriter, pData, iSize);
	}
	pUtf16 = xrtUtf8ViewTo16((xstrview){ (cstr)pData, (size_t)iSize },
		XUTF_STRICT, &iUnits);
	if ( pUtf16 == NULL || !__xrtPlistBinaryHeader(pWriter, 0x6u, iUnits) ) {
		xrtFree(pUtf16); return false;
	}
	for ( i = 0u; i < iUnits; i++ ) {
		uint8 sBytes[2] = {
			(uint8)(pUtf16[i] >> 8u), (uint8)(pUtf16[i] & 0xffu)
		};
		if ( !__xrtPlistBinaryAppend(pWriter, sBytes, 2u) ) {
			xrtFree(pUtf16); return false;
		}
	}
	xrtFree(pUtf16);
	return true;
}

static bool __xrtPlistBinaryWriteObject(xrtplistbinarywriter* pWriter,
	const xrtplistbinaryobject* pObject)
{
	xrtplistnode* pNode = pObject->pNode;
	uint8 iMarker;
	uint8 iSize;
	uint64 iBits;
	uint64 i;
	if ( pObject->bKey ) {
		return __xrtPlistBinaryString(pWriter, pObject->pKey,
			pObject->iKeySize);
	}
	switch ( pNode->iType ) {
	case XRT_PLIST_NULL:
		iMarker = 0x00u;
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u);
	case XRT_PLIST_BOOL:
		iMarker = pNode->Value.bValue ? 0x09u : 0x08u;
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u);
	case XRT_PLIST_INT:
		/* Binary plist 的短整数是无符号的；负数必须写为 8 字节。 */
		if ( pNode->Value.iValue >= 0 && pNode->Value.iValue <= UINT8_MAX ) {
			iSize = 1u; iMarker = 0x10u;
		} else if ( pNode->Value.iValue >= 0 &&
			pNode->Value.iValue <= UINT16_MAX ) { iSize = 2u; iMarker = 0x11u; }
		else if ( pNode->Value.iValue >= 0 &&
			pNode->Value.iValue <= UINT32_MAX ) { iSize = 4u; iMarker = 0x12u; }
		else { iSize = 8u; iMarker = 0x13u; }
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
			__xrtPlistBinaryAppendUInt(pWriter,
				(uint64)pNode->Value.iValue, iSize);
	case XRT_PLIST_REAL:
		iMarker = 0x23u;
		memcpy(&iBits, &pNode->Value.fValue, sizeof(iBits));
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
			__xrtPlistBinaryAppendUInt(pWriter, iBits, 8u);
	case XRT_PLIST_STRING:
		return __xrtPlistBinaryString(pWriter, pNode->Value.Bytes.pData,
			pNode->Value.Bytes.iSize);
	case XRT_PLIST_DATA:
		return __xrtPlistLimitItem(&pWriter->Context,
			pNode->Value.Bytes.iSize, 0u) &&
			__xrtPlistBinaryHeader(pWriter, 0x4u,
				pNode->Value.Bytes.iSize) &&
			__xrtPlistBinaryAppend(pWriter, pNode->Value.Bytes.pData,
				pNode->Value.Bytes.iSize);
	case XRT_PLIST_DATE: {
		double fSeconds;
		if ( !__xrtPlistBinaryTimeToSeconds(pNode->Value.tValue,
			&fSeconds) ) {
			__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_TYPE,
				0u, 0u, 0u, "binary plist date cannot preserve milliseconds");
			return false;
		}
		iMarker = 0x33u;
		memcpy(&iBits, &fSeconds, sizeof(iBits));
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
			__xrtPlistBinaryAppendUInt(pWriter, iBits, 8u);
	}
	case XRT_PLIST_UID:
		iSize = 1u;
		while ( iSize < 8u &&
			(pNode->Value.iUid >> (iSize * 8u)) != 0u ) { iSize++; }
		iMarker = (uint8)(0x80u | (iSize - 1u));
		return __xrtPlistBinaryAppend(pWriter, &iMarker, 1u) &&
			__xrtPlistBinaryAppendUInt(pWriter, pNode->Value.iUid, iSize);
	case XRT_PLIST_ARRAY:
		if ( !__xrtPlistBinaryHeader(pWriter, 0xau,
			pNode->Value.Array.iCount) ) { return false; }
		for ( i = 0u; i < pObject->iReferenceCount; i++ ) {
			if ( !__xrtPlistBinaryAppendUInt(pWriter,
				pObject->pReferences[i], pWriter->iReferenceSize) ) { return false; }
		}
		return true;
	case XRT_PLIST_DICT:
		if ( !__xrtPlistBinaryHeader(pWriter, 0xdu,
			pNode->Value.Dict.iCount) ) { return false; }
		for ( i = 0u; i < pObject->iReferenceCount; i++ ) {
			if ( !__xrtPlistBinaryAppendUInt(pWriter,
				pObject->pReferences[i], pWriter->iReferenceSize) ) { return false; }
		}
		return true;
	default:
		__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_TYPE,
			0u, 0u, 0u, "unsupported plist node type");
		return false;
	}
}

xbuffer* __xrtPlistWriteBinary(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplistbinarywriter tWriter;
	uint64 iRoot;
	uint64* pOffsets = NULL;
	uint64 iOffsetTable;
	uint8 iOffsetSize;
	uint8 sTrailer[32];
	uint64 i;
	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.Context.Options = *pOptions;
	tWriter.Context.Error = pError;
	tWriter.pOutput = xrtBufferCreate();
	if ( tWriter.pOutput == NULL || !xrtBufferReserve(tWriter.pOutput, 1024u) ||
		!__xrtPlistBinaryAddNode(&tWriter,
		pDocument->pRoot, 1u, &iRoot) || tWriter.iObjectCount == 0u ) {
		goto error;
	}
	tWriter.iReferenceSize = __xrtPlistBinarySize(tWriter.iObjectCount - 1u);
	pOffsets = (uint64*)xrtMalloc(sizeof(uint64) *
		(size_t)tWriter.iObjectCount);
	if ( pOffsets == NULL ||
		!__xrtPlistBinaryAppend(&tWriter, "bplist00", 8u) ) { goto error; }
	for ( i = 0u; i < tWriter.iObjectCount; i++ ) {
		pOffsets[i] = tWriter.pOutput->Size;
		if ( !__xrtPlistBinaryWriteObject(&tWriter, &tWriter.pObjects[i]) ) {
			goto error;
		}
	}
	iOffsetTable = tWriter.pOutput->Size;
	iOffsetSize = __xrtPlistBinarySize(iOffsetTable);
	for ( i = 0u; i < tWriter.iObjectCount; i++ ) {
		if ( !__xrtPlistBinaryAppendUInt(&tWriter, pOffsets[i], iOffsetSize) ) {
			goto error;
		}
	}
	memset(sTrailer, 0, sizeof(sTrailer));
	sTrailer[6] = iOffsetSize;
	sTrailer[7] = tWriter.iReferenceSize;
	for ( i = 0u; i < 8u; i++ ) {
		sTrailer[15u - i] = (uint8)(tWriter.iObjectCount >> (i * 8u));
		sTrailer[23u - i] = (uint8)(iRoot >> (i * 8u));
		sTrailer[31u - i] = (uint8)(iOffsetTable >> (i * 8u));
	}
	if ( !__xrtPlistBinaryAppend(&tWriter, sTrailer, sizeof(sTrailer)) ) {
		goto error;
	}
	goto cleanup;

error:
	if ( pError->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_MEMORY,
			0u, 0u, 0u, "could not build binary plist output");
	}
	xrtBufferDestroy(tWriter.pOutput);
	tWriter.pOutput = NULL;
cleanup:
	for ( i = 0u; i < tWriter.iObjectCount; i++ ) {
		xrtFree(tWriter.pObjects[i].pReferences);
	}
	xrtFree(tWriter.pObjects);
	xrtFree(pOffsets);
	return tWriter.pOutput;
}
