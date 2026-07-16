#include "xrt_xml_etree_internal.h"



typedef struct {
	xbuffer pBuffer;
	uint64 iLimit;
	uint32 iIndent;
	bool bPretty;
} xrtxmltreewriter;

XXAPI void xrtXmlElementWriteOptionsInit(xrtxmletreewriteoptions* pOptions)
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
	if ( iSize == 0u ) { return TRUE; }
	return pWriter != NULL && iSize <= UINT32_MAX &&
		(uint64)pWriter->pBuffer->Length <= pWriter->iLimit - iSize &&
		xrtBufferAppend(pWriter->pBuffer, (ptr)sText, (uint32)iSize, XBUF_BINARY);
}

static bool __xrtXmlEtreeWriterText(xrtxmltreewriter* pWriter,
	const char* sText, bool bAttribute)
{
	const unsigned char* p = (const unsigned char*)(sText != NULL ? sText : "");
	const unsigned char* pStart = p;
	while ( *p != 0u ) {
		const char* sEscape = NULL;
		if ( *p == '&' ) { sEscape = "&amp;"; }
		else if ( *p == '<' ) { sEscape = "&lt;"; }
		else if ( *p == '>' ) { sEscape = "&gt;"; }
		else if ( bAttribute && *p == '"' ) { sEscape = "&quot;"; }
		else if ( bAttribute && *p == '\'' ) { sEscape = "&apos;"; }
		if ( sEscape != NULL ) {
			if ( !__xrtXmlEtreeWriterAppend(pWriter, (const char*)pStart,
				(size_t)(p - pStart)) ||
				!__xrtXmlEtreeWriterAppend(pWriter, sEscape, strlen(sEscape)) ) {
				return FALSE;
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
	uint32 i;
	if ( !pWriter->bPretty ) { return TRUE; }
	if ( !__xrtXmlEtreeWriterAppend(pWriter, "\n", 1u) ) { return FALSE; }
	for ( i = 0u; i < iDepth * pWriter->iIndent; i++ ) {
		if ( !__xrtXmlEtreeWriterAppend(pWriter, " ", 1u) ) { return FALSE; }
	}
	return TRUE;
}

static bool __xrtXmlEtreeHasMixedContent(const xrtxmlelement* pElement)
{
	uint32 i;
	if ( pElement == NULL ) { return FALSE; }
	if ( pElement->sText != NULL && pElement->sText[0] != 0 ) { return TRUE; }
	for ( i = 0u; i < pElement->iChildCount; i++ ) {
		if ( pElement->pChildren[i]->sTail != NULL &&
			pElement->pChildren[i]->sTail[0] != 0 ) { return TRUE; }
	}
	return FALSE;
}

static bool __xrtXmlEtreeWriteNode(xrtxmltreewriter* pWriter,
	const xrtxmlelement* pElement, uint32 iDepth, bool bParentPretty)
{
	uint32 i;
	bool bPrettyChildren;
	if ( pElement->iKind == XRT_XML_COMMENT ) {
		return (!bParentPretty || __xrtXmlEtreeIndent(pWriter, iDepth)) &&
			__xrtXmlEtreeWriterAppend(pWriter, "<!--", 4u) &&
			__xrtXmlEtreeWriterAppend(pWriter, (const char*)pElement->sText,
				strlen((const char*)pElement->sText)) &&
			__xrtXmlEtreeWriterAppend(pWriter, "-->", 3u);
	}
	if ( pElement->iKind == XRT_XML_PROCESSING_INSTRUCTION ) {
		str sTarget = xrtXmlElementGetAttribute(pElement, (str)"target");
		return (!bParentPretty || __xrtXmlEtreeIndent(pWriter, iDepth)) &&
			__xrtXmlEtreeWriterAppend(pWriter, "<?", 2u) &&
			__xrtXmlEtreeWriterAppend(pWriter, (const char*)sTarget,
				strlen((const char*)sTarget)) &&
			(pElement->sText[0] == 0 ||
			 (__xrtXmlEtreeWriterAppend(pWriter, " ", 1u) &&
			  __xrtXmlEtreeWriterAppend(pWriter, (const char*)pElement->sText,
				strlen((const char*)pElement->sText)))) &&
			__xrtXmlEtreeWriterAppend(pWriter, "?>", 2u);
	}
	bPrettyChildren = pWriter->bPretty && !__xrtXmlEtreeHasMixedContent(pElement);
	if ( bParentPretty && !__xrtXmlEtreeIndent(pWriter, iDepth) ) { return FALSE; }
	if ( !__xrtXmlEtreeWriterAppend(pWriter, "<", 1u) ||
		!__xrtXmlEtreeWriterAppend(pWriter, (const char*)pElement->sName,
			strlen((const char*)pElement->sName)) ) { return FALSE; }
	for ( i = 0u; i < pElement->iAttributeCount; i++ ) {
		const xrtxmlattribute* pItem = &pElement->pAttributes[i];
		if ( !__xrtXmlEtreeWriterAppend(pWriter, " ", 1u) ||
			!__xrtXmlEtreeWriterAppend(pWriter, (const char*)pItem->sName,
				strlen((const char*)pItem->sName)) ||
			!__xrtXmlEtreeWriterAppend(pWriter, "=\"", 2u) ||
			!__xrtXmlEtreeWriterText(pWriter, (const char*)pItem->sValue, TRUE) ||
			!__xrtXmlEtreeWriterAppend(pWriter, "\"", 1u) ) { return FALSE; }
	}
	if ( pElement->iChildCount == 0u && pElement->sText[0] == 0 ) {
		return __xrtXmlEtreeWriterAppend(pWriter, "/>", 2u);
	}
	if ( !__xrtXmlEtreeWriterAppend(pWriter, ">", 1u) ||
		!__xrtXmlEtreeWriterText(pWriter, (const char*)pElement->sText, FALSE) ) {
		return FALSE;
	}
	for ( i = 0u; i < pElement->iChildCount; i++ ) {
		const xrtxmlelement* pChild = pElement->pChildren[i];
		if ( !__xrtXmlEtreeWriteNode(pWriter, pChild, iDepth + 1u,
			bPrettyChildren) ||
			!__xrtXmlEtreeWriterText(pWriter, (const char*)pChild->sTail,
				FALSE) ) { return FALSE; }
	}
	if ( bPrettyChildren && pElement->iChildCount != 0u &&
		!__xrtXmlEtreeIndent(pWriter, iDepth) ) { return FALSE; }
	return __xrtXmlEtreeWriterAppend(pWriter, "</", 2u) &&
		__xrtXmlEtreeWriterAppend(pWriter, (const char*)pElement->sName,
			strlen((const char*)pElement->sName)) &&
		__xrtXmlEtreeWriterAppend(pWriter, ">", 1u);
}

static bool __xrtXmlEtreeWriteOptions(const xrtxmletreewriteoptions* pOptions,
	xrtxmletreewriteoptions* pResult)
{
	xrtXmlElementWriteOptionsInit(pResult);
	if ( pOptions == NULL ) { return TRUE; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_XML_ETREE_WRITE_OPTIONS_VERSION ||
		pOptions->iIndent > 16u || pOptions->iMaxOutputBytes == 0u ) {
		return FALSE;
	}
	*pResult = *pOptions;
	return TRUE;
}

XXAPI xbuffer xrtXmlElementStringify(const xrtxmlelement* pElement,
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
	tWriter.pBuffer = xrtBufferCreate(4096u);
	tWriter.iLimit = tOptions.iMaxOutputBytes;
	tWriter.iIndent = tOptions.iIndent;
	tWriter.bPretty = tOptions.bPretty;
	if ( tWriter.pBuffer == NULL ||
		(tOptions.bXmlDeclaration && !__xrtXmlEtreeWriterAppend(&tWriter,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 38u)) ||
		(tOptions.bXmlDeclaration && tOptions.bPretty &&
		 !__xrtXmlEtreeWriterAppend(&tWriter, "\n", 1u)) ||
		!__xrtXmlEtreeWriteNode(&tWriter, pElement, 0u, FALSE) ||
		(tOptions.bPretty && !__xrtXmlEtreeWriterAppend(&tWriter, "\n", 1u)) ) {
		xrtBufferDestroy(tWriter.pBuffer);
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_LIMIT,
			"could not encode XML within the configured output limit");
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return tWriter.pBuffer;
}

XXAPI bool xrtXmlElementWrite(const xrtxmlelement* pElement, xwriter pWriter,
	bool bTakeOwnership, const xrtxmletreewriteoptions* pOptions,
	xrtxmlerror* pError)
{
	xbuffer pBuffer = xrtXmlElementStringify(pElement, pOptions, pError);
	bool bOk = pBuffer != NULL && pWriter != NULL &&
		xrtWriterWriteBuffer(pWriter, pBuffer) && xrtWriterFlush(pWriter);
	if ( bTakeOwnership && pWriter != NULL ) { bOk = xrtWriterDestroy(pWriter) && bOk; }
	xrtBufferDestroy(pBuffer);
	if ( !bOk && pError != NULL && pError->iCode == 0 ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_IO,
			"could not write XML output");
	}
	return bOk;
}

XXAPI bool xrtXmlElementSave(str sPath, const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError)
{
	xwriter pWriter = xrtWriterOpen(sPath, TRUE);
	if ( pWriter == NULL ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_IO,
			"could not open XML output file");
		return FALSE;
	}
	return xrtXmlElementWrite(pElement, pWriter, TRUE, pOptions, pError);
}
