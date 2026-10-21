static int diagRowOldVisualColumn(const xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iOffset)
{
	int i;
	int iColumn;
	uint32_t iCodepoint;

	if ( sText == NULL || iOffset <= iLineStart ) return 0;
	iColumn = 0;
	i = iLineStart;
	while ( i < iOffset ) {
		if ( sText[i] == '\t' ) {
			iColumn += __xuiCodeEditTabAdvance(pData, iColumn);
			i++;
		} else {
			i = __xuiCodeEditUtf8Next(sText, iOffset, i, &iCodepoint);
			iColumn += __xuiCodeEditCodepointColumns(iCodepoint);
		}
	}
	return iColumn;
}


static void diagRowOldVisibleByteRange(xui_code_edit_data_t* pData, const char* sText, int iLineStart, int iLineEnd, float fColumnWidth, float fViewportWidth, int* pStart, int* pEnd)
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
				iNextColumn = iColumn + __xuiCodeEditTabAdvance(pData, iColumn);
				i++;
			} else {
				i = __xuiCodeEditUtf8Next(sText, iLineEnd, i, &iCodepoint);
				iNextColumn = iColumn + __xuiCodeEditCodepointColumns(iCodepoint);
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

