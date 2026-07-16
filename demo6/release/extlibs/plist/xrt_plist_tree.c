#include "xrt_plist_internal.h"



static bool __xrtPlistTrackNode(xrtplistdocument* pDocument,
	xrtplistnode* pNode)
{
	xrtplistnode** pItems;
	uint32 iCapacity;
	if ( pDocument->iNodeCount == pDocument->iNodeCapacity ) {
		iCapacity = pDocument->iNodeCapacity != 0u ?
			pDocument->iNodeCapacity * 2u : 32u;
		pItems = (xrtplistnode**)xrtRealloc(pDocument->pNodes,
			sizeof(xrtplistnode*) * iCapacity);
		if ( pItems == NULL ) { return FALSE; }
		pDocument->pNodes = pItems;
		pDocument->iNodeCapacity = iCapacity;
	}
	pDocument->pNodes[pDocument->iNodeCount++] = pNode;
	return TRUE;
}

xrtplistnode* __xrtPlistCreateNode(xrtplistdocument* pDocument, int iType)
{
	xrtplistnode* pNode;
	if ( pDocument == NULL ) { return NULL; }
	pNode = (xrtplistnode*)xrtMalloc(sizeof(*pNode));
	if ( pNode == NULL ) { return NULL; }
	memset(pNode, 0, sizeof(*pNode));
	pNode->pDocument = pDocument;
	pNode->iType = iType;
	if ( !__xrtPlistTrackNode(pDocument, pNode) ) {
		xrtFree(pNode);
		return NULL;
	}
	return pNode;
}

bool __xrtPlistSetBytes(xrtplistnode* pNode, const void* pData,
	uint64 iSize, bool bString)
{
	uint8* pCopy;
	if ( pNode == NULL || (pData == NULL && iSize != 0u) ||
		iSize > SIZE_MAX - (bString ? 1u : 0u) ) { return FALSE; }
	pCopy = (uint8*)xrtMalloc((size_t)iSize + (bString ? 1u : 0u));
	if ( pCopy == NULL && (iSize != 0u || bString) ) { return FALSE; }
	if ( iSize != 0u ) { memcpy(pCopy, pData, (size_t)iSize); }
	if ( bString ) { pCopy[iSize] = 0u; }
	xrtFree(pNode->Value.Bytes.pData);
	pNode->Value.Bytes.pData = pCopy;
	pNode->Value.Bytes.iSize = iSize;
	return TRUE;
}

XXAPI xrtplistdocument* xrtPlistCreate(void)
{
	xrtplistdocument* pDocument =
		(xrtplistdocument*)xrtMalloc(sizeof(*pDocument));
	if ( pDocument != NULL ) { memset(pDocument, 0, sizeof(*pDocument)); }
	return pDocument;
}

XXAPI void xrtPlistDestroy(xrtplistdocument* pDocument)
{
	uint32 i;
	if ( pDocument == NULL ) { return; }
	for ( i = 0u; i < pDocument->iNodeCount; i++ ) {
		xrtplistnode* pNode = pDocument->pNodes[i];
		uint32 j;
		if ( pNode->iType == XRT_PLIST_STRING || pNode->iType == XRT_PLIST_DATA ) {
			xrtFree(pNode->Value.Bytes.pData);
		} else if ( pNode->iType == XRT_PLIST_ARRAY ) {
			xrtFree(pNode->Value.Array.pItems);
		} else if ( pNode->iType == XRT_PLIST_DICT ) {
			for ( j = 0u; j < pNode->Value.Dict.iCount; j++ ) {
				xrtFree(pNode->Value.Dict.pItems[j].pKey);
			}
			xrtFree(pNode->Value.Dict.pItems);
		}
		xrtFree(pNode);
	}
	xrtFree(pDocument->pNodes);
	xrtFree(pDocument);
}

XXAPI xrtplistnode* xrtPlistRoot(const xrtplistdocument* pDocument)
{
	return pDocument != NULL ? pDocument->pRoot : NULL;
}

XXAPI bool xrtPlistSetRoot(xrtplistdocument* pDocument, xrtplistnode* pRoot)
{
	if ( pDocument == NULL || pRoot == NULL ||
		pRoot->pDocument != pDocument ) { return FALSE; }
	pDocument->pRoot = pRoot;
	return TRUE;
}

XXAPI xrtplistnode* xrtPlistCreateNull(xrtplistdocument* pDocument) { return __xrtPlistCreateNode(pDocument, XRT_PLIST_NULL); }
XXAPI xrtplistnode* xrtPlistCreateBool(xrtplistdocument* pDocument, bool bValue) { xrtplistnode* p = __xrtPlistCreateNode(pDocument, XRT_PLIST_BOOL); if ( p != NULL ) { p->Value.bValue = bValue; } return p; }
XXAPI xrtplistnode* xrtPlistCreateInt(xrtplistdocument* pDocument, int64 iValue) { xrtplistnode* p = __xrtPlistCreateNode(pDocument, XRT_PLIST_INT); if ( p != NULL ) { p->Value.iValue = iValue; } return p; }
XXAPI xrtplistnode* xrtPlistCreateReal(xrtplistdocument* pDocument, double fValue) { xrtplistnode* p = __xrtPlistCreateNode(pDocument, XRT_PLIST_REAL); if ( p != NULL ) { p->Value.fValue = fValue; } return p; }
XXAPI xrtplistnode* xrtPlistCreateDate(xrtplistdocument* pDocument, xtime tValue) { xrtplistnode* p = __xrtPlistCreateNode(pDocument, XRT_PLIST_DATE); if ( p != NULL ) { p->Value.tValue = tValue; } return p; }
XXAPI xrtplistnode* xrtPlistCreateArray(xrtplistdocument* pDocument) { return __xrtPlistCreateNode(pDocument, XRT_PLIST_ARRAY); }
XXAPI xrtplistnode* xrtPlistCreateDict(xrtplistdocument* pDocument) { return __xrtPlistCreateNode(pDocument, XRT_PLIST_DICT); }
XXAPI xrtplistnode* xrtPlistCreateUid(xrtplistdocument* pDocument, uint64 iValue) { xrtplistnode* p = __xrtPlistCreateNode(pDocument, XRT_PLIST_UID); if ( p != NULL ) { p->Value.iUid = iValue; } return p; }

XXAPI xrtplistnode* xrtPlistCreateStringEx(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize)
{
	xrtplistnode* pNode;
	if ( iSize > SIZE_MAX || (iSize != 0u &&
		(pValue == NULL || !xrtIsUTF8((str)pValue, (size_t)iSize))) ) { return NULL; }
	pNode = __xrtPlistCreateNode(pDocument, XRT_PLIST_STRING);
	return pNode != NULL && __xrtPlistSetBytes(pNode, pValue, iSize, TRUE) ?
		pNode : NULL;
}

XXAPI xrtplistnode* xrtPlistCreateString(xrtplistdocument* pDocument,
	str sValue)
{
	return sValue != NULL ? xrtPlistCreateStringEx(pDocument, sValue,
		strlen((const char*)sValue)) : NULL;
}

XXAPI xrtplistnode* xrtPlistCreateData(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize)
{
	xrtplistnode* pNode = __xrtPlistCreateNode(pDocument, XRT_PLIST_DATA);
	return pNode != NULL && __xrtPlistSetBytes(pNode, pValue, iSize, FALSE) ?
		pNode : NULL;
}

XXAPI int xrtPlistNodeType(const xrtplistnode* pNode) { return pNode != NULL ? pNode->iType : -1; }
XXAPI bool xrtPlistBool(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_BOOL ? pNode->Value.bValue : FALSE; }
XXAPI int64 xrtPlistInt(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_INT ? pNode->Value.iValue : 0; }
XXAPI double xrtPlistReal(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_REAL ? pNode->Value.fValue : 0.0; }
XXAPI xtime xrtPlistDate(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_DATE ? pNode->Value.tValue : 0; }
XXAPI uint64 xrtPlistUid(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_UID ? pNode->Value.iUid : 0u; }
XXAPI const void* xrtPlistBytes(const xrtplistnode* pNode) { return pNode != NULL && (pNode->iType == XRT_PLIST_STRING || pNode->iType == XRT_PLIST_DATA) ? pNode->Value.Bytes.pData : NULL; }
XXAPI uint64 xrtPlistByteSize(const xrtplistnode* pNode) { return pNode != NULL && (pNode->iType == XRT_PLIST_STRING || pNode->iType == XRT_PLIST_DATA) ? pNode->Value.Bytes.iSize : 0u; }

XXAPI uint32 xrtPlistArrayCount(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_ARRAY ? pNode->Value.Array.iCount : 0u; }
XXAPI xrtplistnode* xrtPlistArrayGet(const xrtplistnode* pNode, uint32 iIndex) { return pNode != NULL && pNode->iType == XRT_PLIST_ARRAY && iIndex < pNode->Value.Array.iCount ? pNode->Value.Array.pItems[iIndex] : NULL; }

XXAPI bool xrtPlistArrayAppend(xrtplistnode* pNode, xrtplistnode* pValue)
{
	xrtplistnode** pItems;
	uint32 iCapacity;
	if ( pNode == NULL || pValue == NULL || pNode->iType != XRT_PLIST_ARRAY ||
		pNode->pDocument != pValue->pDocument ) { return FALSE; }
	if ( pNode->Value.Array.iCount == pNode->Value.Array.iCapacity ) {
		iCapacity = pNode->Value.Array.iCapacity != 0u ?
			pNode->Value.Array.iCapacity * 2u : 4u;
		pItems = (xrtplistnode**)xrtRealloc(pNode->Value.Array.pItems,
			sizeof(xrtplistnode*) * iCapacity);
		if ( pItems == NULL ) { return FALSE; }
		pNode->Value.Array.pItems = pItems;
		pNode->Value.Array.iCapacity = iCapacity;
	}
	pNode->Value.Array.pItems[pNode->Value.Array.iCount++] = pValue;
	return TRUE;
}

XXAPI uint32 xrtPlistDictCount(const xrtplistnode* pNode) { return pNode != NULL && pNode->iType == XRT_PLIST_DICT ? pNode->Value.Dict.iCount : 0u; }
XXAPI const void* xrtPlistDictKey(const xrtplistnode* pNode, uint32 iIndex) { return pNode != NULL && pNode->iType == XRT_PLIST_DICT && iIndex < pNode->Value.Dict.iCount ? pNode->Value.Dict.pItems[iIndex].pKey : NULL; }
XXAPI uint64 xrtPlistDictKeySize(const xrtplistnode* pNode, uint32 iIndex) { return pNode != NULL && pNode->iType == XRT_PLIST_DICT && iIndex < pNode->Value.Dict.iCount ? pNode->Value.Dict.pItems[iIndex].iKeySize : 0u; }
XXAPI xrtplistnode* xrtPlistDictValue(const xrtplistnode* pNode, uint32 iIndex) { return pNode != NULL && pNode->iType == XRT_PLIST_DICT && iIndex < pNode->Value.Dict.iCount ? pNode->Value.Dict.pItems[iIndex].pValue : NULL; }

static int __xrtPlistDictIndex(const xrtplistnode* pNode,
	const void* pKey, uint64 iKeySize)
{
	uint32 i;
	if ( pNode == NULL || pNode->iType != XRT_PLIST_DICT ||
		(pKey == NULL && iKeySize != 0u) ) { return -1; }
	for ( i = 0u; i < pNode->Value.Dict.iCount; i++ ) {
		const xrtplistdictitem* pItem = &pNode->Value.Dict.pItems[i];
		if ( pItem->iKeySize == iKeySize &&
			(iKeySize == 0u || memcmp(pItem->pKey, pKey, (size_t)iKeySize) == 0) ) {
			return (int)i;
		}
	}
	return -1;
}

XXAPI xrtplistnode* xrtPlistDictGet(const xrtplistnode* pNode,
	const void* pKey, uint64 iKeySize)
{
	int i = __xrtPlistDictIndex(pNode, pKey, iKeySize);
	return i >= 0 ? pNode->Value.Dict.pItems[i].pValue : NULL;
}

XXAPI bool xrtPlistDictSet(xrtplistnode* pNode, const void* pKey,
	uint64 iKeySize, xrtplistnode* pValue)
{
	int iIndex;
	xrtplistdictitem* pItems;
	uint8* pCopy;
	uint32 iCapacity;
	if ( pNode == NULL || pValue == NULL || pNode->iType != XRT_PLIST_DICT ||
		pNode->pDocument != pValue->pDocument || iKeySize > SIZE_MAX ||
		(pKey == NULL && iKeySize != 0u) ||
		(iKeySize != 0u && !xrtIsUTF8((str)pKey, (size_t)iKeySize)) ) { return FALSE; }
	iIndex = __xrtPlistDictIndex(pNode, pKey, iKeySize);
	if ( iIndex >= 0 ) {
		pNode->Value.Dict.pItems[iIndex].pValue = pValue;
		return TRUE;
	}
	pCopy = (uint8*)xrtMalloc((size_t)iKeySize + 1u);
	if ( pCopy == NULL ) { return FALSE; }
	if ( iKeySize != 0u ) { memcpy(pCopy, pKey, (size_t)iKeySize); }
	pCopy[iKeySize] = 0u;
	if ( pNode->Value.Dict.iCount == pNode->Value.Dict.iCapacity ) {
		iCapacity = pNode->Value.Dict.iCapacity != 0u ?
			pNode->Value.Dict.iCapacity * 2u : 4u;
		pItems = (xrtplistdictitem*)xrtRealloc(pNode->Value.Dict.pItems,
			sizeof(xrtplistdictitem) * iCapacity);
		if ( pItems == NULL ) { xrtFree(pCopy); return FALSE; }
		pNode->Value.Dict.pItems = pItems;
		pNode->Value.Dict.iCapacity = iCapacity;
	}
	pNode->Value.Dict.pItems[pNode->Value.Dict.iCount].pKey = pCopy;
	pNode->Value.Dict.pItems[pNode->Value.Dict.iCount].iKeySize = iKeySize;
	pNode->Value.Dict.pItems[pNode->Value.Dict.iCount].pValue = pValue;
	pNode->Value.Dict.iCount++;
	return TRUE;
}
