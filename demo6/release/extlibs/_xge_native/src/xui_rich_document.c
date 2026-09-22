#include "xui_internal.h"
#include "xui_json_internal.h"
#include "xui_value_internal.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_RICH_DOCUMENT_MAGIC 0x58524443u
#define XUI_RICH_NODE_MAGIC 0x5852444eu
#define XUI_RICH_HISTORY_LIMIT 64
#define XUI_RICH_HISTORY_SNAPSHOT 1
#define XUI_RICH_HISTORY_TEXT_REPLACE 2
#define XUI_RICH_HISTORY_BLOCK_REPLACE 3
#define XUI_RICH_OBJECT_TEXT "\xef\xbf\xbc"
#define XUI_RICH_OBJECT_SIZE 3
#define XUI_RICH_SERIAL_FORMAT "xui-rich-document"
#define XUI_RICH_SERIAL_VERSION 1
#define XUI_RICH_SERIAL_MAX_DEPTH 32
#define XUI_RICH_SERIAL_MAX_NODES 1000000
#define XUI_RICH_SERIAL_MAX_INPUT ((size_t)256 * 1024u * 1024u)

typedef struct xui_rich_history_t {
	int iKind;
	xui_rich_change_t tChange;
	xui_rich_node pRoot;
	xui_rich_node pOldBlocks;
	xui_rich_node pNewBlocks;
	uint64_t iNextId;
	xui_document_node_id_t iBeforeBlockId;
	xui_document_node_id_t iAfterBlockId;
	xui_document_node_id_t iNodeId;
	int iNodeOffset;
	char* sOldText;
	int iOldSize;
	char* sNewText;
	int iNewSize;
} xui_rich_history_t;

typedef struct xui_rich_observer_t {
	xui_internal_rich_change_proc onChange;
	void* pUser;
} xui_rich_observer_t;

struct xui_rich_node_t {
	uint32_t iMagic;
	xui_document_node_id_t iId;
	int iType;
	xui_rich_node pParent;
	xui_rich_node pFirstChild;
	xui_rich_node pLastChild;
	xui_rich_node pPrev;
	xui_rich_node pNext;
	char* sText;
	int iTextSize;
	char* sResource;
	char* sAltText;
	xui_widget pWidget;
	xui_surface pSurface;
	float fWidth;
	float fHeight;
	float fBaseline;
	xui_rich_text_style_t tStyle;
	xui_rich_paragraph_style_t tParagraphStyle;
	int iRows;
	int iColumns;
	xui_rich_document* ppCellDocuments;
	float fCellPadding;
	float fBorderWidth;
	uint32_t iBorderColor;
	uint32_t iHeaderColor;
	uint32_t iCellColor;
};

struct xui_rich_document_t {
	uint32_t iMagic;
	xui_rich_node pRoot;
	uint64_t iNextId;
	uint32_t iVersion;
	char* sFlatText;
	int iFlatCapacity;
	int iFlatLength;
	int bFlatDirty;
	xui_rich_history_t arrUndo[XUI_RICH_HISTORY_LIMIT];
	xui_rich_history_t arrRedo[XUI_RICH_HISTORY_LIMIT];
	int iUndoCount;
	int iRedoCount;
	int iPendingSnapshot;
	int bApplyingHistory;
	int iTransactionDepth;
	int bTransactionSnapshot;
	int bTransactionChanged;
	xui_rich_change_t tPendingChange;
	xui_rich_change_t tTransactionChange;
	xui_rich_change_t tLastChange;
	xui_rich_observer_t* pObservers;
	int iObserverCount;
	int iObserverCapacity;
	int bDestroyPending;
	xui_rich_document pOwnerDocument;
	xui_document_node_id_t iOwnerTableId;
};

typedef struct xui_rich_builder_t {
	xui_rich_document pDocument;
	xui_rich_node pRoot;
	xui_rich_node pParagraph;
} xui_rich_builder_t;

static xui_rich_node __xuiRichFindNode(xui_rich_node pNode, uint64_t iId);
static int __xuiRichReplaceRoot(xui_rich_document pDocument, xui_rich_node pRoot);
static void __xuiRichChanged(xui_rich_document pDocument);
static void __xuiRichNodeAssignFreshIds(xui_rich_document pDocument, xui_rich_node pNode,
	xui_rich_node pExtraRoot);
static xui_rich_document __xuiRichDocumentClone(xui_rich_document pSource,
	xui_rich_document pOwnerDocument, xui_document_node_id_t iOwnerTableId);

static int __xuiRichDocumentValid(xui_rich_document pDocument)
{
	return pDocument != NULL && pDocument->iMagic == XUI_RICH_DOCUMENT_MAGIC;
}

static int __xuiRichNodeValid(xui_rich_node pNode)
{
	return pNode != NULL && pNode->iMagic == XUI_RICH_NODE_MAGIC;
}

static xui_document_node_id_t __xuiRichFreshIdAvoid(xui_rich_document pDocument, xui_rich_node pExtraRoot)
{
	while ( (pDocument->pRoot != NULL && __xuiRichFindNode(pDocument->pRoot, pDocument->iNextId) != NULL) ||
	        (pExtraRoot != NULL && __xuiRichFindNode(pExtraRoot, pDocument->iNextId) != NULL) )
		pDocument->iNextId++;
	return pDocument->iNextId++;
}

static xui_document_node_id_t __xuiRichFreshId(xui_rich_document pDocument)
{
	return __xuiRichFreshIdAvoid(pDocument, NULL);
}

static int __xuiRichTextBlockType(int iType)
{
	return iType == XUI_RICH_NODE_PARAGRAPH || iType == XUI_RICH_NODE_HEADING ||
	       iType == XUI_RICH_NODE_BLOCK_QUOTE || iType == XUI_RICH_NODE_LIST_ITEM;
}

static int __xuiRichInlineTextType(int iType)
{
	return iType == XUI_RICH_NODE_TEXT || iType == XUI_RICH_NODE_LINK;
}

static int __xuiRichAtomicBlockType(int iType)
{
	return iType == XUI_RICH_NODE_IMAGE || iType == XUI_RICH_NODE_TABLE ||
	       iType == XUI_RICH_NODE_HORIZONTAL_RULE;
}

static char* __xuiRichString(const char* sText)
{
	char* sCopy;
	size_t iSize;
	if ( sText == NULL ) return NULL;
	iSize = strlen(sText);
	sCopy = (char*)xrtMalloc(iSize + 1u);
	if ( sCopy != NULL ) memcpy(sCopy, sText, iSize + 1u);
	return sCopy;
}

static int __xuiRichStyleSame(const xui_rich_text_style_t* pA, const xui_rich_text_style_t* pB)
{
	return pA->pFont == pB->pFont && pA->iTextColor == pB->iTextColor &&
	       pA->iBackgroundColor == pB->iBackgroundColor && pA->iFlags == pB->iFlags &&
	       pA->fBaselineShift == pB->fBaselineShift && pA->iWeight == pB->iWeight &&
	       pA->iSlant == pB->iSlant && pA->fFontSize == pB->fFontSize;
}

static xui_rich_paragraph_style_t __xuiRichParagraphStyle(const xui_rich_paragraph_style_t* pStyle)
{
	xui_rich_paragraph_style_t tStyle;
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = sizeof(tStyle);
	tStyle.iAlign = XUI_RICH_ALIGN_LEFT;
	tStyle.iDirection = XUI_RICH_DIRECTION_AUTO;
	if ( pStyle != NULL ) {
		tStyle = *pStyle;
		tStyle.iSize = sizeof(tStyle);
	}
	return tStyle;
}

static xui_rich_text_style_t __xuiRichStyle(const xui_rich_text_style_t* pStyle)
{
	xui_rich_text_style_t tStyle;
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = sizeof(tStyle);
	if ( pStyle != NULL ) {
		tStyle = *pStyle;
		tStyle.iSize = sizeof(tStyle);
	}
	return tStyle;
}

static int __xuiRichUtf8Valid(const char* sText, int iSize)
{
	const unsigned char* p = (const unsigned char*)sText;
	const unsigned char* pEnd = p + iSize;
	unsigned int c;
	if ( sText == NULL || iSize < 0 ) return 0;
	while ( p < pEnd ) {
		if ( *p < 0x80u ) { p++; continue; }
		if ( (*p & 0xe0u) == 0xc0u ) {
			if ( p + 1 >= pEnd || (p[1] & 0xc0u) != 0x80u ) return 0;
			c = ((unsigned int)(p[0] & 0x1fu) << 6) | (unsigned int)(p[1] & 0x3fu);
			if ( c < 0x80u ) return 0;
			p += 2;
		} else if ( (*p & 0xf0u) == 0xe0u ) {
			if ( p + 2 >= pEnd || (p[1] & 0xc0u) != 0x80u || (p[2] & 0xc0u) != 0x80u ) return 0;
			c = ((unsigned int)(p[0] & 0x0fu) << 12) | ((unsigned int)(p[1] & 0x3fu) << 6) | (unsigned int)(p[2] & 0x3fu);
			if ( c < 0x800u || (c >= 0xd800u && c <= 0xdfffu) ) return 0;
			p += 3;
		} else if ( (*p & 0xf8u) == 0xf0u ) {
			if ( p + 3 >= pEnd || (p[1] & 0xc0u) != 0x80u || (p[2] & 0xc0u) != 0x80u || (p[3] & 0xc0u) != 0x80u ) return 0;
			c = ((unsigned int)(p[0] & 7u) << 18) | ((unsigned int)(p[1] & 0x3fu) << 12) |
			    ((unsigned int)(p[2] & 0x3fu) << 6) | (unsigned int)(p[3] & 0x3fu);
			if ( c < 0x10000u || c > 0x10ffffu ) return 0;
			p += 4;
		} else return 0;
	}
	return 1;
}

static xui_rich_node __xuiRichNodeCreate(xui_rich_document pDocument, int iType, uint64_t iPreferredId)
{
	xui_rich_node pNode = (xui_rich_node)xrtCalloc(1, sizeof(*pNode));
	if ( pNode == NULL ) return NULL;
	pNode->iMagic = XUI_RICH_NODE_MAGIC;
	pNode->iType = iType;
	pNode->iId = iPreferredId != 0 ? iPreferredId : __xuiRichFreshId(pDocument);
	if ( pNode->iId >= pDocument->iNextId ) pDocument->iNextId = pNode->iId + 1u;
	pNode->tStyle.iSize = sizeof(pNode->tStyle);
	pNode->tParagraphStyle = __xuiRichParagraphStyle(NULL);
	return pNode;
}

static void __xuiRichNodeDestroy(xui_rich_node pNode)
{
	xui_rich_node pChild;
	if ( !__xuiRichNodeValid(pNode) ) return;
	while ( (pChild = pNode->pFirstChild) != NULL ) {
		pNode->pFirstChild = pChild->pNext;
		__xuiRichNodeDestroy(pChild);
	}
	if ( pNode->sText != NULL ) xrtFree(pNode->sText);
	if ( pNode->sResource != NULL ) xrtFree(pNode->sResource);
	if ( pNode->sAltText != NULL ) xrtFree(pNode->sAltText);
	if ( pNode->ppCellDocuments != NULL ) {
		int i;
		for ( i = 0; i < pNode->iRows * pNode->iColumns; i++ )
			if ( pNode->ppCellDocuments[i] != NULL ) xuiRichDocumentDestroy(pNode->ppCellDocuments[i]);
		xrtFree(pNode->ppCellDocuments);
	}
	pNode->iMagic = 0;
	xrtFree(pNode);
}

static void __xuiRichNodeAppend(xui_rich_node pParent, xui_rich_node pChild)
{
	pChild->pParent = pParent;
	pChild->pPrev = pParent->pLastChild;
	if ( pParent->pLastChild != NULL ) pParent->pLastChild->pNext = pChild;
	else pParent->pFirstChild = pChild;
	pParent->pLastChild = pChild;
}

static void __xuiRichNodeDetach(xui_rich_node pNode)
{
	xui_rich_node pParent = pNode->pParent;
	if ( pParent == NULL ) return;
	if ( pNode->pPrev != NULL ) pNode->pPrev->pNext = pNode->pNext;
	else pParent->pFirstChild = pNode->pNext;
	if ( pNode->pNext != NULL ) pNode->pNext->pPrev = pNode->pPrev;
	else pParent->pLastChild = pNode->pPrev;
	pNode->pParent = NULL;
	pNode->pPrev = NULL;
	pNode->pNext = NULL;
}

static void __xuiRichNodeInsertBefore(xui_rich_node pParent, xui_rich_node pBefore, xui_rich_node pChild)
{
	if ( pBefore == NULL ) { __xuiRichNodeAppend(pParent, pChild); return; }
	pChild->pParent = pParent;
	pChild->pNext = pBefore;
	pChild->pPrev = pBefore->pPrev;
	if ( pBefore->pPrev != NULL ) pBefore->pPrev->pNext = pChild;
	else pParent->pFirstChild = pChild;
	pBefore->pPrev = pChild;
}

static void __xuiRichMoveChildren(xui_rich_node pFrom, xui_rich_node pTo, xui_rich_node pBefore)
{
	xui_rich_node pNode;
	while ( (pNode = pFrom->pFirstChild) != NULL ) {
		__xuiRichNodeDetach(pNode);
		__xuiRichNodeInsertBefore(pTo, pBefore, pNode);
	}
}

static xui_rich_node __xuiRichNodeClone(xui_rich_document pDocument, xui_rich_node pSource)
{
	xui_rich_node pClone;
	xui_rich_node pChild;
	pClone = __xuiRichNodeCreate(pDocument, pSource->iType, pSource->iId);
	if ( pClone == NULL ) return NULL;
	pClone->tStyle = pSource->tStyle;
	pClone->tParagraphStyle = pSource->tParagraphStyle;
	pClone->pWidget = pSource->pWidget;
	pClone->pSurface = pSource->pSurface;
	pClone->fWidth = pSource->fWidth;
	pClone->fHeight = pSource->fHeight;
	pClone->fBaseline = pSource->fBaseline;
	pClone->iRows = pSource->iRows;
	pClone->iColumns = pSource->iColumns;
	pClone->fCellPadding = pSource->fCellPadding;
	pClone->fBorderWidth = pSource->fBorderWidth;
	pClone->iBorderColor = pSource->iBorderColor;
	pClone->iHeaderColor = pSource->iHeaderColor;
	pClone->iCellColor = pSource->iCellColor;
	pClone->sResource = __xuiRichString(pSource->sResource);
	pClone->sAltText = __xuiRichString(pSource->sAltText);
	if ( (pSource->sResource != NULL && pClone->sResource == NULL) || (pSource->sAltText != NULL && pClone->sAltText == NULL) ) {
		__xuiRichNodeDestroy(pClone); return NULL;
	}
	if ( pSource->iRows > 0 && pSource->iColumns > 0 ) {
		int i, n = pSource->iRows * pSource->iColumns;
		pClone->ppCellDocuments = (xui_rich_document*)xrtCalloc((size_t)n, sizeof(*pClone->ppCellDocuments));
		if ( pClone->ppCellDocuments == NULL ) { __xuiRichNodeDestroy(pClone); return NULL; }
		for ( i = 0; i < n; i++ ) {
			if ( pSource->ppCellDocuments != NULL && pSource->ppCellDocuments[i] != NULL ) {
				pClone->ppCellDocuments[i] = __xuiRichDocumentClone(pSource->ppCellDocuments[i], pDocument, pClone->iId);
				if ( pClone->ppCellDocuments[i] == NULL ) { __xuiRichNodeDestroy(pClone); return NULL; }
			}
		}
	}
	if ( pSource->sText != NULL ) {
		pClone->sText = (char*)xrtMalloc((size_t)pSource->iTextSize + 1u);
		if ( pClone->sText == NULL ) { __xuiRichNodeDestroy(pClone); return NULL; }
		memcpy(pClone->sText, pSource->sText, (size_t)pSource->iTextSize + 1u);
		pClone->iTextSize = pSource->iTextSize;
	}
	for ( pChild = pSource->pFirstChild; pChild != NULL; pChild = pChild->pNext ) {
		xui_rich_node pChildClone = __xuiRichNodeClone(pDocument, pChild);
		if ( pChildClone == NULL ) { __xuiRichNodeDestroy(pClone); return NULL; }
		__xuiRichNodeAppend(pClone, pChildClone);
	}
	return pClone;
}

static xui_rich_document __xuiRichDocumentClone(xui_rich_document pSource,
	xui_rich_document pOwnerDocument, xui_document_node_id_t iOwnerTableId)
{
	xui_rich_document pClone;
	if ( !__xuiRichDocumentValid(pSource) ) return NULL;
	pClone = (xui_rich_document)xrtCalloc(1, sizeof(*pClone));
	if ( pClone == NULL ) return NULL;
	pClone->iMagic = XUI_RICH_DOCUMENT_MAGIC;
	pClone->iNextId = pSource->iNextId;
	pClone->iVersion = pSource->iVersion;
	pClone->bFlatDirty = 1;
	pClone->pOwnerDocument = pOwnerDocument;
	pClone->iOwnerTableId = iOwnerTableId;
	pClone->pRoot = __xuiRichNodeClone(pClone, pSource->pRoot);
	if ( pClone->pRoot == NULL ) { pClone->iMagic = 0; xrtFree(pClone); return NULL; }
	return pClone;
}

static void __xuiRichHistoryRelease(xui_rich_history_t* pState)
{
	if ( pState->pRoot != NULL ) __xuiRichNodeDestroy(pState->pRoot);
	if ( pState->pOldBlocks != NULL ) __xuiRichNodeDestroy(pState->pOldBlocks);
	if ( pState->pNewBlocks != NULL ) __xuiRichNodeDestroy(pState->pNewBlocks);
	if ( pState->sOldText != NULL ) xrtFree(pState->sOldText);
	if ( pState->sNewText != NULL ) xrtFree(pState->sNewText);
	memset(pState, 0, sizeof(*pState));
}

static void __xuiRichHistoryClear(xui_rich_history_t* pHistory, int* pCount)
{
	int i;
	for ( i = 0; i < *pCount; i++ ) __xuiRichHistoryRelease(&pHistory[i]);
	*pCount = 0;
}

static void __xuiRichHistoryAppend(xui_rich_history_t* pHistory, int* pCount, xui_rich_history_t* pState)
{
	if ( *pCount == XUI_RICH_HISTORY_LIMIT ) {
		__xuiRichHistoryRelease(&pHistory[0]);
		memmove(pHistory, pHistory + 1, sizeof(*pHistory) * (XUI_RICH_HISTORY_LIMIT - 1u));
		(*pCount)--;
	}
	pHistory[(*pCount)++] = *pState;
	memset(pState, 0, sizeof(*pState));
}

static int __xuiRichHistoryPush(xui_rich_document pDocument, xui_rich_history_t* pHistory, int* pCount)
{
	xui_rich_history_t tState;
	memset(&tState, 0, sizeof(tState));
	tState.iKind = XUI_RICH_HISTORY_SNAPSHOT;
	tState.pRoot = __xuiRichNodeClone(pDocument, pDocument->pRoot);
	if ( tState.pRoot == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	tState.iNextId = pDocument->iNextId;
	__xuiRichHistoryAppend(pHistory, pCount, &tState);
	return XUI_OK;
}

static int __xuiRichHistoryPushText(xui_rich_history_t* pHistory, int* pCount,
	xui_document_node_id_t iNodeId, int iNodeOffset,
	const char* sOldText, int iOldSize, const char* sNewText, int iNewSize)
{
	xui_rich_history_t tState;
	memset(&tState, 0, sizeof(tState));
	tState.iKind = XUI_RICH_HISTORY_TEXT_REPLACE;
	tState.iNodeId = iNodeId;
	tState.iNodeOffset = iNodeOffset;
	tState.iOldSize = iOldSize;
	tState.iNewSize = iNewSize;
	if ( iOldSize > 0 ) {
		tState.sOldText = (char*)xrtMalloc((size_t)iOldSize);
		if ( tState.sOldText == NULL ) { __xuiRichHistoryRelease(&tState); return XUI_ERROR_OUT_OF_MEMORY; }
		memcpy(tState.sOldText, sOldText, (size_t)iOldSize);
	}
	if ( iNewSize > 0 ) {
		tState.sNewText = (char*)xrtMalloc((size_t)iNewSize);
		if ( tState.sNewText == NULL ) { __xuiRichHistoryRelease(&tState); return XUI_ERROR_OUT_OF_MEMORY; }
		memcpy(tState.sNewText, sNewText, (size_t)iNewSize);
	}
	__xuiRichHistoryAppend(pHistory, pCount, &tState);
	return XUI_OK;
}

static void __xuiRichHistoryPushBlock(xui_rich_history_t* pHistory, int* pCount,
	xui_rich_history_t* pState)
{
	pState->iKind = XUI_RICH_HISTORY_BLOCK_REPLACE;
	__xuiRichHistoryAppend(pHistory, pCount, pState);
}

static void __xuiRichSetChange(xui_rich_document pDocument, int iKind, uint32_t iFlags,
	int iOldStart, int iOldEnd, int iNewStart, int iNewEnd, xui_document_node_id_t iNodeId)
{
	xui_rich_change_t* pChange = &pDocument->tPendingChange;
	memset(pChange, 0, sizeof(*pChange));
	pChange->iSize = sizeof(*pChange);
	pChange->iKind = iKind;
	pChange->iFlags = iFlags;
	pChange->iOldStart = iOldStart;
	pChange->iOldEnd = iOldEnd;
	pChange->iNewStart = iNewStart;
	pChange->iNewEnd = iNewEnd;
	pChange->iNodeId = iNodeId;
}

static void __xuiRichMergeTransactionChange(xui_rich_document pDocument, const xui_rich_change_t* pChange)
{
	xui_rich_change_t* pMerged = &pDocument->tTransactionChange;
	if ( pMerged->iSize == 0 ) {
		*pMerged = *pChange;
		return;
	}
	pMerged->iKind = pMerged->iKind == pChange->iKind ? pMerged->iKind : XUI_RICH_CHANGE_RESET;
	pMerged->iFlags |= pChange->iFlags;
	if ( pChange->iOldStart < pMerged->iOldStart ) pMerged->iOldStart = pChange->iOldStart;
	if ( pChange->iOldEnd > pMerged->iOldEnd ) pMerged->iOldEnd = pChange->iOldEnd;
	if ( pChange->iNewStart < pMerged->iNewStart ) pMerged->iNewStart = pChange->iNewStart;
	if ( pChange->iNewEnd > pMerged->iNewEnd ) pMerged->iNewEnd = pChange->iNewEnd;
	if ( pMerged->iNodeId != pChange->iNodeId ) pMerged->iNodeId = 0;
}

static void __xuiRichChangedEx(xui_rich_document pDocument, int bFlatReady)
{
	xui_rich_change_t tChange;
	xui_rich_document pOwner;
	int i;
	if ( pDocument->tPendingChange.iSize == 0 ) {
		__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_RESET,
			XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
			0, pDocument->iFlatLength, 0, pDocument->iFlatLength, 0);
	}
	tChange = pDocument->tPendingChange;
	memset(&pDocument->tPendingChange, 0, sizeof(pDocument->tPendingChange));
	if ( !bFlatReady ) pDocument->bFlatDirty = 1;
	if ( pDocument->iTransactionDepth > 0 ) {
		pDocument->bTransactionChanged = 1;
		__xuiRichMergeTransactionChange(pDocument, &tChange);
		if ( !pDocument->bApplyingHistory && pDocument->iPendingSnapshot > 0 &&
		     pDocument->iPendingSnapshot <= pDocument->iUndoCount )
			pDocument->arrUndo[pDocument->iPendingSnapshot - 1].tChange = pDocument->tTransactionChange;
		return;
	}
	if ( !pDocument->bApplyingHistory && pDocument->iPendingSnapshot > 0 &&
	     pDocument->iPendingSnapshot <= pDocument->iUndoCount )
		pDocument->arrUndo[pDocument->iPendingSnapshot - 1].tChange = tChange;
	pDocument->iPendingSnapshot = 0;
	pDocument->iVersion++;
	if ( pDocument->iVersion == 0 ) pDocument->iVersion = 1;
	tChange.iVersion = pDocument->iVersion;
	pDocument->tLastChange = tChange;
	for ( i = 0; i < pDocument->iObserverCount; i++ )
		pDocument->pObservers[i].onChange(pDocument, &pDocument->tLastChange, pDocument->pObservers[i].pUser);
	pOwner = pDocument->pOwnerDocument;
	if ( __xuiRichDocumentValid(pOwner) && pOwner != pDocument ) {
		xui_rich_node pTable = __xuiRichFindNode(pOwner->pRoot, pDocument->iOwnerTableId);
		int iOffset;
		if ( pTable != NULL && pTable->iType == XUI_RICH_NODE_TABLE &&
		     xuiInternalRichDocumentNodeOffset(pOwner, pTable, &iOffset) == XUI_OK ) {
			__xuiRichSetChange(pOwner, XUI_RICH_CHANGE_RESOURCE,
				XUI_RICH_CHANGE_RESOURCE_DATA | XUI_RICH_CHANGE_GEOMETRY,
				iOffset, iOffset + XUI_RICH_OBJECT_SIZE, iOffset, iOffset + XUI_RICH_OBJECT_SIZE,
				pTable->iId);
			__xuiRichChanged(pOwner);
		}
	}
}

static void __xuiRichChanged(xui_rich_document pDocument)
{
	__xuiRichChangedEx(pDocument, 0);
}

static int __xuiRichBeginChange(xui_rich_document pDocument)
{
	int iRet;
	if ( pDocument->bApplyingHistory ) return XUI_OK;
	if ( pDocument->iTransactionDepth > 0 && pDocument->bTransactionSnapshot ) return XUI_OK;
	iRet = __xuiRichHistoryPush(pDocument, pDocument->arrUndo, &pDocument->iUndoCount);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->iPendingSnapshot = pDocument->iUndoCount;
	__xuiRichHistoryClear(pDocument->arrRedo, &pDocument->iRedoCount);
	if ( pDocument->iTransactionDepth > 0 ) pDocument->bTransactionSnapshot = 1;
	return XUI_OK;
}

static int __xuiRichFlatReserve(xui_rich_document pDocument, int iNeed)
{
	char* sNew;
	int iCapacity = pDocument->iFlatCapacity;
	if ( iNeed <= iCapacity ) return XUI_OK;
	if ( iCapacity < 64 ) iCapacity = 64;
	while ( iCapacity < iNeed ) iCapacity *= 2;
	sNew = (char*)xrtRealloc(pDocument->sFlatText, (size_t)iCapacity);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pDocument->sFlatText = sNew;
	pDocument->iFlatCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiRichBuildFlat(xui_rich_document pDocument)
{
	xui_rich_node pBlock;
	xui_rich_node pNode;
	int iLength = 0;
	int iRet;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		if ( __xuiRichTextBlockType(pBlock->iType) ) {
			for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext )
				iLength += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
		} else if ( __xuiRichAtomicBlockType(pBlock->iType) ) iLength += XUI_RICH_OBJECT_SIZE;
		if ( pBlock->pNext != NULL ) iLength++;
	}
	iRet = __xuiRichFlatReserve(pDocument, iLength + 1);
	if ( iRet != XUI_OK ) return iRet;
	iLength = 0;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		if ( __xuiRichTextBlockType(pBlock->iType) ) for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
			if ( __xuiRichInlineTextType(pNode->iType) ) {
				memcpy(pDocument->sFlatText + iLength, pNode->sText, (size_t)pNode->iTextSize);
				iLength += pNode->iTextSize;
			} else {
				memcpy(pDocument->sFlatText + iLength, XUI_RICH_OBJECT_TEXT, XUI_RICH_OBJECT_SIZE);
				iLength += XUI_RICH_OBJECT_SIZE;
			}
		} else if ( __xuiRichAtomicBlockType(pBlock->iType) ) {
			memcpy(pDocument->sFlatText + iLength, XUI_RICH_OBJECT_TEXT, XUI_RICH_OBJECT_SIZE);
			iLength += XUI_RICH_OBJECT_SIZE;
		}
		if ( pBlock->pNext != NULL ) pDocument->sFlatText[iLength++] = '\n';
	}
	pDocument->sFlatText[iLength] = 0;
	pDocument->iFlatLength = iLength;
	pDocument->bFlatDirty = 0;
	return XUI_OK;
}

static int __xuiRichEnsureFlat(xui_rich_document pDocument)
{
	return pDocument->bFlatDirty ? __xuiRichBuildFlat(pDocument) : XUI_OK;
}

static int __xuiRichLocateTextRange(xui_rich_document pDocument, int iStart, int iEnd,
	xui_rich_node* ppNode, int* pNodeStart, int* pNodeEnd)
{
	xui_rich_node pBlock;
	xui_rich_node pNode;
	int iAt = 0;
	int iSize;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		if ( __xuiRichTextBlockType(pBlock->iType) ) {
			for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
				iSize = __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
				if ( __xuiRichInlineTextType(pNode->iType) && iStart >= iAt && iEnd <= iAt + iSize ) {
					*ppNode = pNode;
					*pNodeStart = iStart - iAt;
					*pNodeEnd = iEnd - iAt;
					return XUI_OK;
				}
				iAt += iSize;
			}
		} else if ( __xuiRichAtomicBlockType(pBlock->iType) ) iAt += XUI_RICH_OBJECT_SIZE;
		if ( pBlock->pNext != NULL ) iAt++;
	}
	return XUI_ERROR_UNSUPPORTED;
}

static int __xuiRichReplaceNodeBytes(xui_rich_document pDocument, xui_rich_node pNode,
	int iNodeStart, int iOldSize, const char* sNewText, int iNewSize, int iDocumentStart,
	int iChangeKind)
{
	char* sNodeText;
	char* sOldNodeText;
	int iNodeNewSize = pNode->iTextSize - iOldSize + iNewSize;
	int iFlatNewSize = pDocument->iFlatLength - iOldSize + iNewSize;
	int iTailSize;
	int iRet;
	sNodeText = (char*)xrtMalloc((size_t)iNodeNewSize + 1u);
	if ( sNodeText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(sNodeText, pNode->sText, (size_t)iNodeStart);
	if ( iNewSize > 0 ) memcpy(sNodeText + iNodeStart, sNewText, (size_t)iNewSize);
	memcpy(sNodeText + iNodeStart + iNewSize, pNode->sText + iNodeStart + iOldSize,
		(size_t)(pNode->iTextSize - iNodeStart - iOldSize));
	sNodeText[iNodeNewSize] = 0;
	iRet = __xuiRichFlatReserve(pDocument, iFlatNewSize + 1);
	if ( iRet != XUI_OK ) { xrtFree(sNodeText); return iRet; }
	sOldNodeText = pNode->sText;
	pNode->sText = sNodeText;
	pNode->iTextSize = iNodeNewSize;
	iTailSize = pDocument->iFlatLength - iDocumentStart - iOldSize;
	memmove(pDocument->sFlatText + iDocumentStart + iNewSize,
		pDocument->sFlatText + iDocumentStart + iOldSize, (size_t)iTailSize + 1u);
	if ( iNewSize > 0 ) memcpy(pDocument->sFlatText + iDocumentStart, sNewText, (size_t)iNewSize);
	pDocument->iFlatLength = iFlatNewSize;
	pDocument->bFlatDirty = 0;
	xrtFree(sOldNodeText);
	__xuiRichSetChange(pDocument, iChangeKind,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_GEOMETRY,
		iDocumentStart, iDocumentStart + iOldSize,
		iDocumentStart, iDocumentStart + iNewSize, pNode->iId);
	__xuiRichChangedEx(pDocument, 1);
	return XUI_OK;
}

static int __xuiRichTryLocalReplace(xui_rich_document pDocument, int iStart, int iEnd,
	const char* sText, int iTextSize, const xui_rich_text_style_t* pStyle)
{
	xui_rich_node pNode = NULL;
	xui_rich_text_style_t tStyle;
	int iNodeStart;
	int iNodeEnd;
	int iOldSize;
	int iRet;
	if ( pDocument->bApplyingHistory || pDocument->iTransactionDepth > 0 ||
	     strchr(sText, '\n') != NULL || strchr(sText, '\r') != NULL ) return XUI_ERROR_UNSUPPORTED;
	if ( __xuiRichLocateTextRange(pDocument, iStart, iEnd, &pNode, &iNodeStart, &iNodeEnd) != XUI_OK )
		return XUI_ERROR_UNSUPPORTED;
	iOldSize = iNodeEnd - iNodeStart;
	if ( pNode->iTextSize - iOldSize + iTextSize <= 0 ) return XUI_ERROR_UNSUPPORTED;
	tStyle = __xuiRichStyle(pStyle);
	if ( iTextSize > 0 && !__xuiRichStyleSame(&pNode->tStyle, &tStyle) ) return XUI_ERROR_UNSUPPORTED;
	if ( iOldSize == 0 && iTextSize == 0 ) return XUI_OK;
	iRet = __xuiRichFlatReserve(pDocument, pDocument->iFlatLength - iOldSize + iTextSize + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRichHistoryPushText(pDocument->arrUndo, &pDocument->iUndoCount,
		pNode->iId, iNodeStart, pNode->sText + iNodeStart, iOldSize, sText, iTextSize);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRichReplaceNodeBytes(pDocument, pNode, iNodeStart, iOldSize, sText, iTextSize,
		iStart, XUI_RICH_CHANGE_REPLACE);
	if ( iRet != XUI_OK ) {
		__xuiRichHistoryRelease(&pDocument->arrUndo[--pDocument->iUndoCount]);
		return iRet;
	}
	__xuiRichHistoryClear(pDocument->arrRedo, &pDocument->iRedoCount);
	return XUI_OK;
}

static int __xuiRichBuilderBlock(xui_rich_builder_t* pBuilder, int iType, uint64_t iId, const xui_rich_paragraph_style_t* pStyle)
{
	xui_rich_node pParagraph;
	xui_rich_node pExisting;
	if ( !__xuiRichTextBlockType(iType) ) iType = XUI_RICH_NODE_PARAGRAPH;
	for ( pExisting = pBuilder->pRoot->pFirstChild; pExisting != NULL && iId != 0; pExisting = pExisting->pNext )
		if ( pExisting->iId == iId ) iId = 0;
	pParagraph = __xuiRichNodeCreate(pBuilder->pDocument, iType, iId);
	if ( pParagraph == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pParagraph->tParagraphStyle = __xuiRichParagraphStyle(pStyle);
	__xuiRichNodeAppend(pBuilder->pRoot, pParagraph);
	pBuilder->pParagraph = pParagraph;
	return XUI_OK;
}

static int __xuiRichBuilderParagraph(xui_rich_builder_t* pBuilder, uint64_t iId)
{
	return __xuiRichBuilderBlock(pBuilder, XUI_RICH_NODE_PARAGRAPH, iId, NULL);
}

static int __xuiRichStringSame(const char* sA, const char* sB)
{
	if ( sA == sB ) return 1;
	if ( sA == NULL || sB == NULL ) return 0;
	return strcmp(sA, sB) == 0;
}

static int __xuiRichBuilderText(xui_rich_builder_t* pBuilder, const char* sText, int iSize,
	const xui_rich_text_style_t* pStyle, uint64_t iId, int iType, const char* sResource)
{
	xui_rich_node pNode;
	if ( iSize <= 0 ) return XUI_OK;
	if ( iType != XUI_RICH_NODE_LINK ) iType = XUI_RICH_NODE_TEXT;
	if ( pBuilder->pParagraph == NULL && __xuiRichBuilderParagraph(pBuilder, 0) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iId != 0 && __xuiRichFindNode(pBuilder->pRoot, iId) != NULL ) iId = 0;
	if ( iId == 0 && pBuilder->pParagraph->pLastChild != NULL &&
	     pBuilder->pParagraph->pLastChild->iType == iType &&
	     __xuiRichStringSame(pBuilder->pParagraph->pLastChild->sResource, sResource) &&
	     __xuiRichStyleSame(&pBuilder->pParagraph->pLastChild->tStyle, pStyle) ) {
		pNode = pBuilder->pParagraph->pLastChild;
		pNode->sText = (char*)xrtRealloc(pNode->sText, (size_t)pNode->iTextSize + (size_t)iSize + 1u);
		if ( pNode->sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		memcpy(pNode->sText + pNode->iTextSize, sText, (size_t)iSize);
		pNode->iTextSize += iSize;
		pNode->sText[pNode->iTextSize] = 0;
		return XUI_OK;
	}
	pNode = __xuiRichNodeCreate(pBuilder->pDocument, iType, iId);
	if ( pNode == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iType == XUI_RICH_NODE_LINK ) {
		pNode->sResource = __xuiRichString(sResource);
		if ( pNode->sResource == NULL ) { __xuiRichNodeDestroy(pNode); return XUI_ERROR_OUT_OF_MEMORY; }
	}
	pNode->sText = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( pNode->sText == NULL ) { __xuiRichNodeDestroy(pNode); return XUI_ERROR_OUT_OF_MEMORY; }
	memcpy(pNode->sText, sText, (size_t)iSize);
	pNode->sText[iSize] = 0;
	pNode->iTextSize = iSize;
	pNode->tStyle = *pStyle;
	__xuiRichNodeAppend(pBuilder->pParagraph, pNode);
	return XUI_OK;
}

static int __xuiRichBuilderInput(xui_rich_builder_t* pBuilder, const char* sText, int iSize, const xui_rich_text_style_t* pStyle)
{
	int iStart = 0;
	int i;
	int iRet;
	for ( i = 0; i <= iSize; i++ ) {
		if ( i == iSize || sText[i] == '\n' || sText[i] == '\r' ) {
			iRet = __xuiRichBuilderText(pBuilder, sText + iStart, i - iStart, pStyle, 0, XUI_RICH_NODE_TEXT, NULL);
			if ( iRet != XUI_OK ) return iRet;
			if ( i < iSize ) {
				if ( sText[i] == '\r' && i + 1 < iSize && sText[i + 1] == '\n' ) i++;
				iRet = __xuiRichBuilderParagraph(pBuilder, 0);
				if ( iRet != XUI_OK ) return iRet;
			}
			iStart = i + 1;
		}
	}
	return XUI_OK;
}

static int __xuiRichBuilderWidget(xui_rich_builder_t* pBuilder, xui_rich_node pSource)
{
	xui_rich_node pNode;
	uint64_t iId = pSource->iId;
	if ( pBuilder->pParagraph == NULL && __xuiRichBuilderParagraph(pBuilder, 0) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iId != 0 && __xuiRichFindNode(pBuilder->pRoot, iId) != NULL ) iId = 0;
	pNode = __xuiRichNodeCreate(pBuilder->pDocument, XUI_RICH_NODE_INLINE_WIDGET, iId);
	if ( pNode == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pNode->pWidget = pSource->pWidget;
	pNode->fWidth = pSource->fWidth;
	pNode->fHeight = pSource->fHeight;
	pNode->fBaseline = pSource->fBaseline;
	__xuiRichNodeAppend(pBuilder->pParagraph, pNode);
	return XUI_OK;
}

static int __xuiRichBuilderObject(xui_rich_builder_t* pBuilder, xui_rich_node pSource)
{
	xui_rich_node pNode = __xuiRichNodeClone(pBuilder->pDocument, pSource);
	if ( pNode == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( __xuiRichFindNode(pBuilder->pRoot, pNode->iId) != NULL )
		__xuiRichNodeAssignFreshIds(pBuilder->pDocument, pNode, pBuilder->pRoot);
	if ( __xuiRichAtomicBlockType(pSource->iType) ) {
		__xuiRichNodeAppend(pBuilder->pRoot, pNode);
		pBuilder->pParagraph = NULL;
	} else {
		if ( pBuilder->pParagraph == NULL && __xuiRichBuilderParagraph(pBuilder, 0) != XUI_OK ) { __xuiRichNodeDestroy(pNode); return XUI_ERROR_OUT_OF_MEMORY; }
		__xuiRichNodeAppend(pBuilder->pParagraph, pNode);
	}
	return XUI_OK;
}

static int __xuiRichBuilderSliceEx(xui_rich_builder_t* pBuilder, xui_rich_document pSource,
	int iSliceStart, int iSliceEnd, const xui_rich_text_style_t* pOverride, int bApplyStyle,
	uint32_t iSetFlags, uint32_t iClearFlags, const char* sLinkOverride, int bApplyLink)
{
	xui_rich_node pParagraph;
	xui_rich_node pNode;
	int iAt = 0;
	int iStart;
	int iEnd;
	int iNodeStart;
	int iNodeEnd;
	int iRet;
	for ( pParagraph = pSource->pRoot->pFirstChild; pParagraph != NULL; pParagraph = pParagraph->pNext ) {
		if ( __xuiRichAtomicBlockType(pParagraph->iType) ) {
			iNodeStart = iAt;
			iNodeEnd = iAt + XUI_RICH_OBJECT_SIZE;
			iStart = iSliceStart > iNodeStart ? iSliceStart : iNodeStart;
			iEnd = iSliceEnd < iNodeEnd ? iSliceEnd : iNodeEnd;
			if ( iStart == iNodeStart && iEnd == iNodeEnd ) {
				iRet = __xuiRichBuilderObject(pBuilder, pParagraph);
				if ( iRet != XUI_OK ) return iRet;
			}
			iAt = iNodeEnd;
		} else {
		for ( pNode = pParagraph->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
			iNodeStart = iAt;
			iNodeEnd = iAt + (__xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE);
			iStart = iSliceStart > iNodeStart ? iSliceStart : iNodeStart;
			iEnd = iSliceEnd < iNodeEnd ? iSliceEnd : iNodeEnd;
			if ( iStart < iEnd ) {
				if ( pBuilder->pParagraph == NULL ) {
					iRet = __xuiRichBuilderBlock(pBuilder, pParagraph->iType, pParagraph->iId, &pParagraph->tParagraphStyle);
					if ( iRet != XUI_OK ) return iRet;
				}
				if ( __xuiRichInlineTextType(pNode->iType) ) {
					xui_rich_text_style_t tStyle = pNode->tStyle;
					const xui_rich_text_style_t* pStyle = bApplyStyle ? pOverride : &tStyle;
					int iOutputType = bApplyLink ? (sLinkOverride != NULL && sLinkOverride[0] != 0 ? XUI_RICH_NODE_LINK : XUI_RICH_NODE_TEXT) : pNode->iType;
					const char* sOutputResource = bApplyLink ? sLinkOverride : pNode->sResource;
					uint64_t iId = iStart == iNodeStart && iEnd == iNodeEnd && !bApplyStyle ? pNode->iId : 0;
					if ( !bApplyStyle && (iSetFlags != 0 || iClearFlags != 0) ) {
						tStyle.iFlags = (tStyle.iFlags | iSetFlags) & ~iClearFlags;
						iId = 0;
					}
					iRet = __xuiRichBuilderText(pBuilder, pNode->sText + iStart - iNodeStart, iEnd - iStart,
						pStyle, bApplyLink ? 0 : iId, iOutputType, sOutputResource);
					if ( iRet != XUI_OK ) return iRet;
				} else if ( iStart == iNodeStart && iEnd == iNodeEnd ) {
					iRet = pNode->iType == XUI_RICH_NODE_INLINE_WIDGET ? __xuiRichBuilderWidget(pBuilder, pNode) : __xuiRichBuilderObject(pBuilder, pNode);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
			iAt = iNodeEnd;
		}
		}
		if ( pParagraph->pNext != NULL ) {
			if ( iAt >= iSliceStart && iAt < iSliceEnd ) {
				iRet = __xuiRichBuilderBlock(pBuilder, pParagraph->pNext->iType, pParagraph->pNext->iId,
					&pParagraph->pNext->tParagraphStyle);
				if ( iRet != XUI_OK ) return iRet;
			}
			iAt++;
		}
	}
	return XUI_OK;
}

static int __xuiRichBuilderSlice(xui_rich_builder_t* pBuilder, xui_rich_document pSource,
	int iSliceStart, int iSliceEnd, const xui_rich_text_style_t* pOverride, int bApplyStyle)
{
	return __xuiRichBuilderSliceEx(pBuilder, pSource, iSliceStart, iSliceEnd, pOverride, bApplyStyle, 0, 0, NULL, 0);
}

static int __xuiRichBlockRange(xui_rich_document pDocument, int iStart, int iEnd,
	xui_rich_node* ppFirst, xui_rich_node* ppLast, int* pRangeStart, int* pRangeEnd)
{
	xui_rich_node pBlock;
	xui_rich_node pNode;
	xui_rich_node pFirst = NULL;
	xui_rich_node pLast = NULL;
	int iAt = 0;
	int iBlockEnd;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		iBlockEnd = iAt;
		if ( __xuiRichTextBlockType(pBlock->iType) ) {
			for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext )
				iBlockEnd += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
		} else if ( __xuiRichAtomicBlockType(pBlock->iType) ) iBlockEnd += XUI_RICH_OBJECT_SIZE;
		if ( pFirst == NULL && iStart <= iBlockEnd ) {
			pFirst = pBlock;
			*pRangeStart = iAt;
		}
		if ( pFirst != NULL ) {
			pLast = pBlock;
			*pRangeEnd = iBlockEnd;
			if ( iEnd <= iBlockEnd ) break;
		}
		iAt = iBlockEnd + (pBlock->pNext != NULL ? 1 : 0);
	}
	if ( pFirst == NULL || pLast == NULL ) return XUI_ERROR_UNSUPPORTED;
	*ppFirst = pFirst;
	*ppLast = pLast;
	return XUI_OK;
}

static int __xuiRichTryBlockReplace(xui_rich_document pDocument, int iStart, int iEnd,
	const char* sText, int iTextSize, const xui_rich_text_style_t* pStyle)
{
	xui_rich_builder_t tBuilder;
	xui_rich_history_t tState;
	xui_rich_node pFirst;
	xui_rich_node pLast;
	xui_rich_node pAfter;
	xui_rich_node pNode;
	xui_rich_text_style_t tStyle;
	int iRangeStart = 0;
	int iRangeEnd = 0;
	int iOldSize = iEnd - iStart;
	int iNewFlatSize;
	int iTailSize;
	int iRet;
	if ( pDocument->bApplyingHistory || pDocument->iTransactionDepth > 0 || strchr(sText, '\r') != NULL )
		return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiRichBlockRange(pDocument, iStart, iEnd, &pFirst, &pLast, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iNewFlatSize = pDocument->iFlatLength - iOldSize + iTextSize;
	iRet = __xuiRichFlatReserve(pDocument, iNewFlatSize + 1);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tState, 0, sizeof(tState));
	tState.pOldBlocks = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, 0);
	tState.pNewBlocks = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, 0);
	if ( tState.pOldBlocks == NULL || tState.pNewBlocks == NULL ) {
		__xuiRichHistoryRelease(&tState); return XUI_ERROR_OUT_OF_MEMORY;
	}
	tState.iBeforeBlockId = pFirst->pPrev != NULL ? pFirst->pPrev->iId : 0;
	tState.iAfterBlockId = pLast->pNext != NULL ? pLast->pNext->iId : 0;
	tState.iNodeOffset = iStart;
	tState.iOldSize = iOldSize;
	tState.iNewSize = iTextSize;
	if ( iOldSize > 0 ) {
		tState.sOldText = (char*)xrtMalloc((size_t)iOldSize);
		if ( tState.sOldText == NULL ) { __xuiRichHistoryRelease(&tState); return XUI_ERROR_OUT_OF_MEMORY; }
		memcpy(tState.sOldText, pDocument->sFlatText + iStart, (size_t)iOldSize);
	}
	if ( iTextSize > 0 ) {
		tState.sNewText = (char*)xrtMalloc((size_t)iTextSize);
		if ( tState.sNewText == NULL ) { __xuiRichHistoryRelease(&tState); return XUI_ERROR_OUT_OF_MEMORY; }
		memcpy(tState.sNewText, sText, (size_t)iTextSize);
	}
	memset(&tBuilder, 0, sizeof(tBuilder));
	tBuilder.pDocument = pDocument;
	tBuilder.pRoot = tState.pNewBlocks;
	tStyle = __xuiRichStyle(pStyle);
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iRangeStart, iStart, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderInput(&tBuilder, sText, iTextSize, &tStyle);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iRangeEnd, NULL, 0);
	if ( iRet == XUI_OK && tState.pNewBlocks->pFirstChild == NULL ) iRet = __xuiRichBuilderParagraph(&tBuilder, 0);
	if ( iRet != XUI_OK ) { __xuiRichHistoryRelease(&tState); return iRet; }
	pAfter = pLast->pNext;
	pNode = pFirst;
	while ( pNode != pAfter ) {
		xui_rich_node pNext = pNode->pNext;
		__xuiRichNodeDetach(pNode);
		__xuiRichNodeAppend(tState.pOldBlocks, pNode);
		pNode = pNext;
	}
	__xuiRichMoveChildren(tState.pNewBlocks, pDocument->pRoot, pAfter);
	iTailSize = pDocument->iFlatLength - iEnd;
	memmove(pDocument->sFlatText + iStart + iTextSize, pDocument->sFlatText + iEnd, (size_t)iTailSize + 1u);
	if ( iTextSize > 0 ) memcpy(pDocument->sFlatText + iStart, sText, (size_t)iTextSize);
	pDocument->iFlatLength = iNewFlatSize;
	pDocument->bFlatDirty = 0;
	__xuiRichHistoryPushBlock(pDocument->arrUndo, &pDocument->iUndoCount, &tState);
	__xuiRichHistoryClear(pDocument->arrRedo, &pDocument->iRedoCount);
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_REPLACE,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		iStart, iEnd, iStart, iStart + iTextSize, 0);
	__xuiRichChangedEx(pDocument, 1);
	return XUI_OK;
}

static int __xuiRichReplaceRoot(xui_rich_document pDocument, xui_rich_node pRoot)
{
	__xuiRichNodeDestroy(pDocument->pRoot);
	pDocument->pRoot = pRoot;
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichDocumentCreate(xui_rich_document* ppDocument)
{
	xui_rich_document pDocument;
	if ( ppDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	pDocument = (xui_rich_document)xrtCalloc(1, sizeof(*pDocument));
	if ( pDocument == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pDocument->iMagic = XUI_RICH_DOCUMENT_MAGIC;
	pDocument->iNextId = 1;
	pDocument->iVersion = 1;
	pDocument->bFlatDirty = 1;
	pDocument->pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, 0);
	if ( pDocument->pRoot == NULL ) { xrtFree(pDocument); return XUI_ERROR_OUT_OF_MEMORY; }
	if ( xuiRichDocumentAppendParagraph(pDocument) == NULL ) { xuiRichDocumentDestroy(pDocument); return XUI_ERROR_OUT_OF_MEMORY; }
	pDocument->iUndoCount = 0;
	*ppDocument = pDocument;
	return XUI_OK;
}

XUI_API void xuiRichDocumentDestroy(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) ) return;
	if ( pDocument->iObserverCount > 0 ) {
		pDocument->bDestroyPending = 1;
		pDocument->pOwnerDocument = NULL;
		pDocument->iOwnerTableId = 0;
		return;
	}
	__xuiRichNodeDestroy(pDocument->pRoot);
	__xuiRichHistoryClear(pDocument->arrUndo, &pDocument->iUndoCount);
	__xuiRichHistoryClear(pDocument->arrRedo, &pDocument->iRedoCount);
	if ( pDocument->sFlatText != NULL ) xrtFree(pDocument->sFlatText);
	if ( pDocument->pObservers != NULL ) xrtFree(pDocument->pObservers);
	pDocument->iMagic = 0;
	xrtFree(pDocument);
}

XUI_API int xuiRichDocumentBeginTransaction(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iTransactionDepth++ == 0 ) {
		pDocument->bTransactionSnapshot = 0;
		pDocument->bTransactionChanged = 0;
		memset(&pDocument->tTransactionChange, 0, sizeof(pDocument->tTransactionChange));
	}
	return XUI_OK;
}

XUI_API int xuiRichDocumentEndTransaction(xui_rich_document pDocument)
{
	int i;
	if ( !__xuiRichDocumentValid(pDocument) || pDocument->iTransactionDepth <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( --pDocument->iTransactionDepth == 0 ) {
		pDocument->bTransactionSnapshot = 0;
		if ( pDocument->bTransactionChanged ) {
			pDocument->bTransactionChanged = 0;
			pDocument->iVersion++;
			if ( pDocument->iVersion == 0 ) pDocument->iVersion = 1;
			pDocument->tTransactionChange.iVersion = pDocument->iVersion;
			pDocument->tLastChange = pDocument->tTransactionChange;
			memset(&pDocument->tTransactionChange, 0, sizeof(pDocument->tTransactionChange));
			for ( i = 0; i < pDocument->iObserverCount; i++ )
				pDocument->pObservers[i].onChange(pDocument, &pDocument->tLastChange, pDocument->pObservers[i].pUser);
		}
		pDocument->iPendingSnapshot = 0;
	}
	return XUI_OK;
}

XUI_API int xuiRichDocumentClear(xui_rich_document pDocument)
{
	xui_rich_node pRoot;
	xui_rich_node pParagraph;
	int iOldLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldLength = xuiRichDocumentGetLength(pDocument);
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	pParagraph = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_PARAGRAPH, 0);
	if ( pRoot == NULL || pParagraph == NULL ) { __xuiRichNodeDestroy(pRoot); __xuiRichNodeDestroy(pParagraph); return XUI_ERROR_OUT_OF_MEMORY; }
	__xuiRichNodeAppend(pRoot, pParagraph);
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_RESET,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		0, iOldLength, 0, 0, 0);
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

XUI_API xui_rich_node xuiRichDocumentGetRoot(xui_rich_document pDocument)
{
	return __xuiRichDocumentValid(pDocument) ? pDocument->pRoot : NULL;
}

XUI_API xui_rich_node xuiRichDocumentAppendParagraph(xui_rich_document pDocument)
{
	return xuiRichDocumentAppendBlock(pDocument, XUI_RICH_NODE_PARAGRAPH, NULL);
}

XUI_API xui_rich_node xuiRichDocumentAppendBlock(xui_rich_document pDocument, int iNodeType, const xui_rich_paragraph_style_t* pStyle)
{
	xui_rich_node pNode;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichTextBlockType(iNodeType) ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, iNodeType, 0);
	if ( pNode == NULL ) return NULL;
	pNode->tParagraphStyle = __xuiRichParagraphStyle(pStyle);
	__xuiRichNodeAppend(pDocument->pRoot, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendText(xui_rich_document pDocument, xui_rich_node pParagraph, const char* sText, const xui_rich_text_style_t* pStyle)
{
	xui_rich_node pNode;
	int iSize;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pParagraph) || !__xuiRichTextBlockType(pParagraph->iType) || sText == NULL ) return NULL;
	iSize = (int)strlen(sText);
	if ( !__xuiRichUtf8Valid(sText, iSize) || strchr(sText, '\n') != NULL || strchr(sText, '\r') != NULL ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_TEXT, 0);
	if ( pNode == NULL ) return NULL;
	pNode->sText = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( pNode->sText == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
	memcpy(pNode->sText, sText, (size_t)iSize + 1u);
	pNode->iTextSize = iSize;
	pNode->tStyle = __xuiRichStyle(pStyle);
	__xuiRichNodeAppend(pParagraph, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendLink(xui_rich_document pDocument, xui_rich_node pParagraph, const char* sText, const char* sUrl, const xui_rich_text_style_t* pStyle)
{
	xui_rich_node pNode;
	int iSize;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pParagraph) || !__xuiRichTextBlockType(pParagraph->iType) || sText == NULL || sUrl == NULL ) return NULL;
	iSize = (int)strlen(sText);
	if ( !__xuiRichUtf8Valid(sText, iSize) || strchr(sText, '\n') != NULL || strchr(sText, '\r') != NULL ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_LINK, 0);
	if ( pNode == NULL ) return NULL;
	pNode->sText = __xuiRichString(sText);
	pNode->sResource = __xuiRichString(sUrl);
	if ( pNode->sText == NULL || pNode->sResource == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
	pNode->iTextSize = iSize;
	pNode->tStyle = __xuiRichStyle(pStyle);
	if ( pStyle == NULL ) {
		pNode->tStyle.iTextColor = XUI_COLOR_RGBA(20, 92, 170, 255);
		pNode->tStyle.iFlags |= XUI_RICH_STYLE_UNDERLINE;
	}
	__xuiRichNodeAppend(pParagraph, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendWidget(xui_rich_document pDocument, xui_rich_node pParagraph, xui_widget pWidget, float fWidth, float fHeight, float fBaseline)
{
	xui_rich_node pNode;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pParagraph) || !__xuiRichTextBlockType(pParagraph->iType) || pWidget == NULL || fWidth < 0.0f || fHeight < 0.0f ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_INLINE_WIDGET, 0);
	if ( pNode == NULL ) return NULL;
	pNode->pWidget = pWidget;
	pNode->fWidth = fWidth;
	pNode->fHeight = fHeight;
	pNode->fBaseline = fBaseline >= 0.0f ? fBaseline : fHeight;
	__xuiRichNodeAppend(pParagraph, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendInlineImage(xui_rich_document pDocument,
	xui_rich_node pParagraph, const xui_rich_image_desc_t* pDesc)
{
	xui_rich_node pNode;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pParagraph) ||
	     !__xuiRichTextBlockType(pParagraph->iType) || pDesc == NULL ||
	     (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->pSurface == NULL && (pDesc->sSource == NULL || pDesc->sSource[0] == 0)) ||
	     pDesc->fWidth < 0.0f || pDesc->fHeight < 0.0f ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_INLINE_IMAGE, 0);
	if ( pNode == NULL ) return NULL;
	pNode->pSurface = pDesc->pSurface;
	pNode->sResource = __xuiRichString(pDesc->sSource);
	pNode->sAltText = __xuiRichString(pDesc->sAltText);
	if ( (pDesc->sSource != NULL && pNode->sResource == NULL) ||
	     (pDesc->sAltText != NULL && pNode->sAltText == NULL) ) { __xuiRichNodeDestroy(pNode); return NULL; }
	pNode->fWidth = pDesc->fWidth;
	pNode->fHeight = pDesc->fHeight;
	pNode->fBaseline = pDesc->fBaseline > 0.0f ? pDesc->fBaseline : pDesc->fHeight;
	__xuiRichNodeAppend(pParagraph, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendImage(xui_rich_document pDocument, const xui_rich_image_desc_t* pDesc)
{
	xui_rich_node pNode;
	if ( !__xuiRichDocumentValid(pDocument) || pDesc == NULL || (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->pSurface == NULL && (pDesc->sSource == NULL || pDesc->sSource[0] == 0)) || pDesc->fWidth < 0.0f || pDesc->fHeight < 0.0f ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_IMAGE, 0);
	if ( pNode == NULL ) return NULL;
	pNode->pSurface = pDesc->pSurface;
	pNode->sResource = __xuiRichString(pDesc->sSource);
	pNode->sAltText = __xuiRichString(pDesc->sAltText);
	pNode->fWidth = pDesc->fWidth;
	pNode->fHeight = pDesc->fHeight;
	pNode->tParagraphStyle.iAlign = pDesc->iAlign;
	__xuiRichNodeAppend(pDocument->pRoot, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendTable(xui_rich_document pDocument, const xui_rich_table_desc_t* pDesc)
{
	xui_rich_node pNode;
	int n;
	if ( !__xuiRichDocumentValid(pDocument) || pDesc == NULL || (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     pDesc->iRows <= 0 || pDesc->iColumns <= 0 || pDesc->iRows > 1024 || pDesc->iColumns > 256 ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_TABLE, 0);
	if ( pNode == NULL ) return NULL;
	pNode->iRows = pDesc->iRows;
	pNode->iColumns = pDesc->iColumns;
	pNode->fWidth = pDesc->fWidth;
	pNode->fCellPadding = pDesc->fCellPadding > 0.0f ? pDesc->fCellPadding : 6.0f;
	pNode->fBorderWidth = pDesc->fBorderWidth > 0.0f ? pDesc->fBorderWidth : 1.0f;
	pNode->iBorderColor = pDesc->iBorderColor;
	pNode->iHeaderColor = pDesc->iHeaderColor;
	pNode->iCellColor = pDesc->iCellColor;
	n = pNode->iRows * pNode->iColumns;
	pNode->ppCellDocuments = (xui_rich_document*)xrtCalloc((size_t)n, sizeof(*pNode->ppCellDocuments));
	if ( pNode->ppCellDocuments == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
	__xuiRichNodeAppend(pDocument->pRoot, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

XUI_API xui_rich_node xuiRichDocumentAppendHorizontalRule(xui_rich_document pDocument)
{
	xui_rich_node pNode;
	if ( !__xuiRichDocumentValid(pDocument) ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_HORIZONTAL_RULE, 0);
	if ( pNode == NULL ) return NULL;
	__xuiRichNodeAppend(pDocument->pRoot, pNode);
	__xuiRichChanged(pDocument);
	return pNode;
}

static void __xuiRichNodeAssignFreshIds(xui_rich_document pDocument, xui_rich_node pNode,
	xui_rich_node pExtraRoot)
{
	xui_rich_node pChild;
	int i;
	pNode->iId = __xuiRichFreshIdAvoid(pDocument, pExtraRoot);
	if ( pNode->iType == XUI_RICH_NODE_TABLE && pNode->ppCellDocuments != NULL )
		for ( i = 0; i < pNode->iRows * pNode->iColumns; i++ )
			if ( pNode->ppCellDocuments[i] != NULL ) {
				pNode->ppCellDocuments[i]->pOwnerDocument = pDocument;
				pNode->ppCellDocuments[i]->iOwnerTableId = pNode->iId;
			}
	for ( pChild = pNode->pFirstChild; pChild != NULL; pChild = pChild->pNext )
		__xuiRichNodeAssignFreshIds(pDocument, pChild, pExtraRoot);
}

static xui_rich_node __xuiRichDocumentInsertObject(xui_rich_document pDocument, int iStart, int iEnd,
	xui_rich_node pObject)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	xui_rich_node pInserted;
	uint64_t iId;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pObject) || !__xuiRichAtomicBlockType(pObject->iType) ) return NULL;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return NULL;
	iStart = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iStart);
	iEnd = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iEnd);
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return NULL;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return NULL;
	memset(&tBuilder, 0, sizeof(tBuilder));
	tBuilder.pDocument = pDocument;
	tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK ) {
		pInserted = __xuiRichNodeClone(pDocument, pObject);
		if ( pInserted == NULL ) iRet = XUI_ERROR_OUT_OF_MEMORY;
		else {
			__xuiRichNodeAssignFreshIds(pDocument, pInserted, pRoot);
			iId = pInserted->iId;
			__xuiRichNodeAppend(pRoot, pInserted);
			tBuilder.pParagraph = NULL;
		}
	}
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return NULL; }
	(void)__xuiRichReplaceRoot(pDocument, pRoot);
	return __xuiRichFindNode(pDocument->pRoot, iId);
}

XUI_API xui_rich_node xuiRichDocumentInsertImage(xui_rich_document pDocument, int iStart, int iEnd, const xui_rich_image_desc_t* pDesc)
{
	xui_rich_node pTemp;
	xui_rich_node pResult;
	if ( !__xuiRichDocumentValid(pDocument) || pDesc == NULL || (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->pSurface == NULL && (pDesc->sSource == NULL || pDesc->sSource[0] == 0)) || pDesc->fWidth < 0.0f || pDesc->fHeight < 0.0f ) return NULL;
	pTemp = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_IMAGE, 0);
	if ( pTemp == NULL ) return NULL;
	pTemp->pSurface = pDesc->pSurface;
	pTemp->sResource = __xuiRichString(pDesc->sSource);
	pTemp->sAltText = __xuiRichString(pDesc->sAltText);
	pTemp->fWidth = pDesc->fWidth;
	pTemp->fHeight = pDesc->fHeight;
	pTemp->tParagraphStyle.iAlign = pDesc->iAlign;
	pResult = __xuiRichDocumentInsertObject(pDocument, iStart, iEnd, pTemp);
	__xuiRichNodeDestroy(pTemp);
	return pResult;
}

static xui_rich_node __xuiRichTextBlockAt(xui_rich_document pDocument, int iOffset)
{
	xui_rich_node pBlock;
	xui_rich_node pNode;
	int iAt = 0;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		int iEnd = iAt;
		if ( __xuiRichTextBlockType(pBlock->iType) ) {
			for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext )
				iEnd += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
			if ( iOffset >= iAt && iOffset <= iEnd ) return pBlock;
		} else if ( __xuiRichAtomicBlockType(pBlock->iType) ) iEnd += XUI_RICH_OBJECT_SIZE;
		iAt = iEnd + (pBlock->pNext != NULL ? 1 : 0);
	}
	return NULL;
}

XUI_API xui_rich_node xuiRichDocumentInsertInlineImage(xui_rich_document pDocument, int iStart, int iEnd,
	const xui_rich_image_desc_t* pDesc)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	xui_rich_node pBlock;
	xui_rich_node pEndBlock;
	xui_rich_node pImage;
	xui_rich_node pResult;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || pDesc == NULL ||
	     (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->pSurface == NULL && (pDesc->sSource == NULL || pDesc->sSource[0] == 0)) ||
	     pDesc->fWidth < 0.0f || pDesc->fHeight < 0.0f ) return NULL;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return NULL;
	iStart = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iStart);
	iEnd = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iEnd);
	pBlock = __xuiRichTextBlockAt(pDocument, iStart);
	pEndBlock = __xuiRichTextBlockAt(pDocument, iEnd);
	if ( pBlock == NULL || pEndBlock != pBlock ) return NULL;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return NULL;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return NULL;
	memset(&tBuilder, 0, sizeof(tBuilder)); tBuilder.pDocument = pDocument; tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK && tBuilder.pParagraph == NULL )
		iRet = __xuiRichBuilderBlock(&tBuilder, pBlock->iType, pBlock->iId, &pBlock->tParagraphStyle);
	pImage = iRet == XUI_OK ? __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_INLINE_IMAGE, 0) : NULL;
	if ( iRet == XUI_OK && pImage == NULL ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) {
		pImage->pSurface = pDesc->pSurface;
		pImage->sResource = __xuiRichString(pDesc->sSource);
		pImage->sAltText = __xuiRichString(pDesc->sAltText);
		if ( (pDesc->sSource != NULL && pImage->sResource == NULL) ||
		     (pDesc->sAltText != NULL && pImage->sAltText == NULL) ) {
			__xuiRichNodeDestroy(pImage); pImage = NULL; iRet = XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( iRet == XUI_OK ) {
			pImage->fWidth = pDesc->fWidth;
			pImage->fHeight = pDesc->fHeight;
			pImage->fBaseline = pDesc->fBaseline > 0.0f ? pDesc->fBaseline : pDesc->fHeight;
			__xuiRichNodeAppend(tBuilder.pParagraph, pImage);
		}
	}
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return NULL; }
	pResult = pImage;
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_OBJECT,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		iStart, iEnd, iStart, iStart + XUI_RICH_OBJECT_SIZE, pImage->iId);
	(void)__xuiRichReplaceRoot(pDocument, pRoot);
	return pResult;
}

XUI_API xui_rich_node xuiRichDocumentInsertTable(xui_rich_document pDocument, int iStart, int iEnd, const xui_rich_table_desc_t* pDesc)
{
	xui_rich_node pTemp;
	xui_rich_node pResult;
	int n;
	if ( !__xuiRichDocumentValid(pDocument) || pDesc == NULL || (pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ||
	     pDesc->iRows <= 0 || pDesc->iColumns <= 0 || pDesc->iRows > 1024 || pDesc->iColumns > 256 ) return NULL;
	pTemp = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_TABLE, 0);
	if ( pTemp == NULL ) return NULL;
	pTemp->iRows = pDesc->iRows; pTemp->iColumns = pDesc->iColumns; pTemp->fWidth = pDesc->fWidth;
	pTemp->fCellPadding = pDesc->fCellPadding > 0.0f ? pDesc->fCellPadding : 6.0f;
	pTemp->fBorderWidth = pDesc->fBorderWidth > 0.0f ? pDesc->fBorderWidth : 1.0f;
	pTemp->iBorderColor = pDesc->iBorderColor; pTemp->iHeaderColor = pDesc->iHeaderColor; pTemp->iCellColor = pDesc->iCellColor;
	n = pTemp->iRows * pTemp->iColumns;
	pTemp->ppCellDocuments = (xui_rich_document*)xrtCalloc((size_t)n, sizeof(*pTemp->ppCellDocuments));
	if ( pTemp->ppCellDocuments == NULL ) { __xuiRichNodeDestroy(pTemp); return NULL; }
	pResult = __xuiRichDocumentInsertObject(pDocument, iStart, iEnd, pTemp);
	__xuiRichNodeDestroy(pTemp);
	return pResult;
}

XUI_API xui_rich_node xuiRichDocumentInsertHorizontalRule(xui_rich_document pDocument, int iStart, int iEnd)
{
	xui_rich_node pTemp;
	xui_rich_node pResult;
	if ( !__xuiRichDocumentValid(pDocument) ) return NULL;
	pTemp = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_HORIZONTAL_RULE, 0);
	if ( pTemp == NULL ) return NULL;
	pResult = __xuiRichDocumentInsertObject(pDocument, iStart, iEnd, pTemp);
	__xuiRichNodeDestroy(pTemp);
	return pResult;
}

static xui_rich_node __xuiRichFindNode(xui_rich_node pNode, uint64_t iId)
{
	xui_rich_node pChild;
	xui_rich_node pFound;
	if ( pNode->iId == iId ) return pNode;
	for ( pChild = pNode->pFirstChild; pChild != NULL; pChild = pChild->pNext ) {
		pFound = __xuiRichFindNode(pChild, iId);
		if ( pFound != NULL ) return pFound;
	}
	return NULL;
}

XUI_API xui_rich_node xuiRichDocumentFindNode(xui_rich_document pDocument, xui_document_node_id_t iNodeId)
{
	return __xuiRichDocumentValid(pDocument) ? __xuiRichFindNode(pDocument->pRoot, iNodeId) : NULL;
}

XUI_API xui_rich_node xuiRichNodeGetFirstChild(xui_rich_node pNode) { return __xuiRichNodeValid(pNode) ? pNode->pFirstChild : NULL; }
XUI_API xui_rich_node xuiRichNodeGetNextSibling(xui_rich_node pNode) { return __xuiRichNodeValid(pNode) ? pNode->pNext : NULL; }

XUI_API int xuiRichNodeGetInfo(xui_rich_node pNode, xui_rich_node_info_t* pInfo)
{
	if ( !__xuiRichNodeValid(pNode) || pInfo == NULL || (pInfo->iSize != 0 && pInfo->iSize < sizeof(*pInfo)) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iId = pNode->iId;
	pInfo->iParentId = pNode->pParent != NULL ? pNode->pParent->iId : 0;
	pInfo->iType = pNode->iType;
	pInfo->sText = pNode->sText;
	pInfo->iTextSize = pNode->iTextSize;
	pInfo->pWidget = pNode->pWidget;
	pInfo->fWidth = pNode->fWidth;
	pInfo->fHeight = pNode->fHeight;
	pInfo->fBaseline = pNode->fBaseline;
	pInfo->tStyle = pNode->tStyle;
	pInfo->tParagraphStyle = pNode->tParagraphStyle;
	pInfo->sResource = pNode->sResource;
	pInfo->sAltText = pNode->sAltText;
	pInfo->pSurface = pNode->pSurface;
	pInfo->iRows = pNode->iRows;
	pInfo->iColumns = pNode->iColumns;
	pInfo->fCellPadding = pNode->fCellPadding;
	pInfo->fBorderWidth = pNode->fBorderWidth;
	pInfo->iBorderColor = pNode->iBorderColor;
	pInfo->iHeaderColor = pNode->iHeaderColor;
	pInfo->iCellColor = pNode->iCellColor;
	return XUI_OK;
}

XUI_API int xuiRichNodeSetText(xui_rich_document pDocument, xui_rich_node pNode, const char* sText)
{
	char* sNew;
	int iSize;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pNode) || !__xuiRichInlineTextType(pNode->iType) || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iSize = (int)strlen(sText);
	if ( !__xuiRichUtf8Valid(sText, iSize) || strchr(sText, '\n') != NULL || strchr(sText, '\r') != NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	sNew = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(sNew, sText, (size_t)iSize + 1u);
	xrtFree(pNode->sText);
	pNode->sText = sNew;
	pNode->iTextSize = iSize;
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichNodeSetStyle(xui_rich_document pDocument, xui_rich_node pNode, const xui_rich_text_style_t* pStyle)
{
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pNode) || !__xuiRichInlineTextType(pNode->iType) || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pNode->tStyle = __xuiRichStyle(pStyle);
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichNodeSetParagraphStyle(xui_rich_document pDocument, xui_rich_node pNode, const xui_rich_paragraph_style_t* pStyle)
{
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pNode) ||
	     (!__xuiRichTextBlockType(pNode->iType) && !__xuiRichAtomicBlockType(pNode->iType)) || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pNode->tParagraphStyle = __xuiRichParagraphStyle(pStyle);
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichNodeSetResource(xui_rich_document pDocument, xui_rich_node pNode, const char* sResource, const char* sAltText)
{
	char* sNewResource;
	char* sNewAlt;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pNode) ||
	     (pNode->iType != XUI_RICH_NODE_LINK && pNode->iType != XUI_RICH_NODE_IMAGE &&
	      pNode->iType != XUI_RICH_NODE_INLINE_IMAGE) ) return XUI_ERROR_INVALID_ARGUMENT;
	sNewResource = __xuiRichString(sResource);
	sNewAlt = __xuiRichString(sAltText);
	if ( (sResource != NULL && sNewResource == NULL) || (sAltText != NULL && sNewAlt == NULL) ) { xrtFree(sNewResource); xrtFree(sNewAlt); return XUI_ERROR_OUT_OF_MEMORY; }
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) { xrtFree(sNewResource); xrtFree(sNewAlt); return iRet; }
	xrtFree(pNode->sResource); xrtFree(pNode->sAltText);
	pNode->sResource = sNewResource; pNode->sAltText = sNewAlt;
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichImageSetSurface(xui_rich_document pDocument, xui_rich_node pImage,
	xui_surface pSurface, float fWidth, float fHeight)
{
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pImage) ||
	     (pImage->iType != XUI_RICH_NODE_IMAGE && pImage->iType != XUI_RICH_NODE_INLINE_IMAGE) ||
	     fWidth < 0.0f || fHeight < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pImage->pSurface = pSurface; pImage->fWidth = fWidth; pImage->fHeight = fHeight;
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichTableSetCellText(xui_rich_document pDocument, xui_rich_node pTable, int iRow, int iColumn, const char* sText, const xui_rich_text_style_t* pStyle)
{
	xui_rich_document pCell;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pTable) || pTable->iType != XUI_RICH_NODE_TABLE ||
	     iRow < 0 || iRow >= pTable->iRows || iColumn < 0 || iColumn >= pTable->iColumns || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiRichTableGetCellDocument(pDocument, pTable, iRow, iColumn, &pCell) != XUI_OK )
		return XUI_ERROR_OUT_OF_MEMORY;
	return xuiRichDocumentReplace(pCell, 0, xuiRichDocumentGetLength(pCell), sText, pStyle);
}

XUI_API const char* xuiRichTableGetCellText(xui_rich_node pTable, int iRow, int iColumn)
{
	if ( !__xuiRichNodeValid(pTable) || pTable->iType != XUI_RICH_NODE_TABLE ||
	     iRow < 0 || iRow >= pTable->iRows || iColumn < 0 || iColumn >= pTable->iColumns ) return NULL;
	if ( pTable->ppCellDocuments == NULL || pTable->ppCellDocuments[iRow * pTable->iColumns + iColumn] == NULL ) return "";
	return xuiRichDocumentGetText(pTable->ppCellDocuments[iRow * pTable->iColumns + iColumn]);
}

XUI_API int xuiRichTableGetCellStyle(xui_rich_node pTable, int iRow, int iColumn, xui_rich_text_style_t* pStyle)
{
	if ( !__xuiRichNodeValid(pTable) || pTable->iType != XUI_RICH_NODE_TABLE || pStyle == NULL ||
	     iRow < 0 || iRow >= pTable->iRows || iColumn < 0 || iColumn >= pTable->iColumns ) return XUI_ERROR_INVALID_ARGUMENT;
	*pStyle = __xuiRichStyle(NULL);
	if ( pTable->ppCellDocuments != NULL ) {
		xui_rich_document pCell = pTable->ppCellDocuments[iRow * pTable->iColumns + iColumn];
		if ( pCell != NULL ) {
			xui_rich_node pBlock = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pCell));
			xui_rich_node pRun = pBlock != NULL ? xuiRichNodeGetFirstChild(pBlock) : NULL;
			xui_rich_node_info_t tInfo;
			memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
			if ( pRun != NULL && xuiRichNodeGetInfo(pRun, &tInfo) == XUI_OK ) *pStyle = tInfo.tStyle;
		}
	}
	pStyle->iSize = sizeof(*pStyle);
	return XUI_OK;
}

XUI_API int xuiRichTableGetCellDocument(xui_rich_document pDocument, xui_rich_node pTable,
	int iRow, int iColumn, xui_rich_document* ppCellDocument)
{
	xui_rich_document pCell;
	int iIndex;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pTable) ||
	     pTable->iType != XUI_RICH_NODE_TABLE || pTable->pParent == NULL ||
	     xuiRichDocumentFindNode(pDocument, pTable->iId) != pTable || ppCellDocument == NULL ||
	     iRow < 0 || iRow >= pTable->iRows || iColumn < 0 || iColumn >= pTable->iColumns )
		return XUI_ERROR_INVALID_ARGUMENT;
	*ppCellDocument = NULL;
	iIndex = iRow * pTable->iColumns + iColumn;
	pCell = pTable->ppCellDocuments[iIndex];
	if ( pCell == NULL ) {
		iRet = xuiRichDocumentCreate(&pCell);
		if ( iRet != XUI_OK ) return iRet;
		pCell->pOwnerDocument = pDocument;
		pCell->iOwnerTableId = pTable->iId;
		pTable->ppCellDocuments[iIndex] = pCell;
	}
	*ppCellDocument = pCell;
	return XUI_OK;
}

XUI_API int xuiRichTableResize(xui_rich_document pDocument, xui_rich_node pTable, int iRows, int iColumns)
{
	xui_rich_document* ppCellDocuments;
	int r, c, iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pTable) || pTable->iType != XUI_RICH_NODE_TABLE ||
	     iRows <= 0 || iColumns <= 0 || iRows > 1024 || iColumns > 256 ) return XUI_ERROR_INVALID_ARGUMENT;
	ppCellDocuments = (xui_rich_document*)xrtCalloc((size_t)iRows * (size_t)iColumns, sizeof(*ppCellDocuments));
	if ( ppCellDocuments == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	for ( r = 0; r < iRows; r++ ) for ( c = 0; c < iColumns; c++ ) {
		int iNew = r * iColumns + c;
		if ( r < pTable->iRows && c < pTable->iColumns ) {
			int iOld = r * pTable->iColumns + c;
			ppCellDocuments[iNew] = pTable->ppCellDocuments[iOld];
		}
	}
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) { xrtFree(ppCellDocuments); return iRet; }
	for ( r = 0; r < pTable->iRows; r++ ) for ( c = 0; c < pTable->iColumns; c++ ) {
		if ( r >= iRows || c >= iColumns ) {
			xui_rich_document pCell = pTable->ppCellDocuments[r * pTable->iColumns + c];
			if ( pCell != NULL ) xuiRichDocumentDestroy(pCell);
		}
	}
	xrtFree(pTable->ppCellDocuments);
	pTable->ppCellDocuments = ppCellDocuments; pTable->iRows = iRows; pTable->iColumns = iColumns;
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_OBJECT,
		XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY, 0, 0, 0, 0, pTable->iId);
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichDocumentGetLength(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) || __xuiRichEnsureFlat(pDocument) != XUI_OK ) return 0;
	return pDocument->iFlatLength;
}

XUI_API const char* xuiRichDocumentGetText(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) || __xuiRichEnsureFlat(pDocument) != XUI_OK ) return NULL;
	return pDocument->sFlatText;
}

XUI_API int xuiRichDocumentReplace(xui_rich_document pDocument, int iStart, int iEnd, const char* sText, const xui_rich_text_style_t* pStyle)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	xui_rich_text_style_t tStyle;
	int iLength;
	int iTextSize;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iTextSize = (int)strlen(sText);
	if ( !__xuiRichUtf8Valid(sText, iTextSize) ) return XUI_ERROR_INVALID_ARGUMENT;
	iStart = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iStart);
	iEnd = xuiInternalTextGraphemeClamp(pDocument->sFlatText, iLength, iEnd);
	iRet = __xuiRichTryLocalReplace(pDocument, iStart, iEnd, sText, iTextSize, pStyle);
	if ( iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
	iRet = __xuiRichTryBlockReplace(pDocument, iStart, iEnd, sText, iTextSize, pStyle);
	if ( iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(&tBuilder, 0, sizeof(tBuilder));
	tBuilder.pDocument = pDocument;
	tBuilder.pRoot = pRoot;
	tStyle = __xuiRichStyle(pStyle);
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderInput(&tBuilder, sText, iTextSize, &tStyle);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet == XUI_OK && pRoot->pFirstChild == NULL ) iRet = __xuiRichBuilderParagraph(&tBuilder, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return iRet; }
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_REPLACE,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		iStart, iEnd, iStart, iStart + iTextSize, 0);
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

XUI_API int xuiRichDocumentApplyStyle(xui_rich_document pDocument, int iStart, int iEnd, const xui_rich_text_style_t* pStyle)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	xui_rich_text_style_t tStyle;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd <= iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(&tBuilder, 0, sizeof(tBuilder));
	tBuilder.pDocument = pDocument;
	tBuilder.pRoot = pRoot;
	tStyle = __xuiRichStyle(pStyle);
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iStart, iEnd, &tStyle, 1);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return iRet; }
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_STYLE,
		XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_GEOMETRY, iStart, iEnd, iStart, iEnd, 0);
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

XUI_API int xuiRichDocumentUpdateStyleFlags(xui_rich_document pDocument, int iStart, int iEnd,
	uint32_t iSetFlags, uint32_t iClearFlags)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || (iSetFlags & iClearFlags) != 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd <= iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(&tBuilder, 0, sizeof(tBuilder)); tBuilder.pDocument = pDocument; tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSliceEx(&tBuilder, pDocument, iStart, iEnd, NULL, 0, iSetFlags, iClearFlags, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return iRet; }
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_STYLE,
		XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_GEOMETRY, iStart, iEnd, iStart, iEnd, 0);
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

XUI_API int xuiRichDocumentApplyLink(xui_rich_document pDocument, int iStart, int iEnd, const char* sUrl)
{
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || sUrl == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd <= iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(&tBuilder, 0, sizeof(tBuilder)); tBuilder.pDocument = pDocument; tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSliceEx(&tBuilder, pDocument, iStart, iEnd, NULL, 0, 0, 0, sUrl, 1);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return iRet; }
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_STYLE,
		XUI_RICH_CHANGE_FORMAT, iStart, iEnd, iStart, iEnd, 0);
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

XUI_API int xuiRichDocumentApplyParagraphStyle(xui_rich_document pDocument, int iStart, int iEnd, const xui_rich_paragraph_style_t* pStyle)
{
	xui_rich_node pBlock;
	int iAt = 0;
	int iBlockStart;
	int iBlockEnd;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		xui_rich_node pNode;
		iBlockStart = iAt;
		if ( __xuiRichAtomicBlockType(pBlock->iType) ) iAt += XUI_RICH_OBJECT_SIZE;
		else for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext )
			iAt += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
		iBlockEnd = iAt;
		if ( pBlock->pNext != NULL ) iAt++;
		if ( (iStart == iEnd && iStart >= iBlockStart && iStart <= iBlockEnd) ||
		     (iEnd > iBlockStart && iStart <= iBlockEnd) ) pBlock->tParagraphStyle = __xuiRichParagraphStyle(pStyle);
	}
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_PARAGRAPH,
		XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_GEOMETRY, iStart, iEnd, iStart, iEnd, 0);
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichDocumentSetBlockType(xui_rich_document pDocument, int iStart, int iEnd, int iNodeType)
{
	xui_rich_node pBlock;
	int iAt = 0;
	int iBlockStart;
	int iBlockEnd;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichTextBlockType(iNodeType) ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	for ( pBlock = pDocument->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext ) {
		xui_rich_node pNode;
		iBlockStart = iAt;
		if ( __xuiRichAtomicBlockType(pBlock->iType) ) iAt += XUI_RICH_OBJECT_SIZE;
		else for ( pNode = pBlock->pFirstChild; pNode != NULL; pNode = pNode->pNext )
			iAt += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
		iBlockEnd = iAt;
		if ( pBlock->pNext != NULL ) iAt++;
		if ( __xuiRichTextBlockType(pBlock->iType) &&
		     ((iStart == iEnd && iStart >= iBlockStart && iStart <= iBlockEnd) ||
		      (iEnd > iBlockStart && iStart <= iBlockEnd)) ) pBlock->iType = iNodeType;
	}
	__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_PARAGRAPH,
		XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		iStart, iEnd, iStart, iEnd, 0);
	__xuiRichChanged(pDocument);
	return XUI_OK;
}

XUI_API int xuiRichDocumentCloneRange(xui_rich_document pDocument, int iStart, int iEnd, xui_rich_document* ppFragment)
{
	xui_rich_document pFragment;
	xui_rich_builder_t tBuilder;
	xui_rich_node pRoot;
	int iLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || ppFragment == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppFragment = NULL;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiRichDocumentCreate(&pFragment);
	if ( iRet != XUI_OK ) return iRet;
	pRoot = __xuiRichNodeCreate(pFragment, XUI_RICH_NODE_DOCUMENT, 0);
	if ( pRoot == NULL ) { xuiRichDocumentDestroy(pFragment); return XUI_ERROR_OUT_OF_MEMORY; }
	memset(&tBuilder, 0, sizeof(tBuilder)); tBuilder.pDocument = pFragment; tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iStart, iEnd, NULL, 0);
	if ( iRet == XUI_OK && pRoot->pFirstChild == NULL ) iRet = __xuiRichBuilderParagraph(&tBuilder, 0);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); xuiRichDocumentDestroy(pFragment); return iRet; }
	__xuiRichNodeDestroy(pFragment->pRoot);
	pFragment->pRoot = pRoot;
	pFragment->bFlatDirty = 1;
	pFragment->iUndoCount = 0;
	*ppFragment = pFragment;
	return XUI_OK;
}

XUI_API int xuiRichDocumentInsertDocument(xui_rich_document pDocument, int iStart, int iEnd, xui_rich_document pFragment)
{
	xui_rich_builder_t tBuilder;
	xui_rich_document pFreshFragment = NULL;
	xui_rich_node pRoot;
	xui_rich_node pBlock;
	int iLength;
	int iFragmentLength;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichDocumentValid(pFragment) ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	iFragmentLength = xuiRichDocumentGetLength(pFragment);
	if ( iStart < 0 || iEnd < iStart || iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichBeginChange(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiRichDocumentCloneRange(pFragment, 0, iFragmentLength, &pFreshFragment);
	if ( iRet != XUI_OK ) return iRet;
	for ( pBlock = pFreshFragment->pRoot->pFirstChild; pBlock != NULL; pBlock = pBlock->pNext )
		__xuiRichNodeAssignFreshIds(pDocument, pBlock, pFreshFragment->pRoot);
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, pDocument->pRoot->iId);
	if ( pRoot == NULL ) { xuiRichDocumentDestroy(pFreshFragment); return XUI_ERROR_OUT_OF_MEMORY; }
	memset(&tBuilder, 0, sizeof(tBuilder)); tBuilder.pDocument = pDocument; tBuilder.pRoot = pRoot;
	iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, 0, iStart, NULL, 0);
	if ( iRet == XUI_OK && iFragmentLength > 0 ) iRet = __xuiRichBuilderSlice(&tBuilder, pFreshFragment, 0, iFragmentLength, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichBuilderSlice(&tBuilder, pDocument, iEnd, iLength, NULL, 0);
	if ( iRet == XUI_OK && pRoot->pFirstChild == NULL ) iRet = __xuiRichBuilderParagraph(&tBuilder, 0);
	xuiRichDocumentDestroy(pFreshFragment);
	if ( iRet != XUI_OK ) { __xuiRichNodeDestroy(pRoot); return iRet; }
	return __xuiRichReplaceRoot(pDocument, pRoot);
}

int xuiInternalRichDocumentNodeOffset(xui_rich_document pDocument, xui_rich_node pTarget, int* pOffset)
{
	xui_rich_node pParagraph;
	xui_rich_node pNode;
	int iAt = 0;
	if ( !__xuiRichDocumentValid(pDocument) || !__xuiRichNodeValid(pTarget) || pOffset == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( pParagraph = pDocument->pRoot->pFirstChild; pParagraph != NULL; pParagraph = pParagraph->pNext ) {
		if ( pParagraph == pTarget ) { *pOffset = iAt; return XUI_OK; }
		if ( __xuiRichAtomicBlockType(pParagraph->iType) ) {
			iAt += XUI_RICH_OBJECT_SIZE;
			if ( pParagraph->pNext != NULL ) iAt++;
			continue;
		}
		for ( pNode = pParagraph->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
			if ( pNode == pTarget ) { *pOffset = iAt; return XUI_OK; }
			iAt += __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
		}
		if ( pParagraph->pNext != NULL ) iAt++;
	}
	return XUI_ERROR_FILE_NOT_FOUND;
}

XUI_API int xuiRichDocumentOffsetToPosition(xui_rich_document pDocument, int iOffset, xui_document_position_t* pPosition)
{
	xui_rich_node pParagraph;
	xui_rich_node pNode;
	int iAt = 0;
	int iSize;
	int iLength;
	if ( !__xuiRichDocumentValid(pDocument) || pPosition == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiRichDocumentGetLength(pDocument);
	if ( iOffset < 0 || iOffset > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( pParagraph = pDocument->pRoot->pFirstChild; pParagraph != NULL; pParagraph = pParagraph->pNext ) {
		if ( __xuiRichAtomicBlockType(pParagraph->iType) ) {
			if ( iOffset <= iAt + XUI_RICH_OBJECT_SIZE ) {
				pPosition->iNodeId = pParagraph->iId;
				pPosition->iByteOffset = iOffset - iAt;
				pPosition->iAffinity = pPosition->iByteOffset == XUI_RICH_OBJECT_SIZE ? XUI_RICH_AFFINITY_AFTER : XUI_RICH_AFFINITY_BEFORE;
				return XUI_OK;
			}
			iAt += XUI_RICH_OBJECT_SIZE;
			if ( pParagraph->pNext != NULL ) iAt++;
			continue;
		}
		for ( pNode = pParagraph->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
			iSize = __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
			if ( iOffset <= iAt + iSize ) {
				pPosition->iNodeId = pNode->iId;
				pPosition->iByteOffset = iOffset - iAt;
				pPosition->iAffinity = pPosition->iByteOffset == iSize ? XUI_RICH_AFFINITY_AFTER : XUI_RICH_AFFINITY_BEFORE;
				return XUI_OK;
			}
			iAt += iSize;
		}
		if ( pParagraph->pNext != NULL ) {
			if ( iOffset == iAt ) {
				pPosition->iNodeId = pParagraph->iId;
				pPosition->iByteOffset = 0;
				pPosition->iAffinity = XUI_RICH_AFFINITY_AFTER;
				return XUI_OK;
			}
			iAt++;
		}
	}
	pPosition->iNodeId = pDocument->pRoot->pLastChild->iId;
	pPosition->iByteOffset = 0;
	pPosition->iAffinity = XUI_RICH_AFFINITY_AFTER;
	return XUI_OK;
}

XUI_API int xuiRichDocumentPositionToOffset(xui_rich_document pDocument, const xui_document_position_t* pPosition, int* pOffset)
{
	xui_rich_node pNode;
	int iBase;
	int iSize;
	if ( !__xuiRichDocumentValid(pDocument) || pPosition == NULL || pOffset == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pNode = xuiRichDocumentFindNode(pDocument, pPosition->iNodeId);
	if ( pNode == NULL ) return XUI_ERROR_FILE_NOT_FOUND;
	if ( xuiInternalRichDocumentNodeOffset(pDocument, pNode, &iBase) != XUI_OK ) return XUI_ERROR_FILE_NOT_FOUND;
	if ( __xuiRichTextBlockType(pNode->iType) ) { *pOffset = iBase; return XUI_OK; }
	iSize = __xuiRichInlineTextType(pNode->iType) ? pNode->iTextSize : XUI_RICH_OBJECT_SIZE;
	if ( pPosition->iByteOffset < 0 || pPosition->iByteOffset > iSize ) return XUI_ERROR_INVALID_ARGUMENT;
	*pOffset = iBase + pPosition->iByteOffset;
	return XUI_OK;
}

static int __xuiRichApplyHistory(xui_rich_document pDocument, xui_rich_history_t* pFrom, int* pFromCount,
	xui_rich_history_t* pTo, int* pToCount, int bUndo)
{
	xui_rich_history_t tState;
	xui_rich_node pNode;
	xui_rich_node pBefore;
	xui_rich_node pAfter;
	xui_rich_node pCurrent;
	xui_rich_node pNext;
	xui_rich_node pOutgoing;
	xui_rich_node pIncoming;
	const char* sExpected;
	const char* sInsert;
	int iExpectedSize;
	int iInsertSize;
	int iNodeBase;
	int iOldLength;
	int iNewLength;
	int iRet;
	if ( *pFromCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	if ( pFrom[*pFromCount - 1].iKind == XUI_RICH_HISTORY_BLOCK_REPLACE ) {
		tState = pFrom[*pFromCount - 1];
		pBefore = tState.iBeforeBlockId != 0 ? xuiRichDocumentFindNode(pDocument, tState.iBeforeBlockId) : NULL;
		pAfter = tState.iAfterBlockId != 0 ? xuiRichDocumentFindNode(pDocument, tState.iAfterBlockId) : NULL;
		if ( (pBefore != NULL && pBefore->pParent != pDocument->pRoot) ||
		     (pAfter != NULL && pAfter->pParent != pDocument->pRoot) ) return XUI_ERROR;
		pOutgoing = bUndo ? tState.pNewBlocks : tState.pOldBlocks;
		pIncoming = bUndo ? tState.pOldBlocks : tState.pNewBlocks;
		if ( pOutgoing->pFirstChild != NULL || pIncoming->pFirstChild == NULL ) return XUI_ERROR;
		iExpectedSize = bUndo ? tState.iNewSize : tState.iOldSize;
		iInsertSize = bUndo ? tState.iOldSize : tState.iNewSize;
		sExpected = bUndo ? tState.sNewText : tState.sOldText;
		sInsert = bUndo ? tState.sOldText : tState.sNewText;
		if ( __xuiRichEnsureFlat(pDocument) != XUI_OK || tState.iNodeOffset < 0 ||
		     tState.iNodeOffset + iExpectedSize > pDocument->iFlatLength ||
		     (iExpectedSize > 0 && memcmp(pDocument->sFlatText + tState.iNodeOffset, sExpected, (size_t)iExpectedSize) != 0) )
			return XUI_ERROR;
		iRet = __xuiRichFlatReserve(pDocument, pDocument->iFlatLength - iExpectedSize + iInsertSize + 1);
		if ( iRet != XUI_OK ) return iRet;
		pCurrent = pBefore != NULL ? pBefore->pNext : pDocument->pRoot->pFirstChild;
		while ( pCurrent != pAfter ) {
			if ( pCurrent == NULL ) return XUI_ERROR;
			pNext = pCurrent->pNext;
			__xuiRichNodeDetach(pCurrent);
			__xuiRichNodeAppend(pOutgoing, pCurrent);
			pCurrent = pNext;
		}
		__xuiRichMoveChildren(pIncoming, pDocument->pRoot, pAfter);
		iOldLength = pDocument->iFlatLength;
		iNewLength = iOldLength - iExpectedSize + iInsertSize;
		memmove(pDocument->sFlatText + tState.iNodeOffset + iInsertSize,
			pDocument->sFlatText + tState.iNodeOffset + iExpectedSize,
			(size_t)(iOldLength - tState.iNodeOffset - iExpectedSize) + 1u);
		if ( iInsertSize > 0 ) memcpy(pDocument->sFlatText + tState.iNodeOffset, sInsert, (size_t)iInsertSize);
		pDocument->iFlatLength = iNewLength;
		pDocument->bFlatDirty = 0;
		pDocument->bApplyingHistory = 1;
		__xuiRichSetChange(pDocument, XUI_RICH_CHANGE_HISTORY,
			XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
			tState.iNodeOffset, tState.iNodeOffset + iExpectedSize,
			tState.iNodeOffset, tState.iNodeOffset + iInsertSize, 0);
		__xuiRichChangedEx(pDocument, 1);
		pDocument->bApplyingHistory = 0;
		memset(&pFrom[--(*pFromCount)], 0, sizeof(*pFrom));
		__xuiRichHistoryAppend(pTo, pToCount, &tState);
		return XUI_OK;
	}
	if ( pFrom[*pFromCount - 1].iKind == XUI_RICH_HISTORY_TEXT_REPLACE ) {
		tState = pFrom[*pFromCount - 1];
		pNode = xuiRichDocumentFindNode(pDocument, tState.iNodeId);
		if ( pNode == NULL || !__xuiRichInlineTextType(pNode->iType) || __xuiRichEnsureFlat(pDocument) != XUI_OK )
			return XUI_ERROR_FILE_NOT_FOUND;
		sExpected = bUndo ? tState.sNewText : tState.sOldText;
		iExpectedSize = bUndo ? tState.iNewSize : tState.iOldSize;
		sInsert = bUndo ? tState.sOldText : tState.sNewText;
		iInsertSize = bUndo ? tState.iOldSize : tState.iNewSize;
		if ( tState.iNodeOffset < 0 || tState.iNodeOffset + iExpectedSize > pNode->iTextSize ||
		     (iExpectedSize > 0 && memcmp(pNode->sText + tState.iNodeOffset, sExpected, (size_t)iExpectedSize) != 0) )
			return XUI_ERROR;
		if ( xuiInternalRichDocumentNodeOffset(pDocument, pNode, &iNodeBase) != XUI_OK ) return XUI_ERROR_FILE_NOT_FOUND;
		pDocument->bApplyingHistory = 1;
		iRet = __xuiRichReplaceNodeBytes(pDocument, pNode, tState.iNodeOffset, iExpectedSize,
			sInsert, iInsertSize, iNodeBase + tState.iNodeOffset, XUI_RICH_CHANGE_HISTORY);
		pDocument->bApplyingHistory = 0;
		if ( iRet != XUI_OK ) return iRet;
		memset(&pFrom[--(*pFromCount)], 0, sizeof(*pFrom));
		__xuiRichHistoryAppend(pTo, pToCount, &tState);
		return XUI_OK;
	}
	iOldLength = xuiRichDocumentGetLength(pDocument);
	iRet = __xuiRichHistoryPush(pDocument, pTo, pToCount);
	if ( iRet != XUI_OK ) return iRet;
	tState = pFrom[--(*pFromCount)];
	pTo[*pToCount - 1].tChange = tState.tChange;
	pDocument->bApplyingHistory = 1;
	__xuiRichNodeDestroy(pDocument->pRoot);
	pDocument->pRoot = tState.pRoot;
	pDocument->iNextId = tState.iNextId;
	pDocument->bFlatDirty = 1;
	iNewLength = xuiRichDocumentGetLength(pDocument);
	if ( tState.tChange.iSize != 0 ) {
		if ( bUndo ) __xuiRichSetChange(pDocument, XUI_RICH_CHANGE_HISTORY, tState.tChange.iFlags,
			tState.tChange.iNewStart, tState.tChange.iNewEnd,
			tState.tChange.iOldStart, tState.tChange.iOldEnd, tState.tChange.iNodeId);
		else __xuiRichSetChange(pDocument, XUI_RICH_CHANGE_HISTORY, tState.tChange.iFlags,
			tState.tChange.iOldStart, tState.tChange.iOldEnd,
			tState.tChange.iNewStart, tState.tChange.iNewEnd, tState.tChange.iNodeId);
	} else __xuiRichSetChange(pDocument, XUI_RICH_CHANGE_HISTORY,
		XUI_RICH_CHANGE_TEXT | XUI_RICH_CHANGE_FORMAT | XUI_RICH_CHANGE_STRUCTURE | XUI_RICH_CHANGE_GEOMETRY,
		0, iOldLength, 0, iNewLength, 0);
	__xuiRichChanged(pDocument);
	pDocument->bApplyingHistory = 0;
	return XUI_OK;
}

XUI_API int xuiRichDocumentUndo(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiRichApplyHistory(pDocument, pDocument->arrUndo, &pDocument->iUndoCount, pDocument->arrRedo, &pDocument->iRedoCount, 1);
}

XUI_API int xuiRichDocumentRedo(xui_rich_document pDocument)
{
	if ( !__xuiRichDocumentValid(pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiRichApplyHistory(pDocument, pDocument->arrRedo, &pDocument->iRedoCount, pDocument->arrUndo, &pDocument->iUndoCount, 0);
}

XUI_API int xuiRichDocumentCanUndo(xui_rich_document pDocument) { return __xuiRichDocumentValid(pDocument) && pDocument->iUndoCount > 0; }
XUI_API int xuiRichDocumentCanRedo(xui_rich_document pDocument) { return __xuiRichDocumentValid(pDocument) && pDocument->iRedoCount > 0; }
XUI_API uint32_t xuiRichDocumentGetVersion(xui_rich_document pDocument) { return __xuiRichDocumentValid(pDocument) ? pDocument->iVersion : 0; }
XUI_API int xuiRichDocumentGetLastChange(xui_rich_document pDocument, xui_rich_change_t* pChange)
{
	if ( !__xuiRichDocumentValid(pDocument) || pChange == NULL ||
	     (pChange->iSize != 0 && pChange->iSize < sizeof(*pChange)) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pChange = pDocument->tLastChange;
	pChange->iSize = sizeof(*pChange);
	return XUI_OK;
}

/* The native rich-document format stores stable model data only. Runtime font,
   surface and widget pointers must be reconstructed by the host. */
static const char* __xuiRichSerialNodeName(int iType)
{
	switch ( iType ) {
		case XUI_RICH_NODE_PARAGRAPH: return "paragraph";
		case XUI_RICH_NODE_TEXT: return "text";
		case XUI_RICH_NODE_HEADING: return "heading";
		case XUI_RICH_NODE_BLOCK_QUOTE: return "block_quote";
		case XUI_RICH_NODE_LIST_ITEM: return "list_item";
		case XUI_RICH_NODE_LINK: return "link";
		case XUI_RICH_NODE_IMAGE: return "image";
		case XUI_RICH_NODE_TABLE: return "table";
		case XUI_RICH_NODE_HORIZONTAL_RULE: return "horizontal_rule";
		case XUI_RICH_NODE_INLINE_IMAGE: return "inline_image";
		default: return NULL;
	}
}

static int __xuiRichSerialNodeType(const char* sName)
{
	if ( strcmp(sName, "paragraph") == 0 ) return XUI_RICH_NODE_PARAGRAPH;
	if ( strcmp(sName, "text") == 0 ) return XUI_RICH_NODE_TEXT;
	if ( strcmp(sName, "heading") == 0 ) return XUI_RICH_NODE_HEADING;
	if ( strcmp(sName, "block_quote") == 0 ) return XUI_RICH_NODE_BLOCK_QUOTE;
	if ( strcmp(sName, "list_item") == 0 ) return XUI_RICH_NODE_LIST_ITEM;
	if ( strcmp(sName, "link") == 0 ) return XUI_RICH_NODE_LINK;
	if ( strcmp(sName, "image") == 0 ) return XUI_RICH_NODE_IMAGE;
	if ( strcmp(sName, "table") == 0 ) return XUI_RICH_NODE_TABLE;
	if ( strcmp(sName, "horizontal_rule") == 0 ) return XUI_RICH_NODE_HORIZONTAL_RULE;
	if ( strcmp(sName, "inline_image") == 0 ) return XUI_RICH_NODE_INLINE_IMAGE;
	return 0;
}

static int __xuiRichSerialWriteTextStyle(xjsonwriter* pWriter, const xui_rich_text_style_t* pStyle)
{
	if ( pStyle->pFont != NULL ) return XUI_ERROR_UNSUPPORTED;
	if ( !isfinite(pStyle->fBaselineShift) || !isfinite(pStyle->fFontSize) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiJsonWriterObjectKey(pWriter, "textStyle") ||
	     !__xuiJsonWriterIntKey(pWriter, "textColor", (int64)pStyle->iTextColor) ||
	     !__xuiJsonWriterIntKey(pWriter, "backgroundColor", (int64)pStyle->iBackgroundColor) ||
	     !__xuiJsonWriterIntKey(pWriter, "flags", (int64)pStyle->iFlags) ||
	     !__xuiJsonWriterFloatKey(pWriter, "baselineShift", pStyle->fBaselineShift) ||
	     !__xuiJsonWriterIntKey(pWriter, "weight", pStyle->iWeight) ||
	     !__xuiJsonWriterIntKey(pWriter, "slant", pStyle->iSlant) ||
	     !__xuiJsonWriterFloatKey(pWriter, "fontSize", pStyle->fFontSize) ||
	     !xrtJsonWriterEnd(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
	return XUI_OK;
}

static int __xuiRichSerialWriteParagraphStyle(xjsonwriter* pWriter, const xui_rich_paragraph_style_t* pStyle)
{
	if ( !isfinite(pStyle->fIndentLeft) || !isfinite(pStyle->fIndentRight) ||
	     !isfinite(pStyle->fFirstLineIndent) || !isfinite(pStyle->fLineHeight) ||
	     !isfinite(pStyle->fSpaceBefore) || !isfinite(pStyle->fSpaceAfter) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiJsonWriterObjectKey(pWriter, "paragraphStyle") ||
	     !__xuiJsonWriterIntKey(pWriter, "align", pStyle->iAlign) ||
	     !__xuiJsonWriterIntKey(pWriter, "direction", pStyle->iDirection) ||
	     !__xuiJsonWriterIntKey(pWriter, "listType", pStyle->iListType) ||
	     !__xuiJsonWriterIntKey(pWriter, "listLevel", pStyle->iListLevel) ||
	     !__xuiJsonWriterBoolKey(pWriter, "listChecked", pStyle->bListChecked != 0) ||
	     !__xuiJsonWriterIntKey(pWriter, "headingLevel", pStyle->iHeadingLevel) ||
	     !__xuiJsonWriterFloatKey(pWriter, "indentLeft", pStyle->fIndentLeft) ||
	     !__xuiJsonWriterFloatKey(pWriter, "indentRight", pStyle->fIndentRight) ||
	     !__xuiJsonWriterFloatKey(pWriter, "firstLineIndent", pStyle->fFirstLineIndent) ||
	     !__xuiJsonWriterFloatKey(pWriter, "lineHeight", pStyle->fLineHeight) ||
	     !__xuiJsonWriterFloatKey(pWriter, "spaceBefore", pStyle->fSpaceBefore) ||
	     !__xuiJsonWriterFloatKey(pWriter, "spaceAfter", pStyle->fSpaceAfter) ||
	     !__xuiJsonWriterIntKey(pWriter, "backgroundColor", (int64)pStyle->iBackgroundColor) ||
	     !__xuiJsonWriterIntKey(pWriter, "borderColor", (int64)pStyle->iBorderColor) ||
	     !xrtJsonWriterEnd(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
	return XUI_OK;
}

static int __xuiRichSerialWriteDocument(xjsonwriter* pWriter, xui_rich_document pDocument,
	const char* sKey, int iDepth, int* pNodeCount);

static int __xuiRichSerialWriteNode(xjsonwriter* pWriter, xui_rich_node pNode,
	int iDepth, int* pNodeCount)
{
	xui_rich_node pChild;
	const char* sType;
	int i;
	int iRet;
	if ( iDepth > XUI_RICH_SERIAL_MAX_DEPTH || ++(*pNodeCount) > XUI_RICH_SERIAL_MAX_NODES )
		return XUI_ERROR_UNSUPPORTED;
	if ( pNode->iType == XUI_RICH_NODE_INLINE_WIDGET ) return XUI_ERROR_UNSUPPORTED;
	sType = __xuiRichSerialNodeName(pNode->iType);
	if ( sType == NULL || pNode->iId == 0 || pNode->iId > INT64_MAX ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pNode->iType == XUI_RICH_NODE_IMAGE || pNode->iType == XUI_RICH_NODE_INLINE_IMAGE) &&
	     (pNode->sResource == NULL || pNode->sResource[0] == 0) ) return XUI_ERROR_UNSUPPORTED;
	if ( !isfinite(pNode->fWidth) || !isfinite(pNode->fHeight) || !isfinite(pNode->fBaseline) )
		return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xrtJsonWriterObject(pWriter) ||
	     !__xuiJsonWriterIntKey(pWriter, "id", (int64)pNode->iId) ||
	     !__xuiJsonWriterStringKey(pWriter, "type", sType) ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( __xuiRichInlineTextType(pNode->iType) ) {
		if ( pNode->sText == NULL || !__xuiJsonWriterStringKey(pWriter, "text", pNode->sText) ) return XUI_ERROR_OUT_OF_MEMORY;
		iRet = __xuiRichSerialWriteTextStyle(pWriter, &pNode->tStyle);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( __xuiRichTextBlockType(pNode->iType) || __xuiRichAtomicBlockType(pNode->iType) ) {
		iRet = __xuiRichSerialWriteParagraphStyle(pWriter, &pNode->tParagraphStyle);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pNode->iType == XUI_RICH_NODE_LINK ) {
		if ( pNode->sResource == NULL || !__xuiJsonWriterStringKey(pWriter, "resource", pNode->sResource) )
			return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pNode->iType == XUI_RICH_NODE_IMAGE || pNode->iType == XUI_RICH_NODE_INLINE_IMAGE ) {
		if ( !__xuiJsonWriterStringKey(pWriter, "resource", pNode->sResource) ||
		     (pNode->sAltText != NULL ? !__xuiJsonWriterStringKey(pWriter, "alt", pNode->sAltText) : !__xuiJsonWriterNullKey(pWriter, "alt")) ||
		     !__xuiJsonWriterFloatKey(pWriter, "width", pNode->fWidth) ||
		     !__xuiJsonWriterFloatKey(pWriter, "height", pNode->fHeight) ||
		     !__xuiJsonWriterFloatKey(pWriter, "baseline", pNode->fBaseline) ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pNode->iType == XUI_RICH_NODE_TABLE ) {
		if ( pNode->iRows <= 0 || pNode->iColumns <= 0 || pNode->iRows > 1024 || pNode->iColumns > 256 ||
		     !isfinite(pNode->fCellPadding) || !isfinite(pNode->fBorderWidth) ||
		     !__xuiJsonWriterIntKey(pWriter, "rows", pNode->iRows) ||
		     !__xuiJsonWriterIntKey(pWriter, "columns", pNode->iColumns) ||
		     !__xuiJsonWriterFloatKey(pWriter, "width", pNode->fWidth) ||
		     !__xuiJsonWriterFloatKey(pWriter, "cellPadding", pNode->fCellPadding) ||
		     !__xuiJsonWriterFloatKey(pWriter, "borderWidth", pNode->fBorderWidth) ||
		     !__xuiJsonWriterIntKey(pWriter, "borderColor", (int64)pNode->iBorderColor) ||
		     !__xuiJsonWriterIntKey(pWriter, "headerColor", (int64)pNode->iHeaderColor) ||
		     !__xuiJsonWriterIntKey(pWriter, "cellColor", (int64)pNode->iCellColor) ||
		     !__xuiJsonWriterArrayKey(pWriter, "cells") ) return XUI_ERROR_OUT_OF_MEMORY;
		for ( i = 0; i < pNode->iRows * pNode->iColumns; i++ ) {
			if ( pNode->ppCellDocuments == NULL || pNode->ppCellDocuments[i] == NULL ) {
				if ( !xrtJsonWriterNull(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
			} else {
				iRet = __xuiRichSerialWriteDocument(pWriter, pNode->ppCellDocuments[i], NULL, iDepth + 1, pNodeCount);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( !xrtJsonWriterEnd(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( __xuiRichTextBlockType(pNode->iType) ) {
		if ( !__xuiJsonWriterArrayKey(pWriter, "children") ) return XUI_ERROR_OUT_OF_MEMORY;
		for ( pChild = pNode->pFirstChild; pChild != NULL; pChild = pChild->pNext ) {
			iRet = __xuiRichSerialWriteNode(pWriter, pChild, iDepth + 1, pNodeCount);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( !xrtJsonWriterEnd(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	return xrtJsonWriterEnd(pWriter) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiRichSerialWriteDocument(xjsonwriter* pWriter, xui_rich_document pDocument,
	const char* sKey, int iDepth, int* pNodeCount)
{
	xui_rich_node pNode;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || iDepth > XUI_RICH_SERIAL_MAX_DEPTH ||
	     pDocument->pRoot == NULL || pDocument->pRoot->iId == 0 || pDocument->pRoot->iId > INT64_MAX )
		return XUI_ERROR_INVALID_ARGUMENT;
	if ( sKey != NULL ? !__xuiJsonWriterObjectKey(pWriter, sKey) : !xrtJsonWriterObject(pWriter) )
		return XUI_ERROR_OUT_OF_MEMORY;
	if ( !__xuiJsonWriterStringKey(pWriter, "format", XUI_RICH_SERIAL_FORMAT) ||
	     !__xuiJsonWriterIntKey(pWriter, "version", XUI_RICH_SERIAL_VERSION) ||
	     !__xuiJsonWriterIntKey(pWriter, "rootId", (int64)pDocument->pRoot->iId) ||
	     !__xuiJsonWriterArrayKey(pWriter, "nodes") ) return XUI_ERROR_OUT_OF_MEMORY;
	for ( pNode = pDocument->pRoot->pFirstChild; pNode != NULL; pNode = pNode->pNext ) {
		iRet = __xuiRichSerialWriteNode(pWriter, pNode, iDepth + 1, pNodeCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( !xrtJsonWriterEnd(pWriter) || !xrtJsonWriterEnd(pWriter) ) return XUI_ERROR_OUT_OF_MEMORY;
	return XUI_OK;
}

XUI_API int xuiRichDocumentSerialize(xui_rich_document pDocument, int bPretty, char** ppText, size_t* pSize)
{
	xjsonwriteconfig Config;
	xjsonwriter* pWriter;
	char* sText;
	size_t iSize = 0;
	int iNodeCount = 0;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || ppText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppText = NULL;
	if ( pSize != NULL ) *pSize = 0;
	xrtJsonWriteConfigInit(&Config);
	if ( bPretty ) Config.Flags |= XJSON_WRITE_PRETTY;
	pWriter = xrtJsonWriterCreate(&Config);
	if ( pWriter == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiRichSerialWriteDocument(pWriter, pDocument, NULL, 0, &iNodeCount);
	if ( iRet == XUI_OK ) {
		if ( !xrtJsonWriterFinish(pWriter) ) {
			iRet = XUI_ERROR_RESOURCE_FAILED;
		} else {
			sText = xrtJsonWriterTake(pWriter, &iSize);
			if ( sText == NULL ) iRet = XUI_ERROR_OUT_OF_MEMORY;
			else { *ppText = sText; if ( pSize != NULL ) *pSize = iSize; }
		}
	}
	xrtJsonWriterFree(pWriter);
	return iRet;
}

XUI_API void xuiRichDocumentFreeSerialized(char* sText)
{
	xrtFree(sText);
}

typedef struct xui_rich_serial_read_t {
	int iStatus;
	int iNodeCount;
} xui_rich_serial_read_t;

static xvalue* __xuiRichSerialMember(xvalue* pObject, const char* sKey)
{
	return __xuiValueIsObject(pObject) ? __xuiValueObjectGet(pObject, sKey, 0) : NULL;
}

static int __xuiRichSerialInt(xvalue* pObject, const char* sKey, int64 iMin, int64 iMax, int64* pValue)
{
	xvalue* pField = __xuiRichSerialMember(pObject, sKey);
	int64 iValue;
	if ( pField == NULL || !__xuiValueIsInt(pField) ) return 0;
	iValue = __xuiValueGetInt(pField);
	if ( iValue < iMin || iValue > iMax ) return 0;
	*pValue = iValue;
	return 1;
}

static int __xuiRichSerialFloat(xvalue* pObject, const char* sKey, double fMin, double fMax, float* pValue)
{
	xvalue* pField = __xuiRichSerialMember(pObject, sKey);
	double fValue;
	if ( pField == NULL || !xrtValueIsNumber(pField) ) return 0;
	fValue = __xuiValueIsInt(pField) ? (double)__xuiValueGetInt(pField) : __xuiValueGetFloat(pField);
	if ( !isfinite(fValue) || fValue < fMin || fValue > fMax ) return 0;
	*pValue = (float)fValue;
	return 1;
}

static int __xuiRichSerialString(xvalue* pObject, const char* sKey, int bOptional,
	const char** ppText, int* pSize)
{
	xvalue* pField = __xuiRichSerialMember(pObject, sKey);
	if ( pField == NULL || __xuiValueIsNull(pField) ) {
		if ( !bOptional ) return 0;
		*ppText = NULL; *pSize = 0; return 1;
	}
	if ( !__xuiValueIsText(pField) || __xuiValueGetSize(pField) > INT_MAX ) return 0;
	*ppText = (const char*)__xuiValueGetText(pField);
	*pSize = (int)__xuiValueGetSize(pField);
	if ( memchr(*ppText, 0, (size_t)*pSize) != NULL || !__xuiRichUtf8Valid(*ppText, *pSize) ) return 0;
	return 1;
}

static int __xuiRichSerialReadTextStyle(xvalue* pObject, xui_rich_text_style_t* pStyle)
{
	xvalue* pValue = __xuiRichSerialMember(pObject, "textStyle");
	int64 i;
	memset(pStyle, 0, sizeof(*pStyle)); pStyle->iSize = sizeof(*pStyle);
	if ( !__xuiValueIsObject(pValue) ||
	     !__xuiRichSerialInt(pValue, "textColor", 0, UINT32_MAX, &i) ) return 0;
	pStyle->iTextColor = (uint32_t)i;
	if ( !__xuiRichSerialInt(pValue, "backgroundColor", 0, UINT32_MAX, &i) ) return 0;
	pStyle->iBackgroundColor = (uint32_t)i;
	if ( !__xuiRichSerialInt(pValue, "flags", 0, UINT32_MAX, &i) ) return 0;
	pStyle->iFlags = (uint32_t)i;
	if ( !__xuiRichSerialFloat(pValue, "baselineShift", -1000000.0, 1000000.0, &pStyle->fBaselineShift) ||
	     !__xuiRichSerialInt(pValue, "weight", INT_MIN, INT_MAX, &i) ) return 0;
	pStyle->iWeight = (int)i;
	if ( !__xuiRichSerialInt(pValue, "slant", INT_MIN, INT_MAX, &i) ) return 0;
	pStyle->iSlant = (int)i;
	return __xuiRichSerialFloat(pValue, "fontSize", 0.0, 1000000.0, &pStyle->fFontSize);
}

static int __xuiRichSerialReadParagraphStyle(xvalue* pObject, xui_rich_paragraph_style_t* pStyle)
{
	xvalue* pValue = __xuiRichSerialMember(pObject, "paragraphStyle");
	xvalue* pChecked;
	int64 i;
	memset(pStyle, 0, sizeof(*pStyle)); pStyle->iSize = sizeof(*pStyle);
	if ( !__xuiValueIsObject(pValue) || !__xuiRichSerialInt(pValue, "align", XUI_RICH_ALIGN_LEFT, XUI_RICH_ALIGN_JUSTIFY, &i) ) return 0;
	pStyle->iAlign = (int)i;
	if ( !__xuiRichSerialInt(pValue, "direction", XUI_RICH_DIRECTION_AUTO, XUI_RICH_DIRECTION_RTL, &i) ) return 0;
	pStyle->iDirection = (int)i;
	if ( !__xuiRichSerialInt(pValue, "listType", XUI_RICH_LIST_NONE, XUI_RICH_LIST_CHECK, &i) ) return 0;
	pStyle->iListType = (int)i;
	if ( !__xuiRichSerialInt(pValue, "listLevel", 0, 1024, &i) ) return 0;
	pStyle->iListLevel = (int)i;
	pChecked = __xuiRichSerialMember(pValue, "listChecked");
	if ( pChecked == NULL || !__xuiValueIsBool(pChecked) ) return 0;
	pStyle->bListChecked = __xuiValueGetBool(pChecked) ? 1 : 0;
	if ( !__xuiRichSerialInt(pValue, "headingLevel", 0, 6, &i) ) return 0;
	pStyle->iHeadingLevel = (int)i;
	if ( !__xuiRichSerialFloat(pValue, "indentLeft", -1000000.0, 1000000.0, &pStyle->fIndentLeft) ||
	     !__xuiRichSerialFloat(pValue, "indentRight", -1000000.0, 1000000.0, &pStyle->fIndentRight) ||
	     !__xuiRichSerialFloat(pValue, "firstLineIndent", -1000000.0, 1000000.0, &pStyle->fFirstLineIndent) ||
	     !__xuiRichSerialFloat(pValue, "lineHeight", 0.0, 1000000.0, &pStyle->fLineHeight) ||
	     !__xuiRichSerialFloat(pValue, "spaceBefore", 0.0, 1000000.0, &pStyle->fSpaceBefore) ||
	     !__xuiRichSerialFloat(pValue, "spaceAfter", 0.0, 1000000.0, &pStyle->fSpaceAfter) ||
	     !__xuiRichSerialInt(pValue, "backgroundColor", 0, UINT32_MAX, &i) ) return 0;
	pStyle->iBackgroundColor = (uint32_t)i;
	if ( !__xuiRichSerialInt(pValue, "borderColor", 0, UINT32_MAX, &i) ) return 0;
	pStyle->iBorderColor = (uint32_t)i;
	return 1;
}

static xui_rich_document __xuiRichSerialReadDocument(xvalue* pValue, xui_rich_document pOwner,
	xui_document_node_id_t iOwnerTableId, int iDepth, xui_rich_serial_read_t* pRead);

static xui_rich_node __xuiRichSerialReadNode(xui_rich_document pDocument, xvalue* pValue,
	xui_rich_node pParent, int bInline, int iDepth, xui_rich_serial_read_t* pRead)
{
	xui_rich_node pNode = NULL;
	xvalue* pChildren;
	const char* sType;
	const char* sText;
	const char* sResource;
	const char* sAlt;
	int iTypeSize, iTextSize, iResourceSize, iAltSize;
	int64 iId, iValue;
	uint32 i;
	xui_rich_text_style_t tTextStyle;
	xui_rich_paragraph_style_t tParagraphStyle;
	if ( iDepth > XUI_RICH_SERIAL_MAX_DEPTH || ++pRead->iNodeCount > XUI_RICH_SERIAL_MAX_NODES ) {
		pRead->iStatus = XUI_ERROR_UNSUPPORTED; return NULL;
	}
	if ( !__xuiValueIsObject(pValue) || !__xuiRichSerialInt(pValue, "id", 1, INT64_MAX, &iId) ||
	     !__xuiRichSerialString(pValue, "type", 0, &sType, &iTypeSize) || sType[iTypeSize] != 0 ) goto invalid;
	iValue = __xuiRichSerialNodeType(sType);
	if ( iValue == 0 || iValue == XUI_RICH_NODE_INLINE_WIDGET ||
	     (bInline ? !(iValue == XUI_RICH_NODE_TEXT || iValue == XUI_RICH_NODE_LINK || iValue == XUI_RICH_NODE_INLINE_IMAGE)
	              : (!__xuiRichTextBlockType((int)iValue) && !__xuiRichAtomicBlockType((int)iValue))) ||
	     __xuiRichFindNode(pDocument->pRoot, (uint64_t)iId) != NULL ) goto invalid;
	pNode = __xuiRichNodeCreate(pDocument, (int)iValue, (uint64_t)iId);
	if ( pNode == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL; }
	__xuiRichNodeAppend(pParent, pNode);
	if ( __xuiRichInlineTextType(pNode->iType) ) {
		if ( !__xuiRichSerialString(pValue, "text", 0, &sText, &iTextSize) ||
		     memchr(sText, '\n', (size_t)iTextSize) != NULL || memchr(sText, '\r', (size_t)iTextSize) != NULL ||
		     !__xuiRichSerialReadTextStyle(pValue, &tTextStyle) ) goto invalid;
		pNode->sText = (char*)xrtMalloc((size_t)iTextSize + 1u);
		if ( pNode->sText == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL; }
		memcpy(pNode->sText, sText, (size_t)iTextSize); pNode->sText[iTextSize] = 0;
		pNode->iTextSize = iTextSize; pNode->tStyle = tTextStyle;
	}
	if ( __xuiRichTextBlockType(pNode->iType) || __xuiRichAtomicBlockType(pNode->iType) ) {
		if ( !__xuiRichSerialReadParagraphStyle(pValue, &tParagraphStyle) ) goto invalid;
		pNode->tParagraphStyle = tParagraphStyle;
	}
	if ( pNode->iType == XUI_RICH_NODE_LINK ) {
		if ( !__xuiRichSerialString(pValue, "resource", 0, &sResource, &iResourceSize) || iResourceSize == 0 ) goto invalid;
		pNode->sResource = __xuiRichString(sResource);
		if ( pNode->sResource == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL; }
	}
	if ( pNode->iType == XUI_RICH_NODE_IMAGE || pNode->iType == XUI_RICH_NODE_INLINE_IMAGE ) {
		if ( !__xuiRichSerialString(pValue, "resource", 0, &sResource, &iResourceSize) || iResourceSize == 0 ||
		     !__xuiRichSerialString(pValue, "alt", 1, &sAlt, &iAltSize) ||
		     !__xuiRichSerialFloat(pValue, "width", 0.0, 10000000.0, &pNode->fWidth) ||
		     !__xuiRichSerialFloat(pValue, "height", 0.0, 10000000.0, &pNode->fHeight) ||
		     !__xuiRichSerialFloat(pValue, "baseline", 0.0, 10000000.0, &pNode->fBaseline) ) goto invalid;
		pNode->sResource = __xuiRichString(sResource); pNode->sAltText = __xuiRichString(sAlt);
		if ( pNode->sResource == NULL || (sAlt != NULL && pNode->sAltText == NULL) ) {
			pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL;
		}
	}
	if ( pNode->iType == XUI_RICH_NODE_TABLE ) {
		xvalue* pCells = __xuiRichSerialMember(pValue, "cells");
		if ( !__xuiRichSerialInt(pValue, "rows", 1, 1024, &iValue) ) goto invalid;
		pNode->iRows = (int)iValue;
		if ( !__xuiRichSerialInt(pValue, "columns", 1, 256, &iValue) ) goto invalid;
		pNode->iColumns = (int)iValue;
		if ( !__xuiRichSerialFloat(pValue, "width", 0.0, 10000000.0, &pNode->fWidth) ||
		     !__xuiRichSerialFloat(pValue, "cellPadding", 0.0, 1000000.0, &pNode->fCellPadding) ||
		     !__xuiRichSerialFloat(pValue, "borderWidth", 0.0, 1000000.0, &pNode->fBorderWidth) ||
		     !__xuiRichSerialInt(pValue, "borderColor", 0, UINT32_MAX, &iValue) ) goto invalid;
		pNode->iBorderColor = (uint32_t)iValue;
		if ( !__xuiRichSerialInt(pValue, "headerColor", 0, UINT32_MAX, &iValue) ) goto invalid;
		pNode->iHeaderColor = (uint32_t)iValue;
		if ( !__xuiRichSerialInt(pValue, "cellColor", 0, UINT32_MAX, &iValue) ) goto invalid;
		pNode->iCellColor = (uint32_t)iValue;
		if ( !__xuiValueIsArray(pCells) || xrtValueCount(pCells) != (uint32)pNode->iRows * (uint32)pNode->iColumns ) goto invalid;
		pNode->ppCellDocuments = (xui_rich_document*)xrtCalloc((size_t)pNode->iRows * (size_t)pNode->iColumns,
			sizeof(*pNode->ppCellDocuments));
		if ( pNode->ppCellDocuments == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL; }
		for ( i = 0; i < xrtValueCount(pCells); i++ ) {
			xvalue* pCell = xrtValueArrayGet(pCells, i);
			if ( !__xuiValueIsNull(pCell) ) {
				pNode->ppCellDocuments[i] = __xuiRichSerialReadDocument(pCell, pDocument, pNode->iId, iDepth + 1, pRead);
				if ( pNode->ppCellDocuments[i] == NULL ) return NULL;
			}
		}
	}
	if ( __xuiRichTextBlockType(pNode->iType) ) {
		pChildren = __xuiRichSerialMember(pValue, "children");
		if ( !__xuiValueIsArray(pChildren) ) goto invalid;
		for ( i = 0; i < xrtValueCount(pChildren); i++ )
			if ( __xuiRichSerialReadNode(pDocument, xrtValueArrayGet(pChildren, i), pNode, 1, iDepth + 1, pRead) == NULL )
				return NULL;
	}
	return pNode;

invalid:
	pRead->iStatus = XUI_ERROR_INVALID_ARGUMENT;
	return NULL;
}

static xui_rich_document __xuiRichSerialReadDocument(xvalue* pValue, xui_rich_document pOwner,
	xui_document_node_id_t iOwnerTableId, int iDepth, xui_rich_serial_read_t* pRead)
{
	xui_rich_document pDocument = NULL;
	xui_rich_node pRoot;
	xvalue* pNodes;
	const char* sFormat;
	int iFormatSize;
	int64 iVersion, iRootId;
	uint32 i;
	if ( iDepth > XUI_RICH_SERIAL_MAX_DEPTH || !__xuiValueIsObject(pValue) ||
	     !__xuiRichSerialString(pValue, "format", 0, &sFormat, &iFormatSize) ||
	     iFormatSize != (int)strlen(XUI_RICH_SERIAL_FORMAT) || memcmp(sFormat, XUI_RICH_SERIAL_FORMAT, (size_t)iFormatSize) != 0 ||
	     !__xuiRichSerialInt(pValue, "version", XUI_RICH_SERIAL_VERSION, XUI_RICH_SERIAL_VERSION, &iVersion) ||
	     !__xuiRichSerialInt(pValue, "rootId", 1, INT64_MAX, &iRootId) ) goto invalid;
	pNodes = __xuiRichSerialMember(pValue, "nodes");
	if ( !__xuiValueIsArray(pNodes) || xrtValueCount(pNodes) == 0 ) goto invalid;
	pDocument = (xui_rich_document)xrtCalloc(1, sizeof(*pDocument));
	if ( pDocument == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; return NULL; }
	pDocument->iMagic = XUI_RICH_DOCUMENT_MAGIC; pDocument->iNextId = 1; pDocument->iVersion = 1;
	pDocument->bFlatDirty = 1; pDocument->pOwnerDocument = pOwner; pDocument->iOwnerTableId = iOwnerTableId;
	pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, (uint64_t)iRootId);
	if ( pRoot == NULL ) { pRead->iStatus = XUI_ERROR_OUT_OF_MEMORY; xuiRichDocumentDestroy(pDocument); return NULL; }
	pDocument->pRoot = pRoot;
	for ( i = 0; i < xrtValueCount(pNodes); i++ )
		if ( __xuiRichSerialReadNode(pDocument, xrtValueArrayGet(pNodes, i), pRoot, 0, iDepth + 1, pRead) == NULL ) {
			xuiRichDocumentDestroy(pDocument); return NULL;
		}
	return pDocument;

invalid:
	pRead->iStatus = XUI_ERROR_INVALID_ARGUMENT;
	return NULL;
}

XUI_API int xuiRichDocumentDeserialize(const char* sText, size_t iSize, xui_rich_document* ppDocument)
{
	xvalue* pValue;
	xui_rich_document pDocument;
	xui_rich_serial_read_t tRead;
	if ( sText == NULL || ppDocument == NULL || iSize > XUI_RICH_SERIAL_MAX_INPUT ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	if ( iSize == 0 ) iSize = strlen(sText);
	if ( iSize == 0 || iSize > XUI_RICH_SERIAL_MAX_INPUT ) return XUI_ERROR_INVALID_ARGUMENT;
	pValue = xrtJsonParse(xrtStrViewN(sText, iSize));
	if ( pValue == NULL || __xuiValueIsNull(pValue) ) { if ( pValue != NULL ) xrtValueRelease(pValue); return XUI_ERROR_INVALID_ARGUMENT; }
	memset(&tRead, 0, sizeof(tRead)); tRead.iStatus = XUI_OK;
	pDocument = __xuiRichSerialReadDocument(pValue, NULL, 0, 0, &tRead);
	xrtValueRelease(pValue);
	if ( pDocument == NULL ) return tRead.iStatus != XUI_OK ? tRead.iStatus : XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = pDocument;
	return XUI_OK;
}

typedef struct xui_rich_text_writer_t {
	xbuffer tBuffer;
	int iStatus;
} xui_rich_text_writer_t;

static void __xuiRichTextWriterInit(xui_rich_text_writer_t* pWriter)
{
	memset(pWriter, 0, sizeof(*pWriter));
	pWriter->iStatus = (xrtBufferInit(&pWriter->tBuffer) &&
		xrtBufferReserve(&pWriter->tBuffer, 4096u)) ?
		XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiRichTextWriterAppendN(xui_rich_text_writer_t* pWriter, const char* sText, size_t iSize)
{
	if ( pWriter->iStatus != XUI_OK ) return pWriter->iStatus;
	if ( (iSize > 0u) && !xrtBufferAppend(&pWriter->tBuffer,
		(xbytesview){ (const unsigned char*)sText, iSize }) )
		pWriter->iStatus = XUI_ERROR_OUT_OF_MEMORY;
	return pWriter->iStatus;
}

static int __xuiRichTextWriterAppend(xui_rich_text_writer_t* pWriter, const char* sText)
{
	return __xuiRichTextWriterAppendN(pWriter, sText, strlen(sText));
}

static int __xuiRichTextWriterFinish(xui_rich_text_writer_t* pWriter, char** ppText, size_t* pSize)
{
	bytes pText;
	size_t iTextSize;

	if ( pWriter->iStatus == XUI_OK &&
		 !xrtBufferAppendByte(&pWriter->tBuffer, 0u) )
		pWriter->iStatus = XUI_ERROR_OUT_OF_MEMORY;
	if ( pWriter->iStatus != XUI_OK ) { xrtBufferUnit(&pWriter->tBuffer); return pWriter->iStatus; }
	pText = xrtBufferTake(&pWriter->tBuffer, &iTextSize, NULL);
	if ( pText == NULL ) {
		xrtBufferUnit(&pWriter->tBuffer);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*ppText = (char*)pText;
	if ( pSize != NULL ) *pSize = iTextSize - 1u;
	xrtBufferUnit(&pWriter->tBuffer);
	return XUI_OK;
}

static int __xuiRichHtmlEscape(xui_rich_text_writer_t* pWriter, const char* sText, int iSize, int bAttribute)
{
	int i;
	int iStart = 0;
	if ( sText == NULL ) return XUI_OK;
	if ( iSize < 0 ) iSize = (int)strlen(sText);
	for ( i = 0; i < iSize; i++ ) {
		const char* sEscape = NULL;
		if ( sText[i] == '&' ) sEscape = "&amp;";
		else if ( sText[i] == '<' ) sEscape = "&lt;";
		else if ( sText[i] == '>' ) sEscape = "&gt;";
		else if ( bAttribute && sText[i] == '"' ) sEscape = "&quot;";
		else if ( bAttribute && sText[i] == '\'' ) sEscape = "&#39;";
		if ( sEscape != NULL ) {
			if ( i > iStart ) (void)__xuiRichTextWriterAppendN(pWriter, sText + iStart, (size_t)(i - iStart));
			(void)__xuiRichTextWriterAppend(pWriter, sEscape);
			iStart = i + 1;
		}
	}
	if ( iSize > iStart ) (void)__xuiRichTextWriterAppendN(pWriter, sText + iStart, (size_t)(iSize - iStart));
	return pWriter->iStatus;
}

static void __xuiRichHtmlColor(char sColor[48], uint32_t iColor)
{
	unsigned int r = (iColor >> 24) & 0xffu;
	unsigned int g = (iColor >> 16) & 0xffu;
	unsigned int b = (iColor >> 8) & 0xffu;
	unsigned int a = iColor & 0xffu;
	if ( a == 255u ) snprintf(sColor, 48, "#%02X%02X%02X", r, g, b);
	else snprintf(sColor, 48, "rgba(%u,%u,%u,%.3f)", r, g, b, (double)a / 255.0);
}

static int __xuiRichHtmlWriteStyle(xui_rich_text_writer_t* pWriter, const xui_rich_text_style_t* pStyle)
{
	char sValue[96];
	char sColor[48];
	int bAny = 0;
	if ( pStyle->pFont != NULL ) return XUI_ERROR_UNSUPPORTED;
#define XUI_RICH_HTML_CSS_BEGIN() do { if (!bAny) { (void)__xuiRichTextWriterAppend(pWriter, " style=\""); bAny = 1; } } while (0)
	if ( (pStyle->iTextColor & 0xffu) != 0 ) {
		XUI_RICH_HTML_CSS_BEGIN(); __xuiRichHtmlColor(sColor, pStyle->iTextColor);
		(void)__xuiRichTextWriterAppend(pWriter, "color:"); (void)__xuiRichTextWriterAppend(pWriter, sColor); (void)__xuiRichTextWriterAppend(pWriter, ";");
	}
	if ( (pStyle->iBackgroundColor & 0xffu) != 0 ) {
		XUI_RICH_HTML_CSS_BEGIN(); __xuiRichHtmlColor(sColor, pStyle->iBackgroundColor);
		(void)__xuiRichTextWriterAppend(pWriter, "background-color:"); (void)__xuiRichTextWriterAppend(pWriter, sColor); (void)__xuiRichTextWriterAppend(pWriter, ";");
	}
	if ( pStyle->fFontSize > 0.0f ) {
		XUI_RICH_HTML_CSS_BEGIN(); snprintf(sValue, sizeof(sValue), "font-size:%.3gpx;", (double)pStyle->fFontSize);
		(void)__xuiRichTextWriterAppend(pWriter, sValue);
	}
	if ( pStyle->iWeight != 0 ) {
		XUI_RICH_HTML_CSS_BEGIN(); snprintf(sValue, sizeof(sValue), "font-weight:%d;", pStyle->iWeight);
		(void)__xuiRichTextWriterAppend(pWriter, sValue);
	}
	if ( pStyle->iSlant != 0 ) { XUI_RICH_HTML_CSS_BEGIN(); (void)__xuiRichTextWriterAppend(pWriter, "font-style:italic;"); }
	if ( pStyle->fBaselineShift != 0.0f ) {
		XUI_RICH_HTML_CSS_BEGIN(); snprintf(sValue, sizeof(sValue), "vertical-align:%.3gpx;", (double)pStyle->fBaselineShift);
		(void)__xuiRichTextWriterAppend(pWriter, sValue);
	}
	if ( bAny ) (void)__xuiRichTextWriterAppend(pWriter, "\"");
#undef XUI_RICH_HTML_CSS_BEGIN
	return pWriter->iStatus;
}

static int __xuiRichHtmlWriteInline(xui_rich_text_writer_t* pWriter, xui_rich_node pNode)
{
	xui_rich_node_info_t tInfo;
	uint32_t iFlags;
	char sNumber[96];
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET ) return XUI_ERROR_UNSUPPORTED;
	if ( tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE ) {
		if ( tInfo.sResource == NULL || tInfo.sResource[0] == 0 ) return XUI_ERROR_UNSUPPORTED;
		(void)__xuiRichTextWriterAppend(pWriter, "<img src=\""); __xuiRichHtmlEscape(pWriter, tInfo.sResource, -1, 1);
		(void)__xuiRichTextWriterAppend(pWriter, "\" alt=\""); __xuiRichHtmlEscape(pWriter, tInfo.sAltText, -1, 1);
		snprintf(sNumber, sizeof(sNumber), "\" width=\"%.3g\" height=\"%.3g\" data-baseline=\"%.3g\">",
			(double)tInfo.fWidth, (double)tInfo.fHeight, (double)tInfo.fBaseline);
		return __xuiRichTextWriterAppend(pWriter, sNumber);
	}
	if ( tInfo.iType != XUI_RICH_NODE_TEXT && tInfo.iType != XUI_RICH_NODE_LINK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tInfo.iType == XUI_RICH_NODE_LINK ) {
		if ( tInfo.sResource == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		(void)__xuiRichTextWriterAppend(pWriter, "<a href=\""); __xuiRichHtmlEscape(pWriter, tInfo.sResource, -1, 1);
		(void)__xuiRichTextWriterAppend(pWriter, "\">");
	}
	(void)__xuiRichTextWriterAppend(pWriter, "<span");
	if ( __xuiRichHtmlWriteStyle(pWriter, &tInfo.tStyle) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	(void)__xuiRichTextWriterAppend(pWriter, ">");
	iFlags = tInfo.tStyle.iFlags;
	if ( iFlags & XUI_RICH_STYLE_BOLD ) (void)__xuiRichTextWriterAppend(pWriter, "<strong>");
	if ( iFlags & XUI_RICH_STYLE_ITALIC ) (void)__xuiRichTextWriterAppend(pWriter, "<em>");
	if ( iFlags & XUI_RICH_STYLE_UNDERLINE ) (void)__xuiRichTextWriterAppend(pWriter, "<u>");
	if ( iFlags & XUI_RICH_STYLE_STRIKEOUT ) (void)__xuiRichTextWriterAppend(pWriter, "<s>");
	if ( iFlags & XUI_RICH_STYLE_CODE ) (void)__xuiRichTextWriterAppend(pWriter, "<code>");
	if ( iFlags & XUI_RICH_STYLE_SUBSCRIPT ) (void)__xuiRichTextWriterAppend(pWriter, "<sub>");
	if ( iFlags & XUI_RICH_STYLE_SUPERSCRIPT ) (void)__xuiRichTextWriterAppend(pWriter, "<sup>");
	__xuiRichHtmlEscape(pWriter, tInfo.sText, tInfo.iTextSize, 0);
	if ( iFlags & XUI_RICH_STYLE_SUPERSCRIPT ) (void)__xuiRichTextWriterAppend(pWriter, "</sup>");
	if ( iFlags & XUI_RICH_STYLE_SUBSCRIPT ) (void)__xuiRichTextWriterAppend(pWriter, "</sub>");
	if ( iFlags & XUI_RICH_STYLE_CODE ) (void)__xuiRichTextWriterAppend(pWriter, "</code>");
	if ( iFlags & XUI_RICH_STYLE_STRIKEOUT ) (void)__xuiRichTextWriterAppend(pWriter, "</s>");
	if ( iFlags & XUI_RICH_STYLE_UNDERLINE ) (void)__xuiRichTextWriterAppend(pWriter, "</u>");
	if ( iFlags & XUI_RICH_STYLE_ITALIC ) (void)__xuiRichTextWriterAppend(pWriter, "</em>");
	if ( iFlags & XUI_RICH_STYLE_BOLD ) (void)__xuiRichTextWriterAppend(pWriter, "</strong>");
	(void)__xuiRichTextWriterAppend(pWriter, "</span>");
	if ( tInfo.iType == XUI_RICH_NODE_LINK ) (void)__xuiRichTextWriterAppend(pWriter, "</a>");
	return pWriter->iStatus;
}

static int __xuiRichHtmlWriteDocumentBody(xui_rich_text_writer_t* pWriter, xui_rich_document pDocument, int iDepth);

static int __xuiRichHtmlWriteParagraphAttrs(xui_rich_text_writer_t* pWriter,
	const xui_rich_paragraph_style_t* pStyle)
{
	char sStyle[512];
	char sBackground[48];
	char sBorder[48];
	const char* sAlign = pStyle->iAlign == XUI_RICH_ALIGN_CENTER ? "center" :
		pStyle->iAlign == XUI_RICH_ALIGN_RIGHT ? "right" : pStyle->iAlign == XUI_RICH_ALIGN_JUSTIFY ? "justify" : "left";
	const char* sDirection = pStyle->iDirection == XUI_RICH_DIRECTION_RTL ? "rtl" : "ltr";
	__xuiRichHtmlColor(sBackground, pStyle->iBackgroundColor);
	__xuiRichHtmlColor(sBorder, pStyle->iBorderColor);
	snprintf(sStyle, sizeof(sStyle),
		" style=\"text-align:%s;direction:%s;margin-left:%.3gpx;margin-right:%.3gpx;text-indent:%.3gpx;line-height:%.3gpx;margin-top:%.3gpx;margin-bottom:%.3gpx;%s%s%s%s\"",
		sAlign, sDirection, (double)pStyle->fIndentLeft, (double)pStyle->fIndentRight,
		(double)pStyle->fFirstLineIndent, (double)pStyle->fLineHeight, (double)pStyle->fSpaceBefore,
		(double)pStyle->fSpaceAfter, (pStyle->iBackgroundColor & 0xffu) ? "background-color:" : "",
		(pStyle->iBackgroundColor & 0xffu) ? sBackground : "", (pStyle->iBorderColor & 0xffu) ? ";border-color:" : "",
		(pStyle->iBorderColor & 0xffu) ? sBorder : "");
	return __xuiRichTextWriterAppend(pWriter, sStyle);
}

static int __xuiRichHtmlWriteBlock(xui_rich_text_writer_t* pWriter, xui_rich_node pNode, int iDepth)
{
	xui_rich_node_info_t tInfo;
	xui_rich_node pChild;
	const char* sOpen = "p";
	char sTag[96];
	int r, c;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tInfo.iType == XUI_RICH_NODE_IMAGE ) {
		if ( tInfo.sResource == NULL || tInfo.sResource[0] == 0 ) return XUI_ERROR_UNSUPPORTED;
		(void)__xuiRichTextWriterAppend(pWriter, "<figure"); __xuiRichHtmlWriteParagraphAttrs(pWriter, &tInfo.tParagraphStyle);
		(void)__xuiRichTextWriterAppend(pWriter, "><img src=\""); __xuiRichHtmlEscape(pWriter, tInfo.sResource, -1, 1);
		(void)__xuiRichTextWriterAppend(pWriter, "\" alt=\""); __xuiRichHtmlEscape(pWriter, tInfo.sAltText, -1, 1);
		snprintf(sTag, sizeof(sTag), "\" width=\"%.3g\" height=\"%.3g\"></figure>\n", (double)tInfo.fWidth, (double)tInfo.fHeight);
		return __xuiRichTextWriterAppend(pWriter, sTag);
	}
	if ( tInfo.iType == XUI_RICH_NODE_HORIZONTAL_RULE ) return __xuiRichTextWriterAppend(pWriter, "<hr>\n");
	if ( tInfo.iType == XUI_RICH_NODE_TABLE ) {
		(void)__xuiRichTextWriterAppend(pWriter, "<table><tbody>\n");
		for ( r = 0; r < tInfo.iRows; r++ ) {
			(void)__xuiRichTextWriterAppend(pWriter, "<tr>");
			for ( c = 0; c < tInfo.iColumns; c++ ) {
				xui_rich_document pCell = pNode->ppCellDocuments != NULL ? pNode->ppCellDocuments[r * tInfo.iColumns + c] : NULL;
				(void)__xuiRichTextWriterAppend(pWriter, r == 0 ? "<th>" : "<td>");
				if ( pCell != NULL ) {
					int iRet = __xuiRichHtmlWriteDocumentBody(pWriter, pCell, iDepth + 1);
					if ( iRet != XUI_OK ) return iRet;
				}
				(void)__xuiRichTextWriterAppend(pWriter, r == 0 ? "</th>" : "</td>");
			}
			(void)__xuiRichTextWriterAppend(pWriter, "</tr>\n");
		}
		return __xuiRichTextWriterAppend(pWriter, "</tbody></table>\n");
	}
	if ( tInfo.iType == XUI_RICH_NODE_HEADING ) {
		int iLevel = tInfo.tParagraphStyle.iHeadingLevel;
		if ( iLevel < 1 || iLevel > 6 ) iLevel = 1;
		snprintf(sTag, sizeof(sTag), "h%d", iLevel); sOpen = sTag;
	} else if ( tInfo.iType == XUI_RICH_NODE_BLOCK_QUOTE ) sOpen = "blockquote";
	else if ( tInfo.iType == XUI_RICH_NODE_LIST_ITEM ) sOpen = "li";
	(void)__xuiRichTextWriterAppend(pWriter, "<"); (void)__xuiRichTextWriterAppend(pWriter, sOpen);
	__xuiRichHtmlWriteParagraphAttrs(pWriter, &tInfo.tParagraphStyle);
	if ( tInfo.iType == XUI_RICH_NODE_LIST_ITEM ) {
		const char* sList = tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NUMBER ? "number" :
			tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_CHECK ? "check" : "bullet";
		snprintf(sTag, sizeof(sTag), " data-list=\"%s\" data-level=\"%d\"%s", sList,
			tInfo.tParagraphStyle.iListLevel, tInfo.tParagraphStyle.bListChecked ? " data-checked=\"true\"" : "");
		(void)__xuiRichTextWriterAppend(pWriter, sTag);
	}
	(void)__xuiRichTextWriterAppend(pWriter, ">");
	for ( pChild = xuiRichNodeGetFirstChild(pNode); pChild != NULL; pChild = xuiRichNodeGetNextSibling(pChild) ) {
		int iRet = __xuiRichHtmlWriteInline(pWriter, pChild);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)__xuiRichTextWriterAppend(pWriter, "</"); (void)__xuiRichTextWriterAppend(pWriter, sOpen);
	return __xuiRichTextWriterAppend(pWriter, ">\n");
}

static int __xuiRichHtmlWriteDocumentBody(xui_rich_text_writer_t* pWriter, xui_rich_document pDocument, int iDepth)
{
	xui_rich_node pNode;
	if ( iDepth > XUI_RICH_SERIAL_MAX_DEPTH ) return XUI_ERROR_UNSUPPORTED;
	for ( pNode = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pDocument)); pNode != NULL;
	      pNode = xuiRichNodeGetNextSibling(pNode) ) {
		int iRet = __xuiRichHtmlWriteBlock(pWriter, pNode, iDepth);
		if ( iRet != XUI_OK ) return iRet;
	}
	return pWriter->iStatus;
}

XUI_API int xuiRichDocumentExportHtml(xui_rich_document pDocument, char** ppText, size_t* pSize)
{
	xui_rich_text_writer_t tWriter;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || ppText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppText = NULL; if ( pSize != NULL ) *pSize = 0;
	__xuiRichTextWriterInit(&tWriter);
	(void)__xuiRichTextWriterAppend(&tWriter, "<article data-xui-rich-document=\"1\">\n");
	iRet = __xuiRichHtmlWriteDocumentBody(&tWriter, pDocument, 0);
	if ( iRet == XUI_OK ) (void)__xuiRichTextWriterAppend(&tWriter, "</article>\n");
	if ( iRet != XUI_OK ) { xrtBufferUnit(&tWriter.tBuffer); return iRet; }
	return __xuiRichTextWriterFinish(&tWriter, ppText, pSize);
}

typedef struct xui_rich_html_tag_t {
	char sName[32];
	int bClosing;
	int bSelfClosing;
	size_t iAttrStart;
	size_t iAttrEnd;
} xui_rich_html_tag_t;

typedef struct xui_rich_html_parser_t {
	const char* sText;
	size_t iSize;
	size_t iPos;
	int iDepth;
	int iNodeCount;
	int iStatus;
} xui_rich_html_parser_t;

static int __xuiRichHtmlNameChar(unsigned char ch)
{
	return isalnum(ch) || ch == '-' || ch == '_' || ch == ':';
}

static void __xuiRichHtmlSkipSpace(xui_rich_html_parser_t* pParser)
{
	while ( pParser->iPos < pParser->iSize && isspace((unsigned char)pParser->sText[pParser->iPos]) ) pParser->iPos++;
}




/* 在非零结尾字节区间内查找固定片段，替代非标准 GNU memmem。 */
static const char* __xuiRichFindBytes(const char* sText, size_t iTextSize,
	const char* sPart, size_t iPartSize)
{
	size_t i;

	if ( (sText == NULL) || (sPart == NULL) ||
		 (iPartSize == 0u) || (iPartSize > iTextSize) ) return NULL;
	for ( i = 0u; i <= (iTextSize - iPartSize); i++ ) {
		if ( (sText[i] == sPart[0]) &&
			 (memcmp(sText + i, sPart, iPartSize) == 0) ) return sText + i;
	}
	return NULL;
}



static int __xuiRichHtmlReadTag(xui_rich_html_parser_t* pParser, xui_rich_html_tag_t* pTag)
{
	size_t iNameStart;
	size_t iNameEnd;
	char chQuote = 0;
	if ( pParser->iPos >= pParser->iSize || pParser->sText[pParser->iPos] != '<' ) return 0;
	memset(pTag, 0, sizeof(*pTag));
	if ( pParser->iPos + 4 <= pParser->iSize && memcmp(pParser->sText + pParser->iPos, "<!--", 4) == 0 ) {
		const char* pEnd = __xuiRichFindBytes(
			pParser->sText + pParser->iPos + 4,
			pParser->iSize - pParser->iPos - 4, "-->", 3u);
		pParser->iPos = pEnd != NULL ? (size_t)(pEnd - pParser->sText) + 3 : pParser->iSize;
		strcpy(pTag->sName, "!comment"); pTag->bSelfClosing = 1; return 1;
	}
	pParser->iPos++;
	if ( pParser->iPos < pParser->iSize && pParser->sText[pParser->iPos] == '/' ) {
		pTag->bClosing = 1; pParser->iPos++;
	}
	__xuiRichHtmlSkipSpace(pParser);
	if ( pParser->iPos < pParser->iSize && (pParser->sText[pParser->iPos] == '!' || pParser->sText[pParser->iPos] == '?') ) {
		while ( pParser->iPos < pParser->iSize && pParser->sText[pParser->iPos++] != '>' ) {}
		strcpy(pTag->sName, "!declaration"); pTag->bSelfClosing = 1; return 1;
	}
	iNameStart = pParser->iPos;
	while ( pParser->iPos < pParser->iSize && __xuiRichHtmlNameChar((unsigned char)pParser->sText[pParser->iPos]) ) pParser->iPos++;
	iNameEnd = pParser->iPos;
	if ( iNameEnd == iNameStart || iNameEnd - iNameStart >= sizeof(pTag->sName) ) { pParser->iStatus = XUI_ERROR_INVALID_ARGUMENT; return 0; }
	{
		size_t i;
		for ( i = iNameStart; i < iNameEnd; i++ ) pTag->sName[i - iNameStart] = (char)tolower((unsigned char)pParser->sText[i]);
		pTag->sName[iNameEnd - iNameStart] = 0;
	}
	pTag->iAttrStart = pParser->iPos;
	while ( pParser->iPos < pParser->iSize ) {
		char ch = pParser->sText[pParser->iPos];
		if ( chQuote != 0 ) { if ( ch == chQuote ) chQuote = 0; pParser->iPos++; continue; }
		if ( ch == '"' || ch == '\'' ) { chQuote = ch; pParser->iPos++; continue; }
		if ( ch == '>' ) break;
		pParser->iPos++;
	}
	if ( pParser->iPos >= pParser->iSize ) { pParser->iStatus = XUI_ERROR_INVALID_ARGUMENT; return 0; }
	pTag->iAttrEnd = pParser->iPos;
	{
		size_t i = pTag->iAttrEnd;
		while ( i > pTag->iAttrStart && isspace((unsigned char)pParser->sText[i - 1]) ) i--;
		if ( i > pTag->iAttrStart && pParser->sText[i - 1] == '/' ) pTag->bSelfClosing = 1;
	}
	pParser->iPos++;
	if ( strcmp(pTag->sName, "br") == 0 || strcmp(pTag->sName, "img") == 0 || strcmp(pTag->sName, "hr") == 0 ||
	     strcmp(pTag->sName, "meta") == 0 || strcmp(pTag->sName, "link") == 0 || strcmp(pTag->sName, "input") == 0 )
		pTag->bSelfClosing = 1;
	return 1;
}

static int __xuiRichHtmlAttr(const xui_rich_html_parser_t* pParser, const xui_rich_html_tag_t* pTag,
	const char* sName, char* sValue, int iCapacity)
{
	size_t i = pTag->iAttrStart;
	size_t iNameSize = strlen(sName);
	if ( iCapacity <= 0 ) return 0;
	sValue[0] = 0;
	while ( i < pTag->iAttrEnd ) {
		size_t iStart;
		size_t iEnd;
		size_t iValueStart;
		size_t iValueEnd;
		char chQuote = 0;
		while ( i < pTag->iAttrEnd && (isspace((unsigned char)pParser->sText[i]) || pParser->sText[i] == '/') ) i++;
		iStart = i;
		while ( i < pTag->iAttrEnd && __xuiRichHtmlNameChar((unsigned char)pParser->sText[i]) ) i++;
		iEnd = i;
		while ( i < pTag->iAttrEnd && isspace((unsigned char)pParser->sText[i]) ) i++;
		iValueStart = iValueEnd = i;
		if ( i < pTag->iAttrEnd && pParser->sText[i] == '=' ) {
			i++; while ( i < pTag->iAttrEnd && isspace((unsigned char)pParser->sText[i]) ) i++;
			if ( i < pTag->iAttrEnd && (pParser->sText[i] == '"' || pParser->sText[i] == '\'') ) chQuote = pParser->sText[i++];
			iValueStart = i;
			if ( chQuote != 0 ) while ( i < pTag->iAttrEnd && pParser->sText[i] != chQuote ) i++;
			else while ( i < pTag->iAttrEnd && !isspace((unsigned char)pParser->sText[i]) && pParser->sText[i] != '>' ) i++;
			iValueEnd = i;
			if ( chQuote != 0 && i < pTag->iAttrEnd ) i++;
		}
		if ( iEnd > iStart && iEnd - iStart == iNameSize ) {
			size_t j;
			int bSame = 1;
			for ( j = 0; j < iNameSize; j++ ) if ( tolower((unsigned char)pParser->sText[iStart + j]) != tolower((unsigned char)sName[j]) ) { bSame = 0; break; }
			if ( bSame ) {
				size_t n = iValueEnd - iValueStart;
				if ( n >= (size_t)iCapacity ) n = (size_t)iCapacity - 1u;
				memcpy(sValue, pParser->sText + iValueStart, n); sValue[n] = 0; return 1;
			}
		}
		if ( i == iStart ) i++;
	}
	return 0;
}

static int __xuiRichHtmlEntityCodepoint(const char* sText, int iSize, uint32_t* pCodepoint)
{
	if ( iSize == 2 && memcmp(sText, "lt", 2) == 0 ) *pCodepoint = '<';
	else if ( iSize == 2 && memcmp(sText, "gt", 2) == 0 ) *pCodepoint = '>';
	else if ( iSize == 3 && memcmp(sText, "amp", 3) == 0 ) *pCodepoint = '&';
	else if ( iSize == 4 && memcmp(sText, "quot", 4) == 0 ) *pCodepoint = '"';
	else if ( iSize == 4 && memcmp(sText, "apos", 4) == 0 ) *pCodepoint = '\'';
	else if ( iSize == 4 && memcmp(sText, "nbsp", 4) == 0 ) *pCodepoint = 0x00a0u;
	else if ( iSize > 1 && sText[0] == '#' ) {
		uint64_t iValue = 0;
		int i = 1;
		int iBase = 10;
		if ( i < iSize && (sText[i] == 'x' || sText[i] == 'X') ) { iBase = 16; i++; }
		if ( i >= iSize ) return 0;
		for ( ; i < iSize; i++ ) {
			int d;
			if ( sText[i] >= '0' && sText[i] <= '9' ) d = sText[i] - '0';
			else if ( iBase == 16 && sText[i] >= 'a' && sText[i] <= 'f' ) d = sText[i] - 'a' + 10;
			else if ( iBase == 16 && sText[i] >= 'A' && sText[i] <= 'F' ) d = sText[i] - 'A' + 10;
			else return 0;
			if ( d >= iBase || iValue > 0x10ffffu / (uint64_t)iBase ) return 0;
			iValue = iValue * (uint64_t)iBase + (uint64_t)d;
		}
		if ( iValue == 0 || iValue > 0x10ffffu || (iValue >= 0xd800u && iValue <= 0xdfffu) ) return 0;
		*pCodepoint = (uint32_t)iValue;
	} else return 0;
	return 1;
}

static int __xuiRichHtmlUtf8(xui_rich_text_writer_t* pWriter, uint32_t c)
{
	char sUtf8[4];
	int n;
	if ( c < 0x80u ) { sUtf8[0] = (char)c; n = 1; }
	else if ( c < 0x800u ) { sUtf8[0] = (char)(0xc0u | (c >> 6)); sUtf8[1] = (char)(0x80u | (c & 0x3fu)); n = 2; }
	else if ( c < 0x10000u ) { sUtf8[0] = (char)(0xe0u | (c >> 12)); sUtf8[1] = (char)(0x80u | ((c >> 6) & 0x3fu)); sUtf8[2] = (char)(0x80u | (c & 0x3fu)); n = 3; }
	else { sUtf8[0] = (char)(0xf0u | (c >> 18)); sUtf8[1] = (char)(0x80u | ((c >> 12) & 0x3fu)); sUtf8[2] = (char)(0x80u | ((c >> 6) & 0x3fu)); sUtf8[3] = (char)(0x80u | (c & 0x3fu)); n = 4; }
	return __xuiRichTextWriterAppendN(pWriter, sUtf8, (size_t)n);
}

static char* __xuiRichHtmlDecode(const char* sText, size_t iSize, int* pOutSize)
{
	xui_rich_text_writer_t tWriter;
	size_t i = 0;
	char* sResult = NULL;
	size_t iResultSize = 0;
	__xuiRichTextWriterInit(&tWriter);
	while ( i < iSize ) {
		if ( sText[i] == '&' ) {
			size_t j = i + 1;
			uint32_t c;
			while ( j < iSize && j - i <= 16 && sText[j] != ';' && sText[j] != '&' && sText[j] != '<' ) j++;
			if ( j < iSize && sText[j] == ';' && __xuiRichHtmlEntityCodepoint(sText + i + 1, (int)(j - i - 1), &c) ) {
				(void)__xuiRichHtmlUtf8(&tWriter, c); i = j + 1; continue;
			}
		}
		(void)__xuiRichTextWriterAppendN(&tWriter, sText + i, 1); i++;
	}
	if ( __xuiRichTextWriterFinish(&tWriter, &sResult, &iResultSize) != XUI_OK || iResultSize > INT_MAX ) {
		xrtFree(sResult); return NULL;
	}
	*pOutSize = (int)iResultSize;
	return sResult;
}

static int __xuiRichHtmlTextHasContent(const char* sText, int iSize)
{
	int i;
	for ( i = 0; i < iSize; i++ ) if ( !isspace((unsigned char)sText[i]) ) return 1;
	return 0;
}

static xui_rich_node __xuiRichHtmlAppendTextN(xui_rich_document pDocument, xui_rich_node pBlock,
	const char* sText, int iSize, const xui_rich_text_style_t* pStyle, const char* sLink)
{
	xui_rich_node pNode;
	if ( iSize <= 0 ) return pBlock;
	if ( !__xuiRichUtf8Valid(sText, iSize) || memchr(sText, '\r', (size_t)iSize) != NULL ||
	     memchr(sText, '\n', (size_t)iSize) != NULL ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, sLink != NULL ? XUI_RICH_NODE_LINK : XUI_RICH_NODE_TEXT, 0);
	if ( pNode == NULL ) return NULL;
	pNode->sText = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( pNode->sText == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
	memcpy(pNode->sText, sText, (size_t)iSize); pNode->sText[iSize] = 0; pNode->iTextSize = iSize;
	pNode->tStyle = __xuiRichStyle(pStyle);
	if ( sLink != NULL ) {
		pNode->sResource = __xuiRichString(sLink);
		if ( pNode->sResource == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
		if ( pStyle == NULL ) { pNode->tStyle.iTextColor = XUI_COLOR_RGBA(20,92,170,255); pNode->tStyle.iFlags |= XUI_RICH_STYLE_UNDERLINE; }
	}
	__xuiRichNodeAppend(pBlock, pNode);
	return pNode;
}

static int __xuiRichHtmlHex(unsigned char ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static int __xuiRichHtmlParseColor(const char* sText, uint32_t* pColor)
{
	int n = (int)strlen(sText);
	int d[8];
	int i;
	while ( n > 0 && isspace((unsigned char)*sText) ) { sText++; n--; }
	while ( n > 0 && isspace((unsigned char)sText[n - 1]) ) n--;
	if ( n == 7 && sText[0] == '#' ) {
		for ( i = 0; i < 6; i++ ) { d[i] = __xuiRichHtmlHex((unsigned char)sText[i + 1]); if ( d[i] < 0 ) return 0; }
		*pColor = XUI_COLOR_RGBA(d[0] * 16 + d[1], d[2] * 16 + d[3], d[4] * 16 + d[5], 255); return 1;
	}
	if ( n == 4 && sText[0] == '#' ) {
		for ( i = 0; i < 3; i++ ) { d[i] = __xuiRichHtmlHex((unsigned char)sText[i + 1]); if ( d[i] < 0 ) return 0; }
		*pColor = XUI_COLOR_RGBA(d[0] * 17, d[1] * 17, d[2] * 17, 255); return 1;
	}
	return 0;
}

static void __xuiRichHtmlTrim(char** ppStart, char** ppEnd)
{
	while ( *ppStart < *ppEnd && isspace((unsigned char)**ppStart) ) (*ppStart)++;
	while ( *ppEnd > *ppStart && isspace((unsigned char)(*ppEnd)[-1]) ) (*ppEnd)--;
}

static int __xuiRichHtmlCssValue(const char* sStyle, const char* sName, char* sOut, int iCapacity)
{
	const char* p = sStyle;
	size_t iNameSize = strlen(sName);
	while ( p != NULL && *p != 0 ) {
		const char* pColon;
		const char* pEnd = strchr(p, ';');
		char* pNameStart = (char*)(void*)p;
		char* pNameEnd;
		char* pValueStart;
		char* pValueEnd;
		if ( pEnd == NULL ) pEnd = p + strlen(p);
		pColon = (const char*)memchr(p, ':', (size_t)(pEnd - p));
		if ( pColon != NULL ) {
			pNameEnd = (char*)(void*)pColon; pValueStart = (char*)(void*)(pColon + 1); pValueEnd = (char*)(void*)pEnd;
			__xuiRichHtmlTrim(&pNameStart, &pNameEnd); __xuiRichHtmlTrim(&pValueStart, &pValueEnd);
			if ( (size_t)(pNameEnd - pNameStart) == iNameSize ) {
				size_t i;
				int bSame = 1;
				for ( i = 0; i < iNameSize; i++ ) if ( tolower((unsigned char)pNameStart[i]) != tolower((unsigned char)sName[i]) ) { bSame = 0; break; }
				if ( bSame ) {
					size_t n = (size_t)(pValueEnd - pValueStart);
					if ( n >= (size_t)iCapacity ) n = (size_t)iCapacity - 1u;
					memcpy(sOut, pValueStart, n); sOut[n] = 0; return 1;
				}
			}
		}
		p = *pEnd != 0 ? pEnd + 1 : NULL;
	}
	return 0;
}

static float __xuiRichHtmlCssFloat(const char* sValue)
{
	char* pEnd = NULL;
	double f = strtod(sValue, &pEnd);
	return pEnd != sValue && isfinite(f) ? (float)f : 0.0f;
}

static void __xuiRichHtmlApplyTextCss(const char* sCss, xui_rich_text_style_t* pStyle)
{
	char sValue[96];
	uint32_t iColor;
	if ( __xuiRichHtmlCssValue(sCss, "color", sValue, sizeof(sValue)) && __xuiRichHtmlParseColor(sValue, &iColor) ) pStyle->iTextColor = iColor;
	if ( __xuiRichHtmlCssValue(sCss, "background-color", sValue, sizeof(sValue)) && __xuiRichHtmlParseColor(sValue, &iColor) ) pStyle->iBackgroundColor = iColor;
	if ( __xuiRichHtmlCssValue(sCss, "font-size", sValue, sizeof(sValue)) ) pStyle->fFontSize = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "font-weight", sValue, sizeof(sValue)) ) {
		if ( strcmp(sValue, "bold") == 0 ) { pStyle->iFlags |= XUI_RICH_STYLE_BOLD; pStyle->iWeight = 700; }
		else { pStyle->iWeight = atoi(sValue); if ( pStyle->iWeight >= 600 ) pStyle->iFlags |= XUI_RICH_STYLE_BOLD; }
	}
	if ( __xuiRichHtmlCssValue(sCss, "font-style", sValue, sizeof(sValue)) &&
	     (strcmp(sValue, "italic") == 0 || strcmp(sValue, "oblique") == 0) ) pStyle->iFlags |= XUI_RICH_STYLE_ITALIC;
	if ( __xuiRichHtmlCssValue(sCss, "vertical-align", sValue, sizeof(sValue)) ) pStyle->fBaselineShift = __xuiRichHtmlCssFloat(sValue);
}

static void __xuiRichHtmlApplyParagraphCss(const char* sCss, xui_rich_paragraph_style_t* pStyle)
{
	char sValue[96];
	uint32_t iColor;
	if ( __xuiRichHtmlCssValue(sCss, "text-align", sValue, sizeof(sValue)) ) {
		if ( strcmp(sValue, "center") == 0 ) pStyle->iAlign = XUI_RICH_ALIGN_CENTER;
		else if ( strcmp(sValue, "right") == 0 ) pStyle->iAlign = XUI_RICH_ALIGN_RIGHT;
		else if ( strcmp(sValue, "justify") == 0 ) pStyle->iAlign = XUI_RICH_ALIGN_JUSTIFY;
		else pStyle->iAlign = XUI_RICH_ALIGN_LEFT;
	}
	if ( __xuiRichHtmlCssValue(sCss, "direction", sValue, sizeof(sValue)) )
		pStyle->iDirection = strcmp(sValue, "rtl") == 0 ? XUI_RICH_DIRECTION_RTL : XUI_RICH_DIRECTION_LTR;
	if ( __xuiRichHtmlCssValue(sCss, "margin-left", sValue, sizeof(sValue)) ) pStyle->fIndentLeft = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "margin-right", sValue, sizeof(sValue)) ) pStyle->fIndentRight = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "text-indent", sValue, sizeof(sValue)) ) pStyle->fFirstLineIndent = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "line-height", sValue, sizeof(sValue)) ) pStyle->fLineHeight = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "margin-top", sValue, sizeof(sValue)) ) pStyle->fSpaceBefore = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "margin-bottom", sValue, sizeof(sValue)) ) pStyle->fSpaceAfter = __xuiRichHtmlCssFloat(sValue);
	if ( __xuiRichHtmlCssValue(sCss, "background-color", sValue, sizeof(sValue)) && __xuiRichHtmlParseColor(sValue, &iColor) ) pStyle->iBackgroundColor = iColor;
	if ( __xuiRichHtmlCssValue(sCss, "border-color", sValue, sizeof(sValue)) && __xuiRichHtmlParseColor(sValue, &iColor) ) pStyle->iBorderColor = iColor;
}

static int __xuiRichHtmlParseInline(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	xui_rich_node pBlock, const char* sClosing, xui_rich_text_style_t tStyle, const char* sLink);

static int __xuiRichHtmlParseInlineTag(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	xui_rich_node pBlock, const xui_rich_html_tag_t* pTag, xui_rich_text_style_t tStyle, const char* sLink)
{
	char sValue[2048];
	char* sDecoded = NULL;
	int iDecodedSize = 0;
	if ( strcmp(pTag->sName, "br") == 0 ) {
		return __xuiRichHtmlAppendTextN(pDocument, pBlock, " ", 1, &tStyle, sLink) != NULL ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( strcmp(pTag->sName, "img") == 0 ) {
		xui_rich_node pImage;
		char sAlt[2048];
		char sWidth[64];
		char sHeight[64];
		char sBaseline[64];
		if ( !__xuiRichHtmlAttr(pParser, pTag, "src", sValue, sizeof(sValue)) || sValue[0] == 0 ) return XUI_OK;
		sDecoded = __xuiRichHtmlDecode(sValue, strlen(sValue), &iDecodedSize);
		if ( sDecoded == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pImage = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_INLINE_IMAGE, 0);
		if ( pImage == NULL ) { xrtFree(sDecoded); return XUI_ERROR_OUT_OF_MEMORY; }
		pImage->sResource = sDecoded;
		if ( __xuiRichHtmlAttr(pParser, pTag, "alt", sAlt, sizeof(sAlt)) ) pImage->sAltText = __xuiRichHtmlDecode(sAlt, strlen(sAlt), &iDecodedSize);
		if ( __xuiRichHtmlAttr(pParser, pTag, "width", sWidth, sizeof(sWidth)) ) pImage->fWidth = __xuiRichHtmlCssFloat(sWidth);
		if ( __xuiRichHtmlAttr(pParser, pTag, "height", sHeight, sizeof(sHeight)) ) pImage->fHeight = __xuiRichHtmlCssFloat(sHeight);
		if ( __xuiRichHtmlAttr(pParser, pTag, "data-baseline", sBaseline, sizeof(sBaseline)) ) pImage->fBaseline = __xuiRichHtmlCssFloat(sBaseline);
		else pImage->fBaseline = pImage->fHeight;
		__xuiRichNodeAppend(pBlock, pImage); return XUI_OK;
	}
	if ( strcmp(pTag->sName, "strong") == 0 || strcmp(pTag->sName, "b") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_BOLD;
	else if ( strcmp(pTag->sName, "em") == 0 || strcmp(pTag->sName, "i") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_ITALIC;
	else if ( strcmp(pTag->sName, "u") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_UNDERLINE;
	else if ( strcmp(pTag->sName, "s") == 0 || strcmp(pTag->sName, "strike") == 0 || strcmp(pTag->sName, "del") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_STRIKEOUT;
	else if ( strcmp(pTag->sName, "code") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_CODE;
	else if ( strcmp(pTag->sName, "sub") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_SUBSCRIPT;
	else if ( strcmp(pTag->sName, "sup") == 0 ) tStyle.iFlags |= XUI_RICH_STYLE_SUPERSCRIPT;
	if ( __xuiRichHtmlAttr(pParser, pTag, "style", sValue, sizeof(sValue)) ) __xuiRichHtmlApplyTextCss(sValue, &tStyle);
	if ( strcmp(pTag->sName, "a") == 0 && __xuiRichHtmlAttr(pParser, pTag, "href", sValue, sizeof(sValue)) ) {
		sDecoded = __xuiRichHtmlDecode(sValue, strlen(sValue), &iDecodedSize);
		if ( sDecoded == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		sLink = sDecoded;
	}
	if ( !pTag->bSelfClosing ) {
		int iRet = __xuiRichHtmlParseInline(pParser, pDocument, pBlock, pTag->sName, tStyle, sLink);
		xrtFree(sDecoded); return iRet;
	}
	xrtFree(sDecoded);
	return XUI_OK;
}

static int __xuiRichHtmlParseInline(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	xui_rich_node pBlock, const char* sClosing, xui_rich_text_style_t tStyle, const char* sLink)
{
	if ( ++pParser->iDepth > XUI_RICH_SERIAL_MAX_DEPTH ) return XUI_ERROR_UNSUPPORTED;
	while ( pParser->iPos < pParser->iSize ) {
		if ( pParser->sText[pParser->iPos] != '<' ) {
			size_t iStart = pParser->iPos;
			char* sDecoded;
			int iDecodedSize;
			while ( pParser->iPos < pParser->iSize && pParser->sText[pParser->iPos] != '<' ) pParser->iPos++;
			sDecoded = __xuiRichHtmlDecode(pParser->sText + iStart, pParser->iPos - iStart, &iDecodedSize);
			if ( sDecoded == NULL || (__xuiRichHtmlTextHasContent(sDecoded, iDecodedSize) &&
			     __xuiRichHtmlAppendTextN(pDocument, pBlock, sDecoded, iDecodedSize, &tStyle, sLink) == NULL) ) {
				xrtFree(sDecoded); pParser->iDepth--; return XUI_ERROR_OUT_OF_MEMORY;
			}
			xrtFree(sDecoded); continue;
		}
		{
			xui_rich_html_tag_t tTag;
			int iRet;
			if ( !__xuiRichHtmlReadTag(pParser, &tTag) ) { pParser->iDepth--; return pParser->iStatus; }
			if ( tTag.bClosing ) {
				pParser->iDepth--;
				return sClosing == NULL || strcmp(tTag.sName, sClosing) == 0 ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
			}
			if ( tTag.sName[0] == '!' ) continue;
			iRet = __xuiRichHtmlParseInlineTag(pParser, pDocument, pBlock, &tTag, tStyle, sLink);
			if ( iRet != XUI_OK ) { pParser->iDepth--; return iRet; }
		}
	}
	pParser->iDepth--;
	return sClosing == NULL ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
}

static xui_rich_document __xuiRichHtmlCreateEmptyDocument(xui_rich_document pOwner,
	xui_document_node_id_t iOwnerTableId)
{
	xui_rich_document pDocument = (xui_rich_document)xrtCalloc(1, sizeof(*pDocument));
	if ( pDocument == NULL ) return NULL;
	pDocument->iMagic = XUI_RICH_DOCUMENT_MAGIC; pDocument->iNextId = 1; pDocument->iVersion = 1; pDocument->bFlatDirty = 1;
	pDocument->pOwnerDocument = pOwner; pDocument->iOwnerTableId = iOwnerTableId;
	pDocument->pRoot = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_DOCUMENT, 0);
	if ( pDocument->pRoot == NULL ) { pDocument->iMagic = 0; xrtFree(pDocument); return NULL; }
	return pDocument;
}

static int __xuiRichHtmlSkipElement(xui_rich_html_parser_t* pParser, const char* sName)
{
	int iDepth = 1;
	while ( pParser->iPos < pParser->iSize && iDepth > 0 ) {
		if ( pParser->sText[pParser->iPos] != '<' ) { pParser->iPos++; continue; }
		{
			xui_rich_html_tag_t tTag;
			if ( !__xuiRichHtmlReadTag(pParser, &tTag) ) return pParser->iStatus;
			if ( strcmp(tTag.sName, sName) != 0 ) continue;
			if ( tTag.bClosing ) iDepth--;
			else if ( !tTag.bSelfClosing ) iDepth++;
		}
	}
	return iDepth == 0 ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
}

static xui_rich_node __xuiRichHtmlImageNode(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	const xui_rich_html_tag_t* pTag, int bInline)
{
	char sSource[2048];
	char sAlt[2048];
	char sNumber[64];
	char* sDecoded;
	int iDecodedSize;
	xui_rich_node pNode;
	if ( !__xuiRichHtmlAttr(pParser, pTag, "src", sSource, sizeof(sSource)) || sSource[0] == 0 ) return NULL;
	sDecoded = __xuiRichHtmlDecode(sSource, strlen(sSource), &iDecodedSize);
	if ( sDecoded == NULL ) return NULL;
	pNode = __xuiRichNodeCreate(pDocument, bInline ? XUI_RICH_NODE_INLINE_IMAGE : XUI_RICH_NODE_IMAGE, 0);
	if ( pNode == NULL ) { xrtFree(sDecoded); return NULL; }
	pNode->sResource = sDecoded;
	if ( __xuiRichHtmlAttr(pParser, pTag, "alt", sAlt, sizeof(sAlt)) ) {
		pNode->sAltText = __xuiRichHtmlDecode(sAlt, strlen(sAlt), &iDecodedSize);
		if ( pNode->sAltText == NULL ) { __xuiRichNodeDestroy(pNode); return NULL; }
	}
	if ( __xuiRichHtmlAttr(pParser, pTag, "width", sNumber, sizeof(sNumber)) ) pNode->fWidth = __xuiRichHtmlCssFloat(sNumber);
	if ( __xuiRichHtmlAttr(pParser, pTag, "height", sNumber, sizeof(sNumber)) ) pNode->fHeight = __xuiRichHtmlCssFloat(sNumber);
	if ( __xuiRichHtmlAttr(pParser, pTag, "data-baseline", sNumber, sizeof(sNumber)) ) pNode->fBaseline = __xuiRichHtmlCssFloat(sNumber);
	else pNode->fBaseline = pNode->fHeight;
	return pNode;
}

static int __xuiRichHtmlParseBlocks(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	const char* sClosing, int iListType, int iListLevel);

static xui_rich_node __xuiRichHtmlParseParagraph(xui_rich_html_parser_t* pParser,
	xui_rich_document pDocument, const xui_rich_html_tag_t* pTag, int iType, int iListType, int iListLevel)
{
	xui_rich_node pBlock;
	xui_rich_text_style_t tStyle;
	xui_rich_paragraph_style_t tParagraph = __xuiRichParagraphStyle(NULL);
	char sValue[2048];
	if ( __xuiRichHtmlAttr(pParser, pTag, "style", sValue, sizeof(sValue)) ) __xuiRichHtmlApplyParagraphCss(sValue, &tParagraph);
	if ( iType == XUI_RICH_NODE_HEADING ) {
		tParagraph.iHeadingLevel = pTag->sName[1] >= '1' && pTag->sName[1] <= '6' ? pTag->sName[1] - '0' : 1;
	}
	if ( iType == XUI_RICH_NODE_LIST_ITEM ) {
		char sList[32];
		char sLevel[32];
		char sChecked[32];
		tParagraph.iListType = iListType != XUI_RICH_LIST_NONE ? iListType : XUI_RICH_LIST_BULLET;
		tParagraph.iListLevel = iListLevel;
		if ( __xuiRichHtmlAttr(pParser, pTag, "data-list", sList, sizeof(sList)) ) {
			if ( strcmp(sList, "number") == 0 ) tParagraph.iListType = XUI_RICH_LIST_NUMBER;
			else if ( strcmp(sList, "check") == 0 ) tParagraph.iListType = XUI_RICH_LIST_CHECK;
			else tParagraph.iListType = XUI_RICH_LIST_BULLET;
		}
		if ( __xuiRichHtmlAttr(pParser, pTag, "data-level", sLevel, sizeof(sLevel)) ) {
			int n = atoi(sLevel); if ( n >= 0 && n <= 1024 ) tParagraph.iListLevel = n;
		}
		if ( __xuiRichHtmlAttr(pParser, pTag, "data-checked", sChecked, sizeof(sChecked)) )
			tParagraph.bListChecked = strcmp(sChecked, "true") == 0 || strcmp(sChecked, "1") == 0;
	}
	pBlock = __xuiRichNodeCreate(pDocument, iType, 0);
	if ( pBlock == NULL ) return NULL;
	pBlock->tParagraphStyle = tParagraph;
	__xuiRichNodeAppend(pDocument->pRoot, pBlock);
	memset(&tStyle, 0, sizeof(tStyle)); tStyle.iSize = sizeof(tStyle);
	if ( __xuiRichHtmlParseInline(pParser, pDocument, pBlock, pTag->sName, tStyle, NULL) != XUI_OK ) return NULL;
	return pBlock;
}

typedef struct xui_rich_html_cells_t {
	xui_rich_document* pItems;
	int iCount;
	int iCapacity;
	int iRows;
	int iColumns;
} xui_rich_html_cells_t;

static void __xuiRichHtmlCellsUnit(xui_rich_html_cells_t* pCells)
{
	int i;
	for ( i = 0; i < pCells->iCount; i++ ) xuiRichDocumentDestroy(pCells->pItems[i]);
	xrtFree(pCells->pItems); memset(pCells, 0, sizeof(*pCells));
}

static int __xuiRichHtmlCellsAppend(xui_rich_html_cells_t* pCells, xui_rich_document pCell)
{
	if ( pCells->iCount == pCells->iCapacity ) {
		int iNewCapacity = pCells->iCapacity > 0 ? pCells->iCapacity * 2 : 16;
		xui_rich_document* pNew = (xui_rich_document*)xrtRealloc(pCells->pItems, (size_t)iNewCapacity * sizeof(*pNew));
		if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pCells->pItems = pNew; pCells->iCapacity = iNewCapacity;
	}
	pCells->pItems[pCells->iCount++] = pCell;
	return XUI_OK;
}

static xui_rich_node __xuiRichHtmlParseTable(xui_rich_html_parser_t* pParser,
	xui_rich_document pDocument, const xui_rich_html_tag_t* pTableTag)
{
	xui_rich_html_cells_t tCells;
	xui_rich_node pTable = NULL;
	int iCurrentColumns = 0;
	int bInRow = 0;
	char sValue[64];
	memset(&tCells, 0, sizeof(tCells));
	while ( pParser->iPos < pParser->iSize ) {
		if ( pParser->sText[pParser->iPos] != '<' ) { pParser->iPos++; continue; }
		{
			xui_rich_html_tag_t tTag;
			if ( !__xuiRichHtmlReadTag(pParser, &tTag) ) goto failed;
			if ( tTag.bClosing && strcmp(tTag.sName, pTableTag->sName) == 0 ) break;
			if ( !tTag.bClosing && strcmp(tTag.sName, "tr") == 0 ) { bInRow = 1; iCurrentColumns = 0; continue; }
			if ( tTag.bClosing && strcmp(tTag.sName, "tr") == 0 ) {
				if ( bInRow && iCurrentColumns > 0 ) {
					if ( tCells.iColumns == 0 ) tCells.iColumns = iCurrentColumns;
					else if ( tCells.iColumns != iCurrentColumns ) goto failed;
					tCells.iRows++;
				}
				bInRow = 0; continue;
			}
			if ( !tTag.bClosing && bInRow && (strcmp(tTag.sName, "td") == 0 || strcmp(tTag.sName, "th") == 0) ) {
				xui_rich_document pCell = __xuiRichHtmlCreateEmptyDocument(NULL, 0);
				if ( pCell == NULL ) goto failed;
				if ( __xuiRichHtmlParseBlocks(pParser, pCell, tTag.sName, XUI_RICH_LIST_NONE, 0) != XUI_OK ) {
					xuiRichDocumentDestroy(pCell); goto failed;
				}
				if ( pCell->pRoot->pFirstChild == NULL ) {
					xui_rich_node pParagraph = __xuiRichNodeCreate(pCell, XUI_RICH_NODE_PARAGRAPH, 0);
					if ( pParagraph == NULL ) { xuiRichDocumentDestroy(pCell); goto failed; }
					__xuiRichNodeAppend(pCell->pRoot, pParagraph);
				}
				if ( __xuiRichHtmlCellsAppend(&tCells, pCell) != XUI_OK ) { xuiRichDocumentDestroy(pCell); goto failed; }
				iCurrentColumns++; continue;
			}
			if ( !tTag.bClosing && !tTag.bSelfClosing &&
			     (strcmp(tTag.sName, "thead") == 0 || strcmp(tTag.sName, "tbody") == 0 || strcmp(tTag.sName, "tfoot") == 0) ) continue;
		}
	}
	if ( tCells.iRows <= 0 || tCells.iColumns <= 0 || tCells.iCount != tCells.iRows * tCells.iColumns ||
	     tCells.iRows > 1024 || tCells.iColumns > 256 ) goto failed;
	pTable = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_TABLE, 0);
	if ( pTable == NULL ) goto failed;
	pTable->iRows = tCells.iRows; pTable->iColumns = tCells.iColumns; pTable->fCellPadding = 6.0f; pTable->fBorderWidth = 1.0f;
	if ( __xuiRichHtmlAttr(pParser, pTableTag, "width", sValue, sizeof(sValue)) ) pTable->fWidth = __xuiRichHtmlCssFloat(sValue);
	pTable->ppCellDocuments = tCells.pItems;
	{
		int i;
		for ( i = 0; i < tCells.iCount; i++ ) {
			tCells.pItems[i]->pOwnerDocument = pDocument;
			tCells.pItems[i]->iOwnerTableId = pTable->iId;
		}
	}
	tCells.pItems = NULL; tCells.iCount = tCells.iCapacity = 0;
	__xuiRichNodeAppend(pDocument->pRoot, pTable);
	return pTable;

failed:
	__xuiRichNodeDestroy(pTable); __xuiRichHtmlCellsUnit(&tCells); pParser->iStatus = XUI_ERROR_INVALID_ARGUMENT;
	return NULL;
}

static int __xuiRichHtmlParseBlocks(xui_rich_html_parser_t* pParser, xui_rich_document pDocument,
	const char* sClosing, int iListType, int iListLevel)
{
	if ( ++pParser->iDepth > XUI_RICH_SERIAL_MAX_DEPTH ) return XUI_ERROR_UNSUPPORTED;
	while ( pParser->iPos < pParser->iSize ) {
		if ( pParser->sText[pParser->iPos] != '<' ) {
			size_t iStart = pParser->iPos;
			char* sDecoded;
			int iDecodedSize;
			while ( pParser->iPos < pParser->iSize && pParser->sText[pParser->iPos] != '<' ) pParser->iPos++;
			sDecoded = __xuiRichHtmlDecode(pParser->sText + iStart, pParser->iPos - iStart, &iDecodedSize);
			if ( sDecoded == NULL ) { pParser->iDepth--; return XUI_ERROR_OUT_OF_MEMORY; }
			if ( __xuiRichHtmlTextHasContent(sDecoded, iDecodedSize) ) {
				xui_rich_node pBlock = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_PARAGRAPH, 0);
				xui_rich_text_style_t tStyle;
				if ( pBlock == NULL ) { xrtFree(sDecoded); pParser->iDepth--; return XUI_ERROR_OUT_OF_MEMORY; }
				__xuiRichNodeAppend(pDocument->pRoot, pBlock); memset(&tStyle, 0, sizeof(tStyle)); tStyle.iSize = sizeof(tStyle);
				if ( __xuiRichHtmlAppendTextN(pDocument, pBlock, sDecoded, iDecodedSize, &tStyle, NULL) == NULL ) {
					xrtFree(sDecoded); pParser->iDepth--; return XUI_ERROR_OUT_OF_MEMORY;
				}
			}
			xrtFree(sDecoded); continue;
		}
		{
			xui_rich_html_tag_t tTag;
			if ( !__xuiRichHtmlReadTag(pParser, &tTag) ) { pParser->iDepth--; return pParser->iStatus; }
			if ( tTag.bClosing ) {
				pParser->iDepth--;
				return sClosing != NULL && strcmp(tTag.sName, sClosing) == 0 ? XUI_OK :
					(sClosing == NULL ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT);
			}
			if ( tTag.sName[0] == '!' ) continue;
			if ( strcmp(tTag.sName, "script") == 0 || strcmp(tTag.sName, "style") == 0 ) {
				int iRet = tTag.bSelfClosing ? XUI_OK : __xuiRichHtmlSkipElement(pParser, tTag.sName);
				if ( iRet != XUI_OK ) { pParser->iDepth--; return iRet; }
				continue;
			}
			if ( strcmp(tTag.sName, "p") == 0 || strcmp(tTag.sName, "div") == 0 ) {
				if ( __xuiRichHtmlParseParagraph(pParser, pDocument, &tTag, XUI_RICH_NODE_PARAGRAPH, XUI_RICH_LIST_NONE, 0) == NULL ) goto failed;
			} else if ( tTag.sName[0] == 'h' && tTag.sName[1] >= '1' && tTag.sName[1] <= '6' && tTag.sName[2] == 0 ) {
				if ( __xuiRichHtmlParseParagraph(pParser, pDocument, &tTag, XUI_RICH_NODE_HEADING, XUI_RICH_LIST_NONE, 0) == NULL ) goto failed;
			} else if ( strcmp(tTag.sName, "blockquote") == 0 ) {
				if ( __xuiRichHtmlParseParagraph(pParser, pDocument, &tTag, XUI_RICH_NODE_BLOCK_QUOTE, XUI_RICH_LIST_NONE, 0) == NULL ) goto failed;
			} else if ( strcmp(tTag.sName, "li") == 0 ) {
				if ( __xuiRichHtmlParseParagraph(pParser, pDocument, &tTag, XUI_RICH_NODE_LIST_ITEM, iListType, iListLevel) == NULL ) goto failed;
			} else if ( strcmp(tTag.sName, "ul") == 0 || strcmp(tTag.sName, "ol") == 0 ) {
				int iType = strcmp(tTag.sName, "ol") == 0 ? XUI_RICH_LIST_NUMBER : XUI_RICH_LIST_BULLET;
				if ( __xuiRichHtmlParseBlocks(pParser, pDocument, tTag.sName, iType, iListLevel + (iListType != XUI_RICH_LIST_NONE)) != XUI_OK ) goto failed;
			} else if ( strcmp(tTag.sName, "hr") == 0 ) {
				xui_rich_node pRule = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_HORIZONTAL_RULE, 0);
				if ( pRule == NULL ) goto failed;
				__xuiRichNodeAppend(pDocument->pRoot, pRule);
			} else if ( strcmp(tTag.sName, "img") == 0 ) {
				xui_rich_node pImage = __xuiRichHtmlImageNode(pParser, pDocument, &tTag, 0);
				if ( pImage == NULL ) goto failed;
				__xuiRichNodeAppend(pDocument->pRoot, pImage);
			} else if ( strcmp(tTag.sName, "figure") == 0 ) {
				size_t iSaved;
				xui_rich_html_tag_t tImageTag;
				while ( pParser->iPos < pParser->iSize && isspace((unsigned char)pParser->sText[pParser->iPos]) ) pParser->iPos++;
				iSaved = pParser->iPos;
				if ( pParser->iPos < pParser->iSize && pParser->sText[pParser->iPos] == '<' &&
				     __xuiRichHtmlReadTag(pParser, &tImageTag) && !tImageTag.bClosing && strcmp(tImageTag.sName, "img") == 0 ) {
					xui_rich_node pImage = __xuiRichHtmlImageNode(pParser, pDocument, &tImageTag, 0);
					char sStyle[2048];
					if ( pImage == NULL ) goto failed;
					if ( __xuiRichHtmlAttr(pParser, &tTag, "style", sStyle, sizeof(sStyle)) ) __xuiRichHtmlApplyParagraphCss(sStyle, &pImage->tParagraphStyle);
					__xuiRichNodeAppend(pDocument->pRoot, pImage);
					if ( !tTag.bSelfClosing && __xuiRichHtmlSkipElement(pParser, "figure") != XUI_OK ) goto failed;
				} else { pParser->iPos = iSaved; if ( __xuiRichHtmlSkipElement(pParser, "figure") != XUI_OK ) goto failed; }
			} else if ( strcmp(tTag.sName, "table") == 0 ) {
				if ( __xuiRichHtmlParseTable(pParser, pDocument, &tTag) == NULL ) goto failed;
			} else if ( strcmp(tTag.sName, "html") == 0 || strcmp(tTag.sName, "body") == 0 || strcmp(tTag.sName, "main") == 0 ||
			            strcmp(tTag.sName, "section") == 0 || strcmp(tTag.sName, "article") == 0 ) {
				if ( !tTag.bSelfClosing && __xuiRichHtmlParseBlocks(pParser, pDocument, tTag.sName, iListType, iListLevel) != XUI_OK ) goto failed;
			} else if ( !tTag.bSelfClosing ) {
				if ( __xuiRichHtmlParseBlocks(pParser, pDocument, tTag.sName, iListType, iListLevel) != XUI_OK ) goto failed;
			}
		}
	}
	pParser->iDepth--;
	return sClosing == NULL ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;

failed:
	pParser->iDepth--;
	return pParser->iStatus != XUI_OK ? pParser->iStatus : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiRichDocumentImportHtml(const char* sText, size_t iSize, xui_rich_document* ppDocument)
{
	xui_rich_html_parser_t tParser;
	xui_rich_document pDocument;
	int iRet;
	if ( sText == NULL || ppDocument == NULL || iSize > XUI_RICH_SERIAL_MAX_INPUT ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	if ( iSize == 0 ) iSize = strlen(sText);
	if ( iSize == 0 || iSize > XUI_RICH_SERIAL_MAX_INPUT || iSize > INT_MAX || !__xuiRichUtf8Valid(sText, (int)iSize) )
		return XUI_ERROR_INVALID_ARGUMENT;
	pDocument = __xuiRichHtmlCreateEmptyDocument(NULL, 0);
	if ( pDocument == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(&tParser, 0, sizeof(tParser)); tParser.sText = sText; tParser.iSize = iSize; tParser.iStatus = XUI_OK;
	iRet = __xuiRichHtmlParseBlocks(&tParser, pDocument, NULL, XUI_RICH_LIST_NONE, 0);
	if ( iRet == XUI_OK && pDocument->pRoot->pFirstChild == NULL ) {
		xui_rich_node pParagraph = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_PARAGRAPH, 0);
		if ( pParagraph == NULL ) iRet = XUI_ERROR_OUT_OF_MEMORY; else __xuiRichNodeAppend(pDocument->pRoot, pParagraph);
	}
	if ( iRet != XUI_OK ) { xuiRichDocumentDestroy(pDocument); return iRet; }
	*ppDocument = pDocument;
	return XUI_OK;
}

static int __xuiRichMarkdownEscape(xui_rich_text_writer_t* pWriter, const char* sText, int iSize)
{
	int i;
	if ( sText == NULL ) return XUI_OK;
	if ( iSize < 0 ) iSize = (int)strlen(sText);
	for ( i = 0; i < iSize; i++ ) {
		if ( strchr("\\`*_{}[]<>()#+!|~", sText[i]) != NULL ) (void)__xuiRichTextWriterAppendN(pWriter, "\\", 1);
		(void)__xuiRichTextWriterAppendN(pWriter, sText + i, 1);
	}
	return pWriter->iStatus;
}

static int __xuiRichMarkdownWriteInline(xui_rich_text_writer_t* pWriter, xui_rich_node pNode)
{
	xui_rich_node_info_t tInfo;
	uint32_t iFlags;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET ) return XUI_ERROR_UNSUPPORTED;
	if ( tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE ) {
		if ( tInfo.sResource == NULL || tInfo.sResource[0] == 0 ) return XUI_ERROR_UNSUPPORTED;
		(void)__xuiRichTextWriterAppend(pWriter, "!["); __xuiRichMarkdownEscape(pWriter, tInfo.sAltText, -1);
		(void)__xuiRichTextWriterAppend(pWriter, "]("); __xuiRichMarkdownEscape(pWriter, tInfo.sResource, -1);
		return __xuiRichTextWriterAppend(pWriter, ")");
	}
	if ( tInfo.iType != XUI_RICH_NODE_TEXT && tInfo.iType != XUI_RICH_NODE_LINK ) return XUI_ERROR_INVALID_ARGUMENT;
	iFlags = tInfo.tStyle.iFlags;
	if ( tInfo.iType == XUI_RICH_NODE_LINK ) (void)__xuiRichTextWriterAppend(pWriter, "[");
	if ( iFlags & XUI_RICH_STYLE_BOLD ) (void)__xuiRichTextWriterAppend(pWriter, "**");
	if ( iFlags & XUI_RICH_STYLE_ITALIC ) (void)__xuiRichTextWriterAppend(pWriter, "*");
	if ( iFlags & XUI_RICH_STYLE_STRIKEOUT ) (void)__xuiRichTextWriterAppend(pWriter, "~~");
	if ( iFlags & XUI_RICH_STYLE_CODE ) (void)__xuiRichTextWriterAppend(pWriter, "`");
	__xuiRichMarkdownEscape(pWriter, tInfo.sText, tInfo.iTextSize);
	if ( iFlags & XUI_RICH_STYLE_CODE ) (void)__xuiRichTextWriterAppend(pWriter, "`");
	if ( iFlags & XUI_RICH_STYLE_STRIKEOUT ) (void)__xuiRichTextWriterAppend(pWriter, "~~");
	if ( iFlags & XUI_RICH_STYLE_ITALIC ) (void)__xuiRichTextWriterAppend(pWriter, "*");
	if ( iFlags & XUI_RICH_STYLE_BOLD ) (void)__xuiRichTextWriterAppend(pWriter, "**");
	if ( tInfo.iType == XUI_RICH_NODE_LINK ) {
		(void)__xuiRichTextWriterAppend(pWriter, "]("); __xuiRichMarkdownEscape(pWriter, tInfo.sResource, -1);
		(void)__xuiRichTextWriterAppend(pWriter, ")");
	}
	return pWriter->iStatus;
}

static int __xuiRichMarkdownWriteCell(xui_rich_text_writer_t* pWriter, xui_rich_document pCell)
{
	xui_rich_node pBlock;
	int bFirst = 1;
	if ( pCell == NULL ) return XUI_OK;
	for ( pBlock = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pCell)); pBlock != NULL;
	      pBlock = xuiRichNodeGetNextSibling(pBlock) ) {
		xui_rich_node pInline;
		if ( !bFirst ) (void)__xuiRichTextWriterAppend(pWriter, "<br>");
		bFirst = 0;
		for ( pInline = xuiRichNodeGetFirstChild(pBlock); pInline != NULL; pInline = xuiRichNodeGetNextSibling(pInline) ) {
			int iRet = __xuiRichMarkdownWriteInline(pWriter, pInline);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return pWriter->iStatus;
}

static int __xuiRichMarkdownWriteDocument(xui_rich_text_writer_t* pWriter, xui_rich_document pDocument)
{
	xui_rich_node pBlock;
	for ( pBlock = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pDocument)); pBlock != NULL;
	      pBlock = xuiRichNodeGetNextSibling(pBlock) ) {
		xui_rich_node_info_t tInfo;
		xui_rich_node pInline;
		int i;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pBlock, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( tInfo.iType == XUI_RICH_NODE_IMAGE ) {
			if ( tInfo.sResource == NULL || tInfo.sResource[0] == 0 ) return XUI_ERROR_UNSUPPORTED;
			(void)__xuiRichTextWriterAppend(pWriter, "!["); __xuiRichMarkdownEscape(pWriter, tInfo.sAltText, -1);
			(void)__xuiRichTextWriterAppend(pWriter, "]("); __xuiRichMarkdownEscape(pWriter, tInfo.sResource, -1);
			(void)__xuiRichTextWriterAppend(pWriter, ")\n\n"); continue;
		}
		if ( tInfo.iType == XUI_RICH_NODE_HORIZONTAL_RULE ) { (void)__xuiRichTextWriterAppend(pWriter, "---\n\n"); continue; }
		if ( tInfo.iType == XUI_RICH_NODE_TABLE ) {
			int r, c;
			for ( r = 0; r < tInfo.iRows; r++ ) {
				(void)__xuiRichTextWriterAppend(pWriter, "|");
				for ( c = 0; c < tInfo.iColumns; c++ ) {
					xui_rich_document pCell = pBlock->ppCellDocuments != NULL ? pBlock->ppCellDocuments[r * tInfo.iColumns + c] : NULL;
					(void)__xuiRichTextWriterAppend(pWriter, " ");
					if ( __xuiRichMarkdownWriteCell(pWriter, pCell) != XUI_OK ) return pWriter->iStatus;
					(void)__xuiRichTextWriterAppend(pWriter, " |");
				}
				(void)__xuiRichTextWriterAppend(pWriter, "\n");
				if ( r == 0 ) {
					(void)__xuiRichTextWriterAppend(pWriter, "|");
					for ( c = 0; c < tInfo.iColumns; c++ ) (void)__xuiRichTextWriterAppend(pWriter, " --- |");
					(void)__xuiRichTextWriterAppend(pWriter, "\n");
				}
			}
			(void)__xuiRichTextWriterAppend(pWriter, "\n"); continue;
		}
		if ( tInfo.iType == XUI_RICH_NODE_HEADING ) {
			int iLevel = tInfo.tParagraphStyle.iHeadingLevel;
			if ( iLevel < 1 || iLevel > 6 ) iLevel = 1;
			for ( i = 0; i < iLevel; i++ ) (void)__xuiRichTextWriterAppend(pWriter, "#");
			(void)__xuiRichTextWriterAppend(pWriter, " ");
		} else if ( tInfo.iType == XUI_RICH_NODE_BLOCK_QUOTE ) (void)__xuiRichTextWriterAppend(pWriter, "> ");
		else if ( tInfo.iType == XUI_RICH_NODE_LIST_ITEM ) {
			for ( i = 0; i < tInfo.tParagraphStyle.iListLevel; i++ ) (void)__xuiRichTextWriterAppend(pWriter, "  ");
			if ( tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NUMBER ) (void)__xuiRichTextWriterAppend(pWriter, "1. ");
			else if ( tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_CHECK )
				(void)__xuiRichTextWriterAppend(pWriter, tInfo.tParagraphStyle.bListChecked ? "- [x] " : "- [ ] ");
			else (void)__xuiRichTextWriterAppend(pWriter, "- ");
		}
		for ( pInline = xuiRichNodeGetFirstChild(pBlock); pInline != NULL; pInline = xuiRichNodeGetNextSibling(pInline) ) {
			int iRet = __xuiRichMarkdownWriteInline(pWriter, pInline);
			if ( iRet != XUI_OK ) return iRet;
		}
		(void)__xuiRichTextWriterAppend(pWriter, "\n");
		if ( tInfo.iType != XUI_RICH_NODE_LIST_ITEM ) (void)__xuiRichTextWriterAppend(pWriter, "\n");
	}
	return pWriter->iStatus;
}

XUI_API int xuiRichDocumentExportMarkdown(xui_rich_document pDocument, char** ppText, size_t* pSize)
{
	xui_rich_text_writer_t tWriter;
	int iRet;
	if ( !__xuiRichDocumentValid(pDocument) || ppText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppText = NULL; if ( pSize != NULL ) *pSize = 0;
	__xuiRichTextWriterInit(&tWriter);
	iRet = __xuiRichMarkdownWriteDocument(&tWriter, pDocument);
	if ( iRet != XUI_OK ) { xrtBufferUnit(&tWriter.tBuffer); return iRet; }
	return __xuiRichTextWriterFinish(&tWriter, ppText, pSize);
}

static int __xuiRichMarkdownFindUnescaped(const char* sText, int iStart, int iSize, const char* sNeedle)
{
	int iNeedle = (int)strlen(sNeedle);
	int i;
	for ( i = iStart; i + iNeedle <= iSize; i++ ) {
		int iSlashes = 0;
		int j = i;
		while ( j > 0 && sText[--j] == '\\' ) iSlashes++;
		if ( (iSlashes & 1) == 0 && memcmp(sText + i, sNeedle, (size_t)iNeedle) == 0 ) return i;
	}
	return -1;
}

static char* __xuiRichMarkdownUnescape(const char* sText, int iSize, int* pOutSize)
{
	char* sOut = (char*)xrtMalloc((size_t)iSize + 1u);
	int i;
	int n = 0;
	if ( sOut == NULL ) return NULL;
	for ( i = 0; i < iSize; i++ ) {
		if ( sText[i] == '\\' && i + 1 < iSize && strchr("\\`*_{}[]<>()#+-.!|~", sText[i + 1]) != NULL ) i++;
		sOut[n++] = sText[i];
	}
	sOut[n] = 0; *pOutSize = n; return sOut;
}

static int __xuiRichMarkdownAppendPlain(xui_rich_document pDocument, xui_rich_node pBlock,
	const char* sText, int iSize, const xui_rich_text_style_t* pStyle, const char* sLink)
{
	char* sDecoded;
	int iDecodedSize;
	if ( iSize <= 0 ) return XUI_OK;
	sDecoded = __xuiRichMarkdownUnescape(sText, iSize, &iDecodedSize);
	if ( sDecoded == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( __xuiRichHtmlAppendTextN(pDocument, pBlock, sDecoded, iDecodedSize, pStyle, sLink) == NULL ) {
		xrtFree(sDecoded); return XUI_ERROR_OUT_OF_MEMORY;
	}
	xrtFree(sDecoded); return XUI_OK;
}

static int __xuiRichMarkdownParseInlineRange(xui_rich_document pDocument, xui_rich_node pBlock,
	const char* sText, int iSize, xui_rich_text_style_t tStyle, const char* sLink, int iDepth)
{
	int i = 0;
	int iPlain = 0;
	if ( iDepth > XUI_RICH_SERIAL_MAX_DEPTH ) return XUI_ERROR_UNSUPPORTED;
#define XUI_RICH_MD_FLUSH(end_) do { int _r = __xuiRichMarkdownAppendPlain(pDocument, pBlock, sText + iPlain, (end_) - iPlain, &tStyle, sLink); if (_r != XUI_OK) return _r; } while (0)
	while ( i < iSize ) {
		int iClose;
		int iRet;
		int iOpenSize = 0;
		int iCloseSize = 0;
		xui_rich_text_style_t tNested = tStyle;
		if ( sText[i] == '\\' && i + 1 < iSize ) { i += 2; continue; }
		if ( sText[i] == '`' ) { iOpenSize = iCloseSize = 1; tNested.iFlags |= XUI_RICH_STYLE_CODE; }
		else if ( i + 1 < iSize && memcmp(sText + i, "**", 2) == 0 ) { iOpenSize = iCloseSize = 2; tNested.iFlags |= XUI_RICH_STYLE_BOLD; }
		else if ( i + 1 < iSize && memcmp(sText + i, "__", 2) == 0 ) { iOpenSize = iCloseSize = 2; tNested.iFlags |= XUI_RICH_STYLE_BOLD; }
		else if ( i + 1 < iSize && memcmp(sText + i, "~~", 2) == 0 ) { iOpenSize = iCloseSize = 2; tNested.iFlags |= XUI_RICH_STYLE_STRIKEOUT; }
		else if ( sText[i] == '*' || sText[i] == '_' ) { iOpenSize = iCloseSize = 1; tNested.iFlags |= XUI_RICH_STYLE_ITALIC; }
		else if ( sText[i] == '!' && i + 1 < iSize && sText[i + 1] == '[' ) {
			int iLabelEnd = __xuiRichMarkdownFindUnescaped(sText, i + 2, iSize, "](");
			int iUrlEnd = iLabelEnd >= 0 ? __xuiRichMarkdownFindUnescaped(sText, iLabelEnd + 2, iSize, ")") : -1;
			if ( iLabelEnd >= 0 && iUrlEnd >= 0 ) {
				xui_rich_node pImage;
				char* sAlt;
				char* sUrl;
				int iAltSize, iUrlSize;
				XUI_RICH_MD_FLUSH(i);
				sAlt = __xuiRichMarkdownUnescape(sText + i + 2, iLabelEnd - i - 2, &iAltSize);
				sUrl = __xuiRichMarkdownUnescape(sText + iLabelEnd + 2, iUrlEnd - iLabelEnd - 2, &iUrlSize);
				if ( sAlt == NULL || sUrl == NULL || iUrlSize == 0 ) { xrtFree(sAlt); xrtFree(sUrl); return XUI_ERROR_OUT_OF_MEMORY; }
				pImage = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_INLINE_IMAGE, 0);
				if ( pImage == NULL ) { xrtFree(sAlt); xrtFree(sUrl); return XUI_ERROR_OUT_OF_MEMORY; }
				pImage->sAltText = sAlt; pImage->sResource = sUrl; __xuiRichNodeAppend(pBlock, pImage);
				i = iUrlEnd + 1; iPlain = i; continue;
			}
		} else if ( sText[i] == '[' ) {
			int iLabelEnd = __xuiRichMarkdownFindUnescaped(sText, i + 1, iSize, "](");
			int iUrlEnd = iLabelEnd >= 0 ? __xuiRichMarkdownFindUnescaped(sText, iLabelEnd + 2, iSize, ")") : -1;
			if ( iLabelEnd >= 0 && iUrlEnd >= 0 ) {
				char* sUrl;
				int iUrlSize;
				XUI_RICH_MD_FLUSH(i);
				sUrl = __xuiRichMarkdownUnescape(sText + iLabelEnd + 2, iUrlEnd - iLabelEnd - 2, &iUrlSize);
				if ( sUrl == NULL || iUrlSize == 0 ) { xrtFree(sUrl); return XUI_ERROR_OUT_OF_MEMORY; }
				iRet = __xuiRichMarkdownParseInlineRange(pDocument, pBlock, sText + i + 1,
					iLabelEnd - i - 1, tStyle, sUrl, iDepth + 1);
				xrtFree(sUrl); if ( iRet != XUI_OK ) return iRet;
				i = iUrlEnd + 1; iPlain = i; continue;
			}
		}
		if ( iOpenSize == 0 ) { i++; continue; }
		iClose = __xuiRichMarkdownFindUnescaped(sText, i + iOpenSize, iSize,
			iOpenSize == 2 ? (sText[i] == '~' ? "~~" : (sText[i] == '_' ? "__" : "**")) :
			(sText[i] == '`' ? "`" : (sText[i] == '_' ? "_" : "*")));
		if ( iClose < 0 || iClose == i + iOpenSize ) { i += iOpenSize; continue; }
		XUI_RICH_MD_FLUSH(i);
		iRet = __xuiRichMarkdownParseInlineRange(pDocument, pBlock, sText + i + iOpenSize,
			iClose - i - iOpenSize, tNested, sLink, iDepth + 1);
		if ( iRet != XUI_OK ) return iRet;
		i = iClose + iCloseSize; iPlain = i;
	}
	XUI_RICH_MD_FLUSH(iSize);
#undef XUI_RICH_MD_FLUSH
	return XUI_OK;
}

static int __xuiRichMarkdownLine(const char* sText, size_t iSize, size_t iPos, size_t* pStart, size_t* pEnd, size_t* pNext)
{
	size_t i;
	if ( iPos >= iSize ) return 0;
	*pStart = iPos;
	for ( i = iPos; i < iSize && sText[i] != '\n' && sText[i] != '\r'; i++ ) {}
	*pEnd = i;
	if ( i < iSize && sText[i] == '\r' ) i++;
	if ( i < iSize && sText[i] == '\n' ) i++;
	*pNext = i;
	return 1;
}

static int __xuiRichMarkdownTableSeparator(const char* sText, size_t iSize, int* pColumns)
{
	int i = 0;
	int iColumns = 0;
	int bDash = 0;
	while ( i < (int)iSize && isspace((unsigned char)sText[i]) ) i++;
	if ( i < (int)iSize && sText[i] == '|' ) i++;
	while ( i < (int)iSize ) {
		while ( i < (int)iSize && isspace((unsigned char)sText[i]) ) i++;
		if ( i < (int)iSize && sText[i] == ':' ) i++;
		bDash = 0;
		while ( i < (int)iSize && sText[i] == '-' ) { bDash = 1; i++; }
		if ( !bDash ) return 0;
		if ( i < (int)iSize && sText[i] == ':' ) i++;
		while ( i < (int)iSize && isspace((unsigned char)sText[i]) ) i++;
		iColumns++;
		if ( i >= (int)iSize ) break;
		if ( sText[i] != '|' ) return 0;
		i++;
		while ( i < (int)iSize && isspace((unsigned char)sText[i]) ) i++;
		if ( i >= (int)iSize ) break;
	}
	*pColumns = iColumns;
	return iColumns > 0;
}

static int __xuiRichMarkdownSplitRow(const char* sText, int iSize, int* pStarts, int* pEnds, int iCapacity)
{
	int i = 0;
	int iLimit = iSize;
	int iStart;
	int iCount = 0;
	while ( i < iSize && isspace((unsigned char)sText[i]) ) i++;
	if ( i < iSize && sText[i] == '|' ) i++;
	while ( iLimit > i && isspace((unsigned char)sText[iLimit - 1]) ) iLimit--;
	if ( iLimit > i && sText[iLimit - 1] == '|' && (iLimit < 2 || sText[iLimit - 2] != '\\') ) iLimit--;
	iStart = i;
	while ( i <= iLimit ) {
		int bSplit = i == iLimit || (sText[i] == '|' && (i == 0 || sText[i - 1] != '\\'));
		if ( bSplit ) {
			int a = iStart;
			int b = i;
			while ( a < b && isspace((unsigned char)sText[a]) ) a++;
			while ( b > a && isspace((unsigned char)sText[b - 1]) ) b--;
			if ( iCount < iCapacity ) { pStarts[iCount] = a; pEnds[iCount] = b; }
			iCount++; iStart = i + 1;
		}
		i++;
	}
	return iCount;
}

static int __xuiRichMarkdownAppendBlock(xui_rich_document pDocument, int iType,
	const xui_rich_paragraph_style_t* pParagraph, const char* sText, int iSize)
{
	xui_rich_node pBlock = __xuiRichNodeCreate(pDocument, iType, 0);
	xui_rich_text_style_t tStyle;
	if ( pBlock == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( pParagraph != NULL ) pBlock->tParagraphStyle = *pParagraph;
	__xuiRichNodeAppend(pDocument->pRoot, pBlock);
	memset(&tStyle, 0, sizeof(tStyle)); tStyle.iSize = sizeof(tStyle);
	return __xuiRichMarkdownParseInlineRange(pDocument, pBlock, sText, iSize, tStyle, NULL, 0);
}

static xui_rich_node __xuiRichMarkdownBlockImage(xui_rich_document pDocument, const char* sLine, int iSize)
{
	int iLabelEnd;
	int iUrlEnd;
	char* sAlt;
	char* sUrl;
	int iAltSize;
	int iUrlSize;
	xui_rich_node pImage;
	if ( iSize < 5 || sLine[0] != '!' || sLine[1] != '[' ) return NULL;
	iLabelEnd = __xuiRichMarkdownFindUnescaped(sLine, 2, iSize, "](");
	iUrlEnd = iLabelEnd >= 0 ? __xuiRichMarkdownFindUnescaped(sLine, iLabelEnd + 2, iSize, ")") : -1;
	if ( iLabelEnd < 0 || iUrlEnd != iSize - 1 ) return NULL;
	sAlt = __xuiRichMarkdownUnescape(sLine + 2, iLabelEnd - 2, &iAltSize);
	sUrl = __xuiRichMarkdownUnescape(sLine + iLabelEnd + 2, iUrlEnd - iLabelEnd - 2, &iUrlSize);
	if ( sAlt == NULL || sUrl == NULL || iUrlSize == 0 ) { xrtFree(sAlt); xrtFree(sUrl); return NULL; }
	pImage = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_IMAGE, 0);
	if ( pImage == NULL ) { xrtFree(sAlt); xrtFree(sUrl); return NULL; }
	pImage->sAltText = sAlt; pImage->sResource = sUrl;
	return pImage;
}

static int __xuiRichMarkdownParseTable(xui_rich_document pDocument, const char* sText, size_t iSize,
	size_t iHeaderStart, size_t iHeaderEnd, size_t iAfterSeparator, int iColumns, size_t* pNext)
{
	xui_rich_html_cells_t tCells;
	xui_rich_node pTable = NULL;
	size_t iPos = iHeaderStart;
	int iRow = 0;
	memset(&tCells, 0, sizeof(tCells));
	while ( 1 ) {
		size_t iStart;
		size_t iEnd;
		size_t iLineNext;
		int* pStarts;
		int* pEnds;
		int iCount;
		int c;
		if ( iRow == 1 ) iPos = iAfterSeparator;
		if ( iRow == 0 ) { iStart = iHeaderStart; iEnd = iHeaderEnd; iLineNext = iAfterSeparator; }
		else if ( !__xuiRichMarkdownLine(sText, iSize, iPos, &iStart, &iEnd, &iLineNext) ||
		          iStart == iEnd || memchr(sText + iStart, '|', iEnd - iStart) == NULL ) break;
		pStarts = (int*)xrtMalloc((size_t)iColumns * sizeof(*pStarts));
		pEnds = (int*)xrtMalloc((size_t)iColumns * sizeof(*pEnds));
		if ( pStarts == NULL || pEnds == NULL ) { xrtFree(pStarts); xrtFree(pEnds); goto oom; }
		iCount = __xuiRichMarkdownSplitRow(sText + iStart, (int)(iEnd - iStart), pStarts, pEnds, iColumns);
		if ( iCount != iColumns ) { xrtFree(pStarts); xrtFree(pEnds); if ( iRow == 0 ) goto failed; break; }
		for ( c = 0; c < iColumns; c++ ) {
			xui_rich_document pCell = __xuiRichHtmlCreateEmptyDocument(NULL, 0);
			xui_rich_paragraph_style_t tParagraph = __xuiRichParagraphStyle(NULL);
			if ( pCell == NULL || __xuiRichMarkdownAppendBlock(pCell, XUI_RICH_NODE_PARAGRAPH, &tParagraph,
				sText + iStart + pStarts[c], pEnds[c] - pStarts[c]) != XUI_OK ||
			     __xuiRichHtmlCellsAppend(&tCells, pCell) != XUI_OK ) {
				xuiRichDocumentDestroy(pCell); xrtFree(pStarts); xrtFree(pEnds); goto oom;
			}
		}
		xrtFree(pStarts); xrtFree(pEnds); iRow++; iPos = iLineNext;
	}
	if ( iRow <= 0 ) goto failed;
	pTable = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_TABLE, 0);
	if ( pTable == NULL ) goto oom;
	pTable->iRows = iRow; pTable->iColumns = iColumns; pTable->fCellPadding = 6.0f; pTable->fBorderWidth = 1.0f;
	pTable->ppCellDocuments = tCells.pItems;
	{
		int i;
		for ( i = 0; i < tCells.iCount; i++ ) { tCells.pItems[i]->pOwnerDocument = pDocument; tCells.pItems[i]->iOwnerTableId = pTable->iId; }
	}
	tCells.pItems = NULL; tCells.iCount = tCells.iCapacity = 0;
	__xuiRichNodeAppend(pDocument->pRoot, pTable); *pNext = iPos; return XUI_OK;

oom:
	__xuiRichNodeDestroy(pTable); __xuiRichHtmlCellsUnit(&tCells); return XUI_ERROR_OUT_OF_MEMORY;
failed:
	__xuiRichNodeDestroy(pTable); __xuiRichHtmlCellsUnit(&tCells); return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiRichDocumentImportMarkdown(const char* sText, size_t iSize, xui_rich_document* ppDocument)
{
	xui_rich_document pDocument;
	size_t iPos = 0;
	int iRet = XUI_OK;
	if ( sText == NULL || ppDocument == NULL || iSize > XUI_RICH_SERIAL_MAX_INPUT ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	if ( iSize == 0 ) iSize = strlen(sText);
	if ( iSize == 0 || iSize > XUI_RICH_SERIAL_MAX_INPUT || iSize > INT_MAX || !__xuiRichUtf8Valid(sText, (int)iSize) )
		return XUI_ERROR_INVALID_ARGUMENT;
	pDocument = __xuiRichHtmlCreateEmptyDocument(NULL, 0);
	if ( pDocument == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	while ( iPos < iSize ) {
		size_t iStart;
		size_t iEnd;
		size_t iNext;
		const char* pLine;
		int iLineSize;
		int iType = XUI_RICH_NODE_PARAGRAPH;
		int iContent = 0;
		xui_rich_paragraph_style_t tParagraph = __xuiRichParagraphStyle(NULL);
		xui_rich_node pImage;
		if ( !__xuiRichMarkdownLine(sText, iSize, iPos, &iStart, &iEnd, &iNext) ) break;
		iPos = iNext;
		while ( iStart < iEnd && (sText[iEnd - 1] == ' ' || sText[iEnd - 1] == '\t') ) iEnd--;
		if ( iStart == iEnd ) continue;
		pLine = sText + iStart; iLineSize = (int)(iEnd - iStart);
		if ( memchr(pLine, '|', (size_t)iLineSize) != NULL && iPos < iSize ) {
			size_t iSepStart, iSepEnd, iSepNext;
			int iColumns;
			int iStarts[256];
			int iEnds[256];
			int iHeaderColumns = __xuiRichMarkdownSplitRow(pLine, iLineSize, iStarts, iEnds, 256);
			if ( iHeaderColumns > 0 && iHeaderColumns <= 256 &&
			     __xuiRichMarkdownLine(sText, iSize, iPos, &iSepStart, &iSepEnd, &iSepNext) &&
			     __xuiRichMarkdownTableSeparator(sText + iSepStart, iSepEnd - iSepStart, &iColumns) && iColumns == iHeaderColumns ) {
				iRet = __xuiRichMarkdownParseTable(pDocument, sText, iSize, iStart, iEnd, iSepNext, iColumns, &iPos);
				if ( iRet != XUI_OK ) goto failed;
				continue;
			}
		}
		if ( iLineSize >= 3 && (memcmp(pLine, "---", 3) == 0 || memcmp(pLine, "***", 3) == 0 || memcmp(pLine, "___", 3) == 0) ) {
			int i;
			int bRule = 1;
			for ( i = 0; i < iLineSize; i++ ) if ( pLine[i] != pLine[0] && !isspace((unsigned char)pLine[i]) ) { bRule = 0; break; }
			if ( bRule ) {
				xui_rich_node pRule = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_HORIZONTAL_RULE, 0);
				if ( pRule == NULL ) { iRet = XUI_ERROR_OUT_OF_MEMORY; goto failed; }
				__xuiRichNodeAppend(pDocument->pRoot, pRule); continue;
			}
		}
		pImage = __xuiRichMarkdownBlockImage(pDocument, pLine, iLineSize);
		if ( pImage != NULL ) { __xuiRichNodeAppend(pDocument->pRoot, pImage); continue; }
		while ( iContent < iLineSize && pLine[iContent] == '#' ) iContent++;
		if ( iContent > 0 && iContent <= 6 && iContent < iLineSize && pLine[iContent] == ' ' ) {
			iType = XUI_RICH_NODE_HEADING; tParagraph.iHeadingLevel = iContent; iContent++;
		} else if ( iLineSize >= 2 && pLine[0] == '>' && pLine[1] == ' ' ) {
			iType = XUI_RICH_NODE_BLOCK_QUOTE; iContent = 2;
		} else {
			int iIndent = 0;
			int iMarker;
			while ( iIndent < iLineSize && (pLine[iIndent] == ' ' || pLine[iIndent] == '\t') ) iIndent++;
			iMarker = iIndent;
			if ( iMarker + 2 <= iLineSize && (pLine[iMarker] == '-' || pLine[iMarker] == '*' || pLine[iMarker] == '+') &&
			     pLine[iMarker + 1] == ' ' ) {
				iType = XUI_RICH_NODE_LIST_ITEM; tParagraph.iListType = XUI_RICH_LIST_BULLET;
				tParagraph.iListLevel = iIndent / 2; iContent = iMarker + 2;
				if ( iContent + 3 < iLineSize && pLine[iContent] == '[' && pLine[iContent + 2] == ']' && pLine[iContent + 3] == ' ' &&
				     (pLine[iContent + 1] == ' ' || pLine[iContent + 1] == 'x' || pLine[iContent + 1] == 'X') ) {
					tParagraph.iListType = XUI_RICH_LIST_CHECK; tParagraph.bListChecked = pLine[iContent + 1] != ' '; iContent += 4;
				}
			} else {
				int j = iMarker;
				while ( j < iLineSize && isdigit((unsigned char)pLine[j]) ) j++;
				if ( j > iMarker && j + 1 < iLineSize && pLine[j] == '.' && pLine[j + 1] == ' ' ) {
					iType = XUI_RICH_NODE_LIST_ITEM; tParagraph.iListType = XUI_RICH_LIST_NUMBER;
					tParagraph.iListLevel = iIndent / 2; iContent = j + 2;
				} else iContent = 0;
			}
		}
		iRet = __xuiRichMarkdownAppendBlock(pDocument, iType, &tParagraph, pLine + iContent, iLineSize - iContent);
		if ( iRet != XUI_OK ) goto failed;
	}
	if ( pDocument->pRoot->pFirstChild == NULL ) {
		xui_rich_node pParagraph = __xuiRichNodeCreate(pDocument, XUI_RICH_NODE_PARAGRAPH, 0);
		if ( pParagraph == NULL ) { iRet = XUI_ERROR_OUT_OF_MEMORY; goto failed; }
		__xuiRichNodeAppend(pDocument->pRoot, pParagraph);
	}
	*ppDocument = pDocument; return XUI_OK;

failed:
	xuiRichDocumentDestroy(pDocument); return iRet;
}

int xuiInternalRichDocumentAddObserver(xui_rich_document pDocument, xui_internal_rich_change_proc onChange, void* pUser)
{
	xui_rich_observer_t* pNew;
	int i;
	if ( !__xuiRichDocumentValid(pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( onChange == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pDocument->iObserverCount; i++ )
		if ( pDocument->pObservers[i].onChange == onChange && pDocument->pObservers[i].pUser == pUser ) return XUI_OK;
	if ( pDocument->iObserverCount == pDocument->iObserverCapacity ) {
		int iCapacity = pDocument->iObserverCapacity > 0 ? pDocument->iObserverCapacity * 2 : 4;
		pNew = (xui_rich_observer_t*)xrtRealloc(pDocument->pObservers, sizeof(*pNew) * (size_t)iCapacity);
		if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pDocument->pObservers = pNew;
		pDocument->iObserverCapacity = iCapacity;
	}
	pDocument->pObservers[pDocument->iObserverCount].onChange = onChange;
	pDocument->pObservers[pDocument->iObserverCount].pUser = pUser;
	pDocument->iObserverCount++;
	return XUI_OK;
}

int xuiInternalRichDocumentRemoveObserver(xui_rich_document pDocument, xui_internal_rich_change_proc onChange, void* pUser)
{
	int i;
	if ( !__xuiRichDocumentValid(pDocument) || onChange == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pDocument->iObserverCount; i++ ) {
		if ( pDocument->pObservers[i].onChange == onChange && pDocument->pObservers[i].pUser == pUser ) {
			memmove(pDocument->pObservers + i, pDocument->pObservers + i + 1,
				sizeof(*pDocument->pObservers) * (size_t)(pDocument->iObserverCount - i - 1));
			pDocument->iObserverCount--;
			if ( pDocument->iObserverCount == 0 && pDocument->bDestroyPending )
				xuiRichDocumentDestroy(pDocument);
			return XUI_OK;
		}
	}
	return XUI_ERROR_FILE_NOT_FOUND;
}
