#include "xui_internal.h"

#include <string.h>

struct xui_code_command_map_t {
	xui_code_key_binding_t* pBindings;
	int iCount;
	int iCapacity;
};

static int __xuiCodeCommandMapReserve(xui_code_command_map pMap, int iCapacity)
{
	xui_code_key_binding_t* pNew;

	if ( pMap == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pMap->iCapacity ) return XUI_OK;
	if ( iCapacity < pMap->iCapacity * 2 ) iCapacity = pMap->iCapacity * 2;
	if ( iCapacity < 16 ) iCapacity = 16;
	pNew = (xui_code_key_binding_t*)xrtRealloc(pMap->pBindings, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pMap->pBindings = pNew;
	pMap->iCapacity = iCapacity;
	return XUI_OK;
}

XUI_API int xuiCodeCommandMapCreate(xui_code_command_map* ppMap)
{
	xui_code_command_map pMap;
	int iRet;

	if ( ppMap == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppMap = NULL;
	pMap = (xui_code_command_map)xrtMalloc(sizeof(*pMap));
	if ( pMap == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pMap, 0, sizeof(*pMap));
	iRet = xuiCodeCommandMapLoadDefaults(pMap);
	if ( iRet != XUI_OK ) {
		xuiCodeCommandMapDestroy(pMap);
		return iRet;
	}
	*ppMap = pMap;
	return XUI_OK;
}

XUI_API void xuiCodeCommandMapDestroy(xui_code_command_map pMap)
{
	if ( pMap == NULL ) return;
	xrtFree(pMap->pBindings);
	xrtFree(pMap);
}

XUI_API int xuiCodeCommandMapBind(xui_code_command_map pMap, int iKey, uint32_t iModifiers, int iCommand)
{
	int i;
	int iRet;
	xui_code_key_binding_t* pBinding;

	if ( pMap == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pMap->iCount; i++ ) {
		if ( pMap->pBindings[i].iKey == iKey && pMap->pBindings[i].iModifiers == iModifiers ) {
			pMap->pBindings[i].iCommand = iCommand;
			return XUI_OK;
		}
	}
	iRet = __xuiCodeCommandMapReserve(pMap, pMap->iCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pBinding = &pMap->pBindings[pMap->iCount++];
	memset(pBinding, 0, sizeof(*pBinding));
	pBinding->iSize = sizeof(*pBinding);
	pBinding->iKey = iKey;
	pBinding->iModifiers = iModifiers;
	pBinding->iCommand = iCommand;
	return XUI_OK;
}

XUI_API int xuiCodeCommandMapUnbind(xui_code_command_map pMap, int iKey, uint32_t iModifiers)
{
	int i;

	if ( pMap == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pMap->iCount; i++ ) {
		if ( pMap->pBindings[i].iKey == iKey && pMap->pBindings[i].iModifiers == iModifiers ) {
			memmove(&pMap->pBindings[i], &pMap->pBindings[i + 1], sizeof(pMap->pBindings[i]) * (size_t)(pMap->iCount - i - 1));
			pMap->iCount--;
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeCommandMapFind(xui_code_command_map pMap, int iKey, uint32_t iModifiers, int* pCommand)
{
	int i;

	if ( (pMap == NULL) || (pCommand == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = pMap->iCount - 1; i >= 0; i-- ) {
		if ( pMap->pBindings[i].iKey == iKey && pMap->pBindings[i].iModifiers == iModifiers ) {
			*pCommand = pMap->pBindings[i].iCommand;
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeCommandMapGetCount(xui_code_command_map pMap)
{
	return (pMap != NULL) ? pMap->iCount : 0;
}

XUI_API int xuiCodeCommandMapGetBinding(xui_code_command_map pMap, int iIndex, xui_code_key_binding_t* pBinding)
{
	if ( (pMap == NULL) || (pBinding == NULL) || (iIndex < 0) || (iIndex >= pMap->iCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pBinding = pMap->pBindings[iIndex];
	pBinding->iSize = sizeof(*pBinding);
	return XUI_OK;
}

XUI_API int xuiCodeCommandMapLoadDefaults(xui_code_command_map pMap)
{
	if ( pMap == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pMap->iCount = 0;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_LEFT, 0, XUI_CODE_COMMAND_MOVE_LEFT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_RIGHT, 0, XUI_CODE_COMMAND_MOVE_RIGHT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_UP, 0, XUI_CODE_COMMAND_MOVE_UP) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_DOWN, 0, XUI_CODE_COMMAND_MOVE_DOWN) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_LEFT, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_LEFT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_RIGHT, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_RIGHT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_UP, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_UP) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_DOWN, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_DOWN) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_HOME, 0, XUI_CODE_COMMAND_MOVE_LINE_START) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_END, 0, XUI_CODE_COMMAND_MOVE_LINE_END) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_PAGE_UP, 0, XUI_CODE_COMMAND_MOVE_PAGE_UP) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_PAGE_DOWN, 0, XUI_CODE_COMMAND_MOVE_PAGE_DOWN) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_PAGE_UP, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_PAGE_UP) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_PAGE_DOWN, XUI_MOD_SHIFT, XUI_CODE_COMMAND_SELECT_PAGE_DOWN) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_HOME, XUI_MOD_CTRL, XUI_CODE_COMMAND_MOVE_DOCUMENT_START) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_END, XUI_MOD_CTRL, XUI_CODE_COMMAND_MOVE_DOCUMENT_END) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_BACKSPACE, 0, XUI_CODE_COMMAND_DELETE_BACK) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_DELETE, 0, XUI_CODE_COMMAND_DELETE_FORWARD) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_ENTER, 0, XUI_CODE_COMMAND_INSERT_NEWLINE) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_TAB, 0, XUI_CODE_COMMAND_INSERT_TAB) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_TAB, XUI_MOD_SHIFT, XUI_CODE_COMMAND_OUTDENT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'A', XUI_MOD_CTRL, XUI_CODE_COMMAND_SELECT_ALL) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'C', XUI_MOD_CTRL, XUI_CODE_COMMAND_COPY) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'X', XUI_MOD_CTRL, XUI_CODE_COMMAND_CUT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'V', XUI_MOD_CTRL, XUI_CODE_COMMAND_PASTE) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'Z', XUI_MOD_CTRL, XUI_CODE_COMMAND_UNDO) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'Y', XUI_MOD_CTRL, XUI_CODE_COMMAND_REDO) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'F', XUI_MOD_CTRL, XUI_CODE_COMMAND_OPEN_FIND) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'H', XUI_MOD_CTRL, XUI_CODE_COMMAND_OPEN_REPLACE) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, 'G', XUI_MOD_CTRL, XUI_CODE_COMMAND_GOTO_LINE) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_SPACE, XUI_MOD_CTRL, XUI_CODE_COMMAND_SHOW_COMPLETION) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, '/', XUI_MOD_CTRL, XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_F3, 0, XUI_CODE_COMMAND_FIND_NEXT) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( xuiCodeCommandMapBind(pMap, XUI_KEY_F3, XUI_MOD_SHIFT, XUI_CODE_COMMAND_FIND_PREVIOUS) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	return XUI_OK;
}

static int __xuiCodeCommandNeedsDocumentSelection(int iCommand)
{
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_MOVE_LEFT:
	case XUI_CODE_COMMAND_MOVE_RIGHT:
	case XUI_CODE_COMMAND_MOVE_UP:
	case XUI_CODE_COMMAND_MOVE_DOWN:
	case XUI_CODE_COMMAND_MOVE_WORD_LEFT:
	case XUI_CODE_COMMAND_MOVE_WORD_RIGHT:
	case XUI_CODE_COMMAND_MOVE_LINE_START:
	case XUI_CODE_COMMAND_MOVE_LINE_END:
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_START:
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_END:
	case XUI_CODE_COMMAND_MOVE_PAGE_UP:
	case XUI_CODE_COMMAND_MOVE_PAGE_DOWN:
	case XUI_CODE_COMMAND_SELECT_LEFT:
	case XUI_CODE_COMMAND_SELECT_RIGHT:
	case XUI_CODE_COMMAND_SELECT_UP:
	case XUI_CODE_COMMAND_SELECT_DOWN:
	case XUI_CODE_COMMAND_SELECT_PAGE_UP:
	case XUI_CODE_COMMAND_SELECT_PAGE_DOWN:
	case XUI_CODE_COMMAND_SELECT_ALL:
	case XUI_CODE_COMMAND_DELETE_BACK:
	case XUI_CODE_COMMAND_DELETE_FORWARD:
	case XUI_CODE_COMMAND_DELETE_WORD_BACK:
	case XUI_CODE_COMMAND_DELETE_WORD_FORWARD:
	case XUI_CODE_COMMAND_INSERT_NEWLINE:
	case XUI_CODE_COMMAND_INSERT_TAB:
	case XUI_CODE_COMMAND_INDENT:
	case XUI_CODE_COMMAND_OUTDENT:
	case XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT:
	case XUI_CODE_COMMAND_COPY:
	case XUI_CODE_COMMAND_CUT:
	case XUI_CODE_COMMAND_PASTE:
	case XUI_CODE_COMMAND_UNDO:
	case XUI_CODE_COMMAND_REDO:
		return 1;
	default:
		return 0;
	}
}

static int __xuiCodeCommandMovePage(const xui_code_command_context_t* pContext, int iCommand)
{
	xui_code_selection_t tState;
	int iLine;
	int iColumn;
	int iPageLines;
	int bExtend;
	int iRet;

	if ( pContext == NULL || pContext->pDocument == NULL || pContext->pSelection == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeSelectionGetState(pContext->pSelection, &tState);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentOffsetToLineColumn(pContext->pDocument, tState.iCaretOffset, &iLine, &iColumn);
	if ( iRet != XUI_OK ) return iRet;
	if ( tState.iPreferredColumn >= 0 ) iColumn = tState.iPreferredColumn;
	iPageLines = (pContext->iPageLineCount > 0) ? pContext->iPageLineCount : 10;
	bExtend = (iCommand == XUI_CODE_COMMAND_SELECT_PAGE_UP || iCommand == XUI_CODE_COMMAND_SELECT_PAGE_DOWN);
	if ( iCommand == XUI_CODE_COMMAND_MOVE_PAGE_UP || iCommand == XUI_CODE_COMMAND_SELECT_PAGE_UP ) iLine -= iPageLines;
	else iLine += iPageLines;
	return xuiCodeSelectionGotoLineColumn(pContext->pSelection, pContext->pDocument, iLine, iColumn, bExtend);
}

static int __xuiCodeCommandGetProxy(const xui_code_command_context_t* pContext, xui_proxy_t* pProxy)
{
	if ( (pContext == NULL) || (pProxy == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pContext->pProxy == NULL ) return XUI_ERROR_UNSUPPORTED;
	*pProxy = *pContext->pProxy;
	return XUI_OK;
}

static int __xuiCodeCommandCopySelection(const xui_code_command_context_t* pContext, int bCut)
{
	xui_proxy_t tProxy;
	char* sCopy;
	int iStart;
	int iEnd;
	int iLength;
	int iRet;

	if ( (pContext == NULL) || (pContext->pDocument == NULL) || (pContext->pSelection == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bCut && pContext->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiCodeSelectionGetRange(pContext->pSelection, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( iStart == iEnd ) return XUI_OK;
	iRet = __xuiCodeCommandGetProxy(pContext, &tProxy);
	if ( iRet != XUI_OK ) return iRet;
	if ( tProxy.clipboardSetText == NULL ) return XUI_ERROR_UNSUPPORTED;
	iLength = iEnd - iStart;
	sCopy = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiCodeDocumentCopyRange(pContext->pDocument, iStart, iEnd, sCopy, iLength + 1, NULL);
	if ( iRet != XUI_OK ) {
		xrtFree(sCopy);
		return iRet;
	}
	iRet = tProxy.clipboardSetText(&tProxy, sCopy);
	xrtFree(sCopy);
	if ( iRet != XUI_OK ) return iRet;
	if ( bCut ) return xuiCodeEditingDeleteForward(pContext->pDocument, pContext->pSelection, pContext->bReadonly);
	return XUI_OK;
}

static int __xuiCodeCommandPasteClipboard(const xui_code_command_context_t* pContext)
{
	xui_proxy_t tProxy;
	char* sText;
	int iLength;
	int iRet;

	if ( (pContext == NULL) || (pContext->pDocument == NULL) || (pContext->pSelection == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pContext->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeCommandGetProxy(pContext, &tProxy);
	if ( iRet != XUI_OK ) return iRet;
	sText = NULL;
	iRet = xuiInternalClipboardReadProxy(&tProxy, &sText, &iLength);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeEditingInsertText(pContext->pDocument, pContext->pSelection, sText, pContext->bReadonly);
	xrtFree(sText);
	return iRet;
}

static int __xuiCodeCommandInsertNewlineAutoIndent(const xui_code_command_context_t* pContext)
{
	xui_code_selection_t tState;
	char sSmall[256];
	char* sInsert;
	char c;
	int iLine;
	int iLineStart;
	int iLineEnd;
	int iIndentEnd;
	int iIndentLength;
	int iRet;

	if ( pContext == NULL || pContext->pDocument == NULL || pContext->pSelection == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pContext->bReadonly ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiCodeSelectionGetState(pContext->pSelection, &tState);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentOffsetToLineColumn(pContext->pDocument, tState.iCaretOffset, &iLine, NULL);
	if ( iRet != XUI_OK ) return iRet;
	iLineStart = 0;
	iLineEnd = 0;
	iRet = xuiCodeDocumentGetLineRange(pContext->pDocument, iLine, &iLineStart, &iLineEnd);
	if ( iRet != XUI_OK ) return iRet;
	iIndentEnd = iLineStart;
	while ( iIndentEnd < iLineEnd ) {
		iRet = xuiCodeDocumentGetByte(pContext->pDocument, iIndentEnd, &c);
		if ( iRet != XUI_OK ) return iRet;
		if ( c != ' ' && c != '\t' ) break;
		iIndentEnd++;
	}
	iIndentLength = iIndentEnd - iLineStart;
	if ( iIndentLength <= 0 ) {
		return xuiCodeEditingInsertText(pContext->pDocument, pContext->pSelection, "\n", pContext->bReadonly);
	}
	sInsert = sSmall;
	if ( iIndentLength + 2 > (int)sizeof(sSmall) ) {
		sInsert = (char*)xrtMalloc((size_t)iIndentLength + 2u);
		if ( sInsert == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	sInsert[0] = '\n';
	iRet = xuiCodeDocumentCopyRange(pContext->pDocument, iLineStart, iIndentEnd,
	                                sInsert + 1, iIndentLength + 1, NULL);
	if ( iRet != XUI_OK ) {
		if ( sInsert != sSmall ) xrtFree(sInsert);
		return iRet;
	}
	iRet = xuiCodeEditingInsertText(pContext->pDocument, pContext->pSelection, sInsert, pContext->bReadonly);
	if ( sInsert != sSmall ) xrtFree(sInsert);
	return iRet;
}

static int __xuiCodeCommandSelectionSpansLines(const xui_code_command_context_t* pContext, int* pSpansLines)
{
	int iStart;
	int iEnd;
	int iStartLine;
	int iEndLine;
	int iRet;

	if ( pContext == NULL || pContext->pDocument == NULL || pContext->pSelection == NULL || pSpansLines == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pSpansLines = 0;
	iRet = xuiCodeSelectionGetRange(pContext->pSelection, &iStart, &iEnd);
	if ( iRet != XUI_OK ) return iRet;
	if ( iStart == iEnd ) return XUI_OK;
	iRet = xuiCodeDocumentOffsetToLineColumn(pContext->pDocument, iStart, &iStartLine, NULL);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentOffsetToLineColumn(pContext->pDocument, iEnd, &iEndLine, NULL);
	if ( iRet != XUI_OK ) return iRet;
	*pSpansLines = (iStartLine != iEndLine);
	return XUI_OK;
}

XUI_API int xuiCodeCommandExecute(const xui_code_command_context_t* pContext, int iCommand, int* pHandled)
{
	int iRet;
	int bHandled;
	const char* sIndent;
	const char* sLineComment;

	if ( (pContext == NULL) || (pHandled == NULL) || (iCommand <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pHandled = 0;
	if ( pContext->pProviders != NULL ) {
		iRet = xuiCodeProviderExecuteCommand(pContext->pProviders, pContext->pWidget, iCommand, pContext->pCommandData, &bHandled);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_UNSUPPORTED ) return iRet;
		if ( iRet == XUI_OK && bHandled ) {
			*pHandled = 1;
			return XUI_OK;
		}
	}
	if ( __xuiCodeCommandNeedsDocumentSelection(iCommand) && (pContext->pDocument == NULL || pContext->pSelection == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sIndent = (pContext->sIndent != NULL) ? pContext->sIndent : "\t";
	sLineComment = (pContext->sLineComment != NULL) ? pContext->sLineComment : "//";
	switch ( iCommand ) {
	case XUI_CODE_COMMAND_MOVE_LEFT:
	case XUI_CODE_COMMAND_MOVE_RIGHT:
	case XUI_CODE_COMMAND_MOVE_UP:
	case XUI_CODE_COMMAND_MOVE_DOWN:
	case XUI_CODE_COMMAND_MOVE_WORD_LEFT:
	case XUI_CODE_COMMAND_MOVE_WORD_RIGHT:
	case XUI_CODE_COMMAND_MOVE_LINE_START:
	case XUI_CODE_COMMAND_MOVE_LINE_END:
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_START:
	case XUI_CODE_COMMAND_MOVE_DOCUMENT_END:
	case XUI_CODE_COMMAND_SELECT_LEFT:
	case XUI_CODE_COMMAND_SELECT_RIGHT:
	case XUI_CODE_COMMAND_SELECT_UP:
	case XUI_CODE_COMMAND_SELECT_DOWN:
	case XUI_CODE_COMMAND_SELECT_ALL:
		iRet = xuiCodeSelectionMove(pContext->pSelection, pContext->pDocument, iCommand);
		break;
	case XUI_CODE_COMMAND_MOVE_PAGE_UP:
	case XUI_CODE_COMMAND_MOVE_PAGE_DOWN:
	case XUI_CODE_COMMAND_SELECT_PAGE_UP:
	case XUI_CODE_COMMAND_SELECT_PAGE_DOWN:
		iRet = __xuiCodeCommandMovePage(pContext, iCommand);
		break;
	case XUI_CODE_COMMAND_DELETE_BACK:
		iRet = xuiCodeEditingDeleteBackward(pContext->pDocument, pContext->pSelection, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_DELETE_FORWARD:
		iRet = xuiCodeEditingDeleteForward(pContext->pDocument, pContext->pSelection, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_DELETE_WORD_BACK:
		iRet = xuiCodeEditingDeleteWordBackward(pContext->pDocument, pContext->pSelection, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_DELETE_WORD_FORWARD:
		iRet = xuiCodeEditingDeleteWordForward(pContext->pDocument, pContext->pSelection, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_INSERT_NEWLINE:
		iRet = __xuiCodeCommandInsertNewlineAutoIndent(pContext);
		break;
	case XUI_CODE_COMMAND_INSERT_TAB:
		{
			int bSpansLines;
			iRet = __xuiCodeCommandSelectionSpansLines(pContext, &bSpansLines);
			if ( iRet == XUI_OK ) {
				if ( bSpansLines ) {
					iRet = xuiCodeEditingIndentSelection(pContext->pDocument, pContext->pSelection, sIndent, pContext->bReadonly);
				} else {
					iRet = xuiCodeEditingInsertText(pContext->pDocument, pContext->pSelection, sIndent, pContext->bReadonly);
				}
			}
		}
		break;
	case XUI_CODE_COMMAND_INDENT:
		iRet = xuiCodeEditingIndentSelection(pContext->pDocument, pContext->pSelection, sIndent, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_OUTDENT:
		iRet = xuiCodeEditingOutdentSelection(pContext->pDocument, pContext->pSelection, pContext->iIndentColumns, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT:
		iRet = xuiCodeEditingToggleLineComment(pContext->pDocument, pContext->pSelection, sLineComment, pContext->bReadonly);
		break;
	case XUI_CODE_COMMAND_UNDO:
		iRet = xuiCodeDocumentUndo(pContext->pDocument);
		break;
	case XUI_CODE_COMMAND_REDO:
		iRet = xuiCodeDocumentRedo(pContext->pDocument);
		break;
	case XUI_CODE_COMMAND_FOLD_TOGGLE:
		if ( pContext->pFoldState == NULL || pContext->pDocument == NULL || pContext->pSelection == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		{
			xui_code_selection_t tState;
			int iLine;
			iRet = xuiCodeSelectionGetState(pContext->pSelection, &tState);
			if ( iRet == XUI_OK ) iRet = xuiCodeDocumentOffsetToLineColumn(pContext->pDocument, tState.iCaretOffset, &iLine, NULL);
			if ( iRet == XUI_OK ) iRet = xuiCodeFoldStateToggleLine(pContext->pFoldState, iLine);
		}
		break;
	case XUI_CODE_COMMAND_FOLD_ALL:
		if ( pContext->pFoldState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		iRet = xuiCodeFoldStateFoldAll(pContext->pFoldState);
		break;
	case XUI_CODE_COMMAND_UNFOLD_ALL:
		if ( pContext->pFoldState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		iRet = xuiCodeFoldStateUnfoldAll(pContext->pFoldState);
		break;
	case XUI_CODE_COMMAND_COPY:
		iRet = __xuiCodeCommandCopySelection(pContext, 0);
		break;
	case XUI_CODE_COMMAND_CUT:
		iRet = __xuiCodeCommandCopySelection(pContext, 1);
		break;
	case XUI_CODE_COMMAND_PASTE:
		iRet = __xuiCodeCommandPasteClipboard(pContext);
		break;
	case XUI_CODE_COMMAND_FIND_NEXT:
	case XUI_CODE_COMMAND_FIND_PREVIOUS:
	case XUI_CODE_COMMAND_REPLACE_NEXT:
	case XUI_CODE_COMMAND_GOTO_LINE:
	case XUI_CODE_COMMAND_SHOW_COMPLETION:
	case XUI_CODE_COMMAND_SHOW_SIGNATURE_HELP:
		return XUI_ERROR_UNSUPPORTED;
	default:
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( iRet == XUI_OK ) *pHandled = 1;
	return iRet;
}
