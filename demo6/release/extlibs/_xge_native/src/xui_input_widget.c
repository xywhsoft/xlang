#include "xui_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define XUI_INPUT_KEY_BACKSPACE	8
#define XUI_INPUT_KEY_DELETE	46
#define XUI_INPUT_PADDING_LEFT	8.0f
#define XUI_INPUT_PADDING_TOP	4.0f
#define XUI_INPUT_PADDING_RIGHT	8.0f
#define XUI_INPUT_PADDING_BOTTOM	4.0f
#define XUI_INPUT_DECORATION_DEFAULT_WIDTH	22.0f

struct xui_input_decoration_t {
	struct xui_input_decoration_t* pNext;
	int iSide;
	int iKind;
	int iVisibleMode;
	float fWidth;
	float fPadding;
	int iIcon;
	char* sText;
	xui_surface pSurface;
	xui_rect_t tSrc;
	uint32_t iColor;
	uint32_t iHoverColor;
	uint32_t iActiveColor;
	uint32_t iDisabledColor;
	xui_input_decoration_click_proc onClick;
	xui_input_decoration_paint_proc onPaint;
	void* pUser;
	xui_rect_t tRect;
};

typedef struct xui_input_data_t {
	char* sText;
	int iTextCapacity;
	char* sPlaceholder;
	int iPlaceholderCapacity;
	char* sDisplay;
	int iDisplayCapacity;
	char* sScratch;
	int iScratchCapacity;
	char* sUndoText;
	int iUndoCapacity;
	char* sRedoText;
	int iRedoCapacity;
	char* arrMenuTitle[XUI_INPUT_MENU_COUNT];
	xui_widget pMenu;
	xui_input_decoration pLeadingDecoration;
	xui_input_decoration pTrailingDecoration;
	xui_input_decoration pHoverDecoration;
	xui_input_decoration pActiveDecoration;
	xui_font pFont;
	xui_input_change_proc onChange;
	void* pChangeUser;
	int iChangeCount;
	int iCursor;
	int iAnchor;
	int iSelectStart;
	int iSelectEnd;
	int iDragAnchor;
	int bDragging;
	int bPressPending;
	int bPressInsideSelection;
	int bMovingSelection;
	int iPressCursor;
	float fPressX;
	float fPressY;
	float fScrollX;
	int iMaxLength;
	int iTextAlign;
	int bPassword;
	int bReadonly;
	int bError;
	int bCanUndo;
	int iUndoCursor;
	int iUndoSelectStart;
	int iUndoSelectEnd;
	int bCanRedo;
	int iRedoCursor;
	int iRedoSelectStart;
	int iRedoSelectEnd;
	uint32_t iTextColor;
	uint32_t iPlaceholderColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iErrorBackgroundColor;
	uint32_t iErrorBorderColor;
	uint32_t iSelectionColor;
	uint32_t iCursorColor;
	float fBorderWidth;
	float fLeadingDecorationWidth;
	float fTrailingDecorationWidth;
	xui_rect_t tTextRect;
	xui_rect_t tCursorRect;
} xui_input_data_t;

static int __xuiInputMoveCursor(xui_widget pWidget, xui_input_data_t* pData, int iNewCursor, int bExtend);
static int __xuiInputSetSelectionData(xui_input_data_t* pData, int iStart, int iEnd);
static int __xuiInputSyncCursor(xui_widget pWidget, xui_input_data_t* pData);

static int __xuiInputMenuTextId(int iCommand)
{
	switch ( iCommand ) {
	case XUI_INPUT_MENU_UNDO: return XUI_TR_EDIT_UNDO;
	case XUI_INPUT_MENU_CUT: return XUI_TR_EDIT_CUT;
	case XUI_INPUT_MENU_COPY: return XUI_TR_EDIT_COPY;
	case XUI_INPUT_MENU_PASTE: return XUI_TR_EDIT_PASTE;
	case XUI_INPUT_MENU_DELETE: return XUI_TR_EDIT_DELETE;
	case XUI_INPUT_MENU_SELECT_ALL: return XUI_TR_EDIT_SELECT_ALL;
	case XUI_INPUT_MENU_REDO: return XUI_TR_EDIT_REDO;
	default: return XUI_TR_NONE;
	}
}

static int __xuiInputAlignValid(int iAlign)
{
	return (iAlign == XUI_INPUT_ALIGN_LEFT) ||
	       (iAlign == XUI_INPUT_ALIGN_CENTER) ||
	       (iAlign == XUI_INPUT_ALIGN_RIGHT);
}

static int __xuiInputDescValid(const xui_input_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iMaxLength < 0) || !__xuiInputAlignValid(pDesc->iTextAlign) ) {
		return 0;
	}
	if ( pDesc->fBorderWidth < 0.0f ) {
		return 0;
	}
	return 1;
}

static int __xuiInputDecorationSideValid(int iSide)
{
	return (iSide == XUI_INPUT_DECORATION_SIDE_LEADING) ||
	       (iSide == XUI_INPUT_DECORATION_SIDE_TRAILING);
}

static int __xuiInputDecorationKindValid(int iKind)
{
	return (iKind >= XUI_INPUT_DECORATION_NONE) &&
	       (iKind <= XUI_INPUT_DECORATION_CUSTOM_PAINT);
}

static int __xuiInputDecorationVisibleModeValid(int iMode)
{
	return (iMode >= XUI_INPUT_DECORATION_VISIBLE_ALWAYS) &&
	       (iMode <= XUI_INPUT_DECORATION_VISIBLE_FOCUSED_NOT_EMPTY);
}

static int __xuiInputDecorationDescValid(const xui_input_decoration_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 0;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( !__xuiInputDecorationKindValid(pDesc->iKind) ||
	     !__xuiInputDecorationVisibleModeValid(pDesc->iVisibleMode) ) {
		return 0;
	}
	if ( (pDesc->fWidth < 0.0f) || (pDesc->fPadding < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiInputAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiInputColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiInputTextReserve(char** psText, int* pCapacity, int iCapacity)
{
	char* sNew;

	if ( (psText == NULL) || (pCapacity == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCapacity <= *pCapacity ) {
		return XUI_OK;
	}
	sNew = (char*)xrtRealloc(*psText, (size_t)iCapacity);
	if ( sNew == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*psText = sNew;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiInputStringSet(char** psText, int* pCapacity, const char* sText)
{
	int iNeed;
	int iRet;

	if ( sText == NULL ) {
		sText = "";
	}
	iNeed = (int)strlen(sText) + 1;
	iRet = __xuiInputTextReserve(psText, pCapacity, iNeed);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(*psText, sText, (size_t)iNeed);
	return XUI_OK;
}

static char* __xuiInputStringDuplicate(const char* sText)
{
	char* sCopy;
	int iCapacity;

	sCopy = NULL;
	iCapacity = 0;
	if ( __xuiInputStringSet(&sCopy, &iCapacity, sText) != XUI_OK ) {
		return NULL;
	}
	return sCopy;
}

static int __xuiInputUtf8IsCont(unsigned char c)
{
	return (c & 0xc0u) == 0x80u;
}

static int __xuiInputUtf8Next(const char* sText, int iLen, int iPos)
{
	unsigned char c;
	int iNext;

	if ( sText == NULL ) {
		return 0;
	}
	if ( iPos < 0 ) {
		iPos = 0;
	}
	if ( iPos >= iLen ) {
		return iLen;
	}
	c = (unsigned char)sText[iPos];
	if ( c < 0x80u ) {
		iNext = iPos + 1;
	} else if ( (c & 0xe0u) == 0xc0u ) {
		iNext = iPos + 2;
	} else if ( (c & 0xf0u) == 0xe0u ) {
		iNext = iPos + 3;
	} else if ( (c & 0xf8u) == 0xf0u ) {
		iNext = iPos + 4;
	} else {
		iNext = iPos + 1;
	}
	if ( iNext > iLen ) {
		iNext = iLen;
	}
	return iNext;
}

static int __xuiInputUtf8Prev(const char* sText, int iLen, int iPos)
{
	if ( sText == NULL ) {
		return 0;
	}
	if ( iPos > iLen ) {
		iPos = iLen;
	}
	if ( iPos <= 0 ) {
		return 0;
	}
	iPos--;
	while ( (iPos > 0) && __xuiInputUtf8IsCont((unsigned char)sText[iPos]) ) {
		iPos--;
	}
	return iPos;
}

static int __xuiInputUtf8Clamp(const char* sText, int iLen, int iPos)
{
	if ( iPos < 0 ) {
		return 0;
	}
	if ( iPos > iLen ) {
		return iLen;
	}
	while ( (iPos > 0) && (iPos < iLen) && __xuiInputUtf8IsCont((unsigned char)sText[iPos]) ) {
		iPos--;
	}
	return iPos;
}

static int __xuiInputUtf8ClampBytes(const char* sText, int iMaxBytes)
{
	int iLen;
	int iPos;
	int iNext;

	if ( sText == NULL || iMaxBytes <= 0 ) {
		return 0;
	}
	iLen = (int)strlen(sText);
	if ( iLen <= iMaxBytes ) {
		return iLen;
	}
	iPos = 0;
	while ( iPos < iLen ) {
		iNext = __xuiInputUtf8Next(sText, iLen, iPos);
		if ( iNext > iMaxBytes ) {
			break;
		}
		iPos = iNext;
	}
	return iPos;
}

static int __xuiInputUtf8CountRange(const char* sText, int iStart, int iEnd)
{
	int iCount;
	int iPos;

	if ( sText == NULL ) {
		return 0;
	}
	if ( iStart < 0 ) {
		iStart = 0;
	}
	if ( iEnd < iStart ) {
		iEnd = iStart;
	}
	iCount = 0;
	iPos = iStart;
	while ( iPos < iEnd && sText[iPos] != '\0' ) {
		iPos = __xuiInputUtf8Next(sText, iEnd, iPos);
		iCount++;
	}
	return iCount;
}

static int __xuiInputCharClass(const char* sText, int iPos)
{
	unsigned char c;

	if ( (sText == NULL) || (iPos < 0) || (sText[iPos] == '\0') ) {
		return 0;
	}
	c = (unsigned char)sText[iPos];
	if ( c <= 32u ) {
		return 0;
	}
	if ( c >= 128u ) {
		return 1;
	}
	if ( ((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'Z')) ||
	     ((c >= 'a') && (c <= 'z')) || (c == '_') ) {
		return 1;
	}
	return 2;
}

static int __xuiInputPrevWord(xui_input_data_t* pData)
{
	const char* sText;
	int iLen;
	int iPos;
	int iPrev;
	int iClass;

	if ( pData == NULL || pData->sText == NULL ) {
		return 0;
	}
	sText = pData->sText;
	iLen = (int)strlen(sText);
	iPos = __xuiInputUtf8Prev(sText, iLen, pData->iCursor);
	while ( iPos > 0 && __xuiInputCharClass(sText, iPos) == 0 ) {
		iPos = __xuiInputUtf8Prev(sText, iLen, iPos);
	}
	iClass = __xuiInputCharClass(sText, iPos);
	while ( iPos > 0 ) {
		iPrev = __xuiInputUtf8Prev(sText, iLen, iPos);
		if ( __xuiInputCharClass(sText, iPrev) != iClass ) {
			break;
		}
		iPos = iPrev;
	}
	return iPos;
}

static int __xuiInputNextWord(xui_input_data_t* pData)
{
	const char* sText;
	int iLen;
	int iPos;
	int iNext;
	int iClass;

	if ( pData == NULL || pData->sText == NULL ) {
		return 0;
	}
	sText = pData->sText;
	iLen = (int)strlen(sText);
	iPos = __xuiInputUtf8Clamp(sText, iLen, pData->iCursor);
	if ( iPos >= iLen ) {
		return iLen;
	}
	iClass = __xuiInputCharClass(sText, iPos);
	while ( iPos < iLen ) {
		iNext = __xuiInputUtf8Next(sText, iLen, iPos);
		if ( iNext <= iPos ) {
			break;
		}
		if ( __xuiInputCharClass(sText, iNext) != iClass ) {
			iPos = iNext;
			break;
		}
		iPos = iNext;
	}
	while ( iPos < iLen && __xuiInputCharClass(sText, iPos) == 0 ) {
		iNext = __xuiInputUtf8Next(sText, iLen, iPos);
		if ( iNext <= iPos ) {
			break;
		}
		iPos = iNext;
	}
	return iPos;
}

static int __xuiInputSelectWordAt(xui_widget pWidget, xui_input_data_t* pData, int iCursor)
{
	const char* sText;
	int iLen;
	int iStart;
	int iEnd;
	int iPrev;
	int iNext;
	int iClass;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sText = pData->sText;
	iLen = (int)strlen(sText);
	if ( iLen <= 0 ) {
		return __xuiInputMoveCursor(pWidget, pData, 0, 0);
	}
	iStart = __xuiInputUtf8Clamp(sText, iLen, iCursor);
	if ( iStart >= iLen ) {
		iStart = __xuiInputUtf8Prev(sText, iLen, iLen);
	}
	iClass = __xuiInputCharClass(sText, iStart);
	if ( iClass == 0 ) {
		return __xuiInputMoveCursor(pWidget, pData, iCursor, 0);
	}
	iEnd = __xuiInputUtf8Next(sText, iLen, iStart);
	while ( iStart > 0 ) {
		iPrev = __xuiInputUtf8Prev(sText, iLen, iStart);
		if ( __xuiInputCharClass(sText, iPrev) != iClass ) {
			break;
		}
		iStart = iPrev;
	}
	while ( iEnd < iLen ) {
		iNext = __xuiInputUtf8Next(sText, iLen, iEnd);
		if ( __xuiInputCharClass(sText, iEnd) != iClass ) {
			break;
		}
		iEnd = iNext;
	}
	(void)__xuiInputSetSelectionData(pData, iStart, iEnd);
	return __xuiInputSyncCursor(pWidget, pData);
}

static int __xuiInputUtf8Encode(uint32_t iCodepoint, char* sText)
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

static xui_input_data_t* __xuiInputGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "input");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_input_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiInputStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiInputStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiInputStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiInputResolve(xui_widget pWidget, xui_input_data_t* pData, xui_input_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)__xuiInputStyleColor(pWidget, "input.text.color", &pResolved->iTextColor);
	(void)__xuiInputStyleColor(pWidget, "input.placeholder.color", &pResolved->iPlaceholderColor);
	(void)__xuiInputStyleColor(pWidget, "input.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiInputStyleColor(pWidget, "input.background.color", &pResolved->iBackgroundColor);
	(void)__xuiInputStyleColor(pWidget, "input.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiInputStyleColor(pWidget, "input.background.disabled_color", &pResolved->iDisabledBackgroundColor);
	(void)__xuiInputStyleColor(pWidget, "input.border.color", &pResolved->iBorderColor);
	(void)__xuiInputStyleColor(pWidget, "input.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiInputStyleColor(pWidget, "input.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiInputStyleColor(pWidget, "input.error.background_color", &pResolved->iErrorBackgroundColor);
	(void)__xuiInputStyleColor(pWidget, "input.error.border_color", &pResolved->iErrorBorderColor);
	(void)__xuiInputStyleColor(pWidget, "input.selection.color", &pResolved->iSelectionColor);
	(void)__xuiInputStyleColor(pWidget, "input.cursor.color", &pResolved->iCursorColor);
	(void)__xuiInputStyleFloat(pWidget, "input.border.width", &pResolved->fBorderWidth);
	pResolved->pFont = __xuiInputStyleFont(pWidget, pResolved->pFont);
}

static void __xuiInputSelectionRange(xui_input_data_t* pData, int* pStart, int* pEnd)
{
	int iStart;
	int iEnd;

	iStart = (pData != NULL) ? pData->iSelectStart : 0;
	iEnd = (pData != NULL) ? pData->iSelectEnd : 0;
	if ( iStart > iEnd ) {
		int iTmp = iStart;
		iStart = iEnd;
		iEnd = iTmp;
	}
	if ( pStart != NULL ) {
		*pStart = iStart;
	}
	if ( pEnd != NULL ) {
		*pEnd = iEnd;
	}
}

static int __xuiInputHasSelectionData(xui_input_data_t* pData)
{
	int iStart;
	int iEnd;

	if ( pData == NULL ) {
		return 0;
	}
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	return iStart != iEnd;
}

static int __xuiInputSetSelectionData(xui_input_data_t* pData, int iStart, int iEnd)
{
	int iLen;

	if ( (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = (int)strlen(pData->sText);
	iStart = __xuiInputUtf8Clamp(pData->sText, iLen, iStart);
	iEnd = __xuiInputUtf8Clamp(pData->sText, iLen, iEnd);
	pData->iSelectStart = iStart;
	pData->iSelectEnd = iEnd;
	pData->iCursor = iEnd;
	pData->iAnchor = iStart;
	return XUI_OK;
}

static int __xuiInputClearSelectionData(xui_input_data_t* pData)
{
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iSelectStart = pData->iCursor;
	pData->iSelectEnd = pData->iCursor;
	pData->iAnchor = pData->iCursor;
	return XUI_OK;
}

static int __xuiInputAssignTextBytes(xui_input_data_t* pData, const char* sText, int iSize)
{
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
		iSize = 0;
	} else if ( iSize < 0 ) {
		iSize = (int)strlen(sText);
	}
	if ( (pData->iMaxLength > 0) && (iSize > pData->iMaxLength) ) {
		iSize = __xuiInputUtf8ClampBytes(sText, pData->iMaxLength);
	}
	iRet = __xuiInputTextReserve(&pData->sText, &pData->iTextCapacity, iSize + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iSize > 0 ) {
		memcpy(pData->sText, sText, (size_t)iSize);
	}
	pData->sText[iSize] = '\0';
	pData->iCursor = __xuiInputUtf8Clamp(pData->sText, iSize, pData->iCursor);
	pData->iAnchor = __xuiInputUtf8Clamp(pData->sText, iSize, pData->iAnchor);
	pData->iSelectStart = __xuiInputUtf8Clamp(pData->sText, iSize, pData->iSelectStart);
	pData->iSelectEnd = __xuiInputUtf8Clamp(pData->sText, iSize, pData->iSelectEnd);
	return XUI_OK;
}

static int __xuiInputBuildDisplayRange(xui_input_data_t* pData, int iStart, int iEnd, char** psText, int* pCapacity)
{
	int iLen;
	int iCount;
	int iNeed;
	int iRet;

	if ( (pData == NULL) || (pData->sText == NULL) || (psText == NULL) || (pCapacity == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = (int)strlen(pData->sText);
	iStart = __xuiInputUtf8Clamp(pData->sText, iLen, iStart);
	iEnd = __xuiInputUtf8Clamp(pData->sText, iLen, iEnd);
	if ( iEnd < iStart ) {
		iEnd = iStart;
	}
	if ( pData->bPassword ) {
		iCount = __xuiInputUtf8CountRange(pData->sText, iStart, iEnd);
		iRet = __xuiInputTextReserve(psText, pCapacity, iCount + 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( iCount > 0 ) {
			memset(*psText, '*', (size_t)iCount);
		}
		(*psText)[iCount] = '\0';
		return XUI_OK;
	}
	iNeed = iEnd - iStart + 1;
	iRet = __xuiInputTextReserve(psText, pCapacity, iNeed);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iEnd > iStart ) {
		memcpy(*psText, pData->sText + iStart, (size_t)(iEnd - iStart));
	}
	(*psText)[iEnd - iStart] = '\0';
	return XUI_OK;
}

static const char* __xuiInputDisplayAll(xui_input_data_t* pData)
{
	int iLen;

	if ( pData == NULL || pData->sText == NULL ) {
		return "";
	}
	if ( !pData->bPassword ) {
		return pData->sText;
	}
	iLen = (int)strlen(pData->sText);
	if ( __xuiInputBuildDisplayRange(pData, 0, iLen, &pData->sDisplay, &pData->iDisplayCapacity) != XUI_OK ) {
		return "";
	}
	return pData->sDisplay;
}

static const char* __xuiInputDisplayPrefix(xui_input_data_t* pData, int iEnd)
{
	if ( pData == NULL || pData->sText == NULL ) {
		return "";
	}
	if ( __xuiInputBuildDisplayRange(pData, 0, iEnd, &pData->sScratch, &pData->iScratchCapacity) != XUI_OK ) {
		return "";
	}
	return pData->sScratch;
}

static xui_vec2_t __xuiInputMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;

	tSize.fX = 0.0f;
	tSize.fY = 18.0f;
	if ( (sText == NULL) || (sText[0] == '\0') ) {
		return tSize;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) &&
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK) &&
	     (tSize.fX >= 0.0f) && (tSize.fY >= 0.0f) ) {
		return tSize;
	}
	tSize.fX = (float)strlen(sText) * 7.0f;
	tSize.fY = 18.0f;
	return tSize;
}

static float __xuiInputMeasurePrefix(xui_widget pWidget, xui_input_data_t* pData, xui_font pFont, int iEnd)
{
	const char* sPrefix;
	xui_vec2_t tSize;

	sPrefix = __xuiInputDisplayPrefix(pData, iEnd);
	tSize = __xuiInputMeasureText(pWidget, pFont, sPrefix);
	return tSize.fX;
}

static float __xuiInputAlignOffset(xui_input_data_t* pData, float fContentW, float fTextW)
{
	if ( (pData == NULL) || (fTextW >= fContentW) ) {
		return 0.0f;
	}
	if ( pData->iTextAlign == XUI_INPUT_ALIGN_CENTER ) {
		return (fContentW - fTextW) * 0.5f;
	}
	if ( pData->iTextAlign == XUI_INPUT_ALIGN_RIGHT ) {
		return fContentW - fTextW;
	}
	return 0.0f;
}

static float __xuiInputAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static int __xuiInputHasEffectiveFocus(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_context pContext;
	xui_widget pFocus;

	if ( pWidget == NULL ) {
		return 0;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pFocus = xuiGetFocusWidget(pContext);
	if ( pFocus == pWidget ) {
		return 1;
	}
	return (pData != NULL) && (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu);
}

static int __xuiInputDecorationIsVisible(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	int bFocused;
	int bNotEmpty;

	if ( (pWidget == NULL) || (pData == NULL) || (pDecoration == NULL) ||
	     (pDecoration->iKind == XUI_INPUT_DECORATION_NONE) ) {
		return 0;
	}
	bFocused = __xuiInputHasEffectiveFocus(pWidget, pData);
	bNotEmpty = (pData->sText != NULL) && (pData->sText[0] != '\0');
	switch ( pDecoration->iVisibleMode ) {
	case XUI_INPUT_DECORATION_VISIBLE_NOT_EMPTY:
		return bNotEmpty;
	case XUI_INPUT_DECORATION_VISIBLE_FOCUSED:
		return bFocused;
	case XUI_INPUT_DECORATION_VISIBLE_FOCUSED_NOT_EMPTY:
		return bFocused && bNotEmpty;
	case XUI_INPUT_DECORATION_VISIBLE_ALWAYS:
	default:
		return 1;
	}
}

static float __xuiInputDecorationWidth(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	xui_vec2_t tSize;

	if ( pDecoration == NULL ) {
		return 0.0f;
	}
	if ( pDecoration->fWidth > 0.0f ) {
		return pDecoration->fWidth;
	}
	if ( (pDecoration->iKind == XUI_INPUT_DECORATION_TEXT) &&
	     (pDecoration->sText != NULL) &&
	     (pDecoration->sText[0] != '\0') ) {
		tSize = __xuiInputMeasureText(pWidget, pData->pFont, pDecoration->sText);
		return tSize.fX + pDecoration->fPadding * 2.0f;
	}
	return XUI_INPUT_DECORATION_DEFAULT_WIDTH;
}

static void __xuiInputDecorationClearRects(xui_input_decoration pDecoration)
{
	while ( pDecoration != NULL ) {
		pDecoration->tRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		pDecoration = pDecoration->pNext;
	}
}

static void __xuiInputDecorationLayout(xui_widget pWidget, xui_input_data_t* pData, float* pLeadingWidth, float* pTrailingWidth)
{
	xui_input_decoration pDecoration;
	xui_rect_t tWidget;
	float fX;
	float fRight;
	float fY;
	float fH;
	float fWidth;
	float fLeadingWidth;
	float fTrailingWidth;

	if ( pLeadingWidth != NULL ) *pLeadingWidth = 0.0f;
	if ( pTrailingWidth != NULL ) *pTrailingWidth = 0.0f;
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	tWidget = xuiWidgetGetRect(pWidget);
	fY = XUI_INPUT_PADDING_TOP;
	fH = tWidget.fH - XUI_INPUT_PADDING_TOP - XUI_INPUT_PADDING_BOTTOM;
	if ( fH < 0.0f ) fH = 0.0f;
	fX = XUI_INPUT_PADDING_LEFT;
	fLeadingWidth = 0.0f;
	__xuiInputDecorationClearRects(pData->pLeadingDecoration);
	for ( pDecoration = pData->pLeadingDecoration; pDecoration != NULL; pDecoration = pDecoration->pNext ) {
		if ( !__xuiInputDecorationIsVisible(pWidget, pData, pDecoration) ) {
			continue;
		}
		fWidth = __xuiInputDecorationWidth(pWidget, pData, pDecoration);
		if ( fWidth < 0.0f ) fWidth = 0.0f;
		pDecoration->tRect = (xui_rect_t){fX, fY, fWidth, fH};
		fX += fWidth;
		fLeadingWidth += fWidth;
	}

	fRight = tWidget.fW - XUI_INPUT_PADDING_RIGHT;
	fTrailingWidth = 0.0f;
	__xuiInputDecorationClearRects(pData->pTrailingDecoration);
	for ( pDecoration = pData->pTrailingDecoration; pDecoration != NULL; pDecoration = pDecoration->pNext ) {
		if ( !__xuiInputDecorationIsVisible(pWidget, pData, pDecoration) ) {
			continue;
		}
		fWidth = __xuiInputDecorationWidth(pWidget, pData, pDecoration);
		if ( fWidth < 0.0f ) fWidth = 0.0f;
		fRight -= fWidth;
		pDecoration->tRect = (xui_rect_t){fRight, fY, fWidth, fH};
		fTrailingWidth += fWidth;
	}
	if ( pLeadingWidth != NULL ) *pLeadingWidth = fLeadingWidth;
	if ( pTrailingWidth != NULL ) *pTrailingWidth = fTrailingWidth;
}

static int __xuiInputDecorationSyncPadding(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_thickness_t tPadding;
	xui_thickness_t tOld;
	float fLeadingWidth;
	float fTrailingWidth;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInputDecorationLayout(pWidget, pData, &fLeadingWidth, &fTrailingWidth);
	pData->fLeadingDecorationWidth = fLeadingWidth;
	pData->fTrailingDecorationWidth = fTrailingWidth;
	tPadding = (xui_thickness_t){
		XUI_INPUT_PADDING_LEFT + fLeadingWidth,
		XUI_INPUT_PADDING_TOP,
		XUI_INPUT_PADDING_RIGHT + fTrailingWidth,
		XUI_INPUT_PADDING_BOTTOM
	};
	tOld = xuiWidgetGetPadding(pWidget);
	if ( (__xuiInputAbsFloat(tOld.fLeft - tPadding.fLeft) < 0.01f) &&
	     (__xuiInputAbsFloat(tOld.fTop - tPadding.fTop) < 0.01f) &&
	     (__xuiInputAbsFloat(tOld.fRight - tPadding.fRight) < 0.01f) &&
	     (__xuiInputAbsFloat(tOld.fBottom - tPadding.fBottom) < 0.01f) ) {
		return XUI_OK;
	}
	return xuiWidgetSetPadding(pWidget, tPadding);
}

static void __xuiInputEnsureCursorVisible(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_input_data_t tResolved;
	xui_rect_t tContent;
	xui_vec2_t tDisplaySize;
	float fCursorX;
	float fMaxScroll;
	const char* sDisplay;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return;
	}
	(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	__xuiInputResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	if ( tContent.fW <= 1.0f ) {
		pData->fScrollX = 0.0f;
		return;
	}
	sDisplay = __xuiInputDisplayAll(pData);
	tDisplaySize = __xuiInputMeasureText(pWidget, tResolved.pFont, sDisplay);
	if ( tDisplaySize.fX <= tContent.fW ) {
		pData->fScrollX = 0.0f;
		return;
	}
	fCursorX = __xuiInputMeasurePrefix(pWidget, pData, tResolved.pFont, pData->iCursor);
	if ( fCursorX - pData->fScrollX < 2.0f ) {
		pData->fScrollX = fCursorX - 2.0f;
	}
	if ( fCursorX - pData->fScrollX > tContent.fW - 2.0f ) {
		pData->fScrollX = fCursorX - tContent.fW + 2.0f;
	}
	fMaxScroll = tDisplaySize.fX - tContent.fW + 4.0f;
	if ( pData->fScrollX < 0.0f ) {
		pData->fScrollX = 0.0f;
	}
	if ( pData->fScrollX > fMaxScroll ) {
		pData->fScrollX = fMaxScroll;
	}
}

static xui_rect_t __xuiInputCalculateCursorRect(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_input_data_t tResolved;
	xui_rect_t tContent;
	xui_vec2_t tDisplaySize;
	const char* sDisplay;
	float fOffset;
	float fCursorX;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	__xuiInputEnsureCursorVisible(pWidget, pData);
	__xuiInputResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	sDisplay = __xuiInputDisplayAll(pData);
	tDisplaySize = __xuiInputMeasureText(pWidget, tResolved.pFont, sDisplay);
	fOffset = (tDisplaySize.fX <= tContent.fW && pData->fScrollX <= 0.0f) ? __xuiInputAlignOffset(pData, tContent.fW, tDisplaySize.fX) : 0.0f;
	fCursorX = tContent.fX + fOffset + __xuiInputMeasurePrefix(pWidget, pData, tResolved.pFont, pData->iCursor) - pData->fScrollX;
	if ( fCursorX < tContent.fX ) fCursorX = tContent.fX;
	if ( fCursorX > tContent.fX + tContent.fW ) fCursorX = tContent.fX + tContent.fW;
	return (xui_rect_t){fCursorX, tContent.fY + 4.0f, 1.0f, (tContent.fH > 8.0f) ? tContent.fH - 8.0f : tContent.fH};
}

static int __xuiInputInvalidateText(xui_widget pWidget)
{
	xui_input_data_t* pData;

	pData = __xuiInputGetData(pWidget);
	if ( pData != NULL ) {
		(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiInputInvalidatePaint(xui_widget pWidget)
{
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiInputSyncImeMode(xui_widget pWidget, xui_input_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	(void)xuiWidgetSetImeMode(pWidget, (pData->bPassword || pData->bReadonly) ? XUI_IME_DISABLED : XUI_IME_AUTO);
}

static int __xuiInputSyncCursor(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInputEnsureCursorVisible(pWidget, pData);
	pContext = xuiWidgetGetContext(pWidget);
	if ( (pContext != NULL) && (xuiGetFocusWidget(pContext) == pWidget) ) {
		(void)xuiInternalInputSyncIme(pContext);
	}
	return __xuiInputInvalidatePaint(pWidget);
}

static void __xuiInputNotifyChange(xui_widget pWidget, xui_input_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, (pData->sText != NULL) ? pData->sText : "", pData->pChangeUser);
	}
}

static int __xuiInputRecordUndo(xui_input_data_t* pData)
{
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputStringSet(&pData->sUndoText, &pData->iUndoCapacity, pData->sText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pData->iUndoCursor = pData->iCursor;
	pData->iUndoSelectStart = pData->iSelectStart;
	pData->iUndoSelectEnd = pData->iSelectEnd;
	pData->bCanUndo = 1;
	pData->bCanRedo = 0;
	return XUI_OK;
}

static int __xuiInputRecordRedo(xui_input_data_t* pData)
{
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputStringSet(&pData->sRedoText, &pData->iRedoCapacity, pData->sText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pData->iRedoCursor = pData->iCursor;
	pData->iRedoSelectStart = pData->iSelectStart;
	pData->iRedoSelectEnd = pData->iSelectEnd;
	pData->bCanRedo = 1;
	return XUI_OK;
}

static int __xuiInputSetTextInternal(xui_widget pWidget, xui_input_data_t* pData, const char* sText, int bNotify, int bUndo)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( bUndo ) {
		iRet = __xuiInputRecordUndo(pData);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pData->iCursor = (sText != NULL) ? (int)strlen(sText) : 0;
	iRet = __xuiInputAssignTextBytes(pData, sText, -1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pData->iCursor = (int)strlen(pData->sText);
	(void)__xuiInputClearSelectionData(pData);
	__xuiInputEnsureCursorVisible(pWidget, pData);
	if ( bNotify ) {
		__xuiInputNotifyChange(pWidget, pData);
	}
	return __xuiInputInvalidateText(pWidget);
}

static int __xuiInputDecorationClick(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pDecoration == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = XUI_OK;
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR ) {
		iRet = __xuiInputSetTextInternal(pWidget, pData, "", 1, 1);
	}
	if ( (iRet == XUI_OK) && (pDecoration->onClick != NULL) ) {
		pDecoration->onClick(pWidget, pDecoration, pDecoration->pUser);
	}
	return iRet;
}

static int __xuiInputDeleteRange(xui_widget pWidget, xui_input_data_t* pData, int iStart, int iEnd, int bNotify, int bUndo)
{
	int iLen;
	int iMove;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = (int)strlen(pData->sText);
	iStart = __xuiInputUtf8Clamp(pData->sText, iLen, iStart);
	iEnd = __xuiInputUtf8Clamp(pData->sText, iLen, iEnd);
	if ( iStart > iEnd ) {
		int iTmp = iStart;
		iStart = iEnd;
		iEnd = iTmp;
	}
	if ( iStart == iEnd ) {
		return XUI_OK;
	}
	if ( bUndo ) {
		iRet = __xuiInputRecordUndo(pData);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iMove = iLen - iEnd + 1;
	memmove(pData->sText + iStart, pData->sText + iEnd, (size_t)iMove);
	pData->iCursor = iStart;
	(void)__xuiInputClearSelectionData(pData);
	__xuiInputEnsureCursorVisible(pWidget, pData);
	if ( bNotify ) {
		__xuiInputNotifyChange(pWidget, pData);
	}
	return __xuiInputInvalidateText(pWidget);
}

static int __xuiInputInsertText(xui_widget pWidget, xui_input_data_t* pData, const char* sInsert, int iInsertSize)
{
	int iStart;
	int iEnd;
	int iLen;
	int iSelected;
	int iAllowed;
	int iNewLen;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (sInsert == NULL) || pData->bReadonly ) {
		return XUI_OK;
	}
	if ( iInsertSize < 0 ) {
		iInsertSize = (int)strlen(sInsert);
	}
	if ( iInsertSize <= 0 ) {
		return XUI_OK;
	}
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	iLen = (int)strlen(pData->sText);
	iSelected = iEnd - iStart;
	if ( pData->iMaxLength > 0 ) {
		iAllowed = pData->iMaxLength - (iLen - iSelected);
		if ( iAllowed <= 0 ) {
			return XUI_OK;
		}
		if ( iInsertSize > iAllowed ) {
			iInsertSize = __xuiInputUtf8ClampBytes(sInsert, iAllowed);
		}
		if ( iInsertSize <= 0 ) {
			return XUI_OK;
		}
	}
	iRet = __xuiInputRecordUndo(pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iNewLen = iLen - iSelected + iInsertSize;
	iRet = __xuiInputTextReserve(&pData->sText, &pData->iTextCapacity, iNewLen + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memmove(pData->sText + iStart + iInsertSize, pData->sText + iEnd, (size_t)(iLen - iEnd + 1));
	memcpy(pData->sText + iStart, sInsert, (size_t)iInsertSize);
	pData->iCursor = iStart + iInsertSize;
	(void)__xuiInputClearSelectionData(pData);
	__xuiInputEnsureCursorVisible(pWidget, pData);
	__xuiInputNotifyChange(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

static int __xuiInputMoveSelectionToCursor(xui_widget pWidget, xui_input_data_t* pData, int iTarget)
{
	int iStart;
	int iEnd;
	int iLen;
	int iSelected;
	int iMove;
	int iNewStart;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->bReadonly || pData->bPassword ) {
		return XUI_OK;
	}
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	if ( iEnd <= iStart ) {
		return XUI_OK;
	}
	iLen = (int)strlen(pData->sText);
	iTarget = __xuiInputUtf8Clamp(pData->sText, iLen, iTarget);
	if ( (iTarget >= iStart) && (iTarget <= iEnd) ) {
		return XUI_OK;
	}
	iSelected = iEnd - iStart;
	iRet = __xuiInputTextReserve(&pData->sScratch, &pData->iScratchCapacity, iSelected + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(pData->sScratch, pData->sText + iStart, (size_t)iSelected);
	pData->sScratch[iSelected] = '\0';
	iRet = __xuiInputRecordUndo(pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iMove = iLen - iEnd + 1;
	memmove(pData->sText + iStart, pData->sText + iEnd, (size_t)iMove);
	iNewStart = (iTarget > iEnd) ? (iTarget - iSelected) : iTarget;
	memmove(pData->sText + iNewStart + iSelected, pData->sText + iNewStart, (size_t)(iLen - iSelected - iNewStart + 1));
	memcpy(pData->sText + iNewStart, pData->sScratch, (size_t)iSelected);
	(void)__xuiInputSetSelectionData(pData, iNewStart, iNewStart + iSelected);
	__xuiInputEnsureCursorVisible(pWidget, pData);
	__xuiInputNotifyChange(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

static int __xuiInputCopySelection(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_proxy pProxy;
	int iStart;
	int iEnd;
	int iLen;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || pData->bPassword || !__xuiInputHasSelectionData(pData) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->clipboardSetText == NULL) ) {
		return XUI_OK;
	}
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	iLen = iEnd - iStart;
	iRet = __xuiInputTextReserve(&pData->sScratch, &pData->iScratchCapacity, iLen + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(pData->sScratch, pData->sText + iStart, (size_t)iLen);
	pData->sScratch[iLen] = '\0';
	return pProxy->clipboardSetText(pProxy, pData->sScratch);
}

static int __xuiInputPasteClipboard(xui_widget pWidget, xui_input_data_t* pData)
{
	char* sText;
	int iLen;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || pData->bReadonly ) {
		return XUI_OK;
	}
	sText = NULL;
	iRet = xuiInternalClipboardReadText(xuiWidgetGetContext(pWidget), &sText, &iLen);
	if ( iRet == XUI_ERROR_UNSUPPORTED ) return XUI_OK;
	if ( iRet != XUI_OK ) return iRet;
	for ( iLen = 0; sText[iLen] != '\0'; iLen++ ) {
		if ( (sText[iLen] == '\r') || (sText[iLen] == '\n') ) {
			sText[iLen] = '\0';
			break;
		}
	}
	iRet = __xuiInputInsertText(pWidget, pData, sText, iLen);
	xrtFree(sText);
	return iRet;
}

static int __xuiInputMoveCursor(xui_widget pWidget, xui_input_data_t* pData, int iNewCursor, int bExtend)
{
	int iLen;
	int iAnchor;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = (int)strlen(pData->sText);
	iNewCursor = __xuiInputUtf8Clamp(pData->sText, iLen, iNewCursor);
	if ( bExtend ) {
		iAnchor = __xuiInputHasSelectionData(pData) ? pData->iAnchor : pData->iCursor;
		pData->iAnchor = iAnchor;
		pData->iCursor = iNewCursor;
		pData->iSelectStart = iAnchor;
		pData->iSelectEnd = iNewCursor;
	} else {
		pData->iCursor = iNewCursor;
		(void)__xuiInputClearSelectionData(pData);
	}
	return __xuiInputSyncCursor(pWidget, pData);
}

static int __xuiInputCursorFromPoint(xui_widget pWidget, xui_input_data_t* pData, float fX)
{
	xui_input_data_t tResolved;
	xui_rect_t tContent;
	xui_vec2_t tDisplaySize;
	const char* sDisplay;
	int iLen;
	int iPos;
	int iNext;
	float fQuery;
	float fPrev;
	float fNext;
	float fOffset;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->sText == NULL) ) {
		return 0;
	}
	__xuiInputResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	sDisplay = __xuiInputDisplayAll(pData);
	tDisplaySize = __xuiInputMeasureText(pWidget, tResolved.pFont, sDisplay);
	fOffset = (tDisplaySize.fX <= tContent.fW) ? __xuiInputAlignOffset(pData, tContent.fW, tDisplaySize.fX) : 0.0f;
	fQuery = fX - tContent.fX - fOffset + pData->fScrollX;
	if ( fQuery <= 0.0f ) {
		return 0;
	}
	iLen = (int)strlen(pData->sText);
	iPos = 0;
	fPrev = 0.0f;
	while ( iPos < iLen ) {
		iNext = __xuiInputUtf8Next(pData->sText, iLen, iPos);
		fNext = __xuiInputMeasurePrefix(pWidget, pData, tResolved.pFont, iNext);
		if ( fQuery < (fPrev + fNext) * 0.5f ) {
			return iPos;
		}
		iPos = iNext;
		fPrev = fNext;
	}
	return iLen;
}

static int __xuiInputHitSetCursor(xui_widget pWidget, xui_input_data_t* pData, const xui_event_t* pEvent, int bExtend)
{
	xui_rect_t tWorld;
	int iCursor;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	iCursor = __xuiInputCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX);
	return __xuiInputMoveCursor(pWidget, pData, iCursor, bExtend);
}

static int __xuiInputDecorationPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) && (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static xui_input_decoration __xuiInputDecorationHitList(xui_input_decoration pDecoration, float fX, float fY)
{
	while ( pDecoration != NULL ) {
		if ( __xuiInputDecorationPointInRect(pDecoration->tRect, fX, fY) ) {
			return pDecoration;
		}
		pDecoration = pDecoration->pNext;
	}
	return NULL;
}

static xui_input_decoration __xuiInputDecorationHit(xui_widget pWidget, xui_input_data_t* pData, float fWorldX, float fWorldY)
{
	xui_rect_t tWorld;
	xui_input_decoration pDecoration;
	float fX;
	float fY;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return NULL;
	}
	__xuiInputDecorationLayout(pWidget, pData, NULL, NULL);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fX = fWorldX - tWorld.fX;
	fY = fWorldY - tWorld.fY;
	pDecoration = __xuiInputDecorationHitList(pData->pLeadingDecoration, fX, fY);
	if ( pDecoration != NULL ) {
		return pDecoration;
	}
	return __xuiInputDecorationHitList(pData->pTrailingDecoration, fX, fY);
}

static int __xuiInputDecorationCanClick(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pDecoration == NULL) ) {
		return 0;
	}
	if ( !xuiWidgetGetEnabled(pWidget) || pData->bReadonly ) {
		return 0;
	}
	return (pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR) || (pDecoration->onClick != NULL);
}

static int __xuiInputDecorationSetHover(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pHoverDecoration == pDecoration ) {
		return XUI_OK;
	}
	pData->pHoverDecoration = pDecoration;
	return __xuiInputInvalidatePaint(pWidget);
}

static int __xuiInputDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiInputAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect(tRect);
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiInputDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiInputAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	tRect = xuiInternalStrokeCenterRectInside(tRect, fWidth, NULL);
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, xuiInternalSnapSize(fWidth), iColor) : XUI_OK;
}

static const char* __xuiInputDecorationIconAssetName(int iIcon)
{
	switch ( iIcon ) {
	case XUI_INPUT_ICON_SEARCH:
		return "input_search_12";
	case XUI_INPUT_ICON_USER:
		return "input_user_12";
	case XUI_INPUT_ICON_LOCK:
		return "input_lock_12";
	case XUI_INPUT_ICON_EYE:
		return "input_eye_12";
	default:
		return NULL;
	}
}

static const char* __xuiInputDecorationVectorIconName(xui_input_decoration pDecoration)
{
	if ( pDecoration == NULL ) {
		return NULL;
	}
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR ) {
		return "close";
	}
	if ( pDecoration->iKind != XUI_INPUT_DECORATION_ICON ) {
		return NULL;
	}
	switch ( pDecoration->iIcon ) {
	case XUI_INPUT_ICON_SEARCH:
		return "search";
	case XUI_INPUT_ICON_USER:
		return "user";
	case XUI_INPUT_ICON_LOCK:
		return "lock";
	case XUI_INPUT_ICON_EYE:
		return "eye";
	default:
		return NULL;
	}
}

static uint32_t __xuiInputDecorationColor(xui_input_decoration pDecoration, uint32_t iState)
{
	uint32_t iColor;

	if ( pDecoration == NULL ) {
		return XUI_COLOR_RGBA(76, 89, 109, 210);
	}
	iColor = (pDecoration->iColor != 0) ? pDecoration->iColor : XUI_COLOR_RGBA(76, 89, 109, 210);
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iColor = (pDecoration->iDisabledColor != 0) ? pDecoration->iDisabledColor : XUI_COLOR_RGBA(150, 160, 172, 150);
	} else if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) {
		iColor = (pDecoration->iActiveColor != 0) ? pDecoration->iActiveColor : XUI_COLOR_RGBA(31, 96, 184, 255);
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		iColor = (pDecoration->iHoverColor != 0) ? pDecoration->iHoverColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	}
	return iColor;
}

static xui_rect_t __xuiInputDecorationSurfaceSrc(xui_proxy pProxy, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_surface_desc_t tDesc;

	if ( (tSrc.fW > 0.0f) && (tSrc.fH > 0.0f) ) {
		return tSrc;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	if ( (pProxy != NULL) && (pProxy->surfaceGetDesc != NULL) &&
	     (pSurface != NULL) &&
	     (pProxy->surfaceGetDesc(pProxy, pSurface, &tDesc) == XUI_OK) ) {
		return (xui_rect_t){0.0f, 0.0f, (float)tDesc.iWidth, (float)tDesc.iHeight};
	}
	return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

static xui_rect_t __xuiInputDecorationCenterRect(xui_rect_t tOuter, float fW, float fH)
{
	if ( fW > tOuter.fW ) fW = tOuter.fW;
	if ( fH > tOuter.fH ) fH = tOuter.fH;
	if ( fW < 0.0f ) fW = 0.0f;
	if ( fH < 0.0f ) fH = 0.0f;
	return (xui_rect_t){
		tOuter.fX + (tOuter.fW - fW) * 0.5f,
		tOuter.fY + (tOuter.fH - fH) * 0.5f,
		fW,
		fH
	};
}

static xui_rect_t __xuiInputDecorationIconRect(xui_input_decoration pDecoration, xui_rect_t tOuter)
{
	float fSize;
	float fMax;

	fSize = 14.0f;
	if ( pDecoration != NULL ) {
		if ( pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR ) {
			fSize = 10.0f;
		} else if ( (pDecoration->iKind == XUI_INPUT_DECORATION_ICON) && (pDecoration->iIcon == XUI_INPUT_ICON_EYE) ) {
			fSize = 15.0f;
		}
	}
	fMax = tOuter.fH - 6.0f;
	if ( fMax < 8.0f ) fMax = tOuter.fH;
	if ( fSize > fMax ) fSize = fMax;
	if ( fSize < 8.0f ) fSize = 8.0f;
	return __xuiInputDecorationCenterRect(tOuter, fSize, fSize);
}

static int __xuiInputDecorationDrawSurface(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tRect, uint32_t iColor)
{
	xui_rect_t tDst;

	if ( (pProxy == NULL) || (pProxy->drawSurface == NULL) || (pSurface == NULL) ||
	     (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ||
	     (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ||
	     (__xuiInputAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	tDst = __xuiInputDecorationCenterRect(tRect, tSrc.fW, tSrc.fH);
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, xuiInternalSnapRect(tDst), iColor, 0);
}

static int __xuiInputDecorationDrawClearFallback(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	xui_rect_t tIcon;
	float fX0;
	float fY0;
	float fX1;
	float fY1;

	if ( (pProxy == NULL) || (pProxy->drawLine == NULL) || (__xuiInputAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	tIcon = __xuiInputDecorationCenterRect(tRect, 9.5f, 9.5f);
	fX0 = tIcon.fX + 2.0f;
	fY0 = tIcon.fY + 2.0f;
	fX1 = tIcon.fX + tIcon.fW - 2.0f;
	fY1 = tIcon.fY + tIcon.fH - 2.0f;
	(void)pProxy->drawLine(pProxy, pDraw, fX0, fY0, fX1, fY1, 1.2f, iColor);
	return pProxy->drawLine(pProxy, pDraw, fX1, fY0, fX0, fY1, 1.2f, iColor);
}

static uint32_t __xuiInputDecorationState(xui_widget pWidget, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	uint32_t iState;

	iState = 0u;
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( pData != NULL && pDecoration != NULL ) {
		if ( pData->pHoverDecoration == pDecoration ) iState |= XUI_WIDGET_STATE_HOVER;
		if ( pData->pActiveDecoration == pDecoration ) iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static int __xuiInputDecorationDrawOne(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	const char* sVectorIcon;
	const char* sAsset;
	xui_surface pAtlas;
	xui_rect_t tSrc;
	xui_rect_t tIconRect;
	uint32_t iState;
	uint32_t iColor;
	int iRet;

	if ( (pWidget == NULL) || (pDraw == NULL) || (pProxy == NULL) || (pData == NULL) || (pDecoration == NULL) ||
	     (pDecoration->tRect.fW <= 0.0f) || (pDecoration->tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iState = __xuiInputDecorationState(pWidget, pData, pDecoration);
	iColor = __xuiInputDecorationColor(pDecoration, iState);
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_CUSTOM_PAINT ) {
		return (pDecoration->onPaint != NULL) ?
			pDecoration->onPaint(pWidget, pDecoration, pDraw, pDecoration->tRect, iState, pDecoration->pUser) :
			XUI_OK;
	}
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_TEXT ) {
		if ( (pProxy->drawText != NULL) && (pData->pFont != NULL) &&
		     (pDecoration->sText != NULL) && (pDecoration->sText[0] != '\0') &&
		     (__xuiInputAlpha(iColor) != 0) ) {
			return pProxy->drawText(pProxy, pDraw, pData->pFont, pDecoration->sText, xuiInternalSnapRect(pDecoration->tRect), iColor,
				XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
		return XUI_OK;
	}
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_TEXTURE ) {
		tSrc = __xuiInputDecorationSurfaceSrc(pProxy, pDecoration->pSurface, pDecoration->tSrc);
		return __xuiInputDecorationDrawSurface(pProxy, pDraw, pDecoration->pSurface, tSrc, pDecoration->tRect, iColor);
	}
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_ICON ) {
		sAsset = __xuiInputDecorationIconAssetName(pDecoration->iIcon);
	} else if ( pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR ) {
		sAsset = "clear_10";
	} else {
		sAsset = NULL;
	}
	sVectorIcon = __xuiInputDecorationVectorIconName(pDecoration);
	if ( (sVectorIcon != NULL) && (pProxy->drawMeshTriangles != NULL) ) {
		xui_painter_t tPainter;

		memset(&tPainter, 0, sizeof(tPainter));
		tPainter.iMagic = XUI_PAINTER_MAGIC;
		tPainter.pContext = xuiWidgetGetContext(pWidget);
		tPainter.pDraw = pDraw;
		tIconRect = __xuiInputDecorationIconRect(pDecoration, pDecoration->tRect);
		iRet = xuiPainterDrawVectorIcon(&tPainter, sVectorIcon, xuiInternalSnapRect(tIconRect), iColor);
		if ( iRet == XUI_OK ) {
			return XUI_OK;
		}
		if ( (iRet != XUI_ERROR_UNSUPPORTED) && (iRet != XUI_ERROR_FILE_NOT_FOUND) ) {
			return iRet;
		}
	}
	if ( sAsset == NULL ) {
		return XUI_OK;
	}
	pAtlas = NULL;
	iRet = xuiBuiltinAssetGetAtlas(xuiWidgetGetContext(pWidget), &pAtlas);
	memset(&tSrc, 0, sizeof(tSrc));
	if ( iRet == XUI_OK ) {
		iRet = xuiBuiltinAssetGetRect(sAsset, &tSrc);
	}
	if ( (iRet == XUI_OK) && (pAtlas != NULL) ) {
		return __xuiInputDecorationDrawSurface(pProxy, pDraw, pAtlas, tSrc, pDecoration->tRect, iColor);
	}
	if ( pDecoration->iKind == XUI_INPUT_DECORATION_CLEAR ) {
		return __xuiInputDecorationDrawClearFallback(pProxy, pDraw, pDecoration->tRect, iColor);
	}
	return XUI_OK;
}

static int __xuiInputDecorationDrawList(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	int iRet;

	while ( pDecoration != NULL ) {
		iRet = __xuiInputDecorationDrawOne(pWidget, pDraw, pProxy, pData, pDecoration);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pDecoration = pDecoration->pNext;
	}
	return XUI_OK;
}

static int __xuiInputContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_input_data_t* pData;
	xui_input_data_t tResolved;
	xui_font_metrics_t tMetrics;
	xui_proxy pProxy;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInputResolve(pWidget, pData, &tResolved);
	pSize->fX = 180.0f;
	pSize->fY = 20.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (tResolved.pFont != NULL) &&
	     (pProxy->fontGetMetrics(pProxy, tResolved.pFont, &tMetrics) == XUI_OK) &&
	     (tMetrics.fLineHeight > 0.0f) ) {
		pSize->fY = tMetrics.fLineHeight + 4.0f;
	}
	if ( pSize->fY < 22.0f ) {
		pSize->fY = 22.0f;
	}
	return XUI_OK;
}

static int __xuiInputDrawSelection(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_input_data_t* pData, xui_input_data_t* pResolved, xui_rect_t tContent, float fOffset)
{
	xui_rect_t tSel;
	int iStart;
	int iEnd;
	float fX0;
	float fX1;

	if ( !__xuiInputHasSelectionData(pData) ) {
		return XUI_OK;
	}
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	fX0 = tContent.fX + fOffset + __xuiInputMeasurePrefix(pWidget, pData, pResolved->pFont, iStart) - pData->fScrollX;
	fX1 = tContent.fX + fOffset + __xuiInputMeasurePrefix(pWidget, pData, pResolved->pFont, iEnd) - pData->fScrollX;
	if ( fX0 < tContent.fX ) fX0 = tContent.fX;
	if ( fX1 > tContent.fX + tContent.fW ) fX1 = tContent.fX + tContent.fW;
	if ( fX1 <= fX0 ) {
		return XUI_OK;
	}
	tSel.fX = fX0;
	tSel.fY = tContent.fY + 3.0f;
	tSel.fW = fX1 - fX0;
	tSel.fH = tContent.fH - 6.0f;
	return __xuiInputDrawRectFill(pProxy, pDraw, tSel, pResolved->iSelectionColor);
}

static int __xuiInputFirstVisibleByte(xui_widget pWidget, xui_input_data_t* pData, xui_font pFont)
{
	int iLen;
	int iPos;
	int iNext;
	float fWidth;

	if ( (pData == NULL) || (pData->sText == NULL) || (pData->fScrollX <= 0.0f) ) {
		return 0;
	}
	iLen = (int)strlen(pData->sText);
	iPos = 0;
	while ( iPos < iLen ) {
		iNext = __xuiInputUtf8Next(pData->sText, iLen, iPos);
		fWidth = __xuiInputMeasurePrefix(pWidget, pData, pFont, iNext);
		if ( fWidth >= pData->fScrollX ) {
			return iPos;
		}
		iPos = iNext;
	}
	return iLen;
}

static int __xuiInputCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_input_data_t* pData;
	xui_input_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tContent;
	xui_rect_t tText;
	xui_rect_t tCursor;
	xui_vec2_t tDisplaySize;
	const char* sDisplay;
	const char* sDrawText;
	uint32_t iState;
	uint32_t iBackground;
	uint32_t iBorder;
	uint32_t iText;
	float fOffset;
	int iLen;
	int iDrawStart;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiInputResolve(pWidget, pData, &tResolved);
	iState = xuiInputGetState(pWidget);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iBackground = tResolved.iBackgroundColor;
	iBorder = tResolved.iBorderColor;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iBackground = tResolved.iDisabledBackgroundColor;
		iBorder = __xuiInputColorWithAlpha(tResolved.iBorderColor, 120);
	} else if ( (iState & XUI_INPUT_STATE_ERROR) != 0 ) {
		iBackground = tResolved.iErrorBackgroundColor;
		iBorder = tResolved.iErrorBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		iBackground = tResolved.iHoverBackgroundColor;
		iBorder = tResolved.iHoverBorderColor;
	}
	iRet = __xuiInputDrawRectFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiInputDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	__xuiInputDecorationLayout(pWidget, pData, NULL, NULL);
	iRet = __xuiInputDecorationDrawList(pWidget, pDraw, pProxy, pData, pData->pLeadingDecoration);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiInputDecorationDrawList(pWidget, pDraw, pProxy, pData, pData->pTrailingDecoration);
	if ( iRet != XUI_OK ) return iRet;

	tContent = xuiWidgetGetContentRect(pWidget);
	if ( tContent.fW <= 0.0f || tContent.fH <= 0.0f ) {
		return XUI_OK;
	}
	pData->tTextRect = tContent;
	__xuiInputEnsureCursorVisible(pWidget, pData);
	iLen = (pData->sText != NULL) ? (int)strlen(pData->sText) : 0;
	sDisplay = __xuiInputDisplayAll(pData);
	tDisplaySize = __xuiInputMeasureText(pWidget, tResolved.pFont, sDisplay);
	fOffset = (tDisplaySize.fX <= tContent.fW && pData->fScrollX <= 0.0f) ? __xuiInputAlignOffset(pData, tContent.fW, tDisplaySize.fX) : 0.0f;
	iRet = __xuiInputDrawSelection(pWidget, pDraw, pProxy, pData, &tResolved, tContent, fOffset);
	if ( iRet != XUI_OK ) return iRet;

	if ( (tResolved.pFont != NULL) && (pProxy->drawText != NULL) ) {
		if ( iLen > 0 ) {
			if ( tDisplaySize.fX > tContent.fW ) {
				iDrawStart = __xuiInputFirstVisibleByte(pWidget, pData, tResolved.pFont);
				(void)__xuiInputBuildDisplayRange(pData, iDrawStart, iLen, &pData->sDisplay, &pData->iDisplayCapacity);
				sDrawText = pData->sDisplay;
			} else {
				iDrawStart = 0;
				sDrawText = sDisplay;
			}
			(void)iDrawStart;
			tText = tContent;
			tText.fX += fOffset;
			tText.fW -= fOffset;
			iText = ((iState & XUI_WIDGET_STATE_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
			if ( (__xuiInputAlpha(iText) != 0) && (tText.fW > 0.0f) && (tText.fH > 0.0f) ) {
				iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, sDrawText, tText, iText,
					XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
		} else if ( (pData->sPlaceholder != NULL) && (pData->sPlaceholder[0] != '\0') &&
		            (__xuiInputAlpha(tResolved.iPlaceholderColor) != 0) ) {
			uint32_t iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
			if ( pData->iTextAlign == XUI_INPUT_ALIGN_CENTER ) iFlags = XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
			if ( pData->iTextAlign == XUI_INPUT_ALIGN_RIGHT ) iFlags = XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sPlaceholder, tContent, tResolved.iPlaceholderColor, iFlags);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	tCursor = __xuiInputCalculateCursorRect(pWidget, pData);
	pData->tCursorRect = tCursor;
	if ( ((iState & XUI_WIDGET_STATE_FOCUS) != 0) &&
	     ((iState & XUI_WIDGET_STATE_DISABLED) == 0) &&
	     !__xuiInputHasSelectionData(pData) &&
	     (__xuiInputAlpha(tResolved.iCursorColor) != 0) &&
	     (tCursor.fH > 0.0f) &&
	     (pProxy->drawRectFill != NULL) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tCursor), tResolved.iCursorColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiInputUpdateMenu(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_menu_item_t arrItems[9];
	uint32_t iEnabled;
	int bHasSelection;
	int bReadonly;
	int bCanCopy;
	int bAllSelected;
	int iLen;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(arrItems, 0, sizeof(arrItems));
	bHasSelection = __xuiInputHasSelectionData(pData);
	bReadonly = pData->bReadonly;
	bCanCopy = bHasSelection && !pData->bPassword;
	iLen = (pData->sText != NULL) ? (int)strlen(pData->sText) : 0;
	bAllSelected = bHasSelection && (pData->iSelectStart == 0 || pData->iSelectEnd == 0) &&
	               (pData->iSelectStart == iLen || pData->iSelectEnd == iLen);
	iEnabled = XUI_MENU_ITEM_ENABLED;
	arrItems[0].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_UNDO);
	arrItems[0].sShortcut = "Ctrl+Z";
	arrItems[0].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[0].iState = pData->bCanUndo ? iEnabled : 0u;
	arrItems[0].iValue = XUI_INPUT_MENU_UNDO;
	arrItems[1].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_REDO);
	arrItems[1].sShortcut = "Ctrl+Y";
	arrItems[1].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[1].iState = pData->bCanRedo ? iEnabled : 0u;
	arrItems[1].iValue = XUI_INPUT_MENU_REDO;
	arrItems[2].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[3].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_CUT);
	arrItems[3].sShortcut = "Ctrl+X";
	arrItems[3].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[3].iState = (bCanCopy && !bReadonly) ? iEnabled : 0u;
	arrItems[3].iValue = XUI_INPUT_MENU_CUT;
	arrItems[4].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_COPY);
	arrItems[4].sShortcut = "Ctrl+C";
	arrItems[4].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[4].iState = bCanCopy ? iEnabled : 0u;
	arrItems[4].iValue = XUI_INPUT_MENU_COPY;
	arrItems[5].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_PASTE);
	arrItems[5].sShortcut = "Ctrl+V";
	arrItems[5].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[5].iState = !bReadonly ? iEnabled : 0u;
	arrItems[5].iValue = XUI_INPUT_MENU_PASTE;
	arrItems[6].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_DELETE);
	arrItems[6].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[6].iState = (bHasSelection && !bReadonly) ? iEnabled : 0u;
	arrItems[6].iValue = XUI_INPUT_MENU_DELETE;
	arrItems[7].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[8].sText = xuiInputGetMenuTitle(pWidget, XUI_INPUT_MENU_SELECT_ALL);
	arrItems[8].sShortcut = "Ctrl+A";
	arrItems[8].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[8].iState = (iLen > 0 && !bAllSelected) ? iEnabled : 0u;
	arrItems[8].iValue = XUI_INPUT_MENU_SELECT_ALL;
	return xuiMenuSetItems(pData->pMenu, arrItems, 9);
}

static void __xuiInputMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pInput;

	(void)pMenu;
	(void)iIndex;
	pInput = (xui_widget)pUser;
	if ( pInput == NULL ) {
		return;
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pInput), pInput);
	switch ( iValue ) {
	case XUI_INPUT_MENU_UNDO:
		(void)xuiInputUndo(pInput);
		break;
	case XUI_INPUT_MENU_REDO:
		(void)xuiInputRedo(pInput);
		break;
	case XUI_INPUT_MENU_CUT:
		(void)xuiInputCut(pInput);
		break;
	case XUI_INPUT_MENU_COPY:
		(void)xuiInputCopy(pInput);
		break;
	case XUI_INPUT_MENU_PASTE:
		(void)xuiInputPaste(pInput);
		break;
	case XUI_INPUT_MENU_DELETE:
		(void)xuiInputDeleteSelection(pInput);
		break;
	case XUI_INPUT_MENU_SELECT_ALL:
		(void)xuiInputSelectAll(pInput);
		break;
	default:
		break;
	}
}

static int __xuiInputHandleKey(xui_widget pWidget, xui_input_data_t* pData, const xui_event_t* pEvent)
{
	int iLen;
	int iStart;
	int iEnd;
	int iKey;
	int bShift;
	int bCtrl;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLen = (pData->sText != NULL) ? (int)strlen(pData->sText) : 0;
	iKey = pEvent->iKey;
	bShift = ((pEvent->iModifiers & XUI_MOD_SHIFT) != 0);
	bCtrl = ((pEvent->iModifiers & XUI_MOD_CTRL) != 0);
	if ( bCtrl ) {
		iKey = toupper((unsigned char)iKey);
		if ( iKey == 'A' ) {
			(void)xuiInputSelectAll(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'C' ) {
			(void)xuiInputCopy(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'X' ) {
			(void)xuiInputCut(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'V' ) {
			(void)xuiInputPaste(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'Z' ) {
			(void)xuiInputUndo(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iKey == 'Y' ) {
			(void)xuiInputRedo(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_LEFT:
		if ( __xuiInputHasSelectionData(pData) && !bShift ) {
			__xuiInputSelectionRange(pData, &iStart, &iEnd);
			return __xuiInputMoveCursor(pWidget, pData, iStart, 0) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		(void)__xuiInputMoveCursor(pWidget, pData,
			bCtrl ? __xuiInputPrevWord(pData) : __xuiInputUtf8Prev(pData->sText, iLen, pData->iCursor), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
		if ( __xuiInputHasSelectionData(pData) && !bShift ) {
			__xuiInputSelectionRange(pData, &iStart, &iEnd);
			return __xuiInputMoveCursor(pWidget, pData, iEnd, 0) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		(void)__xuiInputMoveCursor(pWidget, pData,
			bCtrl ? __xuiInputNextWord(pData) : __xuiInputUtf8Next(pData->sText, iLen, pData->iCursor), bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		(void)__xuiInputMoveCursor(pWidget, pData, 0, bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		(void)__xuiInputMoveCursor(pWidget, pData, iLen, bShift);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_INPUT_KEY_BACKSPACE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		if ( __xuiInputHasSelectionData(pData) ) {
			(void)xuiInputDeleteSelection(pWidget);
		} else if ( pData->iCursor > 0 ) {
			(void)__xuiInputDeleteRange(pWidget, pData,
				bCtrl ? __xuiInputPrevWord(pData) : __xuiInputUtf8Prev(pData->sText, iLen, pData->iCursor),
				pData->iCursor, 1, 1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_INPUT_KEY_DELETE:
		if ( pData->bReadonly ) return XUI_EVENT_DISPATCH_STOP;
		if ( __xuiInputHasSelectionData(pData) ) {
			(void)xuiInputDeleteSelection(pWidget);
		} else if ( pData->iCursor < iLen ) {
			(void)__xuiInputDeleteRange(pWidget, pData, pData->iCursor,
				bCtrl ? __xuiInputNextWord(pData) : __xuiInputUtf8Next(pData->sText, iLen, pData->iCursor), 1, 1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiInputEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_input_data_t* pData;
	xui_context pContext;
	char sText[8];
	int iTextSize;
	xui_rect_t tWorld;
	xui_input_decoration pDecoration;
	xui_input_decoration pActiveDecoration;
	float fX;
	float fY;
	int iCursor;
	int iStart;
	int iEnd;
	int bInsideSelection;
	float fDX;
	float fDY;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
		pDecoration = __xuiInputDecorationHit(pWidget, pData, pEvent->fX, pEvent->fY);
		(void)__xuiInputDecorationSetHover(pWidget, pData, pDecoration);
		return __xuiInputInvalidatePaint(pWidget);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->pActiveDecoration == NULL ) {
			pData->pHoverDecoration = NULL;
		}
		return __xuiInputInvalidatePaint(pWidget);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		if ( pEvent->iType == XUI_EVENT_BLUR ) {
			pData->pHoverDecoration = NULL;
			pData->pActiveDecoration = NULL;
		}
		(void)__xuiInputDecorationSyncPadding(pWidget, pData);
		return __xuiInputInvalidateText(pWidget);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		return __xuiInputInvalidatePaint(pWidget);
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			(void)xuiSetFocusWidget(pContext, pWidget);
			(void)__xuiInputDecorationSyncPadding(pWidget, pData);
			pDecoration = __xuiInputDecorationHit(pWidget, pData, pEvent->fX, pEvent->fY);
			if ( __xuiInputDecorationCanClick(pWidget, pData, pDecoration) ) {
				(void)xuiSetPointerCapture(pContext, pWidget);
				pData->bDragging = 0;
				pData->bPressPending = 0;
				pData->bPressInsideSelection = 0;
				pData->bMovingSelection = 0;
				pData->pActiveDecoration = pDecoration;
				pData->pHoverDecoration = pDecoration;
				(void)__xuiInputInvalidatePaint(pWidget);
				return XUI_EVENT_DISPATCH_STOP;
			}
			(void)xuiSetPointerCapture(pContext, pWidget);
			pData->bDragging = 0;
			pData->bPressPending = 0;
			pData->bPressInsideSelection = 0;
			pData->bMovingSelection = 0;
			tWorld = xuiWidgetGetWorldRect(pWidget);
			iCursor = __xuiInputCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX);
			__xuiInputSelectionRange(pData, &iStart, &iEnd);
			bInsideSelection = (iStart != iEnd) && (iCursor >= iStart) && (iCursor <= iEnd);
			if ( ((pEvent->iModifiers & XUI_MOD_SHIFT) == 0) && bInsideSelection && !pData->bReadonly && !pData->bPassword ) {
				pData->bPressPending = 1;
				pData->bPressInsideSelection = 1;
				pData->iPressCursor = iCursor;
				pData->fPressX = pEvent->fX;
				pData->fPressY = pEvent->fY;
			} else {
				pData->bDragging = 1;
				(void)__xuiInputHitSetCursor(pWidget, pData, pEvent, ((pEvent->iModifiers & XUI_MOD_SHIFT) != 0));
				pData->iDragAnchor = pData->iAnchor;
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		pDecoration = __xuiInputDecorationHit(pWidget, pData, pEvent->fX, pEvent->fY);
		(void)__xuiInputDecorationSetHover(pWidget, pData, pDecoration);
		if ( pData->pActiveDecoration != NULL ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bPressPending ) {
			fDX = pEvent->fX - pData->fPressX;
			fDY = pEvent->fY - pData->fPressY;
			if ( (fDX * fDX + fDY * fDY) <= 36.0f ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
			pData->bPressPending = 0;
			if ( pData->bPressInsideSelection ) {
				pData->bMovingSelection = 1;
				return XUI_EVENT_DISPATCH_STOP;
			}
			pData->bDragging = 1;
			(void)__xuiInputHitSetCursor(pWidget, pData, pEvent, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bMovingSelection ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bDragging ) {
			(void)__xuiInputHitSetCursor(pWidget, pData, pEvent, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			if ( pData->pActiveDecoration != NULL ) {
				pActiveDecoration = pData->pActiveDecoration;
				pDecoration = __xuiInputDecorationHit(pWidget, pData, pEvent->fX, pEvent->fY);
				pData->pActiveDecoration = NULL;
				pData->pHoverDecoration = pDecoration;
				(void)xuiReleasePointerCapture(pContext, pWidget);
				if ( pDecoration == pActiveDecoration ) {
					(void)__xuiInputDecorationClick(pWidget, pData, pActiveDecoration);
				}
				(void)__xuiInputInvalidatePaint(pWidget);
				return XUI_EVENT_DISPATCH_STOP;
			}
			if ( pData->bMovingSelection ) {
				tWorld = xuiWidgetGetWorldRect(pWidget);
				iCursor = __xuiInputCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX);
				(void)__xuiInputMoveSelectionToCursor(pWidget, pData, iCursor);
			} else if ( pData->bPressPending ) {
				(void)__xuiInputHitSetCursor(pWidget, pData, pEvent, 0);
			}
			pData->bPressPending = 0;
			pData->bPressInsideSelection = 0;
			pData->bMovingSelection = 0;
			pData->bDragging = 0;
			(void)xuiReleasePointerCapture(pContext, pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDragging = 0;
		pData->bPressPending = 0;
		pData->bPressInsideSelection = 0;
		pData->bMovingSelection = 0;
		pData->pActiveDecoration = NULL;
		return XUI_OK;
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		pDecoration = __xuiInputDecorationHit(pWidget, pData, pEvent->fX, pEvent->fY);
		if ( __xuiInputDecorationCanClick(pWidget, pData, pDecoration) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		tWorld = xuiWidgetGetWorldRect(pWidget);
		iCursor = __xuiInputCursorFromPoint(pWidget, pData, pEvent->fX - tWorld.fX);
		(void)__xuiInputSelectWordAt(pWidget, pData, iCursor);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_CONTEXT_MENU:
		(void)xuiSetFocusWidget(pContext, pWidget);
		pData->bDragging = 0;
		pData->bPressPending = 0;
		pData->bPressInsideSelection = 0;
		pData->bMovingSelection = 0;
		(void)xuiReleasePointerCapture(pContext, pWidget);
		if ( pEvent->iKey != XUI_KEY_CONTEXT_MENU && !__xuiInputHasSelectionData(pData) ) {
			(void)__xuiInputHitSetCursor(pWidget, pData, pEvent, 0);
		}
		if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
			tWorld = xuiWidgetGetWorldRect(pWidget);
			fX = tWorld.fX + pData->tCursorRect.fX;
			fY = tWorld.fY + pData->tCursorRect.fY + pData->tCursorRect.fH;
		} else {
			fX = pEvent->fX;
			fY = pEvent->fY;
		}
		(void)xuiInputOpenMenu(pWidget, fX, fY);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_KEY_DOWN:
		return __xuiInputHandleKey(pWidget, pData, pEvent);
	case XUI_EVENT_TEXT:
		if ( pData->bReadonly || ((pEvent->iModifiers & XUI_MOD_CTRL) != 0) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		iTextSize = 0;
		if ( pEvent->iTextSize > 0 && pEvent->sText[0] != '\0' ) {
			iTextSize = pEvent->iTextSize;
			if ( iTextSize > (int)sizeof(sText) - 1 ) {
				iTextSize = (int)sizeof(sText) - 1;
			}
			memcpy(sText, pEvent->sText, (size_t)iTextSize);
		} else if ( pEvent->iCodepoint >= 32u ) {
			iTextSize = __xuiInputUtf8Encode(pEvent->iCodepoint, sText);
		}
		if ( iTextSize > 0 ) {
			sText[iTextSize] = '\0';
			(void)__xuiInputInsertText(pWidget, pData, sText, iTextSize);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_IME_COMPOSITION:
		if ( pData->bReadonly ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iCompositionLength == 0 && pEvent->iTextSize > 0 && pEvent->sText[0] != '\0' ) {
			(void)__xuiInputInsertText(pWidget, pData, pEvent->sText, pEvent->iTextSize);
		}
		return __xuiInputInvalidatePaint(pWidget) == XUI_OK ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static xui_rect_t __xuiInputImeRect(xui_widget pWidget, void* pUser)
{
	xui_input_data_t* pData;
	xui_rect_t tWorld;
	xui_rect_t tRect;

	(void)pUser;
	pData = __xuiInputGetData(pWidget);
	memset(&tRect, 0, sizeof(tRect));
	if ( pData == NULL ) {
		return tRect;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRect = __xuiInputCalculateCursorRect(pWidget, pData);
	pData->tCursorRect = tRect;
	tRect.fX += tWorld.fX;
	tRect.fY += tWorld.fY;
	return tRect;
}

static void __xuiInputDefaultLayout(xui_layout_t* pLayout)
{
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
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiInputDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiInputInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_TEXT, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_IME_COMPOSITION, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiInputEvent, NULL);
	return iRet;
}

static int __xuiInputInitMenu(xui_widget pWidget, xui_input_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pMenu, &tDesc);
	if ( iRet != XUI_OK ) {
		pData->pMenu = NULL;
		return iRet;
	}
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiInputMenuSelect, pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiInputUpdateMenu(pWidget, pData);
}

static int __xuiInputInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_input_data_t* pData;
	const xui_input_desc_t* pDesc;
	xui_theme_t tTheme;
	xui_thickness_t tPadding;
	xui_context pContext;
	int iRet;

	(void)pUser;
	pData = (xui_input_data_t*)pTypeData;
	pDesc = (const xui_input_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiInputDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tTheme, 0, sizeof(tTheme));
	tTheme.iSize = sizeof(tTheme);
	(void)xuiGetTheme(pContext, &tTheme);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iMaxLength = (pDesc != NULL) ? pDesc->iMaxLength : 0;
	pData->iTextAlign = (pDesc != NULL) ? pDesc->iTextAlign : XUI_INPUT_ALIGN_LEFT;
	pData->bPassword = (pDesc != NULL) ? (pDesc->bPassword != 0) : 0;
	pData->bReadonly = (pDesc != NULL) ? (pDesc->bReadonly != 0) : 0;
	pData->bError = (pDesc != NULL) ? (pDesc->bError != 0) : 0;
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(31, 41, 55, 255);
	pData->iPlaceholderColor = (pDesc != NULL && pDesc->iPlaceholderColor != 0) ? pDesc->iPlaceholderColor : XUI_COLOR_RGBA(135, 148, 166, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(150, 160, 172, 255);
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iHoverBackgroundColor = (pDesc != NULL && pDesc->iHoverBackgroundColor != 0) ? pDesc->iHoverBackgroundColor : XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iDisabledBackgroundColor = (pDesc != NULL && pDesc->iDisabledBackgroundColor != 0) ? pDesc->iDisabledBackgroundColor : XUI_COLOR_RGBA(242, 245, 249, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(176, 190, 207, 255);
	pData->iHoverBorderColor = (pDesc != NULL && pDesc->iHoverBorderColor != 0) ? pDesc->iHoverBorderColor : XUI_COLOR_RGBA(105, 166, 226, 255);
	pData->iFocusBorderColor = (pDesc != NULL && pDesc->iFocusBorderColor != 0) ? pDesc->iFocusBorderColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iErrorBackgroundColor = (pDesc != NULL && pDesc->iErrorBackgroundColor != 0) ? pDesc->iErrorBackgroundColor : XUI_COLOR_RGBA(255, 248, 248, 255);
	pData->iErrorBorderColor = (pDesc != NULL && pDesc->iErrorBorderColor != 0) ? pDesc->iErrorBorderColor : XUI_COLOR_RGBA(220, 72, 72, 255);
	pData->iSelectionColor = (pDesc != NULL && pDesc->iSelectionColor != 0) ? pDesc->iSelectionColor : XUI_COLOR_RGBA(47, 128, 237, 78);
	pData->iCursorColor = (pDesc != NULL && pDesc->iCursorColor != 0) ? pDesc->iCursorColor : XUI_COLOR_RGBA(33, 94, 170, 255);
	pData->fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : 1.0f;
	iRet = __xuiInputAssignTextBytes(pData, (pDesc != NULL) ? pDesc->sText : "", -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iCursor = (int)strlen(pData->sText);
	(void)__xuiInputClearSelectionData(pData);
	iRet = __xuiInputStringSet(&pData->sPlaceholder, &pData->iPlaceholderCapacity, (pDesc != NULL) ? pDesc->sPlaceholder : "");
	if ( iRet != XUI_OK ) return iRet;
	tPadding = (xui_thickness_t){XUI_INPUT_PADDING_LEFT, XUI_INPUT_PADDING_TOP, XUI_INPUT_PADDING_RIGHT, XUI_INPUT_PADDING_BOTTOM};
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	__xuiInputSyncImeMode(pWidget, pData);
	(void)xuiWidgetSetImeCandidateRect(pWidget, __xuiInputImeRect, NULL);
	iRet = __xuiInputInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiInputInitMenu(pWidget, pData);
}

static void __xuiInputDecorationFree(xui_input_decoration pDecoration)
{
	if ( pDecoration == NULL ) {
		return;
	}
	if ( pDecoration->sText != NULL ) {
		xrtFree(pDecoration->sText);
	}
	xrtFree(pDecoration);
}

static void __xuiInputDecorationFreeList(xui_input_decoration pDecoration)
{
	xui_input_decoration pNext;

	while ( pDecoration != NULL ) {
		pNext = pDecoration->pNext;
		__xuiInputDecorationFree(pDecoration);
		pDecoration = pNext;
	}
}

static void __xuiInputDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_input_data_t* pData;
	xui_widget pPopup;
	int i;

	(void)pUser;
	pData = (xui_input_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pData->pMenu != NULL ) {
		pPopup = xuiMenuGetPopupWidget(pData->pMenu);
		if ( pPopup != NULL ) {
			xuiWidgetDestroy(pPopup);
		} else {
			xuiWidgetDestroy(pData->pMenu);
		}
		pData->pMenu = NULL;
	}
	__xuiInputDecorationFreeList(pData->pLeadingDecoration);
	__xuiInputDecorationFreeList(pData->pTrailingDecoration);
	pData->pLeadingDecoration = NULL;
	pData->pTrailingDecoration = NULL;
	pData->pHoverDecoration = NULL;
	pData->pActiveDecoration = NULL;
	if ( pData->sText != NULL ) xrtFree(pData->sText);
	if ( pData->sPlaceholder != NULL ) xrtFree(pData->sPlaceholder);
	if ( pData->sDisplay != NULL ) xrtFree(pData->sDisplay);
	if ( pData->sScratch != NULL ) xrtFree(pData->sScratch);
	if ( pData->sUndoText != NULL ) xrtFree(pData->sUndoText);
	if ( pData->sRedoText != NULL ) xrtFree(pData->sRedoText);
	for ( i = 0; i < XUI_INPUT_MENU_COUNT; i++ ) {
		if ( pData->arrMenuTitle[i] != NULL ) {
			xrtFree(pData->arrMenuTitle[i]);
		}
	}
	if ( (pWidget != NULL) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiInputRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) {
		return;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiInputRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	uint32_t iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;

	__xuiInputRegisterStyleProperty(pContext, pType, "input.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.placeholder.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.background.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.border.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.error.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.error.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.selection.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.cursor.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "input.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiInputRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiInputGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "input");
	if ( pType != NULL ) {
		__xuiInputRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "input";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_input_data_t);
	tDesc.onInit = __xuiInputInit;
	tDesc.onDestroy = __xuiInputDestroy;
	tDesc.onContentMeasure = __xuiInputContentMeasure;
	tDesc.onCacheRender = __xuiInputCacheRender;
	__xuiInputDefaultLayout(&tDesc.tLayout);
	__xuiInputDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiInputRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiInputCreate(xui_context pContext, xui_widget* ppWidget, const xui_input_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiInputDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiInputGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiInputSetChange(xui_widget pWidget, xui_input_change_proc onChange, void* pUser)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInputSetText(xui_widget pWidget, const char* sText)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bCanUndo = 0;
	pData->bCanRedo = 0;
	return __xuiInputSetTextInternal(pWidget, pData, sText, 0, 0);
}

XUI_API const char* xuiInputGetText(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : "";
}

XUI_API int xuiInputSetPlaceholder(xui_widget pWidget, const char* sText)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiInputStringSet(&pData->sPlaceholder, &pData->iPlaceholderCapacity, sText);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API const char* xuiInputGetPlaceholder(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL && pData->sPlaceholder != NULL) ? pData->sPlaceholder : "";
}

XUI_API int xuiInputSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	if ( pData->pMenu != NULL ) {
		(void)xuiMenuSetFont(pData->pMenu, pFont);
	}
	return __xuiInputInvalidateText(pWidget);
}

XUI_API xui_font xuiInputGetFont(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiInputSetMaxLength(xui_widget pWidget, int iMaxLength)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || (iMaxLength < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iMaxLength = iMaxLength;
	(void)__xuiInputAssignTextBytes(pData, pData->sText, -1);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputGetMaxLength(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->iMaxLength : 0;
}

XUI_API int xuiInputSetTextAlign(xui_widget pWidget, int iAlign)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputAlignValid(iAlign) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextAlign = iAlign;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputGetTextAlign(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->iTextAlign : XUI_INPUT_ALIGN_LEFT;
}

XUI_API int xuiInputSetPassword(xui_widget pWidget, int bPassword)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bPassword = (bPassword != 0);
	__xuiInputSyncImeMode(pWidget, pData);
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputIsPassword(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->bPassword : 0;
}

XUI_API int xuiInputSetReadonly(xui_widget pWidget, int bReadonly)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bReadonly = (bReadonly != 0);
	__xuiInputSyncImeMode(pWidget, pData);
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputIsReadonly(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->bReadonly : 0;
}

XUI_API int xuiInputSetError(xui_widget pWidget, int bError)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bError = (bError != 0);
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputGetError(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->bError : 0;
}

XUI_API int xuiInputSetSelection(xui_widget pWidget, int iStart, int iEnd)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiInputSetSelectionData(pData, iStart, iEnd);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiInputSyncCursor(pWidget, pData);
}

XUI_API int xuiInputGetSelection(xui_widget pWidget, int* pStart, int* pEnd)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiInputSelectionRange(pData, pStart, pEnd);
	return XUI_OK;
}

XUI_API int xuiInputSelectAll(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInputSetSelection(pWidget, 0, (int)strlen(pData->sText));
}

XUI_API int xuiInputHasSelection(xui_widget pWidget)
{
	return __xuiInputHasSelectionData(__xuiInputGetData(pWidget));
}

XUI_API int xuiInputCopy(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInputCopySelection(pWidget, pData);
}

XUI_API int xuiInputCut(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly || pData->bPassword ) return XUI_OK;
	(void)__xuiInputCopySelection(pWidget, pData);
	return xuiInputDeleteSelection(pWidget);
}

XUI_API int xuiInputPaste(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInputPasteClipboard(pWidget, pData);
}

XUI_API int xuiInputDeleteSelection(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iStart;
	int iEnd;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bReadonly || !__xuiInputHasSelectionData(pData) ) return XUI_OK;
	__xuiInputSelectionRange(pData, &iStart, &iEnd);
	return __xuiInputDeleteRange(pWidget, pData, iStart, iEnd, 1, 1);
}

XUI_API int xuiInputUndo(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pData->bCanUndo ) return XUI_OK;
	iRet = __xuiInputRecordRedo(pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiInputAssignTextBytes(pData, pData->sUndoText, -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iCursor = pData->iUndoCursor;
	pData->iSelectStart = pData->iUndoSelectStart;
	pData->iSelectEnd = pData->iUndoSelectEnd;
	pData->iAnchor = pData->iSelectStart;
	pData->bCanUndo = 0;
	__xuiInputEnsureCursorVisible(pWidget, pData);
	__xuiInputNotifyChange(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputRedo(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pData->bCanRedo ) return XUI_OK;
	iRet = __xuiInputStringSet(&pData->sUndoText, &pData->iUndoCapacity, pData->sText);
	if ( iRet != XUI_OK ) return iRet;
	pData->iUndoCursor = pData->iCursor;
	pData->iUndoSelectStart = pData->iSelectStart;
	pData->iUndoSelectEnd = pData->iSelectEnd;
	pData->bCanUndo = 1;
	iRet = __xuiInputAssignTextBytes(pData, pData->sRedoText, -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iCursor = pData->iRedoCursor;
	pData->iSelectStart = pData->iRedoSelectStart;
	pData->iSelectEnd = pData->iRedoSelectEnd;
	pData->iAnchor = pData->iSelectStart;
	pData->bCanRedo = 0;
	__xuiInputEnsureCursorVisible(pWidget, pData);
	__xuiInputNotifyChange(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputCanUndo(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->bCanUndo : 0;
}

XUI_API int xuiInputCanRedo(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->bCanRedo : 0;
}

XUI_API int xuiInputSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iText, uint32_t iBorder, uint32_t iFocus)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iBackground;
	pData->iTextColor = iText;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iFocus;
	pData->iFocusBorderColor = iFocus;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pText, uint32_t* pBorder, uint32_t* pFocus)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiInputSetErrorColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iErrorBackgroundColor = iBackground;
	pData->iErrorBorderColor = iBorder;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputGetErrorColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iErrorBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iErrorBorderColor;
	return XUI_OK;
}

XUI_API int xuiInputSetExtendedColors(xui_widget pWidget, uint32_t iPlaceholder, uint32_t iDisabledText, uint32_t iHoverBackground, uint32_t iDisabledBackground, uint32_t iHoverBorder, uint32_t iSelection, uint32_t iCursor)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPlaceholderColor = iPlaceholder;
	pData->iDisabledTextColor = iDisabledText;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iSelectionColor = iSelection;
	pData->iCursorColor = iCursor;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API int xuiInputGetExtendedColors(xui_widget pWidget, uint32_t* pPlaceholder, uint32_t* pDisabledText, uint32_t* pHoverBackground, uint32_t* pDisabledBackground, uint32_t* pHoverBorder, uint32_t* pSelection, uint32_t* pCursor)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPlaceholder != NULL ) *pPlaceholder = pData->iPlaceholderColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pSelection != NULL ) *pSelection = pData->iSelectionColor;
	if ( pCursor != NULL ) *pCursor = pData->iCursorColor;
	return XUI_OK;
}

XUI_API int xuiInputSetBorderWidth(xui_widget pWidget, float fBorderWidth)
{
	xui_input_data_t* pData;
	if ( fBorderWidth < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth;
	return __xuiInputInvalidatePaint(pWidget);
}

XUI_API float xuiInputGetBorderWidth(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->fBorderWidth : 0.0f;
}

static xui_input_decoration* __xuiInputDecorationListRef(xui_input_data_t* pData, int iSide)
{
	if ( pData == NULL ) {
		return NULL;
	}
	if ( iSide == XUI_INPUT_DECORATION_SIDE_LEADING ) {
		return &pData->pLeadingDecoration;
	}
	if ( iSide == XUI_INPUT_DECORATION_SIDE_TRAILING ) {
		return &pData->pTrailingDecoration;
	}
	return NULL;
}

static int __xuiInputDecorationBelongsToList(xui_input_decoration pHead, xui_input_decoration pDecoration)
{
	while ( pHead != NULL ) {
		if ( pHead == pDecoration ) {
			return 1;
		}
		pHead = pHead->pNext;
	}
	return 0;
}

static int __xuiInputDecorationBelongs(xui_input_data_t* pData, xui_input_decoration pDecoration)
{
	return (pData != NULL) && (pDecoration != NULL) &&
	       (__xuiInputDecorationBelongsToList(pData->pLeadingDecoration, pDecoration) ||
	        __xuiInputDecorationBelongsToList(pData->pTrailingDecoration, pDecoration));
}

static int __xuiInputDecorationCopyDesc(xui_input_decoration pDecoration, const xui_input_decoration_desc_t* pDesc)
{
	char* sText;

	if ( (pDecoration == NULL) || !__xuiInputDecorationDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sText = NULL;
	if ( (pDesc->sText != NULL) && (pDesc->sText[0] != '\0') ) {
		sText = __xuiInputStringDuplicate(pDesc->sText);
		if ( sText == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pDecoration->sText != NULL ) {
		xrtFree(pDecoration->sText);
	}
	pDecoration->iKind = pDesc->iKind;
	pDecoration->iVisibleMode = pDesc->iVisibleMode;
	pDecoration->fWidth = pDesc->fWidth;
	pDecoration->fPadding = pDesc->fPadding;
	pDecoration->iIcon = pDesc->iIcon;
	pDecoration->sText = sText;
	pDecoration->pSurface = pDesc->pSurface;
	pDecoration->tSrc = pDesc->tSrc;
	pDecoration->iColor = pDesc->iColor;
	pDecoration->iHoverColor = pDesc->iHoverColor;
	pDecoration->iActiveColor = pDesc->iActiveColor;
	pDecoration->iDisabledColor = pDesc->iDisabledColor;
	pDecoration->onClick = pDesc->onClick;
	pDecoration->onPaint = pDesc->onPaint;
	pDecoration->pUser = pDesc->pUser;
	pDecoration->tRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return XUI_OK;
}

XUI_API int xuiInputDecorationAdd(xui_widget pWidget, int iSide, xui_input_decoration* ppDecoration, const xui_input_decoration_desc_t* pDesc)
{
	xui_input_data_t* pData;
	xui_input_decoration* ppList;
	xui_input_decoration pDecoration;
	int iRet;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationSideValid(iSide) || !__xuiInputDecorationDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDecoration = (xui_input_decoration)xrtMalloc(sizeof(*pDecoration));
	if ( pDecoration == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pDecoration, 0, sizeof(*pDecoration));
	pDecoration->iSide = iSide;
	iRet = __xuiInputDecorationCopyDesc(pDecoration, pDesc);
	if ( iRet != XUI_OK ) {
		__xuiInputDecorationFree(pDecoration);
		return iRet;
	}
	ppList = __xuiInputDecorationListRef(pData, iSide);
	while ( *ppList != NULL ) {
		ppList = &(*ppList)->pNext;
	}
	*ppList = pDecoration;
	if ( ppDecoration != NULL ) {
		*ppDecoration = pDecoration;
	}
	(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputDecorationSet(xui_widget pWidget, xui_input_decoration pDecoration, const xui_input_decoration_desc_t* pDesc)
{
	xui_input_data_t* pData;
	int iRet;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationBelongs(pData, pDecoration) || !__xuiInputDecorationDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputDecorationCopyDesc(pDecoration, pDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputDecorationRemove(xui_widget pWidget, xui_input_decoration pDecoration)
{
	xui_input_data_t* pData;
	xui_input_decoration* ppList;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || (pDecoration == NULL) || !__xuiInputDecorationSideValid(pDecoration->iSide) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	ppList = __xuiInputDecorationListRef(pData, pDecoration->iSide);
	while ( (ppList != NULL) && (*ppList != NULL) ) {
		if ( *ppList == pDecoration ) {
			*ppList = pDecoration->pNext;
			if ( pData->pHoverDecoration == pDecoration ) pData->pHoverDecoration = NULL;
			if ( pData->pActiveDecoration == pDecoration ) pData->pActiveDecoration = NULL;
			__xuiInputDecorationFree(pDecoration);
			(void)__xuiInputDecorationSyncPadding(pWidget, pData);
			return __xuiInputInvalidateText(pWidget);
		}
		ppList = &(*ppList)->pNext;
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiInputDecorationClear(xui_widget pWidget, int iSide)
{
	xui_input_data_t* pData;
	xui_input_decoration* ppList;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationSideValid(iSide) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	ppList = __xuiInputDecorationListRef(pData, iSide);
	if ( ppList == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiInputDecorationBelongsToList(*ppList, pData->pHoverDecoration) ) pData->pHoverDecoration = NULL;
	if ( __xuiInputDecorationBelongsToList(*ppList, pData->pActiveDecoration) ) pData->pActiveDecoration = NULL;
	__xuiInputDecorationFreeList(*ppList);
	*ppList = NULL;
	(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	return __xuiInputInvalidateText(pWidget);
}

XUI_API int xuiInputDecorationGetCount(xui_widget pWidget, int iSide)
{
	xui_input_data_t* pData;
	xui_input_decoration* ppList;
	xui_input_decoration pScan;
	int iCount;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationSideValid(iSide) ) {
		return 0;
	}
	ppList = __xuiInputDecorationListRef(pData, iSide);
	if ( ppList == NULL ) {
		return 0;
	}
	iCount = 0;
	for ( pScan = *ppList; pScan != NULL; pScan = pScan->pNext ) {
		++iCount;
	}
	return iCount;
}

XUI_API xui_input_decoration xuiInputDecorationGetAt(xui_widget pWidget, int iSide, int iIndex)
{
	xui_input_data_t* pData;
	xui_input_decoration* ppList;
	xui_input_decoration pScan;
	int i;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationSideValid(iSide) || (iIndex < 0) ) {
		return NULL;
	}
	ppList = __xuiInputDecorationListRef(pData, iSide);
	if ( ppList == NULL ) {
		return NULL;
	}
	i = 0;
	for ( pScan = *ppList; pScan != NULL; pScan = pScan->pNext ) {
		if ( i == iIndex ) {
			return pScan;
		}
		++i;
	}
	return NULL;
}

XUI_API int xuiInputDecorationGetDesc(xui_widget pWidget, xui_input_decoration pDecoration, xui_input_decoration_desc_t* pDesc)
{
	xui_input_data_t* pData;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || (pDesc == NULL) || !__xuiInputDecorationBelongs(pData, pDecoration) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iKind = pDecoration->iKind;
	pDesc->iVisibleMode = pDecoration->iVisibleMode;
	pDesc->fWidth = pDecoration->fWidth;
	pDesc->fPadding = pDecoration->fPadding;
	pDesc->iIcon = pDecoration->iIcon;
	pDesc->sText = pDecoration->sText;
	pDesc->pSurface = pDecoration->pSurface;
	pDesc->tSrc = pDecoration->tSrc;
	pDesc->iColor = pDecoration->iColor;
	pDesc->iHoverColor = pDecoration->iHoverColor;
	pDesc->iActiveColor = pDecoration->iActiveColor;
	pDesc->iDisabledColor = pDecoration->iDisabledColor;
	pDesc->onClick = pDecoration->onClick;
	pDesc->onPaint = pDecoration->onPaint;
	pDesc->pUser = pDecoration->pUser;
	return XUI_OK;
}

XUI_API xui_rect_t xuiInputDecorationGetRect(xui_widget pWidget, xui_input_decoration pDecoration)
{
	xui_input_data_t* pData;

	pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || !__xuiInputDecorationBelongs(pData, pDecoration) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	(void)__xuiInputDecorationSyncPadding(pWidget, pData);
	__xuiInputDecorationLayout(pWidget, pData, NULL, NULL);
	return pDecoration->tRect;
}

XUI_API int xuiInputSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	char* sNew;
	if ( (pData == NULL) || (iCommand < 0) || (iCommand >= XUI_INPUT_MENU_COUNT) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (sTitle == NULL) || (sTitle[0] == '\0') ) {
		if ( pData->arrMenuTitle[iCommand] != NULL ) {
			xrtFree(pData->arrMenuTitle[iCommand]);
			pData->arrMenuTitle[iCommand] = NULL;
		}
		return XUI_OK;
	}
	sNew = __xuiInputStringDuplicate(sTitle);
	if ( sNew == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pData->arrMenuTitle[iCommand] != NULL ) xrtFree(pData->arrMenuTitle[iCommand]);
	pData->arrMenuTitle[iCommand] = sNew;
	return XUI_OK;
}

XUI_API const char* xuiInputGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	if ( (pData == NULL) || (iCommand < 0) || (iCommand >= XUI_INPUT_MENU_COUNT) ) {
		return "";
	}
	return (pData->arrMenuTitle[iCommand] != NULL) ? pData->arrMenuTitle[iCommand] : xuiTranslate(xuiWidgetGetContext(pWidget), __xuiInputMenuTextId(iCommand));
}

XUI_API int xuiInputOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (pData->pMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiInputUpdateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiMenuSetSelect(pData->pMenu, __xuiInputMenuSelect, pWidget);
	return xuiMenuOpenAt(pData->pMenu, pWidget, fX, fY);
}

XUI_API xui_widget xuiInputGetMenuWidget(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->pMenu : NULL;
}

XUI_API xui_rect_t xuiInputGetTextRect(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiInputGetCursorRect(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->tCursorRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API uint32_t xuiInputGetState(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	uint32_t iState;
	if ( pData == NULL ) return 0u;
	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( pData->bError ) {
		iState |= XUI_INPUT_STATE_ERROR;
	}
	return iState;
}

XUI_API int xuiInputGetChangeCount(xui_widget pWidget)
{
	xui_input_data_t* pData = __xuiInputGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
