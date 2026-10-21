static xui_code_edit_line_layout_t* diagPreviousLineLayoutEnsure(xui_proxy pProxy, xui_font pFont,
	xui_code_edit_data_t* pData, const char* sText, int iTextSize, float fColumnWidth)
{
	xui_code_edit_line_layout_t* pLayout;
	uint32_t iVersion;
	float fTabWidth;
	float fX;
	int i;
	int iStart;
	int iRet;
	int iSlot;
	int iOldest;

	if ( pData == NULL || pData->pDocument == NULL || pProxy == NULL || pFont == NULL || sText == NULL ) return NULL;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	iVersion = xuiCodeDocumentGetVersion(pData->pDocument);
    /* Layouts from a previous document revision can never pass the cache key.
       Release them before shaping the new revision, rather than retaining them
       until all 128 slots fill (and repeatedly falling back on allocation failure). */
    if ( pData->iLineLayoutCount > 0 &&
         pData->pLineLayouts[0].iDocumentVersion != iVersion ) {
        __xuiCodeEditLineLayoutsClear(pData);
    }
	for ( i = 0; i < pData->iLineLayoutCount; i++ ) {
		pLayout = &pData->pLineLayouts[i];
		if ( pLayout->iDocumentVersion == iVersion && pLayout->pFont == pFont &&
		     pLayout->iTabColumns == pData->iTabColumns && pLayout->iTextSize == iTextSize &&
		     pLayout->sText != NULL && memcmp(pLayout->sText, sText, (size_t)iTextSize) == 0 ) {
			pLayout->iUseStamp = ++pData->iLineLayoutUseStamp;
			return pLayout;
		}
	}
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
	pLayout = &pData->pLineLayouts[iSlot];
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
			fX = (floorf(fX / fTabWidth) + 1.0f) * fTabWidth;
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
		iRet = __xuiCodeEditShapeText(pProxy, pFont, pLayout->sText + iStart, i - iStart,
			&pLayout->pSegments[pLayout->iSegmentCount].tShape);
		if ( iRet != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
		for ( iStart = 0; iStart < pLayout->pSegments[pLayout->iSegmentCount].tShape.iClusterCount; iStart++ ) {
			xui_text_cluster_t* pCluster = &pLayout->pSegments[pLayout->iSegmentCount].tShape.pClusters[iStart];
			float fEnd = fX + pCluster->fAdvance;
			iRet = __xuiCodeEditLineLayoutAddStop(pLayout, pLayout->pSegments[pLayout->iSegmentCount].iStart + pCluster->iTextStart, fX);
			if ( iRet == XUI_OK ) iRet = __xuiCodeEditLineLayoutAddStop(pLayout, pLayout->pSegments[pLayout->iSegmentCount].iStart + pCluster->iTextEnd, fEnd);
			if ( iRet != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
			fX = fEnd;
		}
		pLayout->iSegmentCount++;
	}
	pLayout->fWidth = fX;
	if ( __xuiCodeEditLineLayoutAddStop(pLayout, iTextSize, fX) != XUI_OK ) { __xuiCodeEditLineLayoutClear(pLayout); return NULL; }
	return pLayout;
}

