#include "../xui.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct xui_code_fold_state_t {
	xui_code_fold_range_t* pRanges;
	int iRangeCount;
	int iRangeCapacity;
	xui_code_range_t* pHiddenRanges;
	int iHiddenRangeCount;
	int iHiddenRangeCapacity;
	int iHiddenTotal;
	int bHiddenDirty;
};

static int __xuiCodeFoldStateReserve(xui_code_fold_state pState, int iCapacity)
{
	xui_code_fold_range_t* pNew;

	if ( iCapacity <= pState->iRangeCapacity ) return XUI_OK;
	if ( iCapacity < pState->iRangeCapacity * 2 ) iCapacity = pState->iRangeCapacity * 2;
	if ( iCapacity < 16 ) iCapacity = 16;
	pNew = (xui_code_fold_range_t*)xrtRealloc(pState->pRanges, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pState->pRanges = pNew;
	pState->iRangeCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeFoldStateReserveHidden(xui_code_fold_state pState, int iCapacity)
{
	xui_code_range_t* pNew;

	if ( iCapacity <= pState->iHiddenRangeCapacity ) return XUI_OK;
	if ( iCapacity < pState->iHiddenRangeCapacity * 2 ) iCapacity = pState->iHiddenRangeCapacity * 2;
	if ( iCapacity < 16 ) iCapacity = 16;
	pNew = (xui_code_range_t*)xrtRealloc(pState->pHiddenRanges, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pState->pHiddenRanges = pNew;
	pState->iHiddenRangeCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeFoldStateRangeValid(const xui_code_fold_range_t* pRange)
{
	return pRange != NULL && pRange->iStartLine >= 0 && pRange->iEndLine > pRange->iStartLine;
}

static void __xuiCodeFoldStateCopyOut(xui_code_fold_range_t* pOut, const xui_code_fold_range_t* pRange)
{
	*pOut = *pRange;
	pOut->iSize = sizeof(*pOut);
}

static int __xuiCodeFoldStateFindHeader(xui_code_fold_state pState, int iLine)
{
	int i;

	for ( i = 0; i < pState->iRangeCount; i++ ) {
		if ( pState->pRanges[i].iStartLine == iLine ) return i;
	}
	for ( i = 0; i < pState->iRangeCount; i++ ) {
		if ( iLine > pState->pRanges[i].iStartLine && iLine <= pState->pRanges[i].iEndLine ) return i;
	}
	return -1;
}

static int __xuiCodeFoldStateCompareHidden(const void* pA, const void* pB)
{
	const xui_code_range_t* pLeft = (const xui_code_range_t*)pA;
	const xui_code_range_t* pRight = (const xui_code_range_t*)pB;

	if ( pLeft->iStart < pRight->iStart ) return -1;
	if ( pLeft->iStart > pRight->iStart ) return 1;
	if ( pLeft->iEnd < pRight->iEnd ) return -1;
	if ( pLeft->iEnd > pRight->iEnd ) return 1;
	return 0;
}

static int __xuiCodeFoldStateBuildHidden(xui_code_fold_state pState)
{
	xui_code_range_t* pRange;
	int i;
	int iOut;
	int iRet;
	int iStart;
	int iEnd;
	int bMerge;

	if ( pState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pState->bHiddenDirty ) return XUI_OK;
	pState->iHiddenRangeCount = 0;
	pState->iHiddenTotal = 0;
	for ( i = 0; i < pState->iRangeCount; i++ ) {
		if ( (pState->pRanges[i].iFlags & XUI_CODE_FOLD_COLLAPSED) == 0 ) continue;
		iStart = pState->pRanges[i].iStartLine + 1;
		iEnd = pState->pRanges[i].iEndLine;
		if ( iEnd < iStart ) continue;
		iRet = __xuiCodeFoldStateReserveHidden(pState, pState->iHiddenRangeCount + 1);
		if ( iRet != XUI_OK ) return iRet;
		pRange = &pState->pHiddenRanges[pState->iHiddenRangeCount++];
		pRange->iStart = iStart;
		pRange->iEnd = iEnd;
	}
	if ( pState->iHiddenRangeCount > 1 ) {
		qsort(pState->pHiddenRanges, (size_t)pState->iHiddenRangeCount, sizeof(pState->pHiddenRanges[0]), __xuiCodeFoldStateCompareHidden);
		iOut = 0;
		for ( i = 0; i < pState->iHiddenRangeCount; i++ ) {
			if ( iOut == 0 ) {
				pState->pHiddenRanges[iOut++] = pState->pHiddenRanges[i];
				continue;
			}
			pRange = &pState->pHiddenRanges[iOut - 1];
			bMerge = (pState->pHiddenRanges[i].iStart <= pRange->iEnd);
			if ( !bMerge && pRange->iEnd < INT_MAX && pState->pHiddenRanges[i].iStart == pRange->iEnd + 1 ) bMerge = 1;
			if ( bMerge ) {
				if ( pState->pHiddenRanges[i].iEnd > pRange->iEnd ) pRange->iEnd = pState->pHiddenRanges[i].iEnd;
			} else {
				pState->pHiddenRanges[iOut++] = pState->pHiddenRanges[i];
			}
		}
		pState->iHiddenRangeCount = iOut;
	}
	for ( i = 0; i < pState->iHiddenRangeCount; i++ ) {
		pState->iHiddenTotal += pState->pHiddenRanges[i].iEnd - pState->pHiddenRanges[i].iStart + 1;
	}
	pState->bHiddenDirty = 0;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateCreate(xui_code_fold_state* ppState)
{
	xui_code_fold_state pState;

	if ( ppState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppState = NULL;
	pState = (xui_code_fold_state)xrtMalloc(sizeof(*pState));
	if ( pState == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pState, 0, sizeof(*pState));
	*ppState = pState;
	return XUI_OK;
}

XUI_API void xuiCodeFoldStateDestroy(xui_code_fold_state pState)
{
	if ( pState == NULL ) return;
	xrtFree(pState->pRanges);
	xrtFree(pState->pHiddenRanges);
	xrtFree(pState);
}

XUI_API void xuiCodeFoldStateClear(xui_code_fold_state pState)
{
	if ( pState == NULL ) return;
	pState->iRangeCount = 0;
	pState->iHiddenRangeCount = 0;
	pState->iHiddenTotal = 0;
	pState->bHiddenDirty = 0;
}

XUI_API int xuiCodeFoldStateSetRanges(xui_code_fold_state pState, const xui_code_fold_range_t* pRanges, int iRangeCount)
{
	int i;
	int iRet;

	if ( (pState == NULL) || (iRangeCount < 0) || (iRangeCount > 0 && pRanges == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < iRangeCount; i++ ) {
		if ( !__xuiCodeFoldStateRangeValid(&pRanges[i]) ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiCodeFoldStateReserve(pState, iRangeCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iRangeCount; i++ ) {
		pState->pRanges[i] = pRanges[i];
		pState->pRanges[i].iSize = sizeof(pState->pRanges[i]);
		pState->pRanges[i].iFlags |= XUI_CODE_FOLD_HEADER;
	}
	pState->iRangeCount = iRangeCount;
	pState->bHiddenDirty = 1;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateBuildFromProvider(xui_code_fold_state pState, xui_code_document pDocument, xui_code_fold_proc onFold, void* pUser)
{
	xui_code_fold_range_t* pRanges;
	int iCount;
	int iRet;

	if ( (pState == NULL) || (pDocument == NULL) || (onFold == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = onFold(pDocument, NULL, 0, &iCount, pUser);
	if ( iRet != XUI_OK ) return iRet;
	pRanges = NULL;
	if ( iCount > 0 ) {
		pRanges = (xui_code_fold_range_t*)xrtMalloc(sizeof(*pRanges) * (size_t)iCount);
		if ( pRanges == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		iRet = onFold(pDocument, pRanges, iCount, &iCount, pUser);
		if ( iRet != XUI_OK ) {
			xrtFree(pRanges);
			return iRet;
		}
	}
	iRet = xuiCodeFoldStateSetRanges(pState, pRanges, iCount);
	xrtFree(pRanges);
	return iRet;
}

XUI_API int xuiCodeFoldStateGetCount(xui_code_fold_state pState)
{
	return (pState != NULL) ? pState->iRangeCount : 0;
}

XUI_API int xuiCodeFoldStateGetRange(xui_code_fold_state pState, int iIndex, xui_code_fold_range_t* pRange)
{
	if ( (pState == NULL) || (pRange == NULL) || (iIndex < 0) || (iIndex >= pState->iRangeCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCodeFoldStateCopyOut(pRange, &pState->pRanges[iIndex]);
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateGetRanges(xui_code_fold_state pState, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	int i;

	if ( (pState == NULL) || (pRangeCount == NULL) || (iRangeCapacity < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pState->iRangeCount; i++ ) {
		if ( pRanges != NULL && i < iRangeCapacity ) __xuiCodeFoldStateCopyOut(&pRanges[i], &pState->pRanges[i]);
	}
	*pRangeCount = pState->iRangeCount;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateToggleLine(xui_code_fold_state pState, int iLine)
{
	int iIndex;

	if ( (pState == NULL) || (iLine < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeFoldStateFindHeader(pState, iLine);
	if ( iIndex < 0 ) return XUI_ERROR_UNSUPPORTED;
	pState->pRanges[iIndex].iFlags ^= XUI_CODE_FOLD_COLLAPSED;
	pState->bHiddenDirty = 1;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateFoldAll(xui_code_fold_state pState)
{
	int i;

	if ( pState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pState->iRangeCount; i++ ) pState->pRanges[i].iFlags |= XUI_CODE_FOLD_COLLAPSED;
	pState->bHiddenDirty = 1;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateUnfoldAll(xui_code_fold_state pState)
{
	int i;

	if ( pState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pState->iRangeCount; i++ ) pState->pRanges[i].iFlags &= ~XUI_CODE_FOLD_COLLAPSED;
	pState->bHiddenDirty = 1;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateIsLineVisible(xui_code_fold_state pState, int iLine, int* pVisible)
{
	int iLow;
	int iHigh;
	int iMid;
	int iRet;

	if ( (pState == NULL) || (iLine < 0) || (pVisible == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pVisible = 1;
	iRet = __xuiCodeFoldStateBuildHidden(pState);
	if ( iRet != XUI_OK ) return iRet;
	iLow = 0;
	iHigh = pState->iHiddenRangeCount - 1;
	while ( iLow <= iHigh ) {
		iMid = (iLow + iHigh) / 2;
		if ( iLine < pState->pHiddenRanges[iMid].iStart ) {
			iHigh = iMid - 1;
		} else if ( iLine > pState->pHiddenRanges[iMid].iEnd ) {
			iLow = iMid + 1;
		} else {
			*pVisible = 0;
			return XUI_OK;
		}
	}
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateGetVisibleLineCount(xui_code_fold_state pState, int iLineCount, int* pVisibleCount)
{
	int i;
	int iStart;
	int iEnd;
	int iHidden;
	int iRet;

	if ( (pState == NULL) || (iLineCount < 0) || (pVisibleCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeFoldStateBuildHidden(pState);
	if ( iRet != XUI_OK ) return iRet;
	iHidden = 0;
	for ( i = 0; i < pState->iHiddenRangeCount; i++ ) {
		iStart = pState->pHiddenRanges[i].iStart;
		iEnd = pState->pHiddenRanges[i].iEnd;
		if ( iStart >= iLineCount ) break;
		if ( iEnd >= iLineCount ) iEnd = iLineCount - 1;
		if ( iEnd >= iStart ) iHidden += iEnd - iStart + 1;
	}
	*pVisibleCount = iLineCount - iHidden;
	if ( *pVisibleCount < 0 ) *pVisibleCount = 0;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateLineToVisibleRow(xui_code_fold_state pState, int iLine, int* pRow)
{
	int i;
	int iHiddenBefore;
	int iRet;

	if ( (pState == NULL) || (iLine < 0) || (pRow == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeFoldStateBuildHidden(pState);
	if ( iRet != XUI_OK ) return iRet;
	iHiddenBefore = 0;
	for ( i = 0; i < pState->iHiddenRangeCount; i++ ) {
		if ( pState->pHiddenRanges[i].iStart >= iLine ) break;
		if ( pState->pHiddenRanges[i].iEnd < iLine ) {
			iHiddenBefore += pState->pHiddenRanges[i].iEnd - pState->pHiddenRanges[i].iStart + 1;
		} else {
			iHiddenBefore += iLine - pState->pHiddenRanges[i].iStart;
			break;
		}
	}
	*pRow = iLine - iHiddenBefore;
	if ( *pRow < 0 ) *pRow = 0;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateVisibleRowToLine(xui_code_fold_state pState, int iLineCount, int iRow, int* pLine)
{
	int i;
	int iCurrentLine;
	int iRemaining;
	int iVisibleSpan;
	int iRet;

	if ( (pState == NULL) || (iLineCount < 0) || (pLine == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iRow < 0 ) iRow = 0;
	if ( iLineCount <= 0 ) {
		*pLine = 0;
		return XUI_OK;
	}
	iRet = __xuiCodeFoldStateBuildHidden(pState);
	if ( iRet != XUI_OK ) return iRet;
	iCurrentLine = 0;
	iRemaining = iRow;
	for ( i = 0; i < pState->iHiddenRangeCount; i++ ) {
		if ( pState->pHiddenRanges[i].iEnd < iCurrentLine ) continue;
		if ( pState->pHiddenRanges[i].iStart >= iLineCount ) break;
		iVisibleSpan = pState->pHiddenRanges[i].iStart - iCurrentLine;
		if ( iVisibleSpan < 0 ) iVisibleSpan = 0;
		if ( iRemaining < iVisibleSpan ) {
			*pLine = iCurrentLine + iRemaining;
			return XUI_OK;
		}
		iRemaining -= iVisibleSpan;
		iCurrentLine = pState->pHiddenRanges[i].iEnd + 1;
		if ( iCurrentLine < 0 || iCurrentLine > iLineCount ) {
			iCurrentLine = iLineCount;
			break;
		}
	}
	*pLine = iCurrentLine + iRemaining;
	if ( *pLine >= iLineCount ) *pLine = iLineCount - 1;
	return XUI_OK;
}

XUI_API int xuiCodeFoldStateBuildVisibleLines(xui_code_fold_state pState, int iLineCount, int* pVisibleLines, int iVisibleCapacity, int* pVisibleCount)
{
	int i;
	int iLine;
	int iCount;
	int iRet;

	if ( (pState == NULL) || (iLineCount < 0) || (iVisibleCapacity < 0) || (pVisibleCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeFoldStateGetVisibleLineCount(pState, iLineCount, &iCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iCount; i++ ) {
		if ( pVisibleLines != NULL && i < iVisibleCapacity ) {
			iRet = xuiCodeFoldStateVisibleRowToLine(pState, iLineCount, i, &iLine);
			if ( iRet != XUI_OK ) return iRet;
			pVisibleLines[i] = iLine;
		}
	}
	*pVisibleCount = iCount;
	return XUI_OK;
}
