#include "xui_internal.h"

#include <string.h>

#define XUI_COMBOBOX_DEFAULT_ITEM_HEIGHT	24.0f
#define XUI_COMBOBOX_DEFAULT_POPUP_MAX_H	168.0f
#define XUI_COMBOBOX_POPUP_INSET		8.0f

typedef struct xui_combobox_data_t {
	xui_widget pMenu;
	xui_widget pInput;
	xui_font pFont;
	xui_combobox_item_t arrItems[XUI_COMBOBOX_ITEM_CAPACITY];
	xui_menu_item_t arrMenuItems[XUI_COMBOBOX_ITEM_CAPACITY];
	xui_combobox_select_proc onSelect;
	void* pSelectUser;
	xui_combobox_text_proc onTextChange;
	void* pTextChangeUser;
	int iItemCount;
	int iSelected;
	int iMode;
	int iMaxLength;
	int bSyncingInput;
	float fItemHeight;
	float fPopupHeight;
	float fPopupMaxHeight;
	int iPopupPlacement;
	int iChangeCount;
	xui_rect_t tTextRect;
	xui_rect_t tButtonRect;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iOpenBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iArrowColor;
	uint32_t iDisabledArrowColor;
	uint32_t iButtonColor;
	uint32_t iButtonHoverColor;
	uint32_t iButtonOpenColor;
	uint32_t iPopupPanelColor;
	uint32_t iPopupBorderColor;
	uint32_t iPopupShadowColor;
	uint32_t iPopupHoverColor;
	uint32_t iPopupTextColor;
	uint32_t iPopupHoverTextColor;
	uint32_t iPopupDisabledTextColor;
	uint32_t iPopupSeparatorColor;
	float fBorderWidth;
} xui_combobox_data_t;

static xui_combobox_data_t* __xuiComboBoxGetData(xui_widget pWidget);
static int __xuiComboBoxRefreshMenu(xui_widget pWidget, xui_combobox_data_t* pData);
static int __xuiComboBoxSyncInputStyle(xui_widget pWidget, xui_combobox_data_t* pData);
static int __xuiComboBoxModeValid(int iMode);

static int __xuiComboBoxDescValid(const xui_combobox_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iItemCount < 0) ||
	     (pDesc->iMaxLength < 0) ||
	     (pDesc->fItemHeight < 0.0f) ||
	     (pDesc->fPopupHeight < 0.0f) ||
	     (pDesc->fPopupMaxHeight < 0.0f) ||
	     (pDesc->fBorderWidth < 0.0f) ) {
		return 0;
	}
	if ( (pDesc->iMode != 0) && !__xuiComboBoxModeValid(pDesc->iMode) ) {
		return 0;
	}
	return 1;
}

static int __xuiComboBoxPlacementValid(int iPlacement)
{
	return (iPlacement == XUI_COMBOBOX_POPUP_AUTO) ||
	       (iPlacement == XUI_COMBOBOX_POPUP_BOTTOM) ||
	       (iPlacement == XUI_COMBOBOX_POPUP_TOP);
}

static int __xuiComboBoxModeValid(int iMode)
{
	return (iMode == XUI_COMBOBOX_MODE_SELECT) ||
	       (iMode == XUI_COMBOBOX_MODE_EDIT);
}

static int __xuiComboBoxAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiComboBoxColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static float __xuiComboBoxMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static const char* __xuiComboBoxText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static int __xuiComboBoxClampCount(int iCount)
{
	if ( iCount < 0 ) return 0;
	if ( iCount > XUI_COMBOBOX_ITEM_CAPACITY ) return XUI_COMBOBOX_ITEM_CAPACITY;
	return iCount;
}

static int __xuiComboBoxStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiComboBoxStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) &&
	     (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiComboBoxStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiComboBoxDefaults(xui_combobox_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iSelected = -1;
	pData->iMode = XUI_COMBOBOX_MODE_SELECT;
	pData->fItemHeight = XUI_COMBOBOX_DEFAULT_ITEM_HEIGHT;
	pData->fPopupHeight = 0.0f;
	pData->fPopupMaxHeight = XUI_COMBOBOX_DEFAULT_POPUP_MAX_H;
	pData->iPopupPlacement = XUI_COMBOBOX_POPUP_AUTO;
	pData->iTextColor = XUI_COLOR_RGBA(33, 55, 79, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 210);
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iHoverBackgroundColor = XUI_COLOR_RGBA(232, 244, 252, 255);
	pData->iOpenBackgroundColor = XUI_COLOR_RGBA(222, 239, 252, 255);
	pData->iDisabledBackgroundColor = XUI_COLOR_RGBA(235, 240, 245, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(132, 174, 214, 255);
	pData->iHoverBorderColor = XUI_COLOR_RGBA(78, 148, 208, 255);
	pData->iFocusBorderColor = XUI_COLOR_RGBA(42, 126, 205, 255);
	pData->iArrowColor = XUI_COLOR_RGBA(42, 92, 136, 255);
	pData->iDisabledArrowColor = XUI_COLOR_RGBA(134, 148, 164, 180);
	pData->iButtonColor = XUI_COLOR_RGBA(236, 246, 253, 255);
	pData->iButtonHoverColor = XUI_COLOR_RGBA(220, 238, 251, 255);
	pData->iButtonOpenColor = XUI_COLOR_RGBA(207, 229, 247, 255);
	pData->iPopupPanelColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pData->iPopupBorderColor = XUI_COLOR_RGBA(122, 164, 202, 255);
	pData->iPopupShadowColor = XUI_COLOR_RGBA(44, 70, 96, 46);
	pData->iPopupHoverColor = XUI_COLOR_RGBA(54, 125, 190, 255);
	pData->iPopupTextColor = XUI_COLOR_RGBA(28, 60, 94, 255);
	pData->iPopupHoverTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iPopupDisabledTextColor = XUI_COLOR_RGBA(142, 152, 166, 210);
	pData->iPopupSeparatorColor = XUI_COLOR_RGBA(202, 218, 232, 255);
	pData->fBorderWidth = 1.0f;
}

static void __xuiComboBoxApplyDesc(xui_combobox_data_t* pData, const xui_combobox_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pFont = pDesc->pFont;
	if ( __xuiComboBoxModeValid(pDesc->iMode) ) pData->iMode = pDesc->iMode;
	pData->iMaxLength = pDesc->iMaxLength;
	if ( pDesc->fItemHeight > 0.0f ) pData->fItemHeight = pDesc->fItemHeight;
	if ( pDesc->fPopupHeight > 0.0f ) pData->fPopupHeight = pDesc->fPopupHeight;
	if ( pDesc->fPopupMaxHeight > 0.0f ) pData->fPopupMaxHeight = pDesc->fPopupMaxHeight;
	if ( __xuiComboBoxPlacementValid(pDesc->iPopupPlacement) ) pData->iPopupPlacement = pDesc->iPopupPlacement;
	if ( __xuiComboBoxAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiComboBoxAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiComboBoxAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiComboBoxAlpha(pDesc->iHoverBackgroundColor) != 0 ) pData->iHoverBackgroundColor = pDesc->iHoverBackgroundColor;
	if ( __xuiComboBoxAlpha(pDesc->iOpenBackgroundColor) != 0 ) pData->iOpenBackgroundColor = pDesc->iOpenBackgroundColor;
	if ( __xuiComboBoxAlpha(pDesc->iDisabledBackgroundColor) != 0 ) pData->iDisabledBackgroundColor = pDesc->iDisabledBackgroundColor;
	if ( __xuiComboBoxAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiComboBoxAlpha(pDesc->iHoverBorderColor) != 0 ) pData->iHoverBorderColor = pDesc->iHoverBorderColor;
	if ( __xuiComboBoxAlpha(pDesc->iFocusBorderColor) != 0 ) pData->iFocusBorderColor = pDesc->iFocusBorderColor;
	if ( __xuiComboBoxAlpha(pDesc->iArrowColor) != 0 ) pData->iArrowColor = pDesc->iArrowColor;
	if ( __xuiComboBoxAlpha(pDesc->iDisabledArrowColor) != 0 ) pData->iDisabledArrowColor = pDesc->iDisabledArrowColor;
	if ( __xuiComboBoxAlpha(pDesc->iButtonColor) != 0 ) pData->iButtonColor = pDesc->iButtonColor;
	if ( __xuiComboBoxAlpha(pDesc->iButtonHoverColor) != 0 ) pData->iButtonHoverColor = pDesc->iButtonHoverColor;
	if ( __xuiComboBoxAlpha(pDesc->iButtonOpenColor) != 0 ) pData->iButtonOpenColor = pDesc->iButtonOpenColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupPanelColor) != 0 ) pData->iPopupPanelColor = pDesc->iPopupPanelColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupBorderColor) != 0 ) pData->iPopupBorderColor = pDesc->iPopupBorderColor;
	if ( pDesc->iPopupShadowColor != 0 ) pData->iPopupShadowColor = pDesc->iPopupShadowColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupHoverColor) != 0 ) pData->iPopupHoverColor = pDesc->iPopupHoverColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupTextColor) != 0 ) pData->iPopupTextColor = pDesc->iPopupTextColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupHoverTextColor) != 0 ) pData->iPopupHoverTextColor = pDesc->iPopupHoverTextColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupDisabledTextColor) != 0 ) pData->iPopupDisabledTextColor = pDesc->iPopupDisabledTextColor;
	if ( __xuiComboBoxAlpha(pDesc->iPopupSeparatorColor) != 0 ) pData->iPopupSeparatorColor = pDesc->iPopupSeparatorColor;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
}

static void __xuiComboBoxResolve(xui_widget pWidget, xui_combobox_data_t* pData, xui_combobox_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->pFont = __xuiComboBoxStyleFont(pWidget, pResolved->pFont);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.text.color", &pResolved->iTextColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.background.color", &pResolved->iBackgroundColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.background.open_color", &pResolved->iOpenBackgroundColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.background.disabled_color", &pResolved->iDisabledBackgroundColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.border.color", &pResolved->iBorderColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.arrow.color", &pResolved->iArrowColor);
	(void)__xuiComboBoxStyleColor(pWidget, "combobox.arrow.disabled_color", &pResolved->iDisabledArrowColor);
	(void)__xuiComboBoxStyleFloat(pWidget, "combobox.border.width", &pResolved->fBorderWidth);
}

static int __xuiComboBoxItemEnabled(const xui_combobox_data_t* pData, int iIndex)
{
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return 0;
	}
	return !pData->arrItems[iIndex].bSeparator && pData->arrItems[iIndex].bEnabled;
}

static int __xuiComboBoxFindEnabled(const xui_combobox_data_t* pData, int iStart, int iStep)
{
	int i;
	int iIndex;

	if ( (pData == NULL) || (pData->iItemCount <= 0) || (iStep == 0) ) {
		return -1;
	}
	iIndex = iStart;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iIndex += iStep;
		if ( iIndex < 0 ) {
			iIndex = pData->iItemCount - 1;
		} else if ( iIndex >= pData->iItemCount ) {
			iIndex = 0;
		}
		if ( __xuiComboBoxItemEnabled(pData, iIndex) ) {
			return iIndex;
		}
	}
	return -1;
}

static int __xuiComboBoxIndexForValue(const xui_combobox_data_t* pData, int iValue)
{
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiComboBoxItemEnabled(pData, i) && pData->arrItems[i].iValue == iValue ) {
			return i;
		}
	}
	return -1;
}

static int __xuiComboBoxIndexForText(const xui_combobox_data_t* pData, const char* sText)
{
	int i;

	if ( (pData == NULL) || (sText == NULL) ) {
		return -1;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiComboBoxItemEnabled(pData, i) &&
		     (strcmp(__xuiComboBoxText(pData->arrItems[i].sText), sText) == 0) ) {
			return i;
		}
	}
	return -1;
}

static const char* __xuiComboBoxSelectedText(const xui_combobox_data_t* pData)
{
	if ( (pData == NULL) ||
	     (pData->iSelected < 0) ||
	     (pData->iSelected >= pData->iItemCount) ) {
		return "";
	}
	return __xuiComboBoxText(pData->arrItems[pData->iSelected].sText);
}

static int __xuiComboBoxSyncInputTextFromSelection(xui_widget pWidget, xui_combobox_data_t* pData)
{
	int iRet;

	(void)pWidget;
	if ( (pData == NULL) || (pData->pInput == NULL) || (pData->iMode != XUI_COMBOBOX_MODE_EDIT) ) {
		return XUI_OK;
	}
	if ( pData->iSelected < 0 ) {
		return XUI_OK;
	}
	pData->bSyncingInput = 1;
	iRet = xuiInputSetText(pData->pInput, __xuiComboBoxSelectedText(pData));
	pData->bSyncingInput = 0;
	return iRet;
}

static int __xuiComboBoxClearSelectionIfTextMismatch(xui_widget pWidget, xui_combobox_data_t* pData, const char* sText)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pData->iSelected < 0) ) {
		return XUI_OK;
	}
	if ( strcmp(__xuiComboBoxSelectedText(pData), __xuiComboBoxText(sText)) == 0 ) {
		return XUI_OK;
	}
	pData->iSelected = -1;
	(void)__xuiComboBoxRefreshMenu(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiComboBoxSetSelectedInternal(xui_widget pWidget, xui_combobox_data_t* pData, int iIndex, int bNotify)
{
	xui_combobox_select_proc onSelect;
	void* pUser;
	int iOld;
	int iValue;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiComboBoxItemEnabled(pData, iIndex) ) {
		iIndex = -1;
	}
	if ( pData->iSelected == iIndex ) {
		return XUI_OK;
	}
	iOld = pData->iSelected;
	pData->iSelected = iIndex;
	pData->iChangeCount++;
	iRet = __xuiComboBoxRefreshMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( iIndex >= 0 ) {
		iRet = __xuiComboBoxSyncInputTextFromSelection(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify && (iOld != iIndex) && (iIndex >= 0) ) {
		onSelect = pData->onSelect;
		pUser = pData->pSelectUser;
		iValue = pData->arrItems[iIndex].iValue;
		if ( onSelect != NULL ) {
			onSelect(pWidget, iIndex, iValue, pUser);
		}
	}
	return XUI_OK;
}

static uint32_t __xuiComboBoxState(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_context pContext;
	xui_widget pFocus;
	xui_widget pPopup;
	uint32_t iState;

	if ( pWidget == NULL ) {
		return 0u;
	}
	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
		iState |= XUI_COMBOBOX_STATE_OPEN;
	}
	if ( pData != NULL ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFocus = xuiGetFocusWidget(pContext);
		pPopup = (pData->pMenu != NULL) ? xuiMenuGetPopupWidget(pData->pMenu) : NULL;
		if ( (pFocus == pWidget) || (pFocus == pData->pInput) || (pFocus == pData->pMenu) || (pFocus == pPopup) ) {
			iState |= XUI_WIDGET_STATE_FOCUS;
		}
	}
	return iState;
}

static int __xuiComboBoxSyncState(xui_widget pWidget, xui_combobox_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiComboBoxState(pWidget, pData));
}

static int __xuiComboBoxApplyMenuStyle(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_combobox_data_t tResolved;
	xui_menu_metrics_t tMetrics;
	xui_menu_colors_t tColors;
	xui_widget pPopup;
	xui_rect_t tRect;
	float fMinWidth;
	float fMaxHeight;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiComboBoxResolve(pWidget, pData, &tResolved);
	iRet = xuiMenuGetMetrics(pData->pMenu, &tMetrics);
	if ( iRet != XUI_OK ) return iRet;
	tRect = xuiWidgetGetWorldRect(pWidget);
	if ( tRect.fW <= 0.0f ) {
		tRect = xuiWidgetGetRect(pWidget);
	}
	fMinWidth = __xuiComboBoxMaxFloat(1.0f, tRect.fW - XUI_COMBOBOX_POPUP_INSET);
	fMaxHeight = (pData->fPopupHeight > 0.0f) ? pData->fPopupHeight : pData->fPopupMaxHeight;
	if ( fMaxHeight <= 0.0f ) {
		fMaxHeight = XUI_COMBOBOX_DEFAULT_POPUP_MAX_H;
	}
	tMetrics.fItemHeight = (pData->fItemHeight > 0.0f) ? pData->fItemHeight : XUI_COMBOBOX_DEFAULT_ITEM_HEIGHT;
	tMetrics.fSeparatorHeight = 9.0f;
	tMetrics.fPaddingX = 1.0f;
	tMetrics.fPaddingY = 1.0f;
	tMetrics.fMarkWidth = 0.0f;
	tMetrics.fIconWidth = 0.0f;
	tMetrics.fShortcutGap = 0.0f;
	tMetrics.fArrowWidth = 0.0f;
	tMetrics.fMinWidth = fMinWidth;
	tMetrics.fMaxHeight = fMaxHeight;
	iRet = xuiMenuSetMetrics(pData->pMenu, &tMetrics);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tColors, 0, sizeof(tColors));
	tColors.iSize = sizeof(tColors);
	tColors.iPanelColor = tResolved.iPopupPanelColor;
	tColors.iBorderColor = tResolved.iPopupBorderColor;
	tColors.iShadowColor = tResolved.iPopupShadowColor;
	tColors.iHoverColor = tResolved.iPopupHoverColor;
	tColors.iTextColor = tResolved.iPopupTextColor;
	tColors.iHoverTextColor = tResolved.iPopupHoverTextColor;
	tColors.iDisabledTextColor = tResolved.iPopupDisabledTextColor;
	tColors.iShortcutColor = __xuiComboBoxColorWithAlpha(tResolved.iPopupTextColor, 180);
	tColors.iDangerTextColor = tResolved.iPopupTextColor;
	tColors.iMarkColor = tResolved.iArrowColor;
	tColors.iSeparatorColor = tResolved.iPopupSeparatorColor;
	tColors.iFocusColor = tResolved.iFocusBorderColor;
	iRet = xuiMenuSetColors(pData->pMenu, &tColors);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiMenuSetFont(pData->pMenu, tResolved.pFont);
	if ( iRet != XUI_OK ) return iRet;
	pPopup = xuiMenuGetPopupWidget(pData->pMenu);
	if ( pPopup != NULL ) {
		(void)xuiPopupSetMatchOwnerWidth(pPopup, 1);
		(void)xuiPopupSetMetrics(pPopup, 3.0f, 1.0f, 4.0f);
	}
	return XUI_OK;
}

static int __xuiComboBoxApplyPlacement(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_widget pPopup;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPopup = xuiMenuGetPopupWidget(pData->pMenu);
	if ( pPopup == NULL ) {
		return XUI_OK;
	}
	(void)xuiPopupSetGap(pPopup, 2.0f);
	switch ( pData->iPopupPlacement ) {
	case XUI_COMBOBOX_POPUP_TOP:
		(void)xuiPopupSetAnchor(pPopup, XUI_POPUP_ANCHOR_TOP_LEFT);
		(void)xuiPopupSetDirection(pPopup, XUI_POPUP_DIRECTION_RIGHT_UP);
		break;
	case XUI_COMBOBOX_POPUP_BOTTOM:
	case XUI_COMBOBOX_POPUP_AUTO:
	default:
		(void)xuiPopupSetAnchor(pPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
		(void)xuiPopupSetDirection(pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
		break;
	}
	return XUI_OK;
}

static int __xuiComboBoxSyncInputStyle(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_combobox_data_t tResolved;
	uint32_t iText;
	int bEdit;
	int bEnabled;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) {
		return XUI_OK;
	}
	__xuiComboBoxResolve(pWidget, pData, &tResolved);
	bEdit = (pData->iMode == XUI_COMBOBOX_MODE_EDIT);
	bEnabled = xuiWidgetGetEnabled(pWidget);
	iText = bEnabled ? tResolved.iTextColor : tResolved.iDisabledTextColor;
	(void)xuiInputSetColors(pData->pInput, XUI_COLOR_RGBA(0, 0, 0, 0), iText, XUI_COLOR_RGBA(0, 0, 0, 0), XUI_COLOR_RGBA(0, 0, 0, 0));
	(void)xuiInputSetErrorColors(pData->pInput, XUI_COLOR_RGBA(0, 0, 0, 0), XUI_COLOR_RGBA(0, 0, 0, 0));
	(void)xuiInputSetReadonly(pData->pInput, !bEdit);
	(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_LEFT);
	if ( xuiInputGetFont(pData->pInput) != tResolved.pFont ) {
		(void)xuiInputSetFont(pData->pInput, tResolved.pFont);
	}
	if ( xuiInputGetMaxLength(pData->pInput) != pData->iMaxLength ) {
		(void)xuiInputSetMaxLength(pData->pInput, pData->iMaxLength);
	}
	(void)xuiWidgetSetEnabled(pData->pInput, bEnabled);
	(void)xuiWidgetSetVisible(pData->pInput, bEdit);
	(void)xuiWidgetSetHitTestVisible(pData->pInput, bEdit);
	(void)xuiWidgetSetFocusable(pData->pInput, bEdit && bEnabled);
	(void)xuiWidgetSetTabStop(pData->pInput, bEdit);
	(void)xuiWidgetSetFocusable(pWidget, !bEdit);
	(void)xuiWidgetSetTabStop(pWidget, !bEdit);
	return XUI_OK;
}

static void __xuiComboBoxInputChanged(xui_widget pInput, const char* sText, void* pUser)
{
	xui_widget pCombo;
	xui_combobox_data_t* pData;
	xui_combobox_text_proc onTextChange;
	void* pTextUser;

	(void)pInput;
	pCombo = (xui_widget)pUser;
	pData = __xuiComboBoxGetData(pCombo);
	if ( (pCombo == NULL) || (pData == NULL) || pData->bSyncingInput || (pData->iMode != XUI_COMBOBOX_MODE_EDIT) ) {
		return;
	}
	(void)__xuiComboBoxClearSelectionIfTextMismatch(pCombo, pData, sText);
	pData->iChangeCount++;
	(void)xuiWidgetInvalidate(pCombo, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	onTextChange = pData->onTextChange;
	pTextUser = pData->pTextChangeUser;
	if ( onTextChange != NULL ) {
		onTextChange(pCombo, __xuiComboBoxText(sText), pTextUser);
	}
}

static int __xuiComboBoxRefreshMenu(xui_widget pWidget, xui_combobox_data_t* pData)
{
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_OK;
	}
	memset(pData->arrMenuItems, 0, sizeof(pData->arrMenuItems));
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].bSeparator ) {
			pData->arrMenuItems[i].iType = XUI_MENU_ITEM_SEPARATOR;
			continue;
		}
		pData->arrMenuItems[i].sText = __xuiComboBoxText(pData->arrItems[i].sText);
		pData->arrMenuItems[i].iType = XUI_MENU_ITEM_NORMAL;
		pData->arrMenuItems[i].iValue = pData->arrItems[i].iValue;
		pData->arrMenuItems[i].iIcon = pData->arrItems[i].iIcon;
		pData->arrMenuItems[i].pUser = pData->arrItems[i].pUser;
		if ( pData->arrItems[i].bEnabled ) {
			pData->arrMenuItems[i].iState |= XUI_MENU_ITEM_ENABLED;
		}
	}
	return xuiMenuSetItems(pData->pMenu, pData->arrMenuItems, pData->iItemCount);
}

static int __xuiComboBoxSetItemsInternal(xui_widget pWidget, xui_combobox_data_t* pData, const char** arrItems, const xui_combobox_item_t* arrItemData, const int* arrEnabled, int iCount)
{
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (iCount < 0) ||
	     ((iCount > 0) && (arrItems == NULL) && (arrItemData == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = __xuiComboBoxClampCount(iCount);
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	for ( i = 0; i < iCount; i++ ) {
		if ( arrItemData != NULL ) {
			pData->arrItems[i] = arrItemData[i];
			if ( pData->arrItems[i].bSeparator ) {
				pData->arrItems[i].bEnabled = 0;
			}
		} else {
			pData->arrItems[i].sText = arrItems[i];
			pData->arrItems[i].iValue = i;
			pData->arrItems[i].bEnabled = 1;
		}
		if ( arrEnabled != NULL && !arrEnabled[i] ) {
			pData->arrItems[i].bEnabled = 0;
		}
	}
	pData->iItemCount = iCount;
	if ( !__xuiComboBoxItemEnabled(pData, pData->iSelected) ) {
		pData->iSelected = -1;
	}
	(void)__xuiComboBoxRefreshMenu(pWidget, pData);
	(void)__xuiComboBoxSyncInputTextFromSelection(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiComboBoxCommit(xui_widget pWidget, xui_combobox_data_t* pData, int iIndex, int bNotify)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiComboBoxItemEnabled(pData, iIndex) ) {
		return XUI_OK;
	}
	iRet = __xuiComboBoxSetSelectedInternal(pWidget, pData, iIndex, bNotify);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiComboBoxClose(pWidget);
	if ( (pData->iMode == XUI_COMBOBOX_MODE_EDIT) && (pData->pInput != NULL) ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pInput);
	}
	return XUI_OK;
}

static void __xuiComboBoxMenuSelected(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pCombo;
	xui_combobox_data_t* pData;

	(void)pMenu;
	(void)iValue;
	pCombo = (xui_widget)pUser;
	pData = __xuiComboBoxGetData(pCombo);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiComboBoxCommit(pCombo, pData, iIndex, 1);
}

static int __xuiComboBoxOpenWithHover(xui_widget pWidget, xui_combobox_data_t* pData, int iHover)
{
	xui_widget pPopup;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		return XUI_OK;
	}
	if ( __xuiComboBoxFindEnabled(pData, -1, 1) < 0 ) {
		return XUI_OK;
	}
	iRet = __xuiComboBoxSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiComboBoxApplyMenuStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pPopup = xuiMenuGetPopupWidget(pData->pMenu);
	if ( pPopup != NULL ) {
		(void)xuiPopupSetMatchOwnerWidth(pPopup, 1);
	}
	iRet = xuiMenuOpenForOwner(pData->pMenu, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiComboBoxApplyPlacement(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( !__xuiComboBoxItemEnabled(pData, iHover) ) {
		iHover = __xuiComboBoxItemEnabled(pData, pData->iSelected) ? pData->iSelected : __xuiComboBoxFindEnabled(pData, -1, 1);
	}
	(void)xuiMenuSetHoverIndex(pData->pMenu, iHover);
	(void)__xuiComboBoxSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiComboBoxRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < tRect.fX + tRect.fW) &&
	       (fY < tRect.fY + tRect.fH);
}

static void __xuiComboBoxUpdateRects(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_rect_t tRect;
	float fButtonW;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	tRect = xuiWidgetGetContentRect(pWidget);
	fButtonW = tRect.fH;
	if ( fButtonW < 24.0f ) fButtonW = 24.0f;
	if ( fButtonW > 36.0f ) fButtonW = 36.0f;
	if ( fButtonW > tRect.fW ) fButtonW = tRect.fW;
	pData->tButtonRect = xuiInternalSnapRect((xui_rect_t){tRect.fX + tRect.fW - fButtonW, tRect.fY, fButtonW, tRect.fH});
	pData->tTextRect = xuiInternalSnapRect((xui_rect_t){tRect.fX + 9.0f, tRect.fY, __xuiComboBoxMaxFloat(1.0f, tRect.fW - fButtonW - 14.0f), tRect.fH});
}

static int __xuiComboBoxPointerDown(xui_widget pWidget, xui_combobox_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int bButton;

	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	__xuiComboBoxUpdateRects(pWidget, pData);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	bButton = __xuiComboBoxRectContains(pData->tButtonRect, fLocalX, fLocalY);
	if ( pData->iMode == XUI_COMBOBOX_MODE_EDIT ) {
		if ( bButton && pData->pInput != NULL ) {
			(void)xuiSetFocusWidget(pContext, pData->pInput);
		}
		if ( !bButton ) {
			return XUI_OK;
		}
	} else {
		(void)xuiSetFocusWidget(pContext, pWidget);
	}
	if ( (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
		(void)xuiComboBoxClose(pWidget);
	} else {
		(void)__xuiComboBoxOpenWithHover(pWidget, pData, pData->iSelected);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiComboBoxKeyDown(xui_widget pWidget, xui_combobox_data_t* pData, const xui_event_t* pEvent)
{
	int iHover;

	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE || !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
			(void)xuiComboBoxClose(pWidget);
		} else {
			(void)__xuiComboBoxOpenWithHover(pWidget, pData, pData->iSelected);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
		iHover = __xuiComboBoxItemEnabled(pData, pData->iSelected) ? pData->iSelected : -1;
		iHover = __xuiComboBoxFindEnabled(pData, iHover, 1);
		(void)__xuiComboBoxOpenWithHover(pWidget, pData, iHover);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP:
		iHover = __xuiComboBoxItemEnabled(pData, pData->iSelected) ? pData->iSelected : pData->iItemCount;
		iHover = __xuiComboBoxFindEnabled(pData, iHover, -1);
		(void)__xuiComboBoxOpenWithHover(pWidget, pData, iHover);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ESCAPE:
		if ( (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
			(void)xuiComboBoxClose(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiComboBoxContextMenu(xui_widget pWidget, xui_combobox_data_t* pData, const xui_event_t* pEvent)
{
	xui_event_t tInputEvent;
	int iRet;

	if ( (pEvent->iPhase != XUI_EVENT_PHASE_TARGET) || (pEvent->pTarget != pWidget) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ||
	     (pData->iMode != XUI_COMBOBOX_MODE_EDIT) ||
	     (pData->pInput == NULL) ||
	     !xuiWidgetGetVisible(pData->pInput) ||
	     !xuiWidgetGetEnabled(pData->pInput) ) {
		return XUI_OK;
	}
	if ( (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
		iRet = xuiComboBoxClose(pWidget);
		if ( iRet != XUI_OK ) return iRet;
	}
	tInputEvent = *pEvent;
	tInputEvent.iSize = sizeof(tInputEvent);
	tInputEvent.pTarget = pData->pInput;
	tInputEvent.pCurrentTarget = NULL;
	tInputEvent.iPhase = 0;
	tInputEvent.iFlags &= ~XUI_EVENT_DISPATCH_STOP;
	iRet = xuiDispatchEvent(xuiWidgetGetContext(pWidget), &tInputEvent);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiComboBoxEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_combobox_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiComboBoxPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiComboBoxContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiComboBoxKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiComboBoxSyncInputStyle(pWidget, pData);
		(void)__xuiComboBoxSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( (pData->pMenu != NULL) && xuiMenuIsOpen(pData->pMenu) ) {
			(void)__xuiComboBoxApplyMenuStyle(pWidget, pData);
			(void)__xuiComboBoxApplyPlacement(pWidget, pData);
		}
		return XUI_OK;
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( ((pEvent->iType == XUI_EVENT_ENABLED_CHANGED) && !xuiWidgetGetEnabled(pWidget)) ||
		     ((pEvent->iType == XUI_EVENT_VISIBLE_CHANGED) && !xuiWidgetGetVisible(pWidget)) ) {
			(void)xuiComboBoxClose(pWidget);
		}
		(void)__xuiComboBoxSyncInputStyle(pWidget, pData);
		(void)__xuiComboBoxSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiComboBoxContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_combobox_data_t* pData;
	xui_combobox_data_t tResolved;
	xui_font_metrics_t tMetrics;
	xui_proxy pProxy;

	(void)tConstraint;
	pData = (xui_combobox_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiComboBoxResolve(pWidget, pData, &tResolved);
	pSize->fX = 148.0f;
	pSize->fY = 28.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (tResolved.pFont != NULL) &&
	     (pProxy->fontGetMetrics(pProxy, tResolved.pFont, &tMetrics) == XUI_OK) &&
	     (tMetrics.fLineHeight > 0.0f) ) {
		pSize->fY = __xuiComboBoxMaxFloat(28.0f, tMetrics.fLineHeight + 10.0f);
	}
	pSize->fX = xuiInternalSnapSize(pSize->fX);
	pSize->fY = xuiInternalSnapSize(pSize->fY);
	return XUI_OK;
}

static int __xuiComboBoxLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_combobox_data_t* pData;
	xui_rect_t tRect;
	xui_rect_t tInput;
	int iRet;

	(void)tContentRect;
	pData = (xui_combobox_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pInput == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiComboBoxSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	__xuiComboBoxUpdateRects(pWidget, pData);
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( pData->iMode == XUI_COMBOBOX_MODE_EDIT ) {
		tInput = (xui_rect_t){1.0f, tRect.fY, __xuiComboBoxMaxFloat(1.0f, pData->tButtonRect.fX - 1.0f), tRect.fH};
	} else {
		tInput = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return xuiWidgetArrange(pData->pInput, xuiInternalSnapRect(tInput));
}

static int __xuiComboBoxDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiComboBoxAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiComboBoxDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiComboBoxAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor) : XUI_OK;
}

static int __xuiComboBoxDrawChevron(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bOpen, uint32_t iColor)
{
	float fCx;
	float fCy;
	float fLeft;
	float fRight;
	float fTop;
	float fBottom;
	int iRet;

	if ( pProxy->drawLine == NULL ) {
		return XUI_OK;
	}
	fCx = tRect.fX + tRect.fW * 0.5f;
	fCy = tRect.fY + tRect.fH * 0.5f;
	fLeft = fCx - 4.0f;
	fRight = fCx + 4.0f;
	fTop = fCy - 2.0f;
	fBottom = fCy + 2.0f;
	if ( bOpen ) {
		iRet = pProxy->drawLine(pProxy, pDraw, fLeft, fBottom, fCx, fTop, 1.7f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, fCx, fTop, fRight, fBottom, 1.7f, iColor);
	}
	iRet = pProxy->drawLine(pProxy, pDraw, fLeft, fTop, fCx, fBottom, 1.7f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fCx, fBottom, fRight, fTop, 1.7f, iColor);
}

static int __xuiComboBoxCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_combobox_data_t* pData;
	xui_combobox_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tButton;
	const char* sText;
	uint32_t iState;
	uint32_t iBackground;
	uint32_t iBorder;
	uint32_t iText;
	uint32_t iArrow;
	uint32_t iButton;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiComboBoxResolve(pWidget, pData, &tResolved);
	__xuiComboBoxUpdateRects(pWidget, pData);
	iState = __xuiComboBoxState(pWidget, pData);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iBackground = tResolved.iBackgroundColor;
	iBorder = tResolved.iBorderColor;
	iText = tResolved.iTextColor;
	iArrow = tResolved.iArrowColor;
	iButton = tResolved.iButtonColor;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iBackground = tResolved.iDisabledBackgroundColor;
		iBorder = __xuiComboBoxColorWithAlpha(tResolved.iBorderColor, 120);
		iText = tResolved.iDisabledTextColor;
		iArrow = tResolved.iDisabledArrowColor;
	} else if ( (iState & XUI_COMBOBOX_STATE_OPEN) != 0 ) {
		iBackground = tResolved.iOpenBackgroundColor;
		iBorder = tResolved.iFocusBorderColor;
		iButton = tResolved.iButtonOpenColor;
	} else if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		iBackground = tResolved.iHoverBackgroundColor;
		iBorder = tResolved.iHoverBorderColor;
		iButton = tResolved.iButtonHoverColor;
	}
	iRet = __xuiComboBoxDrawRectFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	tButton = pData->tButtonRect;
	iRet = __xuiComboBoxDrawRectFill(pProxy, pDraw, tButton, iButton);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pProxy->drawLine != NULL) && (__xuiComboBoxAlpha(iBorder) != 0) ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tButton.fX, tButton.fY + 3.0f, tButton.fX, tButton.fY + tButton.fH - 3.0f, 1.0f, __xuiComboBoxColorWithAlpha(iBorder, 132));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->iMode == XUI_COMBOBOX_MODE_EDIT ) {
		sText = "";
	} else if ( (pData->iSelected >= 0) && (pData->iSelected < pData->iItemCount) ) {
		sText = __xuiComboBoxText(pData->arrItems[pData->iSelected].sText);
	} else {
		sText = "";
	}
	if ( (pProxy->drawText != NULL) && (__xuiComboBoxAlpha(iText) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, sText, pData->tTextRect, iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiComboBoxDrawChevron(pProxy, pDraw, tButton, (iState & XUI_COMBOBOX_STATE_OPEN) != 0, iArrow);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiComboBoxDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
}

static void __xuiComboBoxDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiComboBoxDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiComboBoxInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiComboBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiComboBoxEvent, NULL);
	return iRet;
}

static int __xuiComboBoxCreateInputChild(xui_widget pWidget, xui_combobox_data_t* pData, const xui_combobox_desc_t* pDesc)
{
	xui_input_desc_t tDesc;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = (pDesc != NULL && pDesc->sText != NULL) ? pDesc->sText : "";
	tDesc.sPlaceholder = (pDesc != NULL) ? pDesc->sPlaceholder : "";
	tDesc.pFont = pData->pFont;
	tDesc.iMaxLength = pData->iMaxLength;
	tDesc.iTextAlign = XUI_INPUT_ALIGN_LEFT;
	tDesc.iTextColor = pData->iTextColor;
	tDesc.iDisabledTextColor = pData->iDisabledTextColor;
	tDesc.iBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.iHoverBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.iDisabledBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.iBorderColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.iHoverBorderColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.iFocusBorderColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tDesc.fBorderWidth = 0.0f;
	iRet = xuiInputCreate(xuiWidgetGetContext(pWidget), &pData->pInput, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInputSetChange(pData->pInput, __xuiComboBoxInputChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pInput);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pInput);
		pData->pInput = NULL;
		return iRet;
	}
	return __xuiComboBoxSyncInputStyle(pWidget, pData);
}

static int __xuiComboBoxCreateMenu(xui_widget pWidget, xui_combobox_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pMenu, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiComboBoxMenuSelected, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiComboBoxApplyMenuStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiComboBoxRefreshMenu(pWidget, pData);
}

static int __xuiComboBoxInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_combobox_data_t* pData;
	const xui_combobox_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_combobox_data_t*)pTypeData;
	pDesc = (const xui_combobox_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiComboBoxDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiComboBoxDefaults(pData);
	__xuiComboBoxApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiComboBoxInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiComboBoxCreateInputChild(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiComboBoxCreateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pDesc != NULL ) {
		if ( pDesc->arrItemData != NULL ) {
			iRet = __xuiComboBoxSetItemsInternal(pWidget, pData, NULL, pDesc->arrItemData, pDesc->arrEnabled, pDesc->iItemCount);
		} else {
			iRet = __xuiComboBoxSetItemsInternal(pWidget, pData, pDesc->arrItems, NULL, pDesc->arrEnabled, pDesc->iItemCount);
		}
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->bUseValue ) {
			iRet = __xuiComboBoxSetSelectedInternal(pWidget, pData, __xuiComboBoxIndexForValue(pData, pDesc->iSelectedValue), 0);
		} else if ( pDesc->iSelected >= 0 ) {
			iRet = __xuiComboBoxSetSelectedInternal(pWidget, pData, pDesc->iSelected, 0);
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (pData->iMode == XUI_COMBOBOX_MODE_EDIT) && (pDesc != NULL) && (pDesc->sText != NULL) ) {
		(void)__xuiComboBoxClearSelectionIfTextMismatch(pWidget, pData, pDesc->sText);
	} else {
		(void)__xuiComboBoxSyncInputTextFromSelection(pWidget, pData);
	}
	(void)__xuiComboBoxSyncInputStyle(pWidget, pData);
	(void)__xuiComboBoxSyncState(pWidget, pData);
	return XUI_OK;
}

static void __xuiComboBoxDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_combobox_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_combobox_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiComboBoxRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiComboBoxRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.background.open_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.background.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.border.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.arrow.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.arrow.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "combobox.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiComboBoxRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_combobox_data_t* __xuiComboBoxGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "combobox");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_combobox_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiComboBoxGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "combobox");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "combobox";
	tDesc.iTypeDataSize = sizeof(xui_combobox_data_t);
	tDesc.onInit = __xuiComboBoxInit;
	tDesc.onDestroy = __xuiComboBoxDestroy;
	tDesc.onContentMeasure = __xuiComboBoxContentMeasure;
	tDesc.onLayoutArrange = __xuiComboBoxLayoutArrange;
	tDesc.onCacheRender = __xuiComboBoxCacheRender;
	__xuiComboBoxDefaultLayout(&tLayout);
	__xuiComboBoxDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiComboBoxRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiComboBoxCreate(xui_context pContext, xui_widget* ppWidget, const xui_combobox_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiComboBoxDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiComboBoxGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiComboBoxSetSelect(xui_widget pWidget, xui_combobox_select_proc onSelect, void* pUser)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetTextChange(xui_widget pWidget, xui_combobox_text_proc onChange, void* pUser)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onTextChange = onChange;
	pData->pTextChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetItems(xui_widget pWidget, const char** arrItems, int iCount)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiComboBoxSetItemsInternal(pWidget, pData, arrItems, NULL, NULL, iCount);
}

XUI_API int xuiComboBoxSetItemData(xui_widget pWidget, const xui_combobox_item_t* pItems, int iCount)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiComboBoxSetItemsInternal(pWidget, pData, NULL, pItems, NULL, iCount);
}

XUI_API int xuiComboBoxSetEnabledItems(xui_widget pWidget, const int* arrEnabled, int iCount)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	int i;

	if ( (pData == NULL) || (iCount < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCount > pData->iItemCount ) iCount = pData->iItemCount;
	for ( i = 0; i < iCount; i++ ) {
		if ( !pData->arrItems[i].bSeparator ) {
			pData->arrItems[i].bEnabled = (arrEnabled == NULL) ? 1 : (arrEnabled[i] ? 1 : 0);
		}
	}
	if ( !__xuiComboBoxItemEnabled(pData, pData->iSelected) ) {
		pData->iSelected = -1;
	}
	(void)__xuiComboBoxRefreshMenu(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetItemCount(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_combobox_item_t* xuiComboBoxGetItem(xui_widget pWidget, int iIndex)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return &pData->arrItems[iIndex];
}

XUI_API const char* xuiComboBoxGetItemText(xui_widget pWidget, int iIndex)
{
	const xui_combobox_item_t* pItem = xuiComboBoxGetItem(pWidget, iIndex);
	return (pItem != NULL) ? __xuiComboBoxText(pItem->sText) : "";
}

XUI_API int xuiComboBoxGetItemValue(xui_widget pWidget, int iIndex)
{
	const xui_combobox_item_t* pItem = xuiComboBoxGetItem(pWidget, iIndex);
	return (pItem != NULL) ? pItem->iValue : 0;
}

XUI_API int xuiComboBoxIsItemEnabled(xui_widget pWidget, int iIndex)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return __xuiComboBoxItemEnabled(pData, iIndex);
}

XUI_API int xuiComboBoxSetSelected(xui_widget pWidget, int iIndex)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiComboBoxSetSelectedInternal(pWidget, pData, iIndex, 0);
}

XUI_API int xuiComboBoxGetSelected(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iSelected : -1;
}

XUI_API int xuiComboBoxSetSelectedValue(xui_widget pWidget, int iValue)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiComboBoxSetSelectedInternal(pWidget, pData, __xuiComboBoxIndexForValue(pData, iValue), 0);
}

XUI_API int xuiComboBoxGetSelectedValue(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (pData->iSelected < 0) || (pData->iSelected >= pData->iItemCount) ) {
		return 0;
	}
	return pData->arrItems[pData->iSelected].iValue;
}

XUI_API int xuiComboBoxSetMode(xui_widget pWidget, int iMode)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || !__xuiComboBoxModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iMode == iMode ) return XUI_OK;
	pData->iMode = iMode;
	if ( iMode == XUI_COMBOBOX_MODE_EDIT ) {
		iRet = __xuiComboBoxSyncInputTextFromSelection(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pData->pInput ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	}
	iRet = __xuiComboBoxSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)__xuiComboBoxSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetMode(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iMode : XUI_COMBOBOX_MODE_SELECT;
}

XUI_API int xuiComboBoxSetText(xui_widget pWidget, const char* sText)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	const char* sOld;
	int iIndex;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sText = __xuiComboBoxText(sText);
	if ( pData->iMode != XUI_COMBOBOX_MODE_EDIT ) {
		if ( sText[0] == '\0' ) {
			return __xuiComboBoxSetSelectedInternal(pWidget, pData, -1, 0);
		}
		iIndex = __xuiComboBoxIndexForText(pData, sText);
		if ( iIndex < 0 ) {
			return XUI_ERROR_UNSUPPORTED;
		}
		return __xuiComboBoxSetSelectedInternal(pWidget, pData, iIndex, 0);
	}
	if ( pData->pInput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sOld = xuiInputGetText(pData->pInput);
	if ( strcmp(__xuiComboBoxText(sOld), sText) == 0 ) {
		return XUI_OK;
	}
	pData->bSyncingInput = 1;
	iRet = xuiInputSetText(pData->pInput, sText);
	pData->bSyncingInput = 0;
	if ( iRet != XUI_OK ) return iRet;
	(void)__xuiComboBoxClearSelectionIfTextMismatch(pWidget, pData, sText);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiComboBoxGetText(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return "";
	if ( (pData->iMode == XUI_COMBOBOX_MODE_EDIT) && (pData->pInput != NULL) ) {
		return xuiInputGetText(pData->pInput);
	}
	return __xuiComboBoxSelectedText(pData);
}

XUI_API int xuiComboBoxSetPlaceholder(xui_widget pWidget, const char* sText)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInputSetPlaceholder(pData->pInput, sText);
}

XUI_API const char* xuiComboBoxGetPlaceholder(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetPlaceholder(pData->pInput) : "";
}

XUI_API int xuiComboBoxSetMaxLength(xui_widget pWidget, int iMaxLength)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (iMaxLength < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iMaxLength = iMaxLength;
	if ( pData->pInput != NULL ) {
		iRet = xuiInputSetMaxLength(pData->pInput, iMaxLength);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

XUI_API int xuiComboBoxGetMaxLength(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iMaxLength : 0;
}

XUI_API int xuiComboBoxSetInputMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInputSetMenuTitle(pData->pInput, iCommand, sTitle);
}

XUI_API const char* xuiComboBoxGetInputMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetMenuTitle(pData->pInput, iCommand) : "";
}

XUI_API int xuiComboBoxOpenInputMenu(xui_widget pWidget, float fX, float fY)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iMode != XUI_COMBOBOX_MODE_EDIT ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiComboBoxSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiInputOpenMenu(pData->pInput, fX, fY);
}

XUI_API xui_widget xuiComboBoxGetInputMenuWidget(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetMenuWidget(pData->pInput) : NULL;
}

XUI_API int xuiComboBoxOpen(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiComboBoxOpenWithHover(pWidget, pData, pData->iSelected);
}

XUI_API int xuiComboBoxClose(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pMenu != NULL ) {
		iRet = xuiMenuClose(pData->pMenu);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)__xuiComboBoxSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxToggle(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return (pData->pMenu != NULL && xuiMenuIsOpen(pData->pMenu)) ? xuiComboBoxClose(pWidget) : xuiComboBoxOpen(pWidget);
}

XUI_API int xuiComboBoxIsOpen(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL && pData->pMenu != NULL) ? xuiMenuIsOpen(pData->pMenu) : 0;
}

XUI_API int xuiComboBoxSetPopupHeight(xui_widget pWidget, float fHeight)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (fHeight < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPopupHeight = fHeight;
	if ( pData->pMenu != NULL ) (void)__xuiComboBoxApplyMenuStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API float xuiComboBoxGetPopupHeight(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->fPopupHeight : 0.0f;
}

XUI_API int xuiComboBoxSetPopupMaxHeight(xui_widget pWidget, float fMaxHeight)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (fMaxHeight < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPopupMaxHeight = fMaxHeight;
	if ( pData->pMenu != NULL ) (void)__xuiComboBoxApplyMenuStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API float xuiComboBoxGetPopupMaxHeight(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->fPopupMaxHeight : 0.0f;
}

XUI_API int xuiComboBoxSetPopupPlacement(xui_widget pWidget, int iPlacement)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || !__xuiComboBoxPlacementValid(iPlacement) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPlacement = iPlacement;
	if ( pData->pMenu != NULL && xuiMenuIsOpen(pData->pMenu) ) {
		return __xuiComboBoxApplyPlacement(pWidget, pData);
	}
	return XUI_OK;
}

XUI_API int xuiComboBoxGetPopupPlacement(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iPopupPlacement : XUI_COMBOBOX_POPUP_AUTO;
}

XUI_API int xuiComboBoxSetMetrics(xui_widget pWidget, float fItemHeight, float fBorderWidth)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( (pData == NULL) || (fItemHeight < 0.0f) || (fBorderWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fItemHeight > 0.0f ) pData->fItemHeight = fItemHeight;
	pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetMetrics(xui_widget pWidget, float* pItemHeight, float* pBorderWidth)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetColors(xui_widget pWidget, uint32_t iText, uint32_t iDisabledText, uint32_t iBackground, uint32_t iHoverBackground, uint32_t iOpenBackground, uint32_t iDisabledBackground)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iText;
	pData->iDisabledTextColor = iDisabledText;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iOpenBackgroundColor = iOpenBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetColors(xui_widget pWidget, uint32_t* pText, uint32_t* pDisabledText, uint32_t* pBackground, uint32_t* pHoverBackground, uint32_t* pOpenBackground, uint32_t* pDisabledBackground)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pOpenBackground != NULL ) *pOpenBackground = pData->iOpenBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetBorderColors(xui_widget pWidget, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocusBorder)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iFocusBorderColor = iFocusBorder;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetBorderColors(xui_widget pWidget, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocusBorder)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pFocusBorder != NULL ) *pFocusBorder = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetArrowColors(xui_widget pWidget, uint32_t iArrow, uint32_t iDisabledArrow)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iArrowColor = iArrow;
	pData->iDisabledArrowColor = iDisabledArrow;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetArrowColors(xui_widget pWidget, uint32_t* pArrow, uint32_t* pDisabledArrow)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pArrow != NULL ) *pArrow = pData->iArrowColor;
	if ( pDisabledArrow != NULL ) *pDisabledArrow = pData->iDisabledArrowColor;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iHover, uint32_t iOpen)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iButtonColor = iButton;
	pData->iButtonHoverColor = iHover;
	pData->iButtonOpenColor = iOpen;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiComboBoxGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pHover, uint32_t* pOpen)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pButton != NULL ) *pButton = pData->iButtonColor;
	if ( pHover != NULL ) *pHover = pData->iButtonHoverColor;
	if ( pOpen != NULL ) *pOpen = pData->iButtonOpenColor;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetPopupColors(xui_widget pWidget, uint32_t iPanel, uint32_t iBorder, uint32_t iShadow, uint32_t iHover, uint32_t iText, uint32_t iHoverText, uint32_t iDisabledText, uint32_t iSeparator)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPanelColor = iPanel;
	pData->iPopupBorderColor = iBorder;
	pData->iPopupShadowColor = iShadow;
	pData->iPopupHoverColor = iHover;
	pData->iPopupTextColor = iText;
	pData->iPopupHoverTextColor = iHoverText;
	pData->iPopupDisabledTextColor = iDisabledText;
	pData->iPopupSeparatorColor = iSeparator;
	if ( pData->pMenu != NULL ) return __xuiComboBoxApplyMenuStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiComboBoxGetPopupColors(xui_widget pWidget, uint32_t* pPanel, uint32_t* pBorder, uint32_t* pShadow, uint32_t* pHover, uint32_t* pText, uint32_t* pHoverText, uint32_t* pDisabledText, uint32_t* pSeparator)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPanel != NULL ) *pPanel = pData->iPopupPanelColor;
	if ( pBorder != NULL ) *pBorder = pData->iPopupBorderColor;
	if ( pShadow != NULL ) *pShadow = pData->iPopupShadowColor;
	if ( pHover != NULL ) *pHover = pData->iPopupHoverColor;
	if ( pText != NULL ) *pText = pData->iPopupTextColor;
	if ( pHoverText != NULL ) *pHoverText = pData->iPopupHoverTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iPopupDisabledTextColor;
	if ( pSeparator != NULL ) *pSeparator = pData->iPopupSeparatorColor;
	return XUI_OK;
}

XUI_API int xuiComboBoxSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	if ( pData->pMenu != NULL ) (void)__xuiComboBoxApplyMenuStyle(pWidget, pData);
	(void)__xuiComboBoxSyncInputStyle(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiComboBoxGetFont(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API xui_widget xuiComboBoxGetMenuWidget(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->pMenu : NULL;
}

XUI_API xui_widget xuiComboBoxGetPopupWidget(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL && pData->pMenu != NULL) ? xuiMenuGetPopupWidget(pData->pMenu) : NULL;
}

XUI_API xui_widget xuiComboBoxGetInputWidget(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->pInput : NULL;
}

XUI_API xui_rect_t xuiComboBoxGetButtonRect(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiComboBoxUpdateRects(pWidget, pData);
	return pData->tButtonRect;
}

XUI_API xui_rect_t xuiComboBoxGetTextRect(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiComboBoxUpdateRects(pWidget, pData);
	return pData->tTextRect;
}

XUI_API uint32_t xuiComboBoxGetState(xui_widget pWidget)
{
	return __xuiComboBoxState(pWidget, __xuiComboBoxGetData(pWidget));
}

XUI_API int xuiComboBoxGetChangeCount(xui_widget pWidget)
{
	xui_combobox_data_t* pData = __xuiComboBoxGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
