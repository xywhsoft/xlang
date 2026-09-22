#include "xrt_xml_dom_internal.h"



typedef struct {
	xrtxmldomnode* pDocument;
	xrtxmldomnode** pStack;
	uint32 iDepth;
	uint32 iCapacity;
	xrtxmlattribute* pPendingNamespaces;
	uint32 iPendingNamespaceCount;
	uint32 iPendingNamespaceCapacity;
	xrtxmlerror tError;
	bool bFailed;
} xrtxmldombuilder;

static void __xrtXmlDomBuilderError(xrtxmldombuilder* pBuilder,
	const xrtxmlevent* pEvent, const char* sMessage)
{
	pBuilder->bFailed = true;
	pBuilder->tError.iCode = XRT_XML_ERROR_MEMORY;
	pBuilder->tError.iLine = pEvent != NULL ? pEvent->iLine : 0u;
	pBuilder->tError.iColumn = pEvent != NULL ? pEvent->iColumn : 0u;
	pBuilder->tError.iOffset = pEvent != NULL ? pEvent->iOffset : 0u;
	snprintf(pBuilder->tError.sMessage, sizeof(pBuilder->tError.sMessage),
		"%s", sMessage);
}

static bool __xrtXmlDomPush(xrtxmldombuilder* pBuilder,
	xrtxmldomnode* pNode)
{
	xrtxmldomnode** pItems;
	uint32 iCapacity;
	if ( pBuilder->iDepth == pBuilder->iCapacity ) {
		iCapacity = pBuilder->iCapacity != 0u ? pBuilder->iCapacity * 2u : 16u;
		pItems = (xrtxmldomnode**)xrtRealloc(pBuilder->pStack,
			sizeof(xrtxmldomnode*) * iCapacity);
		if ( pItems == NULL ) { return false; }
		pBuilder->pStack = pItems;
		pBuilder->iCapacity = iCapacity;
	}
	pBuilder->pStack[pBuilder->iDepth++] = pNode;
	return true;
}

static void __xrtXmlDomClearNamespaces(xrtxmldombuilder* pBuilder)
{
	uint32 i;
	for ( i = 0u; i < pBuilder->iPendingNamespaceCount; i++ ) {
		xrtFree(pBuilder->pPendingNamespaces[i].sName);
		xrtFree(pBuilder->pPendingNamespaces[i].sValue);
		memset(&pBuilder->pPendingNamespaces[i], 0, sizeof(xrtxmlattribute));
	}
	pBuilder->iPendingNamespaceCount = 0u;
}

static bool __xrtXmlDomRememberNamespace(xrtxmldombuilder* pBuilder,
	const xrtxmlevent* pEvent)
{
	xrtxmlattribute* pItems;
	xrtxmlattribute* pItem;
	uint32 iCapacity;
	size_t iPrefix = strlen((const char*)pEvent->sPrefix);
	char* sName;
	if ( pBuilder->iPendingNamespaceCount ==
		pBuilder->iPendingNamespaceCapacity ) {
		iCapacity = pBuilder->iPendingNamespaceCapacity != 0u ?
			pBuilder->iPendingNamespaceCapacity * 2u : 4u;
		pItems = (xrtxmlattribute*)xrtRealloc(pBuilder->pPendingNamespaces,
			sizeof(xrtxmlattribute) * iCapacity);
		if ( pItems == NULL ) { return false; }
		memset(pItems + pBuilder->iPendingNamespaceCapacity, 0,
			sizeof(xrtxmlattribute) *
			(iCapacity - pBuilder->iPendingNamespaceCapacity));
		pBuilder->pPendingNamespaces = pItems;
		pBuilder->iPendingNamespaceCapacity = iCapacity;
	}
	sName = (char*)xrtMalloc(6u + (iPrefix != 0u ? 1u + iPrefix : 0u));
	if ( sName == NULL ) { return false; }
	memcpy(sName, "xmlns", 5u);
	if ( iPrefix != 0u ) {
		sName[5] = ':';
		memcpy(sName + 6u, pEvent->sPrefix, iPrefix);
		sName[6u + iPrefix] = 0;
	} else { sName[5] = 0; }
	pItem = &pBuilder->pPendingNamespaces[pBuilder->iPendingNamespaceCount];
	pItem->sName = (str)sName;
	pItem->sValue = __xrtXmlDomCopy((const char*)pEvent->sNamespaceUri);
	if ( pItem->sValue == NULL ) { xrtFree(sName); pItem->sName = NULL; return false; }
	pBuilder->iPendingNamespaceCount++;
	return true;
}

static bool __xrtXmlDomCopyName(xrtxmldomnode* pNode,
	const xrtxmlevent* pEvent)
{
	str sLocal = __xrtXmlDomCopy((const char*)pEvent->sLocalName);
	str sUri = __xrtXmlDomCopy((const char*)pEvent->sNamespaceUri);
	str sPrefix = __xrtXmlDomCopy((const char*)pEvent->sPrefix);
	if ( sLocal == NULL || sUri == NULL || sPrefix == NULL ) {
		xrtFree(sLocal); xrtFree(sUri); xrtFree(sPrefix);
		return false;
	}
	xrtFree(pNode->sLocalName); pNode->sLocalName = sLocal;
	xrtFree(pNode->sNamespaceUri); pNode->sNamespaceUri = sUri;
	xrtFree(pNode->sPrefix); pNode->sPrefix = sPrefix;
	return true;
}

static bool __xrtXmlDomCopyAttributes(xrtxmldomnode* pNode,
	const xrtxmlevent* pEvent)
{
	uint32 i;
	for ( i = 0u; i < pEvent->iAttributeCount; i++ ) {
		const xrtxmlattribute* pSource = &pEvent->pAttributes[i];
		xrtxmlattribute* pTarget;
		str sLocal;
		str sUri;
		str sPrefix;
		if ( !xrtXmlDomSetAttribute(pNode, pSource->sName, pSource->sValue) ) {
			return false;
		}
		pTarget = &pNode->pAttributes[pNode->iAttributeCount - 1u];
		sLocal = __xrtXmlDomCopy((const char*)pSource->sLocalName);
		sUri = __xrtXmlDomCopy((const char*)pSource->sNamespaceUri);
		sPrefix = __xrtXmlDomCopy((const char*)pSource->sPrefix);
		if ( sLocal == NULL || sUri == NULL || sPrefix == NULL ) {
			xrtFree(sLocal); xrtFree(sUri); xrtFree(sPrefix);
			return false;
		}
		xrtFree(pTarget->sLocalName); pTarget->sLocalName = sLocal;
		xrtFree(pTarget->sNamespaceUri); pTarget->sNamespaceUri = sUri;
		xrtFree(pTarget->sPrefix); pTarget->sPrefix = sPrefix;
	}
	return true;
}

static bool __xrtXmlDomStart(xrtxmldombuilder* pBuilder,
	const xrtxmlevent* pEvent)
{
	xrtxmldomnode* pNode = __xrtXmlDomCreateNode(pBuilder->pDocument->pDocument,
		XRT_XML_DOM_ELEMENT_NODE, (const char*)pEvent->sName, "");
	xrtxmldomnode* pParent = pBuilder->iDepth != 0u ?
		pBuilder->pStack[pBuilder->iDepth - 1u] : pBuilder->pDocument;
	uint32 i;
	if ( pNode == NULL || !__xrtXmlDomCopyName(pNode, pEvent) ||
		!__xrtXmlDomCopyAttributes(pNode, pEvent) ) { return false; }
	for ( i = 0u; i < pBuilder->iPendingNamespaceCount; i++ ) {
		xrtxmlattribute* pNamespace = &pBuilder->pPendingNamespaces[i];
		if ( !xrtXmlDomSetAttribute(pNode, pNamespace->sName,
			pNamespace->sValue) ) { return false; }
	}
	__xrtXmlDomClearNamespaces(pBuilder);
	return xrtXmlDomAppendChild(pParent, pNode) && __xrtXmlDomPush(pBuilder, pNode);
}

static bool __xrtXmlDomAppendCharacterData(xrtxmldombuilder* pBuilder,
	const char* sText)
{
	xrtxmldomnode* pParent;
	xrtxmldomnode* pLast;
	xrtxmldomnode* pText;
	if ( pBuilder->iDepth == 0u ) {
		const unsigned char* p = (const unsigned char*)sText;
		while ( *p != 0u ) {
			if ( *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' ) {
				return false;
			}
			p++;
		}
		return true;
	}
	pParent = pBuilder->pStack[pBuilder->iDepth - 1u];
	pLast = pParent->iChildCount != 0u ?
		pParent->pChildren[pParent->iChildCount - 1u] : NULL;
	if ( pLast != NULL && pLast->iType == XRT_XML_DOM_TEXT_NODE ) {
		return __xrtXmlDomAppendText(&pLast->sValue, sText);
	}
	pText = __xrtXmlDomCreateNode(pBuilder->pDocument->pDocument,
		XRT_XML_DOM_TEXT_NODE, "#text", sText);
	return pText != NULL && xrtXmlDomAppendChild(pParent, pText);
}

static bool __xrtXmlDomOnEvent(ptr pContext, const xrtxmlevent* pEvent)
{
	xrtxmldombuilder* pBuilder = (xrtxmldombuilder*)pContext;
	xrtxmldomnode* pNode;
	xrtxmldomnode* pParent;
	if ( pBuilder->bFailed ) { return false; }
	if ( pEvent->iType == XRT_XML_EVENT_START_NAMESPACE ) {
		if ( __xrtXmlDomRememberNamespace(pBuilder, pEvent) ) { return true; }
	} else if ( pEvent->iType == XRT_XML_EVENT_START ) {
		if ( __xrtXmlDomStart(pBuilder, pEvent) ) { return true; }
	} else if ( pEvent->iType == XRT_XML_EVENT_END ) {
		if ( pBuilder->iDepth != 0u ) { pBuilder->iDepth--; return true; }
	} else if ( pEvent->iType == XRT_XML_EVENT_TEXT ) {
		if ( __xrtXmlDomAppendCharacterData(pBuilder,
			(const char*)pEvent->sText) ) { return true; }
	} else if ( pEvent->iType == XRT_XML_EVENT_COMMENT ||
		pEvent->iType == XRT_XML_EVENT_PROCESSING_INSTRUCTION ) {
		pParent = pBuilder->iDepth != 0u ?
			pBuilder->pStack[pBuilder->iDepth - 1u] : pBuilder->pDocument;
		pNode = __xrtXmlDomCreateNode(pBuilder->pDocument->pDocument,
			pEvent->iType == XRT_XML_EVENT_COMMENT ?
				XRT_XML_DOM_COMMENT_NODE :
				XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE,
			pEvent->iType == XRT_XML_EVENT_COMMENT ? "#comment" :
				(const char*)pEvent->sTarget,
			(const char*)pEvent->sText);
		if ( pNode != NULL && xrtXmlDomAppendChild(pParent, pNode) ) { return true; }
	} else {
		return true;
	}
	__xrtXmlDomBuilderError(pBuilder, pEvent, "could not build XML DOM node");
	return false;
}

XRT_API xrtxmldomnode* xrtXmlDomParse(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xrtxmldombuilder tBuilder;
	xrtxmldomnode* pDocument;
	xrtxmldomnode* pRoot;
	bool bOk;
	memset(&tBuilder, 0, sizeof(tBuilder));
	pDocument = xrtXmlDomCreateDocument();
	if ( pDocument == NULL ) {
		if ( pReader != NULL && bTakeOwnership ) { xrtReaderDestroy(pReader); }
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_MEMORY,
			"could not allocate XML DOM document");
		return NULL;
	}
	tBuilder.pDocument = pDocument;
	xrtXmlErrorInit(&tBuilder.tError);
	bOk = xrtXmlSaxParse(pReader, bTakeOwnership, pOptions,
		__xrtXmlDomOnEvent, &tBuilder, pError);
	pRoot = xrtXmlDomDocumentElement(pDocument);
	if ( !bOk || tBuilder.bFailed || tBuilder.iDepth != 0u ||
		pRoot == NULL ) {
		if ( tBuilder.bFailed && pError != NULL ) { *pError = tBuilder.tError; }
		xrtXmlDomNodeRelease(pRoot);
		xrtXmlDomNodeRelease(pDocument);
		pDocument = NULL;
	} else { xrtXmlDomNodeRelease(pRoot); }
	__xrtXmlDomClearNamespaces(&tBuilder);
	xrtFree(tBuilder.pPendingNamespaces);
	xrtFree(tBuilder.pStack);
	return pDocument;
}

XRT_API xrtxmldomnode* xrtXmlDomParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xreader* pReader = xrtReaderFromMemory(
		(xbytesview){ (cbytes)pData, (size_t)iSize });
	return xrtXmlDomParse(pReader, true, pOptions, pError);
}

XRT_API xrtxmldomnode* xrtXmlDomLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xreader* pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		__xrtXmlDomSetError(pError, XRT_XML_ERROR_IO,
			"could not open XML DOM file");
		return NULL;
	}
	return xrtXmlDomParse(pReader, true, pOptions, pError);
}
