#include "../xui.h"

#include <string.h>

static int __xuiCodeLayoutTabColumns(const xui_code_layout_desc_t* pDesc)
{
	return (pDesc != NULL && pDesc->iTabColumns > 0) ? pDesc->iTabColumns : 4;
}

static int __xuiCodeLayoutNextVisualColumn(int iVisualColumn, char c, int iTabColumns)
{
	int iStep;

	if ( c != '\t' ) return iVisualColumn + 1;
	iStep = iTabColumns - (iVisualColumn % iTabColumns);
	return iVisualColumn + ((iStep > 0) ? iStep : iTabColumns);
}

static int __xuiCodeLayoutLineVisualColumns(xui_code_document pDocument, int iLine, int iTabColumns)
{
	int iStart;
	int iEnd;
	int i;
	int iColumn;
	char ch;

	if ( xuiCodeDocumentGetLineRange(pDocument, iLine, &iStart, &iEnd) != XUI_OK ) return 0;
	iColumn = 0;
	for ( i = iStart; i < iEnd; i++ ) {
		if ( xuiCodeDocumentGetByte(pDocument, i, &ch) != XUI_OK ) break;
		iColumn = __xuiCodeLayoutNextVisualColumn(iColumn, ch, iTabColumns);
	}
	return iColumn;
}

static float __xuiCodeLayoutLineWidth(xui_code_document pDocument, int iLine, float fCharWidth, int iTabColumns)
{
	return (float)__xuiCodeLayoutLineVisualColumns(pDocument, iLine, iTabColumns) * fCharWidth;
}

static int __xuiCodeLayoutColumnToVisual(xui_code_document pDocument, int iLine, int iColumn, int iTabColumns)
{
	int iStart;
	int iEnd;
	int i;
	int iLimit;
	int iVisual;
	char ch;

	if ( xuiCodeDocumentGetLineRange(pDocument, iLine, &iStart, &iEnd) != XUI_OK ) return 0;
	if ( iColumn < 0 ) iColumn = 0;
	iLimit = iStart + iColumn;
	if ( iLimit > iEnd ) iLimit = iEnd;
	iVisual = 0;
	for ( i = iStart; i < iLimit; i++ ) {
		if ( xuiCodeDocumentGetByte(pDocument, i, &ch) != XUI_OK ) break;
		iVisual = __xuiCodeLayoutNextVisualColumn(iVisual, ch, iTabColumns);
	}
	return iVisual;
}

static int __xuiCodeLayoutVisualToColumn(xui_code_document pDocument, int iLine, int iVisualColumn, int iTabColumns)
{
	int iStart;
	int iEnd;
	int i;
	int iColumn;
	int iVisual;
	int iNext;
	char ch;

	if ( xuiCodeDocumentGetLineRange(pDocument, iLine, &iStart, &iEnd) != XUI_OK ) return 0;
	if ( iVisualColumn <= 0 ) return 0;
	iVisual = 0;
	iColumn = 0;
	for ( i = iStart; i < iEnd; i++ ) {
		if ( xuiCodeDocumentGetByte(pDocument, i, &ch) != XUI_OK ) break;
		iNext = __xuiCodeLayoutNextVisualColumn(iVisual, ch, iTabColumns);
		if ( iVisualColumn <= iNext ) return iColumn + ((iVisualColumn - iVisual > iNext - iVisualColumn) ? 1 : 0);
		iVisual = iNext;
		iColumn++;
	}
	return iColumn;
}

static int __xuiCodeLayoutVisibleLines(const xui_code_layout_desc_t* pDesc, int** ppVisible, int* pCount)
{
	int iLineCount;
	int* pVisible;
	int iRet;

	if ( (pDesc == NULL) || (pDesc->pDocument == NULL) || (ppVisible == NULL) || (pCount == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLineCount = xuiCodeDocumentGetLineCount(pDesc->pDocument);
	pVisible = (int*)xrtMalloc(sizeof(*pVisible) * (size_t)((iLineCount > 0) ? iLineCount : 1));
	if ( pVisible == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiCodeFoldBuildVisibleLines(iLineCount, pDesc->pFoldRanges, pDesc->iFoldRangeCount, pVisible, iLineCount, pCount);
	if ( iRet != XUI_OK ) {
		xrtFree(pVisible);
		return iRet;
	}
	*ppVisible = pVisible;
	return XUI_OK;
}

static int __xuiCodeLayoutFindVisibleIndex(const int* pVisible, int iCount, int iLine)
{
	int i;

	if ( pVisible == NULL ) return -1;
	for ( i = 0; i < iCount; i++ ) {
		if ( pVisible[i] == iLine ) return i;
	}
	return -1;
}

XUI_API int xuiCodeLayoutBuildVisibleLines(const xui_code_layout_desc_t* pDesc, xui_code_layout_line_t* pLines, int iLineCapacity, int* pLineCount, xui_vec2_t* pContentSize, xui_rect_t* pTextRect)
{
	int* pVisible;
	int iVisibleCount;
	int i;
	int iOut;
	int iStart;
	int iEnd;
	int iRet;
	float fLineHeight;
	float fCharWidth;
	float fContentWidth;
	float fY;
	int iTabColumns;
	xui_rect_t tTextRect;

	if ( (pDesc == NULL) || (pDesc->pDocument == NULL) || (pLineCount == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pLineCount = 0;
	fLineHeight = (pDesc->fLineHeight > 0.0f) ? pDesc->fLineHeight : 18.0f;
	fCharWidth = (pDesc->fCharWidth > 0.0f) ? pDesc->fCharWidth : 8.0f;
	iTabColumns = __xuiCodeLayoutTabColumns(pDesc);
	tTextRect = pDesc->tViewportRect;
	tTextRect.fX += pDesc->fMarginWidth;
	tTextRect.fW -= pDesc->fMarginWidth;
	if ( tTextRect.fW < 0.0f ) tTextRect.fW = 0.0f;
	iRet = __xuiCodeLayoutVisibleLines(pDesc, &pVisible, &iVisibleCount);
	if ( iRet != XUI_OK ) return iRet;
	fContentWidth = 0.0f;
	for ( i = 0; i < iVisibleCount; i++ ) {
		float fWidth = __xuiCodeLayoutLineWidth(pDesc->pDocument, pVisible[i], fCharWidth, iTabColumns);
		if ( fWidth > fContentWidth ) fContentWidth = fWidth;
	}
	iOut = 0;
	for ( i = 0; i < iVisibleCount; i++ ) {
		fY = pDesc->tViewportRect.fY + (float)i * fLineHeight - pDesc->fScrollY;
		if ( fY + fLineHeight < pDesc->tViewportRect.fY ) continue;
		if ( fY > pDesc->tViewportRect.fY + pDesc->tViewportRect.fH ) continue;
		if ( pLines != NULL && iOut < iLineCapacity ) {
			(void)xuiCodeDocumentGetLineRange(pDesc->pDocument, pVisible[i], &iStart, &iEnd);
			memset(&pLines[iOut], 0, sizeof(pLines[iOut]));
			pLines[iOut].iSize = sizeof(pLines[iOut]);
			pLines[iOut].iLine = pVisible[i];
			pLines[iOut].iVisibleIndex = i;
			pLines[iOut].iStartOffset = iStart;
			pLines[iOut].iEndOffset = iEnd;
			pLines[iOut].tRect = (xui_rect_t){
				tTextRect.fX - pDesc->fScrollX,
				fY,
				(fContentWidth > tTextRect.fW) ? fContentWidth : tTextRect.fW,
				fLineHeight
			};
		}
		iOut++;
	}
	*pLineCount = iOut;
	if ( pContentSize != NULL ) {
		pContentSize->fX = fContentWidth;
		pContentSize->fY = (float)iVisibleCount * fLineHeight;
	}
	if ( pTextRect != NULL ) *pTextRect = tTextRect;
	xrtFree(pVisible);
	return XUI_OK;
}

XUI_API int xuiCodeLayoutHitTest(const xui_code_layout_desc_t* pDesc, float fX, float fY, xui_code_hit_t* pHit)
{
	int* pVisible;
	int iVisibleCount;
	int iIndex;
	int iLine;
	int iColumn;
	int iOffset;
	int iRet;
	float fLineHeight;
	float fCharWidth;
	int iTabColumns;
	xui_rect_t tTextRect;

	if ( (pDesc == NULL) || (pDesc->pDocument == NULL) || (pHit == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pHit, 0, sizeof(*pHit));
	pHit->iSize = sizeof(*pHit);
	fLineHeight = (pDesc->fLineHeight > 0.0f) ? pDesc->fLineHeight : 18.0f;
	fCharWidth = (pDesc->fCharWidth > 0.0f) ? pDesc->fCharWidth : 8.0f;
	iTabColumns = __xuiCodeLayoutTabColumns(pDesc);
	iRet = __xuiCodeLayoutVisibleLines(pDesc, &pVisible, &iVisibleCount);
	if ( iRet != XUI_OK ) return iRet;
	iIndex = (int)((fY - pDesc->tViewportRect.fY + pDesc->fScrollY) / fLineHeight);
	if ( iIndex < 0 ) iIndex = 0;
	if ( iIndex >= iVisibleCount ) iIndex = iVisibleCount - 1;
	if ( iIndex < 0 ) {
		xrtFree(pVisible);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLine = pVisible[iIndex];
	tTextRect = pDesc->tViewportRect;
	tTextRect.fX += pDesc->fMarginWidth;
	tTextRect.fW -= pDesc->fMarginWidth;
	iColumn = __xuiCodeLayoutVisualToColumn(pDesc->pDocument, iLine, (int)((fX - tTextRect.fX + pDesc->fScrollX) / fCharWidth + 0.5f), iTabColumns);
	(void)xuiCodeDocumentLineColumnToOffset(pDesc->pDocument, iLine, iColumn, &iOffset);
	pHit->iPart = (fX < tTextRect.fX) ? 1 : 2;
	pHit->iLine = iLine;
	pHit->iColumn = iColumn;
	pHit->iOffset = iOffset;
	pHit->tLineRect = (xui_rect_t){tTextRect.fX - pDesc->fScrollX, pDesc->tViewportRect.fY + (float)iIndex * fLineHeight - pDesc->fScrollY, tTextRect.fW, fLineHeight};
	xrtFree(pVisible);
	return XUI_OK;
}

XUI_API int xuiCodeLayoutGetCaretRect(const xui_code_layout_desc_t* pDesc, int iLine, int iColumn, xui_rect_t* pRect)
{
	int* pVisible;
	int iVisibleCount;
	int iIndex;
	int iRet;
	float fLineHeight;
	float fCharWidth;
	int iVisualColumn;
	int iTabColumns;
	xui_rect_t tTextRect;

	if ( (pDesc == NULL) || (pDesc->pDocument == NULL) || (pRect == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiCodeLayoutVisibleLines(pDesc, &pVisible, &iVisibleCount);
	if ( iRet != XUI_OK ) return iRet;
	iIndex = __xuiCodeLayoutFindVisibleIndex(pVisible, iVisibleCount, iLine);
	xrtFree(pVisible);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iColumn < 0 ) iColumn = 0;
	fLineHeight = (pDesc->fLineHeight > 0.0f) ? pDesc->fLineHeight : 18.0f;
	fCharWidth = (pDesc->fCharWidth > 0.0f) ? pDesc->fCharWidth : 8.0f;
	iTabColumns = __xuiCodeLayoutTabColumns(pDesc);
	iVisualColumn = __xuiCodeLayoutColumnToVisual(pDesc->pDocument, iLine, iColumn, iTabColumns);
	tTextRect = pDesc->tViewportRect;
	tTextRect.fX += pDesc->fMarginWidth;
	*pRect = (xui_rect_t){
		tTextRect.fX + (float)iVisualColumn * fCharWidth - pDesc->fScrollX,
		pDesc->tViewportRect.fY + (float)iIndex * fLineHeight - pDesc->fScrollY,
		1.0f,
		fLineHeight
	};
	return XUI_OK;
}
