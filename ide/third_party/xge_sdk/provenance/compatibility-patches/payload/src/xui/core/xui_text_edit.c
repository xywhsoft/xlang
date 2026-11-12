#include "xui_config.h"
#if XUI_ENABLE_TEXT_EDIT
#include "xui_internal.h"
#include "xui_pointer_press_internal.h"
#include "xui_text_internal.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_TEXT_EDIT_HISTORY_LIMIT	64
#define XUI_TEXT_EDIT_HISTORY_BYTES_LIMIT	(32u * 1024u * 1024u)
#define XUI_TEXT_EDIT_SCROLLBAR_SIZE	8.0f
#define XUI_TEXT_EDIT_PAINT_CACHE_LIMIT	64

typedef struct xui_text_edit_line_t {
	int iStart;
	int iEnd;
	int iPhysicalLine;
	float fY;
	float fW;
} xui_text_edit_line_t;

typedef struct xui_text_edit_history_t {
	char* sRemoved;
	char* sInserted;
	int iRemovedCapacity;
	int iInsertedCapacity;
	int iStart;
	int iRemovedLength;
	int iInsertedLength;
	int iBeforeCursor;
	int iBeforeSelectStart;
	int iBeforeSelectEnd;
	float fBeforeScrollX;
	float fBeforeScrollY;
	int iAfterCursor;
	int iAfterSelectStart;
	int iAfterSelectEnd;
	float fAfterScrollX;
	float fAfterScrollY;
} xui_text_edit_history_t;

typedef struct xui_text_edit_layout_cache_t {
	xui_text_layout pLayout;
	xui_font pFont;
	uint32_t iDpiGeneration;
} xui_text_edit_layout_cache_t;

typedef struct xui_text_edit_paint_cache_t {
	int iLine;
	char* sText;
	int iTextSize;
	double* pAdvances;
	xui_internal_text_chunk_cache_t tChunks;
	int bChunked;
	int bValid;
	xui_font pFont;
	uint32_t iDpiGeneration;
	uint32_t iUseStamp;
	xui_text_shape_t tShape;
} xui_text_edit_paint_cache_t;

typedef struct xui_text_edit_data_t {
	xui_context pContext;
	xui_text_layout pWorkLayout;
	int iCacheActivity;
	int bBuildingLines,bProducingPaint;
	char* sText;
	int iTextCapacity;
	int iTextLength;
	char* sPlaceholder;
	int iPlaceholderCapacity;
	char* sScratch;
	int iScratchCapacity;
	char* sImeText;
	int iImeTextCapacity;
	char* sImeDisplay;
	int iImeDisplayCapacity;
	int iImeDisplayLength;
	char* sFocusText;
	int iFocusTextCapacity;
	char* sFindPattern;
	int iFindPatternCapacity;
	char* sFindReplacement;
	int iFindReplacementCapacity;
	char* arrMenuTitle[XUI_INPUT_MENU_COUNT];
	xui_text_edit_line_t* pLines;
	int iLineCount;
	int iLineCapacity;
	xui_text_edit_line_t* pImeLines;
	int iImeLineCapacity;
	xui_text_edit_layout_cache_t tTextLayout;
	xui_text_edit_layout_cache_t tImeTextLayout;
	xui_text_edit_paint_cache_t arrPaintCache[XUI_TEXT_EDIT_PAINT_CACHE_LIMIT];
	int iPaintCacheCount;
	uint32_t iPaintUseStamp;
	xui_find_result_t* pFindResults;
	int iFindResultCount;
	int iFindResultCapacity;
	int iFindActiveIndex;
	xui_find_result_t tFindActive;
	uint32_t iFindFlags;
	int iFindRangeStart;
	int iFindRangeEnd;
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
	xui_widget pMenu;
	xui_widget pHScrollBar;
	xui_widget pVScrollBar;
	xui_font pFont;
	xui_text_edit_change_proc onChange;
	void* pChangeUser;
	int iChangeCount;
	int iCursor;
	int iAnchor;
	int iSelectStart;
	int iSelectEnd;
	float fPreferredCaretX;
	int bPreferredCaretX;
	int bImeActive;
	int iImeAnchorStart;
	int iImeAnchorEnd;
	int iImeCursor;
	int iImeSelectionStart;
	int iImeSelectionEnd;
	int iImeAttribute;
	xui_pointer_owner_t tPointerOwner;
	int bDragging;
	int bLineSelecting;
	int iLineSelectAnchorStart;
	int iLineSelectAnchorEnd;
	int bPressPending;
	int bPressInsideSelection;
	int bMovingSelection;
	int iPressCursor;
	float fPressX;
	float fPressY;
	float fScrollX;
	float fScrollY;
	float fContentWidth;
	float fContentHeight;
	xui_scroll_model_t tScrollModel;
	int bShowHScrollBar;
	int bShowVScrollBar;
	int bSyncingScrollBars;
	xui_rect_t tScrollViewportRect;
	xui_rect_t tHScrollBarRect;
	xui_rect_t tVScrollBarRect;
	float fLastLayoutWidth;
	float fLineHeight;
	int bLinesDirty;
	int iMaxLength;
	int bReadonly;
	int bWordWrap;
	int bDirectLayout;
	int bLineNumbers;
	float fLineNumberWidth;
	xui_text_edit_history_t arrUndo[XUI_TEXT_EDIT_HISTORY_LIMIT];
	xui_text_edit_history_t arrRedo[XUI_TEXT_EDIT_HISTORY_LIMIT];
	int iUndoCount;
	int iRedoCount;
	int bUndoInsertGroup;
	int iUndoInsertEnd;
	uint32_t iTextColor;
	uint32_t iPlaceholderColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iSelectionColor;
	uint32_t iFindResultColor;
	uint32_t iFindActiveColor;
	uint32_t iCursorColor;
	uint32_t iLineNumberColor;
	uint32_t iLineNumberBackgroundColor;
	uint32_t iLineNumberBorderColor;
	float fBorderWidth;
	float fLineGap;
	xui_rect_t tTextRect;
	xui_rect_t tCursorRect;
} xui_text_edit_data_t;

static int __xuiTextEditEditSetText(xui_widget pWidget, const char* sText);

static const xui_internal_edit_adapter_t g_xuiTextEditAdapter = {
	XUI_EDIT_CAP_TEXT | XUI_EDIT_CAP_SELECTION | XUI_EDIT_CAP_CLIPBOARD |
	XUI_EDIT_CAP_UNDO | XUI_EDIT_CAP_READONLY | XUI_EDIT_CAP_CARET_RECT |
	XUI_EDIT_CAP_CONTEXT_MENU | XUI_EDIT_CAP_IME | XUI_EDIT_CAP_MULTILINE,
	__xuiTextEditEditSetText, xuiTextEditGetText, xuiTextEditSetSelection, xuiTextEditGetSelection,
	xuiTextEditHasSelection, xuiTextEditSelectAll, xuiTextEditCopy, xuiTextEditCut,
	xuiTextEditPaste, xuiTextEditDeleteSelection, xuiTextEditUndo, xuiTextEditRedo,
	xuiTextEditCanUndo, xuiTextEditCanRedo, xuiTextEditSetReadonly, xuiTextEditIsReadonly,
	xuiTextEditGetCursorRect, xuiTextEditOpenMenu
};

static void __xuiTextEditFindWindowApplyLanguage(xui_widget pTextEdit, xui_text_edit_data_t* pData);
static int __xuiTextEditUpdate(xui_widget pWidget, float fDelta, void* pUser);

static int __xuiTextEditMenuTextId(int iCommand)
{
	switch ( iCommand ) {
	case XUI_INPUT_MENU_UNDO: return XUI_TR_EDIT_UNDO;
	case XUI_INPUT_MENU_CUT: return XUI_TR_EDIT_CUT;
	case XUI_INPUT_MENU_COPY: return XUI_TR_EDIT_COPY;
	case XUI_INPUT_MENU_PASTE: return XUI_TR_EDIT_PASTE;
	case XUI_INPUT_MENU_DELETE: return XUI_TR_EDIT_DELETE;
	case XUI_INPUT_MENU_SELECT_ALL: return XUI_TR_EDIT_SELECT_ALL;
	case XUI_INPUT_MENU_REDO: return XUI_TR_EDIT_REDO;
	default: return XUI_TR_NONE;
	}
}

static int __xuiTextEditDescValid(const xui_text_edit_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iMaxLength < 0) ||
	     !isfinite(pDesc->fBorderWidth) ||
	     !isfinite(pDesc->fLineGap) ||
	     !isfinite(pDesc->fLineNumberWidth) ||
	     (pDesc->fBorderWidth < 0.0f) ||
	     (pDesc->fLineGap < 0.0f) ||
	     (pDesc->fLineNumberWidth < 0.0f) ||
	     (pDesc->fBorderWidth > XUI_LAYOUT_UNBOUNDED) ||
	     (pDesc->fLineGap > XUI_LAYOUT_UNBOUNDED) ||
	     (pDesc->fLineNumberWidth > XUI_LAYOUT_UNBOUNDED) ) {
		return 0;
	}
	return 1;
}

static int __xuiTextEditAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiTextEditMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiTextEditStringSize(const char* sText, int* pSize)
{
	size_t iLength;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL ) sText = "";
	iLength = strlen(sText);
	if ( iLength > (size_t)(INT_MAX - 1) ) return XUI_ERROR_BUFFER_TOO_SMALL;
	*pSize = (int)iLength;
	return XUI_OK;
}

static uint32_t __xuiTextEditColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	if ( (iColor & 0xffu) == 0 ) return iColor;
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiTextEditTextReserve(char** psText, int* pCapacity, int iCapacity)
{
	return xuiInternalTextBufferReserve(psText, pCapacity, iCapacity);
}

static int __xuiTextEditStringSet(char** psText, int* pCapacity, const char* sText)
{
	int iNeed;
	int iRet;

	if ( sText == NULL ) {
		sText = "";
	}
	iRet = __xuiTextEditStringSize(sText, &iNeed);
	if ( iRet != XUI_OK ) return iRet;
	iNeed++;
	iRet = __xuiTextEditTextReserve(psText, pCapacity, iNeed);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(*psText, sText, (size_t)iNeed);
	return XUI_OK;
}

static char* __xuiTextEditStringDuplicate(const char* sText)
{
	char* sCopy;
	int iCapacity;

	sCopy = NULL;
	iCapacity = 0;
	if ( __xuiTextEditStringSet(&sCopy, &iCapacity, sText) != XUI_OK ) {
		return NULL;
	}
	return sCopy;
}

static int __xuiTextEditArrayCapacity(int iCurrent, int iRequired,
	int iMinimum, size_t iElementSize, int* pCapacity)
{
	int iCapacity;

	if ( iCurrent < 0 || iRequired < 0 || iMinimum <= 0 ||
	     iElementSize == 0u || pCapacity == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCapacity = iRequired;
	if ( iCurrent > 0 && iCurrent <= INT_MAX / 2 && iCapacity < iCurrent * 2 ) {
		iCapacity = iCurrent * 2;
	}
	if ( iCapacity < iMinimum ) iCapacity = iMinimum;
	if ( (size_t)iCapacity > (size_t)-1 / iElementSize ) {
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiTextEditFindResultReserve(xui_text_edit_data_t* pData, int iCapacity)
{
	xui_find_result_t* pNew;
	int iRet;

	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pData->iFindResultCapacity ) return XUI_OK;
	iRet = __xuiTextEditArrayCapacity(pData->iFindResultCapacity, iCapacity,
		16, sizeof(*pNew), &iCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pNew = (xui_find_result_t*)xrtRealloc(pData->pFindResults, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pData->pFindResults = pNew;
	pData->iFindResultCapacity = iCapacity;
	return XUI_OK;
}

static void __xuiTextEditClearFindResults(xui_text_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iFindResultCount = 0;
	pData->iFindActiveIndex = -1;
	memset(&pData->tFindActive, 0, sizeof(pData->tFindActive));
	pData->tFindActive.iSize = sizeof(pData->tFindActive);
}

static void __xuiTextEditResetFindRange(xui_text_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iFindRangeStart = 0;
	pData->iFindRangeEnd = 0;
}

static void __xuiTextEditInvalidateFindState(xui_text_edit_data_t* pData)
{
	__xuiTextEditClearFindResults(pData);
	__xuiTextEditResetFindRange(pData);
}

static void __xuiTextEditDestroyFindData(xui_text_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	xrtFree(pData->pFindResults);
	xrtFree(pData->sFindPattern);
	xrtFree(pData->sFindReplacement);
	pData->pFindResults = NULL;
	pData->sFindPattern = NULL;
	pData->sFindReplacement = NULL;
	pData->iFindResultCapacity = 0;
	pData->iFindPatternCapacity = 0;
	pData->iFindReplacementCapacity = 0;
	__xuiTextEditClearFindResults(pData);
}

static int __xuiTextEditUtf8Next(const char* sText, int iLen, int iPos)
{
	return xuiInternalTextGraphemeNext(sText, iLen, iPos);
}

static int __xuiTextEditUtf8Prev(const char* sText, int iLen, int iPos)
{
	return xuiInternalTextGraphemePrev(sText, iLen, iPos);
}

static int __xuiTextEditUtf8Clamp(const char* sText, int iLen, int iPos)
{
	return xuiInternalTextGraphemeClamp(sText, iLen, iPos);
}

static int __xuiTextEditUtf8ClampBytes(const char* sText, int iMaxBytes)
{
	int iLen;
	int iPos;
	int iNext;

	if ( sText == NULL || iMaxBytes <= 0 ) {
		return 0;
	}
	if ( __xuiTextEditStringSize(sText, &iLen) != XUI_OK ) return 0;
	if ( iLen <= iMaxBytes ) {
		return iLen;
	}
	iPos = 0;
	while ( iPos < iLen ) {
		iNext = __xuiTextEditUtf8Next(sText, iLen, iPos);
		if ( iNext > iMaxBytes ) {
			break;
		}
		iPos = iNext;
	}
	return iPos;
}

static void __xuiTextEditImeReset(xui_text_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->bImeActive = 0;
	pData->iImeAnchorStart = pData->iCursor;
	pData->iImeAnchorEnd = pData->iCursor;
	pData->iImeCursor = 0;
	pData->iImeSelectionStart = 0;
	pData->iImeSelectionEnd = 0;
	pData->iImeAttribute = XUI_IME_ATTRIBUTE_NONE;
	if ( pData->sImeText != NULL ) pData->sImeText[0] = '\0';
}

static int __xuiTextEditImeSetText(xui_text_edit_data_t* pData, const char* sText, int iTextSize)
{
	int iRet;

	if ( pData == NULL || sText == NULL || iTextSize < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextSize > INT_MAX - 1 ) return XUI_ERROR_BUFFER_TOO_SMALL;
	iRet = __xuiTextEditTextReserve(&pData->sImeText, &pData->iImeTextCapacity, iTextSize + 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( iTextSize > 0 ) memcpy(pData->sImeText, sText, (size_t)iTextSize);
	pData->sImeText[iTextSize] = '\0';
	return XUI_OK;
}

static int __xuiTextEditImeBuildDisplay(xui_text_edit_data_t* pData)
{
	int iDocumentSize;
	int iCompositionSize;
	int iStart;
	int iEnd;
	int iDisplaySize;
	int iSuffixSize;
	int iRet;

	if ( pData == NULL || pData->sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iDocumentSize = pData->iTextLength;
	iCompositionSize = (pData->sImeText != NULL) ? (int)strlen(pData->sImeText) : 0;
	iStart = __xuiTextEditUtf8Clamp(pData->sText, iDocumentSize, pData->iImeAnchorStart);
	iEnd = __xuiTextEditUtf8Clamp(pData->sText, iDocumentSize, pData->iImeAnchorEnd);
	if ( iStart > iEnd ) {
		int iSwap = iStart;
		iStart = iEnd;
		iEnd = iSwap;
	}
	iSuffixSize = iDocumentSize - iEnd;
	if ( iStart > INT_MAX - 1 - iSuffixSize ) return XUI_ERROR_BUFFER_TOO_SMALL;
	iDisplaySize = iStart + iSuffixSize;
	if ( iCompositionSize > INT_MAX - 1 - iDisplaySize ) return XUI_ERROR_BUFFER_TOO_SMALL;
	iDisplaySize += iCompositionSize;
	iRet = __xuiTextEditTextReserve(&pData->sImeDisplay, &pData->iImeDisplayCapacity, iDisplaySize + 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( iStart > 0 ) memcpy(pData->sImeDisplay, pData->sText, (size_t)iStart);
	if ( iCompositionSize > 0 ) memcpy(pData->sImeDisplay + iStart, pData->sImeText, (size_t)iCompositionSize);
	if ( iDocumentSize > iEnd ) {
		memcpy(pData->sImeDisplay + iStart + iCompositionSize,
			pData->sText + iEnd, (size_t)(iDocumentSize - iEnd));
	}
	pData->sImeDisplay[iDisplaySize] = '\0';
	pData->iImeDisplayLength = iDisplaySize;
	pData->iImeAnchorStart = iStart;
	pData->iImeAnchorEnd = iEnd;
	pData->iImeCursor = __xuiTextEditUtf8Clamp(pData->sImeText != NULL ? pData->sImeText : "",
		iCompositionSize, pData->iImeCursor);
	pData->iImeSelectionStart = __xuiTextEditUtf8Clamp(pData->sImeText != NULL ? pData->sImeText : "",
		iCompositionSize, pData->iImeSelectionStart);
	pData->iImeSelectionEnd = __xuiTextEditUtf8Clamp(pData->sImeText != NULL ? pData->sImeText : "",
		iCompositionSize, pData->iImeSelectionEnd);
	if ( pData->iImeSelectionStart > pData->iImeSelectionEnd ) {
		int iSwap = pData->iImeSelectionStart;
		pData->iImeSelectionStart = pData->iImeSelectionEnd;
		pData->iImeSelectionEnd = iSwap;
	}
	return XUI_OK;
}

static int __xuiTextEditPrevWord(xui_text_edit_data_t* pData)
{
	if ( pData == NULL || pData->sText == NULL ) return 0;
	return xuiInternalTextWordPrev(pData->sText, -1, pData->iCursor,
		XUI_INTERNAL_WORD_NATURAL);
}

static int __xuiTextEditNextWord(xui_text_edit_data_t* pData)
{
	if ( pData == NULL || pData->sText == NULL ) return 0;
	return xuiInternalTextWordNext(pData->sText, -1, pData->iCursor,
		XUI_INTERNAL_WORD_NATURAL);
}

static int __xuiTextEditUtf8Encode(uint32_t iCodepoint, char* sText)
{
	if ( sText == NULL ) {
		return 0;
	}
	if ( iCodepoint <= 0x7fu ) {
		sText[0] = (char)iCodepoint;
		return 1;
	}
	if ( iCodepoint <= 0x7ffu ) {
		sText[0] = (char)(0xc0u | ((iCodepoint >> 6) & 0x1fu));
		sText[1] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 2;
	}
	if ( iCodepoint <= 0xffffu ) {
		sText[0] = (char)(0xe0u | ((iCodepoint >> 12) & 0x0fu));
		sText[1] = (char)(0x80u | ((iCodepoint >> 6) & 0x3fu));
		sText[2] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 3;
	}
	if ( iCodepoint <= 0x10ffffu ) {
		sText[0] = (char)(0xf0u | ((iCodepoint >> 18) & 0x07u));
		sText[1] = (char)(0x80u | ((iCodepoint >> 12) & 0x3fu));
		sText[2] = (char)(0x80u | ((iCodepoint >> 6) & 0x3fu));
		sText[3] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 4;
	}
	return 0;
}

static xui_text_edit_data_t* __xuiTextEditGetData(xui_widget pWidget)
{
    if(xuiInternalWidgetIsValid(pWidget) && pWidget->pContext->bCloseRequested)return NULL;
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "textedit");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_text_edit_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiTextEditAlive(xui_context pContext, xui_widget pWidget,
	const xui_text_edit_data_t* pExpected)
{
	return xuiInternalContextIsValid(pContext) && !xuiInternalContextDestroyPending(pContext) &&
		xuiInternalWidgetIsValid(pWidget) && __xuiTextEditGetData(pWidget) == pExpected;
}

static int __xuiTextEditQueryCursor(xui_widget pWidget, int iX, int iY, void* pUser);

static int __xuiTextEditStyleColor(xui_widget pWidget, const char* sName, const char* sFallback, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet != XUI_OK) && (sFallback != NULL) ) {
		memset(&tProperty, 0, sizeof(tProperty));
		tProperty.iSize = sizeof(tProperty);
		iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sFallback, &tProperty);
	}
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiTextEditStyleFloat(xui_widget pWidget, const char* sName, const char* sFallback, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet != XUI_OK) && (sFallback != NULL) ) {
		memset(&tProperty, 0, sizeof(tProperty));
		tProperty.iSize = sizeof(tProperty);
		iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sFallback, &tProperty);
	}
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) &&
	     isfinite(tProperty.tValue.fFloat) && (tProperty.tValue.fFloat >= 0.0f) &&
	     (tProperty.tValue.fFloat <= XUI_LAYOUT_UNBOUNDED) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiTextEditStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiTextEditResolve(xui_widget pWidget, xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)__xuiTextEditStyleColor(pWidget, "textedit.text.color", "input.text.color", &pResolved->iTextColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.placeholder.color", "input.placeholder.color", &pResolved->iPlaceholderColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.text.disabled_color", "input.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.background.color", "input.background.color", &pResolved->iBackgroundColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.background.hover_color", "input.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.background.disabled_color", "input.background.disabled_color", &pResolved->iDisabledBackgroundColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.border.color", "input.border.color", &pResolved->iBorderColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.border.hover_color", "input.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.border.focus_color", "input.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.selection.color", "input.selection.color", &pResolved->iSelectionColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.find.result_color", NULL, &pResolved->iFindResultColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.find.active_color", NULL, &pResolved->iFindActiveColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.cursor.color", "input.cursor.color", &pResolved->iCursorColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.line_number.color", NULL, &pResolved->iLineNumberColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.line_number.background_color", NULL, &pResolved->iLineNumberBackgroundColor);
	(void)__xuiTextEditStyleColor(pWidget, "textedit.line_number.border_color", NULL, &pResolved->iLineNumberBorderColor);
	(void)__xuiTextEditStyleFloat(pWidget, "textedit.border.width", "input.border.width", &pResolved->fBorderWidth);
	(void)__xuiTextEditStyleFloat(pWidget, "textedit.line_gap", NULL, &pResolved->fLineGap);
	(void)__xuiTextEditStyleFloat(pWidget, "textedit.line_number.width", NULL, &pResolved->fLineNumberWidth);
	pResolved->pFont = __xuiTextEditStyleFont(pWidget, pResolved->pFont);
}

static float __xuiTextEditLineNumberWidth(const xui_text_edit_data_t* pResolved)
{
	if ( (pResolved == NULL) || !pResolved->bLineNumbers ) {
		return 0.0f;
	}
	return (pResolved->fLineNumberWidth > 0.0f) ? pResolved->fLineNumberWidth : 44.0f;
}

static xui_rect_t __xuiTextEditRawTextContentRect(xui_widget pWidget, const xui_text_edit_data_t* pResolved)
{
	xui_rect_t tContent;
	float fLineNumberWidth;

	tContent = xuiWidgetGetContentRect(pWidget);
	fLineNumberWidth = __xuiTextEditLineNumberWidth(pResolved);
	if ( fLineNumberWidth > 0.0f ) {
		if ( fLineNumberWidth < tContent.fW ) {
			tContent.fX += fLineNumberWidth;
			tContent.fW -= fLineNumberWidth;
		} else {
			tContent.fX += tContent.fW;
			tContent.fW = 0.0f;
		}
	}
	return tContent;
}

static xui_rect_t __xuiTextEditTextContentRect(xui_widget pWidget, const xui_text_edit_data_t* pResolved)
{
	xui_text_edit_data_t* pData;
	xui_rect_t tContent;

	tContent = __xuiTextEditRawTextContentRect(pWidget, pResolved);
	pData = __xuiTextEditGetData(pWidget);
	if ( pData != NULL ) {
		if ( pData->bShowVScrollBar ) {
			if ( XUI_TEXT_EDIT_SCROLLBAR_SIZE < tContent.fW ) {
				tContent.fW -= XUI_TEXT_EDIT_SCROLLBAR_SIZE;
			} else {
				tContent.fW = 0.0f;
			}
		}
		if ( pData->bShowHScrollBar ) {
			if ( XUI_TEXT_EDIT_SCROLLBAR_SIZE < tContent.fH ) {
				tContent.fH -= XUI_TEXT_EDIT_SCROLLBAR_SIZE;
			} else {
				tContent.fH = 0.0f;
			}
		}
	}
	return tContent;
}

static xui_rect_t __xuiTextEditLineNumberRect(xui_widget pWidget, const xui_text_edit_data_t* pResolved)
{
	xui_text_edit_data_t* pData;
	xui_rect_t tContent;
	float fLineNumberWidth;

	tContent = xuiWidgetGetContentRect(pWidget);
	pData = __xuiTextEditGetData(pWidget);
	if ( (pData != NULL) && pData->bShowHScrollBar ) {
		if ( XUI_TEXT_EDIT_SCROLLBAR_SIZE < tContent.fH ) {
			tContent.fH -= XUI_TEXT_EDIT_SCROLLBAR_SIZE;
		} else {
			tContent.fH = 0.0f;
		}
	}
	fLineNumberWidth = __xuiTextEditLineNumberWidth(pResolved);
	if ( (fLineNumberWidth <= 0.0f) || (tContent.fW <= 0.0f) ) {
		return (xui_rect_t){tContent.fX, tContent.fY, 0.0f, tContent.fH};
	}
	if ( fLineNumberWidth > tContent.fW ) {
		fLineNumberWidth = tContent.fW;
	}
	tContent.fW = fLineNumberWidth;
	return tContent;
}

static int __xuiTextEditLineNumberHit(xui_widget pWidget, xui_text_edit_data_t* pData,
	float fX, float fY)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tWorld;
	xui_rect_t tGutter;

	if ( pWidget == NULL || pData == NULL ) return 0;
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tGutter = __xuiTextEditLineNumberRect(pWidget, &tResolved);
	return tGutter.fW > 0.0f && tGutter.fH > 0.0f &&
		fX >= tWorld.fX + tGutter.fX && fX < tWorld.fX + tGutter.fX + tGutter.fW &&
		fY >= tWorld.fY + tGutter.fY && fY < tWorld.fY + tGutter.fY + tGutter.fH;
}

static int __xuiTextEditQueryCursor(xui_widget pWidget, int iX, int iY, void* pUser)
{
	xui_text_edit_data_t* pData;

	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_CURSOR_NOT_ALLOWED;
	pData = (xui_text_edit_data_t*)pUser;
	if ( pData == NULL ) pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_CURSOR_IBEAM;
	if ( __xuiTextEditLineNumberHit(pWidget, pData, (float)iX, (float)iY) ) return XUI_CURSOR_ARROW;
	return XUI_CURSOR_IBEAM;
}

static void __xuiTextEditSelectionRange(xui_text_edit_data_t* pData, int* pStart, int* pEnd)
{
	int iStart;
	int iEnd;

	iStart = (pData != NULL) ? pData->iSelectStart : 0;
	iEnd = (pData != NULL) ? pData->iSelectEnd : 0;
	if ( iStart > iEnd ) {
		int iTmp = iStart;
		iStart = iEnd;
		iEnd = iTmp;
	}
	if ( pStart != NULL ) {
		*pStart = iStart;
	}
	if ( pEnd != NULL ) {
		*pEnd = iEnd;
	}
}

static int __xuiTextEditHasSelectionData(xui_text_edit_data_t* pData)
{
	int iStart;
	int iEnd;

	if ( pData == NULL ) {
		return 0;
	}
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	return iStart != iEnd;
}

static int __xuiTextEditSetSelectionData(xui_text_edit_data_t* pData, int iStart, int iEnd)
{
	if ( (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iStart = __xuiTextEditUtf8Clamp(pData->sText, pData->iTextLength, iStart);
	iEnd = __xuiTextEditUtf8Clamp(pData->sText, pData->iTextLength, iEnd);
	pData->iSelectStart = iStart;
	pData->iSelectEnd = iEnd;
	pData->iAnchor = iStart;
	pData->iCursor = iEnd;
	pData->bUndoInsertGroup = 0;
	pData->iUndoInsertEnd = 0;
	return XUI_OK;
}

static int __xuiTextEditClearSelectionData(xui_text_edit_data_t* pData)
{
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iSelectStart = pData->iCursor;
	pData->iSelectEnd = pData->iCursor;
	pData->iAnchor = pData->iCursor;
	return XUI_OK;
}

/* An entry remains counted until every actual paint owner has been consumed.
 * A late wait must preserve its source/Font and the entire retry carrier. */
static int __xuiTextEditPaintCacheEntryFree(xui_text_edit_data_t* data,
    xui_text_edit_paint_cache_t* entry,bool* complete)
{
    *complete=false;entry->bValid=0;
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,XUI_OK);
    bool consumed=false,called=false,chunks=false;
    int status=xuiInternalTextShapeFreeOutcome(&entry->tShape,&consumed,&called);
    bool waiting=!consumed && !called && status==XUI_ERROR_BUSY;
    if(!waiting) {
        if(called)xuiInternalContextCleanupOutcome(data->pContext,status,&receipt.status,&receipt.error);
        else xuiInternalCleanupAdd(&receipt,status);
    }
    status=xuiInternalTextChunkCacheDestroyPrepare(data->pContext,&entry->tChunks,&chunks);
    if(status!=XUI_STATUS_DEFERRED)xuiInternalCleanupAdd(&receipt,status);
    waiting=waiting || !consumed || !chunks;
    if(!waiting && entry->pFont && data->pContext->pOwnedProxy) {
        bool fontConsumed=false;
        status=xuiFontReleaseOwnerChecked(entry->pFont,&fontConsumed);
        xuiInternalContextCleanupOutcome(data->pContext,status,&receipt.status,&receipt.error);
        if(fontConsumed)entry->pFont=NULL;else waiting=true;
    }
    if(!waiting) {
        xrtFree(entry->sText);xrtFree(entry->pAdvances);memset(entry,0,sizeof(*entry));*complete=true;
    }
    status=xuiInternalCleanupEnd(NULL,&receipt);
    return status!=XUI_OK ? status:waiting ? XUI_STATUS_DEFERRED:XUI_OK;
}
static int __xuiTextEditPaintCacheClear(xui_text_edit_data_t* data)
{
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,XUI_OK);int kept=0;
    for(int i=0;i<data->iPaintCacheCount;++i) {
        bool complete=false;int status=__xuiTextEditPaintCacheEntryFree(data,&data->arrPaintCache[i],&complete);
        if(status!=XUI_STATUS_DEFERRED)xuiInternalCleanupAdd(&receipt,status);
        if(!complete) {
            if(kept!=i){data->arrPaintCache[kept]=data->arrPaintCache[i];memset(&data->arrPaintCache[i],0,sizeof(data->arrPaintCache[i]));}
            ++kept;
        }
    }
    data->iPaintCacheCount=kept;if(!kept)data->iPaintUseStamp=0;
    int status=xuiInternalCleanupEnd(NULL,&receipt);
    return status!=XUI_OK ? status:kept ? XUI_STATUS_DEFERRED:XUI_OK;
}
static int __xuiTextEditLayoutDrop(xui_text_layout* slot)
{
    if(!*slot)return XUI_OK;
    bool consumed=false;int status=xuiTextLayoutDestroyOwnerChecked(*slot,&consumed);
    if(consumed)*slot=NULL;
    return status==XUI_STATUS_DEFERRED ? XUI_ERROR_BUSY:status;
}
static int __xuiTextEditContentPrepare(xui_text_edit_data_t* data,int line,
    int start,int end,int newEnd)
{
    int status=__xuiTextEditLayoutDrop(&data->pWorkLayout);
    if(status!=XUI_OK)return status;
    if(line<0) {
        status=__xuiTextEditLayoutDrop(&data->tTextLayout.pLayout);
        if(status!=XUI_OK)return status;
        status=__xuiTextEditPaintCacheClear(data);
        return status==XUI_STATUS_DEFERRED ? XUI_ERROR_BUSY:status;
    }
    int at=0;
    for(;at<data->iPaintCacheCount;++at) {
        xui_text_edit_paint_cache_t* entry=&data->arrPaintCache[at];
        if(entry->iLine!=line)continue;
        int base=data->pLines[line].iStart;
        if(entry->bValid && entry->bChunked)
            status=xuiInternalTextChunkCachePrepareEdit(data->pContext,&entry->tChunks,start-base,end-base,newEnd-base);
        else {bool complete=false;status=__xuiTextEditPaintCacheEntryFree(data,entry,&complete);}
        if(status!=XUI_OK)break;
    }
    if(status!=XUI_OK)for(int i=0;i<data->iPaintCacheCount;++i)
        xuiInternalTextChunkCacheCancelEdit(&data->arrPaintCache[i].tChunks);
    return status==XUI_STATUS_DEFERRED ? XUI_ERROR_BUSY:status;
}
static xui_text_edit_paint_cache_t* __xuiTextEditPaintCacheEnsureImpl(xui_widget pWidget,
	xui_text_edit_data_t* pData, xui_font pFont, int iLine, const char* sText, int iTextSize)
{
	xui_text_edit_paint_cache_t* pEntry;
	xui_text_edit_paint_cache_t* pOldest;
	xui_text_item_t tItem;
	uint32_t iDpiGeneration;
	int bNewSlot;
	int bChunked;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pFont == NULL || sText == NULL || iTextSize <= 0 ) return NULL;
    for(int at=0;at<pData->iPaintCacheCount;++at)if(!pData->arrPaintCache[at].bValid) {
        bool complete=false;int status=__xuiTextEditPaintCacheEntryFree(pData,&pData->arrPaintCache[at],&complete);
        if(status!=XUI_OK || !complete)return NULL;
        if(at+1<pData->iPaintCacheCount)memmove(&pData->arrPaintCache[at],&pData->arrPaintCache[at+1],(size_t)(pData->iPaintCacheCount-at-1)*sizeof(pData->arrPaintCache[0]));
        memset(&pData->arrPaintCache[--pData->iPaintCacheCount],0,sizeof(pData->arrPaintCache[0]));--at;
    }
	iDpiGeneration = pWidget->pContext->iDpiGeneration;
	for ( i = 0; i < pData->iPaintCacheCount; i++ ) {
		pEntry = &pData->arrPaintCache[i];
		if ( pEntry->bValid && pEntry->iLine == iLine && pEntry->pFont == pFont &&
		     pEntry->iDpiGeneration == iDpiGeneration && pEntry->iTextSize == iTextSize &&
		     ((pEntry->bChunked && pEntry->tChunks.bValid) ||
		      (!pEntry->bChunked && pEntry->sText != NULL)) ) {
			pEntry->iUseStamp = ++pData->iPaintUseStamp;
			return pEntry;
		}
	}
	bChunked = iTextSize >= XUI_INTERNAL_TEXT_CHUNK_THRESHOLD &&
		xuiInternalTextCopyDisplay(sText, iTextSize, NULL) == iTextSize;
	bNewSlot = pData->iPaintCacheCount < XUI_TEXT_EDIT_PAINT_CACHE_LIMIT;
	if ( bNewSlot ) {
		pEntry = &pData->arrPaintCache[pData->iPaintCacheCount++];
	} else {
		pOldest = &pData->arrPaintCache[0];
		for ( i = 1; i < pData->iPaintCacheCount; i++ ) {
			if ( pData->arrPaintCache[i].iUseStamp < pOldest->iUseStamp ) pOldest = &pData->arrPaintCache[i];
		}
		pEntry = pOldest;
        bool complete=false;
        if(__xuiTextEditPaintCacheEntryFree(pData,pEntry,&complete)!=XUI_OK || !complete)return NULL;
	}
	pEntry->iLine = iLine;
	pEntry->iTextSize = iTextSize;
    iRet=xuiInternalFontAdmit(pData->pContext,pFont);
    if(iRet!=XUI_OK)return NULL;
	pEntry->pFont = pFont;
	pEntry->iDpiGeneration = iDpiGeneration;
	pEntry->iUseStamp = ++pData->iPaintUseStamp;
	if ( bChunked ) {
		iRet = xuiInternalTextChunkCacheEnsure(xuiWidgetGetContext(pWidget),
			&pEntry->tChunks, pFont, sText, iTextSize, iDpiGeneration);
		if ( iRet == XUI_OK ) {
			pEntry->bChunked = pEntry->bValid = 1;
			return pEntry;
		}
        /* Do not route a partial busy chunk through a second producer. */
        if(iRet!=XUI_ERROR_UNSUPPORTED)return NULL;
        bool chunks=false;
        iRet=xuiInternalTextChunkCacheDestroyPrepare(pData->pContext,&pEntry->tChunks,&chunks);
        if(iRet!=XUI_OK || !chunks)return NULL;
	}
	pEntry->sText = (char*)xrtMalloc((size_t)iTextSize + 1u);
	if ( pEntry->sText == NULL ) {
		return NULL;
	}
	memcpy(pEntry->sText, sText, (size_t)iTextSize);
	pEntry->sText[iTextSize] = '\0';
	memset(&tItem, 0, sizeof(tItem));
	tItem.iSize = sizeof(tItem);
	tItem.pFont = pFont;
	tItem.sText = pEntry->sText;
	tItem.iTextSize = iTextSize;
	tItem.iFlags = XUI_TEXT_SHAPE_DEFAULT | XUI_TEXT_SHAPE_RETAIN_PAINT;
	iRet = xuiTextShape(xuiWidgetGetContext(pWidget), &tItem, &pEntry->tShape);
	if ( iRet != XUI_OK ) {
        bool complete=false;(void)__xuiTextEditPaintCacheEntryFree(pData,pEntry,&complete);
		return NULL;
	}
	iRet = xuiInternalTextShapeCaretFragments(pEntry->sText, iTextSize, &pEntry->tShape);
	if ( iRet != XUI_OK ) {
        bool complete=false;(void)__xuiTextEditPaintCacheEntryFree(pData,pEntry,&complete);
		return NULL;
	}
	if ( pEntry->tShape.iClusterCount > 0 ) {
		pEntry->pAdvances = (double*)xrtMalloc(
			((size_t)pEntry->tShape.iClusterCount + 1u) * sizeof(*pEntry->pAdvances));
		if ( pEntry->pAdvances == NULL ) {
	        bool complete=false;(void)__xuiTextEditPaintCacheEntryFree(pData,pEntry,&complete);
			return NULL;
		}
		pEntry->pAdvances[0] = 0.0;
		for ( i = 0; i < pEntry->tShape.iClusterCount; i++ ) {
			pEntry->pAdvances[i + 1] = pEntry->pAdvances[i] +
				(double)pEntry->tShape.pClusters[i].fAdvance;
		}
	}
	pEntry->bValid=1;
	return pEntry;
}
static xui_text_edit_paint_cache_t* __xuiTextEditPaintCacheEnsure(xui_widget pWidget,xui_text_edit_data_t* pData,xui_font pFont,int iLine,const char* sText,int iTextSize)
{
    if(!pData || pData->bProducingPaint)return NULL;
    bool ownActivity=pData->iCacheActivity==0;xui_call_scope_t scope={0};
    if(ownActivity){if(xuiCallScopeBegin(pWidget->pContext,&scope)!=XUI_OK)return NULL;pData->iCacheActivity=2;}
    pData->bProducingPaint=1;
    xui_text_edit_paint_cache_t* result=__xuiTextEditPaintCacheEnsureImpl(pWidget,pData,pFont,iLine,sText,iTextSize);
    pData->bProducingPaint=0;
    if(ownActivity){pData->iCacheActivity=0;xerror* error=xrtTakeError();if(xuiInternalCallScopeEndRestored(&scope,XUI_OK,error)!=XUI_OK)result=NULL;}
    return result;
}


static xui_text_edit_paint_cache_t* __xuiTextEditLineShapeEnsure(xui_widget pWidget,
	xui_text_edit_data_t* pData, xui_font pFont, int iLine)
{
	xui_text_edit_line_t* pLine;
	if ( pData == NULL || iLine < 0 || iLine >= pData->iLineCount ) return NULL;
	pLine = &pData->pLines[iLine];
	if ( pLine->iEnd <= pLine->iStart ) return NULL;
	return __xuiTextEditPaintCacheEnsure(pWidget, pData, pFont, iLine,
		pData->sText + pLine->iStart, pLine->iEnd - pLine->iStart);
}

static float __xuiTextEditPaintCacheWidth(const xui_text_edit_paint_cache_t* pEntry)
{
	if ( pEntry == NULL ) return 0.0f;
	return pEntry->bChunked ? xuiInternalTextChunkCacheWidth(&pEntry->tChunks) :
		pEntry->tShape.fWidth;
}

static void __xuiTextEditPaintCachePatchLine(xui_widget pWidget,
	xui_text_edit_data_t* pData, int iLine, int iEditStart,
	int iOldEnd, int iNewEnd, int iNewLineSize)
{
	int iLineStart;
	int bPatched = 0;
	int i;
	if ( pWidget == NULL || pData == NULL || iLine < 0 ||
	     iLine >= pData->iLineCount ) return;
	iLineStart = pData->pLines[iLine].iStart;
	for ( i = pData->iPaintCacheCount - 1; i >= 0; i-- ) {
		xui_text_edit_paint_cache_t* pEntry = &pData->arrPaintCache[i];
		int bKeep = 0;
		if ( pEntry->iLine != iLine ) continue;
		if ( !bPatched && pEntry->bChunked && (pEntry->tChunks.bValid || pEntry->tChunks.iPreparation==2) &&
		     xuiInternalTextChunkCachePatch(xuiWidgetGetContext(pWidget),
			 &pEntry->tChunks, pData->sText + iLineStart, iNewLineSize,
			 iEditStart - iLineStart, iOldEnd - iLineStart,
			 iNewEnd - iLineStart) == XUI_OK ) {
			pEntry->iTextSize = iNewLineSize;
			pEntry->iUseStamp = ++pData->iPaintUseStamp;
			bPatched = 1;
			bKeep = 1;
		}
		if ( !bKeep ) {
	            bool complete=false;(void)__xuiTextEditPaintCacheEntryFree(pData,pEntry,&complete);
            if(!complete)continue;
			if ( i + 1 < pData->iPaintCacheCount ) {
				memmove(pEntry, pEntry + 1,
					(size_t)(pData->iPaintCacheCount - i - 1) * sizeof(*pEntry));
			}
			pData->iPaintCacheCount--;
			memset(&pData->arrPaintCache[pData->iPaintCacheCount], 0,
				sizeof(pData->arrPaintCache[0]));
		}
	}
}

static int __xuiTextEditLineShapeAdvance(xui_text_edit_paint_cache_t* pEntry,
	int iOffset, float* pAdvance)
{
	int iLo;
	int iHi;
	if ( pEntry == NULL || pAdvance == NULL || pEntry->pAdvances == NULL )
		return (pEntry != NULL && pEntry->bChunked) ?
			xuiInternalTextChunkCacheAdvance(&pEntry->tChunks, iOffset, pAdvance) :
			XUI_ERROR_UNSUPPORTED;
	*pAdvance = 0.0f;
	if ( iOffset <= 0 ) return XUI_OK;
	if ( iOffset >= pEntry->iTextSize ) {
		*pAdvance = (float)pEntry->pAdvances[pEntry->tShape.iClusterCount];
		return XUI_OK;
	}
	iLo = 0;
	iHi = pEntry->tShape.iClusterCount;
	while ( iLo < iHi ) {
		int iMid = iLo + (iHi - iLo) / 2;
		if ( pEntry->tShape.pClusters[iMid].iTextEnd <= iOffset ) iLo = iMid + 1;
		else iHi = iMid;
	}
	*pAdvance = (float)pEntry->pAdvances[iLo];
	return XUI_OK;
}

static int __xuiTextEditLineShapeHitTest(xui_text_edit_paint_cache_t* pEntry,
	float fAdvance, int bGlyph, int* pOffset)
{
	int iLo;
	int iHi;
	if ( pEntry == NULL || pOffset == NULL || pEntry->pAdvances == NULL )
		return (pEntry != NULL && pEntry->bChunked) ?
			xuiInternalTextChunkCacheHitTest(&pEntry->tChunks, fAdvance, bGlyph, pOffset) :
			XUI_ERROR_UNSUPPORTED;
	*pOffset = 0;
	if ( fAdvance <= 0.0f ) return XUI_OK;
	if ( (double)fAdvance >= pEntry->pAdvances[pEntry->tShape.iClusterCount] ) {
		*pOffset = pEntry->iTextSize;
		return XUI_OK;
	}
	iLo = 0;
	iHi = pEntry->tShape.iClusterCount;
	while ( iLo < iHi ) {
		int iMid = iLo + (iHi - iLo) / 2;
		if ( pEntry->pAdvances[iMid + 1] <= (double)fAdvance ) iLo = iMid + 1;
		else iHi = iMid;
	}
	if ( iLo >= pEntry->tShape.iClusterCount ) {
		*pOffset = pEntry->iTextSize;
	} else {
		double fLeft = pEntry->pAdvances[iLo];
		double fRight = pEntry->pAdvances[iLo + 1];
		*pOffset = (bGlyph || (double)fAdvance < (fLeft + fRight) * 0.5) ?
			pEntry->tShape.pClusters[iLo].iTextStart :
			pEntry->tShape.pClusters[iLo].iTextEnd;
	}
	return XUI_OK;
}

static void __xuiTextEditMarkLinesDirty(xui_text_edit_data_t* pData)
{
	if ( pData != NULL ) {
		pData->bLinesDirty = 1;
		pData->bShowHScrollBar = 0;
		pData->bShowVScrollBar = 0;
	}
}

static int __xuiTextEditInvalidateText(xui_widget pWidget)
{
	xui_text_edit_data_t* pData;

	pData = __xuiTextEditGetData(pWidget);
	__xuiTextEditMarkLinesDirty(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTextEditInvalidatePaint(xui_widget pWidget)
{
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTextEditInvalidateGeometry(xui_widget pWidget)
{
	return xuiWidgetInvalidate(pWidget,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTextEditAssignTextBytes(xui_text_edit_data_t* pData, const char* sText, int iSize)
{
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	if ( iSize < 0 ) {
		iRet = __xuiTextEditStringSize(sText, &iSize);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( iSize > INT_MAX - 1 ) return XUI_ERROR_BUFFER_TOO_SMALL;
	if ( pData->iMaxLength > 0 && iSize > pData->iMaxLength ) {
		iSize = __xuiTextEditUtf8ClampBytes(sText, pData->iMaxLength);
	}
	iRet = __xuiTextEditTextReserve(&pData->sText, &pData->iTextCapacity, iSize + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
    iRet=__xuiTextEditContentPrepare(pData,-1,0,0,0);if(iRet!=XUI_OK)return iRet;
	if ( iSize > 0 ) {
		memmove(pData->sText, sText, (size_t)iSize);
	}
	pData->sText[iSize] = '\0';
	pData->iTextLength = iSize;
	pData->iCursor = __xuiTextEditUtf8Clamp(pData->sText, iSize, pData->iCursor);
	pData->iSelectStart = __xuiTextEditUtf8Clamp(pData->sText, iSize, pData->iSelectStart);
	pData->iSelectEnd = __xuiTextEditUtf8Clamp(pData->sText, iSize, pData->iSelectEnd);
	pData->bUndoInsertGroup = 0;
	pData->iUndoInsertEnd = 0;
	__xuiTextEditMarkLinesDirty(pData);
	return XUI_OK;
}

static void __xuiTextEditHistoryStateFree(xui_text_edit_history_t* pState)
{
	if ( pState == NULL ) {
		return;
	}
	xrtFree(pState->sRemoved);
	xrtFree(pState->sInserted);
	memset(pState, 0, sizeof(*pState));
}

static void __xuiTextEditHistoryClear(xui_text_edit_history_t* pStates, int* pCount)
{
	int i;

	if ( (pStates == NULL) || (pCount == NULL) ) {
		return;
	}
	for ( i = 0; i < *pCount; i++ ) {
		__xuiTextEditHistoryStateFree(&pStates[i]);
	}
	*pCount = 0;
}

static int __xuiTextEditHistoryCopy(char** psCopy, int* pCapacity,
	const char* sText, int iLength)
{
	char* sCopy;
	if ( psCopy == NULL || pCapacity == NULL || iLength < 0 ||
	     (iLength > 0 && sText == NULL) )
		return XUI_ERROR_INVALID_ARGUMENT;
	*psCopy = NULL;
	*pCapacity = 0;
	if ( iLength == 0 ) return XUI_OK;
	sCopy = (char*)xrtMalloc((size_t)iLength);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(sCopy, sText, (size_t)iLength);
	*psCopy = sCopy;
	*pCapacity = iLength;
	return XUI_OK;
}

static int __xuiTextEditHistoryBegin(xui_text_edit_data_t* pData,
	xui_text_edit_history_t* pState, int iStart, int iEnd,
	const char* sInserted, int iInsertedLength)
{
	int iRet;
	if ( pData == NULL || pState == NULL || pData->sText == NULL ||
	     iStart < 0 || iEnd < iStart || iEnd > pData->iTextLength ||
	     iInsertedLength < 0 || (iInsertedLength > 0 && sInserted == NULL) )
		return XUI_ERROR_INVALID_ARGUMENT;
	memset(pState, 0, sizeof(*pState));
	pState->iStart = iStart;
	pState->iRemovedLength = iEnd - iStart;
	pState->iInsertedLength = iInsertedLength;
	pState->iBeforeCursor = pData->iCursor;
	pState->iBeforeSelectStart = pData->iSelectStart;
	pState->iBeforeSelectEnd = pData->iSelectEnd;
	pState->fBeforeScrollX = pData->fScrollX;
	pState->fBeforeScrollY = pData->fScrollY;
	iRet = __xuiTextEditHistoryCopy(&pState->sRemoved, &pState->iRemovedCapacity,
		pData->sText + iStart, pState->iRemovedLength);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditHistoryCopy(&pState->sInserted,
		&pState->iInsertedCapacity,
		sInserted, iInsertedLength);
	if ( iRet != XUI_OK ) __xuiTextEditHistoryStateFree(pState);
	return iRet;
}

static void __xuiTextEditHistoryFinish(xui_text_edit_data_t* pData,
	xui_text_edit_history_t* pState)
{
	if ( pData == NULL || pState == NULL ) return;
	pState->iAfterCursor = pData->iCursor;
	pState->iAfterSelectStart = pData->iSelectStart;
	pState->iAfterSelectEnd = pData->iSelectEnd;
	pState->fAfterScrollX = pData->fScrollX;
	pState->fAfterScrollY = pData->fScrollY;
}

static int __xuiTextEditHistoryPush(xui_text_edit_history_t* pStates, int* pCount, const xui_text_edit_history_t* pState)
{
	size_t iBytes;
	int i;
	if ( pStates == NULL || pCount == NULL || pState == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iBytes = (size_t)pState->iRemovedLength + (size_t)pState->iInsertedLength;
	for ( i = 0; i < *pCount; i++ )
		iBytes += (size_t)pStates[i].iRemovedLength + (size_t)pStates[i].iInsertedLength;
	while ( *pCount > 0 && (*pCount >= XUI_TEXT_EDIT_HISTORY_LIMIT ||
	       iBytes > XUI_TEXT_EDIT_HISTORY_BYTES_LIMIT) ) {
		iBytes -= (size_t)pStates[0].iRemovedLength + (size_t)pStates[0].iInsertedLength;
		__xuiTextEditHistoryStateFree(&pStates[0]);
		memmove(pStates, pStates + 1, sizeof(pStates[0]) * (size_t)(*pCount - 1));
		(*pCount)--;
	}
	pStates[*pCount] = *pState;
	(*pCount)++;
	return XUI_OK;
}

static int __xuiTextEditHistoryCommit(xui_text_edit_data_t* pData,
	xui_text_edit_history_t* pState)
{
	int iRet;
	if ( pData == NULL || pState == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTextEditHistoryPush(pData->arrUndo, &pData->iUndoCount, pState);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memset(pState, 0, sizeof(*pState));
	__xuiTextEditHistoryClear(pData->arrRedo, &pData->iRedoCount);
	return XUI_OK;
}

static int __xuiTextEditHistoryCoalesceInsert(xui_text_edit_data_t* pData,
	const char* sText, int iTextSize)
{
	xui_text_edit_history_t* pState;
	int iNewLength;
	int iRet;
	if ( pData == NULL || sText == NULL || iTextSize <= 0 || pData->iUndoCount <= 0 )
		return XUI_ERROR_INVALID_ARGUMENT;
	pState = &pData->arrUndo[pData->iUndoCount - 1];
	if ( pState->iRemovedLength != 0 || pState->iStart < 0 ||
	     pState->iInsertedLength < 0 || pData->iUndoInsertEnd < 0 ||
	     (size_t)pState->iStart + (size_t)pState->iInsertedLength !=
		(size_t)pData->iUndoInsertEnd )
		return XUI_ERROR_INVALID_STATE;
	if ( (size_t)pState->iInsertedLength + (size_t)iTextSize >
	     XUI_TEXT_EDIT_HISTORY_BYTES_LIMIT ) return XUI_ERROR_INVALID_STATE;
	if ( pState->iInsertedLength > INT_MAX - iTextSize )
		return XUI_ERROR_BUFFER_TOO_SMALL;
	iNewLength = pState->iInsertedLength + iTextSize;
	iRet = __xuiTextEditTextReserve(&pState->sInserted,
		&pState->iInsertedCapacity, iNewLength);
	if ( iRet != XUI_OK ) return iRet;
	memcpy(pState->sInserted + pState->iInsertedLength,
		sText, (size_t)iTextSize);
	pState->iInsertedLength = iNewLength;
	return XUI_OK;
}

static void __xuiTextEditNotifyChangeEx(xui_widget pWidget,
	xui_text_edit_data_t* pData, int bClearFindResults)
{
	xui_context pContext;
	xui_text_edit_change_proc onChange;

	if ( pData == NULL ) return;
	pContext = xuiWidgetGetContext(pWidget);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_VALUE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	if ( pData->iChangeCount < INT_MAX ) pData->iChangeCount++;
	if ( bClearFindResults ) __xuiTextEditInvalidateFindState(pData);
	onChange = pData->onChange;
	if ( onChange != NULL ) XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onChange, xui_text_edit_change_proc, pData->pChangeUser, pWidget, (pData->sText != NULL) ? pData->sText : "");
	if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) return;
	(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_TEXT_CHANGED,
		(pData->sText != NULL) ? pData->sText : "", pData->iSelectStart,
		pData->iSelectEnd, 0, 0, 1);
}

static void __xuiTextEditNotifyChange(xui_widget pWidget,
	xui_text_edit_data_t* pData)
{
	__xuiTextEditNotifyChangeEx(pWidget, pData, 1);
}

static int __xuiTextEditLineReserve(xui_text_edit_data_t* pData, int iCapacity)
{
	xui_text_edit_line_t* pLines;
	int iRet;

	if ( (pData == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCapacity <= pData->iLineCapacity ) {
		return XUI_OK;
	}
	iRet = __xuiTextEditArrayCapacity(pData->iLineCapacity, iCapacity,
		16, sizeof(*pLines), &iCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pLines = (xui_text_edit_line_t*)xrtRealloc(pData->pLines, sizeof(*pLines) * (size_t)iCapacity);
	if ( pLines == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->pLines = pLines;
	pData->iLineCapacity = iCapacity;
	return XUI_OK;
}

static float __xuiTextEditLineHeight(xui_widget pWidget, xui_font pFont, float fLineGap)
{
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;
	float fHeight;

	fHeight = 16.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (pFont != NULL) &&
	     (pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK) &&
	     (tMetrics.fLineHeight > 0.0f) ) {
		fHeight = tMetrics.fLineHeight;
	}
	fHeight += fLineGap;
	if ( fHeight < 12.0f ) {
		fHeight = 12.0f;
	}
	return fHeight;
}

static xui_vec2_t __xuiTextEditMeasureRange(xui_widget pWidget, xui_text_edit_data_t* pData, xui_font pFont, int iStart, int iEnd)
{
	xui_vec2_t tSize = {0.0f, pData->fLineHeight};
	xui_text_edit_paint_cache_t* pEntry;
	int lo = 0, hi = pData->iLineCount;
	if ( iEnd <= iStart ) return tSize;
	while ( lo < hi ) {
		int mid = lo + (hi - lo) / 2;
		if ( pData->pLines[mid].iStart <= iStart ) lo = mid + 1; else hi = mid;
	}
	if ( lo > 0 ) {
		float fStart = 0.0f, fEnd = 0.0f;
		if ( pData->tTextLayout.pLayout != NULL ) {
			(void)xuiInternalTextLayoutLineAdvance(pData->tTextLayout.pLayout, lo - 1, iStart, &fStart);
			(void)xuiInternalTextLayoutLineAdvance(pData->tTextLayout.pLayout, lo - 1, iEnd, &fEnd);
		} else {
			pEntry = __xuiTextEditLineShapeEnsure(pWidget, pData, pFont, lo - 1);
			(void)__xuiTextEditLineShapeAdvance(pEntry,
				iStart - pData->pLines[lo - 1].iStart, &fStart);
			(void)__xuiTextEditLineShapeAdvance(pEntry,
				iEnd - pData->pLines[lo - 1].iStart, &fEnd);
		}
		tSize.fX = fEnd - fStart;
	}
	return tSize;
}

static int __xuiTextEditNextVisualCaret(xui_text_edit_data_t* pData, int iEnd, int iOffset)
{
	int iNext;
	if ( pData->tTextLayout.pLayout != NULL &&
	     xuiInternalTextLayoutNextCaret(pData->tTextLayout.pLayout, iOffset, iEnd, &iNext) == XUI_OK &&
	     iNext > iOffset ) return iNext;
	return xuiInternalTextGraphemeNext(pData->sText, iEnd, iOffset);
}

static int __xuiTextEditBuildLinesImpl(xui_widget pWidget, xui_text_edit_data_t* pData, xui_font pFont, float fWrapWidth)
{
	xui_text_edit_paint_cache_t* pEntry;
	xui_text_layout_desc_t tDesc = {0};
	xui_text_layout pLayout = NULL;
	xui_text_line_t tLine;
	float fContentWidth = 0.0f;
	int i, iCount, iRet, iStart = 0, iTextSize;
	uint32_t iDpi;
	if ( pWidget == NULL || pData == NULL || pData->sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iDpi = pWidget->pContext->iDpiGeneration;
	if ( !pData->bLinesDirty && (!pData->bWordWrap || pData->fLastLayoutWidth == fWrapWidth) &&
	     pData->tTextLayout.pFont == pFont && pData->tTextLayout.iDpiGeneration == iDpi ) return XUI_OK;
	pData->bLinesDirty = 1;
	iRet=__xuiTextEditPaintCacheClear(pData);
	if(iRet!=XUI_OK)return iRet==XUI_STATUS_DEFERRED ? XUI_ERROR_BUSY:iRet;
	iTextSize = pData->iTextLength;
	if ( iTextSize == 0 ) {
		iRet = __xuiTextEditLineReserve(pData, 1);
		if ( iRet != XUI_OK ) return iRet;
		pData->fLineHeight = __xuiTextEditLineHeight(pWidget, pFont, pData->fLineGap);
		pData->pLines[0].iStart = 0;
		pData->pLines[0].iEnd = 0;
		pData->pLines[0].iPhysicalLine = 1;
		pData->pLines[0].fY = 0.0f;
		pData->pLines[0].fW = 0.0f;
        iRet=__xuiTextEditLayoutDrop(&pData->tTextLayout.pLayout);if(iRet!=XUI_OK)return iRet;
        iRet=__xuiTextEditLayoutDrop(&pData->pWorkLayout);if(iRet!=XUI_OK)return iRet;
		pData->tTextLayout.pFont = pFont;
		pData->tTextLayout.iDpiGeneration = iDpi;
		pData->iLineCount = 1;
		pData->bDirectLayout = 1;
		pData->fContentWidth = 0.0f;
		pData->fContentHeight = pData->fLineHeight;
		pData->fLastLayoutWidth = fWrapWidth;
		pData->bLinesDirty = 0;
		return XUI_OK;
	}
	if ( !pData->bWordWrap && iTextSize >= XUI_INTERNAL_TEXT_CHUNK_THRESHOLD &&
	     memchr(pData->sText, '\r', (size_t)iTextSize) == NULL &&
	     memchr(pData->sText, '\n', (size_t)iTextSize) == NULL &&
	     xuiInternalTextCopyDisplay(pData->sText, iTextSize, NULL) == iTextSize ) {
		iRet = __xuiTextEditLineReserve(pData, 1);
		if ( iRet != XUI_OK ) return iRet;
		pData->fLineHeight = __xuiTextEditLineHeight(pWidget, pFont, pData->fLineGap);
		pData->pLines[0].iStart = 0;
		pData->pLines[0].iEnd = iTextSize;
		pData->pLines[0].iPhysicalLine = 1;
		pData->pLines[0].fY = 0.0f;
		pData->pLines[0].fW = 0.0f;
        iRet=__xuiTextEditLayoutDrop(&pData->tTextLayout.pLayout);if(iRet!=XUI_OK)return iRet;
        iRet=__xuiTextEditLayoutDrop(&pData->pWorkLayout);if(iRet!=XUI_OK)return iRet;
		pData->tTextLayout.pFont = pFont;
		pData->tTextLayout.iDpiGeneration = iDpi;
		pData->iLineCount = 1;
		pData->bDirectLayout = 1;
		if ( pFont != NULL ) {
			pEntry = __xuiTextEditLineShapeEnsure(pWidget, pData, pFont, 0);
			if ( pEntry == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
			pData->pLines[0].fW = __xuiTextEditPaintCacheWidth(pEntry);
		}
		pData->fContentWidth = pData->pLines[0].fW;
		pData->fContentHeight = pData->fLineHeight;
		pData->fLastLayoutWidth = fWrapWidth;
		pData->bLinesDirty = 0;
		return XUI_OK;
	}
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = pData->sText;
	tDesc.iTextSize = iTextSize;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = fWrapWidth;
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = pData->bWordWrap && fWrapWidth > 16.0f ? XUI_TEXT_WRAP_WORD : XUI_TEXT_WRAP_NONE;
	if ( pFont != NULL ) {
		iRet = xuiInternalTextLayoutEnsure(pWidget->pContext, &pData->pWorkLayout, &tDesc);
        pLayout=pData->pWorkLayout;
		if ( iRet != XUI_OK ) return iRet;
		iCount = xuiTextLayoutGetLineCount(pLayout);
	} else {
		/* Editing remains available before a font is installed; only hard lines exist. */
		int iEnd, iNext, iAt = 0;
		iCount = 0;
		do {
			(void)xuiInternalTextNextHardLine(pData->sText, iTextSize, iAt, &iEnd, &iNext);
			iCount++;
			iAt = iNext;
		} while ( iEnd < iTextSize );
	}
	if ( iCount <= 0 ) { return XUI_ERROR_INVALID_STATE; }
	iRet = __xuiTextEditLineReserve(pData, iCount);
	if ( iRet != XUI_OK ) { return iRet; }
	pData->fLineHeight = __xuiTextEditLineHeight(pWidget, pFont, pData->fLineGap);
	for ( i = 0; i < iCount; i++ ) {
		xui_text_edit_line_t* pLine = &pData->pLines[i];
		if ( pLayout != NULL ) {
			iRet = xuiTextLayoutGetLine(pLayout, i, &tLine);
			if ( iRet != XUI_OK ) { return iRet; }
		} else {
			int iEnd, iNext;
			memset(&tLine, 0, sizeof(tLine));
			(void)xuiInternalTextNextHardLine(pData->sText, iTextSize, iStart, &iEnd, &iNext);
			tLine.iTextOffset = iStart;
			tLine.iTextSize = iEnd - iStart;
			tLine.fW = 0.0f;
			iStart = iNext;
		}
		pLine->iStart = tLine.iTextOffset;
		pLine->iEnd = tLine.iTextOffset + tLine.iTextSize;
		pLine->iPhysicalLine = (i > 0) ? pData->pLines[i - 1].iPhysicalLine : 1;
		if ( i > 0 && pLine->iStart > pData->pLines[i - 1].iEnd ) pLine->iPhysicalLine++;
		pLine->fY = (float)i * pData->fLineHeight;
		pLine->fW = tLine.fW;
		if ( tLine.fW > fContentWidth ) fContentWidth = tLine.fW;
	}
    iRet=__xuiTextEditLayoutDrop(&pData->tTextLayout.pLayout);if(iRet!=XUI_OK)return iRet;
	/* Identity projection lets edits patch the affected hard line without
	 * rebuilding the document-wide layout. Source-mapped text keeps that layout. */
	pData->bDirectLayout = xuiInternalTextCopyDisplay(pData->sText, iTextSize, NULL) == iTextSize;
	if ( !pData->bDirectLayout ) {
        pData->tTextLayout.pLayout=pData->pWorkLayout;pData->pWorkLayout=NULL;
    }
    /* Identity projection keeps this one reusable owned work layout instead
     * of a stack-local carrier. It also owns the resolved Font cache key. */
	pData->tTextLayout.pFont = pFont;
	pData->tTextLayout.iDpiGeneration = iDpi;
	pData->iLineCount = iCount;
	pData->fContentWidth = fContentWidth;
	pData->fContentHeight = (float)iCount * pData->fLineHeight;
	pData->fLastLayoutWidth = fWrapWidth;
	pData->bLinesDirty = 0;
	return XUI_OK;
}
static int __xuiTextEditBuildLines(xui_widget pWidget,xui_text_edit_data_t* pData,xui_font pFont,float fWrapWidth)
{
    if(!pData || pData->bBuildingLines)return XUI_ERROR_BUSY;
    bool ownActivity=pData->iCacheActivity==0;xui_call_scope_t scope={0};
    if(ownActivity){int status=xuiCallScopeBegin(pWidget->pContext,&scope);if(status!=XUI_OK)return status;pData->iCacheActivity=2;}
    pData->bBuildingLines=1;int status=__xuiTextEditBuildLinesImpl(pWidget,pData,pFont,fWrapWidth);pData->bBuildingLines=0;
    if(ownActivity){pData->iCacheActivity=0;xerror* error=xrtTakeError();status=xuiInternalCallScopeEndRestored(&scope,status,error);}
    return status;
}


static xui_rect_t __xuiTextEditViewportFromRaw(xui_rect_t tRaw, int bShowH, int bShowV)
{
	xui_rect_t tViewport;

	tViewport = tRaw;
	if ( bShowV ) {
		tViewport.fW = __xuiTextEditMaxFloat(0.0f, tViewport.fW - XUI_TEXT_EDIT_SCROLLBAR_SIZE);
	}
	if ( bShowH ) {
		tViewport.fH = __xuiTextEditMaxFloat(0.0f, tViewport.fH - XUI_TEXT_EDIT_SCROLLBAR_SIZE);
	}
	return tViewport;
}

static int __xuiTextEditUpdateScrollModel(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tRaw;
	xui_rect_t tViewport;
	float fWrapWidth;
	float fContentWidth;
	float fContentHeight;
	float fScrollX;
	float fScrollY;
	float fMaxX;
	float fMaxY;
	float fLineHeight;
	int bShowH;
	int bShowV;
	int bNextH;
	int bNextV;
	int i;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tRaw = __xuiTextEditRawTextContentRect(pWidget, &tResolved);
	bShowH = pData->bShowHScrollBar;
	bShowV = pData->bShowVScrollBar;
	for ( i = 0; i < 4; i++ ) {
		tViewport = __xuiTextEditViewportFromRaw(tRaw, bShowH, bShowV);
		fWrapWidth = (tViewport.fW > 1.0f) ? tViewport.fW : 1.0f;
		iRet = __xuiTextEditBuildLines(pWidget, pData, tResolved.pFont, fWrapWidth);
		if ( iRet != XUI_OK ) return iRet;
		bNextH = (!pData->bWordWrap && (pData->fContentWidth + 4.0f > tViewport.fW + 0.01f)) ? 1 : 0;
		bNextV = (pData->fContentHeight > tViewport.fH + 0.01f) ? 1 : 0;
		if ( (bNextH == bShowH) && (bNextV == bShowV) ) break;
		bShowH = bNextH;
		bShowV = bNextV;
	}
	tViewport = __xuiTextEditViewportFromRaw(tRaw, bShowH, bShowV);
	fWrapWidth = (tViewport.fW > 1.0f) ? tViewport.fW : 1.0f;
	iRet = __xuiTextEditBuildLines(pWidget, pData, tResolved.pFont, fWrapWidth);
	if ( iRet != XUI_OK ) return iRet;
	pData->bShowHScrollBar = bShowH;
	pData->bShowVScrollBar = bShowV;
	pData->tScrollViewportRect = tViewport;
	pData->tHScrollBarRect = bShowH ? (xui_rect_t){tRaw.fX, tRaw.fY + tViewport.fH, tViewport.fW, XUI_TEXT_EDIT_SCROLLBAR_SIZE} : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tVScrollBarRect = bShowV ? (xui_rect_t){tRaw.fX + tViewport.fW, tRaw.fY, XUI_TEXT_EDIT_SCROLLBAR_SIZE, tViewport.fH} : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( bShowH && !bShowV ) pData->tHScrollBarRect.fW = tRaw.fW;
	if ( bShowV && !bShowH ) pData->tVScrollBarRect.fH = tRaw.fH;
	fContentWidth = pData->bWordWrap ? tViewport.fW : (pData->fContentWidth + 4.0f);
	fContentHeight = pData->fContentHeight;
	if ( fContentWidth < tViewport.fW ) fContentWidth = tViewport.fW;
	if ( fContentHeight < tViewport.fH ) fContentHeight = tViewport.fH;
	iRet = xuiScrollModelSetViewport(&pData->tScrollModel, (xui_rect_t){0.0f, 0.0f, tViewport.fW, tViewport.fH});
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetContentSize(&pData->tScrollModel, fContentWidth, fContentHeight);
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetOffset(&pData->tScrollModel, pData->fScrollX, pData->fScrollY);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiScrollModelGetOffset(&pData->tScrollModel, &fScrollX, &fScrollY);
	pData->fScrollX = fScrollX;
	pData->fScrollY = fScrollY;
	if ( (pData->pHScrollBar != NULL) && (pData->pVScrollBar != NULL) ) {
		(void)xuiScrollModelGetMaxOffset(&pData->tScrollModel, &fMaxX, &fMaxY);
		fLineHeight = (pData->fLineHeight > 0.0f) ? pData->fLineHeight : 16.0f;
		pData->bSyncingScrollBars = 1;
		iRet = xuiWidgetSetRect(pData->pHScrollBar, pData->tHScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetRect(pData->pVScrollBar, pData->tVScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pHScrollBar, bShowH);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pVScrollBar, bShowV);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pHScrollBar, 0.0f, fMaxX, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pVScrollBar, 0.0f, fMaxY, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pHScrollBar, 24.0f, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pVScrollBar, fLineHeight, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pHScrollBar, pData->fScrollX);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pVScrollBar, pData->fScrollY);
		pData->bSyncingScrollBars = 0;
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTextEditFindLineForPos(xui_text_edit_data_t* pData, int iPos)
{
	int iLo;
	int iHi;

	if ( (pData == NULL) || (pData->iLineCount <= 0) ) {
		return 0;
	}
	iLo = 0;
	iHi = pData->iLineCount;
	while ( iLo < iHi ) {
		int iMid = iLo + (iHi - iLo) / 2;
		if ( pData->pLines[iMid].iEnd < iPos ) iLo = iMid + 1;
		else iHi = iMid;
	}
	if ( iLo >= pData->iLineCount ) return pData->iLineCount - 1;
	if ( iPos < pData->pLines[iLo].iStart && iLo > 0 ) return iLo - 1;
	return iLo;
}

static int __xuiTextEditDirectPatchPrepare(xui_text_edit_data_t* pData,
	int iStart, int iEnd, const char* sInsert, int iInsertSize, int* pLine)
{
	int i;
	int iLine;
	int iEndLine;
	int iPhysicalLine;
	if ( pLine == NULL ) return 0;
	*pLine = -1;
	if ( pData == NULL || !pData->bDirectLayout || pData->bLinesDirty ||
	     pData->tTextLayout.pLayout != NULL || pData->iLineCount <= 0 ) return 0;
	for ( i = iStart; i < iEnd; i++ ) {
		if ( pData->sText[i] == '\r' || pData->sText[i] == '\n' ) return 0;
	}
	for ( i = 0; i < iInsertSize; i++ ) {
		if ( sInsert[i] == '\r' || sInsert[i] == '\n' ) return 0;
	}
	if ( iInsertSize > 0 && xuiInternalTextCopyDisplay(sInsert, iInsertSize, NULL) != iInsertSize ) return 0;
	iLine = __xuiTextEditFindLineForPos(pData, iStart);
	iEndLine = __xuiTextEditFindLineForPos(pData, iEnd);
	if ( iLine < 0 || iLine >= pData->iLineCount || iEndLine < 0 ||
	     iEndLine >= pData->iLineCount ) return 0;
	iPhysicalLine = pData->pLines[iLine].iPhysicalLine;
	if ( pData->pLines[iEndLine].iPhysicalLine != iPhysicalLine ) return 0;
	while ( iLine > 0 && pData->pLines[iLine - 1].iPhysicalLine == iPhysicalLine ) iLine--;
	*pLine = iLine;
	return 1;
}

static int __xuiTextEditDirectPatchApply(xui_widget pWidget,
	xui_text_edit_data_t* pData, int iLine, int iDelta,
	int iEditStart, int iOldEnd, int iNewEnd)
{
	xui_text_edit_data_t tResolved;
	xui_text_edit_paint_cache_t* pEntry;
	xui_text_layout_desc_t tDesc = {0};
	xui_text_layout pLayout = NULL;
	xui_text_line_t tLine;
	float fContentWidth;
	int iOldLineEnd;
	int iOldCount;
	int iNewCount;
	int iNewTotal;
	int iParagraphStart;
	int iParagraphEnd;
	int iPhysicalLine;
	int iTailCount;
	int iRet;
	int i;
	if ( pWidget == NULL || pData == NULL || iLine < 0 || iLine >= pData->iLineCount )
		return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	if ( pData->bWordWrap && pData->fLastLayoutWidth > 16.0f && tResolved.pFont != NULL ) {
		iPhysicalLine = pData->pLines[iLine].iPhysicalLine;
		iOldLineEnd = iLine + 1;
		while ( iOldLineEnd < pData->iLineCount &&
		        pData->pLines[iOldLineEnd].iPhysicalLine == iPhysicalLine ) iOldLineEnd++;
		iOldCount = iOldLineEnd - iLine;
		iParagraphStart = pData->pLines[iLine].iStart;
		iParagraphEnd = pData->pLines[iOldLineEnd - 1].iEnd + iDelta;
		if ( iParagraphEnd < iParagraphStart || iParagraphEnd > pData->iTextLength )
			return XUI_ERROR_INVALID_STATE;
		tDesc.iSize = sizeof(tDesc);
		tDesc.sText = pData->sText + iParagraphStart;
		tDesc.iTextSize = iParagraphEnd - iParagraphStart;
		tDesc.pFont = tResolved.pFont;
		tDesc.fMaxWidth = pData->fLastLayoutWidth;
		tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
		tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
		iRet = xuiInternalTextLayoutEnsure(pWidget->pContext, &pData->pWorkLayout, &tDesc);
        pLayout=pData->pWorkLayout;
		if ( iRet != XUI_OK ) return iRet;
		iNewCount = xuiTextLayoutGetLineCount(pLayout);
		if ( iNewCount <= 0 ) {
			return XUI_ERROR_INVALID_STATE;
		}
		iNewTotal = pData->iLineCount - iOldCount + iNewCount;
		iRet = __xuiTextEditLineReserve(pData, iNewTotal);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iTailCount = pData->iLineCount - iOldLineEnd;
		if ( iTailCount > 0 ) {
			memmove(pData->pLines + iLine + iNewCount,
				pData->pLines + iOldLineEnd,
				sizeof(*pData->pLines) * (size_t)iTailCount);
		}
		for ( i = 0; i < iNewCount; i++ ) {
			iRet = xuiTextLayoutGetLine(pLayout, i, &tLine);
			if ( iRet != XUI_OK ) {
					return iRet;
			}
			pData->pLines[iLine + i].iStart = iParagraphStart + tLine.iTextOffset;
			pData->pLines[iLine + i].iEnd = iParagraphStart + tLine.iTextOffset + tLine.iTextSize;
			pData->pLines[iLine + i].iPhysicalLine = iPhysicalLine;
			pData->pLines[iLine + i].fW = tLine.fW;
		}
		for ( i = iLine + iNewCount; i < iNewTotal; i++ ) {
			pData->pLines[i].iStart += iDelta;
			pData->pLines[i].iEnd += iDelta;
		}
		pData->iLineCount = iNewTotal;
		iRet=__xuiTextEditPaintCacheClear(pData);
		if(iRet!=XUI_OK)return iRet==XUI_STATUS_DEFERRED ? XUI_ERROR_BUSY:iRet;
	} else {
		int iNewLineSize = pData->pLines[iLine].iEnd -
			pData->pLines[iLine].iStart + iDelta;
		__xuiTextEditPaintCachePatchLine(pWidget, pData, iLine,
			iEditStart, iOldEnd, iNewEnd, iNewLineSize);
		pData->pLines[iLine].iEnd += iDelta;
		for ( i = iLine + 1; i < pData->iLineCount; i++ ) {
			pData->pLines[i].iStart += iDelta;
			pData->pLines[i].iEnd += iDelta;
		}
	}
	if ( !pData->bWordWrap || pData->fLastLayoutWidth <= 16.0f || tResolved.pFont == NULL ) {
		pEntry = __xuiTextEditLineShapeEnsure(pWidget, pData, tResolved.pFont, iLine);
		pData->pLines[iLine].fW = __xuiTextEditPaintCacheWidth(pEntry);
	}
	fContentWidth = 0.0f;
	for ( i = 0; i < pData->iLineCount; i++ ) {
		pData->pLines[i].fY = (float)i * pData->fLineHeight;
		if ( pData->pLines[i].fW > fContentWidth ) fContentWidth = pData->pLines[i].fW;
	}
	pData->fContentWidth = fContentWidth;
	pData->fContentHeight = (float)pData->iLineCount * pData->fLineHeight;
	pData->bLinesDirty = 0;
	return XUI_OK;
}

static void __xuiTextEditClampScroll(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	(void)__xuiTextEditUpdateScrollModel(pWidget, pData);
}

static int __xuiTextEditUpdateCursorRect(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tContent;
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix;
	int iLine;
	int iCursor;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	iLine = __xuiTextEditFindLineForPos(pData, pData->iCursor);
	pLine = &pData->pLines[iLine];
	iCursor = pData->iCursor;
	if ( iCursor < pLine->iStart ) iCursor = pLine->iStart;
	if ( iCursor > pLine->iEnd ) iCursor = pLine->iEnd;
	tPrefix = __xuiTextEditMeasureRange(pWidget, pData, tResolved.pFont, pLine->iStart, iCursor);
	pData->tCursorRect = (xui_rect_t){
		tContent.fX + tPrefix.fX - pData->fScrollX,
		tContent.fY + pLine->fY - pData->fScrollY + 2.0f,
		1.0f,
		pData->fLineHeight - 4.0f
	};
	pData->tTextRect = tContent;
	return XUI_OK;
}

static int __xuiTextEditEnsureCursorVisible(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tContent;
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix;
	float fCursorX;
	float fCursorY;
	int iLine;
	int iCursor;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	iLine = __xuiTextEditFindLineForPos(pData, pData->iCursor);
	pLine = &pData->pLines[iLine];
	iCursor = pData->iCursor;
	if ( iCursor < pLine->iStart ) iCursor = pLine->iStart;
	if ( iCursor > pLine->iEnd ) iCursor = pLine->iEnd;
	tPrefix = __xuiTextEditMeasureRange(pWidget, pData, tResolved.pFont, pLine->iStart, iCursor);
	fCursorX = tPrefix.fX;
	fCursorY = pLine->fY;
	if ( fCursorX < pData->fScrollX ) {
		pData->fScrollX = fCursorX;
	} else if ( fCursorX > pData->fScrollX + tContent.fW - 2.0f ) {
		pData->fScrollX = fCursorX - tContent.fW + 2.0f;
	}
	if ( fCursorY < pData->fScrollY ) {
		pData->fScrollY = fCursorY;
	} else if ( fCursorY + pData->fLineHeight > pData->fScrollY + tContent.fH ) {
		pData->fScrollY = fCursorY + pData->fLineHeight - tContent.fH;
	}
	__xuiTextEditClampScroll(pWidget, pData);
	return __xuiTextEditUpdateCursorRect(pWidget, pData);
}

static int __xuiTextEditSyncCursor(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_context pContext;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTextEditEnsureCursorVisible(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( (pContext != NULL) && (xuiGetFocusWidget(pContext) == pWidget) ) {
		(void)xuiInternalInputSyncIme(pContext);
	}
	return __xuiTextEditInvalidatePaint(pWidget);
}

static int __xuiTextEditSetTextInternalImpl(xui_widget pWidget, xui_text_edit_data_t* pData, const char* sText, int bNotify, int bUndo)
{
	xui_text_edit_history_t tHistory;
	int iNewSize;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tHistory, 0, sizeof(tHistory));
	if ( sText == NULL ) sText = "";
	iRet = __xuiTextEditStringSize(sText, &iNewSize);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iMaxLength > 0 && iNewSize > pData->iMaxLength )
		iNewSize = __xuiTextEditUtf8ClampBytes(sText, pData->iMaxLength);
	if ( bUndo ) {
		iRet = __xuiTextEditHistoryBegin(pData, &tHistory, 0,
			pData->iTextLength, sText, iNewSize);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iRet = __xuiTextEditAssignTextBytes(pData, sText, -1);
	if ( iRet != XUI_OK ) {
		__xuiTextEditHistoryStateFree(&tHistory);
		return iRet;
	}
	pData->iCursor = pData->iTextLength;
	(void)__xuiTextEditClearSelectionData(pData);
	__xuiTextEditImeReset(pData);
	__xuiTextEditInvalidateFindState(pData);
	int geometryStatus=__xuiTextEditEnsureCursorVisible(pWidget,pData);
	if ( bUndo ) {
		__xuiTextEditHistoryFinish(pData, &tHistory);
		iRet = __xuiTextEditHistoryCommit(pData, &tHistory);
		if ( iRet != XUI_OK ) {
			__xuiTextEditHistoryStateFree(&tHistory);
			return iRet;
		}
	} else {
		__xuiTextEditHistoryClear(pData->arrUndo, &pData->iUndoCount);
		__xuiTextEditHistoryClear(pData->arrRedo, &pData->iRedoCount);
	}
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_VALUE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	iRet = __xuiTextEditInvalidateGeometry(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( bNotify ) __xuiTextEditNotifyChange(pWidget, pData);
	return geometryStatus;
}
static int __xuiTextEditSetTextInternal(xui_widget pWidget,xui_text_edit_data_t* pData,const char* sText,int bNotify,int bUndo)
{

    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=1;
    status=__xuiTextEditSetTextInternalImpl(pWidget,pData,sText,bNotify,bUndo);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


static int __xuiTextEditEditSetText(xui_widget pWidget, const char* sText)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTextEditSetTextInternal(pWidget, pData, sText, 1, 0);
}

static int __xuiTextEditDeleteRangeImpl(xui_widget pWidget, xui_text_edit_data_t* pData, int iStart, int iEnd, int bNotify, int bUndo)
{
	xui_text_edit_history_t tHistory;
	int iLen;
	int iMove;
	int iPatchLine;
	int bPatch;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->bPreferredCaretX = 0;
	iLen = pData->iTextLength;
	iStart = __xuiTextEditUtf8Clamp(pData->sText, iLen, iStart);
	iEnd = __xuiTextEditUtf8Clamp(pData->sText, iLen, iEnd);
	if ( iStart > iEnd ) {
		int iTmp = iStart;
		iStart = iEnd;
		iEnd = iTmp;
	}
	if ( iStart == iEnd ) {
		return XUI_OK;
	}
	memset(&tHistory, 0, sizeof(tHistory));
	bPatch = __xuiTextEditDirectPatchPrepare(pData, iStart, iEnd, NULL, 0, &iPatchLine);
	if ( bUndo ) {
		iRet = __xuiTextEditHistoryBegin(pData, &tHistory, iStart, iEnd, NULL, 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
    iRet=__xuiTextEditContentPrepare(pData,bPatch ? iPatchLine:-1,iStart,iEnd,iStart);
    if(iRet!=XUI_OK){__xuiTextEditHistoryStateFree(&tHistory);return iRet;}
	pData->bUndoInsertGroup = 0;
	pData->iUndoInsertEnd = 0;
	iMove = iLen - iEnd + 1;
	memmove(pData->sText + iStart, pData->sText + iEnd, (size_t)iMove);
	pData->iTextLength = iLen - (iEnd - iStart);
	pData->iCursor = iStart;
	(void)__xuiTextEditClearSelectionData(pData);
	if ( !bPatch || __xuiTextEditDirectPatchApply(pWidget, pData, iPatchLine,
		-(iEnd - iStart), iStart, iEnd, iStart) != XUI_OK ) {
		bPatch = 0;
		__xuiTextEditMarkLinesDirty(pData);
	}
	(void)__xuiTextEditEnsureCursorVisible(pWidget, pData);
	if ( bUndo ) {
		__xuiTextEditHistoryFinish(pData, &tHistory);
		iRet = __xuiTextEditHistoryCommit(pData, &tHistory);
		if ( iRet != XUI_OK ) {
			__xuiTextEditHistoryStateFree(&tHistory);
			return iRet;
		}
	}
	iRet = bPatch ? __xuiTextEditInvalidatePaint(pWidget) : __xuiTextEditInvalidateGeometry(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( bNotify ) __xuiTextEditNotifyChange(pWidget, pData);
	return XUI_OK;
}
static int __xuiTextEditDeleteRange(xui_widget pWidget,xui_text_edit_data_t* pData,int iStart,int iEnd,int bNotify,int bUndo)
{

    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=1;
    status=__xuiTextEditDeleteRangeImpl(pWidget,pData,iStart,iEnd,bNotify,bUndo);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


static int __xuiTextEditInsertTextImpl(xui_widget pWidget, xui_text_edit_data_t* pData, const char* sInsert, int iInsertSize)
{
	xui_text_edit_history_t tHistory;
	xui_text_edit_history_t* pGroupedHistory;
	int iStart;
	int iEnd;
	int iLen;
	int iSelected;
	int iAllowed;
	int iNewLen;
	int iPatchLine;
	int bPatch;
	int bGroupable;
	int bReuseUndo;
	int iPatchRet;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (sInsert == NULL) || pData->bReadonly ) {
		return XUI_OK;
	}
	pData->bPreferredCaretX = 0;
	if ( iInsertSize < 0 ) {
		iRet = __xuiTextEditStringSize(sInsert, &iInsertSize);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( iInsertSize <= 0 ) {
		return XUI_OK;
	}
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	iLen = pData->iTextLength;
	iSelected = iEnd - iStart;
	if ( pData->iMaxLength > 0 ) {
		iAllowed = pData->iMaxLength - (iLen - iSelected);
		if ( iAllowed <= 0 ) {
			return XUI_OK;
		}
		if ( iInsertSize > iAllowed ) {
			iInsertSize = __xuiTextEditUtf8ClampBytes(sInsert, iAllowed);
		}
		if ( iInsertSize <= 0 ) {
			return XUI_OK;
		}
	}
	if ( iInsertSize > INT_MAX - 1 - (iLen - iSelected) )
		return XUI_ERROR_BUFFER_TOO_SMALL;
	bPatch = __xuiTextEditDirectPatchPrepare(pData, iStart, iEnd,
		sInsert, iInsertSize, &iPatchLine);
	bGroupable = iSelected == 0 && memchr(sInsert, '\n', (size_t)iInsertSize) == NULL &&
		memchr(sInsert, '\r', (size_t)iInsertSize) == NULL &&
		xuiInternalTextGraphemeNext(sInsert, iInsertSize, 0) == iInsertSize;
	bReuseUndo = bGroupable && pData->bUndoInsertGroup && pData->iUndoInsertEnd == iStart;
	if ( bReuseUndo && pData->iUndoCount <= 0 ) bReuseUndo = 0;
	if ( bReuseUndo ) {
		xui_text_edit_history_t* pState = &pData->arrUndo[pData->iUndoCount - 1];
		if ( pState->iRemovedLength != 0 || pState->iStart < 0 ||
		     pState->iInsertedLength < 0 ||
		     (size_t)pState->iStart + (size_t)pState->iInsertedLength !=
			(size_t)pData->iUndoInsertEnd ||
		     (size_t)pState->iInsertedLength + (size_t)iInsertSize >
			XUI_TEXT_EDIT_HISTORY_BYTES_LIMIT ) bReuseUndo = 0;
	}
	memset(&tHistory, 0, sizeof(tHistory));
	pGroupedHistory = NULL;
	iNewLen = iLen - iSelected + iInsertSize;
	iRet = __xuiTextEditTextReserve(&pData->sText, &pData->iTextCapacity, iNewLen + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
    iRet=__xuiTextEditContentPrepare(pData,bPatch ? iPatchLine:-1,iStart,iEnd,iStart+iInsertSize);
    if(iRet!=XUI_OK)return iRet;
	if ( bReuseUndo ) {
		iRet = __xuiTextEditHistoryCoalesceInsert(pData, sInsert, iInsertSize);
		if ( iRet != XUI_OK ) return iRet;
		pGroupedHistory = &pData->arrUndo[pData->iUndoCount - 1];
	} else {
		iRet = __xuiTextEditHistoryBegin(pData, &tHistory, iStart, iEnd,
			sInsert, iInsertSize);
		if ( iRet != XUI_OK ) return iRet;
	}
	memmove(pData->sText + iStart + iInsertSize, pData->sText + iEnd, (size_t)(iLen - iEnd + 1));
	memcpy(pData->sText + iStart, sInsert, (size_t)iInsertSize);
	pData->iTextLength = iNewLen;
	pData->iCursor = iStart + iInsertSize;
	(void)__xuiTextEditClearSelectionData(pData);
	pData->bUndoInsertGroup = bGroupable;
	pData->iUndoInsertEnd = bGroupable ? pData->iCursor : 0;
	iPatchRet = bPatch ? __xuiTextEditDirectPatchApply(pWidget, pData, iPatchLine,
		iInsertSize - iSelected, iStart, iEnd, iStart + iInsertSize) : XUI_ERROR_UNSUPPORTED;
	if ( !bPatch || iPatchRet != XUI_OK ) {
		bPatch = 0;
		__xuiTextEditMarkLinesDirty(pData);
	}
	(void)__xuiTextEditEnsureCursorVisible(pWidget, pData);
	if ( pGroupedHistory != NULL ) {
		__xuiTextEditHistoryFinish(pData, pGroupedHistory);
	} else {
		__xuiTextEditHistoryFinish(pData, &tHistory);
		iRet = __xuiTextEditHistoryCommit(pData, &tHistory);
		if ( iRet != XUI_OK ) {
			__xuiTextEditHistoryStateFree(&tHistory);
			return iRet;
		}
	}
	iRet = bPatch ? __xuiTextEditInvalidatePaint(pWidget) : __xuiTextEditInvalidateGeometry(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTextEditNotifyChange(pWidget, pData);
	return XUI_OK;
}
static int __xuiTextEditInsertText(xui_widget pWidget,xui_text_edit_data_t* pData,const char* sInsert,int iInsertSize)
{

    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=1;
    status=__xuiTextEditInsertTextImpl(pWidget,pData,sInsert,iInsertSize);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


static int __xuiTextEditMoveSelectionToCursorImpl(xui_widget pWidget, xui_text_edit_data_t* pData, int iTarget)
{
	xui_text_edit_history_t tHistory;
	char* sHistoryInserted;
	int iHistoryStart;
	int iHistoryEnd;
	int iHistorySize;
	int iStart;
	int iEnd;
	int iLen;
	int iSelected;
	int iMove;
	int iNewStart;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->bReadonly ) {
		return XUI_OK;
	}
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	if ( iEnd <= iStart ) {
		return XUI_OK;
	}
	iLen = pData->iTextLength;
	iTarget = __xuiTextEditUtf8Clamp(pData->sText, iLen, iTarget);
	if ( (iTarget >= iStart) && (iTarget <= iEnd) ) {
		return XUI_OK;
	}
	iSelected = iEnd - iStart;
	iRet = __xuiTextEditTextReserve(&pData->sScratch, &pData->iScratchCapacity, iSelected + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(pData->sScratch, pData->sText + iStart, (size_t)iSelected);
	pData->sScratch[iSelected] = '\0';
	if ( iTarget < iStart ) {
		iHistoryStart = iTarget;
		iHistoryEnd = iEnd;
	} else {
		iHistoryStart = iStart;
		iHistoryEnd = iTarget;
	}
	iHistorySize = iHistoryEnd - iHistoryStart;
	sHistoryInserted = (char*)xrtMalloc((size_t)iHistorySize);
	if ( sHistoryInserted == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iTarget < iStart ) {
		memcpy(sHistoryInserted, pData->sText + iStart, (size_t)iSelected);
		memcpy(sHistoryInserted + iSelected, pData->sText + iTarget,
			(size_t)(iStart - iTarget));
	} else {
		memcpy(sHistoryInserted, pData->sText + iEnd, (size_t)(iTarget - iEnd));
		memcpy(sHistoryInserted + (iTarget - iEnd), pData->sText + iStart,
			(size_t)iSelected);
	}
	iRet = __xuiTextEditHistoryBegin(pData, &tHistory, iHistoryStart,
		iHistoryEnd, sHistoryInserted, iHistorySize);
	xrtFree(sHistoryInserted);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
    iRet=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(iRet!=XUI_OK){__xuiTextEditHistoryStateFree(&tHistory);return iRet;}
	iMove = iLen - iEnd + 1;
	memmove(pData->sText + iStart, pData->sText + iEnd, (size_t)iMove);
	iNewStart = (iTarget > iEnd) ? (iTarget - iSelected) : iTarget;
	memmove(pData->sText + iNewStart + iSelected, pData->sText + iNewStart, (size_t)(iLen - iSelected - iNewStart + 1));
	memcpy(pData->sText + iNewStart, pData->sScratch, (size_t)iSelected);
	(void)__xuiTextEditSetSelectionData(pData, iNewStart, iNewStart + iSelected);
	__xuiTextEditMarkLinesDirty(pData);
	(void)__xuiTextEditEnsureCursorVisible(pWidget, pData);
	__xuiTextEditHistoryFinish(pData, &tHistory);
	iRet = __xuiTextEditHistoryCommit(pData, &tHistory);
	if ( iRet != XUI_OK ) {
		__xuiTextEditHistoryStateFree(&tHistory);
		return iRet;
	}
	iRet = __xuiTextEditInvalidateGeometry(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTextEditNotifyChange(pWidget, pData);
	return XUI_OK;
}
static int __xuiTextEditMoveSelectionToCursor(xui_widget pWidget,xui_text_edit_data_t* pData,int iTarget)
{

    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=1;
    status=__xuiTextEditMoveSelectionToCursorImpl(pWidget,pData,iTarget);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


static int __xuiTextEditCopySelection(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_proxy pProxy;
	int iStart;
	int iEnd;
	int iLen;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTextEditHasSelectionData(pData) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->clipboardSetText == NULL) ) {
		return XUI_OK;
	}
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	iLen = iEnd - iStart;
	iRet = __xuiTextEditTextReserve(&pData->sScratch, &pData->iScratchCapacity, iLen + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(pData->sScratch, pData->sText + iStart, (size_t)iLen);
	pData->sScratch[iLen] = '\0';
	return pProxy->clipboardSetText(pProxy, pData->sScratch);
}

static int __xuiTextEditNormalizeLineBreaks(xui_text_edit_data_t* pData, const char* sText, int iSize, char** psText, int* pSize)
{
	int i;
	int iOut;
	int iRet;

	if ( (pData == NULL) || (sText == NULL) || (psText == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iSize < 0 ) {
		iSize = (int)strlen(sText);
	}
	iRet = __xuiTextEditTextReserve(&pData->sScratch, &pData->iScratchCapacity, iSize + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iOut = 0;
	for ( i = 0; i < iSize; i++ ) {
		if ( sText[i] == '\r' ) {
			if ( (i + 1 < iSize) && (sText[i + 1] == '\n') ) {
				continue;
			}
			pData->sScratch[iOut++] = '\n';
		} else {
			pData->sScratch[iOut++] = sText[i];
		}
	}
	pData->sScratch[iOut] = '\0';
	*psText = pData->sScratch;
	*pSize = iOut;
	return XUI_OK;
}

static int __xuiTextEditPasteClipboard(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	char* sClipboard;
	char* sText;
	int iLen;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || pData->bReadonly ) {
		return XUI_OK;
	}
	sClipboard = NULL;
	iRet = xuiInternalClipboardReadText(xuiWidgetGetContext(pWidget), &sClipboard, &iLen);
	if ( iRet == XUI_ERROR_UNSUPPORTED ) return XUI_OK;
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditNormalizeLineBreaks(pData, sClipboard, -1, &sText, &iLen);
	if ( iRet != XUI_OK ) {
		xrtFree(sClipboard);
		return iRet;
	}
	iRet = __xuiTextEditInsertText(pWidget, pData, sText, iLen);
	xrtFree(sClipboard);
	return iRet;
}

static int __xuiTextEditMoveCursor(xui_widget pWidget, xui_text_edit_data_t* pData, int iNewCursor, int bExtend)
{
	int iLen;
	int iAnchor;
	int iOldStart;
	int iOldEnd;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = pData->iTextLength;
	iOldStart = pData->iSelectStart;
	iOldEnd = pData->iSelectEnd;
	iNewCursor = __xuiTextEditUtf8Clamp(pData->sText, iLen, iNewCursor);
	if ( iNewCursor != pData->iCursor || bExtend || __xuiTextEditHasSelectionData(pData) ) {
		pData->bUndoInsertGroup = 0;
		pData->iUndoInsertEnd = 0;
	}
	if ( bExtend ) {
		iAnchor = __xuiTextEditHasSelectionData(pData) ? pData->iAnchor : pData->iCursor;
		pData->iAnchor = iAnchor;
		pData->iCursor = iNewCursor;
		pData->iSelectStart = iAnchor;
		pData->iSelectEnd = iNewCursor;
	} else {
		pData->iCursor = iNewCursor;
		(void)__xuiTextEditClearSelectionData(pData);
	}
	iRet = __xuiTextEditSyncCursor(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iSelectStart != iOldStart || pData->iSelectEnd != iOldEnd ) {
		__xuiTextEditResetFindRange(pData);
		(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_SELECTION_CHANGED,
			pData->sText, pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
	}
	return XUI_OK;
}

static int __xuiTextEditSelectWordAt(xui_widget pWidget, xui_text_edit_data_t* pData, int iCursor)
{
	const char* sText;
	int iLen;
	int iStart;
	int iEnd;
	xui_internal_word_kind_t iKind;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sText = pData->sText;
	iLen = pData->iTextLength;
	if ( iLen <= 0 ) {
		return __xuiTextEditMoveCursor(pWidget, pData, 0, 0);
	}
	iKind = xuiInternalTextWordRange(sText, iLen, iCursor,
		XUI_INTERNAL_WORD_NATURAL, &iStart, &iEnd);
	if ( iKind == XUI_INTERNAL_WORD_SPACE ) {
		return __xuiTextEditMoveCursor(pWidget, pData, iCursor, 0);
	}
	(void)__xuiTextEditSetSelectionData(pData, iStart, iEnd);
	return __xuiTextEditSyncCursor(pWidget, pData);
}

static int __xuiTextEditMoveCursorVertical(xui_widget pWidget, xui_text_edit_data_t* pData, int iDelta, int bExtend)
{
	xui_text_edit_data_t tResolved;
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix;
	float fQueryX;
	float fPrev;
	float fNext;
	int iLine;
	int iTarget;
	int iPos;
	int iNext;
	int iCursor;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iLine = __xuiTextEditFindLineForPos(pData, pData->iCursor);
	iTarget = iLine + iDelta;
	if ( iTarget < 0 ) iTarget = 0;
	if ( iTarget >= pData->iLineCount ) iTarget = pData->iLineCount - 1;
	pLine = &pData->pLines[iLine];
	iCursor = pData->iCursor;
	if ( iCursor < pLine->iStart ) iCursor = pLine->iStart;
	if ( iCursor > pLine->iEnd ) iCursor = pLine->iEnd;
	tPrefix = __xuiTextEditMeasureRange(pWidget, pData, tResolved.pFont, pLine->iStart, iCursor);
	if ( !pData->bPreferredCaretX ) {
		pData->fPreferredCaretX = tPrefix.fX;
		pData->bPreferredCaretX = 1;
	}
	fQueryX = pData->fPreferredCaretX;
	pLine = &pData->pLines[iTarget];
	if ( pData->tTextLayout.pLayout != NULL &&
	     xuiInternalTextLayoutLineHitTest(pData->tTextLayout.pLayout, iTarget,
		 fQueryX, 0, &iPos) == XUI_OK ) {
		iRet = __xuiTextEditMoveCursor(pWidget, pData, iPos, bExtend);
		pData->bPreferredCaretX = 1;
		return iRet;
	}
	if ( pData->tTextLayout.pLayout == NULL ) {
		xui_text_edit_paint_cache_t* pEntry = __xuiTextEditLineShapeEnsure(
			pWidget, pData, tResolved.pFont, iTarget);
		if ( __xuiTextEditLineShapeHitTest(pEntry, fQueryX, 0, &iPos) == XUI_OK ) {
			iRet = __xuiTextEditMoveCursor(pWidget, pData, pLine->iStart + iPos, bExtend);
			pData->bPreferredCaretX = 1;
			return iRet;
		}
	}
	iPos = pLine->iStart;
	fPrev = 0.0f;
	while ( iPos < pLine->iEnd ) {
		iNext = __xuiTextEditNextVisualCaret(pData, pLine->iEnd, iPos);
		tPrefix = __xuiTextEditMeasureRange(pWidget, pData, tResolved.pFont, pLine->iStart, iNext);
		fNext = tPrefix.fX;
		if ( fQueryX < (fPrev + fNext) * 0.5f ) {
			iRet = __xuiTextEditMoveCursor(pWidget, pData, iPos, bExtend);
			pData->bPreferredCaretX = 1;
			return iRet;
		}
		iPos = iNext;
		fPrev = fNext;
	}
	iRet = __xuiTextEditMoveCursor(pWidget, pData, pLine->iEnd, bExtend);
	pData->bPreferredCaretX = 1;
	return iRet;
}

static int __xuiTextEditPageLineCount(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tContent;
	int iLines;

	if ( (pWidget == NULL) || (pData == NULL) ||
	     (__xuiTextEditUpdateScrollModel(pWidget, pData) != XUI_OK) ) {
		return 1;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	if ( pData->fLineHeight <= 0.0f || tContent.fH <= 0.0f ) {
		return 1;
	}
	iLines = (int)(tContent.fH / pData->fLineHeight) - 1;
	return (iLines > 0) ? iLines : 1;
}

static int __xuiTextEditLineStartForCursor(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	int iRet;
	int iLine;

	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return pData->iCursor;
	}
	iLine = __xuiTextEditFindLineForPos(pData, pData->iCursor);
	return pData->pLines[iLine].iStart;
}

static int __xuiTextEditLineEndForCursor(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	int iRet;
	int iLine;

	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return pData->iCursor;
	}
	iLine = __xuiTextEditFindLineForPos(pData, pData->iCursor);
	return pData->pLines[iLine].iEnd;
}

static int __xuiTextEditOffsetFromPoint(xui_widget pWidget, xui_text_edit_data_t* pData,
	float fLocalX, float fLocalY, int bGlyph)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tContent;
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix;
	float fQueryX;
	float fLineQuery;
	float fPrev;
	float fNext;
	int iLine;
	int iPos;
	int iNext;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return 0;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return pData->iCursor;
	}
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	if ( pData->iLineCount <= 0 ) {
		return 0;
	}
	fLineQuery = fLocalY - tContent.fY + pData->fScrollY;
	iLine = (int)(fLineQuery / pData->fLineHeight);
	if ( iLine < 0 ) iLine = 0;
	if ( iLine >= pData->iLineCount ) iLine = pData->iLineCount - 1;
	pLine = &pData->pLines[iLine];
	fQueryX = fLocalX - tContent.fX + pData->fScrollX;
	if ( fQueryX <= 0.0f ) {
		return pLine->iStart;
	}
	if ( pData->tTextLayout.pLayout != NULL &&
	     xuiInternalTextLayoutLineHitTest(pData->tTextLayout.pLayout, iLine,
		 fQueryX, bGlyph, &iPos) == XUI_OK ) {
		return iPos;
	}
	if ( pData->tTextLayout.pLayout == NULL ) {
		xui_text_edit_paint_cache_t* pEntry = __xuiTextEditLineShapeEnsure(
			pWidget, pData, tResolved.pFont, iLine);
		if ( __xuiTextEditLineShapeHitTest(pEntry, fQueryX, bGlyph, &iPos) == XUI_OK ) {
			return pLine->iStart + iPos;
		}
	}
	iPos = pLine->iStart;
	fPrev = 0.0f;
	while ( iPos < pLine->iEnd ) {
		iNext = __xuiTextEditNextVisualCaret(pData, pLine->iEnd, iPos);
		tPrefix = __xuiTextEditMeasureRange(pWidget, pData, tResolved.pFont, pLine->iStart, iNext);
		fNext = tPrefix.fX;
		if ( fQueryX < (bGlyph ? fNext : (fPrev + fNext) * 0.5f) ) {
			return iPos;
		}
		iPos = iNext;
		fPrev = fNext;
	}
	return pLine->iEnd;
}

static int __xuiTextEditCursorFromPoint(xui_widget pWidget, xui_text_edit_data_t* pData, float fLocalX, float fLocalY)
{
	return __xuiTextEditOffsetFromPoint(pWidget, pData, fLocalX, fLocalY, 0);
}

static int __xuiTextEditHitSetCursor(xui_widget pWidget, xui_text_edit_data_t* pData, const xui_event_t* pEvent, int bExtend)
{
	xui_rect_t tWorld;
	int iCursor;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	iCursor = __xuiTextEditCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
	return __xuiTextEditMoveCursor(pWidget, pData, iCursor, bExtend);
}

static int __xuiTextEditAutoScrollPointer(xui_widget pWidget, xui_text_edit_data_t* pData, const xui_event_t* pEvent)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fViewportLeft;
	float fViewportTop;
	float fViewportRight;
	float fViewportBottom;
	float fDeltaX;
	float fDeltaY;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fViewportLeft = tWorld.fX + tContent.fX;
	fViewportTop = tWorld.fY + tContent.fY;
	fViewportRight = fViewportLeft + tContent.fW;
	fViewportBottom = fViewportTop + tContent.fH;
	fDeltaX = 0.0f;
	fDeltaY = 0.0f;
	if ( pEvent->fX < fViewportLeft ) {
		fDeltaX = pEvent->fX - fViewportLeft;
	} else if ( pEvent->fX > fViewportRight ) {
		fDeltaX = pEvent->fX - fViewportRight;
	}
	if ( pEvent->fY < fViewportTop ) {
		fDeltaY = pEvent->fY - fViewportTop;
	} else if ( pEvent->fY > fViewportBottom ) {
		fDeltaY = pEvent->fY - fViewportBottom;
	}
	if ( (fDeltaX == 0.0f) && (fDeltaY == 0.0f) ) {
		return XUI_OK;
	}
	return xuiTextEditSetScroll(pWidget, pData->fScrollX + fDeltaX, pData->fScrollY + fDeltaY);
}

static int __xuiTextEditAutoScrollLinePointer(xui_widget pWidget,
	xui_text_edit_data_t* pData, const xui_event_t* pEvent)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fTop;
	float fBottom;
	float fDeltaY;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	fTop = tWorld.fY + tContent.fY;
	fBottom = fTop + tContent.fH;
	fDeltaY = 0.0f;
	if ( pEvent->fY < fTop ) fDeltaY = pEvent->fY - fTop;
	else if ( pEvent->fY > fBottom ) fDeltaY = pEvent->fY - fBottom;
	if ( fDeltaY == 0.0f ) return XUI_OK;
	return xuiTextEditSetScroll(pWidget, pData->fScrollX, pData->fScrollY + fDeltaY);
}

static int __xuiTextEditLineSelectionBoundsAtY(xui_widget pWidget,
	xui_text_edit_data_t* pData, float fWorldY, int* pStart, int* pEnd)
{
	xui_text_edit_data_t tResolved;
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLineY;
	int iLine;
	int iFirst;
	int iAfter;
	int iPhysicalLine;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pStart == NULL || pEnd == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iLineCount <= 0 || pData->fLineHeight <= 0.0f ) return XUI_ERROR_INVALID_STATE;
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	fLineY = fWorldY - tWorld.fY - tContent.fY + pData->fScrollY;
	iLine = (fLineY <= 0.0f) ? 0 : (int)(fLineY / pData->fLineHeight);
	if ( iLine >= pData->iLineCount ) iLine = pData->iLineCount - 1;
	iPhysicalLine = pData->pLines[iLine].iPhysicalLine;
	iFirst = iLine;
	while ( iFirst > 0 && pData->pLines[iFirst - 1].iPhysicalLine == iPhysicalLine ) iFirst--;
	iAfter = iLine + 1;
	while ( iAfter < pData->iLineCount && pData->pLines[iAfter].iPhysicalLine == iPhysicalLine ) iAfter++;
	*pStart = pData->pLines[iFirst].iStart;
	*pEnd = (iAfter < pData->iLineCount) ? pData->pLines[iAfter].iStart : pData->iTextLength;
	return XUI_OK;
}

static int __xuiTextEditApplyLineSelection(xui_widget pWidget,
	xui_text_edit_data_t* pData, int iStart, int iEnd, int bBegin)
{
	int iOldStart;
	int iOldEnd;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bBegin ) {
		pData->iLineSelectAnchorStart = iStart;
		pData->iLineSelectAnchorEnd = iEnd;
	}
	iOldStart = pData->iSelectStart;
	iOldEnd = pData->iSelectEnd;
	if ( iStart < pData->iLineSelectAnchorStart ) {
		iRet = __xuiTextEditSetSelectionData(pData, pData->iLineSelectAnchorEnd, iStart);
	} else {
		iRet = __xuiTextEditSetSelectionData(pData, pData->iLineSelectAnchorStart, iEnd);
	}
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditUpdateCursorRect(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditInvalidatePaint(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iSelectStart != iOldStart || pData->iSelectEnd != iOldEnd ) {
		(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_SELECTION_CHANGED,
			pData->sText, pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
	}
	return XUI_OK;
}

static int __xuiTextEditDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTextEditAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect(tRect);
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiTextEditDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTextEditAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	tRect = xuiInternalStrokeCenterRectInside(tRect, fWidth, NULL);
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, xuiInternalSnapSize(fWidth), iColor) : XUI_OK;
}

static int __xuiTextEditContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_text_edit_data_t* pData;
	xui_text_edit_data_t tResolved;
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;

	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	pSize->fX = 320.0f;
	pSize->fY = 120.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (tResolved.pFont != NULL) &&
	     (pProxy->fontGetMetrics(pProxy, tResolved.pFont, &tMetrics) == XUI_OK) &&
	     (tMetrics.fLineHeight > 0.0f) ) {
		pSize->fY = tMetrics.fLineHeight * 6.0f + tResolved.fLineGap * 5.0f;
	}
	pSize->fX += __xuiTextEditLineNumberWidth(&tResolved);
	if ( pSize->fY < 90.0f ) {
		pSize->fY = 90.0f;
	}
	if ( (tConstraint.fX >= 0.0f) &&
		(tConstraint.fX < XUI_LAYOUT_UNBOUNDED) &&
		(pSize->fX > tConstraint.fX) ) {
		pSize->fX = tConstraint.fX;
	}
	if ( (tConstraint.fY >= 0.0f) &&
		(tConstraint.fY < XUI_LAYOUT_UNBOUNDED) &&
		(pSize->fY > tConstraint.fY) ) {
		pSize->fY = tConstraint.fY;
	}
	return XUI_OK;
}

static int __xuiTextEditLineStartsPhysicalLine(xui_text_edit_data_t* pData, int iLine)
{
	if ( (pData == NULL) || (iLine <= 0) || (iLine >= pData->iLineCount) ) {
		return 1;
	}
	return pData->pLines[iLine].iStart > pData->pLines[iLine - 1].iEnd;
}

static int __xuiTextEditLineNumberForLine(xui_text_edit_data_t* pData, int iLine)
{
	if ( (pData == NULL) || (pData->sText == NULL) || (iLine < 0) || (iLine >= pData->iLineCount) ) {
		return 1;
	}
	return pData->pLines[iLine].iPhysicalLine;
}

static int __xuiTextEditDrawLineNumbers(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved, xui_rect_t tTextContent)
{
	xui_rect_t tGutter;
	xui_rect_t tBorder;
	xui_rect_t tLine;
	xui_text_edit_line_t* pLine;
	char sNumber[32];
	float fY;
	int i;
	int iRet;

	tGutter = __xuiTextEditLineNumberRect(pWidget, pResolved);
	if ( (tGutter.fW <= 0.0f) || (tGutter.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iRet = __xuiTextEditDrawRectFill(pProxy, pDraw, tGutter, pResolved->iLineNumberBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	tBorder = (xui_rect_t){tGutter.fX + tGutter.fW - 1.0f, tGutter.fY, 1.0f, tGutter.fH};
	iRet = __xuiTextEditDrawRectFill(pProxy, pDraw, tBorder, pResolved->iLineNumberBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pResolved->pFont == NULL) || (pProxy->drawText == NULL) || (__xuiTextEditAlpha(pResolved->iLineNumberColor) == 0) ) {
		return XUI_OK;
	}
	for ( i = 0; i < pData->iLineCount; i++ ) {
		pLine = &pData->pLines[i];
		fY = tTextContent.fY + pLine->fY - pData->fScrollY;
		if ( fY + pData->fLineHeight < tTextContent.fY ) {
			continue;
		}
		if ( fY > tTextContent.fY + tTextContent.fH ) {
			break;
		}
		if ( !__xuiTextEditLineStartsPhysicalLine(pData, i) ) {
			continue;
		}
		snprintf(sNumber, sizeof(sNumber), "%d", __xuiTextEditLineNumberForLine(pData, i));
		tLine = tGutter;
		tLine.fY = fY;
		tLine.fH = pData->fLineHeight;
		tLine.fX += 2.0f;
		tLine.fW -= 8.0f;
		if ( tLine.fW < 0.0f ) {
			tLine.fW = 0.0f;
		}
		iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pResolved->pFont, .sText=sNumber, .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tLine, pResolved->iLineNumberColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTextEditDrawSelectionLine(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved, xui_rect_t tContent, int iLine)
{
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix0;
	xui_vec2_t tPrefix1;
	xui_rect_t tSel;
	int iStart;
	int iEnd;
	int iSelStart;
	int iSelEnd;
	float fX0;
	float fX1;

	if ( !__xuiTextEditHasSelectionData(pData) ) {
		return XUI_OK;
	}
	if ( (iLine < 0) || (iLine >= pData->iLineCount) ) {
		return XUI_OK;
	}
	pLine = &pData->pLines[iLine];
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	iSelStart = (iStart > pLine->iStart) ? iStart : pLine->iStart;
	iSelEnd = (iEnd < pLine->iEnd) ? iEnd : pLine->iEnd;
	if ( (iSelEnd <= iSelStart) && !((iStart <= pLine->iEnd) && (iEnd > pLine->iEnd)) ) {
		return XUI_OK;
	}
	tPrefix0 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iSelStart);
	tPrefix1 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iSelEnd);
	fX0 = tContent.fX + tPrefix0.fX - pData->fScrollX;
	fX1 = tContent.fX + tPrefix1.fX - pData->fScrollX;
	if ( (iSelEnd == iSelStart) && (iEnd > pLine->iEnd) ) {
		fX1 = fX0 + 4.0f;
	}
	if ( fX0 < tContent.fX ) fX0 = tContent.fX;
	if ( fX1 > tContent.fX + tContent.fW ) fX1 = tContent.fX + tContent.fW;
	if ( fX1 <= fX0 ) {
		return XUI_OK;
	}
	tSel.fX = fX0;
	tSel.fY = tContent.fY + pLine->fY - pData->fScrollY + 1.0f;
	tSel.fW = fX1 - fX0;
	tSel.fH = pData->fLineHeight - 2.0f;
	if ( tSel.fY < tContent.fY ) {
		tSel.fH -= (tContent.fY - tSel.fY);
		tSel.fY = tContent.fY;
	}
	if ( tSel.fY + tSel.fH > tContent.fY + tContent.fH ) {
		tSel.fH = tContent.fY + tContent.fH - tSel.fY;
	}
	if ( tSel.fH <= 0.0f ) {
		return XUI_OK;
	}
	return __xuiTextEditDrawRectFill(pProxy, pDraw, tSel, pResolved->iSelectionColor);
}

static int __xuiTextEditDrawFindRangeLine(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved, xui_rect_t tContent, int iLine, const xui_find_result_t* pResult, uint32_t iColor)
{
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix0;
	xui_vec2_t tPrefix1;
	xui_rect_t tHit;
	int iStart;
	int iEnd;
	int iHitStart;
	int iHitEnd;
	float fX0;
	float fX1;

	if ( pResult == NULL || __xuiTextEditAlpha(iColor) == 0 ) return XUI_OK;
	if ( (iLine < 0) || (iLine >= pData->iLineCount) ) return XUI_OK;
	pLine = &pData->pLines[iLine];
	iStart = pResult->iStart;
	iEnd = pResult->iEnd;
	if ( iEnd < iStart ) {
		int iSwap = iStart;
		iStart = iEnd;
		iEnd = iSwap;
	}
	iHitStart = (iStart > pLine->iStart) ? iStart : pLine->iStart;
	iHitEnd = (iEnd < pLine->iEnd) ? iEnd : pLine->iEnd;
	if ( iEnd == iStart ) {
		if ( iStart < pLine->iStart || iStart > pLine->iEnd ) return XUI_OK;
		iHitStart = iStart;
		iHitEnd = iStart;
	} else if ( (iHitEnd <= iHitStart) && !((iStart <= pLine->iEnd) && (iEnd > pLine->iEnd)) ) {
		return XUI_OK;
	}
	tPrefix0 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iHitStart);
	tPrefix1 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iHitEnd);
	fX0 = tContent.fX + tPrefix0.fX - pData->fScrollX;
	fX1 = tContent.fX + tPrefix1.fX - pData->fScrollX;
	if ( fX1 <= fX0 ) fX1 = fX0 + 3.0f;
	if ( fX0 < tContent.fX ) fX0 = tContent.fX;
	if ( fX1 > tContent.fX + tContent.fW ) fX1 = tContent.fX + tContent.fW;
	if ( fX1 <= fX0 ) return XUI_OK;
	tHit.fX = fX0;
	tHit.fY = tContent.fY + pLine->fY - pData->fScrollY + 1.0f;
	tHit.fW = fX1 - fX0;
	tHit.fH = pData->fLineHeight - 2.0f;
	if ( tHit.fY < tContent.fY ) {
		tHit.fH -= (tContent.fY - tHit.fY);
		tHit.fY = tContent.fY;
	}
	if ( tHit.fY + tHit.fH > tContent.fY + tContent.fH ) {
		tHit.fH = tContent.fY + tContent.fH - tHit.fY;
	}
	if ( tHit.fH <= 0.0f ) return XUI_OK;
	return __xuiTextEditDrawRectFill(pProxy, pDraw, tHit, iColor);
}

static int __xuiTextEditDrawFindLine(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved, xui_rect_t tContent, int iLine)
{
	xui_text_edit_line_t* pLine;
	int i;
	int iLo;
	int iHi;
	int iRet;

	if ( pData == NULL || pData->iFindResultCount <= 0 ) return XUI_OK;
	if ( iLine < 0 || iLine >= pData->iLineCount ) return XUI_OK;
	pLine = &pData->pLines[iLine];
	iLo = 0;
	iHi = pData->iFindResultCount;
	while ( iLo < iHi ) {
		int iMid = iLo + (iHi - iLo) / 2;
		if ( pData->pFindResults[iMid].iEnd <= pLine->iStart ) iLo = iMid + 1;
		else iHi = iMid;
	}
	for ( i = iLo; i < pData->iFindResultCount; i++ ) {
		if ( pData->pFindResults[i].iStart > pLine->iEnd ) break;
		if ( i == pData->iFindActiveIndex ) continue;
		iRet = __xuiTextEditDrawFindRangeLine(pWidget, pDraw, pProxy, pData, pResolved, tContent, iLine, &pData->pFindResults[i], pResolved->iFindResultColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->iFindActiveIndex >= 0 && pData->iFindActiveIndex < pData->iFindResultCount ) {
		iRet = __xuiTextEditDrawFindRangeLine(pWidget, pDraw, pProxy, pData, pResolved, tContent, iLine, &pData->pFindResults[pData->iFindActiveIndex], pResolved->iFindActiveColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTextEditDrawImeUnderlineLine(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy,
	xui_text_edit_data_t* pData, xui_text_edit_data_t* pResolved, xui_rect_t tContent, int iLine)
{
	xui_text_edit_line_t* pLine;
	xui_vec2_t tPrefix0;
	xui_vec2_t tPrefix1;
	xui_rect_t tUnderline;
	int iCompositionSize;
	int iStart;
	int iEnd;
	int iHitStart;
	int iHitEnd;
	float fX0;
	float fX1;

	if ( pData == NULL || !pData->bImeActive || iLine < 0 || iLine >= pData->iLineCount ) return XUI_OK;
	iCompositionSize = (pData->sImeText != NULL) ? (int)strlen(pData->sImeText) : 0;
	if ( iCompositionSize <= 0 ) return XUI_OK;
	iStart = pData->iImeAnchorStart;
	iEnd = iStart + iCompositionSize;
	pLine = &pData->pLines[iLine];
	iHitStart = (iStart > pLine->iStart) ? iStart : pLine->iStart;
	iHitEnd = (iEnd < pLine->iEnd) ? iEnd : pLine->iEnd;
	if ( iHitEnd <= iHitStart ) return XUI_OK;
	tPrefix0 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iHitStart);
	tPrefix1 = __xuiTextEditMeasureRange(pWidget, pData, pResolved->pFont, pLine->iStart, iHitEnd);
	fX0 = tContent.fX + tPrefix0.fX - pData->fScrollX;
	fX1 = tContent.fX + tPrefix1.fX - pData->fScrollX;
	if ( fX0 < tContent.fX ) fX0 = tContent.fX;
	if ( fX1 > tContent.fX + tContent.fW ) fX1 = tContent.fX + tContent.fW;
	if ( fX1 <= fX0 ) return XUI_OK;
	tUnderline.fX = fX0;
	tUnderline.fY = tContent.fY + pLine->fY - pData->fScrollY + pData->fLineHeight - 2.0f;
	tUnderline.fW = fX1 - fX0;
	tUnderline.fH = 1.0f;
	if ( tUnderline.fY < tContent.fY || tUnderline.fY >= tContent.fY + tContent.fH ) return XUI_OK;
	return __xuiTextEditDrawRectFill(pProxy, pDraw, tUnderline,
		pData->iImeAttribute == XUI_IME_ATTRIBUTE_INPUT_ERROR ?
		XUI_COLOR_RGBA(220, 38, 38, 255) : pResolved->iCursorColor);
}

static int __xuiTextEditCacheRenderCore(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId,
	void* pUser, xui_text_edit_data_t* pData)
{
	xui_text_edit_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tContent;
	xui_rect_t tLine;
	xui_rect_t tCursor;
	xui_text_edit_line_t* pLine;
	xui_text_edit_paint_cache_t* pPaintEntry;
	const char* sLineText;
	int iDisplaySize;
	uint32_t iState;
	uint32_t iBackground;
	uint32_t iBorder;
	uint32_t iText;
	float fY;
	int i;
	int iLen;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTextEditResolve(pWidget, pData, &tResolved);
	iState = xuiTextEditGetState(pWidget);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iBackground = tResolved.iBackgroundColor;
	iBorder = tResolved.iBorderColor;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iBackground = tResolved.iDisabledBackgroundColor;
		iBorder = __xuiTextEditColorWithAlpha(tResolved.iBorderColor, 120);
		(void)__xuiTextEditStyleColor(pWidget, "textedit.border.disabled_color", "input.border.disabled_color", &iBorder);
	} else if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		iBackground = tResolved.iHoverBackgroundColor;
		iBorder = tResolved.iHoverBorderColor;
	}
	iRet = __xuiTextEditDrawRectFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
	if ( iRet != XUI_OK ) return iRet;

	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	tContent = __xuiTextEditTextContentRect(pWidget, &tResolved);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		return XUI_OK;
	}
	pData->tTextRect = tContent;
	iRet = __xuiTextEditDrawLineNumbers(pWidget, pDraw, pProxy, pData, &tResolved, tContent);
	if ( iRet != XUI_OK ) return iRet;
	iLen = pData->iTextLength;
	if ( (iLen == 0) && !pData->bImeActive && (pData->sPlaceholder != NULL) && (pData->sPlaceholder[0] != '\0') &&
	     (tResolved.pFont != NULL) && (pProxy->drawText != NULL) && (__xuiTextEditAlpha(tResolved.iPlaceholderColor) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=tResolved.pFont, .sText=pData->sPlaceholder, .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tContent, tResolved.iPlaceholderColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( iLen > 0 && (tResolved.pFont != NULL) && (pProxy->drawText != NULL) ) {
		for ( i = 0; i < pData->iLineCount; i++ ) {
			pLine = &pData->pLines[i];
			fY = tContent.fY + pLine->fY - pData->fScrollY;
			if ( fY + pData->fLineHeight < tContent.fY ) {
				continue;
			}
			if ( fY > tContent.fY + tContent.fH ) {
				break;
			}
			if ( !pData->bImeActive ) {
				iRet = __xuiTextEditDrawFindLine(pWidget, pDraw, pProxy, pData, &tResolved, tContent, i);
				if ( iRet != XUI_OK ) return iRet;
			}
			iRet = __xuiTextEditDrawSelectionLine(pWidget, pDraw, pProxy, pData, &tResolved, tContent, i);
			if ( iRet != XUI_OK ) return iRet;
			iRet = __xuiTextEditDrawImeUnderlineLine(pWidget, pDraw, pProxy, pData, &tResolved, tContent, i);
			if ( iRet != XUI_OK ) return iRet;
			if ( pData->tTextLayout.pLayout != NULL ) {
				iRet = xuiInternalTextLayoutGetDisplayLine(pData->tTextLayout.pLayout, i,
					&sLineText, &iDisplaySize);
				if ( iRet != XUI_OK ) return iRet;
			} else {
				sLineText = pData->sText + pLine->iStart;
				iDisplaySize = pLine->iEnd - pLine->iStart;
			}
			tLine = tContent;
			tLine.fY = fY;
			tLine.fH = pData->fLineHeight;
			if ( !pData->bWordWrap ) {
				tLine.fX = tContent.fX - pData->fScrollX;
				tLine.fW = tContent.fW + pData->fScrollX;
				if ( tLine.fX < tContent.fX ) {
					tLine.fW -= (tContent.fX - tLine.fX);
					tLine.fX = tContent.fX;
				}
			}
			iText = ((iState & XUI_WIDGET_STATE_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
			if ( (__xuiTextEditAlpha(iText) != 0) && (tLine.fW > 0.0f) && (tLine.fH > 0.0f) ) {
				pPaintEntry = NULL;
				if ( pProxy->drawTextShapeRange != NULL && iDisplaySize > 0 ) {
					pPaintEntry = __xuiTextEditPaintCacheEnsure(pWidget, pData,
						tResolved.pFont, i, sLineText, iDisplaySize);
				}
				if ( pPaintEntry != NULL && pPaintEntry->bChunked ) {
					iRet = xuiInternalTextChunkCacheDraw(pProxy, pDraw,
						&pPaintEntry->tChunks, tContent,
						tContent.fX - pData->fScrollX, fY,
						pData->fLineHeight, iText,
						XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
					if ( iRet == XUI_ERROR_UNSUPPORTED ) pPaintEntry = NULL;
				} else if ( pPaintEntry != NULL && pPaintEntry->tShape.pPaint != NULL ) {
					iRet = pProxy->drawTextShapeRange(pProxy, pDraw, &pPaintEntry->tShape,
						0, pPaintEntry->iTextSize, tLine, iText,
						XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
					if ( iRet == XUI_ERROR_UNSUPPORTED ) pPaintEntry = NULL;
				} else {
					iRet = XUI_ERROR_UNSUPPORTED;
				}
				if ( pPaintEntry == NULL ) {
					iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=tResolved.pFont, .sText=sLineText, .iTextSize=iDisplaySize, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tLine, iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
				}
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	(void)__xuiTextEditUpdateCursorRect(pWidget, pData);
	tCursor = pData->tCursorRect;
	if ( tCursor.fX < tContent.fX ) tCursor.fX = tContent.fX;
	if ( tCursor.fX > tContent.fX + tContent.fW ) tCursor.fX = tContent.fX + tContent.fW;
	if ( tCursor.fY < tContent.fY ) {
		tCursor.fH -= (tContent.fY - tCursor.fY);
		tCursor.fY = tContent.fY;
	}
	if ( tCursor.fY + tCursor.fH > tContent.fY + tContent.fH ) {
		tCursor.fH = tContent.fY + tContent.fH - tCursor.fY;
	}
	if ( ((iState & XUI_WIDGET_STATE_FOCUS) != 0) &&
	     ((iState & XUI_WIDGET_STATE_DISABLED) == 0) &&
	     !__xuiTextEditHasSelectionData(pData) &&
	    xuiInternalCaretBlinkVisible(pWidget) &&
	     (__xuiTextEditAlpha(tResolved.iCursorColor) != 0) &&
	     (tCursor.fH > 0.0f) &&
	     (pProxy->drawRectFill != NULL) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tCursor), tResolved.iCursorColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTextEditCacheRenderImpl(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_text_edit_data_t* pData;
	char* sDocument;
	xui_text_edit_line_t* pDocumentLines;
	xui_text_edit_layout_cache_t tDocumentLayout;
	int iDocumentCapacity;
	int iDocumentLength;
	int iDocumentLineCount;
	int iDocumentLineCapacity;
	int iDocumentCursor;
	int iDocumentAnchor;
	int iDocumentSelectStart;
	int iDocumentSelectEnd;
	int bDocumentLinesDirty;
	int bDocumentDirectLayout;
	float fDocumentLastLayoutWidth;
	float fDocumentContentWidth;
	float fDocumentContentHeight;
	float fDocumentLineHeight;
	int iRet;

	if ( pWidget == NULL || pDraw == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL || !pData->bImeActive ) {
		return __xuiTextEditCacheRenderCore(pWidget, pDraw, iStateId, pUser, pData);
	}
	iRet = __xuiTextEditImeBuildDisplay(pData);
	if ( iRet != XUI_OK ) return iRet;
	sDocument = pData->sText;
	iDocumentCapacity = pData->iTextCapacity;
	iDocumentLength = pData->iTextLength;
	pDocumentLines = pData->pLines;
	tDocumentLayout = pData->tTextLayout;
	iDocumentLineCount = pData->iLineCount;
	iDocumentLineCapacity = pData->iLineCapacity;
	iDocumentCursor = pData->iCursor;
	iDocumentAnchor = pData->iAnchor;
	iDocumentSelectStart = pData->iSelectStart;
	iDocumentSelectEnd = pData->iSelectEnd;
	bDocumentLinesDirty = pData->bLinesDirty;
	bDocumentDirectLayout = pData->bDirectLayout;
	fDocumentLastLayoutWidth = pData->fLastLayoutWidth;
	fDocumentContentWidth = pData->fContentWidth;
	fDocumentContentHeight = pData->fContentHeight;
	fDocumentLineHeight = pData->fLineHeight;

	pData->sText = pData->sImeDisplay;
	pData->iTextCapacity = pData->iImeDisplayCapacity;
	pData->iTextLength = pData->iImeDisplayLength;
	pData->pLines = pData->pImeLines;
	pData->tTextLayout = pData->tImeTextLayout;
	pData->iLineCount = 0;
	pData->iLineCapacity = pData->iImeLineCapacity;
	pData->iCursor = pData->iImeAnchorStart + pData->iImeCursor;
	pData->iAnchor = pData->iImeAnchorStart + pData->iImeSelectionStart;
	pData->iSelectStart = pData->iAnchor;
	pData->iSelectEnd = pData->iImeAnchorStart + pData->iImeSelectionEnd;
	pData->bLinesDirty = 1;
	pData->fLastLayoutWidth = -1.0f;
	iRet = __xuiTextEditCacheRenderCore(pWidget, pDraw, iStateId, pUser, pData);

	pData->pImeLines = pData->pLines;
	pData->tImeTextLayout = pData->tTextLayout;
	pData->iImeLineCapacity = pData->iLineCapacity;
	pData->sText = sDocument;
	pData->iTextCapacity = iDocumentCapacity;
	pData->iTextLength = iDocumentLength;
	pData->pLines = pDocumentLines;
	pData->tTextLayout = tDocumentLayout;
	pData->iLineCount = iDocumentLineCount;
	pData->iLineCapacity = iDocumentLineCapacity;
	pData->iCursor = iDocumentCursor;
	pData->iAnchor = iDocumentAnchor;
	pData->iSelectStart = iDocumentSelectStart;
	pData->iSelectEnd = iDocumentSelectEnd;
    /* Direct document metadata no longer owns the shared work Layout after
     * composition has reused it. Refresh its Font/source key before reuse. */
	pData->bLinesDirty = bDocumentLinesDirty || bDocumentDirectLayout;
	pData->bDirectLayout = bDocumentDirectLayout;
	pData->fLastLayoutWidth = fDocumentLastLayoutWidth;
	pData->fContentWidth = fDocumentContentWidth;
	pData->fContentHeight = fDocumentContentHeight;
	pData->fLineHeight = fDocumentLineHeight;
	return iRet;
}
static int __xuiTextEditCacheRender(xui_widget pWidget,xui_draw_context pDraw,uint32_t iStateId,void* pUser)
{
    xui_text_edit_data_t* pData=__xuiTextEditGetData(pWidget);
    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=2;
    status=__xuiTextEditCacheRenderImpl(pWidget,pDraw,iStateId,pUser);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


static int __xuiTextEditUpdateMenu(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_menu_item_t arrItems[9];
	uint32_t iEnabled;
	int bHasSelection;
	int bReadonly;
	int bAllSelected;
	int iLen;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(arrItems, 0, sizeof(arrItems));
	bHasSelection = __xuiTextEditHasSelectionData(pData);
	bReadonly = pData->bReadonly;
	iLen = pData->iTextLength;
	bAllSelected = bHasSelection && (pData->iSelectStart == 0 || pData->iSelectEnd == 0) &&
	               (pData->iSelectStart == iLen || pData->iSelectEnd == iLen);
	iEnabled = XUI_MENU_ITEM_ENABLED;
	arrItems[0].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_UNDO);
	arrItems[0].sShortcut = "Ctrl+Z";
	arrItems[0].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[0].iState = (pData->iUndoCount > 0) ? iEnabled : 0u;
	arrItems[0].iValue = XUI_INPUT_MENU_UNDO;
	arrItems[1].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_REDO);
	arrItems[1].sShortcut = "Ctrl+Y";
	arrItems[1].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[1].iState = (pData->iRedoCount > 0) ? iEnabled : 0u;
	arrItems[1].iValue = XUI_INPUT_MENU_REDO;
	arrItems[2].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[3].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_CUT);
	arrItems[3].sShortcut = "Ctrl+X";
	arrItems[3].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[3].iState = (bHasSelection && !bReadonly) ? iEnabled : 0u;
	arrItems[3].iValue = XUI_INPUT_MENU_CUT;
	arrItems[4].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_COPY);
	arrItems[4].sShortcut = "Ctrl+C";
	arrItems[4].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[4].iState = bHasSelection ? iEnabled : 0u;
	arrItems[4].iValue = XUI_INPUT_MENU_COPY;
	arrItems[5].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_PASTE);
	arrItems[5].sShortcut = "Ctrl+V";
	arrItems[5].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[5].iState = !bReadonly ? iEnabled : 0u;
	arrItems[5].iValue = XUI_INPUT_MENU_PASTE;
	arrItems[6].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_DELETE);
	arrItems[6].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[6].iState = (bHasSelection && !bReadonly) ? iEnabled : 0u;
	arrItems[6].iValue = XUI_INPUT_MENU_DELETE;
	arrItems[7].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[8].sText = xuiTextEditGetMenuTitle(pWidget, XUI_INPUT_MENU_SELECT_ALL);
	arrItems[8].sShortcut = "Ctrl+A";
	arrItems[8].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[8].iState = (iLen > 0 && !bAllSelected) ? iEnabled : 0u;
	arrItems[8].iValue = XUI_INPUT_MENU_SELECT_ALL;
	return xuiMenuSetItems(pData->pMenu, arrItems, 9);
}

static void __xuiTextEditMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pTextEdit;

	(void)pMenu;
	(void)iIndex;
	pTextEdit = (xui_widget)pUser;
	if ( pTextEdit == NULL ) {
		return;
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pTextEdit), pTextEdit);
	switch ( iValue ) {
	case XUI_INPUT_MENU_UNDO:
		(void)xuiTextEditUndo(pTextEdit);
		break;
	case XUI_INPUT_MENU_REDO:
		(void)xuiTextEditRedo(pTextEdit);
		break;
	case XUI_INPUT_MENU_CUT:
		(void)xuiTextEditCut(pTextEdit);
		break;
	case XUI_INPUT_MENU_COPY:
		(void)xuiTextEditCopy(pTextEdit);
		break;
	case XUI_INPUT_MENU_PASTE:
		(void)xuiTextEditPaste(pTextEdit);
		break;
	case XUI_INPUT_MENU_DELETE:
		(void)xuiTextEditDeleteSelection(pTextEdit);
		break;
	case XUI_INPUT_MENU_SELECT_ALL:
		(void)xuiTextEditSelectAll(pTextEdit);
		break;
	default:
		break;
	}
}

static int __xuiTextEditHandleKey(xui_widget pWidget, xui_text_edit_data_t* pData, const xui_event_t* pEvent)
{
	const xui_edit_behavior_t* pBehavior;
	xui_context pContext;
	int iLen;
	int iStart;
	int iEnd;
	int iKey;
	int bShift;
	int bCtrl;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	iLen = pData->iTextLength;
	iKey = pEvent->iKey;
	bShift = ((pEvent->iModifiers & XUI_MOD_SHIFT) != 0);
	bCtrl = ((pEvent->iModifiers & XUI_MOD_CTRL) != 0);
	pBehavior = xuiInternalEditBehavior(pWidget);
	if ( iKey != XUI_KEY_UP && iKey != XUI_KEY_DOWN &&
		 iKey != XUI_KEY_PAGE_UP && iKey != XUI_KEY_PAGE_DOWN ) {
		pData->bPreferredCaretX = 0;
	}
	if ( bCtrl ) {
		iKey = toupper((unsigned char)iKey);
		if ( iKey == 'A' ) {
			(void)xuiTextEditSelectAll(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'C' ) {
			(void)xuiTextEditCopy(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'F' ) {
			(void)xuiTextEditOpenFind(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'H' ) {
			(void)xuiTextEditOpenReplace(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'X' ) {
			(void)xuiTextEditCut(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'V' ) {
			(void)xuiTextEditPaste(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'Z' ) {
			(void)(bShift ? xuiTextEditRedo(pWidget) : xuiTextEditUndo(pWidget));
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'Y' ) {
			(void)xuiTextEditRedo(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_TAB:
		if ( pBehavior != NULL && (pBehavior->iTabBehavior == XUI_EDIT_TAB_INSERT ||
		     pBehavior->iTabBehavior == XUI_EDIT_TAB_INDENT) ) {
			if ( !pData->bReadonly ) {
				iRet = __xuiTextEditInsertText(pWidget, pData, "\t", 1);
				if ( iRet != XUI_OK ) return iRet;
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_F3:
		if ( bShift ) {
			(void)xuiTextEditFindPrevious(pWidget, NULL);
		} else {
			(void)xuiTextEditFindNext(pWidget, NULL);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
		if ( __xuiTextEditHasSelectionData(pData) && !bShift ) {
			__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
			return __xuiTextEditMoveCursor(pWidget, pData, iStart, 0) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		(void)__xuiTextEditMoveCursor(pWidget, pData,
			bCtrl ? __xuiTextEditPrevWord(pData) : __xuiTextEditUtf8Prev(pData->sText, iLen, pData->iCursor), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
		if ( __xuiTextEditHasSelectionData(pData) && !bShift ) {
			__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
			return __xuiTextEditMoveCursor(pWidget, pData, iEnd, 0) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		(void)__xuiTextEditMoveCursor(pWidget, pData,
			bCtrl ? __xuiTextEditNextWord(pData) : __xuiTextEditUtf8Next(pData->sText, iLen, pData->iCursor), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP:
		(void)__xuiTextEditMoveCursorVertical(pWidget, pData, -1, bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
		(void)__xuiTextEditMoveCursorVertical(pWidget, pData, 1, bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_UP:
		(void)__xuiTextEditMoveCursorVertical(pWidget, pData, -__xuiTextEditPageLineCount(pWidget, pData), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN:
		(void)__xuiTextEditMoveCursorVertical(pWidget, pData, __xuiTextEditPageLineCount(pWidget, pData), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		if ( bCtrl ) {
			(void)__xuiTextEditMoveCursor(pWidget, pData, 0, bShift);
		} else {
			(void)__xuiTextEditMoveCursor(pWidget, pData, __xuiTextEditLineStartForCursor(pWidget, pData), bShift);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		if ( bCtrl ) {
			(void)__xuiTextEditMoveCursor(pWidget, pData, iLen, bShift);
		} else {
			(void)__xuiTextEditMoveCursor(pWidget, pData, __xuiTextEditLineEndForCursor(pWidget, pData), bShift);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ENTER:
		if ( pBehavior != NULL && pBehavior->iEnterBehavior == XUI_EDIT_ENTER_COMMIT ) {
			(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_COMMIT, pData->sText,
				pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( !pData->bReadonly ) {
			iRet = __xuiTextEditInsertText(pWidget, pData, "\n", 1);
			if ( iRet != XUI_OK ) return iRet;
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ESCAPE:
		if ( pBehavior != NULL && pBehavior->iEscapeBehavior != XUI_EDIT_ESCAPE_DEFAULT ) {
			if ( pBehavior->iEscapeBehavior == XUI_EDIT_ESCAPE_REVERT &&
			     pData->sFocusText != NULL && !pData->bReadonly ) {
				iRet = __xuiTextEditSetTextInternal(pWidget, pData, pData->sFocusText, 1, 0);
				if ( iRet != XUI_OK ) return iRet;
				if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
					return XUI_EVENT_DISPATCH_STOP;
				}
			}
			(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_CANCEL, pData->sText,
				pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_BACKSPACE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		if ( __xuiTextEditHasSelectionData(pData) ) {
			(void)xuiTextEditDeleteSelection(pWidget);
		} else if ( pData->iCursor > 0 ) {
			(void)__xuiTextEditDeleteRange(pWidget, pData,
				bCtrl ? __xuiTextEditPrevWord(pData) : __xuiTextEditUtf8Prev(pData->sText, iLen, pData->iCursor),
				pData->iCursor, 1, 1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DELETE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		if ( __xuiTextEditHasSelectionData(pData) ) {
			(void)xuiTextEditDeleteSelection(pWidget);
		} else if ( pData->iCursor < iLen ) {
			(void)__xuiTextEditDeleteRange(pWidget, pData, pData->iCursor,
				bCtrl ? __xuiTextEditNextWord(pData) : __xuiTextEditUtf8Next(pData->sText, iLen, pData->iCursor), 1, 1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiTextEditCancelPointer(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bDragging = 0;
	pData->bLineSelecting = 0;
	pData->bPressPending = 0;
	pData->bPressInsideSelection = 0;
	pData->bMovingSelection = 0;
	return xuiInternalPointerOwnerCancel(xuiWidgetGetContext(pWidget), pWidget,
		&pData->tPointerOwner);
}

static int __xuiTextEditEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_text_edit_data_t* pData;
	xui_context pContext;
	char sText[8];
	int iTextSize;
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iCursor;
	int iStart;
	int iEnd;
	int iRet;
	int bInsideSelection;
	float fDX;
	float fDY;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pEvent->pTarget == pData->pHScrollBar) || (pEvent->pTarget == pData->pVScrollBar) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( pData->tPointerOwner.bActive &&
	     (pEvent->iType == XUI_EVENT_POINTER_ENTER ||
	      pEvent->iType == XUI_EVENT_POINTER_LEAVE ||
	      pEvent->iType == XUI_EVENT_POINTER_DOWN ||
	      pEvent->iType == XUI_EVENT_POINTER_MOVE ||
	      pEvent->iType == XUI_EVENT_POINTER_UP ||
	      pEvent->iType == XUI_EVENT_POINTER_DOUBLE_CLICK ||
	      (pEvent->iType == XUI_EVENT_CONTEXT_MENU && pEvent->iKey != XUI_KEY_CONTEXT_MENU)) &&
	     !xuiInternalPointerOwnerMatches(&pData->tPointerOwner, pEvent) ) {
		return XUI_OK;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
		return __xuiTextEditInvalidatePaint(pWidget);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( !xuiWidgetGetEffectiveEnabled(pWidget) || !xuiWidgetGetEffectiveVisible(pWidget) ) {
			iRet = __xuiTextEditCancelPointer(pWidget, pData);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) return XUI_OK;
		}
		if ( (!xuiWidgetGetEffectiveEnabled(pWidget) || !xuiWidgetGetEffectiveVisible(pWidget)) && pData->bImeActive )
			__xuiTextEditImeReset(pData);
		return __xuiTextEditInvalidatePaint(pWidget);
	case XUI_EVENT_FOCUS:
		{
			const xui_edit_behavior_t* pBehavior = xuiInternalEditBehavior(pWidget);
			iRet = __xuiTextEditStringSet(&pData->sFocusText, &pData->iFocusTextCapacity, pData->sText);
			if ( iRet != XUI_OK ) return iRet;
			if ( pBehavior != NULL && pBehavior->bSelectAllOnFocus ) {
				return xuiTextEditSelectAll(pWidget);
			}
		}
		return __xuiTextEditInvalidatePaint(pWidget);
	case XUI_EVENT_BLUR:
		{
			const xui_edit_behavior_t* pBehavior = xuiInternalEditBehavior(pWidget);
			int bCommit = pBehavior != NULL && pBehavior->bCommitOnBlur;
			iRet = __xuiTextEditCancelPointer(pWidget, pData);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) return XUI_OK;
			if ( pData->bImeActive ) __xuiTextEditImeReset(pData);
			iRet = __xuiTextEditInvalidatePaint(pWidget);
			if ( iRet != XUI_OK ) return iRet;
			if ( bCommit ) (void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_COMMIT,
				pData->sText, pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
			return XUI_OK;
		}
	case XUI_EVENT_BOUNDS_CHANGED:
		__xuiTextEditMarkLinesDirty(pData);
		return __xuiTextEditInvalidateText(pWidget);
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			if ( pData->tPointerOwner.bActive ) return XUI_EVENT_DISPATCH_STOP;
			pData->bPreferredCaretX = 0;
			iRet = xuiSetFocusWidget(pContext, pWidget);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
			iRet = xuiInternalPointerOwnerBegin(pContext, pWidget, pEvent,
				&pData->tPointerOwner);
			if ( iRet != XUI_OK ) return iRet;
			pData->bDragging = 0;
			pData->bLineSelecting = 0;
			pData->bPressPending = 0;
			pData->bPressInsideSelection = 0;
			pData->bMovingSelection = 0;
			if ( __xuiTextEditLineNumberHit(pWidget, pData, pEvent->fX, pEvent->fY) ) {
				if ( __xuiTextEditLineSelectionBoundsAtY(pWidget, pData, pEvent->fY, &iStart, &iEnd) == XUI_OK ) {
					pData->bLineSelecting = 1;
					iRet = __xuiTextEditApplyLineSelection(pWidget, pData, iStart, iEnd, 1);
					if ( iRet != XUI_OK ) {
						(void)__xuiTextEditCancelPointer(pWidget, pData);
						return iRet;
					}
				} else {
					iRet = __xuiTextEditCancelPointer(pWidget, pData);
					if ( iRet != XUI_OK ) return iRet;
				}
				return XUI_EVENT_DISPATCH_STOP;
			}
			tWorld = xuiWidgetGetWorldRect(pWidget);
			iCursor = __xuiTextEditCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
			bInsideSelection = (iStart != iEnd) && (iCursor >= iStart) && (iCursor <= iEnd);
			if ( ((pEvent->iModifiers & XUI_MOD_SHIFT) == 0) && bInsideSelection && !pData->bReadonly ) {
				pData->bPressPending = 1;
				pData->bPressInsideSelection = 1;
				pData->iPressCursor = iCursor;
				pData->fPressX = pEvent->fX;
				pData->fPressY = pEvent->fY;
			} else {
				pData->bDragging = 1;
				iRet = __xuiTextEditHitSetCursor(pWidget, pData, pEvent,
					((pEvent->iModifiers & XUI_MOD_SHIFT) != 0));
				if ( iRet != XUI_OK ) {
					(void)__xuiTextEditCancelPointer(pWidget, pData);
					return iRet;
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		if ( pData->tPointerOwner.bActive &&
		     (pEvent->iButtons & XUI_POINTER_BUTTON_LEFT) == 0 ) {
			iRet = __xuiTextEditCancelPointer(pWidget, pData);
			if ( iRet != XUI_OK ) return iRet;
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bLineSelecting ) {
			iRet = __xuiTextEditAutoScrollLinePointer(pWidget, pData, pEvent);
			if ( iRet != XUI_OK ) return iRet;
			if ( __xuiTextEditLineSelectionBoundsAtY(pWidget, pData, pEvent->fY, &iStart, &iEnd) == XUI_OK ) {
				iRet = __xuiTextEditApplyLineSelection(pWidget, pData, iStart, iEnd, 0);
				if ( iRet != XUI_OK ) return iRet;
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bPressPending ) {
			fDX = pEvent->fX - pData->fPressX;
			fDY = pEvent->fY - pData->fPressY;
			if ( (fDX * fDX + fDY * fDY) <= 36.0f ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
			pData->bPressPending = 0;
			if ( pData->bPressInsideSelection ) {
				pData->bMovingSelection = 1;
				return XUI_EVENT_DISPATCH_STOP;
			}
			pData->bDragging = 1;
			iRet = __xuiTextEditHitSetCursor(pWidget, pData, pEvent, 1);
			if ( iRet != XUI_OK ) return iRet;
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bMovingSelection ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bDragging ) {
			iRet = __xuiTextEditAutoScrollPointer(pWidget, pData, pEvent);
			if ( iRet != XUI_OK ) return iRet;
			iRet = __xuiTextEditHitSetCursor(pWidget, pData, pEvent, 1);
			if ( iRet != XUI_OK ) return iRet;
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			int bMoveSelection = pData->bMovingSelection;
			int bSetCursor = pData->bPressPending;
			if ( !xuiInternalPointerOwnerMatches(&pData->tPointerOwner, pEvent) ) return XUI_OK;
			iCursor = 0;
			if ( bMoveSelection ) {
				tWorld = xuiWidgetGetWorldRect(pWidget);
				iCursor = __xuiTextEditCursorFromPoint(pWidget, pData,
					pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY);
			}
			pData->bLineSelecting = 0;
			pData->bPressPending = 0;
			pData->bPressInsideSelection = 0;
			pData->bMovingSelection = 0;
			pData->bDragging = 0;
			iRet = xuiInternalPointerOwnerEnd(pContext, pWidget, pEvent,
				&pData->tPointerOwner);
			if ( iRet != XUI_OK ) return iRet;
			if ( bMoveSelection ) {
				iRet = __xuiTextEditMoveSelectionToCursor(pWidget, pData, iCursor);
				if ( iRet != XUI_OK ) return iRet;
			} else if ( bSetCursor ) {
				iRet = __xuiTextEditHitSetCursor(pWidget, pData, pEvent, 0);
				if ( iRet != XUI_OK ) return iRet;
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_WHEEL:
		iRet = xuiTextEditScrollBy(pWidget, -pEvent->fWheelX * 24.0f,
			-pEvent->fWheelY * (pData->fLineHeight > 0.0f ? pData->fLineHeight * 3.0f : 48.0f));
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( !xuiInternalPointerOwnerCaptureLost(&pData->tPointerOwner, pEvent) ) {
			return XUI_OK;
		}
		pData->bDragging = 0;
		pData->bLineSelecting = 0;
		pData->bPressPending = 0;
		pData->bPressInsideSelection = 0;
		pData->bMovingSelection = 0;
		return __xuiTextEditInvalidatePaint(pWidget);
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			if ( __xuiTextEditLineNumberHit(pWidget, pData, pEvent->fX, pEvent->fY) ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
			tWorld = xuiWidgetGetWorldRect(pWidget);
			/* Select the glyph under the pointer, not the nearest insertion boundary. */
			iCursor = __xuiTextEditOffsetFromPoint(pWidget, pData, pEvent->fX - tWorld.fX, pEvent->fY - tWorld.fY, 1);
			iRet = __xuiTextEditSelectWordAt(pWidget, pData, iCursor);
			if ( iRet != XUI_OK ) return iRet;
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_CONTEXT_MENU:
		iRet = xuiSetFocusWidget(pContext, pWidget);
		if ( iRet != XUI_OK ) return iRet;
		if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		iRet = __xuiTextEditCancelPointer(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey != XUI_KEY_CONTEXT_MENU && !__xuiTextEditHasSelectionData(pData) ) {
			iRet = __xuiTextEditHitSetCursor(pWidget, pData, pEvent, 0);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
			tWorld = xuiWidgetGetWorldRect(pWidget);
			fX = tWorld.fX + pData->tCursorRect.fX;
			fY = tWorld.fY + pData->tCursorRect.fY + pData->tCursorRect.fH;
		} else {
			fX = pEvent->fX;
			fY = pEvent->fY;
		}
		iRet = xuiTextEditOpenMenu(pWidget, fX, fY);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_KEY_DOWN:
		return __xuiTextEditHandleKey(pWidget, pData, pEvent);
	case XUI_EVENT_TEXT:
		if ( pData->bReadonly || pData->bImeActive || ((pEvent->iModifiers & XUI_MOD_CTRL) != 0) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		iTextSize = 0;
		if ( pEvent->iTextSize > 0 && pEvent->sText[0] != '\0' ) {
			iTextSize = pEvent->iTextSize;
			if ( iTextSize > (int)sizeof(sText) - 1 ) {
				iTextSize = __xuiTextEditUtf8ClampBytes(pEvent->sText, (int)sizeof(sText) - 1);
			}
			memcpy(sText, pEvent->sText, (size_t)iTextSize);
		} else if ( pEvent->iCodepoint >= 32u ) {
			iTextSize = __xuiTextEditUtf8Encode(pEvent->iCodepoint, sText);
		}
		if ( iTextSize > 0 ) {
			sText[iTextSize] = '\0';
			iRet = __xuiTextEditInsertText(pWidget, pData, sText, iTextSize);
			if ( iRet != XUI_OK ) return iRet;
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_IME_COMPOSITION:
		if ( pData->bReadonly ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->bCompositionActive ) {
			if ( pEvent->bCompositionReplacementRange ) {
				iTextSize = pData->iTextLength;
				pData->iImeAnchorStart = __xuiTextEditUtf8Clamp(pData->sText, iTextSize,
					pEvent->iCompositionReplacementStart);
				pData->iImeAnchorEnd = __xuiTextEditUtf8Clamp(pData->sText, iTextSize,
					pEvent->iCompositionReplacementEnd);
				if ( pData->iImeAnchorEnd < pData->iImeAnchorStart ) {
					pData->iImeAnchorEnd = pData->iImeAnchorStart;
				}
			} else if ( !pData->bImeActive ) {
				__xuiTextEditSelectionRange(pData, &pData->iImeAnchorStart, &pData->iImeAnchorEnd);
			}
			iRet = __xuiTextEditImeSetText(pData, pEvent->sText, pEvent->iTextSize);
			if ( iRet != XUI_OK ) {
				__xuiTextEditImeReset(pData);
				(void)__xuiTextEditInvalidateText(pWidget);
				(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_COMPOSITION_CHANGED,
					"", pData->iSelectStart, pData->iSelectEnd, 0, 0, 0);
				return iRet;
			}
			pData->bImeActive = 1;
			pData->iImeAttribute = pEvent->iCompositionAttribute;
			pData->iImeCursor = pEvent->iCompositionCursor;
			pData->iImeSelectionStart = pEvent->iCompositionSelectionStart;
			pData->iImeSelectionEnd = pEvent->iCompositionSelectionEnd;
			iRet = __xuiTextEditInvalidateText(pWidget);
			if ( iRet != XUI_OK ) return iRet;
			(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_COMPOSITION_CHANGED, pData->sImeText,
				pData->iSelectStart, pData->iSelectEnd, pData->iImeAnchorStart,
				pData->iImeAnchorStart + pEvent->iTextSize, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iTextSize > 0 && pEvent->sText[0] != '\0' ) {
			if ( pEvent->bCompositionReplacementRange ) {
				iTextSize = pData->iTextLength;
				iStart = __xuiTextEditUtf8Clamp(pData->sText, iTextSize,
					pEvent->iCompositionReplacementStart);
				iEnd = __xuiTextEditUtf8Clamp(pData->sText, iTextSize,
					pEvent->iCompositionReplacementEnd);
				if ( iEnd < iStart ) iEnd = iStart;
			} else if ( pData->bImeActive ) {
				iStart = pData->iImeAnchorStart;
				iEnd = pData->iImeAnchorEnd;
			} else {
				__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
			}
			__xuiTextEditImeReset(pData);
			(void)__xuiTextEditSetSelectionData(pData, iStart, iEnd);
			iRet = __xuiTextEditInsertText(pWidget, pData, pEvent->sText, pEvent->iTextSize);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( pData->bImeActive ) {
			__xuiTextEditImeReset(pData);
			iRet = __xuiTextEditInvalidateText(pWidget);
			if ( iRet != XUI_OK ) return iRet;
		}
		(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_COMPOSITION_CHANGED, "",
			pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static xui_rect_t __xuiTextEditImeRect(xui_widget pWidget, void* pUser)
{
	xui_text_edit_data_t* pData;
	xui_rect_t tWorld;
	xui_rect_t tRect;
	char* sDocument;
	xui_text_edit_line_t* pDocumentLines;
	xui_text_edit_layout_cache_t tDocumentLayout;
	int iDocumentCapacity;
	int iDocumentLength;
	int iDocumentLineCount;
	int iDocumentLineCapacity;
	int iDocumentCursor;
	int iDocumentAnchor;
	int iDocumentSelectStart;
	int iDocumentSelectEnd;
	int bDocumentLinesDirty;
	int bDocumentDirectLayout;
	float fDocumentLastLayoutWidth;
	float fDocumentContentWidth;
	float fDocumentContentHeight;
	float fDocumentLineHeight;
	int iRet;

	(void)pUser;
	pData = __xuiTextEditGetData(pWidget);
	memset(&tRect, 0, sizeof(tRect));
	if ( pData == NULL ) {
		return tRect;
	}
	if ( !pData->bImeActive ) {
		(void)__xuiTextEditUpdateCursorRect(pWidget, pData);
	} else if ( __xuiTextEditImeBuildDisplay(pData) == XUI_OK ) {
		sDocument = pData->sText;
		iDocumentCapacity = pData->iTextCapacity;
		iDocumentLength = pData->iTextLength;
		pDocumentLines = pData->pLines;
		tDocumentLayout = pData->tTextLayout;
		iDocumentLineCount = pData->iLineCount;
		iDocumentLineCapacity = pData->iLineCapacity;
		iDocumentCursor = pData->iCursor;
		iDocumentAnchor = pData->iAnchor;
		iDocumentSelectStart = pData->iSelectStart;
		iDocumentSelectEnd = pData->iSelectEnd;
		bDocumentLinesDirty = pData->bLinesDirty;
		bDocumentDirectLayout = pData->bDirectLayout;
		fDocumentLastLayoutWidth = pData->fLastLayoutWidth;
		fDocumentContentWidth = pData->fContentWidth;
		fDocumentContentHeight = pData->fContentHeight;
		fDocumentLineHeight = pData->fLineHeight;

		pData->sText = pData->sImeDisplay;
		pData->iTextCapacity = pData->iImeDisplayCapacity;
		pData->iTextLength = pData->iImeDisplayLength;
		pData->pLines = pData->pImeLines;
		pData->tTextLayout = pData->tImeTextLayout;
		pData->iLineCount = 0;
		pData->iLineCapacity = pData->iImeLineCapacity;
		pData->iCursor = pData->iImeAnchorStart + pData->iImeCursor;
		pData->iAnchor = pData->iImeAnchorStart + pData->iImeSelectionStart;
		pData->iSelectStart = pData->iAnchor;
		pData->iSelectEnd = pData->iImeAnchorStart + pData->iImeSelectionEnd;
		pData->bLinesDirty = 1;
		pData->fLastLayoutWidth = -1.0f;
		iRet = __xuiTextEditUpdateCursorRect(pWidget, pData);

		pData->pImeLines = pData->pLines;
		pData->tImeTextLayout = pData->tTextLayout;
		pData->iImeLineCapacity = pData->iLineCapacity;
		pData->sText = sDocument;
		pData->iTextCapacity = iDocumentCapacity;
		pData->iTextLength = iDocumentLength;
		pData->pLines = pDocumentLines;
		pData->tTextLayout = tDocumentLayout;
		pData->iLineCount = iDocumentLineCount;
		pData->iLineCapacity = iDocumentLineCapacity;
		pData->iCursor = iDocumentCursor;
		pData->iAnchor = iDocumentAnchor;
		pData->iSelectStart = iDocumentSelectStart;
		pData->iSelectEnd = iDocumentSelectEnd;
		pData->bLinesDirty = bDocumentLinesDirty;
		pData->bDirectLayout = bDocumentDirectLayout;
		pData->fLastLayoutWidth = fDocumentLastLayoutWidth;
		pData->fContentWidth = fDocumentContentWidth;
		pData->fContentHeight = fDocumentContentHeight;
		pData->fLineHeight = fDocumentLineHeight;
		(void)iRet;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRect = pData->tCursorRect;
	tRect.fX += tWorld.fX;
	tRect.fY += tWorld.fY;
	return tRect;
}

static void __xuiTextEditDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_HIDDEN;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiTextEditDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTextEditInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_TEXT, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_IME_COMPOSITION, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTextEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTextEditEvent, NULL);
	return iRet;
}

static int __xuiTextEditInitMenu(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiInternalMenuCreatePart(pWidget,&pData->pMenu,&tDesc);
	if ( iRet != XUI_OK ) {
		pData->pMenu = NULL;
		return iRet;
	}
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiTextEditMenuSelect, pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiTextEditUpdateMenu(pWidget, pData);
}

static void __xuiTextEditHScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_text_edit_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiTextEditGetData(pWidget);
	if ( (pData == NULL) || pData->bSyncingScrollBars ) return;
	(void)xuiTextEditSetScroll(pWidget, fValue, pData->fScrollY);
}

static void __xuiTextEditVScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_text_edit_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiTextEditGetData(pWidget);
	if ( (pData == NULL) || pData->bSyncingScrollBars ) return;
	(void)xuiTextEditSetScroll(pWidget, pData->fScrollX, fValue);
}

static int __xuiTextEditInitScrollBars(xui_widget pWidget, xui_text_edit_data_t* pData)
{
	xui_scrollbar_desc_t tDesc;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fMin = 0.0f;
	tDesc.fMax = 1.0f;
	tDesc.fPage = 1.0f;
	tDesc.iOrientation = XUI_ORIENTATION_HORIZONTAL;
	tDesc.iMode = XUI_SCROLLBAR_MODE_COMPACT;
	tDesc.iButtonMode = XUI_SCROLLBAR_BUTTONS_OFF;
	tDesc.fThickness = XUI_TEXT_EDIT_SCROLLBAR_SIZE;
	tDesc.fMinThumbSize = 18.0f;
	tDesc.iTrackColor = XUI_COLOR_RGBA(226, 232, 240, 255);
	tDesc.iThumbColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	tDesc.iHoverColor = XUI_COLOR_RGBA(100, 116, 139, 255);
	tDesc.iActiveColor = XUI_COLOR_RGBA(71, 85, 105, 255);
	tDesc.iFocusColor = XUI_COLOR_RGBA(59, 130, 246, 255);
	tDesc.iDisabledColor = XUI_COLOR_RGBA(203, 213, 225, 255);
	iRet = xuiInternalScrollBarCreatePart(pWidget,&pData->pHScrollBar,&tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInsertPart(pWidget,pWidget,pData->pHScrollBar,NULL);
	if ( iRet != XUI_OK ) return iRet;
	tDesc.iOrientation = XUI_ORIENTATION_VERTICAL;
	iRet = xuiInternalScrollBarCreatePart(pWidget,&pData->pVScrollBar,&tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInsertPart(pWidget,pWidget,pData->pVScrollBar,NULL);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetVisible(pData->pHScrollBar, 0);
	(void)xuiWidgetSetVisible(pData->pVScrollBar, 0);
	iRet = xuiScrollBarSetChange(pData->pHScrollBar, __xuiTextEditHScrollBarChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetChange(pData->pVScrollBar, __xuiTextEditVScrollBarChanged, pWidget);
	return iRet;
}

static int __xuiTextEditInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_text_edit_data_t* pData;
	const xui_text_edit_desc_t* pDesc;
	xui_theme_t tTheme;
	xui_thickness_t tPadding;
	xui_context pContext;
	xui_edit_behavior_t tEditBehavior;
	int iRet;

	(void)pUser;
	pData = (xui_text_edit_data_t*)pTypeData;
	pData->pContext=pWidget->pContext;
	pDesc = (const xui_text_edit_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTextEditDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tTheme, 0, sizeof(tTheme));
	tTheme.iSize = sizeof(tTheme);
	(void)xuiGetTheme(pContext, &tTheme);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iMaxLength = (pDesc != NULL) ? pDesc->iMaxLength : 0;
	pData->bReadonly = (pDesc != NULL) ? (pDesc->bReadonly != 0) : 0;
	pData->bWordWrap = (pDesc != NULL) ? (pDesc->bWordWrap != 0) : 1;
	pData->bLineNumbers = (pDesc != NULL) ? (pDesc->bLineNumbers != 0) : 0;
	pData->fLineNumberWidth = (pDesc != NULL && pDesc->fLineNumberWidth > 0.0f) ? pDesc->fLineNumberWidth : 44.0f;
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(31, 41, 55, 255);
	pData->iPlaceholderColor = (pDesc != NULL && pDesc->iPlaceholderColor != 0) ? pDesc->iPlaceholderColor : XUI_COLOR_RGBA(135, 148, 166, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(150, 160, 172, 255);
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iHoverBackgroundColor = (pDesc != NULL && pDesc->iHoverBackgroundColor != 0) ? pDesc->iHoverBackgroundColor : XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iDisabledBackgroundColor = (pDesc != NULL && pDesc->iDisabledBackgroundColor != 0) ? pDesc->iDisabledBackgroundColor : XUI_COLOR_RGBA(242, 245, 249, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(176, 190, 207, 255);
	pData->iHoverBorderColor = (pDesc != NULL && pDesc->iHoverBorderColor != 0) ? pDesc->iHoverBorderColor : XUI_COLOR_RGBA(105, 166, 226, 255);
	pData->iFocusBorderColor = (pDesc != NULL && pDesc->iFocusBorderColor != 0) ? pDesc->iFocusBorderColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iSelectionColor = (pDesc != NULL && pDesc->iSelectionColor != 0) ? pDesc->iSelectionColor : XUI_COLOR_RGBA(74, 144, 239, 255);
	pData->iFindResultColor = XUI_COLOR_RGBA(255, 235, 128, 150);
	pData->iFindActiveColor = XUI_COLOR_RGBA(255, 183, 77, 190);
	pData->iCursorColor = (pDesc != NULL && pDesc->iCursorColor != 0) ? pDesc->iCursorColor : XUI_COLOR_RGBA(33, 94, 170, 255);
	pData->iLineNumberColor = (pDesc != NULL && pDesc->iLineNumberColor != 0) ? pDesc->iLineNumberColor : XUI_COLOR_RGBA(112, 129, 150, 255);
	pData->iLineNumberBackgroundColor = (pDesc != NULL && pDesc->iLineNumberBackgroundColor != 0) ? pDesc->iLineNumberBackgroundColor : XUI_COLOR_RGBA(246, 249, 253, 255);
	pData->iLineNumberBorderColor = (pDesc != NULL && pDesc->iLineNumberBorderColor != 0) ? pDesc->iLineNumberBorderColor : XUI_COLOR_RGBA(213, 224, 238, 255);
	pData->fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : 1.0f;
	pData->fLineGap = (pDesc != NULL && pDesc->fLineGap > 0.0f) ? pDesc->fLineGap : 2.0f;
	pData->fLastLayoutWidth = -1.0f;
	pData->bLinesDirty = 1;
	pData->iFindActiveIndex = -1;
	xuiScrollModelInit(&pData->tScrollModel);
	iRet = __xuiTextEditAssignTextBytes(pData, (pDesc != NULL) ? pDesc->sText : "", -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iCursor = pData->iTextLength;
	(void)__xuiTextEditClearSelectionData(pData);
	iRet = __xuiTextEditStringSet(&pData->sPlaceholder, &pData->iPlaceholderCapacity, (pDesc != NULL) ? pDesc->sPlaceholder : "");
	if ( iRet != XUI_OK ) return iRet;
	tPadding = (xui_thickness_t){8.0f, 6.0f, 8.0f, 6.0f};
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetImeMode(pWidget, XUI_IME_AUTO);
	(void)xuiWidgetSetImeCandidateRect(pWidget, __xuiTextEditImeRect, NULL);
	memset(&tEditBehavior, 0, sizeof(tEditBehavior));
	tEditBehavior.iSize = sizeof(tEditBehavior);
	tEditBehavior.iTabBehavior = XUI_EDIT_TAB_FOCUS;
	tEditBehavior.iEnterBehavior = XUI_EDIT_ENTER_NEWLINE;
	tEditBehavior.iEscapeBehavior = XUI_EDIT_ESCAPE_DEFAULT;
	iRet = xuiInternalEditRegister(pWidget, &g_xuiTextEditAdapter, &tEditBehavior);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditInitScrollBars(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTextEditInitMenu(pWidget, pData);
}

static void __xuiTextEditDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_text_edit_data_t* pData;
	xui_widget pPopup;
	int i;

	(void)pUser;
	pData = (xui_text_edit_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pWidget != NULL ) (void)__xuiTextEditCancelPointer(pWidget, pData);
	/* These are tree-owned children, already freed before type destruction. */
	pData->pHScrollBar = NULL;
	pData->pVScrollBar = NULL;
	if ( pData->pMenu != NULL ) {
		pPopup = xuiMenuGetPopupWidget(pData->pMenu);
		if ( pPopup != NULL ) {
			(void)xuiInternalWidgetDestroyPart(pData->pMenu,pPopup);
		} else {
			(void)xuiInternalWidgetDestroyPart(pWidget,pData->pMenu);
		}
		pData->pMenu = NULL;
	}
	if ( pData->pFindWindow != NULL ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFindWindow);
		pData->pFindWindow = NULL;
	}
	if ( pData->sText != NULL ) xrtFree(pData->sText);
	if ( pData->sPlaceholder != NULL ) xrtFree(pData->sPlaceholder);
	if ( pData->sScratch != NULL ) xrtFree(pData->sScratch);
	if ( pData->sImeText != NULL ) xrtFree(pData->sImeText);
	if ( pData->sImeDisplay != NULL ) xrtFree(pData->sImeDisplay);
	if ( pData->sFocusText != NULL ) xrtFree(pData->sFocusText);
	if ( pData->pLines != NULL ) xrtFree(pData->pLines);
	if ( pData->pImeLines != NULL ) xrtFree(pData->pImeLines);
	xuiTextLayoutDestroy(pData->tTextLayout.pLayout);
	xuiTextLayoutDestroy(pData->tImeTextLayout.pLayout);
	(void)__xuiTextEditPaintCacheClear(pData);
	__xuiTextEditDestroyFindData(pData);
	for ( i = 0; i < XUI_INPUT_MENU_COUNT; i++ ) {
		if ( pData->arrMenuTitle[i] != NULL ) {
			xrtFree(pData->arrMenuTitle[i]);
		}
	}
	__xuiTextEditHistoryClear(pData->arrUndo, &pData->iUndoCount);
	__xuiTextEditHistoryClear(pData->arrRedo, &pData->iRedoCount);
	memset(pData, 0, sizeof(*pData));
}

static void __xuiTextEditRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) {
		return;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiTextEditRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	uint32_t iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;

	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.placeholder.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.background.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.border.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.border.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.selection.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.find.result_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.find.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.cursor.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.line_number.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.line_number.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.line_number.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.line_gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "textedit.line_number.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTextEditRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static int __xuiTextEditAccessibleNode(xui_widget pWidget, xui_accessible_node_t* pNode)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiInternalAccessibilityEditNode(pWidget, pNode);
	pNode->sName = pData->sPlaceholder;
	pNode->iState |= XUI_ACCESSIBLE_STATE_MULTILINE;
	return XUI_OK;
}

static const xui_internal_accessibility_adapter_t g_xuiTextEditAccessible = {
	.getNode = __xuiTextEditAccessibleNode, .performAction = xuiInternalAccessibilityEditAction
};

static int __xuiTextEditDestroyPrepare(xui_widget widget,void* raw,bool* complete)
{
    if(!complete)return XUI_ERROR_INVALID_ARGUMENT;
    *complete=false;xui_text_edit_data_t* data=raw;bool waiting=false;
    if(data->iCacheActivity)return XUI_STATUS_DEFERRED;
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,XUI_OK);

    xui_text_layout* layouts[]={&data->tTextLayout.pLayout,&data->tImeTextLayout.pLayout,&data->pWorkLayout};
    for(unsigned i=0;i<3;++i)if(*layouts[i]){
        bool consumed=false;int status=xuiTextLayoutDestroyOwnerChecked(*layouts[i],&consumed);
        if(consumed)*layouts[i]=NULL;
        if(status!=XUI_STATUS_DEFERRED)xuiInternalCleanupAdd(&receipt,status);
        if(!consumed)waiting=true;
    }
    int cacheStatus=__xuiTextEditPaintCacheClear(data);
    if(cacheStatus!=XUI_STATUS_DEFERRED)xuiInternalCleanupAdd(&receipt,cacheStatus);
    if(data->iPaintCacheCount)waiting=true;

    *complete=!waiting;int status=xuiInternalCleanupEnd(NULL,&receipt);
    return status!=XUI_OK ? status:waiting ? XUI_STATUS_DEFERRED:XUI_OK;
}
static int __xuiTextEditDestroyReady(xui_widget widget,const void* raw)
{
    const xui_text_edit_data_t* data=raw;if(data->iCacheActivity)return XUI_ERROR_BUSY;
    bool managed=widget->pContext->pOwnedProxy!=NULL;int status;
    if(data->pWorkLayout){status=xuiInternalTextLayoutDestroyReady(data->pWorkLayout);if(status!=XUI_OK)return status;}
    if(data->tTextLayout.pLayout){status=xuiInternalTextLayoutDestroyReady(data->tTextLayout.pLayout);if(status!=XUI_OK)return status;}
    if(data->tImeTextLayout.pLayout){status=xuiInternalTextLayoutDestroyReady(data->tImeTextLayout.pLayout);if(status!=XUI_OK)return status;}
    for(int i=0;i<data->iPaintCacheCount;++i){
        status=xuiInternalTextShapeReady(&data->arrPaintCache[i].tShape,managed);if(status!=XUI_OK)return status;
        status=xuiInternalTextChunkCacheReady(&data->arrPaintCache[i].tChunks,managed);if(status!=XUI_OK)return status;
    }
    return XUI_OK;
}
static bool __xuiTextEditTraceOwnership(xui_widget widget,const void* raw,xrtownershipvisitor visit,void* user)
{
    (void)widget;const xui_text_edit_data_t* data=raw;
    if(data->iCacheActivity)return false;
    if(data->pWorkLayout && !xuiTextLayoutTraceOwnership(data->pWorkLayout,visit,user))return false;
    if(data->tTextLayout.pLayout && !xuiTextLayoutTraceOwnership(data->tTextLayout.pLayout,visit,user))return false;
    if(data->tImeTextLayout.pLayout && !xuiTextLayoutTraceOwnership(data->tImeTextLayout.pLayout,visit,user))return false;
    for(int i=0;i<data->iPaintCacheCount;++i) {
        if(widget->pContext->pOwnedProxy && data->arrPaintCache[i].pFont && !visit(xuiFontOwnership(data->arrPaintCache[i].pFont),user))return false;
        if(!xuiInternalTextShapeTraceOwnership(&data->arrPaintCache[i].tShape,visit,user) ||
            !xuiInternalTextChunkCacheTraceOwnership(&data->arrPaintCache[i].tChunks,visit,user))return false;
    }
    return true;
}
XUI_API xui_widget_type xuiTextEditGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "textedit");
	if ( pType != NULL ) {
		__xuiTextEditRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "textedit";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_text_edit_data_t);
	tDesc.onInit = __xuiTextEditInit;
	tDesc.onDestroy = __xuiTextEditDestroy;
	tDesc.onContentMeasure = __xuiTextEditContentMeasure;
	tDesc.onCacheRender = __xuiTextEditCacheRender;
	tDesc.onUpdate = __xuiTextEditUpdate;
	tDesc.onQueryCursor = __xuiTextEditQueryCursor;
	__xuiTextEditDefaultLayout(&tDesc.tLayout);
	__xuiTextEditDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiTextEditRegisterStyleProperties(pContext, pType);
	pType->pAccessibleAdapter = &g_xuiTextEditAccessible;
	    static const size_t fontOffsets[]={offsetof(xui_text_edit_data_t,pFont)};
    pType->onTraceOwnership=__xuiTextEditTraceOwnership;pType->onDestroyReady=__xuiTextEditDestroyReady;pType->onDestroyPrepare=__xuiTextEditDestroyPrepare;
    pType->pFontOffsets=fontOffsets;pType->iFontSlotCount=sizeof(fontOffsets)/sizeof(fontOffsets[0]);
    return pType;
}

static int __xuiTextEditCreateWithOwner(xui_context pContext, xui_widget* ppWidget, const xui_text_edit_desc_t* pDesc,xui_widget pNativeOwner)
{
	xui_widget_type pType;

	if ( ppWidget == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	if ( !xuiInternalContextIsValid(pContext) || !__xuiTextEditDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = xuiTextEditGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return (pNativeOwner ? xuiInternalWidgetCreatePart(pNativeOwner,pType,ppWidget,pDesc) : xuiWidgetCreateTyped(pContext,pType,ppWidget,pDesc));
}

XUI_API int xuiTextEditCreate(xui_context pContext, xui_widget* ppWidget, const xui_text_edit_desc_t* pDesc)
{return __xuiTextEditCreateWithOwner(pContext,ppWidget,pDesc,NULL);}
int xuiInternalTextEditCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_text_edit_desc_t* pDesc)
{
    if(ppWidget)*ppWidget=NULL;
    if(!xuiInternalWidgetIsValid(pOwner) || !ppWidget)return XUI_ERROR_INVALID_ARGUMENT;
    xui_context context=xuiWidgetGetContext(pOwner);
    xuiInternalOperationEnter(context);
    int status=__xuiTextEditCreateWithOwner(context,ppWidget,pDesc,pOwner);
    xuiInternalOperationLeave(context);
    return status;
}


XUI_API int xuiTextEditSetChangeOwned(xui_widget pWidget, xui_text_edit_change_proc onChange, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onChange, onChange != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTextEditSetChange(xui_widget pWidget, xui_text_edit_change_proc onChange, void* pUser)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onChange) )
		return xuiTextEditSetChangeOwned(pWidget, onChange, pUser, NULL);
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTextEditSetText(xui_widget pWidget, const char* sText)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTextEditSetTextInternal(pWidget, pData, sText, 0, 0);
}

XUI_API const char* xuiTextEditGetText(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : "";
}

XUI_API int xuiTextEditSetPlaceholder(xui_widget pWidget, const char* sText)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
	if ( sText == NULL ) sText = "";
	if ( strcmp((pData->sPlaceholder != NULL) ? pData->sPlaceholder : "", sText) == 0 ) {
		return XUI_OK;
	}
	iRet = __xuiTextEditStringSet(&pData->sPlaceholder, &pData->iPlaceholderCapacity, sText);
	if ( iRet != XUI_OK ) return iRet;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	return __xuiTextEditInvalidatePaint(pWidget);
}

XUI_API const char* xuiTextEditGetPlaceholder(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL && pData->sPlaceholder != NULL) ? pData->sPlaceholder : "";
}

static int xuiTextEditSetFontImpl(xui_widget pWidget, xui_font pFont)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	xui_font pNewFont;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pNewFont = (pFont != NULL) ? pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( pData->pFont == pNewFont ) return XUI_OK;
    xui_font_slot_update_t fontUpdate={0};
    int fontStatus=xuiInternalFontSlotPrepare(pWidget->pContext,&pData->pFont,pNewFont,pWidget->bTypeInitialized!=0,false,&fontUpdate);
    if(fontStatus!=XUI_OK)return fontStatus;
    fontUpdate.widget=pWidget;
    int cacheStatus=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(cacheStatus!=XUI_OK)return xuiInternalFontSlotEnd(&fontUpdate,false,cacheStatus);
	if ( pData->pMenu != NULL ) {
		iRet = xuiMenuSetFont(pData->pMenu, pNewFont);
		if ( iRet != XUI_OK ) return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
	}
	fontStatus=xuiInternalFontSlotPublish(&fontUpdate);
    if(fontStatus!=XUI_OK)return xuiInternalFontSlotEnd(&fontUpdate,true,fontStatus);
	return xuiInternalFontSlotEnd(&fontUpdate,true,__xuiTextEditInvalidateText(pWidget));
}
XUI_API int xuiTextEditSetFont(xui_widget pWidget,xui_font pFont)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetFontImpl(pWidget,pFont);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API xui_font xuiTextEditGetFont(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

static int xuiTextEditSetMaxLengthImpl(xui_widget pWidget, int iMaxLength)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int iOldMaxLength;
	int iOldTextLength;
	int iRet;
	if ( (pData == NULL) || (iMaxLength < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldMaxLength = pData->iMaxLength;
	if ( iOldMaxLength == iMaxLength ) return XUI_OK;
	iOldTextLength = pData->iTextLength;
    int cacheStatus=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(cacheStatus!=XUI_OK)return cacheStatus;
	pData->iMaxLength = iMaxLength;
	if ( iMaxLength > 0 && pData->iTextLength > iMaxLength ) {
		iRet = __xuiTextEditAssignTextBytes(pData, pData->sText, pData->iTextLength);
		if ( iRet != XUI_OK ) {
			pData->iMaxLength = iOldMaxLength;
			return iRet;
		}
	}
	__xuiTextEditHistoryClear(pData->arrUndo, &pData->iUndoCount);
	__xuiTextEditHistoryClear(pData->arrRedo, &pData->iRedoCount);
	if ( pData->iTextLength != iOldTextLength ) {
		__xuiTextEditImeReset(pData);
		__xuiTextEditInvalidateFindState(pData);
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_VALUE_CHANGED);
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	}
	return __xuiTextEditInvalidateText(pWidget);
}
XUI_API int xuiTextEditSetMaxLength(xui_widget pWidget,int iMaxLength)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetMaxLengthImpl(pWidget,iMaxLength);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetMaxLength(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->iMaxLength : 0;
}

XUI_API int xuiTextEditSetReadonly(xui_widget pWidget, int bReadonly)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	xui_context pContext;
	int bNewReadonly;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bNewReadonly = bReadonly != 0;
	if ( pData->bReadonly == bNewReadonly ) return XUI_OK;
	pContext = xuiWidgetGetContext(pWidget);
	if ( bNewReadonly ) {
		iRet = __xuiTextEditCancelPointer(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		if ( !__xuiTextEditAlive(pContext, pWidget, pData) ) return XUI_OK;
	}
	pData->bReadonly = bNewReadonly;
	if ( bNewReadonly ) {
		if ( pData->bImeActive ) __xuiTextEditImeReset(pData);
	}
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_STATE_CHANGED);
	return __xuiTextEditInvalidatePaint(pWidget);
}

XUI_API int xuiTextEditIsReadonly(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->bReadonly : 0;
}

static int xuiTextEditSetWordWrapImpl(xui_widget pWidget, int bWordWrap)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int bNewWordWrap;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bNewWordWrap = bWordWrap != 0;
	if ( pData->bWordWrap == bNewWordWrap ) return XUI_OK;
    int cacheStatus=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(cacheStatus!=XUI_OK)return cacheStatus;
	pData->bWordWrap = bNewWordWrap;
	if ( bNewWordWrap ) pData->fScrollX = 0.0f;
	__xuiTextEditMarkLinesDirty(pData);
	return __xuiTextEditInvalidateText(pWidget);
}
XUI_API int xuiTextEditSetWordWrap(xui_widget pWidget,int bWordWrap)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetWordWrapImpl(pWidget,bWordWrap);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetWordWrap(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->bWordWrap : 0;
}

static int xuiTextEditSetLineNumbersImpl(xui_widget pWidget, int bVisible, float fWidth)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int bNewVisible;
	float fNewWidth;
	if ( (pData == NULL) || !isfinite(fWidth) || (fWidth < 0.0f) ||
	     (fWidth > XUI_LAYOUT_UNBOUNDED) ) return XUI_ERROR_INVALID_ARGUMENT;
	bNewVisible = bVisible != 0;
	fNewWidth = (fWidth > 0.0f) ? fWidth : pData->fLineNumberWidth;
	if ( pData->bLineNumbers == bNewVisible && pData->fLineNumberWidth == fNewWidth ) {
		return XUI_OK;
	}
    int cacheStatus=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(cacheStatus!=XUI_OK)return cacheStatus;
	pData->bLineNumbers = bNewVisible;
	if ( fWidth > 0.0f ) {
		pData->fLineNumberWidth = fWidth;
	}
	__xuiTextEditMarkLinesDirty(pData);
	return __xuiTextEditInvalidateText(pWidget);
}
XUI_API int xuiTextEditSetLineNumbers(xui_widget pWidget,int bVisible,float fWidth)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetLineNumbersImpl(pWidget,bVisible,fWidth);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetLineNumbers(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->bLineNumbers : 0;
}

XUI_API float xuiTextEditGetLineNumberWidth(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->fLineNumberWidth : 0.0f;
}

XUI_API int xuiTextEditSetLineNumberColors(xui_widget pWidget, uint32_t iText, uint32_t iBackground, uint32_t iBorder)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iLineNumberColor = iText;
	pData->iLineNumberBackgroundColor = iBackground;
	pData->iLineNumberBorderColor = iBorder;
	return __xuiTextEditInvalidatePaint(pWidget);
}

XUI_API int xuiTextEditGetLineNumberColors(xui_widget pWidget, uint32_t* pText, uint32_t* pBackground, uint32_t* pBorder)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pText != NULL ) *pText = pData->iLineNumberColor;
	if ( pBackground != NULL ) *pBackground = pData->iLineNumberBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iLineNumberBorderColor;
	return XUI_OK;
}

static int __xuiTextEditFindSelectionSnapshot(xui_text_edit_data_t* pData, uint32_t iFlags, int* pRangeStart, int* pRangeEnd)
{
	int iStart;
	int iEnd;
	int iLen;

	if ( pData == NULL || pRangeStart == NULL || pRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLen = pData->iTextLength;
	if ( (iFlags & XUI_FIND_SELECTION) != 0 ) {
		if ( pData->iFindRangeEnd > pData->iFindRangeStart ) {
			*pRangeStart = __xuiTextEditUtf8Clamp(pData->sText, iLen, pData->iFindRangeStart);
			*pRangeEnd = __xuiTextEditUtf8Clamp(pData->sText, iLen, pData->iFindRangeEnd);
			return XUI_OK;
		}
		__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
		if ( iEnd <= iStart ) return XUI_ERROR_UNSUPPORTED;
		*pRangeStart = iStart;
		*pRangeEnd = iEnd;
		return XUI_OK;
	}
	*pRangeStart = 0;
	*pRangeEnd = iLen;
	return XUI_OK;
}

static int __xuiTextEditFindResolve(xui_text_edit_data_t* pData, const xui_find_options_t* pOptions, int bBackward, const char** psPattern, const char** psReplacement, uint32_t* pFlags, int* pStartOffset, int* pRangeStart, int* pRangeEnd)
{
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStart;
	int iEnd;
	int iSelStart;
	int iSelEnd;
	int iLen;
	int iRet;

	if ( pData == NULL || psPattern == NULL || pFlags == NULL || pStartOffset == NULL || pRangeStart == NULL || pRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pOptions != NULL && pOptions->iSize != 0 &&
	     pOptions->iSize < sizeof(*pOptions) ) return XUI_ERROR_INVALID_ARGUMENT;
	iLen = pData->iTextLength;
	sPattern = (pOptions != NULL && pOptions->sPattern != NULL) ? pOptions->sPattern : pData->sFindPattern;
	sReplacement = (pOptions != NULL && pOptions->sReplacement != NULL) ? pOptions->sReplacement : pData->sFindReplacement;
	if ( sPattern == NULL || sPattern[0] == '\0' ) return XUI_ERROR_INVALID_ARGUMENT;
	iFlags = (pOptions != NULL) ? pOptions->iFlags : pData->iFindFlags;
	if ( bBackward ) iFlags |= XUI_FIND_BACKWARD;
	else iFlags &= ~XUI_FIND_BACKWARD;
	iRet = __xuiTextEditFindSelectionSnapshot(pData, iFlags, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( pOptions != NULL && (iFlags & XUI_FIND_SELECTION) == 0 && pOptions->iRangeEnd > pOptions->iRangeStart ) {
		iStart = __xuiTextEditUtf8Clamp(pData->sText, iLen, pOptions->iRangeStart);
		iEnd = __xuiTextEditUtf8Clamp(pData->sText, iLen, pOptions->iRangeEnd);
		if ( iEnd < iStart ) {
			int iSwap = iStart;
			iStart = iEnd;
			iEnd = iSwap;
		}
	}
	__xuiTextEditSelectionRange(pData, &iSelStart, &iSelEnd);
	if ( bBackward ) {
		*pStartOffset = (iSelStart != iSelEnd) ? iSelStart : pData->iCursor;
	} else {
		*pStartOffset = (iSelStart != iSelEnd) ? iSelEnd : pData->iCursor;
	}
	if ( pOptions != NULL && pOptions->iStartOffset > 0 ) {
		*pStartOffset = __xuiTextEditUtf8Clamp(pData->sText, iLen, pOptions->iStartOffset);
	}
	*pStartOffset = __xuiTextEditUtf8Clamp(pData->sText, iLen, *pStartOffset);
	if ( *pStartOffset < iStart ) *pStartOffset = iStart;
	if ( *pStartOffset > iEnd ) *pStartOffset = iEnd;
	*psPattern = sPattern;
	if ( psReplacement != NULL ) *psReplacement = (sReplacement != NULL) ? sReplacement : "";
	*pFlags = iFlags;
	*pRangeStart = iStart;
	*pRangeEnd = iEnd;
	return XUI_OK;
}

static int __xuiTextEditStoreFindState(xui_text_edit_data_t* pData, const char* sPattern, const char* sReplacement, uint32_t iFlags, int iRangeStart, int iRangeEnd)
{
	int iPatternSize;
	int iReplacementSize;
	int iRet;

	if ( pData == NULL || sPattern == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sReplacement == NULL ) sReplacement = "";
	iRet = __xuiTextEditStringSize(sPattern, &iPatternSize);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditStringSize(sReplacement, &iReplacementSize);
	if ( iRet != XUI_OK ) return iRet;
	if ( iPatternSize == INT_MAX || iReplacementSize == INT_MAX )
		return XUI_ERROR_BUFFER_TOO_SMALL;
	iRet = __xuiTextEditTextReserve(&pData->sFindPattern,
		&pData->iFindPatternCapacity, iPatternSize + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditTextReserve(&pData->sFindReplacement,
		&pData->iFindReplacementCapacity, iReplacementSize + 1);
	if ( iRet != XUI_OK ) return iRet;
	memmove(pData->sFindPattern, sPattern, (size_t)iPatternSize + 1u);
	memmove(pData->sFindReplacement, sReplacement,
		(size_t)iReplacementSize + 1u);
	pData->iFindFlags = iFlags;
	pData->iFindRangeStart = ((iFlags & XUI_FIND_SELECTION) != 0) ? iRangeStart : 0;
	pData->iFindRangeEnd = ((iFlags & XUI_FIND_SELECTION) != 0) ? iRangeEnd : 0;
	return XUI_OK;
}

static int __xuiTextEditUpdateFindResults(xui_widget pWidget, xui_text_edit_data_t* pData, const char* sPattern, uint32_t iFlags, int iRangeStart, int iRangeEnd, const xui_find_result_t* pActive)
{
	int iCount;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL || sPattern == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iCount = 0;
	iRet = xuiFindCollectText((pData->sText != NULL) ? pData->sText : "", -1, sPattern, iRangeStart, iRangeEnd, iFlags, NULL, 0, &iCount, NULL, 0);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditFindResultReserve(pData, iCount);
	if ( iRet != XUI_OK ) return iRet;
	pData->iFindResultCount = 0;
	pData->iFindActiveIndex = -1;
	memset(&pData->tFindActive, 0, sizeof(pData->tFindActive));
	pData->tFindActive.iSize = sizeof(pData->tFindActive);
	if ( iCount > 0 ) {
		iRet = xuiFindCollectText((pData->sText != NULL) ? pData->sText : "", -1, sPattern, iRangeStart, iRangeEnd, iFlags, pData->pFindResults, iCount, &pData->iFindResultCount, NULL, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pActive != NULL ) {
		pData->tFindActive = *pActive;
		for ( i = 0; i < pData->iFindResultCount; i++ ) {
			if ( pData->pFindResults[i].iStart == pActive->iStart && pData->pFindResults[i].iEnd == pActive->iEnd ) {
				pData->iFindActiveIndex = i;
				break;
			}
		}
	}
	return __xuiTextEditInvalidatePaint(pWidget);
}

static int __xuiTextEditFindRangeAfterReplace(int iOldLength,
	int iNewLength, int iRangeStart, int iRangeEnd, int* pNewRangeEnd)
{
	int iSuffixLength;
	int iNewRangeEnd;

	if ( iOldLength < 0 || iNewLength < 0 || iRangeStart < 0 ||
	     iRangeEnd < iRangeStart || iRangeEnd > iOldLength ||
	     pNewRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iSuffixLength = iOldLength - iRangeEnd;
	if ( iNewLength < iSuffixLength ) return XUI_ERROR_INVALID_STATE;
	iNewRangeEnd = iNewLength - iSuffixLength;
	if ( iNewRangeEnd < iRangeStart ) return XUI_ERROR_INVALID_STATE;
	*pNewRangeEnd = iNewRangeEnd;
	return XUI_OK;
}

static int __xuiTextEditApplyFindResult(xui_widget pWidget, xui_text_edit_data_t* pData, const xui_find_result_t* pResult)
{
	int iRet;

	if ( pWidget == NULL || pData == NULL || pResult == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTextEditSetSelectionData(pData, pResult->iStart, pResult->iEnd);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTextEditSyncCursor(pWidget, pData);
}

static int __xuiTextEditFindMove(xui_widget pWidget, const xui_find_options_t* pOptions, int bBackward)
{
	xui_text_edit_data_t* pData;
	xui_find_result_t tResult;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iRet;

	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTextEditFindResolve(pData, pOptions, bBackward, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tResult, 0, sizeof(tResult));
	iRet = xuiFindText((pData->sText != NULL) ? pData->sText : "", -1, sPattern, iStartOffset, iRangeStart, iRangeEnd, iFlags | XUI_FIND_WRAP, &tResult, NULL, 0);
	if ( iRet != XUI_OK ) {
		__xuiTextEditClearFindResults(pData);
		(void)__xuiTextEditInvalidatePaint(pWidget);
		return iRet;
	}
	iRet = __xuiTextEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditUpdateFindResults(pWidget, pData, sPattern, iFlags, iRangeStart, iRangeEnd, &tResult);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTextEditApplyFindResult(pWidget, pData, &tResult);
}

static int xuiTextEditSetSelectionImpl(xui_widget pWidget, int iStart, int iEnd)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int iRet;
	int iOldStart;
	int iOldEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldStart = pData->iSelectStart;
	iOldEnd = pData->iSelectEnd;
	iRet = __xuiTextEditSetSelectionData(pData, iStart, iEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTextEditSyncCursor(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iSelectStart != iOldStart || pData->iSelectEnd != iOldEnd ) {
		__xuiTextEditResetFindRange(pData);
		(void)xuiInternalEditEmit(pWidget, XUI_EDIT_EVENT_SELECTION_CHANGED,
			pData->sText, pData->iSelectStart, pData->iSelectEnd, 0, 0, 1);
	}
	return XUI_OK;
}
XUI_API int xuiTextEditSetSelection(xui_widget pWidget,int iStart,int iEnd)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetSelectionImpl(pWidget,iStart,iEnd);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetSelection(xui_widget pWidget, int* pStart, int* pEnd)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTextEditSelectionRange(pData, pStart, pEnd);
	return XUI_OK;
}

XUI_API int xuiTextEditSelectAll(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTextEditSetSelection(pWidget, 0, pData->iTextLength);
}

XUI_API int xuiTextEditHasSelection(xui_widget pWidget)
{
	return __xuiTextEditHasSelectionData(__xuiTextEditGetData(pWidget));
}

XUI_API int xuiTextEditCopy(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTextEditCopySelection(pWidget, pData);
}

XUI_API int xuiTextEditCut(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_OK;
	(void)__xuiTextEditCopySelection(pWidget, pData);
	return xuiTextEditDeleteSelection(pWidget);
}

XUI_API int xuiTextEditPaste(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTextEditPasteClipboard(pWidget, pData);
}

XUI_API int xuiTextEditDeleteSelection(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int iStart;
	int iEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly || !__xuiTextEditHasSelectionData(pData) ) return XUI_OK;
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	return __xuiTextEditDeleteRange(pWidget, pData, iStart, iEnd, 1, 1);
}

static void __xuiTextEditFindWindowOptions(xui_text_edit_data_t* pData, xui_find_options_t* pOptions)
{
	uint32_t iFlags;

	if ( pOptions == NULL ) return;
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	if ( pData == NULL ) return;
	pOptions->sPattern = (pData->pFindInput != NULL) ? xuiInputGetText(pData->pFindInput) : pData->sFindPattern;
	pOptions->sReplacement = (pData->pReplaceInput != NULL) ? xuiInputGetText(pData->pReplaceInput) : pData->sFindReplacement;
	pOptions->iStartOffset = -1;
	iFlags = 0u;
	if ( pData->pCaseCheck != NULL && xuiCheckBoxGetChecked(pData->pCaseCheck) ) iFlags |= XUI_FIND_CASE_SENSITIVE;
	if ( pData->pWordCheck != NULL && xuiCheckBoxGetChecked(pData->pWordCheck) ) iFlags |= XUI_FIND_WHOLE_WORD;
	if ( pData->pRegexCheck != NULL && xuiCheckBoxGetChecked(pData->pRegexCheck) ) iFlags |= XUI_FIND_REGEX;
	if ( pData->pEscapeCheck != NULL && xuiCheckBoxGetChecked(pData->pEscapeCheck) ) iFlags |= XUI_FIND_ESCAPE;
	if ( pData->pSelectionCheck != NULL && xuiCheckBoxGetChecked(pData->pSelectionCheck) ) iFlags |= XUI_FIND_SELECTION;
	pOptions->iFlags = iFlags;
}

static void __xuiTextEditFindWindowSetStatus(xui_text_edit_data_t* pData, const char* sText)
{
	if ( pData == NULL || pData->pFindStatus == NULL ) return;
	(void)xuiLabelSetText(pData->pFindStatus, (sText != NULL) ? sText : "");
}

static void __xuiTextEditFindWindowApplyLanguage(xui_widget pTextEdit, xui_text_edit_data_t* pData)
{
	xui_context pContext;

	if ( pTextEdit == NULL || pData == NULL || pData->pFindWindow == NULL ) return;
	pContext = xuiWidgetGetContext(pTextEdit);
	pData->iFindLanguageRevision = xuiGetLanguageRevision(pContext);
	(void)xuiWindowSetTitle(pData->pFindWindow, xuiTranslate(pContext, pData->bFindWindowReplace ? XUI_TR_REPLACE_TITLE : XUI_TR_FIND_TITLE));
	if ( pData->pFindInput != NULL ) (void)xuiInputSetPlaceholder(pData->pFindInput, xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER));
	if ( pData->pReplaceInput != NULL ) (void)xuiInputSetPlaceholder(pData->pReplaceInput, xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER));
	if ( pData->pFindPrevButton != NULL ) (void)xuiButtonSetText(pData->pFindPrevButton, xuiTranslate(pContext, XUI_TR_FIND_PREVIOUS));
	if ( pData->pFindNextButton != NULL ) (void)xuiButtonSetText(pData->pFindNextButton, xuiTranslate(pContext, XUI_TR_FIND_NEXT));
	if ( pData->pReplaceButton != NULL ) (void)xuiButtonSetText(pData->pReplaceButton, xuiTranslate(pContext, XUI_TR_REPLACE_CURRENT));
	if ( pData->pReplaceAllButton != NULL ) (void)xuiButtonSetText(pData->pReplaceAllButton, xuiTranslate(pContext, XUI_TR_REPLACE_ALL));
	if ( pData->pCaseCheck != NULL ) (void)xuiCheckBoxSetText(pData->pCaseCheck, xuiTranslate(pContext, XUI_TR_FIND_CASE));
	if ( pData->pWordCheck != NULL ) (void)xuiCheckBoxSetText(pData->pWordCheck, xuiTranslate(pContext, XUI_TR_FIND_WORD));
	if ( pData->pRegexCheck != NULL ) (void)xuiCheckBoxSetText(pData->pRegexCheck, xuiTranslate(pContext, XUI_TR_FIND_REGEX));
	if ( pData->pEscapeCheck != NULL ) (void)xuiCheckBoxSetText(pData->pEscapeCheck, xuiTranslate(pContext, XUI_TR_FIND_ESCAPE));
	if ( pData->pSelectionCheck != NULL ) (void)xuiCheckBoxSetText(pData->pSelectionCheck, xuiTranslate(pContext, XUI_TR_FIND_SELECTION));
}

static int __xuiTextEditUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_text_edit_data_t* pData;
	xui_context pContext;

	(void)fDelta;
	(void)pUser;
	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFindWindow != NULL ) {
		pContext = xuiWidgetGetContext(pWidget);
		if ( pData->iFindLanguageRevision != xuiGetLanguageRevision(pContext) ) {
			__xuiTextEditFindWindowApplyLanguage(pWidget, pData);
		}
	}
	return XUI_OK;
}

static void __xuiTextEditFindWindowRefresh(xui_widget pTextEdit)
{
	xui_text_edit_data_t* pData;
	xui_find_options_t tOptions;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iRet;
	char sStatus[64];

	pData = __xuiTextEditGetData(pTextEdit);
	if ( pData == NULL ) return;
	__xuiTextEditFindWindowOptions(pData, &tOptions);
	if ( tOptions.sPattern == NULL || tOptions.sPattern[0] == '\0' ) {
		__xuiTextEditClearFindResults(pData);
		(void)__xuiTextEditInvalidatePaint(pTextEdit);
		__xuiTextEditFindWindowSetStatus(pData, "");
		return;
	}
	iRet = __xuiTextEditFindResolve(pData, &tOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) {
		__xuiTextEditClearFindResults(pData);
		(void)__xuiTextEditInvalidatePaint(pTextEdit);
		__xuiTextEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_NO_SEARCH_RANGE));
		return;
	}
	iRet = __xuiTextEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditUpdateFindResults(pTextEdit, pData, sPattern, iFlags, iRangeStart, iRangeEnd, NULL);
	if ( iRet != XUI_OK ) {
		__xuiTextEditClearFindResults(pData);
		(void)__xuiTextEditInvalidatePaint(pTextEdit);
		__xuiTextEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_INVALID_PATTERN));
		return;
	}
	snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_MATCHES_FMT), pData->iFindResultCount);
	__xuiTextEditFindWindowSetStatus(pData, sStatus);
}

static void __xuiTextEditFindInputChange(xui_widget pWidget, const char* sText, void* pUser)
{
	(void)pWidget;
	(void)sText;
	__xuiTextEditFindWindowRefresh((xui_widget)pUser);
}

static void __xuiTextEditFindCheckChange(xui_widget pWidget, int bChecked, void* pUser)
{
	xui_text_edit_data_t* pData;
	int iStart;
	int iEnd;

	(void)pWidget;
	(void)bChecked;
	pData = __xuiTextEditGetData((xui_widget)pUser);
	if ( pData != NULL ) {
		pData->iFindRangeStart = 0;
		pData->iFindRangeEnd = 0;
		if ( pData->pSelectionCheck != NULL && xuiCheckBoxGetChecked(pData->pSelectionCheck) ) {
			__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
			if ( iEnd > iStart ) {
				pData->iFindRangeStart = iStart;
				pData->iFindRangeEnd = iEnd;
			}
		}
	}
	__xuiTextEditFindWindowRefresh((xui_widget)pUser);
}

static void __xuiTextEditFindButtonClick(xui_widget pButton, void* pUser)
{
	xui_widget pTextEdit;
	xui_text_edit_data_t* pData;
	xui_find_options_t tOptions;
	int iCount;
	int iRet;
	char sStatus[64];

	pTextEdit = (xui_widget)pUser;
	pData = __xuiTextEditGetData(pTextEdit);
	if ( pData == NULL ) return;
	__xuiTextEditFindWindowOptions(pData, &tOptions);
	iRet = XUI_OK;
	if ( pButton == pData->pFindPrevButton ) {
		iRet = xuiTextEditFindPrevious(pTextEdit, &tOptions);
	} else if ( pButton == pData->pFindNextButton ) {
		iRet = xuiTextEditFindNext(pTextEdit, &tOptions);
	} else if ( pButton == pData->pReplaceButton ) {
		iRet = xuiTextEditReplaceCurrent(pTextEdit, &tOptions);
	} else if ( pButton == pData->pReplaceAllButton ) {
		iCount = 0;
		iRet = xuiTextEditReplaceAll(pTextEdit, &tOptions, &iCount);
		snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_REPLACED_FMT), iCount);
		__xuiTextEditFindWindowSetStatus(pData, sStatus);
	}
	if ( iRet == XUI_OK && pButton != pData->pReplaceAllButton ) {
		snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_MATCHES_FMT), pData->iFindResultCount);
		__xuiTextEditFindWindowSetStatus(pData, sStatus);
	} else if ( iRet != XUI_OK ) {
		__xuiTextEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pTextEdit), XUI_TR_FIND_NOT_FOUND));
	}
}

static void __xuiTextEditFindWindowClose(xui_widget pWindow, void* pUser)
{
	xui_widget pTextEdit;

	(void)pWindow;
	pTextEdit = (xui_widget)pUser;
	if ( pTextEdit != NULL ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pTextEdit), pTextEdit);
	}
}

static int __xuiTextEditFindWindowKeyDown(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pTextEdit;
	xui_text_edit_data_t* pData;
	int iKey;

	(void)pWidget;
	pTextEdit = (xui_widget)pUser;
	pData = __xuiTextEditGetData(pTextEdit);
	if ( pEvent == NULL || pData == NULL || pData->pFindWindow == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iType != XUI_EVENT_KEY_DOWN ) return XUI_OK;
	iKey = pEvent->iKey;
	if ( iKey >= 'a' && iKey <= 'z' ) iKey = iKey - 'a' + 'A';
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0u ) {
		if ( iKey == 'F' ) {
			(void)xuiTextEditOpenFind(pTextEdit);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'H' ) {
			(void)xuiTextEditOpenReplace(pTextEdit);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( pEvent->iKey == XUI_KEY_F3 ) {
		__xuiTextEditFindButtonClick(((pEvent->iModifiers & XUI_MOD_SHIFT) != 0u) ? pData->pFindPrevButton : pData->pFindNextButton, pTextEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ENTER ) {
		__xuiTextEditFindButtonClick(((pEvent->iModifiers & XUI_MOD_SHIFT) != 0u) ? pData->pFindPrevButton : pData->pFindNextButton, pTextEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
		(void)xuiWindowSetOpen(pData->pFindWindow, 0);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pTextEdit), pTextEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTextEditFindWindowLayout(xui_widget pTextEdit, int bReplace)
{
	xui_text_edit_data_t* pData;
	xui_rect_t tWindow;
	float fHeight;
	float fOptionY;
	float fStatusY;

	pData = __xuiTextEditGetData(pTextEdit);
	if ( pData == NULL || pData->pFindWindow == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	fHeight = bReplace ? 190.0f : 150.0f;
	fOptionY = bReplace ? 86.0f : 50.0f;
	fStatusY = fOptionY + 32.0f;
	tWindow = xuiWidgetGetRect(pData->pFindWindow);
	if ( tWindow.fW < 560.0f ) tWindow.fW = 560.0f;
	tWindow.fH = fHeight;
	(void)xuiWidgetSetRect(pData->pFindWindow, tWindow);
	if ( pData->pFindInput != NULL ) (void)xuiWidgetSetRect(pData->pFindInput, (xui_rect_t){12.0f, 12.0f, 280.0f, 28.0f});
	if ( pData->pFindPrevButton != NULL ) (void)xuiWidgetSetRect(pData->pFindPrevButton, (xui_rect_t){304.0f, 12.0f, 74.0f, 28.0f});
	if ( pData->pFindNextButton != NULL ) (void)xuiWidgetSetRect(pData->pFindNextButton, (xui_rect_t){386.0f, 12.0f, 74.0f, 28.0f});
	if ( pData->pReplaceInput != NULL ) {
		(void)xuiWidgetSetRect(pData->pReplaceInput, (xui_rect_t){12.0f, 48.0f, 280.0f, 28.0f});
		(void)xuiWidgetSetVisible(pData->pReplaceInput, bReplace);
		(void)xuiWidgetSetEnabled(pData->pReplaceInput, bReplace);
	}
	if ( pData->pReplaceButton != NULL ) {
		(void)xuiWidgetSetRect(pData->pReplaceButton, (xui_rect_t){304.0f, 48.0f, 92.0f, 28.0f});
		(void)xuiWidgetSetVisible(pData->pReplaceButton, bReplace);
		(void)xuiWidgetSetEnabled(pData->pReplaceButton, bReplace);
	}
	if ( pData->pReplaceAllButton != NULL ) {
		(void)xuiWidgetSetRect(pData->pReplaceAllButton, (xui_rect_t){404.0f, 48.0f, 132.0f, 28.0f});
		(void)xuiWidgetSetVisible(pData->pReplaceAllButton, bReplace);
		(void)xuiWidgetSetEnabled(pData->pReplaceAllButton, bReplace);
	}
	if ( pData->pCaseCheck != NULL ) (void)xuiWidgetSetRect(pData->pCaseCheck, (xui_rect_t){12.0f, fOptionY, 116.0f, 24.0f});
	if ( pData->pWordCheck != NULL ) (void)xuiWidgetSetRect(pData->pWordCheck, (xui_rect_t){132.0f, fOptionY, 112.0f, 24.0f});
	if ( pData->pRegexCheck != NULL ) (void)xuiWidgetSetRect(pData->pRegexCheck, (xui_rect_t){248.0f, fOptionY, 86.0f, 24.0f});
	if ( pData->pEscapeCheck != NULL ) (void)xuiWidgetSetRect(pData->pEscapeCheck, (xui_rect_t){338.0f, fOptionY, 88.0f, 24.0f});
	if ( pData->pSelectionCheck != NULL ) (void)xuiWidgetSetRect(pData->pSelectionCheck, (xui_rect_t){430.0f, fOptionY, 106.0f, 24.0f});
	if ( pData->pFindStatus != NULL ) (void)xuiWidgetSetRect(pData->pFindStatus, (xui_rect_t){12.0f, fStatusY, 524.0f, 24.0f});
	return XUI_OK;
}

static int __xuiTextEditCreateFindButton(xui_context pContext, xui_widget* ppButton, xui_font pFont, const char* sText, xui_widget pTextEdit)
{
	xui_button_desc_t tButton;
	int iRet;

	memset(&tButton, 0, sizeof(tButton));
	tButton.iSize = sizeof(tButton);
	tButton.pFont = pFont;
	tButton.sText = sText;
	tButton.fBorderWidth = 1.0f;
	iRet = xuiInternalButtonCreatePart(pTextEdit,ppButton,&tButton);
	if ( iRet == XUI_OK ) iRet = xuiButtonSetClick(*ppButton, __xuiTextEditFindButtonClick, pTextEdit);
	return iRet;
}

static int __xuiTextEditCreateFindCheck(xui_context pContext, xui_widget* ppCheck, xui_font pFont, const char* sText, int bChecked, xui_widget pTextEdit)
{
	xui_checkbox_desc_t tCheck;
	int iRet;

	memset(&tCheck, 0, sizeof(tCheck));
	tCheck.iSize = sizeof(tCheck);
	tCheck.pFont = pFont;
	tCheck.sText = sText;
	tCheck.bChecked = bChecked;
	tCheck.fIndicatorSize = 14.0f;
	tCheck.fGap = 4.0f;
	iRet = xuiInternalCheckBoxCreatePart(pTextEdit,ppCheck,&tCheck);
	if ( iRet == XUI_OK ) iRet = xuiCheckBoxSetChange(*ppCheck, __xuiTextEditFindCheckChange, pTextEdit);
	return iRet;
}

static int __xuiTextEditCreateFindWindow(xui_widget pTextEdit, xui_text_edit_data_t* pData)
{
	xui_context pContext;
	xui_widget pRoot;
	xui_widget pClient;
	xui_window_desc_t tWindow;
	xui_input_desc_t tInput;
	xui_label_desc_t tLabel;
	xui_font pFont;
	int iRet;

	if ( pTextEdit == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pTextEdit);
	pRoot = xuiGetRootWidget(pContext);
	if ( pRoot == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	memset(&tWindow, 0, sizeof(tWindow));
	tWindow.iSize = sizeof(tWindow);
	tWindow.sTitle = xuiTranslate(pContext, XUI_TR_FIND_TITLE);
	tWindow.pFont = pFont;
	tWindow.bClosed = 1;
	tWindow.bTopMost = 1;
	tWindow.bHideCollapse = 1;
	tWindow.bHideMaximize = 1;
	tWindow.bNotResizable = 1;
	tWindow.fTitleBarHeight = 28.0f;
	tWindow.fBorderWidth = 1.0f;
	tWindow.fButtonSize = 18.0f;
	iRet = xuiInternalWindowCreatePart(pTextEdit,&pData->pFindWindow,&tWindow);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWindowSetClose(pData->pFindWindow, __xuiTextEditFindWindowClose, pTextEdit);
	pClient = xuiWindowGetClientWidget(pData->pFindWindow);
	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pClient, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetPadding(pClient, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetGap(pClient, 0.0f);
	(void)xuiWidgetSetEventHandler(pClient, XUI_EVENT_KEY_DOWN, __xuiTextEditFindWindowKeyDown, pTextEdit);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.pFont = pFont;
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER);
	tInput.fBorderWidth = 1.0f;
	iRet = xuiInternalInputCreatePart(pTextEdit,&pData->pFindInput,&tInput);
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER);
	if ( iRet == XUI_OK ) iRet = xuiInternalInputCreatePart(pTextEdit,&pData->pReplaceInput,&tInput);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindButton(pContext, &pData->pFindPrevButton, pFont, xuiTranslate(pContext, XUI_TR_FIND_PREVIOUS), pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindButton(pContext, &pData->pFindNextButton, pFont, xuiTranslate(pContext, XUI_TR_FIND_NEXT), pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindButton(pContext, &pData->pReplaceButton, pFont, xuiTranslate(pContext, XUI_TR_REPLACE_CURRENT), pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindButton(pContext, &pData->pReplaceAllButton, pFont, xuiTranslate(pContext, XUI_TR_REPLACE_ALL), pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindCheck(pContext, &pData->pCaseCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_CASE), (pData->iFindFlags & XUI_FIND_CASE_SENSITIVE) != 0, pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindCheck(pContext, &pData->pWordCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_WORD), (pData->iFindFlags & XUI_FIND_WHOLE_WORD) != 0, pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindCheck(pContext, &pData->pRegexCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_REGEX), (pData->iFindFlags & XUI_FIND_REGEX) != 0, pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindCheck(pContext, &pData->pEscapeCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_ESCAPE), (pData->iFindFlags & XUI_FIND_ESCAPE) != 0, pTextEdit);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditCreateFindCheck(pContext, &pData->pSelectionCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_SELECTION), (pData->iFindFlags & XUI_FIND_SELECTION) != 0, pTextEdit);
	memset(&tLabel, 0, sizeof(tLabel));
	tLabel.iSize = sizeof(tLabel);
	tLabel.pFont = pFont;
	tLabel.sText = "";
	tLabel.iTextColor = XUI_COLOR_RGBA(90, 105, 124, 255);
	tLabel.iTextFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	if ( iRet == XUI_OK ) iRet = xuiInternalLabelCreatePart(pTextEdit,&pData->pFindStatus,&tLabel);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiInputSetChange(pData->pFindInput, __xuiTextEditFindInputChange, pTextEdit);
	(void)xuiInputSetChange(pData->pReplaceInput, __xuiTextEditFindInputChange, pTextEdit);
	iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pFindInput,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pReplaceInput,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pFindPrevButton,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pFindNextButton,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pReplaceButton,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pReplaceAllButton,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pCaseCheck,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pWordCheck,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pRegexCheck,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pEscapeCheck,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pSelectionCheck,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,xuiWindowGetClientWidget(pData->pFindWindow),pData->pFindStatus,NULL);
	if ( iRet == XUI_OK ) iRet = xuiInternalWidgetInsertPart(pTextEdit,pRoot,pData->pFindWindow,NULL);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pData->pFindWindow, (xui_rect_t){32.0f, 32.0f, 560.0f, 150.0f});
	__xuiTextEditFindWindowApplyLanguage(pTextEdit, pData);
	return __xuiTextEditFindWindowLayout(pTextEdit, 0);
}

static int __xuiTextEditOpenFindWindow(xui_widget pWidget, int bReplace)
{
	xui_text_edit_data_t* pData;
	xui_rect_t tOwner;
	xui_rect_t tWindow;
	int iStart;
	int iEnd;
	int iLen;
	int iRet;

	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFindWindow == NULL ) {
		iRet = __xuiTextEditCreateFindWindow(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bFindWindowReplace = bReplace ? 1 : 0;
	__xuiTextEditSelectionRange(pData, &iStart, &iEnd);
	pData->iFindRangeStart = 0;
	pData->iFindRangeEnd = 0;
	if ( iEnd > iStart ) {
		pData->iFindRangeStart = iStart;
		pData->iFindRangeEnd = iEnd;
		iLen = iEnd - iStart;
		if ( iLen > 0 && iLen < 256 && pData->sText != NULL ) {
			iRet = __xuiTextEditTextReserve(&pData->sScratch, &pData->iScratchCapacity, iLen + 1);
			if ( iRet != XUI_OK ) return iRet;
			memcpy(pData->sScratch, pData->sText + iStart, (size_t)iLen);
			pData->sScratch[iLen] = '\0';
			(void)xuiInputSetText(pData->pFindInput, pData->sScratch);
		}
	} else if ( pData->sFindPattern != NULL && pData->sFindPattern[0] != '\0' ) {
		(void)xuiInputSetText(pData->pFindInput, pData->sFindPattern);
	}
	if ( pData->sFindReplacement != NULL ) {
		(void)xuiInputSetText(pData->pReplaceInput, pData->sFindReplacement);
	}
	if ( pData->pSelectionCheck != NULL ) {
		(void)xuiCheckBoxSetChecked(pData->pSelectionCheck, (iEnd > iStart && (pData->iFindFlags & XUI_FIND_SELECTION) != 0));
	}
	__xuiTextEditFindWindowApplyLanguage(pWidget, pData);
	(void)__xuiTextEditFindWindowLayout(pWidget, bReplace);
	tOwner = xuiWidgetGetWorldRect(pWidget);
	tWindow = xuiWidgetGetRect(pData->pFindWindow);
	tWindow.fX = tOwner.fX + 16.0f;
	tWindow.fY = tOwner.fY + 16.0f;
	(void)xuiWidgetSetRect(pData->pFindWindow, tWindow);
	(void)xuiWindowSetOpen(pData->pFindWindow, 1);
	(void)xuiWindowBringToFront(pData->pFindWindow);
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pFindInput);
	__xuiTextEditFindWindowRefresh(pWidget);
	return XUI_OK;
}

XUI_API int xuiTextEditOpenFind(xui_widget pWidget)
{
	return __xuiTextEditOpenFindWindow(pWidget, 0);
}

XUI_API int xuiTextEditOpenReplace(xui_widget pWidget)
{
	return __xuiTextEditOpenFindWindow(pWidget, 1);
}

XUI_API xui_widget xuiTextEditGetFindWindow(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return NULL;
	return pData->pFindWindow;
}

XUI_API int xuiTextEditFindNext(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiTextEditFindMove(pWidget, pOptions, 0);
}

XUI_API int xuiTextEditFindPrevious(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiTextEditFindMove(pWidget, pOptions, 1);
}

XUI_API int xuiTextEditReplaceCurrent(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	xui_text_edit_data_t* pData;
	xui_find_result_t tResult;
	const char* sPattern;
	const char* sReplacement;
	char* sOutput;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iSelStart;
	int iSelEnd;
	int iReplaceCount;
	int iOldLength;
	int iOutputLength;
	int iReplacementLength;
	int iNewRangeEnd;
	int bChanged;
	int bFindRefreshed;
	int iRet;

	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	sOutput = NULL;
	bChanged = 0;
	bFindRefreshed = 0;
	iRet = __xuiTextEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTextEditSelectionRange(pData, &iSelStart, &iSelEnd);
	if ( iSelEnd <= iSelStart ) {
		iRet = __xuiTextEditFindMove(pWidget, pOptions, 0);
		if ( iRet != XUI_OK ) return iRet;
		__xuiTextEditSelectionRange(pData, &iSelStart, &iSelEnd);
	}
	memset(&tResult, 0, sizeof(tResult));
	iRet = xuiFindText((pData->sText != NULL) ? pData->sText : "", -1, sPattern, iSelStart, iSelStart, iSelEnd, iFlags & ~XUI_FIND_WRAP, &tResult, NULL, 0);
	if ( iRet != XUI_OK || tResult.iStart != iSelStart || tResult.iEnd != iSelEnd ) return XUI_ERROR_UNSUPPORTED;
	iOldLength = pData->iTextLength;
	iRet = xuiFindReplaceAllText((pData->sText != NULL) ? pData->sText : "", iOldLength, sPattern, sReplacement, iSelStart, iSelEnd, iFlags, &sOutput, &iOutputLength, &iReplaceCount, NULL, 0);
	if ( iRet != XUI_OK ) goto cleanup;
	if ( iReplaceCount <= 0 ) {
		iRet = XUI_ERROR_UNSUPPORTED;
		goto cleanup;
	}
	iReplacementLength = iOutputLength - (iOldLength - (iSelEnd - iSelStart));
	if ( iReplacementLength < 0 ) {
		iRet = XUI_ERROR_INVALID_STATE;
		goto cleanup;
	}
	iRet = __xuiTextEditFindRangeAfterReplace(iOldLength, iOutputLength,
		iRangeStart, iRangeEnd, &iNewRangeEnd);
	if ( iRet != XUI_OK ) goto cleanup;
	/* Capture caller-owned strings before replacing the document buffer. */
	iRet = __xuiTextEditStoreFindState(pData, sPattern, sReplacement,
		iFlags, iRangeStart, iNewRangeEnd);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiTextEditSetTextInternal(pWidget, pData, sOutput, 0, 1);
	if ( iRet == XUI_OK ) bChanged = 1;
	if ( iRet == XUI_OK ) {
		pData->iFindFlags = iFlags;
		pData->iFindRangeStart = ((iFlags & XUI_FIND_SELECTION) != 0) ?
			iRangeStart : 0;
		pData->iFindRangeEnd = ((iFlags & XUI_FIND_SELECTION) != 0) ?
			iNewRangeEnd : 0;
	}
	if ( iRet == XUI_OK ) iRet = __xuiTextEditSetSelectionData(pData, iSelStart, iSelStart + iReplacementLength);
	if ( iRet == XUI_OK ) iRet = __xuiTextEditSyncCursor(pWidget, pData);
	if ( iRet == XUI_OK ) {
		tResult.iStart = iSelStart;
		tResult.iEnd = iSelStart + iReplacementLength;
		iRet = __xuiTextEditUpdateFindResults(pWidget, pData,
			pData->sFindPattern, pData->iFindFlags, iRangeStart,
			iNewRangeEnd, &tResult);
		if ( iRet == XUI_OK ) bFindRefreshed = 1;
	}

cleanup:
	xuiFindFreeText(sOutput);
	if ( bChanged ) __xuiTextEditNotifyChangeEx(pWidget, pData,
		!bFindRefreshed);
	return iRet;
}

XUI_API int xuiTextEditReplaceAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pReplaceCount)
{
	xui_text_edit_data_t* pData;
	const char* sPattern;
	const char* sReplacement;
	char* sOutput;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iOldLength;
	int iOutputLength;
	int iNewRangeEnd;
	int iCount;
	int bChanged;
	int bFindRefreshed;
	int iRet;

	pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	sOutput = NULL;
	bChanged = 0;
	bFindRefreshed = 0;
	iRet = __xuiTextEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iOldLength = pData->iTextLength;
	iRet = xuiFindReplaceAllText((pData->sText != NULL) ? pData->sText : "", iOldLength, sPattern, sReplacement, iRangeStart, iRangeEnd, iFlags, &sOutput, &iOutputLength, &iCount, NULL, 0);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiTextEditFindRangeAfterReplace(iOldLength, iOutputLength,
		iRangeStart, iRangeEnd, &iNewRangeEnd);
	if ( iRet != XUI_OK ) goto cleanup;
	/* Capture caller-owned strings before replacing the document buffer. */
	iRet = __xuiTextEditStoreFindState(pData, sPattern, sReplacement,
		iFlags, iRangeStart, iNewRangeEnd);
	if ( iRet != XUI_OK ) goto cleanup;
	if ( iCount > 0 ) {
		iRet = __xuiTextEditSetTextInternal(pWidget, pData, sOutput, 0, 1);
		if ( iRet == XUI_OK ) bChanged = 1;
		if ( iRet == XUI_OK ) {
			pData->iFindFlags = iFlags;
			pData->iFindRangeStart = ((iFlags & XUI_FIND_SELECTION) != 0) ?
				iRangeStart : 0;
			pData->iFindRangeEnd = ((iFlags & XUI_FIND_SELECTION) != 0) ?
				iNewRangeEnd : 0;
		}
		if ( iRet == XUI_OK ) iRet = __xuiTextEditSetSelectionData(pData, iRangeStart, iRangeStart);
		if ( iRet == XUI_OK ) iRet = __xuiTextEditSyncCursor(pWidget, pData);
	}
	if ( iRet == XUI_OK && iCount > 0 ) {
		iRet = __xuiTextEditUpdateFindResults(pWidget, pData,
			pData->sFindPattern, pData->iFindFlags, iRangeStart,
			iNewRangeEnd, NULL);
		if ( iRet == XUI_OK ) bFindRefreshed = 1;
	} else if ( iRet == XUI_OK ) {
		__xuiTextEditClearFindResults(pData);
		iRet = __xuiTextEditInvalidatePaint(pWidget);
	}
	if ( iRet == XUI_OK && pReplaceCount != NULL ) *pReplaceCount = iCount;

cleanup:
	xuiFindFreeText(sOutput);
	if ( bChanged ) __xuiTextEditNotifyChangeEx(pWidget, pData,
		!bFindRefreshed);
	return iRet;
}

XUI_API int xuiTextEditClearFind(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTextEditClearFindResults(pData);
	return __xuiTextEditInvalidatePaint(pWidget);
}

static int __xuiTextEditApplyHistoryStateImpl(xui_widget pWidget,
	xui_text_edit_data_t* pData, const xui_text_edit_history_t* pState, int bRedo)
{
	const char* sInsert;
	int iRemoveLength;
	int iInsertLength;
	int iPatchLine;
	int iOldEnd;
	int iNewLength;
	int bPatch;
	int iRet;
	int iLen;

	if ( pWidget == NULL || pData == NULL || pState == NULL || pData->sText == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRemoveLength = bRedo ? pState->iRemovedLength : pState->iInsertedLength;
	iInsertLength = bRedo ? pState->iInsertedLength : pState->iRemovedLength;
	sInsert = bRedo ? pState->sInserted : pState->sRemoved;
	if ( pState->iStart < 0 || iRemoveLength < 0 || iInsertLength < 0 ||
	     pState->iStart > pData->iTextLength ||
	     iRemoveLength > pData->iTextLength - pState->iStart ||
	     (iInsertLength > 0 && sInsert == NULL) ) return XUI_ERROR_INVALID_STATE;
	if ( iInsertLength > INT_MAX - 1 - (pData->iTextLength - iRemoveLength) )
		return XUI_ERROR_INVALID_STATE;
	iOldEnd = pState->iStart + iRemoveLength;
	bPatch = __xuiTextEditDirectPatchPrepare(pData, pState->iStart,
		iOldEnd, sInsert, iInsertLength, &iPatchLine);
	iNewLength = pData->iTextLength - iRemoveLength + iInsertLength;
	iRet = __xuiTextEditTextReserve(&pData->sText, &pData->iTextCapacity,
		iNewLength + 1);
	if ( iRet != XUI_OK ) return iRet;
    iRet=__xuiTextEditContentPrepare(pData,bPatch ? iPatchLine:-1,pState->iStart,iOldEnd,pState->iStart+iInsertLength);
    if(iRet!=XUI_OK)return iRet;
	memmove(pData->sText + pState->iStart + iInsertLength,
		pData->sText + iOldEnd,
		(size_t)(pData->iTextLength - iOldEnd + 1));
	if ( iInsertLength > 0 ) memcpy(pData->sText + pState->iStart,
		sInsert, (size_t)iInsertLength);
	pData->iTextLength = iNewLength;
	if ( !bPatch || __xuiTextEditDirectPatchApply(pWidget, pData, iPatchLine,
		iInsertLength - iRemoveLength, pState->iStart, iOldEnd,
		pState->iStart + iInsertLength) != XUI_OK ) {
		bPatch = 0;
		__xuiTextEditMarkLinesDirty(pData);
	}
	iLen = pData->iTextLength;
	pData->iCursor = __xuiTextEditUtf8Clamp(pData->sText, iLen,
		bRedo ? pState->iAfterCursor : pState->iBeforeCursor);
	pData->iSelectStart = __xuiTextEditUtf8Clamp(pData->sText, iLen,
		bRedo ? pState->iAfterSelectStart : pState->iBeforeSelectStart);
	pData->iSelectEnd = __xuiTextEditUtf8Clamp(pData->sText, iLen,
		bRedo ? pState->iAfterSelectEnd : pState->iBeforeSelectEnd);
	pData->iAnchor = pData->iSelectStart;
	pData->fScrollX = bRedo ? pState->fAfterScrollX : pState->fBeforeScrollX;
	pData->fScrollY = bRedo ? pState->fAfterScrollY : pState->fBeforeScrollY;
	pData->bUndoInsertGroup = 0;
	pData->iUndoInsertEnd = 0;
	(void)__xuiTextEditEnsureCursorVisible(pWidget, pData);
	return bPatch ? __xuiTextEditInvalidatePaint(pWidget) :
		__xuiTextEditInvalidateGeometry(pWidget);
}
static int __xuiTextEditApplyHistoryState(xui_widget pWidget,xui_text_edit_data_t* pData,const xui_text_edit_history_t* pState,int bRedo)
{

    if(!pData)return XUI_ERROR_INVALID_ARGUMENT;
    if(pData->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    pData->iCacheActivity=1;
    status=__xuiTextEditApplyHistoryStateImpl(pWidget,pData,pState,bRedo);
    pData->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditUndo(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	xui_text_edit_history_t tUndo;
	int bApplied;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iUndoCount <= 0 ) return XUI_OK;
	tUndo = pData->arrUndo[pData->iUndoCount - 1];
	memset(&pData->arrUndo[pData->iUndoCount - 1], 0, sizeof(pData->arrUndo[0]));
	pData->iUndoCount--;
	iRet = __xuiTextEditApplyHistoryState(pWidget, pData, &tUndo, 0);
	bApplied = (iRet == XUI_OK);
	if ( iRet == XUI_OK ) {
		iRet = __xuiTextEditHistoryPush(pData->arrRedo, &pData->iRedoCount, &tUndo);
		if ( iRet == XUI_OK ) memset(&tUndo, 0, sizeof(tUndo));
	} else {
		(void)__xuiTextEditHistoryPush(pData->arrUndo, &pData->iUndoCount, &tUndo);
		memset(&tUndo, 0, sizeof(tUndo));
	}
	__xuiTextEditHistoryStateFree(&tUndo);
	if ( bApplied ) __xuiTextEditNotifyChange(pWidget, pData);
	return iRet;
}

XUI_API int xuiTextEditRedo(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	xui_text_edit_history_t tRedo;
	int bApplied;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iRedoCount <= 0 ) return XUI_OK;
	tRedo = pData->arrRedo[pData->iRedoCount - 1];
	memset(&pData->arrRedo[pData->iRedoCount - 1], 0, sizeof(pData->arrRedo[0]));
	pData->iRedoCount--;
	iRet = __xuiTextEditApplyHistoryState(pWidget, pData, &tRedo, 1);
	bApplied = (iRet == XUI_OK);
	if ( iRet == XUI_OK ) {
		iRet = __xuiTextEditHistoryPush(pData->arrUndo, &pData->iUndoCount, &tRedo);
		if ( iRet == XUI_OK ) memset(&tRedo, 0, sizeof(tRedo));
	} else {
		(void)__xuiTextEditHistoryPush(pData->arrRedo, &pData->iRedoCount, &tRedo);
		memset(&tRedo, 0, sizeof(tRedo));
	}
	__xuiTextEditHistoryStateFree(&tRedo);
	if ( bApplied ) __xuiTextEditNotifyChange(pWidget, pData);
	return iRet;
}

XUI_API int xuiTextEditCanUndo(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? (pData->iUndoCount > 0) : 0;
}

XUI_API int xuiTextEditCanRedo(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? (pData->iRedoCount > 0) : 0;
}

XUI_API xui_scroll_model_t* xuiTextEditGetScrollModel(xui_widget widget)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(widget);
    if(!data || data->iCacheActivity)return NULL;
    xui_call_scope_t scope={0};xui_context context=widget->pContext;
    if(xuiCallScopeBegin(context,&scope)!=XUI_OK)return NULL;
    data->iCacheActivity=2;int status=__xuiTextEditUpdateScrollModel(widget,data);
    xui_scroll_model_t* result=&data->tScrollModel;
    bool closing=widget->bDestroyPending || context->bCloseRequested || context->bDestroyPending;
    data->iCacheActivity=0;xerror* error=xrtTakeError();
    return xuiInternalCallScopeEndRestored(&scope,status,error)==XUI_OK && !closing ? result:NULL;
}

XUI_API xui_widget xuiTextEditGetHScrollBarWidget(xui_widget widget)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(widget);
    if(!data || data->iCacheActivity)return NULL;
    xui_call_scope_t scope={0};xui_context context=widget->pContext;
    if(xuiCallScopeBegin(context,&scope)!=XUI_OK)return NULL;
    data->iCacheActivity=2;int status=__xuiTextEditUpdateScrollModel(widget,data);
    xui_widget result=data->pHScrollBar;
    bool closing=widget->bDestroyPending || context->bCloseRequested || context->bDestroyPending;
    data->iCacheActivity=0;xerror* error=xrtTakeError();
    return xuiInternalCallScopeEndRestored(&scope,status,error)==XUI_OK && !closing ? result:NULL;
}

XUI_API xui_widget xuiTextEditGetVScrollBarWidget(xui_widget widget)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(widget);
    if(!data || data->iCacheActivity)return NULL;
    xui_call_scope_t scope={0};xui_context context=widget->pContext;
    if(xuiCallScopeBegin(context,&scope)!=XUI_OK)return NULL;
    data->iCacheActivity=2;int status=__xuiTextEditUpdateScrollModel(widget,data);
    xui_widget result=data->pVScrollBar;
    bool closing=widget->bDestroyPending || context->bCloseRequested || context->bDestroyPending;
    data->iCacheActivity=0;xerror* error=xrtTakeError();
    return xuiInternalCallScopeEndRestored(&scope,status,error)==XUI_OK && !closing ? result:NULL;
}

static int xuiTextEditSetScrollImpl(xui_widget pWidget, float fScrollX, float fScrollY)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	float fOldScrollX;
	float fOldScrollY;
	int iRet;
	if ( pData == NULL || !isfinite(fScrollX) || !isfinite(fScrollY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fScrollX < 0.0f ) fScrollX = 0.0f;
	if ( fScrollY < 0.0f ) fScrollY = 0.0f;
	fOldScrollX = pData->fScrollX;
	fOldScrollY = pData->fScrollY;
	pData->fScrollX = fScrollX;
	pData->fScrollY = fScrollY;
	iRet = __xuiTextEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) {
		pData->fScrollX = fOldScrollX;
		pData->fScrollY = fOldScrollY;
		(void)xuiScrollModelSetOffset(&pData->tScrollModel, fOldScrollX, fOldScrollY);
		return iRet;
	}
	iRet = __xuiTextEditUpdateCursorRect(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTextEditInvalidatePaint(pWidget);
}
XUI_API int xuiTextEditSetScroll(xui_widget pWidget,float fScrollX,float fScrollY)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetScrollImpl(pWidget,fScrollX,fScrollY);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetScroll(xui_widget pWidget, float* pScrollX, float* pScrollY)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pScrollX != NULL ) *pScrollX = pData->fScrollX;
	if ( pScrollY != NULL ) *pScrollY = pData->fScrollY;
	return XUI_OK;
}

XUI_API int xuiTextEditScrollBy(xui_widget pWidget, float fDeltaX, float fDeltaY)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	float fScrollX;
	float fScrollY;
	if ( pData == NULL || !isfinite(fDeltaX) || !isfinite(fDeltaY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fScrollX = pData->fScrollX + fDeltaX;
	fScrollY = pData->fScrollY + fDeltaY;
	if ( !isfinite(fScrollX) || !isfinite(fScrollY) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTextEditSetScroll(pWidget, fScrollX, fScrollY);
}

XUI_API int xuiTextEditSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iText, uint32_t iBorder, uint32_t iFocus)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iBackground;
	pData->iTextColor = iText;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iFocus;
	pData->iFocusBorderColor = iFocus;
	return __xuiTextEditInvalidatePaint(pWidget);
}

XUI_API int xuiTextEditGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pText, uint32_t* pBorder, uint32_t* pFocus)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiTextEditSetExtendedColors(xui_widget pWidget, uint32_t iPlaceholder, uint32_t iDisabledText, uint32_t iHoverBackground, uint32_t iDisabledBackground, uint32_t iHoverBorder, uint32_t iSelection, uint32_t iCursor, uint32_t iFindResult, uint32_t iFindActive)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPlaceholderColor = iPlaceholder;
	pData->iDisabledTextColor = iDisabledText;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iSelectionColor = iSelection;
	pData->iCursorColor = iCursor;
	pData->iFindResultColor = iFindResult;
	pData->iFindActiveColor = iFindActive;
	return __xuiTextEditInvalidatePaint(pWidget);
}

XUI_API int xuiTextEditGetExtendedColors(xui_widget pWidget, uint32_t* pPlaceholder, uint32_t* pDisabledText, uint32_t* pHoverBackground, uint32_t* pDisabledBackground, uint32_t* pHoverBorder, uint32_t* pSelection, uint32_t* pCursor, uint32_t* pFindResult, uint32_t* pFindActive)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPlaceholder != NULL ) *pPlaceholder = pData->iPlaceholderColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pSelection != NULL ) *pSelection = pData->iSelectionColor;
	if ( pCursor != NULL ) *pCursor = pData->iCursorColor;
	if ( pFindResult != NULL ) *pFindResult = pData->iFindResultColor;
	if ( pFindActive != NULL ) *pFindActive = pData->iFindActiveColor;
	return XUI_OK;
}

static int xuiTextEditSetVisualMetricsImpl(xui_widget pWidget, float fBorderWidth, float fLineGap)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL || !isfinite(fBorderWidth) || !isfinite(fLineGap) ||
	     fBorderWidth < 0.0f || fLineGap < 0.0f ||
	     fBorderWidth > XUI_LAYOUT_UNBOUNDED || fLineGap > XUI_LAYOUT_UNBOUNDED ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->fBorderWidth == fBorderWidth && pData->fLineGap == fLineGap ) {
		return XUI_OK;
	}
    int cacheStatus=__xuiTextEditContentPrepare(pData,-1,0,0,0);
    if(cacheStatus!=XUI_OK)return cacheStatus;
	pData->fBorderWidth = fBorderWidth;
	pData->fLineGap = fLineGap;
	return __xuiTextEditInvalidateText(pWidget);
}
XUI_API int xuiTextEditSetVisualMetrics(xui_widget pWidget,float fBorderWidth,float fLineGap)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(pWidget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->iCacheActivity)return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(pWidget->pContext,&scope);
    if(status!=XUI_OK)return status;
    data->iCacheActivity=1;status=xuiTextEditSetVisualMetricsImpl(pWidget,fBorderWidth,fLineGap);data->iCacheActivity=0;
    xerror* error=xrtTakeError();return xuiInternalCallScopeEndRestored(&scope,status,error);
}


XUI_API int xuiTextEditGetVisualMetrics(xui_widget pWidget, float* pBorderWidth, float* pLineGap)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pLineGap != NULL ) *pLineGap = pData->fLineGap;
	return XUI_OK;
}

XUI_API int xuiTextEditSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	char* sNew;
	if ( (pData == NULL) || (iCommand < 0) || (iCommand >= XUI_INPUT_MENU_COUNT) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (sTitle == NULL) || (sTitle[0] == '\0') ) {
		if ( pData->arrMenuTitle[iCommand] != NULL ) {
			xrtFree(pData->arrMenuTitle[iCommand]);
			pData->arrMenuTitle[iCommand] = NULL;
		}
		return XUI_OK;
	}
	sNew = __xuiTextEditStringDuplicate(sTitle);
	if ( sNew == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pData->arrMenuTitle[iCommand] != NULL ) xrtFree(pData->arrMenuTitle[iCommand]);
	pData->arrMenuTitle[iCommand] = sNew;
	return XUI_OK;
}

XUI_API const char* xuiTextEditGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	if ( (pData == NULL) || (iCommand < 0) || (iCommand >= XUI_INPUT_MENU_COUNT) ) {
		return "";
	}
	return (pData->arrMenuTitle[iCommand] != NULL) ? pData->arrMenuTitle[iCommand] : xuiTranslate(xuiWidgetGetContext(pWidget), __xuiTextEditMenuTextId(iCommand));
}

XUI_API int xuiTextEditOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (pData->pMenu == NULL) || !isfinite(fX) || !isfinite(fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTextEditUpdateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiMenuSetSelect(pData->pMenu, __xuiTextEditMenuSelect, pWidget);
	return xuiMenuOpenAt(pData->pMenu, pWidget, fX, fY);
}

XUI_API xui_widget xuiTextEditGetMenuWidget(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->pMenu : NULL;
}

XUI_API xui_rect_t xuiTextEditGetTextRect(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiTextEditGetCursorRect(xui_widget widget)
{
    xui_rect_t result={0};xui_text_edit_data_t* data=__xuiTextEditGetData(widget);
    if(!data || data->iCacheActivity)return result;
    xui_call_scope_t scope={0};if(xuiCallScopeBegin(widget->pContext,&scope)!=XUI_OK)return result;
    data->iCacheActivity=2;int status=__xuiTextEditUpdateCursorRect(widget,data);
    if(status==XUI_OK)result=data->tCursorRect;
    data->iCacheActivity=0;xerror* error=xrtTakeError();
    return xuiInternalCallScopeEndRestored(&scope,status,error)==XUI_OK ? result:(xui_rect_t){0};
}

XUI_API int xuiTextEditGetLineCount(xui_widget widget)
{
    xui_text_edit_data_t* data=__xuiTextEditGetData(widget);
    if(!data || data->iCacheActivity)return 0;
    xui_call_scope_t scope={0};if(xuiCallScopeBegin(widget->pContext,&scope)!=XUI_OK)return 0;
    data->iCacheActivity=2;int status=__xuiTextEditUpdateCursorRect(widget,data),count=data->iLineCount;
    data->iCacheActivity=0;xerror* error=xrtTakeError();
    return xuiInternalCallScopeEndRestored(&scope,status,error)==XUI_OK ? count:0;
}

XUI_API uint32_t xuiTextEditGetState(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	uint32_t iState;
	if ( pData == NULL ) return 0u;
	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( pData->bReadonly ) {
		iState |= XUI_TEXT_EDIT_STATE_READONLY;
	}
	return iState;
}

XUI_API int xuiTextEditGetChangeCount(xui_widget pWidget)
{
	xui_text_edit_data_t* pData = __xuiTextEditGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

#endif
