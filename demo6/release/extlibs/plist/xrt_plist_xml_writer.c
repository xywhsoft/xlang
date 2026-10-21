#include "xrt_plist_internal.h"



typedef struct {
	xbuffer* pOutput;
	xrtplistcontext Context;
	xrtplistnode** pStack;
	uint32 iStackCount;
	uint32 iStackCapacity;
} xrtplistxmlwriter;

static bool __xrtPlistXmlAppend(xrtplistxmlwriter* pWriter,
	const void* pData, size_t iSize)
{
	uint64 iLimit = pWriter->Context.Options.tLimits.iMaxOutputBytes;
	if ( iSize == 0u ) { return true; }
	if ( pData == NULL ||
		(iLimit != 0u && ((uint64)iSize > iLimit ||
		 (uint64)pWriter->pOutput->Size > iLimit - (uint64)iSize)) ||
		!xrtBufferAppend(pWriter->pOutput,
			(xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "plist XML output exceeds the configured limit");
		return false;
	}
	return true;
}

static bool __xrtPlistXmlText(xrtplistxmlwriter* pWriter, const char* sText)
{
	return __xrtPlistXmlAppend(pWriter, sText, strlen(sText));
}

static bool __xrtPlistXmlIndent(xrtplistxmlwriter* pWriter, uint32 iDepth)
{
	uint32 i;
	if ( !pWriter->Context.Options.bPretty ) { return true; }
	if ( !__xrtPlistXmlText(pWriter, "\n") ) { return false; }
	for ( i = 0u; i < iDepth * pWriter->Context.Options.iIndent; i++ ) {
		if ( !__xrtPlistXmlText(pWriter, " ") ) { return false; }
	}
	return true;
}

static bool __xrtPlistXmlCodepoint(const uint8* pData, uint64 iSize,
	uint64* pIndex, uint32* pCodepoint)
{
	uint8 c = pData[*pIndex];
	uint32 iValue;
	uint32 iExtra;
	uint32 i;
	if ( c < 0x80u ) { *pCodepoint = c; (*pIndex)++; return true; }
	if ( c >= 0xc2u && c <= 0xdfu ) { iValue = c & 0x1fu; iExtra = 1u; }
	else if ( c >= 0xe0u && c <= 0xefu ) { iValue = c & 0x0fu; iExtra = 2u; }
	else if ( c >= 0xf0u && c <= 0xf4u ) { iValue = c & 0x07u; iExtra = 3u; }
	else { return false; }
	if ((uint64)iExtra >= iSize - *pIndex) { return false; }
	for ( i = 1u; i <= iExtra; i++ ) {
		uint8 d = pData[*pIndex + i];
		if ( (d & 0xc0u) != 0x80u ) { return false; }
		iValue = (iValue << 6u) | (d & 0x3fu);
	}
	if ( (iExtra == 2u && iValue < 0x800u) ||
		(iExtra == 3u && iValue < 0x10000u) || iValue > 0x10ffffu ||
		(iValue >= 0xd800u && iValue <= 0xdfffu) ) { return false; }
	*pCodepoint = iValue;
	*pIndex += iExtra + 1u;
	return true;
}

static bool __xrtPlistXmlEscape(xrtplistxmlwriter* pWriter,
	const uint8* pData, uint64 iSize)
{
	uint64 i = 0u;
	uint64 iStart = 0u;
	while ( i < iSize ) {
		uint64 iCharacter = i;
		uint32 iCodepoint;
		const char* sEscape = NULL;
		if ( !__xrtPlistXmlCodepoint(pData, iSize, &i, &iCodepoint) ||
			!((iCodepoint == 0x9u || iCodepoint == 0xau || iCodepoint == 0xdu) ||
			  (iCodepoint >= 0x20u && iCodepoint <= 0xd7ffu) ||
			  (iCodepoint >= 0xe000u && iCodepoint <= 0xfffdu) ||
			  (iCodepoint >= 0x10000u && iCodepoint <= 0x10ffffu)) ) {
			__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_TYPE,
				0u, 0u, 0u, "plist string contains a character forbidden by XML 1.0");
			return false;
		}
		if ( iCodepoint == '&' ) { sEscape = "&amp;"; }
		else if ( iCodepoint == '<' ) { sEscape = "&lt;"; }
		else if ( iCodepoint == '>' ) { sEscape = "&gt;"; }
		/* 字面 CR 会被 XML 解析器规范化为 LF，字符引用才能原值往返。 */
		else if ( iCodepoint == '\r' ) { sEscape = "&#xD;"; }
		if ( sEscape != NULL ) {
			if ( !__xrtPlistXmlAppend(pWriter, pData + iStart,
				(size_t)(iCharacter - iStart)) ||
				!__xrtPlistXmlText(pWriter, sEscape) ) { return false; }
			iStart = i;
		}
	}
	return __xrtPlistXmlAppend(pWriter, pData + iStart,
		(size_t)(iSize - iStart));
}

static bool __xrtPlistXmlPush(xrtplistxmlwriter* pWriter,
	xrtplistnode* pNode)
{
	xrtplistnode** pItems;
	uint32 i;
	uint32 iCapacity;
	for ( i = 0u; i < pWriter->iStackCount; i++ ) {
		if ( pWriter->pStack[i] == pNode ) {
			__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_FORMAT,
				0u, 0u, 0u, "plist value graph contains a cycle");
			return false;
		}
	}
	if ( pWriter->iStackCount == pWriter->iStackCapacity ) {
		iCapacity = pWriter->iStackCapacity != 0u ?
			pWriter->iStackCapacity * 2u : 16u;
		pItems = (xrtplistnode**)xrtRealloc(pWriter->pStack,
			sizeof(xrtplistnode*) * iCapacity);
		if ( pItems == NULL ) { return false; }
		pWriter->pStack = pItems;
		pWriter->iStackCapacity = iCapacity;
	}
	pWriter->pStack[pWriter->iStackCount++] = pNode;
	return true;
}

static int __xrtPlistKeyCompare(const xrtplistdictitem* pLeft,
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

static uint32* __xrtPlistXmlDictOrder(const xrtplistnode* pNode,
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
			while ( j != 0u && __xrtPlistKeyCompare(
				&pNode->Value.Dict.pItems[pOrder[j - 1u]],
				&pNode->Value.Dict.pItems[iValue]) > 0 ) {
				pOrder[j] = pOrder[j - 1u]; j--;
			}
			pOrder[j] = iValue;
		}
	}
	return pOrder;
}

static bool __xrtPlistXmlWriteValue(xrtplistxmlwriter* pWriter,
	xrtplistnode* pNode, uint32 iDepth)
{
	char sNumber[128];
	uint32 i;
	bool bResult = false;
	if ( pNode == NULL || !__xrtPlistLimitNode(&pWriter->Context,
		iDepth, 0u) || !__xrtPlistXmlPush(pWriter, pNode) ) { return false; }
	switch ( pNode->iType ) {
	case XRT_PLIST_BOOL:
		bResult = __xrtPlistXmlText(pWriter,
			pNode->Value.bValue ? "<true/>" : "<false/>");
		break;
	case XRT_PLIST_INT:
		snprintf(sNumber, sizeof(sNumber), "%lld",
			(long long)pNode->Value.iValue);
		bResult = __xrtPlistXmlText(pWriter, "<integer>") &&
			__xrtPlistXmlText(pWriter, sNumber) &&
			__xrtPlistXmlText(pWriter, "</integer>");
		break;
	case XRT_PLIST_REAL: {
		size_t iNumberSize;
		if ( !isfinite(pNode->Value.fValue) ||
			!xrtNumWrite(pNode->Value.fValue, sNumber, sizeof(sNumber),
				&iNumberSize, XNUMBER_FLOAT_COMPACT) ) {
			break;
		}
		bResult = __xrtPlistXmlText(pWriter, "<real>") &&
			__xrtPlistXmlText(pWriter, sNumber) &&
			__xrtPlistXmlText(pWriter, "</real>");
		break;
	}
	case XRT_PLIST_STRING:
		bResult = __xrtPlistLimitItem(&pWriter->Context,
			pNode->Value.Bytes.iSize, 0u) &&
			__xrtPlistXmlText(pWriter, "<string>") &&
			__xrtPlistXmlEscape(pWriter, pNode->Value.Bytes.pData,
				pNode->Value.Bytes.iSize) &&
			__xrtPlistXmlText(pWriter, "</string>");
		break;
	case XRT_PLIST_DATA: {
		str sEncoded = NULL;
		if ( !__xrtPlistLimitItem(&pWriter->Context,
			pNode->Value.Bytes.iSize, 0u) ) { break; }
		if ( pNode->Value.Bytes.iSize != 0u ) {
			sEncoded = xrtBase64EncodeNew(pNode->Value.Bytes.pData,
				(size_t)pNode->Value.Bytes.iSize, NULL);
			if ( sEncoded == NULL ) { break; }
		}
		bResult = __xrtPlistXmlText(pWriter, "<data>") &&
			(sEncoded == NULL || __xrtPlistXmlText(pWriter,
				(const char*)sEncoded)) &&
			__xrtPlistXmlText(pWriter, "</data>");
		xrtFree(sEncoded);
		break;
	}
	case XRT_PLIST_DATE: {
		xdatetime tDateTime;
		if ( pNode->Value.tValue % XRT_TIME_SECOND != 0 ) {
			__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_TYPE,
				0u, 0u, 0u, "plist XML dates require whole seconds");
			break;
		}
		if ( !xrtTimeSplit(pNode->Value.tValue, &tDateTime) ||
			tDateTime.Year < 0 || tDateTime.Year > 9999 ) { break; }
		snprintf(sNumber, sizeof(sNumber),
			"%04lld-%02d-%02dT%02d:%02d:%02dZ", (long long)tDateTime.Year,
			tDateTime.Month, tDateTime.Day, tDateTime.Hour,
			tDateTime.Minute, tDateTime.Second);
		bResult = __xrtPlistXmlText(pWriter, "<date>") &&
			__xrtPlistXmlText(pWriter, sNumber) &&
			__xrtPlistXmlText(pWriter, "</date>");
		break;
	}
	case XRT_PLIST_ARRAY:
		if ( pWriter->Context.Options.tLimits.iMaxEntries != 0u &&
			pNode->Value.Array.iCount >
			pWriter->Context.Options.tLimits.iMaxEntries ) { break; }
		bResult = __xrtPlistXmlText(pWriter, "<array>");
		for ( i = 0u; bResult && i < pNode->Value.Array.iCount; i++ ) {
			bResult = __xrtPlistXmlIndent(pWriter, iDepth + 1u) &&
				__xrtPlistXmlWriteValue(pWriter, pNode->Value.Array.pItems[i],
					iDepth + 1u);
		}
		if ( bResult && pNode->Value.Array.iCount != 0u ) {
			bResult = __xrtPlistXmlIndent(pWriter, iDepth);
		}
		bResult = bResult && __xrtPlistXmlText(pWriter, "</array>");
		break;
	case XRT_PLIST_DICT: {
		uint32* pOrder;
		if ( pWriter->Context.Options.tLimits.iMaxEntries != 0u &&
			pNode->Value.Dict.iCount >
			pWriter->Context.Options.tLimits.iMaxEntries ) { break; }
		pOrder = __xrtPlistXmlDictOrder(pNode,
			pWriter->Context.Options.bSortKeys);
		if ( pNode->Value.Dict.iCount != 0u && pOrder == NULL ) { break; }
		bResult = __xrtPlistXmlText(pWriter, "<dict>");
		for ( i = 0u; bResult && i < pNode->Value.Dict.iCount; i++ ) {
			xrtplistdictitem* pItem = &pNode->Value.Dict.pItems[pOrder[i]];
			bResult = __xrtPlistLimitItem(&pWriter->Context,
				pItem->iKeySize, 0u) &&
				__xrtPlistXmlIndent(pWriter, iDepth + 1u) &&
				__xrtPlistXmlText(pWriter, "<key>") &&
				__xrtPlistXmlEscape(pWriter, pItem->pKey, pItem->iKeySize) &&
				__xrtPlistXmlText(pWriter, "</key>") &&
				__xrtPlistXmlIndent(pWriter, iDepth + 1u) &&
				__xrtPlistXmlWriteValue(pWriter, pItem->pValue, iDepth + 1u);
		}
		if ( bResult && pNode->Value.Dict.iCount != 0u ) {
			bResult = __xrtPlistXmlIndent(pWriter, iDepth);
		}
		bResult = bResult && __xrtPlistXmlText(pWriter, "</dict>");
		xrtFree(pOrder);
		break;
	}
	case XRT_PLIST_NULL:
	case XRT_PLIST_UID:
		__xrtPlistSetError(pWriter->Context.Error, XRT_PLIST_ERROR_TYPE,
			0u, 0u, 0u, "null and UID values require binary plist output");
		break;
	default: break;
	}
	pWriter->iStackCount--;
	if ( !bResult && pWriter->Context.Error->iCode == XRT_PLIST_OK ) {
		const xerror* pCause = xrtGetError();
		bool bMemory = pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY;
		__xrtPlistSetError(pWriter->Context.Error,
			bMemory ? XRT_PLIST_ERROR_MEMORY : XRT_PLIST_ERROR_TYPE,
			0u, 0u, 0u, bMemory ? "could not allocate plist XML output" :
			"plist value cannot be encoded as XML");
	}
	return bResult;
}

xbuffer* __xrtPlistWriteXml(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplistxmlwriter tWriter;
	const char* sHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">";
	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.Context.Options = *pOptions;
	tWriter.Context.Error = pError;
	tWriter.pOutput = xrtBufferCreate();
	if ( tWriter.pOutput == NULL || !xrtBufferReserve(tWriter.pOutput, 1024u) ||
		pOptions->iIndent > 16u ||
		!__xrtPlistXmlText(&tWriter, sHeader) ||
		!__xrtPlistXmlIndent(&tWriter, 1u) ||
		!__xrtPlistXmlWriteValue(&tWriter, pDocument->pRoot, 1u) ||
		!__xrtPlistXmlIndent(&tWriter, 0u) ||
		!__xrtPlistXmlText(&tWriter, "</plist>\n") ) {
		if ( pError->iCode == XRT_PLIST_OK ) {
			__xrtPlistSetError(pError, XRT_PLIST_ERROR_MEMORY,
				0u, 0u, 0u, "could not allocate plist XML output");
		}
		xrtBufferDestroy(tWriter.pOutput);
		tWriter.pOutput = NULL;
	}
	xrtFree(tWriter.pStack);
	return tWriter.pOutput;
}
