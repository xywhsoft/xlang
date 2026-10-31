#include "xui_config.h"
#if XUI_ENABLE_CODE_EDIT
#include "xui.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct xui_code_annotation_index_t {
	int iStart;
	int iEnd;
	int iIndex;
	int iPrefixMaxEnd;
} xui_code_annotation_index_t;

struct xui_code_annotation_store_t {
	xui_code_marker_t* pMarkers;
	int iMarkerCount;
	int iMarkerCapacity;
	xui_code_indicator_t* pIndicators;
	int iIndicatorCount;
	int iIndicatorCapacity;
	xui_code_annotation_index_t* pBackgroundIndex;
	int iBackgroundCount;
	int iBackgroundIndexCapacity;
	int bBackgroundIndexDirty;
	xui_code_diagnostic_t* pDiagnostics;
	int iDiagnosticCount;
	int iDiagnosticCapacity;
	xui_code_annotation_index_t* pDiagnosticIndex;
	int iDiagnosticIndexCapacity;
	uint32_t iDiagnosticVersion;
};

static char* __xuiCodeAnnotationStrDup(const char* sText)
{
	char* sCopy;
	size_t iLength;

	if ( sText == NULL ) return NULL;
	iLength = strlen(sText);
	sCopy = (char*)xrtMalloc(iLength + 1u);
	if ( sCopy == NULL ) return NULL;
	memcpy(sCopy, sText, iLength + 1u);
	return sCopy;
}

static void __xuiCodeAnnotationFreeMarker(xui_code_marker_t* pMarker)
{
	if ( pMarker == NULL ) return;
	xrtFree((void*)pMarker->sTooltip);
	memset(pMarker, 0, sizeof(*pMarker));
}

static void __xuiCodeAnnotationFreeDiagnostic(xui_code_diagnostic_t* pDiagnostic)
{
	if ( pDiagnostic == NULL ) return;
	xrtFree((void*)pDiagnostic->sCode);
	xrtFree((void*)pDiagnostic->sMessage);
	xrtFree((void*)pDiagnostic->sSource);
	memset(pDiagnostic, 0, sizeof(*pDiagnostic));
}

static int __xuiCodeAnnotationNextCapacity(int iCurrent, int iRequired,
	size_t iItemSize, int* pCapacity)
{
	int iCapacity;

	if ( iCurrent < 0 || iRequired < 0 || iItemSize == 0 || pCapacity == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iRequired <= iCurrent ) {
		*pCapacity = iCurrent;
		return XUI_OK;
	}
	iCapacity = (iCurrent > 0) ? iCurrent : 16;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > INT_MAX / 2 ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > SIZE_MAX / iItemSize ) return XUI_ERROR_LIMIT_EXCEEDED;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeAnnotationReserveMarkers(xui_code_annotation_store pStore, int iCapacity)
{
	xui_code_marker_t* pNew;
	int iNewCapacity;
	int iRet;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pStore->iMarkerCapacity ) return XUI_OK;
	iRet = __xuiCodeAnnotationNextCapacity(pStore->iMarkerCapacity, iCapacity,
		sizeof(*pNew), &iNewCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pNew = (xui_code_marker_t*)xrtRealloc(pStore->pMarkers,
		sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pStore->pMarkers = pNew;
	pStore->iMarkerCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeAnnotationReserveIndicators(xui_code_annotation_store pStore, int iCapacity)
{
	xui_code_indicator_t* pNew;
	int iNewCapacity;
	int iRet;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pStore->iIndicatorCapacity ) return XUI_OK;
	iRet = __xuiCodeAnnotationNextCapacity(pStore->iIndicatorCapacity, iCapacity,
		sizeof(*pNew), &iNewCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pNew = (xui_code_indicator_t*)xrtRealloc(pStore->pIndicators,
		sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pStore->pIndicators = pNew;
	pStore->iIndicatorCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeAnnotationReserveDiagnostics(xui_code_annotation_store pStore, int iCapacity)
{
	xui_code_diagnostic_t* pNew;
	int iNewCapacity;
	int iRet;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pStore->iDiagnosticCapacity ) return XUI_OK;
	iRet = __xuiCodeAnnotationNextCapacity(pStore->iDiagnosticCapacity, iCapacity,
		sizeof(*pNew), &iNewCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pNew = (xui_code_diagnostic_t*)xrtRealloc(pStore->pDiagnostics,
		sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pStore->pDiagnostics = pNew;
	pStore->iDiagnosticCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeAnnotationReserveDiagnosticIndex(xui_code_annotation_store pStore, int iCapacity)
{
	xui_code_annotation_index_t* pNew;
	int iNewCapacity;
	int iRet;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pStore->iDiagnosticIndexCapacity ) return XUI_OK;
	iRet = __xuiCodeAnnotationNextCapacity(pStore->iDiagnosticIndexCapacity,
		iCapacity, sizeof(*pNew), &iNewCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pNew = (xui_code_annotation_index_t*)xrtRealloc(pStore->pDiagnosticIndex,
		sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pStore->pDiagnosticIndex = pNew;
	pStore->iDiagnosticIndexCapacity = iNewCapacity;
	return XUI_OK;
}

/* Background indices reuse the diagnostic range ordering/prefix scheme.
 * Append-sorted inputs update the index without sorting on the render path. */
static int __xuiCodeAnnotationIsBackground(const xui_code_indicator_t* pItem)
{
    return pItem->iStyle == XUI_CODE_INDICATOR_BACKGROUND &&
        pItem->iIndicator != XUI_CODE_INDICATOR_SEARCH_RESULT;
}
static int __xuiCodeAnnotationReserveBackgroundIndex(xui_code_annotation_store pStore, int iCapacity)
{
    xui_code_annotation_index_t* pNew;
    int iNewCapacity, iRet;
    if ( iCapacity <= pStore->iBackgroundIndexCapacity ) return XUI_OK;
    iRet = __xuiCodeAnnotationNextCapacity(pStore->iBackgroundIndexCapacity, iCapacity,
        sizeof(*pNew), &iNewCapacity);
    if ( iRet != XUI_OK ) return iRet;
    pNew = (xui_code_annotation_index_t*)xrtRealloc(pStore->pBackgroundIndex, sizeof(*pNew) * (size_t)iNewCapacity);
    if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
    pStore->pBackgroundIndex = pNew;
    pStore->iBackgroundIndexCapacity = iNewCapacity;
    return XUI_OK;
}
static void __xuiCodeAnnotationBackgroundCompacted(xui_code_annotation_store pStore)
{
    int i;
    pStore->iBackgroundCount = 0;
    for ( i = 0; i < pStore->iIndicatorCount; ++i ) {
        if ( __xuiCodeAnnotationIsBackground(&pStore->pIndicators[i]) ) ++pStore->iBackgroundCount;
    }
    pStore->bBackgroundIndexDirty = pStore->iBackgroundCount > 0;
}

static int __xuiCodeAnnotationCompareRangeIndex(const void* pLeft, const void* pRight)
{
	const xui_code_annotation_index_t* pA = (const xui_code_annotation_index_t*)pLeft;
	const xui_code_annotation_index_t* pB = (const xui_code_annotation_index_t*)pRight;
	if ( pA->iStart != pB->iStart ) return (pA->iStart < pB->iStart) ? -1 : 1;
	if ( pA->iEnd != pB->iEnd ) return (pA->iEnd < pB->iEnd) ? -1 : 1;
	return (pA->iIndex < pB->iIndex) ? -1 : (pA->iIndex > pB->iIndex);
}

static int __xuiCodeAnnotationRebuildDiagnosticIndex(xui_code_annotation_store pStore)
{
	int i;
	int iMaxEnd;
	int iRet;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pStore->iDiagnosticCount == 0 ) return XUI_OK;
	iRet = __xuiCodeAnnotationReserveDiagnosticIndex(pStore, pStore->iDiagnosticCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) {
		pStore->pDiagnosticIndex[i].iStart = pStore->pDiagnostics[i].tRange.iStart;
		pStore->pDiagnosticIndex[i].iEnd = pStore->pDiagnostics[i].tRange.iEnd;
		pStore->pDiagnosticIndex[i].iIndex = i;
		pStore->pDiagnosticIndex[i].iPrefixMaxEnd = pStore->pDiagnostics[i].tRange.iEnd;
	}
	qsort(pStore->pDiagnosticIndex, (size_t)pStore->iDiagnosticCount, sizeof(*pStore->pDiagnosticIndex), __xuiCodeAnnotationCompareRangeIndex);
	iMaxEnd = 0;
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) {
		if ( pStore->pDiagnosticIndex[i].iEnd > iMaxEnd ) iMaxEnd = pStore->pDiagnosticIndex[i].iEnd;
		pStore->pDiagnosticIndex[i].iPrefixMaxEnd = iMaxEnd;
	}
	return XUI_OK;
}

static void __xuiCodeAnnotationBumpDiagnosticVersion(xui_code_annotation_store pStore)
{
	if ( pStore == NULL ) return;
	pStore->iDiagnosticVersion++;
	if ( pStore->iDiagnosticVersion == 0u ) pStore->iDiagnosticVersion = 1u;
}

static void __xuiCodeAnnotationFreeDiagnosticStorage(xui_code_annotation_store pStore)
{
	int i;

	if ( pStore == NULL ) return;
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) {
		__xuiCodeAnnotationFreeDiagnostic(&pStore->pDiagnostics[i]);
	}
	xrtFree(pStore->pDiagnostics);
	xrtFree(pStore->pDiagnosticIndex);
	pStore->pDiagnostics = NULL;
	pStore->pDiagnosticIndex = NULL;
	pStore->iDiagnosticCount = 0;
	pStore->iDiagnosticCapacity = 0;
	pStore->iDiagnosticIndexCapacity = 0;
}

static int __xuiCodeAnnotationRangeFirst(const xui_code_annotation_index_t* pIndex, int iCount, int iStart)
{
    int iLow = 0, iHigh = iCount;
    while ( iLow < iHigh ) {
        int iMid = iLow + (iHigh - iLow) / 2;
        if ( pIndex[iMid].iPrefixMaxEnd > iStart ) iHigh = iMid;
        else iLow = iMid + 1;
    }
    return iLow;
}
static void __xuiCodeAnnotationRebuildBackgroundIndex(xui_code_annotation_store pStore)
{
    int i, iCount = 0, iMaxEnd = 0;
    if ( !pStore->bBackgroundIndexDirty ) return;
    /* Capacity is acquired before publishing each eligible SetIndicator. No
     * allocation can first fail here while an already accepted model draws. */
    for ( i = 0; i < pStore->iIndicatorCount; ++i ) {
        const xui_code_indicator_t* pItem = &pStore->pIndicators[i];
        xui_code_annotation_index_t* pEntry;
        if ( !__xuiCodeAnnotationIsBackground(pItem) ) continue;
        pEntry = &pStore->pBackgroundIndex[iCount++];
        *pEntry = (xui_code_annotation_index_t){pItem->tRange.iStart, pItem->tRange.iEnd, i, pItem->tRange.iEnd};
    }
    if ( iCount > 1 ) qsort(pStore->pBackgroundIndex, (size_t)iCount,
        sizeof(*pStore->pBackgroundIndex), __xuiCodeAnnotationCompareRangeIndex);
    for ( i = 0; i < iCount; ++i ) {
        if ( pStore->pBackgroundIndex[i].iEnd > iMaxEnd ) iMaxEnd = pStore->pBackgroundIndex[i].iEnd;
        pStore->pBackgroundIndex[i].iPrefixMaxEnd = iMaxEnd;
    }
    pStore->bBackgroundIndexDirty = 0;
}

/* Private renderer bridge: borrowed owner-thread model, visible byte bounds.
 * All generic background indicators currently use the native result color;
 * merge their clipped union to avoid repeated alpha blending or duplicate
 * draw calls. Find-result ID7 remains owned by its existing dedicated path. */
int __xuiCodeAnnotationVisitBackgroundRanges(xui_code_annotation_store pStore,
    int iStart, int iEnd, int (*pVisit)(void*, int, int), void* pUser)
{
    int i, iUnionStart = -1, iUnionEnd = -1;
    if ( pStore == NULL || iStart < 0 || iEnd <= iStart || pVisit == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
    __xuiCodeAnnotationRebuildBackgroundIndex(pStore);
    i = __xuiCodeAnnotationRangeFirst(pStore->pBackgroundIndex, pStore->iBackgroundCount, iStart);
    for ( ; i < pStore->iBackgroundCount; ++i ) {
        const xui_code_annotation_index_t* pEntry = &pStore->pBackgroundIndex[i];
        int iLeft, iRight, iRet;
        if ( pEntry->iStart >= iEnd ) break;
        if ( pEntry->iEnd <= iStart ) continue;
        iLeft = pEntry->iStart > iStart ? pEntry->iStart : iStart;
        iRight = pEntry->iEnd < iEnd ? pEntry->iEnd : iEnd;
        if ( iUnionStart >= 0 && iLeft > iUnionEnd ) {
            iRet = pVisit(pUser, iUnionStart, iUnionEnd);
            if ( iRet != XUI_OK ) return iRet;
            iUnionStart = -1;
        }
        if ( iUnionStart < 0 ) { iUnionStart = iLeft; iUnionEnd = iRight; }
        else if ( iRight > iUnionEnd ) iUnionEnd = iRight;
    }
    return iUnionStart < 0 ? XUI_OK : pVisit(pUser, iUnionStart, iUnionEnd);
}

static int __xuiCodeAnnotationRangeOverlaps(int iStartA, int iEndA, int iStartB, int iEndB)
{
	return iStartA < iEndB && iStartB < iEndA;
}

static int __xuiCodeAnnotationRangeContains(const xui_code_range_t* pRange, int iOffset)
{
	return pRange->iStart <= iOffset && iOffset < pRange->iEnd;
}

static int __xuiCodeAnnotationStringEquals(const char* sLeft, const char* sRight)
{
	if ( sLeft == sRight ) return 1;
	if ( sLeft == NULL || sRight == NULL ) return 0;
	return strcmp(sLeft, sRight) == 0;
}

static int __xuiCodeAnnotationDiagnosticEquals(
	const xui_code_diagnostic_t* pLeft, const xui_code_diagnostic_t* pRight)
{
	if ( pLeft == NULL || pRight == NULL ) return 0;
	return pLeft->tRange.iStart == pRight->tRange.iStart &&
		pLeft->tRange.iEnd == pRight->tRange.iEnd &&
		pLeft->iSeverity == pRight->iSeverity &&
		pLeft->iUserData == pRight->iUserData &&
		__xuiCodeAnnotationStringEquals(pLeft->sCode, pRight->sCode) &&
		__xuiCodeAnnotationStringEquals(pLeft->sMessage, pRight->sMessage) &&
		__xuiCodeAnnotationStringEquals(pLeft->sSource, pRight->sSource);
}

static void __xuiCodeAnnotationCopyMarkerOut(xui_code_marker_t* pOut, const xui_code_marker_t* pMarker)
{
	*pOut = *pMarker;
	pOut->iSize = sizeof(*pOut);
}

static void __xuiCodeAnnotationCopyIndicatorOut(xui_code_indicator_t* pOut, const xui_code_indicator_t* pIndicator)
{
	*pOut = *pIndicator;
	pOut->iSize = sizeof(*pOut);
}

static void __xuiCodeAnnotationCopyDiagnosticOut(xui_code_diagnostic_t* pOut, const xui_code_diagnostic_t* pDiagnostic)
{
	*pOut = *pDiagnostic;
	pOut->iSize = sizeof(*pOut);
}

XUI_API int xuiCodeAnnotationStoreCreate(xui_code_annotation_store* ppStore)
{
	xui_code_annotation_store pStore;

	if ( ppStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppStore = NULL;
	pStore = (xui_code_annotation_store)xrtMalloc(sizeof(*pStore));
	if ( pStore == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pStore, 0, sizeof(*pStore));
	*ppStore = pStore;
	return XUI_OK;
}

XUI_API void xuiCodeAnnotationStoreDestroy(xui_code_annotation_store pStore)
{
	if ( pStore == NULL ) return;
	xuiCodeAnnotationStoreClear(pStore);
	xrtFree(pStore->pMarkers);
	xrtFree(pStore->pIndicators);
	xrtFree(pStore->pDiagnostics);
	xrtFree(pStore->pDiagnosticIndex);
	xrtFree(pStore->pBackgroundIndex);
	xrtFree(pStore);
}

XUI_API void xuiCodeAnnotationStoreClear(xui_code_annotation_store pStore)
{
	int bHadDiagnostics;
	int i;

	if ( pStore == NULL ) return;
	bHadDiagnostics = pStore->iDiagnosticCount > 0;
	for ( i = 0; i < pStore->iMarkerCount; i++ ) __xuiCodeAnnotationFreeMarker(&pStore->pMarkers[i]);
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) __xuiCodeAnnotationFreeDiagnostic(&pStore->pDiagnostics[i]);
	pStore->iMarkerCount = 0;
	pStore->iIndicatorCount = 0;
	pStore->iBackgroundCount = 0;
	pStore->bBackgroundIndexDirty = 0;
	pStore->iDiagnosticCount = 0;
	if ( bHadDiagnostics ) __xuiCodeAnnotationBumpDiagnosticVersion(pStore);
}

XUI_API int xuiCodeAnnotationSetMarker(xui_code_annotation_store pStore, int iLine, int iMarker, uint32_t iFlags, const char* sTooltip, uintptr_t iUserData)
{
	xui_code_marker_t* pMarker;
	char* sCopy;
	int i;
	int iRet;

	if ( (pStore == NULL) || (iLine < 0) || (iMarker <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	sCopy = __xuiCodeAnnotationStrDup(sTooltip);
	if ( sTooltip != NULL && sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	for ( i = 0; i < pStore->iMarkerCount; i++ ) {
		if ( pStore->pMarkers[i].iLine == iLine && pStore->pMarkers[i].iMarker == iMarker ) {
			xrtFree((void*)pStore->pMarkers[i].sTooltip);
			pStore->pMarkers[i].sTooltip = sCopy;
			pStore->pMarkers[i].iFlags = iFlags;
			pStore->pMarkers[i].iUserData = iUserData;
			return XUI_OK;
		}
	}
	if ( pStore->iMarkerCount == INT_MAX ) {
		xrtFree(sCopy);
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	iRet = __xuiCodeAnnotationReserveMarkers(pStore, pStore->iMarkerCount + 1);
	if ( iRet != XUI_OK ) {
		xrtFree(sCopy);
		return iRet;
	}
	pMarker = &pStore->pMarkers[pStore->iMarkerCount++];
	memset(pMarker, 0, sizeof(*pMarker));
	pMarker->iSize = sizeof(*pMarker);
	pMarker->iLine = iLine;
	pMarker->iMarker = iMarker;
	pMarker->iFlags = iFlags;
	pMarker->sTooltip = sCopy;
	pMarker->iUserData = iUserData;
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationClearMarker(xui_code_annotation_store pStore, int iLine, int iMarker)
{
	int i;

	if ( (pStore == NULL) || (iLine < 0) || (iMarker <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pStore->iMarkerCount; i++ ) {
		if ( pStore->pMarkers[i].iLine == iLine && pStore->pMarkers[i].iMarker == iMarker ) {
			__xuiCodeAnnotationFreeMarker(&pStore->pMarkers[i]);
			memmove(&pStore->pMarkers[i], &pStore->pMarkers[i + 1], sizeof(pStore->pMarkers[i]) * (size_t)(pStore->iMarkerCount - i - 1));
			pStore->iMarkerCount--;
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeAnnotationClearMarkers(xui_code_annotation_store pStore, int iLine)
{
	int iOriginalCount;
	int iWrite;
	int i;
	int iCleared;

	if ( (pStore == NULL) || (iLine < -1) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOriginalCount = pStore->iMarkerCount;
	iCleared = 0;
	iWrite = 0;
	for ( i = 0; i < iOriginalCount; i++ ) {
		if ( iLine < 0 || pStore->pMarkers[i].iLine == iLine ) {
			__xuiCodeAnnotationFreeMarker(&pStore->pMarkers[i]);
			iCleared++;
			continue;
		}
		if ( iWrite != i ) pStore->pMarkers[iWrite] = pStore->pMarkers[i];
		iWrite++;
	}
	if ( iWrite < iOriginalCount ) {
		memset(&pStore->pMarkers[iWrite], 0,
			sizeof(*pStore->pMarkers) * (size_t)(iOriginalCount - iWrite));
	}
	pStore->iMarkerCount = iWrite;
	return (iCleared > 0) ? XUI_OK : XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeAnnotationGetMarkers(xui_code_annotation_store pStore, int iLine, xui_code_marker_t* pMarkers, int iMarkerCapacity, int* pMarkerCount)
{
	int i;
	int iCount;

	if ( (pStore == NULL) || (iLine < 0) || (pMarkerCount == NULL) ||
	     iMarkerCapacity < 0 || (pMarkers == NULL && iMarkerCapacity > 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = 0;
	for ( i = 0; i < pStore->iMarkerCount; i++ ) {
		if ( pStore->pMarkers[i].iLine != iLine ) continue;
		if ( pMarkers != NULL && iCount < iMarkerCapacity ) __xuiCodeAnnotationCopyMarkerOut(&pMarkers[iCount], &pStore->pMarkers[i]);
		iCount++;
	}
	*pMarkerCount = iCount;
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationSetIndicator(xui_code_annotation_store pStore, int iIndicator, int iStyle, int iStart, int iEnd, uint32_t iFlags, uintptr_t iUserData)
{
    xui_code_indicator_t* pIndicator;
    int iRet, bBackground;
    if ( pStore == NULL || iIndicator <= 0 || iStart < 0 || iEnd <= iStart ) return XUI_ERROR_INVALID_ARGUMENT;
    if ( pStore->iIndicatorCount == INT_MAX ) return XUI_ERROR_LIMIT_EXCEEDED;
    iRet = __xuiCodeAnnotationReserveIndicators(pStore, pStore->iIndicatorCount + 1);
    if ( iRet != XUI_OK ) return iRet;
    bBackground = iStyle == XUI_CODE_INDICATOR_BACKGROUND && iIndicator != XUI_CODE_INDICATOR_SEARCH_RESULT;
    if ( bBackground ) {
        iRet = __xuiCodeAnnotationReserveBackgroundIndex(pStore, pStore->iBackgroundCount + 1);
        if ( iRet != XUI_OK ) return iRet;
    }
    pIndicator = &pStore->pIndicators[pStore->iIndicatorCount];
    memset(pIndicator, 0, sizeof(*pIndicator));
    pIndicator->iSize = sizeof(*pIndicator);
    pIndicator->iIndicator = iIndicator; pIndicator->iStyle = iStyle;
    pIndicator->tRange.iStart = iStart; pIndicator->tRange.iEnd = iEnd;
    pIndicator->iFlags = iFlags; pIndicator->iUserData = iUserData;
    if ( bBackground ) {
        xui_code_annotation_index_t tEntry = {iStart, iEnd, pStore->iIndicatorCount, iEnd};
        if ( !pStore->bBackgroundIndexDirty ) {
            if ( pStore->iBackgroundCount > 0 ) {
                const xui_code_annotation_index_t* pLast = &pStore->pBackgroundIndex[pStore->iBackgroundCount - 1];
                if ( __xuiCodeAnnotationCompareRangeIndex(pLast, &tEntry) > 0 ) pStore->bBackgroundIndexDirty = 1;
                if ( pLast->iPrefixMaxEnd > tEntry.iPrefixMaxEnd ) tEntry.iPrefixMaxEnd = pLast->iPrefixMaxEnd;
            }
            if ( !pStore->bBackgroundIndexDirty ) pStore->pBackgroundIndex[pStore->iBackgroundCount] = tEntry;
        }
        ++pStore->iBackgroundCount;
    }
    ++pStore->iIndicatorCount;
    return XUI_OK;
}

XUI_API int xuiCodeAnnotationClearIndicator(xui_code_annotation_store pStore, int iIndicator, int iStart, int iEnd)
{
	xui_code_indicator_t* pNew;
	xui_code_indicator_t tIndicator;
	xui_code_indicator_t tRight;
	int iOriginalCount;
	int iFinalCount;
	int iSplitCount;
	int iRemoveCount;
	int iWrite;
	int i;
	int iCleared;
	int iBackgroundFinalCount, iRet;

	if ( (pStore == NULL) || (iIndicator <= 0) || (iStart < 0) || (iEnd <= iStart) ) return XUI_ERROR_INVALID_ARGUMENT;
	iCleared = 0;
	iSplitCount = 0;
	iRemoveCount = 0;
	for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
		if ( pStore->pIndicators[i].iIndicator == iIndicator && __xuiCodeAnnotationRangeOverlaps(pStore->pIndicators[i].tRange.iStart, pStore->pIndicators[i].tRange.iEnd, iStart, iEnd) ) {
			iCleared++;
			if ( iStart <= pStore->pIndicators[i].tRange.iStart &&
			     iEnd >= pStore->pIndicators[i].tRange.iEnd ) {
				iRemoveCount++;
			} else if ( iStart > pStore->pIndicators[i].tRange.iStart &&
			            iEnd < pStore->pIndicators[i].tRange.iEnd ) {
				iSplitCount++;
			}
		}
	}
	if ( iCleared == 0 ) return XUI_ERROR_UNSUPPORTED;
	if ( iSplitCount == 0 ) {
		iWrite = 0;
		for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
			tIndicator = pStore->pIndicators[i];
			if ( tIndicator.iIndicator != iIndicator ||
			     !__xuiCodeAnnotationRangeOverlaps(tIndicator.tRange.iStart,
				tIndicator.tRange.iEnd, iStart, iEnd) ) {
				pStore->pIndicators[iWrite++] = tIndicator;
				continue;
			}
			if ( iStart <= tIndicator.tRange.iStart &&
			     iEnd >= tIndicator.tRange.iEnd ) continue;
			if ( iStart <= tIndicator.tRange.iStart ) {
				tIndicator.tRange.iStart = iEnd;
			} else {
				tIndicator.tRange.iEnd = iStart;
			}
			pStore->pIndicators[iWrite++] = tIndicator;
		}
		pStore->iIndicatorCount = iWrite;
		__xuiCodeAnnotationBackgroundCompacted(pStore);
		return XUI_OK;
	}
	if ( iSplitCount > INT_MAX - pStore->iIndicatorCount ) return XUI_ERROR_LIMIT_EXCEEDED;
	iOriginalCount = pStore->iIndicatorCount;
	iFinalCount = iOriginalCount - iRemoveCount + iSplitCount;
	if ( (size_t)iFinalCount > SIZE_MAX / sizeof(*pNew) ) return XUI_ERROR_LIMIT_EXCEEDED;
	pNew = NULL;
	if ( iFinalCount > 0 ) {
		pNew = (xui_code_indicator_t*)xrtMalloc(sizeof(*pNew) * (size_t)iFinalCount);
		if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	iWrite = 0;
	for ( i = 0; i < iOriginalCount; i++ ) {
		tIndicator = pStore->pIndicators[i];
		if ( tIndicator.iIndicator != iIndicator ||
		     !__xuiCodeAnnotationRangeOverlaps(tIndicator.tRange.iStart,
			 tIndicator.tRange.iEnd, iStart, iEnd) ) {
			pNew[iWrite++] = tIndicator;
			continue;
		}
		if ( iStart <= tIndicator.tRange.iStart && iEnd >= tIndicator.tRange.iEnd ) {
			continue;
		}
		if ( iStart <= tIndicator.tRange.iStart ) {
			tIndicator.tRange.iStart = iEnd;
			pNew[iWrite++] = tIndicator;
			continue;
		}
		if ( iEnd >= tIndicator.tRange.iEnd ) {
			tIndicator.tRange.iEnd = iStart;
			pNew[iWrite++] = tIndicator;
			continue;
		}
		tRight = tIndicator;
		tIndicator.tRange.iEnd = iStart;
		tRight.tRange.iStart = iEnd;
		pNew[iWrite++] = tIndicator;
		pNew[iWrite++] = tRight;
	}
	if ( iWrite != iFinalCount ) {
		xrtFree(pNew);
		return XUI_ERROR_INVALID_STATE;
	}
    iBackgroundFinalCount = 0;
    for ( i = 0; i < iFinalCount; ++i ) {
        if ( __xuiCodeAnnotationIsBackground(&pNew[i]) ) ++iBackgroundFinalCount;
    }
    iRet = __xuiCodeAnnotationReserveBackgroundIndex(pStore, iBackgroundFinalCount);
    if ( iRet != XUI_OK ) {
        xrtFree(pNew);
        return iRet;
    }
	xrtFree(pStore->pIndicators);
	pStore->pIndicators = pNew;
	pStore->iIndicatorCount = iFinalCount;
	pStore->iIndicatorCapacity = iFinalCount;
	__xuiCodeAnnotationBackgroundCompacted(pStore);
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationClearIndicators(xui_code_annotation_store pStore, int iIndicator)
{
	int iWrite;
	int i;
	int iCleared;

	if ( (pStore == NULL) || (iIndicator < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iCleared = 0;
	iWrite = 0;
	for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
		if ( iIndicator == 0 || pStore->pIndicators[i].iIndicator == iIndicator ) {
			iCleared++;
			continue;
		}
		if ( iWrite != i ) pStore->pIndicators[iWrite] = pStore->pIndicators[i];
		iWrite++;
	}
	pStore->iIndicatorCount = iWrite;
	if ( iCleared > 0 ) __xuiCodeAnnotationBackgroundCompacted(pStore);
	return (iCleared > 0) ? XUI_OK : XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeAnnotationGetIndicatorsAt(xui_code_annotation_store pStore, int iOffset, xui_code_indicator_t* pIndicators, int iIndicatorCapacity, int* pIndicatorCount)
{
	int i;
	int iCount;

	if ( (pStore == NULL) || (iOffset < 0) || (pIndicatorCount == NULL) ||
	     iIndicatorCapacity < 0 || (pIndicators == NULL && iIndicatorCapacity > 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = 0;
	for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
		if ( !__xuiCodeAnnotationRangeContains(&pStore->pIndicators[i].tRange, iOffset) ) continue;
		if ( pIndicators != NULL && iCount < iIndicatorCapacity ) __xuiCodeAnnotationCopyIndicatorOut(&pIndicators[iCount], &pStore->pIndicators[i]);
		iCount++;
	}
	*pIndicatorCount = iCount;
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationSetDiagnostics(xui_code_annotation_store pStore, const xui_code_diagnostic_t* pDiagnostics, int iDiagnosticCount)
{
	xui_code_annotation_store_t tNew;
	xui_code_diagnostic_t* pDiagnostic;
	char* sCode;
	char* sMessage;
	char* sSource;
	int i;
	int iRet;

	if ( (pStore == NULL) || (iDiagnosticCount < 0) || (iDiagnosticCount > 0 && pDiagnostics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < iDiagnosticCount; i++ ) {
		if ( (pDiagnostics[i].iSize != 0u &&
		     pDiagnostics[i].iSize < sizeof(pDiagnostics[i])) ||
		     pDiagnostics[i].tRange.iStart < 0 ||
		     pDiagnostics[i].tRange.iEnd <= pDiagnostics[i].tRange.iStart ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	if ( iDiagnosticCount == 0 && pStore->iDiagnosticCount == 0 ) return XUI_OK;
	if ( iDiagnosticCount == pStore->iDiagnosticCount ) {
		for ( i = 0; i < iDiagnosticCount; i++ ) {
			if ( !__xuiCodeAnnotationDiagnosticEquals(
			     &pStore->pDiagnostics[i], &pDiagnostics[i]) ) break;
		}
		if ( i == iDiagnosticCount ) return XUI_OK;
	}
	memset(&tNew, 0, sizeof(tNew));
	iRet = __xuiCodeAnnotationReserveDiagnostics(&tNew, iDiagnosticCount);
	if ( iRet == XUI_OK ) {
		iRet = __xuiCodeAnnotationReserveDiagnosticIndex(&tNew, iDiagnosticCount);
	}
	if ( iRet != XUI_OK ) {
		__xuiCodeAnnotationFreeDiagnosticStorage(&tNew);
		return iRet;
	}
	for ( i = 0; i < iDiagnosticCount; i++ ) {
		sCode = __xuiCodeAnnotationStrDup(pDiagnostics[i].sCode);
		sMessage = __xuiCodeAnnotationStrDup(pDiagnostics[i].sMessage);
		sSource = __xuiCodeAnnotationStrDup(pDiagnostics[i].sSource);
		if ( (pDiagnostics[i].sCode != NULL && sCode == NULL) || (pDiagnostics[i].sMessage != NULL && sMessage == NULL) || (pDiagnostics[i].sSource != NULL && sSource == NULL) ) {
			xrtFree(sCode);
			xrtFree(sMessage);
			xrtFree(sSource);
			__xuiCodeAnnotationFreeDiagnosticStorage(&tNew);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pDiagnostic = &tNew.pDiagnostics[tNew.iDiagnosticCount++];
		memset(pDiagnostic, 0, sizeof(*pDiagnostic));
		*pDiagnostic = pDiagnostics[i];
		pDiagnostic->iSize = sizeof(*pDiagnostic);
		pDiagnostic->sCode = sCode;
		pDiagnostic->sMessage = sMessage;
		pDiagnostic->sSource = sSource;
	}
	iRet = __xuiCodeAnnotationRebuildDiagnosticIndex(&tNew);
	if ( iRet != XUI_OK ) {
		__xuiCodeAnnotationFreeDiagnosticStorage(&tNew);
		return iRet;
	}
	__xuiCodeAnnotationFreeDiagnosticStorage(pStore);
	pStore->pDiagnostics = tNew.pDiagnostics;
	pStore->iDiagnosticCount = tNew.iDiagnosticCount;
	pStore->iDiagnosticCapacity = tNew.iDiagnosticCapacity;
	pStore->pDiagnosticIndex = tNew.pDiagnosticIndex;
	pStore->iDiagnosticIndexCapacity = tNew.iDiagnosticIndexCapacity;
	tNew.pDiagnostics = NULL;
	tNew.pDiagnosticIndex = NULL;
	__xuiCodeAnnotationBumpDiagnosticVersion(pStore);
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationClearDiagnostics(xui_code_annotation_store pStore)
{
	int i;

	if ( pStore == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pStore->iDiagnosticCount == 0 ) return XUI_OK;
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) __xuiCodeAnnotationFreeDiagnostic(&pStore->pDiagnostics[i]);
	pStore->iDiagnosticCount = 0;
	__xuiCodeAnnotationBumpDiagnosticVersion(pStore);
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationGetDiagnosticCount(xui_code_annotation_store pStore)
{
	return (pStore != NULL) ? pStore->iDiagnosticCount : 0;
}

XUI_API int xuiCodeAnnotationGetDiagnostic(xui_code_annotation_store pStore, int iIndex, xui_code_diagnostic_t* pDiagnostic)
{
	if ( (pStore == NULL) || (pDiagnostic == NULL) || (iIndex < 0) || (iIndex >= pStore->iDiagnosticCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCodeAnnotationCopyDiagnosticOut(pDiagnostic, &pStore->pDiagnostics[iIndex]);
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationGetDiagnosticsAt(xui_code_annotation_store pStore, int iOffset, xui_code_diagnostic_t* pDiagnostics, int iDiagnosticCapacity, int* pDiagnosticCount)
{
	if ( iOffset < 0 || iOffset == INT_MAX ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiCodeAnnotationGetDiagnosticsInRange(pStore, iOffset, iOffset + 1, pDiagnostics, NULL, iDiagnosticCapacity, pDiagnosticCount);
}

XUI_API int xuiCodeAnnotationGetDiagnosticsInRange(xui_code_annotation_store pStore, int iStart, int iEnd, xui_code_diagnostic_t* pDiagnostics, int* pIndices, int iDiagnosticCapacity, int* pDiagnosticCount)
{
	const xui_code_annotation_index_t* pEntry;
	int i;
	int iCount;
	int iIndex;

	if ( (pStore == NULL) || (iStart < 0) || (iEnd <= iStart) ||
	     (iDiagnosticCapacity < 0) || (pDiagnosticCount == NULL) ||
	     (pDiagnostics == NULL && pIndices == NULL && iDiagnosticCapacity > 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = 0;
	i = __xuiCodeAnnotationRangeFirst(pStore->pDiagnosticIndex, pStore->iDiagnosticCount, iStart);
	for ( ; i < pStore->iDiagnosticCount; i++ ) {
		pEntry = &pStore->pDiagnosticIndex[i];
		if ( pEntry->iStart >= iEnd ) break;
		if ( pEntry->iEnd <= iStart ) continue;
		iIndex = pEntry->iIndex;
		if ( iCount < iDiagnosticCapacity ) {
			if ( pDiagnostics != NULL ) __xuiCodeAnnotationCopyDiagnosticOut(&pDiagnostics[iCount], &pStore->pDiagnostics[iIndex]);
			if ( pIndices != NULL ) pIndices[iCount] = iIndex;
		}
		iCount++;
	}
	*pDiagnosticCount = iCount;
	return XUI_OK;
}

XUI_API uint32_t xuiCodeAnnotationGetDiagnosticVersion(xui_code_annotation_store pStore)
{
	return (pStore != NULL) ? pStore->iDiagnosticVersion : 0u;
}

static int __xuiCodeAnnotationTrackRange(xui_code_range_t* pRange,
	int iStartOffset, int iEndOffset, int iNewEndOffset, int bApply)
{
	int64_t iDelta;
	int64_t iStart;
	int64_t iEnd;

	if ( pRange == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iDelta = (int64_t)iNewEndOffset - (int64_t)iEndOffset;
	iStart = pRange->iStart;
	iEnd = pRange->iEnd;
	if ( iEnd <= iStartOffset ) return XUI_OK;
	if ( pRange->iStart >= iEndOffset ) {
		iStart += iDelta;
		iEnd += iDelta;
	} else {
		if ( iStart >= iStartOffset ) iStart = iStartOffset;
		if ( iEnd <= iEndOffset ) iEnd = iNewEndOffset;
		else iEnd += iDelta;
		if ( iEnd <= iStart ) iEnd = iStart + 1;
	}
	if ( iStart < 0 || iEnd <= iStart || iStart > INT_MAX || iEnd > INT_MAX ) {
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	if ( bApply ) {
		pRange->iStart = (int)iStart;
		pRange->iEnd = (int)iEnd;
	}
	return XUI_OK;
}

XUI_API int xuiCodeAnnotationTrackEdit(xui_code_annotation_store pStore, int iStartOffset, int iEndOffset, int iNewEndOffset, int iStartLine, int iEndLine, int iNewEndLine)
{
	xui_code_range_t tTrackedRange;
	int64_t iLineDelta;
	int64_t iNewLine;
	int bDiagnosticRangeChanged;
	int bDiagnosticTouched;
	int i;
	int iRet;

	if ( (pStore == NULL) || (iStartOffset < 0) || (iEndOffset < iStartOffset) || (iNewEndOffset < iStartOffset) || (iStartLine < 0) || (iEndLine < iStartLine) || (iNewEndLine < iStartLine) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLineDelta = (int64_t)iNewEndLine - (int64_t)iEndLine;
	bDiagnosticRangeChanged = 0;
	bDiagnosticTouched = 0;
	for ( i = 0; i < pStore->iMarkerCount; i++ ) {
		iNewLine = pStore->pMarkers[i].iLine;
		if ( iNewLine > iEndLine ) iNewLine += iLineDelta;
		else if ( iNewLine >= iStartLine ) iNewLine = iStartLine;
		if ( iNewLine < 0 || iNewLine > INT_MAX ) return XUI_ERROR_LIMIT_EXCEEDED;
	}
	for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
		iRet = __xuiCodeAnnotationTrackRange(&pStore->pIndicators[i].tRange,
			iStartOffset, iEndOffset, iNewEndOffset, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) {
		tTrackedRange = pStore->pDiagnostics[i].tRange;
		iRet = __xuiCodeAnnotationTrackRange(&tTrackedRange,
			iStartOffset, iEndOffset, iNewEndOffset, 1);
		if ( iRet != XUI_OK ) return iRet;
		if ( tTrackedRange.iStart != pStore->pDiagnostics[i].tRange.iStart ||
		     tTrackedRange.iEnd != pStore->pDiagnostics[i].tRange.iEnd ) {
			bDiagnosticRangeChanged = 1;
			bDiagnosticTouched = 1;
		} else if ( iEndOffset > iStartOffset &&
		           __xuiCodeAnnotationRangeOverlaps(pStore->pDiagnostics[i].tRange.iStart,
			   pStore->pDiagnostics[i].tRange.iEnd, iStartOffset, iEndOffset) ) {
			bDiagnosticTouched = 1;
		}
	}
	if ( bDiagnosticRangeChanged ) {
		iRet = __xuiCodeAnnotationReserveDiagnosticIndex(pStore,
			pStore->iDiagnosticCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pStore->iMarkerCount; i++ ) {
		if ( pStore->pMarkers[i].iLine > iEndLine ) {
			pStore->pMarkers[i].iLine = (int)((int64_t)pStore->pMarkers[i].iLine + iLineDelta);
		}
		else if ( pStore->pMarkers[i].iLine >= iStartLine ) pStore->pMarkers[i].iLine = iStartLine;
	}
	for ( i = 0; i < pStore->iIndicatorCount; i++ ) {
		(void)__xuiCodeAnnotationTrackRange(&pStore->pIndicators[i].tRange,
			iStartOffset, iEndOffset, iNewEndOffset, 1);
	}
	pStore->bBackgroundIndexDirty = pStore->iBackgroundCount > 0;
	for ( i = 0; i < pStore->iDiagnosticCount; i++ ) {
		(void)__xuiCodeAnnotationTrackRange(&pStore->pDiagnostics[i].tRange,
			iStartOffset, iEndOffset, iNewEndOffset, 1);
	}
	if ( bDiagnosticRangeChanged ) {
		iRet = __xuiCodeAnnotationRebuildDiagnosticIndex(pStore);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( bDiagnosticTouched ) __xuiCodeAnnotationBumpDiagnosticVersion(pStore);
	return XUI_OK;
}

#endif
