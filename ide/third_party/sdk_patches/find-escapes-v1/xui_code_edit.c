#include "xui_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct xui_code_completion_owned_item_t {
	xui_code_completion_item_t tItem;
	char* sLabel;
	char* sInsertText;
	char* sDetail;
	char* sDocumentation;
	char* sFilterText;
	char* sSortText;
	char* sCommitCharacters;
	int iMatchScore;
} xui_code_completion_owned_item_t;

typedef struct xui_code_edit_text_segment_t {
	int iStart;
	int iEnd;
	double fX;
	xui_text_shape_t tShape;
} xui_code_edit_text_segment_t;

typedef struct xui_code_edit_caret_stop_t {
	int iOffset;
	double fX;
} xui_code_edit_caret_stop_t;

typedef struct xui_code_edit_line_layout_t {
	uint32_t iDocumentVersion;
	xui_font pFont;
	int iTabColumns;
	char* sText;
	int iTextSize;
	xui_code_edit_text_segment_t* pSegments;
	int iSegmentCount;
	int iSegmentCapacity;
	xui_code_edit_caret_stop_t* pStops;
	int iStopCount;
	int iStopCapacity;
	double fWidth;
	int* pWrapOffsets;
	int iWrapOffsetCount;
	int iWrapOffsetCapacity;
	float fWrapWidth;
	uint32_t iUseStamp;
    xui_text_shape_cursor pCursor;
    xui_text_shape_cursor_free_proc onCursorFree;
    void* pCursorUser;
} xui_code_edit_line_layout_t;

typedef struct xui_code_edit_column_point_t {
    int iOffset;
    int iColumn;
} xui_code_edit_column_point_t;

typedef struct xui_code_edit_data_t {
	xui_code_document pDocument;
	xui_code_selection_model pSelection;
	xui_code_theme pTheme;
	xui_code_fold_state pFoldState;
	xui_code_annotation_store pAnnotations;
	xui_code_token_buffer pTokenBuffer;
	xui_code_provider_set pProviders;
	xui_code_margin_model pMargins;
	xui_code_command_map pCommandMap;
	xui_code_language_registry pLanguages;
	xui_code_find_scope pFindScope;
	char* sFindPattern;
	int iFindPatternCapacity;
	char* sFindReplacement;
	int iFindReplacementCapacity;
	xui_find_result_t* pFindResults;
	int iFindResultCount;
	int iFindResultCapacity;
	int iFindActiveIndex;
	xui_find_result_t tFindActive;
	uint32_t iFindFlags;
	int iFindRangeStart;
	int iFindRangeEnd;
	int bFindResultScopeMode;
	char sFindResultPosition[32];
	char sFindResultPreview[160];
	xui_widget pFindWindow;
	xui_widget pFindInput;
	xui_widget pReplaceInput;
	xui_widget pFindStatus;
	xui_widget pFindPrevButton;
	xui_widget pFindNextButton;
	xui_widget pReplaceButton;
	xui_widget pReplaceAllButton;
	xui_widget pFindAllButton;
	xui_widget pCaseCheck;
	xui_widget pWordCheck;
	xui_widget pRegexCheck;
	xui_widget pEscapeCheck;
	xui_widget pSelectionCheck;
	xui_widget pScopeCheck;
	xui_widget pFindResultList;
	int bFindWindowReplace;
	uint32_t iFindLanguageRevision;
	xui_widget pMenu;
	char* arrMenuTitle[12];
	xui_widget pHScrollBar;
	xui_widget pVScrollBar;
	xui_font pFont;
    xui_text_shape_visit_proc onShapeVisit;
    void* pShapeVisitUser;
    xui_text_shape_cursor_visit_proc onCursorVisit;
    xui_text_shape_cursor_free_proc onCursorFree;
    void* pCursorVisitUser;
	char sLanguage[32];
	char sError[160];
	int bReadonly;
	int bWordWrap;
	int iTabColumns;
	int iIndentColumns;
	int iEolMode;
	int iFlags;
	uint32_t iDisplayOptions;
	int bDragging;
	int iDragAnchor;
	xui_code_diagnostic_hover_proc onDiagnosticHover;
	void* pDiagnosticHoverUser;
	int iHoveredDiagnostic;
	xui_rect_t tHoveredDiagnosticRect;
	int bImeComposing;
	int iImeAnchorStart;
	int iImeAnchorEnd;
	int iImeCursor;
	int iImeSelectionStart;
	int iImeSelectionEnd;
	char* sImeComposition;
	int iImeCompositionCapacity;
	char* sInlineCompletion;
	int iInlineCompletionCapacity;
	int iInlineCompletionOffset;
	int iInlineCompletionLine;
	int iInlineCompletionExtraRows;
	uint32_t iInlineCompletionVersion;
	xui_code_completion_owned_item_t* pCompletionItems;
	int* pCompletionVisible;
	const char** arrCompletionLabels;
	int iCompletionSourceCount;
	int iCompletionCount;
	int iCompletionCapacity;
	int iCompletionSelected;
	int iCompletionStartOffset;
	uint32_t iCompletionDocumentVersion;
	xui_widget pCompletionPopup;
	xui_widget pCompletionList;
	int bCompletionPending;
	int bCompletionAutoShow;
	int iCompletionMinPrefix;
	int iCompletionMaxItems;
	float fCompletionDelay;
	float fCompletionElapsed;
	xui_code_input_proc onInput;
	void* pInputUser;
	int bInputDispatch;
	xui_widget pAssistPopup;
	xui_widget pAssistContent;
	uint32_t arrAssistColors[3];
	int iAssistKind;
	char* sAssistLabel;
	int iAssistLabelCapacity;
	char* sAssistDocumentation;
	int iAssistDocumentationCapacity;
	xui_code_range_t tAssistActiveRange;
	xui_code_placeholder_t* pPlaceholders;
	int iPlaceholderCount;
	int iPlaceholderActive;
	xui_scroll_model_t tScrollModel;
	int bShowHScrollBar;
	int bShowVScrollBar;
	int bSyncingScrollBars;
	xui_rect_t tScrollViewportRect;
	int bCaretFollowViewport;
	xui_rect_t tCaretRequestedViewport;
	xui_rect_t tHScrollBarRect;
	xui_rect_t tVScrollBarRect;
	xui_rect_t tMinimapRect;
	float fMinimapWidth;
	int bMinimapDragging;
	float fScrollX;
	float fScrollY;
	uint32_t iMaxLineLengthVersion;
	int iMaxLineLengthTabColumns;
	int iCachedMaxLineLength;
	xui_code_edit_line_layout_t* pLineLayouts;
	int iLineLayoutCount;
	int iLineLayoutCapacity;
	uint32_t iLineLayoutUseStamp;
    uint32_t iLineLayoutVersion;
	int* pWrapRows;
	int* pWrapTree;
	unsigned char* pWrapMeasured;
	int iWrapIndexCount;
	uint32_t iWrapDocumentVersion;
	uint32_t iWrapFoldSignature;
	float fWrapLayoutWidth;
	xui_font pWrapLayoutFont;
	int iWrapLayoutTabColumns;
	char* sReadBuffer;
	int iReadBufferCapacity;
	int iReadRangeStart;
	int iReadRangeEnd;
	uint32_t iReadVersion;
    int bReadRangeValid;
    xui_code_edit_column_point_t* pColumnPoints;
    int iColumnPointCount;
    int iColumnPointCapacity;
    int bColumnIndexAllocationFailed;
    xui_code_document pColumnIndexDocument;
    uint32_t iColumnIndexVersion;
    int iColumnIndexReadStart;
    int iColumnIndexTabColumns;
	/* Two viewport results share one owned-buffer validity context. */
	int bVisibleRangeCached;
	xui_code_document pVisibleRangeDocument;
	uint32_t iVisibleRangeVersion;
	int iVisibleRangeReadStart;
	int iVisibleRangeReadEnd;
	int iVisibleRangeFirstColumn;
	int iVisibleRangeLastColumn;
	int iVisibleRangeTabColumns;
	int iVisibleRangeStart;
	int iVisibleRangeEnd;
	int bVisibleRangeSecondCached;
	int iVisibleRangeSecondFirstColumn;
	int iVisibleRangeSecondLastColumn;
	int iVisibleRangeSecondStart;
	int iVisibleRangeSecondEnd;
	uint32_t iEditNotifyVersion;
	int iEditNotifyAnchor;
	int iEditNotifyCaret;
} xui_code_edit_data_t;

#define XUI_CODE_EDIT_MAX_LINE_SAMPLE_LINES 2048
#define XUI_CODE_EDIT_MAX_LINE_SAMPLE_BYTES 4096
#define XUI_CODE_EDIT_TOKEN_WINDOW_BYTES 262144
#define XUI_CODE_EDIT_LINE_LAYOUT_CACHE_CAPACITY 96
#define XUI_CODE_EDIT_ASSIST_NONE 0
#define XUI_CODE_EDIT_ASSIST_SIGNATURE 1
#define XUI_CODE_EDIT_ASSIST_HINT 2
#define XUI_CODE_EDIT_ASSIST_COMPLETION_DOCUMENTATION 3

typedef struct xui_code_find_scope_editor_t {
	xui_widget pEditor;
	char* sName;
} xui_code_find_scope_editor_t;

struct xui_code_find_scope_t {
	xui_code_find_scope_editor_t* pEditors;
	int iEditorCount;
	int iEditorCapacity;
	xui_code_find_result_t* pResults;
	int iResultCount;
	int iResultCapacity;
	int iActiveIndex;
	xui_code_find_activate_proc onActivate;
	void* pActivateUser;
};

static int __xuiCodeEditExecuteCommand(xui_widget pWidget, xui_code_edit_data_t* pData, int iCommand, const void* pCommandData, int* pHandled);
static int __xuiCodeEditUpdateScrollModel(xui_widget pWidget, xui_code_edit_data_t* pData);
static void __xuiCodeEditFindWindowApplyLanguage(xui_widget pCodeEdit, xui_code_edit_data_t* pData);
static int __xuiCodeEditUpdate(xui_widget pWidget, float fDelta, void* pUser);
static uint32_t __xuiCodeEditColor(xui_widget pWidget, const char* sName, uint32_t iDefaultColor);
static int __xuiCodeEditVisibleRowToLine(xui_code_edit_data_t* pData, int iRow);
static int __xuiCodeEditDocumentVisibleRowToLine(xui_code_edit_data_t* pData,
	int iRow);
static const char* __xuiCodeFindScopeGetEditorNameByWidget(xui_code_find_scope pScope, xui_widget pEditor);
static int __xuiCodeEditInlineCompletionValid(xui_code_edit_data_t* pData);
static int __xuiCodeEditInlineCompletionClearData(xui_code_edit_data_t* pData);
static int __xuiCodeEditInlineCompletionConsume(xui_code_edit_data_t* pData, const char* sText, int iTextLength);
static int __xuiCodeEditShowCompletionInternal(xui_widget pWidget, xui_code_edit_data_t* pData, int bManual);
static int __xuiCodeEditCancelCompletionInternal(xui_code_edit_data_t* pData);
static int __xuiCodeEditCommitCompletionInternal(xui_widget pWidget, xui_code_edit_data_t* pData, int iIndex);
static int __xuiCodeEditCommitCompletionCharacter(xui_widget pWidget,
	xui_code_edit_data_t* pData, const char* sText);
static void __xuiCodeEditCompletionItemsClear(xui_code_edit_data_t* pData);
static xui_code_completion_owned_item_t* __xuiCodeEditCompletionVisibleItem(
	xui_code_edit_data_t* pData, int iVisibleIndex);
static void __xuiCodeEditCompletionListSelected(xui_widget pList, int iIndex, void* pUser);
static void __xuiCodeEditCompletionSyncInline(xui_widget pWidget, xui_code_edit_data_t* pData);
static int __xuiCodeEditCompletionRefreshVisible(xui_widget pWidget,
	xui_code_edit_data_t* pData, const char* sPrefix);
static int __xuiCodeEditCompletionPrefix(xui_code_edit_data_t* pData,
	int* pStart, int* pCaret, char* sPrefix, int iPrefixCapacity);
static int __xuiCodeEditRefreshCompletionSession(xui_widget pWidget,
	xui_code_edit_data_t* pData);
static int __xuiCodeEditProcessTextInput(xui_widget pWidget, xui_code_edit_data_t* pData,
	const char* sText, int iTextSize, int iSource, const xui_event_t* pSourceEvent);
static int __xuiCodeEditApplyInputAction(xui_widget pWidget, xui_code_edit_data_t* pData,
	const xui_code_input_action_t* pAction);
static int __xuiCodeEditRequestSignatureInternal(xui_widget pWidget, xui_code_edit_data_t* pData);
static int __xuiCodeEditCloseAssistInternal(xui_code_edit_data_t* pData);
static void __xuiCodeEditShowCompletionDocumentation(xui_code_edit_data_t* pData,
	const xui_code_completion_owned_item_t* pItem);
static float __xuiCodeEditMeasureTextRange(xui_proxy pProxy, xui_font pFont,
	const char* sText, int iStart, int iEnd, float fFallbackWidth);
static int __xuiCodeEditReadRange(xui_code_edit_data_t* pData,
	int iStart, int iEnd, const char** psText);
static void __xuiCodeEditLineLayoutsClear(xui_code_edit_data_t* pData);
static double __xuiCodeEditLineLayoutOffsetX(
	const xui_code_edit_line_layout_t* pLayout, int iOffset);
static void __xuiCodeEditWrapIndexClear(xui_code_edit_data_t* pData);
static int __xuiCodeEditWrapPrepare(xui_widget pWidget,
	xui_code_edit_data_t* pData, float fWrapWidth);
static int __xuiCodeEditWrapEnsureLine(xui_widget pWidget,
	xui_code_edit_data_t* pData, int iLine, float fWrapWidth,
	xui_code_edit_line_layout_t** ppLayout);
static int __xuiCodeEditWrapLineVisualRow(xui_code_edit_data_t* pData,
	int iLine);
static int __xuiCodeEditWrapLineRows(xui_code_edit_data_t* pData, int iLine);
static int __xuiCodeEditWrapTotalRows(xui_code_edit_data_t* pData);
static int __xuiCodeEditWrapVisualRowLine(xui_code_edit_data_t* pData,
	int iVisualRow, int* pSegmentRow);
static int __xuiCodeEditWrapMeasureViewport(xui_widget pWidget,
	xui_code_edit_data_t* pData, float fWrapWidth, float fLineHeight,
	float fViewportHeight);

static void __xuiCodeEditAdjustPlaceholders(xui_code_edit_data_t* pData,
	int iStart, int iEnd, int iInsertedLength)
{
	int iDelta;
	int i;
	int iRangeStart;
	int iRangeEnd;

	if ( pData == NULL || pData->iPlaceholderCount <= 0 ||
	     iStart < 0 || iEnd < iStart || iInsertedLength < 0 ) return;
	iDelta = iInsertedLength - (iEnd - iStart);
	for ( i = 0; i < pData->iPlaceholderCount; i++ ) {
		iRangeStart = pData->pPlaceholders[i].tRange.iStart;
		iRangeEnd = pData->pPlaceholders[i].tRange.iEnd;
		if ( iRangeStart < iStart ) {
			/* Preserve the start before an edit or insertion. */
		} else if ( iRangeStart > iEnd || (iRangeStart == iEnd && iEnd > iStart) ) {
			iRangeStart += iDelta;
		} else {
			iRangeStart = iStart;
		}
		if ( iRangeEnd < iStart ) {
			/* Preserve the end before an edit or insertion. */
		} else if ( iRangeEnd >= iEnd ) {
			iRangeEnd += iDelta;
		} else {
			iRangeEnd = iStart + iInsertedLength;
		}
		if ( iRangeEnd < iRangeStart ) iRangeEnd = iRangeStart;
		pData->pPlaceholders[i].tRange.iStart = iRangeStart;
		pData->pPlaceholders[i].tRange.iEnd = iRangeEnd;
	}
}

static float __xuiCodeEditMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiCodeEditTextReserve(char** psText, int* pCapacity, int iCapacity)
{
	char* sNew;

	if ( psText == NULL || pCapacity == NULL || iCapacity <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= *pCapacity ) return XUI_OK;
	sNew = (char*)xrtRealloc(*psText, (size_t)iCapacity);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	*psText = sNew;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeEditUtf8ClampOffset(const char* sText, int iLength, int iOffset)
{
	if ( sText == NULL || iLength <= 0 || iOffset <= 0 ) return 0;
	if ( iOffset > iLength ) iOffset = iLength;
	while ( iOffset > 0 && iOffset < iLength &&
	        (((unsigned char)sText[iOffset] & 0xc0u) == 0x80u) ) iOffset--;
	return iOffset;
}

static void __xuiCodeEditImeReset(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->bImeComposing = 0;
	pData->iImeAnchorStart = 0;
	pData->iImeAnchorEnd = 0;
	pData->iImeCursor = 0;
	pData->iImeSelectionStart = 0;
	pData->iImeSelectionEnd = 0;
	if ( pData->sImeComposition != NULL ) pData->sImeComposition[0] = '\0';
}

static int __xuiCodeEditStringSet(char** psText, int* pCapacity, const char* sText)
{
	int iNeed;
	int iRet;

	if ( sText == NULL ) sText = "";
	iNeed = (int)strlen(sText) + 1;
	iRet = __xuiCodeEditTextReserve(psText, pCapacity, iNeed);
	if ( iRet != XUI_OK ) return iRet;
	memcpy(*psText, sText, (size_t)iNeed);
	return XUI_OK;
}

static char* __xuiCodeEditStringDup(const char* sText)
{
	char* sCopy;
	int iCapacity;

	sCopy = NULL;
	iCapacity = 0;
	if ( __xuiCodeEditStringSet(&sCopy, &iCapacity, sText) != XUI_OK ) return NULL;
	return sCopy;
}

static int __xuiCodeEditFindResultReserve(xui_code_edit_data_t* pData, int iCapacity)
{
	xui_find_result_t* pNew;

	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pData->iFindResultCapacity ) return XUI_OK;
	if ( iCapacity < pData->iFindResultCapacity * 2 ) iCapacity = pData->iFindResultCapacity * 2;
	if ( iCapacity < 16 ) iCapacity = 16;
	pNew = (xui_find_result_t*)xrtRealloc(pData->pFindResults, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pData->pFindResults = pNew;
	pData->iFindResultCapacity = iCapacity;
	return XUI_OK;
}

static void __xuiCodeEditClearFindResults(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iFindResultCount = 0;
	pData->iFindActiveIndex = -1;
	pData->bFindResultScopeMode = 0;
	memset(&pData->tFindActive, 0, sizeof(pData->tFindActive));
	if ( pData->pAnnotations != NULL ) {
		(void)xuiCodeAnnotationClearIndicators(pData->pAnnotations, XUI_CODE_INDICATOR_SEARCH_RESULT);
	}
	if ( pData->pFindResultList != NULL ) {
		(void)xuiTableViewRefreshAdapter(pData->pFindResultList);
	}
}

static int __xuiCodeEditAfterDocumentReplace(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFoldState != NULL ) xuiCodeFoldStateClear(pData->pFoldState);
	if ( pData->pTokenBuffer != NULL ) xuiCodeTokenBufferClear(pData->pTokenBuffer);
	if ( pData->pAnnotations != NULL ) xuiCodeAnnotationStoreClear(pData->pAnnotations);
	__xuiCodeEditClearFindResults(pData);
	if ( pData->pSelection != NULL ) {
		iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, 0, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bDragging = 0;
	pData->iDragAnchor = 0;
	__xuiCodeEditImeReset(pData);
	(void)__xuiCodeEditCancelCompletionInternal(pData);
	(void)__xuiCodeEditInlineCompletionClearData(pData);
	(void)__xuiCodeEditCloseAssistInternal(pData);
	xrtFree(pData->pPlaceholders);
	pData->pPlaceholders = NULL;
	pData->iPlaceholderCount = 0;
	pData->iPlaceholderActive = 0;
	pData->fScrollX = 0.0f;
	pData->fScrollY = 0.0f;
	pData->iMaxLineLengthVersion = 0;
	pData->iMaxLineLengthTabColumns = 0;
	pData->iCachedMaxLineLength = 0;
	__xuiCodeEditLineLayoutsClear(pData);
	__xuiCodeEditWrapIndexClear(pData);
	(void)xuiScrollModelSetOffset(&pData->tScrollModel, 0.0f, 0.0f);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiCodeEditDestroyFindData(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	xrtFree(pData->sFindPattern);
	xrtFree(pData->sFindReplacement);
	xrtFree(pData->pFindResults);
	pData->sFindPattern = NULL;
	pData->sFindReplacement = NULL;
	pData->pFindResults = NULL;
	pData->iFindPatternCapacity = 0;
	pData->iFindReplacementCapacity = 0;
	pData->iFindResultCapacity = 0;
}

static int __xuiCodeEditAlpha(uint32_t iColor)
{
	return (int)((iColor >> 24) & 0xffu);
}

static xui_font __xuiCodeEditResolveFont(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_context pContext;

	if ( pData != NULL && pData->pFont != NULL ) return pData->pFont;
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	return (pContext != NULL) ? xuiGetDefaultFont(pContext) : NULL;
}

static float __xuiCodeEditLineHeight(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_font_metrics_t tMetrics;

	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	pProxy = (pContext != NULL) ? xuiInternalContextGetProxy(pContext) : NULL;
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pFont != NULL ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f ) {
			return (tMetrics.fLineHeight > 18.0f) ? tMetrics.fLineHeight : 18.0f;
		}
	}
	return 18.0f;
}

static float __xuiCodeEditFontLineHeight(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_font_metrics_t tMetrics;

	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	pProxy = (pContext != NULL) ? xuiInternalContextGetProxy(pContext) : NULL;
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pFont != NULL ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f ) {
			return tMetrics.fLineHeight;
		}
	}
	return 18.0f;
}

static float __xuiCodeEditLineTextOffsetY(xui_widget pWidget, xui_code_edit_data_t* pData, float fLineHeight)
{
	float fFontLineHeight;

	fFontLineHeight = __xuiCodeEditFontLineHeight(pWidget, pData);
	if ( fLineHeight > fFontLineHeight && fFontLineHeight > 0.0f ) {
		return (fLineHeight - fFontLineHeight) * 0.5f;
	}
	return 0.0f;
}

static float __xuiCodeEditCaretHeight(xui_widget pWidget, xui_code_edit_data_t* pData, float fLineHeight)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_font_metrics_t tMetrics;
	float fGlyphHeight;
	float fMaxHeight;

	if ( fLineHeight <= 1.0f ) return 1.0f;
	fMaxHeight = __xuiCodeEditFontLineHeight(pWidget, pData);
	if ( fMaxHeight <= 0.0f || fMaxHeight > fLineHeight ) fMaxHeight = fLineHeight;
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	pProxy = (pContext != NULL) ? xuiInternalContextGetProxy(pContext) : NULL;
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pFont != NULL ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK ) {
			fGlyphHeight = (tMetrics.fDescent < 0.0f) ? (tMetrics.fAscent - tMetrics.fDescent) : (tMetrics.fAscent + tMetrics.fDescent);
			if ( fGlyphHeight <= 0.0f ) fGlyphHeight = tMetrics.fAscent - tMetrics.fDescent;
			if ( fGlyphHeight <= 0.0f ) fGlyphHeight = tMetrics.fSize;
			if ( fGlyphHeight > 0.0f ) {
				if ( fGlyphHeight > fMaxHeight ) fGlyphHeight = fMaxHeight;
				return (fGlyphHeight > 1.0f) ? fGlyphHeight : 1.0f;
			}
		}
	}
	return fMaxHeight;
}

static float __xuiCodeEditColumnWidth(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_vec2_t tSize;

	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	pProxy = (pContext != NULL) ? xuiInternalContextGetProxy(pContext) : NULL;
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	if ( pProxy != NULL && pProxy->textMeasure != NULL && pFont != NULL ) {
		memset(&tSize, 0, sizeof(tSize));
		if ( pProxy->textMeasure(pProxy, pFont, "M", &tSize) == XUI_OK && tSize.fX > 0.0f ) {
			return (tSize.fX > 8.0f) ? tSize.fX : 8.0f;
		}
	}
	return 8.0f;
}

static int __xuiCodeEditLineVisible(xui_code_edit_data_t* pData, int iLine)
{
	int bVisible;

	if ( pData == NULL || pData->pFoldState == NULL ) return 1;
	bVisible = 1;
	if ( xuiCodeFoldStateIsLineVisible(pData->pFoldState, iLine, &bVisible) != XUI_OK ) return 1;
	return bVisible;
}

static int __xuiCodeEditHasFoldRanges(xui_code_edit_data_t* pData)
{
	return (pData != NULL && pData->pFoldState != NULL && xuiCodeFoldStateGetCount(pData->pFoldState) > 0);
}

static int __xuiCodeEditLineToDocumentVisibleRow(xui_code_edit_data_t* pData, int iLine)
{
	int iRow;

	if ( pData == NULL || iLine <= 0 ) return 0;
	if ( !__xuiCodeEditHasFoldRanges(pData) ) return iLine;
	if ( xuiCodeFoldStateLineToVisibleRow(pData->pFoldState, iLine, &iRow) != XUI_OK ) return iLine;
	return iRow;
}

static int __xuiCodeEditDocumentVisibleLineCount(xui_code_edit_data_t* pData)
{
	int iCount;
	int iVisible;

	if ( pData == NULL || pData->pDocument == NULL ) return 1;
	if ( pData->pFoldState == NULL || xuiCodeFoldStateGetCount(pData->pFoldState) <= 0 ) {
		iVisible = xuiCodeDocumentGetLineCount(pData->pDocument);
		return (iVisible > 0) ? iVisible : 1;
	}
	iVisible = xuiCodeDocumentGetLineCount(pData->pDocument);
	iCount = iVisible;
	(void)xuiCodeFoldStateGetVisibleLineCount(pData->pFoldState, iVisible, &iCount);
	return (iCount > 0) ? iCount : 1;
}

static int __xuiCodeEditInlineCompletionLayout(xui_code_edit_data_t* pData, int* pAnchorLine, int* pAnchorRow, int* pExtraRows)
{
	if ( pAnchorLine != NULL ) *pAnchorLine = -1;
	if ( pAnchorRow != NULL ) *pAnchorRow = -1;
	if ( pExtraRows != NULL ) *pExtraRows = 0;
	if ( !__xuiCodeEditInlineCompletionValid(pData) || pData->iInlineCompletionExtraRows <= 0 ||
		!__xuiCodeEditLineVisible(pData, pData->iInlineCompletionLine) ) return 0;
	if ( pAnchorLine != NULL ) *pAnchorLine = pData->iInlineCompletionLine;
	if ( pAnchorRow != NULL ) {
		*pAnchorRow = __xuiCodeEditWrapLineVisualRow(pData,
			pData->iInlineCompletionLine) +
			__xuiCodeEditWrapLineRows(pData, pData->iInlineCompletionLine) - 1;
	}
	if ( pExtraRows != NULL ) *pExtraRows = pData->iInlineCompletionExtraRows;
	return 1;
}

static int __xuiCodeEditLineToVisibleRow(xui_code_edit_data_t* pData, int iLine)
{
	int iRow;
	int iAnchorLine;
	int iExtraRows;

	iRow = __xuiCodeEditWrapLineVisualRow(pData, iLine);
	if ( __xuiCodeEditInlineCompletionLayout(pData, &iAnchorLine, NULL, &iExtraRows) && iLine > iAnchorLine ) {
		iRow += iExtraRows;
	}
	return iRow;
}

static int __xuiCodeEditVisibleLineCount(xui_code_edit_data_t* pData)
{
	int iCount;
	int iExtraRows;

	iCount = (pData != NULL && pData->bWordWrap && pData->pWrapTree != NULL) ?
		__xuiCodeEditWrapTotalRows(pData) :
		__xuiCodeEditDocumentVisibleLineCount(pData);
	if ( __xuiCodeEditInlineCompletionLayout(pData, NULL, NULL, &iExtraRows) ) iCount += iExtraRows;
	return iCount;
}

static int __xuiCodeEditTabColumns(const xui_code_edit_data_t* pData)
{
	if ( pData != NULL && pData->iTabColumns > 0 ) return pData->iTabColumns;
	return 4;
}

static int __xuiCodeEditIndentColumns(const xui_code_edit_data_t* pData)
{
	if ( pData != NULL && pData->iIndentColumns > 0 ) return pData->iIndentColumns;
	return __xuiCodeEditTabColumns(pData);
}

static int __xuiCodeEditTabAdvance(const xui_code_edit_data_t* pData, int iVisualColumn)
{
	int iTabColumns;
	int iRemainder;

	iTabColumns = __xuiCodeEditTabColumns(pData);
	iRemainder = iVisualColumn % iTabColumns;
	return (iRemainder == 0) ? iTabColumns : (iTabColumns - iRemainder);
}

static int __xuiCodeEditUtf8Next(const char* sText, int iLength, int iOffset, uint32_t* pCodepoint)
{
	unsigned char c0;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;
	uint32_t iCodepoint;
	int iStep;

	if ( pCodepoint != NULL ) *pCodepoint = 0u;
	if ( sText == NULL ) return 0;
	if ( iOffset < 0 ) return 0;
	if ( iOffset >= iLength ) return iLength;
	c0 = (unsigned char)sText[iOffset];
	iCodepoint = c0;
	iStep = 1;
	if ( c0 < 0x80u ) {
		iCodepoint = c0;
		iStep = 1;
	} else if ( (c0 & 0xE0u) == 0xC0u && iOffset + 1 < iLength ) {
		c1 = (unsigned char)sText[iOffset + 1];
		iCodepoint = ((uint32_t)(c0 & 0x1Fu) << 6) | (uint32_t)(c1 & 0x3Fu);
		iStep = 2;
	} else if ( (c0 & 0xF0u) == 0xE0u && iOffset + 2 < iLength ) {
		c1 = (unsigned char)sText[iOffset + 1];
		c2 = (unsigned char)sText[iOffset + 2];
		iCodepoint = ((uint32_t)(c0 & 0x0Fu) << 12) | ((uint32_t)(c1 & 0x3Fu) << 6) | (uint32_t)(c2 & 0x3Fu);
		iStep = 3;
	} else if ( (c0 & 0xF8u) == 0xF0u && iOffset + 3 < iLength ) {
		c1 = (unsigned char)sText[iOffset + 1];
		c2 = (unsigned char)sText[iOffset + 2];
		c3 = (unsigned char)sText[iOffset + 3];
		iCodepoint = ((uint32_t)(c0 & 0x07u) << 18) | ((uint32_t)(c1 & 0x3Fu) << 12) | ((uint32_t)(c2 & 0x3Fu) << 6) | (uint32_t)(c3 & 0x3Fu);
		iStep = 4;
	}
	if ( pCodepoint != NULL ) *pCodepoint = iCodepoint;
	if ( iOffset + iStep > iLength ) return iLength;
	return xuiInternalTextGraphemeNext(sText, iLength, iOffset);
}

static int __xuiCodeEditCodepointColumns(uint32_t iCodepoint)
{
	if ( iCodepoint == 0u ) return 0;
	if ( iCodepoint < 0x80u ) return 1;
	if ( (iCodepoint >= 0x1100u && iCodepoint <= 0x11FFu) ||
	     (iCodepoint >= 0x2E80u && iCodepoint <= 0xA4CFu) ||
	     (iCodepoint >= 0xAC00u && iCodepoint <= 0xD7AFu) ||
	     (iCodepoint >= 0xF900u && iCodepoint <= 0xFAFFu) ||
	     (iCodepoint >= 0xFE10u && iCodepoint <= 0xFE6Fu) ||
	     (iCodepoint >= 0xFF00u && iCodepoint <= 0xFFEFu) ||
	     (iCodepoint >= 0x20000u && iCodepoint <= 0x3FFFDu) ) {
		return 2;
	}
	return 1;
}

/* One owned-range prefix index shared by byte and visual-column queries.
   Points are original grapheme boundaries. A point requires four following
   bytes in the caller's limit: the old grapheme decoder may inspect the next
   UTF-8 codepoint before deciding the preceding boundary. No EOF point is
   reused under a different truncated limit. No text or font state is retained. */
#define XUI_CODE_EDIT_COLUMN_INDEX_MIN_BYTES 65536
#define XUI_CODE_EDIT_COLUMN_INDEX_STRIDE 4096
#define XUI_CODE_EDIT_COLUMN_INDEX_LIMIT 4096
#ifndef XUI_CODE_EDIT_COLUMN_INDEX_TRACE
#define XUI_CODE_EDIT_COLUMN_INDEX_TRACE(event,value) ((void)0)
#endif
#ifndef XUI_CODE_EDIT_COLUMN_INDEX_REALLOC
#define XUI_CODE_EDIT_COLUMN_INDEX_REALLOC(pointer,size) xrtRealloc(pointer,size)
#endif

static void __xuiCodeEditColumnIndexClear(xui_code_edit_data_t* pData)
{
    xrtFree(pData->pColumnPoints);pData->pColumnPoints=NULL;
    pData->iColumnPointCount=0;pData->iColumnPointCapacity=0;
    pData->pColumnIndexDocument=NULL;pData->bColumnIndexAllocationFailed=0;
}

static int __xuiCodeEditColumnIndexPrepare(xui_code_edit_data_t* pData,
    const char* sText,int iLineStart,int iLimit)
{
    int tabs;
    if (!pData || !pData->pDocument || !pData->bReadRangeValid ||
        sText!=pData->sReadBuffer || iLineStart!=0 ||
        iLimit<XUI_CODE_EDIT_COLUMN_INDEX_MIN_BYTES ||
        iLimit>pData->iReadRangeEnd-pData->iReadRangeStart ||
        pData->iReadVersion!=xuiCodeDocumentGetVersion(pData->pDocument)) return 0;
    tabs=__xuiCodeEditTabColumns(pData);
    if (pData->pColumnIndexDocument!=pData->pDocument ||
        pData->iColumnIndexVersion!=pData->iReadVersion ||
        pData->iColumnIndexReadStart!=pData->iReadRangeStart ||
        pData->iColumnIndexTabColumns!=tabs) {
        pData->iColumnPointCount=0;pData->bColumnIndexAllocationFailed=0;
        pData->pColumnIndexDocument=pData->pDocument;
        pData->iColumnIndexVersion=pData->iReadVersion;
        pData->iColumnIndexReadStart=pData->iReadRangeStart;
        pData->iColumnIndexTabColumns=tabs;
        XUI_CODE_EDIT_COLUMN_INDEX_TRACE(3,0);
    }
    return 1;
}

static int __xuiCodeEditColumnIndexSafeCount(xui_code_edit_data_t* pData,int limit)
{
    int low=0,high=pData->iColumnPointCount;
    while (low<high) {
        int mid=low+(high-low)/2;
        if (pData->pColumnPoints[mid].iOffset<=limit-4) low=mid+1;
        else high=mid;
    }
    return low;
}

static void __xuiCodeEditColumnIndexSeedOffset(xui_code_edit_data_t* pData,
    int limit,int* offset,int* column)
{
    int count=__xuiCodeEditColumnIndexSafeCount(pData,limit);
    *offset=0;*column=0;
    if (count>0) {
        *offset=pData->pColumnPoints[count-1].iOffset;
        *column=pData->pColumnPoints[count-1].iColumn;
        XUI_CODE_EDIT_COLUMN_INDEX_TRACE(1,*offset);
    }
}

static void __xuiCodeEditColumnIndexSeedColumn(xui_code_edit_data_t* pData,
    int limit,int target,int* offset,int* column)
{
    int low=0,high=__xuiCodeEditColumnIndexSafeCount(pData,limit);
    *offset=0;*column=0;
    while (low<high) {
        int mid=low+(high-low)/2;
        if (pData->pColumnPoints[mid].iColumn<=target) low=mid+1;
        else high=mid;
    }
    if (low>0) {
        *offset=pData->pColumnPoints[low-1].iOffset;
        *column=pData->pColumnPoints[low-1].iColumn;
        XUI_CODE_EDIT_COLUMN_INDEX_TRACE(1,*offset);
    }
}

static void __xuiCodeEditColumnIndexRecord(xui_code_edit_data_t* pData,
    int offset,int column,int limit)
{
    int previous=pData->iColumnPointCount>0?
        pData->pColumnPoints[pData->iColumnPointCount-1].iOffset:0;
    int capacity;
    xui_code_edit_column_point_t* points;
    if (pData->bColumnIndexAllocationFailed || offset>limit-4 || offset-previous<XUI_CODE_EDIT_COLUMN_INDEX_STRIDE ||
        pData->iColumnPointCount>=XUI_CODE_EDIT_COLUMN_INDEX_LIMIT) return;
    if (pData->iColumnPointCount==pData->iColumnPointCapacity) {
        capacity=pData->iColumnPointCapacity>0?pData->iColumnPointCapacity*2:16;
        if (capacity>XUI_CODE_EDIT_COLUMN_INDEX_LIMIT) capacity=XUI_CODE_EDIT_COLUMN_INDEX_LIMIT;
        points=XUI_CODE_EDIT_COLUMN_INDEX_REALLOC(pData->pColumnPoints,(size_t)capacity*sizeof(*points));
        if (!points) {pData->bColumnIndexAllocationFailed=1;XUI_CODE_EDIT_COLUMN_INDEX_TRACE(4,capacity);return;}
        pData->pColumnPoints=points;pData->iColumnPointCapacity=capacity;
    }
    pData->pColumnPoints[pData->iColumnPointCount++]=(xui_code_edit_column_point_t){offset,column};
    XUI_CODE_EDIT_COLUMN_INDEX_TRACE(2,pData->iColumnPointCount);
}

static int __xuiCodeEditLineVisualColumn(xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iOffset)
{
	int i;
	int iColumn;
	uint32_t iCodepoint;

	if ( sText == NULL || iOffset <= iLineStart ) return 0;
    int indexed=__xuiCodeEditColumnIndexPrepare(pData,sText,iLineStart,iOffset);
	iColumn = 0;
	i = iLineStart;
    if (indexed) __xuiCodeEditColumnIndexSeedOffset(pData,iOffset,&i,&iColumn);
	while ( i < iOffset ) {
		if ( sText[i] == '\t' ) {
			iColumn += __xuiCodeEditTabAdvance(pData, iColumn);
			i++;
		} else {
			i = __xuiCodeEditUtf8Next(sText, iOffset, i, &iCodepoint);
			iColumn += __xuiCodeEditCodepointColumns(iCodepoint);
		}
        if (indexed) __xuiCodeEditColumnIndexRecord(pData,i,iColumn,iOffset);
	}
	return iColumn;
}

static int __xuiCodeEditLineColumnFromVisual(const xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, int iVisualColumn)
{
	int i;
	int iColumn;
	int iNextColumn;
	int iTextColumn;
	uint32_t iCodepoint;

	if ( sText == NULL || iVisualColumn <= 0 ) return 0;
	iColumn = 0;
	iTextColumn = 0;
	for ( i = iLineStart; i < iLineEnd; ) {
		if ( sText[i] == '\t' ) {
			iNextColumn = iColumn + __xuiCodeEditTabAdvance(pData, iColumn);
			i++;
		} else {
			i = __xuiCodeEditUtf8Next(sText, iLineEnd, i, &iCodepoint);
			iNextColumn = iColumn + __xuiCodeEditCodepointColumns(iCodepoint);
		}
		if ( iVisualColumn < iNextColumn ) {
			return iTextColumn;
		}
		iColumn = iNextColumn;
		iTextColumn++;
	}
	return iTextColumn;
}

static int __xuiCodeEditLineColumnFromVisualFloat(const xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, float fVisualColumn)
{
	int i;
	int iColumn;
	int iNextColumn;
	int iTextColumn;
	float fMidColumn;
	uint32_t iCodepoint;

	if ( sText == NULL || fVisualColumn <= 0.0f ) return 0;
	iColumn = 0;
	iTextColumn = 0;
	for ( i = iLineStart; i < iLineEnd; ) {
		if ( sText[i] == '\t' ) {
			iNextColumn = iColumn + __xuiCodeEditTabAdvance(pData, iColumn);
			i++;
		} else {
			i = __xuiCodeEditUtf8Next(sText, iLineEnd, i, &iCodepoint);
			iNextColumn = iColumn + __xuiCodeEditCodepointColumns(iCodepoint);
		}
		if ( fVisualColumn < (float)iNextColumn ) {
			fMidColumn = ((float)iColumn + (float)iNextColumn) * 0.5f;
			return (fVisualColumn >= fMidColumn) ? (iTextColumn + 1) : iTextColumn;
		}
		iColumn = iNextColumn;
		iTextColumn++;
	}
	return iTextColumn;
}

static int __xuiCodeEditLineOffsetFromVisualColumn(const xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, int iVisualColumn)
{
	int i;
	int iColumn;
	int iNextColumn;
	uint32_t iCodepoint;

	if ( sText == NULL || iVisualColumn <= 0 ) return iLineStart;
	iColumn = 0;
	for ( i = iLineStart; i < iLineEnd; ) {
		if ( sText[i] == '\t' ) {
			iNextColumn = iColumn + __xuiCodeEditTabAdvance(pData, iColumn);
			i++;
		} else {
			i = __xuiCodeEditUtf8Next(sText, iLineEnd, i, &iCodepoint);
			iNextColumn = iColumn + __xuiCodeEditCodepointColumns(iCodepoint);
		}
		if ( iNextColumn > iVisualColumn ) return i;
		iColumn = iNextColumn;
	}
	return iLineEnd;
}

static void __xuiCodeEditVisibleByteRange(xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, float fColumnWidth, float fViewportWidth, int* pStart, int* pEnd)
{
	int iFirstColumn;
	int iLastColumn;
	int iVisibleColumns;
	int iStart;
	int iEnd;
	int i;
	int iColumn;
	int iNextColumn;
	int iTabColumns;
	int bOwnRead;
	int bHaveStart;
	int bCacheContext;
	uint32_t iCodepoint;

	if ( pStart != NULL ) *pStart = iLineStart;
	if ( pEnd != NULL ) *pEnd = iLineEnd;
	if ( pData == NULL || sText == NULL || iLineEnd <= iLineStart || fColumnWidth <= 0.0f || fViewportWidth <= 0.0f ) return;
	iFirstColumn = (int)(pData->fScrollX / fColumnWidth) - 8;
	if ( iFirstColumn < 0 ) iFirstColumn = 0;
	iVisibleColumns = (int)(fViewportWidth / fColumnWidth) + 24;
	if ( iVisibleColumns < 32 ) iVisibleColumns = 32;
	iLastColumn = iFirstColumn + iVisibleColumns;
	iTabColumns = __xuiCodeEditTabColumns(pData);
	bOwnRead = pData->bReadRangeValid && pData->pDocument != NULL && sText == pData->sReadBuffer &&
		iLineStart == 0 && iLineEnd == pData->iReadRangeEnd - pData->iReadRangeStart &&
		pData->iReadVersion == xuiCodeDocumentGetVersion(pData->pDocument);
	bCacheContext = bOwnRead && pData->bVisibleRangeCached &&
		pData->pVisibleRangeDocument == pData->pDocument &&
		pData->iVisibleRangeVersion == pData->iReadVersion &&
		pData->iVisibleRangeReadStart == pData->iReadRangeStart &&
		pData->iVisibleRangeReadEnd == pData->iReadRangeEnd &&
		pData->iVisibleRangeTabColumns == iTabColumns;
	if ( bCacheContext && pData->iVisibleRangeFirstColumn == iFirstColumn &&
		pData->iVisibleRangeLastColumn == iLastColumn ) {
		iStart = pData->iVisibleRangeStart;
		iEnd = pData->iVisibleRangeEnd;
	} else if ( bCacheContext && pData->bVisibleRangeSecondCached &&
		pData->iVisibleRangeSecondFirstColumn == iFirstColumn &&
		pData->iVisibleRangeSecondLastColumn == iLastColumn ) {
		iStart = pData->iVisibleRangeSecondStart;
		iEnd = pData->iVisibleRangeSecondEnd;
	} else {
		/* Keep the old strict 'next column > target' boundary rule, but
		   find both viewport endpoints in one grapheme traversal. */
		iStart = iLineStart;
		iEnd = iLineEnd;
		bHaveStart = iFirstColumn == 0;
		iColumn = 0;
        int indexed=__xuiCodeEditColumnIndexPrepare(pData,sText,iLineStart,iLineEnd);
        i=iLineStart;
        if (indexed) __xuiCodeEditColumnIndexSeedColumn(pData,iLineEnd,iFirstColumn,&i,&iColumn);
		for ( ; i < iLineEnd; ) {
			if ( sText[i] == '\t' ) {
				iNextColumn = iColumn + __xuiCodeEditTabAdvance(pData, iColumn);
				i++;
			} else {
				i = __xuiCodeEditUtf8Next(sText, iLineEnd, i, &iCodepoint);
				iNextColumn = iColumn + __xuiCodeEditCodepointColumns(iCodepoint);
			}
            if (indexed) __xuiCodeEditColumnIndexRecord(pData,i,iNextColumn,iLineEnd);
			if ( !bHaveStart && iNextColumn > iFirstColumn ) {
				iStart = i;
				bHaveStart = 1;
			}
			if ( iNextColumn > iLastColumn ) { iEnd = i; break; }
			iColumn = iNextColumn;
		}
		if ( !bHaveStart ) iStart = iLineEnd;
		if ( iStart < iLineStart ) iStart = iLineStart;
		if ( iEnd > iLineEnd ) iEnd = iLineEnd;
		if ( iEnd < iStart ) iEnd = iStart;
		if ( bOwnRead ) {
			/* Scroll modelling and paint use different viewport widths.
			   Keep their last two results without retaining another text copy. */
			pData->bVisibleRangeSecondCached = bCacheContext;
			if ( bCacheContext ) {
				pData->iVisibleRangeSecondFirstColumn = pData->iVisibleRangeFirstColumn;
				pData->iVisibleRangeSecondLastColumn = pData->iVisibleRangeLastColumn;
				pData->iVisibleRangeSecondStart = pData->iVisibleRangeStart;
				pData->iVisibleRangeSecondEnd = pData->iVisibleRangeEnd;
			}
			pData->bVisibleRangeCached = 1;
			pData->pVisibleRangeDocument = pData->pDocument;
			pData->iVisibleRangeVersion = pData->iReadVersion;
			pData->iVisibleRangeReadStart = pData->iReadRangeStart;
			pData->iVisibleRangeReadEnd = pData->iReadRangeEnd;
			pData->iVisibleRangeFirstColumn = iFirstColumn;
			pData->iVisibleRangeLastColumn = iLastColumn;
			pData->iVisibleRangeTabColumns = iTabColumns;
			pData->iVisibleRangeStart = iStart;
			pData->iVisibleRangeEnd = iEnd;
		}
	}
	if ( pStart != NULL ) *pStart = iStart;
	if ( pEnd != NULL ) *pEnd = iEnd;
}

static int __xuiCodeEditMaxLineLength(xui_code_edit_data_t* pData)
{
	uint32_t iVersion;
	int i;
	int iStart;
	int iEnd;
	int iMeasureEnd;
	int iLineCount;
	int iLength;
	int iMax;
	int iTabColumns;
	const char* sLine;

	if ( pData == NULL || pData->pDocument == NULL ) return 0;
	iVersion = xuiCodeDocumentGetChangeVersion(pData->pDocument);
	iTabColumns = __xuiCodeEditTabColumns(pData);
	if ( pData->iMaxLineLengthVersion == iVersion &&
	     pData->iMaxLineLengthTabColumns == iTabColumns ) {
		return pData->iCachedMaxLineLength;
	}
	iMax = 0;
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( iLineCount > XUI_CODE_EDIT_MAX_LINE_SAMPLE_LINES ) iLineCount = XUI_CODE_EDIT_MAX_LINE_SAMPLE_LINES;
	for ( i = 0; i < iLineCount; i++ ) {
		if ( xuiCodeDocumentGetLineRange(pData->pDocument, i, &iStart, &iEnd) == XUI_OK ) {
			iMeasureEnd = iEnd;
			if ( iMeasureEnd - iStart > XUI_CODE_EDIT_MAX_LINE_SAMPLE_BYTES ) iMeasureEnd = iStart + XUI_CODE_EDIT_MAX_LINE_SAMPLE_BYTES;
			if ( __xuiCodeEditReadRange(pData, iStart, iMeasureEnd, &sLine) != XUI_OK ) continue;
			iLength = __xuiCodeEditLineVisualColumn(pData, sLine, 0, iMeasureEnd - iStart);
			if ( iLength > iMax ) iMax = iLength;
		}
	}
	pData->iMaxLineLengthVersion = iVersion;
	pData->iMaxLineLengthTabColumns = iTabColumns;
	pData->iCachedMaxLineLength = iMax;
	return iMax;
}

static void __xuiCodeEditObserveLineLength(xui_code_edit_data_t* pData, const char* sText, int iStart, int iEnd)
{
	int iLength;

	if ( pData == NULL || sText == NULL || iEnd < iStart ) return;
	iLength = __xuiCodeEditLineVisualColumn(pData, sText, iStart, iEnd);
	if ( iLength > pData->iCachedMaxLineLength ) pData->iCachedMaxLineLength = iLength;
}

static void __xuiCodeEditObserveVisibleLineLengths(xui_code_edit_data_t* pData, float fColumnWidth, float fLineHeight, float fViewportWidth, float fViewportHeight)
{
	const char* sText;
	int iLineCount;
	int iFirstVisibleRow;
	int iVisibleRows;
	int iLine;
	int iRow;
	int iVisibleCount;
	int iStart;
	int iEnd;
	int iVisibleStart;
	int iVisibleEnd;

	if ( pData == NULL || pData->pDocument == NULL || fLineHeight <= 0.0f ) return;
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( iLineCount <= 0 ) return;
	iFirstVisibleRow = (int)((pData->fScrollY - 4.0f) / fLineHeight);
	if ( iFirstVisibleRow < 0 ) iFirstVisibleRow = 0;
	if ( iFirstVisibleRow > 0 ) iFirstVisibleRow--;
	iVisibleRows = (int)(fViewportHeight / fLineHeight) + 4;
	iVisibleCount = __xuiCodeEditVisibleLineCount(pData);
	for ( iRow = iFirstVisibleRow; iRow < iVisibleCount && iRow <= iFirstVisibleRow + iVisibleRows; iRow++ ) {
		iLine = __xuiCodeEditVisibleRowToLine(pData, iRow);
		if ( iLine < 0 || iLine >= iLineCount ) break;
		if ( xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd) == XUI_OK ) {
			if ( __xuiCodeEditReadRange(pData, iStart, iEnd, &sText) != XUI_OK ) continue;
			__xuiCodeEditVisibleByteRange(pData, sText, 0, iEnd - iStart,
				fColumnWidth, fViewportWidth, &iVisibleStart, &iVisibleEnd);
			__xuiCodeEditObserveLineLength(pData, sText, iVisibleStart, iVisibleEnd);
		}
	}
}

static float __xuiCodeEditMeasureTextRange(xui_proxy pProxy, xui_font pFont, const char* sText, int iStart, int iEnd, float fFallbackWidth)
{
	xui_vec2_t tSize;
	char sSmall[256];
	char* sMeasure;
	int iLength;
	int i;
	int iRet;

	if ( iEnd <= iStart ) return 0.0f;
	if ( pProxy == NULL || pProxy->textMeasure == NULL || pFont == NULL || sText == NULL ) return fFallbackWidth;
	iLength = iEnd - iStart;
	sMeasure = sSmall;
	if ( iLength >= (int)sizeof(sSmall) ) {
		sMeasure = (char*)xrtMalloc((size_t)iLength + 1u);
		if ( sMeasure == NULL ) return fFallbackWidth;
	}
	for ( i = 0; i < iLength; i++ ) {
		unsigned char ch = (unsigned char)sText[iStart + i];
		sMeasure[i] = (ch < 32u) ? ' ' : (char)ch;
	}
	sMeasure[iLength] = '\0';
	memset(&tSize, 0, sizeof(tSize));
	iRet = pProxy->textMeasure(pProxy, pFont, sMeasure, &tSize);
	if ( sMeasure != sSmall ) xrtFree(sMeasure);
	if ( iRet != XUI_OK || tSize.fX < 0.0f ) return fFallbackWidth;
	return tSize.fX;
}

static void __xuiCodeEditLineLayoutClear(xui_code_edit_line_layout_t* pLayout)
{
	int i;

	if ( pLayout == NULL ) return;
    if (pLayout->pCursor && pLayout->onCursorFree) pLayout->onCursorFree(pLayout->pCursor,pLayout->pCursorUser);
	for ( i = 0; i < pLayout->iSegmentCount; i++ ) xuiTextShapeFree(&pLayout->pSegments[i].tShape);
	xrtFree(pLayout->sText);
	xrtFree(pLayout->pSegments);
	xrtFree(pLayout->pStops);
	xrtFree(pLayout->pWrapOffsets);
	memset(pLayout, 0, sizeof(*pLayout));
}

static void __xuiCodeEditLineLayoutsClear(xui_code_edit_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	for ( i = 0; i < pData->iLineLayoutCount; i++ ) __xuiCodeEditLineLayoutClear(&pData->pLineLayouts[i]);
	xrtFree(pData->pLineLayouts);
	pData->pLineLayouts = NULL;
	pData->iLineLayoutCount = 0;
	pData->iLineLayoutCapacity = 0;
	pData->iLineLayoutUseStamp = 0;
    pData->iLineLayoutVersion=0;
	pData->bVisibleRangeCached = 0;
}

static int __xuiCodeEditLineLayoutReserveSegments(xui_code_edit_line_layout_t* pLayout, int iCapacity)
{
	xui_code_edit_text_segment_t* pItems;

	if ( iCapacity <= pLayout->iSegmentCapacity ) return XUI_OK;
	if ( iCapacity < pLayout->iSegmentCapacity * 2 ) iCapacity = pLayout->iSegmentCapacity * 2;
	if ( iCapacity < 4 ) iCapacity = 4;
	pItems = (xui_code_edit_text_segment_t*)xrtRealloc(pLayout->pSegments, sizeof(*pItems) * (size_t)iCapacity);
	if ( pItems == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pLayout->pSegments = pItems;
	pLayout->iSegmentCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeEditWrapVisualSegment(xui_widget pWidget,
	xui_code_edit_data_t* pData, int iVisualRow, int* pLine,
	int* pSegment, int* pStart, int* pEnd, double* pStartX)
{
	xui_code_edit_line_layout_t* pLayout;
	int iAnchorRow;
	int iExtraRows;
	int iSegment;
	int iLine;
	int iStart;
	int iEnd;
	int iRet;

	if ( pLine != NULL ) *pLine = 0;
	if ( pSegment != NULL ) *pSegment = 0;
	if ( pStart != NULL ) *pStart = 0;
	if ( pEnd != NULL ) *pEnd = 0;
	if ( pStartX != NULL ) *pStartX = 0.0f;
	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiCodeEditInlineCompletionLayout(pData, NULL, &iAnchorRow,
		&iExtraRows) && iVisualRow > iAnchorRow + iExtraRows ) {
		iVisualRow -= iExtraRows;
	}
	iLine = pData->bWordWrap ?
		__xuiCodeEditWrapVisualRowLine(pData, iVisualRow, &iSegment) :
		__xuiCodeEditDocumentVisibleRowToLine(pData, iVisualRow);
	iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bWordWrap ) {
		iRet = __xuiCodeEditWrapEnsureLine(pWidget, pData, iLine,
			pData->fWrapLayoutWidth, &pLayout);
		if ( iRet != XUI_OK ) return iRet;
		if ( iSegment < 0 ) iSegment = 0;
		if ( iSegment >= pLayout->iWrapOffsetCount - 1 ) {
			iSegment = pLayout->iWrapOffsetCount - 2;
		}
		if ( iSegment < 0 ) iSegment = 0;
		if ( pLayout->iWrapOffsetCount > 1 ) {
			if ( pStart != NULL ) *pStart =
				iStart + pLayout->pWrapOffsets[iSegment];
			if ( pEnd != NULL ) *pEnd =
				iStart + pLayout->pWrapOffsets[iSegment + 1];
			if ( pStartX != NULL ) *pStartX =
				__xuiCodeEditLineLayoutOffsetX(pLayout,
					pLayout->pWrapOffsets[iSegment]);
		} else {
			if ( pStart != NULL ) *pStart = iStart;
			if ( pEnd != NULL ) *pEnd = iEnd;
		}
	} else {
		if ( pStart != NULL ) *pStart = iStart;
		if ( pEnd != NULL ) *pEnd = iEnd;
	}
	if ( pLine != NULL ) *pLine = iLine;
	if ( pSegment != NULL ) *pSegment = iSegment;
	return XUI_OK;
}

static int __xuiCodeEditWrapOffsetGeometry(xui_widget pWidget,
	xui_code_edit_data_t* pData, int iOffset, int* pVisualRow,
	int* pSegmentStart, int* pSegmentEnd, double* pOffsetX)
{
	xui_code_edit_line_layout_t* pLayout;
	int iLine;
	int iLineStart;
	int iLineEnd;
	int iRelative;
	int iSegment;
	int iRet;

	if ( pWidget == NULL || pData == NULL || iOffset < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiCodeDocumentOffsetToLineColumn(pData->pDocument, iOffset,
		&iLine, NULL);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine,
		&iLineStart, &iLineEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRelative = iOffset - iLineStart;
	if ( iRelative < 0 ) iRelative = 0;
	if ( iRelative > iLineEnd - iLineStart ) iRelative = iLineEnd - iLineStart;
	if ( !pData->bWordWrap ) {
		if ( pVisualRow != NULL ) *pVisualRow =
			__xuiCodeEditLineToVisibleRow(pData, iLine);
		if ( pSegmentStart != NULL ) *pSegmentStart = iLineStart;
		if ( pSegmentEnd != NULL ) *pSegmentEnd = iLineEnd;
		if ( pOffsetX != NULL ) *pOffsetX = -1.0f;
		return XUI_OK;
	}
	iRet = __xuiCodeEditWrapEnsureLine(pWidget, pData, iLine,
		pData->fWrapLayoutWidth, &pLayout);
	if ( iRet != XUI_OK ) return iRet;
	iSegment = 0;
	while ( iSegment + 1 < pLayout->iWrapOffsetCount - 1 &&
		iRelative >= pLayout->pWrapOffsets[iSegment + 1] ) iSegment++;
	if ( pVisualRow != NULL ) *pVisualRow =
		__xuiCodeEditLineToVisibleRow(pData, iLine) + iSegment;
	if ( pSegmentStart != NULL ) *pSegmentStart =
		iLineStart + pLayout->pWrapOffsets[iSegment];
	if ( pSegmentEnd != NULL ) *pSegmentEnd =
		iLineStart + pLayout->pWrapOffsets[iSegment + 1];
	if ( pOffsetX != NULL ) *pOffsetX =
		__xuiCodeEditLineLayoutOffsetX(pLayout, iRelative) -
		__xuiCodeEditLineLayoutOffsetX(pLayout,
			pLayout->pWrapOffsets[iSegment]);
	return XUI_OK;
}

static int __xuiCodeEditLineLayoutReserveStops(xui_code_edit_line_layout_t* pLayout, int iCapacity)
{
	xui_code_edit_caret_stop_t* pItems;

	if ( iCapacity <= pLayout->iStopCapacity ) return XUI_OK;
	if ( iCapacity < pLayout->iStopCapacity * 2 ) iCapacity = pLayout->iStopCapacity * 2;
	if ( iCapacity < 8 ) iCapacity = 8;
	pItems = (xui_code_edit_caret_stop_t*)xrtRealloc(pLayout->pStops, sizeof(*pItems) * (size_t)iCapacity);
	if ( pItems == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pLayout->pStops = pItems;
	pLayout->iStopCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeEditLineLayoutAddStop(xui_code_edit_line_layout_t* pLayout, int iOffset, double fX)
{
	int iRet;

	if ( pLayout->iStopCount > 0 && pLayout->pStops[pLayout->iStopCount - 1].iOffset == iOffset ) {
		pLayout->pStops[pLayout->iStopCount - 1].fX = fX;
		return XUI_OK;
	}
	iRet = __xuiCodeEditLineLayoutReserveStops(pLayout, pLayout->iStopCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pLayout->pStops[pLayout->iStopCount].iOffset = iOffset;
	pLayout->pStops[pLayout->iStopCount].fX = fX;
	pLayout->iStopCount++;
	return XUI_OK;
}

static int __xuiCodeEditShapeText(xui_proxy pProxy, xui_font pFont, const char* sText, int iTextSize, xui_text_shape_t* pShape)
{
	xui_vec2_t tSize;
	int iAt;
	int iNext;
	int iCount;
	int i;

	if ( pProxy == NULL || pFont == NULL || sText == NULL || iTextSize < 0 || pShape == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pProxy->textShape != NULL ) return pProxy->textShape(pProxy, pFont, sText, iTextSize, XUI_TEXT_SHAPE_DEFAULT, pShape);
	if ( pProxy->textMeasure == NULL ) return XUI_ERROR_UNSUPPORTED;
	memset(pShape, 0, sizeof(*pShape));
	pShape->iSize = sizeof(*pShape);
	pShape->iTextSize = iTextSize;
	for ( iAt = 0, iCount = 0; iAt < iTextSize; iAt = iNext, iCount++ ) {
		iNext = __xuiCodeEditUtf8Next(sText, iTextSize, iAt, NULL);
		if ( iNext <= iAt ) iNext = iAt + 1;
	}
	if ( iCount > 0 ) {
		pShape->pClusters = (xui_text_cluster_t*)xrtCalloc((size_t)iCount, sizeof(*pShape->pClusters));
		if ( pShape->pClusters == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	pShape->iClusterCount = iCount;
	for ( iAt = 0, i = 0; iAt < iTextSize; iAt = iNext, i++ ) {
		char sUtf8[8];
		int iBytes;
		iNext = __xuiCodeEditUtf8Next(sText, iTextSize, iAt, NULL);
		if ( iNext <= iAt ) iNext = iAt + 1;
		iBytes = iNext - iAt;
		if ( iBytes >= (int)sizeof(sUtf8) ) { xuiTextShapeFree(pShape); return XUI_ERROR_INVALID_ARGUMENT; }
		memcpy(sUtf8, sText + iAt, (size_t)iBytes);
		sUtf8[iBytes] = '\0';
		memset(&tSize, 0, sizeof(tSize));
		if ( pProxy->textMeasure(pProxy, pFont, sUtf8, &tSize) != XUI_OK ) { xuiTextShapeFree(pShape); return XUI_ERROR_UNSUPPORTED; }
		pShape->pClusters[i].iSize = sizeof(pShape->pClusters[i]);
		pShape->pClusters[i].iTextStart = iAt;
		pShape->pClusters[i].iTextEnd = iNext;
		pShape->pClusters[i].fAdvance = tSize.fX;
		pShape->fWidth += tSize.fX;
	}
	return XUI_OK;
}

static double __xuiCodeEditShapedTextOffsetX(xui_proxy pProxy, xui_font pFont,
	const char* sText, int iTextSize, int iOffset, float fFallbackWidth)
{
	xui_text_shape_t tShape;
	double fX;
	int i;

	if ( sText == NULL || iTextSize <= 0 || iOffset <= 0 ) return 0.0f;
	if ( iOffset > iTextSize ) iOffset = iTextSize;
	memset(&tShape, 0, sizeof(tShape));
	if ( __xuiCodeEditShapeText(pProxy, pFont, sText, iTextSize, &tShape) != XUI_OK ) return fFallbackWidth;
	fX = 0.0f;
	for ( i = 0; i < tShape.iClusterCount; i++ ) {
		if ( tShape.pClusters[i].iTextStart >= iOffset ) break;
		fX += tShape.pClusters[i].fAdvance;
		if ( tShape.pClusters[i].iTextEnd >= iOffset ) break;
	}
	xuiTextShapeFree(&tShape);
	return fX;
}

typedef struct xui_code_edit_layout_visit_t {
    xui_code_edit_line_layout_t* pLayout;
    int iSegmentStart;
    double fX;
    xui_text_shape_resume_t* pResume;
    int iOldStopCount,bStarted;
} xui_code_edit_layout_visit_t;

static int __xuiCodeEditLayoutVisitCluster(const xui_text_cluster_t* pCluster, void* pUser)
{
    xui_code_edit_layout_visit_t* pState=(xui_code_edit_layout_visit_t*)pUser;
    double fEnd;
    int iRet;
    if (pState->pResume && !pState->bStarted) {
        int offset=pState->pResume->iReplayTextOffset;
        int lo=0,hi=pState->iOldStopCount;
        if (offset<0 || offset!=pCluster->iTextStart) return XUI_ERROR_INVALID_ARGUMENT;
        while (lo<hi) {int mid=lo+(hi-lo)/2;if (pState->pLayout->pStops[mid].iOffset<offset) lo=mid+1;else hi=mid;}
        if (lo>=pState->iOldStopCount || pState->pLayout->pStops[lo].iOffset!=offset) return XUI_ERROR_INVALID_ARGUMENT;
        pState->fX=pState->pLayout->pStops[lo].fX;pState->pLayout->iStopCount=lo+1;pState->bStarted=1;
    }
    fEnd=pState->fX+pCluster->fAdvance;
    iRet=__xuiCodeEditLineLayoutAddStop(pState->pLayout,pState->iSegmentStart+pCluster->iTextStart,pState->fX);
    if (iRet==XUI_OK) iRet=__xuiCodeEditLineLayoutAddStop(pState->pLayout,pState->iSegmentStart+pCluster->iTextEnd,fEnd);
    if (iRet==XUI_OK) pState->fX=fEnd;
    return iRet;
}

#include "xui_cursor_layout_internal.inc"

static xui_code_edit_line_layout_t* __xuiCodeEditLineLayoutEnsure(xui_proxy pProxy, xui_font pFont,
	xui_code_edit_data_t* pData, const char* sText, int iTextSize, float fColumnWidth)
{
	xui_code_edit_line_layout_t* pLayout;
	uint32_t iVersion;
	float fTabWidth;
	double fX;
	int i;
	int iStart;
	int iRet;
	int iSlot;
	int iOldest;
    int bCursorSingle;

	if ( pData == NULL || pData->pDocument == NULL || pProxy == NULL || pFont == NULL || sText == NULL ) return NULL;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
    if (pData->iLineLayoutVersion!=iVersion) {
        if (pData->onCursorVisit) __xuiCodeEditKeepCursorLayout(pData,pFont);
        else __xuiCodeEditLineLayoutsClear(pData);
        pData->iLineLayoutVersion=iVersion;
    }
    bCursorSingle=pData->onCursorVisit && iTextSize>=4096 && !memchr(sText,'\t',(size_t)iTextSize);
	for ( i = 0; i < pData->iLineLayoutCount; i++ ) {
		pLayout = &pData->pLineLayouts[i];
		if ( pLayout->iDocumentVersion == iVersion && pLayout->pFont == pFont &&
		     pLayout->iTabColumns == pData->iTabColumns && pLayout->iTextSize == iTextSize &&
		     pLayout->sText != NULL && memcmp(pLayout->sText, sText, (size_t)iTextSize) == 0 ) {
			pLayout->iUseStamp = ++pData->iLineLayoutUseStamp;
			return pLayout;
		}
	}
    iSlot=-1;
    if (bCursorSingle) for (i=0;i<pData->iLineLayoutCount;++i) {
        pLayout=&pData->pLineLayouts[i];
        if (pLayout->iDocumentVersion!=iVersion && pLayout->pFont==pFont && pLayout->iTabColumns==pData->iTabColumns) {iSlot=i;break;}
    }
    if (iSlot<0) {
	if ( pData->iLineLayoutCount < XUI_CODE_EDIT_LINE_LAYOUT_CACHE_CAPACITY ) {
		if ( pData->iLineLayoutCount == pData->iLineLayoutCapacity ) {
			int iCapacity = pData->iLineLayoutCapacity > 0 ? pData->iLineLayoutCapacity * 2 : 16;
			xui_code_edit_line_layout_t* pItems;
			if ( iCapacity > XUI_CODE_EDIT_LINE_LAYOUT_CACHE_CAPACITY ) iCapacity = XUI_CODE_EDIT_LINE_LAYOUT_CACHE_CAPACITY;
			pItems = (xui_code_edit_line_layout_t*)xrtRealloc(pData->pLineLayouts, sizeof(*pItems) * (size_t)iCapacity);
			if ( pItems == NULL ) return NULL;
			memset(pItems + pData->iLineLayoutCapacity, 0, sizeof(*pItems) * (size_t)(iCapacity - pData->iLineLayoutCapacity));
			pData->pLineLayouts = pItems;
			pData->iLineLayoutCapacity = iCapacity;
		}
		iSlot = pData->iLineLayoutCount++;
	} else {
		iOldest = 0;
		for ( i = 1; i < pData->iLineLayoutCount; i++ ) {
			if ( pData->pLineLayouts[i].iUseStamp < pData->pLineLayouts[iOldest].iUseStamp ) iOldest = i;
		}
		iSlot = iOldest;
	}
    }
	pLayout = &pData->pLineLayouts[iSlot];
    if (bCursorSingle) return __xuiCodeEditCursorSingleLayout(pProxy,pFont,pData,pLayout,sText,iTextSize,iVersion);
	__xuiCodeEditLineLayoutClear(pLayout);
	pLayout->sText = (char*)xrtMalloc((size_t)iTextSize + 1u);
	if ( pLayout->sText == NULL ) return NULL;
	memcpy(pLayout->sText, sText, (size_t)iTextSize);
	pLayout->sText[iTextSize] = '\0';
	pLayout->iDocumentVersion = iVersion;
	pLayout->pFont = pFont;
	pLayout->iTabColumns = pData->iTabColumns;
	pLayout->iTextSize = iTextSize;
	pLayout->iUseStamp = ++pData->iLineLayoutUseStamp;
	fTabWidth = (float)__xuiCodeEditTabColumns(pData) * fColumnWidth;
	if ( fTabWidth <= 0.0f ) fTabWidth = fColumnWidth > 0.0f ? fColumnWidth : 8.0f;
	fX = 0.0f;
	iRet = __xuiCodeEditLineLayoutAddStop(pLayout, 0, fX);
	if ( iRet != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
	for ( i = 0; i < iTextSize; ) {
		if ( pLayout->sText[i] == '\t' ) {
			fX = (floor(fX / fTabWidth) + 1.0) * fTabWidth;
			iRet = __xuiCodeEditLineLayoutAddStop(pLayout, i + 1, fX);
			if ( iRet != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
			i++;
			continue;
		}
		iStart = i;
		while ( i < iTextSize && pLayout->sText[i] != '\t' ) i++;
		iRet = __xuiCodeEditLineLayoutReserveSegments(pLayout, pLayout->iSegmentCount + 1);
		if ( iRet != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
		memset(&pLayout->pSegments[pLayout->iSegmentCount], 0, sizeof(*pLayout->pSegments));
		pLayout->pSegments[pLayout->iSegmentCount].iStart = iStart;
		pLayout->pSegments[pLayout->iSegmentCount].iEnd = i;
		pLayout->pSegments[pLayout->iSegmentCount].fX = fX;
        {
            xui_code_edit_text_segment_t* pSegment = &pLayout->pSegments[pLayout->iSegmentCount];
            xui_code_edit_layout_visit_t tVisit;
            memset(&tVisit,0,sizeof(tVisit));
            tVisit.pLayout = pLayout;
            tVisit.iSegmentStart = iStart;
            tVisit.fX = fX;
            iRet=__xuiCodeEditVisitLayoutShape(pProxy,pFont,pData,pLayout->sText+iStart,
                i-iStart,&tVisit,&pSegment->tShape);
            if ( iRet != XUI_OK ) {
                /* The uncommitted segment is not included in iSegmentCount. */
                xuiTextShapeFree(&pSegment->tShape);
                __xuiCodeEditLineLayoutClear(pLayout);
                return NULL;
            }
            fX = tVisit.fX;
        }
		pLayout->iSegmentCount++;
	}
	pLayout->fWidth = fX;
	if ( __xuiCodeEditLineLayoutAddStop(pLayout, iTextSize, fX) != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
	return pLayout;
}

static double __xuiCodeEditLineLayoutOffsetX(const xui_code_edit_line_layout_t* pLayout, int iOffset)
{
    int lo=0,hi;
    if (!pLayout || pLayout->iStopCount<=0 || iOffset<=0) return 0.0f;
    hi=pLayout->iStopCount;
    while (lo<hi) {int mid=lo+(hi-lo)/2;if (pLayout->pStops[mid].iOffset<iOffset) lo=mid+1;else hi=mid;}
    if (lo<pLayout->iStopCount && pLayout->pStops[lo].iOffset==iOffset) return pLayout->pStops[lo].fX;
    return lo>0 ? pLayout->pStops[lo-1].fX : 0.0f;
}

static int __xuiCodeEditLineLayoutOffsetFromX(const xui_code_edit_line_layout_t* pLayout, double fX)
{
	int i;

	if ( pLayout == NULL || pLayout->iStopCount <= 0 || fX <= 0.0f ) return 0;
	for ( i = 1; i < pLayout->iStopCount; i++ ) {
		double fMiddle = (pLayout->pStops[i - 1].fX + pLayout->pStops[i].fX) * 0.5f;
		if ( fX < fMiddle ) return pLayout->pStops[i - 1].iOffset;
		if ( fX <= pLayout->pStops[i].fX ) return pLayout->pStops[i].iOffset;
	}
	return pLayout->pStops[pLayout->iStopCount - 1].iOffset;
}

static int __xuiCodeEditWrapBreakByte(unsigned char c)
{
	return c == ' ' || c == '\t' || c == ',' || c == ';' || c == ':' ||
		c == '.' || c == ')' || c == ']' || c == '}' || c == '/' ||
		c == '\\' || c == '-' || c == '+' || c == '=' || c == '|';
}

static int __xuiCodeEditLineLayoutReserveWraps(xui_code_edit_line_layout_t* pLayout,
	int iCapacity)
{
	int* pOffsets;

	if ( iCapacity <= pLayout->iWrapOffsetCapacity ) return XUI_OK;
	if ( iCapacity < pLayout->iWrapOffsetCapacity * 2 ) {
		iCapacity = pLayout->iWrapOffsetCapacity * 2;
	}
	if ( iCapacity < 8 ) iCapacity = 8;
	pOffsets = (int*)xrtRealloc(pLayout->pWrapOffsets,
		sizeof(*pOffsets) * (size_t)iCapacity);
	if ( pOffsets == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pLayout->pWrapOffsets = pOffsets;
	pLayout->iWrapOffsetCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeEditLineLayoutBuildWraps(xui_code_edit_line_layout_t* pLayout,
	float fWrapWidth)
{
	int iStartStop;
	int iCandidate;
	int iWordBreak;
	int i;
	int iStartOffset;
	double fLimit;
	int iRet;

	if ( pLayout == NULL || pLayout->iStopCount <= 0 || fWrapWidth <= 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pLayout->iWrapOffsetCount > 0 &&
		fabsf(pLayout->fWrapWidth - fWrapWidth) < 0.01f ) return XUI_OK;
	pLayout->iWrapOffsetCount = 0;
	pLayout->fWrapWidth = fWrapWidth;
	iRet = __xuiCodeEditLineLayoutReserveWraps(pLayout, 2);
	if ( iRet != XUI_OK ) return iRet;
	pLayout->pWrapOffsets[pLayout->iWrapOffsetCount++] = 0;
	iStartStop = 0;
	while ( iStartStop < pLayout->iStopCount - 1 ) {
		iStartOffset = pLayout->pStops[iStartStop].iOffset;
		fLimit = pLayout->pStops[iStartStop].fX + fWrapWidth;
		iCandidate = iStartStop + 1;
		while ( iCandidate + 1 < pLayout->iStopCount &&
			pLayout->pStops[iCandidate + 1].fX <= fLimit + 0.01f ) {
			iCandidate++;
		}
		if ( pLayout->pStops[iCandidate].fX > fLimit + 0.01f &&
			iCandidate > iStartStop + 1 ) iCandidate--;
		if ( iCandidate <= iStartStop ) iCandidate = iStartStop + 1;
		iWordBreak = -1;
		for ( i = iStartStop + 1; i <= iCandidate; i++ ) {
			int iOffset = pLayout->pStops[i].iOffset;
			if ( iOffset > iStartOffset &&
				__xuiCodeEditWrapBreakByte((unsigned char)pLayout->sText[iOffset - 1]) ) {
				iWordBreak = i;
			}
		}
		if ( iWordBreak > iStartStop ) iCandidate = iWordBreak;
		iRet = __xuiCodeEditLineLayoutReserveWraps(pLayout,
			pLayout->iWrapOffsetCount + 1);
		if ( iRet != XUI_OK ) return iRet;
		pLayout->pWrapOffsets[pLayout->iWrapOffsetCount++] =
			pLayout->pStops[iCandidate].iOffset;
		iStartStop = iCandidate;
	}
	if ( pLayout->iWrapOffsetCount == 1 ||
		pLayout->pWrapOffsets[pLayout->iWrapOffsetCount - 1] != pLayout->iTextSize ) {
		iRet = __xuiCodeEditLineLayoutReserveWraps(pLayout,
			pLayout->iWrapOffsetCount + 1);
		if ( iRet != XUI_OK ) return iRet;
		pLayout->pWrapOffsets[pLayout->iWrapOffsetCount++] = pLayout->iTextSize;
	}
	return XUI_OK;
}

static double __xuiCodeEditLineOffsetX(xui_proxy pProxy, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iOffset, float fColumnWidth)
{
	xui_code_edit_line_layout_t* pLayout;
	int iTextSize;

	(void)iLineStart;
	if ( pData == NULL || sText == NULL || iOffset <= 0 ) return 0.0f;
	iTextSize = (int)strlen(sText);
	pLayout = __xuiCodeEditLineLayoutEnsure(pProxy, pFont, pData, sText, iTextSize, fColumnWidth);
	return __xuiCodeEditLineLayoutOffsetX(pLayout, iOffset);
}

static int __xuiCodeEditReadRange(xui_code_edit_data_t* pData,
	int iStart, int iEnd, const char** psText)
{
	uint32_t iVersion;
	int iLength;
	int iRet;

	if ( pData == NULL || pData->pDocument == NULL || psText == NULL ||
	     iStart < 0 || iEnd < iStart ) return XUI_ERROR_INVALID_ARGUMENT;
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	if ( pData->bReadRangeValid && pData->sReadBuffer != NULL && pData->iReadVersion == iVersion &&
	     iStart == pData->iReadRangeStart && iEnd == pData->iReadRangeEnd ) {
		*psText = pData->sReadBuffer;
		return XUI_OK;
	}
	iLength = iEnd - iStart;
	/* CopyRange can replace bytes in the same allocation, even on failure. */
    pData->bReadRangeValid=0;
	pData->bVisibleRangeCached = 0;
	iRet = __xuiCodeEditTextReserve(&pData->sReadBuffer,
		&pData->iReadBufferCapacity, iLength + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentCopyRange(pData->pDocument, iStart, iEnd,
		pData->sReadBuffer, pData->iReadBufferCapacity, NULL);
	if ( iRet != XUI_OK ) return iRet;
	pData->iReadRangeStart = iStart;
	pData->iReadRangeEnd = iEnd;
	pData->iReadVersion = iVersion;
    pData->bReadRangeValid=1;
	*psText = pData->sReadBuffer;
	return XUI_OK;
}

static int __xuiCodeEditLineColumnFromX(xui_proxy pProxy, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, double fX, float fColumnWidth)
{
	xui_code_edit_line_layout_t* pLayout;
	int iByteOffset;
	int iColumn;
	int i;
	int iNext;

	(void)iLineStart;
	if ( pData == NULL || sText == NULL || fX <= 0.0f ) return 0;
	pLayout = __xuiCodeEditLineLayoutEnsure(pProxy, pFont, pData, sText, iLineEnd, fColumnWidth);
	iByteOffset = __xuiCodeEditLineLayoutOffsetFromX(pLayout, fX);
	iColumn = 0;
	for ( i = 0; i < iByteOffset; i = iNext, iColumn++ ) {
		iNext = __xuiCodeEditUtf8Next(sText, iByteOffset, i, NULL);
		if ( iNext <= i ) iNext = i + 1;
	}
	return iColumn;
}

static int __xuiCodeEditDocumentVisibleRowToLine(xui_code_edit_data_t* pData, int iRow)
{
	int iLineCount;
	int iLine;

	if ( pData == NULL || pData->pDocument == NULL ) return 0;
	if ( iRow < 0 ) iRow = 0;
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( !__xuiCodeEditHasFoldRanges(pData) ) {
		if ( iLineCount <= 0 ) return 0;
		return (iRow < iLineCount) ? iRow : (iLineCount - 1);
	}
	if ( xuiCodeFoldStateVisibleRowToLine(pData->pFoldState, iLineCount, iRow, &iLine) != XUI_OK ) return 0;
	return iLine;
}

static int __xuiCodeEditVisibleRowIsInlineCompletion(xui_code_edit_data_t* pData, int iRow)
{
	int iAnchorRow;
	int iExtraRows;

	return __xuiCodeEditInlineCompletionLayout(pData, NULL, &iAnchorRow, &iExtraRows) &&
		iRow > iAnchorRow && iRow <= iAnchorRow + iExtraRows;
}

static int __xuiCodeEditVisibleRowToLine(xui_code_edit_data_t* pData, int iRow)
{
	int iAnchorLine;
	int iAnchorRow;
	int iExtraRows;

	if ( __xuiCodeEditInlineCompletionLayout(pData, &iAnchorLine, &iAnchorRow, &iExtraRows) ) {
		if ( iRow > iAnchorRow && iRow <= iAnchorRow + iExtraRows ) return iAnchorLine;
		if ( iRow > iAnchorRow + iExtraRows ) iRow -= iExtraRows;
	}
	if ( pData != NULL && pData->bWordWrap && pData->pWrapTree != NULL ) {
		return __xuiCodeEditWrapVisualRowLine(pData, iRow, NULL);
	}
	return __xuiCodeEditDocumentVisibleRowToLine(pData, iRow);
}

static int __xuiCodeEditWrapTreePrefix(const xui_code_edit_data_t* pData,
	int iCount)
{
	int iSum = 0;

	if ( pData == NULL || pData->pWrapTree == NULL ) return 0;
	if ( iCount > pData->iWrapIndexCount ) iCount = pData->iWrapIndexCount;
	while ( iCount > 0 ) {
		iSum += pData->pWrapTree[iCount];
		iCount -= iCount & -iCount;
	}
	return iSum;
}

static void __xuiCodeEditWrapTreeSet(xui_code_edit_data_t* pData,
	int iIndex, int iRows)
{
	int iDelta;
	int i;

	if ( pData == NULL || pData->pWrapRows == NULL ||
		pData->pWrapTree == NULL || iIndex < 0 ||
		iIndex >= pData->iWrapIndexCount ) return;
	if ( iRows < 1 ) iRows = 1;
	iDelta = iRows - pData->pWrapRows[iIndex];
	if ( iDelta == 0 ) return;
	pData->pWrapRows[iIndex] = iRows;
	for ( i = iIndex + 1; i <= pData->iWrapIndexCount; i += i & -i ) {
		pData->pWrapTree[i] += iDelta;
	}
}

static uint32_t __xuiCodeEditWrapFoldSignature(xui_code_edit_data_t* pData)
{
	xui_code_fold_range_t tRange;
	uint32_t iHash = 2166136261u;
	int i;
	int iCount;

	if ( pData == NULL || pData->pFoldState == NULL ) return iHash;
	iCount = xuiCodeFoldStateGetCount(pData->pFoldState);
	for ( i = 0; i < iCount; i++ ) {
		memset(&tRange, 0, sizeof(tRange));
		if ( xuiCodeFoldStateGetRange(pData->pFoldState, i, &tRange) != XUI_OK ) continue;
		iHash = (iHash ^ (uint32_t)tRange.iStartLine) * 16777619u;
		iHash = (iHash ^ (uint32_t)tRange.iEndLine) * 16777619u;
		iHash = (iHash ^ tRange.iFlags) * 16777619u;
	}
	return iHash;
}

static void __xuiCodeEditWrapIndexClear(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return;
	xrtFree(pData->pWrapRows);
	xrtFree(pData->pWrapTree);
	xrtFree(pData->pWrapMeasured);
	pData->pWrapRows = NULL;
	pData->pWrapTree = NULL;
	pData->pWrapMeasured = NULL;
	pData->iWrapIndexCount = 0;
	pData->iWrapDocumentVersion = 0u;
	pData->iWrapFoldSignature = 0u;
	pData->fWrapLayoutWidth = 0.0f;
	pData->pWrapLayoutFont = NULL;
	pData->iWrapLayoutTabColumns = 0;
}

static int __xuiCodeEditWrapIndexReset(xui_code_edit_data_t* pData,
	int iCount)
{
	int* pRows;
	int* pTree;
	unsigned char* pMeasured;
	int i;

	if ( iCount < 1 ) iCount = 1;
	pRows = (int*)xrtMalloc(sizeof(*pRows) * (size_t)iCount);
	pTree = (int*)xrtMalloc(sizeof(*pTree) * (size_t)(iCount + 1));
	pMeasured = (unsigned char*)xrtCalloc((size_t)iCount, sizeof(*pMeasured));
	if ( pRows == NULL || pTree == NULL || pMeasured == NULL ) {
		xrtFree(pRows);
		xrtFree(pTree);
		xrtFree(pMeasured);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iCount; i++ ) pRows[i] = 1;
	pTree[0] = 0;
	for ( i = 1; i <= iCount; i++ ) pTree[i] = i & -i;
	xrtFree(pData->pWrapRows);
	xrtFree(pData->pWrapTree);
	xrtFree(pData->pWrapMeasured);
	pData->pWrapRows = pRows;
	pData->pWrapTree = pTree;
	pData->pWrapMeasured = pMeasured;
	pData->iWrapIndexCount = iCount;
	return XUI_OK;
}

static int __xuiCodeEditWrapPrepare(xui_widget pWidget,
	xui_code_edit_data_t* pData, float fWrapWidth)
{
	xui_code_range_t tRange;
	xui_font pFont;
	uint32_t iVersion;
	uint32_t iFoldSignature;
	int iVisibleCount;
	int iStartLine;
	int iEndLine;
	int iStartRow;
	int iEndRow;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pData->pDocument == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pData->bWordWrap ) {
		__xuiCodeEditWrapIndexClear(pData);
		return XUI_OK;
	}
	if ( fWrapWidth < 1.0f ) fWrapWidth = 1.0f;
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	iFoldSignature = __xuiCodeEditWrapFoldSignature(pData);
	iVisibleCount = __xuiCodeEditDocumentVisibleLineCount(pData);
	if ( pData->pWrapRows == NULL || pData->iWrapIndexCount != iVisibleCount ||
		pData->iWrapFoldSignature != iFoldSignature ||
		fabsf(pData->fWrapLayoutWidth - fWrapWidth) >= 0.01f ||
		pData->pWrapLayoutFont != pFont ||
		pData->iWrapLayoutTabColumns != __xuiCodeEditTabColumns(pData) ) {
		iRet = __xuiCodeEditWrapIndexReset(pData, iVisibleCount);
		if ( iRet != XUI_OK ) return iRet;
		pData->iWrapFoldSignature = iFoldSignature;
		pData->fWrapLayoutWidth = fWrapWidth;
		pData->pWrapLayoutFont = pFont;
		pData->iWrapLayoutTabColumns = __xuiCodeEditTabColumns(pData);
		pData->iWrapDocumentVersion = iVersion;
		return XUI_OK;
	}
	if ( pData->iWrapDocumentVersion == iVersion ) return XUI_OK;
	memset(&tRange, 0, sizeof(tRange));
	if ( xuiCodeDocumentGetLastEditRange(pData->pDocument, &tRange) != XUI_OK ||
		xuiCodeDocumentOffsetToLineColumn(pData->pDocument, tRange.iStart,
			&iStartLine, NULL) != XUI_OK ||
		xuiCodeDocumentOffsetToLineColumn(pData->pDocument, tRange.iEnd,
			&iEndLine, NULL) != XUI_OK ) {
		iStartRow = 0;
		iEndRow = pData->iWrapIndexCount - 1;
	} else {
		iStartRow = __xuiCodeEditLineToDocumentVisibleRow(pData, iStartLine);
		iEndRow = __xuiCodeEditLineToDocumentVisibleRow(pData, iEndLine);
		if ( iStartRow < 0 ) iStartRow = 0;
		if ( iEndRow < iStartRow ) iEndRow = iStartRow;
		if ( iEndRow >= pData->iWrapIndexCount ) iEndRow = pData->iWrapIndexCount - 1;
	}
	for ( i = iStartRow; i <= iEndRow; i++ ) {
		pData->pWrapMeasured[i] = 0u;
		__xuiCodeEditWrapTreeSet(pData, i, 1);
	}
	pData->iWrapDocumentVersion = iVersion;
	return XUI_OK;
}

static int __xuiCodeEditWrapEnsureLine(xui_widget pWidget,
	xui_code_edit_data_t* pData, int iLine, float fWrapWidth,
	xui_code_edit_line_layout_t** ppLayout)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_code_edit_line_layout_t* pLayout;
	const char* sText;
	float fColumnWidth;
	int iStart;
	int iEnd;
	int iRow;
	int iRows;
	int iRet;

	if ( ppLayout != NULL ) *ppLayout = NULL;
	if ( pWidget == NULL || pData == NULL || iLine < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditReadRange(pData, iStart, iEnd, &sText);
	if ( iRet != XUI_OK ) return iRet;
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	pLayout = __xuiCodeEditLineLayoutEnsure(pProxy, pFont, pData,
		sText, iEnd - iStart, fColumnWidth);
	if ( pLayout == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiCodeEditLineLayoutBuildWraps(pLayout, fWrapWidth);
	if ( iRet != XUI_OK ) return iRet;
	iRow = __xuiCodeEditLineToDocumentVisibleRow(pData, iLine);
	if ( iRow >= 0 && iRow < pData->iWrapIndexCount ) {
		iRows = pLayout->iWrapOffsetCount - 1;
		if ( iRows < 1 ) iRows = 1;
		__xuiCodeEditWrapTreeSet(pData, iRow, iRows);
		pData->pWrapMeasured[iRow] = 1u;
	}
	if ( ppLayout != NULL ) *ppLayout = pLayout;
	return XUI_OK;
}

static int __xuiCodeEditWrapLineVisualRow(xui_code_edit_data_t* pData,
	int iLine)
{
	int iRow;

	if ( pData == NULL || !pData->bWordWrap || pData->pWrapTree == NULL ) {
		return __xuiCodeEditLineToDocumentVisibleRow(pData, iLine);
	}
	iRow = __xuiCodeEditLineToDocumentVisibleRow(pData, iLine);
	if ( iRow <= 0 ) return 0;
	return __xuiCodeEditWrapTreePrefix(pData, iRow);
}

static int __xuiCodeEditWrapLineRows(xui_code_edit_data_t* pData, int iLine)
{
	int iRow;

	if ( pData == NULL || !pData->bWordWrap || pData->pWrapRows == NULL ) return 1;
	iRow = __xuiCodeEditLineToDocumentVisibleRow(pData, iLine);
	if ( iRow < 0 || iRow >= pData->iWrapIndexCount ) return 1;
	return pData->pWrapRows[iRow] > 0 ? pData->pWrapRows[iRow] : 1;
}

static int __xuiCodeEditWrapTotalRows(xui_code_edit_data_t* pData)
{
	if ( pData == NULL || pData->pWrapTree == NULL ) {
		return __xuiCodeEditDocumentVisibleLineCount(pData);
	}
	return __xuiCodeEditWrapTreePrefix(pData, pData->iWrapIndexCount);
}

static int __xuiCodeEditWrapVisualRowLine(xui_code_edit_data_t* pData,
	int iVisualRow, int* pSegmentRow)
{
	int iIndex;
	int iBit;
	int iSum;
	int iNext;

	if ( pSegmentRow != NULL ) *pSegmentRow = 0;
	if ( pData == NULL || pData->pWrapTree == NULL ||
		pData->iWrapIndexCount <= 0 ) {
		return __xuiCodeEditDocumentVisibleRowToLine(pData, iVisualRow);
	}
	if ( iVisualRow < 0 ) iVisualRow = 0;
	if ( iVisualRow >= __xuiCodeEditWrapTotalRows(pData) ) {
		iVisualRow = __xuiCodeEditWrapTotalRows(pData) - 1;
	}
	iIndex = 0;
	iSum = 0;
	for ( iBit = 1; (iBit << 1) <= pData->iWrapIndexCount; iBit <<= 1 ) {}
	for ( ; iBit != 0; iBit >>= 1 ) {
		iNext = iIndex + iBit;
		if ( iNext <= pData->iWrapIndexCount &&
			iSum + pData->pWrapTree[iNext] <= iVisualRow ) {
			iIndex = iNext;
			iSum += pData->pWrapTree[iNext];
		}
	}
	if ( iIndex >= pData->iWrapIndexCount ) iIndex = pData->iWrapIndexCount - 1;
	if ( pSegmentRow != NULL ) *pSegmentRow = iVisualRow - iSum;
	return __xuiCodeEditDocumentVisibleRowToLine(pData, iIndex);
}

static int __xuiCodeEditWrapMeasureViewport(xui_widget pWidget,
	xui_code_edit_data_t* pData, float fWrapWidth, float fLineHeight,
	float fViewportHeight)
{
	int iFirstRow;
	int iRowCount;
	int iRow;
	int iLine;
	int iPreviousLine;
	int iPass;
	int iRet;

	if ( pData == NULL || !pData->bWordWrap ) return XUI_OK;
	iRet = __xuiCodeEditWrapPrepare(pWidget, pData, fWrapWidth);
	if ( iRet != XUI_OK ) return iRet;
	if ( fLineHeight <= 0.0f ) fLineHeight = 1.0f;
	iFirstRow = (int)(pData->fScrollY / fLineHeight) - 2;
	if ( iFirstRow < 0 ) iFirstRow = 0;
	/* A width change resets unmeasured lines to one row. Measure the last
	   known row instead of skipping all lines at the old scroll offset. */
	if ( iFirstRow >= __xuiCodeEditWrapTotalRows(pData) ) {
		iFirstRow = __xuiCodeEditWrapTotalRows(pData) - 1;
		if ( iFirstRow < 0 ) iFirstRow = 0;
	}
	iRowCount = (int)(fViewportHeight / fLineHeight) + 6;
	if ( iRowCount < 8 ) iRowCount = 8;
	for ( iRow = iFirstRow; iRow < iFirstRow + iRowCount; iRow++ ) {
		if ( iRow >= __xuiCodeEditWrapTotalRows(pData) ) break;
		iPreviousLine = -1;
		for ( iPass = 0; iPass < 3; iPass++ ) {
			iLine = __xuiCodeEditWrapVisualRowLine(pData, iRow, NULL);
			if ( iLine == iPreviousLine ) break;
			iPreviousLine = iLine;
			iRet = __xuiCodeEditWrapEnsureLine(pWidget, pData, iLine,
				fWrapWidth, NULL);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiCodeEditSetError(xui_code_edit_data_t* pData, const char* sError)
{
	if ( pData == NULL ) return;
	if ( sError == NULL ) sError = "";
	strncpy(pData->sError, sError, sizeof(pData->sError) - 1u);
	pData->sError[sizeof(pData->sError) - 1u] = '\0';
}

static int __xuiCodeEditDescValid(const xui_code_edit_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( pDesc->iTabColumns < 0 || pDesc->iIndentColumns < 0 ) return 0;
	return 1;
}

static xui_code_edit_data_t* __xuiCodeEditGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "codeedit");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_code_edit_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiCodeEditAdapterCommand(xui_widget pWidget, int iCommand)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	int bHandled = 0;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditExecuteCommand(pWidget, pData, iCommand, NULL, &bHandled);
	return (iRet == XUI_OK && !bHandled) ? XUI_ERROR_UNSUPPORTED : iRet;
}

static int __xuiCodeEditAdapterSetText(xui_widget pWidget, const char* sText)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_selection_t tSelection;
	int iRet;
	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeEditSetText(pWidget, sText);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tSelection, 0, sizeof(tSelection));
	(void)xuiCodeSelectionGetState(pData->pSelection, &tSelection);
	pData->iEditNotifyVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	pData->iEditNotifyAnchor = tSelection.iAnchorOffset;
	pData->iEditNotifyCaret = tSelection.iCaretOffset;
	(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_TEXT_CHANGED, NULL,
		xuiCodeDocumentGetLength(pData->pDocument), tSelection.iAnchorOffset,
		tSelection.iCaretOffset, 0, 0, 1);
	return XUI_OK;
}

static int __xuiCodeEditAdapterSetSelection(xui_widget pWidget, int iStart, int iEnd)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, iStart, iEnd);
	if ( iRet == XUI_OK ) {
		(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_SELECTION_CHANGED, NULL,
			xuiCodeDocumentGetLength(pData->pDocument), iStart, iEnd, 0, 0, 1);
		pData->iEditNotifyAnchor = iStart;
		pData->iEditNotifyCaret = iEnd;
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditAdapterGetSelection(xui_widget pWidget, int* pStart, int* pEnd)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? xuiCodeSelectionGetRange(pData->pSelection, pStart, pEnd) : XUI_ERROR_INVALID_ARGUMENT;
}

static int __xuiCodeEditAdapterHasSelection(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? xuiCodeSelectionHasSelection(pData->pSelection) : 0;
}

static int __xuiCodeEditAdapterSelectAll(xui_widget pWidget)
{
	return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_SELECT_ALL);
}

static int __xuiCodeEditAdapterCopy(xui_widget pWidget) { return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_COPY); }
static int __xuiCodeEditAdapterCut(xui_widget pWidget) { return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_CUT); }
static int __xuiCodeEditAdapterPaste(xui_widget pWidget) { return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_PASTE); }
static int __xuiCodeEditAdapterUndo(xui_widget pWidget) { return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_UNDO); }
static int __xuiCodeEditAdapterRedo(xui_widget pWidget) { return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_REDO); }

static int __xuiCodeEditAdapterDeleteSelection(xui_widget pWidget)
{
	if ( !__xuiCodeEditAdapterHasSelection(pWidget) ) return XUI_OK;
	return __xuiCodeEditAdapterCommand(pWidget, XUI_CODE_COMMAND_DELETE_FORWARD);
}

static int __xuiCodeEditAdapterCanUndo(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? xuiCodeDocumentCanUndo(pData->pDocument) : 0;
}

static int __xuiCodeEditAdapterCanRedo(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? xuiCodeDocumentCanRedo(pData->pDocument) : 0;
}

static xui_rect_t __xuiCodeEditAdapterCaretRect(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_selection_t tSelection;
	xui_rect_t tRect = {0.0f, 0.0f, 0.0f, 0.0f};
	xui_rect_t tWorld;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( pData != NULL && xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK ) {
		if ( xuiCodeEditGetOffsetRect(pWidget, tSelection.iCaretOffset, &tRect) == XUI_OK ) {
			tWorld = xuiWidgetGetWorldRect(pWidget);
			tRect.fX -= tWorld.fX;
			tRect.fY -= tWorld.fY;
		}
	}
	return tRect;
}

static const xui_internal_edit_adapter_t g_xuiCodeEditAdapter = {
	XUI_EDIT_CAP_TEXT | XUI_EDIT_CAP_SELECTION | XUI_EDIT_CAP_CLIPBOARD |
	XUI_EDIT_CAP_UNDO | XUI_EDIT_CAP_READONLY | XUI_EDIT_CAP_CARET_RECT |
	XUI_EDIT_CAP_CONTEXT_MENU | XUI_EDIT_CAP_IME | XUI_EDIT_CAP_MULTILINE |
	XUI_EDIT_CAP_CODE,
	__xuiCodeEditAdapterSetText, xuiCodeEditGetText, __xuiCodeEditAdapterSetSelection,
	__xuiCodeEditAdapterGetSelection, __xuiCodeEditAdapterHasSelection,
	__xuiCodeEditAdapterSelectAll, __xuiCodeEditAdapterCopy, __xuiCodeEditAdapterCut,
	__xuiCodeEditAdapterPaste, __xuiCodeEditAdapterDeleteSelection,
	__xuiCodeEditAdapterUndo, __xuiCodeEditAdapterRedo,
	__xuiCodeEditAdapterCanUndo, __xuiCodeEditAdapterCanRedo,
	xuiCodeEditSetReadonly, xuiCodeEditIsReadonly,
	__xuiCodeEditAdapterCaretRect, xuiCodeEditOpenMenu
};

static void __xuiCodeEditDestroyOwned(xui_code_edit_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	if ( pData->pCompletionPopup != NULL ) {
		xuiWidgetDestroy(pData->pCompletionPopup);
		pData->pCompletionPopup = NULL;
		pData->pCompletionList = NULL;
	}
	if ( pData->pAssistPopup != NULL ) {
		xuiWidgetDestroy(pData->pAssistPopup);
		pData->pAssistPopup = NULL;
		pData->pAssistContent = NULL;
	}
	__xuiCodeEditCompletionItemsClear(pData);
	xrtFree(pData->pCompletionItems);
	xrtFree(pData->pCompletionVisible);
	xrtFree(pData->arrCompletionLabels);
	pData->pCompletionItems = NULL;
	pData->pCompletionVisible = NULL;
	pData->arrCompletionLabels = NULL;
	pData->iCompletionCapacity = 0;
	if ( pData->pMenu != NULL ) {
		xui_widget pPopup = xuiMenuGetPopupWidget(pData->pMenu);
		if ( pPopup != NULL ) {
			xuiWidgetDestroy(pPopup);
		} else {
			xuiWidgetDestroy(pData->pMenu);
		}
		pData->pMenu = NULL;
	}
	if ( pData->pHScrollBar != NULL ) {
		xuiWidgetDestroy(pData->pHScrollBar);
		pData->pHScrollBar = NULL;
	}
	if ( pData->pVScrollBar != NULL ) {
		xuiWidgetDestroy(pData->pVScrollBar);
		pData->pVScrollBar = NULL;
	}
	if ( pData->pFindWindow != NULL ) {
		xuiWidgetDestroy(pData->pFindWindow);
		pData->pFindWindow = NULL;
	}
	for ( i = 0; i < (int)(sizeof(pData->arrMenuTitle) / sizeof(pData->arrMenuTitle[0])); ++i ) {
		xrtFree(pData->arrMenuTitle[i]);
		pData->arrMenuTitle[i] = NULL;
	}
	__xuiCodeEditDestroyFindData(pData);
	__xuiCodeEditLineLayoutsClear(pData);
	__xuiCodeEditWrapIndexClear(pData);
	xrtFree(pData->sInlineCompletion);
    __xuiCodeEditColumnIndexClear(pData);
	xrtFree(pData->sReadBuffer);
	xrtFree(pData->sImeComposition);
	xrtFree(pData->sAssistLabel);
	xrtFree(pData->sAssistDocumentation);
	xrtFree(pData->pPlaceholders);
	pData->sInlineCompletion = NULL;
	pData->sReadBuffer = NULL;
	pData->sImeComposition = NULL;
	pData->sAssistLabel = NULL;
	pData->sAssistDocumentation = NULL;
	pData->pPlaceholders = NULL;
	pData->iInlineCompletionCapacity = 0;
	pData->iReadBufferCapacity = 0;
	pData->iImeCompositionCapacity = 0;
	xuiCodeCommandMapDestroy(pData->pCommandMap);
	xuiCodeMarginModelDestroy(pData->pMargins);
	xuiCodeProviderSetDestroy(pData->pProviders);
	xuiCodeTokenBufferDestroy(pData->pTokenBuffer);
	xuiCodeLanguageRegistryDestroy(pData->pLanguages);
	xuiCodeAnnotationStoreDestroy(pData->pAnnotations);
	xuiCodeFoldStateDestroy(pData->pFoldState);
	xuiCodeThemeDestroy(pData->pTheme);
	xuiCodeSelectionDestroy(pData->pSelection);
	xuiCodeDocumentDestroy(pData->pDocument);
	pData->pCommandMap = NULL;
	pData->pMargins = NULL;
	pData->pProviders = NULL;
	pData->pTokenBuffer = NULL;
	pData->pAnnotations = NULL;
	pData->pFoldState = NULL;
	pData->pTheme = NULL;
	pData->pSelection = NULL;
	pData->pDocument = NULL;
}

enum {
	XUI_CODE_EDIT_MENU_TITLE_UNDO = 0,
	XUI_CODE_EDIT_MENU_TITLE_REDO,
	XUI_CODE_EDIT_MENU_TITLE_CUT,
	XUI_CODE_EDIT_MENU_TITLE_COPY,
	XUI_CODE_EDIT_MENU_TITLE_PASTE,
	XUI_CODE_EDIT_MENU_TITLE_DELETE,
	XUI_CODE_EDIT_MENU_TITLE_SELECT_ALL,
	XUI_CODE_EDIT_MENU_TITLE_FIND,
	XUI_CODE_EDIT_MENU_TITLE_REPLACE,
	XUI_CODE_EDIT_MENU_TITLE_GOTO_LINE,
	XUI_CODE_EDIT_MENU_TITLE_TOGGLE_COMMENT,
	XUI_CODE_EDIT_MENU_TITLE_TOGGLE_FOLD,
	XUI_CODE_EDIT_MENU_TITLE_COUNT
};

static int __xuiCodeEditMenuTitleIndexForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_UNDO: return XUI_CODE_EDIT_MENU_TITLE_UNDO;
	case XUI_CODE_COMMAND_REDO: return XUI_CODE_EDIT_MENU_TITLE_REDO;
	case XUI_CODE_COMMAND_CUT: return XUI_CODE_EDIT_MENU_TITLE_CUT;
	case XUI_CODE_COMMAND_COPY: return XUI_CODE_EDIT_MENU_TITLE_COPY;
	case XUI_CODE_COMMAND_PASTE: return XUI_CODE_EDIT_MENU_TITLE_PASTE;
	case XUI_CODE_COMMAND_DELETE_FORWARD: return XUI_CODE_EDIT_MENU_TITLE_DELETE;
	case XUI_CODE_COMMAND_SELECT_ALL: return XUI_CODE_EDIT_MENU_TITLE_SELECT_ALL;
	case XUI_CODE_COMMAND_OPEN_FIND: return XUI_CODE_EDIT_MENU_TITLE_FIND;
	case XUI_CODE_COMMAND_OPEN_REPLACE: return XUI_CODE_EDIT_MENU_TITLE_REPLACE;
	case XUI_CODE_COMMAND_GOTO_LINE: return XUI_CODE_EDIT_MENU_TITLE_GOTO_LINE;
	case XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT: return XUI_CODE_EDIT_MENU_TITLE_TOGGLE_COMMENT;
	case XUI_CODE_COMMAND_FOLD_TOGGLE: return XUI_CODE_EDIT_MENU_TITLE_TOGGLE_FOLD;
	default: return -1;
	}
}

static int __xuiCodeEditMenuTitleTranslationForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_UNDO: return XUI_TR_EDIT_UNDO;
	case XUI_CODE_COMMAND_REDO: return XUI_TR_EDIT_REDO;
	case XUI_CODE_COMMAND_CUT: return XUI_TR_EDIT_CUT;
	case XUI_CODE_COMMAND_COPY: return XUI_TR_EDIT_COPY;
	case XUI_CODE_COMMAND_PASTE: return XUI_TR_EDIT_PASTE;
	case XUI_CODE_COMMAND_DELETE_FORWARD: return XUI_TR_EDIT_DELETE;
	case XUI_CODE_COMMAND_SELECT_ALL: return XUI_TR_EDIT_SELECT_ALL;
	case XUI_CODE_COMMAND_OPEN_FIND: return XUI_TR_FIND_TITLE;
	case XUI_CODE_COMMAND_OPEN_REPLACE: return XUI_TR_REPLACE_TITLE;
	case XUI_CODE_COMMAND_GOTO_LINE: return XUI_TR_CODE_GOTO_LINE;
	case XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT: return XUI_TR_CODE_TOGGLE_COMMENT;
	case XUI_CODE_COMMAND_FOLD_TOGGLE: return XUI_TR_CODE_TOGGLE_FOLD;
	default: return 0;
	}
}

static const char* __xuiCodeEditMenuTitleForCommand(xui_widget pWidget, xui_code_edit_data_t* pData, int iCommand)
{
	xui_context pContext;
	int iIndex;
	int iTranslation;

	if ( pData == NULL ) return "";
	iIndex = __xuiCodeEditMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_CODE_EDIT_MENU_TITLE_COUNT ) return "";
	if ( pData->arrMenuTitle[iIndex] != NULL ) return pData->arrMenuTitle[iIndex];
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	iTranslation = __xuiCodeEditMenuTitleTranslationForCommand(iCommand);
	return (iTranslation != 0) ? xuiTranslate(pContext, iTranslation) : "";
}

static int __xuiCodeEditMenuCommandForValue(int iValue)
{
	switch ( iValue ) {
	case 1: return XUI_CODE_COMMAND_UNDO;
	case 2: return XUI_CODE_COMMAND_REDO;
	case 3: return XUI_CODE_COMMAND_CUT;
	case 4: return XUI_CODE_COMMAND_COPY;
	case 5: return XUI_CODE_COMMAND_PASTE;
	case 6: return XUI_CODE_COMMAND_DELETE_FORWARD;
	case 7: return XUI_CODE_COMMAND_SELECT_ALL;
	case 8: return XUI_CODE_COMMAND_OPEN_FIND;
	case 9: return XUI_CODE_COMMAND_OPEN_REPLACE;
	case 10: return XUI_CODE_COMMAND_GOTO_LINE;
	case 11: return XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT;
	case 12: return XUI_CODE_COMMAND_FOLD_TOGGLE;
	default: return 0;
	}
}

static void __xuiCodeEditMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pWidget;
	xui_code_edit_data_t* pData;
	int iCommand;
	int bHandled;

	(void)pMenu;
	(void)iIndex;
	pWidget = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	iCommand = __xuiCodeEditMenuCommandForValue(iValue);
	if ( pData == NULL || iCommand <= 0 ) return;
	(void)__xuiCodeEditExecuteCommand(pWidget, pData, iCommand, NULL, &bHandled);
}

static uint32_t __xuiCodeEditProviderCommandState(xui_widget pWidget, xui_code_edit_data_t* pData, int iCommand, uint32_t iDefaultState)
{
	int bEnabled;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return 0u;
	bEnabled = 0;
	iRet = xuiCodeProviderCanExecuteCommand(pData->pProviders, pWidget, iCommand, &bEnabled);
	if ( iRet == XUI_OK ) return bEnabled ? XUI_MENU_ITEM_ENABLED : 0u;
	if ( iRet == XUI_ERROR_UNSUPPORTED ) return iDefaultState;
	return 0u;
}

static int __xuiCodeEditUpdateMenu(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_menu_item_t arrItems[15];
	uint32_t iEnabled;
	uint32_t iEditEnabled;
	uint32_t iSelectionEnabled;
	uint32_t iUndoEnabled;
	uint32_t iRedoEnabled;
	int iLen;
	int bHasSelection;

	if ( pWidget == NULL || pData == NULL || pData->pMenu == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrItems, 0, sizeof(arrItems));
	iEnabled = XUI_MENU_ITEM_ENABLED;
	iEditEnabled = pData->bReadonly ? 0u : XUI_MENU_ITEM_ENABLED;
	bHasSelection = xuiCodeSelectionHasSelection(pData->pSelection);
	iSelectionEnabled = bHasSelection ? XUI_MENU_ITEM_ENABLED : 0u;
	iUndoEnabled = xuiCodeDocumentCanUndo(pData->pDocument) ? XUI_MENU_ITEM_ENABLED : 0u;
	iRedoEnabled = xuiCodeDocumentCanRedo(pData->pDocument) ? XUI_MENU_ITEM_ENABLED : 0u;
	iLen = xuiCodeDocumentGetLength(pData->pDocument);

	arrItems[0].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_UNDO);
	arrItems[0].sShortcut = "Ctrl+Z";
	arrItems[0].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[0].iState = iUndoEnabled;
	arrItems[0].iValue = 1;
	arrItems[1].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_REDO);
	arrItems[1].sShortcut = "Ctrl+Y";
	arrItems[1].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[1].iState = iRedoEnabled;
	arrItems[1].iValue = 2;
	arrItems[2].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[3].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_CUT);
	arrItems[3].sShortcut = "Ctrl+X";
	arrItems[3].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[3].iState = iEditEnabled & iSelectionEnabled;
	arrItems[3].iValue = 3;
	arrItems[4].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_COPY);
	arrItems[4].sShortcut = "Ctrl+C";
	arrItems[4].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[4].iState = iSelectionEnabled;
	arrItems[4].iValue = 4;
	arrItems[5].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_PASTE);
	arrItems[5].sShortcut = "Ctrl+V";
	arrItems[5].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[5].iState = iEditEnabled;
	arrItems[5].iValue = 5;
	arrItems[6].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_DELETE_FORWARD);
	arrItems[6].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[6].iState = iEditEnabled & iSelectionEnabled;
	arrItems[6].iValue = 6;
	arrItems[7].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[8].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_SELECT_ALL);
	arrItems[8].sShortcut = "Ctrl+A";
	arrItems[8].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[8].iState = (iLen > 0 && !bHasSelection) ? iEnabled : 0u;
	arrItems[8].iValue = 7;
	arrItems[9].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[10].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_OPEN_FIND);
	arrItems[10].sShortcut = "Ctrl+F";
	arrItems[10].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[10].iState = iEnabled;
	arrItems[10].iValue = 8;
	arrItems[11].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_OPEN_REPLACE);
	arrItems[11].sShortcut = "Ctrl+H";
	arrItems[11].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[11].iState = pData->bReadonly ? 0u : iEnabled;
	arrItems[11].iValue = 9;
	arrItems[12].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_GOTO_LINE);
	arrItems[12].sShortcut = "Ctrl+G";
	arrItems[12].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[12].iState = __xuiCodeEditProviderCommandState(pWidget, pData, XUI_CODE_COMMAND_GOTO_LINE, 0u);
	arrItems[12].iValue = 10;
	arrItems[13].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT);
	arrItems[13].sShortcut = "Ctrl+/";
	arrItems[13].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[13].iState = iEditEnabled;
	arrItems[13].iValue = 11;
	arrItems[14].sText = __xuiCodeEditMenuTitleForCommand(pWidget, pData, XUI_CODE_COMMAND_FOLD_TOGGLE);
	arrItems[14].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[14].iState = iEnabled;
	arrItems[14].iValue = 12;
	return xuiMenuSetItems(pData->pMenu, arrItems, 15);
}

static int __xuiCodeEditInitMenu(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pMenu, &tDesc);
	if ( iRet != XUI_OK ) {
		pData->pMenu = NULL;
		return iRet;
	}
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiCodeEditMenuSelect, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCodeEditUpdateMenu(pWidget, pData);
}

static void __xuiCodeEditHScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_code_edit_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pData->bSyncingScrollBars ) return;
	(void)xuiCodeEditSetScroll(pWidget, fValue, pData->fScrollY);
}

static void __xuiCodeEditVScrollBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_code_edit_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pData->bSyncingScrollBars ) return;
	(void)xuiCodeEditSetScroll(pWidget, pData->fScrollX, fValue);
}

static int __xuiCodeEditInitScrollBars(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_context pContext;
	xui_scrollbar_desc_t tDesc;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fMin = 0.0f;
	tDesc.fMax = 1.0f;
	tDesc.fPage = 1.0f;
	tDesc.iOrientation = XUI_ORIENTATION_HORIZONTAL;
	tDesc.iMode = XUI_SCROLLBAR_MODE_COMPACT;
	tDesc.iButtonMode = XUI_SCROLLBAR_BUTTONS_OFF;
	tDesc.fThickness = 8.0f;
	tDesc.fMinThumbSize = 18.0f;
	tDesc.iTrackColor = XUI_COLOR_RGBA(226, 232, 240, 255);
	tDesc.iThumbColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	tDesc.iHoverColor = XUI_COLOR_RGBA(100, 116, 139, 255);
	tDesc.iActiveColor = XUI_COLOR_RGBA(71, 85, 105, 255);
	tDesc.iFocusColor = XUI_COLOR_RGBA(59, 130, 246, 255);
	tDesc.iDisabledColor = XUI_COLOR_RGBA(203, 213, 225, 255);
	iRet = xuiScrollBarCreate(pContext, &pData->pHScrollBar, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pHScrollBar);
	if ( iRet != XUI_OK ) return iRet;
	tDesc.iOrientation = XUI_ORIENTATION_VERTICAL;
	iRet = xuiScrollBarCreate(pContext, &pData->pVScrollBar, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pVScrollBar);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetVisible(pData->pHScrollBar, 0);
	(void)xuiWidgetSetVisible(pData->pVScrollBar, 0);
	iRet = xuiScrollBarSetChange(pData->pHScrollBar, __xuiCodeEditHScrollBarChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetChange(pData->pVScrollBar, __xuiCodeEditVScrollBarChanged, pWidget);
	return iRet;
}

static int __xuiCodeEditQueryCursor(xui_widget pWidget, int iX, int iY, void* pUser)
{
	(void)iX;
	(void)iY;
	(void)pUser;
	return xuiWidgetGetEnabled(pWidget) ? XUI_CURSOR_IBEAM : XUI_CURSOR_NOT_ALLOWED;
}

static void __xuiCodeEditSyncAssistColors(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	uint32_t arrColors[3];
	arrColors[0] = __xuiCodeEditColor(pWidget, "codeedit.assist.active_color", XUI_COLOR_RGBA(219,234,254,255));
	arrColors[1] = __xuiCodeEditColor(pWidget, "codeedit.assist.text.color", XUI_COLOR_RGBA(15,23,42,255));
	arrColors[2] = __xuiCodeEditColor(pWidget, "codeedit.assist.documentation.color", XUI_COLOR_RGBA(71,85,105,255));
	if ( memcmp(arrColors, pData->arrAssistColors, sizeof(arrColors)) == 0 ) return;
	memcpy(pData->arrAssistColors, arrColors, sizeof(arrColors));
	/* The assist content lives in the overlay tree, outside the editor's cache. */
	if ( pData->pAssistContent != NULL )
		(void)xuiWidgetInvalidate(pData->pAssistContent, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCodeEditAssistCacheRender(xui_widget pWidget, xui_draw_context pDraw,
	uint32_t iStateId, void* pUser)
{
	xui_widget pCodeEdit = (xui_widget)pUser;
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pCodeEdit);
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_rect_t tRect;
	xui_rect_t tLabel;
	float fPrefix;
	float fActive;
	int iLabelLength;
	int iStart;
	int iEnd;
	int iRet;

	(void)iStateId;
	if ( pWidget == NULL || pDraw == NULL || pData == NULL || pData->sAssistLabel == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiCodeEditSyncAssistColors(pCodeEdit, pData);
	pContext = xuiWidgetGetContext(pCodeEdit);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	if ( pProxy == NULL || pProxy->drawText == NULL || pFont == NULL ) return XUI_OK;
	tRect = xuiWidgetGetContentRect(pWidget);
	tLabel = (xui_rect_t){tRect.fX + 8.0f, tRect.fY + 4.0f, tRect.fW - 16.0f, 26.0f};
	iLabelLength = (int)strlen(pData->sAssistLabel);
	iStart = pData->tAssistActiveRange.iStart;
	iEnd = pData->tAssistActiveRange.iEnd;
	if ( iStart < 0 ) iStart = 0;
	if ( iEnd < iStart ) iEnd = iStart;
	if ( iEnd > iLabelLength ) iEnd = iLabelLength;
	if ( iStart < iEnd && pProxy->drawRectFill != NULL ) {
		fPrefix = __xuiCodeEditMeasureTextRange(pProxy, pFont,
			pData->sAssistLabel, 0, iStart, (float)iStart * 8.0f);
		fActive = __xuiCodeEditMeasureTextRange(pProxy, pFont,
			pData->sAssistLabel, iStart, iEnd, (float)(iEnd - iStart) * 8.0f);
		(void)pProxy->drawRectFill(pProxy, pDraw,
			xuiInternalSnapRect((xui_rect_t){tLabel.fX + fPrefix - 2.0f,
				tLabel.fY + 3.0f, fActive + 4.0f, tLabel.fH - 6.0f}),
			pData->arrAssistColors[0]);
	}
	iRet = pProxy->drawText(pProxy, pDraw, pFont, pData->sAssistLabel, tLabel,
		pData->arrAssistColors[1],
		XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->sAssistDocumentation != NULL && pData->sAssistDocumentation[0] != '\0' ) {
		iRet = pProxy->drawText(pProxy, pDraw, pFont, pData->sAssistDocumentation,
			(xui_rect_t){tRect.fX + 8.0f, tRect.fY + 30.0f, tRect.fW - 16.0f, tRect.fH - 34.0f},
			pData->arrAssistColors[2],
			XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return iRet;
}

static int __xuiCodeEditInitAssist(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_popup_desc_t tPopup;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tPopup, 0, sizeof(tPopup));
	tPopup.iSize = sizeof(tPopup);
	tPopup.pOwner = pWidget;
	tPopup.fContentWidth = 480.0f;
	tPopup.fContentHeight = 64.0f;
	tPopup.fMaxWidth = 640.0f;
	tPopup.fMaxHeight = 160.0f;
	tPopup.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tPopup.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopup.iOutsidePolicy = XUI_POPUP_OUTSIDE_IGNORE;
	tPopup.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tPopup.iEscapePolicy = XUI_POPUP_ESCAPE_IGNORE;
	tPopup.iFocusPolicy = XUI_POPUP_FOCUS_NONE;
	tPopup.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopup.fPadding = 0.0f;
	tPopup.fBorderWidth = 1.0f;
	tPopup.iPanelColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	tPopup.iBorderColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pAssistPopup, &tPopup);
	if ( iRet != XUI_OK ) return iRet;
	pData->pAssistContent = xuiPopupGetContentWidget(pData->pAssistPopup);
	if ( pData->pAssistContent == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetSetCacheRenderCallback(pData->pAssistContent,
		__xuiCodeEditAssistCacheRender, pWidget);
}

static int __xuiCodeEditInitCompletion(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_popup_desc_t tPopup;
	xui_list_view_desc_t tList;
	xui_widget pContent;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tPopup, 0, sizeof(tPopup));
	tPopup.iSize = sizeof(tPopup);
	tPopup.pOwner = pWidget;
	tPopup.fContentWidth = 360.0f;
	tPopup.fContentHeight = 192.0f;
	tPopup.fMaxHeight = 192.0f;
	tPopup.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tPopup.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopup.iOutsidePolicy = XUI_POPUP_OUTSIDE_IGNORE;
	tPopup.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tPopup.iEscapePolicy = XUI_POPUP_ESCAPE_IGNORE;
	tPopup.iFocusPolicy = XUI_POPUP_FOCUS_NONE;
	tPopup.bConsumeInside = 1;
	tPopup.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopup.fPadding = 0.0f;
	tPopup.fBorderWidth = 1.0f;
	tPopup.iPanelColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	tPopup.iBorderColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pCompletionPopup, &tPopup);
	if ( iRet != XUI_OK ) return iRet;
	pContent = xuiPopupGetContentWidget(pData->pCompletionPopup);
	if ( pContent == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	memset(&tList, 0, sizeof(tList));
	tList.iSize = sizeof(tList);
	tList.pFont = pData->pFont;
	tList.fItemHeight = 24.0f;
	tList.fPadding = 6.0f;
	tList.iSelectionMode = XUI_SELECTION_SINGLE;
	tList.iSelected = -1;
	tList.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tList.iBackgroundColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	tList.iBorderColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tList.iHoverColor = XUI_COLOR_RGBA(226, 232, 240, 255);
	tList.iSelectedColor = XUI_COLOR_RGBA(37, 99, 235, 255);
	tList.iTextColor = XUI_COLOR_RGBA(30, 41, 59, 255);
	iRet = xuiListViewCreate(xuiWidgetGetContext(pWidget), &pData->pCompletionList, &tList);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pContent, pData->pCompletionList);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetFocusable(pData->pCompletionList, 0);
	iRet = xuiWidgetSetRect(pData->pCompletionList, (xui_rect_t){0.0f, 0.0f, 360.0f, 192.0f});
	if ( iRet != XUI_OK ) return iRet;
	return xuiListViewSetSelect(pData->pCompletionList,
		__xuiCodeEditCompletionListSelected, pWidget);
}

static int __xuiCodeEditInitOwned(xui_code_edit_data_t* pData, const xui_code_edit_desc_t* pDesc)
{
	int iRet;

	iRet = xuiCodeDocumentCreate(&pData->pDocument);
	if ( iRet == XUI_OK ) iRet = xuiCodeSelectionCreate(&pData->pSelection);
	if ( iRet == XUI_OK ) iRet = xuiCodeThemeCreate(&pData->pTheme);
	if ( iRet == XUI_OK ) iRet = xuiCodeFoldStateCreate(&pData->pFoldState);
	if ( iRet == XUI_OK ) iRet = xuiCodeAnnotationStoreCreate(&pData->pAnnotations);
	if ( iRet == XUI_OK ) iRet = xuiCodeTokenBufferCreate(&pData->pTokenBuffer);
	if ( iRet == XUI_OK ) iRet = xuiCodeProviderSetCreate(&pData->pProviders);
	if ( iRet == XUI_OK ) iRet = xuiCodeMarginModelCreate(&pData->pMargins);
	if ( iRet == XUI_OK ) iRet = xuiCodeCommandMapCreate(&pData->pCommandMap);
	if ( iRet == XUI_OK ) iRet = xuiCodeLanguageRegistryCreate(&pData->pLanguages);
	if ( iRet == XUI_OK ) iRet = xuiCodeLanguageRegistryLoadDefaults(pData->pLanguages);
	if ( iRet == XUI_OK ) {
		iRet = xuiCodeMarginModelLoadDefaults(
			pData->pMargins,
			(pDesc == NULL) ? 1 : pDesc->bShowLineNumbers,
			(pDesc == NULL) ? 1 : pDesc->bShowMarkerMargin,
			(pDesc == NULL) ? 1 : pDesc->bShowFoldMargin,
			(pDesc == NULL) ? 1 : pDesc->bShowDiagnosticMargin);
	}
	if ( iRet != XUI_OK ) {
		__xuiCodeEditDestroyOwned(pData);
		__xuiCodeEditSetError(pData, "CodeEdit owned state create failed");
		return iRet;
	}
	return XUI_OK;
}

static void __xuiCodeEditBuildIndent(xui_code_edit_data_t* pData, char* sIndent, int iCapacity)
{
	int i;
	int iColumns;

	if ( sIndent == NULL || iCapacity <= 0 ) return;
	if ( pData == NULL ||
	     (pData->iFlags & XUI_CODE_EDIT_INDENT_WITH_TABS) != 0 ||
	     (pData->iFlags & XUI_CODE_EDIT_EXPAND_TABS) == 0 ) {
		sIndent[0] = '\t';
		if ( iCapacity > 1 ) sIndent[1] = '\0';
		return;
	}
	iColumns = __xuiCodeEditIndentColumns(pData);
	if ( iColumns >= iCapacity ) iColumns = iCapacity - 1;
	for ( i = 0; i < iColumns; i++ ) sIndent[i] = ' ';
	sIndent[iColumns] = '\0';
}

static int __xuiCodeEditCommandContext(xui_widget pWidget, xui_code_edit_data_t* pData, xui_proxy_t* pProxy, xui_code_command_context_t* pContext)
{
	xui_code_language_t tLanguage;
	xui_rect_t tRect;
	const char* sLineComment;
	float fLineHeight;
	int iPageLines;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pContext, 0, sizeof(*pContext));
	if ( pProxy != NULL ) {
		iRet = xuiGetProxy(xuiWidgetGetContext(pWidget), pProxy);
		if ( iRet == XUI_OK ) pContext->pProxy = pProxy;
	}
	pContext->iSize = sizeof(*pContext);
	pContext->pWidget = pWidget;
	pContext->pDocument = pData->pDocument;
	pContext->pSelection = pData->pSelection;
	pContext->pFoldState = pData->pFoldState;
	pContext->pProviders = pData->pProviders;
	pContext->bReadonly = pData->bReadonly;
	pContext->iIndentColumns = __xuiCodeEditIndentColumns(pData);
	tRect = xuiWidgetGetRect(pWidget);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	iPageLines = (fLineHeight > 0.0f) ? (int)((tRect.fH - 8.0f) / fLineHeight) : 0;
	pContext->iPageLineCount = (iPageLines > 1) ? iPageLines : 1;
	sLineComment = "//";
	memset(&tLanguage, 0, sizeof(tLanguage));
	if ( pData->sLanguage[0] != '\0' &&
	     xuiCodeLanguageRegistryFind(pData->pLanguages, pData->sLanguage, &tLanguage) == XUI_OK &&
	     tLanguage.sLineComment != NULL ) {
		sLineComment = tLanguage.sLineComment;
	}
	pContext->sLineComment = sLineComment;
	return XUI_OK;
}

static int __xuiCodeEditUpdateScrollModel(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_rect_t tRect;
	xui_rect_t tViewport;
	float fMarginWidth;
	float fColumnWidth;
	float fLineHeight;
	float fContentWidth;
	float fContentHeight;
	float fScrollX;
	float fScrollY;
	float fMaxX;
	float fMaxY;
	float fBarSize;
	float fMinimapWidth;
	int bShowH;
	int bShowV;
	int bNextH;
	int bNextV;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect = xuiWidgetGetContentRect(pWidget);
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	fBarSize = 8.0f;
	fMinimapWidth = ((pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_MINIMAP) != 0) ?
		pData->fMinimapWidth : 0.0f;
	if ( fMinimapWidth < 0.0f ) fMinimapWidth = 0.0f;
	if ( fMinimapWidth > tRect.fW * 0.35f ) fMinimapWidth = tRect.fW * 0.35f;
	if ( pData->bWordWrap ) {
		fContentWidth = tRect.fW;
		fContentHeight = tRect.fH;
		pData->fScrollX = 0.0f;
	} else {
		__xuiCodeEditObserveVisibleLineLengths(pData, fColumnWidth, fLineHeight,
			tRect.fW, tRect.fH);
		fContentWidth = fMarginWidth + 8.0f +
			(float)__xuiCodeEditMaxLineLength(pData) * fColumnWidth;
		fContentHeight = 8.0f +
			(float)__xuiCodeEditVisibleLineCount(pData) * fLineHeight;
	}
	if ( fContentWidth < tRect.fW ) fContentWidth = tRect.fW;
	if ( fContentHeight < tRect.fH ) fContentHeight = tRect.fH;
	bShowH = 0;
	/* Start wrapped layout at its settled width; toggling the vertical bar
	   on every update discards the lazy row index. */
	bShowV = pData->bWordWrap ? pData->bShowVScrollBar : 0;
	for ( i = 0; i < 4; i++ ) {
		tViewport = (xui_rect_t){
			0.0f,
			0.0f,
			__xuiCodeEditMaxFloat(0.0f, tRect.fW - fMinimapWidth - (bShowV ? fBarSize : 0.0f)),
			__xuiCodeEditMaxFloat(0.0f, tRect.fH - (bShowH ? fBarSize : 0.0f))
		};
		if ( pData->bWordWrap ) {
			iRet = __xuiCodeEditWrapMeasureViewport(pWidget, pData,
				__xuiCodeEditMaxFloat(1.0f,
					tViewport.fW - fMarginWidth - 8.0f),
				fLineHeight, tViewport.fH);
			if ( iRet != XUI_OK ) return iRet;
			fContentWidth = tViewport.fW;
			fContentHeight = 8.0f +
				(float)__xuiCodeEditVisibleLineCount(pData) * fLineHeight;
			if ( fContentHeight < tRect.fH ) fContentHeight = tRect.fH;
		}
		bNextH = pData->bWordWrap ? 0 :
			(fContentWidth > (tViewport.fW + 0.01f));
		bNextV = fContentHeight > (tViewport.fH + 0.01f);
		if ( fMinimapWidth > 0.0f ) bNextV = 0;
		if ( bNextH == bShowH && bNextV == bShowV ) break;
		bShowH = bNextH;
		bShowV = bNextV;
	}
	tViewport = (xui_rect_t){
		0.0f,
		0.0f,
		__xuiCodeEditMaxFloat(0.0f, tRect.fW - fMinimapWidth - (bShowV ? fBarSize : 0.0f)),
		__xuiCodeEditMaxFloat(0.0f, tRect.fH - (bShowH ? fBarSize : 0.0f))
	};
	pData->bShowHScrollBar = bShowH;
	pData->bShowVScrollBar = bShowV;
	pData->tScrollViewportRect = tViewport;
	pData->tHScrollBarRect = bShowH ? (xui_rect_t){0.0f, tViewport.fH, tViewport.fW, fBarSize} : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tMinimapRect = (fMinimapWidth > 0.0f) ?
		(xui_rect_t){tViewport.fW, 0.0f, fMinimapWidth, tRect.fH} :
		(xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tVScrollBarRect = bShowV ?
		(xui_rect_t){tViewport.fW + fMinimapWidth, 0.0f, fBarSize, tViewport.fH} :
		(xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( bShowH && !bShowV ) pData->tHScrollBarRect.fW = tRect.fW - fMinimapWidth;
	iRet = xuiScrollModelSetViewport(&pData->tScrollModel, tViewport);
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetContentSize(&pData->tScrollModel, fContentWidth, fContentHeight);
	if ( iRet == XUI_OK ) iRet = xuiScrollModelSetOffset(&pData->tScrollModel, pData->fScrollX, pData->fScrollY);
	if ( iRet == XUI_OK ) {
		(void)xuiScrollModelGetOffset(&pData->tScrollModel, &fScrollX, &fScrollY);
		pData->fScrollX = fScrollX;
		pData->fScrollY = fScrollY;
	}
	if ( iRet == XUI_OK && pData->pHScrollBar != NULL && pData->pVScrollBar != NULL ) {
		(void)xuiScrollModelGetMaxOffset(&pData->tScrollModel, &fMaxX, &fMaxY);
		pData->bSyncingScrollBars = 1;
		iRet = xuiWidgetSetRect(pData->pHScrollBar, pData->tHScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetRect(pData->pVScrollBar, pData->tVScrollBarRect);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pHScrollBar, bShowH);
		if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pVScrollBar, bShowV);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pHScrollBar, 0.0f, fMaxX, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pVScrollBar, 0.0f, fMaxY, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pHScrollBar, fColumnWidth, tViewport.fW);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pVScrollBar, fLineHeight, tViewport.fH);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pHScrollBar, pData->fScrollX);
		if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pVScrollBar, pData->fScrollY);
		pData->bSyncingScrollBars = 0;
		if ( iRet != XUI_OK ) return iRet;
	}
	return iRet;
}

static int __xuiCodeEditExecuteCommand(xui_widget pWidget, xui_code_edit_data_t* pData, int iCommand, const void* pCommandData, int* pHandled)
{
	xui_code_command_context_t tContext;
	xui_proxy_t tProxy;
	char* sClipboard;
	char sIndent[16];
	int iClipboardSize;
	int iRet;

	if ( pHandled != NULL ) *pHandled = 0;
	if ( pData == NULL || iCommand <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_OPEN_FIND:
		iRet = xuiCodeEditOpenFind(pWidget);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_OPEN_REPLACE:
		iRet = xuiCodeEditOpenReplace(pWidget);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_ACCEPT_INLINE_COMPLETION:
		iRet = xuiCodeEditAcceptInlineCompletion(pWidget);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_CANCEL_INLINE_COMPLETION:
		iRet = xuiCodeEditClearInlineCompletion(pWidget);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_SHOW_COMPLETION:
		iRet = __xuiCodeEditShowCompletionInternal(pWidget, pData, 1);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_SHOW_SIGNATURE_HELP:
		iRet = __xuiCodeEditRequestSignatureInternal(pWidget, pData);
		if ( pHandled != NULL ) *pHandled = 1;
		return iRet;
	case XUI_CODE_COMMAND_FIND_NEXT:
		iRet = xuiCodeEditFindNext(pWidget, NULL);
		if ( pHandled != NULL ) *pHandled = 1;
		return (iRet == XUI_ERROR_INVALID_ARGUMENT) ? XUI_ERROR_UNSUPPORTED : iRet;
	case XUI_CODE_COMMAND_FIND_PREVIOUS:
		iRet = xuiCodeEditFindPrevious(pWidget, NULL);
		if ( pHandled != NULL ) *pHandled = 1;
		return (iRet == XUI_ERROR_INVALID_ARGUMENT) ? XUI_ERROR_UNSUPPORTED : iRet;
	case XUI_CODE_COMMAND_REPLACE_NEXT:
		iRet = xuiCodeEditReplaceCurrent(pWidget, NULL);
		if ( pHandled != NULL ) *pHandled = 1;
		return (iRet == XUI_ERROR_INVALID_ARGUMENT) ? XUI_ERROR_UNSUPPORTED : iRet;
	default:
		break;
	}
	memset(&tProxy, 0, sizeof(tProxy));
	memset(sIndent, 0, sizeof(sIndent));
	__xuiCodeEditBuildIndent(pData, sIndent, (int)sizeof(sIndent));
	iRet = __xuiCodeEditCommandContext(pWidget, pData, &tProxy, &tContext);
	if ( iRet != XUI_OK ) return iRet;
	tContext.sIndent = sIndent;
	tContext.pCommandData = pCommandData;
	if ( iCommand == XUI_CODE_COMMAND_PASTE ) {
		sClipboard = NULL;
		iClipboardSize = 0;
		iRet = xuiInternalClipboardReadProxy(&tProxy, &sClipboard, &iClipboardSize);
		if ( iRet == XUI_OK && iClipboardSize > 0 ) {
			iRet = __xuiCodeEditProcessTextInput(pWidget, pData, sClipboard,
				iClipboardSize, XUI_CODE_INPUT_SOURCE_PASTE, NULL);
		}
		xrtFree(sClipboard);
		if ( pHandled != NULL && iRet == XUI_OK ) *pHandled = 1;
		return iRet;
	}
	iRet = xuiCodeCommandExecute(&tContext, iCommand, pHandled);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit command failed");
	if ( iRet == XUI_OK && (pHandled == NULL || *pHandled) ) {
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditUtf8Encode(uint32_t iCodepoint, char* sText)
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

static int __xuiCodeEditApplyTextEditsRaw(xui_code_edit_data_t* pData,
	const xui_code_text_edit_t* pEdits, int iEditCount,
	int bSetSelection, int iSelectionAnchor, int iSelectionCaret)
{
	int iLength;
	int i;
	int iRet;
	int iStartLine;
	int iEndLine;
	int iNewEndLine;
	int iNewEndOffset;

	if ( pData == NULL || iEditCount < 0 || (iEditCount > 0 && pEdits == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLength = xuiCodeDocumentGetLength(pData->pDocument);
	for ( i = 0; i < iEditCount; i++ ) {
		if ( pEdits[i].sText == NULL || pEdits[i].tRange.iStart < 0 ||
		     pEdits[i].tRange.iEnd < pEdits[i].tRange.iStart ||
		     pEdits[i].tRange.iEnd > iLength ||
		     (i > 0 && pEdits[i].tRange.iStart < pEdits[i - 1].tRange.iEnd) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	for ( i = iEditCount - 1; i >= 0; i-- ) {
		(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, pEdits[i].tRange.iStart, &iStartLine, NULL);
		(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, pEdits[i].tRange.iEnd, &iEndLine, NULL);
		iRet = xuiCodeDocumentReplace(pData->pDocument,
			pEdits[i].tRange.iStart, pEdits[i].tRange.iEnd, pEdits[i].sText);
		if ( iRet != XUI_OK ) return iRet;
		iNewEndOffset = pEdits[i].tRange.iStart + (int)strlen(pEdits[i].sText);
		(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, iNewEndOffset, &iNewEndLine, NULL);
		if ( pData->pAnnotations != NULL ) {
			(void)xuiCodeAnnotationTrackEdit(pData->pAnnotations,
				pEdits[i].tRange.iStart, pEdits[i].tRange.iEnd, iNewEndOffset,
				iStartLine, iEndLine, iNewEndLine);
		}
		if ( pData->pFoldState != NULL ) {
			(void)xuiCodeFoldStateTrackEdit(pData->pFoldState, iStartLine, iEndLine, iNewEndLine);
		}
		__xuiCodeEditAdjustPlaceholders(pData,
			pEdits[i].tRange.iStart, pEdits[i].tRange.iEnd,
			(int)strlen(pEdits[i].sText));
	}
	if ( bSetSelection ) {
		iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
			iSelectionAnchor, iSelectionCaret);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditApplyInputAction(xui_widget pWidget, xui_code_edit_data_t* pData,
	const xui_code_input_action_t* pAction)
{
	(void)pWidget;
	if ( pData == NULL || pAction == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditApplyTextEditsRaw(pData,
		((pAction->iFlags & XUI_CODE_INPUT_ACTION_APPLY_EDITS) != 0) ? pAction->pEdits : NULL,
		((pAction->iFlags & XUI_CODE_INPUT_ACTION_APPLY_EDITS) != 0) ? pAction->iEditCount : 0,
		(pAction->iFlags & XUI_CODE_INPUT_ACTION_SET_SELECTION) != 0,
		pAction->iSelectionAnchor, pAction->iSelectionCaret);
}

static int __xuiCodeEditDispatchInput(xui_widget pWidget, xui_code_edit_data_t* pData,
	xui_code_input_event_t* pEvent, xui_code_input_action_t* pAction)
{
	int iRet;

	if ( pData == NULL || pEvent == NULL || pAction == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pAction, 0, sizeof(*pAction));
	pAction->iSize = sizeof(*pAction);
	if ( pData->onInput == NULL ) return XUI_OK;
	if ( pData->bInputDispatch ) return XUI_ERROR_UNSUPPORTED;
	pData->bInputDispatch = 1;
	iRet = pData->onInput((xui_widget_t*)pWidget, pEvent, pAction, pData->pInputUser);
	pData->bInputDispatch = 0;
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit input handler failed");
		memset(pAction, 0, sizeof(*pAction));
		pAction->iSize = sizeof(*pAction);
		return XUI_OK;
	}
	return XUI_OK;
}

static int __xuiCodeEditApplyAssistAction(xui_widget pWidget, xui_code_edit_data_t* pData,
	uint32_t iFlags)
{
	int iRet;

	if ( (iFlags & XUI_CODE_INPUT_ACTION_CLOSE_ASSIST) != 0 ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		(void)__xuiCodeEditCloseAssistInternal(pData);
	}
	if ( (iFlags & XUI_CODE_INPUT_ACTION_SHOW_COMPLETION) != 0 ) {
		iRet = __xuiCodeEditShowCompletionInternal(pWidget, pData, 1);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
	}
	if ( (iFlags & XUI_CODE_INPUT_ACTION_SHOW_SIGNATURE) != 0 ) {
		iRet = __xuiCodeEditRequestSignatureInternal(pWidget, pData);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditProcessTextInput(xui_widget pWidget, xui_code_edit_data_t* pData,
	const char* sText, int iTextSize, int iSource, const xui_event_t* pSourceEvent)
{
	xui_code_input_event_t tInput;
	xui_code_input_action_t tPreviewAction;
	xui_code_input_action_t tCommittedAction;
	xui_code_selection_t tBefore;
	xui_code_selection_t tAfter;
	xui_code_selection_t tInsertSelection;
	xui_code_range_t tChanged;
	uint32_t iAssistFlags;
	int iStart;
	int iEnd;
	int iRet;
	int iEndRet;
	int bChanged;
	int bCompletionRetained;

	if ( pWidget == NULL || pData == NULL || sText == NULL || iTextSize < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	if ( iTextSize == 0 ) return XUI_OK;
	memset(&tBefore, 0, sizeof(tBefore));
	memset(&tAfter, 0, sizeof(tAfter));
	memset(&tChanged, 0, sizeof(tChanged));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tBefore) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	iStart = tBefore.iAnchorOffset;
	iEnd = tBefore.iCaretOffset;
	if ( iStart > iEnd ) { int iSwap = iStart; iStart = iEnd; iEnd = iSwap; }
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.iPhase = XUI_CODE_INPUT_PREVIEW;
	tInput.iSource = iSource;
	tInput.pDocument = pData->pDocument;
	tInput.iDocumentVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	tInput.sText = sText;
	tInput.iTextSize = iTextSize;
	tInput.iCodepoint = (pSourceEvent != NULL) ? pSourceEvent->iCodepoint : 0u;
	tInput.iKey = (pSourceEvent != NULL) ? pSourceEvent->iKey : 0;
	tInput.iModifiers = (pSourceEvent != NULL) ? pSourceEvent->iModifiers : 0u;
	tInput.tSelectionBefore = tBefore;
	tInput.tSelectionAfter = tBefore;
	tInput.tReplacedRange = (xui_code_range_t){iStart, iEnd};
	tInput.tInsertedRange = (xui_code_range_t){iStart, iStart};
	iRet = __xuiCodeEditDispatchInput(pWidget, pData, &tInput, &tPreviewAction);
	if ( iRet != XUI_OK ) return iRet;
	iAssistFlags = tPreviewAction.iFlags;
	bChanged = 0;
	iRet = xuiCodeDocumentBeginEdit(pData->pDocument);
	if ( iRet != XUI_OK ) return iRet;
	if ( (tPreviewAction.iFlags & (XUI_CODE_INPUT_ACTION_APPLY_EDITS | XUI_CODE_INPUT_ACTION_SET_SELECTION)) != 0 ) {
		iRet = __xuiCodeEditApplyInputAction(pWidget, pData, &tPreviewAction);
		if ( iRet == XUI_OK && (tPreviewAction.iFlags & XUI_CODE_INPUT_ACTION_APPLY_EDITS) != 0 &&
		     tPreviewAction.iEditCount > 0 ) bChanged = 1;
	}
	if ( iRet == XUI_OK && (tPreviewAction.iFlags & XUI_CODE_INPUT_ACTION_CONSUME) == 0 ) {
		memset(&tInsertSelection, 0, sizeof(tInsertSelection));
		(void)xuiCodeSelectionGetState(pData->pSelection, &tInsertSelection);
		iRet = xuiCodeEditingInsertText(pData->pDocument, pData->pSelection, sText, pData->bReadonly);
		if ( iRet == XUI_OK ) {
			int iReplaceStart = tInsertSelection.iAnchorOffset;
			int iReplaceEnd = tInsertSelection.iCaretOffset;
			if ( iReplaceStart > iReplaceEnd ) {
				int iSwap = iReplaceStart;
				iReplaceStart = iReplaceEnd;
				iReplaceEnd = iSwap;
			}
			(void)xuiCodeDocumentGetLastEditRange(pData->pDocument, &tChanged);
			__xuiCodeEditAdjustPlaceholders(pData, iReplaceStart, iReplaceEnd,
				tChanged.iEnd - tChanged.iStart);
			bChanged = 1;
		}
	}
	if ( iRet == XUI_OK && bChanged ) {
		(void)xuiCodeSelectionGetState(pData->pSelection, &tAfter);
		(void)xuiCodeDocumentGetLastEditRange(pData->pDocument, &tChanged);
		tInput.iPhase = XUI_CODE_INPUT_COMMITTED;
		tInput.iDocumentVersion = xuiCodeDocumentGetVersion(pData->pDocument);
		tInput.tSelectionAfter = tAfter;
		tInput.tInsertedRange = tChanged;
		iRet = __xuiCodeEditDispatchInput(pWidget, pData, &tInput, &tCommittedAction);
		if ( iRet == XUI_OK && (tCommittedAction.iFlags &
			(XUI_CODE_INPUT_ACTION_APPLY_EDITS | XUI_CODE_INPUT_ACTION_SET_SELECTION)) != 0 ) {
			iRet = __xuiCodeEditApplyInputAction(pWidget, pData, &tCommittedAction);
		}
		iAssistFlags |= tCommittedAction.iFlags;
	}
	iEndRet = xuiCodeDocumentEndEdit(pData->pDocument);
	if ( iRet == XUI_OK ) iRet = iEndRet;
	if ( iRet != XUI_OK ) return iRet;
	if ( !bChanged ) return __xuiCodeEditApplyAssistAction(pWidget, pData, iAssistFlags);
	bCompletionRetained = 0;
	if ( (iAssistFlags & XUI_CODE_INPUT_ACTION_SHOW_COMPLETION) == 0 &&
	     pData->iCompletionSourceCount > 0 && pData->pCompletionPopup != NULL &&
	     xuiPopupIsOpen(pData->pCompletionPopup) ) {
		if ( __xuiCodeEditRefreshCompletionSession(pWidget, pData) == XUI_OK ) {
			bCompletionRetained = 1;
		} else {
			(void)__xuiCodeEditCancelCompletionInternal(pData);
		}
	}
	if ( !bCompletionRetained &&
	     (iAssistFlags & XUI_CODE_INPUT_ACTION_SHOW_COMPLETION) == 0 &&
	     pData->bCompletionAutoShow ) {
		pData->bCompletionPending = 1;
		pData->fCompletionElapsed = 0.0f;
	}
	(void)xuiCodeEditEnsureCaretVisible(pWidget);
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return __xuiCodeEditApplyAssistAction(pWidget, pData, iAssistFlags);
}

static int __xuiCodeEditInsertEventText(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent)
{
	char sEncoded[8];
	const char* sText;
	int bInlineCompletion;
	int iTextSize;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly || ((pEvent->iModifiers & XUI_MOD_CTRL) != 0) ) return XUI_EVENT_DISPATCH_STOP;
	iTextSize = 0;
	sText = NULL;
	memset(sEncoded, 0, sizeof(sEncoded));
	if ( pEvent->iTextSize > 0 && pEvent->sText[0] != '\0' ) {
		iTextSize = pEvent->iTextSize;
		sText = pEvent->sText;
	} else if ( pEvent->iCodepoint >= 32u ) {
		iTextSize = __xuiCodeEditUtf8Encode(pEvent->iCodepoint, sEncoded);
		sEncoded[iTextSize] = '\0';
		sText = sEncoded;
	}
	if ( sText == NULL || iTextSize <= 0 ) return XUI_EVENT_DISPATCH_STOP;
	if ( pData->pCompletionPopup != NULL && xuiPopupIsOpen(pData->pCompletionPopup) &&
	     pData->iCompletionSelected >= 0 &&
	     pData->iCompletionSelected < pData->iCompletionCount ) {
		xui_code_completion_owned_item_t* pItem =
			__xuiCodeEditCompletionVisibleItem(pData, pData->iCompletionSelected);
		const char* sCommitCharacters =
			(pItem != NULL) ? pItem->sCommitCharacters : NULL;
		if ( iTextSize < 8 && sCommitCharacters != NULL && sCommitCharacters[0] != '\0' &&
		     strstr(sCommitCharacters, sText) != NULL ) {
			iRet = __xuiCodeEditCommitCompletionCharacter(pWidget, pData, sText);
			__xuiCodeEditSetError(pData,
				(iRet == XUI_OK) ? "" : "CodeEdit completion character commit failed");
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	bInlineCompletion = __xuiCodeEditInlineCompletionValid(pData);
	iRet = __xuiCodeEditProcessTextInput(pWidget, pData, sText, iTextSize,
		XUI_CODE_INPUT_SOURCE_KEYBOARD, pEvent);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit text insert failed");
	if ( iRet == XUI_OK && bInlineCompletion ) {
		(void)__xuiCodeEditInlineCompletionConsume(pData, sText, iTextSize);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiCodeEditCommitText(xui_widget pWidget, xui_code_edit_data_t* pData, const char* sText)
{
	int iRet;

	if ( pWidget == NULL || pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	if ( sText[0] == '\0' ) return XUI_OK;
	iRet = __xuiCodeEditProcessTextInput(pWidget, pData, sText, (int)strlen(sText),
		XUI_CODE_INPUT_SOURCE_IME, NULL);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit text insert failed");
	return iRet;
}

static void __xuiCodeEditImeSetReplacementRange(xui_code_edit_data_t* pData,
	const xui_event_t* pEvent)
{
	int iDocumentLength = xuiCodeDocumentGetLength(pData->pDocument);

	pData->iImeAnchorStart = pEvent->iCompositionReplacementStart;
	pData->iImeAnchorEnd = pEvent->iCompositionReplacementEnd;
	if ( pData->iImeAnchorStart < 0 ) pData->iImeAnchorStart = 0;
	if ( pData->iImeAnchorStart > iDocumentLength ) pData->iImeAnchorStart = iDocumentLength;
	if ( pData->iImeAnchorEnd < pData->iImeAnchorStart ) {
		pData->iImeAnchorEnd = pData->iImeAnchorStart;
	}
	if ( pData->iImeAnchorEnd > iDocumentLength ) pData->iImeAnchorEnd = iDocumentLength;
}

static int __xuiCodeEditImeComposition(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent)
{
	xui_code_selection_t tSelection;
	int iTextSize;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
	iTextSize = pEvent->iTextSize;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(pEvent->sText);
	if ( pEvent->bCompositionActive ) {
		if ( pEvent->bCompositionReplacementRange ) {
			__xuiCodeEditImeSetReplacementRange(pData, pEvent);
		} else if ( !pData->bImeComposing ) {
			memset(&tSelection, 0, sizeof(tSelection));
			if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK ) {
				pData->iImeAnchorStart = tSelection.iAnchorOffset;
				pData->iImeAnchorEnd = tSelection.iCaretOffset;
				if ( pData->iImeAnchorStart > pData->iImeAnchorEnd ) {
					int iSwap = pData->iImeAnchorStart;
					pData->iImeAnchorStart = pData->iImeAnchorEnd;
					pData->iImeAnchorEnd = iSwap;
				}
			}
		}
		if ( iTextSize < 0 ) iTextSize = 0;
		iRet = __xuiCodeEditTextReserve(&pData->sImeComposition,
			&pData->iImeCompositionCapacity, iTextSize + 1);
		if ( iRet != XUI_OK ) return iRet;
		if ( iTextSize > 0 ) memcpy(pData->sImeComposition, pEvent->sText, (size_t)iTextSize);
		pData->sImeComposition[iTextSize] = '\0';
		pData->iImeCursor = __xuiCodeEditUtf8ClampOffset(pData->sImeComposition,
			iTextSize, pEvent->iCompositionCursor);
		pData->iImeSelectionStart = __xuiCodeEditUtf8ClampOffset(pData->sImeComposition,
			iTextSize, pEvent->iCompositionSelectionStart);
		pData->iImeSelectionEnd = __xuiCodeEditUtf8ClampOffset(pData->sImeComposition,
			iTextSize, pEvent->iCompositionSelectionEnd);
		if ( pData->iImeSelectionEnd < pData->iImeSelectionStart ) {
			pData->iImeSelectionEnd = pData->iImeSelectionStart;
		}
		memset(&tSelection, 0, sizeof(tSelection));
		pData->bImeComposing = 1;
		__xuiCodeEditSetError(pData, "");
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iTextSize <= 0 || pEvent->sText[0] == '\0' ) {
		__xuiCodeEditImeReset(pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->bCompositionReplacementRange ) {
		__xuiCodeEditImeSetReplacementRange(pData, pEvent);
		iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
			pData->iImeAnchorStart, pData->iImeAnchorEnd);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( pData->bImeComposing ) {
		iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
			pData->iImeAnchorStart, pData->iImeAnchorEnd);
		if ( iRet != XUI_OK ) return iRet;
	}
	__xuiCodeEditImeReset(pData);
	iRet = __xuiCodeEditCommitText(pWidget, pData, pEvent->sText);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static xui_rect_t __xuiCodeEditImeCandidateRect(xui_widget pWidget, void* pUser)
{
	xui_code_edit_data_t* pData;
	xui_context pContext;
	xui_proxy_t* pProxy;
	xui_font pFont;
	xui_code_selection_t tSelection;
	xui_rect_t tWorld;
	float fColumnWidth;
	int iOffset;

	(void)pUser;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pData->pDocument == NULL || pData->pSelection == NULL ) {
		return tWorld;
	}
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ) {
		return tWorld;
	}
	iOffset = pData->bImeComposing ? pData->iImeAnchorStart :
		tSelection.iCaretOffset;
	if ( xuiCodeEditGetOffsetRect(pWidget, iOffset, &tWorld) != XUI_OK ) {
		return xuiWidgetGetWorldRect(pWidget);
	}
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	if ( pData->bImeComposing && pData->sImeComposition != NULL ) {
		tWorld.fX += __xuiCodeEditShapedTextOffsetX(pProxy, pFont, pData->sImeComposition,
			(int)strlen(pData->sImeComposition), pData->iImeCursor, (float)pData->iImeCursor * fColumnWidth);
	}
	return tWorld;
}

static int __xuiCodeEditHitOffsetEx(xui_widget pWidget, xui_code_edit_data_t* pData, float fX, float fY, int bClamp, int* pOffset)
{
	xui_rect_t tWorld;
	float fViewportX;
	float fViewportY;
	float fViewportW;
	float fViewportH;
	float fMarginWidth;
	float fColumnWidth;
	float fLineHeight;
	double fTextLocalX;
	double fSegmentStartX;
	const char* sText;
	xui_context pContext;
	xui_proxy_t* pProxy;
	xui_font pFont;
	int iStart;
	int iEnd;
	int iLine;
	int iColumn;
	int iLineCount;
	int iVisualRow;

	if ( pWidget == NULL || pData == NULL || pData->pDocument == NULL || pOffset == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	fViewportX = tWorld.fX + pData->tScrollViewportRect.fX;
	fViewportY = tWorld.fY + pData->tScrollViewportRect.fY;
	fViewportW = pData->tScrollViewportRect.fW;
	fViewportH = pData->tScrollViewportRect.fH;
	if ( fViewportW <= 0.0f || fViewportH <= 0.0f ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( (fX != fX) || (fY != fY) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( bClamp ) {
		if ( fX < fViewportX ) fX = fViewportX;
		if ( fY < fViewportY ) fY = fViewportY;
		if ( fX >= fViewportX + fViewportW ) fX = fViewportX + fViewportW - 0.001f;
		if ( fY >= fViewportY + fViewportH ) fY = fViewportY + fViewportH - 0.001f;
	} else if ( (fX < fViewportX) || (fY < fViewportY) ||
	            (fX >= fViewportX + fViewportW) ||
	            (fY >= fViewportY + fViewportH) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	iVisualRow = (int)((fY - fViewportY - 4.0f + pData->fScrollY) / fLineHeight);
	iLine = __xuiCodeEditVisibleRowToLine(pData, iVisualRow);
	fTextLocalX = (double)fX - fViewportX - fMarginWidth - 4.0 + pData->fScrollX;
	fSegmentStartX = 0.0f;
	if ( pData->bWordWrap && !__xuiCodeEditVisibleRowIsInlineCompletion(pData, iVisualRow) ) {
		if ( __xuiCodeEditWrapVisualSegment(pWidget, pData, iVisualRow,
			&iLine, NULL, NULL, NULL,
			&fSegmentStartX) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
		fTextLocalX += fSegmentStartX;
	}
	if ( iLine < 0 ) iLine = 0;
	if ( fTextLocalX < 0.0f ) fTextLocalX = 0.0f;
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( iLineCount <= 0 ) {
		*pOffset = 0;
		return XUI_OK;
	}
	if ( iLine >= iLineCount ) iLine = iLineCount - 1;
	iStart = 0;
	iEnd = 0;
	if ( xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd) != XUI_OK ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiCodeEditReadRange(pData, iStart, iEnd, &sText) != XUI_OK ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	iColumn = __xuiCodeEditLineColumnFromX(pProxy, pFont, pData, sText,
		0, iEnd - iStart, fTextLocalX, fColumnWidth);
	return xuiCodeDocumentLineColumnToOffset(pData->pDocument, iLine, iColumn, pOffset);
}

static int __xuiCodeEditHitOffset(xui_widget pWidget, xui_code_edit_data_t* pData, float fX, float fY, int* pOffset)
{
	return __xuiCodeEditHitOffsetEx(pWidget, pData, fX, fY, 0, pOffset);
}

static int __xuiCodeEditHitTestDiagnosticInternal(xui_widget pWidget, xui_code_edit_data_t* pData, float fX, float fY, xui_code_diagnostic_hit_t* pHit)
{
	xui_code_diagnostic_t arrLocal[32];
	xui_code_diagnostic_t* pDiagnostics;
	xui_code_diagnostic_t* pAllocated;
	xui_code_diagnostic_t* pDiagnostic;
	xui_code_diagnostic_t tBest;
	int arrLocalIndices[32];
	int* pIndices;
	int* pAllocatedIndices;
	xui_rect_t tWorld;
	xui_rect_t tRect;
	xui_rect_t tBestRect;
	const char* sText;
	xui_context pContext;
	xui_proxy_t* pProxy;
	xui_font pFont;
	float fViewportX;
	float fViewportY;
	float fViewportRight;
	float fViewportBottom;
	float fMarginWidth;
	float fColumnWidth;
	float fLineHeight;
	double fStartX;
	double fEndX;
	int iVisibleRow;
	int iLine;
	int iLineCount;
	int iLineStart;
	int iLineEnd;
	int iStart;
	int iEnd;
	int iCount;
	int i;
	int iBest;
	int iBestSeverity;
	int iBestLength;
	int iSeverity;
	int iLength;

	if ( pHit != NULL ) memset(pHit, 0, sizeof(*pHit));
	if ( pWidget == NULL || pData == NULL || pHit == NULL || pData->pDocument == NULL || pData->pAnnotations == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiCodeEditUpdateScrollModel(pWidget, pData) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fViewportX = tWorld.fX + pData->tScrollViewportRect.fX;
	fViewportY = tWorld.fY + pData->tScrollViewportRect.fY;
	fViewportRight = fViewportX + pData->tScrollViewportRect.fW;
	fViewportBottom = fViewportY + pData->tScrollViewportRect.fH;
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	if ( fX < fViewportX + fMarginWidth || fX >= fViewportRight || fY < fViewportY || fY >= fViewportBottom ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	if ( fLineHeight <= 0.0f || fY - fViewportY - 4.0f + pData->fScrollY < 0.0f ) return XUI_ERROR_UNSUPPORTED;
	iVisibleRow = (int)((fY - fViewportY - 4.0f + pData->fScrollY) / fLineHeight);
	iLine = __xuiCodeEditVisibleRowToLine(pData, iVisibleRow);
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( iLine < 0 || iLine >= iLineCount ) return XUI_ERROR_UNSUPPORTED;
	iLineStart = 0;
	iLineEnd = 0;
	if ( xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iLineStart, &iLineEnd) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	if ( __xuiCodeEditReadRange(pData, iLineStart, iLineEnd, &sText) != XUI_OK ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pAllocated = NULL;
	pAllocatedIndices = NULL;
	pDiagnostics = arrLocal;
	pIndices = arrLocalIndices;
	if ( xuiCodeAnnotationGetDiagnosticsInRange(pData->pAnnotations, iLineStart,
		(iLineEnd > iLineStart) ? iLineEnd : iLineStart + 1,
		arrLocal, arrLocalIndices, (int)(sizeof(arrLocal) / sizeof(arrLocal[0])), &iCount) != XUI_OK || iCount <= 0 ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( iCount > (int)(sizeof(arrLocal) / sizeof(arrLocal[0])) ) {
		pAllocated = (xui_code_diagnostic_t*)xrtMalloc(sizeof(*pAllocated) * (size_t)iCount);
		pAllocatedIndices = (int*)xrtMalloc(sizeof(*pAllocatedIndices) * (size_t)iCount);
		if ( pAllocated == NULL || pAllocatedIndices == NULL ) {
			xrtFree(pAllocated);
			xrtFree(pAllocatedIndices);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pDiagnostics = pAllocated;
		pIndices = pAllocatedIndices;
		if ( xuiCodeAnnotationGetDiagnosticsInRange(pData->pAnnotations, iLineStart,
			(iLineEnd > iLineStart) ? iLineEnd : iLineStart + 1,
			pDiagnostics, pIndices, iCount, &iCount) != XUI_OK ) {
			xrtFree(pAllocated);
			xrtFree(pAllocatedIndices);
			return XUI_ERROR_UNSUPPORTED;
		}
	}
	iBest = -1;
	iBestSeverity = 0x7fffffff;
	iBestLength = 0x7fffffff;
	memset(&tBest, 0, sizeof(tBest));
	memset(&tBestRect, 0, sizeof(tBestRect));
	for ( i = 0; i < iCount; i++ ) {
		pDiagnostic = &pDiagnostics[i];
		iStart = (pDiagnostic->tRange.iStart > iLineStart) ? pDiagnostic->tRange.iStart : iLineStart;
		iEnd = (pDiagnostic->tRange.iEnd < iLineEnd) ? pDiagnostic->tRange.iEnd : iLineEnd;
		if ( iStart > iLineEnd ) continue;
		if ( iEnd < iStart ) iEnd = iStart;
		fStartX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
			iStart - iLineStart, fColumnWidth);
		fEndX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
			iEnd - iLineStart, fColumnWidth);
		if ( fEndX <= fStartX ) fEndX = fStartX + fColumnWidth;
		tRect = (xui_rect_t){
			fViewportX + fMarginWidth + 4.0f + fStartX - pData->fScrollX,
			fViewportY + 4.0f + (float)iVisibleRow * fLineHeight - pData->fScrollY,
			fEndX - fStartX,
			fLineHeight
		};
		if ( tRect.fX < fViewportX + fMarginWidth ) {
			tRect.fW -= (fViewportX + fMarginWidth - tRect.fX);
			tRect.fX = fViewportX + fMarginWidth;
		}
		if ( tRect.fX + tRect.fW > fViewportRight ) tRect.fW = fViewportRight - tRect.fX;
		if ( tRect.fY < fViewportY ) {
			tRect.fH -= (fViewportY - tRect.fY);
			tRect.fY = fViewportY;
		}
		if ( tRect.fY + tRect.fH > fViewportBottom ) tRect.fH = fViewportBottom - tRect.fY;
		if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f || fX < tRect.fX || fX >= tRect.fX + tRect.fW || fY < tRect.fY || fY >= tRect.fY + tRect.fH ) continue;
		iSeverity = (pDiagnostic->iSeverity >= XUI_CODE_DIAGNOSTIC_ERROR && pDiagnostic->iSeverity <= XUI_CODE_DIAGNOSTIC_HINT) ? pDiagnostic->iSeverity : 0x7ffffffe;
		iLength = pDiagnostic->tRange.iEnd - pDiagnostic->tRange.iStart;
		if ( iLength < 0 ) iLength = 0x7ffffffe;
		if ( iBest < 0 || iSeverity < iBestSeverity || (iSeverity == iBestSeverity && iLength < iBestLength) ) {
			iBest = pIndices[i];
			iBestSeverity = iSeverity;
			iBestLength = iLength;
			tBest = *pDiagnostic;
			tBestRect = tRect;
		}
	}
	xrtFree(pAllocated);
	xrtFree(pAllocatedIndices);
	if ( iBest < 0 ) return XUI_ERROR_UNSUPPORTED;
	pHit->iSize = sizeof(*pHit);
	pHit->iIndex = iBest;
	pHit->tRect = tBestRect;
	pHit->tDiagnostic = tBest;
	pHit->tDiagnostic.iSize = sizeof(pHit->tDiagnostic);
	return XUI_OK;
}

static int __xuiCodeEditInlineCompletionValid(xui_code_edit_data_t* pData)
{
	xui_code_selection_t tSelection;

	if ( pData == NULL || pData->pDocument == NULL || pData->pSelection == NULL ||
	     pData->sInlineCompletion == NULL || pData->sInlineCompletion[0] == '\0' ) return 0;
	if ( xuiCodeSelectionGetCount(pData->pSelection) != 1 ) return 0;
	if ( xuiCodeDocumentGetVersion(pData->pDocument) != pData->iInlineCompletionVersion ) return 0;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ) return 0;
	return tSelection.iAnchorOffset == pData->iInlineCompletionOffset &&
	       tSelection.iCaretOffset == pData->iInlineCompletionOffset;
}

static int __xuiCodeEditInlineCompletionClearData(xui_code_edit_data_t* pData)
{
	int bChanged;

	if ( pData == NULL ) return 0;
	bChanged = pData->sInlineCompletion != NULL && pData->sInlineCompletion[0] != '\0';
	if ( pData->sInlineCompletion != NULL ) pData->sInlineCompletion[0] = '\0';
	pData->iInlineCompletionOffset = 0;
	pData->iInlineCompletionLine = 0;
	pData->iInlineCompletionExtraRows = 0;
	pData->iInlineCompletionVersion = 0u;
	return bChanged;
}

static void __xuiCodeEditInlineCompletionRefreshLayout(xui_code_edit_data_t* pData)
{
	int iExtraRows;
	int iLine;
	int i;

	if ( pData == NULL || pData->sInlineCompletion == NULL || pData->sInlineCompletion[0] == '\0' ) {
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		return;
	}
	iExtraRows = 0;
	for ( i = 0; pData->sInlineCompletion[i] != '\0'; i++ ) {
		if ( pData->sInlineCompletion[i] == '\r' ) {
			iExtraRows++;
			if ( pData->sInlineCompletion[i + 1] == '\n' ) i++;
		} else if ( pData->sInlineCompletion[i] == '\n' ) {
			iExtraRows++;
		}
	}
	iLine = 0;
	(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, pData->iInlineCompletionOffset, &iLine, NULL);
	pData->iInlineCompletionLine = iLine;
	pData->iInlineCompletionExtraRows = iExtraRows;
	pData->iInlineCompletionVersion = xuiCodeDocumentGetVersion(pData->pDocument);
}

static int __xuiCodeEditInlineCompletionConsume(xui_code_edit_data_t* pData, const char* sText, int iTextLength)
{
	int iCompletionLength;

	if ( pData == NULL || sText == NULL || iTextLength <= 0 ||
	     pData->sInlineCompletion == NULL || pData->sInlineCompletion[0] == '\0' ) return 0;
	iCompletionLength = (int)strlen(pData->sInlineCompletion);
	if ( iTextLength > iCompletionLength ||
	     memcmp(pData->sInlineCompletion, sText, (size_t)iTextLength) != 0 ) {
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		return 0;
	}
	memmove(pData->sInlineCompletion,
		pData->sInlineCompletion + iTextLength,
		(size_t)(iCompletionLength - iTextLength) + 1u);
	pData->iInlineCompletionOffset += iTextLength;
	__xuiCodeEditInlineCompletionRefreshLayout(pData);
	return 1;
}

static void __xuiCodeEditCompletionItemClear(xui_code_completion_owned_item_t* pItem)
{
	if ( pItem == NULL ) return;
	xrtFree(pItem->sLabel);
	xrtFree(pItem->sInsertText);
	xrtFree(pItem->sDetail);
	xrtFree(pItem->sDocumentation);
	xrtFree(pItem->sFilterText);
	xrtFree(pItem->sSortText);
	xrtFree(pItem->sCommitCharacters);
	memset(pItem, 0, sizeof(*pItem));
}

static void __xuiCodeEditCompletionItemsClear(xui_code_edit_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	for ( i = 0; i < pData->iCompletionSourceCount; i++ ) {
		__xuiCodeEditCompletionItemClear(&pData->pCompletionItems[i]);
	}
	pData->iCompletionSourceCount = 0;
	pData->iCompletionCount = 0;
	pData->iCompletionSelected = -1;
	pData->iCompletionStartOffset = 0;
	pData->iCompletionDocumentVersion = 0u;
}

static int __xuiCodeEditCompletionReserve(xui_code_edit_data_t* pData, int iCapacity)
{
	xui_code_completion_owned_item_t* pNewItems;
	int* pNewVisible;
	const char** pNewLabels;
	int iNewCapacity;

	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pData->iCompletionCapacity ) return XUI_OK;
	iNewCapacity = (pData->iCompletionCapacity > 0) ? pData->iCompletionCapacity : 16;
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > 16384 ) return XUI_ERROR_OUT_OF_MEMORY;
		iNewCapacity *= 2;
	}
	pNewItems = (xui_code_completion_owned_item_t*)xrtMalloc(
		sizeof(*pNewItems) * (size_t)iNewCapacity);
	pNewVisible = (int*)xrtMalloc(sizeof(*pNewVisible) * (size_t)iNewCapacity);
	pNewLabels = (const char**)xrtMalloc(sizeof(*pNewLabels) * (size_t)iNewCapacity);
	if ( pNewItems == NULL || pNewVisible == NULL || pNewLabels == NULL ) {
		xrtFree(pNewItems);
		xrtFree(pNewVisible);
		xrtFree(pNewLabels);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pNewItems, 0, sizeof(*pNewItems) * (size_t)iNewCapacity);
	memset(pNewVisible, 0, sizeof(*pNewVisible) * (size_t)iNewCapacity);
	memset(pNewLabels, 0, sizeof(*pNewLabels) * (size_t)iNewCapacity);
	if ( pData->iCompletionSourceCount > 0 ) {
		memcpy(pNewItems, pData->pCompletionItems,
			sizeof(*pNewItems) * (size_t)pData->iCompletionSourceCount);
	}
	if ( pData->iCompletionCount > 0 ) {
		memcpy(pNewVisible, pData->pCompletionVisible,
			sizeof(*pNewVisible) * (size_t)pData->iCompletionCount);
		memcpy(pNewLabels, pData->arrCompletionLabels,
			sizeof(*pNewLabels) * (size_t)pData->iCompletionCount);
	}
	xrtFree(pData->pCompletionItems);
	xrtFree(pData->pCompletionVisible);
	xrtFree(pData->arrCompletionLabels);
	pData->pCompletionItems = pNewItems;
	pData->pCompletionVisible = pNewVisible;
	pData->arrCompletionLabels = pNewLabels;
	pData->iCompletionCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeEditCompletionCopyItem(xui_code_completion_owned_item_t* pDst, const xui_code_completion_item_t* pSrc)
{
	const char* sFilterText;
	const char* sSortText;
	const char* sCommitCharacters;

	if ( pDst == NULL || pSrc == NULL || pSrc->sLabel == NULL || pSrc->sLabel[0] == '\0' ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sFilterText = (pSrc->iSize >= offsetof(xui_code_completion_item_t, sFilterText) +
		sizeof(pSrc->sFilterText) && pSrc->sFilterText != NULL && pSrc->sFilterText[0] != '\0') ?
		pSrc->sFilterText : pSrc->sLabel;
	sSortText = (pSrc->iSize >= offsetof(xui_code_completion_item_t, sSortText) +
		sizeof(pSrc->sSortText) && pSrc->sSortText != NULL && pSrc->sSortText[0] != '\0') ?
		pSrc->sSortText : pSrc->sLabel;
	sCommitCharacters = (pSrc->iSize >= offsetof(xui_code_completion_item_t, sCommitCharacters) +
		sizeof(pSrc->sCommitCharacters) && pSrc->sCommitCharacters != NULL) ?
		pSrc->sCommitCharacters : "";
	memset(pDst, 0, sizeof(*pDst));
	pDst->sLabel = __xuiCodeEditStringDup(pSrc->sLabel);
	pDst->sInsertText = __xuiCodeEditStringDup(
		(pSrc->sInsertText != NULL && pSrc->sInsertText[0] != '\0') ? pSrc->sInsertText : pSrc->sLabel);
	pDst->sDetail = __xuiCodeEditStringDup((pSrc->sDetail != NULL) ? pSrc->sDetail : "");
	pDst->sDocumentation = __xuiCodeEditStringDup((pSrc->sDocumentation != NULL) ? pSrc->sDocumentation : "");
	pDst->sFilterText = __xuiCodeEditStringDup(sFilterText);
	pDst->sSortText = __xuiCodeEditStringDup(sSortText);
	pDst->sCommitCharacters = __xuiCodeEditStringDup(sCommitCharacters);
	if ( pDst->sLabel == NULL || pDst->sInsertText == NULL ||
	     pDst->sDetail == NULL || pDst->sDocumentation == NULL ||
	     pDst->sFilterText == NULL || pDst->sSortText == NULL ||
	     pDst->sCommitCharacters == NULL ) {
		__xuiCodeEditCompletionItemClear(pDst);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst->tItem = *pSrc;
	pDst->tItem.iSize = sizeof(pDst->tItem);
	pDst->tItem.sLabel = pDst->sLabel;
	pDst->tItem.sInsertText = pDst->sInsertText;
	pDst->tItem.sDetail = pDst->sDetail;
	pDst->tItem.sDocumentation = pDst->sDocumentation;
	pDst->tItem.sFilterText = pDst->sFilterText;
	pDst->tItem.sSortText = pDst->sSortText;
	pDst->tItem.sCommitCharacters = pDst->sCommitCharacters;
	return XUI_OK;
}

static int __xuiCodeEditCompletionAsciiLower(int ch)
{
	return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}

static int __xuiCodeEditCompletionMatchScore(const char* sCandidate, const char* sPrefix)
{
	const unsigned char* pCandidate;
	const unsigned char* pPrefix;
	int bExactCase;
	int iSkipped;

	if ( sCandidate == NULL || sPrefix == NULL ) return -1;
	if ( sPrefix[0] == '\0' ) return 0;
	pCandidate = (const unsigned char*)sCandidate;
	pPrefix = (const unsigned char*)sPrefix;
	bExactCase = 1;
	while ( *pPrefix != '\0' && *pCandidate != '\0' &&
	        __xuiCodeEditCompletionAsciiLower(*pCandidate) ==
	        __xuiCodeEditCompletionAsciiLower(*pPrefix) ) {
		if ( *pCandidate != *pPrefix ) bExactCase = 0;
		pCandidate++;
		pPrefix++;
	}
	if ( *pPrefix == '\0' ) return bExactCase ? 0 : 1;
	pCandidate = (const unsigned char*)sCandidate;
	pPrefix = (const unsigned char*)sPrefix;
	iSkipped = 0;
	while ( *pCandidate != '\0' && *pPrefix != '\0' ) {
		if ( __xuiCodeEditCompletionAsciiLower(*pCandidate) ==
		     __xuiCodeEditCompletionAsciiLower(*pPrefix) ) {
			pPrefix++;
		} else {
			iSkipped++;
		}
		pCandidate++;
	}
	return (*pPrefix == '\0') ? 2 + iSkipped : -1;
}

static int __xuiCodeEditCompletionCompare(const void* pA, const void* pB)
{
	const xui_code_completion_owned_item_t* pItemA =
		(const xui_code_completion_owned_item_t*)pA;
	const xui_code_completion_owned_item_t* pItemB =
		(const xui_code_completion_owned_item_t*)pB;
	int iCompare;

	if ( pItemA->iMatchScore != pItemB->iMatchScore ) {
		return pItemA->iMatchScore - pItemB->iMatchScore;
	}
	if ( pItemA->tItem.iSortOrder != pItemB->tItem.iSortOrder ) {
		return pItemA->tItem.iSortOrder - pItemB->tItem.iSortOrder;
	}
	iCompare = strcmp(pItemA->sSortText, pItemB->sSortText);
	if ( iCompare != 0 ) return iCompare;
	return strcmp(pItemA->sLabel, pItemB->sLabel);
}

static xui_code_completion_owned_item_t* __xuiCodeEditCompletionVisibleItem(
	xui_code_edit_data_t* pData, int iVisibleIndex)
{
	int iSourceIndex;

	if ( pData == NULL || iVisibleIndex < 0 || iVisibleIndex >= pData->iCompletionCount ||
	     pData->pCompletionVisible == NULL ) return NULL;
	iSourceIndex = pData->pCompletionVisible[iVisibleIndex];
	if ( iSourceIndex < 0 || iSourceIndex >= pData->iCompletionSourceCount ) return NULL;
	return &pData->pCompletionItems[iSourceIndex];
}

static int __xuiCodeEditCompletionPrefix(xui_code_edit_data_t* pData, int* pStart, int* pCaret,
	char* sPrefix, int iPrefixCapacity)
{
	xui_code_selection_t tSelection;
	char ch;
	int iStart;
	int iCaret;

	if ( pData == NULL || pStart == NULL || pCaret == NULL || sPrefix == NULL || iPrefixCapacity <= 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetCount(pData->pSelection) != 1 ||
	     xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ||
	     tSelection.iAnchorOffset != tSelection.iCaretOffset ) return XUI_ERROR_UNSUPPORTED;
	iCaret = tSelection.iCaretOffset;
	iStart = iCaret;
	while ( iStart > 0 ) {
		if ( xuiCodeDocumentGetByte(pData->pDocument, iStart - 1, &ch) != XUI_OK ) break;
		if ( !((unsigned char)ch == '_' || ((unsigned char)ch >= '0' && (unsigned char)ch <= '9') ||
		       ((unsigned char)ch >= 'A' && (unsigned char)ch <= 'Z') ||
		       ((unsigned char)ch >= 'a' && (unsigned char)ch <= 'z') ||
		       (unsigned char)ch >= 0x80u) ) break;
		iStart--;
	}
	if ( iCaret - iStart >= iPrefixCapacity ) iStart = iCaret - iPrefixCapacity + 1;
	if ( xuiCodeDocumentCopyRange(pData->pDocument, iStart, iCaret,
		sPrefix, iPrefixCapacity, NULL) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	*pStart = iStart;
	*pCaret = iCaret;
	return XUI_OK;
}

static int __xuiCodeEditCancelCompletionInternal(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bCompletionPending = 0;
	pData->fCompletionElapsed = 0.0f;
	if ( pData->pCompletionPopup != NULL && xuiPopupIsOpen(pData->pCompletionPopup) ) {
		(void)xuiPopupSetOpen(pData->pCompletionPopup, 0);
	}
	if ( pData->iAssistKind == XUI_CODE_EDIT_ASSIST_COMPLETION_DOCUMENTATION ) {
		(void)__xuiCodeEditCloseAssistInternal(pData);
	}
	__xuiCodeEditCompletionItemsClear(pData);
	return XUI_OK;
}

static void __xuiCodeEditCompletionListSelected(xui_widget pList, int iIndex, void* pUser)
{
	xui_widget pWidget;
	xui_code_edit_data_t* pData;

	(void)pList;
	pWidget = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return;
	(void)__xuiCodeEditCommitCompletionInternal(pWidget, pData, iIndex);
}

static void __xuiCodeEditCompletionSyncInline(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_code_completion_owned_item_t* pItem;
	xui_code_selection_t tSelection;
	const char* sDocument;
	const char* sInsert;
	int iPrefixLength;

	if ( pWidget == NULL || pData == NULL ||
	     pData->iCompletionSelected < 0 ||
	     pData->iCompletionSelected >= pData->iCompletionCount ) return;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ||
	     tSelection.iAnchorOffset != tSelection.iCaretOffset ||
	     tSelection.iCaretOffset < pData->iCompletionStartOffset ) return;
	pItem = __xuiCodeEditCompletionVisibleItem(pData, pData->iCompletionSelected);
	if ( pItem == NULL ) return;
	iPrefixLength = tSelection.iCaretOffset - pData->iCompletionStartOffset;
	sInsert = pItem->sInsertText;
	if ( __xuiCodeEditReadRange(pData, pData->iCompletionStartOffset,
		tSelection.iCaretOffset, &sDocument) != XUI_OK ) return;
	(void)__xuiCodeEditInlineCompletionClearData(pData);
	__xuiCodeEditShowCompletionDocumentation(pData, pItem);
	if ( sInsert == NULL || (int)strlen(sInsert) <= iPrefixLength ||
	     strncmp(sInsert, sDocument, (size_t)iPrefixLength) != 0 ) return;
	(void)xuiCodeEditSetInlineCompletion(pWidget, tSelection.iCaretOffset, sInsert + iPrefixLength);
}

static int __xuiCodeEditCompletionRefreshVisible(xui_widget pWidget,
	xui_code_edit_data_t* pData, const char* sPrefix)
{
	int iRows;
	int i;
	int iScore;
	int iRet;

	if ( pWidget == NULL || pData == NULL || sPrefix == NULL ||
	     pData->pCompletionList == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iCompletionSourceCount; i++ ) {
		iScore = __xuiCodeEditCompletionMatchScore(
			pData->pCompletionItems[i].sFilterText, sPrefix);
		pData->pCompletionItems[i].iMatchScore =
			(iScore < 0) ? 0x3fffffff : iScore;
	}
	qsort(pData->pCompletionItems, (size_t)pData->iCompletionSourceCount,
		sizeof(*pData->pCompletionItems), __xuiCodeEditCompletionCompare);
	pData->iCompletionCount = 0;
	for ( i = 0; i < pData->iCompletionSourceCount; i++ ) {
		if ( pData->pCompletionItems[i].iMatchScore == 0x3fffffff ) break;
		pData->pCompletionVisible[pData->iCompletionCount] = i;
		pData->arrCompletionLabels[pData->iCompletionCount] =
			pData->pCompletionItems[i].sLabel;
		pData->iCompletionCount++;
	}
	if ( pData->iCompletionCount <= 0 ) {
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		return XUI_ERROR_UNSUPPORTED;
	}
	pData->iCompletionSelected = 0;
	pData->iCompletionDocumentVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	iRet = xuiListViewSetItems(pData->pCompletionList,
		pData->arrCompletionLabels, pData->iCompletionCount);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiListViewSetSelected(pData->pCompletionList, 0);
	iRows = (pData->iCompletionCount < 8) ? pData->iCompletionCount : 8;
	(void)xuiWidgetSetRect(pData->pCompletionList,
		(xui_rect_t){0.0f, 0.0f, 360.0f, (float)iRows * 24.0f});
	(void)xuiPopupSetContentSize(pData->pCompletionPopup, 360.0f, (float)iRows * 24.0f);
	__xuiCodeEditCompletionSyncInline(pWidget, pData);
	return XUI_OK;
}

static int __xuiCodeEditRefreshCompletionSession(xui_widget pWidget,
	xui_code_edit_data_t* pData)
{
	char sPrefix[256];
	int iStart;
	int iCaret;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pData->iCompletionSourceCount <= 0 ||
	     pData->pCompletionPopup == NULL || !xuiPopupIsOpen(pData->pCompletionPopup) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	iRet = __xuiCodeEditCompletionPrefix(pData, &iStart, &iCaret,
		sPrefix, (int)sizeof(sPrefix));
	if ( iRet != XUI_OK || iStart != pData->iCompletionStartOffset ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	return __xuiCodeEditCompletionRefreshVisible(pWidget, pData, sPrefix);
}

static int __xuiCodeEditOpenCompletionItems(xui_widget pWidget, xui_code_edit_data_t* pData,
	int iStart, int iCaret, const char* sPrefix,
	const xui_code_completion_item_t* pItems, int iCount)
{
	xui_rect_t tAnchor;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL || sPrefix == NULL ||
	     pData->pCompletionPopup == NULL || pData->pCompletionList == NULL ||
	     iCount < 0 || (iCount > 0 && pItems == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)iCaret;
	if ( iCount > pData->iCompletionMaxItems ) iCount = pData->iCompletionMaxItems;
	__xuiCodeEditCompletionItemsClear(pData);
	iRet = __xuiCodeEditCompletionReserve(pData, iCount);
	if ( iRet != XUI_OK ) goto cleanup;
	for ( i = 0; i < iCount; i++ ) {
		if ( pItems[i].sLabel == NULL || pItems[i].sLabel[0] == '\0' ) continue;
		iRet = __xuiCodeEditCompletionCopyItem(
			&pData->pCompletionItems[pData->iCompletionSourceCount], &pItems[i]);
		if ( iRet != XUI_OK ) goto cleanup;
		pData->iCompletionSourceCount++;
	}
	if ( pData->iCompletionSourceCount <= 0 ) {
		iRet = XUI_ERROR_UNSUPPORTED;
		goto cleanup;
	}
	pData->iCompletionStartOffset = iStart;
	iRet = __xuiCodeEditCompletionRefreshVisible(pWidget, pData, sPrefix);
	if ( iRet != XUI_OK ) goto cleanup;
	tAnchor = __xuiCodeEditImeCandidateRect(pWidget, NULL);
	(void)xuiPopupSetAnchorRect(pData->pCompletionPopup, tAnchor);
	(void)xuiPopupSetOpen(pData->pCompletionPopup, 1);
	pData->bCompletionPending = 0;
	pData->fCompletionElapsed = 0.0f;
	__xuiCodeEditCompletionSyncInline(pWidget, pData);
	iRet = XUI_OK;

cleanup:
	if ( iRet != XUI_OK ) (void)__xuiCodeEditCancelCompletionInternal(pData);
	return iRet;
}

static int __xuiCodeEditShowCompletionInternal(xui_widget pWidget, xui_code_edit_data_t* pData, int bManual)
{
	xui_code_completion_item_t* pItems;
	char sPrefix[256];
	int iStart;
	int iCaret;
	int iCount;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditCompletionPrefix(pData, &iStart, &iCaret, sPrefix, (int)sizeof(sPrefix));
	if ( iRet != XUI_OK ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		return iRet;
	}
	if ( !bManual && (int)strlen(sPrefix) < pData->iCompletionMinPrefix ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		return XUI_ERROR_UNSUPPORTED;
	}
	pItems = (xui_code_completion_item_t*)xrtMalloc(
		sizeof(*pItems) * (size_t)pData->iCompletionMaxItems);
	if ( pItems == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pItems, 0, sizeof(*pItems) * (size_t)pData->iCompletionMaxItems);
	iCount = 0;
	iRet = xuiCodeProviderRequestCompletion(pData->pProviders, pWidget, iCaret, sPrefix,
		pItems, pData->iCompletionMaxItems, &iCount);
	if ( iRet == XUI_OK && iCount > 0 ) {
		iRet = __xuiCodeEditOpenCompletionItems(
			pWidget, pData, iStart, iCaret, sPrefix, pItems, iCount);
	} else {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		if ( iRet == XUI_OK ) iRet = XUI_ERROR_UNSUPPORTED;
	}
	xrtFree(pItems);
	return iRet;
}

static int __xuiCodeEditCommitCompletionCharacter(xui_widget pWidget,
	xui_code_edit_data_t* pData, const char* sText)
{
	int iSelected;
	int iRet;
	int iEndRet;

	if ( pWidget == NULL || pData == NULL || sText == NULL || sText[0] == '\0' ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iSelected = pData->iCompletionSelected;
	iRet = xuiCodeDocumentBeginEdit(pData->pDocument);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditCommitCompletionInternal(pWidget, pData, iSelected);
	if ( iRet == XUI_OK ) {
		iRet = __xuiCodeEditProcessTextInput(pWidget, pData, sText,
			(int)strlen(sText), XUI_CODE_INPUT_SOURCE_KEYBOARD, NULL);
	}
	iEndRet = xuiCodeDocumentEndEdit(pData->pDocument);
	if ( iRet == XUI_OK ) iRet = iEndRet;
	if ( iRet == XUI_OK ) {
		pData->bCompletionPending = 0;
		pData->fCompletionElapsed = 0.0f;
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget,
			XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditCommitCompletionInternal(xui_widget pWidget, xui_code_edit_data_t* pData, int iIndex)
{
	xui_code_completion_owned_item_t* pItem;
	xui_code_selection_t tSelection;
	char* sInsertText;
	int iInsertCapacity;
	int iRet;

	if ( pWidget == NULL || pData == NULL || iIndex < 0 || iIndex >= pData->iCompletionCount ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pItem = __xuiCodeEditCompletionVisibleItem(pData, iIndex);
	if ( pItem == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iCompletionDocumentVersion != xuiCodeDocumentGetVersion(pData->pDocument) ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		return XUI_ERROR_UNSUPPORTED;
	}
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ||
	     tSelection.iAnchorOffset != tSelection.iCaretOffset ||
	     tSelection.iCaretOffset < pData->iCompletionStartOffset ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		return XUI_ERROR_UNSUPPORTED;
	}
	sInsertText = NULL;
	iInsertCapacity = 0;
	iRet = __xuiCodeEditStringSet(&sInsertText, &iInsertCapacity,
		pItem->sInsertText);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
		pData->iCompletionStartOffset, tSelection.iCaretOffset);
	if ( iRet == XUI_OK ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		iRet = __xuiCodeEditProcessTextInput(pWidget, pData, sInsertText,
			(int)strlen(sInsertText), XUI_CODE_INPUT_SOURCE_COMPLETION, NULL);
	}
	xrtFree(sInsertText);
	if ( iRet == XUI_OK ) {
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget,
			XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditDiagnosticRectSame(xui_rect_t tA, xui_rect_t tB)
{
	return tA.fX == tB.fX && tA.fY == tB.fY && tA.fW == tB.fW && tA.fH == tB.fH;
}

static void __xuiCodeEditUpdateDiagnosticHover(xui_widget pWidget, xui_code_edit_data_t* pData, float fX, float fY, int bLeave)
{
	xui_code_diagnostic_hit_t tHit;
	int iRet;

	if ( pData == NULL ) return;
	memset(&tHit, 0, sizeof(tHit));
	iRet = bLeave ? XUI_ERROR_UNSUPPORTED : __xuiCodeEditHitTestDiagnosticInternal(pWidget, pData, fX, fY, &tHit);
	if ( iRet == XUI_OK ) {
		if ( pData->iHoveredDiagnostic == tHit.iIndex && __xuiCodeEditDiagnosticRectSame(pData->tHoveredDiagnosticRect, tHit.tRect) ) return;
		pData->iHoveredDiagnostic = tHit.iIndex;
		pData->tHoveredDiagnosticRect = tHit.tRect;
		if ( pData->onDiagnosticHover != NULL ) pData->onDiagnosticHover(pWidget, &tHit, pData->pDiagnosticHoverUser);
		return;
	}
	if ( pData->iHoveredDiagnostic < 0 ) return;
	pData->iHoveredDiagnostic = -1;
	memset(&pData->tHoveredDiagnosticRect, 0, sizeof(pData->tHoveredDiagnosticRect));
	if ( pData->onDiagnosticHover != NULL ) pData->onDiagnosticHover(pWidget, NULL, pData->pDiagnosticHoverUser);
}

static int __xuiCodeEditAutoScrollPointer(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fViewportX;
	float fViewportY;
	float fViewportRight;
	float fViewportBottom;
	float fMarginWidth;
	float fDeltaX;
	float fDeltaY;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	fViewportX = tWorld.fX + pData->tScrollViewportRect.fX + fMarginWidth;
	fViewportY = tWorld.fY + pData->tScrollViewportRect.fY;
	fViewportRight = tWorld.fX + pData->tScrollViewportRect.fX + pData->tScrollViewportRect.fW;
	fViewportBottom = tWorld.fY + pData->tScrollViewportRect.fY + pData->tScrollViewportRect.fH;
	fDeltaX = 0.0f;
	fDeltaY = 0.0f;
	if ( pEvent->fX < fViewportX ) {
		fDeltaX = pEvent->fX - fViewportX;
	} else if ( pEvent->fX > fViewportRight ) {
		fDeltaX = pEvent->fX - fViewportRight;
	}
	if ( pEvent->fY < fViewportY ) {
		fDeltaY = pEvent->fY - fViewportY;
	} else if ( pEvent->fY > fViewportBottom ) {
		fDeltaY = pEvent->fY - fViewportBottom;
	}
	if ( fDeltaX == 0.0f && fDeltaY == 0.0f ) return XUI_OK;
	return xuiCodeEditSetScroll(pWidget, pData->fScrollX + fDeltaX, pData->fScrollY + fDeltaY);
}

static int __xuiCodeEditPointerSelect(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent, int bExtend)
{
	int iOffset;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bExtend ) {
		iRet = __xuiCodeEditAutoScrollPointer(pWidget, pData, pEvent);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiCodeEditHitOffsetEx(pWidget, pData, pEvent->fX, pEvent->fY, bExtend, &iOffset);
	if ( iRet != XUI_OK ) return iRet;
	if ( !bExtend ) pData->iDragAnchor = iOffset;
	iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, pData->iDragAnchor, iOffset);
	if ( iRet == XUI_OK ) {
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditPointerSelectWordOrLine(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent)
{
	int iOffset;
	int iLine;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditHitOffset(pWidget, pData, pEvent->fX, pEvent->fY, &iOffset);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0u ) {
		iRet = xuiCodeDocumentOffsetToLineColumn(pData->pDocument, iOffset, &iLine, NULL);
		if ( iRet == XUI_OK ) iRet = xuiCodeSelectionSelectLine(pData->pSelection, pData->pDocument, iLine, 1);
	} else {
		iRet = xuiCodeSelectionSelectWord(pData->pSelection, pData->pDocument, iOffset);
	}
	if ( iRet == XUI_OK ) {
		pData->bDragging = 0;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiCodeEditDispatchMarginEvent(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_event_t* pEvent, int* pDispatched)
{
	xui_code_margin_hit_t tHit;
	xui_code_margin_info_t tInfo;
	xui_rect_t tViewport;
	xui_rect_t tLineRect;
	float fLineHeight;
	int iLineCount;
	int iRet;

	if ( pDispatched != NULL ) *pDispatched = 0;
	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	tViewport = xuiWidgetGetWorldRect(pWidget);
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	tViewport.fW = pData->tScrollViewportRect.fW;
	tViewport.fH = pData->tScrollViewportRect.fH;
	iRet = xuiCodeMarginModelHitTest(pData->pMargins, tViewport, pEvent->fX, pEvent->fY, fLineHeight, pData->fScrollY, &tHit);
	if ( iRet == XUI_ERROR_UNSUPPORTED ) return XUI_OK;
	if ( iRet != XUI_OK ) return iRet;
	tHit.iLine = __xuiCodeEditVisibleRowToLine(pData, tHit.iLine);
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	if ( tHit.iLine < 0 || tHit.iLine >= iLineCount ) return XUI_OK;
	memset(&tInfo, 0, sizeof(tInfo));
	iRet = xuiCodeMarginModelGet(pData->pMargins, tHit.iIndex, &tInfo);
	if ( iRet != XUI_OK ) return iRet;
	tLineRect = tHit.tRect;
	tLineRect.fY = tViewport.fY + 4.0f + (float)__xuiCodeEditLineToVisibleRow(pData, tHit.iLine) * fLineHeight - pData->fScrollY;
	tLineRect.fH = fLineHeight;
	if ( tInfo.onEvent != NULL ) {
		iRet = tInfo.onEvent(pWidget, tInfo.iId, tHit.iLine, pEvent->iType, tLineRect, tInfo.pUser);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( pEvent->iType == XUI_EVENT_POINTER_DOWN && tInfo.iKind == XUI_CODE_MARGIN_FOLD ) {
		iRet = xuiCodeFoldStateToggleLine(pData->pFoldState, tHit.iLine);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
		if ( iRet == XUI_OK ) {
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	}
	if ( pDispatched != NULL ) *pDispatched = 1;
	return XUI_OK;
}

static int __xuiCodeEditMinimapScrollAt(xui_widget pWidget, xui_code_edit_data_t* pData, float fY)
{
	xui_rect_t tWorld;
	xui_rect_t tMinimap;
	float fMaxY;
	float fRatio;

	if ( pWidget == NULL || pData == NULL ||
	     (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_MINIMAP) == 0 ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tMinimap = pData->tMinimapRect;
	tMinimap.fX += tWorld.fX;
	tMinimap.fY += tWorld.fY;
	if ( tMinimap.fW <= 0.0f || tMinimap.fH <= 0.0f ) return XUI_ERROR_UNSUPPORTED;
	fMaxY = 0.0f;
	(void)xuiScrollModelGetMaxOffset(&pData->tScrollModel, NULL, &fMaxY);
	fRatio = (fY - tMinimap.fY) / tMinimap.fH;
	if ( fRatio < 0.0f ) fRatio = 0.0f;
	if ( fRatio > 1.0f ) fRatio = 1.0f;
	return xuiCodeEditSetScroll(pWidget, pData->fScrollX, fRatio * fMaxY);
}

static int __xuiCodeEditMinimapHit(xui_widget pWidget, xui_code_edit_data_t* pData, float fX, float fY)
{
	xui_rect_t tWorld;
	xui_rect_t tMinimap;

	if ( pWidget == NULL || pData == NULL ||
	     (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_MINIMAP) == 0 ) return 0;
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tMinimap = pData->tMinimapRect;
	tMinimap.fX += tWorld.fX;
	tMinimap.fY += tWorld.fY;
	return tMinimap.fW > 0.0f && tMinimap.fH > 0.0f &&
		fX >= tMinimap.fX && fX < tMinimap.fX + tMinimap.fW &&
		fY >= tMinimap.fY && fY < tMinimap.fY + tMinimap.fH;
}

static int __xuiCodeEditEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_code_edit_data_t* pData;
	const xui_edit_behavior_t* pBehavior;
	int iCommand;
	int bHandled;
	int bMarginEvent;
	int iRet;
	uint32_t iDocumentVersion;

	(void)pUser;
	if ( pWidget == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pBehavior = xuiInternalEditBehavior(pWidget);
	if ( (pEvent->pTarget == pData->pHScrollBar) || (pEvent->pTarget == pData->pVScrollBar) ) {
		return XUI_OK;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_FOCUS:
		if ( pBehavior != NULL && pBehavior->bSelectAllOnFocus ) {
			(void)__xuiCodeEditAdapterSelectAll(pWidget);
		}
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_OK;
	case XUI_EVENT_BLUR:
		if ( pBehavior != NULL && pBehavior->bCommitOnBlur ) {
			(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_COMMIT, NULL,
				xuiCodeDocumentGetLength(pData->pDocument), pData->iEditNotifyAnchor,
				pData->iEditNotifyCaret, 0, 0, 1);
		}
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		(void)__xuiCodeEditCloseAssistInternal(pData);
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		__xuiCodeEditImeReset(pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		(void)__xuiCodeEditCloseAssistInternal(pData);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( __xuiCodeEditMinimapHit(pWidget, pData, pEvent->fX, pEvent->fY) ) {
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			pData->bMinimapDragging = 1;
			iRet = __xuiCodeEditMinimapScrollAt(pWidget, pData, pEvent->fY);
			return (iRet == XUI_OK || iRet == XUI_ERROR_UNSUPPORTED) ?
				(int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		iRet = __xuiCodeEditDispatchMarginEvent(pWidget, pData, pEvent, &bMarginEvent);
		if ( iRet != XUI_OK ) return iRet;
		if ( bMarginEvent ) {
			(void)__xuiCodeEditInlineCompletionClearData(pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		pData->bDragging = 1;
		iRet = __xuiCodeEditPointerSelect(pWidget, pData, pEvent, ((pEvent->iModifiers & XUI_MOD_SHIFT) != 0));
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_MOVE:
		if ( pData->bMinimapDragging ) {
			if ( (pEvent->iButtons & XUI_POINTER_BUTTON_LEFT) == 0 ) {
				pData->bMinimapDragging = 0;
				(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
				return XUI_EVENT_DISPATCH_STOP;
			}
			iRet = __xuiCodeEditMinimapScrollAt(pWidget, pData, pEvent->fY);
			return (iRet == XUI_OK || iRet == XUI_ERROR_UNSUPPORTED) ?
				(int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		__xuiCodeEditUpdateDiagnosticHover(pWidget, pData, pEvent->fX, pEvent->fY, 0);
		if ( !pData->bDragging ) return XUI_OK;
		if ( (pEvent->iButtons & XUI_POINTER_BUTTON_LEFT) == 0 ) {
			pData->bDragging = 0;
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		iRet = __xuiCodeEditPointerSelect(pWidget, pData, pEvent, 1);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) return XUI_EVENT_DISPATCH_STOP;
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		if ( pData->bMinimapDragging ) {
			pData->bMinimapDragging = 0;
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		iRet = __xuiCodeEditDispatchMarginEvent(pWidget, pData, pEvent, &bMarginEvent);
		if ( iRet != XUI_OK ) return iRet;
		pData->bDragging = 0;
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_WHEEL:
		iRet = xuiCodeEditSetScroll(pWidget, pData->fScrollX + pEvent->fWheelX * 32.0f, pData->fScrollY - pEvent->fWheelY * 48.0f);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDragging = 0;
		pData->bMinimapDragging = 0;
		return XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		__xuiCodeEditUpdateDiagnosticHover(pWidget, pData, pEvent->fX, pEvent->fY, 1);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		iRet = __xuiCodeEditPointerSelectWordOrLine(pWidget, pData, pEvent);
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_CONTEXT_MENU:
	{
		float fMenuX = (float)pEvent->fX;
		float fMenuY = (float)pEvent->fY;
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( pEvent->iKey != XUI_KEY_CONTEXT_MENU && !xuiCodeSelectionHasSelection(pData->pSelection) ) {
			(void)__xuiCodeEditPointerSelect(pWidget, pData, pEvent, 0);
		} else if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
			xui_code_selection_t tSelection;
			xui_rect_t tAnchor;
			memset(&tSelection, 0, sizeof(tSelection));
			if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK &&
			     xuiCodeEditGetOffsetRect(pWidget, tSelection.iCaretOffset, &tAnchor) == XUI_OK ) {
				fMenuX = (float)tAnchor.fX;
				fMenuY = (float)(tAnchor.fY + tAnchor.fH);
			}
		}
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		(void)xuiCodeEditOpenMenu(pWidget, fMenuX, fMenuY);
		return XUI_EVENT_DISPATCH_STOP;
	}
	case XUI_EVENT_KEY_DOWN:
		if ( pData->pCompletionPopup != NULL && xuiPopupIsOpen(pData->pCompletionPopup) &&
		     pData->iCompletionCount > 0 ) {
			if ( pEvent->iKey == XUI_KEY_UP || pEvent->iKey == XUI_KEY_DOWN ||
			     pEvent->iKey == XUI_KEY_PAGE_UP || pEvent->iKey == XUI_KEY_PAGE_DOWN ||
			     pEvent->iKey == XUI_KEY_HOME || pEvent->iKey == XUI_KEY_END ) {
				if ( pEvent->iKey == XUI_KEY_UP ) {
					pData->iCompletionSelected--;
					if ( pData->iCompletionSelected < 0 ) pData->iCompletionSelected = pData->iCompletionCount - 1;
				} else if ( pEvent->iKey == XUI_KEY_DOWN ) {
					pData->iCompletionSelected++;
					if ( pData->iCompletionSelected >= pData->iCompletionCount ) pData->iCompletionSelected = 0;
				} else if ( pEvent->iKey == XUI_KEY_PAGE_UP ) {
					pData->iCompletionSelected -= 8;
					if ( pData->iCompletionSelected < 0 ) pData->iCompletionSelected = 0;
				} else if ( pEvent->iKey == XUI_KEY_PAGE_DOWN ) {
					pData->iCompletionSelected += 8;
					if ( pData->iCompletionSelected >= pData->iCompletionCount ) {
						pData->iCompletionSelected = pData->iCompletionCount - 1;
					}
				} else if ( pEvent->iKey == XUI_KEY_HOME ) {
					pData->iCompletionSelected = 0;
				} else {
					pData->iCompletionSelected = pData->iCompletionCount - 1;
				}
				(void)xuiListViewSetSelected(pData->pCompletionList, pData->iCompletionSelected);
				(void)xuiListViewEnsureVisible(pData->pCompletionList, pData->iCompletionSelected);
				__xuiCodeEditCompletionSyncInline(pWidget, pData);
				return XUI_EVENT_DISPATCH_STOP;
			}
			if ( pEvent->iKey == XUI_KEY_ENTER || pEvent->iKey == XUI_KEY_TAB ) {
				iRet = __xuiCodeEditCommitCompletionInternal(pWidget, pData, pData->iCompletionSelected);
				return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
			}
			if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
				(void)__xuiCodeEditCancelCompletionInternal(pData);
				(void)__xuiCodeEditInlineCompletionClearData(pData);
				(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		if ( pEvent->iKey == XUI_KEY_TAB && pEvent->iModifiers == 0u && __xuiCodeEditInlineCompletionValid(pData) ) {
			iRet = __xuiCodeEditExecuteCommand(pWidget, pData, XUI_CODE_COMMAND_ACCEPT_INLINE_COMPLETION, NULL, &bHandled);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		if ( pEvent->iKey == XUI_KEY_ESCAPE && pData->sInlineCompletion != NULL && pData->sInlineCompletion[0] != '\0' ) {
			(void)__xuiCodeEditExecuteCommand(pWidget, pData, XUI_CODE_COMMAND_CANCEL_INLINE_COMPLETION, NULL, &bHandled);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_ESCAPE && pData->pAssistPopup != NULL &&
		     xuiPopupIsOpen(pData->pAssistPopup) ) {
			(void)__xuiCodeEditCloseAssistInternal(pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pBehavior != NULL && pEvent->iKey == XUI_KEY_TAB &&
		     pBehavior->iTabBehavior == XUI_EDIT_TAB_FOCUS ) return XUI_OK;
		if ( pBehavior != NULL && pEvent->iKey == XUI_KEY_ENTER &&
		     pBehavior->iEnterBehavior == XUI_EDIT_ENTER_COMMIT ) {
			(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_COMMIT, NULL,
				xuiCodeDocumentGetLength(pData->pDocument), pData->iEditNotifyAnchor,
				pData->iEditNotifyCaret, 0, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pBehavior != NULL && pEvent->iKey == XUI_KEY_ESCAPE &&
		     pBehavior->iEscapeBehavior != XUI_EDIT_ESCAPE_DEFAULT ) {
			(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_CANCEL, NULL,
				xuiCodeDocumentGetLength(pData->pDocument), pData->iEditNotifyAnchor,
				pData->iEditNotifyCaret, 0, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		iRet = xuiCodeCommandMapFind(pData->pCommandMap, pEvent->iKey, pEvent->iModifiers, &iCommand);
		if ( iRet != XUI_OK && pEvent->iKey >= 'a' && pEvent->iKey <= 'z' ) {
			iRet = xuiCodeCommandMapFind(pData->pCommandMap, pEvent->iKey - 'a' + 'A', pEvent->iModifiers, &iCommand);
		}
		if ( iRet != XUI_OK ) return XUI_OK;
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		iDocumentVersion = xuiCodeDocumentGetVersion(pData->pDocument);
		iRet = __xuiCodeEditExecuteCommand(pWidget, pData, iCommand, NULL, &bHandled);
		if ( iRet == XUI_OK && bHandled && pData->iCompletionSourceCount > 0 ) {
			if ( (iCommand == XUI_CODE_COMMAND_DELETE_BACK ||
			     iCommand == XUI_CODE_COMMAND_DELETE_FORWARD) &&
			     iDocumentVersion != xuiCodeDocumentGetVersion(pData->pDocument) &&
			     __xuiCodeEditRefreshCompletionSession(pWidget, pData) == XUI_OK ) {
				/* Keep the provider result and restore candidates by local filtering. */
			} else if ( iCommand != XUI_CODE_COMMAND_COPY ) {
				(void)__xuiCodeEditCancelCompletionInternal(pData);
			}
		}
		if ( iRet == XUI_OK || iRet == XUI_ERROR_UNSUPPORTED ) return XUI_EVENT_DISPATCH_STOP;
		return iRet;
	case XUI_EVENT_TEXT:
		if ( pData->bImeComposing ) return XUI_EVENT_DISPATCH_STOP;
		return __xuiCodeEditInsertEventText(pWidget, pData, pEvent);
	case XUI_EVENT_IME_COMPOSITION:
		(void)__xuiCodeEditInlineCompletionClearData(pData);
		iRet = __xuiCodeEditImeComposition(pWidget, pData, pEvent);
		if ( iRet == XUI_OK || iRet == XUI_EVENT_DISPATCH_STOP ) {
			(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_COMPOSITION_CHANGED,
				pData->bImeComposing ? pData->sImeComposition : NULL,
				pData->bImeComposing && pData->sImeComposition != NULL ?
				(int)strlen(pData->sImeComposition) : 0,
				pData->iEditNotifyAnchor, pData->iEditNotifyCaret,
				pData->bImeComposing ? pData->iImeAnchorStart : 0,
				pData->bImeComposing ? pData->iImeAnchorEnd : 0, 1);
		}
		return iRet;
	case XUI_EVENT_COMMAND:
		if ( pEvent->iCommand != XUI_CODE_COMMAND_ACCEPT_INLINE_COMPLETION &&
		     pEvent->iCommand != XUI_CODE_COMMAND_CANCEL_INLINE_COMPLETION ) {
			(void)__xuiCodeEditInlineCompletionClearData(pData);
		}
		iRet = __xuiCodeEditExecuteCommand(pWidget, pData, pEvent->iCommand, pEvent->pData, &bHandled);
		if ( iRet == XUI_OK || iRet == XUI_ERROR_UNSUPPORTED ) return bHandled ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		return iRet;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCodeEditInitEvents(xui_widget pWidget)
{
	int iRet;

	if ( pWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_TEXT, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_IME_COMPOSITION, __xuiCodeEditEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_COMMAND, __xuiCodeEditEvent, NULL);
	return iRet;
}

static int __xuiCodeEditInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_code_edit_data_t* pData;
	const xui_code_edit_desc_t* pDesc;
	xui_edit_behavior_t tEditBehavior;
	xui_code_selection_t tSelection;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || !__xuiCodeEditDescValid((const xui_code_edit_desc_t*)pCreateData) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_code_edit_data_t*)pTypeData;
	pDesc = (const xui_code_edit_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->bReadonly = (pDesc != NULL && pDesc->bReadonly) ? 1 : 0;
	pData->bWordWrap = (pDesc != NULL && pDesc->bWordWrap) ? 1 : 0;
	pData->iTabColumns = (pDesc != NULL && pDesc->iTabColumns > 0) ? pDesc->iTabColumns : 4;
	pData->iIndentColumns = (pDesc != NULL && pDesc->iIndentColumns > 0) ? pDesc->iIndentColumns : pData->iTabColumns;
	pData->iEolMode = (pDesc != NULL && pDesc->iEolMode >= XUI_CODE_EOL_AUTO && pDesc->iEolMode <= XUI_CODE_EOL_CR) ? pDesc->iEolMode : XUI_CODE_EOL_LF;
	pData->iFlags = (pDesc != NULL) ? pDesc->iFlags : 0;
	pData->iDisplayOptions = (uint32_t)pData->iFlags &
		(XUI_CODE_EDIT_SHOW_WHITESPACE | XUI_CODE_EDIT_SHOW_EOL |
		 XUI_CODE_EDIT_SHOW_INDENT_GUIDES | XUI_CODE_EDIT_SHOW_MINIMAP);
	pData->fMinimapWidth = 96.0f;
	pData->iHoveredDiagnostic = -1;
	pData->iCompletionSelected = -1;
	pData->bCompletionAutoShow = 1;
	pData->iCompletionMinPrefix = 1;
	pData->iCompletionMaxItems = 256;
	pData->fCompletionDelay = 0.075f;
	xuiScrollModelInit(&pData->tScrollModel);
	if ( pDesc != NULL && pDesc->sLanguage != NULL ) {
		strncpy(pData->sLanguage, pDesc->sLanguage, sizeof(pData->sLanguage) - 1u);
	}
	iRet = __xuiCodeEditInitOwned(pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentSetText(pData->pDocument, (pDesc != NULL) ? pDesc->sText : NULL);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, xuiCodeDocumentGetLastError(pData->pDocument));
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetImeMode(pWidget, XUI_IME_ENABLED);
	(void)xuiWidgetSetImeCandidateRect(pWidget, __xuiCodeEditImeCandidateRect, NULL);
	memset(&tEditBehavior, 0, sizeof(tEditBehavior));
	tEditBehavior.iSize = sizeof(tEditBehavior);
	tEditBehavior.iTabBehavior = XUI_EDIT_TAB_INDENT;
	tEditBehavior.iEnterBehavior = XUI_EDIT_ENTER_NEWLINE;
	tEditBehavior.iEscapeBehavior = XUI_EDIT_ESCAPE_DEFAULT;
	iRet = xuiInternalEditRegister(pWidget, &g_xuiCodeEditAdapter, &tEditBehavior);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	pData->iEditNotifyVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK ) {
		pData->iEditNotifyAnchor = tSelection.iAnchorOffset;
		pData->iEditNotifyCaret = tSelection.iCaretOffset;
	}
	iRet = __xuiCodeEditInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit event wiring failed");
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	iRet = __xuiCodeEditInitScrollBars(pWidget, pData);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit scrollbar create failed");
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	iRet = __xuiCodeEditInitCompletion(pWidget, pData);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit completion popup create failed");
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	iRet = __xuiCodeEditInitAssist(pWidget, pData);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit assist popup create failed");
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	iRet = __xuiCodeEditInitMenu(pWidget, pData);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit menu create failed");
		__xuiCodeEditDestroyOwned(pData);
		return iRet;
	}
	return XUI_OK;
}

static void __xuiCodeEditDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_code_edit_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_code_edit_data_t*)pTypeData;
	__xuiCodeEditDestroyOwned(pData);
	if ( pData != NULL ) memset(pData, 0, sizeof(*pData));
}

static int __xuiCodeEditMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pSize = (xui_vec2_t){320.0f, 220.0f};
	return XUI_OK;
}

static void __xuiCodeEditDefaultLayout(xui_layout_t* pLayout)
{
	if ( pLayout == NULL ) return;
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
	pLayout->fPreferredWidth = 320.0f;
	pLayout->fPreferredHeight = 220.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiCodeEditDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	if ( pPolicy == NULL ) return;
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCodeEditDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiCodeEditAlpha(iColor) == 0 ) return XUI_OK;
	return (pProxy != NULL && pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor) : XUI_OK;
}

static int __xuiCodeEditInvalidateView(xui_widget pWidget)
{
	uint32_t iStateId;
	int i;

	if ( pWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iStateId = xuiWidgetGetStateId(pWidget);
	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		if ( pWidget->pCacheSlots[i].iStateId == iStateId ) {
			pWidget->pCacheSlots[i].iFlags |= XUI_WIDGET_DIRTY_CACHE;
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static xui_rect_t __xuiCodeEditIntersectRect(xui_rect_t tA, xui_rect_t tB)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = (tA.fX > tB.fX) ? tA.fX : tB.fX;
	fTop = (tA.fY > tB.fY) ? tA.fY : tB.fY;
	fRight = ((tA.fX + tA.fW) < (tB.fX + tB.fW)) ? (tA.fX + tA.fW) : (tB.fX + tB.fW);
	fBottom = ((tA.fY + tA.fH) < (tB.fY + tB.fH)) ? (tA.fY + tA.fH) : (tB.fY + tB.fH);
	if ( (fRight <= fLeft) || (fBottom <= fTop) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return (xui_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop};
}

static int __xuiCodeEditBodyClipBegin(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tClip, xui_rect_t* pOldClip, int* pHadOldClip, int* pActive)
{
	xui_rect_t tOldClip;
	int bHadOldClip;
	int iRet;

	if ( pHadOldClip != NULL ) *pHadOldClip = 0;
	if ( pActive != NULL ) *pActive = 0;
	if ( (pProxy == NULL) || (pDraw == NULL) || (pOldClip == NULL) || (pHadOldClip == NULL) || (pActive == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pProxy->drawClipGet == NULL) || (pProxy->drawClipSet == NULL) || (pProxy->drawClipClear == NULL) ) {
		return XUI_OK;
	}
	memset(&tOldClip, 0, sizeof(tOldClip));
	bHadOldClip = 0;
	iRet = pProxy->drawClipGet(pProxy, pDraw, &tOldClip, &bHadOldClip);
	if ( iRet != XUI_OK ) return iRet;
	*pOldClip = tOldClip;
	*pHadOldClip = bHadOldClip ? 1 : 0;
	if ( bHadOldClip ) {
		tClip = __xuiCodeEditIntersectRect(tClip, tOldClip);
	}
	iRet = pProxy->drawClipSet(pProxy, pDraw, tClip);
	if ( iRet != XUI_OK ) return iRet;
	*pActive = 1;
	return XUI_OK;
}

static int __xuiCodeEditBodyClipEnd(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tOldClip, int bHadOldClip, int bActive)
{
	if ( !bActive ) return XUI_OK;
	if ( (pProxy == NULL) || (pDraw == NULL) || (pProxy->drawClipSet == NULL) || (pProxy->drawClipClear == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return bHadOldClip ? pProxy->drawClipSet(pProxy, pDraw, tOldClip) : pProxy->drawClipClear(pProxy, pDraw);
}

static int __xuiCodeEditRenderLineText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, int iStart, int iEnd, xui_rect_t tRect, uint32_t iColor)
{
	char sSmall[512];
	char* sLine;
	int iLength;
	int i;
	int iRet;

	if ( pProxy == NULL || pProxy->drawText == NULL || pFont == NULL || sText == NULL ) return XUI_OK;
	if ( iEnd < iStart ) iEnd = iStart;
	iLength = iEnd - iStart;
	if ( iLength <= 0 ) return XUI_OK;
	sLine = (iLength < (int)sizeof(sSmall)) ? sSmall : (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	for ( i = 0; i < iLength; i++ ) {
		unsigned char ch = (unsigned char)sText[iStart + i];
		sLine[i] = (ch < 32u) ? ' ' : (char)ch;
	}
	sLine[iLength] = '\0';
	iRet = pProxy->drawText(pProxy, pDraw, pFont, sLine, xuiInternalSnapRect(tRect), iColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
	if ( sLine != sSmall ) xrtFree(sLine);
	return iRet;
}

static int __xuiCodeEditRenderLineTextVisual(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iStart, int iEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight, uint32_t iColor)
{
	xui_rect_t tRect;
	int i;
	int iNext;
	int iSpanStart;
	int iVisualStart;
	int iVisualEnd;
	int iRet;
	int iSpanColumns;
	uint32_t iCodepoint;
	double fSpanX;
	double fSpanW;

	if ( iEnd <= iStart ) return XUI_OK;
	i = iStart;
	while ( i < iEnd ) {
		if ( sText[i] == '\t' ) {
			i++;
			continue;
		}
		iNext = __xuiCodeEditUtf8Next(sText, iEnd, i, &iCodepoint);
		if ( iNext <= i ) iNext = i + 1;
		iSpanStart = i;
		iSpanColumns = __xuiCodeEditCodepointColumns(iCodepoint);
		i = iNext;
		while ( i < iEnd && sText[i] != '\t' ) {
			iNext = __xuiCodeEditUtf8Next(sText, iEnd, i, &iCodepoint);
			if ( iNext <= i ) iNext = i + 1;
			if ( __xuiCodeEditCodepointColumns(iCodepoint) != iSpanColumns ) break;
			i = iNext;
		}
		fSpanX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, iLineStart, iSpanStart, fColumnWidth);
		fSpanW = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, iLineStart, i, fColumnWidth) - fSpanX;
		if ( fSpanW <= 0.0f ) {
			iVisualStart = __xuiCodeEditLineVisualColumn(pData, sText, iLineStart, iSpanStart);
			iVisualEnd = __xuiCodeEditLineVisualColumn(pData, sText, iLineStart, i);
			fSpanX = (float)iVisualStart * fColumnWidth;
			fSpanW = (float)(iVisualEnd - iVisualStart) * fColumnWidth;
		}
		tRect = (xui_rect_t){
			fTextX + 4.0f + fSpanX - pData->fScrollX,
			fY,
			fSpanW,
			fLineHeight
		};
		iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, sText, iSpanStart, i, tRect, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( pColor == NULL ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( iRet == XUI_OK && tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static uint32_t __xuiCodeEditColor(xui_widget pWidget, const char* sName, uint32_t iDefaultColor)
{
	uint32_t iColor;

	iColor = iDefaultColor;
	(void)__xuiCodeEditStyleColor(pWidget, sName, &iColor);
	return iColor;
}

static uint32_t __xuiCodeEditDiagnosticColor(xui_widget pWidget, int iSeverity)
{
	switch ( iSeverity ) {
	case XUI_CODE_DIAGNOSTIC_WARNING:
		return __xuiCodeEditColor(pWidget, "codeedit.diagnostic.warning.color", XUI_COLOR_RGBA(217, 119, 6, 255));
	case XUI_CODE_DIAGNOSTIC_INFO:
		return __xuiCodeEditColor(pWidget, "codeedit.diagnostic.info.color", XUI_COLOR_RGBA(37, 99, 235, 255));
	case XUI_CODE_DIAGNOSTIC_HINT:
		return __xuiCodeEditColor(pWidget, "codeedit.diagnostic.hint.color", XUI_COLOR_RGBA(100, 116, 139, 255));
	case XUI_CODE_DIAGNOSTIC_ERROR:
	default:
		return __xuiCodeEditColor(pWidget, "codeedit.diagnostic.error.color", XUI_COLOR_RGBA(220, 38, 38, 255));
	}
}

static const char* __xuiCodeEditSyntaxColorProperty(int iTokenKind)
{
	switch ( iTokenKind ) {
	case XUI_CODE_TOKEN_TEXT:
	case XUI_CODE_TOKEN_IDENTIFIER: return "codeedit.text.color";
	case XUI_CODE_TOKEN_KEYWORD: return "codeedit.syntax.keyword.color";
	case XUI_CODE_TOKEN_TYPE: return "codeedit.syntax.type.color";
	case XUI_CODE_TOKEN_NUMBER: return "codeedit.syntax.number.color";
	case XUI_CODE_TOKEN_STRING: return "codeedit.syntax.string.color";
	case XUI_CODE_TOKEN_CHAR: return "codeedit.syntax.char.color";
	case XUI_CODE_TOKEN_COMMENT: return "codeedit.syntax.comment.color";
	case XUI_CODE_TOKEN_PREPROCESSOR: return "codeedit.syntax.preprocessor.color";
	case XUI_CODE_TOKEN_OPERATOR: return "codeedit.syntax.operator.color";
	case XUI_CODE_TOKEN_BRACE: return "codeedit.syntax.brace.color";
	case XUI_CODE_TOKEN_ERROR: return "codeedit.syntax.error.color";
	default: break;
	}
	return NULL;
}

static int __xuiCodeEditEnsureTokens(xui_code_edit_data_t* pData, int iStartOffset, int iEndOffset)
{
	xui_code_token_t* pTokens;
	uint32_t iVersion;
	uint32_t iBufferVersion;
	int iBufferStart;
	int iBufferEnd;
	int iLength;
	int iCount;
	int iRet;

	if ( pData == NULL || pData->pDocument == NULL || pData->pTokenBuffer == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	iLength = xuiCodeDocumentGetLength(pData->pDocument);
	if ( iStartOffset < 0 ) iStartOffset = 0;
	if ( iStartOffset > iLength ) iStartOffset = iLength;
	if ( iEndOffset < iStartOffset ) iEndOffset = iStartOffset;
	if ( iEndOffset > iLength ) iEndOffset = iLength;
	if ( iEndOffset - iStartOffset > XUI_CODE_EDIT_TOKEN_WINDOW_BYTES ) iEndOffset = iStartOffset + XUI_CODE_EDIT_TOKEN_WINDOW_BYTES;
	if ( xuiCodeTokenBufferGetRange(pData->pTokenBuffer, &iBufferVersion, &iBufferStart, &iBufferEnd) == XUI_OK &&
	     iBufferVersion == iVersion && iStartOffset >= iBufferStart && iEndOffset <= iBufferEnd ) {
		return XUI_OK;
	}
	iCount = 0;
	iRet = xuiCodeLexerCTokenizeDocumentRange(pData->pDocument,
		iStartOffset, iEndOffset, NULL, 0, &iCount);
	if ( iRet != XUI_OK ) return iRet;
	if ( iCount <= 0 ) return xuiCodeTokenBufferSetRange(pData->pTokenBuffer, NULL, 0, iVersion, iStartOffset, iEndOffset);
	pTokens = (xui_code_token_t*)xrtMalloc(sizeof(*pTokens) * (size_t)iCount);
	if ( pTokens == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiCodeLexerCTokenizeDocumentRange(pData->pDocument,
		iStartOffset, iEndOffset, pTokens, iCount, &iCount);
	if ( iRet == XUI_OK ) iRet = xuiCodeTokenBufferSetRange(pData->pTokenBuffer, pTokens, iCount, iVersion, iStartOffset, iEndOffset);
	xrtFree(pTokens);
	return iRet;
}

static int __xuiCodeEditRenderTokenSpan(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, xui_code_token_t tToken, double fTextX, float fY, float fColumnWidth, float fLineHeight)
{
	xui_code_style_t tStyle;
	const char* sProperty;
	uint32_t iColor;
	int iStart;
	int iEnd;
	int iRet;

	if ( tToken.iEndOffset <= iLineStart || tToken.iStartOffset >= iLineEnd ) return XUI_OK;
	iStart = (tToken.iStartOffset > iLineStart) ? tToken.iStartOffset : iLineStart;
	iEnd = (tToken.iEndOffset < iLineEnd) ? tToken.iEndOffset : iLineEnd;
	if ( iEnd <= iStart ) return XUI_OK;
	memset(&tStyle, 0, sizeof(tStyle));
	iRet = xuiCodeThemeGetTokenStyle(pData->pTheme, tToken.iKind, &tStyle);
	if ( iRet != XUI_OK || __xuiCodeEditAlpha(tStyle.iForeground) == 0 ) return XUI_OK;
	sProperty = __xuiCodeEditSyntaxColorProperty(tToken.iKind);
	iColor = (sProperty != NULL) ? __xuiCodeEditColor(pWidget, sProperty, tStyle.iForeground) : tStyle.iForeground;
	return __xuiCodeEditRenderLineTextVisual(pProxy, pDraw, pFont, pData, sText, iLineStart, iStart, iEnd, fTextX, fY, fColumnWidth, fLineHeight, iColor);
}

static int __xuiCodeEditRenderStyledLine(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iRenderStart, int iRenderEnd, const xui_code_token_t* pTokens, int iTokenCount, double fTextX, float fY, float fColumnWidth, float fLineHeight, uint32_t iTextColor)
{
	xui_code_edit_line_layout_t* pLayout;
	xui_code_token_t tToken;
	int iCursor;
	int iToken;
	int iStart;
	int iEnd;
	int iRet;

	if ( iRenderEnd <= iRenderStart ) return XUI_OK;
	if ( pProxy != NULL && pProxy->drawTextSpans != NULL ) {
		pLayout = __xuiCodeEditLineLayoutEnsure(pProxy, pFont, pData, sText, (int)strlen(sText), fColumnWidth);
		if ( pLayout != NULL ) {
			int iSegment;
			for ( iSegment = 0; iSegment < pLayout->iSegmentCount; iSegment++ ) {
				xui_code_edit_text_segment_t* pSegment = &pLayout->pSegments[iSegment];
				xui_text_paint_span_t arrSpans[128];
				xui_rect_t tRect;
				int iSpanCount = 0;
				if ( pSegment->iEnd <= iRenderStart || pSegment->iStart >= iRenderEnd ) continue;
				for ( iToken = 0; iToken < iTokenCount && iSpanCount < (int)(sizeof(arrSpans) / sizeof(arrSpans[0])); iToken++ ) {
					xui_code_style_t tStyle;
					const char* sProperty;
					int iSpanStart = pTokens[iToken].iStartOffset;
					int iSpanEnd = pTokens[iToken].iEndOffset;
					if ( iSpanEnd <= pSegment->iStart || iSpanStart >= pSegment->iEnd ) continue;
					memset(&tStyle, 0, sizeof(tStyle));
					if ( xuiCodeThemeGetTokenStyle(pData->pTheme, pTokens[iToken].iKind, &tStyle) != XUI_OK ||
					     __xuiCodeEditAlpha(tStyle.iForeground) == 0 ) continue;
					sProperty = __xuiCodeEditSyntaxColorProperty(pTokens[iToken].iKind);
					arrSpans[iSpanCount].iSize = sizeof(arrSpans[iSpanCount]);
					arrSpans[iSpanCount].iStart = (iSpanStart > pSegment->iStart ? iSpanStart : pSegment->iStart) - pSegment->iStart;
					arrSpans[iSpanCount].iEnd = (iSpanEnd < pSegment->iEnd ? iSpanEnd : pSegment->iEnd) - pSegment->iStart;
					arrSpans[iSpanCount].iColor = sProperty != NULL ?
						__xuiCodeEditColor(pWidget, sProperty, tStyle.iForeground) : tStyle.iForeground;
					iSpanCount++;
				}
				tRect = (xui_rect_t){fTextX + 4.0f + pSegment->fX - pData->fScrollX,
					fY, pSegment->tShape.fWidth, fLineHeight};
				iRet = pProxy->drawTextSpans(pProxy, pDraw, pFont,
					pLayout->sText + pSegment->iStart, pSegment->iEnd - pSegment->iStart,
					tRect, iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP,
					arrSpans, iSpanCount);
				if ( iRet != XUI_OK ) return iRet;
			}
			return XUI_OK;
		}
	}
	iCursor = iRenderStart;
	for ( iToken = 0; iToken < iTokenCount; iToken++ ) {
		if ( pTokens[iToken].iEndOffset <= iRenderStart ) continue;
		if ( pTokens[iToken].iStartOffset >= iRenderEnd ) break;
		iStart = (pTokens[iToken].iStartOffset > iRenderStart) ? pTokens[iToken].iStartOffset : iRenderStart;
		iEnd = (pTokens[iToken].iEndOffset < iRenderEnd) ? pTokens[iToken].iEndOffset : iRenderEnd;
		if ( iStart > iCursor ) {
			iRet = __xuiCodeEditRenderLineTextVisual(pProxy, pDraw, pFont, pData, sText, iLineStart, iCursor, iStart, fTextX, fY, fColumnWidth, fLineHeight, iTextColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( iEnd > iStart ) {
			tToken = pTokens[iToken];
			tToken.iStartOffset = iStart;
			tToken.iEndOffset = iEnd;
			iRet = __xuiCodeEditRenderTokenSpan(pWidget, pProxy, pDraw, pFont, pData, sText, iLineStart, iRenderEnd, tToken, fTextX, fY, fColumnWidth, fLineHeight);
			if ( iRet != XUI_OK ) return iRet;
			if ( iEnd > iCursor ) iCursor = iEnd;
		}
	}
	if ( iCursor < iRenderEnd ) {
		iRet = __xuiCodeEditRenderLineTextVisual(pProxy, pDraw, pFont, pData, sText, iLineStart, iCursor, iRenderEnd, fTextX, fY, fColumnWidth, fLineHeight, iTextColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditRenderWhitespace(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight)
{
	uint32_t iMarkerColor;
	xui_rect_t tRect;
	const char* sMark;
	int i;
	int iIndentColumns;
	int iRet;

	if ( pData == NULL || pData->iDisplayOptions == 0u ) return XUI_OK;
	if ( sText == NULL ) return XUI_OK;
	iMarkerColor = __xuiCodeEditColor(pWidget, "codeedit.whitespace.color", XUI_COLOR_RGBA(148, 163, 184, 255));
	if ( (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_INDENT_GUIDES) != 0u ) {
		iIndentColumns = 0;
		for ( i = iLineStart; i < iLineEnd; i++ ) {
			if ( sText[i] == ' ' ) iIndentColumns++;
			else if ( sText[i] == '\t' ) iIndentColumns += __xuiCodeEditTabAdvance(pData, iIndentColumns);
			else break;
			if ( iIndentColumns > 0 && (iIndentColumns % __xuiCodeEditIndentColumns(pData)) == 0 ) {
				tRect = (xui_rect_t){
					fTextX + 4.0f + (float)iIndentColumns * fColumnWidth - pData->fScrollX,
					fY + 2.0f,
					1.0f,
					fLineHeight - 4.0f
				};
				iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tRect, iMarkerColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	for ( i = iLineStart; i < iLineEnd; i++ ) {
		sMark = NULL;
		if ( (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_WHITESPACE) != 0u ) {
			if ( sText[i] == ' ' ) sMark = ".";
			else if ( sText[i] == '\t' ) sMark = ">";
		}
		if ( sMark != NULL ) {
			tRect = (xui_rect_t){
				fTextX + 4.0f + __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, iLineStart, i, fColumnWidth) - pData->fScrollX,
				fY,
				(sText[i] == '\t') ? (float)__xuiCodeEditTabAdvance(pData, __xuiCodeEditLineVisualColumn(pData, sText, iLineStart, i)) * fColumnWidth : fColumnWidth,
				fLineHeight
			};
			iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, sMark, 0, (int)strlen(sMark), tRect, iMarkerColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_EOL) != 0u ) {
		tRect = (xui_rect_t){
			fTextX + 4.0f + __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, iLineStart, iLineEnd, fColumnWidth) - pData->fScrollX,
			fY,
			fColumnWidth,
			fLineHeight
		};
		iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, "$", 0, 1, tRect, iMarkerColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditRenderSelectionRange(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, const xui_code_selection_t* pSelection, int iLineStart, int iLineEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight, uint32_t iColor)
{
	xui_rect_t tSel;
	const char* sText;
	int iSelStart;
	int iSelEnd;
	int iStart;
	int iEnd;
	double fStartX;
	double fEndX;

	if ( pData == NULL || pSelection == NULL ) return XUI_OK;
	iSelStart = pSelection->iAnchorOffset;
	iSelEnd = pSelection->iCaretOffset;
	if ( iSelEnd < iSelStart ) {
		int iTemp = iSelStart;
		iSelStart = iSelEnd;
		iSelEnd = iTemp;
	}
	iStart = (iSelStart > iLineStart) ? iSelStart : iLineStart;
	iEnd = (iSelEnd < iLineEnd) ? iSelEnd : iLineEnd;
	if ( iEnd <= iStart ) return XUI_OK;
	if ( __xuiCodeEditReadRange(pData, iLineStart, iLineEnd, &sText) != XUI_OK ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	fStartX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
		iStart - iLineStart, fColumnWidth);
	fEndX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
		iEnd - iLineStart, fColumnWidth);
	tSel = (xui_rect_t){
		tContent.fX + fTextX + 4.0f + fStartX - pData->fScrollX,
		fY,
		fEndX - fStartX,
		fLineHeight
	};
	if ( tSel.fX < tContent.fX ) {
		tSel.fW -= (tContent.fX - tSel.fX);
		tSel.fX = tContent.fX;
	}
	if ( tSel.fX + tSel.fW > tContent.fX + tContent.fW ) tSel.fW = tContent.fX + tContent.fW - tSel.fX;
	if ( tSel.fW <= 0.0f ) return XUI_OK;
	return __xuiCodeEditDrawRectFill(pProxy, pDraw, tSel, iColor);
}

static int __xuiCodeEditRenderSelection(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, int iLine, int iLineStart, int iLineEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight, uint32_t iColor)
{
	xui_code_selection_t tSelection;
	int i;
	int iCount;
	int iRet;

	if ( pData == NULL ) return XUI_OK;
	iCount = xuiCodeSelectionGetCount(pData->pSelection);
	for ( i = 0; i < iCount; i++ ) {
		memset(&tSelection, 0, sizeof(tSelection));
		iRet = xuiCodeSelectionGetAt(pData->pSelection, i, &tSelection);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiCodeEditRenderSelectionRange(pProxy, pDraw, pFont, pData, tContent, &tSelection, iLineStart, iLineEnd, fTextX, fY, fColumnWidth, fLineHeight, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)iLine;
	return XUI_OK;
}

static int __xuiCodeEditRenderFindRange(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, int iLineStart, int iLineEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight, int iStart, int iEnd, uint32_t iColor)
{
	xui_rect_t tRect;
	const char* sText;
	double fStartX;
	double fEndX;

	if ( pData == NULL || iEnd <= iStart ) return XUI_OK;
	if ( iStart < iLineStart ) iStart = iLineStart;
	if ( iEnd > iLineEnd ) iEnd = iLineEnd;
	if ( iEnd <= iStart ) return XUI_OK;
	if ( __xuiCodeEditReadRange(pData, iLineStart, iLineEnd, &sText) != XUI_OK ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	fStartX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
		iStart - iLineStart, fColumnWidth);
	fEndX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
		iEnd - iLineStart, fColumnWidth);
	tRect = (xui_rect_t){
		tContent.fX + fTextX + 4.0f + fStartX - pData->fScrollX,
		fY,
		fEndX - fStartX,
		fLineHeight
	};
	if ( tRect.fX < tContent.fX ) {
		tRect.fW -= (tContent.fX - tRect.fX);
		tRect.fX = tContent.fX;
	}
	if ( tRect.fX + tRect.fW > tContent.fX + tContent.fW ) tRect.fW = tContent.fX + tContent.fW - tRect.fX;
	if ( tRect.fW <= 0.0f ) return XUI_OK;
	return __xuiCodeEditDrawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiCodeEditRenderFindResults(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, int iLineStart, int iLineEnd, double fTextX, float fY, float fColumnWidth, float fLineHeight, uint32_t iResultColor, uint32_t iActiveColor)
{
	const xui_find_result_t* pResult;
	uint32_t iColor;
	int iLow;
	int iHigh;
	int iMid;
	int i;
	int iRet;

	if ( pData == NULL || pData->iFindResultCount <= 0 ) return XUI_OK;
	iLow = 0;
	iHigh = pData->iFindResultCount;
	while ( iLow < iHigh ) {
		iMid = iLow + (iHigh - iLow) / 2;
		if ( pData->pFindResults[iMid].iEnd <= iLineStart ) iLow = iMid + 1;
		else iHigh = iMid;
	}
	for ( i = iLow; i < pData->iFindResultCount; i++ ) {
		pResult = &pData->pFindResults[i];
		if ( pResult->iStart >= iLineEnd ) break;
		iColor = (i == pData->iFindActiveIndex) ? iActiveColor : iResultColor;
		iRet = __xuiCodeEditRenderFindRange(pProxy, pDraw, pFont, pData, tContent, iLineStart, iLineEnd, fTextX, fY, fColumnWidth, fLineHeight, pResult->iStart, pResult->iEnd, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditRenderInlineCompletion(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont,
	xui_code_edit_data_t* pData, xui_rect_t tTextContent, xui_rect_t tCaret,
	float fCaretLineY, float fTextOffsetY, float fColumnWidth, float fLineHeight, uint32_t iColor)
{
	const char* sText;
	int iLength;
	int iStart;
	int iEnd;
	int iLine;
	int iRet;
	double fTextX;
	float fTextY;

	if ( pData == NULL || !__xuiCodeEditInlineCompletionValid((xui_code_edit_data_t*)pData) ) return XUI_OK;
	sText = pData->sInlineCompletion;
	iLength = (int)strlen(sText);
	iStart = 0;
	iLine = 0;
	while ( iStart <= iLength ) {
		iEnd = iStart;
		while ( iEnd < iLength && sText[iEnd] != '\n' && sText[iEnd] != '\r' ) iEnd++;
		fTextX = (iLine == 0) ? ((double)tCaret.fX - 4.0 + pData->fScrollX) : tTextContent.fX;
		fTextY = fCaretLineY + fTextOffsetY + (float)iLine * fLineHeight;
		if ( fTextY > tTextContent.fY + tTextContent.fH ) break;
		if ( iEnd > iStart ) {
			iRet = __xuiCodeEditRenderLineTextVisual(pProxy, pDraw, pFont, pData, sText,
				iStart, iStart, iEnd, fTextX, fTextY, fColumnWidth, fLineHeight, iColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( iEnd >= iLength ) break;
		if ( sText[iEnd] == '\r' && iEnd + 1 < iLength && sText[iEnd + 1] == '\n' ) iEnd++;
		iStart = iEnd + 1;
		iLine++;
	}
	return XUI_OK;
}

static int __xuiCodeEditDrawDiagnosticWave(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	double fX;
	float fNextX;
	float fY;
	float fNextY;
	double fEndX;
	int iUp;
	int iRet;

	if ( tRect.fW <= 0.0f ) return XUI_OK;
	if ( pProxy == NULL || pProxy->drawLine == NULL ) return __xuiCodeEditDrawRectFill(pProxy, pDraw, tRect, iColor);
	fX = tRect.fX;
	fY = tRect.fY + 0.5f;
	fEndX = tRect.fX + tRect.fW;
	iUp = 0;
	while ( fX < fEndX ) {
		fNextX = fX + 2.0f;
		if ( fNextX > fEndX ) fNextX = fEndX;
		fNextY = tRect.fY + (iUp ? 0.5f : 2.0f);
		iRet = pProxy->drawLine(pProxy, pDraw, fX, fY, fNextX, fNextY, 1.0f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		fX = fNextX;
		fY = fNextY;
		iUp = !iUp;
	}
	return XUI_OK;
}

static int __xuiCodeEditRenderDiagnostics(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, int iLineStart, int iLineEnd, float fY, float fColumnWidth, float fLineHeight)
{
	xui_code_diagnostic_t arrLocal[32];
	xui_code_diagnostic_t* pDiagnostics;
	xui_code_diagnostic_t* pAllocated;
	xui_code_diagnostic_t* pDiagnostic;
	xui_rect_t tRect;
	const char* sText;
	double fStartX;
	double fEndX;
	int iCount;
	int iSeverity;
	int iStart;
	int iEnd;
	int i;
	int iRet;

	if ( pData == NULL || pData->pAnnotations == NULL ) return XUI_OK;
	pAllocated = NULL;
	pDiagnostics = arrLocal;
	iRet = xuiCodeAnnotationGetDiagnosticsInRange(pData->pAnnotations, iLineStart,
		(iLineEnd > iLineStart) ? iLineEnd : iLineStart + 1,
		arrLocal, NULL, (int)(sizeof(arrLocal) / sizeof(arrLocal[0])), &iCount);
	if ( iRet != XUI_OK || iCount <= 0 ) return iRet;
	if ( iCount > (int)(sizeof(arrLocal) / sizeof(arrLocal[0])) ) {
		pAllocated = (xui_code_diagnostic_t*)xrtMalloc(sizeof(*pAllocated) * (size_t)iCount);
		if ( pAllocated == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pDiagnostics = pAllocated;
		iRet = xuiCodeAnnotationGetDiagnosticsInRange(pData->pAnnotations, iLineStart,
			(iLineEnd > iLineStart) ? iLineEnd : iLineStart + 1,
			pDiagnostics, NULL, iCount, &iCount);
		if ( iRet != XUI_OK ) {
			xrtFree(pAllocated);
			return iRet;
		}
	}
	iRet = __xuiCodeEditReadRange(pData, iLineStart, iLineEnd, &sText);
	if ( iRet != XUI_OK ) {
		xrtFree(pAllocated);
		return iRet;
	}
	/* Draw less severe diagnostics first so errors remain visible when ranges overlap. */
	for ( iSeverity = XUI_CODE_DIAGNOSTIC_HINT; iSeverity >= XUI_CODE_DIAGNOSTIC_ERROR; iSeverity-- ) {
		for ( i = 0; i < iCount; i++ ) {
			pDiagnostic = &pDiagnostics[i];
			if ( pDiagnostic->iSeverity != iSeverity ) continue;
			iStart = (pDiagnostic->tRange.iStart > iLineStart) ? pDiagnostic->tRange.iStart : iLineStart;
			iEnd = (pDiagnostic->tRange.iEnd < iLineEnd) ? pDiagnostic->tRange.iEnd : iLineEnd;
			if ( iStart > iLineEnd ) continue;
			if ( iEnd < iStart ) iEnd = iStart;
			fStartX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
				iStart - iLineStart, fColumnWidth);
			fEndX = __xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
				iEnd - iLineStart, fColumnWidth);
			if ( fEndX <= fStartX ) fEndX = fStartX + fColumnWidth;
			tRect = (xui_rect_t){
				tContent.fX + 4.0f + fStartX - pData->fScrollX,
				fY + fLineHeight - 3.0f,
				fEndX - fStartX,
				3.0f
			};
			if ( tRect.fX < tContent.fX ) {
				tRect.fW -= (tContent.fX - tRect.fX);
				tRect.fX = tContent.fX;
			}
			if ( tRect.fX + tRect.fW > tContent.fX + tContent.fW ) tRect.fW = tContent.fX + tContent.fW - tRect.fX;
			if ( tRect.fW <= 0.0f ) continue;
			iRet = __xuiCodeEditDrawDiagnosticWave(pProxy, pDraw, tRect, __xuiCodeEditDiagnosticColor(pWidget, iSeverity));
			if ( iRet != XUI_OK ) {
				xrtFree(pAllocated);
				return iRet;
			}
		}
	}
	xrtFree(pAllocated);
	return XUI_OK;
}

static int __xuiCodeEditLineHasFoldHeader(xui_code_edit_data_t* pData, int iLine)
{
	xui_code_fold_range_t tRange;
	int i;
	int iCount;

	if ( pData == NULL || pData->pFoldState == NULL ) return 0;
	iCount = xuiCodeFoldStateGetCount(pData->pFoldState);
	for ( i = 0; i < iCount; i++ ) {
		if ( xuiCodeFoldStateGetRange(pData->pFoldState, i, &tRange) == XUI_OK &&
		     tRange.iStartLine == iLine ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiCodeEditRenderMargins(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_code_edit_data_t* pData, xui_rect_t tContent, const xui_code_margin_info_t* pMargins, int iMarginCount, int iLine, int iActiveLine, int iLineStart, int iLineEnd, float fY, float fLineHeight)
{
	xui_code_marker_t arrMarkers[8];
	xui_code_diagnostic_t arrDiagnostics[8];
	xui_rect_t tLineRect;
	char sNumber[32];
	int i;
	int iCount;
	int iRet;

	if ( pMargins == NULL || iMarginCount <= 0 ) return XUI_OK;
	for ( i = 0; i < iMarginCount; i++ ) {
		tLineRect = (xui_rect_t){pMargins[i].tRect.fX, fY, pMargins[i].tRect.fW, fLineHeight};
		if ( tLineRect.fX + tLineRect.fW > tContent.fW ) continue;
		if ( pMargins[i].onRender != NULL ) {
			iRet = pMargins[i].onRender(pWidget, pMargins[i].iId, iLine, pDraw, tLineRect, pMargins[i].pUser);
			if ( iRet != XUI_OK ) return iRet;
			continue;
		}
		if ( pMargins[i].iKind == XUI_CODE_MARGIN_LINE_NUMBER ) {
			if ( iLine == iActiveLine ) {
				iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tLineRect,
					__xuiCodeEditColor(pWidget, "codeedit.line_number.active_background_color", XUI_COLOR_RGBA(226,232,240,255)));
				if ( iRet != XUI_OK ) return iRet;
			}
			snprintf(sNumber, sizeof(sNumber), "%d", iLine + 1);
			iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, sNumber, 0, (int)strlen(sNumber),
				(xui_rect_t){tLineRect.fX + 2.0f, tLineRect.fY, tLineRect.fW - 4.0f, tLineRect.fH},
				(iLine == iActiveLine) ? __xuiCodeEditColor(pWidget, "codeedit.line_number.active_color", XUI_COLOR_RGBA(15,23,42,255)) :
				__xuiCodeEditColor(pWidget, "codeedit.line_number.color", XUI_COLOR_RGBA(100,116,139,255)));
			if ( iRet != XUI_OK ) return iRet;
		} else if ( pMargins[i].iKind == XUI_CODE_MARGIN_MARKER ) {
			iCount = 0;
			iRet = xuiCodeAnnotationGetMarkers(pData->pAnnotations, iLine, arrMarkers, (int)(sizeof(arrMarkers) / sizeof(arrMarkers[0])), &iCount);
			if ( iRet != XUI_OK ) return iRet;
			if ( iCount > 0 ) {
				iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){tLineRect.fX + 4.0f, tLineRect.fY + 5.0f, 7.0f, 7.0f},
					__xuiCodeEditColor(pWidget, "codeedit.marker.color", XUI_COLOR_RGBA(37,99,235,255)));
				if ( iRet != XUI_OK ) return iRet;
			}
		} else if ( pMargins[i].iKind == XUI_CODE_MARGIN_FOLD ) {
			if ( __xuiCodeEditLineHasFoldHeader(pData, iLine) ) {
				iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, "-", 0, 1, (xui_rect_t){tLineRect.fX + 4.0f, tLineRect.fY, 8.0f, tLineRect.fH},
					__xuiCodeEditColor(pWidget, "codeedit.fold.color", XUI_COLOR_RGBA(71,85,105,255)));
				if ( iRet != XUI_OK ) return iRet;
			}
		} else if ( pMargins[i].iKind == XUI_CODE_MARGIN_DIAGNOSTIC ) {
			iCount = 0;
			iRet = xuiCodeAnnotationGetDiagnosticsAt(pData->pAnnotations, iLineStart, arrDiagnostics, (int)(sizeof(arrDiagnostics) / sizeof(arrDiagnostics[0])), &iCount);
			if ( iRet != XUI_OK ) return iRet;
			if ( iCount == 0 && iLineEnd > iLineStart ) {
				iRet = xuiCodeAnnotationGetDiagnosticsAt(pData->pAnnotations, iLineEnd - 1, arrDiagnostics, (int)(sizeof(arrDiagnostics) / sizeof(arrDiagnostics[0])), &iCount);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( iCount > 0 ) {
				uint32_t iColor = __xuiCodeEditDiagnosticColor(pWidget, arrDiagnostics[0].iSeverity);
				iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){tLineRect.fX + 4.0f, tLineRect.fY + 5.0f, 7.0f, 7.0f}, iColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiCodeEditRenderMinimap(xui_widget pWidget, xui_proxy pProxy,
	xui_draw_context pDraw, xui_code_edit_data_t* pData)
{
	char sLine[121];
	xui_rect_t tRect;
	xui_rect_t tBar;
	xui_rect_t tViewport;
	uint32_t iBackground;
	uint32_t iText;
	uint32_t iComment;
	uint32_t iViewport;
	uint32_t iViewportBorder;
	uint32_t iDivider;
	float fMaxY;
	float fThumbHeight;
	float fThumbY;
	float fScaleX;
	int iLineCount;
	int iPixelRows;
	int iLine;
	int iStart;
	int iEnd;
	int iLeading;
	int iLength;
	int iRow;
	int iRet;

	if ( pWidget == NULL || pProxy == NULL || pData == NULL ||
	     (pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_MINIMAP) == 0 ) return XUI_OK;
	tRect = pData->tMinimapRect;
	if ( tRect.fW <= 1.0f || tRect.fH <= 1.0f ) return XUI_OK;
	iBackground = __xuiCodeEditColor(pWidget, "codeedit.minimap.background_color",
		XUI_COLOR_RGBA(241, 245, 249, 255));
	iText = __xuiCodeEditColor(pWidget, "codeedit.minimap.text_color",
		XUI_COLOR_RGBA(100, 116, 139, 150));
	iComment = __xuiCodeEditColor(pWidget, "codeedit.minimap.comment_color",
		XUI_COLOR_RGBA(34, 139, 94, 150));
	iViewport = __xuiCodeEditColor(pWidget, "codeedit.minimap.viewport_color",
		XUI_COLOR_RGBA(59, 130, 246, 46));
	iViewportBorder = __xuiCodeEditColor(pWidget, "codeedit.minimap.viewport_border_color",
		XUI_COLOR_RGBA(37, 99, 235, 116));
	iDivider = __xuiCodeEditColor(pWidget, "codeedit.minimap.divider_color",
		XUI_COLOR_RGBA(203, 213, 225, 255));
	iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
		(xui_rect_t){tRect.fX, tRect.fY, 1.0f, tRect.fH}, iDivider);
	if ( iRet != XUI_OK ) return iRet;
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	iPixelRows = (int)tRect.fH;
	if ( iPixelRows < 1 ) iPixelRows = 1;
	fScaleX = (tRect.fW - 8.0f) / 120.0f;
	if ( fScaleX < 0.35f ) fScaleX = 0.35f;
	for ( iRow = 0; iRow < iPixelRows; iRow++ ) {
		iLine = (int)(((uint64_t)iRow * (uint64_t)iLineCount) / (uint64_t)iPixelRows);
		if ( iLine >= iLineCount ) iLine = iLineCount - 1;
		if ( iLine < 0 || xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd) != XUI_OK ) continue;
		if ( iEnd > iStart + 120 ) iEnd = iStart + 120;
		if ( xuiCodeDocumentCopyRange(pData->pDocument, iStart, iEnd,
			sLine, (int)sizeof(sLine), NULL) != XUI_OK ) continue;
		iLeading = 0;
		while ( iStart + iLeading < iEnd &&
		       (sLine[iLeading] == ' ' || sLine[iLeading] == '\t') &&
		       iLeading < 32 ) iLeading++;
		iLength = iEnd - iStart - iLeading;
		if ( iLength <= 0 ) continue;
		if ( iLength > 120 - iLeading ) iLength = 120 - iLeading;
		tBar = (xui_rect_t){
			tRect.fX + 4.0f + (float)iLeading * fScaleX,
			tRect.fY + (float)iRow,
			(float)iLength * fScaleX,
			1.0f
		};
		if ( tBar.fX + tBar.fW > tRect.fX + tRect.fW - 2.0f ) {
			tBar.fW = tRect.fX + tRect.fW - 2.0f - tBar.fX;
		}
		if ( tBar.fW > 0.0f ) {
			uint32_t iColor = (iStart + iLeading + 1 < iEnd &&
				sLine[iLeading] == '/' && sLine[iLeading + 1] == '/') ?
				iComment : iText;
			iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tBar, iColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	fMaxY = 0.0f;
	(void)xuiScrollModelGetMaxOffset(&pData->tScrollModel, NULL, &fMaxY);
	if ( fMaxY > 0.0f ) {
		fThumbHeight = tRect.fH * pData->tScrollViewportRect.fH /
			(pData->tScrollViewportRect.fH + fMaxY);
		if ( fThumbHeight < 20.0f ) fThumbHeight = 20.0f;
		if ( fThumbHeight > tRect.fH ) fThumbHeight = tRect.fH;
		fThumbY = tRect.fY + (tRect.fH - fThumbHeight) * (pData->fScrollY / fMaxY);
		tViewport = (xui_rect_t){tRect.fX + 1.0f, fThumbY,
			tRect.fW - 1.0f, fThumbHeight};
		iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tViewport, iViewport);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
			(xui_rect_t){tViewport.fX, tViewport.fY, tViewport.fW, 1.0f}, iViewportBorder);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
			(xui_rect_t){tViewport.fX, tViewport.fY + tViewport.fH - 1.0f,
				tViewport.fW, 1.0f}, iViewportBorder);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCodeEditCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_code_edit_data_t* pData;
	xui_context pContext;
	xui_proxy_t* pProxy;
	xui_font pFont;
	xui_rect_t tRect;
	xui_rect_t tContent;
	xui_rect_t tTextContent;
	xui_rect_t tCaret;
	xui_rect_t tIme;
	xui_code_margin_info_t arrMargins[12];
	xui_code_selection_t tSelection;
	const char* sText;
	uint32_t iBackgroundColor;
	uint32_t iDividerColor;
	uint32_t iTextColor;
	uint32_t iCurrentLineColor;
	uint32_t iSelectionColor;
	uint32_t iFindResultColor;
	uint32_t iFindActiveColor;
	uint32_t iCaretColor;
	uint32_t iImeColor;
	uint32_t iInlineCompletionColor;
	int iLineCount;
	int iMarginCount;
	int iLine;
	int iVisibleRow;
	int iFirstVisibleRow;
	int iFirstLine;
	int iLastLine;
	int iVisibleRows;
	int iVisibleCount;
	int iStart;
	int iEnd;
	int iRenderStart;
	int iRenderEnd;
	int iPaintStart;
	int iPaintEnd;
	int iWrapSegment;
	int iTokenStart;
	int iTokenEnd;
	int iActiveLine;
	int iCaretLine;
	int iCaretColumn;
	int iCaretColumnOffset;
	int iCaretStart;
	int iCaretEnd;
	int iCaretVisualRow;
	float fY;
	float fColumnWidth;
	float fLineHeight;
	float fCaretHeight;
	float fCaretLineY;
	double fCaretWrapX;
	float fTextOffsetY;
	float fTextY;
	double fWrapStartX;
	float fTextBaseX;
	xui_code_token_t arrTokens[256];
	int iTokenCount;
	int iRet;

	(void)iStateId;
	(void)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) return XUI_OK;
	__xuiCodeEditSyncAssistColors(pWidget, pData);
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	tRect = xuiWidgetGetContentRect(pWidget);
	tContent = pData->tScrollViewportRect;
	memset(arrMargins, 0, sizeof(arrMargins));
	iMarginCount = 0;
	tTextContent = tContent;
	iRet = xuiCodeMarginModelLayout(pData->pMargins, tContent, arrMargins, (int)(sizeof(arrMargins) / sizeof(arrMargins[0])), &iMarginCount, &tTextContent);
	if ( iRet != XUI_OK ) return iRet;
	iBackgroundColor = pData->bReadonly ?
		__xuiCodeEditColor(pWidget, "codeedit.readonly.background_color", XUI_COLOR_RGBA(241, 245, 249, 255)) :
		__xuiCodeEditColor(pWidget, "codeedit.background.color", XUI_COLOR_RGBA(248, 250, 252, 255));
	iDividerColor = __xuiCodeEditColor(pWidget, "codeedit.divider.color", XUI_COLOR_RGBA(203, 213, 225, 255));
	iCurrentLineColor = __xuiCodeEditColor(pWidget, "codeedit.current_line.color", XUI_COLOR_RGBA(226, 232, 240, 128));
	iSelectionColor = __xuiCodeEditColor(pWidget, "codeedit.selection.color", XUI_COLOR_RGBA(74, 144, 239, 255));
	iFindResultColor = __xuiCodeEditColor(pWidget, "codeedit.find.result_color", XUI_COLOR_RGBA(255, 235, 128, 150));
	iFindActiveColor = __xuiCodeEditColor(pWidget, "codeedit.find.active_color", XUI_COLOR_RGBA(255, 183, 77, 190));
	iCaretColor = __xuiCodeEditColor(pWidget, "codeedit.caret.color", XUI_COLOR_RGBA(15, 23, 42, 255));
	iImeColor = __xuiCodeEditColor(pWidget, "codeedit.ime.color", XUI_COLOR_RGBA(37, 99, 235, 255));
	iInlineCompletionColor = __xuiCodeEditColor(pWidget, "codeedit.inline_completion.color", XUI_COLOR_RGBA(100, 116, 139, 170));
	iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){0.0f, 0.0f, tRect.fW, tRect.fH}, iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( tTextContent.fX > 0.0f ) {
		iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){0.0f, 0.0f, tTextContent.fX, tContent.fH},
			__xuiCodeEditColor(pWidget, "codeedit.margin.background_color", XUI_COLOR_RGBA(241,245,249,255)));
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){tTextContent.fX - 1.0f, 0.0f, 1.0f, tContent.fH}, iDividerColor);
	if ( iRet != XUI_OK ) return iRet;
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	fCaretHeight = __xuiCodeEditCaretHeight(pWidget, pData, fLineHeight);
	fTextOffsetY = __xuiCodeEditLineTextOffsetY(pWidget, pData, fLineHeight);
	iTextColor = pData->bReadonly ?
		__xuiCodeEditColor(pWidget, "codeedit.text.readonly_color", XUI_COLOR_RGBA(71, 85, 105, 255)) :
		__xuiCodeEditColor(pWidget, "codeedit.text.color", XUI_COLOR_RGBA(15, 23, 42, 255));
	iLineCount = xuiCodeDocumentGetLineCount(pData->pDocument);
	iActiveLine = -1;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK ) {
		(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, tSelection.iCaretOffset, &iActiveLine, NULL);
	}
	iFirstVisibleRow = (int)((pData->fScrollY - 4.0f) / fLineHeight);
	if ( iFirstVisibleRow < 0 ) iFirstVisibleRow = 0;
	if ( iFirstVisibleRow > 0 ) iFirstVisibleRow--;
	iVisibleRows = (fLineHeight > 0.0f) ? ((int)(tContent.fH / fLineHeight) + 4) : 4;
	iFirstLine = __xuiCodeEditVisibleRowToLine(pData, iFirstVisibleRow);
	iLastLine = __xuiCodeEditVisibleRowToLine(pData, iFirstVisibleRow + iVisibleRows);
	if ( iLastLine < iFirstLine ) iLastLine = iFirstLine;
	iTokenStart = 0;
	iTokenEnd = 0;
	if ( iLineCount > 0 ) {
		iStart = 0;
		iEnd = 0;
		if ( xuiCodeDocumentGetLineRange(pData->pDocument, iFirstLine, &iStart, &iEnd) == XUI_OK ) iTokenStart = iStart;
		iStart = 0;
		iEnd = 0;
		if ( xuiCodeDocumentGetLineRange(pData->pDocument, iLastLine, &iStart, &iEnd) == XUI_OK ) iTokenEnd = iEnd;
		if ( iTokenEnd < iTokenStart ) iTokenEnd = iTokenStart;
	}
	iRet = __xuiCodeEditEnsureTokens(pData, iTokenStart, iTokenEnd);
	if ( iRet != XUI_OK ) return iRet;
	iVisibleCount = __xuiCodeEditVisibleLineCount(pData);
	for ( iVisibleRow = iFirstVisibleRow; iVisibleRow < iVisibleCount; iVisibleRow++ ) {
		if ( __xuiCodeEditVisibleRowIsInlineCompletion(pData, iVisibleRow) ) continue;
		iWrapSegment = 0;
		fWrapStartX = 0.0f;
		iPaintStart = 0;
		iPaintEnd = 0;
		if ( pData->bWordWrap ) {
			iRet = __xuiCodeEditWrapVisualSegment(pWidget, pData,
				iVisibleRow, &iLine, &iWrapSegment, &iPaintStart,
				&iPaintEnd, &fWrapStartX);
			if ( iRet != XUI_OK ) return iRet;
		} else {
			iLine = __xuiCodeEditVisibleRowToLine(pData, iVisibleRow);
		}
		if ( iLine < 0 || iLine >= iLineCount ) break;
		fY = 4.0f + (float)iVisibleRow * fLineHeight - pData->fScrollY;
		if ( fY + fLineHeight < 0.0f ) {
			continue;
		}
		if ( fY > tContent.fH ) break;
		fTextY = fY + fTextOffsetY;
		iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiCodeEditReadRange(pData, iStart, iEnd, &sText);
		if ( iRet != XUI_OK ) return iRet;
		if ( pData->bWordWrap ) {
			iRenderStart = iPaintStart - iStart;
			iRenderEnd = iPaintEnd - iStart;
		} else {
			iPaintStart = iStart;
			iPaintEnd = iEnd;
			__xuiCodeEditVisibleByteRange(pData, sText, 0, iEnd - iStart,
				fColumnWidth, tTextContent.fW, &iRenderStart, &iRenderEnd);
		}
		iRenderStart += iStart;
		iRenderEnd += iStart;
		fTextBaseX = tTextContent.fX - fWrapStartX;
		iTokenCount = 0;
		iRet = xuiCodeTokenBufferGetTokensInRange(pData->pTokenBuffer, xuiCodeDocumentGetVersion(pData->pDocument), iStart, iEnd, arrTokens, (int)(sizeof(arrTokens) / sizeof(arrTokens[0])), &iTokenCount);
		if ( iRet != XUI_OK ) iTokenCount = 0;
		if ( iTokenCount > (int)(sizeof(arrTokens) / sizeof(arrTokens[0])) ) iTokenCount = (int)(sizeof(arrTokens) / sizeof(arrTokens[0]));
		{
			int iTokenIndex;
			for ( iTokenIndex = 0; iTokenIndex < iTokenCount; iTokenIndex++ ) {
				arrTokens[iTokenIndex].iStartOffset -= iStart;
				arrTokens[iTokenIndex].iEndOffset -= iStart;
			}
		}
		iRet = (iWrapSegment == 0) ?
			__xuiCodeEditRenderMargins(pWidget, pProxy, pDraw, pFont, pData,
				tContent, arrMargins, iMarginCount, iLine, iActiveLine,
				iStart, iEnd, fY, fLineHeight) : XUI_OK;
		if ( iRet != XUI_OK ) return iRet;
		{
			xui_rect_t tOldClip;
			int bHadOldClip;
			int bClipActive;
			int iClipRet;

			iRet = __xuiCodeEditBodyClipBegin(pProxy, pDraw, tTextContent, &tOldClip, &bHadOldClip, &bClipActive);
			if ( iRet != XUI_OK ) return iRet;
			if ( iLine == iActiveLine ) {
				iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, (xui_rect_t){tTextContent.fX, fY, tTextContent.fW, fLineHeight}, iCurrentLineColor);
			}
			if ( iRet == XUI_OK ) {
				iRet = __xuiCodeEditRenderFindResults(pProxy, pDraw, pFont, pData,
					tTextContent, iPaintStart, iPaintEnd, 0.0f, fY,
					fColumnWidth, fLineHeight, iFindResultColor, iFindActiveColor);
			}
			if ( iRet == XUI_OK ) {
				iRet = __xuiCodeEditRenderSelection(pProxy, pDraw, pFont, pData,
					tTextContent, iLine, iPaintStart, iPaintEnd, 0.0f,
					fY, fColumnWidth, fLineHeight, iSelectionColor);
			}
			if ( iRet == XUI_OK ) {
				iRet = __xuiCodeEditRenderStyledLine(pWidget, pProxy, pDraw, pFont,
					pData, sText, 0, iRenderStart - iStart, iRenderEnd - iStart,
					arrTokens, iTokenCount, fTextBaseX, fTextY,
					fColumnWidth, fLineHeight, iTextColor);
			}
			if ( iRet == XUI_OK ) {
				iRet = __xuiCodeEditRenderWhitespace(pWidget, pProxy, pDraw, pFont,
					pData, sText, iRenderStart - iStart, iRenderEnd - iStart,
					fTextBaseX, fTextY, fColumnWidth, fLineHeight);
			}
			if ( iRet == XUI_OK ) {
				iRet = __xuiCodeEditRenderDiagnostics(pWidget, pProxy, pDraw, pFont,
					pData, tTextContent, iPaintStart, iPaintEnd, fY,
					fColumnWidth, fLineHeight);
			}
			iClipRet = __xuiCodeEditBodyClipEnd(pProxy, pDraw, tOldClip, bHadOldClip, bClipActive);
			if ( iRet != XUI_OK ) return iRet;
			if ( iClipRet != XUI_OK ) return iClipRet;
		}
	}
	{
		int iSelectionIndex;
		int iSelectionCount;

		iSelectionCount = xuiCodeSelectionGetCount(pData->pSelection);
		for ( iSelectionIndex = 0; iSelectionIndex < iSelectionCount; iSelectionIndex++ ) {
			int bCaretVisible;
			int bInlineVisible;
			int iInlineExtraRows;

			memset(&tSelection, 0, sizeof(tSelection));
			if ( xuiCodeSelectionGetAt(pData->pSelection, iSelectionIndex, &tSelection) != XUI_OK ) continue;
			if ( tSelection.iAnchorOffset != tSelection.iCaretOffset &&
			     !(pData->bImeComposing && iSelectionIndex == XUI_CODE_SELECTION_PRIMARY) ) continue;
			if ( xuiCodeDocumentOffsetToLineColumn(pData->pDocument, (pData->bImeComposing && iSelectionIndex == XUI_CODE_SELECTION_PRIMARY) ? pData->iImeAnchorStart : tSelection.iCaretOffset, &iCaretLine, &iCaretColumn) != XUI_OK ) continue;
			iCaretStart = 0;
			iCaretEnd = 0;
			(void)xuiCodeDocumentGetLineRange(pData->pDocument, iCaretLine, &iCaretStart, &iCaretEnd);
			iRet = __xuiCodeEditReadRange(pData, iCaretStart, iCaretEnd, &sText);
			if ( iRet != XUI_OK ) return iRet;
			iCaretColumnOffset = iCaretStart;
			(void)xuiCodeDocumentLineColumnToOffset(pData->pDocument, iCaretLine, iCaretColumn, &iCaretColumnOffset);
			iCaretVisualRow = __xuiCodeEditLineToVisibleRow(pData, iCaretLine);
			fCaretWrapX = -1.0f;
			if ( pData->bWordWrap ) {
				iRet = __xuiCodeEditWrapOffsetGeometry(pWidget, pData,
					(pData->bImeComposing && iSelectionIndex == XUI_CODE_SELECTION_PRIMARY) ?
						pData->iImeAnchorStart : tSelection.iCaretOffset,
					&iCaretVisualRow, NULL, NULL, &fCaretWrapX);
				if ( iRet != XUI_OK ) return iRet;
			}
			fCaretLineY = 4.0f + (float)iCaretVisualRow * fLineHeight - pData->fScrollY;
			tCaret = (xui_rect_t){
				tTextContent.fX + 4.0f + ((fCaretWrapX >= 0.0f) ? fCaretWrapX :
					__xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
						iCaretColumnOffset - iCaretStart, fColumnWidth)) - pData->fScrollX,
				fCaretLineY + fTextOffsetY,
				1.0f,
				fCaretHeight
			};
			bCaretVisible = tCaret.fX >= 0.0f && tCaret.fX <= tContent.fW &&
				tCaret.fY + tCaret.fH >= 0.0f && tCaret.fY <= tContent.fH;
			iInlineExtraRows = 0;
			bInlineVisible = iSelectionIndex == XUI_CODE_SELECTION_PRIMARY && !pData->bImeComposing &&
				__xuiCodeEditInlineCompletionLayout(pData, NULL, NULL, &iInlineExtraRows) &&
				fCaretLineY + (float)(iInlineExtraRows + 1) * fLineHeight >= 0.0f && fCaretLineY <= tContent.fH;
			if ( bCaretVisible || bInlineVisible ) {
				xui_rect_t tOldClip;
				int bHadOldClip;
				int bClipActive;
				int iClipRet;

				iRet = __xuiCodeEditBodyClipBegin(pProxy, pDraw, tTextContent, &tOldClip, &bHadOldClip, &bClipActive);
				if ( iRet != XUI_OK ) return iRet;
				if ( bInlineVisible || (iSelectionIndex == XUI_CODE_SELECTION_PRIMARY && !pData->bImeComposing && bCaretVisible) ) {
					iRet = __xuiCodeEditRenderInlineCompletion(pProxy, pDraw, pFont, pData, tTextContent, tCaret,
						fCaretLineY, fTextOffsetY, fColumnWidth, fLineHeight, iInlineCompletionColor);
				}
				if ( bCaretVisible && iSelectionIndex == XUI_CODE_SELECTION_PRIMARY && pData->bImeComposing && pData->sImeComposition != NULL ) {
					int iImeLength = (int)strlen(pData->sImeComposition);
					int iImeEndLine = iCaretLine;
					int iSuffixStart = iCaretEnd;
					float fImeAnchorX = tCaret.fX;
					float fImeWidth = __xuiCodeEditShapedTextOffsetX(pProxy, pFont,
						pData->sImeComposition, iImeLength, iImeLength, (float)iImeLength * fColumnWidth);
					float fImeCursorX = __xuiCodeEditShapedTextOffsetX(pProxy, pFont,
						pData->sImeComposition, iImeLength, pData->iImeCursor, (float)pData->iImeCursor * fColumnWidth);
					float fImeSelectX0 = __xuiCodeEditShapedTextOffsetX(pProxy, pFont,
						pData->sImeComposition, iImeLength, pData->iImeSelectionStart,
						(float)pData->iImeSelectionStart * fColumnWidth);
					float fImeSelectX1 = __xuiCodeEditShapedTextOffsetX(pProxy, pFont,
						pData->sImeComposition, iImeLength, pData->iImeSelectionEnd,
						(float)pData->iImeSelectionEnd * fColumnWidth);

					(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument,
						pData->iImeAnchorEnd, &iImeEndLine, NULL);
					iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
						(xui_rect_t){fImeAnchorX, fCaretLineY, tTextContent.fX + tTextContent.fW - fImeAnchorX, fLineHeight},
						iBackgroundColor);
					if ( iRet == XUI_OK && iCaretLine == iActiveLine ) {
						iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
							(xui_rect_t){fImeAnchorX, fCaretLineY, tTextContent.fX + tTextContent.fW - fImeAnchorX, fLineHeight},
							iCurrentLineColor);
					}
					if ( iRet == XUI_OK && fImeSelectX1 > fImeSelectX0 ) {
						iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
							(xui_rect_t){fImeAnchorX + fImeSelectX0, fCaretLineY,
								fImeSelectX1 - fImeSelectX0, fLineHeight}, iSelectionColor);
					}
					tIme = (xui_rect_t){fImeAnchorX, fCaretLineY,
						tTextContent.fX + tTextContent.fW - fImeAnchorX, fLineHeight};
					if ( iRet == XUI_OK && iImeEndLine == iCaretLine ) {
						iSuffixStart = pData->iImeAnchorEnd - iCaretStart;
						if ( iSuffixStart < 0 ) iSuffixStart = 0;
						if ( iSuffixStart < iCaretEnd - iCaretStart ) {
							tIme.fX = fImeAnchorX + fImeWidth;
							tIme.fW = tTextContent.fX + tTextContent.fW - tIme.fX;
							iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont, sText,
								iSuffixStart, iCaretEnd - iCaretStart, tIme, iTextColor);
						}
					}
					tIme.fX = fImeAnchorX;
					tIme.fW = tTextContent.fX + tTextContent.fW - fImeAnchorX;
					if ( iRet == XUI_OK && iImeLength > 0 ) {
						iRet = __xuiCodeEditRenderLineText(pProxy, pDraw, pFont,
							pData->sImeComposition, 0, iImeLength, tIme, iImeColor);
					}
					if ( iRet == XUI_OK && fImeWidth > 0.0f ) {
						iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw,
							(xui_rect_t){fImeAnchorX, fCaretLineY + fLineHeight - 2.0f, fImeWidth, 1.0f}, iImeColor);
					}
					tCaret.fX = fImeAnchorX + fImeCursorX;
				}
				if ( iRet == XUI_OK && bCaretVisible && xuiInternalCaretBlinkVisible(pWidget) ) {
					iRet = __xuiCodeEditDrawRectFill(pProxy, pDraw, tCaret, iCaretColor);
				}
				iClipRet = __xuiCodeEditBodyClipEnd(pProxy, pDraw, tOldClip, bHadOldClip, bClipActive);
				if ( iRet != XUI_OK ) return iRet;
				if ( iClipRet != XUI_OK ) return iClipRet;
			}
		}
	}
	iRet = __xuiCodeEditRenderMinimap(pWidget, pProxy, pDraw, pData);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_OK;
}

static void __xuiCodeEditRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiCodeEditRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty = XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.margin.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.line_number.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.line_number.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.line_number.active_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.marker.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.fold.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.assist.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.assist.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.assist.documentation.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);

	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.text.readonly_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.readonly.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.current_line.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.divider.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.selection.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.find.result_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.find.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.inline_completion.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.comment_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.viewport_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.viewport_border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.minimap.divider_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.caret.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.ime.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.whitespace.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.diagnostic.error.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.diagnostic.warning.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.diagnostic.info.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.diagnostic.hint.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.keyword.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.type.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.number.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.string.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.char.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.comment.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.preprocessor.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.operator.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.brace.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "codeedit.syntax.error.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCodeEditRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, XUI_WIDGET_DIRTY_LAYOUT | iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiCodeEditGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "codeedit");
	if ( pType != NULL ) {
		__xuiCodeEditRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "codeedit";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_code_edit_data_t);
	tDesc.onInit = __xuiCodeEditInit;
	tDesc.onDestroy = __xuiCodeEditDestroy;
	tDesc.onContentMeasure = __xuiCodeEditMeasure;
	tDesc.onCacheRender = __xuiCodeEditCacheRender;
	tDesc.onUpdate = __xuiCodeEditUpdate;
	tDesc.onQueryCursor = __xuiCodeEditQueryCursor;
	__xuiCodeEditDefaultLayout(&tDesc.tLayout);
	__xuiCodeEditDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiCodeEditRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCodeEditCreate(xui_context pContext, xui_widget* ppWidget, const xui_code_edit_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiCodeEditDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiCodeEditGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_code_document xuiCodeEditGetDocument(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pDocument : NULL;
}

XUI_API xui_code_selection_model xuiCodeEditGetSelection(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pSelection : NULL;
}

XUI_API xui_code_theme xuiCodeEditGetTheme(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pTheme : NULL;
}

XUI_API int xuiCodeEditSetTheme(xui_widget pWidget, xui_code_theme pTheme)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pTheme == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeThemeCopy(pData->pTheme, pTheme);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditSetStyle(xui_widget pWidget, int iStyleId, const xui_code_style_t* pStyle)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeThemeSetStyle(pData->pTheme, iStyleId, pStyle);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_code_fold_state xuiCodeEditGetFoldState(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pFoldState : NULL;
}

XUI_API xui_code_annotation_store xuiCodeEditGetAnnotations(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pAnnotations : NULL;
}

XUI_API int xuiCodeEditHitTestDiagnostic(xui_widget pWidget, float fX, float fY, xui_code_diagnostic_hit_t* pHit)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditHitTestDiagnosticInternal(pWidget, pData, fX, fY, pHit);
}

XUI_API int xuiCodeEditSetDiagnosticHover(xui_widget pWidget, xui_code_diagnostic_hover_proc onHover, void* pUser)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iHoveredDiagnostic >= 0 && pData->onDiagnosticHover != NULL ) {
		pData->onDiagnosticHover(pWidget, NULL, pData->pDiagnosticHoverUser);
	}
	pData->onDiagnosticHover = onHover;
	pData->pDiagnosticHoverUser = pUser;
	pData->iHoveredDiagnostic = -1;
	memset(&pData->tHoveredDiagnosticRect, 0, sizeof(pData->tHoveredDiagnosticRect));
	return XUI_OK;
}

XUI_API xui_code_token_buffer xuiCodeEditGetTokenBuffer(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pTokenBuffer : NULL;
}

XUI_API xui_code_provider_set xuiCodeEditGetProviders(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pProviders : NULL;
}

XUI_API xui_code_margin_model xuiCodeEditGetMargins(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pMargins : NULL;
}

static int __xuiCodeEditDefaultMarginVisible(xui_code_edit_data_t* pData, int iKind)
{
	xui_code_margin_info_t tInfo;
	int iCount;
	int i;

	if ( pData == NULL || pData->pMargins == NULL ) return 0;
	iCount = xuiCodeMarginModelGetCount(pData->pMargins);
	for ( i = 0; i < iCount; i++ ) {
		if ( xuiCodeMarginModelGet(pData->pMargins, i, &tInfo) != XUI_OK ) continue;
		if ( tInfo.iKind == iKind ) return ((tInfo.iFlags & XUI_CODE_MARGIN_VISIBLE) != 0) ? 1 : 0;
	}
	return 0;
}

XUI_API int xuiCodeEditSetDefaultMargins(xui_widget pWidget, int bShowLineNumbers, int bShowMarkerMargin, int bShowFoldMargin, int bShowDiagnosticMargin)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	int iRet;

	if ( pData == NULL || pData->pMargins == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeMarginModelLoadDefaults(pData->pMargins,
		bShowLineNumbers ? 1 : 0,
		bShowMarkerMargin ? 1 : 0,
		bShowFoldMargin ? 1 : 0,
		bShowDiagnosticMargin ? 1 : 0);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetDefaultMargins(xui_widget pWidget, int* pShowLineNumbers, int* pShowMarkerMargin, int* pShowFoldMargin, int* pShowDiagnosticMargin)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pShowLineNumbers != NULL ) *pShowLineNumbers = __xuiCodeEditDefaultMarginVisible(pData, XUI_CODE_MARGIN_LINE_NUMBER);
	if ( pShowMarkerMargin != NULL ) *pShowMarkerMargin = __xuiCodeEditDefaultMarginVisible(pData, XUI_CODE_MARGIN_MARKER);
	if ( pShowFoldMargin != NULL ) *pShowFoldMargin = __xuiCodeEditDefaultMarginVisible(pData, XUI_CODE_MARGIN_FOLD);
	if ( pShowDiagnosticMargin != NULL ) *pShowDiagnosticMargin = __xuiCodeEditDefaultMarginVisible(pData, XUI_CODE_MARGIN_DIAGNOSTIC);
	return XUI_OK;
}

XUI_API xui_code_command_map xuiCodeEditGetCommandMap(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pCommandMap : NULL;
}

XUI_API xui_code_language_registry xuiCodeEditGetLanguageRegistry(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pLanguages : NULL;
}

XUI_API int xuiCodeEditSetLanguage(xui_widget pWidget, const char* sLanguage)
{
	xui_code_edit_data_t* pData;
	xui_code_language_t tLanguage;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || sLanguage == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sLanguage[0] != '\0' ) {
		memset(&tLanguage, 0, sizeof(tLanguage));
		if ( xuiCodeLanguageRegistryFind(pData->pLanguages, sLanguage, &tLanguage) != XUI_OK ) {
			__xuiCodeEditSetError(pData, "CodeEdit language not registered");
			return XUI_ERROR_UNSUPPORTED;
		}
	}
	strncpy(pData->sLanguage, sLanguage, sizeof(pData->sLanguage) - 1u);
	pData->sLanguage[sizeof(pData->sLanguage) - 1u] = '\0';
	__xuiCodeEditSetError(pData, "");
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiCodeEditGetLanguage(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->sLanguage : "";
}

XUI_API xui_widget xuiCodeEditGetMenuWidget(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pMenu : NULL;
}

XUI_API int xuiCodeEditSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_code_edit_data_t* pData;
	char* sNew;
	int iIndex;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeEditMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_CODE_EDIT_MENU_TITLE_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sTitle == NULL || sTitle[0] == '\0' ) {
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = NULL;
		if ( pData->pMenu != NULL ) (void)__xuiCodeEditUpdateMenu(pWidget, pData);
		return XUI_OK;
	}
	sNew = __xuiCodeEditStringDup(sTitle);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	xrtFree(pData->arrMenuTitle[iIndex]);
	pData->arrMenuTitle[iIndex] = sNew;
	if ( pData->pMenu != NULL ) (void)__xuiCodeEditUpdateMenu(pWidget, pData);
	return XUI_OK;
}

XUI_API const char* xuiCodeEditGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	return __xuiCodeEditMenuTitleForCommand(pWidget, pData, iCommand);
}

XUI_API int xuiCodeEditSetTextShapeCursorVisitor(xui_widget pWidget,
    xui_text_shape_cursor_visit_proc onVisit,xui_text_shape_cursor_free_proc onFree,void* pUser)
{
    xui_code_edit_data_t* pData=__xuiCodeEditGetData(pWidget);
    if (!pData || ((onVisit==NULL)!=(onFree==NULL))) return XUI_ERROR_INVALID_ARGUMENT;
    if (pData->onCursorVisit==onVisit && pData->onCursorFree==onFree && pData->pCursorVisitUser==pUser) return XUI_OK;
    __xuiCodeEditLineLayoutsClear(pData);__xuiCodeEditWrapIndexClear(pData);
    pData->onCursorVisit=onVisit;pData->onCursorFree=onFree;pData->pCursorVisitUser=pUser;
    return xuiWidgetInvalidate(pWidget,XUI_WIDGET_DIRTY_LAYOUT|XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditSetTextShapeVisitor(xui_widget pWidget,
    xui_text_shape_visit_proc onVisit, void* pUser)
{
    xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
    if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
    if ( pData->onShapeVisit == onVisit && pData->pShapeVisitUser == pUser ) return XUI_OK;
    __xuiCodeEditLineLayoutsClear(pData);
    __xuiCodeEditWrapIndexClear(pData);
    pData->onCursorVisit=NULL;pData->onCursorFree=NULL;pData->pCursorVisitUser=NULL;
    pData->onShapeVisit = onVisit;
    pData->pShapeVisitUser = pUser;
    return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	__xuiCodeEditLineLayoutsClear(pData);
	__xuiCodeEditWrapIndexClear(pData);
	if ( pData->pMenu != NULL ) (void)xuiMenuSetFont(pData->pMenu, pFont);
	if ( pData->pFindWindow != NULL ) {
		(void)xuiWindowSetFont(pData->pFindWindow, pFont);
		if ( pData->pFindInput != NULL ) (void)xuiInputSetFont(pData->pFindInput, pFont);
		if ( pData->pReplaceInput != NULL ) (void)xuiInputSetFont(pData->pReplaceInput, pFont);
		if ( pData->pFindPrevButton != NULL ) (void)xuiButtonSetFont(pData->pFindPrevButton, pFont);
		if ( pData->pFindNextButton != NULL ) (void)xuiButtonSetFont(pData->pFindNextButton, pFont);
		if ( pData->pFindAllButton != NULL ) (void)xuiButtonSetFont(pData->pFindAllButton, pFont);
		if ( pData->pReplaceButton != NULL ) (void)xuiButtonSetFont(pData->pReplaceButton, pFont);
		if ( pData->pReplaceAllButton != NULL ) (void)xuiButtonSetFont(pData->pReplaceAllButton, pFont);
		if ( pData->pCaseCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pCaseCheck, pFont);
		if ( pData->pWordCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pWordCheck, pFont);
		if ( pData->pRegexCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pRegexCheck, pFont);
		if ( pData->pEscapeCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pEscapeCheck, pFont);
		if ( pData->pSelectionCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pSelectionCheck, pFont);
		if ( pData->pScopeCheck != NULL ) (void)xuiCheckBoxSetFont(pData->pScopeCheck, pFont);
		if ( pData->pFindResultList != NULL ) (void)xuiTableViewSetFont(pData->pFindResultList, pFont);
		if ( pData->pFindStatus != NULL ) (void)xuiLabelSetFont(pData->pFindStatus, pFont);
	}
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiCodeEditGetFont(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

static int __xuiCodeEditClampOffset(xui_code_edit_data_t* pData, int iOffset)
{
	int iLength;

	if ( pData == NULL || pData->pDocument == NULL ) return 0;
	iLength = xuiCodeDocumentGetLength(pData->pDocument);
	if ( iOffset < 0 ) return 0;
	if ( iOffset > iLength ) return iLength;
	return iOffset;
}

static int __xuiCodeEditFindSelectionSnapshot(xui_code_edit_data_t* pData, uint32_t iFlags, int* pRangeStart, int* pRangeEnd)
{
	int iStart;
	int iEnd;
	int iLength;

	if ( pData == NULL || pRangeStart == NULL || pRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = xuiCodeDocumentGetLength(pData->pDocument);
	if ( (iFlags & XUI_FIND_SELECTION) != 0 ) {
		if ( pData->iFindRangeEnd > pData->iFindRangeStart ) {
			*pRangeStart = __xuiCodeEditClampOffset(pData, pData->iFindRangeStart);
			*pRangeEnd = __xuiCodeEditClampOffset(pData, pData->iFindRangeEnd);
			return (*pRangeEnd > *pRangeStart) ? XUI_OK : XUI_ERROR_UNSUPPORTED;
		}
		iStart = 0;
		iEnd = 0;
		if ( xuiCodeSelectionGetRange(pData->pSelection, &iStart, &iEnd) != XUI_OK || iEnd <= iStart ) {
			return XUI_ERROR_UNSUPPORTED;
		}
		*pRangeStart = iStart;
		*pRangeEnd = iEnd;
		return XUI_OK;
	}
	*pRangeStart = 0;
	*pRangeEnd = iLength;
	return XUI_OK;
}

static int __xuiCodeEditFindResolve(xui_code_edit_data_t* pData, const xui_find_options_t* pOptions, int bBackward, const char** psPattern, const char** psReplacement, uint32_t* pFlags, int* pStartOffset, int* pRangeStart, int* pRangeEnd)
{
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStart;
	int iEnd;
	int iSelStart;
	int iSelEnd;
	int iRet;

	if ( pData == NULL || psPattern == NULL || pFlags == NULL || pStartOffset == NULL || pRangeStart == NULL || pRangeEnd == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sPattern = (pOptions != NULL && pOptions->sPattern != NULL) ? pOptions->sPattern : pData->sFindPattern;
	sReplacement = (pOptions != NULL && pOptions->sReplacement != NULL) ? pOptions->sReplacement : pData->sFindReplacement;
	if ( sPattern == NULL || sPattern[0] == '\0' ) return XUI_ERROR_INVALID_ARGUMENT;
	iFlags = (pOptions != NULL) ? pOptions->iFlags : pData->iFindFlags;
	if ( bBackward ) iFlags |= XUI_FIND_BACKWARD;
	else iFlags &= ~XUI_FIND_BACKWARD;
	iRet = __xuiCodeEditFindSelectionSnapshot(pData, iFlags, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( pOptions != NULL && (iFlags & XUI_FIND_SELECTION) == 0 && pOptions->iRangeEnd > pOptions->iRangeStart ) {
		iStart = __xuiCodeEditClampOffset(pData, pOptions->iRangeStart);
		iEnd = __xuiCodeEditClampOffset(pData, pOptions->iRangeEnd);
		if ( iEnd < iStart ) {
			int iSwap = iStart;
			iStart = iEnd;
			iEnd = iSwap;
		}
	}
	iSelStart = 0;
	iSelEnd = 0;
	(void)xuiCodeSelectionGetRange(pData->pSelection, &iSelStart, &iSelEnd);
	*pStartOffset = bBackward ? ((iSelStart != iSelEnd) ? iSelStart : iSelEnd) : ((iSelStart != iSelEnd) ? iSelEnd : iSelStart);
	if ( pOptions != NULL && pOptions->iStartOffset > 0 ) {
		*pStartOffset = __xuiCodeEditClampOffset(pData, pOptions->iStartOffset);
	}
	*pStartOffset = __xuiCodeEditClampOffset(pData, *pStartOffset);
	if ( *pStartOffset < iStart ) *pStartOffset = iStart;
	if ( *pStartOffset > iEnd ) *pStartOffset = iEnd;
	*psPattern = sPattern;
	if ( psReplacement != NULL ) *psReplacement = (sReplacement != NULL) ? sReplacement : "";
	*pFlags = iFlags;
	*pRangeStart = iStart;
	*pRangeEnd = iEnd;
	return XUI_OK;
}

static int __xuiCodeEditStoreFindState(xui_code_edit_data_t* pData, const char* sPattern, const char* sReplacement, uint32_t iFlags, int iRangeStart, int iRangeEnd)
{
	int iRet;

	if ( pData == NULL || sPattern == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditStringSet(&pData->sFindPattern, &pData->iFindPatternCapacity, sPattern);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditStringSet(&pData->sFindReplacement, &pData->iFindReplacementCapacity, (sReplacement != NULL) ? sReplacement : "");
	if ( iRet != XUI_OK ) return iRet;
	pData->iFindFlags = iFlags;
	pData->iFindRangeStart = ((iFlags & XUI_FIND_SELECTION) != 0) ? iRangeStart : 0;
	pData->iFindRangeEnd = ((iFlags & XUI_FIND_SELECTION) != 0) ? iRangeEnd : 0;
	return XUI_OK;
}

static void __xuiCodeEditBuildFindPreview(xui_code_document pDocument, int iOffset, char* sPreview, int iCapacity)
{
	char ch;
	int iLine;
	int iStart;
	int iEnd;
	int i;
	int iOut;

	if ( sPreview == NULL || iCapacity <= 0 ) return;
	sPreview[0] = '\0';
	if ( pDocument == NULL ) return;
	iLine = 0;
	if ( xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, &iLine, NULL) != XUI_OK ) return;
	iStart = 0;
	iEnd = 0;
	if ( xuiCodeDocumentGetLineRange(pDocument, iLine, &iStart, &iEnd) != XUI_OK ) return;
	while ( iStart < iEnd ) {
		if ( xuiCodeDocumentGetByte(pDocument, iStart, &ch) != XUI_OK ||
		     (ch != ' ' && ch != '\t') ) break;
		iStart++;
	}
	iOut = 0;
	for ( i = iStart; i < iEnd && iOut + 1 < iCapacity; i++ ) {
		if ( xuiCodeDocumentGetByte(pDocument, i, &ch) != XUI_OK ||
		     ch == '\r' || ch == '\n' ) break;
		sPreview[iOut++] = (ch == '\t') ? ' ' : ch;
	}
	sPreview[iOut] = '\0';
}

static void __xuiCodeEditFillFindResult(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_find_result_t* pFind, xui_code_find_result_t* pResult)
{
	if ( pResult == NULL ) return;
	memset(pResult, 0, sizeof(*pResult));
	pResult->iSize = sizeof(*pResult);
	pResult->pEditor = pWidget;
	if ( pFind == NULL || pData == NULL ) return;
	pResult->iStart = pFind->iStart;
	pResult->iEnd = pFind->iEnd;
	pResult->iLine = 0;
	pResult->iColumn = 0;
	(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, pFind->iStart, &pResult->iLine, &pResult->iColumn);
	__xuiCodeEditBuildFindPreview(pData->pDocument, pFind->iStart, pResult->sPreview, (int)sizeof(pResult->sPreview));
}

static void __xuiCodeEditFormatFindPosition(int iLine, int iColumn, char* sBuffer, int iCapacity)
{
	if ( sBuffer == NULL || iCapacity <= 0 ) return;
	snprintf(sBuffer, (size_t)iCapacity, "%d:%d", iLine + 1, iColumn + 1);
}

static int __xuiCodeEditFindResultTableColumns(xui_code_edit_data_t* pData, int bScope)
{
	xui_table_view_column_t arrColumns[3];
	xui_context pContext;
	int iCount;
	int i;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bFindResultScopeMode = bScope ? 1 : 0;
	if ( pData->pFindResultList == NULL ) return XUI_OK;
	pContext = xuiWidgetGetContext(pData->pFindResultList);
	memset(arrColumns, 0, sizeof(arrColumns));
	if ( bScope ) {
		arrColumns[0].sTitle = xuiTranslate(pContext, XUI_TR_FIND_COL_FILE);
		arrColumns[0].fWidth = 150.0f;
		arrColumns[0].fMinWidth = 80.0f;
		arrColumns[1].sTitle = xuiTranslate(pContext, XUI_TR_FIND_COL_POSITION);
		arrColumns[1].fWidth = 84.0f;
		arrColumns[1].fMinWidth = 64.0f;
		arrColumns[2].sTitle = xuiTranslate(pContext, XUI_TR_FIND_COL_CONTENT);
		arrColumns[2].fWidth = 340.0f;
		arrColumns[2].fMinWidth = 120.0f;
		iCount = 3;
	} else {
		arrColumns[0].sTitle = xuiTranslate(pContext, XUI_TR_FIND_COL_POSITION);
		arrColumns[0].fWidth = 84.0f;
		arrColumns[0].fMinWidth = 64.0f;
		arrColumns[1].sTitle = xuiTranslate(pContext, XUI_TR_FIND_COL_CONTENT);
		arrColumns[1].fWidth = 492.0f;
		arrColumns[1].fMinWidth = 160.0f;
		iCount = 2;
	}
	for ( i = 0; i < iCount; i++ ) {
		arrColumns[i].bVisibleSet = 1;
		arrColumns[i].bVisible = 1;
		arrColumns[i].bResizableSet = 1;
		arrColumns[i].bResizable = 1;
	}
	return xuiTableViewSetColumns(pData->pFindResultList, arrColumns, iCount);
}

static int __xuiCodeEditFindResultTableCount(xui_widget pWidget, void* pUser)
{
	xui_widget pCodeEdit;
	xui_code_edit_data_t* pData;

	(void)pWidget;
	pCodeEdit = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL ) return 0;
	if ( pData->bFindResultScopeMode && pData->pFindScope != NULL ) {
		return pData->pFindScope->iResultCount;
	}
	return pData->iFindResultCount;
}

static int __xuiCodeEditFindResultTableCell(xui_widget pWidget, int iRow, int iColumn, xui_table_view_cell_t* pCell, void* pUser)
{
	xui_widget pCodeEdit;
	xui_code_edit_data_t* pData;
	xui_find_result_t* pFind;
	xui_code_find_result_t* pScopeResult;
	int iLine;
	int iColumnIndex;

	(void)pWidget;
	pCodeEdit = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL || pCell == NULL || iRow < 0 || iColumn < 0 ) return 0;
	if ( pData->bFindResultScopeMode ) {
		if ( pData->pFindScope == NULL || iRow >= pData->pFindScope->iResultCount ) return 0;
		pScopeResult = &pData->pFindScope->pResults[iRow];
		if ( iColumn == 0 ) {
			pCell->sText = __xuiCodeFindScopeGetEditorNameByWidget(pData->pFindScope, pScopeResult->pEditor);
			return 1;
		}
		if ( iColumn == 1 ) {
			__xuiCodeEditFormatFindPosition(pScopeResult->iLine, pScopeResult->iColumn, pData->sFindResultPosition, (int)sizeof(pData->sFindResultPosition));
			pCell->sText = pData->sFindResultPosition;
			return 1;
		}
		if ( iColumn == 2 ) {
			pCell->sText = pScopeResult->sPreview;
			return 1;
		}
		return 0;
	}
	if ( iRow >= pData->iFindResultCount ) return 0;
	pFind = &pData->pFindResults[iRow];
	if ( iColumn == 0 ) {
		iLine = 0;
		iColumnIndex = 0;
		(void)xuiCodeDocumentOffsetToLineColumn(pData->pDocument, pFind->iStart, &iLine, &iColumnIndex);
		__xuiCodeEditFormatFindPosition(iLine, iColumnIndex, pData->sFindResultPosition, (int)sizeof(pData->sFindResultPosition));
		pCell->sText = pData->sFindResultPosition;
		return 1;
	}
	if ( iColumn == 1 ) {
		__xuiCodeEditBuildFindPreview(pData->pDocument, pFind->iStart, pData->sFindResultPreview, (int)sizeof(pData->sFindResultPreview));
		pCell->sText = pData->sFindResultPreview;
		return 1;
	}
	return 0;
}

static int __xuiCodeEditRefreshFindResultTable(xui_code_edit_data_t* pData, int bScope, int iActiveIndex)
{
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditFindResultTableColumns(pData, bScope);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->pFindResultList == NULL ) return XUI_OK;
	iRet = xuiTableViewRefreshAdapter(pData->pFindResultList);
	if ( iRet != XUI_OK ) return iRet;
	if ( iActiveIndex >= 0 ) {
		iRet = xuiTableViewSetSelectedRow(pData->pFindResultList, iActiveIndex);
		if ( iRet == XUI_OK ) iRet = xuiTableViewEnsureCellVisible(pData->pFindResultList, iActiveIndex, 0);
	}
	return iRet;
}

static int __xuiCodeEditSyncFindResultList(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	(void)pWidget;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditRefreshFindResultTable(pData, 0, pData->iFindActiveIndex);
}

static int __xuiCodeEditCopyDocumentRange(xui_code_edit_data_t* pData,
	int iStart, int iEnd, char** psText, int* pLength)
{
	char* sText;
	int iLength;
	int iRet;

	if ( pData == NULL || pData->pDocument == NULL || psText == NULL ||
	     iStart < 0 || iEnd < iStart ||
	     iEnd > xuiCodeDocumentGetLength(pData->pDocument) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*psText = NULL;
	iLength = iEnd - iStart;
	sText = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiCodeDocumentCopyRange(pData->pDocument, iStart, iEnd,
		sText, iLength + 1, NULL);
	if ( iRet != XUI_OK ) {
		xrtFree(sText);
		return iRet;
	}
	*psText = sText;
	if ( pLength != NULL ) *pLength = iLength;
	return XUI_OK;
}

static int __xuiCodeEditUpdateFindResults(xui_widget pWidget, xui_code_edit_data_t* pData, const char* sPattern, uint32_t iFlags, int iRangeStart, int iRangeEnd, const xui_find_result_t* pActive)
{
	char* sText;
	uint32_t iCollectFlags;
	int iTextLength;
	int iCount;
	int i;
	int iRet;

	if ( pWidget == NULL || pData == NULL || sPattern == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sText = NULL;
	iRet = __xuiCodeEditCopyDocumentRange(pData, iRangeStart, iRangeEnd,
		&sText, &iTextLength);
	if ( iRet != XUI_OK ) return iRet;
	iCollectFlags = iFlags & ~(XUI_FIND_BACKWARD | XUI_FIND_WRAP);
	iCount = 0;
	iRet = xuiFindCollectText(sText, iTextLength, sPattern, 0, iTextLength,
		iCollectFlags, NULL, 0, &iCount, pData->sError, (int)sizeof(pData->sError));
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiCodeEditFindResultReserve(pData, iCount);
	if ( iRet != XUI_OK ) goto cleanup;
	pData->iFindResultCount = 0;
	pData->iFindActiveIndex = -1;
	(void)xuiCodeAnnotationClearIndicators(pData->pAnnotations, XUI_CODE_INDICATOR_SEARCH_RESULT);
	if ( iCount > 0 ) {
		iRet = xuiFindCollectText(sText, iTextLength, sPattern, 0, iTextLength,
			iCollectFlags, pData->pFindResults, iCount,
			&pData->iFindResultCount, pData->sError, (int)sizeof(pData->sError));
		if ( iRet != XUI_OK ) goto cleanup;
		for ( i = 0; i < pData->iFindResultCount; i++ ) {
			pData->pFindResults[i].iStart += iRangeStart;
			pData->pFindResults[i].iEnd += iRangeStart;
		}
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
	for ( i = 0; i < pData->iFindResultCount; i++ ) {
		(void)xuiCodeAnnotationSetIndicator(pData->pAnnotations, XUI_CODE_INDICATOR_SEARCH_RESULT, XUI_CODE_INDICATOR_BACKGROUND, pData->pFindResults[i].iStart, pData->pFindResults[i].iEnd, 0u, 0u);
	}
	iRet = __xuiCodeEditSyncFindResultList(pWidget, pData);
	if ( iRet == XUI_OK ) {
		iRet = xuiWidgetInvalidate(pWidget,
			XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}

cleanup:
	if ( iRet != XUI_OK ) {
		__xuiCodeEditClearFindResults(pData);
		(void)__xuiCodeEditSyncFindResultList(pWidget, pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	xrtFree(sText);
	return iRet;
}

static int __xuiCodeEditApplyFindResult(xui_widget pWidget, xui_code_edit_data_t* pData, const xui_find_result_t* pResult)
{
	int iRet;

	if ( pWidget == NULL || pData == NULL || pResult == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, pResult->iStart, pResult->iEnd);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	iRet = xuiCodeEditEnsureCaretVisible(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCodeEditFindMove(xui_widget pWidget, const xui_find_options_t* pOptions, int bBackward)
{
	xui_code_edit_data_t* pData;
	xui_find_result_t tResult;
	char* sRangeText;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iRangeLength;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sRangeText = NULL;
	iRet = __xuiCodeEditFindResolve(pData, pOptions, bBackward, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditCopyDocumentRange(pData, iRangeStart, iRangeEnd,
		&sRangeText, &iRangeLength);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tResult, 0, sizeof(tResult));
	iRet = xuiFindText(sRangeText, iRangeLength, sPattern,
		iStartOffset - iRangeStart, 0, iRangeLength, iFlags | XUI_FIND_WRAP,
		&tResult, pData->sError, (int)sizeof(pData->sError));
	xrtFree(sRangeText);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditClearFindResults(pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return iRet;
	}
	tResult.iStart += iRangeStart;
	tResult.iEnd += iRangeStart;
	iRet = __xuiCodeEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditUpdateFindResults(pWidget, pData, sPattern, iFlags, iRangeStart, iRangeEnd, &tResult);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditApplyFindResult(pWidget, pData, &tResult);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit find failed");
	return iRet;
}

XUI_API int xuiCodeEditSetFindScope(xui_widget pWidget, xui_code_find_scope pScope)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFindScope = pScope;
	return XUI_OK;
}

XUI_API xui_code_find_scope xuiCodeEditGetFindScope(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pFindScope : NULL;
}

XUI_API int xuiCodeEditSetText(xui_widget pWidget, const char* sText)
{
	return xuiCodeEditSetTextLength(pWidget, sText, -1);
}

XUI_API int xuiCodeEditSetTextLength(xui_widget pWidget, const char* sText, int iLength)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeDocumentSetTextLength(pData->pDocument, sText, iLength);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : xuiCodeDocumentGetLastError(pData->pDocument));
	if ( iRet == XUI_OK ) {
		iRet = __xuiCodeEditAfterDocumentReplace(pWidget, pData);
		if ( iRet != XUI_OK ) __xuiCodeEditSetError(pData, "CodeEdit document state reset failed");
	}
	return iRet;
}

XUI_API int xuiCodeEditLoadTextFile(xui_widget pWidget, const char* sPath, int iCharset)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeDocumentLoadTextFile(pData->pDocument, sPath, iCharset);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : xuiCodeDocumentGetLastError(pData->pDocument));
	if ( iRet == XUI_OK ) {
		iRet = __xuiCodeEditAfterDocumentReplace(pWidget, pData);
		if ( iRet != XUI_OK ) __xuiCodeEditSetError(pData, "CodeEdit document state reset failed");
	}
	return iRet;
}

XUI_API const char* xuiCodeEditGetText(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? xuiCodeDocumentGetText(pData->pDocument) : "";
}

XUI_API int xuiCodeEditSetInputHandler(xui_widget pWidget, xui_code_input_proc onInput, void* pUser)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bInputDispatch ) return XUI_ERROR_UNSUPPORTED;
	pData->onInput = onInput;
	pData->pInputUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeEditApplyTextEdits(xui_widget pWidget,
	const xui_code_text_edit_t* pEdits, int iEditCount,
	int iSelectionAnchor, int iSelectionCaret)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	int iRet;
	int iEndRet;

	if ( pData == NULL || pData->bReadonly || pData->bInputDispatch ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiCodeDocumentBeginEdit(pData->pDocument);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditApplyTextEditsRaw(pData, pEdits, iEditCount,
		iSelectionAnchor >= 0 && iSelectionCaret >= 0,
		iSelectionAnchor, iSelectionCaret);
	iEndRet = xuiCodeDocumentEndEdit(pData->pDocument);
	if ( iRet == XUI_OK ) iRet = iEndRet;
	if ( iRet == XUI_OK ) {
		(void)xuiCodeEditEnsureCaretVisible(pWidget);
		(void)xuiWidgetInvalidate(pWidget,
			XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

XUI_API int xuiCodeEditSetInlineCompletion(xui_widget pWidget, int iOffset, const char* sText)
{
	xui_code_edit_data_t* pData;
	xui_code_selection_t tSelection;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText[0] == '\0' ) return xuiCodeEditClearInlineCompletion(pWidget);
	if ( iOffset < 0 || iOffset > xuiCodeDocumentGetLength(pData->pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiCodeSelectionGetCount(pData->pSelection) != 1 ) return XUI_ERROR_UNSUPPORTED;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ||
	     tSelection.iAnchorOffset != iOffset || tSelection.iCaretOffset != iOffset ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeEditStringSet(&pData->sInlineCompletion, &pData->iInlineCompletionCapacity, sText);
	if ( iRet != XUI_OK ) return iRet;
	pData->iInlineCompletionOffset = iOffset;
	__xuiCodeEditInlineCompletionRefreshLayout(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditClearInlineCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;
	int bChanged;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bChanged = __xuiCodeEditInlineCompletionClearData(pData);
	return bChanged ? xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER) : XUI_OK;
}

XUI_API int xuiCodeEditHasInlineCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return __xuiCodeEditInlineCompletionValid(pData);
}

XUI_API const char* xuiCodeEditGetInlineCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return __xuiCodeEditInlineCompletionValid(pData) ? pData->sInlineCompletion : "";
}

XUI_API int xuiCodeEditAcceptInlineCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;
	char* sCompletion;
	int iRet;
	int iEndRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly || !__xuiCodeEditInlineCompletionValid(pData) ) return XUI_ERROR_UNSUPPORTED;
	sCompletion = pData->sInlineCompletion;
	pData->sInlineCompletion = NULL;
	pData->iInlineCompletionCapacity = 0;
	pData->iInlineCompletionOffset = 0;
	pData->iInlineCompletionLine = 0;
	pData->iInlineCompletionExtraRows = 0;
	pData->iInlineCompletionVersion = 0u;
	iRet = xuiCodeDocumentBeginEdit(pData->pDocument);
	if ( iRet == XUI_OK ) {
		iRet = xuiCodeEditingInsertText(pData->pDocument, pData->pSelection, sCompletion, pData->bReadonly);
		iEndRet = xuiCodeDocumentEndEdit(pData->pDocument);
		if ( iRet == XUI_OK ) iRet = iEndRet;
	}
	xrtFree(sCompletion);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit inline completion insert failed");
	if ( iRet == XUI_OK ) (void)xuiCodeEditEnsureCaretVisible(pWidget);
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return iRet;
}

XUI_API int xuiCodeEditSetVirtualText(xui_widget pWidget, const xui_code_virtual_text_t* pVirtualText)
{
	if ( pVirtualText == NULL ||
	     (pVirtualText->iSize != 0 && pVirtualText->iSize < sizeof(*pVirtualText)) ||
	     pVirtualText->iKind != XUI_CODE_VIRTUAL_TEXT_INLINE_COMPLETION || pVirtualText->sText == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiCodeEditSetInlineCompletion(pWidget, pVirtualText->iOffset, pVirtualText->sText);
}

XUI_API int xuiCodeEditGetVirtualText(xui_widget pWidget, xui_code_virtual_text_t* pVirtualText)
{
	xui_code_edit_data_t* pData;

	if ( pVirtualText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pVirtualText, 0, sizeof(*pVirtualText));
	pVirtualText->iSize = sizeof(*pVirtualText);
	if ( !__xuiCodeEditInlineCompletionValid(pData) ) return XUI_ERROR_UNSUPPORTED;
	pVirtualText->iKind = XUI_CODE_VIRTUAL_TEXT_INLINE_COMPLETION;
	pVirtualText->iOffset = pData->iInlineCompletionOffset;
	pVirtualText->sText = pData->sInlineCompletion;
	return XUI_OK;
}

XUI_API int xuiCodeEditClearVirtualText(xui_widget pWidget, int iKind)
{
	if ( iKind != 0 && iKind != XUI_CODE_VIRTUAL_TEXT_INLINE_COMPLETION ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiCodeEditClearInlineCompletion(pWidget);
}

XUI_API int xuiCodeEditShowCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditShowCompletionInternal(pWidget, pData, 1);
}

XUI_API int xuiCodeEditCancelCompletion(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditCancelCompletionInternal(pData);
}

XUI_API int xuiCodeEditIsCompletionOpen(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return pData != NULL && pData->pCompletionPopup != NULL &&
		xuiPopupIsOpen(pData->pCompletionPopup) && pData->iCompletionCount > 0;
}

XUI_API int xuiCodeEditGetCompletionCount(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->iCompletionCount : 0;
}

XUI_API int xuiCodeEditGetCompletionSelected(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->iCompletionSelected : -1;
}

XUI_API xui_widget xuiCodeEditGetCompletionPopupWidget(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pCompletionPopup : NULL;
}

XUI_API int xuiCodeEditApplyCompletionItems(xui_widget pWidget, uint32_t iDocumentVersion,
	int iOffset, const xui_code_completion_item_t* pItems, int iItemCount)
{
	xui_code_edit_data_t* pData;
	char sPrefix[256];
	int iStart;
	int iCaret;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || iOffset < 0 || iItemCount < 0 ||
	     (iItemCount > 0 && pItems == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iDocumentVersion != xuiCodeDocumentGetVersion(pData->pDocument) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	iRet = __xuiCodeEditCompletionPrefix(
		pData, &iStart, &iCaret, sPrefix, (int)sizeof(sPrefix));
	if ( iRet != XUI_OK || iCaret != iOffset ) return XUI_ERROR_UNSUPPORTED;
	if ( iItemCount == 0 ) {
		(void)__xuiCodeEditCancelCompletionInternal(pData);
		return XUI_ERROR_UNSUPPORTED;
	}
	return __xuiCodeEditOpenCompletionItems(
		pWidget, pData, iStart, iCaret, sPrefix, pItems, iItemCount);
}

XUI_API int xuiCodeEditSetCompletionOptions(xui_widget pWidget, int bAutoShow,
	int iMinPrefix, int iMaxItems, float fDelay)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || iMinPrefix < 0 || iMaxItems <= 0 || iMaxItems > 16384 || fDelay < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->bCompletionAutoShow = bAutoShow ? 1 : 0;
	pData->iCompletionMinPrefix = iMinPrefix;
	pData->iCompletionMaxItems = iMaxItems;
	pData->fCompletionDelay = fDelay;
	if ( !pData->bCompletionAutoShow ) {
		pData->bCompletionPending = 0;
		pData->fCompletionElapsed = 0.0f;
	}
	return XUI_OK;
}

XUI_API int xuiCodeEditGetCompletionOptions(xui_widget pWidget, int* pAutoShow,
	int* pMinPrefix, int* pMaxItems, float* pDelay)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pAutoShow != NULL ) *pAutoShow = pData->bCompletionAutoShow;
	if ( pMinPrefix != NULL ) *pMinPrefix = pData->iCompletionMinPrefix;
	if ( pMaxItems != NULL ) *pMaxItems = pData->iCompletionMaxItems;
	if ( pDelay != NULL ) *pDelay = pData->fCompletionDelay;
	return XUI_OK;
}

static int __xuiCodeEditCloseAssistInternal(xui_code_edit_data_t* pData)
{
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pAssistPopup != NULL && xuiPopupIsOpen(pData->pAssistPopup) ) {
		(void)xuiPopupSetOpen(pData->pAssistPopup, 0);
	}
	pData->iAssistKind = XUI_CODE_EDIT_ASSIST_NONE;
	pData->tAssistActiveRange = (xui_code_range_t){0, 0};
	if ( pData->sAssistLabel != NULL ) pData->sAssistLabel[0] = '\0';
	if ( pData->sAssistDocumentation != NULL ) pData->sAssistDocumentation[0] = '\0';
	return XUI_OK;
}

static void __xuiCodeEditShowCompletionDocumentation(xui_code_edit_data_t* pData,
	const xui_code_completion_owned_item_t* pItem)
{
	xui_rect_t tCompletion;
	xui_rect_t tAnchor;
	const char* sTitle;
	int iRet;

	if ( pData == NULL || pItem == NULL || pData->pAssistPopup == NULL ||
	     pData->pCompletionPopup == NULL ) return;
	if ( (pItem->sDetail == NULL || pItem->sDetail[0] == '\0') &&
	     (pItem->sDocumentation == NULL || pItem->sDocumentation[0] == '\0') ) {
		if ( pData->iAssistKind == XUI_CODE_EDIT_ASSIST_COMPLETION_DOCUMENTATION ) {
			(void)__xuiCodeEditCloseAssistInternal(pData);
		}
		return;
	}
	sTitle = (pItem->sDetail != NULL && pItem->sDetail[0] != '\0') ?
		pItem->sDetail : pItem->sLabel;
	iRet = __xuiCodeEditStringSet(&pData->sAssistLabel,
		&pData->iAssistLabelCapacity, sTitle);
	if ( iRet != XUI_OK ) return;
	iRet = __xuiCodeEditStringSet(&pData->sAssistDocumentation,
		&pData->iAssistDocumentationCapacity,
		(pItem->sDocumentation != NULL) ? pItem->sDocumentation : "");
	if ( iRet != XUI_OK ) return;
	tCompletion = xuiPopupGetPopupRect(pData->pCompletionPopup);
	tAnchor = (xui_rect_t){tCompletion.fX + tCompletion.fW,
		tCompletion.fY, 1.0f, tCompletion.fH};
	pData->iAssistKind = XUI_CODE_EDIT_ASSIST_COMPLETION_DOCUMENTATION;
	pData->tAssistActiveRange = (xui_code_range_t){0, 0};
	(void)xuiPopupSetContentSize(pData->pAssistPopup, 420.0f,
		(pData->sAssistDocumentation[0] != '\0') ? 64.0f : 36.0f);
	(void)xuiPopupSetAnchorRect(pData->pAssistPopup, tAnchor);
	(void)xuiWidgetInvalidate(pData->pAssistContent,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	(void)xuiPopupSetOpen(pData->pAssistPopup, 1);
}

XUI_API int xuiCodeEditGetOffsetRect(xui_widget pWidget, int iOffset, xui_rect_t* pRect)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	xui_rect_t tWorld;
	const char* sText;
	float fMarginWidth;
	float fColumnWidth;
	float fLineHeight;
	double fOffsetX;
	double fWrapOffsetX;
	int iLine;
	int iColumn;
	int iStart;
	int iEnd;
	int iColumnOffset;
	int iVisualRow;
	int iRet;

	if ( pData == NULL || pRect == NULL || iOffset < 0 ||
	     iOffset > xuiCodeDocumentGetLength(pData->pDocument) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentOffsetToLineColumn(pData->pDocument, iOffset, &iLine, &iColumn);
	if ( iRet != XUI_OK ) return iRet;
	iStart = 0;
	iEnd = 0;
	iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditReadRange(pData, iStart, iEnd, &sText);
	if ( iRet != XUI_OK ) return iRet;
	iColumnOffset = iStart;
	(void)xuiCodeDocumentLineColumnToOffset(pData->pDocument, iLine, iColumn, &iColumnOffset);
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	iVisualRow = __xuiCodeEditLineToVisibleRow(pData, iLine);
	fWrapOffsetX = -1.0f;
	if ( pData->bWordWrap ) {
		iRet = __xuiCodeEditWrapOffsetGeometry(pWidget, pData, iOffset,
			&iVisualRow, NULL, NULL, &fWrapOffsetX);
		if ( iRet != XUI_OK ) return iRet;
	}
	fOffsetX = (fWrapOffsetX >= 0.0f) ? fWrapOffsetX :
		__xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
			iColumnOffset - iStart, fColumnWidth);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	*pRect = (xui_rect_t){
		tWorld.fX + fMarginWidth + 4.0f + fOffsetX - pData->fScrollX,
		tWorld.fY + 4.0f + (float)iVisualRow * fLineHeight - pData->fScrollY,
		2.0f, fLineHeight
	};
	return XUI_OK;
}

XUI_API int xuiCodeEditGetTextOffsetRect(xui_widget pWidget, int iByteOffset,
	xui_rect_t* pRect)
{
	return xuiCodeEditGetOffsetRect(pWidget, iByteOffset, pRect);
}

XUI_API int xuiCodeEditHitTestText(xui_widget pWidget, float fContextX,
	float fContextY, xui_code_edit_text_hit_t* pHit)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_edit_line_layout_t* pLayout;
	xui_rect_t tWorld;
	xui_rect_t tViewport;
	xui_rect_t tCaret;
	xui_rect_t tClusterEnd;
	xui_context pContext;
	xui_proxy pProxy;
	xui_font pFont;
	const char* sText;
	float fMarginWidth;
	float fColumnWidth;
	float fLineHeight;
	double fSegmentStartX;
	double fSegmentEndX;
	int iOffset;
	int iLine;
	int iLineStart;
	int iLineEnd;
	int iSegmentStart;
	int iSegmentEnd;
	int iVisualRow;
	int iClusterStart;
	int iClusterEnd;
	int iAt;
	int iColumn;
	int iRet;

	if ( pData == NULL || pHit == NULL || (pHit->iSize != 0u &&
		pHit->iSize < sizeof(*pHit)) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pHit, 0, sizeof(*pHit));
	pHit->iSize = sizeof(*pHit);
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tViewport = (xui_rect_t){tWorld.fX + pData->tScrollViewportRect.fX,
		tWorld.fY + pData->tScrollViewportRect.fY,
		pData->tScrollViewportRect.fW, pData->tScrollViewportRect.fH};
	iRet = __xuiCodeEditHitOffsetEx(pWidget, pData, fContextX, fContextY,
		1, &iOffset);
	if ( iRet != XUI_OK ) return iRet;
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	iVisualRow = (int)((fContextY - tViewport.fY - 4.0f +
		pData->fScrollY) / fLineHeight);
	if ( iVisualRow < 0 ) iVisualRow = 0;
	if ( iVisualRow >= __xuiCodeEditVisibleLineCount(pData) ) {
		iVisualRow = __xuiCodeEditVisibleLineCount(pData) - 1;
	}
	iRet = __xuiCodeEditWrapVisualSegment(pWidget, pData, iVisualRow,
		&iLine, NULL, &iSegmentStart, &iSegmentEnd, &fSegmentStartX);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentGetLineRange(pData->pDocument, iLine,
		&iLineStart, &iLineEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditReadRange(pData, iLineStart, iLineEnd, &sText);
	if ( iRet != XUI_OK ) return iRet;
	if ( iOffset < iSegmentStart ) iOffset = iSegmentStart;
	if ( iOffset > iSegmentEnd ) iOffset = iSegmentEnd;
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = __xuiCodeEditResolveFont(pWidget, pData);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	pLayout = __xuiCodeEditLineLayoutEnsure(pProxy, pFont, pData, sText,
		iLineEnd - iLineStart, fColumnWidth);
	if ( pLayout == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	fSegmentEndX = __xuiCodeEditLineLayoutOffsetX(pLayout,
		iSegmentEnd - iLineStart);
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	pHit->tLineRect = (xui_rect_t){tViewport.fX + fMarginWidth + 4.0f,
		tViewport.fY + 4.0f + (float)iVisualRow * fLineHeight -
			pData->fScrollY,
		fSegmentEndX - fSegmentStartX, fLineHeight};
	if ( pHit->tLineRect.fW < 0.0f ) pHit->tLineRect.fW = 0.0f;
	pHit->bInsideText = fContextX >= pHit->tLineRect.fX &&
		fContextX <= pHit->tLineRect.fX + pHit->tLineRect.fW &&
		fContextY >= pHit->tLineRect.fY &&
		fContextY < pHit->tLineRect.fY + pHit->tLineRect.fH &&
		fContextX >= tViewport.fX && fContextX < tViewport.fX + tViewport.fW &&
		fContextY >= tViewport.fY && fContextY < tViewport.fY + tViewport.fH;
	iRet = xuiCodeEditGetOffsetRect(pWidget, iOffset, &tCaret);
	if ( iRet != XUI_OK ) return iRet;
	if ( iOffset > iSegmentStart && fContextX < tCaret.fX ) {
		iClusterStart = xuiInternalTextGraphemePrev(sText,
			iLineEnd - iLineStart, iOffset - iLineStart) + iLineStart;
		iClusterEnd = iOffset;
		pHit->bTrailing = 1;
	} else {
		iClusterStart = iOffset;
		iClusterEnd = (iOffset < iSegmentEnd) ?
			xuiInternalTextGraphemeNext(sText, iLineEnd - iLineStart,
				iOffset - iLineStart) + iLineStart : iOffset;
	}
	if ( iClusterStart < iSegmentStart ) iClusterStart = iSegmentStart;
	if ( iClusterEnd > iSegmentEnd ) iClusterEnd = iSegmentEnd;
	iRet = xuiCodeEditGetOffsetRect(pWidget, iClusterStart,
		&pHit->tCharacterRect);
	if ( iRet != XUI_OK ) return iRet;
	if ( iClusterEnd > iClusterStart &&
		xuiCodeEditGetOffsetRect(pWidget, iClusterEnd, &tClusterEnd) == XUI_OK &&
		tClusterEnd.fY == pHit->tCharacterRect.fY ) {
		pHit->tCharacterRect.fW = tClusterEnd.fX - pHit->tCharacterRect.fX;
	}
	if ( pHit->tCharacterRect.fW < 1.0f ) pHit->tCharacterRect.fW = 1.0f;
	iColumn = 0;
	for ( iAt = iSegmentStart - iLineStart;
		iAt < iOffset - iLineStart; iColumn++ ) {
		int iNext = xuiInternalTextGraphemeNext(sText,
			iLineEnd - iLineStart, iAt);
		if ( iNext <= iAt ) break;
		iAt = iNext;
	}
	pHit->iByteOffset = iOffset;
	pHit->iLine = iLine;
	pHit->iVisualLine = iVisualRow;
	pHit->iVisualColumn = iColumn;
	pHit->iClusterStart = iClusterStart;
	pHit->iClusterEnd = iClusterEnd;
	return XUI_OK;
}

XUI_API int xuiCodeEditShowSignatureHelp(xui_widget pWidget,
	const xui_code_signature_help_t* pHelp)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_selection_t tSelection;
	xui_rect_t tAnchor;
	const xui_code_signature_parameter_t* pParameter;
	const char* sFound;
	int iRet;

	if ( pData == NULL || pHelp == NULL || pHelp->sLabel == NULL ||
	     pHelp->iParameterCount < 0 ||
	     (pHelp->iParameterCount > 0 && pHelp->pParameters == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiCodeEditStringSet(&pData->sAssistLabel,
		&pData->iAssistLabelCapacity, pHelp->sLabel);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditStringSet(&pData->sAssistDocumentation,
		&pData->iAssistDocumentationCapacity,
		(pHelp->sDocumentation != NULL) ? pHelp->sDocumentation : "");
	if ( iRet != XUI_OK ) return iRet;
	pData->tAssistActiveRange = (xui_code_range_t){0, 0};
	if ( pHelp->iActiveParameter >= 0 && pHelp->iActiveParameter < pHelp->iParameterCount ) {
		pParameter = &pHelp->pParameters[pHelp->iActiveParameter];
		pData->tAssistActiveRange = pParameter->tLabelRange;
		if ( pData->tAssistActiveRange.iEnd <= pData->tAssistActiveRange.iStart &&
		     pParameter->sLabel != NULL && pParameter->sLabel[0] != '\0' ) {
			sFound = strstr(pHelp->sLabel, pParameter->sLabel);
			if ( sFound != NULL ) {
				pData->tAssistActiveRange.iStart = (int)(sFound - pHelp->sLabel);
				pData->tAssistActiveRange.iEnd = pData->tAssistActiveRange.iStart + (int)strlen(pParameter->sLabel);
			}
		}
	}
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiCodeEditGetOffsetRect(pWidget, tSelection.iCaretOffset, &tAnchor);
	if ( iRet != XUI_OK ) return iRet;
	pData->iAssistKind = XUI_CODE_EDIT_ASSIST_SIGNATURE;
	(void)xuiPopupSetContentSize(pData->pAssistPopup, 480.0f,
		(pData->sAssistDocumentation[0] != '\0') ? 64.0f : 36.0f);
	(void)xuiPopupSetAnchorRect(pData->pAssistPopup, tAnchor);
	(void)xuiWidgetInvalidate(pData->pAssistContent,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return xuiPopupSetOpen(pData->pAssistPopup, 1);
}

static int __xuiCodeEditRequestSignatureInternal(xui_widget pWidget, xui_code_edit_data_t* pData)
{
	xui_code_signature_help_t tHelp;
	xui_code_selection_t tSelection;
	int iRet;

	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tSelection, 0, sizeof(tSelection));
	if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
	memset(&tHelp, 0, sizeof(tHelp));
	iRet = xuiCodeProviderRequestSignature(pData->pProviders, pWidget,
		tSelection.iCaretOffset, &tHelp);
	if ( iRet != XUI_OK ) return iRet;
	return xuiCodeEditShowSignatureHelp(pWidget, &tHelp);
}

XUI_API int xuiCodeEditRequestSignatureHelp(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? __xuiCodeEditRequestSignatureInternal(pWidget, pData) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCodeEditCloseSignatureHelp(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iAssistKind != XUI_CODE_EDIT_ASSIST_SIGNATURE ) return XUI_OK;
	return __xuiCodeEditCloseAssistInternal(pData);
}

XUI_API int xuiCodeEditIsSignatureHelpOpen(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return pData != NULL && pData->iAssistKind == XUI_CODE_EDIT_ASSIST_SIGNATURE &&
		pData->pAssistPopup != NULL && xuiPopupIsOpen(pData->pAssistPopup);
}

XUI_API int xuiCodeEditShowHint(xui_widget pWidget, const xui_code_hover_t* pHint)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_rect_t tAnchor;
	int iOffset;
	int iRet;

	if ( pData == NULL || pHint == NULL || pHint->sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditStringSet(&pData->sAssistLabel,
		&pData->iAssistLabelCapacity, pHint->sText);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditStringSet(&pData->sAssistDocumentation,
		&pData->iAssistDocumentationCapacity, "");
	if ( iRet != XUI_OK ) return iRet;
	pData->tAssistActiveRange = (xui_code_range_t){0, 0};
	iOffset = pHint->tRange.iStart;
	if ( iOffset < 0 || iOffset > xuiCodeDocumentGetLength(pData->pDocument) ) {
		xui_code_selection_t tSelection;
		memset(&tSelection, 0, sizeof(tSelection));
		if ( xuiCodeSelectionGetState(pData->pSelection, &tSelection) != XUI_OK ) return XUI_ERROR_UNSUPPORTED;
		iOffset = tSelection.iCaretOffset;
	}
	iRet = xuiCodeEditGetOffsetRect(pWidget, iOffset, &tAnchor);
	if ( iRet != XUI_OK ) return iRet;
	pData->iAssistKind = XUI_CODE_EDIT_ASSIST_HINT;
	(void)xuiPopupSetContentSize(pData->pAssistPopup, 420.0f, 38.0f);
	(void)xuiPopupSetAnchorRect(pData->pAssistPopup, tAnchor);
	(void)xuiWidgetInvalidate(pData->pAssistContent,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return xuiPopupSetOpen(pData->pAssistPopup, 1);
}

XUI_API int xuiCodeEditCloseHint(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iAssistKind != XUI_CODE_EDIT_ASSIST_HINT ) return XUI_OK;
	return __xuiCodeEditCloseAssistInternal(pData);
}

XUI_API int xuiCodeEditIsHintOpen(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return pData != NULL && pData->iAssistKind == XUI_CODE_EDIT_ASSIST_HINT &&
		pData->pAssistPopup != NULL && xuiPopupIsOpen(pData->pAssistPopup);
}

static int __xuiCodeEditPlaceholderCompare(const void* pLeft, const void* pRight)
{
	const xui_code_placeholder_t* pA = (const xui_code_placeholder_t*)pLeft;
	const xui_code_placeholder_t* pB = (const xui_code_placeholder_t*)pRight;
	if ( pA->iIndex != pB->iIndex ) return pA->iIndex - pB->iIndex;
	return pA->tRange.iStart - pB->tRange.iStart;
}

XUI_API int xuiCodeEditSetPlaceholders(xui_widget pWidget,
	const xui_code_placeholder_t* pPlaceholders, int iCount, int iActiveIndex)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_placeholder_t* pCopy;
	int iLength;
	int i;

	if ( pData == NULL || iCount < 0 || (iCount > 0 && pPlaceholders == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCount == 0 ) return xuiCodeEditClearPlaceholders(pWidget);
	iLength = xuiCodeDocumentGetLength(pData->pDocument);
	for ( i = 0; i < iCount; i++ ) {
		if ( pPlaceholders[i].tRange.iStart < 0 ||
		     pPlaceholders[i].tRange.iEnd < pPlaceholders[i].tRange.iStart ||
		     pPlaceholders[i].tRange.iEnd > iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	pCopy = (xui_code_placeholder_t*)xrtMalloc(sizeof(*pCopy) * (size_t)iCount);
	if ( pCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(pCopy, pPlaceholders, sizeof(*pCopy) * (size_t)iCount);
	qsort(pCopy, (size_t)iCount, sizeof(*pCopy), __xuiCodeEditPlaceholderCompare);
	xrtFree(pData->pPlaceholders);
	pData->pPlaceholders = pCopy;
	pData->iPlaceholderCount = iCount;
	pData->iPlaceholderActive = 0;
	for ( i = 0; i < iCount; i++ ) {
		if ( pCopy[i].iIndex == iActiveIndex ) { pData->iPlaceholderActive = i; break; }
	}
	return xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
		pCopy[pData->iPlaceholderActive].tRange.iStart,
		pCopy[pData->iPlaceholderActive].tRange.iEnd);
}

XUI_API int xuiCodeEditClearPlaceholders(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xrtFree(pData->pPlaceholders);
	pData->pPlaceholders = NULL;
	pData->iPlaceholderCount = 0;
	pData->iPlaceholderActive = 0;
	return XUI_OK;
}

XUI_API int xuiCodeEditMovePlaceholder(xui_widget pWidget, int iDirection)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	xui_code_placeholder_t* pPlaceholder;

	if ( pData == NULL || iDirection == 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iPlaceholderCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	pData->iPlaceholderActive += (iDirection > 0) ? 1 : -1;
	if ( pData->iPlaceholderActive < 0 ) pData->iPlaceholderActive = pData->iPlaceholderCount - 1;
	if ( pData->iPlaceholderActive >= pData->iPlaceholderCount ) pData->iPlaceholderActive = 0;
	pPlaceholder = &pData->pPlaceholders[pData->iPlaceholderActive];
	return xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument,
		pPlaceholder->tRange.iStart, pPlaceholder->tRange.iEnd);
}

XUI_API int xuiCodeEditGetActivePlaceholder(xui_widget pWidget,
	xui_code_placeholder_t* pPlaceholder)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pPlaceholder == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iPlaceholderCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	*pPlaceholder = pData->pPlaceholders[pData->iPlaceholderActive];
	return XUI_OK;
}

XUI_API int xuiCodeEditFindAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pResultCount)
{
	xui_code_edit_data_t* pData;
	const char* sPattern;
	const char* sReplacement;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pResultCount != NULL ) *pResultCount = 0;
	iRet = __xuiCodeEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditUpdateFindResults(pWidget, pData, sPattern, iFlags, iRangeStart, iRangeEnd, NULL);
	if ( iRet == XUI_OK && pResultCount != NULL ) *pResultCount = pData->iFindResultCount;
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit find all failed");
	return iRet;
}

XUI_API int xuiCodeEditFindNext(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiCodeEditFindMove(pWidget, pOptions, 0);
}

XUI_API int xuiCodeEditFindPrevious(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	return __xuiCodeEditFindMove(pWidget, pOptions, 1);
}

XUI_API int xuiCodeEditReplaceCurrent(xui_widget pWidget, const xui_find_options_t* pOptions)
{
	xui_code_edit_data_t* pData;
	xui_find_result_t tResult;
	const char* sPattern;
	const char* sReplacement;
	char* sRangeText;
	char* sOutput;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iSelStart;
	int iSelEnd;
	int iReplaceCount;
	int iRangeTextLength;
	int iOutputLength;
	int iReplacementLength;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	sRangeText = NULL;
	sOutput = NULL;
	iRet = __xuiCodeEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iSelStart = 0;
	iSelEnd = 0;
	(void)xuiCodeSelectionGetRange(pData->pSelection, &iSelStart, &iSelEnd);
	if ( iSelEnd <= iSelStart ) {
		iRet = __xuiCodeEditFindMove(pWidget, pOptions, 0);
		if ( iRet != XUI_OK ) return iRet;
		(void)xuiCodeSelectionGetRange(pData->pSelection, &iSelStart, &iSelEnd);
	}
	memset(&tResult, 0, sizeof(tResult));
	iRet = __xuiCodeEditCopyDocumentRange(pData, iSelStart, iSelEnd,
		&sRangeText, &iRangeTextLength);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiFindText(sRangeText, iRangeTextLength, sPattern, 0, 0,
		iRangeTextLength, iFlags & ~XUI_FIND_WRAP, &tResult,
		pData->sError, (int)sizeof(pData->sError));
	if ( iRet != XUI_OK || tResult.iStart != 0 ||
	     tResult.iEnd != iRangeTextLength ) {
		iRet = XUI_ERROR_UNSUPPORTED;
		goto cleanup;
	}
	iRet = xuiFindReplaceAllText(sRangeText, iRangeTextLength, sPattern,
		sReplacement, 0, iRangeTextLength, iFlags, &sOutput,
		&iOutputLength, &iReplaceCount, pData->sError, (int)sizeof(pData->sError));
	if ( iRet != XUI_OK ) goto cleanup;
	if ( iReplaceCount <= 0 ) {
		iRet = XUI_ERROR_UNSUPPORTED;
		goto cleanup;
	}
	iReplacementLength = iOutputLength;
	iRet = xuiCodeDocumentReplace(pData->pDocument, iSelStart, iSelEnd,
		(sOutput != NULL) ? sOutput : "");
	if ( iRet == XUI_OK ) iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, iSelStart, iSelStart + iReplacementLength);
	if ( iRet == XUI_OK ) iRet = xuiCodeEditEnsureCaretVisible(pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) {
		tResult.iStart = iSelStart;
		tResult.iEnd = iSelStart + iReplacementLength;
		iRet = __xuiCodeEditUpdateFindResults(pWidget, pData, sPattern, iFlags, iRangeStart, iRangeEnd, &tResult);
	}
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit replace failed");

cleanup:
	xrtFree(sRangeText);
	xuiFindFreeText(sOutput);
	return iRet;
}

XUI_API int xuiCodeEditReplaceAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pReplaceCount)
{
	xui_code_edit_data_t* pData;
	const char* sPattern;
	const char* sReplacement;
	char* sRangeText;
	char* sOutput;
	uint32_t iFlags;
	int iStartOffset;
	int iRangeStart;
	int iRangeEnd;
	int iRangeLength;
	int iOutputLength;
	int iCount;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	sRangeText = NULL;
	sOutput = NULL;
	iOutputLength = 0;
	iRet = __xuiCodeEditFindResolve(pData, pOptions, 0, &sPattern, &sReplacement, &iFlags, &iStartOffset, &iRangeStart, &iRangeEnd);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeEditCopyDocumentRange(pData, iRangeStart, iRangeEnd,
		&sRangeText, &iRangeLength);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiFindReplaceAllText(sRangeText, iRangeLength, sPattern,
		sReplacement, 0, iRangeLength, iFlags, &sOutput, &iOutputLength,
		&iCount, pData->sError, (int)sizeof(pData->sError));
	if ( iRet != XUI_OK ) goto cleanup;
	if ( pReplaceCount != NULL ) *pReplaceCount = iCount;
	if ( iCount > 0 ) {
		iRet = xuiCodeDocumentReplace(pData->pDocument, iRangeStart,
			iRangeEnd, (sOutput != NULL) ? sOutput : "");
		if ( iRet == XUI_OK ) iRet = xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, iRangeStart, iRangeStart);
		if ( iRet == XUI_OK ) iRet = xuiCodeEditEnsureCaretVisible(pWidget);
	}
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditStoreFindState(pData, sPattern, sReplacement, iFlags, iRangeStart, iRangeEnd);
	if ( iRet == XUI_OK ) {
		__xuiCodeEditClearFindResults(pData);
		iRet = xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit replace all failed");

cleanup:
	xrtFree(sRangeText);
	xuiFindFreeText(sOutput);
	(void)iOutputLength;
	return iRet;
}

XUI_API int xuiCodeEditClearFind(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCodeEditClearFindResults(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetFindResultCount(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->iFindResultCount : 0;
}

XUI_API int xuiCodeEditGetFindResult(xui_widget pWidget, int iIndex, xui_code_find_result_t* pResult)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pResult == NULL || iIndex < 0 || iIndex >= pData->iFindResultCount ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCodeEditFillFindResult(pWidget, pData, &pData->pFindResults[iIndex], pResult);
	return XUI_OK;
}

static int __xuiCodeFindScopeReserveEditors(xui_code_find_scope pScope, int iCapacity)
{
	xui_code_find_scope_editor_t* pNew;

	if ( pScope == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pScope->iEditorCapacity ) return XUI_OK;
	if ( iCapacity < pScope->iEditorCapacity * 2 ) iCapacity = pScope->iEditorCapacity * 2;
	if ( iCapacity < 4 ) iCapacity = 4;
	pNew = (xui_code_find_scope_editor_t*)xrtRealloc(pScope->pEditors, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iCapacity > pScope->iEditorCapacity ) {
		memset(pNew + pScope->iEditorCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - pScope->iEditorCapacity));
	}
	pScope->pEditors = pNew;
	pScope->iEditorCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeFindScopeReserveResults(xui_code_find_scope pScope, int iCapacity)
{
	xui_code_find_result_t* pNew;

	if ( pScope == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pScope->iResultCapacity ) return XUI_OK;
	if ( iCapacity < pScope->iResultCapacity * 2 ) iCapacity = pScope->iResultCapacity * 2;
	if ( iCapacity < 32 ) iCapacity = 32;
	pNew = (xui_code_find_result_t*)xrtRealloc(pScope->pResults, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pScope->pResults = pNew;
	pScope->iResultCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeFindScopeSetEntryName(xui_code_find_scope_editor_t* pEntry, const char* sName)
{
	char* sCopy;

	if ( pEntry == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sName == NULL || sName[0] == '\0' ) {
		xrtFree(pEntry->sName);
		pEntry->sName = NULL;
		return XUI_OK;
	}
	sCopy = __xuiCodeEditStringDup(sName);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	xrtFree(pEntry->sName);
	pEntry->sName = sCopy;
	return XUI_OK;
}

static const char* __xuiCodeFindScopeGetEditorNameByWidget(xui_code_find_scope pScope, xui_widget pEditor)
{
	int i;

	if ( pScope == NULL || pEditor == NULL ) return "";
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( pScope->pEditors[i].pEditor == pEditor ) {
			return (pScope->pEditors[i].sName != NULL) ? pScope->pEditors[i].sName : "";
		}
	}
	return "";
}

XUI_API int xuiCodeFindScopeCreate(xui_code_find_scope* ppScope)
{
	xui_code_find_scope pScope;

	if ( ppScope == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppScope = NULL;
	pScope = (xui_code_find_scope)xrtMalloc(sizeof(*pScope));
	if ( pScope == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pScope, 0, sizeof(*pScope));
	pScope->iActiveIndex = -1;
	*ppScope = pScope;
	return XUI_OK;
}

XUI_API void xuiCodeFindScopeDestroy(xui_code_find_scope pScope)
{
	int i;

	if ( pScope == NULL ) return;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( xuiCodeEditGetFindScope(pScope->pEditors[i].pEditor) == pScope ) {
			(void)xuiCodeEditSetFindScope(pScope->pEditors[i].pEditor, NULL);
		}
		xrtFree(pScope->pEditors[i].sName);
	}
	xrtFree(pScope->pEditors);
	xrtFree(pScope->pResults);
	xrtFree(pScope);
}

XUI_API int xuiCodeFindScopeClearEditors(xui_code_find_scope pScope)
{
	int i;

	if ( pScope == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( xuiCodeEditGetFindScope(pScope->pEditors[i].pEditor) == pScope ) {
			(void)xuiCodeEditSetFindScope(pScope->pEditors[i].pEditor, NULL);
		}
		xrtFree(pScope->pEditors[i].sName);
		pScope->pEditors[i].sName = NULL;
		pScope->pEditors[i].pEditor = NULL;
	}
	pScope->iEditorCount = 0;
	pScope->iResultCount = 0;
	pScope->iActiveIndex = -1;
	return XUI_OK;
}

XUI_API int xuiCodeFindScopeAddEditor(xui_code_find_scope pScope, xui_widget pEditor)
{
	return xuiCodeFindScopeAddEditorNamed(pScope, pEditor, NULL);
}

XUI_API int xuiCodeFindScopeAddEditorNamed(xui_code_find_scope pScope, xui_widget pEditor, const char* sName)
{
	int i;
	int iRet;

	if ( pScope == NULL || __xuiCodeEditGetData(pEditor) == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( pScope->pEditors[i].pEditor == pEditor ) {
			if ( sName != NULL ) return __xuiCodeFindScopeSetEntryName(&pScope->pEditors[i], sName);
			return XUI_OK;
		}
	}
	iRet = __xuiCodeFindScopeReserveEditors(pScope, pScope->iEditorCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pScope->pEditors[pScope->iEditorCount].pEditor = pEditor;
	pScope->pEditors[pScope->iEditorCount].sName = NULL;
	iRet = __xuiCodeFindScopeSetEntryName(&pScope->pEditors[pScope->iEditorCount], sName);
	if ( iRet != XUI_OK ) return iRet;
	pScope->iEditorCount++;
	pScope->iResultCount = 0;
	pScope->iActiveIndex = -1;
	(void)xuiCodeEditSetFindScope(pEditor, pScope);
	return XUI_OK;
}

XUI_API int xuiCodeFindScopeSetEditorName(xui_code_find_scope pScope, xui_widget pEditor, const char* sName)
{
	int i;

	if ( pScope == NULL || pEditor == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( pScope->pEditors[i].pEditor == pEditor ) {
			return __xuiCodeFindScopeSetEntryName(&pScope->pEditors[i], sName);
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeFindScopeRemoveEditor(xui_code_find_scope pScope, xui_widget pEditor)
{
	int i;

	if ( pScope == NULL || pEditor == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		if ( pScope->pEditors[i].pEditor == pEditor ) {
			xrtFree(pScope->pEditors[i].sName);
			memmove(&pScope->pEditors[i], &pScope->pEditors[i + 1], sizeof(pScope->pEditors[i]) * (size_t)(pScope->iEditorCount - i - 1));
			pScope->iEditorCount--;
			if ( pScope->iEditorCount >= 0 ) memset(&pScope->pEditors[pScope->iEditorCount], 0, sizeof(pScope->pEditors[pScope->iEditorCount]));
			pScope->iResultCount = 0;
			pScope->iActiveIndex = -1;
			if ( xuiCodeEditGetFindScope(pEditor) == pScope ) (void)xuiCodeEditSetFindScope(pEditor, NULL);
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeFindScopeGetEditorCount(xui_code_find_scope pScope)
{
	return (pScope != NULL) ? pScope->iEditorCount : 0;
}

XUI_API xui_widget xuiCodeFindScopeGetEditor(xui_code_find_scope pScope, int iIndex)
{
	if ( pScope == NULL || iIndex < 0 || iIndex >= pScope->iEditorCount ) return NULL;
	return pScope->pEditors[iIndex].pEditor;
}

XUI_API const char* xuiCodeFindScopeGetEditorName(xui_code_find_scope pScope, int iIndex)
{
	if ( pScope == NULL || iIndex < 0 || iIndex >= pScope->iEditorCount ) return "";
	return (pScope->pEditors[iIndex].sName != NULL) ? pScope->pEditors[iIndex].sName : "";
}

XUI_API int xuiCodeFindScopeSetActivate(xui_code_find_scope pScope, xui_code_find_activate_proc onActivate, void* pUser)
{
	if ( pScope == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pScope->onActivate = onActivate;
	pScope->pActivateUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeFindScopeFindAll(xui_code_find_scope pScope, const xui_find_options_t* pOptions, int* pResultCount)
{
	xui_code_find_result_t tResult;
	xui_code_edit_data_t* pData;
	xui_widget pEditor;
	int i;
	int j;
	int iCount;
	int iRet;

	if ( pScope == NULL || pOptions == NULL || pOptions->sPattern == NULL || pOptions->sPattern[0] == '\0' ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pResultCount != NULL ) *pResultCount = 0;
	pScope->iResultCount = 0;
	pScope->iActiveIndex = -1;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		pEditor = pScope->pEditors[i].pEditor;
		pData = __xuiCodeEditGetData(pEditor);
		if ( pData == NULL ) continue;
		iCount = 0;
		iRet = xuiCodeEditFindAll(pEditor, pOptions, &iCount);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) goto fail;
		iRet = __xuiCodeFindScopeReserveResults(pScope, pScope->iResultCount + iCount);
		if ( iRet != XUI_OK ) goto fail;
		for ( j = 0; j < pData->iFindResultCount; j++ ) {
			__xuiCodeEditFillFindResult(pEditor, pData, &pData->pFindResults[j], &tResult);
			pScope->pResults[pScope->iResultCount++] = tResult;
		}
	}
	if ( pResultCount != NULL ) *pResultCount = pScope->iResultCount;
	return XUI_OK;
fail:
	pScope->iResultCount = 0;
	pScope->iActiveIndex = -1;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		pEditor = pScope->pEditors[i].pEditor;
		pData = __xuiCodeEditGetData(pEditor);
		if ( pData == NULL ) continue;
		__xuiCodeEditClearFindResults(pData);
		(void)xuiWidgetInvalidate(pEditor, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

XUI_API int xuiCodeFindScopeGetResultCount(xui_code_find_scope pScope)
{
	return (pScope != NULL) ? pScope->iResultCount : 0;
}

XUI_API int xuiCodeFindScopeGetResult(xui_code_find_scope pScope, int iIndex, xui_code_find_result_t* pResult)
{
	if ( pScope == NULL || pResult == NULL || iIndex < 0 || iIndex >= pScope->iResultCount ) return XUI_ERROR_INVALID_ARGUMENT;
	*pResult = pScope->pResults[iIndex];
	pResult->iSize = sizeof(*pResult);
	return XUI_OK;
}

XUI_API const char* xuiCodeFindScopeGetResultEditorName(xui_code_find_scope pScope, int iIndex)
{
	if ( pScope == NULL || iIndex < 0 || iIndex >= pScope->iResultCount ) return "";
	return __xuiCodeFindScopeGetEditorNameByWidget(pScope, pScope->pResults[iIndex].pEditor);
}

XUI_API int xuiCodeFindScopeActivateResult(xui_code_find_scope pScope, int iIndex)
{
	xui_code_find_result_t* pResult;
	xui_code_edit_data_t* pData;
	xui_find_result_t tFind;
	int iRet;

	if ( pScope == NULL || iIndex < 0 || iIndex >= pScope->iResultCount ) return XUI_ERROR_INVALID_ARGUMENT;
	pResult = &pScope->pResults[iIndex];
	pData = __xuiCodeEditGetData(pResult->pEditor);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tFind, 0, sizeof(tFind));
	tFind.iSize = sizeof(tFind);
	tFind.iStart = pResult->iStart;
	tFind.iEnd = pResult->iEnd;
	iRet = __xuiCodeEditApplyFindResult(pResult->pEditor, pData, &tFind);
	if ( iRet != XUI_OK ) return iRet;
	pScope->iActiveIndex = iIndex;
	if ( pScope->onActivate != NULL ) {
		pScope->onActivate(pScope, pResult->pEditor, pResult, pScope->pActivateUser);
	}
	return XUI_OK;
}

static void __xuiCodeEditFindWindowOptions(xui_code_edit_data_t* pData, xui_find_options_t* pOptions)
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

static int __xuiCodeEditFindWindowUseScope(xui_code_edit_data_t* pData)
{
	return pData != NULL && pData->pFindScope != NULL && pData->pScopeCheck != NULL && xuiCheckBoxGetChecked(pData->pScopeCheck);
}

static void __xuiCodeEditFindWindowSetStatus(xui_code_edit_data_t* pData, const char* sText)
{
	if ( pData == NULL || pData->pFindStatus == NULL ) return;
	(void)xuiLabelSetText(pData->pFindStatus, (sText != NULL) ? sText : "");
}

static void __xuiCodeEditFindWindowApplyLanguage(xui_widget pCodeEdit, xui_code_edit_data_t* pData)
{
	xui_context pContext;

	if ( pCodeEdit == NULL || pData == NULL || pData->pFindWindow == NULL ) return;
	pContext = xuiWidgetGetContext(pCodeEdit);
	pData->iFindLanguageRevision = xuiGetLanguageRevision(pContext);
	(void)xuiWindowSetTitle(pData->pFindWindow, xuiTranslate(pContext, pData->bFindWindowReplace ? XUI_TR_REPLACE_TITLE : XUI_TR_FIND_TITLE));
	if ( pData->pFindInput != NULL ) (void)xuiInputSetPlaceholder(pData->pFindInput, xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER));
	if ( pData->pReplaceInput != NULL ) (void)xuiInputSetPlaceholder(pData->pReplaceInput, xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER));
	if ( pData->pFindPrevButton != NULL ) (void)xuiButtonSetText(pData->pFindPrevButton, xuiTranslate(pContext, XUI_TR_FIND_PREVIOUS));
	if ( pData->pFindNextButton != NULL ) (void)xuiButtonSetText(pData->pFindNextButton, xuiTranslate(pContext, XUI_TR_FIND_NEXT));
	if ( pData->pFindAllButton != NULL ) (void)xuiButtonSetText(pData->pFindAllButton, xuiTranslate(pContext, XUI_TR_FIND_ALL));
	if ( pData->pReplaceButton != NULL ) (void)xuiButtonSetText(pData->pReplaceButton, xuiTranslate(pContext, XUI_TR_REPLACE_CURRENT));
	if ( pData->pReplaceAllButton != NULL ) (void)xuiButtonSetText(pData->pReplaceAllButton, xuiTranslate(pContext, XUI_TR_REPLACE_ALL));
	if ( pData->pCaseCheck != NULL ) (void)xuiCheckBoxSetText(pData->pCaseCheck, xuiTranslate(pContext, XUI_TR_FIND_CASE));
	if ( pData->pWordCheck != NULL ) (void)xuiCheckBoxSetText(pData->pWordCheck, xuiTranslate(pContext, XUI_TR_FIND_WORD));
	if ( pData->pRegexCheck != NULL ) (void)xuiCheckBoxSetText(pData->pRegexCheck, xuiTranslate(pContext, XUI_TR_FIND_REGEX));
	if ( pData->pEscapeCheck != NULL ) (void)xuiCheckBoxSetText(pData->pEscapeCheck, xuiTranslate(pContext, XUI_TR_FIND_ESCAPE));
	if ( pData->pSelectionCheck != NULL ) (void)xuiCheckBoxSetText(pData->pSelectionCheck, xuiTranslate(pContext, XUI_TR_FIND_SELECTION));
	if ( pData->pScopeCheck != NULL ) (void)xuiCheckBoxSetText(pData->pScopeCheck, xuiTranslate(pContext, XUI_TR_FIND_SCOPE));
	(void)__xuiCodeEditFindResultTableColumns(pData, pData->bFindResultScopeMode);
}

static int __xuiCodeEditUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_code_edit_data_t* pData;
	xui_context pContext;
	xui_code_selection_t tSelection;
	uint32_t iVersion;
	int bSelectionChanged;
	int iRet;

	(void)pUser;
	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	/* Input may request a caret position before this frame settles dock
	   and window layout. Repeat only when that requested viewport changes.
	   Explicit scrolling cancels following, so reading elsewhere stays put. */
	if ( pData->bCaretFollowViewport ) {
		xui_rect_t tViewport = xuiWidgetGetContentRect(pWidget);
		if ( tViewport.fW != pData->tCaretRequestedViewport.fW ||
		     tViewport.fH != pData->tCaretRequestedViewport.fH ) {
			iRet = xuiCodeEditEnsureCaretVisible(pWidget);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	__xuiCodeEditSyncAssistColors(pWidget, pData);
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	memset(&tSelection, 0, sizeof(tSelection));
	bSelectionChanged = xuiCodeSelectionGetState(pData->pSelection, &tSelection) == XUI_OK &&
		(tSelection.iAnchorOffset != pData->iEditNotifyAnchor ||
		 tSelection.iCaretOffset != pData->iEditNotifyCaret);
	if ( iVersion != pData->iEditNotifyVersion ) {
		pData->iEditNotifyVersion = iVersion;
		(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_TEXT_CHANGED, NULL,
			xuiCodeDocumentGetLength(pData->pDocument), tSelection.iAnchorOffset,
			tSelection.iCaretOffset, 0, 0, 1);
	}
	if ( bSelectionChanged ) {
		(void)xuiInternalEditEmitSized(pWidget, XUI_EDIT_EVENT_SELECTION_CHANGED, NULL,
			xuiCodeDocumentGetLength(pData->pDocument), tSelection.iAnchorOffset,
			tSelection.iCaretOffset, 0, 0, 1);
		pData->iEditNotifyAnchor = tSelection.iAnchorOffset;
		pData->iEditNotifyCaret = tSelection.iCaretOffset;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( pData->iCompletionCount > 0 && pData->pCompletionPopup != NULL &&
	     !xuiPopupIsOpen(pData->pCompletionPopup) ) {
		__xuiCodeEditCompletionItemsClear(pData);
	}
	if ( pData->bCompletionPending ) {
		if ( xuiGetFocusWidget(pContext) != pWidget || pData->bImeComposing ) {
			pData->bCompletionPending = 0;
			pData->fCompletionElapsed = 0.0f;
		} else {
			if ( fDelta > 0.0f ) pData->fCompletionElapsed += fDelta;
			if ( pData->fCompletionElapsed >= pData->fCompletionDelay ) {
				iRet = __xuiCodeEditShowCompletionInternal(pWidget, pData, 0);
				if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) {
					__xuiCodeEditSetError(pData, "CodeEdit completion request failed");
				}
			}
		}
	}
	if ( pData->pFindWindow != NULL ) {
		if ( pData->iFindLanguageRevision != xuiGetLanguageRevision(pContext) ) {
			__xuiCodeEditFindWindowApplyLanguage(pWidget, pData);
		}
	}
	return XUI_OK;
}

static int __xuiCodeEditSyncScopeList(xui_code_edit_data_t* pData, xui_code_find_scope pScope)
{
	if ( pData == NULL || pScope == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCodeEditRefreshFindResultTable(pData, 1, pScope->iActiveIndex);
}

static void __xuiCodeEditFindWindowRefresh(xui_widget pCodeEdit)
{
	xui_code_edit_data_t* pData;
	xui_find_options_t tOptions;
	char sStatus[64];
	int iCount;
	int iRet;
	int bUseScope;

	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL ) return;
	__xuiCodeEditFindWindowOptions(pData, &tOptions);
	bUseScope = __xuiCodeEditFindWindowUseScope(pData);
	if ( tOptions.sPattern == NULL || tOptions.sPattern[0] == '\0' ) {
		__xuiCodeEditClearFindResults(pData);
		if ( bUseScope && pData->pFindScope != NULL ) {
			pData->pFindScope->iResultCount = 0;
			pData->pFindScope->iActiveIndex = -1;
		}
		(void)__xuiCodeEditRefreshFindResultTable(pData, bUseScope, -1);
		(void)xuiWidgetInvalidate(pCodeEdit, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		__xuiCodeEditFindWindowSetStatus(pData, "");
		return;
	}
	iCount = 0;
	if ( bUseScope ) {
		tOptions.iFlags &= ~XUI_FIND_SELECTION;
		iRet = xuiCodeFindScopeFindAll(pData->pFindScope, &tOptions, &iCount);
		if ( iRet == XUI_OK ) iRet = __xuiCodeEditSyncScopeList(pData, pData->pFindScope);
	} else {
		iRet = xuiCodeEditFindAll(pCodeEdit, &tOptions, &iCount);
	}
	if ( iRet != XUI_OK ) {
		__xuiCodeEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pCodeEdit), XUI_TR_FIND_INVALID_PATTERN));
		return;
	}
	snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pCodeEdit), XUI_TR_FIND_MATCHES_FMT), iCount);
	__xuiCodeEditFindWindowSetStatus(pData, sStatus);
}

static void __xuiCodeEditFindInputChange(xui_widget pWidget, const char* sText, void* pUser)
{
	(void)pWidget;
	(void)sText;
	__xuiCodeEditFindWindowRefresh((xui_widget)pUser);
}

static void __xuiCodeEditFindCheckChange(xui_widget pWidget, int bChecked, void* pUser)
{
	(void)pWidget;
	(void)bChecked;
	__xuiCodeEditFindWindowRefresh((xui_widget)pUser);
}

static void __xuiCodeEditFindTableSelect(xui_widget pWidget, int iIndex, int iColumn, int iSelectionMode, void* pUser)
{
	xui_widget pCodeEdit;
	xui_code_edit_data_t* pData;
	xui_find_result_t tFind;

	(void)pWidget;
	(void)iColumn;
	(void)iSelectionMode;
	pCodeEdit = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL || iIndex < 0 ) return;
	if ( __xuiCodeEditFindWindowUseScope(pData) ) {
		(void)xuiCodeFindScopeActivateResult(pData->pFindScope, iIndex);
		return;
	}
	if ( iIndex >= pData->iFindResultCount ) return;
	tFind = pData->pFindResults[iIndex];
	pData->iFindActiveIndex = iIndex;
	(void)__xuiCodeEditApplyFindResult(pCodeEdit, pData, &tFind);
	(void)xuiWidgetInvalidate(pCodeEdit, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiCodeEditFindButtonClick(xui_widget pButton, void* pUser)
{
	xui_widget pCodeEdit;
	xui_code_edit_data_t* pData;
	xui_find_options_t tOptions;
	char sStatus[64];
	int iCount;
	int iIndex;
	int iRet;

	pCodeEdit = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL ) return;
	__xuiCodeEditFindWindowOptions(pData, &tOptions);
	iRet = XUI_OK;
	if ( __xuiCodeEditFindWindowUseScope(pData) && (pButton == pData->pFindPrevButton || pButton == pData->pFindNextButton || pButton == pData->pFindAllButton) ) {
		iRet = xuiCodeFindScopeFindAll(pData->pFindScope, &tOptions, &iCount);
		if ( iRet == XUI_OK ) iRet = __xuiCodeEditSyncScopeList(pData, pData->pFindScope);
		if ( iRet == XUI_OK && pData->pFindScope->iResultCount > 0 && pButton != pData->pFindAllButton ) {
			iIndex = pData->pFindScope->iActiveIndex;
			if ( iIndex < 0 ) iIndex = (pButton == pData->pFindPrevButton) ? pData->pFindScope->iResultCount : -1;
			iIndex += (pButton == pData->pFindPrevButton) ? -1 : 1;
			if ( iIndex < 0 ) iIndex = pData->pFindScope->iResultCount - 1;
			if ( iIndex >= pData->pFindScope->iResultCount ) iIndex = 0;
			iRet = xuiCodeFindScopeActivateResult(pData->pFindScope, iIndex);
			if ( iRet == XUI_OK && pData->pFindResultList != NULL ) {
				(void)xuiTableViewSetSelectedRow(pData->pFindResultList, iIndex);
				(void)xuiTableViewEnsureCellVisible(pData->pFindResultList, iIndex, 0);
			}
		}
	} else if ( pButton == pData->pFindPrevButton ) {
		iRet = xuiCodeEditFindPrevious(pCodeEdit, &tOptions);
	} else if ( pButton == pData->pFindNextButton ) {
		iRet = xuiCodeEditFindNext(pCodeEdit, &tOptions);
	} else if ( pButton == pData->pFindAllButton ) {
		iRet = xuiCodeEditFindAll(pCodeEdit, &tOptions, &iCount);
	} else if ( pButton == pData->pReplaceButton ) {
		iRet = xuiCodeEditReplaceCurrent(pCodeEdit, &tOptions);
	} else if ( pButton == pData->pReplaceAllButton ) {
		iRet = xuiCodeEditReplaceAll(pCodeEdit, &tOptions, &iCount);
		snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pCodeEdit), XUI_TR_FIND_REPLACED_FMT), iCount);
		__xuiCodeEditFindWindowSetStatus(pData, sStatus);
	}
	if ( iRet == XUI_OK && pButton != pData->pReplaceAllButton ) {
		if ( __xuiCodeEditFindWindowUseScope(pData) ) iCount = pData->pFindScope->iResultCount;
		else iCount = pData->iFindResultCount;
		snprintf(sStatus, sizeof(sStatus), xuiTranslate(xuiWidgetGetContext(pCodeEdit), XUI_TR_FIND_MATCHES_FMT), iCount);
		__xuiCodeEditFindWindowSetStatus(pData, sStatus);
	} else if ( iRet != XUI_OK ) {
		__xuiCodeEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pCodeEdit),
			iRet == XUI_ERROR_INVALID_ARGUMENT ? XUI_TR_FIND_INVALID_PATTERN : XUI_TR_FIND_NOT_FOUND));
	}
}

static void __xuiCodeEditFindWindowClose(xui_widget pWindow, void* pUser)
{
	xui_widget pCodeEdit = (xui_widget)pUser;
	(void)pWindow;
	if ( pCodeEdit != NULL ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pCodeEdit), pCodeEdit);
	}
}

static int __xuiCodeEditFindWindowKeyDown(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pCodeEdit;
	xui_code_edit_data_t* pData;
	int iKey;

	(void)pWidget;
	pCodeEdit = (xui_widget)pUser;
	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pEvent == NULL || pData == NULL || pData->pFindWindow == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iType != XUI_EVENT_KEY_DOWN ) return XUI_OK;
	iKey = pEvent->iKey;
	if ( iKey >= 'a' && iKey <= 'z' ) iKey = iKey - 'a' + 'A';
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0u ) {
		if ( iKey == 'F' ) {
			(void)xuiCodeEditOpenFind(pCodeEdit);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'H' ) {
			(void)xuiCodeEditOpenReplace(pCodeEdit);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( pEvent->iKey == XUI_KEY_F3 ) {
		__xuiCodeEditFindButtonClick(((pEvent->iModifiers & XUI_MOD_SHIFT) != 0u) ? pData->pFindPrevButton : pData->pFindNextButton, pCodeEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ENTER ) {
		__xuiCodeEditFindButtonClick(((pEvent->iModifiers & XUI_MOD_SHIFT) != 0u) ? pData->pFindPrevButton : pData->pFindNextButton, pCodeEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
		(void)xuiWindowSetOpen(pData->pFindWindow, 0);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pCodeEdit), pCodeEdit);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiCodeEditFindWindowLayout(xui_widget pCodeEdit, int bReplace)
{
	xui_code_edit_data_t* pData;
	xui_rect_t tWindow;
	float fHeight;
	float fResultY;

	pData = __xuiCodeEditGetData(pCodeEdit);
	if ( pData == NULL || pData->pFindWindow == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	fHeight = bReplace ? 420.0f : 384.0f;
	fResultY = bReplace ? 122.0f : 86.0f;
	tWindow = xuiWidgetGetRect(pData->pFindWindow);
	if ( tWindow.fW < 720.0f ) tWindow.fW = 720.0f;
	tWindow.fH = fHeight;
	(void)xuiWidgetSetRect(pData->pFindWindow, tWindow);
	(void)xuiWidgetSetRect(pData->pFindInput, (xui_rect_t){12.0f, 12.0f, 320.0f, 28.0f});
	(void)xuiWidgetSetRect(pData->pFindPrevButton, (xui_rect_t){346.0f, 12.0f, 80.0f, 28.0f});
	(void)xuiWidgetSetRect(pData->pFindNextButton, (xui_rect_t){432.0f, 12.0f, 88.0f, 28.0f});
	(void)xuiWidgetSetRect(pData->pFindAllButton, (xui_rect_t){526.0f, 12.0f, 130.0f, 28.0f});
	(void)xuiWidgetSetVisible(pData->pReplaceInput, bReplace);
	(void)xuiWidgetSetVisible(pData->pReplaceButton, bReplace);
	(void)xuiWidgetSetVisible(pData->pReplaceAllButton, bReplace);
	if ( bReplace ) {
		(void)xuiWidgetSetRect(pData->pReplaceInput, (xui_rect_t){12.0f, 50.0f, 320.0f, 28.0f});
		(void)xuiWidgetSetRect(pData->pReplaceButton, (xui_rect_t){346.0f, 50.0f, 116.0f, 28.0f});
		(void)xuiWidgetSetRect(pData->pReplaceAllButton, (xui_rect_t){470.0f, 50.0f, 148.0f, 28.0f});
	}
	(void)xuiWidgetSetRect(pData->pCaseCheck, (xui_rect_t){12.0f, fResultY - 34.0f, 112.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pWordCheck, (xui_rect_t){132.0f, fResultY - 34.0f, 108.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pRegexCheck, (xui_rect_t){248.0f, fResultY - 34.0f, 80.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pEscapeCheck, (xui_rect_t){336.0f, fResultY - 34.0f, 84.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pSelectionCheck, (xui_rect_t){428.0f, fResultY - 34.0f, 108.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pScopeCheck, (xui_rect_t){544.0f, fResultY - 34.0f, 96.0f, 24.0f});
	(void)xuiWidgetSetRect(pData->pFindResultList, (xui_rect_t){12.0f, fResultY, 676.0f, 220.0f});
	(void)xuiWidgetSetRect(pData->pFindStatus, (xui_rect_t){12.0f, fResultY + 228.0f, 676.0f, 24.0f});
	return XUI_OK;
}

static int __xuiCodeEditCreateFindButton(xui_context pContext, xui_widget* ppButton, xui_font pFont, const char* sText, xui_widget pCodeEdit)
{
	xui_button_desc_t tButton;
	int iRet;

	memset(&tButton, 0, sizeof(tButton));
	tButton.iSize = sizeof(tButton);
	tButton.pFont = pFont;
	tButton.sText = sText;
	tButton.fBorderWidth = 1.0f;
	iRet = xuiButtonCreate(pContext, ppButton, &tButton);
	if ( iRet == XUI_OK ) iRet = xuiButtonSetClick(*ppButton, __xuiCodeEditFindButtonClick, pCodeEdit);
	return iRet;
}

static int __xuiCodeEditCreateFindCheck(xui_context pContext, xui_widget* ppCheck, xui_font pFont, const char* sText, int bChecked, xui_widget pCodeEdit)
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
	iRet = xuiCheckBoxCreate(pContext, ppCheck, &tCheck);
	if ( iRet == XUI_OK ) iRet = xuiCheckBoxSetChange(*ppCheck, __xuiCodeEditFindCheckChange, pCodeEdit);
	return iRet;
}

static int __xuiCodeEditCreateFindWindow(xui_widget pCodeEdit, xui_code_edit_data_t* pData)
{
	xui_context pContext;
	xui_widget pRoot;
	xui_widget pClient;
	xui_window_desc_t tWindow;
	xui_input_desc_t tInput;
	xui_label_desc_t tLabel;
	xui_table_view_desc_t tTable;
	xui_font pFont;
	int iRet;

	if ( pCodeEdit == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pCodeEdit);
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
	iRet = xuiWindowCreate(pContext, &pData->pFindWindow, &tWindow);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWindowSetClose(pData->pFindWindow, __xuiCodeEditFindWindowClose, pCodeEdit);
	pClient = xuiWindowGetClientWidget(pData->pFindWindow);
	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pClient, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetPadding(pClient, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetGap(pClient, 0.0f);
	(void)xuiWidgetSetEventHandler(pClient, XUI_EVENT_KEY_DOWN, __xuiCodeEditFindWindowKeyDown, pCodeEdit);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.pFont = pFont;
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_FIND_PLACEHOLDER);
	tInput.fBorderWidth = 1.0f;
	iRet = xuiInputCreate(pContext, &pData->pFindInput, &tInput);
	tInput.sPlaceholder = xuiTranslate(pContext, XUI_TR_REPLACE_PLACEHOLDER);
	if ( iRet == XUI_OK ) iRet = xuiInputCreate(pContext, &pData->pReplaceInput, &tInput);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindButton(pContext, &pData->pFindPrevButton, pFont, xuiTranslate(pContext, XUI_TR_FIND_PREVIOUS), pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindButton(pContext, &pData->pFindNextButton, pFont, xuiTranslate(pContext, XUI_TR_FIND_NEXT), pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindButton(pContext, &pData->pFindAllButton, pFont, xuiTranslate(pContext, XUI_TR_FIND_ALL), pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindButton(pContext, &pData->pReplaceButton, pFont, xuiTranslate(pContext, XUI_TR_REPLACE_CURRENT), pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindButton(pContext, &pData->pReplaceAllButton, pFont, xuiTranslate(pContext, XUI_TR_REPLACE_ALL), pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pCaseCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_CASE), (pData->iFindFlags & XUI_FIND_CASE_SENSITIVE) != 0, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pWordCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_WORD), (pData->iFindFlags & XUI_FIND_WHOLE_WORD) != 0, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pRegexCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_REGEX), (pData->iFindFlags & XUI_FIND_REGEX) != 0, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pEscapeCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_ESCAPE), (pData->iFindFlags & XUI_FIND_ESCAPE) != 0, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pSelectionCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_SELECTION), (pData->iFindFlags & XUI_FIND_SELECTION) != 0, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditCreateFindCheck(pContext, &pData->pScopeCheck, pFont, xuiTranslate(pContext, XUI_TR_FIND_SCOPE), 0, pCodeEdit);
	memset(&tTable, 0, sizeof(tTable));
	tTable.iSize = sizeof(tTable);
	tTable.pFont = pFont;
	tTable.fDefaultRowHeight = 22.0f;
	tTable.fHeaderHeight = 24.0f;
	tTable.iSelectionMode = XUI_TABLE_VIEW_SELECTION_ROW;
	tTable.onCount = __xuiCodeEditFindResultTableCount;
	tTable.onCell = __xuiCodeEditFindResultTableCell;
	tTable.pAdapterUser = pCodeEdit;
	if ( iRet == XUI_OK ) iRet = xuiTableViewCreate(pContext, &pData->pFindResultList, &tTable);
	if ( iRet == XUI_OK ) iRet = xuiTableViewSetSelect(pData->pFindResultList, __xuiCodeEditFindTableSelect, pCodeEdit);
	if ( iRet == XUI_OK ) iRet = __xuiCodeEditFindResultTableColumns(pData, 0);
	memset(&tLabel, 0, sizeof(tLabel));
	tLabel.iSize = sizeof(tLabel);
	tLabel.pFont = pFont;
	tLabel.sText = "";
	tLabel.iTextColor = XUI_COLOR_RGBA(90, 105, 124, 255);
	tLabel.iTextFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	if ( iRet == XUI_OK ) iRet = xuiLabelCreate(pContext, &pData->pFindStatus, &tLabel);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiInputSetChange(pData->pFindInput, __xuiCodeEditFindInputChange, pCodeEdit);
	(void)xuiInputSetChange(pData->pReplaceInput, __xuiCodeEditFindInputChange, pCodeEdit);
	iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindInput);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pReplaceInput);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindPrevButton);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindNextButton);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindAllButton);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pReplaceButton);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pReplaceAllButton);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pCaseCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pWordCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pRegexCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pEscapeCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pSelectionCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pScopeCheck);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindResultList);
	if ( iRet == XUI_OK ) iRet = xuiWindowAddChild(pData->pFindWindow, pData->pFindStatus);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pRoot, pData->pFindWindow);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pData->pFindWindow, (xui_rect_t){32.0f, 32.0f, 720.0f, 384.0f});
	__xuiCodeEditFindWindowApplyLanguage(pCodeEdit, pData);
	return __xuiCodeEditFindWindowLayout(pCodeEdit, 0);
}

static int __xuiCodeEditOpenFindWindow(xui_widget pWidget, int bReplace)
{
	xui_code_edit_data_t* pData;
	xui_rect_t tOwner;
	xui_rect_t tWindow;
	char* sSelected;
	int iStart;
	int iEnd;
	int iLen;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFindWindow == NULL ) {
		iRet = __xuiCodeEditCreateFindWindow(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bFindWindowReplace = bReplace ? 1 : 0;
	iStart = 0;
	iEnd = 0;
	(void)xuiCodeSelectionGetRange(pData->pSelection, &iStart, &iEnd);
	pData->iFindRangeStart = 0;
	pData->iFindRangeEnd = 0;
	if ( iEnd > iStart ) {
		pData->iFindRangeStart = iStart;
		pData->iFindRangeEnd = iEnd;
		iLen = iEnd - iStart;
		if ( iLen > 0 && iLen < 256 ) {
			sSelected = (char*)xrtMalloc((size_t)iLen + 1u);
			if ( sSelected == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
			iRet = xuiCodeDocumentCopyRange(pData->pDocument, iStart, iEnd,
				sSelected, iLen + 1, NULL);
			if ( iRet != XUI_OK ) {
				xrtFree(sSelected);
				return iRet;
			}
			(void)xuiInputSetText(pData->pFindInput, sSelected);
			xrtFree(sSelected);
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
	if ( pData->pScopeCheck != NULL ) {
		(void)xuiWidgetSetVisible(pData->pScopeCheck, pData->pFindScope != NULL);
	}
	__xuiCodeEditFindWindowApplyLanguage(pWidget, pData);
	(void)__xuiCodeEditFindWindowLayout(pWidget, bReplace);
	tOwner = xuiWidgetGetWorldRect(pWidget);
	tWindow = xuiWidgetGetRect(pData->pFindWindow);
	tWindow.fX = tOwner.fX + 16.0f;
	tWindow.fY = tOwner.fY + 16.0f;
	(void)xuiWidgetSetRect(pData->pFindWindow, tWindow);
	(void)xuiWindowSetOpen(pData->pFindWindow, 1);
	(void)xuiWindowBringToFront(pData->pFindWindow);
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pFindInput);
	__xuiCodeEditFindWindowRefresh(pWidget);
	return XUI_OK;
}

XUI_API int xuiCodeEditOpenFind(xui_widget pWidget)
{
	return __xuiCodeEditOpenFindWindow(pWidget, 0);
}

XUI_API int xuiCodeEditOpenReplace(xui_widget pWidget)
{
	return __xuiCodeEditOpenFindWindow(pWidget, 1);
}

XUI_API xui_widget xuiCodeEditGetFindWindow(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->pFindWindow : NULL;
}

static int __xuiCodeEditAfterReplaceAll(xui_widget pWidget, xui_code_edit_data_t* pData, const char* sReplacement, int iReplaceCount, int iRet)
{
	xui_code_range_t tRange;
	int iIndicatorCount;
	int iMarkRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit replace failed");
		return iRet;
	}
	(void)xuiCodeSearchClearResultIndicators(pData->pAnnotations, 0);
	if ( iReplaceCount > 0 && sReplacement != NULL && sReplacement[0] != '\0' ) {
		iIndicatorCount = 0;
		iMarkRet = xuiCodeSearchMarkAllPlain(pData->pDocument, pData->pAnnotations, sReplacement, XUI_CODE_SEARCH_CASE_SENSITIVE, XUI_CODE_INDICATOR_SEARCH_RESULT, &iIndicatorCount);
		if ( iMarkRet != XUI_OK && iMarkRet != XUI_ERROR_UNSUPPORTED ) {
			__xuiCodeEditSetError(pData, "CodeEdit replace indicator update failed");
			return iMarkRet;
		}
	}
	if ( xuiCodeDocumentGetLastEditRange(pData->pDocument, &tRange) == XUI_OK ) {
		(void)xuiCodeSelectionSetRange(pData->pSelection, pData->pDocument, tRange.iStart, tRange.iEnd);
	}
	__xuiCodeEditSetError(pData, "");
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditReplaceAllPlain(xui_widget pWidget, const char* sPattern, const char* sReplacement, uint32_t iFlags, int* pReplaceCount)
{
	xui_code_edit_data_t* pData;
	int iCount;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) {
		__xuiCodeEditSetError(pData, "CodeEdit is readonly");
		return XUI_ERROR_UNSUPPORTED;
	}
	iCount = 0;
	iRet = xuiCodeSearchReplaceAllPlain(pData->pDocument, sPattern, sReplacement, iFlags, &iCount);
	if ( pReplaceCount != NULL ) *pReplaceCount = iCount;
	return __xuiCodeEditAfterReplaceAll(pWidget, pData, sReplacement, iCount, iRet);
}

XUI_API int xuiCodeEditReplaceAllRegex(xui_widget pWidget, const char* sPattern, const char* sReplacement, uint32_t iFlags, int* pReplaceCount)
{
	xui_code_edit_data_t* pData;
	char sError[160];
	int iCount;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly ) {
		__xuiCodeEditSetError(pData, "CodeEdit is readonly");
		return XUI_ERROR_UNSUPPORTED;
	}
	memset(sError, 0, sizeof(sError));
	iCount = 0;
	iRet = xuiCodeSearchReplaceAllRegex(pData->pDocument, sPattern, sReplacement, iFlags, &iCount, sError, (int)sizeof(sError));
	if ( iRet != XUI_OK && sError[0] != '\0' ) __xuiCodeEditSetError(pData, sError);
	if ( pReplaceCount != NULL ) *pReplaceCount = iCount;
	return __xuiCodeEditAfterReplaceAll(pWidget, pData, sReplacement, iCount, iRet);
}

XUI_API int xuiCodeEditSetReadonly(xui_widget pWidget, int bReadonly)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bReadonly = bReadonly ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditIsReadonly(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->bReadonly : 0;
}

XUI_API xui_scroll_model_t* xuiCodeEditGetScrollModel(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return NULL;
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	return &pData->tScrollModel;
}

XUI_API xui_widget xuiCodeEditGetHScrollBarWidget(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return NULL;
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	return pData->pHScrollBar;
}

XUI_API xui_widget xuiCodeEditGetVScrollBarWidget(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return NULL;
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	return pData->pVScrollBar;
}

XUI_API int xuiCodeEditSetScroll(xui_widget pWidget, float fScrollX, float fScrollY)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fScrollX < 0.0f ) fScrollX = 0.0f;
	if ( fScrollY < 0.0f ) fScrollY = 0.0f;
	pData->bCaretFollowViewport = 0;
	pData->fScrollX = fScrollX;
	pData->fScrollY = fScrollY;
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCodeEditInvalidateView(pWidget);
}

XUI_API int xuiCodeEditGetScroll(xui_widget pWidget, float* pScrollX, float* pScrollY)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pScrollX != NULL ) *pScrollX = pData->fScrollX;
	if ( pScrollY != NULL ) *pScrollY = pData->fScrollY;
	return XUI_OK;
}

XUI_API int xuiCodeEditEnsureCaretVisible(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;
	xui_code_selection_t tSelection;
	xui_context pContext;
	xui_proxy_t* pProxy;
	xui_font pFont;
	xui_rect_t tCaret;
	xui_scroll_model_t tTextScrollModel;
	const char* sText;
	float fColumnWidth;
	float fMarginWidth;
	float fLineHeight;
	double fCaretX;
	double fWrapCaretX;
	float fMeasuredWrapWidth;
	int iMeasuredWrapRows;
	int iVisibilityPass;
	float fPreviousWrapWidth;
	int iWrapPass;
	int iStart;
	int iEnd;
	int iLine;
	int iColumn;
	int iColumnOffset;
	int iCaretVisualRow;
	int iRet;
	float fScrollX;
	float fScrollY;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tSelection, 0, sizeof(tSelection));
	iRet = xuiCodeSelectionGetState(pData->pSelection, &tSelection);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentOffsetToLineColumn(pData->pDocument, tSelection.iCaretOffset, &iLine, &iColumn);
	if ( iRet != XUI_OK ) return iRet;
	iStart = 0;
	iEnd = 0;
	(void)xuiCodeDocumentGetLineRange(pData->pDocument, iLine, &iStart, &iEnd);
	iRet = __xuiCodeEditReadRange(pData, iStart, iEnd, &sText);
	if ( iRet != XUI_OK ) return iRet;
	if ( !pData->bWordWrap ) {
		__xuiCodeEditObserveLineLength(pData, sText, 0, iEnd - iStart);
		pData->iMaxLineLengthVersion = xuiCodeDocumentGetChangeVersion(pData->pDocument);
		pData->iMaxLineLengthTabColumns = __xuiCodeEditTabColumns(pData);
	}
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iColumnOffset = iStart;
	(void)xuiCodeDocumentLineColumnToOffset(pData->pDocument, iLine, iColumn, &iColumnOffset);
	pContext = xuiWidgetGetContext(pWidget);
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	fMarginWidth = 0.0f;
	(void)xuiCodeMarginModelGetTotalWidth(pData->pMargins, &fMarginWidth);
	fColumnWidth = __xuiCodeEditColumnWidth(pWidget, pData);
	fLineHeight = __xuiCodeEditLineHeight(pWidget, pData);
	/* Moving the viewport can measure previously unseen preceding lines
	   and shift the caret row. Settle those bounded viewport measurements
	   before painting instead of waiting for a later input/frame. */
	for ( iVisibilityPass = 0; iVisibilityPass < 8; iVisibilityPass++ ) {
	iCaretVisualRow = __xuiCodeEditLineToVisibleRow(pData, iLine);
	fWrapCaretX = -1.0f;
	if ( pData->bWordWrap ) {
		/* Measuring a previously unseen caret line can increase the row
		   count and introduce a scrollbar. Settle its width and content range
		   before clamping the requested offset. */
		for ( iWrapPass = 0; iWrapPass < 4; iWrapPass++ ) {
			fPreviousWrapWidth = pData->fWrapLayoutWidth;
			iRet = __xuiCodeEditWrapOffsetGeometry(pWidget, pData,
				tSelection.iCaretOffset, NULL, NULL, NULL, NULL);
			if ( iRet != XUI_OK ) return iRet;
			iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
			if ( iRet != XUI_OK ) return iRet;
			if ( fabsf(pData->fWrapLayoutWidth - fPreviousWrapWidth) < 0.01f ) break;
		}
		iRet = __xuiCodeEditWrapOffsetGeometry(pWidget, pData,
			tSelection.iCaretOffset, &iCaretVisualRow, NULL, NULL,
			&fWrapCaretX);
		if ( iRet != XUI_OK ) return iRet;
	}
	fCaretX = (fWrapCaretX >= 0.0f) ? fWrapCaretX :
		__xuiCodeEditLineOffsetX(pProxy, pFont, pData, sText, 0,
			iColumnOffset - iStart, fColumnWidth);
	tCaret = (xui_rect_t){
		fCaretX,
		(float)iCaretVisualRow * fLineHeight,
		4.0f + fColumnWidth,
		fLineHeight
	};
	/* Caret coordinates are relative to text, while the live model also
	   includes margins and padding. Keep its scrollbar range unchanged and
	   perform visibility in a local text viewport, so Home still selects zero. */
	tTextScrollModel = pData->tScrollModel;
	tTextScrollModel.tViewportRect.fW = __xuiCodeEditMaxFloat(0.0f,
		(float)tTextScrollModel.tViewportRect.fW - fMarginWidth - 8.0f);
	tTextScrollModel.tViewportRect.fH = __xuiCodeEditMaxFloat(0.0f,
		(float)tTextScrollModel.tViewportRect.fH - 8.0f);
	tTextScrollModel.fContentWidth = __xuiCodeEditMaxFloat(0.0f,
		tTextScrollModel.fContentWidth - fMarginWidth - 8.0f);
	tTextScrollModel.fContentHeight = __xuiCodeEditMaxFloat(0.0f,
		tTextScrollModel.fContentHeight - 8.0f);
	iRet = xuiScrollModelEnsureRectVisible(&tTextScrollModel, tCaret);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiScrollModelGetOffset(&tTextScrollModel, &fScrollX, &fScrollY);
	iRet = xuiScrollModelSetOffset(&pData->tScrollModel, fScrollX, fScrollY);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiScrollModelGetOffset(&pData->tScrollModel, &fScrollX, &fScrollY);
	pData->fScrollX = fScrollX;
	pData->fScrollY = fScrollY;
	if ( !pData->bWordWrap ) break;
	iMeasuredWrapRows = __xuiCodeEditWrapTotalRows(pData);
	fMeasuredWrapWidth = pData->fWrapLayoutWidth;
	iRet = __xuiCodeEditUpdateScrollModel(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( iMeasuredWrapRows == __xuiCodeEditWrapTotalRows(pData) &&
	     fabsf(fMeasuredWrapWidth - pData->fWrapLayoutWidth) < 0.01f ) break;
	}
	pData->bCaretFollowViewport = 1;
	pData->tCaretRequestedViewport = xuiWidgetGetContentRect(pWidget);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditSetDisplayOptions(xui_widget pWidget, uint32_t iOptions)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisplayOptions = iOptions &
		(XUI_CODE_EDIT_SHOW_WHITESPACE | XUI_CODE_EDIT_SHOW_EOL |
		 XUI_CODE_EDIT_SHOW_INDENT_GUIDES | XUI_CODE_EDIT_SHOW_MINIMAP);
	return xuiWidgetInvalidate(pWidget,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_STYLE |
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiCodeEditGetDisplayOptions(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->iDisplayOptions : 0u;
}

XUI_API int xuiCodeEditSetMinimap(xui_widget pWidget, int bVisible, float fWidth)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || fWidth < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fWidth > 0.0f ) pData->fMinimapWidth = fWidth;
	if ( pData->fMinimapWidth < 40.0f ) pData->fMinimapWidth = 40.0f;
	if ( pData->fMinimapWidth > 240.0f ) pData->fMinimapWidth = 240.0f;
	if ( bVisible ) {
		pData->iDisplayOptions |= XUI_CODE_EDIT_SHOW_MINIMAP;
	} else {
		pData->iDisplayOptions &= ~XUI_CODE_EDIT_SHOW_MINIMAP;
		pData->bMinimapDragging = 0;
	}
	return xuiWidgetInvalidate(pWidget,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetMinimap(xui_widget pWidget, int* pVisible, float* pWidth)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pVisible != NULL ) *pVisible =
		(pData->iDisplayOptions & XUI_CODE_EDIT_SHOW_MINIMAP) != 0;
	if ( pWidth != NULL ) *pWidth = pData->fMinimapWidth;
	return XUI_OK;
}

XUI_API xui_rect_t xuiCodeEditGetMinimapRect(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	(void)__xuiCodeEditUpdateScrollModel(pWidget, pData);
	return pData->tMinimapRect;
}

XUI_API int xuiCodeEditSetWordWrap(xui_widget pWidget, int bWordWrap)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bWordWrap = bWordWrap ? 1 : 0;
	pData->fScrollX = 0.0f;
	__xuiCodeEditWrapIndexClear(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetWordWrap(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->bWordWrap : 0;
}

XUI_API int xuiCodeEditSetEolMode(xui_widget pWidget, int iEolMode)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || iEolMode < XUI_CODE_EOL_AUTO || iEolMode > XUI_CODE_EOL_CR ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iEolMode = iEolMode;
	return XUI_OK;
}

XUI_API int xuiCodeEditGetEolMode(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->iEolMode : XUI_CODE_EOL_AUTO;
}

XUI_API int xuiCodeEditSetTabColumns(xui_widget pWidget, int iTabColumns)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || iTabColumns <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTabColumns = iTabColumns;
	if ( pData->iIndentColumns <= 0 ) pData->iIndentColumns = iTabColumns;
	__xuiCodeEditLineLayoutsClear(pData);
	__xuiCodeEditWrapIndexClear(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetTabColumns(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? __xuiCodeEditTabColumns(pData) : 0;
}

XUI_API int xuiCodeEditSetIndentColumns(xui_widget pWidget, int iIndentColumns)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || iIndentColumns <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iIndentColumns = iIndentColumns;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetIndentColumns(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? __xuiCodeEditIndentColumns(pData) : 0;
}

XUI_API int xuiCodeEditSetExpandTabs(xui_widget pWidget, int bExpandTabs)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bExpandTabs ) {
		pData->iFlags |= XUI_CODE_EDIT_EXPAND_TABS;
		pData->iFlags &= ~XUI_CODE_EDIT_INDENT_WITH_TABS;
	} else {
		pData->iFlags &= ~XUI_CODE_EDIT_EXPAND_TABS;
	}
	return XUI_OK;
}

XUI_API int xuiCodeEditGetExpandTabs(xui_widget pWidget)
{
	xui_code_edit_data_t* pData;

	pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL &&
	        (pData->iFlags & XUI_CODE_EDIT_EXPAND_TABS) != 0 &&
	        (pData->iFlags & XUI_CODE_EDIT_INDENT_WITH_TABS) == 0) ? 1 : 0;
}

XUI_API int xuiCodeEditOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_code_edit_data_t* pData;
	int iRet;

	pData = __xuiCodeEditGetData(pWidget);
	if ( pData == NULL || pData->pMenu == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeEditUpdateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) {
		__xuiCodeEditSetError(pData, "CodeEdit menu update failed");
		return iRet;
	}
	(void)xuiMenuSetSelect(pData->pMenu, __xuiCodeEditMenuSelect, pWidget);
	iRet = xuiMenuOpenAt(pData->pMenu, pWidget, fX, fY);
	__xuiCodeEditSetError(pData, (iRet == XUI_OK) ? "" : "CodeEdit menu open failed");
	return iRet;
}

XUI_API const char* xuiCodeEditGetLastError(xui_widget pWidget)
{
	xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
	return (pData != NULL) ? pData->sError : "invalid codeedit widget";
}
