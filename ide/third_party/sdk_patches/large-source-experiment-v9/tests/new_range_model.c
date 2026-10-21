typedef struct diag_new_data {
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
} diag_new_data;

static int diagNewTabColumns(const diag_new_data* pData)
{
	if ( pData != NULL && pData->iTabColumns > 0 ) return pData->iTabColumns;
	return 4;
}

static int diagNewTabAdvance(const diag_new_data* pData, int iVisualColumn)
{
	int iTabColumns;
	int iRemainder;

	iTabColumns = diagNewTabColumns(pData);
	iRemainder = iVisualColumn % iTabColumns;
	return (iRemainder == 0) ? iTabColumns : (iTabColumns - iRemainder);
}

static int diagNewNext(const char* sText, int iLength, int iOffset, uint32_t* pCodepoint)
{
	unsigned char c0;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;
	uint32_t iCodepoint;
	int iStep;

	++newCalls;
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

static int diagNewColumns(uint32_t iCodepoint)
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

static void diagNewVisible(diag_new_data* pData, const char* sText, int iLineStart, int iLineEnd, float fColumnWidth, float fViewportWidth, int* pStart, int* pEnd)
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
	iTabColumns = diagNewTabColumns(pData);
	bOwnRead = pData->pDocument != NULL && sText == pData->sReadBuffer &&
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
		for ( i = iLineStart; i < iLineEnd; ) {
			if ( sText[i] == '\t' ) {
				iNextColumn = iColumn + diagNewTabAdvance(pData, iColumn);
				i++;
			} else {
				i = diagNewNext(sText, iLineEnd, i, &iCodepoint);
				iNextColumn = iColumn + diagNewColumns(iCodepoint);
			}
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

static int diagNewReserve(char** psText, int* pCapacity, int iCapacity)
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

static int diagNewRead(diag_new_data* pData,
	int iStart, int iEnd, const char** psText)
{
	uint32_t iVersion;
	int iLength;
	int iRet;

	if ( pData == NULL || pData->pDocument == NULL || psText == NULL ||
	     iStart < 0 || iEnd < iStart ) return XUI_ERROR_INVALID_ARGUMENT;
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
	if ( pData->sReadBuffer != NULL && pData->iReadVersion == iVersion &&
	     iStart == pData->iReadRangeStart && iEnd == pData->iReadRangeEnd ) {
		*psText = pData->sReadBuffer;
		return XUI_OK;
	}
	iLength = iEnd - iStart;
	/* CopyRange can replace bytes in the same allocation, even on failure. */
	pData->bVisibleRangeCached = 0;
	iRet = diagNewReserve(&pData->sReadBuffer,
		&pData->iReadBufferCapacity, iLength + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentCopyRange(pData->pDocument, iStart, iEnd,
		pData->sReadBuffer, pData->iReadBufferCapacity, NULL);
	if ( iRet != XUI_OK ) return iRet;
	pData->iReadRangeStart = iStart;
	pData->iReadRangeEnd = iEnd;
	pData->iReadVersion = iVersion;
	*psText = pData->sReadBuffer;
	return XUI_OK;
}
