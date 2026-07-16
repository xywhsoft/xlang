#include "xrt_xml_dom_internal.h"



typedef struct {
	xbuffer pBuffer;
	uint64 iLimit;
	uint32 iIndent;
	bool bPretty;
} xrtxmldomwriter;

XXAPI void xrtXmlDomWriteOptionsInit(xrtxmldomwriteoptions* pOptions)
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
	if ( iSize == 0u ) { return TRUE; }
	return pWriter != NULL && iSize <= UINT32_MAX &&
		(uint64)pWriter->pBuffer->Length <= pWriter->iLimit - iSize &&
		xrtBufferAppend(pWriter->pBuffer, (ptr)sText, (uint32)iSize, XBUF_BINARY);
}

static bool __xrtXmlDomWriterEscape(xrtxmldomwriter* pWriter,
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
			if ( !__xrtXmlDomWriterAppend(pWriter, (const char*)pStart,
				(size_t)(p - pStart)) ||
				!__xrtXmlDomWriterAppend(pWriter, sEscape, strlen(sEscape)) ) {
				return FALSE;
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
	uint32 i;
	if ( !pWriter->bPretty ) { return TRUE; }
	if ( !__xrtXmlDomWriterAppend(pWriter, "\n", 1u) ) { return FALSE; }
	for ( i = 0u; i < iDepth * pWriter->iIndent; i++ ) {
		if ( !__xrtXmlDomWriterAppend(pWriter, " ", 1u) ) { return FALSE; }
	}
	return TRUE;
}

static bool __xrtXmlDomHasTextChildren(const xrtxmldomnode* pNode)
{
	uint32 i;
	for ( i = 0u; pNode != NULL && i < pNode->iChildCount; i++ ) {
		if ( pNode->pChildren[i]->iType == XRT_XML_DOM_TEXT_NODE &&
			pNode->pChildren[i]->sValue[0] != 0 ) { return TRUE; }
	}
	return FALSE;
}

static bool __xrtXmlDomWriteNode(xrtxmldomwriter* pWriter,
	const xrtxmldomnode* pNode, uint32 iDepth, bool bParentPretty)
{
	uint32 i;
	bool bPrettyChildren;
	if ( pNode->iType == XRT_XML_DOM_TEXT_NODE ) {
		return __xrtXmlDomWriterEscape(pWriter, (const char*)pNode->sValue,
			FALSE);
	}
	if ( bParentPretty && !__xrtXmlDomWriterIndent(pWriter, iDepth) ) {
		return FALSE;
	}
	if ( pNode->iType == XRT_XML_DOM_COMMENT_NODE ) {
		if ( strstr((const char*)pNode->sValue, "--") != NULL ) { return FALSE; }
		return __xrtXmlDomWriterAppend(pWriter, "<!--", 4u) &&
			__xrtXmlDomWriterAppend(pWriter, (const char*)pNode->sValue,
				strlen((const char*)pNode->sValue)) &&
			__xrtXmlDomWriterAppend(pWriter, "-->", 3u);
	}
	if ( pNode->iType == XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE ) {
		if ( strstr((const char*)pNode->sValue, "?>") != NULL ) { return FALSE; }
		return __xrtXmlDomWriterAppend(pWriter, "<?", 2u) &&
			__xrtXmlDomWriterAppend(pWriter, (const char*)pNode->sName,
				strlen((const char*)pNode->sName)) &&
			(pNode->sValue[0] == 0 ||
			 (__xrtXmlDomWriterAppend(pWriter, " ", 1u) &&
			  __xrtXmlDomWriterAppend(pWriter, (const char*)pNode->sValue,
				strlen((const char*)pNode->sValue)))) &&
			__xrtXmlDomWriterAppend(pWriter, "?>", 2u);
	}
	if ( pNode->iType == XRT_XML_DOM_DOCUMENT_NODE ) {
		for ( i = 0u; i < pNode->iChildCount; i++ ) {
			if ( !__xrtXmlDomWriteNode(pWriter, pNode->pChildren[i], 0u,
				pWriter->bPretty && i != 0u) ) { return FALSE; }
		}
		return TRUE;
	}
	if ( pNode->iType != XRT_XML_DOM_ELEMENT_NODE ) { return FALSE; }
	bPrettyChildren = pWriter->bPretty && !__xrtXmlDomHasTextChildren(pNode);
	if ( !__xrtXmlDomWriterAppend(pWriter, "<", 1u) ||
		!__xrtXmlDomWriterAppend(pWriter, (const char*)pNode->sName,
			strlen((const char*)pNode->sName)) ) { return FALSE; }
	for ( i = 0u; i < pNode->iAttributeCount; i++ ) {
		const xrtxmlattribute* pAttribute = &pNode->pAttributes[i];
		if ( !__xrtXmlDomWriterAppend(pWriter, " ", 1u) ||
			!__xrtXmlDomWriterAppend(pWriter, (const char*)pAttribute->sName,
				strlen((const char*)pAttribute->sName)) ||
			!__xrtXmlDomWriterAppend(pWriter, "=\"", 2u) ||
			!__xrtXmlDomWriterEscape(pWriter,
				(const char*)pAttribute->sValue, TRUE) ||
			!__xrtXmlDomWriterAppend(pWriter, "\"", 1u) ) { return FALSE; }
	}
	if ( pNode->iChildCount == 0u ) {
		return __xrtXmlDomWriterAppend(pWriter, "/>", 2u);
	}
	if ( !__xrtXmlDomWriterAppend(pWriter, ">", 1u) ) { return FALSE; }
	for ( i = 0u; i < pNode->iChildCount; i++ ) {
		if ( !__xrtXmlDomWriteNode(pWriter, pNode->pChildren[i], iDepth + 1u,
			bPrettyChildren) ) { return FALSE; }
	}
	if ( bPrettyChildren && pNode->iChildCount != 0u &&
		!__xrtXmlDomWriterIndent(pWriter, iDepth) ) { return FALSE; }
	return __xrtXmlDomWriterAppend(pWriter, "</", 2u) &&
		__xrtXmlDomWriterAppend(pWriter, (const char*)pNode->sName,
			strlen((const char*)pNode->sName)) &&
		__xrtXmlDomWriterAppend(pWriter, ">", 1u);
}

static bool __xrtXmlDomValidateWriteOptions(
	const xrtxmldomwriteoptions* pOptions, xrtxmldomwriteoptions* pResult)
{
	xrtXmlDomWriteOptionsInit(pResult);
	if ( pOptions == NULL ) { return TRUE; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_XML_DOM_WRITE_OPTIONS_VERSION ||
		pOptions->iIndent > 16u || pOptions->iMaxOutputBytes == 0u ) {
		return FALSE;
	}
	*pResult = *pOptions;
	return TRUE;
}

XXAPI xbuffer xrtXmlDomStringify(const xrtxmldomnode* pNode,
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
	tWriter.pBuffer = xrtBufferCreate(4096u);
	tWriter.iLimit = tOptions.iMaxOutputBytes;
	tWriter.iIndent = tOptions.iIndent;
	tWriter.bPretty = tOptions.bPretty;
	if ( tWriter.pBuffer == NULL ||
		(tOptions.bXmlDeclaration && !__xrtXmlDomWriterAppend(&tWriter,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 38u)) ||
		(tOptions.bXmlDeclaration && tOptions.bPretty &&
		 !__xrtXmlDomWriterAppend(&tWriter, "\n", 1u)) ||
		!__xrtXmlDomWriteNode(&tWriter, pNode, 0u, FALSE) ||
		(tOptions.bPretty && !__xrtXmlDomWriterAppend(&tWriter, "\n", 1u)) ) {
		xrtBufferDestroy(tWriter.pBuffer);
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_LIMIT,
			"could not encode XML DOM within the configured output limit");
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return tWriter.pBuffer;
}

XXAPI bool xrtXmlDomWrite(const xrtxmldomnode* pNode, xwriter pWriter,
	bool bTakeOwnership, const xrtxmldomwriteoptions* pOptions,
	xrtxmlerror* pError)
{
	xbuffer pBuffer = xrtXmlDomStringify(pNode, pOptions, pError);
	bool bOk = pBuffer != NULL && pWriter != NULL &&
		xrtWriterWriteBuffer(pWriter, pBuffer) && xrtWriterFlush(pWriter);
	if ( bTakeOwnership && pWriter != NULL ) {
		bOk = xrtWriterDestroy(pWriter) && bOk;
	}
	xrtBufferDestroy(pBuffer);
	if ( !bOk && pError != NULL && pError->iCode == 0 ) {
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_IO,
			"could not write XML DOM output");
	}
	return bOk;
}

XXAPI bool xrtXmlDomSave(str sPath, const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError)
{
	xwriter pWriter = xrtWriterOpen(sPath, TRUE);
	if ( pWriter == NULL ) {
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_IO,
			"could not open XML DOM output file");
		return FALSE;
	}
	return xrtXmlDomWrite(pNode, pWriter, TRUE, pOptions, pError);
}
