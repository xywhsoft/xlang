#include "xrt_plist_internal.h"



typedef struct {
	const uint8* pData;
	uint64 iSize;
	uint64 iOffsetTable;
	uint64 iObjectCount;
	uint8 iOffsetSize;
	uint8 iReferenceSize;
	uint64* pOffsets;
	uint8* pStates;
	xrtplistnode** pNodes;
	xrtplistdocument* pDocument;
	xrtplistcontext Context;
} xrtplistbinaryreader;

static bool __xrtPlistBinaryRange(const xrtplistbinaryreader* pReader,
	uint64 iOffset, uint64 iSize)
{
	return iOffset <= pReader->iOffsetTable &&
		iSize <= pReader->iOffsetTable - iOffset;
}

static bool __xrtPlistBinaryUInt(const uint8* pData, uint8 iSize,
	uint64* pValue)
{
	uint64 iValue = 0u;
	uint8 i;
	if ( pData == NULL || pValue == NULL || iSize == 0u || iSize > 8u ) {
		return FALSE;
	}
	for ( i = 0u; i < iSize; i++ ) { iValue = (iValue << 8u) | pData[i]; }
	*pValue = iValue;
	return TRUE;
}

static bool __xrtPlistBinaryLength(xrtplistbinaryreader* pReader,
	uint64 iOffset, uint8 iInline, uint64* pLength, uint64* pHeaderSize)
{
	uint64 iBytes;
	uint64 iValue;
	uint8 iMarker;
	if ( iInline != 0x0fu ) {
		*pLength = iInline;
		*pHeaderSize = 1u;
		return TRUE;
	}
	if ( !__xrtPlistBinaryRange(pReader, iOffset + 1u, 1u) ) { return FALSE; }
	iMarker = pReader->pData[iOffset + 1u];
	if ( (iMarker >> 4u) != 0x1u || (iMarker & 0x0fu) > 3u ) { return FALSE; }
	iBytes = 1ull << (iMarker & 0x0fu);
	if ( !__xrtPlistBinaryRange(pReader, iOffset + 2u, iBytes) ||
		!__xrtPlistBinaryUInt(pReader->pData + iOffset + 2u,
			(uint8)iBytes, &iValue) ) { return FALSE; }
	*pLength = iValue;
	*pHeaderSize = 2u + iBytes;
	return TRUE;
}

static bool __xrtPlistBinaryUtf16(const uint8* pData, uint64 iUnits,
	uint8** ppUtf8, uint64* pUtf8Size)
{
	uint16* pUtf16;
	u8str pConverted;
	size_t iConverted = 0u;
	uint64 i;
	if ( iUnits > (SIZE_MAX / sizeof(uint16)) - 1u ) { return FALSE; }
	pUtf16 = (uint16*)xrtMalloc(((size_t)iUnits + 1u) * sizeof(uint16));
	if ( pUtf16 == NULL ) { return FALSE; }
	for ( i = 0u; i < iUnits; i++ ) {
		uint16 c = (uint16)(((uint16)pData[i * 2u] << 8u) | pData[i * 2u + 1u]);
		if ( c >= 0xd800u && c <= 0xdbffu ) {
			if ( i + 1u >= iUnits ) { xrtFree(pUtf16); return FALSE; }
			if ( pData[(i + 1u) * 2u] < 0xdcu ||
				pData[(i + 1u) * 2u] > 0xdfu ) { xrtFree(pUtf16); return FALSE; }
		} else if ( c >= 0xdc00u && c <= 0xdfffu ) {
			if ( i == 0u || pUtf16[i - 1u] < 0xd800u ||
				pUtf16[i - 1u] > 0xdbffu ) { xrtFree(pUtf16); return FALSE; }
		}
		pUtf16[i] = c;
	}
	pUtf16[iUnits] = 0u;
	pConverted = xrtUTF16to8(pUtf16, (size_t)iUnits, &iConverted);
	xrtFree(pUtf16);
	if ( pConverted == NULL ) { return FALSE; }
	*ppUtf8 = (uint8*)pConverted;
	*pUtf8Size = (uint64)iConverted;
	return TRUE;
}

static xrtplistnode* __xrtPlistBinaryObject(xrtplistbinaryreader* pReader,
	uint64 iObject, uint32 iDepth)
{
	uint64 iOffset;
	uint64 iLength;
	uint64 iHeader;
	uint64 iBytes;
	uint64 iRaw;
	uint8 iMarker;
	uint8 iKind;
	uint8 iInfo;
	xrtplistnode* pNode = NULL;
	uint64 i;
	if ( iObject >= pReader->iObjectCount ) {
		__xrtPlistSetError(pReader->Context.Error, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, 0u, "binary plist object reference is out of range");
		return NULL;
	}
	if ( pReader->pStates[iObject] == 2u ) { return pReader->pNodes[iObject]; }
	if ( pReader->pStates[iObject] == 1u ) {
		__xrtPlistSetError(pReader->Context.Error, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, pReader->pOffsets[iObject], "binary plist contains a reference cycle");
		return NULL;
	}
	iOffset = pReader->pOffsets[iObject];
	if ( !__xrtPlistBinaryRange(pReader, iOffset, 1u) ||
		!__xrtPlistLimitNode(&pReader->Context, iDepth, iOffset) ) { return NULL; }
	pReader->pStates[iObject] = 1u;
	iMarker = pReader->pData[iOffset];
	iKind = iMarker >> 4u;
	iInfo = iMarker & 0x0fu;
	if ( iKind == 0x0u ) {
		if ( iInfo == 0x0u ) { pNode = xrtPlistCreateNull(pReader->pDocument); }
		else if ( iInfo == 0x8u || iInfo == 0x9u ) {
			pNode = xrtPlistCreateBool(pReader->pDocument, iInfo == 0x9u);
		}
	} else if ( iKind == 0x1u ) {
		if ( iInfo > 4u ) { goto invalid; }
		iBytes = 1ull << iInfo;
		if ( !__xrtPlistBinaryRange(pReader, iOffset + 1u, iBytes) ) { goto invalid; }
		if ( iBytes <= 8u ) {
			if ( !__xrtPlistBinaryUInt(pReader->pData + iOffset + 1u,
				(uint8)iBytes, &iRaw) ) { goto invalid; }
			if ( iBytes < 8u && (iRaw & (1ull << (iBytes * 8u - 1u))) != 0u ) {
				iRaw |= ~((1ull << (iBytes * 8u)) - 1u);
			}
			pNode = xrtPlistCreateInt(pReader->pDocument, (int64)iRaw);
		} else {
			uint64 iHigh;
			uint64 iLow;
			if ( !__xrtPlistBinaryUInt(pReader->pData + iOffset + 1u, 8u,
				&iHigh) || !__xrtPlistBinaryUInt(pReader->pData + iOffset + 9u,
				8u, &iLow) || !((iHigh == 0u && iLow <= INT64_MAX) ||
				(iHigh == UINT64_MAX && (iLow >> 63u) != 0u)) ) { goto invalid; }
			pNode = xrtPlistCreateInt(pReader->pDocument, (int64)iLow);
		}
	} else if ( iKind == 0x2u ) {
		if ( iInfo == 2u ) {
			uint32 iBits;
			float fValue;
			if ( !__xrtPlistBinaryRange(pReader, iOffset + 1u, 4u) ) { goto invalid; }
			iBits = ((uint32)pReader->pData[iOffset + 1u] << 24u) |
				((uint32)pReader->pData[iOffset + 2u] << 16u) |
				((uint32)pReader->pData[iOffset + 3u] << 8u) |
				pReader->pData[iOffset + 4u];
			memcpy(&fValue, &iBits, sizeof(fValue));
			pNode = xrtPlistCreateReal(pReader->pDocument, (double)fValue);
		} else if ( iInfo == 3u ) {
			double fValue;
			if ( !__xrtPlistBinaryRange(pReader, iOffset + 1u, 8u) ||
				!__xrtPlistBinaryUInt(pReader->pData + iOffset + 1u, 8u,
				&iRaw) ) { goto invalid; }
			memcpy(&fValue, &iRaw, sizeof(fValue));
			pNode = xrtPlistCreateReal(pReader->pDocument, fValue);
		}
	} else if ( iKind == 0x3u && iInfo == 0x3u ) {
		double fSeconds;
		xtime tEpoch = xrtDateTimeSerial(2001, 1, 1, 0, 0, 0);
		if ( !__xrtPlistBinaryRange(pReader, iOffset + 1u, 8u) ||
			!__xrtPlistBinaryUInt(pReader->pData + iOffset + 1u, 8u, &iRaw) ) {
			goto invalid;
		}
		memcpy(&fSeconds, &iRaw, sizeof(fSeconds));
		if ( !isfinite(fSeconds) ||
			fSeconds < (double)INT64_MIN - (double)tEpoch ||
			fSeconds > (double)INT64_MAX - (double)tEpoch ) { goto invalid; }
		pNode = xrtPlistCreateDate(pReader->pDocument,
			tEpoch + (xtime)llround(fSeconds));
	} else if ( iKind == 0x4u || iKind == 0x5u || iKind == 0x6u ) {
		uint8* pUtf8 = NULL;
		uint64 iUtf8Size = 0u;
		if ( !__xrtPlistBinaryLength(pReader, iOffset, iInfo, &iLength,
			&iHeader) || !__xrtPlistLimitItem(&pReader->Context,
			iKind == 0x6u && iLength <= UINT64_MAX / 2u ? iLength * 2u : iLength,
			iOffset) ) { goto invalid; }
		iBytes = iKind == 0x6u ? iLength * 2u : iLength;
		if ( (iKind == 0x6u && iLength > UINT64_MAX / 2u) ||
			!__xrtPlistBinaryRange(pReader, iOffset + iHeader, iBytes) ) {
			goto invalid;
		}
		if ( iKind == 0x4u ) {
			pNode = xrtPlistCreateData(pReader->pDocument,
				pReader->pData + iOffset + iHeader, iLength);
		} else if ( iKind == 0x5u ) {
			for ( i = 0u; i < iLength; i++ ) {
				if ( pReader->pData[iOffset + iHeader + i] > 0x7fu ) { goto invalid; }
			}
			pNode = xrtPlistCreateStringEx(pReader->pDocument,
				pReader->pData + iOffset + iHeader, iLength);
		} else if ( __xrtPlistBinaryUtf16(
			pReader->pData + iOffset + iHeader, iLength, &pUtf8, &iUtf8Size) ) {
			pNode = xrtPlistCreateStringEx(pReader->pDocument, pUtf8, iUtf8Size);
			xrtFree(pUtf8);
		}
	} else if ( iKind == 0x8u ) {
		iBytes = (uint64)iInfo + 1u;
		if ( iBytes > 8u || !__xrtPlistBinaryRange(pReader,
			iOffset + 1u, iBytes) || !__xrtPlistBinaryUInt(
			pReader->pData + iOffset + 1u, (uint8)iBytes, &iRaw) ) { goto invalid; }
		pNode = xrtPlistCreateUid(pReader->pDocument, iRaw);
	} else if ( iKind == 0xau || iKind == 0xdu ) {
		uint64 iReferenceCount;
		if ( !__xrtPlistBinaryLength(pReader, iOffset, iInfo, &iLength,
			&iHeader) || (pReader->Context.Options.tLimits.iMaxEntries != 0u &&
			iLength > pReader->Context.Options.tLimits.iMaxEntries) ) { goto invalid; }
		iReferenceCount = iKind == 0xdu ? iLength * 2u : iLength;
		if ( (iKind == 0xdu && iLength > UINT64_MAX / 2u) ||
			iReferenceCount > UINT64_MAX / pReader->iReferenceSize ||
			!__xrtPlistBinaryRange(pReader, iOffset + iHeader,
				iReferenceCount * pReader->iReferenceSize) ) { goto invalid; }
		pNode = iKind == 0xau ? xrtPlistCreateArray(pReader->pDocument) :
			xrtPlistCreateDict(pReader->pDocument);
		if ( pNode == NULL ) { goto invalid; }
		pReader->pNodes[iObject] = pNode;
		if ( iKind == 0xau ) {
			for ( i = 0u; i < iLength; i++ ) {
				xrtplistnode* pChild;
				if ( !__xrtPlistBinaryUInt(pReader->pData + iOffset + iHeader +
					i * pReader->iReferenceSize, pReader->iReferenceSize, &iRaw) ) {
					goto invalid;
				}
				pChild = __xrtPlistBinaryObject(pReader, iRaw, iDepth + 1u);
				if ( pChild == NULL || !xrtPlistArrayAppend(pNode, pChild) ) { goto invalid; }
			}
		} else {
			for ( i = 0u; i < iLength; i++ ) {
				uint64 iKeyObject;
				uint64 iValueObject;
				xrtplistnode* pKey;
				xrtplistnode* pValue;
				if ( !__xrtPlistBinaryUInt(pReader->pData + iOffset + iHeader +
					i * pReader->iReferenceSize, pReader->iReferenceSize,
					&iKeyObject) || !__xrtPlistBinaryUInt(pReader->pData +
					iOffset + iHeader + (iLength + i) * pReader->iReferenceSize,
					pReader->iReferenceSize, &iValueObject) ) { goto invalid; }
				pKey = __xrtPlistBinaryObject(pReader, iKeyObject, iDepth + 1u);
				pValue = __xrtPlistBinaryObject(pReader, iValueObject, iDepth + 1u);
				if ( pKey == NULL || pKey->iType != XRT_PLIST_STRING ||
					pValue == NULL || xrtPlistDictGet(pNode,
					pKey->Value.Bytes.pData, pKey->Value.Bytes.iSize) != NULL ||
					!xrtPlistDictSet(pNode, pKey->Value.Bytes.pData,
					pKey->Value.Bytes.iSize, pValue) ) { goto invalid; }
			}
		}
	}
	if ( pNode == NULL ) { goto invalid; }
	pReader->pNodes[iObject] = pNode;
	pReader->pStates[iObject] = 2u;
	return pNode;

invalid:
	if ( pReader->Context.Error->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pReader->Context.Error, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, iOffset, "invalid or unsupported binary plist object");
	}
	return NULL;
}

xrtplistdocument* __xrtPlistParseBinary(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplistbinaryreader tReader;
	const uint8* pBytes = (const uint8*)pData;
	const uint8* pTrailer;
	uint64 iTopObject;
	uint64 iTableBytes;
	uint64 i;
	memset(&tReader, 0, sizeof(tReader));
	if ( pData == NULL || iSize < 40u || memcmp(pData, "bplist00", 8u) != 0 ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, 0u, "invalid binary plist header");
		return NULL;
	}
	pTrailer = pBytes + iSize - 32u;
	tReader.iOffsetSize = pTrailer[6];
	tReader.iReferenceSize = pTrailer[7];
	if ( !__xrtPlistBinaryUInt(pTrailer + 8u, 8u, &tReader.iObjectCount) ||
		!__xrtPlistBinaryUInt(pTrailer + 16u, 8u, &iTopObject) ||
		!__xrtPlistBinaryUInt(pTrailer + 24u, 8u, &tReader.iOffsetTable) ||
		tReader.iObjectCount == 0u || tReader.iObjectCount > UINT32_MAX ||
		iTopObject >= tReader.iObjectCount || tReader.iOffsetSize == 0u ||
		tReader.iOffsetSize > 8u || tReader.iReferenceSize == 0u ||
		tReader.iReferenceSize > 8u ||
		tReader.iObjectCount > UINT64_MAX / tReader.iOffsetSize ) {
		goto invalid_trailer;
	}
	iTableBytes = tReader.iObjectCount * tReader.iOffsetSize;
	if ( tReader.iOffsetTable < 8u || tReader.iOffsetTable > iSize - 32u ||
		iTableBytes > iSize - 32u - tReader.iOffsetTable ) { goto invalid_trailer; }
	if ( pOptions->tLimits.iMaxNodes != 0u &&
		tReader.iObjectCount > pOptions->tLimits.iMaxNodes ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "binary plist object table exceeds the configured limit");
		return NULL;
	}
	tReader.pData = pBytes;
	tReader.iSize = iSize;
	tReader.pOffsets = (uint64*)xrtMalloc(sizeof(uint64) *
		(size_t)tReader.iObjectCount);
	tReader.pStates = (uint8*)xrtMalloc((size_t)tReader.iObjectCount);
	tReader.pNodes = (xrtplistnode**)xrtMalloc(sizeof(xrtplistnode*) *
		(size_t)tReader.iObjectCount);
	tReader.pDocument = xrtPlistCreate();
	if ( tReader.pOffsets == NULL || tReader.pStates == NULL ||
		tReader.pNodes == NULL || tReader.pDocument == NULL ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_MEMORY,
			0u, 0u, 0u, "could not allocate binary plist parser state");
		goto cleanup;
	}
	memset(tReader.pStates, 0, (size_t)tReader.iObjectCount);
	memset(tReader.pNodes, 0, sizeof(xrtplistnode*) *
		(size_t)tReader.iObjectCount);
	for ( i = 0u; i < tReader.iObjectCount; i++ ) {
		if ( !__xrtPlistBinaryUInt(pBytes + tReader.iOffsetTable +
			i * tReader.iOffsetSize, tReader.iOffsetSize,
			&tReader.pOffsets[i]) || tReader.pOffsets[i] < 8u ||
			tReader.pOffsets[i] >= tReader.iOffsetTable ) { goto invalid_offsets; }
	}
	tReader.Context.Options = *pOptions;
	tReader.Context.Error = pError;
	if ( !xrtPlistSetRoot(tReader.pDocument,
		__xrtPlistBinaryObject(&tReader, iTopObject, 1u)) ) {
		goto cleanup;
	}
	goto done;

invalid_offsets:
	__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
		0u, 0u, 0u, "invalid binary plist offset table");
	goto cleanup;
invalid_trailer:
	__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
		0u, 0u, 0u, "invalid binary plist trailer");
	return NULL;
cleanup:
	xrtPlistDestroy(tReader.pDocument);
	tReader.pDocument = NULL;
done:
	xrtFree(tReader.pOffsets);
	xrtFree(tReader.pStates);
	xrtFree(tReader.pNodes);
	return tReader.pDocument;
}
