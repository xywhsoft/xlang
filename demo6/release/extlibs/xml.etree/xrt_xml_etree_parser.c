#include "xrt_xml_etree_internal.h"



typedef struct {
	xrtxmldocument* pDocument;
	xrtxmlelement** pStack;
	uint32 iDepth;
	uint32 iCapacity;
	xrtxmlelement* pLastClosed;
	xrtxmlattribute* pPendingNamespaces;
	uint32 iPendingNamespaceCount;
	uint32 iPendingNamespaceCapacity;
	xrtxmlerror tError;
	bool bFailed;
} xrtxmltreebuilder;

static void __xrtXmlEtreeClearNamespaces(xrtxmltreebuilder* pBuilder)
{
	uint32 i;
	for ( i = 0u; i < pBuilder->iPendingNamespaceCount; i++ ) {
		xrtFree(pBuilder->pPendingNamespaces[i].sName);
		xrtFree(pBuilder->pPendingNamespaces[i].sValue);
		memset(&pBuilder->pPendingNamespaces[i], 0, sizeof(xrtxmlattribute));
	}
	pBuilder->iPendingNamespaceCount = 0u;
}

static bool __xrtXmlEtreeRememberNamespace(xrtxmltreebuilder* pBuilder,
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
		if ( pItems == NULL ) { return FALSE; }
		memset(pItems + pBuilder->iPendingNamespaceCapacity, 0,
			sizeof(xrtxmlattribute) *
			(iCapacity - pBuilder->iPendingNamespaceCapacity));
		pBuilder->pPendingNamespaces = pItems;
		pBuilder->iPendingNamespaceCapacity = iCapacity;
	}
	sName = (char*)xrtMalloc(6u + (iPrefix != 0u ? 1u + iPrefix : 0u));
	if ( sName == NULL ) { return FALSE; }
	memcpy(sName, "xmlns", 5u);
	if ( iPrefix != 0u ) {
		sName[5] = ':';
		memcpy(sName + 6, pEvent->sPrefix, iPrefix);
		sName[6 + iPrefix] = 0;
	} else {
		sName[5] = 0;
	}
	pItem = &pBuilder->pPendingNamespaces[pBuilder->iPendingNamespaceCount];
	pItem->sName = (str)sName;
	pItem->sValue = __xrtXmlEtreeCopy((const char*)pEvent->sNamespaceUri);
	if ( pItem->sValue == NULL ) { xrtFree(sName); pItem->sName = NULL; return FALSE; }
	pBuilder->iPendingNamespaceCount++;
	return TRUE;
}

static void __xrtXmlEtreeBuilderError(xrtxmltreebuilder* pBuilder,
	const xrtxmlevent* pEvent, const char* sMessage)
{
	pBuilder->bFailed = TRUE;
	pBuilder->tError.iCode = XRT_XML_ERROR_MEMORY;
	pBuilder->tError.iLine = pEvent != NULL ? pEvent->iLine : 0u;
	pBuilder->tError.iColumn = pEvent != NULL ? pEvent->iColumn : 0u;
	pBuilder->tError.iOffset = pEvent != NULL ? pEvent->iOffset : 0u;
	snprintf(pBuilder->tError.sMessage, sizeof(pBuilder->tError.sMessage),
		"%s", sMessage);
}

static bool __xrtXmlEtreePush(xrtxmltreebuilder* pBuilder,
	xrtxmlelement* pElement)
{
	xrtxmlelement** pItems;
	uint32 iCapacity;
	if ( pBuilder->iDepth == pBuilder->iCapacity ) {
		iCapacity = pBuilder->iCapacity != 0u ? pBuilder->iCapacity * 2u : 16u;
		pItems = (xrtxmlelement**)xrtRealloc(pBuilder->pStack,
			sizeof(xrtxmlelement*) * iCapacity);
		if ( pItems == NULL ) { return FALSE; }
		pBuilder->pStack = pItems;
		pBuilder->iCapacity = iCapacity;
	}
	pBuilder->pStack[pBuilder->iDepth++] = pElement;
	return TRUE;
}

static bool __xrtXmlEtreeCopyEventName(xrtxmlelement* pElement,
	const xrtxmlevent* pEvent)
{
	str sLocal = __xrtXmlEtreeCopy((const char*)pEvent->sLocalName);
	str sUri = __xrtXmlEtreeCopy((const char*)pEvent->sNamespaceUri);
	str sPrefix = __xrtXmlEtreeCopy((const char*)pEvent->sPrefix);
	if ( sLocal == NULL || sUri == NULL || sPrefix == NULL ) {
		xrtFree(sLocal); xrtFree(sUri); xrtFree(sPrefix);
		return FALSE;
	}
	xrtFree(pElement->sLocalName); pElement->sLocalName = sLocal;
	xrtFree(pElement->sNamespaceUri); pElement->sNamespaceUri = sUri;
	xrtFree(pElement->sPrefix); pElement->sPrefix = sPrefix;
	return TRUE;
}

static bool __xrtXmlEtreeCopyAttributes(xrtxmlelement* pElement,
	const xrtxmlevent* pEvent)
{
	uint32 i;
	for ( i = 0u; i < pEvent->iAttributeCount; i++ ) {
		const xrtxmlattribute* pSource = &pEvent->pAttributes[i];
		if ( !xrtXmlElementSetAttribute(pElement, pSource->sName,
			pSource->sValue) ) { return FALSE; }
		{
			xrtxmlattribute* pTarget =
				&pElement->pAttributes[pElement->iAttributeCount - 1u];
			str sLocal = __xrtXmlEtreeCopy((const char*)pSource->sLocalName);
			str sUri = __xrtXmlEtreeCopy((const char*)pSource->sNamespaceUri);
			str sPrefix = __xrtXmlEtreeCopy((const char*)pSource->sPrefix);
			if ( sLocal == NULL || sUri == NULL || sPrefix == NULL ) {
				xrtFree(sLocal); xrtFree(sUri); xrtFree(sPrefix);
				return FALSE;
			}
			xrtFree(pTarget->sLocalName); pTarget->sLocalName = sLocal;
			xrtFree(pTarget->sNamespaceUri); pTarget->sNamespaceUri = sUri;
			xrtFree(pTarget->sPrefix); pTarget->sPrefix = sPrefix;
		}
	}
	return TRUE;
}

static bool __xrtXmlEtreeStartElement(xrtxmltreebuilder* pBuilder,
	const xrtxmlevent* pEvent)
{
	xrtxmlelement* pNode;
	uint32 i;
	pNode = __xrtXmlEtreeCreateNode(pBuilder->pDocument, XRT_XML_ELEMENT,
		(const char*)pEvent->sName);
	if ( pNode == NULL || !__xrtXmlEtreeCopyEventName(pNode, pEvent) ||
		!__xrtXmlEtreeCopyAttributes(pNode, pEvent) ) { goto failed; }
	for ( i = 0u; i < pBuilder->iPendingNamespaceCount; i++ ) {
		xrtxmlattribute* pNamespace = &pBuilder->pPendingNamespaces[i];
		if ( !xrtXmlElementSetAttribute(pNode, pNamespace->sName,
			pNamespace->sValue) ) { goto failed; }
	}
	__xrtXmlEtreeClearNamespaces(pBuilder);
	if ( (pBuilder->iDepth != 0u && !__xrtXmlEtreeAppendNode(
			pBuilder->pStack[pBuilder->iDepth - 1u], pNode)) ||
		!__xrtXmlEtreePush(pBuilder, pNode) ) { goto failed; }
	if ( pBuilder->pDocument->pRoot == NULL ) {
		pBuilder->pDocument->pRoot = pNode;
	}
	pBuilder->pLastClosed = NULL;
	return TRUE;

failed:
	if ( pNode != NULL && pNode->pParent == NULL &&
		pBuilder->pDocument->pRoot != pNode ) {
		__xrtXmlEtreeFreeNode(pNode);
	}
	__xrtXmlEtreeBuilderError(pBuilder, pEvent,
		"could not build XML element");
	return FALSE;
}

static bool __xrtXmlEtreeOnEvent(ptr pContext, const xrtxmlevent* pEvent)
{
	xrtxmltreebuilder* pBuilder = (xrtxmltreebuilder*)pContext;
	xrtxmlelement* pNode;
	if ( pBuilder->bFailed ) { return FALSE; }
	if ( pEvent->iType == XRT_XML_EVENT_START_NAMESPACE ) {
		if ( !__xrtXmlEtreeRememberNamespace(pBuilder, pEvent) ) {
			__xrtXmlEtreeBuilderError(pBuilder, pEvent,
				"could not preserve XML namespace declaration");
			return FALSE;
		}
	} else if ( pEvent->iType == XRT_XML_EVENT_START ) {
		if ( !__xrtXmlEtreeStartElement(pBuilder, pEvent) ) { return FALSE; }
	} else if ( pEvent->iType == XRT_XML_EVENT_END ) {
		if ( pBuilder->iDepth == 0u ) {
			__xrtXmlEtreeBuilderError(pBuilder, pEvent,
				"unbalanced XML element events");
			return FALSE;
		}
		pBuilder->pLastClosed = pBuilder->pStack[--pBuilder->iDepth];
	} else if ( pEvent->iType == XRT_XML_EVENT_TEXT ) {
		if ( pBuilder->pLastClosed != NULL ) {
			if ( !__xrtXmlEtreeAppendText(&pBuilder->pLastClosed->sTail,
				(const char*)pEvent->sText) ) { goto text_failed; }
		} else if ( pBuilder->iDepth != 0u &&
			!__xrtXmlEtreeAppendText(
				&pBuilder->pStack[pBuilder->iDepth - 1u]->sText,
				(const char*)pEvent->sText) ) {
			goto text_failed;
		}
	} else if ( pEvent->iType == XRT_XML_EVENT_COMMENT ||
		pEvent->iType == XRT_XML_EVENT_PROCESSING_INSTRUCTION ) {
		if ( pBuilder->iDepth == 0u ) { return TRUE; }
		pNode = __xrtXmlEtreeCreateNode(pBuilder->pDocument,
			pEvent->iType == XRT_XML_EVENT_COMMENT ? XRT_XML_COMMENT :
			XRT_XML_PROCESSING_INSTRUCTION,
			pEvent->iType == XRT_XML_EVENT_COMMENT ? "#comment" :
			"#processing-instruction");
		if ( pNode == NULL || !xrtXmlElementSetText(pNode, pEvent->sText) ||
			(pEvent->iType == XRT_XML_EVENT_PROCESSING_INSTRUCTION &&
			 !xrtXmlElementSetAttribute(pNode, (str)"target", pEvent->sTarget)) ||
			!__xrtXmlEtreeAppendNode(pBuilder->pStack[pBuilder->iDepth - 1u],
				pNode) ) {
			if ( pNode != NULL && pNode->pParent == NULL ) {
				__xrtXmlEtreeFreeNode(pNode);
			}
			__xrtXmlEtreeBuilderError(pBuilder, pEvent,
				"could not build XML special node");
			return FALSE;
		}
		pBuilder->pLastClosed = pNode;
	}
	return TRUE;

text_failed:
	__xrtXmlEtreeBuilderError(pBuilder, pEvent,
		"could not append XML text");
	return FALSE;
}

XXAPI xrtxmlelement* xrtXmlElementParse(xreader pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xrtxmltreebuilder tBuilder;
	xrtxmldocument* pDocument;
	bool bOk;
	memset(&tBuilder, 0, sizeof(tBuilder));
	pDocument = (xrtxmldocument*)xrtMalloc(sizeof(*pDocument));
	if ( pDocument == NULL ) {
		if ( pReader != NULL && bTakeOwnership ) { xrtReaderDestroy(pReader); }
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_MEMORY,
			"could not allocate XML document");
		return NULL;
	}
	memset(pDocument, 0, sizeof(*pDocument));
	pDocument->iReferences = 1u;
	tBuilder.pDocument = pDocument;
	xrtXmlErrorInit(&tBuilder.tError);
	bOk = xrtXmlSaxParse(pReader, bTakeOwnership, pOptions,
		__xrtXmlEtreeOnEvent, &tBuilder, pError);
	if ( !bOk || tBuilder.bFailed || pDocument->pRoot == NULL ||
		tBuilder.iDepth != 0u ) {
		if ( tBuilder.bFailed && pError != NULL ) { *pError = tBuilder.tError; }
		__xrtXmlEtreeFreeNode(pDocument->pRoot);
		xrtFree(pDocument);
		__xrtXmlEtreeClearNamespaces(&tBuilder);
		xrtFree(tBuilder.pPendingNamespaces);
		xrtFree(tBuilder.pStack);
		return NULL;
	}
	xrtFree(tBuilder.pStack);
	__xrtXmlEtreeClearNamespaces(&tBuilder);
	xrtFree(tBuilder.pPendingNamespaces);
	return pDocument->pRoot;
}

XXAPI xrtxmlelement* xrtXmlElementParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xreader pReader = xrtReaderFromMemory(pData, iSize);
	return xrtXmlElementParse(pReader, TRUE, pOptions, pError);
}

XXAPI xrtxmlelement* xrtXmlElementLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xreader pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		__xrtXmlEtreeSetError(pError, XRT_XML_ERROR_IO,
			"could not open XML file");
		return NULL;
	}
	return xrtXmlElementParse(pReader, TRUE, pOptions, pError);
}
