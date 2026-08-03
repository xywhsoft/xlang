#include "../xui.h"

#include <limits.h>
#include <string.h>

typedef struct xui_code_line_t {
	int iStart;
	int iEnd;
	int iTextEnd;
} xui_code_line_t;

typedef struct xui_code_edit_action_t {
	int iStart;
	char* sDeleted;
	int iDeletedLength;
	char* sInserted;
	int iInsertedLength;
} xui_code_edit_action_t;

typedef struct xui_code_edit_group_t {
	xui_code_edit_action_t* pActions;
	int iActionCount;
	int iActionCapacity;
	uint64_t iBeforeState;
	uint64_t iAfterState;
} xui_code_edit_group_t;

struct xui_code_document_t {
	char* sBuffer;
	int iLength;
	int iBufferCapacity;
	int iGapStart;
	int iGapEnd;
	char* sSnapshot;
	int iSnapshotCapacity;
	int bSnapshotValid;
	xui_code_line_t* pLines;
	int iLineCount;
	int iLineCapacity;
	int iPendingLineShiftStart;
	int iPendingLineShiftDelta;
	xui_code_edit_group_t* pUndo;
	int iUndoCount;
	int iUndoCapacity;
	xui_code_edit_group_t* pRedo;
	int iRedoCount;
	int iRedoCapacity;
	xui_code_edit_group_t tEditGroup;
	int iEditDepth;
	int bHasLastEditRange;
	int bChangePending;
	int bApplyingHistory;
	int iLastEditStart;
	int iLastEditEnd;
	uint32_t iVersion;
	uint32_t iChangeVersion;
	uint32_t iSavedVersion;
	uint64_t iState;
	uint64_t iSavedState;
	uint64_t iNextState;
	int bDirty;
	xui_code_document_change_proc onChange;
	void* pChangeUser;
	char sError[160];
};

static int __xuiCodeDocumentRebuildLines(xui_code_document pDocument);

static void __xuiCodeDocumentMaterializeLineShift(xui_code_document pDocument)
{
	xui_code_line_t* pLine;
	int i;

	if ( pDocument == NULL || pDocument->iPendingLineShiftDelta == 0 ) return;
	for ( i = pDocument->iPendingLineShiftStart; i < pDocument->iLineCount; i++ ) {
		pLine = &pDocument->pLines[i];
		pLine->iStart += pDocument->iPendingLineShiftDelta;
		pLine->iEnd += pDocument->iPendingLineShiftDelta;
		pLine->iTextEnd += pDocument->iPendingLineShiftDelta;
	}
	pDocument->iPendingLineShiftStart = 0;
	pDocument->iPendingLineShiftDelta = 0;
}

static int __xuiCodeDocumentLineField(xui_code_document pDocument,
	int iLine, int iField)
{
	int iValue;

	if ( pDocument == NULL || iLine < 0 || iLine >= pDocument->iLineCount ) return 0;
	if ( iField == 0 ) iValue = pDocument->pLines[iLine].iStart;
	else if ( iField == 1 ) iValue = pDocument->pLines[iLine].iEnd;
	else iValue = pDocument->pLines[iLine].iTextEnd;
	if ( pDocument->iPendingLineShiftDelta != 0 &&
	     iLine >= pDocument->iPendingLineShiftStart ) {
		iValue += pDocument->iPendingLineShiftDelta;
	}
	return iValue;
}

static int __xuiCodeDocumentGapSize(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iGapEnd - pDocument->iGapStart;
}

static unsigned char __xuiCodeDocumentByteAt(xui_code_document pDocument, int iOffset)
{
	int iPhysical;

	if ( pDocument == NULL || iOffset < 0 || iOffset >= pDocument->iLength ) return 0;
	iPhysical = iOffset;
	if ( iOffset >= pDocument->iGapStart ) iPhysical += __xuiCodeDocumentGapSize(pDocument);
	return (unsigned char)pDocument->sBuffer[iPhysical];
}

static int __xuiCodeDocumentCopyRangeRaw(xui_code_document pDocument,
	int iStart, int iEnd, char* sOutput)
{
	int iBeforeEnd;
	int iBeforeLength;
	int iAfterLength;

	if ( pDocument == NULL || sOutput == NULL ||
	     iStart < 0 || iEnd < iStart || iEnd > pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iBeforeEnd = (iEnd < pDocument->iGapStart) ? iEnd : pDocument->iGapStart;
	iBeforeLength = iBeforeEnd - iStart;
	if ( iBeforeLength < 0 ) iBeforeLength = 0;
	if ( iBeforeLength > 0 ) {
		memcpy(sOutput, pDocument->sBuffer + iStart, (size_t)iBeforeLength);
	}
	iAfterLength = iEnd - (iStart + iBeforeLength);
	if ( iAfterLength > 0 ) {
		int iAfterStart = iStart + iBeforeLength;
		memcpy(sOutput + iBeforeLength,
			pDocument->sBuffer + iAfterStart + __xuiCodeDocumentGapSize(pDocument),
			(size_t)iAfterLength);
	}
	return XUI_OK;
}

static void __xuiCodeDocumentSetError(xui_code_document pDocument, const char* sError)
{
	if ( pDocument == NULL ) return;
	if ( sError == NULL ) sError = "";
	strncpy(pDocument->sError, sError, sizeof(pDocument->sError) - 1u);
	pDocument->sError[sizeof(pDocument->sError) - 1u] = '\0';
}

static int __xuiCodeDocumentUtf8Next(xui_code_document pDocument, int iOffset)
{
	unsigned char c;
	int iStep;

	if ( pDocument == NULL ) return 0;
	if ( iOffset < 0 ) return 0;
	if ( iOffset >= pDocument->iLength ) return pDocument->iLength;
	c = __xuiCodeDocumentByteAt(pDocument, iOffset);
	if ( c < 0x80u ) iStep = 1;
	else if ( (c & 0xE0u) == 0xC0u ) iStep = 2;
	else if ( (c & 0xF0u) == 0xE0u ) iStep = 3;
	else if ( (c & 0xF8u) == 0xF0u ) iStep = 4;
	else iStep = 1;
	if ( iOffset + iStep > pDocument->iLength ) return pDocument->iLength;
	return iOffset + iStep;
}

static int __xuiCodeDocumentClampOffset(xui_code_document pDocument, int iOffset)
{
	if ( pDocument == NULL ) return 0;
	if ( iOffset <= 0 ) return 0;
	if ( iOffset >= pDocument->iLength ) return pDocument->iLength;
	while ( iOffset > 0 && (__xuiCodeDocumentByteAt(pDocument, iOffset) & 0xC0u) == 0x80u ) {
		iOffset--;
	}
	return iOffset;
}

static int __xuiCodeDocumentReserveSnapshot(xui_code_document pDocument, int iCapacity)
{
	char* sNew;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pDocument->iSnapshotCapacity ) return XUI_OK;
	if ( iCapacity < pDocument->iSnapshotCapacity * 2 ) {
		iCapacity = pDocument->iSnapshotCapacity * 2;
	}
	if ( iCapacity < 32 ) iCapacity = 32;
	sNew = (char*)xrtRealloc(pDocument->sSnapshot, (size_t)iCapacity);
	if ( sNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->sSnapshot = sNew;
	pDocument->iSnapshotCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentEnsureSnapshot(xui_code_document pDocument)
{
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bSnapshotValid ) return XUI_OK;
	iRet = __xuiCodeDocumentReserveSnapshot(pDocument, pDocument->iLength + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeDocumentCopyRangeRaw(pDocument, 0, pDocument->iLength, pDocument->sSnapshot);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->sSnapshot[pDocument->iLength] = '\0';
	pDocument->bSnapshotValid = 1;
	return XUI_OK;
}

static int __xuiCodeDocumentMoveGap(xui_code_document pDocument, int iOffset)
{
	int iMove;

	if ( pDocument == NULL || iOffset < 0 || iOffset > pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iOffset < pDocument->iGapStart ) {
		iMove = pDocument->iGapStart - iOffset;
		memmove(pDocument->sBuffer + pDocument->iGapEnd - iMove,
			pDocument->sBuffer + iOffset, (size_t)iMove);
		pDocument->iGapStart = iOffset;
		pDocument->iGapEnd -= iMove;
	} else if ( iOffset > pDocument->iGapStart ) {
		iMove = iOffset - pDocument->iGapStart;
		memmove(pDocument->sBuffer + pDocument->iGapStart,
			pDocument->sBuffer + pDocument->iGapEnd, (size_t)iMove);
		pDocument->iGapStart += iMove;
		pDocument->iGapEnd += iMove;
	}
	return XUI_OK;
}

static int __xuiCodeDocumentEnsureGap(xui_code_document pDocument, int iNeed)
{
	char* sNew;
	int iGapSize;
	int iSuffixLength;
	int iGrowth;
	int iNewCapacity;
	int iNewGapEnd;

	if ( pDocument == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iGapSize = __xuiCodeDocumentGapSize(pDocument);
	if ( iGapSize >= iNeed ) return XUI_OK;
	iGrowth = pDocument->iLength / 8;
	if ( iGrowth < 4096 ) iGrowth = 4096;
	if ( iGrowth > 1024 * 1024 ) iGrowth = 1024 * 1024;
	if ( iGrowth < iNeed - iGapSize ) iGrowth = iNeed - iGapSize;
	if ( pDocument->iLength > INT_MAX - iGapSize - iGrowth ) return XUI_ERROR_OUT_OF_MEMORY;
	iNewCapacity = pDocument->iLength + iGapSize + iGrowth;
	iSuffixLength = pDocument->iBufferCapacity - pDocument->iGapEnd;
	sNew = (char*)xrtRealloc(pDocument->sBuffer, (size_t)iNewCapacity);
	if ( sNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->sBuffer = sNew;
	iNewGapEnd = iNewCapacity - iSuffixLength;
	if ( iSuffixLength > 0 && iNewGapEnd != pDocument->iGapEnd ) {
		memmove(pDocument->sBuffer + iNewGapEnd,
			pDocument->sBuffer + pDocument->iGapEnd,
			(size_t)iSuffixLength);
	}
	pDocument->iGapEnd = iNewGapEnd;
	pDocument->iBufferCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentSetOwnedText(xui_code_document pDocument, char* sText, int iLength)
{
	int iRet;
	int iGapReserve;

	if ( pDocument == NULL || sText == NULL || iLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	xrtFree(pDocument->sBuffer);
	pDocument->sBuffer = sText;
	pDocument->iLength = iLength;
	pDocument->iBufferCapacity = iLength + 1;
	pDocument->iGapStart = iLength;
	pDocument->iGapEnd = iLength + 1;
	pDocument->bSnapshotValid = 0;
	iGapReserve = 4096;
	if ( iLength > 0 && iLength / 64 > iGapReserve ) iGapReserve = iLength / 64;
	if ( iGapReserve > 1024 * 1024 ) iGapReserve = 1024 * 1024;
	iRet = __xuiCodeDocumentEnsureGap(pDocument, iGapReserve);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCodeDocumentRebuildLines(pDocument);
}

static int __xuiCodeDocumentReserveLines(xui_code_document pDocument, int iCapacity)
{
	xui_code_line_t* pNew;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pDocument->iLineCapacity ) return XUI_OK;
	if ( iCapacity < pDocument->iLineCapacity * 2 ) {
		iCapacity = pDocument->iLineCapacity * 2;
	}
	if ( iCapacity < 16 ) iCapacity = 16;
	pNew = (xui_code_line_t*)xrtRealloc(pDocument->pLines, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->pLines = pNew;
	pDocument->iLineCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentAppendLine(xui_code_document pDocument, int iStart, int iEnd, int iTextEnd)
{
	int iRet;

	iRet = __xuiCodeDocumentReserveLines(pDocument, pDocument->iLineCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->pLines[pDocument->iLineCount].iStart = iStart;
	pDocument->pLines[pDocument->iLineCount].iEnd = iEnd;
	pDocument->pLines[pDocument->iLineCount].iTextEnd = iTextEnd;
	pDocument->iLineCount++;
	return XUI_OK;
}

static int __xuiCodeDocumentRebuildLines(xui_code_document pDocument)
{
	xui_code_line_t* pLine;
	int i;
	int iStart;
	int iRet;
	int iLineCount;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->iPendingLineShiftStart = 0;
	pDocument->iPendingLineShiftDelta = 0;
	iLineCount = 1;
	for ( i = 0; i < pDocument->iLength; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			if ( iLineCount == INT_MAX ) return XUI_ERROR_OUT_OF_MEMORY;
			iLineCount++;
		}
	}
	iRet = __xuiCodeDocumentReserveLines(pDocument, iLineCount);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->iLineCount = 0;
	iStart = 0;
	for ( i = 0; i < pDocument->iLength; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			pLine = &pDocument->pLines[pDocument->iLineCount++];
			pLine->iStart = iStart;
			pLine->iEnd = i + 1;
			pLine->iTextEnd = i;
			iStart = i + 1;
		}
	}
	pLine = &pDocument->pLines[pDocument->iLineCount++];
	pLine->iStart = iStart;
	pLine->iEnd = pDocument->iLength;
	pLine->iTextEnd = pDocument->iLength;
	return XUI_OK;
}

static int __xuiCodeDocumentFindLine(xui_code_document pDocument, int iOffset)
{
	int iLow;
	int iHigh;
	int iMid;

	if ( pDocument == NULL || pDocument->iLineCount <= 0 ) return 0;
	if ( iOffset <= 0 ) return 0;
	if ( iOffset >= pDocument->iLength ) return pDocument->iLineCount - 1;
	iLow = 0;
	iHigh = pDocument->iLineCount - 1;
	while ( iLow <= iHigh ) {
		iMid = iLow + (iHigh - iLow) / 2;
		if ( iOffset < __xuiCodeDocumentLineField(pDocument, iMid, 0) ) {
			iHigh = iMid - 1;
		} else if ( iOffset >= __xuiCodeDocumentLineField(pDocument, iMid, 1) &&
		            iMid + 1 < pDocument->iLineCount ) {
			iLow = iMid + 1;
		} else {
			return iMid;
		}
	}
	if ( iLow < 0 ) return 0;
	if ( iLow >= pDocument->iLineCount ) return pDocument->iLineCount - 1;
	return iLow;
}

static int __xuiCodeDocumentUpdateLinesForReplace(xui_code_document pDocument,
	int iStartLine, int iEndLine, int iOldSegmentEnd, int iDelta)
{
	xui_code_line_t* pLine;
	int iNewSegmentEnd;
	int iSuffixStart;
	int iSuffixCount;
	int iAffectedCount;
	int iOldAffectedCount;
	int iNewLineCount;
	int iWrite;
	int iStart;
	int i;
	int iRet;

	if ( pDocument == NULL || iStartLine < 0 || iEndLine < iStartLine ||
	     iEndLine >= pDocument->iLineCount ) return XUI_ERROR_INVALID_ARGUMENT;
	iNewSegmentEnd = iOldSegmentEnd + iDelta;
	if ( iNewSegmentEnd < __xuiCodeDocumentLineField(pDocument, iStartLine, 0) ) {
		iNewSegmentEnd = __xuiCodeDocumentLineField(pDocument, iStartLine, 0);
	}
	if ( iNewSegmentEnd > pDocument->iLength ) iNewSegmentEnd = pDocument->iLength;
	iSuffixStart = iEndLine + 1;
	iSuffixCount = pDocument->iLineCount - iSuffixStart;
	iOldAffectedCount = iEndLine - iStartLine + 1;
	iAffectedCount = 0;
	for ( i = __xuiCodeDocumentLineField(pDocument, iStartLine, 0); i < iNewSegmentEnd; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) iAffectedCount++;
	}
	if ( iSuffixCount == 0 ||
	     iNewSegmentEnd <= __xuiCodeDocumentLineField(pDocument, iStartLine, 0) ||
	     __xuiCodeDocumentByteAt(pDocument, iNewSegmentEnd - 1) != '\n' ) {
		iAffectedCount++;
	}
	if ( iStartLine == iEndLine && iAffectedCount == iOldAffectedCount &&
	     (pDocument->iPendingLineShiftDelta == 0 ||
	      pDocument->iPendingLineShiftStart == iSuffixStart) ) {
		iStart = __xuiCodeDocumentLineField(pDocument, iStartLine, 0);
		pLine = &pDocument->pLines[iStartLine];
		pLine->iStart = iStart;
		pLine->iEnd = iNewSegmentEnd;
		pLine->iTextEnd = (iNewSegmentEnd > iStart &&
			__xuiCodeDocumentByteAt(pDocument, iNewSegmentEnd - 1) == '\n') ?
			iNewSegmentEnd - 1 : iNewSegmentEnd;
		if ( iSuffixCount > 0 ) {
			pDocument->iPendingLineShiftStart = iSuffixStart;
			pDocument->iPendingLineShiftDelta += iDelta;
		}
		return XUI_OK;
	}
	__xuiCodeDocumentMaterializeLineShift(pDocument);
	iNewLineCount = iStartLine + iAffectedCount + iSuffixCount;
	if ( iNewLineCount <= 0 ) iNewLineCount = 1;
	iRet = __xuiCodeDocumentReserveLines(pDocument, iNewLineCount);
	if ( iRet != XUI_OK ) return iRet;
	if ( iSuffixCount > 0 ) {
		memmove(&pDocument->pLines[iStartLine + iAffectedCount],
			&pDocument->pLines[iSuffixStart],
			sizeof(*pDocument->pLines) * (size_t)iSuffixCount);
		for ( i = 0; i < iSuffixCount; i++ ) {
			pLine = &pDocument->pLines[iStartLine + iAffectedCount + i];
			pLine->iStart += iDelta;
			pLine->iEnd += iDelta;
			pLine->iTextEnd += iDelta;
		}
	}
	iWrite = iStartLine;
	iStart = pDocument->pLines[iStartLine].iStart;
	for ( i = iStart; i < iNewSegmentEnd; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			pLine = &pDocument->pLines[iWrite++];
			pLine->iStart = iStart;
			pLine->iEnd = i + 1;
			pLine->iTextEnd = i;
			iStart = i + 1;
		}
	}
	if ( iWrite < iStartLine + iAffectedCount ) {
		pLine = &pDocument->pLines[iWrite++];
		pLine->iStart = iStart;
		pLine->iEnd = iNewSegmentEnd;
		pLine->iTextEnd = iNewSegmentEnd;
	}
	pDocument->iLineCount = iNewLineCount;
	return XUI_OK;
}

static int __xuiCodeDocumentReplaceRaw(xui_code_document pDocument,
	int iStart, int iEnd, const char* sText, int iTextLength)
{
	int iOldLength;
	int iNewLength;
	int iDelta;
	int iStartLine;
	int iEndLine;
	int iOldSegmentEnd;
	int iRet;

	if ( pDocument == NULL || sText == NULL || iTextLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iStart < 0 || iEnd < iStart || iEnd > pDocument->iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldLength = pDocument->iLength;
	iNewLength = iOldLength - (iEnd - iStart) + iTextLength;
	if ( iNewLength < 0 ) return XUI_ERROR_OUT_OF_MEMORY;
	iStartLine = __xuiCodeDocumentFindLine(pDocument, iStart);
	iEndLine = __xuiCodeDocumentFindLine(pDocument, iEnd);
	iOldSegmentEnd = __xuiCodeDocumentLineField(pDocument, iEndLine, 1);
	iRet = __xuiCodeDocumentMoveGap(pDocument, iStart);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->iGapEnd += iEnd - iStart;
	iRet = __xuiCodeDocumentEnsureGap(pDocument, iTextLength);
	if ( iRet != XUI_OK ) return iRet;
	if ( iTextLength > 0 ) {
		memcpy(pDocument->sBuffer + pDocument->iGapStart, sText, (size_t)iTextLength);
		pDocument->iGapStart += iTextLength;
	}
	pDocument->iLength = iNewLength;
	pDocument->bSnapshotValid = 0;
	iDelta = iTextLength - (iEnd - iStart);
	iRet = __xuiCodeDocumentUpdateLinesForReplace(pDocument, iStartLine, iEndLine, iOldSegmentEnd, iDelta);
	if ( iRet != XUI_OK ) {
		/* The text is already valid; recover the derived index deterministically. */
		iRet = __xuiCodeDocumentRebuildLines(pDocument);
	}
	return iRet;
}

static char* __xuiCodeDocumentNormalizeText(const char* sText, int iInputLength, int* pLength)
{
	char* sOut;
	int i;
	int j;

	if ( sText == NULL ) sText = "";
	if ( iInputLength < 0 ) iInputLength = (int)strlen(sText);
	sOut = (char*)xrtMalloc((size_t)iInputLength + 1u);
	if ( sOut == NULL ) return NULL;
	j = 0;
	for ( i = 0; i < iInputLength; i++ ) {
		if ( sText[i] == '\r' ) {
			if ( (i + 1 < iInputLength) && (sText[i + 1] == '\n') ) i++;
			sOut[j++] = '\n';
		} else {
			sOut[j++] = sText[i];
		}
	}
	sOut[j] = '\0';
	if ( pLength != NULL ) *pLength = j;
	return sOut;
}

static int __xuiCodeDocumentNormalizeTextInPlace(char* sText, int iInputLength, int* pLength)
{
	int i;
	int j;

	if ( sText == NULL || iInputLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	j = 0;
	for ( i = 0; i < iInputLength; i++ ) {
		if ( sText[i] == '\r' ) {
			if ( (i + 1 < iInputLength) && (sText[i + 1] == '\n') ) i++;
			sText[j++] = '\n';
		} else {
			sText[j++] = sText[i];
		}
	}
	sText[j] = '\0';
	if ( pLength != NULL ) *pLength = j;
	return XUI_OK;
}

static int __xuiCodeDocumentValidateUtf8(xui_code_document pDocument, const char* sText, int iLength)
{
	if ( sText == NULL ) return XUI_OK;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	if ( !xrtIsUTF8((str)sText, (size_t)iLength) ) {
		__xuiCodeDocumentSetError(pDocument, "invalid UTF-8 input");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

static void __xuiCodeDocumentActionClear(xui_code_edit_action_t* pAction)
{
	if ( pAction == NULL ) return;
	xrtFree(pAction->sDeleted);
	xrtFree(pAction->sInserted);
	memset(pAction, 0, sizeof(*pAction));
}

static void __xuiCodeDocumentGroupClear(xui_code_edit_group_t* pGroup)
{
	int i;

	if ( pGroup == NULL ) return;
	for ( i = 0; i < pGroup->iActionCount; i++ ) {
		__xuiCodeDocumentActionClear(&pGroup->pActions[i]);
	}
	xrtFree(pGroup->pActions);
	memset(pGroup, 0, sizeof(*pGroup));
}

static void __xuiCodeDocumentGroupArrayClear(xui_code_edit_group_t* pGroups, int iCount)
{
	int i;

	if ( pGroups == NULL ) return;
	for ( i = 0; i < iCount; i++ ) {
		__xuiCodeDocumentGroupClear(&pGroups[i]);
	}
}

static int __xuiCodeDocumentGroupArrayReserve(xui_code_document pDocument, xui_code_edit_group_t** ppGroups, int* pCapacity, int iNeed)
{
	xui_code_edit_group_t* pNew;
	int iCapacity;

	if ( pDocument == NULL || ppGroups == NULL || pCapacity == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeed <= *pCapacity ) return XUI_OK;
	iCapacity = (*pCapacity > 0) ? *pCapacity : 8;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > INT_MAX / 2 ) return XUI_ERROR_OUT_OF_MEMORY;
		iCapacity *= 2;
	}
	pNew = (xui_code_edit_group_t*)xrtRealloc(*ppGroups, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pNew + *pCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - *pCapacity));
	*ppGroups = pNew;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentGroupReserveActions(xui_code_document pDocument, xui_code_edit_group_t* pGroup, int iNeed)
{
	xui_code_edit_action_t* pNew;
	int iCapacity;

	if ( pDocument == NULL || pGroup == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeed <= pGroup->iActionCapacity ) return XUI_OK;
	iCapacity = (pGroup->iActionCapacity > 0) ? pGroup->iActionCapacity : 4;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > INT_MAX / 2 ) return XUI_ERROR_OUT_OF_MEMORY;
		iCapacity *= 2;
	}
	pNew = (xui_code_edit_action_t*)xrtRealloc(pGroup->pActions, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pNew + pGroup->iActionCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - pGroup->iActionCapacity));
	pGroup->pActions = pNew;
	pGroup->iActionCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentActionInit(xui_code_document pDocument, xui_code_edit_action_t* pAction,
	int iStart, const char* sDeleted, int iDeletedLength, const char* sInserted, int iInsertedLength)
{
	if ( pDocument == NULL || pAction == NULL || iStart < 0 || iDeletedLength < 0 || iInsertedLength < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pAction, 0, sizeof(*pAction));
	if ( iDeletedLength > 0 ) {
		pAction->sDeleted = (char*)xrtMalloc((size_t)iDeletedLength + 1u);
		if ( pAction->sDeleted == NULL ) goto oom;
		if ( sDeleted != NULL ) {
			memcpy(pAction->sDeleted, sDeleted, (size_t)iDeletedLength);
		} else if ( __xuiCodeDocumentCopyRangeRaw(pDocument, iStart,
			iStart + iDeletedLength, pAction->sDeleted) != XUI_OK ) {
			__xuiCodeDocumentActionClear(pAction);
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		pAction->sDeleted[iDeletedLength] = '\0';
	}
	if ( iInsertedLength > 0 ) {
		pAction->sInserted = (char*)xrtMalloc((size_t)iInsertedLength + 1u);
		if ( pAction->sInserted == NULL ) goto oom;
		memcpy(pAction->sInserted, sInserted, (size_t)iInsertedLength);
		pAction->sInserted[iInsertedLength] = '\0';
	}
	pAction->iStart = iStart;
	pAction->iDeletedLength = iDeletedLength;
	pAction->iInsertedLength = iInsertedLength;
	return XUI_OK;

oom:
	__xuiCodeDocumentActionClear(pAction);
	__xuiCodeDocumentSetError(pDocument, "out of memory");
	return XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiCodeDocumentTryMergeAction(xui_code_edit_group_t* pGroup,
	const xui_code_edit_action_t* pAction)
{
	xui_code_edit_action_t* pPrevious;
	char* sMerged;
	int iMergedLength;

	if ( pGroup == NULL || pAction == NULL || pGroup->iActionCount != 1 ) return 0;
	pPrevious = &pGroup->pActions[0];
	if ( pPrevious->iDeletedLength == 0 && pAction->iDeletedLength == 0 &&
	     pPrevious->iInsertedLength > 0 && pAction->iInsertedLength > 0 &&
	     pPrevious->iStart + pPrevious->iInsertedLength == pAction->iStart ) {
		iMergedLength = pPrevious->iInsertedLength + pAction->iInsertedLength;
		sMerged = (char*)xrtRealloc(pPrevious->sInserted, (size_t)iMergedLength + 1u);
		if ( sMerged == NULL ) return 0;
		memcpy(sMerged + pPrevious->iInsertedLength, pAction->sInserted,
			(size_t)pAction->iInsertedLength);
		sMerged[iMergedLength] = '\0';
		pPrevious->sInserted = sMerged;
		pPrevious->iInsertedLength = iMergedLength;
		return 1;
	}
	if ( pPrevious->iInsertedLength == 0 && pAction->iInsertedLength == 0 &&
	     pPrevious->iDeletedLength > 0 && pAction->iDeletedLength > 0 ) {
		iMergedLength = pPrevious->iDeletedLength + pAction->iDeletedLength;
		sMerged = (char*)xrtMalloc((size_t)iMergedLength + 1u);
		if ( sMerged == NULL ) return 0;
		if ( pAction->iStart + pAction->iDeletedLength == pPrevious->iStart ) {
			memcpy(sMerged, pAction->sDeleted, (size_t)pAction->iDeletedLength);
			memcpy(sMerged + pAction->iDeletedLength, pPrevious->sDeleted,
				(size_t)pPrevious->iDeletedLength);
			pPrevious->iStart = pAction->iStart;
		} else if ( pAction->iStart == pPrevious->iStart ) {
			memcpy(sMerged, pPrevious->sDeleted, (size_t)pPrevious->iDeletedLength);
			memcpy(sMerged + pPrevious->iDeletedLength, pAction->sDeleted,
				(size_t)pAction->iDeletedLength);
		} else {
			xrtFree(sMerged);
			return 0;
		}
		sMerged[iMergedLength] = '\0';
		xrtFree(pPrevious->sDeleted);
		pPrevious->sDeleted = sMerged;
		pPrevious->iDeletedLength = iMergedLength;
		return 1;
	}
	return 0;
}

static void __xuiCodeDocumentClearRedo(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return;
	__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
	pDocument->iRedoCount = 0;
}

static void __xuiCodeDocumentSetLastEditRange(xui_code_document pDocument, int iStart, int iEnd)
{
	if ( pDocument == NULL ) return;
	if ( iStart < 0 ) iStart = 0;
	if ( iEnd < iStart ) iEnd = iStart;
	if ( iEnd > pDocument->iLength ) iEnd = pDocument->iLength;
	if ( !pDocument->bHasLastEditRange || pDocument->iEditDepth <= 0 ) {
		pDocument->iLastEditStart = iStart;
		pDocument->iLastEditEnd = iEnd;
		pDocument->bHasLastEditRange = 1;
		return;
	}
	if ( iStart < pDocument->iLastEditStart ) pDocument->iLastEditStart = iStart;
	if ( iEnd > pDocument->iLastEditEnd ) pDocument->iLastEditEnd = iEnd;
}

static void __xuiCodeDocumentNotifyChanged(xui_code_document pDocument)
{
	xui_code_range_t tRange;

	if ( pDocument == NULL ) return;
	pDocument->bChangePending = 1;
	if ( pDocument->iEditDepth > 0 ) return;
	if ( pDocument->onChange == NULL ) {
		pDocument->bChangePending = 0;
		return;
	}
	tRange.iStart = pDocument->bHasLastEditRange ? pDocument->iLastEditStart : 0;
	tRange.iEnd = pDocument->bHasLastEditRange ? pDocument->iLastEditEnd : pDocument->iLength;
	pDocument->bChangePending = 0;
	pDocument->onChange(pDocument, tRange, pDocument->iVersion, pDocument->pChangeUser);
}

XUI_API int xuiCodeDocumentCreate(xui_code_document* ppDocument)
{
	xui_code_document pDocument;
	int iRet;

	if ( ppDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	pDocument = (xui_code_document)xrtMalloc(sizeof(*pDocument));
	if ( pDocument == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pDocument, 0, sizeof(*pDocument));
	pDocument->sBuffer = (char*)xrtMalloc(4096u);
	if ( pDocument->sBuffer == NULL ) {
		xrtFree(pDocument);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->iBufferCapacity = 4096;
	pDocument->iGapStart = 0;
	pDocument->iGapEnd = 4096;
	iRet = __xuiCodeDocumentRebuildLines(pDocument);
	if ( iRet != XUI_OK ) {
		xuiCodeDocumentDestroy(pDocument);
		return iRet;
	}
	*ppDocument = pDocument;
	return XUI_OK;
}

XUI_API void xuiCodeDocumentDestroy(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return;
	xrtFree(pDocument->sBuffer);
	xrtFree(pDocument->sSnapshot);
	xrtFree(pDocument->pLines);
	__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
	__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
	__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
	xrtFree(pDocument->pUndo);
	xrtFree(pDocument->pRedo);
	xrtFree(pDocument);
}

XUI_API int xuiCodeDocumentSetText(xui_code_document pDocument, const char* sText)
{
	return xuiCodeDocumentSetTextLength(pDocument, sText, -1);
}

XUI_API int xuiCodeDocumentSetTextLength(xui_code_document pDocument, const char* sText, int iLength)
{
	char* sNormalized;
	int iNormalizedLength;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL ) {
		if ( iLength > 0 ) return XUI_ERROR_INVALID_ARGUMENT;
		sText = "";
		iLength = 0;
	}
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, iLength);
	if ( iRet != XUI_OK ) return iRet;
	sNormalized = __xuiCodeDocumentNormalizeText(sText, iLength, &iNormalizedLength);
	if ( sNormalized == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiCodeDocumentSetOwnedText(pDocument, sNormalized, iNormalizedLength);
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->iSavedVersion = pDocument->iVersion;
		pDocument->bDirty = 0;
		pDocument->iState = ++pDocument->iNextState;
		pDocument->iSavedState = pDocument->iState;
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, iNormalizedLength);
		__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
		__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
		__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		pDocument->iUndoCount = 0;
		pDocument->iRedoCount = 0;
		__xuiCodeDocumentNotifyChanged(pDocument);
		sNormalized = NULL;
	}
	xrtFree(sNormalized);
	return iRet;
}

XUI_API const char* xuiCodeDocumentGetText(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return "";
	if ( __xuiCodeDocumentEnsureSnapshot(pDocument) != XUI_OK ) return "";
	return (pDocument->sSnapshot != NULL) ? pDocument->sSnapshot : "";
}

XUI_API int xuiCodeDocumentGetByte(xui_code_document pDocument, int iOffset, char* pByte)
{
	if ( pDocument == NULL || pByte == NULL ||
	     iOffset < 0 || iOffset >= pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pByte = (char)__xuiCodeDocumentByteAt(pDocument, iOffset);
	return XUI_OK;
}

XUI_API int xuiCodeDocumentCopyRange(xui_code_document pDocument,
	int iStart, int iEnd, char* sOutput, int iCapacity, int* pLength)
{
	int iLength;
	int iRet;

	if ( pDocument == NULL || iStart < 0 || iEnd < iStart ||
	     iEnd > pDocument->iLength || iCapacity < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLength = iEnd - iStart;
	if ( pLength != NULL ) *pLength = iLength;
	if ( sOutput == NULL ) return (iCapacity == 0) ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= iLength ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiCodeDocumentCopyRangeRaw(pDocument, iStart, iEnd, sOutput);
	if ( iRet != XUI_OK ) return iRet;
	sOutput[iLength] = '\0';
	return XUI_OK;
}

XUI_API int xuiCodeDocumentLoadTextFile(xui_code_document pDocument, const char* sPath, int iCharset)
{
	char* sText;
	size_t iSize;
	int iLength;
	int iRet;

	if ( (pDocument == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iSize = 0;
	sText = (char*)xrtFileReadAll((str)sPath, iCharset, &iSize);
	if ( sText == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "file read failed");
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	if ( iSize > (size_t)INT_MAX ) {
		__xuiCodeDocumentSetError(pDocument, "file too large");
		xrtFree(sText);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iLength = (int)iSize;
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, iLength);
	if ( iRet == XUI_OK ) iRet = __xuiCodeDocumentNormalizeTextInPlace(sText, iLength, &iLength);
	if ( iRet == XUI_OK ) {
		iRet = __xuiCodeDocumentSetOwnedText(pDocument, sText, iLength);
		if ( iRet == XUI_OK ) {
			pDocument->iVersion++;
			pDocument->iChangeVersion++;
			pDocument->iSavedVersion = pDocument->iVersion;
			pDocument->bDirty = 0;
			pDocument->iState = ++pDocument->iNextState;
			pDocument->iSavedState = pDocument->iState;
			__xuiCodeDocumentSetLastEditRange(pDocument, 0, iLength);
			__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
			__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
			__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
			pDocument->iUndoCount = 0;
			pDocument->iRedoCount = 0;
			__xuiCodeDocumentNotifyChanged(pDocument);
			sText = NULL;
		}
	}
	xrtFree(sText);
	return iRet;
}

XUI_API int xuiCodeDocumentSaveTextFile(xui_code_document pDocument, const char* sPath, int iCharset)
{
	int iRet;

	if ( (pDocument == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xrtFileWriteAll((str)sPath, (str)xuiCodeDocumentGetText(pDocument), (size_t)pDocument->iLength, iCharset);
	if ( iRet == 0 && pDocument->iLength > 0 ) {
		__xuiCodeDocumentSetError(pDocument, "file write failed");
		return XUI_ERROR_RESOURCE_FAILED;
	}
	return XUI_OK;
}

XUI_API int xuiCodeDocumentGetLength(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iLength;
}

XUI_API int xuiCodeDocumentGetLineCount(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iLineCount;
}

XUI_API int xuiCodeDocumentGetLineRange(xui_code_document pDocument, int iLine, int* pStart, int* pEnd)
{
	if ( (pDocument == NULL) || (iLine < 0) || (iLine >= pDocument->iLineCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pStart != NULL ) *pStart = __xuiCodeDocumentLineField(pDocument, iLine, 0);
	if ( pEnd != NULL ) *pEnd = __xuiCodeDocumentLineField(pDocument, iLine, 2);
	return XUI_OK;
}

XUI_API int xuiCodeDocumentOffsetToLineColumn(xui_code_document pDocument, int iOffset, int* pLine, int* pColumn)
{
	int iLow;
	int iHigh;
	int iMid;
	int iLine;
	int iPos;
	int iColumn;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iOffset = __xuiCodeDocumentClampOffset(pDocument, iOffset);
	iLow = 0;
	iHigh = pDocument->iLineCount - 1;
	iLine = 0;
	while ( iLow <= iHigh ) {
		iMid = (iLow + iHigh) / 2;
		if ( iOffset < __xuiCodeDocumentLineField(pDocument, iMid, 0) ) {
			iHigh = iMid - 1;
		} else if ( (iOffset >= __xuiCodeDocumentLineField(pDocument, iMid, 1)) &&
		            (iMid + 1 < pDocument->iLineCount) ) {
			iLow = iMid + 1;
		} else {
			iLine = iMid;
			break;
		}
	}
	if ( iLow > iHigh ) iLine = (iLow < pDocument->iLineCount) ? iLow : (pDocument->iLineCount - 1);
	if ( pLine != NULL ) *pLine = iLine;
	if ( pColumn != NULL ) {
		iColumn = 0;
		iPos = __xuiCodeDocumentLineField(pDocument, iLine, 0);
		while ( iPos < iOffset &&
		        iPos < __xuiCodeDocumentLineField(pDocument, iLine, 2) ) {
			iPos = __xuiCodeDocumentUtf8Next(pDocument, iPos);
			iColumn++;
		}
		*pColumn = iColumn;
	}
	return XUI_OK;
}

XUI_API int xuiCodeDocumentLineColumnToOffset(xui_code_document pDocument, int iLine, int iColumn, int* pOffset)
{
	int iOffset;
	int i;

	if ( (pDocument == NULL) || (iLine < 0) || (iLine >= pDocument->iLineCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iColumn < 0 ) iColumn = 0;
	iOffset = __xuiCodeDocumentLineField(pDocument, iLine, 0);
	for ( i = 0; i < iColumn &&
	     iOffset < __xuiCodeDocumentLineField(pDocument, iLine, 2); i++ ) {
		iOffset = __xuiCodeDocumentUtf8Next(pDocument, iOffset);
	}
	iOffset = __xuiCodeDocumentClampOffset(pDocument, iOffset);
	if ( pOffset != NULL ) *pOffset = iOffset;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentReplace(xui_code_document pDocument, int iStart, int iEnd, const char* sText)
{
	xui_code_edit_action_t tAction;
	xui_code_edit_group_t* pGroup;
	char* sNormalized;
	int iInsertLength;
	int iDeletedLength;
	int bStandalone;
	int bMerged;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, -1);
	if ( iRet != XUI_OK ) return iRet;
	iStart = __xuiCodeDocumentClampOffset(pDocument, iStart);
	iEnd = __xuiCodeDocumentClampOffset(pDocument, iEnd);
	if ( iEnd < iStart ) {
		int iTemp = iStart;
		iStart = iEnd;
		iEnd = iTemp;
	}
	sNormalized = __xuiCodeDocumentNormalizeText(sText, -1, &iInsertLength);
	if ( sNormalized == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iDeletedLength = iEnd - iStart;
	if ( iDeletedLength == 0 && iInsertLength == 0 ) {
		xrtFree(sNormalized);
		return XUI_OK;
	}
	memset(&tAction, 0, sizeof(tAction));
	iRet = __xuiCodeDocumentActionInit(pDocument, &tAction, iStart,
		NULL, iDeletedLength, sNormalized, iInsertLength);
	if ( iRet != XUI_OK ) {
		xrtFree(sNormalized);
		return iRet;
	}
	bStandalone = (pDocument->iEditDepth == 0);
	bMerged = 0;
	if ( bStandalone ) {
		iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
			&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
		if ( iRet != XUI_OK ) goto cleanup;
		pGroup = &pDocument->pUndo[pDocument->iUndoCount];
		__xuiCodeDocumentGroupClear(pGroup);
		pGroup->iBeforeState = pDocument->iState;
	} else {
		pGroup = &pDocument->tEditGroup;
	}
	iRet = __xuiCodeDocumentGroupReserveActions(pDocument, pGroup, pGroup->iActionCount + 1);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiCodeDocumentReplaceRaw(pDocument, iStart, iEnd, sNormalized, iInsertLength);
	if ( iRet != XUI_OK ) goto cleanup;
	if ( bStandalone && pDocument->iRedoCount == 0 && pDocument->iUndoCount > 0 ) {
		xui_code_edit_group_t* pPrevious = &pDocument->pUndo[pDocument->iUndoCount - 1];
		if ( pPrevious->iAfterState != pDocument->iSavedState &&
		     __xuiCodeDocumentTryMergeAction(pPrevious, &tAction) ) {
			pGroup = pPrevious;
			bMerged = 1;
		}
	}
	if ( !bMerged ) {
		pGroup->pActions[pGroup->iActionCount++] = tAction;
		memset(&tAction, 0, sizeof(tAction));
	}
	__xuiCodeDocumentClearRedo(pDocument);
	pDocument->iVersion++;
	pDocument->iChangeVersion++;
	pDocument->iState = ++pDocument->iNextState;
	pGroup->iAfterState = pDocument->iState;
	pDocument->bDirty = (pDocument->iState != pDocument->iSavedState);
	if ( bStandalone && !bMerged ) pDocument->iUndoCount++;
	__xuiCodeDocumentSetLastEditRange(pDocument, iStart, iStart + iInsertLength);
	__xuiCodeDocumentNotifyChanged(pDocument);

cleanup:
	__xuiCodeDocumentActionClear(&tAction);
	xrtFree(sNormalized);
	return iRet;
}

XUI_API int xuiCodeDocumentInsert(xui_code_document pDocument, int iOffset, const char* sText)
{
	return xuiCodeDocumentReplace(pDocument, iOffset, iOffset, sText);
}

XUI_API int xuiCodeDocumentDelete(xui_code_document pDocument, int iStart, int iEnd)
{
	return xuiCodeDocumentReplace(pDocument, iStart, iEnd, "");
}

XUI_API int xuiCodeDocumentBeginEdit(xui_code_document pDocument)
{
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iEditDepth == 0 ) {
		iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
			&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
		if ( iRet != XUI_OK ) return iRet;
		__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		pDocument->tEditGroup.iBeforeState = pDocument->iState;
		pDocument->bHasLastEditRange = 0;
	}
	pDocument->iEditDepth++;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentEndEdit(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->iEditDepth--;
	if ( pDocument->iEditDepth == 0 ) {
		if ( pDocument->tEditGroup.iActionCount > 0 ) {
			pDocument->pUndo[pDocument->iUndoCount++] = pDocument->tEditGroup;
			memset(&pDocument->tEditGroup, 0, sizeof(pDocument->tEditGroup));
		} else {
			__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		}
		if ( pDocument->bChangePending ) __xuiCodeDocumentNotifyChanged(pDocument);
	}
	return XUI_OK;
}

XUI_API int xuiCodeDocumentSetChangeCallback(xui_code_document pDocument, xui_code_document_change_proc onChange, void* pUser)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->onChange = onChange;
	pDocument->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentUndo(xui_code_document pDocument)
{
	xui_code_edit_group_t* pGroup;
	xui_code_edit_action_t* pAction;
	int i;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth > 0 || pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iUndoCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pRedo,
		&pDocument->iRedoCapacity, pDocument->iRedoCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pGroup = &pDocument->pUndo[pDocument->iUndoCount - 1];
	pDocument->bApplyingHistory = 1;
	iRet = XUI_OK;
	for ( i = pGroup->iActionCount - 1; i >= 0; i-- ) {
		pAction = &pGroup->pActions[i];
		iRet = __xuiCodeDocumentReplaceRaw(pDocument,
			pAction->iStart,
			pAction->iStart + pAction->iInsertedLength,
			(pAction->sDeleted != NULL) ? pAction->sDeleted : "",
			pAction->iDeletedLength);
		if ( iRet != XUI_OK ) break;
	}
	pDocument->bApplyingHistory = 0;
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->iState = pGroup->iBeforeState;
		pDocument->bDirty = (pDocument->iState != pDocument->iSavedState);
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, pDocument->iLength);
		pDocument->pRedo[pDocument->iRedoCount++] = *pGroup;
		memset(pGroup, 0, sizeof(*pGroup));
		pDocument->iUndoCount--;
		__xuiCodeDocumentNotifyChanged(pDocument);
	}
	return iRet;
}

XUI_API int xuiCodeDocumentRedo(xui_code_document pDocument)
{
	xui_code_edit_group_t* pGroup;
	xui_code_edit_action_t* pAction;
	int i;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth > 0 || pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iRedoCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
		&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pGroup = &pDocument->pRedo[pDocument->iRedoCount - 1];
	pDocument->bApplyingHistory = 1;
	iRet = XUI_OK;
	for ( i = 0; i < pGroup->iActionCount; i++ ) {
		pAction = &pGroup->pActions[i];
		iRet = __xuiCodeDocumentReplaceRaw(pDocument,
			pAction->iStart,
			pAction->iStart + pAction->iDeletedLength,
			(pAction->sInserted != NULL) ? pAction->sInserted : "",
			pAction->iInsertedLength);
		if ( iRet != XUI_OK ) break;
	}
	pDocument->bApplyingHistory = 0;
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->iState = pGroup->iAfterState;
		pDocument->bDirty = (pDocument->iState != pDocument->iSavedState);
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, pDocument->iLength);
		pDocument->pUndo[pDocument->iUndoCount++] = *pGroup;
		memset(pGroup, 0, sizeof(*pGroup));
		pDocument->iRedoCount--;
		__xuiCodeDocumentNotifyChanged(pDocument);
	}
	return iRet;
}

XUI_API int xuiCodeDocumentCanUndo(xui_code_document pDocument)
{
	return (pDocument != NULL) && (pDocument->iUndoCount > 0);
}

XUI_API int xuiCodeDocumentCanRedo(xui_code_document pDocument)
{
	return (pDocument != NULL) && (pDocument->iRedoCount > 0);
}

XUI_API uint32_t xuiCodeDocumentGetVersion(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->iVersion : 0u;
}

XUI_API uint32_t xuiCodeDocumentGetChangeVersion(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->iChangeVersion : 0u;
}

XUI_API int xuiCodeDocumentGetLastEditRange(xui_code_document pDocument, xui_code_range_t* pRange)
{
	if ( (pDocument == NULL) || (pRange == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pDocument->bHasLastEditRange ) return XUI_ERROR_UNSUPPORTED;
	pRange->iStart = pDocument->iLastEditStart;
	pRange->iEnd = pDocument->iLastEditEnd;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentGetDirty(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->bDirty : 0;
}

XUI_API int xuiCodeDocumentSetDirty(xui_code_document pDocument, int bDirty)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->bDirty = bDirty ? 1 : 0;
	if ( !pDocument->bDirty ) {
		pDocument->iSavedVersion = pDocument->iVersion;
		pDocument->iSavedState = pDocument->iState;
	}
	return XUI_OK;
}

XUI_API const char* xuiCodeDocumentGetLastError(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return "invalid document";
	return pDocument->sError;
}
