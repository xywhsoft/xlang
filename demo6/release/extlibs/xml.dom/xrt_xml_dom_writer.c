#include "xrt_xml_dom_internal.h"



typedef struct {
	xbuffer* pBuffer;
	uint64 iLimit;
	uint32 iIndent;
	bool bPretty;
	int iErrorCode;
} xrtxmldomwriter;

XRT_API void xrtXmlDomWriteOptionsInit(xrtxmldomwriteoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_XML_DOM_WRITE_OPTIONS_VERSION;
	pOptions->iIndent = 2u;
	pOptions->iMaxOutputBytes = 512u * 1024u * 1024u;
}

static bool __xrtXmlDomWriterAppend(xrtxmldomwriter* pWriter,
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

/* 写出器是最后一道边界：树节点可由 C 调用方直接构造或修改。 */
static bool __xrtXmlDomWriterValidText(xrtxmldomwriter* pWriter,
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

static bool __xrtXmlDomWriterEscape(xrtxmldomwriter* pWriter,
	const char* sText, bool bAttribute)
{
	const unsigned char* p = (const unsigned char*)(sText != NULL ? sText : "");
	const unsigned char* pStart = p;
	if ( !__xrtXmlDomWriterValidText(pWriter, (const char*)p) ) { return false; }
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
			if ( !__xrtXmlDomWriterAppend(pWriter, (const char*)pStart,
				(size_t)(p - pStart)) ||
				!__xrtXmlDomWriterAppend(pWriter, sEscape, strlen(sEscape)) ) {
				return false;
			}
			pStart = p + 1;
		}
		p++;
	}
	return __xrtXmlDomWriterAppend(pWriter, (const char*)pStart,
		(size_t)(p - pStart));
}

static bool __xrtXmlDomWriterIndent(xrtxmldomwriter* pWriter, uint32 iDepth)
{
	static const char sSpaces[] = "                                ";
	uint64 iSpaces = (uint64)iDepth * pWriter->iIndent;
	if ( !pWriter->bPretty ) { return true; }
	if ( iSpaces >= pWriter->iLimit ||
		(uint64)pWriter->pBuffer->Size >= pWriter->iLimit - iSpaces ) {
		return false;
	}
	if ( !__xrtXmlDomWriterAppend(pWriter, "\n", 1u) ) { return false; }
	while ( iSpaces != 0u ) {
		size_t iChunk = iSpaces < sizeof(sSpaces) - 1u ?
			(size_t)iSpaces : sizeof(sSpaces) - 1u;
		if ( !__xrtXmlDomWriterAppend(pWriter, sSpaces, iChunk) ) {
			return false;
		}
		iSpaces -= iChunk;
	}
	return true;
}

static bool __xrtXmlDomHasTextChildren(const xrtxmldomnode* pNode)
{
	uint32 i;
	for ( i = 0u; pNode != NULL && i < pNode->iChildCount; i++ ) {
		if ( pNode->pChildren[i]->iType == XRT_XML_DOM_TEXT_NODE &&
			pNode->pChildren[i]->sValue[0] != 0 ) { return true; }
	}
	return false;
}

typedef struct {
	const xrtxmldomnode* pNode;
	uint32 iNextChild;
	uint32 iDepth;
	bool bParentPretty;
	bool bPrettyChildren;
	bool bOpened;
} xrtxmldomwriteframe;

static bool __xrtXmlDomWriteNode(xrtxmldomwriter* pWriter,
	const xrtxmldomnode* pNode)
{
	xrtxmldomwriteframe* pStack = (xrtxmldomwriteframe*)xrtMalloc(
		32u * sizeof(*pStack));
	size_t iCapacity = 32u;
	size_t iUsed = 1u;
	bool bOk = pStack != NULL;
	if ( !bOk ) { pWriter->iErrorCode = XRT_XML_ERROR_MEMORY; return false; }
	pStack[0] = (xrtxmldomwriteframe){ pNode, 0u, 0u, false, false, false };
	while ( iUsed != 0u ) {
		xrtxmldomwriteframe* pFrame = &pStack[iUsed - 1u];
		const xrtxmldomnode* pCurrent = pFrame->pNode;
		uint32 i;
		if ( !pFrame->bOpened ) {
			if ( pCurrent->iType == XRT_XML_DOM_TEXT_NODE ) {
				bOk = __xrtXmlDomWriterEscape(pWriter,
					(const char*)pCurrent->sValue, false);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( pFrame->bParentPretty &&
				!__xrtXmlDomWriterIndent(pWriter, pFrame->iDepth) ) {
				bOk = false;
				break;
			}
			if ( pCurrent->iType == XRT_XML_DOM_COMMENT_NODE ) {
				size_t iSize = strlen((const char*)pCurrent->sValue);
				if ( strstr((const char*)pCurrent->sValue, "--") != NULL ||
					(iSize != 0u && pCurrent->sValue[iSize - 1u] == '-') ||
					!__xrtXmlDomWriterValidText(pWriter,
						(const char*)pCurrent->sValue) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
					bOk = false;
					break;
				}
				bOk = __xrtXmlDomWriterAppend(pWriter, "<!--", 4u) &&
					__xrtXmlDomWriterAppend(pWriter,
						(const char*)pCurrent->sValue, iSize) &&
					__xrtXmlDomWriterAppend(pWriter, "-->", 3u);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( pCurrent->iType == XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE ) {
				if ( !xrtXmlNameValid(pCurrent->sName) ||
					xrtStrCaseEqual((xstrview){ (const char*)pCurrent->sName,
						strlen((const char*)pCurrent->sName) },
						XRT_STR_LITERAL("xml")) ||
					strstr((const char*)pCurrent->sValue, "?>") != NULL ||
					!__xrtXmlDomWriterValidText(pWriter,
						(const char*)pCurrent->sValue) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
					bOk = false;
					break;
				}
				bOk = __xrtXmlDomWriterAppend(pWriter, "<?", 2u) &&
					__xrtXmlDomWriterAppend(pWriter,
						(const char*)pCurrent->sName,
						strlen((const char*)pCurrent->sName)) &&
					(pCurrent->sValue[0] == 0 ||
					 (__xrtXmlDomWriterAppend(pWriter, " ", 1u) &&
					  __xrtXmlDomWriterAppend(pWriter,
						(const char*)pCurrent->sValue,
						strlen((const char*)pCurrent->sValue)))) &&
					__xrtXmlDomWriterAppend(pWriter, "?>", 2u);
				iUsed--;
				if ( !bOk ) { break; }
				continue;
			}
			if ( pCurrent->iType != XRT_XML_DOM_DOCUMENT_NODE &&
				pCurrent->iType != XRT_XML_DOM_ELEMENT_NODE ) {
				pWriter->iErrorCode = XRT_XML_ERROR_FORMAT;
				bOk = false;
				break;
			}
			pFrame->bOpened = true;
			if ( pCurrent->iType == XRT_XML_DOM_DOCUMENT_NODE ) { continue; }
			pFrame->bPrettyChildren = pWriter->bPretty &&
				!__xrtXmlDomHasTextChildren(pCurrent);
			bOk = __xrtXmlDomWriterAppend(pWriter, "<", 1u) &&
				__xrtXmlDomWriterAppend(pWriter,
					(const char*)pCurrent->sName,
					strlen((const char*)pCurrent->sName));
			for ( i = 0u; bOk && i < pCurrent->iAttributeCount; i++ ) {
				const xrtxmlattribute* pAttribute = &pCurrent->pAttributes[i];
				bOk = __xrtXmlDomWriterAppend(pWriter, " ", 1u) &&
					__xrtXmlDomWriterAppend(pWriter,
						(const char*)pAttribute->sName,
						strlen((const char*)pAttribute->sName)) &&
					__xrtXmlDomWriterAppend(pWriter, "=\"", 2u) &&
					__xrtXmlDomWriterEscape(pWriter,
						(const char*)pAttribute->sValue, true) &&
					__xrtXmlDomWriterAppend(pWriter, "\"", 1u);
			}
			if ( bOk ) {
				bOk = __xrtXmlDomWriterAppend(pWriter,
					pCurrent->iChildCount == 0u ? "/>" : ">",
					pCurrent->iChildCount == 0u ? 2u : 1u);
			}
			if ( !bOk ) { break; }
			if ( pCurrent->iChildCount == 0u ) { iUsed--; }
			continue;
		}
		if ( pFrame->iNextChild < pCurrent->iChildCount ) {
			uint32 iChild = pFrame->iNextChild++;
			uint32 iChildDepth = pCurrent->iType == XRT_XML_DOM_DOCUMENT_NODE ?
				0u : pFrame->iDepth + 1u;
			bool bChildPretty = pCurrent->iType == XRT_XML_DOM_DOCUMENT_NODE ?
				pWriter->bPretty && iChild != 0u : pFrame->bPrettyChildren;
			const xrtxmldomnode* pChild = pCurrent->pChildren[iChild];
			if ( iChildDepth < pFrame->iDepth ) {
				bOk = false;
				break;
			}
			if ( iUsed == iCapacity ) {
				xrtxmldomwriteframe* pNew;
				if ( iCapacity > SIZE_MAX / (2u * sizeof(*pStack)) ) {
					pWriter->iErrorCode = XRT_XML_ERROR_MEMORY;
					bOk = false;
					break;
				}
				pNew = (xrtxmldomwriteframe*)xrtRealloc(pStack,
					2u * iCapacity * sizeof(*pStack));
				if ( pNew == NULL ) {
					pWriter->iErrorCode = XRT_XML_ERROR_MEMORY;
					bOk = false;
					break;
				}
				pStack = pNew;
				iCapacity *= 2u;
			}
			pStack[iUsed++] = (xrtxmldomwriteframe){ pChild, 0u,
				iChildDepth, bChildPretty, false, false };
			continue;
		}
		if ( pCurrent->iType == XRT_XML_DOM_ELEMENT_NODE ) {
			bOk = (!pFrame->bPrettyChildren ||
				__xrtXmlDomWriterIndent(pWriter, pFrame->iDepth)) &&
				__xrtXmlDomWriterAppend(pWriter, "</", 2u) &&
				__xrtXmlDomWriterAppend(pWriter,
					(const char*)pCurrent->sName,
					strlen((const char*)pCurrent->sName)) &&
				__xrtXmlDomWriterAppend(pWriter, ">", 1u);
			if ( !bOk ) { break; }
		}
		iUsed--;
	}
	xrtFree(pStack);
	return bOk;
}

static bool __xrtXmlDomValidateWriteOptions(
	const xrtxmldomwriteoptions* pOptions, xrtxmldomwriteoptions* pResult)
{
	xrtXmlDomWriteOptionsInit(pResult);
	if ( pOptions == NULL ) { return true; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_XML_DOM_WRITE_OPTIONS_VERSION ||
		pOptions->iIndent > 16u || pOptions->iMaxOutputBytes == 0u ) {
		return false;
	}
	*pResult = *pOptions;
	return true;
}

XRT_API xbuffer* xrtXmlDomStringify(const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError)
{
	xrtxmldomwriteoptions tOptions;
	xrtxmldomwriter tWriter;
	if ( pNode == NULL ||
		!__xrtXmlDomValidateWriteOptions(pOptions, &tOptions) ) {
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_ARGUMENT,
			"invalid XML DOM writer options");
		return NULL;
	}
	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.pBuffer = xrtBufferCreate();
	if ( tWriter.pBuffer == NULL ) {
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_MEMORY,
			"could not allocate XML DOM writer buffer");
		return NULL;
	}
	tWriter.iLimit = tOptions.iMaxOutputBytes;
	tWriter.iIndent = tOptions.iIndent;
	tWriter.bPretty = tOptions.bPretty;
	tWriter.iErrorCode = XRT_XML_ERROR_LIMIT;
	if ( (tOptions.bXmlDeclaration && !__xrtXmlDomWriterAppend(&tWriter,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 38u)) ||
		(tOptions.bXmlDeclaration && tOptions.bPretty &&
		 !__xrtXmlDomWriterAppend(&tWriter, "\n", 1u)) ||
		!__xrtXmlDomWriteNode(&tWriter, pNode) ||
		(tOptions.bPretty && !__xrtXmlDomWriterAppend(&tWriter, "\n", 1u)) ) {
		xrtBufferDestroy(tWriter.pBuffer);
		__xrtXmlDomSetError(pError, tWriter.iErrorCode,
			tWriter.iErrorCode == XRT_XML_ERROR_FORMAT ?
			"XML DOM contains invalid XML content" :
			tWriter.iErrorCode == XRT_XML_ERROR_MEMORY ?
			"could not allocate XML DOM writer state" :
			"could not encode XML DOM within the configured output limit");
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return tWriter.pBuffer;
}

/* 保留底层分配失败；文件和流的其他失败仍归为 IO。 */
static int __xrtXmlDomOutputErrorCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_XML_ERROR_MEMORY : XRT_XML_ERROR_IO;
}

XRT_API bool xrtXmlDomWrite(const xrtxmldomnode* pNode, xwriter* pWriter,
	bool bTakeOwnership, const xrtxmldomwriteoptions* pOptions,
	xrtxmlerror* pError)
{
	xbuffer* pBuffer = xrtXmlDomStringify(pNode, pOptions, pError);
	bool bOk = false;
	int iOutputError = XRT_XML_ERROR_IO;
	if ( pBuffer != NULL && pWriter != NULL ) {
		xrtClearError();
		bOk = xrtWriterWriteFull(pWriter, pBuffer->Data, pBuffer->Size, NULL) &&
			xrtWriterFlush(pWriter);
		if ( !bOk ) { iOutputError = __xrtXmlDomOutputErrorCode(); }
	}
	if ( bTakeOwnership && pWriter != NULL ) {
		bool bClosed = xrtWriterDestroy(pWriter);
		if ( bOk && !bClosed ) {
			iOutputError = __xrtXmlDomOutputErrorCode();
			bOk = false;
		}
	}
	xrtBufferDestroy(pBuffer);
	if ( !bOk && pError != NULL && pError->iCode == 0 ) {
		__xrtXmlDomSetError(pError, iOutputError,
			"could not write XML DOM output");
	}
	return bOk;
}

XRT_API bool xrtXmlDomSave(str sPath, const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError)
{
	xbuffer* pBuffer = xrtXmlDomStringify(pNode, pOptions, pError);
	bool bOk;
	if ( pBuffer == NULL ) { return false; }
	xrtClearError();
	bOk = xrtFileWriteAtomic((cstr)sPath,
		(xbytesview){ pBuffer->Data, pBuffer->Size });
	if ( !bOk ) {
		__xrtXmlDomSetError(pError, __xrtXmlDomOutputErrorCode(),
			"could not save XML DOM output file");
	}
	xrtBufferDestroy(pBuffer);
	return bOk;
}
