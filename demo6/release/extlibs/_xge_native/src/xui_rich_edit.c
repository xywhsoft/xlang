#include "xui_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define XUI_RICH_EDIT_SCROLL_STEP 48.0f
#define XUI_RICH_EDIT_SCROLLBAR_SIZE 8.0f
#define XUI_RICH_ACCESSIBLE_ROOT_ID UINT64_C(0x8000000000000000)
#define XUI_RICH_ACCESSIBLE_CELL_FLAG UINT64_C(0xc000000000000000)

typedef struct xui_rich_atom_t {
	xui_rich_node pNode;
	int iNodeType;
	int iNodeStart;
	int iNodeEnd;
	int iDocumentStart;
	int iDocumentEnd;
	xui_font pFont;
	uint32_t iTextColor;
	uint32_t iBackgroundColor;
	uint32_t iStyleFlags;
	float fBaselineShift;
	float fWidth;
	float fHeight;
	float fBaseline;
	uint32_t iBreakFlags;
} xui_rich_atom_t;

typedef struct xui_rich_layout_fragment_t {
	xui_rich_fragment_t tPublic;
	xui_rich_node pNode;
	xui_font pFont;
	uint32_t iTextColor;
	uint32_t iBackgroundColor;
	uint32_t iStyleFlags;
} xui_rich_layout_fragment_t;

typedef struct xui_rich_block_layout_t {
	xui_rich_node pNode;
	xui_rect_t tRect;
	int iType;
	int iDocumentStart;
	int iDocumentEnd;
	int iFragmentStart;
	int iFragmentCount;
	xui_rich_layout_fragment_t* pFragments;
	int iLineStart;
	int iLineCount;
	int iFragmentDocumentBase;
	int iFragmentLineBase;
	float fFragmentYBase;
	float fContentWidth;
} xui_rich_block_layout_t;

typedef struct xui_rich_flow_t {
	xui_rich_atom_t* pAtoms;
	int iAtomCount;
} xui_rich_flow_t;

typedef struct xui_rich_font_cache_t {
	xui_font pSource;
	xui_font pFont;
	float fSize;
} xui_rich_font_cache_t;

typedef struct xui_rich_edit_data_t {
	xui_rich_document pDocument;
	int bOwnDocument;
	xlayout_context_t* pFlowLayout;
	xui_font pFont;
	xui_rich_font_set_t tFonts;
	xui_rich_font_cache_t* pFontCache;
	int iFontCacheCount;
	int iFontCacheCapacity;
	float fZoom;
	xui_rich_text_style_t tTypingStyle;
	int bTypingStyleOverride;
	xui_rich_edit_change_proc onChange;
	void* pChangeUser;
	xui_rich_edit_link_proc onLinkClick;
	void* pLinkClickUser;
	xui_rich_edit_event_proc onEvent;
	void* pEventUser;
	char* sFindPattern;
	int iFindPatternCapacity;
	char* sFindReplacement;
	int iFindReplacementCapacity;
	xui_find_result_t* pFindResults;
	int iFindResultCount;
	int iFindResultCapacity;
	int iFindActiveIndex;
	uint32_t iFindFlags;
	int iFindRangeStart;
	int iFindRangeEnd;
	int bUpdatingFind;
	xui_widget pFindWindow;
	xui_widget pFindInput;
	xui_widget pReplaceInput;
	xui_widget pFindStatus;
	xui_widget pFindPrevButton;
	xui_widget pFindNextButton;
	xui_widget pReplaceButton;
	xui_widget pReplaceAllButton;
	xui_widget pCaseCheck;
	xui_widget pWordCheck;
	xui_widget pRegexCheck;
	xui_widget pEscapeCheck;
	xui_widget pSelectionCheck;
	int bFindWindowReplace;
	uint32_t iFindLanguageRevision;
	xui_rich_node pPressedLink;
	xui_rich_node pPressedObject;
	xui_widget pMenu;
	xui_widget pCellEditor;
	xui_document_node_id_t iActiveTableId;
	int iActiveCellRow;
	int iActiveCellColumn;
	float fPressX;
	float fPressY;
	int bPointerMoved;
	xui_rich_layout_fragment_t* pFragments;
	int iFragmentCount;
	int iFragmentCapacity;
	int iTotalFragmentCount;
	xui_rich_atom_t* pAtoms;
	int iAtomCount;
	int iAtomCapacity;
	xui_rich_block_layout_t* pBlocks;
	int iBlockCount;
	int iBlockCapacity;
	char* sScratch;
	int iScratchCapacity;
	char* sImeText;
	int iImeCapacity;
	int bImeActive;
	int iImeStart;
	int iImeEnd;
	int iImeCursor;
	int iAnchor;
	int iCaret;
	int iNotifiedAnchor;
	int iNotifiedCaret;
	float fNotifiedScrollX;
	float fNotifiedScrollY;
	float fNotifiedZoom;
	int bDragging;
	int bReadonly;
	int bWordWrap;
	int bLayoutDirty;
	int bIncrementalLayout;
	int iDirtyBlock;
	int iDirtyBlockEnd;
	xui_rich_change_t tLayoutChange;
	uint32_t iLayoutVersion;
	float fLayoutWidth;
	float fContentWidth;
	float fContentHeight;
	float fScrollX;
	float fScrollY;
	xui_scroll_model_t tScrollModel;
	xui_widget pHScrollBar;
	xui_widget pVScrollBar;
	xui_rect_t tScrollViewportRect;
	xui_rect_t tHScrollBarRect;
	xui_rect_t tVScrollBarRect;
	int bShowHScrollBar;
	int bShowVScrollBar;
	int bSyncingScrollBars;
	float fPreferredCaretX;
	int bPreferredCaretX;
	float fBorderWidth;
	float fPadding;
	float fLineGap;
	float fParagraphGap;
	uint32_t iTextColor;
	uint32_t iBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iSelectionColor;
	uint32_t iCursorColor;
	uint32_t iFindResultColor;
	uint32_t iFindActiveColor;
	xui_rect_t tCursorRect;
} xui_rich_edit_data_t;

static int __xuiRichEditReserve(void** ppData, int* pCapacity, int iNeed, size_t iItemSize);
static void __xuiRichEditFindApplyLanguage(xui_widget pRichEdit, xui_rich_edit_data_t* pData);

static xui_font __xuiRichEditResolveBaseFont(xui_rich_edit_data_t* pData, const xui_rich_text_style_t* pStyle)
{
	uint32_t iFlags = pStyle->iFlags;
	if ( pStyle->pFont != NULL ) return pStyle->pFont;
	if ( (iFlags & (XUI_RICH_STYLE_BOLD | XUI_RICH_STYLE_ITALIC)) == (XUI_RICH_STYLE_BOLD | XUI_RICH_STYLE_ITALIC) && pData->tFonts.pBoldItalic != NULL ) return pData->tFonts.pBoldItalic;
	if ( (iFlags & XUI_RICH_STYLE_BOLD) != 0 && pData->tFonts.pBold != NULL ) return pData->tFonts.pBold;
	if ( (iFlags & XUI_RICH_STYLE_ITALIC) != 0 && pData->tFonts.pItalic != NULL ) return pData->tFonts.pItalic;
	return pData->tFonts.pNormal != NULL ? pData->tFonts.pNormal : pData->pFont;
}

static void __xuiRichEditClearFontCache(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_proxy pProxy;
	int i;
	if ( pData == NULL ) return;
	pProxy = pWidget != NULL ? xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget)) : NULL;
	if ( pProxy != NULL && pProxy->fontDestroy != NULL )
		for ( i = 0; i < pData->iFontCacheCount; i++ )
			if ( pData->pFontCache[i].pFont != NULL ) pProxy->fontDestroy(pProxy, pData->pFontCache[i].pFont);
	if ( pData->pFontCache != NULL ) xrtFree(pData->pFontCache);
	pData->pFontCache = NULL;
	pData->iFontCacheCount = 0;
	pData->iFontCacheCapacity = 0;
}

static xui_font __xuiRichEditSizedFont(xui_widget pWidget, xui_rich_edit_data_t* pData,
	xui_font pSource, float fStyleSize)
{
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;
	xui_font pFont = NULL;
	float fSize;
	int i;
	int iRet;
	if ( pSource == NULL ) return NULL;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( fStyleSize > 0.0f ) fSize = fStyleSize * pData->fZoom;
	else {
		if ( pData->fZoom == 1.0f ) return pSource;
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy == NULL || pProxy->fontGetMetrics == NULL ||
		     pProxy->fontGetMetrics(pProxy, pSource, &tMetrics) != XUI_OK || tMetrics.fSize <= 0.0f ) return NULL;
		fSize = tMetrics.fSize * pData->fZoom;
	}
	if ( fSize <= 0.0f ) return pSource;
	memset(&tMetrics, 0, sizeof(tMetrics));
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL &&
	     pProxy->fontGetMetrics(pProxy, pSource, &tMetrics) == XUI_OK && fabsf(tMetrics.fSize - fSize) < 0.01f )
		return pSource;
	for ( i = 0; i < pData->iFontCacheCount; i++ )
		if ( pData->pFontCache[i].pSource == pSource && fabsf(pData->pFontCache[i].fSize - fSize) < 0.01f )
			return pData->pFontCache[i].pFont;
	if ( pProxy == NULL || pProxy->fontCreateSized == NULL ) return NULL;
	iRet = pProxy->fontCreateSized(pProxy, &pFont, pSource, fSize);
	if ( iRet != XUI_OK || pFont == NULL ) return NULL;
	iRet = __xuiRichEditReserve((void**)&pData->pFontCache, &pData->iFontCacheCapacity,
		pData->iFontCacheCount + 1, sizeof(*pData->pFontCache));
	if ( iRet != XUI_OK ) { if ( pProxy->fontDestroy != NULL ) pProxy->fontDestroy(pProxy, pFont); return NULL; }
	pData->pFontCache[pData->iFontCacheCount].pSource = pSource;
	pData->pFontCache[pData->iFontCacheCount].pFont = pFont;
	pData->pFontCache[pData->iFontCacheCount].fSize = fSize;
	pData->iFontCacheCount++;
	return pFont;
}

static xui_font __xuiRichEditResolveFont(xui_widget pWidget, xui_rich_edit_data_t* pData,
	const xui_rich_text_style_t* pStyle)
{
	return __xuiRichEditSizedFont(pWidget, pData, __xuiRichEditResolveBaseFont(pData, pStyle), pStyle->fFontSize);
}

static xui_rich_edit_data_t* __xuiRichEditData(xui_widget pWidget)
{
	return pWidget != NULL ? (xui_rich_edit_data_t*)xuiWidgetGetTypeData(pWidget) : NULL;
}

static int __xuiRichEditEndCellEdit(xui_widget pWidget, xui_rich_edit_data_t* pData, int bFocusParent)
{
	xui_widget pEditor;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pEditor = pData->pCellEditor;
	pData->pCellEditor = NULL;
	pData->iActiveTableId = 0;
	pData->iActiveCellRow = -1;
	pData->iActiveCellColumn = -1;
	if ( pEditor != NULL ) xuiWidgetDestroy(pEditor);
	if ( bFocusParent ) (void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiRichEditMenuTextId(int iCommand)
{
	switch ( iCommand ) {
	case XUI_RICH_COMMAND_UNDO: return XUI_TR_EDIT_UNDO;
	case XUI_RICH_COMMAND_REDO: return XUI_TR_EDIT_REDO;
	case XUI_RICH_COMMAND_CUT: return XUI_TR_EDIT_CUT;
	case XUI_RICH_COMMAND_COPY: return XUI_TR_EDIT_COPY;
	case XUI_RICH_COMMAND_PASTE: return XUI_TR_EDIT_PASTE;
	case XUI_RICH_COMMAND_DELETE: return XUI_TR_EDIT_DELETE;
	case XUI_RICH_COMMAND_SELECT_ALL: return XUI_TR_EDIT_SELECT_ALL;
	case XUI_RICH_COMMAND_FIND: return XUI_TR_FIND_TITLE;
	case XUI_RICH_COMMAND_REPLACE: return XUI_TR_REPLACE_TITLE;
	default: return XUI_TR_NONE;
	}
}

static int __xuiRichEditReserve(void** ppData, int* pCapacity, int iNeed, size_t iItemSize)
{
	void* pNew;
	int iCapacity = *pCapacity;
	if ( iNeed <= iCapacity ) return XUI_OK;
	if ( iCapacity < 32 ) iCapacity = 32;
	while ( iCapacity < iNeed ) iCapacity *= 2;
	pNew = xrtRealloc(*ppData, iItemSize * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	*ppData = pNew;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiRichEditScratch(xui_rich_edit_data_t* pData, int iNeed)
{
	return __xuiRichEditReserve((void**)&pData->sScratch, &pData->iScratchCapacity, iNeed, 1u);
}

static int __xuiRichEditStringSet(char** psText, int* pCapacity, const char* sText)
{
	int iLength;
	int iRet;
	if ( psText == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL ) sText = "";
	iLength = (int)strlen(sText);
	iRet = __xuiRichEditReserve((void**)psText, pCapacity, iLength + 1, 1u);
	if ( iRet != XUI_OK ) return iRet;
	memcpy(*psText, sText, (size_t)iLength + 1u);
	return XUI_OK;
}

static void __xuiRichEditClearFindData(xui_rich_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iFindResultCount = 0;
	pData->iFindActiveIndex = -1;
}

static int __xuiRichEditFindResultReserve(xui_rich_edit_data_t* pData, int iCapacity)
{
	return __xuiRichEditReserve((void**)&pData->pFindResults, &pData->iFindResultCapacity,
		iCapacity, sizeof(*pData->pFindResults));
}

static xui_rect_t __xuiRichEditRawContentRect(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_rect_t tRect = xuiWidgetGetContentRect(pWidget);
	float fInset = pData->fBorderWidth + pData->fPadding;
	tRect.fX += fInset;
	tRect.fY += fInset;
	tRect.fW -= fInset * 2.0f;
	tRect.fH -= fInset * 2.0f;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return tRect;
}

static xui_rect_t __xuiRichEditViewportFromRaw(xui_rect_t tRaw, int bShowH, int bShowV)
{
	if ( bShowV ) {
		tRaw.fW -= XUI_RICH_EDIT_SCROLLBAR_SIZE;
		if ( tRaw.fW < 0.0f ) tRaw.fW = 0.0f;
	}
	if ( bShowH ) {
		tRaw.fH -= XUI_RICH_EDIT_SCROLLBAR_SIZE;
		if ( tRaw.fH < 0.0f ) tRaw.fH = 0.0f;
	}
	return tRaw;
}

static xui_rect_t __xuiRichEditContentRect(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	return __xuiRichEditViewportFromRaw(__xuiRichEditRawContentRect(pWidget, pData),
		pData->bShowHScrollBar, pData->bShowVScrollBar);
}

static int __xuiRichEditAtom(xui_rich_edit_data_t* pData, xui_rich_node pNode,
	const xui_rich_node_info_t* pInfo, xui_font pFont, int iNodeStart, int iNodeEnd,
	int iDocumentStart, int iDocumentEnd, float fWidth, float fHeight, float fAscent, uint32_t iBreakFlags)
{
	xui_rich_atom_t* pAtom;
	int iRet;
	iRet = __xuiRichEditReserve((void**)&pData->pAtoms, &pData->iAtomCapacity, pData->iAtomCount + 1, sizeof(*pData->pAtoms));
	if ( iRet != XUI_OK ) return iRet;
	pAtom = &pData->pAtoms[pData->iAtomCount++];
	memset(pAtom, 0, sizeof(*pAtom));
	pAtom->pNode = pNode;
	pAtom->iNodeType = pInfo->iType;
	pAtom->iNodeStart = iNodeStart;
	pAtom->iNodeEnd = iNodeEnd;
	pAtom->iDocumentStart = iDocumentStart;
	pAtom->iDocumentEnd = iDocumentEnd;
	if ( pInfo->iType == XUI_RICH_NODE_INLINE_WIDGET || pInfo->iType == XUI_RICH_NODE_INLINE_IMAGE ) {
		pAtom->fWidth = pInfo->fWidth;
		pAtom->fHeight = pInfo->fHeight;
		pAtom->fBaseline = pInfo->fBaseline;
		return XUI_OK;
	}
	pAtom->pFont = pFont;
	pAtom->iTextColor = (pInfo->tStyle.iTextColor & 0xffu) != 0 ? pInfo->tStyle.iTextColor : pData->iTextColor;
	pAtom->iBackgroundColor = pInfo->tStyle.iBackgroundColor;
	pAtom->iStyleFlags = pInfo->tStyle.iFlags;
	pAtom->fBaselineShift = pInfo->tStyle.fBaselineShift * pData->fZoom;
	pAtom->fWidth = fWidth;
	pAtom->fHeight = fHeight;
	pAtom->fBaseline = fAscent + pAtom->fBaselineShift;
	pAtom->iBreakFlags = iBreakFlags;
	return XUI_OK;
}

static int __xuiRichEditBuildAtoms(xui_widget pWidget, xui_rich_edit_data_t* pData, xui_rich_node pParagraph, int* pDocumentAt)
{
	xui_rich_node pNode;
	xui_rich_node_info_t tInfo;
	xui_rich_node_info_t tBlockInfo;
	xui_text_shape_t tShape;
	xui_font pFont;
	int i;
	int iRet;
	pData->iAtomCount = 0;
	memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
	(void)xuiRichNodeGetInfo(pParagraph, &tBlockInfo);
	for ( pNode = xuiRichNodeGetFirstChild(pParagraph); pNode != NULL; pNode = xuiRichNodeGetNextSibling(pNode) ) {
		memset(&tInfo, 0, sizeof(tInfo));
		tInfo.iSize = sizeof(tInfo);
		iRet = xuiRichNodeGetInfo(pNode, &tInfo);
		if ( iRet != XUI_OK ) return iRet;
		if ( tInfo.iType == XUI_RICH_NODE_TEXT || tInfo.iType == XUI_RICH_NODE_LINK ) {
			if ( tInfo.tStyle.pFont == NULL && tBlockInfo.iType == XUI_RICH_NODE_HEADING ) {
				int iLevel = tBlockInfo.tParagraphStyle.iHeadingLevel;
				if ( iLevel <= 1 ) tInfo.tStyle.pFont = pData->tFonts.pHeading1;
				else if ( iLevel == 2 ) tInfo.tStyle.pFont = pData->tFonts.pHeading2;
				else tInfo.tStyle.pFont = pData->tFonts.pHeading3;
				if ( tInfo.tStyle.pFont == NULL ) tInfo.tStyle.iFlags |= XUI_RICH_STYLE_BOLD;
			}
			pFont = __xuiRichEditResolveFont(pWidget, pData, &tInfo.tStyle);
			if ( pFont == NULL ) return XUI_ERROR_UNSUPPORTED;
			memset(&tShape, 0, sizeof(tShape));
			iRet = xuiTextShape(xuiWidgetGetContext(pWidget), pFont, tInfo.sText, tInfo.iTextSize,
				XUI_TEXT_SHAPE_DEFAULT, &tShape);
			if ( iRet != XUI_OK ) return iRet;
			for ( i = 0; i < tShape.iClusterCount; i++ ) {
				xui_text_cluster_t* pCluster = &tShape.pClusters[i];
				uint32_t iBreak = (pCluster->iFlags & XUI_TEXT_CLUSTER_LINE_BREAK) != 0
					? XLAYOUT_FRAGMENT_BREAK_AFTER : XLAYOUT_FRAGMENT_NONE;
				iRet = __xuiRichEditAtom(pData, pNode, &tInfo, pFont,
					pCluster->iTextStart, pCluster->iTextEnd,
					*pDocumentAt + pCluster->iTextStart, *pDocumentAt + pCluster->iTextEnd,
					pCluster->fAdvance, tShape.fLineHeight, tShape.fAscent, iBreak);
				if ( iRet != XUI_OK ) { xuiTextShapeFree(&tShape); return iRet; }
			}
			xuiTextShapeFree(&tShape);
			*pDocumentAt += tInfo.iTextSize;
		} else if ( tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET || tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE ) {
			float fWidth = tInfo.fWidth * pData->fZoom;
			float fHeight = tInfo.fHeight * pData->fZoom;
			float fBaseline = tInfo.fBaseline * pData->fZoom;
			if ( tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE && tInfo.pSurface != NULL &&
			     (fWidth <= 0.0f || fHeight <= 0.0f) ) {
				xui_proxy pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
				xui_surface_desc_t tSurfaceDesc;
				memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
				if ( pProxy != NULL && pProxy->surfaceGetDesc != NULL &&
				     pProxy->surfaceGetDesc(pProxy, tInfo.pSurface, &tSurfaceDesc) == XUI_OK ) {
					if ( fWidth <= 0.0f ) fWidth = (float)tSurfaceDesc.iWidth * pData->fZoom;
					if ( fHeight <= 0.0f ) fHeight = (float)tSurfaceDesc.iHeight * pData->fZoom;
				}
			}
			if ( fWidth <= 0.0f ) fWidth = 24.0f * pData->fZoom;
			if ( fHeight <= 0.0f ) fHeight = 24.0f * pData->fZoom;
			if ( fBaseline <= 0.0f ) fBaseline = fHeight;
			tInfo.fWidth = fWidth; tInfo.fHeight = fHeight; tInfo.fBaseline = fBaseline;
			iRet = __xuiRichEditAtom(pData, pNode, &tInfo, NULL, 0, 3,
				*pDocumentAt, *pDocumentAt + 3, 0.0f, 0.0f, 0.0f, XLAYOUT_FRAGMENT_NONE);
			if ( iRet != XUI_OK ) return iRet;
			*pDocumentAt += 3;
		}
	}
	return XUI_OK;
}

static uint32_t __xuiRichFlowCount(xlayout_context_t* pContext, xlayout_node_t iNode, float fMax, void* pUser)
{
	(void)pContext; (void)iNode; (void)fMax;
	return (uint32_t)((xui_rich_flow_t*)pUser)->iAtomCount;
}

static bool __xuiRichFlowMeasure(xlayout_context_t* pContext, xlayout_node_t iNode, uint32_t iIndex,
	float fAvailable, void* pUser, xlayout_fragment_spec_t* pOutput)
{
	xui_rich_flow_t* pFlow = (xui_rich_flow_t*)pUser;
	xui_rich_atom_t* pAtom;
	(void)pContext; (void)iNode; (void)fAvailable;
	if ( iIndex >= (uint32_t)pFlow->iAtomCount ) return false;
	pAtom = &pFlow->pAtoms[iIndex];
	memset(pOutput, 0, sizeof(*pOutput));
	pOutput->width = pAtom->fWidth;
	pOutput->height = pAtom->fHeight;
	pOutput->baseline = pAtom->fBaseline;
	pOutput->flags = pAtom->iBreakFlags;
	pOutput->tag = (uintptr_t)iIndex;
	return true;
}

static int __xuiRichEditAddFragment(xui_rich_edit_data_t* pData, const xui_rich_atom_t* pAtom,
	xlayout_fragment_t tLayout, float fOffsetY, int iLine)
{
	xui_rich_layout_fragment_t* pFragment;
	int iRet = __xuiRichEditReserve((void**)&pData->pFragments, &pData->iFragmentCapacity,
		pData->iFragmentCount + 1, sizeof(*pData->pFragments));
	if ( iRet != XUI_OK ) return iRet;
	pFragment = &pData->pFragments[pData->iFragmentCount++];
	memset(pFragment, 0, sizeof(*pFragment));
	pFragment->tPublic.iSize = sizeof(pFragment->tPublic);
	pFragment->tPublic.iNodeId = 0;
	{
		xui_rich_node_info_t tInfo;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pAtom->pNode, &tInfo) == XUI_OK ) pFragment->tPublic.iNodeId = tInfo.iId;
	}
	pFragment->tPublic.iStartOffset = pAtom->iNodeStart;
	pFragment->tPublic.iEndOffset = pAtom->iNodeEnd;
	pFragment->tPublic.iDocumentStart = pAtom->iDocumentStart;
	pFragment->tPublic.iDocumentEnd = pAtom->iDocumentEnd;
	pFragment->tPublic.tRect = (xui_rect_t){tLayout.rect.x, tLayout.rect.y + fOffsetY, tLayout.rect.width, tLayout.rect.height};
	pFragment->tPublic.fBaseline = tLayout.baseline;
	pFragment->tPublic.iLine = iLine;
	pFragment->tPublic.iNodeType = pAtom->iNodeType;
	pFragment->pNode = pAtom->pNode;
	pFragment->pFont = pAtom->pFont;
	pFragment->iTextColor = pAtom->iTextColor;
	pFragment->iBackgroundColor = pAtom->iBackgroundColor;
	pFragment->iStyleFlags = pAtom->iStyleFlags;
	return XUI_OK;
}

static int __xuiRichEditAddBlock(xui_rich_edit_data_t* pData, xui_rich_node pNode, int iType,
	int iDocumentStart, int iDocumentEnd, xui_rect_t tRect,
	int iFragmentStart, int iLineStart, int iLineCount)
{
	xui_rich_block_layout_t* pBlock;
	int iCount = pData->iFragmentCount - iFragmentStart;
	int iRet = __xuiRichEditReserve((void**)&pData->pBlocks, &pData->iBlockCapacity,
		pData->iBlockCount + 1, sizeof(*pData->pBlocks));
	if ( iRet != XUI_OK ) return iRet;
	pBlock = &pData->pBlocks[pData->iBlockCount++];
	memset(pBlock, 0, sizeof(*pBlock));
	pBlock->pNode = pNode;
	pBlock->iType = iType;
	pBlock->iDocumentStart = iDocumentStart;
	pBlock->iDocumentEnd = iDocumentEnd;
	pBlock->tRect = tRect;
	pBlock->iFragmentStart = pData->iTotalFragmentCount;
	pBlock->iFragmentCount = iCount;
	if ( iCount > 0 ) {
		pBlock->pFragments = (xui_rich_layout_fragment_t*)xrtMalloc(sizeof(*pBlock->pFragments) * (size_t)iCount);
		if ( pBlock->pFragments == NULL ) { pData->iBlockCount--; return XUI_ERROR_OUT_OF_MEMORY; }
		memcpy(pBlock->pFragments, pData->pFragments + iFragmentStart,
			sizeof(*pBlock->pFragments) * (size_t)iCount);
	}
	pBlock->iLineStart = iLineStart;
	pBlock->iLineCount = iLineCount;
	pBlock->iFragmentDocumentBase = iDocumentStart;
	pBlock->iFragmentLineBase = iLineStart;
	pBlock->fFragmentYBase = tRect.fY;
	pBlock->fContentWidth = tRect.fX + tRect.fW;
	{
		int i;
		for ( i = 0; i < iCount; i++ ) {
			float fRight = pBlock->pFragments[i].tPublic.tRect.fX + pBlock->pFragments[i].tPublic.tRect.fW;
			if ( fRight > pBlock->fContentWidth ) pBlock->fContentWidth = fRight;
		}
	}
	pData->iTotalFragmentCount += iCount;
	return XUI_OK;
}

static void __xuiRichEditReleaseBlocks(xui_rich_edit_data_t* pData)
{
	int i;
	for ( i = 0; i < pData->iBlockCount; i++ ) {
		if ( pData->pBlocks[i].pFragments != NULL ) xrtFree(pData->pBlocks[i].pFragments);
		pData->pBlocks[i].pFragments = NULL;
	}
	pData->iBlockCount = 0;
	pData->iTotalFragmentCount = 0;
}

static int __xuiRichEditGetLayoutFragment(xui_rich_edit_data_t* pData, int iIndex,
	xui_rich_layout_fragment_t* pFragment)
{
	int iLow = 0;
	int iHigh = pData->iBlockCount - 1;
	while ( iLow <= iHigh ) {
		int iMiddle = iLow + (iHigh - iLow) / 2;
		xui_rich_block_layout_t* pBlock = &pData->pBlocks[iMiddle];
		if ( iIndex < pBlock->iFragmentStart ) iHigh = iMiddle - 1;
		else if ( iIndex >= pBlock->iFragmentStart + pBlock->iFragmentCount ) iLow = iMiddle + 1;
		else {
			*pFragment = pBlock->pFragments[iIndex - pBlock->iFragmentStart];
			pFragment->tPublic.iDocumentStart += pBlock->iDocumentStart - pBlock->iFragmentDocumentBase;
			pFragment->tPublic.iDocumentEnd += pBlock->iDocumentStart - pBlock->iFragmentDocumentBase;
			pFragment->tPublic.iLine += pBlock->iLineStart - pBlock->iFragmentLineBase;
			pFragment->tPublic.tRect.fY += pBlock->tRect.fY - pBlock->fFragmentYBase;
			return XUI_OK;
		}
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

static void __xuiRichEditGetBlockFragment(const xui_rich_block_layout_t* pBlock, int iIndex,
	xui_rich_layout_fragment_t* pFragment)
{
	*pFragment = pBlock->pFragments[iIndex];
	pFragment->tPublic.iDocumentStart += pBlock->iDocumentStart - pBlock->iFragmentDocumentBase;
	pFragment->tPublic.iDocumentEnd += pBlock->iDocumentStart - pBlock->iFragmentDocumentBase;
	pFragment->tPublic.iLine += pBlock->iLineStart - pBlock->iFragmentLineBase;
	pFragment->tPublic.tRect.fY += pBlock->tRect.fY - pBlock->fFragmentYBase;
}

static int __xuiRichEditFindBlockByOffset(xui_rich_edit_data_t* pData, int iOffset)
{
	int iLow = 0;
	int iHigh = pData->iBlockCount - 1;
	while ( iLow <= iHigh ) {
		int iMiddle = iLow + (iHigh - iLow) / 2;
		xui_rich_block_layout_t* pBlock = &pData->pBlocks[iMiddle];
		if ( iOffset < pBlock->iDocumentStart ) iHigh = iMiddle - 1;
		else if ( iOffset > pBlock->iDocumentEnd ) iLow = iMiddle + 1;
		else return iMiddle;
	}
	if ( iLow >= pData->iBlockCount ) return pData->iBlockCount - 1;
	return iLow < 0 ? 0 : iLow;
}

static int __xuiRichEditFragmentWhitespace(const xui_rich_layout_fragment_t* pFragment)
{
	xui_rich_node_info_t tInfo;
	int i;
	if ( pFragment->tPublic.iNodeType != XUI_RICH_NODE_TEXT && pFragment->tPublic.iNodeType != XUI_RICH_NODE_LINK ) return 0;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pFragment->pNode, &tInfo) != XUI_OK || tInfo.sText == NULL ) return 0;
	for ( i = pFragment->tPublic.iStartOffset; i < pFragment->tPublic.iEndOffset; i++ )
		if ( !isspace((unsigned char)tInfo.sText[i]) ) return 0;
	return 1;
}

static int __xuiRichEditAddAtomicFragment(xui_rich_edit_data_t* pData, xui_rich_node pNode,
	const xui_rich_node_info_t* pInfo, int iDocumentStart, xui_rect_t tRect, int iLine)
{
	xui_rich_layout_fragment_t* pFragment;
	int iRet = __xuiRichEditReserve((void**)&pData->pFragments, &pData->iFragmentCapacity,
		pData->iFragmentCount + 1, sizeof(*pData->pFragments));
	if ( iRet != XUI_OK ) return iRet;
	pFragment = &pData->pFragments[pData->iFragmentCount++];
	memset(pFragment, 0, sizeof(*pFragment));
	pFragment->tPublic.iSize = sizeof(pFragment->tPublic);
	pFragment->tPublic.iNodeId = pInfo->iId;
	pFragment->tPublic.iStartOffset = 0;
	pFragment->tPublic.iEndOffset = 3;
	pFragment->tPublic.iDocumentStart = iDocumentStart;
	pFragment->tPublic.iDocumentEnd = iDocumentStart + 3;
	pFragment->tPublic.tRect = tRect;
	pFragment->tPublic.fBaseline = tRect.fH;
	pFragment->tPublic.iLine = iLine;
	pFragment->tPublic.iNodeType = pInfo->iType;
	pFragment->pNode = pNode;
	return XUI_OK;
}

static int __xuiRichEditAddEmptyFragment(xui_rich_edit_data_t* pData, xui_rich_node pNode,
	const xui_rich_node_info_t* pInfo, int iDocumentStart, xui_rect_t tRect, int iLine)
{
	xui_rich_layout_fragment_t* pFragment;
	int iRet = __xuiRichEditReserve((void**)&pData->pFragments, &pData->iFragmentCapacity,
		pData->iFragmentCount + 1, sizeof(*pData->pFragments));
	if ( iRet != XUI_OK ) return iRet;
	pFragment = &pData->pFragments[pData->iFragmentCount++];
	memset(pFragment, 0, sizeof(*pFragment));
	pFragment->tPublic.iSize = sizeof(pFragment->tPublic);
	pFragment->tPublic.iNodeId = pInfo->iId;
	pFragment->tPublic.iDocumentStart = iDocumentStart;
	pFragment->tPublic.iDocumentEnd = iDocumentStart;
	pFragment->tPublic.tRect = tRect;
	pFragment->tPublic.fBaseline = tRect.fH;
	pFragment->tPublic.iLine = iLine;
	pFragment->tPublic.iNodeType = pInfo->iType;
	pFragment->pNode = pNode;
	return XUI_OK;
}

static void __xuiRichEditAlignParagraph(xui_rich_edit_data_t* pData, int iFragmentStart,
	float fWidth, int iAlign, float fIndentLeft, float fIndentRight)
{
	int i = iFragmentStart;
	float fAvailable = fWidth - fIndentLeft - fIndentRight;
	while ( i < pData->iFragmentCount ) {
		int iLine = pData->pFragments[i].tPublic.iLine;
		int iBegin = i;
		float fRight = 0.0f;
		float fShift = fIndentLeft;
		while ( i < pData->iFragmentCount && pData->pFragments[i].tPublic.iLine == iLine ) {
			float fItemRight = pData->pFragments[i].tPublic.tRect.fX + pData->pFragments[i].tPublic.tRect.fW;
			if ( fItemRight > fRight ) fRight = fItemRight;
			i++;
		}
		if ( iAlign == XUI_RICH_ALIGN_CENTER ) fShift += (fAvailable - fRight) * 0.5f;
		else if ( iAlign == XUI_RICH_ALIGN_RIGHT ) fShift += fAvailable - fRight;
		else if ( iAlign == XUI_RICH_ALIGN_JUSTIFY && i < pData->iFragmentCount && i - iBegin > 1 ) {
			int iGaps = 0;
			float fExtra;
			float fAccum = 0.0f;
			int j;
			for ( j = iBegin; j < i - 1; j++ ) if ( __xuiRichEditFragmentWhitespace(&pData->pFragments[j]) ) iGaps++;
			if ( iGaps == 0 ) iGaps = i - iBegin - 1;
			fExtra = (fAvailable - fRight) / (float)iGaps;
			if ( fExtra < 0.0f ) fExtra = 0.0f;
			for ( j = iBegin; j < i; j++ ) {
				pData->pFragments[j].tPublic.tRect.fX += fIndentLeft + fAccum;
				if ( j < i - 1 && (__xuiRichEditFragmentWhitespace(&pData->pFragments[j]) || iGaps == i - iBegin - 1) ) fAccum += fExtra;
			}
			continue;
		}
		if ( fShift < 0.0f ) fShift = 0.0f;
		while ( iBegin < i ) pData->pFragments[iBegin++].tPublic.tRect.fX += fShift;
	}
}

static int __xuiRichEditLayoutParagraph(xui_widget pWidget, xui_rich_edit_data_t* pData,
	xui_rich_node pParagraph, int* pDocumentAt, float fWidth, float* pY, int* pLineBase)
{
	xlayout_context_t* pLayout;
	xlayout_node_t iRoot;
	xlayout_node_t iSource;
	xlayout_style_t tStyle;
	xlayout_constraints_t tConstraints;
	xlayout_measure_t tMeasure;
	xlayout_fragment_t tFragment;
	xui_rich_flow_t tFlow;
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;
	float fEmptyHeight = 16.0f;
	float fLastY = -1.0f;
	int iFragmentStart = pData->iFragmentCount;
	int iDocumentStart = *pDocumentAt;
	float fBlockY = *pY;
	int iLine = *pLineBase;
	int iLineStart = *pLineBase;
	int iRet;
	uint32_t i;
	iRet = __xuiRichEditBuildAtoms(pWidget, pData, pParagraph, pDocumentAt);
	if ( iRet != XUI_OK ) return iRet;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pData->pFont != NULL ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pData->pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f ) fEmptyHeight = tMetrics.fLineHeight;
	}
	if ( pData->iAtomCount == 0 ) {
		xui_rich_node_info_t tBlockInfo;
		float fLineY;
		float fLineHeight;
		memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
		(void)xuiRichNodeGetInfo(pParagraph, &tBlockInfo);
		*pY += tBlockInfo.tParagraphStyle.fSpaceBefore * pData->fZoom;
		fLineY = *pY;
		fLineHeight = tBlockInfo.tParagraphStyle.fLineHeight * pData->fZoom > fEmptyHeight ?
			tBlockInfo.tParagraphStyle.fLineHeight * pData->fZoom : fEmptyHeight;
		iRet = __xuiRichEditAddEmptyFragment(pData, pParagraph, &tBlockInfo, iDocumentStart,
			(xui_rect_t){tBlockInfo.tParagraphStyle.fIndentLeft * pData->fZoom, fLineY, 0.0f, fLineHeight}, *pLineBase);
		if ( iRet != XUI_OK ) return iRet;
		*pY += fLineHeight;
		*pY += tBlockInfo.tParagraphStyle.fSpaceAfter * pData->fZoom;
		(void)__xuiRichEditAddBlock(pData, pParagraph, tBlockInfo.iType, iDocumentStart,
			*pDocumentAt, (xui_rect_t){0.0f, fBlockY, fWidth, *pY - fBlockY},
			iFragmentStart, iLineStart, 1);
		(*pLineBase)++;
		return XUI_OK;
	}
	pLayout = pData->pFlowLayout;
	if ( pLayout == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	iRoot = xLayoutNodeCreate(pLayout, XLAYOUT_ROLE_CONTAINER);
	iSource = xLayoutNodeCreate(pLayout, XLAYOUT_ROLE_LEAF);
	if ( iRoot == XLAYOUT_NODE_INVALID || iSource == XLAYOUT_NODE_INVALID ) return XUI_ERROR_OUT_OF_MEMORY;
	tStyle = xLayoutStyleDefault();
	tStyle.container.format = XLAYOUT_FORMAT_FLOW;
	tStyle.container.axis = XLAYOUT_HORIZONTAL;
	tStyle.container.align_items = XLAYOUT_ALIGN_BASELINE;
	tStyle.container.row_gap = pData->fLineGap * pData->fZoom;
	(void)xLayoutNodeSetStyle(pLayout, iRoot, &tStyle);
	tFlow.pAtoms = pData->pAtoms;
	tFlow.iAtomCount = pData->iAtomCount;
	(void)xLayoutNodeSetUserData(pLayout, iSource, &tFlow);
	(void)xLayoutNodeSetFragmentSource(pLayout, iSource, __xuiRichFlowCount, __xuiRichFlowMeasure);
	(void)xLayoutNodeAppend(pLayout, iRoot, iSource);
	tConstraints = xLayoutConstraints(pData->bWordWrap ? fWidth : XLAYOUT_UNBOUNDED, XLAYOUT_UNBOUNDED);
	if ( !xLayoutMeasure(pLayout, iRoot, &tConstraints, &tMeasure) ||
	     !xLayoutArrange(pLayout, iRoot, (xlayout_rect_t){0.0f, 0.0f, pData->bWordWrap ? fWidth : tMeasure.width, tMeasure.height}) ) {
		xLayoutNodeDestroy(pLayout, iRoot);
		return XUI_ERROR_UNSUPPORTED;
	}
	for ( i = 0; i < xLayoutNodeFragmentCount(pLayout, iSource); i++ ) {
		if ( !xLayoutNodeGetFragment(pLayout, iSource, i, &tFragment) ) continue;
		if ( fLastY >= 0.0f && tFragment.rect.y > fLastY + 0.1f ) iLine++;
		fLastY = tFragment.rect.y;
		iRet = __xuiRichEditAddFragment(pData, &pData->pAtoms[(int)tFragment.tag], tFragment, *pY, iLine);
		if ( iRet != XUI_OK ) { xLayoutNodeDestroy(pLayout, iRoot); return iRet; }
	}
	if ( tMeasure.width > pData->fContentWidth ) pData->fContentWidth = tMeasure.width;
	{
		xui_rich_node_info_t tBlockInfo;
		memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
		if ( xuiRichNodeGetInfo(pParagraph, &tBlockInfo) == XUI_OK ) {
			float fBefore = tBlockInfo.tParagraphStyle.fSpaceBefore * pData->fZoom;
			float fIndentLeft = tBlockInfo.tParagraphStyle.fIndentLeft * pData->fZoom;
			int j;
			if ( tBlockInfo.iType == XUI_RICH_NODE_BLOCK_QUOTE ) fIndentLeft += 16.0f * pData->fZoom;
			if ( tBlockInfo.iType == XUI_RICH_NODE_LIST_ITEM || tBlockInfo.tParagraphStyle.iListType != XUI_RICH_LIST_NONE ) fIndentLeft += 24.0f * pData->fZoom;
			for ( j = iFragmentStart; j < pData->iFragmentCount; j++ ) pData->pFragments[j].tPublic.tRect.fY += fBefore;
			__xuiRichEditAlignParagraph(pData, iFragmentStart, fWidth, tBlockInfo.tParagraphStyle.iAlign,
				fIndentLeft + tBlockInfo.tParagraphStyle.fFirstLineIndent * pData->fZoom,
				tBlockInfo.tParagraphStyle.fIndentRight * pData->fZoom);
			*pY += fBefore + tBlockInfo.tParagraphStyle.fSpaceAfter * pData->fZoom;
		}
	}
	*pY += tMeasure.height;
	{
		xui_rich_node_info_t tBlockInfo;
		memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
		if ( xuiRichNodeGetInfo(pParagraph, &tBlockInfo) == XUI_OK )
			(void)__xuiRichEditAddBlock(pData, pParagraph, tBlockInfo.iType, iDocumentStart,
				*pDocumentAt, (xui_rect_t){0.0f, fBlockY, fWidth, *pY - fBlockY},
				iFragmentStart, iLineStart, iLine - iLineStart + 1);
	}
	*pLineBase = iLine + 1;
	xLayoutNodeDestroy(pLayout, iRoot);
	return XUI_OK;
}

static int __xuiRichEditLayoutAtomic(xui_widget pWidget, xui_rich_edit_data_t* pData,
	xui_rich_node pNode, int* pDocumentAt, float fWidth, float* pY, int* pLine)
{
	xui_rich_node_info_t tInfo;
	xui_proxy pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	xui_font_metrics_t tMetrics;
	xui_vec2_t tSize;
	xui_rect_t tRect;
	float fW;
	float fH;
	float fLineHeight = 18.0f;
	int i;
	int iFragmentStart = pData->iFragmentCount;
	int iLineStart = *pLine;
	int iRet;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tMetrics, 0, sizeof(tMetrics));
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pData->pFont != NULL &&
	     pProxy->fontGetMetrics(pProxy, pData->pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f )
		fLineHeight = tMetrics.fLineHeight;
	if ( tInfo.iType == XUI_RICH_NODE_IMAGE ) {
		fW = tInfo.fWidth * pData->fZoom;
		fH = tInfo.fHeight * pData->fZoom;
		if ( tInfo.pSurface != NULL && pProxy != NULL && pProxy->surfaceGetDesc != NULL && (fW <= 0.0f || fH <= 0.0f) ) {
			xui_surface_desc_t tDesc;
			memset(&tDesc, 0, sizeof(tDesc));
			if ( pProxy->surfaceGetDesc(pProxy, tInfo.pSurface, &tDesc) == XUI_OK ) {
				if ( fW <= 0.0f ) fW = (float)tDesc.iWidth * pData->fZoom;
				if ( fH <= 0.0f ) fH = (float)tDesc.iHeight * pData->fZoom;
			}
		}
		if ( fW <= 0.0f ) fW = fWidth * 0.5f;
		if ( fH <= 0.0f ) fH = 96.0f * pData->fZoom;
		if ( fW > fWidth ) { fH *= fWidth / fW; fW = fWidth; }
	} else if ( tInfo.iType == XUI_RICH_NODE_TABLE ) {
		float fMaxRow = fLineHeight + tInfo.fCellPadding * pData->fZoom * 2.0f;
		fW = tInfo.fWidth > 0.0f ? tInfo.fWidth * pData->fZoom : fWidth;
		if ( fW > fWidth ) fW = fWidth;
		for ( i = 0; i < tInfo.iRows; i++ ) {
			float fRow = fLineHeight + tInfo.fCellPadding * pData->fZoom * 2.0f;
			int j;
			for ( j = 0; j < tInfo.iColumns; j++ ) {
				const char* sCell = xuiRichTableGetCellText(pNode, i, j);
				memset(&tSize, 0, sizeof(tSize));
				if ( sCell != NULL && pProxy != NULL && pProxy->textMeasure != NULL && pData->pFont != NULL &&
				     pProxy->textMeasure(pProxy, __xuiRichEditSizedFont(pWidget, pData, pData->pFont, 0.0f), sCell, &tSize) == XUI_OK &&
				     tSize.fY + tInfo.fCellPadding * pData->fZoom * 2.0f > fRow )
					fRow = tSize.fY + tInfo.fCellPadding * pData->fZoom * 2.0f;
			}
			if ( fRow > fMaxRow ) fMaxRow = fRow;
		}
		fH = fMaxRow * (float)tInfo.iRows;
	} else {
		fW = fWidth;
		fH = 17.0f * pData->fZoom;
	}
	tRect = (xui_rect_t){0.0f, *pY + tInfo.tParagraphStyle.fSpaceBefore * pData->fZoom, fW, fH};
	if ( tInfo.tParagraphStyle.iAlign == XUI_RICH_ALIGN_CENTER ) tRect.fX = (fWidth - fW) * 0.5f;
	else if ( tInfo.tParagraphStyle.iAlign == XUI_RICH_ALIGN_RIGHT ) tRect.fX = fWidth - fW;
	iRet = __xuiRichEditAddAtomicFragment(pData, pNode, &tInfo, *pDocumentAt, tRect, *pLine);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRichEditAddBlock(pData, pNode, tInfo.iType, *pDocumentAt, *pDocumentAt + 3, tRect,
		iFragmentStart, iLineStart, 1);
	if ( iRet != XUI_OK ) return iRet;
	*pDocumentAt += 3;
	*pY = tRect.fY + tRect.fH + tInfo.tParagraphStyle.fSpaceAfter * pData->fZoom;
	(*pLine)++;
	if ( fW > pData->fContentWidth ) pData->fContentWidth = fW;
	return XUI_OK;
}

static void __xuiRichEditRecomputeContentWidth(xui_rich_edit_data_t* pData)
{
	float fWidth = 0.0f;
	int i;
	for ( i = 0; i < pData->iBlockCount; i++ ) if ( pData->pBlocks[i].fContentWidth > fWidth )
		fWidth = pData->pBlocks[i].fContentWidth;
	pData->fContentWidth = fWidth;
}

static int __xuiRichEditIncrementalLayout(xui_widget pWidget, xui_rich_edit_data_t* pData, float fWidth)
{
	xui_rich_block_layout_t tOldBlock;
	xui_rich_node pNode;
	xui_rich_node_info_t tNodeInfo;
	xui_rich_node_info_t tBlockInfo;
	float fY;
	float fYDelta;
	int iDocumentAt;
	int iDocumentDelta;
	int iLine;
	int iLineDelta;
	int iFragmentDelta;
	int iOldBlockCount;
	int iOldTotalFragments;
	int i;
	int iRet;
	if ( pData->iDirtyBlock < 0 || pData->iDirtyBlock >= pData->iBlockCount ||
	     pData->tLayoutChange.iNodeId == 0 ) return XUI_ERROR_UNSUPPORTED;
	pNode = xuiRichDocumentFindNode(pData->pDocument, pData->tLayoutChange.iNodeId);
	memset(&tNodeInfo, 0, sizeof(tNodeInfo)); tNodeInfo.iSize = sizeof(tNodeInfo);
	if ( pNode == NULL || xuiRichNodeGetInfo(pNode, &tNodeInfo) != XUI_OK || tNodeInfo.iParentId == 0 )
		return XUI_ERROR_UNSUPPORTED;
	pNode = xuiRichDocumentFindNode(pData->pDocument, tNodeInfo.iParentId);
	memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
	if ( pNode == NULL || xuiRichNodeGetInfo(pNode, &tBlockInfo) != XUI_OK ||
	     (tBlockInfo.iType != XUI_RICH_NODE_PARAGRAPH && tBlockInfo.iType != XUI_RICH_NODE_HEADING &&
	      tBlockInfo.iType != XUI_RICH_NODE_BLOCK_QUOTE && tBlockInfo.iType != XUI_RICH_NODE_LIST_ITEM) ||
	     pData->pBlocks[pData->iDirtyBlock].pNode != pNode ) return XUI_ERROR_UNSUPPORTED;
	tOldBlock = pData->pBlocks[pData->iDirtyBlock];
	iOldBlockCount = pData->iBlockCount;
	iOldTotalFragments = pData->iTotalFragmentCount;
	pData->iFragmentCount = 0;
	pData->iTotalFragmentCount = tOldBlock.iFragmentStart;
	pData->iBlockCount = pData->iDirtyBlock;
	iDocumentAt = tOldBlock.iDocumentStart;
	fY = tOldBlock.tRect.fY;
	iLine = tOldBlock.iLineStart;
	iRet = __xuiRichEditLayoutParagraph(pWidget, pData, pNode, &iDocumentAt, fWidth, &fY, &iLine);
	if ( iRet != XUI_OK ) {
		pData->pBlocks[pData->iDirtyBlock] = tOldBlock;
		pData->iBlockCount = iOldBlockCount;
		pData->iTotalFragmentCount = iOldTotalFragments;
		return iRet;
	}
	iFragmentDelta = pData->pBlocks[pData->iDirtyBlock].iFragmentCount - tOldBlock.iFragmentCount;
	iDocumentDelta = pData->pBlocks[pData->iDirtyBlock].iDocumentEnd - tOldBlock.iDocumentEnd;
	iLineDelta = pData->pBlocks[pData->iDirtyBlock].iLineCount - tOldBlock.iLineCount;
	fYDelta = pData->pBlocks[pData->iDirtyBlock].tRect.fH - tOldBlock.tRect.fH;
	if ( tOldBlock.pFragments != NULL ) xrtFree(tOldBlock.pFragments);
	pData->iBlockCount = iOldBlockCount;
	pData->iTotalFragmentCount = iOldTotalFragments + iFragmentDelta;
	for ( i = pData->iDirtyBlock + 1; i < pData->iBlockCount; i++ ) {
		pData->pBlocks[i].iDocumentStart += iDocumentDelta;
		pData->pBlocks[i].iDocumentEnd += iDocumentDelta;
		pData->pBlocks[i].iFragmentStart += iFragmentDelta;
		pData->pBlocks[i].iLineStart += iLineDelta;
		pData->pBlocks[i].tRect.fY += fYDelta;
	}
	pData->fContentHeight += fYDelta;
	__xuiRichEditRecomputeContentWidth(pData);
	pData->fLayoutWidth = fWidth;
	pData->iLayoutVersion = xuiRichDocumentGetVersion(pData->pDocument);
	pData->bLayoutDirty = 0;
	pData->bIncrementalLayout = 0;
	return XUI_OK;
}

static int __xuiRichEditIncrementalStructureLayout(xui_widget pWidget, xui_rich_edit_data_t* pData, float fWidth)
{
	xui_rich_block_layout_t* pOldBlocks;
	xui_rich_node pRoot;
	xui_rich_node pNode;
	xui_rich_node pAfter;
	float fStartY;
	float fOldEndY;
	float fY;
	float fYDelta;
	float fOldContentHeight = pData->fContentHeight;
	float fOldContentWidth = pData->fContentWidth;
	int iOldBlockCount = pData->iBlockCount;
	int iOldTotalFragments = pData->iTotalFragmentCount;
	int iOldTailDocument;
	int iOldTailLine;
	int iOldTailFragment;
	int iDocumentAt;
	int iLine;
	int iDocumentDelta;
	int iLineDelta;
	int iFragmentDelta;
	int iNewEnd;
	int iTailCount;
	int i;
	int iRet = XUI_OK;
	if ( pData->iDirtyBlock < 0 || pData->iDirtyBlockEnd < pData->iDirtyBlock ||
	     pData->iDirtyBlockEnd >= pData->iBlockCount ) return XUI_ERROR_UNSUPPORTED;
	pOldBlocks = (xui_rich_block_layout_t*)xrtMalloc(sizeof(*pOldBlocks) *
		(size_t)(iOldBlockCount - pData->iDirtyBlock));
	if ( pOldBlocks == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(pOldBlocks, pData->pBlocks + pData->iDirtyBlock,
		sizeof(*pOldBlocks) * (size_t)(iOldBlockCount - pData->iDirtyBlock));
	iTailCount = iOldBlockCount - pData->iDirtyBlockEnd - 1;
	pAfter = iTailCount > 0 ? pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock + 1].pNode : NULL;
	pRoot = xuiRichDocumentGetRoot(pData->pDocument);
	pNode = pData->iDirtyBlock > 0 ? xuiRichNodeGetNextSibling(pData->pBlocks[pData->iDirtyBlock - 1].pNode) :
		xuiRichNodeGetFirstChild(pRoot);
	if ( pNode == NULL || pNode == pAfter ) { xrtFree(pOldBlocks); return XUI_ERROR_UNSUPPORTED; }
	fStartY = pOldBlocks[0].tRect.fY;
	fOldEndY = iTailCount > 0 ? pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock + 1].tRect.fY : fOldContentHeight;
	iOldTailDocument = iTailCount > 0 ? pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock + 1].iDocumentStart :
		xuiRichDocumentGetLength(pData->pDocument) -
		(pData->tLayoutChange.iNewEnd - pData->tLayoutChange.iNewStart) +
		(pData->tLayoutChange.iOldEnd - pData->tLayoutChange.iOldStart);
	iOldTailLine = iTailCount > 0 ? pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock + 1].iLineStart :
		pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock].iLineStart +
		pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock].iLineCount;
	iOldTailFragment = iTailCount > 0 ? pOldBlocks[pData->iDirtyBlockEnd - pData->iDirtyBlock + 1].iFragmentStart : iOldTotalFragments;
	pData->iBlockCount = pData->iDirtyBlock;
	pData->iTotalFragmentCount = pOldBlocks[0].iFragmentStart;
	pData->iFragmentCount = 0;
	iDocumentAt = pOldBlocks[0].iDocumentStart;
	iLine = pOldBlocks[0].iLineStart;
	fY = fStartY;
	while ( pNode != pAfter ) {
		xui_rich_node_info_t tInfo;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) { iRet = XUI_ERROR_INVALID_ARGUMENT; break; }
		pData->iFragmentCount = 0;
		if ( tInfo.iType == XUI_RICH_NODE_IMAGE || tInfo.iType == XUI_RICH_NODE_TABLE || tInfo.iType == XUI_RICH_NODE_HORIZONTAL_RULE )
			iRet = __xuiRichEditLayoutAtomic(pWidget, pData, pNode, &iDocumentAt, fWidth, &fY, &iLine);
		else iRet = __xuiRichEditLayoutParagraph(pWidget, pData, pNode, &iDocumentAt, fWidth, &fY, &iLine);
		if ( iRet != XUI_OK ) break;
		pNode = xuiRichNodeGetNextSibling(pNode);
		if ( pNode != NULL ) { iDocumentAt++; fY += pData->fParagraphGap * pData->fZoom; }
		if ( pNode == NULL && pAfter != NULL ) { iRet = XUI_ERROR_UNSUPPORTED; break; }
	}
	if ( iRet != XUI_OK ) {
		for ( i = pData->iDirtyBlock; i < pData->iBlockCount; i++ )
			if ( pData->pBlocks[i].pFragments != NULL ) xrtFree(pData->pBlocks[i].pFragments);
		memcpy(pData->pBlocks + pData->iDirtyBlock, pOldBlocks,
			sizeof(*pOldBlocks) * (size_t)(iOldBlockCount - pData->iDirtyBlock));
		pData->iBlockCount = iOldBlockCount;
		pData->iTotalFragmentCount = iOldTotalFragments;
		pData->fContentHeight = fOldContentHeight;
		pData->fContentWidth = fOldContentWidth;
		xrtFree(pOldBlocks);
		return iRet;
	}
	iNewEnd = pData->iBlockCount;
	iDocumentDelta = iDocumentAt - iOldTailDocument;
	iLineDelta = iLine - iOldTailLine;
	iFragmentDelta = pData->iTotalFragmentCount - iOldTailFragment;
	fYDelta = fY - fOldEndY;
	iRet = __xuiRichEditReserve((void**)&pData->pBlocks, &pData->iBlockCapacity,
		iNewEnd + iTailCount, sizeof(*pData->pBlocks));
	if ( iRet != XUI_OK ) {
		for ( i = pData->iDirtyBlock; i < pData->iBlockCount; i++ )
			if ( pData->pBlocks[i].pFragments != NULL ) xrtFree(pData->pBlocks[i].pFragments);
		memcpy(pData->pBlocks + pData->iDirtyBlock, pOldBlocks,
			sizeof(*pOldBlocks) * (size_t)(iOldBlockCount - pData->iDirtyBlock));
		pData->iBlockCount = iOldBlockCount;
		pData->iTotalFragmentCount = iOldTotalFragments;
		pData->fContentHeight = fOldContentHeight;
		pData->fContentWidth = fOldContentWidth;
		xrtFree(pOldBlocks);
		return iRet;
	}
	for ( i = 0; i <= pData->iDirtyBlockEnd - pData->iDirtyBlock; i++ )
		if ( pOldBlocks[i].pFragments != NULL ) xrtFree(pOldBlocks[i].pFragments);
	if ( iTailCount > 0 ) memcpy(pData->pBlocks + iNewEnd,
		pOldBlocks + pData->iDirtyBlockEnd - pData->iDirtyBlock + 1,
		sizeof(*pOldBlocks) * (size_t)iTailCount);
	pData->iBlockCount = iNewEnd + iTailCount;
	for ( i = iNewEnd; i < pData->iBlockCount; i++ ) {
		pData->pBlocks[i].iDocumentStart += iDocumentDelta;
		pData->pBlocks[i].iDocumentEnd += iDocumentDelta;
		pData->pBlocks[i].iFragmentStart += iFragmentDelta;
		pData->pBlocks[i].iLineStart += iLineDelta;
		pData->pBlocks[i].tRect.fY += fYDelta;
	}
	pData->iTotalFragmentCount = iOldTotalFragments + iFragmentDelta;
	pData->fContentHeight = fOldContentHeight + fYDelta;
	__xuiRichEditRecomputeContentWidth(pData);
	pData->fLayoutWidth = fWidth;
	pData->iLayoutVersion = xuiRichDocumentGetVersion(pData->pDocument);
	pData->bLayoutDirty = 0;
	pData->bIncrementalLayout = 0;
	xrtFree(pOldBlocks);
	return XUI_OK;
}

static int __xuiRichEditEnsureLayout(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_rect_t tContent;
	xui_rich_node pRoot;
	xui_rich_node pParagraph;
	float fY = 0.0f;
	float fWidth;
	int iDocumentAt = 0;
	int iLine = 0;
	int iRet;
	if ( pData->pDocument == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tContent = __xuiRichEditContentRect(pWidget, pData);
	fWidth = tContent.fW > 1.0f ? tContent.fW : 1.0f;
	if ( !pData->bLayoutDirty && pData->iLayoutVersion == xuiRichDocumentGetVersion(pData->pDocument) && pData->fLayoutWidth == fWidth ) return XUI_OK;
	if ( pData->bIncrementalLayout && pData->fLayoutWidth == fWidth ) {
		iRet = (pData->tLayoutChange.iFlags & XUI_RICH_CHANGE_STRUCTURE) != 0 ?
			__xuiRichEditIncrementalStructureLayout(pWidget, pData, fWidth) :
			__xuiRichEditIncrementalLayout(pWidget, pData, fWidth);
		if ( iRet == XUI_OK ) return XUI_OK;
		pData->bIncrementalLayout = 0;
	}
	__xuiRichEditReleaseBlocks(pData);
	pData->iFragmentCount = 0;
	pData->fContentWidth = 0.0f;
	pRoot = xuiRichDocumentGetRoot(pData->pDocument);
	for ( pParagraph = xuiRichNodeGetFirstChild(pRoot); pParagraph != NULL; pParagraph = xuiRichNodeGetNextSibling(pParagraph) ) {
		xui_rich_node_info_t tInfo;
		pData->iFragmentCount = 0;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pParagraph, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( tInfo.iType == XUI_RICH_NODE_IMAGE || tInfo.iType == XUI_RICH_NODE_TABLE || tInfo.iType == XUI_RICH_NODE_HORIZONTAL_RULE )
			iRet = __xuiRichEditLayoutAtomic(pWidget, pData, pParagraph, &iDocumentAt, fWidth, &fY, &iLine);
		else iRet = __xuiRichEditLayoutParagraph(pWidget, pData, pParagraph, &iDocumentAt, fWidth, &fY, &iLine);
		if ( iRet != XUI_OK ) return iRet;
		if ( xuiRichNodeGetNextSibling(pParagraph) != NULL ) { iDocumentAt++; fY += pData->fParagraphGap * pData->fZoom; }
	}
	pData->fContentHeight = fY;
	pData->fLayoutWidth = fWidth;
	pData->iLayoutVersion = xuiRichDocumentGetVersion(pData->pDocument);
	pData->bLayoutDirty = 0;
	pData->bIncrementalLayout = 0;
	return XUI_OK;
}

static int __xuiRichEditUpdateScrollModel(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_rect_t tRaw;
	xui_rect_t tViewport;
	float fContentWidth;
	float fContentHeight;
	float fScrollX;
	float fScrollY;
	float fMaxX;
	float fMaxY;
	int bShowH;
	int bShowV;
	int bNextH;
	int bNextV;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tRaw = __xuiRichEditRawContentRect(pWidget, pData);
	bShowH = pData->bShowHScrollBar;
	bShowV = pData->bShowVScrollBar;
	for ( i = 0; i < 4; i++ ) {
		pData->bShowHScrollBar = bShowH;
		pData->bShowVScrollBar = bShowV;
		tViewport = __xuiRichEditViewportFromRaw(tRaw, bShowH, bShowV);
		iRet = __xuiRichEditEnsureLayout(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		bNextH = (!pData->bWordWrap && pData->fContentWidth > tViewport.fW + 0.01f) ? 1 : 0;
		bNextV = (pData->fContentHeight > tViewport.fH + 0.01f) ? 1 : 0;
		if ( bNextH == bShowH && bNextV == bShowV ) break;
		bShowH = bNextH;
		bShowV = bNextV;
	}
	pData->bShowHScrollBar = bShowH;
	pData->bShowVScrollBar = bShowV;
	tViewport = __xuiRichEditViewportFromRaw(tRaw, bShowH, bShowV);
	iRet = __xuiRichEditEnsureLayout(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->tScrollViewportRect = tViewport;
	pData->tHScrollBarRect = bShowH ?
		(xui_rect_t){tRaw.fX, tRaw.fY + tViewport.fH, tViewport.fW, XUI_RICH_EDIT_SCROLLBAR_SIZE} :
		(xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tVScrollBarRect = bShowV ?
		(xui_rect_t){tRaw.fX + tViewport.fW, tRaw.fY, XUI_RICH_EDIT_SCROLLBAR_SIZE, tViewport.fH} :
		(xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( bShowH && !bShowV ) pData->tHScrollBarRect.fW = tRaw.fW;
	if ( bShowV && !bShowH ) pData->tVScrollBarRect.fH = tRaw.fH;
	fContentWidth = pData->bWordWrap ? tViewport.fW : pData->fContentWidth;
	fContentHeight = pData->fContentHeight;
	if ( fContentWidth < tViewport.fW ) fContentWidth = tViewport.fW;
	if ( fContentHeight < tViewport.fH ) fContentHeight = tViewport.fH;
	iRet = xuiScrollModelSetViewport(&pData->tScrollModel,
		(xui_rect_t){0.0f, 0.0f, tViewport.fW, tViewport.fH});
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetContentSize(&pData->tScrollModel, fContentWidth, fContentHeight);
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetOffset(&pData->tScrollModel, pData->fScrollX, pData->fScrollY);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiScrollModelGetOffset(&pData->tScrollModel, &fScrollX, &fScrollY);
	pData->fScrollX = fScrollX;
	pData->fScrollY = fScrollY;
	if ( pData->pHScrollBar != NULL && pData->pVScrollBar != NULL ) {
		(void)xuiScrollModelGetMaxOffset(&pData->tScrollModel, &fMaxX, &fMaxY);
		pData->bSyncingScrollBars = 1;
		iRet = xuiWidgetSetRect(pData->pHScrollBar, pData->tHScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetRect(pData->pVScrollBar, pData->tVScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pHScrollBar, bShowH);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pVScrollBar, bShowV);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pHScrollBar, 0.0f, fMaxX, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pVScrollBar, 0.0f, fMaxY, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pHScrollBar, 24.0f, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pVScrollBar, XUI_RICH_EDIT_SCROLL_STEP, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pHScrollBar, pData->fScrollX);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pVScrollBar, pData->fScrollY);
		pData->bSyncingScrollBars = 0;
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiRichEditVisitWidgets(xui_widget pWidget, xui_rich_node pNode, int bAttach)
{
	xui_rich_node_info_t tInfo;
	xui_rich_node pChild;
	int iRet;
	for ( pChild = xuiRichNodeGetFirstChild(pNode); pChild != NULL; pChild = xuiRichNodeGetNextSibling(pChild) ) {
		memset(&tInfo, 0, sizeof(tInfo));
		tInfo.iSize = sizeof(tInfo);
		iRet = xuiRichNodeGetInfo(pChild, &tInfo);
		if ( iRet != XUI_OK ) return iRet;
		if ( tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET && tInfo.pWidget != NULL ) {
			xui_widget pParent = xuiWidgetGetParent(tInfo.pWidget);
			if ( bAttach ) {
				if ( pParent == NULL ) {
					iRet = xuiWidgetAddChild(pWidget, tInfo.pWidget);
					if ( iRet != XUI_OK ) return iRet;
				} else if ( pParent != pWidget ) return XUI_ERROR_INVALID_ARGUMENT;
			} else if ( pParent == pWidget ) {
				iRet = xuiWidgetRemoveFromParent(tInfo.pWidget);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		iRet = __xuiRichEditVisitWidgets(pWidget, pChild, bAttach);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiRichEditContainsWidget(xui_rich_node pNode, xui_widget pWidget)
{
	xui_rich_node pChild;
	xui_rich_node_info_t tInfo;
	for ( pChild = xuiRichNodeGetFirstChild(pNode); pChild != NULL; pChild = xuiRichNodeGetNextSibling(pChild) ) {
		memset(&tInfo, 0, sizeof(tInfo));
		tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pChild, &tInfo) == XUI_OK &&
		     tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET && tInfo.pWidget == pWidget ) return 1;
		if ( __xuiRichEditContainsWidget(pChild, pWidget) ) return 1;
	}
	return 0;
}

static int __xuiRichEditSyncWidgets(xui_widget pWidget, xui_rich_edit_data_t* pData,
	xui_rich_document pDocument, int bAttach)
{
	xui_rich_node pRoot = pDocument != NULL ? xuiRichDocumentGetRoot(pDocument) : NULL;
	xui_widget pChild;
	xui_widget pNext;
	int iRet;
	if ( pRoot == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditVisitWidgets(pWidget, pRoot, bAttach);
	if ( iRet != XUI_OK || !bAttach ) return iRet;
	for ( pChild = xuiWidgetGetFirstChild(pWidget); pChild != NULL; pChild = pNext ) {
		pNext = xuiWidgetGetNextSibling(pChild);
		if ( pChild != pData->pCellEditor && pChild != pData->pHScrollBar && pChild != pData->pVScrollBar &&
		     !__xuiRichEditContainsWidget(pRoot, pChild) ) {
			iRet = xuiWidgetRemoveFromParent(pChild);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static xui_rect_t __xuiRichEditFragmentRect(xui_widget pWidget, xui_rich_edit_data_t* pData, const xui_rich_layout_fragment_t* pFragment)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	xui_rect_t tRect = pFragment->tPublic.tRect;
	tRect.fX += tContent.fX - pData->fScrollX;
	tRect.fY += tContent.fY - pData->fScrollY;
	return tRect;
}

static xui_rect_t __xuiRichEditCaret(xui_widget pWidget, xui_rich_edit_data_t* pData, int iOffset)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	xui_rect_t tRect = {tContent.fX - pData->fScrollX, tContent.fY - pData->fScrollY, 1.0f, 16.0f};
	xui_rich_layout_fragment_t tFragment;
	xui_rich_block_layout_t* pBlock;
	int iBlock;
	int i;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	if ( pData->iBlockCount <= 0 ) return tRect;
	iBlock = __xuiRichEditFindBlockByOffset(pData, iOffset);
	if ( iBlock < 0 ) return tRect;
	pBlock = &pData->pBlocks[iBlock];
	for ( i = 0; i < pBlock->iFragmentCount; i++ ) {
		xui_rich_layout_fragment_t* pFragment = &tFragment;
		__xuiRichEditGetBlockFragment(pBlock, i, pFragment);
		if ( iOffset <= pFragment->tPublic.iDocumentStart ) {
			tRect = __xuiRichEditFragmentRect(pWidget, pData, pFragment);
			tRect.fW = 1.0f;
			break;
		}
		if ( iOffset <= pFragment->tPublic.iDocumentEnd ) {
			tRect = __xuiRichEditFragmentRect(pWidget, pData, pFragment);
			tRect.fX += tRect.fW;
			tRect.fW = 1.0f;
			break;
		}
		tRect = __xuiRichEditFragmentRect(pWidget, pData, pFragment);
		tRect.fX += tRect.fW;
		tRect.fW = 1.0f;
	}
	return tRect;
}

static int __xuiRichEditHit(xui_widget pWidget, xui_rich_edit_data_t* pData, float fLocalX, float fLocalY)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	float fX = fLocalX - tContent.fX + pData->fScrollX;
	float fY = fLocalY - tContent.fY + pData->fScrollY;
	int iLineStart;
	int iLineEnd;
	int iNext;
	int i;
	int iBest;
	float fBest;
	float fDocumentTop;
	float fDocumentBottom;
	xui_rich_layout_fragment_t tFragment;
	xui_rich_layout_fragment_t tNextFragment;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	if ( pData->iTotalFragmentCount == 0 || __xuiRichEditGetLayoutFragment(pData, 0, &tFragment) != XUI_OK ) return 0;

	/* Outside the document vertically, editors conventionally clamp to its ends. */
	fDocumentTop = tFragment.tPublic.tRect.fY;
	fDocumentBottom = fDocumentTop + tFragment.tPublic.tRect.fH;
	for ( i = 1; i < pData->iTotalFragmentCount; i++ ) {
		xui_rect_t tRect;
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		tRect = tFragment.tPublic.tRect;
		if ( tRect.fY < fDocumentTop ) fDocumentTop = tRect.fY;
		if ( tRect.fY + tRect.fH > fDocumentBottom ) fDocumentBottom = tRect.fY + tRect.fH;
	}
	if ( fY < fDocumentTop ) return 0;
	if ( fY > fDocumentBottom ) return xuiRichDocumentGetLength(pData->pDocument);

	/* Pick a visual line first. Horizontal distance must never select another line. */
	iLineStart = 0;
	for (;;) {
		int iLine;
		float fBottom;
		float fNextTop;
		(void)__xuiRichEditGetLayoutFragment(pData, iLineStart, &tFragment);
		iLine = tFragment.tPublic.iLine;
		fBottom = tFragment.tPublic.tRect.fY + tFragment.tPublic.tRect.fH;
		iLineEnd = iLineStart + 1;
		while ( iLineEnd < pData->iTotalFragmentCount ) {
			xui_rect_t tRect;
			(void)__xuiRichEditGetLayoutFragment(pData, iLineEnd, &tNextFragment);
			if ( tNextFragment.tPublic.iLine != iLine ) break;
			tRect = tNextFragment.tPublic.tRect;
			if ( tRect.fY + tRect.fH > fBottom ) fBottom = tRect.fY + tRect.fH;
			iLineEnd++;
		}
		if ( iLineEnd >= pData->iTotalFragmentCount ) break;
		iNext = iLineEnd;
		(void)__xuiRichEditGetLayoutFragment(pData, iLineEnd, &tNextFragment);
		fNextTop = tNextFragment.tPublic.tRect.fY;
		while ( iNext < pData->iTotalFragmentCount ) {
			(void)__xuiRichEditGetLayoutFragment(pData, iNext, &tFragment);
			if ( tFragment.tPublic.iLine != tNextFragment.tPublic.iLine ) break;
			if ( tFragment.tPublic.tRect.fY < fNextTop ) fNextTop = tFragment.tPublic.tRect.fY;
			iNext++;
		}
		if ( fY < (fBottom + fNextTop) * 0.5f ) break;
		iLineStart = iLineEnd;
	}

	/* Every fragment contributes a leading and trailing caret stop. */
	(void)__xuiRichEditGetLayoutFragment(pData, iLineStart, &tFragment);
	iBest = tFragment.tPublic.iDocumentStart;
	fBest = 3.402823466e+38F;
	for ( i = iLineStart; i < iLineEnd; i++ ) {
		xui_rect_t tRect;
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		tRect = tFragment.tPublic.tRect;
		float fDistance = fX >= tRect.fX ? fX - tRect.fX : tRect.fX - fX;
		if ( fDistance < fBest ) {
			fBest = fDistance;
			iBest = tFragment.tPublic.iDocumentStart;
		}
		fDistance = fX >= tRect.fX + tRect.fW ? fX - tRect.fX - tRect.fW : tRect.fX + tRect.fW - fX;
		if ( fDistance <= fBest ) {
			fBest = fDistance;
			iBest = tFragment.tPublic.iDocumentEnd;
		}
	}
	return iBest;
}

static xui_rich_node __xuiRichEditLinkAt(xui_widget pWidget, xui_rich_edit_data_t* pData,
	float fLocalX, float fLocalY)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	float fX = fLocalX - tContent.fX + pData->fScrollX;
	float fY = fLocalY - tContent.fY + pData->fScrollY;
	xui_rich_layout_fragment_t tFragment;
	int i;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rich_layout_fragment_t* pFragment = &tFragment;
		(void)__xuiRichEditGetLayoutFragment(pData, i, pFragment);
		xui_rect_t tRect = pFragment->tPublic.tRect;
		if ( pFragment->tPublic.iNodeType == XUI_RICH_NODE_LINK && fX >= tRect.fX &&
		     fX <= tRect.fX + tRect.fW && fY >= tRect.fY && fY <= tRect.fY + tRect.fH ) return pFragment->pNode;
	}
	return NULL;
}

static int __xuiRichEditAtomicAt(xui_widget pWidget, xui_rich_edit_data_t* pData,
	float fLocalX, float fLocalY, int* pStart, int* pEnd, xui_rich_node* ppNode)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	float fX = fLocalX - tContent.fX + pData->fScrollX;
	float fY = fLocalY - tContent.fY + pData->fScrollY;
	xui_rich_layout_fragment_t tFragment;
	int i;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rect_t tRect;
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( tFragment.tPublic.iNodeType == XUI_RICH_NODE_TEXT ||
		     tFragment.tPublic.iNodeType == XUI_RICH_NODE_LINK ) continue;
		tRect = tFragment.tPublic.tRect;
		if ( fX >= tRect.fX && fX <= tRect.fX + tRect.fW &&
		     fY >= tRect.fY && fY <= tRect.fY + tRect.fH ) {
			if ( pStart != NULL ) *pStart = tFragment.tPublic.iDocumentStart;
			if ( pEnd != NULL ) *pEnd = tFragment.tPublic.iDocumentEnd;
			if ( ppNode != NULL ) *ppNode = tFragment.pNode;
			return 1;
		}
	}
	return 0;
}

static void __xuiRichEditSelection(xui_rich_edit_data_t* pData, int* pStart, int* pEnd)
{
	*pStart = pData->iAnchor < pData->iCaret ? pData->iAnchor : pData->iCaret;
	*pEnd = pData->iAnchor < pData->iCaret ? pData->iCaret : pData->iAnchor;
}

static void __xuiRichEditEmit(xui_widget pWidget, xui_rich_edit_data_t* pData,
	int iType, xui_rich_node pNode)
{
	xui_rich_edit_event_t tEvent;
	xui_rich_node_info_t tInfo;
	if ( pData == NULL || pData->onEvent == NULL ) return;
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iSize = sizeof(tEvent);
	tEvent.iType = iType;
	tEvent.iAnchor = pData->iAnchor;
	tEvent.iCaret = pData->iCaret;
	__xuiRichEditSelection(pData, &tEvent.iSelectionStart, &tEvent.iSelectionEnd);
	tEvent.fScrollX = pData->fScrollX;
	tEvent.fScrollY = pData->fScrollY;
	tEvent.fZoom = pData->fZoom;
	if ( pNode != NULL ) {
		memset(&tInfo, 0, sizeof(tInfo));
		tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pNode, &tInfo) == XUI_OK ) {
			tEvent.iNodeId = tInfo.iId;
			tEvent.iNodeType = tInfo.iType;
		}
	}
	pData->onEvent(pWidget, &tEvent, pData->pEventUser);
}

static void __xuiRichEditNotifyState(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	if ( pData->iNotifiedAnchor != pData->iAnchor || pData->iNotifiedCaret != pData->iCaret ) {
		pData->iNotifiedAnchor = pData->iAnchor;
		pData->iNotifiedCaret = pData->iCaret;
		__xuiRichEditEmit(pWidget, pData, XUI_RICH_EDIT_EVENT_SELECTION_CHANGED, NULL);
		(void)xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED,
			XUI_RICH_ACCESSIBLE_ROOT_ID);
	}
	if ( pData->fNotifiedScrollX != pData->fScrollX || pData->fNotifiedScrollY != pData->fScrollY ) {
		pData->fNotifiedScrollX = pData->fScrollX;
		pData->fNotifiedScrollY = pData->fScrollY;
		__xuiRichEditEmit(pWidget, pData, XUI_RICH_EDIT_EVENT_SCROLL_CHANGED, NULL);
		(void)xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED,
			XUI_RICH_ACCESSIBLE_ROOT_ID);
	}
	if ( pData->fNotifiedZoom != pData->fZoom ) {
		pData->fNotifiedZoom = pData->fZoom;
		__xuiRichEditEmit(pWidget, pData, XUI_RICH_EDIT_EVENT_ZOOM_CHANGED, NULL);
		(void)xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED,
			XUI_RICH_ACCESSIBLE_ROOT_ID);
	}
}

static int __xuiRichEditStyleAt(xui_rich_edit_data_t* pData, int iOffset, xui_rich_text_style_t* pStyle)
{
	xui_document_position_t tPosition;
	xui_rich_node pNode;
	xui_rich_node_info_t tInfo;
	if ( pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pStyle = pData->tTypingStyle;
	pStyle->iSize = sizeof(*pStyle);
	if ( xuiRichDocumentOffsetToPosition(pData->pDocument, iOffset, &tPosition) != XUI_OK ) return XUI_OK;
	pNode = xuiRichDocumentFindNode(pData->pDocument, tPosition.iNodeId);
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( pNode != NULL && xuiRichNodeGetInfo(pNode, &tInfo) == XUI_OK &&
	     (tInfo.iType == XUI_RICH_NODE_TEXT || tInfo.iType == XUI_RICH_NODE_LINK) ) *pStyle = tInfo.tStyle;
	pStyle->iSize = sizeof(*pStyle);
	return XUI_OK;
}

static int __xuiRichEditParagraphAt(xui_rich_edit_data_t* pData, int iOffset,
	xui_rich_node* ppBlock, xui_rich_node_info_t* pInfo)
{
	xui_document_position_t tPosition;
	xui_rich_node pNode;
	xui_rich_node_info_t tNodeInfo;
	if ( xuiRichDocumentOffsetToPosition(pData->pDocument, iOffset, &tPosition) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	pNode = xuiRichDocumentFindNode(pData->pDocument, tPosition.iNodeId);
	if ( pNode == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tNodeInfo, 0, sizeof(tNodeInfo)); tNodeInfo.iSize = sizeof(tNodeInfo);
	if ( xuiRichNodeGetInfo(pNode, &tNodeInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tNodeInfo.iType == XUI_RICH_NODE_TEXT || tNodeInfo.iType == XUI_RICH_NODE_LINK ||
	     tNodeInfo.iType == XUI_RICH_NODE_INLINE_WIDGET || tNodeInfo.iType == XUI_RICH_NODE_INLINE_IMAGE )
		pNode = xuiRichDocumentFindNode(pData->pDocument, tNodeInfo.iParentId);
	if ( pNode == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pInfo != NULL ) { memset(pInfo, 0, sizeof(*pInfo)); pInfo->iSize = sizeof(*pInfo); (void)xuiRichNodeGetInfo(pNode, pInfo); }
	if ( ppBlock != NULL ) *ppBlock = pNode;
	return XUI_OK;
}

static void __xuiRichEditEnsureCaretVisible(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	xui_rect_t tCaret = __xuiRichEditCaret(pWidget, pData, pData->iCaret);
	if ( tCaret.fX < tContent.fX ) pData->fScrollX -= tContent.fX - tCaret.fX;
	else if ( tCaret.fX > tContent.fX + tContent.fW ) pData->fScrollX += tCaret.fX - tContent.fX - tContent.fW;
	if ( tCaret.fY < tContent.fY ) pData->fScrollY -= tContent.fY - tCaret.fY;
	else if ( tCaret.fY + tCaret.fH > tContent.fY + tContent.fH ) pData->fScrollY += tCaret.fY + tCaret.fH - tContent.fY - tContent.fH;
	if ( pData->fScrollX < 0.0f ) pData->fScrollX = 0.0f;
	if ( pData->fScrollY < 0.0f ) pData->fScrollY = 0.0f;
}

static void __xuiRichEditDocumentChanged(xui_rich_document pDocument, const xui_rich_change_t* pChange, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rich_node pChangedNode;
	xui_rich_node_info_t tNodeInfo;
	xui_rich_node pBlockNode = NULL;
	int iLength;
	int i;
	(void)pDocument;
	if ( pData == NULL ) return;
	if ( !pData->bUpdatingFind ) __xuiRichEditClearFindData(pData);
	if ( pData->pCellEditor != NULL &&
	     (pChange == NULL || (pChange->iFlags & XUI_RICH_CHANGE_STRUCTURE) != 0 ||
	      xuiRichDocumentFindNode(pData->pDocument, pData->iActiveTableId) == NULL) )
		(void)__xuiRichEditEndCellEdit(pWidget, pData, 0);
	iLength = xuiRichDocumentGetLength(pData->pDocument);
	if ( pData->iAnchor > iLength ) pData->iAnchor = iLength;
	if ( pData->iCaret > iLength ) pData->iCaret = iLength;
	pData->bIncrementalLayout = 0;
	if ( pChange != NULL && pData->iBlockCount > 0 && pChange->iKind != XUI_RICH_CHANGE_RESET &&
	     (pChange->iFlags & XUI_RICH_CHANGE_GEOMETRY) != 0 &&
	     (pChange->iFlags & XUI_RICH_CHANGE_STRUCTURE) != 0 ) {
		int iStartBlock = __xuiRichEditFindBlockByOffset(pData, pChange->iOldStart);
		int iEndProbe = pChange->iOldEnd > pChange->iOldStart ? pChange->iOldEnd - 1 : pChange->iOldStart;
		int iEndBlock = __xuiRichEditFindBlockByOffset(pData, iEndProbe);
		if ( iStartBlock >= 0 && iEndBlock >= iStartBlock ) {
			pData->bIncrementalLayout = 1;
			pData->iDirtyBlock = iStartBlock;
			pData->iDirtyBlockEnd = iEndBlock;
			pData->tLayoutChange = *pChange;
		}
	} else if ( pChange != NULL && pData->iBlockCount > 0 &&
	     (pChange->iFlags & XUI_RICH_CHANGE_STRUCTURE) == 0 &&
	     (pChange->iFlags & XUI_RICH_CHANGE_GEOMETRY) != 0 && pChange->iNodeId != 0 ) {
		pChangedNode = xuiRichDocumentFindNode(pData->pDocument, pChange->iNodeId);
		memset(&tNodeInfo, 0, sizeof(tNodeInfo)); tNodeInfo.iSize = sizeof(tNodeInfo);
		if ( pChangedNode != NULL && xuiRichNodeGetInfo(pChangedNode, &tNodeInfo) == XUI_OK && tNodeInfo.iParentId != 0 )
			pBlockNode = xuiRichDocumentFindNode(pData->pDocument, tNodeInfo.iParentId);
		for ( i = 0; pBlockNode != NULL && i < pData->iBlockCount; i++ ) if ( pData->pBlocks[i].pNode == pBlockNode ) {
			pData->bIncrementalLayout = 1;
			pData->iDirtyBlock = i;
			pData->iDirtyBlockEnd = i;
			pData->tLayoutChange = *pChange;
			break;
		}
	}
	pData->bLayoutDirty = 1;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	(void)xuiWidgetNotifyAccessibility(pWidget,
		pChange != NULL && (pChange->iFlags & XUI_RICH_CHANGE_STRUCTURE) != 0 ?
			XUI_ACCESSIBLE_EVENT_TREE_CHANGED : XUI_ACCESSIBLE_EVENT_VALUE_CHANGED,
		pChange != NULL ? pChange->iNodeId : XUI_RICH_ACCESSIBLE_ROOT_ID);
	if ( pData->onChange != NULL ) pData->onChange(pWidget, pData->pDocument, pChange, pData->pChangeUser);
}

static int __xuiRichEditDrawRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	return (iColor & 0xffu) != 0 && pProxy->drawRectFill != NULL ? pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor) : XUI_OK;
}

static xui_rect_t __xuiRichEditBlockRect(xui_widget pWidget, xui_rich_edit_data_t* pData, xui_rect_t tRect)
{
	xui_rect_t tContent = __xuiRichEditContentRect(pWidget, pData);
	tRect.fX += tContent.fX - pData->fScrollX;
	tRect.fY += tContent.fY - pData->fScrollY;
	return tRect;
}

static int __xuiRichEditTableCellAt(xui_widget pWidget, xui_rich_edit_data_t* pData,
	float fLocalX, float fLocalY, xui_rich_node* ppTable, int* pRow, int* pColumn, xui_rect_t* pCellRect)
{
	int i;
	if ( __xuiRichEditEnsureLayout(pWidget, pData) != XUI_OK ) return 0;
	for ( i = 0; i < pData->iBlockCount; i++ ) {
		xui_rich_block_layout_t* pBlock = &pData->pBlocks[i];
		xui_rich_node_info_t tInfo;
		xui_rect_t tRect;
		int iRow;
		int iColumn;
		if ( pBlock->iType != XUI_RICH_NODE_TABLE ) continue;
		tRect = __xuiRichEditBlockRect(pWidget, pData, pBlock->tRect);
		if ( fLocalX < tRect.fX || fLocalX >= tRect.fX + tRect.fW ||
		     fLocalY < tRect.fY || fLocalY >= tRect.fY + tRect.fH ) continue;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pBlock->pNode, &tInfo) != XUI_OK || tInfo.iRows <= 0 || tInfo.iColumns <= 0 ) return 0;
		iColumn = (int)((fLocalX - tRect.fX) * (float)tInfo.iColumns / tRect.fW);
		iRow = (int)((fLocalY - tRect.fY) * (float)tInfo.iRows / tRect.fH);
		if ( iColumn >= tInfo.iColumns ) iColumn = tInfo.iColumns - 1;
		if ( iRow >= tInfo.iRows ) iRow = tInfo.iRows - 1;
		if ( ppTable != NULL ) *ppTable = pBlock->pNode;
		if ( pRow != NULL ) *pRow = iRow;
		if ( pColumn != NULL ) *pColumn = iColumn;
		if ( pCellRect != NULL ) *pCellRect = (xui_rect_t){
			tRect.fX + tRect.fW * (float)iColumn / (float)tInfo.iColumns,
			tRect.fY + tRect.fH * (float)iRow / (float)tInfo.iRows,
			tRect.fW / (float)tInfo.iColumns, tRect.fH / (float)tInfo.iRows};
		return 1;
	}
	return 0;
}

static int __xuiRichEditRenderBlocks(xui_widget pWidget, xui_rich_edit_data_t* pData,
	xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tContent)
{
	int i;
	int iListNumber = 0;
	for ( i = 0; i < pData->iBlockCount; i++ ) {
		xui_rich_block_layout_t* pBlock = &pData->pBlocks[i];
		xui_rich_node_info_t tInfo;
		xui_rect_t tRect = __xuiRichEditBlockRect(pWidget, pData, pBlock->tRect);
		if ( tRect.fY + tRect.fH < tContent.fY || tRect.fY > tContent.fY + tContent.fH ) continue;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pBlock->pNode, &tInfo) != XUI_OK ) continue;
		if ( (tInfo.tParagraphStyle.iBackgroundColor & 0xffu) != 0 )
			(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect, tInfo.tParagraphStyle.iBackgroundColor);
		if ( tInfo.iType == XUI_RICH_NODE_BLOCK_QUOTE && pProxy->drawLine != NULL )
			(void)pProxy->drawLine(pProxy, pDraw, tRect.fX + 4.0f * pData->fZoom, tRect.fY,
				tRect.fX + 4.0f * pData->fZoom, tRect.fY + tRect.fH, 3.0f * pData->fZoom,
				(tInfo.tParagraphStyle.iBorderColor & 0xffu) != 0 ?
				tInfo.tParagraphStyle.iBorderColor : XUI_COLOR_RGBA(90,120,160,255));
		if ( (tInfo.iType == XUI_RICH_NODE_LIST_ITEM || tInfo.tParagraphStyle.iListType != XUI_RICH_LIST_NONE) && pProxy->drawText != NULL ) {
			char sMarker[24];
			if ( tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NUMBER ) snprintf(sMarker, sizeof(sMarker), "%d.", ++iListNumber);
			else if ( tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_CHECK )
				strcpy(sMarker, tInfo.tParagraphStyle.bListChecked ? "[x]" : "[ ]");
			else strcpy(sMarker, "\xe2\x80\xa2");
			(void)pProxy->drawText(pProxy, pDraw,
				__xuiRichEditSizedFont(pWidget, pData, pData->pFont, 0.0f), sMarker,
				(xui_rect_t){tRect.fX + tInfo.tParagraphStyle.fIndentLeft * pData->fZoom,
					tRect.fY, 22.0f * pData->fZoom, tRect.fH},
				pData->iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		} else iListNumber = 0;
		if ( tInfo.iType == XUI_RICH_NODE_IMAGE ) {
			if ( tInfo.pSurface != NULL && pProxy->drawSurface != NULL && pProxy->surfaceGetDesc != NULL ) {
				xui_surface_desc_t tDesc;
				memset(&tDesc, 0, sizeof(tDesc));
				if ( pProxy->surfaceGetDesc(pProxy, tInfo.pSurface, &tDesc) == XUI_OK )
					(void)pProxy->drawSurface(pProxy, pDraw, tInfo.pSurface,
						(xui_rect_t){0.0f, 0.0f, (float)tDesc.iWidth, (float)tDesc.iHeight}, tRect, 0xffffffffu, 0);
			} else {
				(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect, XUI_COLOR_RGBA(238,241,245,255));
				if ( pProxy->drawRectStroke != NULL ) (void)pProxy->drawRectStroke(pProxy, pDraw, tRect, 1.0f, XUI_COLOR_RGBA(170,180,192,255));
				if ( pProxy->drawText != NULL ) (void)pProxy->drawText(pProxy, pDraw,
					__xuiRichEditSizedFont(pWidget, pData, pData->pFont, 0.0f),
					tInfo.sAltText != NULL ? tInfo.sAltText : "Image", tRect, XUI_COLOR_RGBA(90,100,112,255),
					XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			}
		} else if ( tInfo.iType == XUI_RICH_NODE_TABLE ) {
			float fCellW = tInfo.iColumns > 0 ? tRect.fW / (float)tInfo.iColumns : tRect.fW;
			float fCellH = tInfo.iRows > 0 ? tRect.fH / (float)tInfo.iRows : tRect.fH;
			uint32_t iBorder = (tInfo.iBorderColor & 0xffu) != 0 ? tInfo.iBorderColor : XUI_COLOR_RGBA(170,180,192,255);
			int r, c;
			for ( r = 0; r < tInfo.iRows; r++ ) for ( c = 0; c < tInfo.iColumns; c++ ) {
				xui_rect_t tCell = {tRect.fX + fCellW * c, tRect.fY + fCellH * r, fCellW, fCellH};
				uint32_t iCell = r == 0 ? tInfo.iHeaderColor : tInfo.iCellColor;
				xui_rich_text_style_t tCellStyle;
				const char* sCell = xuiRichTableGetCellText(pBlock->pNode, r, c);
				if ( (iCell & 0xffu) != 0 ) (void)__xuiRichEditDrawRect(pProxy, pDraw, tCell, iCell);
				if ( pProxy->drawRectStroke != NULL ) (void)pProxy->drawRectStroke(pProxy, pDraw, tCell,
					tInfo.fBorderWidth > 0.0f ? tInfo.fBorderWidth * pData->fZoom : 1.0f, iBorder);
				memset(&tCellStyle, 0, sizeof(tCellStyle)); tCellStyle.iSize = sizeof(tCellStyle);
				(void)xuiRichTableGetCellStyle(pBlock->pNode, r, c, &tCellStyle);
				if ( sCell != NULL && pProxy->drawText != NULL ) {
					xui_font pCellFont = __xuiRichEditSizedFont(pWidget, pData,
						tCellStyle.pFont != NULL ? tCellStyle.pFont : pData->pFont, tCellStyle.fFontSize);
					tCell.fX += tInfo.fCellPadding * pData->fZoom; tCell.fY += tInfo.fCellPadding * pData->fZoom;
					tCell.fW -= tInfo.fCellPadding * pData->fZoom * 2.0f;
					tCell.fH -= tInfo.fCellPadding * pData->fZoom * 2.0f;
					(void)pProxy->drawText(pProxy, pDraw,
						pCellFont != NULL ? pCellFont : pData->pFont, sCell, tCell,
						(tCellStyle.iTextColor & 0xffu) != 0 ? tCellStyle.iTextColor : pData->iTextColor,
						XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP |
						((tCellStyle.iFlags & XUI_RICH_STYLE_UNDERLINE) ? XUI_TEXT_UNDERLINE : 0));
				}
			}
		} else if ( tInfo.iType == XUI_RICH_NODE_HORIZONTAL_RULE && pProxy->drawLine != NULL ) {
			uint32_t iColor = (tInfo.tParagraphStyle.iBorderColor & 0xffu) != 0 ? tInfo.tParagraphStyle.iBorderColor : XUI_COLOR_RGBA(170,180,192,255);
			(void)pProxy->drawLine(pProxy, pDraw, tRect.fX, tRect.fY + tRect.fH * 0.5f,
				tRect.fX + tRect.fW, tRect.fY + tRect.fH * 0.5f, 1.0f, iColor);
		}
	}
	return XUI_OK;
}

static int __xuiRichEditRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_proxy pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	xui_rect_t tBounds = xuiWidgetGetContentRect(pWidget);
	xui_rect_t tContent;
	xui_rect_t tRect;
	xui_rich_node_info_t tInfo;
	xui_rich_layout_fragment_t tFragment;
	int iStart;
	int iEnd;
	int i;
	int iRet;
	(void)iStateId; (void)pUser;
	if ( pData == NULL || pProxy == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)__xuiRichEditDrawRect(pProxy, pDraw, tBounds, pData->iBackgroundColor);
	tContent = __xuiRichEditContentRect(pWidget, pData);
	(void)__xuiRichEditRenderBlocks(pWidget, pData, pProxy, pDraw, tContent);
	__xuiRichEditSelection(pData, &iStart, &iEnd);
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rich_layout_fragment_t* pFragment = &tFragment;
		(void)__xuiRichEditGetLayoutFragment(pData, i, pFragment);
		tRect = __xuiRichEditFragmentRect(pWidget, pData, pFragment);
		if ( tRect.fY + tRect.fH < tContent.fY || tRect.fY > tContent.fY + tContent.fH ||
		     tRect.fX + tRect.fW < tContent.fX || tRect.fX > tContent.fX + tContent.fW ) continue;
		(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect, pFragment->iBackgroundColor);
		if ( pData->iFindResultCount > 0 ) {
			int iFind;
			for ( iFind = 0; iFind < pData->iFindResultCount; iFind++ ) {
				xui_find_result_t* pFind = &pData->pFindResults[iFind];
				if ( pFind->iEnd > pFragment->tPublic.iDocumentStart &&
				     pFind->iStart < pFragment->tPublic.iDocumentEnd ) {
					(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect,
						iFind == pData->iFindActiveIndex ? pData->iFindActiveColor : pData->iFindResultColor);
					break;
				}
			}
		}
		if ( pFragment->tPublic.iNodeType == XUI_RICH_NODE_INLINE_IMAGE ) {
			memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
			if ( xuiRichNodeGetInfo(pFragment->pNode, &tInfo) == XUI_OK ) {
				if ( tInfo.pSurface != NULL && pProxy->drawSurface != NULL && pProxy->surfaceGetDesc != NULL ) {
					xui_surface_desc_t tSurfaceDesc;
					memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
					if ( pProxy->surfaceGetDesc(pProxy, tInfo.pSurface, &tSurfaceDesc) == XUI_OK )
						(void)pProxy->drawSurface(pProxy, pDraw, tInfo.pSurface,
							(xui_rect_t){0.0f, 0.0f, (float)tSurfaceDesc.iWidth, (float)tSurfaceDesc.iHeight},
							tRect, 0xffffffffu, 0);
				} else {
					(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect, XUI_COLOR_RGBA(238,241,245,255));
					if ( pProxy->drawRectStroke != NULL ) (void)pProxy->drawRectStroke(pProxy, pDraw, tRect,
						1.0f, XUI_COLOR_RGBA(170,180,192,255));
				}
			}
		}
		if ( iEnd > pFragment->tPublic.iDocumentStart && iStart < pFragment->tPublic.iDocumentEnd )
			(void)__xuiRichEditDrawRect(pProxy, pDraw, tRect, pData->iSelectionColor);
	}
	for ( i = 0; i < pData->iTotalFragmentCount; ) {
		xui_rich_layout_fragment_t tNext;
		xui_rich_layout_fragment_t* pFragment = &tFragment;
		xui_rect_t tNextRect;
		int iRunEnd;
		int iTextEnd;
		(void)__xuiRichEditGetLayoutFragment(pData, i, pFragment);
		if ( pFragment->tPublic.iNodeType != XUI_RICH_NODE_TEXT && pFragment->tPublic.iNodeType != XUI_RICH_NODE_LINK ) { i++; continue; }
		tRect = __xuiRichEditFragmentRect(pWidget, pData, pFragment);
		iRunEnd = i + 1;
		iTextEnd = pFragment->tPublic.iEndOffset;
		while ( iRunEnd < pData->iTotalFragmentCount ) {
			(void)__xuiRichEditGetLayoutFragment(pData, iRunEnd, &tNext);
			tNextRect = __xuiRichEditFragmentRect(pWidget, pData, &tNext);
			if ( tNext.pNode != pFragment->pNode || tNext.pFont != pFragment->pFont ||
			     tNext.iTextColor != pFragment->iTextColor || tNext.iStyleFlags != pFragment->iStyleFlags ||
			     tNext.tPublic.iLine != pFragment->tPublic.iLine ||
			     tNext.tPublic.iStartOffset != iTextEnd ||
			     tNextRect.fX < tRect.fX + tRect.fW - 0.25f ||
			     tNextRect.fX > tRect.fX + tRect.fW + 0.25f ) break;
			iTextEnd = tNext.tPublic.iEndOffset;
			tRect.fW = tNextRect.fX + tNextRect.fW - tRect.fX;
			if ( tNextRect.fH > tRect.fH ) tRect.fH = tNextRect.fH;
			iRunEnd++;
		}
		if ( tRect.fY + tRect.fH < tContent.fY || tRect.fY > tContent.fY + tContent.fH ) { i = iRunEnd; continue; }
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pFragment->pNode, &tInfo) != XUI_OK ) { i = iRunEnd; continue; }
		iRet = __xuiRichEditScratch(pData, iTextEnd - pFragment->tPublic.iStartOffset + 1);
		if ( iRet != XUI_OK ) return iRet;
		memcpy(pData->sScratch, tInfo.sText + pFragment->tPublic.iStartOffset,
			(size_t)(iTextEnd - pFragment->tPublic.iStartOffset));
		pData->sScratch[iTextEnd - pFragment->tPublic.iStartOffset] = 0;
		if ( pProxy->drawText != NULL ) (void)pProxy->drawText(pProxy, pDraw, pFragment->pFont, pData->sScratch,
			tRect, pFragment->iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP |
			((pFragment->iStyleFlags & XUI_RICH_STYLE_UNDERLINE) ? XUI_TEXT_UNDERLINE : 0));
		if ( (pFragment->iStyleFlags & XUI_RICH_STYLE_STRIKEOUT) != 0 && pProxy->drawLine != NULL )
			(void)pProxy->drawLine(pProxy, pDraw, tRect.fX, tRect.fY + tRect.fH * 0.52f,
				tRect.fX + tRect.fW, tRect.fY + tRect.fH * 0.52f, 1.0f, pFragment->iTextColor);
		i = iRunEnd;
	}
		if ( pData->bImeActive && pData->sImeText != NULL && pData->sImeText[0] != 0 && pProxy->drawText != NULL ) {
		tRect = __xuiRichEditCaret(pWidget, pData, pData->iImeStart);
		tRect.fW = tContent.fX + tContent.fW - tRect.fX;
		(void)pProxy->drawText(pProxy, pDraw,
			__xuiRichEditSizedFont(pWidget, pData, pData->pFont, 0.0f), pData->sImeText, tRect, pData->iTextColor,
			XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_UNDERLINE | XUI_TEXT_CLIP);
	}
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget && pProxy->drawRectFill != NULL ) {
		pData->tCursorRect = __xuiRichEditCaret(pWidget, pData, pData->bImeActive ? pData->iImeStart : pData->iCaret);
		(void)__xuiRichEditDrawRect(pProxy, pDraw, pData->tCursorRect, pData->iCursorColor);
	}
	if ( pData->fBorderWidth > 0.0f && pProxy->drawRectStroke != NULL ) {
		uint32_t iBorder = xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ? pData->iFocusBorderColor : pData->iBorderColor;
		tRect = xuiInternalStrokeCenterRectInside(tBounds, pData->fBorderWidth, NULL);
		(void)pProxy->drawRectStroke(pProxy, pDraw, tRect, pData->fBorderWidth, iBorder);
	}
	return XUI_OK;
}

static int __xuiRichEditArrangeChildren(xui_widget pWidget, xui_rect_t tContent, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rich_node_info_t tInfo;
	xui_rich_layout_fragment_t tFragment;
	xui_rect_t tEditorContent;
	int i;
	(void)tContent; (void)pUser;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiRichEditUpdateScrollModel(pWidget, pData) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	tEditorContent = __xuiRichEditContentRect(pWidget, pData);
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rich_layout_fragment_t* pFragment = &tFragment;
		(void)__xuiRichEditGetLayoutFragment(pData, i, pFragment);
		xui_rect_t tRect;
		if ( pFragment->tPublic.iNodeType != XUI_RICH_NODE_INLINE_WIDGET ) continue;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pFragment->pNode, &tInfo) != XUI_OK || tInfo.pWidget == NULL ) continue;
		if ( xuiWidgetGetParent(tInfo.pWidget) != pWidget ) continue;
		tRect = pFragment->tPublic.tRect;
		tRect.fX += tEditorContent.fX - pData->fScrollX;
		tRect.fY += tEditorContent.fY - pData->fScrollY;
		if ( xuiLayoutArrangeChild(pWidget, tInfo.pWidget, tRect) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pCellEditor != NULL ) {
		xui_rich_node pTable = xuiRichDocumentFindNode(pData->pDocument, pData->iActiveTableId);
		xui_rich_node_info_t tTableInfo;
		for ( i = 0; pTable != NULL && i < pData->iBlockCount; i++ ) if ( pData->pBlocks[i].pNode == pTable ) {
			xui_rect_t tRect = __xuiRichEditBlockRect(pWidget, pData, pData->pBlocks[i].tRect);
			memset(&tTableInfo, 0, sizeof(tTableInfo)); tTableInfo.iSize = sizeof(tTableInfo);
			if ( xuiRichNodeGetInfo(pTable, &tTableInfo) != XUI_OK || tTableInfo.iRows <= 0 || tTableInfo.iColumns <= 0 ) break;
			tRect.fX += tRect.fW * (float)pData->iActiveCellColumn / (float)tTableInfo.iColumns;
			tRect.fY += tRect.fH * (float)pData->iActiveCellRow / (float)tTableInfo.iRows;
			tRect.fW /= (float)tTableInfo.iColumns;
			tRect.fH /= (float)tTableInfo.iRows;
			if ( xuiLayoutArrangeChild(pWidget, pData->pCellEditor, tRect) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
			break;
		}
	}
	return XUI_OK;
}

static int __xuiRichEditPrepareLayout(xui_widget pWidget, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pUser;
	int iRet;
	if ( pData == NULL || pData->pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditSyncWidgets(pWidget, pData, pData->pDocument, 1);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiRichEditUpdateScrollModel(pWidget, pData);
}

static int __xuiRichEditMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget; (void)tConstraint; (void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 420.0f;
	pSize->fY = 240.0f;
	return XUI_OK;
}

static int __xuiRichEditMove(xui_widget pWidget, xui_rich_edit_data_t* pData, int iOffset, int bExtend)
{
	int iLength = xuiRichDocumentGetLength(pData->pDocument);
	const char* sText = xuiRichDocumentGetText(pData->pDocument);
	if ( iOffset < 0 ) iOffset = 0;
	if ( iOffset > iLength ) iOffset = iLength;
	iOffset = xuiInternalTextGraphemeClamp(sText, iLength, iOffset);
	pData->iCaret = iOffset;
	if ( !bExtend ) pData->iAnchor = iOffset;
	pData->bTypingStyleOverride = 0;
	pData->bPreferredCaretX = 0;
	__xuiRichEditEnsureCaretVisible(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiRichEditWordPrev(const char* sText, int iLength, int iOffset)
{
	int i = xuiInternalTextGraphemePrev(sText, iLength, iOffset);
	int iClass;
	int iPrev;
	while ( i > 0 && isspace((unsigned char)sText[i]) ) i = xuiInternalTextGraphemePrev(sText, iLength, i);
	iClass = ((unsigned char)sText[i] >= 0x80u) ? 3 :
		(isalnum((unsigned char)sText[i]) || sText[i] == '_' ? 1 : 2);
	while ( i > 0 ) {
		iPrev = xuiInternalTextGraphemePrev(sText, iLength, i);
		if ( isspace((unsigned char)sText[iPrev]) ) break;
		if ( ((unsigned char)sText[iPrev] >= 0x80u ? 3 :
		     (isalnum((unsigned char)sText[iPrev]) || sText[iPrev] == '_' ? 1 : 2)) != iClass ) break;
		if ( iClass == 3 ) break;
		i = iPrev;
	}
	return i;
}

static int __xuiRichEditWordNext(const char* sText, int iLength, int iOffset)
{
	int i = iOffset;
	int iClass;
	if ( i >= iLength ) return iLength;
	iClass = ((unsigned char)sText[i] >= 0x80u) ? 3 :
		(isalnum((unsigned char)sText[i]) || sText[i] == '_' ? 1 : 2);
	while ( i < iLength && !isspace((unsigned char)sText[i]) ) {
		int iCurrentClass = ((unsigned char)sText[i] >= 0x80u) ? 3 :
			(isalnum((unsigned char)sText[i]) || sText[i] == '_' ? 1 : 2);
		if ( iCurrentClass != iClass ) break;
		i = xuiInternalTextGraphemeNext(sText, iLength, i);
		if ( iClass == 3 ) break;
	}
	while ( i < iLength && isspace((unsigned char)sText[i]) ) i = xuiInternalTextGraphemeNext(sText, iLength, i);
	return i;
}

static void __xuiRichEditSelectWord(xui_rich_edit_data_t* pData, int iOffset)
{
	const char* sText = xuiRichDocumentGetText(pData->pDocument);
	int iLength = xuiRichDocumentGetLength(pData->pDocument);
	int iStart;
	int iEnd;
	int iClass;
	if ( iLength <= 0 ) { pData->iAnchor = pData->iCaret = 0; return; }
	if ( iOffset >= iLength ) iOffset = xuiInternalTextGraphemePrev(sText, iLength, iLength);
	iOffset = xuiInternalTextGraphemeClamp(sText, iLength, iOffset);
	if ( isspace((unsigned char)sText[iOffset]) ) {
		iStart = iOffset;
		iEnd = xuiInternalTextGraphemeNext(sText, iLength, iOffset);
		while ( iStart > 0 ) {
			int iPrev = xuiInternalTextGraphemePrev(sText, iLength, iStart);
			if ( !isspace((unsigned char)sText[iPrev]) || sText[iPrev] == '\n' || sText[iPrev] == '\r' ) break;
			iStart = iPrev;
		}
		while ( iEnd < iLength && isspace((unsigned char)sText[iEnd]) && sText[iEnd] != '\n' && sText[iEnd] != '\r' )
			iEnd = xuiInternalTextGraphemeNext(sText, iLength, iEnd);
	} else {
		iClass = ((unsigned char)sText[iOffset] >= 0x80u) ? 3 :
			(isalnum((unsigned char)sText[iOffset]) || sText[iOffset] == '_' ? 1 : 2);
		iStart = iOffset;
		iEnd = xuiInternalTextGraphemeNext(sText, iLength, iOffset);
		if ( iClass != 3 ) {
			while ( iStart > 0 ) {
				int iPrev = xuiInternalTextGraphemePrev(sText, iLength, iStart);
				int iPrevClass = ((unsigned char)sText[iPrev] >= 0x80u) ? 3 :
					(isalnum((unsigned char)sText[iPrev]) || sText[iPrev] == '_' ? 1 : 2);
				if ( iPrevClass != iClass ) break;
				iStart = iPrev;
			}
			while ( iEnd < iLength ) {
				int iEndClass = ((unsigned char)sText[iEnd] >= 0x80u) ? 3 :
					(isalnum((unsigned char)sText[iEnd]) || sText[iEnd] == '_' ? 1 : 2);
				if ( iEndClass != iClass ) break;
				iEnd = xuiInternalTextGraphemeNext(sText, iLength, iEnd);
			}
		}
	}
	pData->iAnchor = iStart;
	pData->iCaret = iEnd;
}

static int __xuiRichEditLineMove(xui_widget pWidget, xui_rich_edit_data_t* pData, int iDirection, int bExtend)
{
	xui_rect_t tCaret;
	xui_rich_layout_fragment_t tFragment;
	int iCurrentLine = 0;
	int iTargetLine;
	int iMaxLine = 0;
	int iBest = pData->iCaret;
	float fBest = 3.402823466e+38F;
	int i;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	tCaret = __xuiRichEditCaret(pWidget, pData, pData->iCaret);
	if ( !pData->bPreferredCaretX ) { pData->fPreferredCaretX = tCaret.fX; pData->bPreferredCaretX = 1; }
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( tFragment.tPublic.iLine > iMaxLine ) iMaxLine = tFragment.tPublic.iLine;
		if ( pData->iCaret >= tFragment.tPublic.iDocumentStart && pData->iCaret <= tFragment.tPublic.iDocumentEnd )
			iCurrentLine = tFragment.tPublic.iLine;
	}
	iTargetLine = iCurrentLine + iDirection;
	if ( iTargetLine < 0 ) iTargetLine = 0;
	if ( iTargetLine > iMaxLine ) iTargetLine = iMaxLine;
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rect_t r;
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( tFragment.tPublic.iLine != iTargetLine ) continue;
		r = __xuiRichEditFragmentRect(pWidget, pData, &tFragment);
		float d0 = r.fX > pData->fPreferredCaretX ? r.fX - pData->fPreferredCaretX : pData->fPreferredCaretX - r.fX;
		float d1 = r.fX + r.fW > pData->fPreferredCaretX ? r.fX + r.fW - pData->fPreferredCaretX : pData->fPreferredCaretX - r.fX - r.fW;
		if ( d0 < fBest ) { fBest = d0; iBest = tFragment.tPublic.iDocumentStart; }
		if ( d1 < fBest ) { fBest = d1; iBest = tFragment.tPublic.iDocumentEnd; }
	}
	{
		int iLength = xuiRichDocumentGetLength(pData->pDocument);
		const char* sText = xuiRichDocumentGetText(pData->pDocument);
		if ( iBest < 0 ) iBest = 0;
		if ( iBest > iLength ) iBest = iLength;
		pData->iCaret = xuiInternalTextGraphemeClamp(sText, iLength, iBest);
		if ( !bExtend ) pData->iAnchor = pData->iCaret;
	}
	__xuiRichEditEnsureCaretVisible(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiRichEditLineBounds(xui_widget pWidget, xui_rich_edit_data_t* pData, int* pStart, int* pEnd)
{
	xui_rich_layout_fragment_t tFragment;
	int iLine = 0;
	int i;
	*pStart = *pEnd = pData->iCaret;
	(void)__xuiRichEditEnsureLayout(pWidget, pData);
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( pData->iCaret >= tFragment.tPublic.iDocumentStart && pData->iCaret <= tFragment.tPublic.iDocumentEnd ) { iLine = tFragment.tPublic.iLine; break; }
	}
	for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( tFragment.tPublic.iLine != iLine ) continue;
		if ( *pStart == pData->iCaret || tFragment.tPublic.iDocumentStart < *pStart ) *pStart = tFragment.tPublic.iDocumentStart;
		if ( tFragment.tPublic.iDocumentEnd > *pEnd ) *pEnd = tFragment.tPublic.iDocumentEnd;
	}
}

static int __xuiRichEditReplaceSelection(xui_widget pWidget, xui_rich_edit_data_t* pData, const char* sText)
{
	xui_rich_text_style_t tStyle;
	int iStart;
	int iEnd;
	int iRet;
	__xuiRichEditSelection(pData, &iStart, &iEnd);
	tStyle = pData->tTypingStyle;
	if ( !pData->bTypingStyleOverride ) (void)__xuiRichEditStyleAt(pData, iStart, &tStyle);
	iRet = xuiRichDocumentReplace(pData->pDocument, iStart, iEnd, sText, &tStyle);
	if ( iRet != XUI_OK ) return iRet;
	pData->iCaret = iStart + (int)strlen(sText);
	pData->iAnchor = pData->iCaret;
	__xuiRichEditEnsureCaretVisible(pWidget, pData);
	return XUI_OK;
}

static int __xuiRichEditCopyRange(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_proxy pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	const char* sText = xuiRichDocumentGetText(pData->pDocument);
	xui_rich_document pFragment = NULL;
	xui_clipboard_item_t arrItems[3];
	char* sNative = NULL;
	char* sHtml = NULL;
	size_t iNativeSize = 0u;
	size_t iHtmlSize = 0u;
	int iItemCount = 1;
	int iStart;
	int iEnd;
	int iRet;
	__xuiRichEditSelection(pData, &iStart, &iEnd);
	if ( iStart == iEnd || pProxy == NULL || pProxy->clipboardSetText == NULL ) return XUI_OK;
	iRet = __xuiRichEditScratch(pData, iEnd - iStart + 1);
	if ( iRet != XUI_OK ) return iRet;
	memcpy(pData->sScratch, sText + iStart, (size_t)(iEnd - iStart));
	pData->sScratch[iEnd - iStart] = 0;
	if ( pProxy->clipboardSetItems == NULL ) return pProxy->clipboardSetText(pProxy, pData->sScratch);
	memset(arrItems, 0, sizeof(arrItems));
	arrItems[0].sFormat = XUI_CLIPBOARD_FORMAT_TEXT_UTF8;
	arrItems[0].pData = pData->sScratch;
	arrItems[0].iDataSize = (size_t)(iEnd - iStart);
	if ( xuiRichDocumentCloneRange(pData->pDocument, iStart, iEnd, &pFragment) == XUI_OK ) {
		if ( xuiRichDocumentSerialize(pFragment, 0, &sNative, &iNativeSize) == XUI_OK ) {
			arrItems[iItemCount].sFormat = XUI_CLIPBOARD_FORMAT_RICH_DOCUMENT;
			arrItems[iItemCount].pData = sNative;
			arrItems[iItemCount].iDataSize = iNativeSize;
			iItemCount++;
		}
		if ( xuiRichDocumentExportHtml(pFragment, &sHtml, &iHtmlSize) == XUI_OK ) {
			arrItems[iItemCount].sFormat = XUI_CLIPBOARD_FORMAT_HTML;
			arrItems[iItemCount].pData = sHtml;
			arrItems[iItemCount].iDataSize = iHtmlSize;
			iItemCount++;
		}
	}
	iRet = pProxy->clipboardSetItems(pProxy, arrItems, iItemCount);
	if ( sNative != NULL ) xuiRichDocumentFreeSerialized(sNative);
	if ( sHtml != NULL ) xuiRichDocumentFreeSerialized(sHtml);
	if ( pFragment != NULL ) xuiRichDocumentDestroy(pFragment);
	return iRet;
}

static int __xuiRichEditUpdateMenu(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	static const int arrCommands[] = {
		XUI_RICH_COMMAND_UNDO, XUI_RICH_COMMAND_REDO, 0,
		XUI_RICH_COMMAND_CUT, XUI_RICH_COMMAND_COPY, XUI_RICH_COMMAND_PASTE,
		XUI_RICH_COMMAND_DELETE, 0, XUI_RICH_COMMAND_SELECT_ALL, 0,
		XUI_RICH_COMMAND_FIND, XUI_RICH_COMMAND_REPLACE
	};
	static const char* arrShortcuts[] = { "Ctrl+Z", "Ctrl+Y", NULL, "Ctrl+X", "Ctrl+C", "Ctrl+V", NULL, NULL, "Ctrl+A", NULL, "Ctrl+F", "Ctrl+H" };
	xui_menu_item_t arrItems[sizeof(arrCommands) / sizeof(arrCommands[0])];
	int i;
	if ( pWidget == NULL || pData == NULL || pData->pMenu == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrItems, 0, sizeof(arrItems));
	for ( i = 0; i < (int)(sizeof(arrCommands) / sizeof(arrCommands[0])); i++ ) {
		int iState = XUI_RICH_COMMAND_STATE_DISABLED;
		if ( arrCommands[i] == 0 ) { arrItems[i].iType = XUI_MENU_ITEM_SEPARATOR; continue; }
		(void)xuiRichEditQueryCommand(pWidget, arrCommands[i], &iState);
		arrItems[i].sText = xuiTranslate(xuiWidgetGetContext(pWidget), __xuiRichEditMenuTextId(arrCommands[i]));
		arrItems[i].sShortcut = arrShortcuts[i];
		arrItems[i].iType = XUI_MENU_ITEM_NORMAL;
		arrItems[i].iState = iState != XUI_RICH_COMMAND_STATE_DISABLED ? XUI_MENU_ITEM_ENABLED : 0u;
		arrItems[i].iValue = arrCommands[i];
	}
	return xuiMenuSetItems(pData->pMenu, arrItems, (int)(sizeof(arrCommands) / sizeof(arrCommands[0])));
}

static void __xuiRichEditMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	(void)pMenu;
	(void)iIndex;
	if ( pWidget == NULL ) return;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	(void)xuiRichEditExecuteCommand(pWidget, iValue, NULL);
}

static int __xuiRichEditInitMenu(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pMenu, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiRichEditMenuSelect, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiRichEditUpdateMenu(pWidget, pData);
}

static int __xuiRichEditBlockRange(xui_rich_edit_data_t* pData, xui_rich_node pBlock,
	int* pStart, int* pEnd)
{
	xui_rich_node pNext;
	int iStart;
	int iEnd;
	if ( pData == NULL || pBlock == NULL || pStart == NULL || pEnd == NULL ||
	     xuiInternalRichDocumentNodeOffset(pData->pDocument, pBlock, &iStart) != XUI_OK )
		return XUI_ERROR_INVALID_ARGUMENT;
	pNext = xuiRichNodeGetNextSibling(pBlock);
	if ( pNext != NULL ) {
		if ( xuiInternalRichDocumentNodeOffset(pData->pDocument, pNext, &iEnd) != XUI_OK )
			return XUI_ERROR_INVALID_ARGUMENT;
		if ( iEnd > iStart ) iEnd--;
	} else iEnd = xuiRichDocumentGetLength(pData->pDocument);
	*pStart = iStart;
	*pEnd = iEnd;
	return XUI_OK;
}

static int __xuiRichEditListLevel(xui_rich_edit_data_t* pData, int iDelta)
{
	xui_rich_node pBlock;
	xui_rich_node_info_t tInfo;
	xui_rich_paragraph_style_t tStyle;
	int iStart;
	int iEnd;
	if ( __xuiRichEditParagraphAt(pData, pData->iCaret, &pBlock, &tInfo) != XUI_OK ||
	     tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NONE ) return XUI_ERROR_UNSUPPORTED;
	tStyle = tInfo.tParagraphStyle; tStyle.iSize = sizeof(tStyle);
	tStyle.iListLevel += iDelta;
	if ( tStyle.iListLevel < 0 ) tStyle.iListLevel = 0;
	if ( tStyle.iListLevel > 16 ) tStyle.iListLevel = 16;
	if ( __xuiRichEditBlockRange(pData, pBlock, &iStart, &iEnd) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiRichDocumentApplyParagraphStyle(pData->pDocument, iStart, iEnd, &tStyle);
}

static int __xuiRichEditToggleCheckAt(xui_widget pWidget, xui_rich_edit_data_t* pData,
	float fLocalX, float fLocalY)
{
	int i;
	if ( pData->bReadonly || __xuiRichEditEnsureLayout(pWidget, pData) != XUI_OK ) return 0;
	for ( i = 0; i < pData->iBlockCount; i++ ) {
		xui_rich_block_layout_t* pBlock = &pData->pBlocks[i];
		xui_rich_node_info_t tInfo;
		xui_rich_paragraph_style_t tStyle;
		xui_rect_t tRect;
		int iStart;
		int iEnd;
		if ( pBlock->iType != XUI_RICH_NODE_LIST_ITEM ) continue;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pBlock->pNode, &tInfo) != XUI_OK ||
		     tInfo.tParagraphStyle.iListType != XUI_RICH_LIST_CHECK ) continue;
		tRect = __xuiRichEditBlockRect(pWidget, pData, pBlock->tRect);
		if ( fLocalY < tRect.fY || fLocalY >= tRect.fY + tRect.fH ||
		     fLocalX < tRect.fX + tInfo.tParagraphStyle.fIndentLeft * pData->fZoom ||
		     fLocalX >= tRect.fX + (tInfo.tParagraphStyle.fIndentLeft + 22.0f) * pData->fZoom ) continue;
		if ( __xuiRichEditBlockRange(pData, pBlock->pNode, &iStart, &iEnd) != XUI_OK ) return 0;
		tStyle = tInfo.tParagraphStyle; tStyle.iSize = sizeof(tStyle);
		tStyle.bListChecked = !tStyle.bListChecked;
		if ( xuiRichDocumentApplyParagraphStyle(pData->pDocument, iStart, iEnd, &tStyle) == XUI_OK ) {
			pData->iAnchor = pData->iCaret = iStart;
			return 1;
		}
		return 0;
	}
	return 0;
}

static int __xuiRichEditListEnter(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_rich_node pBlock;
	xui_rich_node_info_t tInfo;
	xui_rich_paragraph_style_t tStyle;
	int iStart;
	int iEnd;
	int iSelectionStart;
	int iSelectionEnd;
	int iRet;
	__xuiRichEditSelection(pData, &iSelectionStart, &iSelectionEnd);
	if ( iSelectionStart != iSelectionEnd ||
	     __xuiRichEditParagraphAt(pData, pData->iCaret, &pBlock, &tInfo) != XUI_OK ||
	     tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NONE )
		return __xuiRichEditReplaceSelection(pWidget, pData, "\n");
	if ( __xuiRichEditBlockRange(pData, pBlock, &iStart, &iEnd) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	tStyle = tInfo.tParagraphStyle; tStyle.iSize = sizeof(tStyle);
	if ( iStart == iEnd ) {
		tStyle.iListType = XUI_RICH_LIST_NONE;
		tStyle.iListLevel = 0;
		tStyle.bListChecked = 0;
		iRet = xuiRichDocumentBeginTransaction(pData->pDocument);
		if ( iRet == XUI_OK ) iRet = xuiRichDocumentSetBlockType(pData->pDocument, iStart, iEnd, XUI_RICH_NODE_PARAGRAPH);
		if ( iRet == XUI_OK ) iRet = xuiRichDocumentApplyParagraphStyle(pData->pDocument, iStart, iEnd, &tStyle);
		(void)xuiRichDocumentEndTransaction(pData->pDocument);
		return iRet;
	}
	tStyle.bListChecked = 0;
	iRet = xuiRichDocumentBeginTransaction(pData->pDocument);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditReplaceSelection(pWidget, pData, "\n");
	if ( iRet == XUI_OK ) iRet = xuiRichDocumentSetBlockType(pData->pDocument, pData->iCaret, pData->iCaret, XUI_RICH_NODE_LIST_ITEM);
	if ( iRet == XUI_OK ) iRet = xuiRichDocumentApplyParagraphStyle(pData->pDocument, pData->iCaret, pData->iCaret, &tStyle);
	(void)xuiRichDocumentEndTransaction(pData->pDocument);
	return iRet;
}

static int __xuiRichEditHandleKey(xui_widget pWidget, xui_rich_edit_data_t* pData, const xui_event_t* pEvent)
{
	const char* sText = xuiRichDocumentGetText(pData->pDocument);
	int iLength = xuiRichDocumentGetLength(pData->pDocument);
	int iStart;
	int iEnd;
	int iKey = pEvent->iKey;
	int bShift = (pEvent->iModifiers & XUI_MOD_SHIFT) != 0;
	int bCtrl = (pEvent->iModifiers & XUI_MOD_CTRL) != 0;
	if ( bCtrl ) {
		iKey = toupper((unsigned char)iKey);
		if ( iKey == 'A' ) { (void)xuiRichEditSelectAll(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'C' ) { (void)xuiRichEditCopy(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'X' ) { (void)xuiRichEditCut(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'V' ) { (void)xuiRichEditPaste(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'Z' ) { (void)(bShift ? xuiRichEditRedo(pWidget) : xuiRichEditUndo(pWidget)); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'Y' ) { (void)xuiRichEditRedo(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'B' ) { (void)xuiRichEditExecuteCommand(pWidget, XUI_RICH_COMMAND_BOLD, NULL); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'I' ) { (void)xuiRichEditExecuteCommand(pWidget, XUI_RICH_COMMAND_ITALIC, NULL); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'U' ) { (void)xuiRichEditExecuteCommand(pWidget, XUI_RICH_COMMAND_UNDERLINE, NULL); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'F' ) { (void)xuiRichEditOpenFind(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == 'H' ) { (void)xuiRichEditOpenReplace(pWidget); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == '+' || iKey == '=' ) { (void)xuiRichEditSetZoom(pWidget, pData->fZoom * 1.1f); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == '-' || iKey == '_' ) { (void)xuiRichEditSetZoom(pWidget, pData->fZoom / 1.1f); return XUI_EVENT_DISPATCH_STOP; }
		if ( iKey == '0' ) { (void)xuiRichEditSetZoom(pWidget, 1.0f); return XUI_EVENT_DISPATCH_STOP; }
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_F3:
		(void)(bShift ? xuiRichEditFindPrevious(pWidget, NULL) : xuiRichEditFindNext(pWidget, NULL));
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		(void)__xuiRichEditMove(pWidget, pData, iStart != iEnd && !bShift ? iStart :
			(bCtrl ? __xuiRichEditWordPrev(sText, iLength, pData->iCaret) : xuiInternalTextGraphemePrev(sText, iLength, pData->iCaret)), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		(void)__xuiRichEditMove(pWidget, pData, iStart != iEnd && !bShift ? iEnd :
			(bCtrl ? __xuiRichEditWordNext(sText, iLength, pData->iCaret) : xuiInternalTextGraphemeNext(sText, iLength, pData->iCaret)), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP: return __xuiRichEditLineMove(pWidget, pData, -1, bShift), XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN: return __xuiRichEditLineMove(pWidget, pData, 1, bShift), XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_UP: return __xuiRichEditLineMove(pWidget, pData, -10, bShift), XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN: return __xuiRichEditLineMove(pWidget, pData, 10, bShift), XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		if ( bCtrl ) iStart = 0; else __xuiRichEditLineBounds(pWidget, pData, &iStart, &iEnd);
		(void)__xuiRichEditMove(pWidget, pData, iStart, bShift); return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		if ( bCtrl ) iEnd = iLength; else __xuiRichEditLineBounds(pWidget, pData, &iStart, &iEnd);
		(void)__xuiRichEditMove(pWidget, pData, iEnd, bShift); return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ENTER:
		if ( !pData->bReadonly ) (void)__xuiRichEditListEnter(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_TAB:
		if ( !pData->bReadonly && __xuiRichEditListLevel(pData, bShift ? -1 : 1) == XUI_OK )
			return XUI_EVENT_DISPATCH_STOP;
		return XUI_OK;
	case XUI_KEY_BACKSPACE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		if ( iStart == iEnd && iStart > 0 ) iStart = bCtrl ? __xuiRichEditWordPrev(sText, iLength, iStart) :
			xuiInternalTextGraphemePrev(sText, iLength, iStart);
		pData->iAnchor = iStart; pData->iCaret = iEnd;
		(void)__xuiRichEditReplaceSelection(pWidget, pData, "");
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DELETE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		if ( iStart == iEnd && iEnd < iLength ) iEnd = bCtrl ? __xuiRichEditWordNext(sText, iLength, iEnd) :
			xuiInternalTextGraphemeNext(sText, iLength, iEnd);
		pData->iAnchor = iStart; pData->iCaret = iEnd;
		(void)__xuiRichEditReplaceSelection(pWidget, pData, "");
		return XUI_EVENT_DISPATCH_STOP;
	default: return XUI_OK;
	}
}

static int __xuiRichEditImeText(xui_rich_edit_data_t* pData, const char* sText, int iSize)
{
	char* sNew;
	if ( iSize < 0 ) iSize = sText != NULL ? (int)strlen(sText) : 0;
	if ( iSize + 1 > pData->iImeCapacity ) {
		sNew = (char*)xrtRealloc(pData->sImeText, (size_t)iSize + 1u);
		if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pData->sImeText = sNew;
		pData->iImeCapacity = iSize + 1;
	}
	if ( iSize > 0 && sText != NULL ) memcpy(pData->sImeText, sText, (size_t)iSize);
	pData->sImeText[iSize] = 0;
	return XUI_OK;
}

static void __xuiRichEditImeReset(xui_rich_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->bImeActive = 0;
	pData->iImeStart = pData->iCaret;
	pData->iImeEnd = pData->iCaret;
	pData->iImeCursor = 0;
	if ( pData->sImeText != NULL ) pData->sImeText[0] = 0;
}

static int __xuiRichEditAutoScrollPointer(xui_widget pWidget, xui_rich_edit_data_t* pData,
	const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fDeltaX = 0.0f;
	float fDeltaY = 0.0f;
	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = __xuiRichEditContentRect(pWidget, pData);
	fLeft = tWorld.fX + tContent.fX;
	fTop = tWorld.fY + tContent.fY;
	fRight = fLeft + tContent.fW;
	fBottom = fTop + tContent.fH;
	if ( pEvent->fX < fLeft ) fDeltaX = pEvent->fX - fLeft;
	else if ( pEvent->fX > fRight ) fDeltaX = pEvent->fX - fRight;
	if ( pEvent->fY < fTop ) fDeltaY = pEvent->fY - fTop;
	else if ( pEvent->fY > fBottom ) fDeltaY = pEvent->fY - fBottom;
	if ( fDeltaX == 0.0f && fDeltaY == 0.0f ) return XUI_OK;
	return xuiRichEditSetScroll(pWidget, pData->fScrollX + fDeltaX, pData->fScrollY + fDeltaY);
}

static int __xuiRichEditEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_context pContext = xuiWidgetGetContext(pWidget);
	xui_rect_t tWorld;
	char sUtf8[8];
	int iSize = 0;
	int iStart;
	int iEnd;
	(void)pUser;
	if ( pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_BLUR:
		__xuiRichEditImeReset(pData);
		pData->pPressedLink = NULL;
		pData->pPressedObject = NULL;
		pData->bDragging = 0;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_FOCUS:
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			if ( pData->pCellEditor != NULL ) (void)__xuiRichEditEndCellEdit(pWidget, pData, 0);
			if ( pData->bImeActive ) __xuiRichEditImeReset(pData);
			(void)xuiSetFocusWidget(pContext, pWidget);
			tWorld = xuiWidgetGetWorldRect(pWidget);
			if ( __xuiRichEditToggleCheckAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY) ) {
				(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
			(void)xuiSetPointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType, pWidget);
			pData->pPressedObject = NULL;
			if ( (pEvent->iModifiers & XUI_MOD_SHIFT) == 0 &&
			     __xuiRichEditAtomicAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY,
				&iStart, &iEnd, &pData->pPressedObject) ) {
				pData->iAnchor = iStart;
				pData->iCaret = iEnd;
			} else {
				pData->iCaret = __xuiRichEditHit(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
				if ( (pEvent->iModifiers & XUI_MOD_SHIFT) == 0 ) pData->iAnchor = pData->iCaret;
			}
			pData->pPressedLink = __xuiRichEditLinkAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			pData->fPressX = pEvent->fX;
			pData->fPressY = pEvent->fY;
			pData->bPointerMoved = 0;
			pData->bDragging = 1;
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			xui_rich_node pTable = NULL;
			int iRow;
			int iColumn;
			xui_rich_node_info_t tTableInfo;
			tWorld = xuiWidgetGetWorldRect(pWidget);
			if ( __xuiRichEditTableCellAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY,
				&pTable, &iRow, &iColumn, NULL) ) {
				memset(&tTableInfo, 0, sizeof(tTableInfo)); tTableInfo.iSize = sizeof(tTableInfo);
				if ( xuiRichNodeGetInfo(pTable, &tTableInfo) == XUI_OK )
					(void)xuiRichEditBeginTableCellEdit(pWidget, tTableInfo.iId, iRow, iColumn);
				return XUI_EVENT_DISPATCH_STOP;
			}
			iStart = __xuiRichEditHit(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			__xuiRichEditSelectWord(pData, iStart);
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		if ( pData->bDragging ) {
			float fDX = pEvent->fX - pData->fPressX;
			float fDY = pEvent->fY - pData->fPressY;
			if ( fDX * fDX + fDY * fDY > 9.0f ) pData->bPointerMoved = 1;
			(void)__xuiRichEditAutoScrollPointer(pWidget, pData, pEvent);
			tWorld = xuiWidgetGetWorldRect(pWidget);
			pData->iCaret = __xuiRichEditHit(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( pData->pPressedLink != NULL && !pData->bPointerMoved && pData->onLinkClick != NULL ) {
			xui_rich_node pLink;
			xui_rich_node_info_t tInfo;
			tWorld = xuiWidgetGetWorldRect(pWidget);
			pLink = __xuiRichEditLinkAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
			if ( pLink == pData->pPressedLink && xuiRichNodeGetInfo(pLink, &tInfo) == XUI_OK )
				pData->onLinkClick(pWidget, tInfo.iId, tInfo.sResource, pData->pLinkClickUser);
		}
		if ( pData->pPressedObject != NULL && !pData->bPointerMoved ) {
			xui_rich_node pObject = NULL;
			tWorld = xuiWidgetGetWorldRect(pWidget);
			(void)__xuiRichEditAtomicAt(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY,
				NULL, NULL, &pObject);
			if ( pObject == pData->pPressedObject )
				__xuiRichEditEmit(pWidget, pData, XUI_RICH_EDIT_EVENT_OBJECT_ACTIVATE, pObject);
		}
		pData->pPressedLink = NULL;
		pData->pPressedObject = NULL;
		pData->bDragging = 0;
		(void)xuiReleasePointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType, pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST: pData->pPressedLink = NULL; pData->pPressedObject = NULL; pData->bDragging = 0; return XUI_OK;
	case XUI_EVENT_POINTER_WHEEL:
		if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 ) {
			float fDelta = pEvent->fWheelY != 0.0f ? pEvent->fWheelY : -pEvent->fWheelX;
			if ( fDelta != 0.0f ) (void)xuiRichEditSetZoom(pWidget,
				pData->fZoom * (fDelta > 0.0f ? 1.1f : (1.0f / 1.1f)));
			return XUI_EVENT_DISPATCH_STOP;
		}
		(void)xuiRichEditSetScroll(pWidget, pData->fScrollX - pEvent->fWheelX * XUI_RICH_EDIT_SCROLL_STEP,
			pData->fScrollY - pEvent->fWheelY * XUI_RICH_EDIT_SCROLL_STEP);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_CONTEXT_MENU:
		(void)xuiSetFocusWidget(pContext, pWidget);
		pData->pPressedLink = NULL;
		pData->pPressedObject = NULL;
		pData->bDragging = 0;
		(void)xuiReleasePointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType, pWidget);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
			xui_rect_t tCaret = __xuiRichEditCaret(pWidget, pData, pData->iCaret);
			return xuiRichEditOpenMenu(pWidget, tWorld.fX + tCaret.fX, tWorld.fY + tCaret.fY + tCaret.fH) == XUI_OK ?
				XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		{
			int iHit = __xuiRichEditHit(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			if ( iStart == iEnd || iHit < iStart || iHit > iEnd ) pData->iAnchor = pData->iCaret = iHit;
		}
		return xuiRichEditOpenMenu(pWidget, pEvent->fX, pEvent->fY) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_KEY_DOWN: return __xuiRichEditHandleKey(pWidget, pData, pEvent);
	case XUI_EVENT_TEXT:
		if ( pData->bReadonly || (pEvent->iModifiers & XUI_MOD_CTRL) != 0 ) return XUI_EVENT_DISPATCH_STOP;
		if ( pEvent->iTextSize > 0 ) { (void)xuiRichEditInsertText(pWidget, pEvent->sText); return XUI_EVENT_DISPATCH_STOP; }
		if ( pEvent->iCodepoint >= 32u ) {
			uint32_t c = pEvent->iCodepoint;
			if ( c < 0x80u ) sUtf8[iSize++] = (char)c;
			else if ( c < 0x800u ) { sUtf8[iSize++] = (char)(0xc0u | (c >> 6)); sUtf8[iSize++] = (char)(0x80u | (c & 0x3fu)); }
			else if ( c < 0x10000u ) { sUtf8[iSize++] = (char)(0xe0u | (c >> 12)); sUtf8[iSize++] = (char)(0x80u | ((c >> 6) & 0x3fu)); sUtf8[iSize++] = (char)(0x80u | (c & 0x3fu)); }
			else { sUtf8[iSize++] = (char)(0xf0u | (c >> 18)); sUtf8[iSize++] = (char)(0x80u | ((c >> 12) & 0x3fu)); sUtf8[iSize++] = (char)(0x80u | ((c >> 6) & 0x3fu)); sUtf8[iSize++] = (char)(0x80u | (c & 0x3fu)); }
			sUtf8[iSize] = 0; (void)xuiRichEditInsertText(pWidget, sUtf8);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_IME_COMPOSITION:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		if ( pEvent->bCompositionActive ) {
			if ( pEvent->bCompositionReplacementRange ) { pData->iImeStart = pEvent->iCompositionReplacementStart; pData->iImeEnd = pEvent->iCompositionReplacementEnd; }
			else if ( !pData->bImeActive ) __xuiRichEditSelection(pData, &pData->iImeStart, &pData->iImeEnd);
			pData->bImeActive = 1; pData->iImeCursor = pEvent->iCompositionCursor;
			(void)__xuiRichEditImeText(pData, pEvent->sText, pEvent->iTextSize);
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iTextSize > 0 ) {
			if ( pEvent->bCompositionReplacementRange ) { iStart = pEvent->iCompositionReplacementStart; iEnd = pEvent->iCompositionReplacementEnd; }
			else if ( pData->bImeActive ) { iStart = pData->iImeStart; iEnd = pData->iImeEnd; }
			else __xuiRichEditSelection(pData, &iStart, &iEnd);
			pData->bImeActive = 0; pData->iAnchor = iStart; pData->iCaret = iEnd;
			(void)__xuiRichEditReplaceSelection(pWidget, pData, pEvent->sText);
		} else __xuiRichEditImeReset(pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_BOUNDS_CHANGED: pData->bLayoutDirty = 1; return XUI_OK;
	default: break;
	}
	return XUI_OK;
}

static xui_rect_t __xuiRichEditImeRect(xui_widget pWidget, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rect_t tRect = {0,0,0,0};
	xui_rect_t tWorld;
	(void)pUser;
	if ( pData == NULL ) return tRect;
	tRect = __xuiRichEditCaret(pWidget, pData, pData->bImeActive ? pData->iImeStart : pData->iCaret);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRect.fX += tWorld.fX;
	tRect.fY += tWorld.fY;
	return tRect;
}

static int __xuiRichEditUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	uint32_t iRevision;
	(void)fDelta;
	(void)pUser;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiRichEditNotifyState(pWidget, pData);
	iRevision = xuiGetLanguageRevision(xuiWidgetGetContext(pWidget));
	if ( pData->pFindWindow != NULL && pData->iFindLanguageRevision != iRevision )
		(void)__xuiRichEditFindApplyLanguage(pWidget, pData);
	return XUI_OK;
}

static void __xuiRichEditDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_HIDDEN;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 420.0f;
	pLayout->fPreferredHeight = 240.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiRichEditHScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	(void)pBar;
	if ( pData == NULL || pData->bSyncingScrollBars ) return;
	(void)xuiRichEditSetScroll(pWidget, fValue, pData->fScrollY);
}

static void __xuiRichEditVScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	(void)pBar;
	if ( pData == NULL || pData->bSyncingScrollBars ) return;
	(void)xuiRichEditSetScroll(pWidget, pData->fScrollX, fValue);
}

static int __xuiRichEditInitScrollBars(xui_widget pWidget, xui_rich_edit_data_t* pData)
{
	xui_scrollbar_desc_t tDesc;
	int iRet;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fMin = 0.0f;
	tDesc.fMax = 1.0f;
	tDesc.fPage = 1.0f;
	tDesc.iOrientation = XUI_ORIENTATION_HORIZONTAL;
	tDesc.iMode = XUI_SCROLLBAR_MODE_COMPACT;
	tDesc.iButtonMode = XUI_SCROLLBAR_BUTTONS_OFF;
	tDesc.fThickness = XUI_RICH_EDIT_SCROLLBAR_SIZE;
	tDesc.fMinThumbSize = 18.0f;
	tDesc.iTrackColor = XUI_COLOR_RGBA(226, 232, 240, 255);
	tDesc.iThumbColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	tDesc.iHoverColor = XUI_COLOR_RGBA(100, 116, 139, 255);
	tDesc.iActiveColor = XUI_COLOR_RGBA(71, 85, 105, 255);
	tDesc.iFocusColor = XUI_COLOR_RGBA(59, 130, 246, 255);
	tDesc.iDisabledColor = XUI_COLOR_RGBA(203, 213, 225, 255);
	iRet = xuiScrollBarCreate(xuiWidgetGetContext(pWidget), &pData->pHScrollBar, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pHScrollBar);
	if ( iRet != XUI_OK ) return iRet;
	tDesc.iOrientation = XUI_ORIENTATION_VERTICAL;
	iRet = xuiScrollBarCreate(xuiWidgetGetContext(pWidget), &pData->pVScrollBar, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pVScrollBar);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetVisible(pData->pHScrollBar, 0);
	(void)xuiWidgetSetVisible(pData->pVScrollBar, 0);
	iRet = xuiScrollBarSetChange(pData->pHScrollBar, __xuiRichEditHScrollBarChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetChange(pData->pVScrollBar, __xuiRichEditVScrollBarChanged, pWidget);
	return iRet;
}

static uint64_t __xuiRichEditAccessibleCellId(uint64_t iTableId, int iRow, int iColumn)
{
	uint64_t iValue = iTableId ^ ((uint64_t)(uint32_t)iRow << 32) ^ (uint32_t)iColumn;
	iValue ^= iValue >> 33; iValue *= UINT64_C(0xff51afd7ed558ccd);
	iValue ^= iValue >> 33; iValue *= UINT64_C(0xc4ceb9fe1a85ec53);
	iValue ^= iValue >> 33;
	return XUI_RICH_ACCESSIBLE_CELL_FLAG | (iValue & UINT64_C(0x3fffffffffffffff));
}

static int __xuiRichEditAccessibleNodeCountRecursive(xui_rich_node pNode)
{
	xui_rich_node pChild;
	xui_rich_node_info_t tInfo;
	int iCount = 1;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) == XUI_OK && tInfo.iType == XUI_RICH_NODE_TABLE )
		iCount += tInfo.iRows * tInfo.iColumns;
	for ( pChild = xuiRichNodeGetFirstChild(pNode); pChild != NULL;
	      pChild = xuiRichNodeGetNextSibling(pChild) )
		iCount += __xuiRichEditAccessibleNodeCountRecursive(pChild);
	return iCount;
}

static int __xuiRichEditAccessibleCount(xui_widget pWidget, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pUser;
	xui_rich_node pNode;
	int iCount = 1;
	(void)pWidget;
	if ( pData == NULL || pData->pDocument == NULL ) return 0;
	for ( pNode = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pData->pDocument)); pNode != NULL;
	      pNode = xuiRichNodeGetNextSibling(pNode) )
		iCount += __xuiRichEditAccessibleNodeCountRecursive(pNode);
	return iCount;
}

static xui_rich_node __xuiRichEditAccessibleNodeAtRecursive(xui_rich_node pNode,
	int* pIndex, xui_rich_node* ppCellTable, int* pCellRow, int* pCellColumn)
{
	xui_rich_node pChild;
	xui_rich_node_info_t tInfo;
	int iCell;
	if ( *pIndex == 0 ) return pNode;
	(*pIndex)--;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) == XUI_OK && tInfo.iType == XUI_RICH_NODE_TABLE ) {
		for ( iCell = 0; iCell < tInfo.iRows * tInfo.iColumns; iCell++ ) {
			if ( *pIndex == 0 ) {
				*ppCellTable = pNode; *pCellRow = iCell / tInfo.iColumns;
				*pCellColumn = iCell % tInfo.iColumns; return NULL;
			}
			(*pIndex)--;
		}
	}
	for ( pChild = xuiRichNodeGetFirstChild(pNode); pChild != NULL;
	      pChild = xuiRichNodeGetNextSibling(pChild) ) {
		xui_rich_node pFound = __xuiRichEditAccessibleNodeAtRecursive(pChild, pIndex,
			ppCellTable, pCellRow, pCellColumn);
		if ( pFound != NULL || *ppCellTable != NULL ) return pFound;
	}
	return NULL;
}

static xui_rich_node __xuiRichEditAccessibleNodeAt(xui_rich_edit_data_t* pData, int iIndex,
	xui_rich_node* ppCellTable, int* pCellRow, int* pCellColumn)
{
	xui_rich_node pNode;
	*ppCellTable = NULL; *pCellRow = -1; *pCellColumn = -1;
	if ( iIndex <= 0 ) return NULL;
	iIndex--;
	for ( pNode = xuiRichNodeGetFirstChild(xuiRichDocumentGetRoot(pData->pDocument)); pNode != NULL;
	      pNode = xuiRichNodeGetNextSibling(pNode) ) {
		xui_rich_node pFound = __xuiRichEditAccessibleNodeAtRecursive(pNode, &iIndex,
			ppCellTable, pCellRow, pCellColumn);
		if ( pFound != NULL || *ppCellTable != NULL ) return pFound;
	}
	return NULL;
}

static int __xuiRichEditAccessibleNodeRange(xui_rich_edit_data_t* pData, xui_rich_node pNode,
	int* pStart, int* pEnd)
{
	xui_rich_node_info_t tInfo;
	int i;
	for ( i = 0; i < pData->iBlockCount; i++ ) if ( pData->pBlocks[i].pNode == pNode ) {
		*pStart = pData->pBlocks[i].iDocumentStart; *pEnd = pData->pBlocks[i].iDocumentEnd; return XUI_OK;
	}
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ||
	     xuiInternalRichDocumentNodeOffset(pData->pDocument, pNode, pStart) != XUI_OK )
		return XUI_ERROR_FILE_NOT_FOUND;
	*pEnd = *pStart + ((tInfo.iType == XUI_RICH_NODE_TEXT || tInfo.iType == XUI_RICH_NODE_LINK) ?
		tInfo.iTextSize : 3);
	return XUI_OK;
}

static xui_rect_t __xuiRichEditAccessibleNodeBounds(xui_widget pWidget,
	xui_rich_edit_data_t* pData, xui_rich_node pNode)
{
	xui_rect_t tResult = {0,0,0,0};
	xui_rect_t tWorld = xuiWidgetGetWorldRect(pWidget);
	xui_rich_node_info_t tInfo;
	xui_rich_layout_fragment_t tFragment;
	int bFound = 0;
	int i;
	for ( i = 0; i < pData->iBlockCount; i++ ) if ( pData->pBlocks[i].pNode == pNode ) {
		tResult = __xuiRichEditBlockRect(pWidget, pData, pData->pBlocks[i].tRect); bFound = 1; break;
	}
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( !bFound && xuiRichNodeGetInfo(pNode, &tInfo) == XUI_OK &&
	     tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET && tInfo.pWidget != NULL )
		return xuiWidgetGetWorldRect(tInfo.pWidget);
	if ( !bFound ) for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
		xui_rect_t tRect;
		(void)__xuiRichEditGetLayoutFragment(pData, i, &tFragment);
		if ( tFragment.tPublic.iNodeId != tInfo.iId ) continue;
		tRect = __xuiRichEditFragmentRect(pWidget, pData, &tFragment);
		if ( !bFound ) { tResult = tRect; bFound = 1; }
		else {
			float fRight = tResult.fX + tResult.fW > tRect.fX + tRect.fW ? tResult.fX + tResult.fW : tRect.fX + tRect.fW;
			float fBottom = tResult.fY + tResult.fH > tRect.fY + tRect.fH ? tResult.fY + tResult.fH : tRect.fY + tRect.fH;
			if ( tRect.fX < tResult.fX ) tResult.fX = tRect.fX;
			if ( tRect.fY < tResult.fY ) tResult.fY = tRect.fY;
			tResult.fW = fRight - tResult.fX; tResult.fH = fBottom - tResult.fY;
		}
	}
	if ( bFound ) { tResult.fX += tWorld.fX; tResult.fY += tWorld.fY; }
	return tResult;
}

static int __xuiRichEditAccessibleRole(const xui_rich_node_info_t* pInfo)
{
	switch ( pInfo->iType ) {
	case XUI_RICH_NODE_PARAGRAPH: return XUI_ACCESSIBLE_ROLE_PARAGRAPH;
	case XUI_RICH_NODE_HEADING: return XUI_ACCESSIBLE_ROLE_HEADING;
	case XUI_RICH_NODE_BLOCK_QUOTE: return XUI_ACCESSIBLE_ROLE_BLOCK_QUOTE;
	case XUI_RICH_NODE_LIST_ITEM: return pInfo->tParagraphStyle.iListType == XUI_RICH_LIST_CHECK ?
		XUI_ACCESSIBLE_ROLE_CHECKBOX : XUI_ACCESSIBLE_ROLE_LIST_ITEM;
	case XUI_RICH_NODE_TEXT: return XUI_ACCESSIBLE_ROLE_TEXT;
	case XUI_RICH_NODE_LINK: return XUI_ACCESSIBLE_ROLE_LINK;
	case XUI_RICH_NODE_IMAGE: case XUI_RICH_NODE_INLINE_IMAGE: return XUI_ACCESSIBLE_ROLE_IMAGE;
	case XUI_RICH_NODE_TABLE: return XUI_ACCESSIBLE_ROLE_TABLE;
	case XUI_RICH_NODE_HORIZONTAL_RULE: return XUI_ACCESSIBLE_ROLE_SEPARATOR;
	case XUI_RICH_NODE_INLINE_WIDGET: return XUI_ACCESSIBLE_ROLE_EMBEDDED_OBJECT;
	default: return XUI_ACCESSIBLE_ROLE_GENERIC;
	}
}

static int __xuiRichEditAccessibleGet(xui_widget pWidget, int iIndex,
	xui_accessible_node_t* pAccessible, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pUser;
	xui_rich_node pNode;
	xui_rich_node pCellTable;
	xui_rich_node_info_t tInfo;
	xui_rich_node_info_t tRootInfo;
	xui_rect_t tViewport;
	int iRow, iColumn, iStart = 0, iEnd = 0, iSelectionStart, iSelectionEnd;
	if ( pData == NULL || __xuiRichEditEnsureLayout(pWidget, pData) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tRootInfo, 0, sizeof(tRootInfo)); tRootInfo.iSize = sizeof(tRootInfo);
	(void)xuiRichNodeGetInfo(xuiRichDocumentGetRoot(pData->pDocument), &tRootInfo);
	if ( iIndex == 0 ) {
		pAccessible->iId = XUI_RICH_ACCESSIBLE_ROOT_ID;
		pAccessible->iRole = XUI_ACCESSIBLE_ROLE_DOCUMENT;
		pAccessible->sName = xuiWidgetGetAccessibleName(pWidget) != NULL ? xuiWidgetGetAccessibleName(pWidget) : "Rich text document";
		pAccessible->sValue = xuiRichDocumentGetText(pData->pDocument);
		pAccessible->sDescription = xuiWidgetGetAccessibleDescription(pWidget);
		pAccessible->tBounds = xuiWidgetGetWorldRect(pWidget);
		pAccessible->iTextEnd = xuiRichDocumentGetLength(pData->pDocument);
		pAccessible->iState = XUI_ACCESSIBLE_STATE_FOCUSABLE |
			(pData->bReadonly ? XUI_ACCESSIBLE_STATE_READONLY : XUI_ACCESSIBLE_STATE_EDITABLE);
		return XUI_OK;
	}
	pNode = __xuiRichEditAccessibleNodeAt(pData, iIndex, &pCellTable, &iRow, &iColumn);
	if ( pCellTable != NULL ) {
		xui_rect_t tTable;
		memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
		if ( xuiRichNodeGetInfo(pCellTable, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
		tTable = __xuiRichEditAccessibleNodeBounds(pWidget, pData, pCellTable);
		pAccessible->iId = __xuiRichEditAccessibleCellId(tInfo.iId, iRow, iColumn);
		pAccessible->iParentId = tInfo.iId;
		pAccessible->iRole = XUI_ACCESSIBLE_ROLE_CELL;
		pAccessible->sValue = xuiRichTableGetCellText(pCellTable, iRow, iColumn);
		pAccessible->iRow = iRow; pAccessible->iColumn = iColumn;
		pAccessible->tBounds = (xui_rect_t){tTable.fX + tTable.fW * iColumn / tInfo.iColumns,
			tTable.fY + tTable.fH * iRow / tInfo.iRows, tTable.fW / tInfo.iColumns, tTable.fH / tInfo.iRows};
		pAccessible->iState = pData->bReadonly ? XUI_ACCESSIBLE_STATE_READONLY : XUI_ACCESSIBLE_STATE_EDITABLE;
		return XUI_OK;
	}
	if ( pNode == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	pAccessible->iId = tInfo.iId;
	pAccessible->iParentId = tInfo.iParentId == tRootInfo.iId ? XUI_RICH_ACCESSIBLE_ROOT_ID : tInfo.iParentId;
	pAccessible->iRole = __xuiRichEditAccessibleRole(&tInfo);
	pAccessible->sName = (tInfo.iType == XUI_RICH_NODE_IMAGE || tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE) ?
		tInfo.sAltText : (tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET && tInfo.pWidget != NULL ?
		xuiWidgetGetAccessibleName(tInfo.pWidget) : NULL);
	pAccessible->sValue = (tInfo.iType == XUI_RICH_NODE_TEXT || tInfo.iType == XUI_RICH_NODE_LINK) ? tInfo.sText : NULL;
	pAccessible->sDescription = tInfo.iType == XUI_RICH_NODE_LINK ? tInfo.sResource : NULL;
	pAccessible->tBounds = __xuiRichEditAccessibleNodeBounds(pWidget, pData, pNode);
	if ( __xuiRichEditAccessibleNodeRange(pData, pNode, &iStart, &iEnd) == XUI_OK ) {
		pAccessible->iTextStart = iStart; pAccessible->iTextEnd = iEnd;
		__xuiRichEditSelection(pData, &iSelectionStart, &iSelectionEnd);
		if ( iSelectionEnd > iStart && iSelectionStart < iEnd ) pAccessible->iState |= XUI_ACCESSIBLE_STATE_SELECTED;
	}
	pAccessible->iLevel = tInfo.iType == XUI_RICH_NODE_HEADING ? tInfo.tParagraphStyle.iHeadingLevel :
		tInfo.tParagraphStyle.iListLevel;
	if ( tInfo.iType == XUI_RICH_NODE_TABLE ) { pAccessible->iRowCount = tInfo.iRows; pAccessible->iColumnCount = tInfo.iColumns; }
	if ( tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.bListChecked )
		pAccessible->iState |= XUI_ACCESSIBLE_STATE_CHECKED;
	if ( tInfo.iType == XUI_RICH_NODE_LINK || tInfo.iType == XUI_RICH_NODE_IMAGE ||
	     tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE || tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET )
		pAccessible->iState |= XUI_ACCESSIBLE_STATE_FOCUSABLE;
	tViewport = __xuiRichEditContentRect(pWidget, pData); tViewport.fX += xuiWidgetGetWorldRect(pWidget).fX; tViewport.fY += xuiWidgetGetWorldRect(pWidget).fY;
	if ( pAccessible->tBounds.fX + pAccessible->tBounds.fW <= tViewport.fX || pAccessible->tBounds.fX >= tViewport.fX + tViewport.fW ||
	     pAccessible->tBounds.fY + pAccessible->tBounds.fH <= tViewport.fY || pAccessible->tBounds.fY >= tViewport.fY + tViewport.fH )
		pAccessible->iState |= XUI_ACCESSIBLE_STATE_OFFSCREEN;
	return XUI_OK;
}

static int __xuiRichEditAccessibleAction(xui_widget pWidget, uint64_t iNodeId,
	int iAction, const void* pActionData, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pUser;
	xui_rich_node pNode;
	xui_rich_node_info_t tInfo;
	int iStart, iEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iAction == XUI_ACCESSIBLE_ACTION_FOCUS ) return xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iAction == XUI_ACCESSIBLE_ACTION_SET_SELECTION ) {
		const xui_accessible_selection_t* pSelection = (const xui_accessible_selection_t*)pActionData;
		if ( pSelection == NULL || (pSelection->iSize != 0 && pSelection->iSize < sizeof(*pSelection)) ) return XUI_ERROR_INVALID_ARGUMENT;
		return xuiRichEditSetSelection(pWidget, pSelection->iAnchor, pSelection->iCaret);
	}
	if ( iNodeId == XUI_RICH_ACCESSIBLE_ROOT_ID || (iNodeId & XUI_RICH_ACCESSIBLE_CELL_FLAG) == XUI_RICH_ACCESSIBLE_CELL_FLAG )
		return XUI_ERROR_UNSUPPORTED;
	pNode = xuiRichDocumentFindNode(pData->pDocument, iNodeId);
	if ( pNode == NULL ) return XUI_ERROR_FILE_NOT_FOUND;
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( xuiRichNodeGetInfo(pNode, &tInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iAction == XUI_ACCESSIBLE_ACTION_SCROLL_INTO_VIEW ) {
		if ( __xuiRichEditAccessibleNodeRange(pData, pNode, &iStart, &iEnd) != XUI_OK ) return XUI_ERROR_FILE_NOT_FOUND;
		pData->iAnchor = pData->iCaret = iStart; __xuiRichEditEnsureCaretVisible(pWidget, pData);
		return xuiRichEditSetScroll(pWidget, pData->fScrollX, pData->fScrollY);
	}
	if ( iAction == XUI_ACCESSIBLE_ACTION_TOGGLE && tInfo.iType == XUI_RICH_NODE_LIST_ITEM &&
	     tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_CHECK && !pData->bReadonly ) {
		xui_rich_paragraph_style_t tStyle = tInfo.tParagraphStyle;
		if ( __xuiRichEditAccessibleNodeRange(pData, pNode, &iStart, &iEnd) != XUI_OK ) return XUI_ERROR_FILE_NOT_FOUND;
		tStyle.iSize = sizeof(tStyle); tStyle.bListChecked = !tStyle.bListChecked;
		return xuiRichDocumentApplyParagraphStyle(pData->pDocument, iStart, iEnd, &tStyle);
	}
	if ( iAction == XUI_ACCESSIBLE_ACTION_ACTIVATE ) {
		if ( tInfo.iType == XUI_RICH_NODE_LINK && pData->onLinkClick != NULL )
			pData->onLinkClick(pWidget, tInfo.iId, tInfo.sResource, pData->pLinkClickUser);
		else if ( tInfo.iType == XUI_RICH_NODE_IMAGE || tInfo.iType == XUI_RICH_NODE_INLINE_IMAGE ||
		          tInfo.iType == XUI_RICH_NODE_INLINE_WIDGET )
			__xuiRichEditEmit(pWidget, pData, XUI_RICH_EDIT_EVENT_OBJECT_ACTIVATE, pNode);
		else return XUI_ERROR_UNSUPPORTED;
		return XUI_OK;
	}
	return XUI_ERROR_UNSUPPORTED;
}

static int __xuiRichEditInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pTypeData;
	const xui_rich_edit_desc_t* pDesc = (const xui_rich_edit_desc_t*)pCreateData;
	xui_theme_t tTheme;
	xui_rich_node pParagraph;
	int iRet;
	(void)pUser;
	memset(&tTheme, 0, sizeof(tTheme)); tTheme.iSize = sizeof(tTheme); (void)xuiGetTheme(xuiWidgetGetContext(pWidget), &tTheme);
	pData->pFont = pDesc != NULL && pDesc->pFont != NULL ? pDesc->pFont : tTheme.pFont;
	memset(&pData->tFonts, 0, sizeof(pData->tFonts));
	pData->tFonts.iSize = sizeof(pData->tFonts);
	pData->tFonts.pNormal = pData->pFont;
	pData->iTextColor = pDesc != NULL && (pDesc->iTextColor & 0xffu) != 0 ? pDesc->iTextColor : tTheme.iTextColor;
	pData->iBackgroundColor = pDesc != NULL && (pDesc->iBackgroundColor & 0xffu) != 0 ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255,255,255,255);
	pData->iBorderColor = pDesc != NULL && (pDesc->iBorderColor & 0xffu) != 0 ? pDesc->iBorderColor : tTheme.iBorderColor;
	pData->iFocusBorderColor = pDesc != NULL && (pDesc->iFocusBorderColor & 0xffu) != 0 ? pDesc->iFocusBorderColor : tTheme.iAccentColor;
	pData->iSelectionColor = pDesc != NULL && (pDesc->iSelectionColor & 0xffu) != 0 ? pDesc->iSelectionColor : tTheme.iSelectionColor;
	pData->iCursorColor = pDesc != NULL && (pDesc->iCursorColor & 0xffu) != 0 ? pDesc->iCursorColor : pData->iTextColor;
	pData->iFindResultColor = pDesc != NULL && (pDesc->iFindResultColor & 0xffu) != 0 ? pDesc->iFindResultColor : XUI_COLOR_RGBA(255,235,128,150);
	pData->iFindActiveColor = pDesc != NULL && (pDesc->iFindActiveColor & 0xffu) != 0 ? pDesc->iFindActiveColor : XUI_COLOR_RGBA(255,183,77,190);
	pData->iFindActiveIndex = -1;
	pData->fBorderWidth = pDesc != NULL && pDesc->fBorderWidth > 0.0f ? pDesc->fBorderWidth : 1.0f;
	pData->fPadding = pDesc != NULL && pDesc->fPadding > 0.0f ? pDesc->fPadding : 8.0f;
	pData->fLineGap = pDesc != NULL && pDesc->fLineGap >= 0.0f ? pDesc->fLineGap : 2.0f;
	pData->fParagraphGap = pDesc != NULL && pDesc->fParagraphGap >= 0.0f ? pDesc->fParagraphGap : 6.0f;
	pData->bReadonly = pDesc != NULL ? pDesc->bReadonly : 0;
	pData->bWordWrap = pDesc == NULL || pDesc->bWordWrap;
	pData->fZoom = 1.0f;
	pData->bLayoutDirty = 1;
	pData->fLayoutWidth = -1.0f;
	pData->iNotifiedAnchor = -1;
	pData->iNotifiedCaret = -1;
	pData->fNotifiedScrollX = -1.0f;
	pData->fNotifiedScrollY = -1.0f;
	pData->fNotifiedZoom = -1.0f;
	xuiScrollModelInit(&pData->tScrollModel);
	pData->tTypingStyle.iSize = sizeof(pData->tTypingStyle);
	pData->tTypingStyle.pFont = NULL;
	pData->tTypingStyle.iTextColor = pData->iTextColor;
	if ( pDesc != NULL && pDesc->pDocument != NULL ) { pData->pDocument = pDesc->pDocument; pData->bOwnDocument = pDesc->bOwnDocument; }
	else {
		iRet = xuiRichDocumentCreate(&pData->pDocument); if ( iRet != XUI_OK ) return iRet;
		pData->bOwnDocument = 1;
		if ( pDesc != NULL && pDesc->sText != NULL && pDesc->sText[0] != 0 ) {
			pParagraph = xuiRichDocumentGetRoot(pData->pDocument);
			(void)pParagraph;
			(void)xuiRichDocumentReplace(pData->pDocument, 0, 0, pDesc->sText, &pData->tTypingStyle);
		}
	}
	pData->pFlowLayout = xLayoutContextCreate(NULL);
	if ( pData->pFlowLayout == NULL ) {
		if ( pData->bOwnDocument ) xuiRichDocumentDestroy(pData->pDocument);
		pData->pDocument = NULL;
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	(void)xuiInternalRichDocumentAddObserver(pData->pDocument, __xuiRichEditDocumentChanged, pWidget);
	iRet = __xuiRichEditInitScrollBars(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetImeMode(pWidget, XUI_IME_ENABLED);
	(void)xuiWidgetSetImeCandidateRect(pWidget, __xuiRichEditImeRect, NULL);
	(void)xuiWidgetSetAccessibilityProvider(pWidget, __xuiRichEditAccessibleCount,
		__xuiRichEditAccessibleGet, __xuiRichEditAccessibleAction, pData);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_TEXT, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_IME_COMPOSITION, __xuiRichEditEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiRichEditEvent, NULL);
	return __xuiRichEditInitMenu(pWidget, pData);
}

static void __xuiRichEditDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_rich_edit_data_t* pData = (xui_rich_edit_data_t*)pTypeData;
	xui_widget pPopup;
	(void)pWidget; (void)pUser;
	if ( pData->pDocument != NULL ) {
		(void)xuiInternalRichDocumentRemoveObserver(pData->pDocument, __xuiRichEditDocumentChanged, pWidget);
		if ( pData->bOwnDocument ) xuiRichDocumentDestroy(pData->pDocument);
	}
	if ( pData->pFlowLayout != NULL ) xLayoutContextDestroy(pData->pFlowLayout);
	__xuiRichEditClearFontCache(pWidget, pData);
	if ( pData->pFindWindow != NULL ) { xuiWidgetDestroy(pData->pFindWindow); pData->pFindWindow = NULL; }
	if ( pData->pMenu != NULL ) {
		pPopup = xuiMenuGetPopupWidget(pData->pMenu);
		if ( pPopup != NULL ) xuiWidgetDestroy(pPopup); else xuiWidgetDestroy(pData->pMenu);
		pData->pMenu = NULL;
	}
	if ( pData->pFragments != NULL ) xrtFree(pData->pFragments);
	if ( pData->pAtoms != NULL ) xrtFree(pData->pAtoms);
	__xuiRichEditReleaseBlocks(pData);
	if ( pData->pBlocks != NULL ) xrtFree(pData->pBlocks);
	if ( pData->sScratch != NULL ) xrtFree(pData->sScratch);
	if ( pData->sImeText != NULL ) xrtFree(pData->sImeText);
	if ( pData->sFindPattern != NULL ) xrtFree(pData->sFindPattern);
	if ( pData->sFindReplacement != NULL ) xrtFree(pData->sFindReplacement);
	if ( pData->pFindResults != NULL ) xrtFree(pData->pFindResults);
}

static int __xuiRichEditEnsureType(xui_context pContext, xui_widget_type* ppType)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;
	if ( pContext == NULL || ppType == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppType = NULL;
	pType = xuiWidgetFindType(pContext, "RichEdit");
	if ( pType != NULL ) { *ppType = pType; return XUI_OK; }
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "RichEdit";
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_rich_edit_data_t);
	tDesc.onInit = __xuiRichEditInit;
	tDesc.onDestroy = __xuiRichEditDestroy;
	tDesc.onContentMeasure = __xuiRichEditMeasure;
	tDesc.onLayoutPrepare = __xuiRichEditPrepareLayout;
	tDesc.onLayoutChildren = __xuiRichEditArrangeChildren;
	tDesc.onCacheRender = __xuiRichEditRender;
	tDesc.onUpdate = __xuiRichEditUpdate;
	__xuiRichEditDefaultLayout(&tDesc.tLayout);
	memset(&tDesc.tCachePolicy, 0, sizeof(tDesc.tCachePolicy));
	tDesc.tCachePolicy.iSize = sizeof(tDesc.tCachePolicy);
	tDesc.tCachePolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tDesc.tCachePolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet == XUI_OK ) *ppType = pType;
	return iRet;
}

XUI_API xui_widget_type xuiRichEditGetType(xui_context pContext)
{
	xui_widget_type pType = NULL;
	(void)__xuiRichEditEnsureType(pContext, &pType);
	return pType;
}

XUI_API int xuiRichEditCreate(xui_context pContext, xui_widget* ppWidget, const xui_rich_edit_desc_t* pDesc)
{
	xui_widget_type pType;
	int iRet;
	if ( pContext == NULL || ppWidget == NULL || (pDesc != NULL && pDesc->iSize != 0 && pDesc->iSize < sizeof(*pDesc)) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditEnsureType(pContext, &pType);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_rich_document xuiRichEditGetDocument(xui_widget pWidget) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); return pData != NULL ? pData->pDocument : NULL; }

XUI_API int xuiRichEditSetDocument(xui_widget pWidget, xui_rich_document pDocument, int bOwnDocument)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	int iRet;
	if ( pData == NULL || pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pDocument == pDocument ) { pData->bOwnDocument = bOwnDocument ? 1 : 0; return XUI_OK; }
	if ( pData->pDocument != NULL ) {
		iRet = __xuiRichEditSyncWidgets(pWidget, pData, pData->pDocument, 0);
		if ( iRet != XUI_OK ) return iRet;
		(void)xuiInternalRichDocumentRemoveObserver(pData->pDocument, __xuiRichEditDocumentChanged, pWidget);
		if ( pData->bOwnDocument ) xuiRichDocumentDestroy(pData->pDocument);
	}
	pData->pDocument = pDocument; pData->bOwnDocument = bOwnDocument ? 1 : 0; pData->iAnchor = pData->iCaret = 0; pData->bLayoutDirty = 1;
	(void)xuiInternalRichDocumentAddObserver(pDocument, __xuiRichEditDocumentChanged, pWidget);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRichEditSetChange(xui_widget pWidget, xui_rich_edit_change_proc onChange, void* pUser) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; pData->onChange = onChange; pData->pChangeUser = pUser; return XUI_OK; }
XUI_API int xuiRichEditSetLinkClick(xui_widget pWidget, xui_rich_edit_link_proc onClick, void* pUser) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; pData->onLinkClick = onClick; pData->pLinkClickUser = pUser; return XUI_OK; }
XUI_API int xuiRichEditSetEvent(xui_widget pWidget, xui_rich_edit_event_proc onEvent, void* pUser) { xui_rich_edit_data_t* pData=__xuiRichEditData(pWidget);if(!pData)return XUI_ERROR_INVALID_ARGUMENT;pData->onEvent=onEvent;pData->pEventUser=pUser;pData->iNotifiedAnchor=-1;pData->iNotifiedCaret=-1;pData->fNotifiedScrollX=-1.0f;pData->fNotifiedScrollY=-1.0f;pData->fNotifiedZoom=-1.0f;return XUI_OK; }
XUI_API int xuiRichEditSetSelection(xui_widget pWidget, int iStart, int iEnd) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int n; if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; n = xuiRichDocumentGetLength(pData->pDocument); if ( iStart < 0 || iEnd < 0 || iStart > n || iEnd > n ) return XUI_ERROR_INVALID_ARGUMENT; pData->iAnchor = iStart; return __xuiRichEditMove(pWidget, pData, iEnd, 1); }
XUI_API int xuiRichEditGetSelection(xui_widget pWidget, int* pStart, int* pEnd) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; if ( pStart != NULL ) *pStart = pData->iAnchor; if ( pEnd != NULL ) *pEnd = pData->iCaret; return XUI_OK; }
XUI_API int xuiRichEditSelectAll(xui_widget pWidget) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; pData->iAnchor = 0; pData->iCaret = xuiRichDocumentGetLength(pData->pDocument); return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER); }
XUI_API int xuiRichEditInsertText(xui_widget pWidget, const char* sText) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT; return pData->bReadonly ? XUI_ERROR_UNSUPPORTED : __xuiRichEditReplaceSelection(pWidget, pData, sText); }
XUI_API int xuiRichEditApplyStyle(xui_widget pWidget, const xui_rich_text_style_t* pStyle) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s,e; if ( pData == NULL || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT; __xuiRichEditSelection(pData,&s,&e); if ( s == e ) { pData->tTypingStyle = *pStyle; pData->tTypingStyle.iSize = sizeof(pData->tTypingStyle); pData->bTypingStyleOverride = 1; return XUI_OK; } return xuiRichDocumentApplyStyle(pData->pDocument,s,e,pStyle); }
XUI_API int xuiRichEditApplyLink(xui_widget pWidget, const char* sUrl)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL || sUrl == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	if ( s == e ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiRichDocumentApplyLink(pData->pDocument, s, e, sUrl);
}
XUI_API int xuiRichEditApplyParagraphStyle(xui_widget pWidget, const xui_rich_paragraph_style_t* pStyle)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL || pStyle == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	return xuiRichDocumentApplyParagraphStyle(pData->pDocument, s, e, pStyle);
}

static int __xuiRichEditFinishObjectInsert(xui_widget pWidget, xui_rich_edit_data_t* pData, xui_rich_node pNode)
{
	int iOffset;
	if ( pNode == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiInternalRichDocumentNodeOffset(pData->pDocument, pNode, &iOffset) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iAnchor = pData->iCaret = iOffset + 3;
	pData->bLayoutDirty = 1;
	__xuiRichEditEnsureCaretVisible(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiRichEditInsertImage(xui_widget pWidget, const xui_rich_image_desc_t* pDesc)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL || pDesc == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	return __xuiRichEditFinishObjectInsert(pWidget, pData, xuiRichDocumentInsertImage(pData->pDocument, s, e, pDesc));
}

XUI_API int xuiRichEditInsertInlineImage(xui_widget pWidget, const xui_rich_image_desc_t* pDesc)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL || pDesc == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	return __xuiRichEditFinishObjectInsert(pWidget, pData,
		xuiRichDocumentInsertInlineImage(pData->pDocument, s, e, pDesc));
}

XUI_API int xuiRichEditInsertTable(xui_widget pWidget, const xui_rich_table_desc_t* pDesc)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL || pDesc == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	return __xuiRichEditFinishObjectInsert(pWidget, pData, xuiRichDocumentInsertTable(pData->pDocument, s, e, pDesc));
}

XUI_API int xuiRichEditInsertHorizontalRule(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int s, e;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	return __xuiRichEditFinishObjectInsert(pWidget, pData, xuiRichDocumentInsertHorizontalRule(pData->pDocument, s, e));
}

XUI_API int xuiRichEditBeginTableCellEdit(xui_widget pWidget, xui_document_node_id_t iTableId,
	int iRow, int iColumn)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rich_document pCellDocument;
	xui_rich_node pTable;
	xui_rich_node_info_t tInfo;
	xui_rich_edit_desc_t tDesc;
	xui_widget pEditor = NULL;
	int iRet;
	if ( pData == NULL || pData->bReadonly || iTableId == 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pTable = xuiRichDocumentFindNode(pData->pDocument, iTableId);
	memset(&tInfo, 0, sizeof(tInfo)); tInfo.iSize = sizeof(tInfo);
	if ( pTable == NULL || xuiRichNodeGetInfo(pTable, &tInfo) != XUI_OK || tInfo.iType != XUI_RICH_NODE_TABLE ||
	     iRow < 0 || iRow >= tInfo.iRows || iColumn < 0 || iColumn >= tInfo.iColumns ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pCellEditor != NULL ) (void)__xuiRichEditEndCellEdit(pWidget, pData, 0);
	iRet = xuiRichTableGetCellDocument(pData->pDocument, pTable, iRow, iColumn, &pCellDocument);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tDesc, 0, sizeof(tDesc)); tDesc.iSize = sizeof(tDesc);
	tDesc.pDocument = pCellDocument;
	tDesc.pFont = pData->pFont;
	tDesc.bOwnDocument = 0;
	tDesc.bWordWrap = 1;
	tDesc.iTextColor = pData->iTextColor;
	tDesc.iBackgroundColor = tInfo.iCellColor;
	tDesc.iBorderColor = pData->iFocusBorderColor;
	tDesc.iFocusBorderColor = pData->iFocusBorderColor;
	tDesc.iSelectionColor = pData->iSelectionColor;
	tDesc.iCursorColor = pData->iCursorColor;
	tDesc.fBorderWidth = 1.0f;
	tDesc.fPadding = tInfo.fCellPadding > 1.0f ? tInfo.fCellPadding - 1.0f : 0.0f;
	tDesc.fLineGap = pData->fLineGap;
	tDesc.fParagraphGap = pData->fParagraphGap;
	iRet = xuiRichEditCreate(xuiWidgetGetContext(pWidget), &pEditor, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pEditor);
	if ( iRet != XUI_OK ) { xuiWidgetDestroy(pEditor); return iRet; }
	pData->pCellEditor = pEditor;
	pData->iActiveTableId = iTableId;
	pData->iActiveCellRow = iRow;
	pData->iActiveCellColumn = iColumn;
	(void)xuiRichEditSetFontSet(pEditor, &pData->tFonts);
	(void)xuiRichEditSetZoom(pEditor, pData->fZoom);
	(void)xuiRichEditSetSelection(pEditor, xuiRichDocumentGetLength(pCellDocument), xuiRichDocumentGetLength(pCellDocument));
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pEditor);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRichEditEndTableCellEdit(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	return pData != NULL ? __xuiRichEditEndCellEdit(pWidget, pData, 1) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API xui_widget xuiRichEditGetTableCellEditor(xui_widget pWidget, xui_document_node_id_t* pTableId,
	int* pRow, int* pColumn)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	if ( pData == NULL ) return NULL;
	if ( pTableId != NULL ) *pTableId = pData->iActiveTableId;
	if ( pRow != NULL ) *pRow = pData->iActiveCellRow;
	if ( pColumn != NULL ) *pColumn = pData->iActiveCellColumn;
	return pData->pCellEditor;
}

XUI_API int xuiRichEditSetFontSet(xui_widget pWidget, const xui_rich_font_set_t* pFonts)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	if ( pData == NULL || pFonts == NULL || (pFonts->iSize != 0 && pFonts->iSize < sizeof(*pFonts)) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tFonts = *pFonts; pData->tFonts.iSize = sizeof(pData->tFonts);
	if ( pData->tFonts.pNormal == NULL ) pData->tFonts.pNormal = pData->pFont;
	__xuiRichEditClearFontCache(pWidget, pData);
	pData->bLayoutDirty = 1;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRichEditGetFontSet(xui_widget pWidget, xui_rich_font_set_t* pFonts)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	if ( pData == NULL || pFonts == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pFonts = pData->tFonts; pFonts->iSize = sizeof(*pFonts); return XUI_OK;
}

static uint32_t __xuiRichEditCommandFlag(int iCommand)
{
	switch ( iCommand ) {
	case XUI_RICH_COMMAND_BOLD: return XUI_RICH_STYLE_BOLD;
	case XUI_RICH_COMMAND_ITALIC: return XUI_RICH_STYLE_ITALIC;
	case XUI_RICH_COMMAND_UNDERLINE: return XUI_RICH_STYLE_UNDERLINE;
	case XUI_RICH_COMMAND_STRIKEOUT: return XUI_RICH_STYLE_STRIKEOUT;
	default: return 0;
	}
}

XUI_API int xuiRichEditQueryCommand(xui_widget pWidget, int iCommand, int* pState)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	uint32_t iFlag;
	xui_rich_layout_fragment_t tFragment;
	int s, e, i, bSeen = 0, bOn = 0, bOff = 0;
	if ( pData == NULL || pState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pState = XUI_RICH_COMMAND_STATE_OFF;
	if ( iCommand != XUI_RICH_COMMAND_COPY && iCommand != XUI_RICH_COMMAND_SELECT_ALL &&
	     iCommand != XUI_RICH_COMMAND_FIND && pData->bReadonly ) {
		*pState = XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK;
	}
	if ( iCommand == XUI_RICH_COMMAND_UNDO ) { *pState = xuiRichDocumentCanUndo(pData->pDocument) ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK; }
	if ( iCommand == XUI_RICH_COMMAND_REDO ) { *pState = xuiRichDocumentCanRedo(pData->pDocument) ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK; }
	__xuiRichEditSelection(pData, &s, &e);
	if ( iCommand == XUI_RICH_COMMAND_COPY || iCommand == XUI_RICH_COMMAND_CUT ) { *pState = s != e ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK; }
	if ( iCommand == XUI_RICH_COMMAND_DELETE ) { *pState = s != e ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK; }
	if ( iCommand == XUI_RICH_COMMAND_PASTE ) { *pState = XUI_RICH_COMMAND_STATE_ON; return XUI_OK; }
	if ( iCommand == XUI_RICH_COMMAND_FIND || iCommand == XUI_RICH_COMMAND_REPLACE ) { *pState = XUI_RICH_COMMAND_STATE_ON; return XUI_OK; }
	if ( iCommand == XUI_RICH_COMMAND_SELECT_ALL ) {
		int iLength = xuiRichDocumentGetLength(pData->pDocument);
		*pState = iLength > 0 && !(s == 0 && e == iLength) ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED;
		return XUI_OK;
	}
	iFlag = __xuiRichEditCommandFlag(iCommand);
	if ( iFlag != 0 ) {
		if ( s == e ) { xui_rich_text_style_t tStyle; (void)__xuiRichEditStyleAt(pData, s, &tStyle); *pState = (tStyle.iFlags & iFlag) ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; return XUI_OK; }
		(void)__xuiRichEditEnsureLayout(pWidget, pData);
		for ( i = 0; i < pData->iTotalFragmentCount; i++ ) {
			xui_rich_layout_fragment_t* pFragment = &tFragment;
			(void)__xuiRichEditGetLayoutFragment(pData, i, pFragment);
			if ( pFragment->tPublic.iNodeType != XUI_RICH_NODE_TEXT && pFragment->tPublic.iNodeType != XUI_RICH_NODE_LINK ) continue;
			if ( e <= pFragment->tPublic.iDocumentStart || s >= pFragment->tPublic.iDocumentEnd ) continue;
			bSeen = 1; if ( (pFragment->iStyleFlags & iFlag) != 0 ) bOn = 1; else bOff = 1;
		}
		*pState = bOn && bOff ? XUI_RICH_COMMAND_STATE_MIXED : (bSeen && bOn ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF);
		return XUI_OK;
	}
	if ( iCommand >= XUI_RICH_COMMAND_ALIGN_LEFT && iCommand <= XUI_RICH_COMMAND_ALIGN_JUSTIFY ) {
		xui_rich_node_info_t tInfo; int iAlign = iCommand - XUI_RICH_COMMAND_ALIGN_LEFT;
		if ( __xuiRichEditParagraphAt(pData, pData->iCaret, NULL, &tInfo) == XUI_OK )
			*pState = tInfo.tParagraphStyle.iAlign == iAlign ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF;
		return XUI_OK;
	}
	if ( iCommand >= XUI_RICH_COMMAND_PARAGRAPH && iCommand <= XUI_RICH_COMMAND_OUTDENT_LIST ) {
		xui_rich_node_info_t tInfo;
		if ( __xuiRichEditParagraphAt(pData, pData->iCaret, NULL, &tInfo) != XUI_OK ) {
			*pState = XUI_RICH_COMMAND_STATE_DISABLED; return XUI_OK;
		}
		switch ( iCommand ) {
		case XUI_RICH_COMMAND_PARAGRAPH: *pState = tInfo.iType == XUI_RICH_NODE_PARAGRAPH ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_HEADING_1: case XUI_RICH_COMMAND_HEADING_2: case XUI_RICH_COMMAND_HEADING_3:
			*pState = tInfo.iType == XUI_RICH_NODE_HEADING && tInfo.tParagraphStyle.iHeadingLevel == iCommand - XUI_RICH_COMMAND_HEADING_1 + 1 ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_BLOCK_QUOTE: *pState = tInfo.iType == XUI_RICH_NODE_BLOCK_QUOTE ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_BULLET_LIST: *pState = tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_BULLET ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_NUMBER_LIST: *pState = tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_NUMBER ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_CHECK_LIST: *pState = tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.iListType == XUI_RICH_LIST_CHECK ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_OFF; break;
		case XUI_RICH_COMMAND_INDENT_LIST:
			*pState = tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.iListLevel < 16 ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; break;
		case XUI_RICH_COMMAND_OUTDENT_LIST:
			*pState = tInfo.iType == XUI_RICH_NODE_LIST_ITEM && tInfo.tParagraphStyle.iListLevel > 0 ? XUI_RICH_COMMAND_STATE_ON : XUI_RICH_COMMAND_STATE_DISABLED; break;
		default: break;
		}
		return XUI_OK;
	}
	return XUI_OK;
}

XUI_API int xuiRichEditExecuteCommand(xui_widget pWidget, int iCommand, const void* pCommandData)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rich_text_style_t tStyle;
	xui_rich_paragraph_style_t tParagraph;
	xui_rich_node_info_t tBlockInfo;
	uint32_t iFlag;
	int s, e, iState, iType = XUI_RICH_NODE_PARAGRAPH;
	(void)pCommandData;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly && iCommand != XUI_RICH_COMMAND_COPY &&
	     iCommand != XUI_RICH_COMMAND_SELECT_ALL && iCommand != XUI_RICH_COMMAND_FIND )
		return XUI_ERROR_UNSUPPORTED;
	switch ( iCommand ) {
	case XUI_RICH_COMMAND_UNDO: return xuiRichEditUndo(pWidget);
	case XUI_RICH_COMMAND_REDO: return xuiRichEditRedo(pWidget);
	case XUI_RICH_COMMAND_CUT: return xuiRichEditCut(pWidget);
	case XUI_RICH_COMMAND_COPY: return xuiRichEditCopy(pWidget);
	case XUI_RICH_COMMAND_PASTE: return xuiRichEditPaste(pWidget);
	case XUI_RICH_COMMAND_DELETE:
		if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
		__xuiRichEditSelection(pData, &s, &e);
		return s != e ? __xuiRichEditReplaceSelection(pWidget, pData, "") : XUI_OK;
	case XUI_RICH_COMMAND_SELECT_ALL: return xuiRichEditSelectAll(pWidget);
	case XUI_RICH_COMMAND_FIND: return xuiRichEditOpenFind(pWidget);
	case XUI_RICH_COMMAND_REPLACE: return xuiRichEditOpenReplace(pWidget);
	case XUI_RICH_COMMAND_INDENT_LIST: return pData->bReadonly ? XUI_ERROR_UNSUPPORTED : __xuiRichEditListLevel(pData, 1);
	case XUI_RICH_COMMAND_OUTDENT_LIST: return pData->bReadonly ? XUI_ERROR_UNSUPPORTED : __xuiRichEditListLevel(pData, -1);
	default: break;
	}
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	__xuiRichEditSelection(pData, &s, &e);
	iFlag = __xuiRichEditCommandFlag(iCommand);
	if ( iFlag != 0 ) {
		(void)xuiRichEditQueryCommand(pWidget, iCommand, &iState);
		if ( s == e ) { (void)__xuiRichEditStyleAt(pData, s, &tStyle); if ( iState == XUI_RICH_COMMAND_STATE_ON ) tStyle.iFlags &= ~iFlag; else tStyle.iFlags |= iFlag; pData->tTypingStyle = tStyle; pData->bTypingStyleOverride = 1; return XUI_OK; }
		return xuiRichDocumentUpdateStyleFlags(pData->pDocument, s, e,
			iState == XUI_RICH_COMMAND_STATE_ON ? 0 : iFlag, iState == XUI_RICH_COMMAND_STATE_ON ? iFlag : 0);
	}
	memset(&tBlockInfo, 0, sizeof(tBlockInfo)); tBlockInfo.iSize = sizeof(tBlockInfo);
	if ( __xuiRichEditParagraphAt(pData, pData->iCaret, NULL, &tBlockInfo) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	tParagraph = tBlockInfo.tParagraphStyle; tParagraph.iSize = sizeof(tParagraph);
	if ( iCommand >= XUI_RICH_COMMAND_ALIGN_LEFT && iCommand <= XUI_RICH_COMMAND_ALIGN_JUSTIFY )
		tParagraph.iAlign = iCommand - XUI_RICH_COMMAND_ALIGN_LEFT;
	else {
		switch ( iCommand ) {
		case XUI_RICH_COMMAND_PARAGRAPH: iType = XUI_RICH_NODE_PARAGRAPH; tParagraph.iHeadingLevel = 0; tParagraph.iListType = XUI_RICH_LIST_NONE; tParagraph.bListChecked = 0; break;
		case XUI_RICH_COMMAND_HEADING_1: iType = XUI_RICH_NODE_HEADING; tParagraph.iHeadingLevel = 1; break;
		case XUI_RICH_COMMAND_HEADING_2: iType = XUI_RICH_NODE_HEADING; tParagraph.iHeadingLevel = 2; break;
		case XUI_RICH_COMMAND_HEADING_3: iType = XUI_RICH_NODE_HEADING; tParagraph.iHeadingLevel = 3; break;
		case XUI_RICH_COMMAND_BLOCK_QUOTE: iType = XUI_RICH_NODE_BLOCK_QUOTE; break;
		case XUI_RICH_COMMAND_BULLET_LIST: iType = XUI_RICH_NODE_LIST_ITEM; tParagraph.iListType = XUI_RICH_LIST_BULLET; tParagraph.bListChecked = 0; break;
		case XUI_RICH_COMMAND_NUMBER_LIST: iType = XUI_RICH_NODE_LIST_ITEM; tParagraph.iListType = XUI_RICH_LIST_NUMBER; tParagraph.bListChecked = 0; break;
		case XUI_RICH_COMMAND_CHECK_LIST: iType = XUI_RICH_NODE_LIST_ITEM; tParagraph.iListType = XUI_RICH_LIST_CHECK; break;
		default: return XUI_ERROR_UNSUPPORTED;
		}
		(void)xuiRichDocumentBeginTransaction(pData->pDocument);
		(void)xuiRichDocumentSetBlockType(pData->pDocument, s, e, iType);
		{
			int iRet = xuiRichDocumentApplyParagraphStyle(pData->pDocument, s, e, &tParagraph);
			(void)xuiRichDocumentEndTransaction(pData->pDocument);
			return iRet;
		}
	}
	return xuiRichDocumentApplyParagraphStyle(pData->pDocument, s, e, &tParagraph);
}

typedef struct xui_rich_toolbar_command_t {
	const char* sText;
	int iTextId;
	int iCommand;
	uint32_t iGroup;
	int bToggle;
} xui_rich_toolbar_command_t;

static const xui_rich_toolbar_command_t g_arrXuiRichToolbarCommands[] = {
	{"Undo",XUI_TR_EDIT_UNDO,XUI_RICH_COMMAND_UNDO,XUI_RICH_TOOLBAR_HISTORY,0},
	{"Redo",XUI_TR_EDIT_REDO,XUI_RICH_COMMAND_REDO,XUI_RICH_TOOLBAR_HISTORY,0},
	{"B",XUI_TR_RICH_BOLD,XUI_RICH_COMMAND_BOLD,XUI_RICH_TOOLBAR_INLINE_FORMAT,1},
	{"I",XUI_TR_RICH_ITALIC,XUI_RICH_COMMAND_ITALIC,XUI_RICH_TOOLBAR_INLINE_FORMAT,1},
	{"U",XUI_TR_RICH_UNDERLINE,XUI_RICH_COMMAND_UNDERLINE,XUI_RICH_TOOLBAR_INLINE_FORMAT,1},
	{"S",XUI_TR_RICH_STRIKEOUT,XUI_RICH_COMMAND_STRIKEOUT,XUI_RICH_TOOLBAR_INLINE_FORMAT,1},
	{"Left",XUI_TR_RICH_ALIGN_LEFT,XUI_RICH_COMMAND_ALIGN_LEFT,XUI_RICH_TOOLBAR_ALIGNMENT,1},
	{"Center",XUI_TR_RICH_ALIGN_CENTER,XUI_RICH_COMMAND_ALIGN_CENTER,XUI_RICH_TOOLBAR_ALIGNMENT,1},
	{"Right",XUI_TR_RICH_ALIGN_RIGHT,XUI_RICH_COMMAND_ALIGN_RIGHT,XUI_RICH_TOOLBAR_ALIGNMENT,1},
	{"Justify",XUI_TR_RICH_ALIGN_JUSTIFY,XUI_RICH_COMMAND_ALIGN_JUSTIFY,XUI_RICH_TOOLBAR_ALIGNMENT,1},
	{"P",XUI_TR_RICH_PARAGRAPH,XUI_RICH_COMMAND_PARAGRAPH,XUI_RICH_TOOLBAR_BLOCKS,1},
	{"H1",XUI_TR_RICH_HEADING_1,XUI_RICH_COMMAND_HEADING_1,XUI_RICH_TOOLBAR_BLOCKS,1},
	{"H2",XUI_TR_RICH_HEADING_2,XUI_RICH_COMMAND_HEADING_2,XUI_RICH_TOOLBAR_BLOCKS,1},
	{"H3",XUI_TR_RICH_HEADING_3,XUI_RICH_COMMAND_HEADING_3,XUI_RICH_TOOLBAR_BLOCKS,1},
	{"Quote",XUI_TR_RICH_BLOCK_QUOTE,XUI_RICH_COMMAND_BLOCK_QUOTE,XUI_RICH_TOOLBAR_BLOCKS,1},
	{"Bullet",XUI_TR_RICH_BULLET_LIST,XUI_RICH_COMMAND_BULLET_LIST,XUI_RICH_TOOLBAR_LISTS,1},
	{"Number",XUI_TR_RICH_NUMBER_LIST,XUI_RICH_COMMAND_NUMBER_LIST,XUI_RICH_TOOLBAR_LISTS,1},
	{"Check",XUI_TR_RICH_CHECK_LIST,XUI_RICH_COMMAND_CHECK_LIST,XUI_RICH_TOOLBAR_LISTS,1},
	{"Indent",XUI_TR_RICH_INDENT,XUI_RICH_COMMAND_INDENT_LIST,XUI_RICH_TOOLBAR_LISTS,0},
	{"Outdent",XUI_TR_RICH_OUTDENT,XUI_RICH_COMMAND_OUTDENT_LIST,XUI_RICH_TOOLBAR_LISTS,0},
	{"Find",XUI_TR_FIND_TITLE,XUI_RICH_COMMAND_FIND,XUI_RICH_TOOLBAR_FIND,0},
	{"Replace",XUI_TR_REPLACE_TITLE,XUI_RICH_COMMAND_REPLACE,XUI_RICH_TOOLBAR_FIND,0}
};

XUI_API int xuiRichEditSetupToolbar(xui_widget pWidget, xui_widget pToolbar, uint32_t iGroups)
{
	xui_toolbar_item_t arrItems[XUI_TOOLBAR_ITEM_CAPACITY];
	uint32_t iLastGroup = 0;
	int iCount = 0;
	int i;
	if ( __xuiRichEditData(pWidget) == NULL || pToolbar == NULL ||
	     !xuiWidgetIsType(pToolbar, xuiToolbarGetType(xuiWidgetGetContext(pWidget))) )
		return XUI_ERROR_INVALID_ARGUMENT;
	if ( iGroups == 0 ) iGroups = XUI_RICH_TOOLBAR_DEFAULT;
	memset(arrItems, 0, sizeof(arrItems));
	for ( i = 0; i < (int)(sizeof(g_arrXuiRichToolbarCommands) / sizeof(g_arrXuiRichToolbarCommands[0])); i++ ) {
		const xui_rich_toolbar_command_t* pCommand = &g_arrXuiRichToolbarCommands[i];
		if ( (iGroups & pCommand->iGroup) == 0 ) continue;
		if ( iLastGroup != 0 && iLastGroup != pCommand->iGroup ) {
			arrItems[iCount].iType = XUI_TOOLBAR_ITEM_SEPARATOR; iCount++;
		}
		arrItems[iCount].sText = pCommand->sText;
		arrItems[iCount].sTooltip = xuiTranslate(xuiWidgetGetContext(pWidget), pCommand->iTextId);
		arrItems[iCount].iType = pCommand->bToggle ? XUI_TOOLBAR_ITEM_TOGGLE : XUI_TOOLBAR_ITEM_BUTTON;
		arrItems[iCount].iState = XUI_TOOLBAR_ITEM_ENABLED;
		arrItems[iCount].iValue = pCommand->iCommand;
		arrItems[iCount].iGroup = (int)pCommand->iGroup;
		iCount++; iLastGroup = pCommand->iGroup;
	}
	if ( xuiToolbarSetItems(pToolbar, arrItems, iCount) != XUI_OK ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiRichEditSyncToolbar(pWidget, pToolbar);
}

XUI_API int xuiRichEditSyncToolbar(xui_widget pWidget, xui_widget pToolbar)
{
	int iCount;
	int i;
	if ( __xuiRichEditData(pWidget) == NULL || pToolbar == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iCount = xuiToolbarGetItemCount(pToolbar);
	for ( i = 0; i < iCount; i++ ) {
		const xui_toolbar_item_t* pItem = xuiToolbarGetItem(pToolbar, i);
		const xui_rich_toolbar_command_t* pCommand = NULL;
		int iState;
		int j;
		if ( pItem == NULL || pItem->iType == XUI_TOOLBAR_ITEM_SEPARATOR ) continue;
		for ( j = 0; j < (int)(sizeof(g_arrXuiRichToolbarCommands) / sizeof(g_arrXuiRichToolbarCommands[0])); j++ ) {
			if ( g_arrXuiRichToolbarCommands[j].iCommand == pItem->iValue ) {
				pCommand = &g_arrXuiRichToolbarCommands[j]; break;
			}
		}
		if ( pCommand != NULL ) (void)xuiToolbarSetItemTooltip(pToolbar, i,
			xuiTranslate(xuiWidgetGetContext(pWidget), pCommand->iTextId));
		if ( xuiRichEditQueryCommand(pWidget, pItem->iValue, &iState) != XUI_OK ) continue;
		(void)xuiToolbarSetItemEnabled(pToolbar, i, iState != XUI_RICH_COMMAND_STATE_DISABLED);
		(void)xuiToolbarSetItemChecked(pToolbar, i,
			iState == XUI_RICH_COMMAND_STATE_ON || iState == XUI_RICH_COMMAND_STATE_MIXED);
	}
	return XUI_OK;
}

XUI_API int xuiRichEditExecuteToolbarItem(xui_widget pWidget, xui_widget pToolbar, int iIndex)
{
	const xui_toolbar_item_t* pItem;
	int iRet;
	if ( __xuiRichEditData(pWidget) == NULL || pToolbar == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = xuiToolbarGetItem(pToolbar, iIndex);
	if ( pItem == NULL || pItem->iType == XUI_TOOLBAR_ITEM_SEPARATOR ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiRichEditExecuteCommand(pWidget, pItem->iValue, NULL);
	(void)xuiRichEditSyncToolbar(pWidget, pToolbar);
	return iRet;
}
XUI_API int xuiRichEditCopy(xui_widget pWidget) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); return pData != NULL ? __xuiRichEditCopyRange(pWidget,pData) : XUI_ERROR_INVALID_ARGUMENT; }
XUI_API int xuiRichEditCut(xui_widget pWidget) { xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget); int r; if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT; if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED; r=__xuiRichEditCopyRange(pWidget,pData); return r==XUI_OK ? __xuiRichEditReplaceSelection(pWidget,pData,"") : r; }

static int __xuiRichEditFindResolve(xui_rich_edit_data_t* pData,
	const xui_find_options_t* pOptions, int bBackward, const char** psPattern,
	const char** psReplacement, uint32_t* pFlags, int* pStartOffset,
	int* pRangeStart, int* pRangeEnd)
{
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iLength;
	int iStart;
	int iEnd;
	int iSelStart;
	int iSelEnd;
	if ( pData == NULL || psPattern == NULL || pFlags == NULL || pStartOffset == NULL ||
	     pRangeStart == NULL || pRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sPattern = pOptions != NULL && pOptions->sPattern != NULL ? pOptions->sPattern : pData->sFindPattern;
	sReplacement = pOptions != NULL && pOptions->sReplacement != NULL ? pOptions->sReplacement : pData->sFindReplacement;
	if ( sPattern == NULL || sPattern[0] == 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iFlags = pOptions != NULL ? pOptions->iFlags : pData->iFindFlags;
	if ( bBackward ) iFlags |= XUI_FIND_BACKWARD; else iFlags &= ~XUI_FIND_BACKWARD;
	iLength = xuiRichDocumentGetLength(pData->pDocument);
	__xuiRichEditSelection(pData, &iSelStart, &iSelEnd);
	if ( (iFlags & XUI_FIND_SELECTION) != 0 ) {
		if ( pData->iFindRangeEnd > pData->iFindRangeStart ) { iStart = pData->iFindRangeStart; iEnd = pData->iFindRangeEnd; }
		else if ( iSelEnd > iSelStart ) { iStart = iSelStart; iEnd = iSelEnd; }
		else return XUI_ERROR_UNSUPPORTED;
	} else { iStart = 0; iEnd = iLength; }
	if ( pOptions != NULL && (iFlags & XUI_FIND_SELECTION) == 0 && pOptions->iRangeEnd > pOptions->iRangeStart ) {
		iStart = pOptions->iRangeStart; iEnd = pOptions->iRangeEnd;
	}
	if ( iStart < 0 ) iStart = 0;
	if ( iEnd > iLength ) iEnd = iLength;
	if ( iEnd < iStart ) { int iSwap = iStart; iStart = iEnd; iEnd = iSwap; }
	*pStartOffset = bBackward ? (iSelEnd > iSelStart ? iSelStart : pData->iCaret) :
		(iSelEnd > iSelStart ? iSelEnd : pData->iCaret);
	if ( pOptions != NULL && pOptions->iStartOffset > 0 ) *pStartOffset = pOptions->iStartOffset;
	if ( *pStartOffset < iStart ) *pStartOffset = iStart;
	if ( *pStartOffset > iEnd ) *pStartOffset = iEnd;
	*psPattern = sPattern;
	if ( psReplacement != NULL ) *psReplacement = sReplacement != NULL ? sReplacement : "";
	*pFlags = iFlags; *pRangeStart = iStart; *pRangeEnd = iEnd;
	return XUI_OK;
}

static int __xuiRichEditStoreFind(xui_rich_edit_data_t* pData, const char* sPattern,
	const char* sReplacement, uint32_t iFlags, int iRangeStart, int iRangeEnd)
{
	int iRet = __xuiRichEditStringSet(&pData->sFindPattern, &pData->iFindPatternCapacity, sPattern);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditStringSet(&pData->sFindReplacement,
		&pData->iFindReplacementCapacity, sReplacement != NULL ? sReplacement : "");
	if ( iRet != XUI_OK ) return iRet;
	pData->iFindFlags = iFlags;
	pData->iFindRangeStart = (iFlags & XUI_FIND_SELECTION) != 0 ? iRangeStart : 0;
	pData->iFindRangeEnd = (iFlags & XUI_FIND_SELECTION) != 0 ? iRangeEnd : 0;
	return XUI_OK;
}

static int __xuiRichEditCollectFind(xui_widget pWidget, xui_rich_edit_data_t* pData,
	const char* sPattern, uint32_t iFlags, int iRangeStart, int iRangeEnd,
	const xui_find_result_t* pActive, int iMaxResults)
{
	const char* sText = xuiRichDocumentGetText(pData->pDocument);
	int iCount = 0;
	int iStoreCount;
	int i;
	int iRet = xuiFindCollectText(sText, -1, sPattern, iRangeStart, iRangeEnd,
		iFlags, NULL, 0, &iCount, NULL, 0);
	if ( iRet != XUI_OK ) return iRet;
	iStoreCount = iCount;
	if ( iMaxResults > 0 && iStoreCount > iMaxResults ) iStoreCount = iMaxResults;
	iRet = __xuiRichEditFindResultReserve(pData, iStoreCount);
	if ( iRet != XUI_OK ) return iRet;
	pData->iFindResultCount = 0; pData->iFindActiveIndex = -1;
	if ( iStoreCount > 0 ) {
		iRet = xuiFindCollectText(sText, -1, sPattern, iRangeStart, iRangeEnd, iFlags,
			pData->pFindResults, iStoreCount, &iCount, NULL, 0);
		if ( iRet != XUI_OK ) return iRet;
		pData->iFindResultCount = iStoreCount;
	}
	if ( pActive != NULL ) for ( i = 0; i < pData->iFindResultCount; i++ ) {
		if ( pData->pFindResults[i].iStart == pActive->iStart && pData->pFindResults[i].iEnd == pActive->iEnd ) {
			pData->iFindActiveIndex = i; break;
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiRichEditFindMove(xui_widget pWidget, const xui_find_options_t* pOptions, int bBackward)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_find_result_t tResult;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset, iRangeStart, iRangeEnd, iRet;
	int iMaxResults = pOptions != NULL ? pOptions->iMaxResults : 0;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditFindResolve(pData, pOptions, bBackward, &sPattern, &sReplacement,
		&iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tResult, 0, sizeof(tResult)); tResult.iSize = sizeof(tResult);
	iRet = xuiFindText(xuiRichDocumentGetText(pData->pDocument), -1, sPattern,
		iStartOffset, iRangeStart, iRangeEnd, iFlags | XUI_FIND_WRAP, &tResult, NULL, 0);
	if ( iRet != XUI_OK ) { __xuiRichEditClearFindData(pData); return iRet; }
	iRet = __xuiRichEditStoreFind(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCollectFind(pWidget, pData, sPattern, iFlags,
		iRangeStart, iRangeEnd, &tResult, iMaxResults);
	if ( iRet == XUI_OK ) iRet = xuiRichEditSetSelection(pWidget, tResult.iStart, tResult.iEnd);
	return iRet;
}

static int __xuiRichEditClipboardDocument(xui_proxy pProxy, const char* sFormat,
	int bHtml, xui_rich_document* ppDocument)
{
	char* sData;
	int iSize;
	int iRead;
	int iRet;
	if ( pProxy == NULL || pProxy->clipboardGetData == NULL || ppDocument == NULL ) return XUI_ERROR_UNSUPPORTED;
	*ppDocument = NULL;
	iSize = pProxy->clipboardGetData(pProxy, sFormat, NULL, 0u);
	if ( iSize <= 0 ) return iSize < 0 ? iSize : XUI_ERROR_FILE_NOT_FOUND;
	sData = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( sData == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRead = pProxy->clipboardGetData(pProxy, sFormat, sData, (size_t)iSize);
	if ( iRead != iSize ) { xrtFree(sData); return iRead < 0 ? iRead : XUI_ERROR_RESOURCE_FAILED; }
	sData[iSize] = 0;
	iRet = bHtml ? xuiRichDocumentImportHtml(sData, (size_t)iSize, ppDocument) :
		xuiRichDocumentDeserialize(sData, (size_t)iSize, ppDocument);
	xrtFree(sData);
	return iRet;
}

XUI_API int xuiRichEditPaste(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_proxy pProxy;
	xui_rich_document pFragment = NULL;
	char* s = NULL;
	int n = 0;
	int r;
	int iStart;
	int iEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	r = __xuiRichEditClipboardDocument(pProxy, XUI_CLIPBOARD_FORMAT_RICH_DOCUMENT, 0, &pFragment);
	if ( r != XUI_OK ) r = __xuiRichEditClipboardDocument(pProxy, XUI_CLIPBOARD_FORMAT_HTML, 1, &pFragment);
	if ( r == XUI_OK && pFragment != NULL ) {
		__xuiRichEditSelection(pData, &iStart, &iEnd);
		r = xuiRichDocumentInsertDocument(pData->pDocument, iStart, iEnd, pFragment);
		if ( r == XUI_OK ) {
			pData->iAnchor = pData->iCaret = iStart + xuiRichDocumentGetLength(pFragment);
			pData->bLayoutDirty = 1;
		}
		xuiRichDocumentDestroy(pFragment);
		return r;
	}
	if ( pFragment != NULL ) xuiRichDocumentDestroy(pFragment);
	r = xuiInternalClipboardReadText(xuiWidgetGetContext(pWidget), &s, &n);
	if ( r == XUI_OK ) r = __xuiRichEditReplaceSelection(pWidget, pData, s != NULL ? s : "");
	if ( s != NULL ) xrtFree(s);
	return r;
}

XUI_API int xuiRichEditFindNext(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiRichEditFindMove(pWidget, pOptions, 0);
}

XUI_API int xuiRichEditFindPrevious(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiRichEditFindMove(pWidget, pOptions, 1);
}

static int __xuiRichEditReplaceMatches(xui_widget pWidget, xui_rich_edit_data_t* pData,
	const char* sPattern, const char* sReplacement, uint32_t iFlags,
	xui_find_result_t* pResults, int iCount, int* pReplaceCount)
{
	const char* sText;
	char** arrReplacement;
	int* arrLength;
	int i;
	int iRet = XUI_OK;
	int iCaret = 0;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	if ( iCount <= 0 ) return XUI_OK;
	sText = xuiRichDocumentGetText(pData->pDocument);
	arrReplacement = (char**)xrtCalloc((size_t)iCount, sizeof(*arrReplacement));
	arrLength = (int*)xrtCalloc((size_t)iCount, sizeof(*arrLength));
	if ( arrReplacement == NULL || arrLength == NULL ) { xrtFree(arrReplacement); xrtFree(arrLength); return XUI_ERROR_OUT_OF_MEMORY; }
	for ( i = 0; i < iCount; i++ ) {
		iRet = xuiFindBuildReplacement(sText, -1, sPattern, sReplacement, iFlags,
			&pResults[i], &arrReplacement[i], &arrLength[i], NULL, 0);
		if ( iRet != XUI_OK ) break;
	}
	if ( iRet == XUI_OK ) {
		pData->bUpdatingFind = 1;
		iRet = xuiRichDocumentBeginTransaction(pData->pDocument);
		for ( i = iCount - 1; iRet == XUI_OK && i >= 0; i-- ) {
			xui_rich_text_style_t tStyle = pData->tTypingStyle;
			(void)__xuiRichEditStyleAt(pData, pResults[i].iStart, &tStyle);
			iRet = xuiRichDocumentReplace(pData->pDocument, pResults[i].iStart,
				pResults[i].iEnd, arrReplacement[i], &tStyle);
		}
		{
			int iEndRet = xuiRichDocumentEndTransaction(pData->pDocument);
			if ( iRet == XUI_OK ) iRet = iEndRet;
		}
		pData->bUpdatingFind = 0;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( i == 0 ) iCaret = pResults[i].iStart + arrLength[i];
		xuiFindFreeText(arrReplacement[i]);
	}
	xrtFree(arrReplacement); xrtFree(arrLength);
	if ( iRet == XUI_OK ) {
		pData->iAnchor = pData->iCaret = iCaret;
		if ( pReplaceCount != NULL ) *pReplaceCount = iCount;
		__xuiRichEditClearFindData(pData);
		__xuiRichEditEnsureCaretVisible(pWidget, pData);
	}
	return iRet;
}

XUI_API int xuiRichEditReplaceCurrent(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_find_result_t tResult;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset, iRangeStart, iRangeEnd, iStart, iEnd, iRet, iCount;
	int iOldLength;
	int iNewRangeEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiRichEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement,
		&iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	__xuiRichEditSelection(pData, &iStart, &iEnd);
	if ( iStart == iEnd ) {
		iRet = __xuiRichEditFindMove(pWidget, pOptions, 0);
		if ( iRet != XUI_OK ) return iRet;
		__xuiRichEditSelection(pData, &iStart, &iEnd);
	}
	memset(&tResult, 0, sizeof(tResult)); tResult.iSize = sizeof(tResult);
	iRet = xuiFindText(xuiRichDocumentGetText(pData->pDocument), -1, sPattern,
		iStart, iStart, iEnd, iFlags & ~XUI_FIND_WRAP, &tResult, NULL, 0);
	if ( iRet != XUI_OK || tResult.iStart != iStart || tResult.iEnd != iEnd ) return XUI_ERROR_UNSUPPORTED;
	iOldLength = xuiRichDocumentGetLength(pData->pDocument);
	iRet = __xuiRichEditReplaceMatches(pWidget, pData, sPattern, sReplacement, iFlags, &tResult, 1, &iCount);
	if ( iRet == XUI_OK ) {
		iNewRangeEnd = (iFlags & XUI_FIND_SELECTION) != 0 ?
			iRangeEnd + xuiRichDocumentGetLength(pData->pDocument) - iOldLength :
			xuiRichDocumentGetLength(pData->pDocument);
		(void)__xuiRichEditStoreFind(pData, sPattern, sReplacement, iFlags, iRangeStart, iNewRangeEnd);
		(void)__xuiRichEditCollectFind(pWidget, pData, sPattern, iFlags, iRangeStart,
			iNewRangeEnd, NULL,
			pOptions != NULL ? pOptions->iMaxResults : 0);
	}
	return iRet;
}

XUI_API int xuiRichEditReplaceAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pReplaceCount)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_find_result_t* pResults = NULL;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset, iRangeStart, iRangeEnd, iCount = 0, iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	iRet = __xuiRichEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement,
		&iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiFindCollectText(xuiRichDocumentGetText(pData->pDocument), -1, sPattern,
		iRangeStart, iRangeEnd, iFlags, NULL, 0, &iCount, NULL, 0);
	if ( iRet != XUI_OK || iCount == 0 ) return iRet;
	pResults = (xui_find_result_t*)xrtMalloc((size_t)iCount * sizeof(*pResults));
	if ( pResults == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiFindCollectText(xuiRichDocumentGetText(pData->pDocument), -1, sPattern,
		iRangeStart, iRangeEnd, iFlags, pResults, iCount, &iCount, NULL, 0);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditReplaceMatches(pWidget, pData, sPattern,
		sReplacement, iFlags, pResults, iCount, pReplaceCount);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditStoreFind(pData, sPattern, sReplacement,
		iFlags, iRangeStart, iRangeEnd);
	xrtFree(pResults);
	return iRet;
}

XUI_API int xuiRichEditClearFind(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiRichEditClearFindData(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRichEditGetFindResultCount(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	return pData != NULL ? pData->iFindResultCount : 0;
}

XUI_API int xuiRichEditGetFindResult(xui_widget pWidget, int iIndex, xui_find_result_t* pResult)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	if ( pData == NULL || pResult == NULL || iIndex < 0 || iIndex >= pData->iFindResultCount )
		return XUI_ERROR_INVALID_ARGUMENT;
	*pResult = pData->pFindResults[iIndex]; pResult->iSize = sizeof(*pResult);
	return XUI_OK;
}

static void __xuiRichEditFindWindowOptions(xui_rich_edit_data_t* pData, xui_find_options_t* pOptions)
{
	uint32_t iFlags = 0u;
	memset(pOptions, 0, sizeof(*pOptions)); pOptions->iSize = sizeof(*pOptions);
	pOptions->sPattern = pData->pFindInput != NULL ? xuiInputGetText(pData->pFindInput) : pData->sFindPattern;
	pOptions->sReplacement = pData->pReplaceInput != NULL ? xuiInputGetText(pData->pReplaceInput) : pData->sFindReplacement;
	pOptions->iStartOffset = -1;
	if ( pData->pCaseCheck != NULL && xuiCheckBoxGetChecked(pData->pCaseCheck) ) iFlags |= XUI_FIND_CASE_SENSITIVE;
	if ( pData->pWordCheck != NULL && xuiCheckBoxGetChecked(pData->pWordCheck) ) iFlags |= XUI_FIND_WHOLE_WORD;
	if ( pData->pRegexCheck != NULL && xuiCheckBoxGetChecked(pData->pRegexCheck) ) iFlags |= XUI_FIND_REGEX;
	if ( pData->pEscapeCheck != NULL && xuiCheckBoxGetChecked(pData->pEscapeCheck) ) iFlags |= XUI_FIND_ESCAPE;
	if ( pData->pSelectionCheck != NULL && xuiCheckBoxGetChecked(pData->pSelectionCheck) ) iFlags |= XUI_FIND_SELECTION;
	pOptions->iFlags = iFlags;
}

static void __xuiRichEditFindStatus(xui_rich_edit_data_t* pData, const char* sText)
{
	if ( pData != NULL && pData->pFindStatus != NULL ) (void)xuiLabelSetText(pData->pFindStatus, sText != NULL ? sText : "");
}

static void __xuiRichEditFindApplyLanguage(xui_widget pRichEdit, xui_rich_edit_data_t* pData)
{
	xui_context pContext;
	if ( pRichEdit == NULL || pData == NULL || pData->pFindWindow == NULL ) return;
	pContext = xuiWidgetGetContext(pRichEdit);
	pData->iFindLanguageRevision = xuiGetLanguageRevision(pContext);
	(void)xuiWindowSetTitle(pData->pFindWindow, xuiTranslate(pContext,
		pData->bFindWindowReplace ? XUI_TR_REPLACE_TITLE : XUI_TR_FIND_TITLE));
	(void)xuiInputSetPlaceholder(pData->pFindInput, xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER));
	(void)xuiInputSetPlaceholder(pData->pReplaceInput, xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER));
	(void)xuiButtonSetText(pData->pFindPrevButton, xuiTranslate(pContext, XUI_TR_FIND_PREVIOUS));
	(void)xuiButtonSetText(pData->pFindNextButton, xuiTranslate(pContext, XUI_TR_FIND_NEXT));
	(void)xuiButtonSetText(pData->pReplaceButton, xuiTranslate(pContext, XUI_TR_REPLACE_CURRENT));
	(void)xuiButtonSetText(pData->pReplaceAllButton, xuiTranslate(pContext, XUI_TR_REPLACE_ALL));
	(void)xuiCheckBoxSetText(pData->pCaseCheck, xuiTranslate(pContext, XUI_TR_FIND_CASE));
	(void)xuiCheckBoxSetText(pData->pWordCheck, xuiTranslate(pContext, XUI_TR_FIND_WORD));
	(void)xuiCheckBoxSetText(pData->pRegexCheck, xuiTranslate(pContext, XUI_TR_FIND_REGEX));
	(void)xuiCheckBoxSetText(pData->pEscapeCheck, xuiTranslate(pContext, XUI_TR_FIND_ESCAPE));
	(void)xuiCheckBoxSetText(pData->pSelectionCheck, xuiTranslate(pContext, XUI_TR_FIND_SELECTION));
}

static void __xuiRichEditFindRefresh(xui_widget pRichEdit)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pRichEdit);
	xui_find_options_t tOptions;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset, iRangeStart, iRangeEnd, iRet;
	char sStatus[64];
	if ( pData == NULL ) return;
	__xuiRichEditFindWindowOptions(pData, &tOptions);
	if ( tOptions.sPattern == NULL || tOptions.sPattern[0] == 0 ) {
		(void)xuiRichEditClearFind(pRichEdit); __xuiRichEditFindStatus(pData, ""); return;
	}
	iRet = __xuiRichEditFindResolve(pData, &tOptions, 0, &sPattern, &sReplacement,
		&iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditStoreFind(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCollectFind(pRichEdit, pData, sPattern,
		iFlags, iRangeStart, iRangeEnd, NULL, 0);
	if ( iRet != XUI_OK ) {
		(void)xuiRichEditClearFind(pRichEdit);
		__xuiRichEditFindStatus(pData, xuiTranslate(xuiWidgetGetContext(pRichEdit),
			iRet == XUI_ERROR_UNSUPPORTED ? XUI_TR_FIND_NO_SEARCH_RANGE : XUI_TR_FIND_INVALID_PATTERN));
		return;
	}
	snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pRichEdit),
		XUI_TR_FIND_MATCHES_FMT), pData->iFindResultCount);
	__xuiRichEditFindStatus(pData, sStatus);
}

static void __xuiRichEditFindInputChange(xui_widget pWidget, const char* sText, void* pUser)
{
	(void)pWidget; (void)sText; __xuiRichEditFindRefresh((xui_widget)pUser);
}

static void __xuiRichEditFindCheckChange(xui_widget pWidget, int bChecked, void* pUser)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData((xui_widget)pUser);
	int iStart, iEnd;
	(void)pWidget; (void)bChecked;
	if ( pData != NULL ) {
		pData->iFindRangeStart = pData->iFindRangeEnd = 0;
		if ( pData->pSelectionCheck != NULL && xuiCheckBoxGetChecked(pData->pSelectionCheck) ) {
			__xuiRichEditSelection(pData, &iStart, &iEnd);
			if ( iEnd > iStart ) { pData->iFindRangeStart = iStart; pData->iFindRangeEnd = iEnd; }
		}
	}
	__xuiRichEditFindRefresh((xui_widget)pUser);
}

static void __xuiRichEditFindButtonClick(xui_widget pButton, void* pUser)
{
	xui_widget pRichEdit = (xui_widget)pUser;
	xui_rich_edit_data_t* pData = __xuiRichEditData(pRichEdit);
	xui_find_options_t tOptions;
	char sStatus[64];
	int iCount = 0;
	int iRet = XUI_OK;
	if ( pData == NULL ) return;
	__xuiRichEditFindWindowOptions(pData, &tOptions);
	if ( pButton == pData->pFindPrevButton ) iRet = xuiRichEditFindPrevious(pRichEdit, &tOptions);
	else if ( pButton == pData->pFindNextButton ) iRet = xuiRichEditFindNext(pRichEdit, &tOptions);
	else if ( pButton == pData->pReplaceButton ) iRet = xuiRichEditReplaceCurrent(pRichEdit, &tOptions);
	else if ( pButton == pData->pReplaceAllButton ) iRet = xuiRichEditReplaceAll(pRichEdit, &tOptions, &iCount);
	if ( iRet == XUI_OK ) {
		snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pRichEdit),
			pButton == pData->pReplaceAllButton ? XUI_TR_FIND_REPLACED_FMT : XUI_TR_FIND_MATCHES_FMT),
			pButton == pData->pReplaceAllButton ? iCount : pData->iFindResultCount);
		__xuiRichEditFindStatus(pData, sStatus);
	} else __xuiRichEditFindStatus(pData, xuiTranslate(xuiWidgetGetContext(pRichEdit), XUI_TR_FIND_NOT_FOUND));
}

static void __xuiRichEditFindWindowClose(xui_widget pWindow, void* pUser)
{
	xui_widget pRichEdit = (xui_widget)pUser;
	(void)pWindow;
	if ( pRichEdit != NULL ) (void)xuiSetFocusWidget(xuiWidgetGetContext(pRichEdit), pRichEdit);
}

static int __xuiRichEditFindWindowKey(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pRichEdit = (xui_widget)pUser;
	xui_rich_edit_data_t* pData = __xuiRichEditData(pRichEdit);
	int iKey;
	(void)pWidget;
	if ( pEvent == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iKey = pEvent->iKey; if ( iKey >= 'a' && iKey <= 'z' ) iKey -= 'a' - 'A';
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 && iKey == 'F' ) { (void)xuiRichEditOpenFind(pRichEdit); return XUI_EVENT_DISPATCH_STOP; }
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 && iKey == 'H' ) { (void)xuiRichEditOpenReplace(pRichEdit); return XUI_EVENT_DISPATCH_STOP; }
	if ( pEvent->iKey == XUI_KEY_F3 || pEvent->iKey == XUI_KEY_ENTER ) {
		__xuiRichEditFindButtonClick((pEvent->iModifiers & XUI_MOD_SHIFT) != 0 ? pData->pFindPrevButton : pData->pFindNextButton, pRichEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
		(void)xuiWindowSetOpen(pData->pFindWindow, 0);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pRichEdit), pRichEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiRichEditFindLayout(xui_rich_edit_data_t* pData, int bReplace)
{
	xui_rect_t tWindow;
	float fOptionY = bReplace ? 86.0f : 50.0f;
	float fStatusY = fOptionY + 32.0f;
	if ( pData == NULL || pData->pFindWindow == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tWindow = xuiWidgetGetRect(pData->pFindWindow); tWindow.fW = 560.0f; tWindow.fH = bReplace ? 190.0f : 150.0f;
	(void)xuiWidgetSetRect(pData->pFindWindow, tWindow);
	(void)xuiWidgetSetRect(pData->pFindInput, (xui_rect_t){12,12,280,28});
	(void)xuiWidgetSetRect(pData->pFindPrevButton, (xui_rect_t){304,12,74,28});
	(void)xuiWidgetSetRect(pData->pFindNextButton, (xui_rect_t){386,12,74,28});
	(void)xuiWidgetSetRect(pData->pReplaceInput, (xui_rect_t){12,48,280,28});
	(void)xuiWidgetSetRect(pData->pReplaceButton, (xui_rect_t){304,48,92,28});
	(void)xuiWidgetSetRect(pData->pReplaceAllButton, (xui_rect_t){404,48,132,28});
	(void)xuiWidgetSetVisible(pData->pReplaceInput, bReplace); (void)xuiWidgetSetEnabled(pData->pReplaceInput, bReplace);
	(void)xuiWidgetSetVisible(pData->pReplaceButton, bReplace); (void)xuiWidgetSetEnabled(pData->pReplaceButton, bReplace);
	(void)xuiWidgetSetVisible(pData->pReplaceAllButton, bReplace); (void)xuiWidgetSetEnabled(pData->pReplaceAllButton, bReplace);
	(void)xuiWidgetSetRect(pData->pCaseCheck, (xui_rect_t){12,fOptionY,116,24});
	(void)xuiWidgetSetRect(pData->pWordCheck, (xui_rect_t){132,fOptionY,112,24});
	(void)xuiWidgetSetRect(pData->pRegexCheck, (xui_rect_t){248,fOptionY,86,24});
	(void)xuiWidgetSetRect(pData->pEscapeCheck, (xui_rect_t){338,fOptionY,88,24});
	(void)xuiWidgetSetRect(pData->pSelectionCheck, (xui_rect_t){430,fOptionY,106,24});
	(void)xuiWidgetSetRect(pData->pFindStatus, (xui_rect_t){12,fStatusY,524,24});
	return XUI_OK;
}

static int __xuiRichEditCreateFindButton(xui_context pContext, xui_widget* ppButton,
	xui_font pFont, const char* sText, xui_widget pRichEdit)
{
	xui_button_desc_t tDesc; int iRet;
	memset(&tDesc, 0, sizeof(tDesc)); tDesc.iSize = sizeof(tDesc); tDesc.pFont = pFont; tDesc.sText = sText; tDesc.fBorderWidth = 1.0f;
	iRet = xuiButtonCreate(pContext, ppButton, &tDesc);
	if ( iRet == XUI_OK ) iRet = xuiButtonSetClick(*ppButton, __xuiRichEditFindButtonClick, pRichEdit);
	return iRet;
}

static int __xuiRichEditCreateFindCheck(xui_context pContext, xui_widget* ppCheck,
	xui_font pFont, const char* sText, int bChecked, xui_widget pRichEdit)
{
	xui_checkbox_desc_t tDesc; int iRet;
	memset(&tDesc, 0, sizeof(tDesc)); tDesc.iSize = sizeof(tDesc); tDesc.pFont = pFont; tDesc.sText = sText;
	tDesc.bChecked = bChecked; tDesc.fIndicatorSize = 14.0f; tDesc.fGap = 4.0f;
	iRet = xuiCheckBoxCreate(pContext, ppCheck, &tDesc);
	if ( iRet == XUI_OK ) iRet = xuiCheckBoxSetChange(*ppCheck, __xuiRichEditFindCheckChange, pRichEdit);
	return iRet;
}

static int __xuiRichEditCreateFindWindow(xui_widget pRichEdit, xui_rich_edit_data_t* pData)
{
	xui_context pContext = xuiWidgetGetContext(pRichEdit);
	xui_widget pRoot = xuiGetRootWidget(pContext);
	xui_widget pClient;
	xui_window_desc_t tWindow;
	xui_input_desc_t tInput;
	xui_label_desc_t tLabel;
	xui_font pFont = pData->pFont != NULL ? pData->pFont : xuiGetDefaultFont(pContext);
	xui_widget arrChildren[12];
	int iRet;
	int i;
	if ( pRoot == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	memset(&tWindow, 0, sizeof(tWindow)); tWindow.iSize = sizeof(tWindow); tWindow.sTitle = xuiTranslate(pContext, XUI_TR_FIND_TITLE);
	tWindow.pFont = pFont; tWindow.bClosed = 1; tWindow.bTopMost = 1; tWindow.bHideCollapse = 1; tWindow.bHideMaximize = 1;
	tWindow.bNotResizable = 1; tWindow.fTitleBarHeight = 28; tWindow.fBorderWidth = 1; tWindow.fButtonSize = 18;
	iRet = xuiWindowCreate(pContext, &pData->pFindWindow, &tWindow); if ( iRet != XUI_OK ) return iRet;
	(void)xuiWindowSetClose(pData->pFindWindow, __xuiRichEditFindWindowClose, pRichEdit);
	pClient = xuiWindowGetClientWidget(pData->pFindWindow);
	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_MANUAL); (void)xuiWidgetSetFlowMode(pClient, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetPadding(pClient, (xui_thickness_t){0,0,0,0}); (void)xuiWidgetSetGap(pClient, 0);
	(void)xuiWidgetSetEventHandler(pClient, XUI_EVENT_KEY_DOWN, __xuiRichEditFindWindowKey, pRichEdit);
	memset(&tInput, 0, sizeof(tInput)); tInput.iSize = sizeof(tInput); tInput.pFont = pFont; tInput.fBorderWidth = 1;
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER); iRet = xuiInputCreate(pContext, &pData->pFindInput, &tInput);
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER); if ( iRet == XUI_OK ) iRet = xuiInputCreate(pContext, &pData->pReplaceInput, &tInput);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindButton(pContext,&pData->pFindPrevButton,pFont,xuiTranslate(pContext,XUI_TR_FIND_PREVIOUS),pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindButton(pContext,&pData->pFindNextButton,pFont,xuiTranslate(pContext,XUI_TR_FIND_NEXT),pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindButton(pContext,&pData->pReplaceButton,pFont,xuiTranslate(pContext,XUI_TR_REPLACE_CURRENT),pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindButton(pContext,&pData->pReplaceAllButton,pFont,xuiTranslate(pContext,XUI_TR_REPLACE_ALL),pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindCheck(pContext,&pData->pCaseCheck,pFont,xuiTranslate(pContext,XUI_TR_FIND_CASE),0,pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindCheck(pContext,&pData->pWordCheck,pFont,xuiTranslate(pContext,XUI_TR_FIND_WORD),0,pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindCheck(pContext,&pData->pRegexCheck,pFont,xuiTranslate(pContext,XUI_TR_FIND_REGEX),0,pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindCheck(pContext,&pData->pEscapeCheck,pFont,xuiTranslate(pContext,XUI_TR_FIND_ESCAPE),0,pRichEdit);
	if ( iRet == XUI_OK ) iRet = __xuiRichEditCreateFindCheck(pContext,&pData->pSelectionCheck,pFont,xuiTranslate(pContext,XUI_TR_FIND_SELECTION),0,pRichEdit);
	memset(&tLabel,0,sizeof(tLabel)); tLabel.iSize=sizeof(tLabel); tLabel.pFont=pFont; tLabel.sText=""; tLabel.iTextColor=XUI_COLOR_RGBA(90,105,124,255); tLabel.iTextFlags=XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE|XUI_TEXT_CLIP;
	if ( iRet == XUI_OK ) iRet = xuiLabelCreate(pContext, &pData->pFindStatus, &tLabel);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiInputSetChange(pData->pFindInput,__xuiRichEditFindInputChange,pRichEdit); (void)xuiInputSetChange(pData->pReplaceInput,__xuiRichEditFindInputChange,pRichEdit);
	arrChildren[0]=pData->pFindInput; arrChildren[1]=pData->pReplaceInput; arrChildren[2]=pData->pFindPrevButton; arrChildren[3]=pData->pFindNextButton;
	arrChildren[4]=pData->pReplaceButton; arrChildren[5]=pData->pReplaceAllButton; arrChildren[6]=pData->pCaseCheck; arrChildren[7]=pData->pWordCheck;
	arrChildren[8]=pData->pRegexCheck; arrChildren[9]=pData->pEscapeCheck; arrChildren[10]=pData->pSelectionCheck; arrChildren[11]=pData->pFindStatus;
	for(i=0;i<12&&iRet==XUI_OK;i++) iRet=xuiWindowAddChild(pData->pFindWindow,arrChildren[i]);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pRoot, pData->pFindWindow);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pData->pFindWindow,(xui_rect_t){32,32,560,150}); return __xuiRichEditFindLayout(pData,0);
}

static int __xuiRichEditOpenFindWindow(xui_widget pWidget, int bReplace)
{
	xui_rich_edit_data_t* pData=__xuiRichEditData(pWidget); xui_rect_t tOwner,tWindow; int iStart,iEnd,iLen,iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFindWindow == NULL ) {
		iRet = __xuiRichEditCreateFindWindow(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bFindWindowReplace=bReplace?1:0; __xuiRichEditSelection(pData,&iStart,&iEnd); pData->iFindRangeStart=pData->iFindRangeEnd=0;
	if(iEnd>iStart){pData->iFindRangeStart=iStart;pData->iFindRangeEnd=iEnd;iLen=iEnd-iStart;if(iLen<256){iRet=__xuiRichEditScratch(pData,iLen+1);if(iRet!=XUI_OK)return iRet;memcpy(pData->sScratch,xuiRichDocumentGetText(pData->pDocument)+iStart,(size_t)iLen);pData->sScratch[iLen]=0;(void)xuiInputSetText(pData->pFindInput,pData->sScratch);}}
	else if ( pData->sFindPattern != NULL ) (void)xuiInputSetText(pData->pFindInput, pData->sFindPattern);
	if ( pData->sFindReplacement != NULL ) (void)xuiInputSetText(pData->pReplaceInput, pData->sFindReplacement);
	(void)xuiCheckBoxSetChecked(pData->pSelectionCheck,iEnd>iStart&&(pData->iFindFlags&XUI_FIND_SELECTION)!=0); __xuiRichEditFindApplyLanguage(pWidget,pData); (void)__xuiRichEditFindLayout(pData,bReplace);
	tOwner=xuiWidgetGetWorldRect(pWidget);tWindow=xuiWidgetGetRect(pData->pFindWindow);tWindow.fX=tOwner.fX+16;tWindow.fY=tOwner.fY+16;(void)xuiWidgetSetRect(pData->pFindWindow,tWindow);
	(void)xuiWindowSetOpen(pData->pFindWindow,1);(void)xuiWindowBringToFront(pData->pFindWindow);(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget),pData->pFindInput);__xuiRichEditFindRefresh(pWidget);return XUI_OK;
}

XUI_API int xuiRichEditOpenFind(xui_widget pWidget){return __xuiRichEditOpenFindWindow(pWidget,0);}
XUI_API int xuiRichEditOpenReplace(xui_widget pWidget){return __xuiRichEditOpenFindWindow(pWidget,1);}
XUI_API xui_widget xuiRichEditGetFindWindow(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);return p?p->pFindWindow:NULL;}

XUI_API int xuiRichEditOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	int iRet;
	if ( pData == NULL || pData->pMenu == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRichEditUpdateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiMenuOpenAt(pData->pMenu, pWidget, fX, fY);
}

XUI_API xui_widget xuiRichEditGetMenuWidget(xui_widget pWidget)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	return pData != NULL ? pData->pMenu : NULL;
}

static int __xuiRichEditHistory(xui_widget pWidget, int bRedo)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	xui_rich_change_t tChange;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = bRedo ? xuiRichDocumentRedo(pData->pDocument) : xuiRichDocumentUndo(pData->pDocument);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tChange, 0, sizeof(tChange));
	tChange.iSize = sizeof(tChange);
	if ( xuiRichDocumentGetLastChange(pData->pDocument, &tChange) == XUI_OK )
		pData->iAnchor = pData->iCaret = tChange.iNewEnd;
	else
		pData->iAnchor = pData->iCaret = xuiRichDocumentGetLength(pData->pDocument);
	pData->bTypingStyleOverride = 0;
	pData->bPreferredCaretX = 0;
	__xuiRichEditEnsureCaretVisible(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRichEditUndo(xui_widget pWidget) { return __xuiRichEditHistory(pWidget, 0); }
XUI_API int xuiRichEditRedo(xui_widget pWidget) { return __xuiRichEditHistory(pWidget, 1); }
XUI_API int xuiRichEditSetReadonly(xui_widget pWidget, int bReadonly)
{
	xui_rich_edit_data_t* pData = __xuiRichEditData(pWidget);
	int bValue = bReadonly ? 1 : 0;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly == bValue ) return XUI_OK;
	pData->bReadonly = bValue;
	(void)xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED,
		XUI_RICH_ACCESSIBLE_ROOT_ID);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}
XUI_API int xuiRichEditIsReadonly(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);return p?p->bReadonly:0;}
XUI_API int xuiRichEditSetScroll(xui_widget pWidget,float x,float y){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);float sx,sy;int r;if(!p)return XUI_ERROR_INVALID_ARGUMENT;r=__xuiRichEditUpdateScrollModel(pWidget,p);if(r!=XUI_OK)return r;r=xuiScrollModelSetOffset(&p->tScrollModel,x,y);if(r!=XUI_OK)return r;(void)xuiScrollModelGetOffset(&p->tScrollModel,&sx,&sy);p->fScrollX=sx;p->fScrollY=sy;if(p->pHScrollBar&&p->pVScrollBar){p->bSyncingScrollBars=1;(void)xuiScrollBarSetValue(p->pHScrollBar,sx);(void)xuiScrollBarSetValue(p->pVScrollBar,sy);p->bSyncingScrollBars=0;}(void)xuiInternalInputRefreshImePosition(xuiWidgetGetContext(pWidget));return xuiWidgetInvalidate(pWidget,XUI_WIDGET_DIRTY_LAYOUT|XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);}
XUI_API int xuiRichEditGetScroll(xui_widget pWidget,float*x,float*y){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);if(!p)return XUI_ERROR_INVALID_ARGUMENT;if(x)*x=p->fScrollX;if(y)*y=p->fScrollY;return XUI_OK;}
XUI_API int xuiRichEditScrollBy(xui_widget pWidget,float x,float y){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);if(!p)return XUI_ERROR_INVALID_ARGUMENT;return xuiRichEditSetScroll(pWidget,p->fScrollX+x,p->fScrollY+y);}
XUI_API int xuiRichEditSetZoom(xui_widget pWidget,float z){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);if(!p||!isfinite(z)||z<0.25f||z>8.0f)return XUI_ERROR_INVALID_ARGUMENT;if(fabsf(p->fZoom-z)<0.0001f)return XUI_OK;__xuiRichEditClearFontCache(pWidget,p);p->fZoom=z;p->bLayoutDirty=1;p->bIncrementalLayout=0;return xuiWidgetInvalidate(pWidget,XUI_WIDGET_DIRTY_LAYOUT|XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);}
XUI_API float xuiRichEditGetZoom(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);return p?p->fZoom:0.0f;}
XUI_API xui_scroll_model_t* xuiRichEditGetScrollModel(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);if(!p)return NULL;(void)__xuiRichEditUpdateScrollModel(pWidget,p);return &p->tScrollModel;}
XUI_API xui_widget xuiRichEditGetHScrollBarWidget(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);return p?p->pHScrollBar:NULL;}
XUI_API xui_widget xuiRichEditGetVScrollBarWidget(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);return p?p->pVScrollBar:NULL;}
XUI_API int xuiRichEditGetFragmentCount(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);if(!p)return 0;(void)__xuiRichEditEnsureLayout(pWidget,p);return p->iTotalFragmentCount;}
XUI_API int xuiRichEditGetFragment(xui_widget pWidget,int i,xui_rich_fragment_t*f){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);xui_rich_layout_fragment_t t;if(!p||!f)return XUI_ERROR_INVALID_ARGUMENT;(void)__xuiRichEditEnsureLayout(pWidget,p);if(i<0||i>=p->iTotalFragmentCount||__xuiRichEditGetLayoutFragment(p,i,&t)!=XUI_OK)return XUI_ERROR_INVALID_ARGUMENT;*f=t.tPublic;f->iSize=sizeof(*f);return XUI_OK;}
XUI_API xui_rect_t xuiRichEditGetCursorRect(xui_widget pWidget){xui_rich_edit_data_t*p=__xuiRichEditData(pWidget);xui_rect_t r={0,0,0,0};if(p)r=__xuiRichEditCaret(pWidget,p,p->iCaret);return r;}
