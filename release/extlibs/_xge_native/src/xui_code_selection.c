#include "xui_internal.h"

#include <ctype.h>
#include <string.h>

struct xui_code_selection_model_t {
	xui_code_selection_t tPrimary;
	xui_code_selection_t* pExtra;
	int iExtraCount;
	int iExtraCapacity;
};

#define XUI_CODE_SELECTION_EXTRA_INITIAL 4

static int __xuiCodeSelectionClamp(xui_code_document pDocument, int iOffset)
{
	int iLength;
	int iLine;
	int iColumn;
	int iClamped;

	if ( pDocument == NULL ) return 0;
	iLength = xuiCodeDocumentGetLength(pDocument);
	if ( iOffset < 0 ) return 0;
	if ( iOffset > iLength ) return iLength;
	if ( xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, &iLine, &iColumn) != XUI_OK ) return iOffset;
	if ( xuiCodeDocumentLineColumnToOffset(pDocument, iLine, iColumn, &iClamped) != XUI_OK ) return iOffset;
	return iClamped;
}

static char __xuiCodeSelectionByteAt(xui_code_document pDocument, int iOffset)
{
	char c;

	c = '\0';
	(void)xuiCodeDocumentGetByte(pDocument, iOffset, &c);
	return c;
}

static int __xuiCodeSelectionUnicodeRead(void* pUser, int iOffset, unsigned char* pByte)
{
	char c;

	if ( pUser == NULL || pByte == NULL ) return 0;
	c = '\0';
	if ( xuiCodeDocumentGetByte((xui_code_document)pUser, iOffset, &c) != XUI_OK ) return 0;
	*pByte = (unsigned char)c;
	return 1;
}

static int __xuiCodeSelectionUtf8Next(xui_code_document pDocument, int iLength, int iOffset)
{
	if ( pDocument == NULL ) return 0;
	return xuiInternalTextGraphemeNextRead(__xuiCodeSelectionUnicodeRead,
		pDocument, iLength, iOffset);
}

static int __xuiCodeSelectionUtf8Prev(xui_code_document pDocument, int iLength, int iOffset)
{
	if ( pDocument == NULL ) return 0;
	return xuiInternalTextGraphemePrevRead(__xuiCodeSelectionUnicodeRead,
		pDocument, iLength, iOffset);
}

static int __xuiCodeSelectionWordChar(char c)
{
	unsigned char ch = (unsigned char)c;
	return isalnum(ch) || ch == '_';
}

static int __xuiCodeSelectionMoveWordLeft(xui_code_document pDocument, int iOffset)
{
	int iPrevious;

	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	while ( iOffset > 0 ) {
		iPrevious = __xuiCodeSelectionUtf8Prev(pDocument, xuiCodeDocumentGetLength(pDocument), iOffset);
		if ( __xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iPrevious)) ) break;
		iOffset = iPrevious;
	}
	while ( iOffset > 0 ) {
		iPrevious = __xuiCodeSelectionUtf8Prev(pDocument, xuiCodeDocumentGetLength(pDocument), iOffset);
		if ( !__xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iPrevious)) ) break;
		iOffset = iPrevious;
	}
	return iOffset;
}

static int __xuiCodeSelectionMoveWordRight(xui_code_document pDocument, int iOffset)
{
	int iLength;
	int iNext;

	iLength = xuiCodeDocumentGetLength(pDocument);
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	while ( iOffset < iLength &&
	        __xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset)) ) {
		iNext = __xuiCodeSelectionUtf8Next(pDocument, iLength, iOffset);
		if ( iNext <= iOffset ) break;
		iOffset = iNext;
	}
	while ( iOffset < iLength &&
	        !__xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset)) ) {
		iNext = __xuiCodeSelectionUtf8Next(pDocument, iLength, iOffset);
		if ( iNext <= iOffset ) break;
		iOffset = iNext;
	}
	return iOffset;
}

static int __xuiCodeSelectionWordStart(xui_code_document pDocument, int iOffset)
{
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	if ( iOffset >= xuiCodeDocumentGetLength(pDocument) ||
	     !__xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset)) ) return iOffset;
	while ( iOffset > 0 && __xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset - 1)) ) iOffset--;
	return iOffset;
}

static int __xuiCodeSelectionWordEnd(xui_code_document pDocument, int iOffset)
{
	int iLength;

	iLength = xuiCodeDocumentGetLength(pDocument);
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	if ( iOffset >= iLength || !__xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset)) ) return iOffset;
	while ( iOffset < iLength && __xuiCodeSelectionWordChar(__xuiCodeSelectionByteAt(pDocument, iOffset)) ) iOffset++;
	return iOffset;
}

static void __xuiCodeSelectionApplyCaret(xui_code_selection_model pSelection, xui_code_document pDocument, int iOffset, int bExtend, int iPreferredColumn)
{
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	if ( !bExtend ) pSelection->iExtraCount = 0;
	if ( !bExtend ) pSelection->tPrimary.iAnchorOffset = iOffset;
	pSelection->tPrimary.iCaretOffset = iOffset;
	pSelection->tPrimary.iPreferredColumn = iPreferredColumn;
	pSelection->tPrimary.iFlags &= ~(XUI_CODE_SELECTION_REVERSED | XUI_CODE_SELECTION_INACTIVE);
	if ( pSelection->tPrimary.iCaretOffset < pSelection->tPrimary.iAnchorOffset ) {
		pSelection->tPrimary.iFlags |= XUI_CODE_SELECTION_REVERSED;
	}
}

static void __xuiCodeSelectionNormalizeState(xui_code_selection_t* pState)
{
	if ( pState == NULL ) return;
	pState->iSize = sizeof(*pState);
	pState->iFlags &= ~(XUI_CODE_SELECTION_REVERSED);
	if ( pState->iCaretOffset < pState->iAnchorOffset ) {
		pState->iFlags |= XUI_CODE_SELECTION_REVERSED;
	}
}

static int __xuiCodeSelectionEnsureExtra(xui_code_selection_model pSelection, int iNeeded)
{
	xui_code_selection_t* pNew;
	int iCapacity;

	if ( pSelection == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeeded <= pSelection->iExtraCapacity ) return XUI_OK;
	iCapacity = pSelection->iExtraCapacity;
	if ( iCapacity <= 0 ) iCapacity = XUI_CODE_SELECTION_EXTRA_INITIAL;
	while ( iCapacity < iNeeded ) iCapacity *= 2;
	pNew = (xui_code_selection_t*)xrtRealloc(pSelection->pExtra, sizeof(xui_code_selection_t) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pSelection->pExtra = pNew;
	pSelection->iExtraCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeSelectionLineOffset(xui_code_document pDocument, int iLine, int iColumn, int* pOffset)
{
	int iLineCount;
	int iRet;

	iLineCount = xuiCodeDocumentGetLineCount(pDocument);
	if ( iLine < 0 ) iLine = 0;
	if ( iLine >= iLineCount ) iLine = iLineCount - 1;
	if ( iColumn < 0 ) iColumn = 0;
	iRet = xuiCodeDocumentLineColumnToOffset(pDocument, iLine, iColumn, pOffset);
	return iRet;
}

XUI_API int xuiCodeSelectionCreate(xui_code_selection_model* ppSelection)
{
	xui_code_selection_model pSelection;

	if ( ppSelection == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppSelection = NULL;
	pSelection = (xui_code_selection_model)xrtMalloc(sizeof(*pSelection));
	if ( pSelection == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pSelection, 0, sizeof(*pSelection));
	pSelection->tPrimary.iSize = sizeof(pSelection->tPrimary);
	*ppSelection = pSelection;
	return XUI_OK;
}

XUI_API void xuiCodeSelectionDestroy(xui_code_selection_model pSelection)
{
	if ( pSelection == NULL ) return;
	xrtFree(pSelection->pExtra);
	xrtFree(pSelection);
}

XUI_API void xuiCodeSelectionClear(xui_code_selection_model pSelection)
{
	if ( pSelection == NULL ) return;
	memset(&pSelection->tPrimary, 0, sizeof(pSelection->tPrimary));
	pSelection->tPrimary.iSize = sizeof(pSelection->tPrimary);
	pSelection->iExtraCount = 0;
}

XUI_API int xuiCodeSelectionSetRange(xui_code_selection_model pSelection, xui_code_document pDocument, int iAnchorOffset, int iCaretOffset)
{
	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSelection->tPrimary.iSize = sizeof(pSelection->tPrimary);
	pSelection->tPrimary.iAnchorOffset = __xuiCodeSelectionClamp(pDocument, iAnchorOffset);
	pSelection->tPrimary.iCaretOffset = __xuiCodeSelectionClamp(pDocument, iCaretOffset);
	xuiCodeDocumentOffsetToLineColumn(pDocument, pSelection->tPrimary.iCaretOffset, NULL, &pSelection->tPrimary.iPreferredColumn);
	pSelection->tPrimary.iFlags = 0;
	if ( pSelection->tPrimary.iCaretOffset < pSelection->tPrimary.iAnchorOffset ) {
		pSelection->tPrimary.iFlags |= XUI_CODE_SELECTION_REVERSED;
	}
	pSelection->iExtraCount = 0;
	return XUI_OK;
}

XUI_API int xuiCodeSelectionSetState(xui_code_selection_model pSelection, xui_code_document pDocument, const xui_code_selection_t* pState)
{
	xui_code_selection_t tState;

	if ( (pSelection == NULL) || (pDocument == NULL) || (pState == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pState->iAnchorOffset < 0 || pState->iCaretOffset < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	tState = *pState;
	tState.iSize = sizeof(tState);
	tState.iAnchorOffset = __xuiCodeSelectionClamp(pDocument, tState.iAnchorOffset);
	tState.iCaretOffset = __xuiCodeSelectionClamp(pDocument, tState.iCaretOffset);
	if ( tState.iPreferredColumn < 0 ) {
		xuiCodeDocumentOffsetToLineColumn(pDocument, tState.iCaretOffset, NULL, &tState.iPreferredColumn);
	}
	__xuiCodeSelectionNormalizeState(&tState);
	tState.iFlags &= ~XUI_CODE_SELECTION_INACTIVE;
	pSelection->tPrimary = tState;
	pSelection->iExtraCount = 0;
	return XUI_OK;
}

XUI_API int xuiCodeSelectionGetRange(xui_code_selection_model pSelection, int* pStart, int* pEnd)
{
	int iStart;
	int iEnd;

	if ( (pSelection == NULL) || (pStart == NULL) || (pEnd == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iStart = pSelection->tPrimary.iAnchorOffset;
	iEnd = pSelection->tPrimary.iCaretOffset;
	if ( iEnd < iStart ) {
		int iTemp = iStart;
		iStart = iEnd;
		iEnd = iTemp;
	}
	*pStart = iStart;
	*pEnd = iEnd;
	return XUI_OK;
}

XUI_API int xuiCodeSelectionGetState(xui_code_selection_model pSelection, xui_code_selection_t* pState)
{
	if ( (pSelection == NULL) || (pState == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pState = pSelection->tPrimary;
	pState->iSize = sizeof(*pState);
	return XUI_OK;
}

XUI_API int xuiCodeSelectionHasSelection(xui_code_selection_model pSelection)
{
	int i;

	if ( pSelection == NULL ) return 0;
	if ( pSelection->tPrimary.iAnchorOffset != pSelection->tPrimary.iCaretOffset ) return 1;
	for ( i = 0; i < pSelection->iExtraCount; i++ ) {
		if ( pSelection->pExtra[i].iAnchorOffset != pSelection->pExtra[i].iCaretOffset ) return 1;
	}
	return 0;
}

XUI_API int xuiCodeSelectionGetCount(xui_code_selection_model pSelection)
{
	return (pSelection != NULL) ? (1 + pSelection->iExtraCount) : 0;
}

XUI_API int xuiCodeSelectionGetAt(xui_code_selection_model pSelection, int iIndex, xui_code_selection_t* pState)
{
	if ( (pSelection == NULL) || (pState == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iIndex < 0 || iIndex >= xuiCodeSelectionGetCount(pSelection) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iIndex == XUI_CODE_SELECTION_PRIMARY ) return xuiCodeSelectionGetState(pSelection, pState);
	*pState = pSelection->pExtra[iIndex - 1];
	pState->iSize = sizeof(*pState);
	return XUI_OK;
}

XUI_API int xuiCodeSelectionAdd(xui_code_selection_model pSelection, const xui_code_selection_t* pState)
{
	xui_code_selection_t tState;
	int iRet;

	if ( (pSelection == NULL) || (pState == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pState->iAnchorOffset < 0 || pState->iCaretOffset < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeSelectionEnsureExtra(pSelection, pSelection->iExtraCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	tState = *pState;
	__xuiCodeSelectionNormalizeState(&tState);
	tState.iFlags |= XUI_CODE_SELECTION_INACTIVE;
	pSelection->pExtra[pSelection->iExtraCount++] = tState;
	return XUI_OK;
}

XUI_API int xuiCodeSelectionSelectAll(xui_code_selection_model pSelection, xui_code_document pDocument)
{
	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiCodeSelectionSetRange(pSelection, pDocument, 0, xuiCodeDocumentGetLength(pDocument));
}

XUI_API int xuiCodeSelectionSelectWord(xui_code_selection_model pSelection, xui_code_document pDocument, int iOffset)
{
	int iStart;
	int iEnd;

	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iStart = __xuiCodeSelectionWordStart(pDocument, iOffset);
	iEnd = __xuiCodeSelectionWordEnd(pDocument, iOffset);
	return xuiCodeSelectionSetRange(pSelection, pDocument, iStart, iEnd);
}

XUI_API int xuiCodeSelectionSelectLine(xui_code_selection_model pSelection, xui_code_document pDocument, int iLine, int bIncludeLineBreak)
{
	int iLineCount;
	int iStart;
	int iEnd;
	int iRet;

	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iLineCount = xuiCodeDocumentGetLineCount(pDocument);
	if ( iLine < 0 ) iLine = 0;
	if ( iLine >= iLineCount ) iLine = iLineCount - 1;
	iRet = xuiCodeDocumentGetLineRange(pDocument, iLine, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( bIncludeLineBreak ) {
		if ( iLine + 1 < iLineCount ) {
			iRet = xuiCodeDocumentGetLineRange(pDocument, iLine + 1, &iEnd, NULL);
			if ( iRet != XUI_OK ) return iRet;
		} else {
			iEnd = xuiCodeDocumentGetLength(pDocument);
		}
	}
	return xuiCodeSelectionSetRange(pSelection, pDocument, iStart, iEnd);
}

XUI_API int xuiCodeSelectionGotoOffset(xui_code_selection_model pSelection, xui_code_document pDocument, int iOffset, int bExtend)
{
	int iColumn;

	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, NULL, &iColumn);
	__xuiCodeSelectionApplyCaret(pSelection, pDocument, iOffset, bExtend, iColumn);
	return XUI_OK;
}

XUI_API int xuiCodeSelectionGotoLineColumn(xui_code_selection_model pSelection, xui_code_document pDocument, int iLine, int iColumn, int bExtend)
{
	int iOffset;
	int iRet;

	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeSelectionLineOffset(pDocument, iLine, iColumn, &iOffset);
	if ( iRet != XUI_OK ) return iRet;
	__xuiCodeSelectionApplyCaret(pSelection, pDocument, iOffset, bExtend, iColumn);
	return XUI_OK;
}

XUI_API int xuiCodeSelectionMove(xui_code_selection_model pSelection, xui_code_document pDocument, int iCommand)
{
	int iLine;
	int iColumn;
	int iOffset;
	int iLength;
	int bExtend;
	int iRet;

	if ( (pSelection == NULL) || (pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOffset = pSelection->tPrimary.iCaretOffset;
	iLength = xuiCodeDocumentGetLength(pDocument);
	bExtend = 0;
	if ( iCommand == XUI_CODE_COMMAND_SELECT_LEFT ) { iCommand = XUI_CODE_COMMAND_MOVE_LEFT; bExtend = 1; }
	else if ( iCommand == XUI_CODE_COMMAND_SELECT_RIGHT ) { iCommand = XUI_CODE_COMMAND_MOVE_RIGHT; bExtend = 1; }
	else if ( iCommand == XUI_CODE_COMMAND_SELECT_UP ) { iCommand = XUI_CODE_COMMAND_MOVE_UP; bExtend = 1; }
	else if ( iCommand == XUI_CODE_COMMAND_SELECT_DOWN ) { iCommand = XUI_CODE_COMMAND_MOVE_DOWN; bExtend = 1; }
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_MOVE_LEFT:
		iOffset = __xuiCodeSelectionUtf8Prev(pDocument, iLength, iOffset);
		break;
	case XUI_CODE_COMMAND_MOVE_RIGHT:
		iOffset = __xuiCodeSelectionUtf8Next(pDocument, iLength, iOffset);
		break;
	case XUI_CODE_COMMAND_MOVE_WORD_LEFT:
		iOffset = __xuiCodeSelectionMoveWordLeft(pDocument, iOffset);
		break;
	case XUI_CODE_COMMAND_MOVE_WORD_RIGHT:
		iOffset = __xuiCodeSelectionMoveWordRight(pDocument, iOffset);
		break;
	case XUI_CODE_COMMAND_MOVE_LINE_START:
		iRet = xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, &iLine, NULL);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiCodeDocumentGetLineRange(pDocument, iLine, &iOffset, NULL);
		if ( iRet != XUI_OK ) return iRet;
		break;
	case XUI_CODE_COMMAND_MOVE_LINE_END:
		iRet = xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, &iLine, NULL);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiCodeDocumentGetLineRange(pDocument, iLine, NULL, &iOffset);
		if ( iRet != XUI_OK ) return iRet;
		break;
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_START:
		iOffset = 0;
		break;
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_END:
		iOffset = iLength;
		break;
	case XUI_CODE_COMMAND_MOVE_UP:
	case XUI_CODE_COMMAND_MOVE_DOWN:
		iRet = xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, &iLine, &iColumn);
		if ( iRet != XUI_OK ) return iRet;
		if ( pSelection->tPrimary.iPreferredColumn >= 0 ) iColumn = pSelection->tPrimary.iPreferredColumn;
		if ( iCommand == XUI_CODE_COMMAND_MOVE_UP ) iLine--;
		else iLine++;
		iRet = __xuiCodeSelectionLineOffset(pDocument, iLine, iColumn, &iOffset);
		if ( iRet != XUI_OK ) return iRet;
		__xuiCodeSelectionApplyCaret(pSelection, pDocument, iOffset, bExtend, iColumn);
		return XUI_OK;
	case XUI_CODE_COMMAND_SELECT_ALL:
		return xuiCodeSelectionSelectAll(pSelection, pDocument);
	default:
		return XUI_ERROR_UNSUPPORTED;
	}
	iOffset = __xuiCodeSelectionClamp(pDocument, iOffset);
	xuiCodeDocumentOffsetToLineColumn(pDocument, iOffset, NULL, &iColumn);
	__xuiCodeSelectionApplyCaret(pSelection, pDocument, iOffset, bExtend, iColumn);
	return XUI_OK;
}
