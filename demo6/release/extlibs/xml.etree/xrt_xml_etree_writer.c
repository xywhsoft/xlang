#include "xrt_xml_etree_internal.h"



typedef struct {
	xbuffer* pBuffer;
	uint64 iLimit;
	uint32 iIndent;
	bool bPretty;
	int iErrorCode;
} xrtxmltreewriter;

XRT_API void xrtXmlElementWriteOptionsInit(xrtxmletreewriteoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_XML_ETREE_WRITE_OPTIONS_VERSION;
	pOptions->iIndent = 2u;
	pOptions->iMaxOutputBytes = 512u * 1024u * 1024u;
}

static bool __xrtXmlEtreeWriterAppend(xrtxmltreewriter* pWriter,
	const char* sText, size_t iSize)
{
	if ( iSize == 0u ) { return true; }
	if ( pWriter == NULL || (uint64)iSize > pWriter->iLimit ||
		(uint64)pWriter->pBuffer->Size > pWriter->iLimit - (uint64)iSize ) {
		return false;
	}
	if ( !xrtBufferAppend(pWriter->pBuffer,
		(xbytesview){ (cbytes)sText, iSize }) ) {
		pWriter->iErrorCode = XRT_XML_ERROR_MEMORY;
		return false;
	}
	return true;
}

/* ElementTree 可被 C 调用方直接修改，序列化时再次确认 XML 字符。 */
static bool __xrtXmlEtreeWriterValidText(xrtxmltreewriter* pWriter,
	const char* sText)
{
	size_t iPosition = 0u;
	size_t iSize = strlen(sText != NULL ? sText : "");
	while ( iPosition < iSize ) {
		uint32 iScalar;
		size_t iRead = 0u;
		if ( xrtUtf8Decode((xstrview){ sText + iPosition,
			iSize - iPosition }, &iScalar, &iRead) != XUTF_OK || iRead == 0u ||
			(iScalar < 0x20u && iScalar != '\t' &&
			 iScalar != '\n' && iScalar != '\r') ||
			iScalar == 0xfffeu || iScalar == 0xffffu ) {
			pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
			return false;
		}
		iPosition += iRead;
	}
	return true;
}

static bool __xrtXmlEtreeWriterText(xrtxmltreewriter* pWriter,
	const char* sText, bool bAttribute)
{
	const unsigned char* p = (const unsigned char*)(sText != NULL ? sText : "");
	const unsigned char* pStart = p;
	if ( !__xrtXmlEtreeWriterValidText(pWriter, (const char*)p) ) { return false; }
	while ( *p != 0u ) {
		const char* sEscape = NULL;
		if ( *p == '&' ) { sEscape = "&amp;"; }
		else if ( *p == '<' ) { sEscape = "&lt;"; }
		else if ( *p == '>' ) { sEscape = "&gt;"; }
		else if ( bAttribute && *p == '"' ) { sEscape = "&quot;"; }
		else if ( bAttribute && *p == '\'' ) { sEscape = "&apos;"; }
		else if ( bAttribute && *p == '\t' ) { sEscape = "&#x9;"; }
		else if ( bAttribute && *p == '\n' ) { sEscape = "&#xA;"; }
		else if ( *p == '\r' ) { sEscape = "&#xD;"; }
		if ( sEscape != NULL ) {
			if ( !__xrtXmlEtreeWriterAppend(pWriter, (const char*)pStart,
				(size_t)(p - pStart)) ||
				!__xrtXmlEtreeWriterAppend(pWriter, sEscape, strlen(sEscape)) ) {
				return false;
			}
			pStart = p + 1;
		}
		p++;
	}
	return __xrtXmlEtreeWriterAppend(pWriter, (const char*)pStart,
		(size_t)(p - pStart));
}

static bool __xrtXmlEtreeIndent(xrtxmltreewriter* pWriter, uint32 iDepth)
{
	static const char sSpaces[] = "                                ";
	uint64 iSpaces = (uint64)iDepth * pWriter->iIndent;
	if ( !pWriter->bPretty ) { return true; }
	if ( iSpaces >= pWriter->iLimit ||
		(uint64)pWriter->pBuffer->Size >= pWriter->iLimit - iSpaces ) {
		return false;
	}
	if ( !__xrtXmlEtreeWriterAppend(pWriter, "\n", 1u) ) { return false; }
	while ( iSpaces != 0u ) {
		size_t iChunk = iSpaces < sizeof(sSpaces) - 1u ?
			(size_t)iSpaces : sizeof(sSpaces) - 1u;
		if ( !__xrtXmlEtreeWriterAppend(pWriter, sSpaces, iChunk) ) {
			return false;
		}
		iSpaces -= iChunk;
	}
	return true;
}

static bool __xrtXmlEtreeHasMixedContent(const xrtxmlelement* pElement)
{
	uint32 i;
	if ( pElement == NULL ) { return false; }
	if ( pElement->sText != NULL && pElement->sText[0] != 0 ) { return true; }
	for ( i = 0u; i < pElement->iChildCount; i++ ) {
		if ( pElement->pChildren[i]->sTail != NULL &&
			pElement->pChildren[i]->sTail[0] != 0 ) { return true; }
	}
	return false;
}

typedef struct {
	const xrtxmlelement* pElement;
	uint32 iNextChild;
	uint32 iDepth;
	bool bParentPretty;
	bool bPrettyChildren;
	bool bOpened;
} xrtxmletreewriteframe;

static bool __xrtXmlEtreeWriteNode(xrtxmltreewriter* pWriter,
	const xrtxmlelement* pElement)
{
	xrtxmletreewriteframe* pStack = (xrtxmletreewriteframe*)xrtMalloc(
		32u * sizeof(*pStack));
	size_t iCapacity = 32u;
	size_t iUsed = 1u;
	bool bOk = pStack != NULL;
	if ( !bOk ) { pWriter->iErrorCode = XRT_XML_ERROR_MEMORY; return false; }
	pStack[0] = (xrtxmletreewriteframe){ pElement, 0u, 0u,
		false, false, false };
	while ( iUsed != 0u ) {
		xrtxmletreewriteframe* pFrame = &pStack[iUsed - 1u];
		const xrtxmlelement* pCurrent = pFrame->pElement;
		uint32 i;
		if ( !pFrame->bOpened ) {
			if ( pCurrent->iKind == XRT_XML_COMMENT ) {
				size_t iSize = strlen((const char*)pCurrent->sText);
				if ( strstr((const char*)pCurrent->sText, "--") != NULL ||
					(iSize != 0u && pCurrent->sText[iSize - 1u] == '-') ||
					!__xrtXmlEtreeWriterValidText(pWriter,
						(const char*)pCurrent->sText) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
					bOk = false;
					break;
				}
				bOk = (!pFrame->bParentPretty ||
					__xrtXmlEtreeIndent(pWriter, pFrame->iDepth)) &&
					__xrtXmlEtreeWriterAppend(pWriter, "<!--", 4u) &&
					__xrtXmlEtreeWriterAppend(pWriter,
						(const char*)pCurrent->sText, iSize) &&
					__xrtXmlEtreeWriterAppend(pWriter, "-->", 3u);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( pCurrent->iKind == XRT_XML_PROCESSING_INSTRUCTION ) {
				str sTarget = xrtXmlElementGetAttribute(pCurrent, (str)"target");
				if ( !xrtXmlNameValid(sTarget) ||
					xrtStrCaseEqual((xstrview){ (const char*)sTarget,
						strlen((const char*)sTarget) },
						XRT_STR_LITERAL("xml")) ||
					strstr((const char*)pCurrent->sText, "?>") != NULL ||
					!__xrtXmlEtreeWriterValidText(pWriter,
						(const char*)pCurrent->sText) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
					bOk = false;
					break;
				}
				bOk = (!pFrame->bParentPretty ||
					__xrtXmlEtreeIndent(pWriter, pFrame->iDepth)) &&
					__xrtXmlEtreeWriterAppend(pWriter, "<?", 2u) &&
					__xrtXmlEtreeWriterAppend(pWriter,
						(const char*)sTarget, strlen((const char*)sTarget)) &&
					(pCurrent->sText[0] == 0 ||
					 (__xrtXmlEtreeWriterAppend(pWriter, " ", 1u) &&
					  __xrtXmlEtreeWriterAppend(pWriter,
						(const char*)pCurrent->sText,
						strlen((const char*)pCurrent->sText)))) &&
					__xrtXmlEtreeWriterAppend(pWriter, "?>", 2u);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( pCurrent->iKind != XRT_XML_ELEMENT ) {
				pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
				bOk = false;
				break;
			}
			pFrame->bPrettyChildren = pWriter->bPretty &&
				!__xrtXmlEtreeHasMixedContent(pCurrent);
			if ( pFrame->bParentPretty &&
				!__xrtXmlEtreeIndent(pWriter, pFrame->iDepth) ) {
				bOk = false;
				break;
			}
			bOk = __xrtXmlEtreeWriterAppend(pWriter, "<", 1u) &&
				__xrtXmlEtreeWriterAppend(pWriter,
					(const char*)pCurrent->sName,
					strlen((const char*)pCurrent->sName));
			for ( i = 0u; bOk && i < pCurrent->iAttributeCount; i++ ) {
				const xrtxmlattribute* pItem = &pCurrent->pAttributes[i];
				bOk = __xrtXmlEtreeWriterAppend(pWriter, " ", 1u) &&
					__xrtXmlEtreeWriterAppend(pWriter,
						(const char*)pItem->sName,
						strlen((const char*)pItem->sName)) &&
					__xrtXmlEtreeWriterAppend(pWriter, "=\"", 2u) &&
					__xrtXmlEtreeWriterText(pWriter,
						(const char*)pItem->sValue, true) &&
					__xrtXmlEtreeWriterAppend(pWriter, "\"", 1u);
			}
			if ( bOk && pCurrent->iChildCount == 0u &&
				pCurrent->sText[0] == 0 ) {
				bOk = __xrtXmlEtreeWriterAppend(pWriter, "/>", 2u);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( bOk ) {
				bOk = __xrtXmlEtreeWriterAppend(pWriter, ">", 1u) &&
					__xrtXmlEtreeWriterText(pWriter,
						(const char*)pCurrent->sText, false);
			}
			if ( !bOk ) { break; }
			pFrame->bOpened = true;
			continue;
		}
		if ( pFrame->iNextChild != 0u ) {
			const xrtxmlelement* pPrevious =
				pCurrent->pChildren[pFrame->iNextChild - 1u];
			if ( !__xrtXmlEtreeWriterText(pWriter,
				(const char*)pPrevious->sTail, false) ) {
				bOk = false;
				break;
			}
		}
		if ( pFrame->iNextChild < pCurrent->iChildCount ) {
			uint32 iChild = pFrame->iNextChild++;
			uint32 iChildDepth = pFrame->iDepth + 1u;
			const xrtxmlelement* pChild = pCurrent->pChildren[iChild];
			bool bChildPretty = pFrame->bPrettyChildren;
			if ( iChildDepth < pFrame->iDepth ) {
				bOk = false;
				break;
			}
			if ( iUsed == iCapacity ) {
				xrtxmletreewriteframe* pNew;
				if ( iCapacity > SIZE_MAX / (2u * sizeof(*pStack)) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_MEMORY;
					bOk = false;
					break;
				}
				pNew = (xrtxmletreewriteframe*)xrtRealloc(pStack,
					2u * iCapacity * sizeof(*pStack));
				if ( pNew == NULL ) {
					pWriter->iErrorCode = XRT_XML_ERROR_MEMORY;
					bOk = false;
					break;
				}
				pStack = pNew;
				iCapacity *= 2u;
			}
			pStack[iUsed++] = (xrtxmletreewriteframe){ pChild, 0u,
				iChildDepth, bChildPretty, false, false };
			continue;
		}
		bOk = (!pFrame->bPrettyChildren || pCurrent->iChildCount == 0u ||
			__xrtXmlEtreeIndent(pWriter, pFrame->iDepth)) &&
			__xrtXmlEtreeWriterAppend(pWriter, "</", 2u) &&
			__xrtXmlEtreeWriterAppend(pWriter,
				(const char*)pCurrent->sName,
				strlen((const char*)pCurrent->sName)) &&
			__xrtXmlEtreeWriterAppend(pWriter, ">", 1u);
		if ( !bOk ) { break; }
		iUsed--;
	}
	xrtFree(pStack);
	return bOk;
}

static bool __xrtXmlEtreeWriteOptions(const xrtxmletreewriteoptions* pOptions,
	xrtxmletreewriteoptions* pResult)
{
	xrtXmlElementWriteOptionsInit(pResult);
	if ( pOptions == NULL ) { return true; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_XML_ETREE_WRITE_OPTIONS_VERSION ||
		pOptions->iIndent > 16u || pOptions->iMaxOutputBytes == 0u ) {
		return false;
	}
	*pResult = *pOptions;
	return true;
}

XRT_API xbuffer* xrtXmlElementStringify(const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError)
{
	xrtxmletreewriteoptions tOptions;
	xrtxmltreewriter tWriter;
	if ( pElement == NULL || !__xrtXmlEtreeWriteOptions(pOptions, &tOptions) ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_ARGUMENT,
			"invalid XML ElementTree writer options");
		return NULL;
	}
	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.pBuffer = xrtBufferCreate();
	if ( tWriter.pBuffer == NULL ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_MEMORY,
			"could not allocate XML ElementTree writer buffer");
		return NULL;
	}
	tWriter.iLimit = tOptions.iMaxOutputBytes;
	tWriter.iIndent = tOptions.iIndent;
	tWriter.bPretty = tOptions.bPretty;
	tWriter.iErrorCode = XRT_XML_ERROR_LIMIT;
	if ( (tOptions.bXmlDeclaration && !__xrtXmlEtreeWriterAppend(&tWriter,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 38u)) ||
		(tOptions.bXmlDeclaration && tOptions.bPretty &&
		 !__xrtXmlEtreeWriterAppend(&tWriter, "\n", 1u)) ||
		!__xrtXmlEtreeWriteNode(&tWriter, pElement) ||
		(tOptions.bPretty && !__xrtXmlEtreeWriterAppend(&tWriter, "\n", 1u)) ) {
		xrtBufferDestroy(tWriter.pBuffer);
		__xrtXmlEtreeSetError(pError, tWriter.iErrorCode,
			tWriter.iErrorCode == XRT_XML_ERROR_FORMAT ?
			"XML contains invalid XML content" :
			tWriter.iErrorCode == XRT_XML_ERROR_MEMORY ?
			"could not allocate XML ElementTree writer state" :
			"could not encode XML within the configured output limit");
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return tWriter.pBuffer;
}

/* 保留底层分配失败；文件和流的其他失败仍归为 IO。 */
static int __xrtXmlEtreeOutputErrorCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_XML_ERROR_MEMORY : XRT_XML_ERROR_IO;
}

XRT_API bool xrtXmlElementWrite(const xrtxmlelement* pElement, xwriter* pWriter,
	bool bTakeOwnership, const xrtxmletreewriteoptions* pOptions,
	xrtxmlerror* pError)
{
	xbuffer* pBuffer;
	bool bOk = false;
	int iOutputError = XRT_XML_ERROR_IO;
	if ( pWriter == NULL ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_ARGUMENT,
			"XML ElementTree output writer is null");
		return false;
	}
	pBuffer = xrtXmlElementStringify(pElement, pOptions, pError);
	if ( pBuffer != NULL && pWriter != NULL ) {
		xrtClearError();
		bOk = xrtWriterWriteFull(pWriter, pBuffer->Data, pBuffer->Size, NULL) &&
			xrtWriterFlush(pWriter);
		if ( !bOk ) { iOutputError = __xrtXmlEtreeOutputErrorCode(); }
	}
	if ( bTakeOwnership && pWriter != NULL ) {
		bool bClosed = xrtWriterDestroy(pWriter);
		if ( bOk && !bClosed ) {
			iOutputError = __xrtXmlEtreeOutputErrorCode();
			bOk = false;
		}
	}
	xrtBufferDestroy(pBuffer);
	if ( !bOk && pError != NULL && pError->iCode == 0 ) {
		__xrtXmlEtreeSetError(pError, iOutputError,
			"could not write XML output");
	}
	return bOk;
}

XRT_API bool xrtXmlElementSave(str sPath, const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError)
{
	xbuffer* pBuffer;
	bool bOk;
	if ( sPath == NULL || sPath[0] == 0 ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_ARGUMENT,
			"XML ElementTree output path is empty");
		return false;
	}
	pBuffer = xrtXmlElementStringify(pElement, pOptions, pError);
	if ( pBuffer == NULL ) { return false; }
	xrtClearError();
	bOk = xrtFileWriteAtomic((cstr)sPath,
		(xbytesview){ pBuffer->Data, pBuffer->Size });
	if ( !bOk ) {
		__xrtXmlEtreeSetError(pError, __xrtXmlEtreeOutputErrorCode(),
			"could not save XML output file");
	}
	xrtBufferDestroy(pBuffer);
	return bOk;
}
