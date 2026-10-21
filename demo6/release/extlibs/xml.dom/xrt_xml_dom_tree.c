#include "xrt_xml_dom_internal.h"



str __xrtXmlDomCopy(const char* sText)
{
	return xrtStrDup(sText != NULL ? sText : "");
}

void __xrtXmlDomSetError(xrtxmlerror* pError, int iCode,
	const char* sMessage)
{
	if ( pError == NULL ) { return; }
	xrtXmlErrorInit(pError);
	pError->iCode = iCode;
	snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
		sMessage != NULL ? sMessage : "XML DOM operation failed");
}

bool __xrtXmlDomValidName(const char* sName)
{
	return xrtXmlNameValid((str)sName);
}

static bool __xrtXmlDomTrackNode(xrtxmldomdocument* pDocument,
	xrtxmldomnode* pNode)
{
	xrtxmldomnode** pItems;
	uint32 iCapacity;
	if ( pDocument->iNodeCount == pDocument->iNodeCapacity ) {
		iCapacity = pDocument->iNodeCapacity != 0u ?
			pDocument->iNodeCapacity * 2u : 32u;
		pItems = (xrtxmldomnode**)xrtRealloc(pDocument->pNodes,
			sizeof(xrtxmldomnode*) * iCapacity);
		if ( pItems == NULL ) { return false; }
		pDocument->pNodes = pItems;
		pDocument->iNodeCapacity = iCapacity;
	}
	pDocument->pNodes[pDocument->iNodeCount++] = pNode;
	return true;
}

xrtxmldomnode* __xrtXmlDomCreateNode(xrtxmldomdocument* pDocument,
	int iType, const char* sName, const char* sValue)
{
	xrtxmldomnode* pNode;
	if ( pDocument == NULL || sName == NULL ||
		(iType == XRT_XML_DOM_ELEMENT_NODE && !__xrtXmlDomValidName(sName)) ) {
		return NULL;
	}
	pNode = (xrtxmldomnode*)xrtMalloc(sizeof(*pNode));
	if ( pNode == NULL ) { return NULL; }
	memset(pNode, 0, sizeof(*pNode));
	pNode->pDocument = pDocument;
	pNode->iType = iType;
	pNode->sName = __xrtXmlDomCopy(sName);
	pNode->sLocalName = __xrtXmlDomCopy(sName);
	pNode->sNamespaceUri = __xrtXmlDomCopy("");
	pNode->sPrefix = __xrtXmlDomCopy("");
	pNode->sValue = __xrtXmlDomCopy(sValue);
	if ( pNode->sName == NULL || pNode->sLocalName == NULL ||
		pNode->sNamespaceUri == NULL || pNode->sPrefix == NULL ||
		pNode->sValue == NULL || !__xrtXmlDomTrackNode(pDocument, pNode) ) {
		xrtFree(pNode->sName); xrtFree(pNode->sLocalName);
		xrtFree(pNode->sNamespaceUri); xrtFree(pNode->sPrefix);
		xrtFree(pNode->sValue); xrtFree(pNode);
		return NULL;
	}
	return pNode;
}

bool __xrtXmlDomAppendText(str* psTarget, const char* sText)
{
	size_t iOld;
	size_t iAdd;
	char* sResult;
	if ( psTarget == NULL || sText == NULL ) { return false; }
	iOld = *psTarget != NULL ? strlen((const char*)*psTarget) : 0u;
	iAdd = strlen(sText);
	if ( iAdd > SIZE_MAX - iOld - 1u ) { return false; }
	sResult = (char*)xrtMalloc(iOld + iAdd + 1u);
	if ( sResult == NULL ) { return false; }
	if ( iOld != 0u ) { memcpy(sResult, *psTarget, iOld); }
	if ( iAdd != 0u ) { memcpy(sResult + iOld, sText, iAdd); }
	sResult[iOld + iAdd] = 0;
	xrtFree(*psTarget);
	*psTarget = (str)sResult;
	return true;
}

static void __xrtXmlDomFreeNode(xrtxmldomnode* pNode)
{
	uint32 i;
	if ( pNode == NULL ) { return; }
	for ( i = 0u; i < pNode->iAttributeCount; i++ ) {
		xrtFree(pNode->pAttributes[i].sName);
		xrtFree(pNode->pAttributes[i].sLocalName);
		xrtFree(pNode->pAttributes[i].sNamespaceUri);
		xrtFree(pNode->pAttributes[i].sPrefix);
		xrtFree(pNode->pAttributes[i].sValue);
	}
	xrtFree(pNode->pAttributes);
	xrtFree(pNode->pChildren);
	xrtFree(pNode->sName); xrtFree(pNode->sLocalName);
	xrtFree(pNode->sNamespaceUri); xrtFree(pNode->sPrefix);
	xrtFree(pNode->sValue);
	xrtFree(pNode);
}

void __xrtXmlDomFreeDocument(xrtxmldomdocument* pDocument)
{
	uint32 i;
	if ( pDocument == NULL ) { return; }
	for ( i = 0u; i < pDocument->iNodeCount; i++ ) {
		__xrtXmlDomFreeNode(pDocument->pNodes[i]);
	}
	xrtFree(pDocument->pNodes);
	xrtFree(pDocument);
}

XRT_API xrtxmldomnode* xrtXmlDomCreateDocument(void)
{
	xrtxmldomdocument* pDocument =
		(xrtxmldomdocument*)xrtMalloc(sizeof(*pDocument));
	if ( pDocument == NULL ) { return NULL; }
	memset(pDocument, 0, sizeof(*pDocument));
	pDocument->iReferences = 1u;
	pDocument->pDocumentNode = __xrtXmlDomCreateNode(pDocument,
		XRT_XML_DOM_DOCUMENT_NODE, "#document", "");
	if ( pDocument->pDocumentNode == NULL ) {
		__xrtXmlDomFreeDocument(pDocument);
		return NULL;
	}
	return pDocument->pDocumentNode;
}

static xrtxmldomnode* __xrtXmlDomDocumentNode(xrtxmldomnode* pNode)
{
	return pNode != NULL && pNode->pDocument != NULL ?
		pNode->pDocument->pDocumentNode : NULL;
}

XRT_API xrtxmldomnode* xrtXmlDomCreateElement(xrtxmldomnode* pDocument,
	str sName)
{
	xrtxmldomnode* pDoc = __xrtXmlDomDocumentNode(pDocument);
	xrtxmldomnode* pNode = pDoc != NULL ? __xrtXmlDomCreateNode(pDoc->pDocument,
		XRT_XML_DOM_ELEMENT_NODE, (const char*)sName, "") : NULL;
	return xrtXmlDomNodeRetain(pNode);
}

XRT_API xrtxmldomnode* xrtXmlDomCreateText(xrtxmldomnode* pDocument,
	str sText)
{
	xrtxmldomnode* pDoc = __xrtXmlDomDocumentNode(pDocument);
	xrtxmldomnode* pNode = pDoc != NULL ? __xrtXmlDomCreateNode(pDoc->pDocument,
		XRT_XML_DOM_TEXT_NODE, "#text", (const char*)sText) : NULL;
	return xrtXmlDomNodeRetain(pNode);
}

XRT_API xrtxmldomnode* xrtXmlDomCreateComment(xrtxmldomnode* pDocument,
	str sText)
{
	xrtxmldomnode* pDoc = __xrtXmlDomDocumentNode(pDocument);
	xrtxmldomnode* pNode = pDoc != NULL ? __xrtXmlDomCreateNode(pDoc->pDocument,
		XRT_XML_DOM_COMMENT_NODE, "#comment", (const char*)sText) : NULL;
	return xrtXmlDomNodeRetain(pNode);
}

XRT_API xrtxmldomnode* xrtXmlDomCreateProcessingInstruction(
	xrtxmldomnode* pDocument, str sTarget, str sText)
{
	xrtxmldomnode* pDoc = __xrtXmlDomDocumentNode(pDocument);
	xrtxmldomnode* pNode = pDoc != NULL && __xrtXmlDomValidName((const char*)sTarget) ?
		__xrtXmlDomCreateNode(pDoc->pDocument,
			XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE,
			(const char*)sTarget, (const char*)sText) : NULL;
	return xrtXmlDomNodeRetain(pNode);
}

XRT_API xrtxmldomnode* xrtXmlDomNodeRetain(xrtxmldomnode* pNode)
{
	if ( pNode == NULL || pNode->pDocument == NULL ||
		pNode->pDocument->iReferences == UINT32_MAX ) { return NULL; }
	pNode->pDocument->iReferences++;
	return pNode;
}

XRT_API void xrtXmlDomNodeRelease(xrtxmldomnode* pNode)
{
	xrtxmldomdocument* pDocument;
	if ( pNode == NULL || pNode->pDocument == NULL ) { return; }
	pDocument = pNode->pDocument;
	if ( pDocument->iReferences > 1u ) { pDocument->iReferences--; return; }
	__xrtXmlDomFreeDocument(pDocument);
}

XRT_API int xrtXmlDomNodeType(const xrtxmldomnode* pNode)
{
	return pNode != NULL ? pNode->iType : 0;
}

XRT_API str xrtXmlDomNodeTypeName(const xrtxmldomnode* pNode)
{
	if ( pNode == NULL ) { return NULL; }
	switch ( pNode->iType ) {
	case XRT_XML_DOM_ELEMENT_NODE: return (str)"element";
	case XRT_XML_DOM_TEXT_NODE: return (str)"text";
	case XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE: return (str)"processingInstruction";
	case XRT_XML_DOM_COMMENT_NODE: return (str)"comment";
	case XRT_XML_DOM_DOCUMENT_NODE: return (str)"document";
	default: return (str)"unknown";
	}
}

XRT_API str xrtXmlDomNodeName(const xrtxmldomnode* pNode) { return pNode != NULL ? pNode->sName : NULL; }
XRT_API str xrtXmlDomNodeLocalName(const xrtxmldomnode* pNode) { return pNode != NULL ? pNode->sLocalName : NULL; }
XRT_API str xrtXmlDomNodeNamespaceUri(const xrtxmldomnode* pNode) { return pNode != NULL ? pNode->sNamespaceUri : NULL; }
XRT_API str xrtXmlDomNodePrefix(const xrtxmldomnode* pNode) { return pNode != NULL ? pNode->sPrefix : NULL; }
XRT_API str xrtXmlDomNodeValue(const xrtxmldomnode* pNode) { return pNode != NULL ? pNode->sValue : NULL; }

XRT_API bool xrtXmlDomNodeSetValue(xrtxmldomnode* pNode, str sValue)
{
	str sCopy;
	if ( pNode == NULL || pNode->iType == XRT_XML_DOM_DOCUMENT_NODE ||
		pNode->iType == XRT_XML_DOM_ELEMENT_NODE ) { return false; }
	sCopy = __xrtXmlDomCopy((const char*)sValue);
	if ( sCopy == NULL ) { return false; }
	xrtFree(pNode->sValue);
	pNode->sValue = sCopy;
	return true;
}

XRT_API xrtxmldomnode* xrtXmlDomNodeParent(const xrtxmldomnode* pNode)
{
	return pNode != NULL ? xrtXmlDomNodeRetain(pNode->pParent) : NULL;
}

XRT_API uint32 xrtXmlDomNodeChildCount(const xrtxmldomnode* pNode)
{
	return pNode != NULL ? pNode->iChildCount : 0u;
}

XRT_API xrtxmldomnode* xrtXmlDomNodeChild(const xrtxmldomnode* pNode,
	uint32 iIndex)
{
	return pNode != NULL && iIndex < pNode->iChildCount ?
		xrtXmlDomNodeRetain(pNode->pChildren[iIndex]) : NULL;
}

XRT_API xrtxmldomnode* xrtXmlDomDocumentElement(const xrtxmldomnode* pDocument)
{
	uint32 i;
	if ( pDocument == NULL || pDocument->iType != XRT_XML_DOM_DOCUMENT_NODE ) {
		return NULL;
	}
	for ( i = 0u; i < pDocument->iChildCount; i++ ) {
		if ( pDocument->pChildren[i]->iType == XRT_XML_DOM_ELEMENT_NODE ) {
			return xrtXmlDomNodeRetain(pDocument->pChildren[i]);
		}
	}
	return NULL;
}

static bool __xrtXmlDomIsAncestor(const xrtxmldomnode* pNode,
	const xrtxmldomnode* pPossibleAncestor)
{
	while ( pNode != NULL ) {
		if ( pNode == pPossibleAncestor ) { return true; }
		pNode = pNode->pParent;
	}
	return false;
}

XRT_API bool xrtXmlDomRemoveChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild)
{
	uint32 i;
	if ( pParent == NULL || pChild == NULL || pChild->pParent != pParent ) {
		return false;
	}
	for ( i = 0u; i < pParent->iChildCount; i++ ) {
		if ( pParent->pChildren[i] == pChild ) {
			if ( i + 1u < pParent->iChildCount ) {
				memmove(&pParent->pChildren[i], &pParent->pChildren[i + 1u],
					sizeof(xrtxmldomnode*) * (pParent->iChildCount - i - 1u));
			}
			pParent->iChildCount--;
			pChild->pParent = NULL;
			return true;
		}
	}
	return false;
}

XRT_API bool xrtXmlDomAppendChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild)
{
	xrtxmldomnode** pItems;
	uint32 iCapacity;
	uint32 i;
	if ( pParent == NULL || pChild == NULL ||
		pParent->pDocument != pChild->pDocument ||
		pParent->iType == XRT_XML_DOM_TEXT_NODE ||
		pParent->iType == XRT_XML_DOM_COMMENT_NODE ||
		pParent->iType == XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE ||
		__xrtXmlDomIsAncestor(pParent, pChild) ) { return false; }
	if ( pParent->iType == XRT_XML_DOM_DOCUMENT_NODE &&
		pChild->iType == XRT_XML_DOM_ELEMENT_NODE ) {
		for ( i = 0u; i < pParent->iChildCount; i++ ) {
			if ( pParent->pChildren[i] != pChild &&
				pParent->pChildren[i]->iType == XRT_XML_DOM_ELEMENT_NODE ) {
				return false;
			}
		}
	}
	/* 先完成目标扩容，失败时不得改变原树。 */
	if ( pChild->pParent != pParent &&
		pParent->iChildCount == pParent->iChildCapacity ) {
		iCapacity = pParent->iChildCapacity != 0u ?
			pParent->iChildCapacity * 2u : 4u;
		pItems = (xrtxmldomnode**)xrtRealloc(pParent->pChildren,
			sizeof(xrtxmldomnode*) * iCapacity);
		if ( pItems == NULL ) { return false; }
		pParent->pChildren = pItems;
		pParent->iChildCapacity = iCapacity;
	}
	if ( pChild->pParent != NULL &&
		!xrtXmlDomRemoveChild(pChild->pParent, pChild) ) { return false; }
	pParent->pChildren[pParent->iChildCount++] = pChild;
	pChild->pParent = pParent;
	return true;
}

typedef struct {
	const xrtxmldomnode* pNode;
	uint32 iNextChild;
} xrtxmldomtextframe;

static bool __xrtXmlDomTextContentAppend(const xrtxmldomnode* pNode,
	xbuffer* pBuffer)
{
	xrtxmldomtextframe* pStack = (xrtxmldomtextframe*)xrtMalloc(
		32u * sizeof(*pStack));
	size_t iCapacity = 32u;
	size_t iDepth = 1u;
	bool bOk = pStack != NULL;
	if ( !bOk ) { return false; }
	pStack[0] = (xrtxmldomtextframe){ pNode, 0u };
	while ( iDepth != 0u ) {
		xrtxmldomtextframe* pFrame = &pStack[iDepth - 1u];
		if ( pFrame->pNode->iType == XRT_XML_DOM_TEXT_NODE ) {
			size_t iLength = strlen((const char*)pFrame->pNode->sValue);
			if ( iLength != 0u && !xrtBufferAppend(pBuffer,
				(xbytesview){ (cbytes)pFrame->pNode->sValue, iLength }) ) {
				bOk = false;
				break;
			}
			iDepth--;
		} else if ( pFrame->iNextChild < pFrame->pNode->iChildCount ) {
			const xrtxmldomnode* pChild =
				pFrame->pNode->pChildren[pFrame->iNextChild++];
			if ( iDepth == iCapacity ) {
				xrtxmldomtextframe* pNew;
				if ( iCapacity > SIZE_MAX / (2u * sizeof(*pStack)) ) {
					bOk = false;
					break;
				}
				pNew = (xrtxmldomtextframe*)xrtRealloc(pStack,
					2u * iCapacity * sizeof(*pStack));
				if ( pNew == NULL ) { bOk = false; break; }
				pStack = pNew;
				iCapacity *= 2u;
			}
			pStack[iDepth++] = (xrtxmldomtextframe){ pChild, 0u };
		} else { iDepth--; }
	}
	xrtFree(pStack);
	return bOk;
}

XRT_API str xrtXmlDomTextContent(const xrtxmldomnode* pNode)
{
	xbuffer* pBuffer;
	str sResult;
	if ( pNode == NULL ) { return NULL; }
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL || !__xrtXmlDomTextContentAppend(pNode, pBuffer) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	sResult = xrtStrDupN((const char*)pBuffer->Data, pBuffer->Size);
	xrtBufferDestroy(pBuffer);
	return sResult;
}

static int __xrtXmlDomAttributeIndex(const xrtxmldomnode* pNode,
	const char* sName)
{
	uint32 i;
	if ( pNode == NULL || sName == NULL ) { return -1; }
	for ( i = 0u; i < pNode->iAttributeCount; i++ ) {
		if ( strcmp((const char*)pNode->pAttributes[i].sName, sName) == 0 ) {
			return (int)i;
		}
	}
	return -1;
}

XRT_API uint32 xrtXmlDomAttributeCount(const xrtxmldomnode* pNode)
{
	return pNode != NULL ? pNode->iAttributeCount : 0u;
}

XRT_API str xrtXmlDomAttributeName(const xrtxmldomnode* pNode, uint32 iIndex)
{
	return pNode != NULL && iIndex < pNode->iAttributeCount ?
		pNode->pAttributes[iIndex].sName : NULL;
}

XRT_API str xrtXmlDomAttributeValue(const xrtxmldomnode* pNode, uint32 iIndex)
{
	return pNode != NULL && iIndex < pNode->iAttributeCount ?
		pNode->pAttributes[iIndex].sValue : NULL;
}

XRT_API str xrtXmlDomGetAttribute(const xrtxmldomnode* pNode, str sName)
{
	int i = __xrtXmlDomAttributeIndex(pNode, (const char*)sName);
	return i >= 0 ? pNode->pAttributes[i].sValue : NULL;
}

static bool __xrtXmlDomSetString(str* psTarget, const char* sText)
{
	str sCopy = __xrtXmlDomCopy(sText);
	if ( sCopy == NULL ) { return false; }
	xrtFree(*psTarget);
	*psTarget = sCopy;
	return true;
}

XRT_API bool xrtXmlDomSetAttribute(xrtxmldomnode* pNode, str sName, str sValue)
{
	int iIndex;
	xrtxmlattribute* pItems;
	xrtxmlattribute* pItem;
	uint32 iCapacity;
	if ( pNode == NULL || pNode->iType != XRT_XML_DOM_ELEMENT_NODE ||
		!__xrtXmlDomValidName((const char*)sName) ) { return false; }
	iIndex = __xrtXmlDomAttributeIndex(pNode, (const char*)sName);
	if ( iIndex >= 0 ) {
		return __xrtXmlDomSetString(&pNode->pAttributes[iIndex].sValue,
			(const char*)sValue);
	}
	if ( pNode->iAttributeCount == pNode->iAttributeCapacity ) {
		iCapacity = pNode->iAttributeCapacity != 0u ?
			pNode->iAttributeCapacity * 2u : 4u;
		pItems = (xrtxmlattribute*)xrtRealloc(pNode->pAttributes,
			sizeof(xrtxmlattribute) * iCapacity);
		if ( pItems == NULL ) { return false; }
		memset(pItems + pNode->iAttributeCapacity, 0,
			sizeof(xrtxmlattribute) * (iCapacity - pNode->iAttributeCapacity));
		pNode->pAttributes = pItems;
		pNode->iAttributeCapacity = iCapacity;
	}
	pItem = &pNode->pAttributes[pNode->iAttributeCount];
	pItem->sName = __xrtXmlDomCopy((const char*)sName);
	pItem->sLocalName = __xrtXmlDomCopy((const char*)sName);
	pItem->sNamespaceUri = __xrtXmlDomCopy("");
	pItem->sPrefix = __xrtXmlDomCopy("");
	pItem->sValue = __xrtXmlDomCopy((const char*)sValue);
	if ( pItem->sName == NULL || pItem->sLocalName == NULL ||
		pItem->sNamespaceUri == NULL || pItem->sPrefix == NULL ||
		pItem->sValue == NULL ) {
		xrtFree(pItem->sName); xrtFree(pItem->sLocalName);
		xrtFree(pItem->sNamespaceUri); xrtFree(pItem->sPrefix);
		xrtFree(pItem->sValue);
		memset(pItem, 0, sizeof(*pItem));
		return false;
	}
	pNode->iAttributeCount++;
	return true;
}

XRT_API bool xrtXmlDomRemoveAttribute(xrtxmldomnode* pNode, str sName)
{
	int iIndex = __xrtXmlDomAttributeIndex(pNode, (const char*)sName);
	xrtxmlattribute* pItem;
	if ( iIndex < 0 ) { return false; }
	pItem = &pNode->pAttributes[iIndex];
	xrtFree(pItem->sName); xrtFree(pItem->sLocalName);
	xrtFree(pItem->sNamespaceUri); xrtFree(pItem->sPrefix);
	xrtFree(pItem->sValue);
	if ( (uint32)iIndex + 1u < pNode->iAttributeCount ) {
		memmove(pItem, pItem + 1,
			sizeof(xrtxmlattribute) * (pNode->iAttributeCount -
			(uint32)iIndex - 1u));
	}
	pNode->iAttributeCount--;
	memset(&pNode->pAttributes[pNode->iAttributeCount], 0,
		sizeof(xrtxmlattribute));
	return true;
}
