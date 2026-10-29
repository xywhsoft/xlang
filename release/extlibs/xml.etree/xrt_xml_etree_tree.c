#include "xrt_xml_etree_internal.h"



str __xrtXmlEtreeCopy(const char* sText)
{
	return xrtStrDup(sText != NULL ? sText : "");
}

void __xrtXmlEtreeSetError(xrtxmlerror* pError, int iCode,
	const char* sMessage)
{
	if ( pError == NULL ) { return; }
	xrtXmlErrorInit(pError);
	pError->iCode = iCode;
	snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
		sMessage != NULL ? sMessage : "XML ElementTree operation failed");
}

static bool __xrtXmlEtreeValidName(const char* sName)
{
	return xrtXmlNameValid((str)sName);
}

xrtxmlelement* __xrtXmlEtreeCreateNode(xrtxmldocument* pDocument,
	int iKind, const char* sName)
{
	xrtxmlelement* pElement;
	if ( pDocument == NULL ||
		(iKind == XRT_XML_ELEMENT && !__xrtXmlEtreeValidName(sName)) ) {
		return NULL;
	}
	pElement = (xrtxmlelement*)xrtMalloc(sizeof(*pElement));
	if ( pElement == NULL ) { return NULL; }
	memset(pElement, 0, sizeof(*pElement));
	pElement->pDocument = pDocument;
	pElement->iKind = iKind;
	pElement->sName = __xrtXmlEtreeCopy(sName);
	pElement->sLocalName = __xrtXmlEtreeCopy(sName);
	pElement->sNamespaceUri = __xrtXmlEtreeCopy("");
	pElement->sPrefix = __xrtXmlEtreeCopy("");
	pElement->sText = __xrtXmlEtreeCopy("");
	pElement->sTail = __xrtXmlEtreeCopy("");
	if ( pElement->sName == NULL || pElement->sLocalName == NULL ||
		pElement->sNamespaceUri == NULL || pElement->sPrefix == NULL ||
		pElement->sText == NULL || pElement->sTail == NULL ) {
		__xrtXmlEtreeFreeNode(pElement);
		return NULL;
	}
	return pElement;
}

static void __xrtXmlEtreeFreeOne(xrtxmlelement* pElement)
{
	uint32 i;
	for ( i = 0u; i < pElement->iAttributeCount; i++ ) {
		xrtFree(pElement->pAttributes[i].sName);
		xrtFree(pElement->pAttributes[i].sLocalName);
		xrtFree(pElement->pAttributes[i].sNamespaceUri);
		xrtFree(pElement->pAttributes[i].sPrefix);
		xrtFree(pElement->pAttributes[i].sValue);
	}
	xrtFree(pElement->pChildren);
	xrtFree(pElement->pAttributes);
	xrtFree(pElement->sName);
	xrtFree(pElement->sLocalName);
	xrtFree(pElement->sNamespaceUri);
	xrtFree(pElement->sPrefix);
	xrtFree(pElement->sText);
	xrtFree(pElement->sTail);
	xrtFree(pElement);
}

void __xrtXmlEtreeFreeNode(xrtxmlelement* pElement)
{
	xrtxmlelement* pRoot = pElement;
	while ( pElement != NULL ) {
		xrtxmlelement* pParent;
		bool bRoot;
		if ( pElement->iChildCount != 0u ) {
			pElement = pElement->pChildren[--pElement->iChildCount];
			continue;
		}
		pParent = pElement->pParent;
		bRoot = pElement == pRoot;
		__xrtXmlEtreeFreeOne(pElement);
		pElement = bRoot ? NULL : pParent;
	}
}

bool __xrtXmlEtreeAppendNode(xrtxmlelement* pParent,
	xrtxmlelement* pChild)
{
	xrtxmlelement** pItems;
	uint32 iCapacity;
	if ( pParent == NULL || pChild == NULL ||
		pParent->pDocument != pChild->pDocument || pChild->pParent != NULL ) {
		return false;
	}
	if ( pParent->iChildCount == pParent->iChildCapacity ) {
		iCapacity = pParent->iChildCapacity != 0u ?
			pParent->iChildCapacity * 2u : 4u;
		pItems = (xrtxmlelement**)xrtRealloc(pParent->pChildren,
			sizeof(xrtxmlelement*) * iCapacity);
		if ( pItems == NULL ) { return false; }
		pParent->pChildren = pItems;
		pParent->iChildCapacity = iCapacity;
	}
	pChild->pParent = pParent;
	pParent->pChildren[pParent->iChildCount++] = pChild;
	return true;
}

bool __xrtXmlEtreeAppendText(str* psTarget, const char* sText)
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

XRT_API xrtxmlelement* xrtXmlElementCreate(str sName)
{
	xrtxmldocument* pDocument;
	xrtxmlelement* pRoot;
	if ( sName == NULL ) { return NULL; }
	pDocument = (xrtxmldocument*)xrtMalloc(sizeof(*pDocument));
	if ( pDocument == NULL ) { return NULL; }
	memset(pDocument, 0, sizeof(*pDocument));
	pDocument->iReferences = 1u;
	pRoot = __xrtXmlEtreeCreateNode(pDocument, XRT_XML_ELEMENT,
		(const char*)sName);
	if ( pRoot == NULL ) { xrtFree(pDocument); return NULL; }
	pDocument->pRoot = pRoot;
	return pRoot;
}

XRT_API xrtxmlelement* xrtXmlElementAppend(xrtxmlelement* pParent, str sName)
{
	xrtxmlelement* pChild;
	if ( pParent == NULL || sName == NULL ) { return NULL; }
	pChild = __xrtXmlEtreeCreateNode(pParent->pDocument, XRT_XML_ELEMENT,
		(const char*)sName);
	if ( pChild == NULL || !__xrtXmlEtreeAppendNode(pParent, pChild) ) {
		__xrtXmlEtreeFreeNode(pChild);
		return NULL;
	}
	return xrtXmlElementRetain(pChild);
}

XRT_API xrtxmlelement* xrtXmlElementRetain(xrtxmlelement* pElement)
{
	if ( pElement == NULL || pElement->pDocument == NULL ||
		pElement->pDocument->iReferences == UINT32_MAX ) { return NULL; }
	pElement->pDocument->iReferences++;
	return pElement;
}

XRT_API void xrtXmlElementRelease(xrtxmlelement* pElement)
{
	xrtxmldocument* pDocument;
	if ( pElement == NULL || pElement->pDocument == NULL ) { return; }
	pDocument = pElement->pDocument;
	if ( pDocument->iReferences > 1u ) { pDocument->iReferences--; return; }
	__xrtXmlEtreeFreeNode(pDocument->pRoot);
	xrtFree(pDocument);
}

XRT_API int xrtXmlElementKind(const xrtxmlelement* pElement)
{
	return pElement != NULL ? pElement->iKind : 0;
}

XRT_API const xrtxmlelement* xrtXmlElementParent(const xrtxmlelement* pElement)
{
	return pElement != NULL ? pElement->pParent : NULL;
}

XRT_API str xrtXmlElementName(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sName : NULL; }
XRT_API str xrtXmlElementLocalName(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sLocalName : NULL; }
XRT_API str xrtXmlElementNamespaceUri(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sNamespaceUri : NULL; }
XRT_API str xrtXmlElementPrefix(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sPrefix : NULL; }
XRT_API str xrtXmlElementText(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sText : NULL; }
XRT_API str xrtXmlElementTail(const xrtxmlelement* pElement) { return pElement != NULL ? pElement->sTail : NULL; }

static bool __xrtXmlEtreeSetString(str* psTarget, str sText)
{
	str sCopy = __xrtXmlEtreeCopy((const char*)(sText != NULL ? sText : (str)""));
	if ( sCopy == NULL ) { return false; }
	xrtFree(*psTarget);
	*psTarget = sCopy;
	return true;
}

XRT_API bool xrtXmlElementSetText(xrtxmlelement* pElement, str sText)
{
	return pElement != NULL && __xrtXmlEtreeSetString(&pElement->sText, sText);
}

XRT_API bool xrtXmlElementSetTail(xrtxmlelement* pElement, str sText)
{
	return pElement != NULL && __xrtXmlEtreeSetString(&pElement->sTail, sText);
}

XRT_API uint32 xrtXmlElementChildCount(const xrtxmlelement* pElement)
{
	return pElement != NULL ? pElement->iChildCount : 0u;
}

XRT_API xrtxmlelement* xrtXmlElementChild(const xrtxmlelement* pElement,
	uint32 iIndex)
{
	return pElement != NULL && iIndex < pElement->iChildCount ?
		xrtXmlElementRetain(pElement->pChildren[iIndex]) : NULL;
}

XRT_API uint32 xrtXmlElementAttributeCount(const xrtxmlelement* pElement)
{
	return pElement != NULL ? pElement->iAttributeCount : 0u;
}

XRT_API str xrtXmlElementAttributeName(const xrtxmlelement* pElement,
	uint32 iIndex)
{
	return pElement != NULL && iIndex < pElement->iAttributeCount ?
		pElement->pAttributes[iIndex].sName : NULL;
}

XRT_API str xrtXmlElementAttributeValue(const xrtxmlelement* pElement,
	uint32 iIndex)
{
	return pElement != NULL && iIndex < pElement->iAttributeCount ?
		pElement->pAttributes[iIndex].sValue : NULL;
}

static int __xrtXmlEtreeAttributeIndex(const xrtxmlelement* pElement,
	const char* sName)
{
	uint32 i;
	if ( pElement == NULL || sName == NULL ) { return -1; }
	for ( i = 0u; i < pElement->iAttributeCount; i++ ) {
		if ( strcmp((const char*)pElement->pAttributes[i].sName, sName) == 0 ) {
			return (int)i;
		}
	}
	return -1;
}

XRT_API str xrtXmlElementGetAttribute(const xrtxmlelement* pElement, str sName)
{
	int i = __xrtXmlEtreeAttributeIndex(pElement, (const char*)sName);
	return i >= 0 ? pElement->pAttributes[i].sValue : NULL;
}

XRT_API bool xrtXmlElementSetAttribute(xrtxmlelement* pElement, str sName,
	str sValue)
{
	int iIndex;
	xrtxmlattribute* pItems;
	xrtxmlattribute tItem = { 0 };
	uint32 iCapacity;
	if ( pElement == NULL || !__xrtXmlEtreeValidName((const char*)sName) ) {
		return false;
	}
	iIndex = __xrtXmlEtreeAttributeIndex(pElement, (const char*)sName);
	if ( iIndex >= 0 ) {
		return __xrtXmlEtreeSetString(&pElement->pAttributes[iIndex].sValue,
			sValue);
	}
	if ( pElement->iAttributeCount == pElement->iAttributeCapacity ) {
		iCapacity = pElement->iAttributeCapacity != 0u ?
			pElement->iAttributeCapacity * 2u : 4u;
		pItems = (xrtxmlattribute*)xrtRealloc(pElement->pAttributes,
			sizeof(xrtxmlattribute) * iCapacity);
		if ( pItems == NULL ) { return false; }
		memset(pItems + pElement->iAttributeCapacity, 0,
			sizeof(xrtxmlattribute) * (iCapacity - pElement->iAttributeCapacity));
		pElement->pAttributes = pItems;
		pElement->iAttributeCapacity = iCapacity;
	}
	/* 全部分配成功后再提交，失败时不留下不可见的半成品属性。 */
	tItem.sName = __xrtXmlEtreeCopy((const char*)sName);
	tItem.sLocalName = __xrtXmlEtreeCopy((const char*)sName);
	tItem.sNamespaceUri = __xrtXmlEtreeCopy("");
	tItem.sPrefix = __xrtXmlEtreeCopy("");
	tItem.sValue = __xrtXmlEtreeCopy((const char*)(sValue != NULL ? sValue : (str)""));
	if ( tItem.sName == NULL || tItem.sLocalName == NULL ||
		tItem.sNamespaceUri == NULL || tItem.sPrefix == NULL ||
		tItem.sValue == NULL ) {
		xrtFree(tItem.sName);
		xrtFree(tItem.sLocalName);
		xrtFree(tItem.sNamespaceUri);
		xrtFree(tItem.sPrefix);
		xrtFree(tItem.sValue);
		return false;
	}
	pElement->pAttributes[pElement->iAttributeCount] = tItem;
	pElement->iAttributeCount++;
	return true;
}

XRT_API bool xrtXmlElementRemoveAttribute(xrtxmlelement* pElement, str sName)
{
	int iIndex = __xrtXmlEtreeAttributeIndex(pElement, (const char*)sName);
	xrtxmlattribute* pItem;
	if ( iIndex < 0 ) { return false; }
	pItem = &pElement->pAttributes[iIndex];
	xrtFree(pItem->sName); xrtFree(pItem->sLocalName);
	xrtFree(pItem->sNamespaceUri); xrtFree(pItem->sPrefix);
	xrtFree(pItem->sValue);
	if ( (uint32)iIndex + 1u < pElement->iAttributeCount ) {
		memmove(pItem, pItem + 1,
			sizeof(xrtxmlattribute) * (pElement->iAttributeCount -
			(uint32)iIndex - 1u));
	}
	pElement->iAttributeCount--;
	memset(&pElement->pAttributes[pElement->iAttributeCount], 0,
		sizeof(xrtxmlattribute));
	return true;
}

static bool __xrtXmlEtreeNameMatches(const xrtxmlelement* pElement,
	const char* sName, size_t iSize)
{
	return pElement != NULL && (iSize == 1u && sName[0] == '*' ? true :
		strlen((const char*)pElement->sName) == iSize &&
		memcmp(pElement->sName, sName, iSize) == 0);
}

typedef struct {
	const xrtxmlelement* pElement;
	uint32 iNextChild;
} xrtxmletreewalkframe;

/* 同一条前序遍历供 find 和 findAll 使用，结果次序保持文档顺序。 */
static bool __xrtXmlEtreeWalk(const xrtxmlelement* pRoot,
	const char* sName, size_t iSize, bool bRecursive,
	xrtxmlelement** ppFirst, xvalue* pMatches)
{
	xrtxmletreewalkframe* pStack = (xrtxmletreewalkframe*)xrtMalloc(
		32u * sizeof(*pStack));
	size_t iCapacity = 32u;
	size_t iDepth = 1u;
	bool bOk = pStack != NULL;
	if ( !bOk ) { return false; }
	pStack[0] = (xrtxmletreewalkframe){ pRoot, 0u };
	while ( iDepth != 0u ) {
		xrtxmletreewalkframe* pFrame = &pStack[iDepth - 1u];
		xrtxmlelement* pChild;
		if ( pFrame->iNextChild == pFrame->pElement->iChildCount ) {
			iDepth--;
			continue;
		}
		pChild = pFrame->pElement->pChildren[pFrame->iNextChild++];
		if ( __xrtXmlEtreeNameMatches(pChild, sName, iSize) ) {
			if ( ppFirst != NULL ) { *ppFirst = pChild; break; }
			if ( pMatches != NULL ) {
				xrtxmlelement* pRetained = xrtXmlElementRetain(pChild);
			xvalue* pValue = pRetained != NULL ?
				xrtValuePointer(pRetained) : NULL;
				if ( pValue == NULL ||
					!xrtValueArrayAppendTake(pMatches, &pValue) ) {
					xrtValueRelease(pValue);
					xrtXmlElementRelease(pRetained);
					bOk = false;
					break;
				}
			}
		}
		if ( !bRecursive || pChild->iChildCount == 0u ) { continue; }
		if ( iDepth == iCapacity ) {
			xrtxmletreewalkframe* pNew;
			if ( iCapacity > SIZE_MAX / (2u * sizeof(*pStack)) ) {
				bOk = false;
				break;
			}
			pNew = (xrtxmletreewalkframe*)xrtRealloc(pStack,
				2u * iCapacity * sizeof(*pStack));
			if ( pNew == NULL ) { bOk = false; break; }
			pStack = pNew;
			iCapacity *= 2u;
		}
		pStack[iDepth++] = (xrtxmletreewalkframe){ pChild, 0u };
	}
	xrtFree(pStack);
	return bOk;
}

XRT_API xrtxmlelement* xrtXmlElementFind(const xrtxmlelement* pElement,
	str sPath)
{
	const char* p = (const char*)sPath;
	const xrtxmlelement* pCurrent = pElement;
	if ( pCurrent == NULL || p == NULL || *p == 0 ) { return NULL; }
	if ( p[0] == '/' && p[1] == '/' ) {
		xrtxmlelement* pFound = NULL;
		p += 2;
		if ( !__xrtXmlEtreeWalk(pCurrent, p, strlen(p), true,
			&pFound, NULL) ) { return NULL; }
		return xrtXmlElementRetain(pFound);
	}
	while ( *p != 0 ) {
		const char* pEnd = strchr(p, '/');
		size_t iSize = pEnd != NULL ? (size_t)(pEnd - p) : strlen(p);
		uint32 i;
		const xrtxmlelement* pNext = NULL;
		if ( iSize == 1u && p[0] == '.' ) {
			pNext = pCurrent;
		} else {
			for ( i = 0u; i < pCurrent->iChildCount; i++ ) {
				if ( __xrtXmlEtreeNameMatches(pCurrent->pChildren[i], p, iSize) ) {
					pNext = pCurrent->pChildren[i];
					break;
				}
			}
		}
		if ( pNext == NULL ) { return NULL; }
		pCurrent = pNext;
		p = pEnd != NULL ? pEnd + 1 : p + iSize;
	}
	return xrtXmlElementRetain((xrtxmlelement*)pCurrent);
}

static void __xrtXmlEtreeReleaseMatches(xvalue* pMatches)
{
	size_t i;
	for ( i = 0u; i < xrtValueCount(pMatches); i++ ) {
		ptr pHandle = NULL;
		if ( xrtValueGetPointer(xrtValueArrayGet(pMatches, i), &pHandle) ) {
			xrtXmlElementRelease((xrtxmlelement*)pHandle);
		}
	}
	xrtValueRelease(pMatches);
}

XRT_API xvalue* xrtXmlElementFindAll(const xrtxmlelement* pElement, str sPath)
{
	const char* p = (const char*)sPath;
	bool bRecursive;
	xvalue* pResult;
	if ( pElement == NULL || p == NULL || *p == 0 ) { return NULL; }
	bRecursive = p[0] == '/' && p[1] == '/';
	if ( bRecursive ) { p += 2; }
	if ( strchr(p, '/') != NULL ) { return NULL; }
	pResult = xrtValueArray();
	if ( pResult == NULL ) { return NULL; }
	if ( !__xrtXmlEtreeWalk(pElement, p, strlen(p), bRecursive,
		NULL, pResult) ) {
		__xrtXmlEtreeReleaseMatches(pResult);
		return NULL;
	}
	return pResult;
}
