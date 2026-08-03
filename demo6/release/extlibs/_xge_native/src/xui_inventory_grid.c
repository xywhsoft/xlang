#include "xui_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define XUI_INVENTORY_DEFAULT_SLOT_SIZE 48.0f
#define XUI_INVENTORY_DEFAULT_COLUMNS 8
#define XUI_INVENTORY_DEFAULT_VISIBLE_ROWS 4
#define XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS 28
#define XUI_INVENTORY_TOOLTIP_TEXT_CAPACITY 256
#define XUI_INVENTORY_TOOLTIP_META_CAPACITY 128
#define XUI_INVENTORY_PI 3.14159265358979323846f

#define XUI_INVENTORY_STATE_HOVER 0x00010000u
#define XUI_INVENTORY_STATE_ACTIVE 0x00020000u
#define XUI_INVENTORY_STATE_SELECTED 0x00040000u
#define XUI_INVENTORY_STATE_CURRENT 0x00080000u
#define XUI_INVENTORY_STATE_DRAG_SOURCE 0x00100000u
#define XUI_INVENTORY_STATE_DROP_TARGET 0x00200000u

typedef struct xui_inventory_grid_data_t {
	xui_inventory_slot_t* arrSlots;
	xui_font pFont;
	xui_scroll_model_t tScroll;
	xui_inventory_grid_layout_t tLayout;
	xui_inventory_grid_colors_t tColors;
	xui_inventory_gamepad_profile_t tGamepadProfile;
	xui_inventory_select_proc onSelect;
	void* pSelectUser;
	xui_inventory_activate_proc onActivate;
	void* pActivateUser;
	xui_inventory_context_proc onContext;
	void* pContextUser;
	xui_inventory_drag_proc onDrag;
	void* pDragUser;
	xui_inventory_drop_proc onDrop;
	void* pDropUser;
	xui_inventory_split_proc onSplit;
	void* pSplitUser;
	xui_inventory_tooltip_proc onTooltip;
	void* pTooltipUser;
	xui_inventory_slot_render_proc onRenderSlot;
	void* pRenderSlotUser;
	xui_inventory_animation_render_proc onRenderAnimation;
	void* pRenderAnimationUser;
	xui_widget pSplitPopup;
	xui_widget pSplitTitle;
	xui_widget pSplitInput;
	xui_widget pSplitOk;
	xui_widget pSplitCancel;
	int iSlotCount;
	int iCurrent;
	int iHoverSlot;
	int iActiveSlot;
	int iAnchorSlot;
	int iDragSource;
	int iDropTarget;
	int iDropMode;
	int iChangeCount;
	int iSelectCount;
	int iActivateCount;
	int iContextCount;
	int iDragCount;
	int iDropCount;
	int iSplitSlot;
	int iSplitMin;
	int iSplitMax;
	int iSplitCount;
	int iSplitCommitCount;
	int bTooltipVisible;
	int iTooltipSlot;
	int iResolvedColumns;
	int iResolvedRows;
	xui_inventory_visible_range_t tLastPaintRange;
	int iLastPaintSlotCount;
	float fContentWidth;
	float fContentHeight;
	float fPressX;
	float fPressY;
	float fDragX;
	float fDragY;
	char sTooltipTitle[XUI_INVENTORY_GRID_TEXT_CAPACITY];
	char sTooltipText[XUI_INVENTORY_TOOLTIP_TEXT_CAPACITY];
	char sTooltipMeta[XUI_INVENTORY_TOOLTIP_META_CAPACITY];
	int bPointerDown;
	int bDragging;
} xui_inventory_grid_data_t;

typedef struct xui_inventory_sort_context_t {
	xui_widget pWidget;
	xui_inventory_grid_data_t* pData;
	int iSortMode;
	uint32_t iFlags;
	xui_inventory_compare_proc onCompare;
	void* pCompareUser;
} xui_inventory_sort_context_t;

static int __xuiInventoryAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiInventoryMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiInventoryMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiInventoryClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiInventoryClampInt(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiInventoryMinInt(int iA, int iB)
{
	return (iA < iB) ? iA : iB;
}

static int __xuiInventoryFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiInventoryRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static void __xuiInventorySafeCopy(char* sDst, int iCapacity, const char* sSrc)
{
	if ( (sDst == NULL) || (iCapacity <= 0) ) {
		return;
	}
	if ( sSrc == NULL ) {
		sSrc = "";
	}
	strncpy(sDst, sSrc, (size_t)iCapacity - 1u);
	sDst[iCapacity - 1] = '\0';
}

static xui_inventory_grid_data_t* __xuiInventoryGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "inventorygrid");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_inventory_grid_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiInventoryLayoutValid(const xui_inventory_grid_layout_t* pLayout)
{
	if ( pLayout == NULL ) {
		return 1;
	}
	if ( (pLayout->iSize != 0) && (pLayout->iSize < sizeof(*pLayout)) ) {
		return 0;
	}
	if ( (pLayout->iColumns < 0) ||
	     ((pLayout->fSlotSize != 0.0f) && !__xuiInventoryFloatValid(pLayout->fSlotSize)) ||
	     ((pLayout->fSlotGap != 0.0f) && !__xuiInventoryFloatValid(pLayout->fSlotGap)) ||
	     ((pLayout->fPadding != 0.0f) && !__xuiInventoryFloatValid(pLayout->fPadding)) ||
	     ((pLayout->fIconPadding != 0.0f) && !__xuiInventoryFloatValid(pLayout->fIconPadding)) ||
	     ((pLayout->fBorderWidth != 0.0f) && !__xuiInventoryFloatValid(pLayout->fBorderWidth)) ||
	     ((pLayout->fWheelStep != 0.0f) && !__xuiInventoryFloatValid(pLayout->fWheelStep)) ||
	     ((pLayout->fDragThreshold != 0.0f) && !__xuiInventoryFloatValid(pLayout->fDragThreshold)) ) {
		return 0;
	}
	return 1;
}

static int __xuiInventoryDescValid(const xui_inventory_grid_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( pDesc->iSlotCount < 0 ) {
		return 0;
	}
	if ( pDesc->bHasLayout && !__xuiInventoryLayoutValid(&pDesc->tLayout) ) {
		return 0;
	}
	return 1;
}

static void __xuiInventoryDefaultLayoutValues(xui_inventory_grid_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iSize = sizeof(*pLayout);
	pLayout->iColumns = XUI_INVENTORY_DEFAULT_COLUMNS;
	pLayout->iSelectionMode = XUI_INVENTORY_SELECTION_SINGLE;
	pLayout->fSlotSize = XUI_INVENTORY_DEFAULT_SLOT_SIZE;
	pLayout->fSlotGap = 8.0f;
	pLayout->fPadding = 10.0f;
	pLayout->fIconPadding = 8.0f;
	pLayout->fBorderWidth = 1.0f;
	pLayout->fWheelStep = XUI_INVENTORY_DEFAULT_SLOT_SIZE + 8.0f;
	pLayout->fDragThreshold = 5.0f;
}

static void __xuiInventoryDefaultColors(xui_inventory_grid_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(243, 248, 252, 255);
	pColors->iSlotColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pColors->iEmptyColor = XUI_COLOR_RGBA(233, 241, 249, 255);
	pColors->iHoverColor = XUI_COLOR_RGBA(229, 243, 255, 255);
	pColors->iActiveColor = XUI_COLOR_RGBA(216, 235, 253, 255);
	pColors->iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pColors->iDisabledColor = XUI_COLOR_RGBA(220, 229, 238, 185);
	pColors->iLockedColor = XUI_COLOR_RGBA(25, 38, 52, 88);
	pColors->iBorderColor = XUI_COLOR_RGBA(126, 168, 208, 255);
	pColors->iQualityColor = XUI_COLOR_RGBA(126, 168, 208, 255);
	pColors->iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 215);
	pColors->iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
	pColors->iMutedTextColor = XUI_COLOR_RGBA(96, 117, 139, 230);
	pColors->iCountColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iHotkeyColor = XUI_COLOR_RGBA(64, 89, 118, 230);
	pColors->iCooldownColor = XUI_COLOR_RGBA(21, 30, 42, 145);
	pColors->iDurabilityColor = XUI_COLOR_RGBA(78, 186, 124, 255);
	pColors->iDragColor = XUI_COLOR_RGBA(47, 128, 237, 110);
	pColors->iDropColor = XUI_COLOR_RGBA(42, 178, 120, 220);
}

static void __xuiInventoryDefaultGamepadProfile(xui_inventory_gamepad_profile_t* pProfile)
{
	if ( pProfile == NULL ) {
		return;
	}
	memset(pProfile, 0, sizeof(*pProfile));
	pProfile->iSize = sizeof(*pProfile);
	pProfile->iFlags = XUI_INVENTORY_GAMEPAD_SKIP_DISABLED | XUI_INVENTORY_GAMEPAD_SELECT_ON_MOVE;
	pProfile->iAcceptButton = XUI_INVENTORY_GAMEPAD_BUTTON_ACCEPT;
	pProfile->iCancelButton = XUI_INVENTORY_GAMEPAD_BUTTON_CANCEL;
	pProfile->iContextButton = XUI_INVENTORY_GAMEPAD_BUTTON_CONTEXT;
	pProfile->iLeftButton = XUI_INVENTORY_GAMEPAD_BUTTON_LEFT;
	pProfile->iRightButton = XUI_INVENTORY_GAMEPAD_BUTTON_RIGHT;
	pProfile->iUpButton = XUI_INVENTORY_GAMEPAD_BUTTON_UP;
	pProfile->iDownButton = XUI_INVENTORY_GAMEPAD_BUTTON_DOWN;
	pProfile->iPageUpButton = XUI_INVENTORY_GAMEPAD_BUTTON_PAGE_UP;
	pProfile->iPageDownButton = XUI_INVENTORY_GAMEPAD_BUTTON_PAGE_DOWN;
	pProfile->iHomeButton = XUI_INVENTORY_GAMEPAD_BUTTON_HOME;
	pProfile->iEndButton = XUI_INVENTORY_GAMEPAD_BUTTON_END;
}

static void __xuiInventoryApplyLayout(xui_inventory_grid_layout_t* pDst, const xui_inventory_grid_layout_t* pSrc)
{
	if ( (pDst == NULL) || (pSrc == NULL) ) {
		return;
	}
	if ( pSrc->iColumns >= 0 ) pDst->iColumns = pSrc->iColumns;
	if ( pSrc->iSelectionMode == XUI_INVENTORY_SELECTION_NONE ||
	     pSrc->iSelectionMode == XUI_INVENTORY_SELECTION_SINGLE ||
	     pSrc->iSelectionMode == XUI_INVENTORY_SELECTION_MULTI ) {
		pDst->iSelectionMode = pSrc->iSelectionMode;
	}
	if ( pSrc->fSlotSize > 0.0f ) pDst->fSlotSize = pSrc->fSlotSize;
	if ( pSrc->fSlotGap > 0.0f ) pDst->fSlotGap = pSrc->fSlotGap;
	if ( pSrc->fPadding > 0.0f ) pDst->fPadding = pSrc->fPadding;
	if ( pSrc->fIconPadding > 0.0f ) pDst->fIconPadding = pSrc->fIconPadding;
	if ( pSrc->fBorderWidth > 0.0f ) pDst->fBorderWidth = pSrc->fBorderWidth;
	if ( pSrc->fWheelStep > 0.0f ) pDst->fWheelStep = pSrc->fWheelStep;
	if ( pSrc->fDragThreshold > 0.0f ) pDst->fDragThreshold = pSrc->fDragThreshold;
}

static void __xuiInventoryApplyColors(xui_inventory_grid_colors_t* pDst, const xui_inventory_grid_colors_t* pSrc)
{
	if ( (pDst == NULL) || (pSrc == NULL) ) {
		return;
	}
	if ( pSrc->iBackgroundColor != 0u ) pDst->iBackgroundColor = pSrc->iBackgroundColor;
	if ( pSrc->iSlotColor != 0u ) pDst->iSlotColor = pSrc->iSlotColor;
	if ( pSrc->iEmptyColor != 0u ) pDst->iEmptyColor = pSrc->iEmptyColor;
	if ( pSrc->iHoverColor != 0u ) pDst->iHoverColor = pSrc->iHoverColor;
	if ( pSrc->iActiveColor != 0u ) pDst->iActiveColor = pSrc->iActiveColor;
	if ( pSrc->iSelectedColor != 0u ) pDst->iSelectedColor = pSrc->iSelectedColor;
	if ( pSrc->iDisabledColor != 0u ) pDst->iDisabledColor = pSrc->iDisabledColor;
	if ( pSrc->iLockedColor != 0u ) pDst->iLockedColor = pSrc->iLockedColor;
	if ( pSrc->iBorderColor != 0u ) pDst->iBorderColor = pSrc->iBorderColor;
	if ( pSrc->iQualityColor != 0u ) pDst->iQualityColor = pSrc->iQualityColor;
	if ( pSrc->iFocusColor != 0u ) pDst->iFocusColor = pSrc->iFocusColor;
	if ( pSrc->iTextColor != 0u ) pDst->iTextColor = pSrc->iTextColor;
	if ( pSrc->iMutedTextColor != 0u ) pDst->iMutedTextColor = pSrc->iMutedTextColor;
	if ( pSrc->iCountColor != 0u ) pDst->iCountColor = pSrc->iCountColor;
	if ( pSrc->iHotkeyColor != 0u ) pDst->iHotkeyColor = pSrc->iHotkeyColor;
	if ( pSrc->iCooldownColor != 0u ) pDst->iCooldownColor = pSrc->iCooldownColor;
	if ( pSrc->iDurabilityColor != 0u ) pDst->iDurabilityColor = pSrc->iDurabilityColor;
	if ( pSrc->iDragColor != 0u ) pDst->iDragColor = pSrc->iDragColor;
	if ( pSrc->iDropColor != 0u ) pDst->iDropColor = pSrc->iDropColor;
}

static int __xuiInventoryStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiInventoryStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiInventoryFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static int __xuiInventoryStyleInt(xui_widget pWidget, const char* sName, int* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     ((tProperty.tValue.iType == XUI_STYLE_VALUE_INT) || (tProperty.tValue.iType == XUI_STYLE_VALUE_BOOL)) ) {
		*pValue = tProperty.tValue.iInt;
		return 1;
	}
	return 0;
}

static xui_font __xuiInventoryStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiInventoryResolve(xui_widget pWidget, const xui_inventory_grid_data_t* pData, xui_inventory_grid_data_t* pResolved)
{
	int iValue;

	*pResolved = *pData;
	pResolved->pFont = __xuiInventoryStyleFont(pWidget, (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget)));
	(void)__xuiInventoryStyleColor(pWidget, "inventory.background.color", &pResolved->tColors.iBackgroundColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.background.color", &pResolved->tColors.iSlotColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.empty_color", &pResolved->tColors.iEmptyColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.hover_color", &pResolved->tColors.iHoverColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.active_color", &pResolved->tColors.iActiveColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.selected_color", &pResolved->tColors.iSelectedColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.disabled_color", &pResolved->tColors.iDisabledColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.locked_color", &pResolved->tColors.iLockedColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.border_color", &pResolved->tColors.iBorderColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.quality_color", &pResolved->tColors.iQualityColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.slot.focus_color", &pResolved->tColors.iFocusColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.text.color", &pResolved->tColors.iTextColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.text.muted_color", &pResolved->tColors.iMutedTextColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.count.color", &pResolved->tColors.iCountColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.hotkey.color", &pResolved->tColors.iHotkeyColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.cooldown.color", &pResolved->tColors.iCooldownColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.durability.color", &pResolved->tColors.iDurabilityColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.drag.color", &pResolved->tColors.iDragColor);
	(void)__xuiInventoryStyleColor(pWidget, "inventory.drop.color", &pResolved->tColors.iDropColor);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.slot.size", &pResolved->tLayout.fSlotSize);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.slot.gap", &pResolved->tLayout.fSlotGap);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.padding", &pResolved->tLayout.fPadding);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.icon.padding", &pResolved->tLayout.fIconPadding);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.border.width", &pResolved->tLayout.fBorderWidth);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.wheel.step", &pResolved->tLayout.fWheelStep);
	(void)__xuiInventoryStyleFloat(pWidget, "inventory.drag.threshold", &pResolved->tLayout.fDragThreshold);
	iValue = pResolved->tLayout.iColumns;
	if ( __xuiInventoryStyleInt(pWidget, "inventory.columns", &iValue) && iValue >= 0 ) pResolved->tLayout.iColumns = iValue;
	if ( pResolved->tLayout.fSlotSize < 1.0f ) pResolved->tLayout.fSlotSize = XUI_INVENTORY_DEFAULT_SLOT_SIZE;
	if ( pResolved->tLayout.fWheelStep <= 0.0f ) pResolved->tLayout.fWheelStep = pResolved->tLayout.fSlotSize + pResolved->tLayout.fSlotGap;
}

static int __xuiInventoryComputeColumns(const xui_inventory_grid_data_t* pData, xui_rect_t tViewport)
{
	float fUnit;
	float fAvailable;
	int iColumns;

	if ( pData == NULL ) {
		return 1;
	}
	if ( pData->tLayout.iColumns > 0 ) {
		return pData->tLayout.iColumns;
	}
	fUnit = pData->tLayout.fSlotSize + pData->tLayout.fSlotGap;
	if ( fUnit <= 0.0f ) {
		return 1;
	}
	fAvailable = tViewport.fW - (pData->tLayout.fPadding * 2.0f) + pData->tLayout.fSlotGap;
	iColumns = (int)(fAvailable / fUnit);
	if ( iColumns < 1 ) {
		iColumns = 1;
	}
	if ( (pData->iSlotCount > 0) && (iColumns > pData->iSlotCount) ) {
		iColumns = pData->iSlotCount;
	}
	return iColumns;
}

static int __xuiInventorySyncLayout(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved)
{
	xui_rect_t tViewport;
	float fStride;
	int iColumns;
	int iRows;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tViewport = xuiWidgetGetContentRect(pWidget);
	tViewport = xuiInternalSnapRect(tViewport);
	iColumns = __xuiInventoryComputeColumns(pResolved, tViewport);
	if ( iColumns < 1 ) iColumns = 1;
	iRows = (pData->iSlotCount + iColumns - 1) / iColumns;
	if ( iRows < 0 ) iRows = 0;
	fStride = pResolved->tLayout.fSlotSize + pResolved->tLayout.fSlotGap;
	pData->iResolvedColumns = iColumns;
	pData->iResolvedRows = iRows;
	pData->fContentWidth = (pResolved->tLayout.fPadding * 2.0f) +
		((iColumns > 0) ? ((float)iColumns * pResolved->tLayout.fSlotSize + (float)(iColumns - 1) * pResolved->tLayout.fSlotGap) : 0.0f);
	pData->fContentHeight = (pResolved->tLayout.fPadding * 2.0f) +
		((iRows > 0) ? ((float)iRows * pResolved->tLayout.fSlotSize + (float)(iRows - 1) * pResolved->tLayout.fSlotGap) : 0.0f);
	if ( fStride <= 0.0f ) {
		pData->fContentHeight = pResolved->tLayout.fPadding * 2.0f;
	}
	(void)xuiScrollModelSetViewport(&pData->tScroll, tViewport);
	(void)xuiScrollModelSetContentSize(&pData->tScroll, __xuiInventoryMaxFloat(tViewport.fW, pData->fContentWidth), __xuiInventoryMaxFloat(tViewport.fH, pData->fContentHeight));
	return XUI_OK;
}

static xui_rect_t __xuiInventorySlotContentRect(const xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved, int iSlot)
{
	float fStride;
	int iColumns;
	int iRow;
	int iColumn;

	if ( (pData == NULL) || (pResolved == NULL) || (iSlot < 0) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	iColumns = (pData->iResolvedColumns > 0) ? pData->iResolvedColumns : pResolved->tLayout.iColumns;
	if ( iColumns <= 0 ) iColumns = 1;
	iRow = iSlot / iColumns;
	iColumn = iSlot % iColumns;
	fStride = pResolved->tLayout.fSlotSize + pResolved->tLayout.fSlotGap;
	return xuiInternalSnapRect((xui_rect_t){
		pResolved->tLayout.fPadding + (float)iColumn * fStride,
		pResolved->tLayout.fPadding + (float)iRow * fStride,
		pResolved->tLayout.fSlotSize,
		pResolved->tLayout.fSlotSize
	});
}

static xui_rect_t __xuiInventorySlotLocalRect(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved, int iSlot)
{
	xui_rect_t tViewport;
	xui_rect_t tSlot;
	float fOffsetX;
	float fOffsetY;

	tViewport = xuiWidgetGetContentRect(pWidget);
	tSlot = __xuiInventorySlotContentRect(pData, pResolved, iSlot);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fOffsetX, &fOffsetY);
	tSlot.fX += tViewport.fX - fOffsetX;
	tSlot.fY += tViewport.fY - fOffsetY;
	return xuiInternalSnapRect(tSlot);
}

static int __xuiInventorySlotEnabled(const xui_inventory_slot_t* pSlot)
{
	if ( pSlot == NULL ) {
		return 0;
	}
	return ((pSlot->iFlags & (XUI_INVENTORY_SLOT_DISABLED | XUI_INVENTORY_SLOT_LOCKED)) == 0u);
}

static int __xuiInventorySelected(const xui_inventory_slot_t* pSlot)
{
	return (pSlot != NULL) && ((pSlot->iFlags & XUI_INVENTORY_SLOT_SELECTED) != 0u);
}

static void __xuiInventoryClearSelectionData(xui_inventory_grid_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	for ( i = 0; i < pData->iSlotCount; i++ ) {
		pData->arrSlots[i].iFlags &= ~XUI_INVENTORY_SLOT_SELECTED;
	}
}

static void __xuiInventoryNormalizeSlot(xui_inventory_slot_t* pSlot)
{
	if ( pSlot == NULL ) {
		return;
	}
	pSlot->iSize = sizeof(*pSlot);
	if ( pSlot->iIconTint == 0u ) {
		pSlot->iIconTint = XUI_COLOR_WHITE;
	}
	if ( pSlot->iAnimationTint == 0u ) {
		pSlot->iAnimationTint = XUI_COLOR_WHITE;
	}
	if ( pSlot->fAnimationScale <= 0.0f ) {
		pSlot->fAnimationScale = 1.0f;
	}
	pSlot->fCooldownRate = __xuiInventoryClampFloat(pSlot->fCooldownRate, 0.0f, 1.0f);
	pSlot->fDurabilityRate = __xuiInventoryClampFloat(pSlot->fDurabilityRate, 0.0f, 1.0f);
	pSlot->sText[XUI_INVENTORY_GRID_TEXT_CAPACITY - 1] = '\0';
	pSlot->sHotkey[XUI_INVENTORY_GRID_HOTKEY_CAPACITY - 1] = '\0';
	if ( (pSlot->pIcon == NULL) && (pSlot->iItemId == 0) && (pSlot->sText[0] == '\0') ) {
		pSlot->iFlags |= XUI_INVENTORY_SLOT_EMPTY;
	} else {
		pSlot->iFlags &= ~XUI_INVENTORY_SLOT_EMPTY;
	}
	if ( pSlot->pAnimation != NULL ) {
		pSlot->iFlags |= XUI_INVENTORY_SLOT_ANIMATION;
	}
}

static int __xuiInventoryInvalidate(xui_widget pWidget, uint32_t iFlags)
{
	if ( pWidget == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetInvalidate(pWidget, iFlags);
}

static xui_rect_t __xuiInventoryIntersectRect(xui_rect_t tA, xui_rect_t tB)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = __xuiInventoryMaxFloat(tA.fX, tB.fX);
	fTop = __xuiInventoryMaxFloat(tA.fY, tB.fY);
	fRight = __xuiInventoryMinFloat(tA.fX + tA.fW, tB.fX + tB.fW);
	fBottom = __xuiInventoryMinFloat(tA.fY + tA.fH, tB.fY + tB.fH);
	if ( (fRight <= fLeft) || (fBottom <= fTop) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return xuiInternalSnapRect((xui_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop});
}

static int __xuiInventoryInvalidateSlot(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot)
{
	xui_inventory_grid_data_t tResolved;
	xui_rect_t tBounds;
	xui_rect_t tSlot;

	if ( (pWidget == NULL) || (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) {
		return XUI_OK;
	}
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	if ( __xuiInventorySyncLayout(pWidget, pData, &tResolved) != XUI_OK ) {
		return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	tSlot = __xuiInventorySlotLocalRect(pWidget, pData, &tResolved, iSlot);
	tSlot.fX -= 3.0f;
	tSlot.fY -= 3.0f;
	tSlot.fW += 6.0f;
	tSlot.fH += 6.0f;
	tBounds = xuiWidgetGetRect(pWidget);
	tBounds.fX = 0.0f;
	tBounds.fY = 0.0f;
	tSlot = __xuiInventoryIntersectRect(tSlot, tBounds);
	if ( (tSlot.fW <= 0.0f) || (tSlot.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return xuiWidgetInvalidateRect(pWidget, tSlot, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiInventoryInvalidateHoverSlots(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iOldSlot, int iNewSlot)
{
	int iRet;

	iRet = __xuiInventoryInvalidateSlot(pWidget, pData, iOldSlot);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiInventoryInvalidateSlot(pWidget, pData, iNewSlot);
}

static void __xuiInventoryClearVisibleRange(xui_inventory_visible_range_t* pRange)
{
	if ( pRange == NULL ) return;
	memset(pRange, 0, sizeof(*pRange));
	pRange->iSize = sizeof(*pRange);
	pRange->iFirstSlot = -1;
	pRange->iLastSlot = -1;
	pRange->iFirstRow = -1;
	pRange->iLastRow = -1;
	pRange->iFirstColumn = -1;
	pRange->iLastColumn = -1;
}

static int __xuiInventoryComputeVisibleRange(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved, xui_inventory_visible_range_t* pRange)
{
	xui_rect_t tViewport;
	float fOffsetX;
	float fOffsetY;
	float fStride;
	int iFirstRow;
	int iLastRow;
	int iFirstColumn;
	int iLastColumn;
	int iRow;
	int iStart;
	int iEnd;
	int iCount;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) || (pRange == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInventoryClearVisibleRange(pRange);
	pRange->iTotalRows = pData->iResolvedRows;
	pRange->iTotalColumns = pData->iResolvedColumns;
	if ( (pData->iSlotCount <= 0) || (pData->iResolvedRows <= 0) || (pData->iResolvedColumns <= 0) ) {
		return XUI_OK;
	}
	fStride = pResolved->tLayout.fSlotSize + pResolved->tLayout.fSlotGap;
	if ( (fStride <= 0.0f) || (pResolved->tLayout.fSlotSize <= 0.0f) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetContentRect(pWidget);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fOffsetX, &fOffsetY);

	iFirstRow = (int)floorf((fOffsetY - pResolved->tLayout.fPadding - pResolved->tLayout.fSlotSize) / fStride) + 1;
	iLastRow = (int)ceilf((fOffsetY + tViewport.fH - pResolved->tLayout.fPadding) / fStride) - 1;
	iFirstColumn = (int)floorf((fOffsetX - pResolved->tLayout.fPadding - pResolved->tLayout.fSlotSize) / fStride) + 1;
	iLastColumn = (int)ceilf((fOffsetX + tViewport.fW - pResolved->tLayout.fPadding) / fStride) - 1;
	if ( iFirstRow < 0 ) iFirstRow = 0;
	if ( iFirstColumn < 0 ) iFirstColumn = 0;
	if ( iLastRow >= pData->iResolvedRows ) iLastRow = pData->iResolvedRows - 1;
	if ( iLastColumn >= pData->iResolvedColumns ) iLastColumn = pData->iResolvedColumns - 1;
	if ( (iFirstRow > iLastRow) || (iFirstColumn > iLastColumn) ) {
		return XUI_OK;
	}

	iCount = 0;
	for ( iRow = iFirstRow; iRow <= iLastRow; iRow++ ) {
		iStart = iRow * pData->iResolvedColumns + iFirstColumn;
		iEnd = iRow * pData->iResolvedColumns + iLastColumn;
		if ( iStart >= pData->iSlotCount ) break;
		if ( iEnd >= pData->iSlotCount ) iEnd = pData->iSlotCount - 1;
		if ( iStart <= iEnd ) {
			if ( pRange->iFirstSlot < 0 ) pRange->iFirstSlot = iStart;
			pRange->iLastSlot = iEnd;
			iCount += iEnd - iStart + 1;
		}
	}
	if ( iCount <= 0 ) {
		__xuiInventoryClearVisibleRange(pRange);
		pRange->iTotalRows = pData->iResolvedRows;
		pRange->iTotalColumns = pData->iResolvedColumns;
		return XUI_OK;
	}
	pRange->iSlotCount = iCount;
	pRange->iFirstRow = iFirstRow;
	pRange->iLastRow = iLastRow;
	pRange->iRowCount = iLastRow - iFirstRow + 1;
	pRange->iFirstColumn = iFirstColumn;
	pRange->iLastColumn = iLastColumn;
	pRange->iColumnCount = iLastColumn - iFirstColumn + 1;
	pRange->iPaintSlotCount = iCount;
	return XUI_OK;
}

static int __xuiInventoryAsciiLower(int iChar)
{
	if ( (iChar >= 'A') && (iChar <= 'Z') ) {
		return iChar + ('a' - 'A');
	}
	return iChar;
}

static int __xuiInventoryTextContains(const char* sText, const char* sNeedle, int bCaseInsensitive)
{
	const char* sScan;
	const char* sA;
	const char* sB;
	int iA;
	int iB;

	if ( (sNeedle == NULL) || (sNeedle[0] == '\0') ) return 1;
	if ( sText == NULL ) return 0;
	for ( sScan = sText; *sScan != '\0'; sScan++ ) {
		sA = sScan;
		sB = sNeedle;
		while ( (*sA != '\0') && (*sB != '\0') ) {
			iA = (unsigned char)*sA;
			iB = (unsigned char)*sB;
			if ( bCaseInsensitive ) {
				iA = __xuiInventoryAsciiLower(iA);
				iB = __xuiInventoryAsciiLower(iB);
			}
			if ( iA != iB ) break;
			sA++;
			sB++;
		}
		if ( *sB == '\0' ) return 1;
	}
	return 0;
}

static int __xuiInventoryCompareInt(int iA, int iB)
{
	if ( iA < iB ) return -1;
	if ( iA > iB ) return 1;
	return 0;
}

static int __xuiInventoryCompareUInt(uint32_t iA, uint32_t iB)
{
	if ( iA < iB ) return -1;
	if ( iA > iB ) return 1;
	return 0;
}

static int __xuiInventoryCompareText(const char* sA, const char* sB, int bCaseInsensitive)
{
	int iA;
	int iB;

	if ( sA == NULL ) sA = "";
	if ( sB == NULL ) sB = "";
	while ( (*sA != '\0') || (*sB != '\0') ) {
		iA = (unsigned char)*sA;
		iB = (unsigned char)*sB;
		if ( bCaseInsensitive ) {
			iA = __xuiInventoryAsciiLower(iA);
			iB = __xuiInventoryAsciiLower(iB);
		}
		if ( iA < iB ) return -1;
		if ( iA > iB ) return 1;
		if ( *sA != '\0' ) sA++;
		if ( *sB != '\0' ) sB++;
	}
	return 0;
}

static int __xuiInventoryQueryMatches(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_inventory_slot_query_t* pQuery, int iSlot)
{
	const xui_inventory_slot_t* pSlot;

	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return 0;
	if ( pQuery == NULL ) return 1;
	pSlot = &pData->arrSlots[iSlot];
	if ( ((pQuery->iFlags & XUI_INVENTORY_QUERY_EXCLUDE_EMPTY) != 0u) &&
	     ((pSlot->iFlags & XUI_INVENTORY_SLOT_EMPTY) != 0u) ) {
		return 0;
	}
	if ( (pQuery->iRequiredFlags != 0u) && ((pSlot->iFlags & pQuery->iRequiredFlags) != pQuery->iRequiredFlags) ) {
		return 0;
	}
	if ( (pQuery->iRejectedFlags != 0u) && ((pSlot->iFlags & pQuery->iRejectedFlags) != 0u) ) {
		return 0;
	}
	if ( !__xuiInventoryTextContains(pSlot->sText, pQuery->sTextContains, (pQuery->iFlags & XUI_INVENTORY_QUERY_CASE_INSENSITIVE) != 0u) ) {
		return 0;
	}
	if ( (pQuery->onFilter != NULL) && !pQuery->onFilter(pWidget, iSlot, pSlot, pQuery->pFilterUser) ) {
		return 0;
	}
	return 1;
}

static int __xuiInventoryCompareSlots(const xui_inventory_sort_context_t* pSort, int iSlotA, int iSlotB)
{
	const xui_inventory_slot_t* pSlotA;
	const xui_inventory_slot_t* pSlotB;
	int iRet;

	if ( (pSort == NULL) || (pSort->pData == NULL) ) return __xuiInventoryCompareInt(iSlotA, iSlotB);
	if ( (iSlotA < 0) || (iSlotA >= pSort->pData->iSlotCount) ||
	     (iSlotB < 0) || (iSlotB >= pSort->pData->iSlotCount) ) {
		return __xuiInventoryCompareInt(iSlotA, iSlotB);
	}
	pSlotA = &pSort->pData->arrSlots[iSlotA];
	pSlotB = &pSort->pData->arrSlots[iSlotB];
	iRet = 0;
	if ( (pSort->iSortMode == XUI_INVENTORY_SORT_CUSTOM) && (pSort->onCompare != NULL) ) {
		iRet = pSort->onCompare(pSort->pWidget, iSlotA, pSlotA, iSlotB, pSlotB, pSort->pCompareUser);
	} else {
		switch ( pSort->iSortMode ) {
		case XUI_INVENTORY_SORT_SLOT_ID:
			iRet = __xuiInventoryCompareInt(pSlotA->iSlotId, pSlotB->iSlotId);
			break;
		case XUI_INVENTORY_SORT_ITEM_ID:
			iRet = __xuiInventoryCompareInt(pSlotA->iItemId, pSlotB->iItemId);
			break;
		case XUI_INVENTORY_SORT_ITEM_TYPE:
			iRet = __xuiInventoryCompareInt(pSlotA->iItemType, pSlotB->iItemType);
			break;
		case XUI_INVENTORY_SORT_SLOT_TYPE:
			iRet = __xuiInventoryCompareInt(pSlotA->iSlotType, pSlotB->iSlotType);
			break;
		case XUI_INVENTORY_SORT_COUNT:
			iRet = __xuiInventoryCompareInt(pSlotA->iCount, pSlotB->iCount);
			break;
		case XUI_INVENTORY_SORT_QUALITY:
			iRet = __xuiInventoryCompareUInt(pSlotA->iQualityColor, pSlotB->iQualityColor);
			break;
		case XUI_INVENTORY_SORT_TEXT:
			iRet = __xuiInventoryCompareText(pSlotA->sText, pSlotB->sText, (pSort->iFlags & XUI_INVENTORY_QUERY_CASE_INSENSITIVE) != 0u);
			break;
		case XUI_INVENTORY_SORT_SLOT_INDEX:
		case XUI_INVENTORY_SORT_NONE:
		default:
			iRet = __xuiInventoryCompareInt(iSlotA, iSlotB);
			break;
		}
	}
	if ( iRet == 0 ) {
		iRet = __xuiInventoryCompareInt(iSlotA, iSlotB);
	}
	if ( (pSort->iFlags & XUI_INVENTORY_QUERY_SORT_DESCENDING) != 0u ) {
		iRet = -iRet;
	}
	return iRet;
}

static void __xuiInventorySortIndices(const xui_inventory_sort_context_t* pSort, int* arrSlots, int iLeft, int iRight)
{
	int i;
	int j;
	int iPivot;
	int iTemp;

	if ( (pSort == NULL) || (arrSlots == NULL) || (iLeft >= iRight) ) return;
	i = iLeft;
	j = iRight;
	iPivot = arrSlots[(iLeft + iRight) / 2];
	while ( i <= j ) {
		while ( __xuiInventoryCompareSlots(pSort, arrSlots[i], iPivot) < 0 ) i++;
		while ( __xuiInventoryCompareSlots(pSort, arrSlots[j], iPivot) > 0 ) j--;
		if ( i <= j ) {
			iTemp = arrSlots[i];
			arrSlots[i] = arrSlots[j];
			arrSlots[j] = iTemp;
			i++;
			j--;
		}
	}
	if ( iLeft < j ) __xuiInventorySortIndices(pSort, arrSlots, iLeft, j);
	if ( i < iRight ) __xuiInventorySortIndices(pSort, arrSlots, i, iRight);
}

static int __xuiInventorySetHover(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot)
{
	int iOldSlot;

	if ( (pData == NULL) || (pData->iHoverSlot == iSlot) ) {
		return XUI_OK;
	}
	iOldSlot = pData->iHoverSlot;
	pData->iHoverSlot = iSlot;
	return __xuiInventoryInvalidateHoverSlots(pWidget, pData, iOldSlot, iSlot);
}

static void __xuiInventoryNotifySelect(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, iSlot, pData->pSelectUser);
	}
}

static void __xuiInventoryNotifyActivate(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot, int iButton)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iActivateCount++;
	if ( pData->onActivate != NULL ) {
		pData->onActivate(pWidget, iSlot, iButton, pData->pActivateUser);
	}
}

static int __xuiInventorySetCurrentInternal(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot, int bNotify)
{
	int iOld;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSlot < 0) || (iSlot >= pData->iSlotCount) || !__xuiInventorySlotEnabled(&pData->arrSlots[iSlot]) ) {
		iSlot = -1;
	}
	iOld = pData->iCurrent;
	if ( iOld == iSlot ) {
		return XUI_OK;
	}
	pData->iCurrent = iSlot;
	if ( iSlot >= 0 ) {
		pData->iAnchorSlot = iSlot;
	}
	pData->iChangeCount++;
	(void)xuiInventoryGridEnsureSlotVisible(pWidget, iSlot);
	(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) {
		__xuiInventoryNotifySelect(pWidget, pData, iSlot);
	}
	return XUI_OK;
}

static int __xuiInventorySelectRange(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iFrom, int iTo, int bNotify)
{
	int iStart;
	int iEnd;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (iTo < 0) || (iTo >= pData->iSlotCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiInventorySlotEnabled(&pData->arrSlots[iTo]) ) {
		return XUI_OK;
	}
	if ( (iFrom < 0) || (iFrom >= pData->iSlotCount) ) {
		iFrom = iTo;
	}
	iStart = (iFrom < iTo) ? iFrom : iTo;
	iEnd = (iFrom < iTo) ? iTo : iFrom;
	__xuiInventoryClearSelectionData(pData);
	for ( i = iStart; i <= iEnd; i++ ) {
		if ( __xuiInventorySlotEnabled(&pData->arrSlots[i]) ) {
			pData->arrSlots[i].iFlags |= XUI_INVENTORY_SLOT_SELECTED;
		}
	}
	pData->iCurrent = iTo;
	pData->iChangeCount++;
	(void)xuiInventoryGridEnsureSlotVisible(pWidget, iTo);
	(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) {
		__xuiInventoryNotifySelect(pWidget, pData, iTo);
	}
	return XUI_OK;
}

static int __xuiInventorySelectSlot(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot, uint32_t iModifiers, int bNotify)
{
	int bCtrl;
	int bShift;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSlot < 0) || (iSlot >= pData->iSlotCount) || !__xuiInventorySlotEnabled(&pData->arrSlots[iSlot]) ) {
		return XUI_OK;
	}
	if ( pData->tLayout.iSelectionMode == XUI_INVENTORY_SELECTION_NONE ) {
		return __xuiInventorySetCurrentInternal(pWidget, pData, iSlot, bNotify);
	}
	if ( pData->tLayout.iSelectionMode == XUI_INVENTORY_SELECTION_SINGLE ) {
		__xuiInventoryClearSelectionData(pData);
		pData->arrSlots[iSlot].iFlags |= XUI_INVENTORY_SLOT_SELECTED;
		pData->iCurrent = iSlot;
		pData->iAnchorSlot = iSlot;
		pData->iChangeCount++;
		(void)xuiInventoryGridEnsureSlotVisible(pWidget, iSlot);
		(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( bNotify ) __xuiInventoryNotifySelect(pWidget, pData, iSlot);
		return XUI_OK;
	}
	bCtrl = ((iModifiers & XUI_MOD_CTRL) != 0);
	bShift = ((iModifiers & XUI_MOD_SHIFT) != 0);
	if ( bShift && (pData->iAnchorSlot >= 0) ) {
		return __xuiInventorySelectRange(pWidget, pData, pData->iAnchorSlot, iSlot, bNotify);
	}
	if ( bCtrl ) {
		if ( __xuiInventorySelected(&pData->arrSlots[iSlot]) ) {
			pData->arrSlots[iSlot].iFlags &= ~XUI_INVENTORY_SLOT_SELECTED;
		} else {
			pData->arrSlots[iSlot].iFlags |= XUI_INVENTORY_SLOT_SELECTED;
		}
	} else {
		__xuiInventoryClearSelectionData(pData);
		pData->arrSlots[iSlot].iFlags |= XUI_INVENTORY_SLOT_SELECTED;
	}
	pData->iCurrent = iSlot;
	pData->iAnchorSlot = iSlot;
	pData->iChangeCount++;
	(void)xuiInventoryGridEnsureSlotVisible(pWidget, iSlot);
	(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) __xuiInventoryNotifySelect(pWidget, pData, iSlot);
	return XUI_OK;
}

static int __xuiInventorySuggestedDropMode(const xui_inventory_slot_t* pFrom, const xui_inventory_slot_t* pTo)
{
	if ( (pFrom == NULL) || (pTo == NULL) ) {
		return XUI_INVENTORY_DROP_NONE;
	}
	if ( (pTo->iFlags & XUI_INVENTORY_SLOT_EMPTY) != 0u ) {
		return XUI_INVENTORY_DROP_MOVE;
	}
	if ( (pFrom->iItemId != 0) && (pFrom->iItemId == pTo->iItemId) &&
	     (pTo->iMaxCount > 0) && (pTo->iCount < pTo->iMaxCount) ) {
		return XUI_INVENTORY_DROP_STACK;
	}
	return XUI_INVENTORY_DROP_SWAP;
}

static int __xuiInventoryHitLocal(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved, float fLocalX, float fLocalY, xui_inventory_hit_t* pHit)
{
	xui_rect_t tViewport;
	xui_rect_t tSlot;
	xui_rect_t tIcon;
	xui_rect_t tCount;
	xui_rect_t tHotkey;
	float fOffsetX;
	float fOffsetY;
	float fContentX;
	float fContentY;
	float fStride;
	int iColumn;
	int iRow;
	int iSlot;

	if ( pHit != NULL ) {
		memset(pHit, 0, sizeof(*pHit));
		pHit->iSize = sizeof(*pHit);
		pHit->iPart = XUI_INVENTORY_HIT_NONE;
		pHit->iSlot = -1;
		pHit->fX = fLocalX;
		pHit->fY = fLocalY;
	}
	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iSlotCount <= 0 ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetContentRect(pWidget);
	if ( !__xuiInventoryRectContains(tViewport, fLocalX, fLocalY) ) {
		return XUI_OK;
	}
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fOffsetX, &fOffsetY);
	fContentX = fLocalX - tViewport.fX + fOffsetX - pResolved->tLayout.fPadding;
	fContentY = fLocalY - tViewport.fY + fOffsetY - pResolved->tLayout.fPadding;
	if ( (fContentX < 0.0f) || (fContentY < 0.0f) ) {
		return XUI_OK;
	}
	fStride = pResolved->tLayout.fSlotSize + pResolved->tLayout.fSlotGap;
	if ( fStride <= 0.0f ) {
		return XUI_OK;
	}
	iColumn = (int)(fContentX / fStride);
	iRow = (int)(fContentY / fStride);
	if ( (iColumn < 0) || (iColumn >= pData->iResolvedColumns) || (iRow < 0) ) {
		return XUI_OK;
	}
	if ( (fContentX - (float)iColumn * fStride) >= pResolved->tLayout.fSlotSize ||
	     (fContentY - (float)iRow * fStride) >= pResolved->tLayout.fSlotSize ) {
		return XUI_OK;
	}
	iSlot = iRow * pData->iResolvedColumns + iColumn;
	if ( (iSlot < 0) || (iSlot >= pData->iSlotCount) ) {
		return XUI_OK;
	}
	tSlot = __xuiInventorySlotLocalRect(pWidget, pData, pResolved, iSlot);
	if ( pHit != NULL ) {
		pHit->iPart = XUI_INVENTORY_HIT_SLOT;
		pHit->iSlot = iSlot;
		pHit->tRect = tSlot;
		tIcon = xuiInternalInsetRect(tSlot, pResolved->tLayout.fIconPadding);
		tCount = (xui_rect_t){tSlot.fX + tSlot.fW - 22.0f, tSlot.fY + tSlot.fH - 15.0f, 20.0f, 13.0f};
		tHotkey = (xui_rect_t){tSlot.fX + 4.0f, tSlot.fY + 3.0f, 18.0f, 13.0f};
		if ( __xuiInventoryRectContains(tCount, fLocalX, fLocalY) ) {
			pHit->iPart = XUI_INVENTORY_HIT_COUNT;
		} else if ( __xuiInventoryRectContains(tHotkey, fLocalX, fLocalY) ) {
			pHit->iPart = XUI_INVENTORY_HIT_HOTKEY;
		} else if ( __xuiInventoryRectContains(tIcon, fLocalX, fLocalY) ) {
			pHit->iPart = XUI_INVENTORY_HIT_ICON;
		}
	}
	return XUI_OK;
}

static int __xuiInventoryHitWorld(xui_widget pWidget, xui_inventory_grid_data_t* pData, float fWorldX, float fWorldY, xui_inventory_hit_t* pHit)
{
	xui_inventory_grid_data_t tResolved;
	xui_rect_t tWorld;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	return __xuiInventoryHitLocal(pWidget, pData, &tResolved, fWorldX - tWorld.fX, fWorldY - tWorld.fY, pHit);
}

static int __xuiInventoryDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiInventoryAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) && (pDraw != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiInventoryDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiInventoryAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return __xuiInventoryDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiInventoryDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiInventoryAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pDraw != NULL) ) {
		if ( pProxy->drawRectStroke != NULL ) {
			return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), fWidth, iColor);
		}
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiInventoryDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (sText == NULL) || (sText[0] == '\0') || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiInventoryAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pDraw == NULL) || (pFont == NULL) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, xuiInternalSnapRect(tRect), iColor, iFlags | XUI_TEXT_CLIP);
}

static xui_vec2_t __xuiInventoryMeasureText(xui_proxy pProxy, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( (sText == NULL) || (sText[0] == '\0') || (pProxy == NULL) || (pProxy->textMeasure == NULL) || (pFont == NULL) ) {
		return tSize;
	}
	(void)pProxy->textMeasure(pProxy, pFont, sText, &tSize);
	return tSize;
}

static int __xuiInventoryBuildTooltip(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlot, const xui_inventory_slot_t* pSlot)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pSlot == NULL) || ((pSlot->iFlags & XUI_INVENTORY_SLOT_EMPTY) != 0u) ) {
		return 0;
	}
	if ( pSlot->sText[0] != '\0' ) {
		__xuiInventorySafeCopy(pData->sTooltipTitle, (int)sizeof(pData->sTooltipTitle), pSlot->sText);
	} else if ( pSlot->iItemId != 0 ) {
		snprintf(pData->sTooltipTitle, sizeof(pData->sTooltipTitle), "Item %d", pSlot->iItemId);
	} else {
		snprintf(pData->sTooltipTitle, sizeof(pData->sTooltipTitle), "Slot %d", iSlot);
	}
	pData->sTooltipTitle[sizeof(pData->sTooltipTitle) - 1u] = '\0';

	if ( pSlot->sHotkey[0] != '\0' ) {
		snprintf(pData->sTooltipMeta, sizeof(pData->sTooltipMeta), "Item %d  Slot %d  Hotkey %s", pSlot->iItemId, pSlot->iSlotId, pSlot->sHotkey);
	} else {
		snprintf(pData->sTooltipMeta, sizeof(pData->sTooltipMeta), "Item %d  Slot %d", pSlot->iItemId, pSlot->iSlotId);
	}
	pData->sTooltipMeta[sizeof(pData->sTooltipMeta) - 1u] = '\0';

	if ( pData->onTooltip != NULL ) {
		pData->sTooltipText[0] = '\0';
		iRet = pData->onTooltip(pWidget, iSlot, pSlot, pData->sTooltipText, (int)sizeof(pData->sTooltipText), pData->pTooltipUser);
		pData->sTooltipText[sizeof(pData->sTooltipText) - 1u] = '\0';
		if ( iRet != XUI_OK ) {
			return 0;
		}
	} else if ( pSlot->iMaxCount > 1 ) {
		snprintf(pData->sTooltipText, sizeof(pData->sTooltipText), "Stack %d / %d", pSlot->iCount, pSlot->iMaxCount);
	} else if ( pSlot->iCount > 1 ) {
		snprintf(pData->sTooltipText, sizeof(pData->sTooltipText), "Stack %d", pSlot->iCount);
	} else if ( (pSlot->iFlags & XUI_INVENTORY_SLOT_LOCKED) != 0u ) {
		__xuiInventorySafeCopy(pData->sTooltipText, (int)sizeof(pData->sTooltipText), "Locked slot");
	} else if ( (pSlot->iFlags & XUI_INVENTORY_SLOT_DISABLED) != 0u ) {
		__xuiInventorySafeCopy(pData->sTooltipText, (int)sizeof(pData->sTooltipText), "Disabled slot");
	} else {
		__xuiInventorySafeCopy(pData->sTooltipText, (int)sizeof(pData->sTooltipText), "Ready");
	}
	pData->sTooltipText[sizeof(pData->sTooltipText) - 1u] = '\0';
	return 1;
}

static xui_vec2_t __xuiInventoryTooltipMeasure(xui_context pContext, xui_widget pOwner, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_proxy pProxy;
	xui_font pFont;
	xui_vec2_t tTitle;
	xui_vec2_t tBody;
	xui_vec2_t tMeta;
	xui_vec2_t tSize;
	float fTextWidth;
	float fTextHeight;

	(void)pOwner;
	tSize.fX = 220.0f;
	tSize.fY = 70.0f;
	pData = (xui_inventory_grid_data_t*)pUser;
	if ( !xuiInternalContextIsValid(pContext) || (pData == NULL) ) {
		return tSize;
	}
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	tTitle = __xuiInventoryMeasureText(pProxy, pFont, pData->sTooltipTitle);
	tBody = __xuiInventoryMeasureText(pProxy, pFont, pData->sTooltipText);
	tMeta = __xuiInventoryMeasureText(pProxy, pFont, pData->sTooltipMeta);
	fTextWidth = __xuiInventoryMaxFloat(tTitle.fX, __xuiInventoryMaxFloat(tBody.fX, tMeta.fX));
	fTextHeight = 18.0f;
	if ( pData->sTooltipText[0] != '\0' ) fTextHeight += 18.0f;
	if ( pData->sTooltipMeta[0] != '\0' ) fTextHeight += 16.0f;
	tSize.fX = __xuiInventoryClampFloat(62.0f + fTextWidth, 210.0f, 330.0f);
	tSize.fY = __xuiInventoryMaxFloat(58.0f, 16.0f + __xuiInventoryMaxFloat(36.0f, fTextHeight));
	return tSize;
}

static int __xuiInventoryTooltipPaint(xui_context pContext, xui_widget pOwner, xui_draw_context pDraw, xui_rect_t tRect, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	const xui_inventory_slot_t* pSlot;
	xui_proxy pProxy;
	xui_surface_desc_t tSurfaceDesc;
	xui_font pFont;
	xui_rect_t tIcon;
	xui_rect_t tIconInner;
	xui_rect_t tSrc;
	xui_rect_t tText;
	uint32_t iQuality;
	int iRet;

	(void)pOwner;
	pData = (xui_inventory_grid_data_t*)pUser;
	if ( (pData == NULL) || (pData->iTooltipSlot < 0) || (pData->iTooltipSlot >= pData->iSlotCount) ) {
		return XUI_OK;
	}
	pSlot = &pData->arrSlots[pData->iTooltipSlot];
	pProxy = xuiInternalContextGetProxy(pContext);
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iQuality = (pSlot->iQualityColor != 0u) ? pSlot->iQualityColor : pData->tColors.iQualityColor;
	if ( iQuality == 0u ) {
		iQuality = pData->tColors.iBorderColor;
	}
	tIcon = (xui_rect_t){tRect.fX + 10.0f, tRect.fY + 10.0f, 36.0f, 36.0f};
	iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tIcon, XUI_COLOR_RGBA(245, 249, 253, 255));
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiInventoryDrawStroke(pProxy, pDraw, tIcon, 1.0f, iQuality);
	if ( iRet != XUI_OK ) return iRet;
	tIconInner = xuiInternalInsetRect(tIcon, 6.0f);
	if ( (pSlot->pIcon != NULL) && (pProxy->drawSurface != NULL) ) {
		memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
		if ( pProxy->surfaceGetDesc != NULL ) {
			(void)pProxy->surfaceGetDesc(pProxy, pSlot->pIcon, &tSurfaceDesc);
		}
		tSrc = pSlot->tIconSrc;
		if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
			tSrc.fX = 0.0f;
			tSrc.fY = 0.0f;
			tSrc.fW = (tSurfaceDesc.iWidth > 0) ? (float)tSurfaceDesc.iWidth : pData->tLayout.fSlotSize;
			tSrc.fH = (tSurfaceDesc.iHeight > 0) ? (float)tSurfaceDesc.iHeight : pData->tLayout.fSlotSize;
		}
		iRet = pProxy->drawSurface(pProxy, pDraw, pSlot->pIcon, tSrc, xuiInternalSnapRect(tIconInner), (pSlot->iIconTint != 0u) ? pSlot->iIconTint : XUI_COLOR_WHITE, 0);
		if ( iRet != XUI_OK ) return iRet;
	} else {
		iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tIconInner, iQuality);
		if ( iRet != XUI_OK ) return iRet;
	}
	tText = (xui_rect_t){tRect.fX + 56.0f, tRect.fY + 9.0f, tRect.fW - 66.0f, 18.0f};
	iRet = __xuiInventoryDrawText(pProxy, pDraw, pFont, pData->sTooltipTitle, tText, pData->tColors.iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE);
	if ( iRet != XUI_OK ) return iRet;
	tText.fY += 19.0f;
	iRet = __xuiInventoryDrawText(pProxy, pDraw, pFont, pData->sTooltipText, tText, pData->tColors.iMutedTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE);
	if ( iRet != XUI_OK ) return iRet;
	tText.fY = tRect.fY + tRect.fH - 23.0f;
	tText.fH = 16.0f;
	return __xuiInventoryDrawText(pProxy, pDraw, pFont, pData->sTooltipMeta, tText, pData->tColors.iHotkeyColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE);
}

static int __xuiInventoryTooltipResolve(xui_context pContext, xui_widget pWidget, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_hit_t tHit;
	xui_rect_t tWorld;
	xui_rect_t tAnchor;

	pData = (xui_inventory_grid_data_t*)pUser;
	if ( !xuiInternalContextIsValid(pContext) || (pWidget == NULL) || (pDesc == NULL) || (pData == NULL) || !pData->bTooltipVisible ) {
		return 0;
	}
	if ( __xuiInventoryHitWorld(pWidget, pData, pContext->fTooltipMouseX, pContext->fTooltipMouseY, &tHit) != XUI_OK ) {
		return 0;
	}
	if ( (tHit.iSlot < 0) || (tHit.iSlot >= pData->iSlotCount) ) {
		return 0;
	}
	if ( !__xuiInventoryBuildTooltip(pWidget, pData, tHit.iSlot, &pData->arrSlots[tHit.iSlot]) ) {
		return 0;
	}
	pData->iTooltipSlot = tHit.iSlot;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tAnchor = tHit.tRect;
	tAnchor.fX += tWorld.fX;
	tAnchor.fY += tWorld.fY;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_CUSTOM;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_WIDGET_RIGHT;
	pDesc->bCustomAnchorRect = 1;
	pDesc->tAnchorRect = tAnchor;
	pDesc->fOffsetX = 8.0f;
	pDesc->fOffsetY = 0.0f;
	pDesc->fDelay = 0.18f;
	pDesc->bFollowCursor = 0;
	pDesc->onMeasure = __xuiInventoryTooltipMeasure;
	pDesc->onPaint = __xuiInventoryTooltipPaint;
	pDesc->pUser = pData;
	return 1;
}

static int __xuiInventoryDrawCooldownRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tSlot, float fRate, uint32_t iColor)
{
	xui_rect_t tOverlay;

	fRate = __xuiInventoryClampFloat(fRate, 0.0f, 1.0f);
	if ( fRate <= 0.0f ) {
		return XUI_OK;
	}
	tOverlay = tSlot;
	tOverlay.fY += tOverlay.fH * (1.0f - fRate);
	tOverlay.fH *= fRate;
	return __xuiInventoryDrawFill(pProxy, pDraw, tOverlay, iColor);
}

static int __xuiInventoryDrawCooldownRadial(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tSlot, float fRate, uint32_t iColor)
{
	xui_mesh_vertex_t arrVertices[XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS + 2];
	uint32_t arrIndices[XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS * 3];
	float fCenterX;
	float fCenterY;
	float fRadius;
	float fSweep;
	int iSegments;
	int i;

	fRate = __xuiInventoryClampFloat(fRate, 0.0f, 1.0f);
	if ( (fRate <= 0.0f) || (tSlot.fW <= 0.0f) || (tSlot.fH <= 0.0f) || (__xuiInventoryAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pProxy->drawMeshTriangles == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	fCenterX = tSlot.fX + tSlot.fW * 0.5f;
	fCenterY = tSlot.fY + tSlot.fH * 0.5f;
	fRadius = __xuiInventoryMinFloat(tSlot.fW, tSlot.fH) * 0.5f;
	if ( fRadius > 1.0f ) {
		fRadius -= 1.0f;
	}
	fSweep = XUI_INVENTORY_PI * 2.0f * fRate;
	iSegments = (int)((float)XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS * fRate + 0.999f);
	if ( iSegments < 1 ) iSegments = 1;
	if ( iSegments > XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS ) iSegments = XUI_INVENTORY_COOLDOWN_RADIAL_SEGMENTS;
	arrVertices[0].fX = fCenterX;
	arrVertices[0].fY = fCenterY;
	arrVertices[0].iColor = iColor;
	for ( i = 0; i <= iSegments; i++ ) {
		float fAngle;
		float fStep;

		fStep = (iSegments > 0) ? ((float)i / (float)iSegments) : 0.0f;
		fAngle = -XUI_INVENTORY_PI * 0.5f + fSweep * fStep;
		arrVertices[i + 1].fX = fCenterX + cosf(fAngle) * fRadius;
		arrVertices[i + 1].fY = fCenterY + sinf(fAngle) * fRadius;
		arrVertices[i + 1].iColor = iColor;
	}
	for ( i = 0; i < iSegments; i++ ) {
		arrIndices[i * 3 + 0] = 0u;
		arrIndices[i * 3 + 1] = (uint32_t)(i + 1);
		arrIndices[i * 3 + 2] = (uint32_t)(i + 2);
	}
	return pProxy->drawMeshTriangles(pProxy, pDraw, arrVertices, iSegments + 2, arrIndices, iSegments * 3, 0);
}

static int __xuiInventoryDrawSlot(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved, int iSlot)
{
	const xui_inventory_slot_t* pSlot;
	xui_surface_desc_t tSurfaceDesc;
	xui_rect_t tSlot;
	xui_rect_t tIcon;
	xui_rect_t tSrc;
	xui_rect_t tText;
	xui_rect_t tBar;
	float fCooldownRate;
	char sCount[32];
	uint32_t iState;
	uint32_t iFill;
	uint32_t iBorder;
	uint32_t iTextColor;
	int iHandled;
	int iRet;

	pSlot = &pData->arrSlots[iSlot];
	tSlot = __xuiInventorySlotLocalRect(pWidget, pData, pResolved, iSlot);
	iState = 0u;
	if ( pData->iHoverSlot == iSlot ) iState |= XUI_INVENTORY_STATE_HOVER;
	if ( pData->iActiveSlot == iSlot ) iState |= XUI_INVENTORY_STATE_ACTIVE;
	if ( __xuiInventorySelected(pSlot) ) iState |= XUI_INVENTORY_STATE_SELECTED;
	if ( pData->iCurrent == iSlot ) iState |= XUI_INVENTORY_STATE_CURRENT;
	if ( pData->iDragSource == iSlot ) iState |= XUI_INVENTORY_STATE_DRAG_SOURCE;
	if ( pData->iDropTarget == iSlot ) iState |= XUI_INVENTORY_STATE_DROP_TARGET;
	if ( !__xuiInventorySlotEnabled(pSlot) || !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	if ( pResolved->onRenderSlot != NULL ) {
		iHandled = pResolved->onRenderSlot(pWidget, iSlot, pSlot, pDraw, tSlot, iState, pResolved->pRenderSlotUser);
		if ( iHandled < 0 ) return iHandled;
		if ( iHandled ) return XUI_OK;
	}
	iFill = ((pSlot->iFlags & XUI_INVENTORY_SLOT_EMPTY) != 0u) ? pResolved->tColors.iEmptyColor : pResolved->tColors.iSlotColor;
	if ( (iState & XUI_INVENTORY_STATE_SELECTED) != 0u ) iFill = pResolved->tColors.iSelectedColor;
	else if ( (iState & XUI_INVENTORY_STATE_ACTIVE) != 0u ) iFill = pResolved->tColors.iActiveColor;
	else if ( (iState & XUI_INVENTORY_STATE_HOVER) != 0u ) iFill = pResolved->tColors.iHoverColor;
	iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tSlot, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iBorder = (pSlot->iQualityColor != 0u) ? pSlot->iQualityColor : pResolved->tColors.iBorderColor;
	if ( (pSlot->iFlags & XUI_INVENTORY_SLOT_HIGHLIGHT) != 0u ) {
		iBorder = pResolved->tColors.iQualityColor;
	}
	iRet = __xuiInventoryDrawStroke(pProxy, pDraw, tSlot, pResolved->tLayout.fBorderWidth, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pSlot->pIcon != NULL) && (pProxy->drawSurface != NULL) ) {
		memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
		if ( pProxy->surfaceGetDesc != NULL ) {
			(void)pProxy->surfaceGetDesc(pProxy, pSlot->pIcon, &tSurfaceDesc);
		}
		tSrc = pSlot->tIconSrc;
		if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
			tSrc.fX = 0.0f;
			tSrc.fY = 0.0f;
			tSrc.fW = (tSurfaceDesc.iWidth > 0) ? (float)tSurfaceDesc.iWidth : pResolved->tLayout.fSlotSize;
			tSrc.fH = (tSurfaceDesc.iHeight > 0) ? (float)tSurfaceDesc.iHeight : pResolved->tLayout.fSlotSize;
		}
		tIcon = xuiInternalInsetRect(tSlot, pResolved->tLayout.fIconPadding);
		iRet = pProxy->drawSurface(pProxy, pDraw, pSlot->pIcon, tSrc, xuiInternalSnapRect(tIcon), (pSlot->iIconTint != 0u) ? pSlot->iIconTint : XUI_COLOR_WHITE, 0);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( pSlot->sText[0] != '\0' ) {
		tText = xuiInternalInsetRect(tSlot, 5.0f);
		iTextColor = ((iState & XUI_WIDGET_STATE_DISABLED) != 0u) ? pResolved->tColors.iMutedTextColor : pResolved->tColors.iTextColor;
		iRet = __xuiInventoryDrawText(pProxy, pDraw, pResolved->pFont, pSlot->sText, tText, iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (((pSlot->iFlags & XUI_INVENTORY_SLOT_ANIMATION) != 0u) || (pSlot->pAnimation != NULL)) &&
	     (pResolved->onRenderAnimation != NULL) ) {
		xui_rect_t tAnimation;
		float fScale;

		tAnimation = tSlot;
		fScale = (pSlot->fAnimationScale > 0.0f) ? pSlot->fAnimationScale : 1.0f;
		if ( fScale != 1.0f ) {
			float fW;
			float fH;

			fW = tSlot.fW * fScale;
			fH = tSlot.fH * fScale;
			tAnimation.fX = tSlot.fX + (tSlot.fW - fW) * 0.5f;
			tAnimation.fY = tSlot.fY + (tSlot.fH - fH) * 0.5f;
			tAnimation.fW = fW;
			tAnimation.fH = fH;
		}
		iRet = pResolved->onRenderAnimation(
			pWidget,
			iSlot,
			pSlot,
			pSlot->pAnimation,
			pDraw,
			xuiInternalSnapRect(tAnimation),
			iState,
			pSlot->iAnimationFlags,
			(pSlot->iAnimationTint != 0u) ? pSlot->iAnimationTint : XUI_COLOR_WHITE,
			pResolved->pRenderAnimationUser);
		if ( iRet != XUI_OK ) return iRet;
	}
	fCooldownRate = __xuiInventoryClampFloat(pSlot->fCooldownRate, 0.0f, 1.0f);
	if ( ((pSlot->iFlags & XUI_INVENTORY_SLOT_COOLDOWN) != 0u) && (fCooldownRate > 0.0f) ) {
		if ( (pSlot->iFlags & XUI_INVENTORY_SLOT_COOLDOWN_RADIAL) != 0u ) {
			iRet = __xuiInventoryDrawCooldownRadial(pProxy, pDraw, tSlot, fCooldownRate, pResolved->tColors.iCooldownColor);
			if ( iRet == XUI_ERROR_UNSUPPORTED ) {
				iRet = __xuiInventoryDrawCooldownRect(pProxy, pDraw, tSlot, fCooldownRate, pResolved->tColors.iCooldownColor);
			}
		} else {
			iRet = __xuiInventoryDrawCooldownRect(pProxy, pDraw, tSlot, fCooldownRate, pResolved->tColors.iCooldownColor);
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((pSlot->iFlags & XUI_INVENTORY_SLOT_DURABILITY) != 0u) && (pSlot->fDurabilityRate > 0.0f) ) {
		tBar = (xui_rect_t){tSlot.fX + 5.0f, tSlot.fY + tSlot.fH - 6.0f, (tSlot.fW - 10.0f) * pSlot->fDurabilityRate, 3.0f};
		iRet = __xuiInventoryDrawFill(pProxy, pDraw, tBar, pResolved->tColors.iDurabilityColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pSlot->sHotkey[0] != '\0' ) {
		tText = (xui_rect_t){tSlot.fX + 4.0f, tSlot.fY + 2.0f, tSlot.fW - 8.0f, 14.0f};
		iRet = __xuiInventoryDrawText(pProxy, pDraw, pResolved->pFont, pSlot->sHotkey, tText, pResolved->tColors.iHotkeyColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pSlot->iCount > 1 ) {
		snprintf(sCount, sizeof(sCount), "%d", pSlot->iCount);
		tText = (xui_rect_t){tSlot.fX + 2.0f, tSlot.fY + tSlot.fH - 17.0f, tSlot.fW - 5.0f, 15.0f};
		iRet = __xuiInventoryDrawText(pProxy, pDraw, pResolved->pFont, sCount, tText, pResolved->tColors.iCountColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_BOTTOM);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (pSlot->iFlags & XUI_INVENTORY_SLOT_LOCKED) != 0u ) {
		iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tSlot, pResolved->tColors.iLockedColor);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0u ) {
		iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tSlot, pResolved->tColors.iDisabledColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (iState & XUI_INVENTORY_STATE_DROP_TARGET) != 0u ) {
		iRet = __xuiInventoryDrawStroke(pProxy, pDraw, xuiInternalInsetRect(tSlot, 1.0f), 2.0f, pResolved->tColors.iDropColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (iState & XUI_INVENTORY_STATE_DRAG_SOURCE) != 0u ) {
		iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tSlot, pResolved->tColors.iDragColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iState & XUI_INVENTORY_STATE_CURRENT) != 0u) &&
	     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
	     xuiWidgetGetEnabled(pWidget) ) {
		iRet = __xuiInventoryDrawStroke(pProxy, pDraw, xuiInternalInsetRect(tSlot, 1.0f), 1.0f, pResolved->tColors.iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiInventoryDrawDragPreview(xui_proxy pProxy, xui_draw_context pDraw, xui_inventory_grid_data_t* pData, const xui_inventory_grid_data_t* pResolved)
{
	const xui_inventory_slot_t* pSlot;
	xui_surface_desc_t tSurfaceDesc;
	xui_rect_t tSlot;
	xui_rect_t tIcon;
	xui_rect_t tSrc;
	xui_rect_t tText;
	char sCount[32];
	int iRet;

	if ( (pData == NULL) || (pResolved == NULL) || !pData->bDragging ||
	     (pData->iDragSource < 0) || (pData->iDragSource >= pData->iSlotCount) ) {
		return XUI_OK;
	}
	pSlot = &pData->arrSlots[pData->iDragSource];
	tSlot = xuiInternalSnapRect((xui_rect_t){
		pData->fDragX - pResolved->tLayout.fSlotSize * 0.5f,
		pData->fDragY - pResolved->tLayout.fSlotSize * 0.5f,
		pResolved->tLayout.fSlotSize,
		pResolved->tLayout.fSlotSize
	});
	iRet = __xuiInventoryDrawRectFill(pProxy, pDraw, tSlot, pResolved->tColors.iDragColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiInventoryDrawStroke(pProxy, pDraw, tSlot, 1.0f, (pSlot->iQualityColor != 0u) ? pSlot->iQualityColor : pResolved->tColors.iFocusColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pSlot->pIcon != NULL) && (pProxy != NULL) && (pProxy->drawSurface != NULL) ) {
		memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
		if ( pProxy->surfaceGetDesc != NULL ) {
			(void)pProxy->surfaceGetDesc(pProxy, pSlot->pIcon, &tSurfaceDesc);
		}
		tSrc = pSlot->tIconSrc;
		if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
			tSrc.fX = 0.0f;
			tSrc.fY = 0.0f;
			tSrc.fW = (tSurfaceDesc.iWidth > 0) ? (float)tSurfaceDesc.iWidth : pResolved->tLayout.fSlotSize;
			tSrc.fH = (tSurfaceDesc.iHeight > 0) ? (float)tSurfaceDesc.iHeight : pResolved->tLayout.fSlotSize;
		}
		tIcon = xuiInternalInsetRect(tSlot, pResolved->tLayout.fIconPadding);
		iRet = pProxy->drawSurface(pProxy, pDraw, pSlot->pIcon, tSrc, xuiInternalSnapRect(tIcon), (pSlot->iIconTint != 0u) ? pSlot->iIconTint : XUI_COLOR_WHITE, 0);
		if ( iRet != XUI_OK ) return iRet;
	} else if ( pSlot->sText[0] != '\0' ) {
		tText = xuiInternalInsetRect(tSlot, 5.0f);
		iRet = __xuiInventoryDrawText(pProxy, pDraw, pResolved->pFont, pSlot->sText, tText, pResolved->tColors.iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pSlot->iCount > 1 ) {
		snprintf(sCount, sizeof(sCount), "%d", pSlot->iCount);
		tText = (xui_rect_t){tSlot.fX + 2.0f, tSlot.fY + tSlot.fH - 17.0f, tSlot.fW - 5.0f, 15.0f};
		iRet = __xuiInventoryDrawText(pProxy, pDraw, pResolved->pFont, sCount, tText, pResolved->tColors.iCountColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_BOTTOM);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiInventoryCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;
	xui_inventory_visible_range_t tRange;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tViewport;
	xui_rect_t tSlot;
	int iRow;
	int iColumn;
	int iSlot;
	int iPainted;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetContentRect(pWidget);
	iRet = __xuiInventoryDrawFill(pProxy, pDraw, tRect, tResolved.tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	__xuiInventoryClearVisibleRange(&pData->tLastPaintRange);
	pData->iLastPaintSlotCount = 0;
	if ( (pData->iSlotCount <= 0) || (pData->iResolvedColumns <= 0) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetContentRect(pWidget);
	iRet = __xuiInventoryComputeVisibleRange(pWidget, pData, &tResolved, &tRange);
	if ( iRet != XUI_OK ) return iRet;
	if ( tRange.iSlotCount <= 0 ) {
		pData->tLastPaintRange = tRange;
		return __xuiInventoryDrawDragPreview(pProxy, pDraw, pData, &tResolved);
	}
	iPainted = 0;
	for ( iRow = tRange.iFirstRow; iRow <= tRange.iLastRow; iRow++ ) {
		for ( iColumn = tRange.iFirstColumn; iColumn <= tRange.iLastColumn; iColumn++ ) {
			iSlot = iRow * pData->iResolvedColumns + iColumn;
			if ( iSlot >= pData->iSlotCount ) break;
			tSlot = __xuiInventorySlotLocalRect(pWidget, pData, &tResolved, iSlot);
			if ( (tSlot.fX + tSlot.fW <= tViewport.fX) ||
			     (tSlot.fY + tSlot.fH <= tViewport.fY) ||
			     (tSlot.fX >= tViewport.fX + tViewport.fW) ||
			     (tSlot.fY >= tViewport.fY + tViewport.fH) ) {
				continue;
			}
			iRet = __xuiInventoryDrawSlot(pWidget, pProxy, pDraw, pData, &tResolved, iSlot);
			if ( iRet != XUI_OK ) return iRet;
			iPainted++;
		}
	}
	tRange.iPaintSlotCount = iPainted;
	pData->tLastPaintRange = tRange;
	pData->iLastPaintSlotCount = iPainted;
	return __xuiInventoryDrawDragPreview(pProxy, pDraw, pData, &tResolved);
}

static int __xuiInventoryPointerMove(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_inventory_hit_t tHit;
	xui_rect_t tWorld;
	float fDX;
	float fDY;
	float fOldDragX;
	float fOldDragY;
	int iOldDrop;
	int bCanDrag;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	(void)__xuiInventoryHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( pData->bPointerDown && (pData->iActiveSlot >= 0) && ((pEvent->iButtons & XUI_POINTER_BUTTON_LEFT) != 0u) ) {
		fDX = pEvent->fX - pData->fPressX;
		fDY = pEvent->fY - pData->fPressY;
		if ( !pData->bDragging && ((fDX * fDX + fDY * fDY) >= (pData->tLayout.fDragThreshold * pData->tLayout.fDragThreshold)) ) {
			bCanDrag = 1;
			if ( pData->onDrag != NULL ) {
				bCanDrag = pData->onDrag(pWidget, pData->iActiveSlot, pData->pDragUser);
			}
			if ( bCanDrag ) {
				pData->bDragging = 1;
				pData->iDragSource = pData->iActiveSlot;
				pData->iDragCount++;
			}
		}
		if ( pData->bDragging ) {
			iOldDrop = pData->iDropTarget;
			fOldDragX = pData->fDragX;
			fOldDragY = pData->fDragY;
			tWorld = xuiWidgetGetWorldRect(pWidget);
			pData->fDragX = pEvent->fX - tWorld.fX;
			pData->fDragY = pEvent->fY - tWorld.fY;
			pData->iDropTarget = (tHit.iSlot >= 0 && tHit.iSlot != pData->iDragSource) ? tHit.iSlot : -1;
			pData->iDropMode = (pData->iDropTarget >= 0) ? __xuiInventorySuggestedDropMode(&pData->arrSlots[pData->iDragSource], &pData->arrSlots[pData->iDropTarget]) : XUI_INVENTORY_DROP_NONE;
			if ( (iOldDrop != pData->iDropTarget) || (fOldDragX != pData->fDragX) || (fOldDragY != pData->fDragY) ) {
				(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	return __xuiInventorySetHover(pWidget, pData, tHit.iSlot);
}

static int __xuiInventoryPointerDown(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_inventory_hit_t tHit;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) || !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	(void)__xuiInventoryHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		pData->bPointerDown = 1;
		pData->bDragging = 0;
		pData->iActiveSlot = tHit.iSlot;
		pData->iDragSource = -1;
		pData->iDropTarget = -1;
		pData->iDropMode = XUI_INVENTORY_DROP_NONE;
		pData->fPressX = pEvent->fX;
		pData->fPressY = pEvent->fY;
		pData->fDragX = pEvent->fX - xuiWidgetGetWorldRect(pWidget).fX;
		pData->fDragY = pEvent->fY - xuiWidgetGetWorldRect(pWidget).fY;
		(void)__xuiInventorySetHover(pWidget, pData, tHit.iSlot);
		if ( tHit.iSlot >= 0 ) {
			(void)__xuiInventorySelectSlot(pWidget, pData, tHit.iSlot, pEvent->iModifiers, 1);
		}
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iButton == XUI_POINTER_BUTTON_RIGHT ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( tHit.iSlot >= 0 ) {
			(void)__xuiInventorySelectSlot(pWidget, pData, tHit.iSlot, 0, 1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiInventoryPointerUp(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_inventory_hit_t tHit;
	int iFrom;
	int iTo;
	int iMode;
	int iAccept;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	iFrom = pData->iDragSource;
	iTo = pData->iDropTarget;
	iMode = pData->iDropMode;
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	(void)__xuiInventoryHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( pData->bDragging && (iFrom >= 0) && (iTo >= 0) && (pData->onDrop != NULL) ) {
		iAccept = pData->onDrop(pWidget, iFrom, iTo, iMode, pData->pDropUser);
		if ( iAccept ) {
			pData->iDropCount++;
			pData->iChangeCount++;
		}
	}
	if ( !pData->bDragging && (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) && (tHit.iSlot >= 0) && (tHit.iSlot == pData->iActiveSlot) ) {
		__xuiInventoryNotifyActivate(pWidget, pData, tHit.iSlot, pEvent->iButton);
	}
	pData->bPointerDown = 0;
	pData->bDragging = 0;
	pData->iActiveSlot = -1;
	pData->iDragSource = -1;
	pData->iDropTarget = -1;
	pData->iDropMode = XUI_INVENTORY_DROP_NONE;
	(void)__xuiInventorySetHover(pWidget, pData, tHit.iSlot);
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER) | (int)XUI_EVENT_DISPATCH_STOP;
}

static int __xuiInventoryContextMenu(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_inventory_hit_t tHit;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	(void)__xuiInventoryHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( (tHit.iSlot >= 0) && !__xuiInventorySelected(&pData->arrSlots[tHit.iSlot]) ) {
		(void)__xuiInventorySelectSlot(pWidget, pData, tHit.iSlot, 0, 1);
	}
	pData->iContextCount++;
	if ( pData->onContext != NULL ) {
		pData->onContext(pWidget, tHit.iSlot, pEvent->fX, pEvent->fY, pData->pContextUser);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiInventoryKeyMove(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iDelta, uint32_t iModifiers)
{
	int iTarget;

	if ( pData->iSlotCount <= 0 ) {
		return XUI_OK;
	}
	iTarget = (pData->iCurrent >= 0) ? (pData->iCurrent + iDelta) : 0;
	iTarget = __xuiInventoryClampInt(iTarget, 0, pData->iSlotCount - 1);
	while ( (iTarget >= 0) && (iTarget < pData->iSlotCount) ) {
		if ( __xuiInventorySlotEnabled(&pData->arrSlots[iTarget]) ) {
			(void)__xuiInventorySelectSlot(pWidget, pData, iTarget, iModifiers, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		iTarget += (iDelta >= 0) ? 1 : -1;
	}
	return XUI_OK;
}

static int __xuiInventoryGamepadFindTarget(xui_inventory_grid_data_t* pData, int iStart, int iStep, int iMin, int iMax, int bWrap, int bSkipDisabled)
{
	int iTarget;
	int iGuard;

	if ( (pData == NULL) || (iStep == 0) || (iMin > iMax) ) {
		return -1;
	}
	if ( iStart < iMin ) {
		iStart = bWrap ? iMax : iMin;
	} else if ( iStart > iMax ) {
		iStart = bWrap ? iMin : iMax;
	}
	iTarget = iStart;
	iGuard = iMax - iMin + 1;
	while ( iGuard-- > 0 ) {
		if ( (iTarget >= iMin) && (iTarget <= iMax) ) {
			if ( __xuiInventorySlotEnabled(&pData->arrSlots[iTarget]) ) {
				return iTarget;
			}
			if ( !bSkipDisabled ) {
				return -1;
			}
		}
		iTarget += iStep;
		if ( iTarget < iMin ) {
			if ( !bWrap ) return -1;
			iTarget = iMax;
		} else if ( iTarget > iMax ) {
			if ( !bWrap ) return -1;
			iTarget = iMin;
		}
	}
	return -1;
}

static int __xuiInventoryGamepadLastColumnSlot(const xui_inventory_grid_data_t* pData, int iColumn, int iColumns)
{
	int iTarget;

	if ( (pData == NULL) || (iColumns <= 0) || (iColumn < 0) || (iColumn >= iColumns) || (iColumn >= pData->iSlotCount) ) {
		return -1;
	}
	iTarget = ((pData->iSlotCount - 1) / iColumns) * iColumns + iColumn;
	while ( iTarget >= pData->iSlotCount ) {
		iTarget -= iColumns;
	}
	return (iTarget >= 0) ? iTarget : -1;
}

static int __xuiInventoryGamepadFindColumnTarget(xui_inventory_grid_data_t* pData, int iStart, int iColumn, int iColumns, int iStepRows, int bWrap, int bSkipDisabled)
{
	int iTarget;
	int iGuard;

	if ( (pData == NULL) || (iColumns <= 0) || (iStepRows == 0) || (iColumn < 0) || (iColumn >= iColumns) ) {
		return -1;
	}
	iTarget = iStart;
	iGuard = pData->iResolvedRows + 1;
	if ( iGuard < 1 ) iGuard = (pData->iSlotCount / iColumns) + 2;
	while ( iGuard-- > 0 ) {
		if ( (iTarget >= 0) && (iTarget < pData->iSlotCount) ) {
			if ( __xuiInventorySlotEnabled(&pData->arrSlots[iTarget]) ) {
				return iTarget;
			}
			if ( !bSkipDisabled ) {
				return -1;
			}
		}
		iTarget += iStepRows * iColumns;
		if ( (iTarget < 0) || (iTarget >= pData->iSlotCount) ) {
			if ( !bWrap ) return -1;
			iTarget = (iStepRows > 0) ? iColumn : __xuiInventoryGamepadLastColumnSlot(pData, iColumn, iColumns);
			if ( iTarget < 0 ) return -1;
		}
	}
	return -1;
}

static int __xuiInventoryGamepadSetTarget(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iTarget, uint32_t iModifiers)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iTarget < 0) || (iTarget >= pData->iSlotCount) || !__xuiInventorySlotEnabled(&pData->arrSlots[iTarget]) ) {
		return XUI_OK;
	}
	if ( (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_MULTI_SELECT_MODIFIERS) == 0u ) {
		iModifiers = 0u;
	}
	if ( (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_SELECT_ON_MOVE) != 0u ) {
		return __xuiInventorySelectSlot(pWidget, pData, iTarget, iModifiers, 1);
	}
	return __xuiInventorySetCurrentInternal(pWidget, pData, iTarget, 1);
}

static int __xuiInventoryGamepadMoveLinear(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iDelta, uint32_t iModifiers)
{
	int iTarget;
	int iStep;

	if ( (pData == NULL) || (pData->iSlotCount <= 0) || (iDelta == 0) ) {
		return XUI_OK;
	}
	iTarget = (pData->iCurrent >= 0) ? (pData->iCurrent + iDelta) : 0;
	iTarget = __xuiInventoryClampInt(iTarget, 0, pData->iSlotCount - 1);
	iStep = (iDelta >= 0) ? 1 : -1;
	iTarget = __xuiInventoryGamepadFindTarget(pData, iTarget, iStep, 0, pData->iSlotCount - 1, 0, (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_SKIP_DISABLED) != 0u);
	return __xuiInventoryGamepadSetTarget(pWidget, pData, iTarget, iModifiers);
}

static int __xuiInventoryGamepadMoveHorizontal(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iDelta, uint32_t iModifiers)
{
	int iColumns;
	int iRowStart;
	int iRowEnd;
	int iTarget;
	int iStep;

	if ( (pData == NULL) || (pData->iSlotCount <= 0) ) {
		return XUI_OK;
	}
	if ( (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_WRAP_ROWS) == 0u || pData->iCurrent < 0 ) {
		return __xuiInventoryGamepadMoveLinear(pWidget, pData, iDelta, iModifiers);
	}
	iColumns = pData->iResolvedColumns;
	if ( iColumns <= 0 ) {
		return __xuiInventoryGamepadMoveLinear(pWidget, pData, iDelta, iModifiers);
	}
	iStep = (iDelta >= 0) ? 1 : -1;
	iRowStart = (pData->iCurrent / iColumns) * iColumns;
	iRowEnd = __xuiInventoryMinInt(iRowStart + iColumns - 1, pData->iSlotCount - 1);
	iTarget = pData->iCurrent + iStep;
	if ( iTarget < iRowStart ) {
		iTarget = iRowEnd;
	} else if ( iTarget > iRowEnd ) {
		iTarget = iRowStart;
	}
	iTarget = __xuiInventoryGamepadFindTarget(pData, iTarget, iStep, iRowStart, iRowEnd, 1, (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_SKIP_DISABLED) != 0u);
	return __xuiInventoryGamepadSetTarget(pWidget, pData, iTarget, iModifiers);
}

static int __xuiInventoryGamepadMoveVertical(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iDelta, uint32_t iModifiers)
{
	int iColumns;
	int iColumn;
	int iTarget;
	int iStepRows;

	if ( (pData == NULL) || (pData->iSlotCount <= 0) ) {
		return XUI_OK;
	}
	if ( (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_WRAP_COLUMNS) == 0u || pData->iCurrent < 0 ) {
		return __xuiInventoryGamepadMoveLinear(pWidget, pData, iDelta, iModifiers);
	}
	iColumns = pData->iResolvedColumns;
	if ( iColumns <= 0 ) {
		return __xuiInventoryGamepadMoveLinear(pWidget, pData, iDelta, iModifiers);
	}
	iColumn = pData->iCurrent % iColumns;
	iStepRows = (iDelta >= 0) ? 1 : -1;
	iTarget = pData->iCurrent + (iStepRows * iColumns);
	if ( iTarget < 0 ) {
		iTarget = __xuiInventoryGamepadLastColumnSlot(pData, iColumn, iColumns);
	} else if ( iTarget >= pData->iSlotCount ) {
		iTarget = iColumn;
	}
	iTarget = __xuiInventoryGamepadFindColumnTarget(pData, iTarget, iColumn, iColumns, iStepRows, 1, (pData->tGamepadProfile.iFlags & XUI_INVENTORY_GAMEPAD_SKIP_DISABLED) != 0u);
	return __xuiInventoryGamepadSetTarget(pWidget, pData, iTarget, iModifiers);
}

static int __xuiInventoryGamepadOpenContext(xui_widget pWidget, xui_inventory_grid_data_t* pData)
{
	xui_rect_t tSlot;
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iSlot;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iSlot = pData->iCurrent;
	if ( (iSlot < 0) || (iSlot >= pData->iSlotCount) || !__xuiInventorySlotEnabled(&pData->arrSlots[iSlot]) ) {
		return XUI_OK;
	}
	if ( !__xuiInventorySelected(&pData->arrSlots[iSlot]) ) {
		iRet = __xuiInventorySelectSlot(pWidget, pData, iSlot, 0u, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = xuiInventoryGridGetSlotRect(pWidget, iSlot, &tSlot);
	if ( iRet != XUI_OK ) return iRet;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fX = tWorld.fX + tSlot.fX + tSlot.fW * 0.5f;
	fY = tWorld.fY + tSlot.fY + tSlot.fH * 0.5f;
	pData->iContextCount++;
	if ( pData->onContext != NULL ) {
		pData->onContext(pWidget, iSlot, fX, fY, pData->pContextUser);
	}
	return XUI_OK;
}

static int __xuiInventoryKeyDown(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_inventory_grid_data_t tResolved;
	xui_rect_t tViewport;
	int iRows;
	int iDelta;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	switch ( pEvent->iKey ) {
	case XUI_KEY_LEFT:
		return __xuiInventoryKeyMove(pWidget, pData, -1, pEvent->iModifiers);
	case XUI_KEY_RIGHT:
		return __xuiInventoryKeyMove(pWidget, pData, 1, pEvent->iModifiers);
	case XUI_KEY_UP:
		return __xuiInventoryKeyMove(pWidget, pData, -pData->iResolvedColumns, pEvent->iModifiers);
	case XUI_KEY_DOWN:
		return __xuiInventoryKeyMove(pWidget, pData, pData->iResolvedColumns, pEvent->iModifiers);
	case XUI_KEY_PAGE_UP:
	case XUI_KEY_PAGE_DOWN:
		tViewport = xuiWidgetGetContentRect(pWidget);
		iRows = (int)(tViewport.fH / (tResolved.tLayout.fSlotSize + tResolved.tLayout.fSlotGap));
		if ( iRows < 1 ) iRows = 1;
		iDelta = iRows * pData->iResolvedColumns;
		if ( pEvent->iKey == XUI_KEY_PAGE_UP ) iDelta = -iDelta;
		return __xuiInventoryKeyMove(pWidget, pData, iDelta, pEvent->iModifiers);
	case XUI_KEY_HOME:
		return __xuiInventoryKeyMove(pWidget, pData, -pData->iSlotCount, pEvent->iModifiers);
	case XUI_KEY_END:
		return __xuiInventoryKeyMove(pWidget, pData, pData->iSlotCount, pEvent->iModifiers);
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( (pData->iCurrent >= 0) && (pData->iCurrent < pData->iSlotCount) ) {
			__xuiInventoryNotifyActivate(pWidget, pData, pData->iCurrent, XUI_POINTER_BUTTON_LEFT);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiInventoryWheel(xui_widget pWidget, xui_inventory_grid_data_t* pData, const xui_event_t* pEvent)
{
	float fOldY;
	float fNewY;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	fOldY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, NULL, &fOldY);
	(void)xuiScrollModelScrollBy(&pData->tScroll, 0.0f, -pEvent->fWheelY * pData->tLayout.fWheelStep);
	fNewY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, NULL, &fNewY);
	if ( fNewY != fOldY ) {
		pData->iChangeCount++;
		return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER) | (int)XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiInventoryEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_inventory_grid_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiInventoryPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( !pData->bPointerDown ) return __xuiInventorySetHover(pWidget, pData, -1);
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiInventoryPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiInventoryPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT || pEvent->iButton == 0 ) {
			xui_inventory_hit_t tHit;
			(void)__xuiInventoryHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
			if ( tHit.iSlot >= 0 ) {
				__xuiInventoryNotifyActivate(pWidget, pData, tHit.iSlot, XUI_POINTER_BUTTON_LEFT);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		break;
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiInventoryContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiInventoryKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiInventoryWheel(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bPointerDown = 0;
		pData->bDragging = 0;
		pData->iActiveSlot = -1;
		pData->iDragSource = -1;
		pData->iDropTarget = -1;
		return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( pEvent->iType == XUI_EVENT_BOUNDS_CHANGED ) {
			xui_inventory_grid_data_t tResolved;
			__xuiInventoryResolve(pWidget, pData, &tResolved);
			(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
		}
		return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiInventoryContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;
	int iColumns;
	int iRows;

	(void)tConstraint;
	if ( (pWidget == NULL) || (pSize == NULL) || (pUser == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_inventory_grid_data_t*)pUser;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	iColumns = (tResolved.tLayout.iColumns > 0) ? tResolved.tLayout.iColumns : XUI_INVENTORY_DEFAULT_COLUMNS;
	if ( pData->iSlotCount > 0 && iColumns > pData->iSlotCount ) iColumns = pData->iSlotCount;
	if ( iColumns < 1 ) iColumns = 1;
	iRows = (pData->iSlotCount + iColumns - 1) / iColumns;
	if ( iRows < 1 ) iRows = XUI_INVENTORY_DEFAULT_VISIBLE_ROWS;
	if ( iRows > XUI_INVENTORY_DEFAULT_VISIBLE_ROWS ) iRows = XUI_INVENTORY_DEFAULT_VISIBLE_ROWS;
	pSize->fX = tResolved.tLayout.fPadding * 2.0f + (float)iColumns * tResolved.tLayout.fSlotSize + (float)(iColumns - 1) * tResolved.tLayout.fSlotGap;
	pSize->fY = tResolved.tLayout.fPadding * 2.0f + (float)iRows * tResolved.tLayout.fSlotSize + (float)(iRows - 1) * tResolved.tLayout.fSlotGap;
	pSize->fX = xuiInternalSnapSize(pSize->fX);
	pSize->fY = xuiInternalSnapSize(pSize->fY);
	return XUI_OK;
}

static int __xuiInventoryArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;

	(void)tContentRect;
	if ( (pWidget == NULL) || (pUser == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_inventory_grid_data_t*)pUser;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	return __xuiInventorySyncLayout(pWidget, pData, &tResolved);
}

static void __xuiInventoryDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiInventoryDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiInventoryResetSplitState(xui_inventory_grid_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iSplitSlot = -1;
	pData->iSplitMin = 0;
	pData->iSplitMax = 0;
	pData->iSplitCount = 0;
}

static int __xuiInventorySplitSlotAvailable(xui_inventory_grid_data_t* pData, int iSlot)
{
	xui_inventory_slot_t* pSlot;

	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) {
		return 0;
	}
	pSlot = &pData->arrSlots[iSlot];
	return ((pSlot->iFlags & XUI_INVENTORY_SLOT_EMPTY) == 0u) &&
	       __xuiInventorySlotEnabled(pSlot) &&
	       (pSlot->iCount > 1);
}

static int __xuiInventoryCloseSplitPopupInternal(xui_widget pWidget, xui_inventory_grid_data_t* pData)
{
	int iRet;

	(void)pWidget;
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = XUI_OK;
	if ( pData->pSplitPopup != NULL && xuiPopupIsOpen(pData->pSplitPopup) ) {
		iRet = xuiPopupSetOpen(pData->pSplitPopup, 0);
	}
	__xuiInventoryResetSplitState(pData);
	return iRet;
}

static int __xuiInventorySplitInputCount(xui_inventory_grid_data_t* pData)
{
	float fValue;
	int iValue;

	if ( pData == NULL ) {
		return 0;
	}
	if ( pData->pSplitInput != NULL ) {
		(void)xuiNumericInputCommit(pData->pSplitInput);
		fValue = xuiNumericInputGetValue(pData->pSplitInput);
		iValue = (int)floorf(fValue + 0.5f);
	} else {
		iValue = pData->iSplitCount;
	}
	return __xuiInventoryClampInt(iValue, pData->iSplitMin, pData->iSplitMax);
}

static int __xuiInventoryCommitSplitPopupInternal(xui_widget pWidget, xui_inventory_grid_data_t* pData)
{
	int iSlot;
	int iCount;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pSplitPopup == NULL) || !xuiPopupIsOpen(pData->pSplitPopup) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iSlot = pData->iSplitSlot;
	if ( !__xuiInventorySplitSlotAvailable(pData, iSlot) ) {
		return __xuiInventoryCloseSplitPopupInternal(pWidget, pData);
	}
	iCount = __xuiInventorySplitInputCount(pData);
	pData->iSplitCount = iCount;
	pData->iSplitCommitCount++;
	pData->iChangeCount++;
	if ( pData->onSplit != NULL ) {
		pData->onSplit(pWidget, iSlot, iCount, pData->pSplitUser);
	}
	return __xuiInventoryCloseSplitPopupInternal(pWidget, pData);
}

static void __xuiInventorySplitDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_widget pGrid;
	xui_inventory_grid_data_t* pData;

	(void)pWidget;
	pGrid = (xui_widget)pUser;
	pData = __xuiInventoryGetData(pGrid);
	if ( pData != NULL ) {
		(void)__xuiInventoryCommitSplitPopupInternal(pGrid, pData);
	}
}

static void __xuiInventorySplitCancelAction(xui_widget pWidget, void* pUser)
{
	xui_widget pGrid;
	xui_inventory_grid_data_t* pData;

	(void)pWidget;
	pGrid = (xui_widget)pUser;
	pData = __xuiInventoryGetData(pGrid);
	if ( pData != NULL ) {
		(void)__xuiInventoryCloseSplitPopupInternal(pGrid, pData);
	}
}

static void __xuiInventorySplitOkClick(xui_widget pWidget, void* pUser)
{
	__xuiInventorySplitDefaultAction(pWidget, pUser);
}

static void __xuiInventorySplitCancelClick(xui_widget pWidget, void* pUser)
{
	__xuiInventorySplitCancelAction(pWidget, pUser);
}

static void __xuiInventorySplitPopupChanged(xui_widget pWidget, int bOpen, void* pUser)
{
	xui_widget pGrid;
	xui_inventory_grid_data_t* pData;

	(void)pWidget;
	pGrid = (xui_widget)pUser;
	pData = __xuiInventoryGetData(pGrid);
	if ( (pData != NULL) && !bOpen ) {
		__xuiInventoryResetSplitState(pData);
	}
}

static int __xuiInventoryCreateSplitLabel(xui_widget pGrid, xui_inventory_grid_data_t* pData, xui_widget pContent)
{
	xui_label_desc_t tDesc;
	xui_widget pLabel;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = "Split stack";
	tDesc.pFont = pData->pFont;
	tDesc.iTextColor = XUI_COLOR_RGBA(45, 58, 78, 255);
	tDesc.iTextFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	iRet = xuiLabelCreate(xuiWidgetGetContext(pGrid), &pLabel, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pLabel, (xui_rect_t){10.0f, 8.0f, 200.0f, 18.0f});
	(void)xuiWidgetSetHitTestVisible(pLabel, 0);
	iRet = xuiWidgetAddChild(pContent, pLabel);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pLabel);
		return iRet;
	}
	pData->pSplitTitle = pLabel;
	return XUI_OK;
}

static int __xuiInventoryCreateSplitInput(xui_widget pGrid, xui_inventory_grid_data_t* pData, xui_widget pContent)
{
	xui_numeric_input_desc_t tDesc;
	xui_widget pInput;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = pData->pFont;
	tDesc.fMin = 1.0f;
	tDesc.fMax = 99.0f;
	tDesc.fStep = 1.0f;
	tDesc.fValue = 1.0f;
	tDesc.iPrecision = 0;
	tDesc.bInteger = 1;
	tDesc.bSpinnerVisible = 1;
	tDesc.iBackgroundColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	tDesc.iBorderColor = XUI_COLOR_RGBA(158, 188, 218, 255);
	tDesc.iFocusBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	tDesc.iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
	iRet = xuiNumericInputCreate(xuiWidgetGetContext(pGrid), &pInput, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pInput, (xui_rect_t){10.0f, 32.0f, 200.0f, 28.0f});
	iRet = xuiWidgetAddChild(pContent, pInput);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pInput);
		return iRet;
	}
	pData->pSplitInput = pInput;
	return XUI_OK;
}

static int __xuiInventoryCreateSplitButton(xui_widget pGrid, xui_widget pContent, const char* sText, xui_rect_t tRect, int bPrimary, xui_button_click_proc onClick, xui_widget* ppButton)
{
	xui_button_desc_t tDesc;
	xui_widget pButton;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = sText;
	tDesc.pFont = xuiGetDefaultFont(xuiWidgetGetContext(pGrid));
	tDesc.iTextFlags = XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	tDesc.fBorderWidth = 1.0f;
	if ( bPrimary ) {
		tDesc.iTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
		tDesc.iNormalColor = XUI_COLOR_RGBA(47, 128, 237, 255);
		tDesc.iHoverColor = XUI_COLOR_RGBA(62, 140, 243, 255);
		tDesc.iActiveColor = XUI_COLOR_RGBA(35, 112, 214, 255);
		tDesc.iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 255);
		tDesc.iBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	} else {
		tDesc.iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
		tDesc.iNormalColor = XUI_COLOR_RGBA(255, 255, 255, 255);
		tDesc.iHoverColor = XUI_COLOR_RGBA(240, 247, 253, 255);
		tDesc.iActiveColor = XUI_COLOR_RGBA(225, 238, 250, 255);
		tDesc.iFocusColor = XUI_COLOR_RGBA(235, 245, 255, 255);
		tDesc.iBorderColor = XUI_COLOR_RGBA(158, 188, 218, 255);
	}
	iRet = xuiButtonCreate(xuiWidgetGetContext(pGrid), &pButton, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pButton, tRect);
	(void)xuiButtonSetClick(pButton, onClick, pGrid);
	iRet = xuiWidgetAddChild(pContent, pButton);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pButton);
		return iRet;
	}
	if ( ppButton != NULL ) *ppButton = pButton;
	return XUI_OK;
}

static int __xuiInventoryCreateSplitPopup(xui_widget pWidget, xui_inventory_grid_data_t* pData)
{
	xui_popup_desc_t tPopupDesc;
	xui_widget pContent;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPopupDesc, 0, sizeof(tPopupDesc));
	tPopupDesc.iSize = sizeof(tPopupDesc);
	tPopupDesc.pOwner = pWidget;
	tPopupDesc.fContentWidth = 220.0f;
	tPopupDesc.fContentHeight = 96.0f;
	tPopupDesc.fPadding = 4.0f;
	tPopupDesc.fBorderWidth = 1.0f;
	tPopupDesc.fShadowSize = 4.0f;
	tPopupDesc.iAnchor = XUI_POPUP_ANCHOR_FIXED;
	tPopupDesc.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopupDesc.iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	tPopupDesc.iOwnerPolicy = XUI_POPUP_OWNER_CLOSE;
	tPopupDesc.iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	tPopupDesc.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	tPopupDesc.bConsumeInside = 1;
	tPopupDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopupDesc.iPanelColor = XUI_COLOR_RGBA(248, 251, 255, 255);
	tPopupDesc.iBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	tPopupDesc.iShadowColor = XUI_COLOR_RGBA(88, 121, 156, 72);
	tPopupDesc.iBackdropColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pSplitPopup, &tPopupDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetChange(pData->pSplitPopup, __xuiInventorySplitPopupChanged, pWidget);
	if ( iRet != XUI_OK ) goto fail;
	pContent = xuiPopupGetContentWidget(pData->pSplitPopup);
	if ( pContent == NULL ) {
		iRet = XUI_ERROR_NOT_INITIALIZED;
		goto fail;
	}
	(void)xuiWidgetSetLayoutType(pContent, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetDefaultAction(pContent, __xuiInventorySplitDefaultAction, pWidget);
	(void)xuiWidgetSetCancelAction(pContent, __xuiInventorySplitCancelAction, pWidget);
	iRet = __xuiInventoryCreateSplitLabel(pWidget, pData, pContent);
	if ( iRet != XUI_OK ) goto fail;
	iRet = __xuiInventoryCreateSplitInput(pWidget, pData, pContent);
	if ( iRet != XUI_OK ) goto fail;
	iRet = __xuiInventoryCreateSplitButton(pWidget, pContent, "Cancel", (xui_rect_t){84.0f, 68.0f, 58.0f, 24.0f}, 0, __xuiInventorySplitCancelClick, &pData->pSplitCancel);
	if ( iRet != XUI_OK ) goto fail;
	iRet = __xuiInventoryCreateSplitButton(pWidget, pContent, "OK", (xui_rect_t){152.0f, 68.0f, 58.0f, 24.0f}, 1, __xuiInventorySplitOkClick, &pData->pSplitOk);
	if ( iRet != XUI_OK ) goto fail;
	iRet = xuiPopupSetFocusPolicy(pData->pSplitPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pSplitInput);
	if ( iRet != XUI_OK ) goto fail;
	return XUI_OK;

fail:
	if ( pData->pSplitPopup != NULL ) {
		xuiWidgetDestroy(pData->pSplitPopup);
	}
	pData->pSplitPopup = NULL;
	pData->pSplitTitle = NULL;
	pData->pSplitInput = NULL;
	pData->pSplitOk = NULL;
	pData->pSplitCancel = NULL;
	return iRet;
}

static int __xuiInventoryInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiInventoryEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiInventoryEvent, NULL);
	return iRet;
}

static int __xuiInventorySetSlotCountInternal(xui_widget pWidget, xui_inventory_grid_data_t* pData, int iSlotCount)
{
	xui_inventory_slot_t* arrSlots;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (iSlotCount < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iSlotCount == pData->iSlotCount ) {
		return XUI_OK;
	}
	if ( iSlotCount == 0 ) {
		xrtFree(pData->arrSlots);
		pData->arrSlots = NULL;
		pData->iSlotCount = 0;
		pData->iCurrent = -1;
		pData->iHoverSlot = -1;
		pData->iActiveSlot = -1;
		pData->iAnchorSlot = -1;
		pData->iDragSource = -1;
		pData->iDropTarget = -1;
	} else {
		arrSlots = (xui_inventory_slot_t*)xrtRealloc(pData->arrSlots, sizeof(*arrSlots) * (size_t)iSlotCount);
		if ( arrSlots == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( iSlotCount > pData->iSlotCount ) {
			memset(arrSlots + pData->iSlotCount, 0, sizeof(*arrSlots) * (size_t)(iSlotCount - pData->iSlotCount));
			for ( i = pData->iSlotCount; i < iSlotCount; i++ ) {
				arrSlots[i].iSize = sizeof(arrSlots[i]);
				arrSlots[i].iSlotId = i;
				arrSlots[i].iIconTint = XUI_COLOR_WHITE;
				arrSlots[i].iAnimationTint = XUI_COLOR_WHITE;
				arrSlots[i].fAnimationScale = 1.0f;
				arrSlots[i].iFlags = XUI_INVENTORY_SLOT_EMPTY;
			}
		}
		pData->arrSlots = arrSlots;
		pData->iSlotCount = iSlotCount;
		if ( pData->iCurrent >= iSlotCount ) pData->iCurrent = -1;
		if ( pData->iHoverSlot >= iSlotCount ) pData->iHoverSlot = -1;
		if ( pData->iActiveSlot >= iSlotCount ) pData->iActiveSlot = -1;
		if ( pData->iAnchorSlot >= iSlotCount ) pData->iAnchorSlot = -1;
		if ( pData->iDragSource >= iSlotCount ) pData->iDragSource = -1;
		if ( pData->iDropTarget >= iSlotCount ) pData->iDropTarget = -1;
		if ( pData->iSplitSlot >= iSlotCount ) {
			(void)__xuiInventoryCloseSplitPopupInternal(pWidget, pData);
		}
	}
	pData->iChangeCount++;
	__xuiInventoryClearVisibleRange(&pData->tLastPaintRange);
	pData->iLastPaintSlotCount = 0;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiInventoryInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	const xui_inventory_grid_desc_t* pDesc;
	xui_context pContext;
	int i;
	int iRet;

	(void)pUser;
	pData = (xui_inventory_grid_data_t*)pTypeData;
	pDesc = (const xui_inventory_grid_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiInventoryDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pContext = xuiWidgetGetContext(pWidget);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	__xuiInventoryDefaultLayoutValues(&pData->tLayout);
	__xuiInventoryDefaultColors(&pData->tColors);
	__xuiInventoryDefaultGamepadProfile(&pData->tGamepadProfile);
	if ( (pDesc != NULL) && pDesc->bHasLayout ) {
		__xuiInventoryApplyLayout(&pData->tLayout, &pDesc->tLayout);
	}
	if ( (pDesc != NULL) && pDesc->bHasColors ) {
		__xuiInventoryApplyColors(&pData->tColors, &pDesc->tColors);
	}
	xuiScrollModelInit(&pData->tScroll);
	pData->iCurrent = -1;
	pData->iHoverSlot = -1;
	pData->iActiveSlot = -1;
	pData->iAnchorSlot = -1;
	pData->iDragSource = -1;
	pData->iDropTarget = -1;
	pData->iDropMode = XUI_INVENTORY_DROP_NONE;
	pData->bTooltipVisible = 1;
	pData->iTooltipSlot = -1;
	__xuiInventoryClearVisibleRange(&pData->tLastPaintRange);
	__xuiInventoryResetSplitState(pData);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	iRet = __xuiInventoryInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWidgetSetTooltipResolver(pWidget, __xuiInventoryTooltipResolve, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiInventoryCreateSplitPopup(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (pDesc != NULL) && (pDesc->iSlotCount > 0) ) {
		iRet = __xuiInventorySetSlotCountInternal(pWidget, pData, pDesc->iSlotCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->arrSlots != NULL ) {
			for ( i = 0; i < pDesc->iSlotCount; i++ ) {
				pData->arrSlots[i] = pDesc->arrSlots[i];
				__xuiInventoryNormalizeSlot(&pData->arrSlots[i]);
			}
		}
	}
	return XUI_OK;
}

static void __xuiInventoryDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_inventory_grid_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_inventory_grid_data_t*)pTypeData;
	if ( pData != NULL ) {
		(void)xuiWidgetClearTooltip(pWidget);
		if ( pData->pSplitPopup != NULL ) {
			(void)xuiPopupSetOpen(pData->pSplitPopup, 0);
			xuiWidgetDestroy(pData->pSplitPopup);
			pData->pSplitPopup = NULL;
		}
		if ( pData->arrSlots != NULL ) {
			xrtFree(pData->arrSlots);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiInventoryRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiInventoryRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.empty_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.locked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.quality_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.text.muted_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.count.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.hotkey.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.cooldown.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.durability.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.drag.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.drop.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.slot.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.icon.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.wheel.step", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.drag.threshold", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "inventory.columns", XUI_STYLE_VALUE_INT, iLayoutDirty, 0);
	__xuiInventoryRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static int __xuiInventoryXsonSetText(xvalue pTable, const char* sKey, const char* sValue)
{
	if ( sValue == NULL ) {
		return XUI_OK;
	}
	return xvoTableSetText(pTable, sKey, 0, (ptr)(void*)sValue, 0, FALSE) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiInventoryXsonSetInt(xvalue pTable, const char* sKey, int iValue)
{
	return xvoTableSetInt(pTable, sKey, 0, iValue) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiInventoryXsonSetFloat(xvalue pTable, const char* sKey, float fValue)
{
	return xvoTableSetFloat(pTable, sKey, 0, fValue) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiInventoryXsonSetBool(xvalue pTable, const char* sKey, int bValue)
{
	return xvoTableSetBool(pTable, sKey, 0, bValue ? TRUE : FALSE) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiInventoryXsonSetObject(xvalue pTable, const char* sKey, xvalue pChild)
{
	if ( pChild == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( !xvoTableSetValue(pTable, sKey, 0, pChild, TRUE) ) {
		xvoUnref(pChild);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiInventoryXsonSetColor(xvalue pTable, const char* sKey, uint32_t iColor)
{
	char sColor[16];

	snprintf(sColor, sizeof(sColor), "#%02X%02X%02X%02X",
		(unsigned int)((iColor >> 24) & 0xffu),
		(unsigned int)((iColor >> 16) & 0xffu),
		(unsigned int)((iColor >> 8) & 0xffu),
		(unsigned int)(iColor & 0xffu));
	sColor[sizeof(sColor) - 1u] = '\0';
	return __xuiInventoryXsonSetText(pTable, sKey, sColor);
}

static int __xuiInventoryXsonSetRect(xvalue pTable, const char* sKey, xui_rect_t tRect)
{
	xvalue pRect;
	int iRet;

	pRect = xvoCreateTable();
	if ( pRect == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiInventoryXsonSetFloat(pRect, "x", tRect.fX);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pRect, "y", tRect.fY);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pRect, "w", tRect.fW);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pRect, "h", tRect.fH);
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pTable, sKey, pRect);
		pRect = NULL;
	}
	if ( pRect != NULL ) {
		xvoUnref(pRect);
	}
	return iRet;
}

static int __xuiInventoryAppendSlotXValue(xvalue pSlots, int iSlot, const xui_inventory_slot_t* pSlot)
{
	xvalue pObj;
	xvalue pAnimation;
	int iRet;

	pObj = xvoCreateTable();
	if ( pObj == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiInventoryXsonSetInt(pObj, "index", iSlot);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "slotId", pSlot->iSlotId);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "itemId", pSlot->iItemId);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "count", pSlot->iCount);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "maxCount", pSlot->iMaxCount);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "slotType", pSlot->iSlotType);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "itemType", pSlot->iItemType);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pObj, "flags", (int)pSlot->iFlags);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pObj, "qualityColor", pSlot->iQualityColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pObj, "iconTint", pSlot->iIconTint);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetBool(pObj, "hasIcon", pSlot->pIcon != NULL);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetRect(pObj, "iconSrc", pSlot->tIconSrc);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pObj, "cooldownRate", pSlot->fCooldownRate);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pObj, "durabilityRate", pSlot->fDurabilityRate);
	if ( iRet == XUI_OK && pSlot->sText[0] != '\0' ) iRet = __xuiInventoryXsonSetText(pObj, "text", pSlot->sText);
	if ( iRet == XUI_OK && pSlot->sHotkey[0] != '\0' ) iRet = __xuiInventoryXsonSetText(pObj, "hotkey", pSlot->sHotkey);
	if ( iRet == XUI_OK && ((pSlot->pAnimation != NULL) || ((pSlot->iFlags & XUI_INVENTORY_SLOT_ANIMATION) != 0u) ||
	     (pSlot->iAnimationFlags != 0u) || (pSlot->fAnimationScale != 1.0f) || (pSlot->iAnimationTint != XUI_COLOR_WHITE)) ) {
		pAnimation = xvoCreateTable();
		if ( pAnimation == NULL ) {
			iRet = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			iRet = __xuiInventoryXsonSetBool(pAnimation, "hasObject", pSlot->pAnimation != NULL);
			if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pAnimation, "flags", (int)pSlot->iAnimationFlags);
			if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pAnimation, "scale", pSlot->fAnimationScale);
			if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pAnimation, "tint", pSlot->iAnimationTint);
			if ( iRet == XUI_OK ) {
				iRet = __xuiInventoryXsonSetObject(pObj, "animation", pAnimation);
				pAnimation = NULL;
			}
			if ( pAnimation != NULL ) {
				xvoUnref(pAnimation);
			}
		}
	}
	if ( iRet == XUI_OK ) {
		if ( !xvoArrayAppendValue(pSlots, pObj, TRUE) ) {
			iRet = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			pObj = NULL;
		}
	}
	if ( pObj != NULL ) {
		xvoUnref(pObj);
	}
	return iRet;
}

XUI_API xui_widget_type xuiInventoryGridGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_cache_policy_t tPolicy;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "inventorygrid");
	if ( pType != NULL ) {
		__xuiInventoryRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "inventorygrid";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_inventory_grid_data_t);
	tDesc.onInit = __xuiInventoryInit;
	tDesc.onDestroy = __xuiInventoryDestroy;
	tDesc.onContentMeasure = __xuiInventoryContentMeasure;
	tDesc.onLayoutArrange = __xuiInventoryArrange;
	tDesc.onCacheRender = __xuiInventoryCacheRender;
	__xuiInventoryDefaultLayout(&tDesc.tLayout);
	__xuiInventoryDefaultCachePolicy(&tPolicy);
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiInventoryRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiInventoryGridCreate(xui_context pContext, xui_widget* ppWidget, const xui_inventory_grid_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiInventoryDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiInventoryGridGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiInventoryGridSetSlotCount(xui_widget pWidget, int iSlotCount)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInventorySetSlotCountInternal(pWidget, pData, iSlotCount);
}

XUI_API int xuiInventoryGridGetSlotCount(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iSlotCount : 0;
}

XUI_API int xuiInventoryGridSetSlot(xui_widget pWidget, int iSlot, const xui_inventory_slot_t* pSlot)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_slot_t tSlot;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tSlot, 0, sizeof(tSlot));
	if ( pSlot != NULL ) {
		tSlot = *pSlot;
	}
	tSlot.iSlotId = (tSlot.iSlotId != 0) ? tSlot.iSlotId : iSlot;
	__xuiInventoryNormalizeSlot(&tSlot);
	pData->arrSlots[iSlot] = tSlot;
	if ( (pData->iSplitSlot == iSlot) && !__xuiInventorySplitSlotAvailable(pData, iSlot) ) {
		(void)__xuiInventoryCloseSplitPopupInternal(pWidget, pData);
	}
	pData->iChangeCount++;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridGetSlot(xui_widget pWidget, int iSlot, xui_inventory_slot_t* pSlot)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pSlot == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pSlot = pData->arrSlots[iSlot];
	return XUI_OK;
}

XUI_API int xuiInventoryGridClearSlot(xui_widget pWidget, int iSlot)
{
	xui_inventory_slot_t tSlot;

	if ( iSlot < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tSlot, 0, sizeof(tSlot));
	tSlot.iSize = sizeof(tSlot);
	tSlot.iSlotId = iSlot;
	tSlot.iIconTint = XUI_COLOR_WHITE;
	tSlot.iAnimationTint = XUI_COLOR_WHITE;
	tSlot.fAnimationScale = 1.0f;
	tSlot.iFlags = XUI_INVENTORY_SLOT_EMPTY;
	return xuiInventoryGridSetSlot(pWidget, iSlot, &tSlot);
}

XUI_API int xuiInventoryGridClearAll(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData;
	int i;

	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iSlotCount; i++ ) {
		(void)xuiInventoryGridClearSlot(pWidget, i);
	}
	pData->iCurrent = -1;
	pData->iHoverSlot = -1;
	pData->iActiveSlot = -1;
	pData->iAnchorSlot = -1;
	pData->iDragSource = -1;
	pData->iDropTarget = -1;
	pData->iChangeCount++;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridSetCurrent(xui_widget pWidget, int iSlot, int bNotify)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInventorySetCurrentInternal(pWidget, pData, iSlot, bNotify);
}

XUI_API int xuiInventoryGridGetCurrent(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iCurrent : -1;
}

XUI_API int xuiInventoryGridSetSelected(xui_widget pWidget, int iSlot, int bSelected, int bNotify)
{
	xui_inventory_grid_data_t* pData;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->tLayout.iSelectionMode == XUI_INVENTORY_SELECTION_SINGLE && bSelected ) {
		__xuiInventoryClearSelectionData(pData);
	}
	if ( bSelected ) {
		pData->arrSlots[iSlot].iFlags |= XUI_INVENTORY_SLOT_SELECTED;
		pData->iCurrent = iSlot;
	} else {
		pData->arrSlots[iSlot].iFlags &= ~XUI_INVENTORY_SLOT_SELECTED;
		if ( pData->iCurrent == iSlot ) pData->iCurrent = -1;
	}
	pData->iChangeCount++;
	(void)__xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) {
		__xuiInventoryNotifySelect(pWidget, pData, bSelected ? iSlot : -1);
	}
	return XUI_OK;
}

XUI_API int xuiInventoryGridGetSelected(xui_widget pWidget, int iSlot)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return 0;
	return __xuiInventorySelected(&pData->arrSlots[iSlot]);
}

XUI_API int xuiInventoryGridClearSelection(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiInventoryClearSelectionData(pData);
	pData->iCurrent = -1;
	pData->iChangeCount++;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridSetLayout(xui_widget pWidget, const xui_inventory_grid_layout_t* pLayout)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || !__xuiInventoryLayoutValid(pLayout) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiInventoryDefaultLayoutValues(&pData->tLayout);
	__xuiInventoryApplyLayout(&pData->tLayout, pLayout);
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridGetLayout(xui_widget pWidget, xui_inventory_grid_layout_t* pLayout)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pLayout == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pLayout = pData->tLayout;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = (pFont != NULL) ? pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( pData->pSplitTitle != NULL ) (void)xuiLabelSetFont(pData->pSplitTitle, pData->pFont);
	if ( pData->pSplitInput != NULL ) (void)xuiNumericInputSetFont(pData->pSplitInput, pData->pFont);
	if ( pData->pSplitOk != NULL ) (void)xuiButtonSetFont(pData->pSplitOk, pData->pFont);
	if ( pData->pSplitCancel != NULL ) (void)xuiButtonSetFont(pData->pSplitCancel, pData->pFont);
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiInventoryGridGetFont(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiInventoryGridSetMetrics(xui_widget pWidget, float fSlotSize, float fSlotGap, float fPadding, float fIconPadding, float fBorderWidth)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (fSlotSize <= 0.0f) || (fSlotGap < 0.0f) || (fPadding < 0.0f) || (fIconPadding < 0.0f) || (fBorderWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tLayout.fSlotSize = fSlotSize;
	pData->tLayout.fSlotGap = fSlotGap;
	pData->tLayout.fPadding = fPadding;
	pData->tLayout.fIconPadding = fIconPadding;
	pData->tLayout.fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridSetColors(xui_widget pWidget, const xui_inventory_grid_colors_t* pColors)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	pData->tColors.iSize = sizeof(pData->tColors);
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridGetColors(xui_widget pWidget, xui_inventory_grid_colors_t* pColors)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	return XUI_OK;
}

XUI_API xui_scroll_model_t* xuiInventoryGridGetScrollModel(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? &pData->tScroll : NULL;
}

XUI_API int xuiInventoryGridGetSlotRect(xui_widget pWidget, int iSlot, xui_rect_t* pRect)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pRect == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	*pRect = __xuiInventorySlotLocalRect(pWidget, pData, &tResolved, iSlot);
	return XUI_OK;
}

XUI_API int xuiInventoryGridHitTest(xui_widget pWidget, float fX, float fY, xui_inventory_hit_t* pHit)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInventoryHitWorld(pWidget, pData, fX, fY, pHit);
}

XUI_API int xuiInventoryGridEnsureSlotVisible(xui_widget pWidget, int iSlot)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;
	xui_rect_t tContent;
	float fOldX;
	float fOldY;
	float fNewX;
	float fNewY;

	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_OK;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	tContent = __xuiInventorySlotContentRect(pData, &tResolved, iSlot);
	fOldX = 0.0f;
	fOldY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fOldX, &fOldY);
	(void)xuiScrollModelEnsureRectVisible(&pData->tScroll, tContent);
	fNewX = 0.0f;
	fNewY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fNewX, &fNewY);
	if ( (fOldX != fNewX) || (fOldY != fNewY) ) {
		return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

XUI_API int xuiInventoryGridGetVisibleRange(xui_widget pWidget, xui_inventory_visible_range_t* pRange)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pRange == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	return __xuiInventoryComputeVisibleRange(pWidget, pData, &tResolved, pRange);
}

XUI_API int xuiInventoryGridGetLastPaintRange(xui_widget pWidget, xui_inventory_visible_range_t* pRange)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pRange == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pRange = pData->tLastPaintRange;
	return XUI_OK;
}

XUI_API int xuiInventoryGridGetLastPaintSlotCount(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iLastPaintSlotCount : 0;
}

XUI_API int xuiInventoryGridQuerySlots(xui_widget pWidget, const xui_inventory_slot_query_t* pQuery, int* arrSlots, int iSlotCapacity, int* pSlotCount)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_sort_context_t tSort;
	int* arrMatched;
	int i;
	int iMatched;
	int iCopyCount;
	int iSortMode;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pSlotCount == NULL) || (iSlotCapacity < 0) || ((iSlotCapacity > 0) && (arrSlots == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pQuery != NULL) && (pQuery->iSize != 0u) && (pQuery->iSize < sizeof(*pQuery)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pSlotCount = 0;
	if ( pData->iSlotCount <= 0 ) {
		return XUI_OK;
	}
	arrMatched = (int*)xrtMalloc(sizeof(*arrMatched) * (size_t)pData->iSlotCount);
	if ( arrMatched == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iMatched = 0;
	for ( i = 0; i < pData->iSlotCount; i++ ) {
		if ( __xuiInventoryQueryMatches(pWidget, pData, pQuery, i) ) {
			arrMatched[iMatched++] = i;
		}
	}
	if ( iMatched > 1 ) {
		iSortMode = (pQuery != NULL) ? pQuery->iSortMode : XUI_INVENTORY_SORT_NONE;
		if ( (iSortMode == XUI_INVENTORY_SORT_NONE) && (pQuery != NULL) && (pQuery->onCompare != NULL) ) {
			iSortMode = XUI_INVENTORY_SORT_CUSTOM;
		}
		if ( (iSortMode != XUI_INVENTORY_SORT_NONE) || ((pQuery != NULL) && ((pQuery->iFlags & XUI_INVENTORY_QUERY_SORT_DESCENDING) != 0u)) ) {
			memset(&tSort, 0, sizeof(tSort));
			tSort.pWidget = pWidget;
			tSort.pData = pData;
			tSort.iSortMode = iSortMode;
			tSort.iFlags = (pQuery != NULL) ? pQuery->iFlags : 0u;
			tSort.onCompare = (pQuery != NULL) ? pQuery->onCompare : NULL;
			tSort.pCompareUser = (pQuery != NULL) ? pQuery->pCompareUser : NULL;
			__xuiInventorySortIndices(&tSort, arrMatched, 0, iMatched - 1);
		}
	}
	*pSlotCount = iMatched;
	iCopyCount = (iMatched < iSlotCapacity) ? iMatched : iSlotCapacity;
	for ( i = 0; i < iCopyCount; i++ ) {
		arrSlots[i] = arrMatched[i];
	}
	xrtFree(arrMatched);
	return XUI_OK;
}

XUI_API int xuiInventoryGridSortSlots(xui_widget pWidget, int* arrSlots, int iSlotCount, int iSortMode, uint32_t iFlags, xui_inventory_compare_proc onCompare, void* pUser)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_sort_context_t tSort;
	int i;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlotCount < 0) || ((iSlotCount > 0) && (arrSlots == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iSlotCount; i++ ) {
		if ( (arrSlots[i] < 0) || (arrSlots[i] >= pData->iSlotCount) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	if ( iSlotCount <= 1 ) {
		return XUI_OK;
	}
	if ( (iSortMode == XUI_INVENTORY_SORT_NONE) && (onCompare != NULL) ) {
		iSortMode = XUI_INVENTORY_SORT_CUSTOM;
	}
	if ( (iSortMode == XUI_INVENTORY_SORT_NONE) && ((iFlags & XUI_INVENTORY_QUERY_SORT_DESCENDING) == 0u) ) {
		return XUI_OK;
	}
	memset(&tSort, 0, sizeof(tSort));
	tSort.pWidget = pWidget;
	tSort.pData = pData;
	tSort.iSortMode = iSortMode;
	tSort.iFlags = iFlags;
	tSort.onCompare = onCompare;
	tSort.pCompareUser = pUser;
	__xuiInventorySortIndices(&tSort, arrSlots, 0, iSlotCount - 1);
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetGamepadProfile(xui_widget pWidget, const xui_inventory_gamepad_profile_t* pProfile)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_gamepad_profile_t tProfile;

	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pProfile == NULL ) {
		__xuiInventoryDefaultGamepadProfile(&pData->tGamepadProfile);
		return XUI_OK;
	}
	if ( (pProfile->iSize != 0u) && (pProfile->iSize < sizeof(*pProfile)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tProfile, 0, sizeof(tProfile));
	tProfile = *pProfile;
	tProfile.iSize = sizeof(tProfile);
	pData->tGamepadProfile = tProfile;
	return XUI_OK;
}

XUI_API int xuiInventoryGridGetGamepadProfile(xui_widget pWidget, xui_inventory_gamepad_profile_t* pProfile)
{
	xui_inventory_grid_data_t* pData;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (pProfile == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pProfile = pData->tGamepadProfile;
	pProfile->iSize = sizeof(*pProfile);
	return XUI_OK;
}

XUI_API int xuiInventoryGridGamepadButton(xui_widget pWidget, int iButton, int bPressed, uint32_t iModifiers)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_grid_data_t tResolved;
	xui_inventory_gamepad_profile_t* pProfile;
	xui_rect_t tViewport;
	int iRows;
	int iDelta;
	int iTarget;
	int bSkipDisabled;

	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iButton == 0) || !bPressed ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	__xuiInventoryResolve(pWidget, pData, &tResolved);
	(void)__xuiInventorySyncLayout(pWidget, pData, &tResolved);
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	pProfile = &pData->tGamepadProfile;
	if ( iButton == pProfile->iLeftButton ) {
		return __xuiInventoryGamepadMoveHorizontal(pWidget, pData, -1, iModifiers);
	}
	if ( iButton == pProfile->iRightButton ) {
		return __xuiInventoryGamepadMoveHorizontal(pWidget, pData, 1, iModifiers);
	}
	if ( iButton == pProfile->iUpButton ) {
		return __xuiInventoryGamepadMoveVertical(pWidget, pData, -pData->iResolvedColumns, iModifiers);
	}
	if ( iButton == pProfile->iDownButton ) {
		return __xuiInventoryGamepadMoveVertical(pWidget, pData, pData->iResolvedColumns, iModifiers);
	}
	if ( iButton == pProfile->iPageUpButton || iButton == pProfile->iPageDownButton ) {
		tViewport = xuiWidgetGetContentRect(pWidget);
		iRows = (int)(tViewport.fH / (tResolved.tLayout.fSlotSize + tResolved.tLayout.fSlotGap));
		if ( iRows < 1 ) iRows = 1;
		iDelta = iRows * pData->iResolvedColumns;
		if ( iButton == pProfile->iPageUpButton ) iDelta = -iDelta;
		return __xuiInventoryGamepadMoveLinear(pWidget, pData, iDelta, iModifiers);
	}
	bSkipDisabled = ((pProfile->iFlags & XUI_INVENTORY_GAMEPAD_SKIP_DISABLED) != 0u);
	if ( iButton == pProfile->iHomeButton ) {
		iTarget = __xuiInventoryGamepadFindTarget(pData, 0, 1, 0, pData->iSlotCount - 1, 0, bSkipDisabled);
		return __xuiInventoryGamepadSetTarget(pWidget, pData, iTarget, iModifiers);
	}
	if ( iButton == pProfile->iEndButton ) {
		iTarget = __xuiInventoryGamepadFindTarget(pData, pData->iSlotCount - 1, -1, 0, pData->iSlotCount - 1, 0, bSkipDisabled);
		return __xuiInventoryGamepadSetTarget(pWidget, pData, iTarget, iModifiers);
	}
	if ( iButton == pProfile->iAcceptButton ) {
		if ( (pData->pSplitPopup != NULL) && xuiPopupIsOpen(pData->pSplitPopup) ) {
			return __xuiInventoryCommitSplitPopupInternal(pWidget, pData);
		}
		if ( (pData->iCurrent >= 0) && (pData->iCurrent < pData->iSlotCount) && __xuiInventorySlotEnabled(&pData->arrSlots[pData->iCurrent]) ) {
			__xuiInventoryNotifyActivate(pWidget, pData, pData->iCurrent, XUI_POINTER_BUTTON_LEFT);
		}
		return XUI_OK;
	}
	if ( iButton == pProfile->iContextButton ) {
		return __xuiInventoryGamepadOpenContext(pWidget, pData);
	}
	if ( iButton == pProfile->iCancelButton ) {
		if ( (pData->pSplitPopup != NULL) && xuiPopupIsOpen(pData->pSplitPopup) ) {
			return __xuiInventoryCloseSplitPopupInternal(pWidget, pData);
		}
		return XUI_OK;
	}
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetSelectCallback(xui_widget pWidget, xui_inventory_select_proc onSelect, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetActivateCallback(xui_widget pWidget, xui_inventory_activate_proc onActivate, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onActivate = onActivate;
	pData->pActivateUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetContextCallback(xui_widget pWidget, xui_inventory_context_proc onContext, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onContext = onContext;
	pData->pContextUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetDragCallback(xui_widget pWidget, xui_inventory_drag_proc onDrag, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onDrag = onDrag;
	pData->pDragUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetDropCallback(xui_widget pWidget, xui_inventory_drop_proc onDrop, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onDrop = onDrop;
	pData->pDropUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetSplitCallback(xui_widget pWidget, xui_inventory_split_proc onSplit, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSplit = onSplit;
	pData->pSplitUser = pUser;
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetTooltipVisible(xui_widget pWidget, int bVisible)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	xui_context pContext;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bTooltipVisible = bVisible ? 1 : 0;
	if ( !pData->bTooltipVisible ) {
		pData->iTooltipSlot = -1;
		pContext = xuiWidgetGetContext(pWidget);
		if ( (pContext != NULL) && (xuiWidgetTooltipGetOwner(pContext) == pWidget) ) {
			xuiInternalTooltipCancel(pContext);
		}
	}
	return XUI_OK;
}

XUI_API int xuiInventoryGridGetTooltipVisible(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->bTooltipVisible : 0;
}

XUI_API int xuiInventoryGridSetTooltipCallback(xui_widget pWidget, xui_inventory_tooltip_proc onTooltip, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	xui_context pContext;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onTooltip = onTooltip;
	pData->pTooltipUser = pUser;
	pContext = xuiWidgetGetContext(pWidget);
	if ( (pContext != NULL) && (xuiWidgetTooltipGetOwner(pContext) == pWidget) ) {
		xuiInternalTooltipCancel(pContext);
	}
	return XUI_OK;
}

XUI_API int xuiInventoryGridSetRenderCallback(xui_widget pWidget, xui_inventory_slot_render_proc onRender, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRenderSlot = onRender;
	pData->pRenderSlotUser = pUser;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridSetAnimationRenderCallback(xui_widget pWidget, xui_inventory_animation_render_proc onRender, void* pUser)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRenderAnimation = onRender;
	pData->pRenderAnimationUser = pUser;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiInventoryGridOpenSplitPopup(xui_widget pWidget, int iSlot, float fX, float fY)
{
	xui_inventory_grid_data_t* pData;
	xui_inventory_slot_t* pSlot;
	xui_rect_t tSlotRect;
	xui_rect_t tWorld;
	char sTitle[96];
	int iDefault;
	int iRet;

	pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || !__xuiInventorySplitSlotAvailable(pData, iSlot) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pSplitPopup == NULL ) {
		iRet = __xuiInventoryCreateSplitPopup(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	pSlot = &pData->arrSlots[iSlot];
	pData->iSplitSlot = iSlot;
	pData->iSplitMin = 1;
	pData->iSplitMax = pSlot->iCount - 1;
	iDefault = pSlot->iCount / 2;
	if ( iDefault < 1 ) iDefault = 1;
	pData->iSplitCount = __xuiInventoryClampInt(iDefault, pData->iSplitMin, pData->iSplitMax);
	if ( pData->pSplitTitle != NULL ) {
		snprintf(sTitle, sizeof(sTitle), "Split %s", (pSlot->sText[0] != '\0') ? pSlot->sText : "stack");
		sTitle[sizeof(sTitle) - 1u] = '\0';
		(void)xuiLabelSetText(pData->pSplitTitle, sTitle);
	}
	if ( pData->pSplitInput != NULL ) {
		iRet = xuiNumericInputSetRange(pData->pSplitInput, (float)pData->iSplitMin, (float)pData->iSplitMax);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiNumericInputSetValue(pData->pSplitInput, (float)pData->iSplitCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( !__xuiInventoryFloatValid(fX) || !__xuiInventoryFloatValid(fY) ) {
		tWorld = xuiWidgetGetWorldRect(pWidget);
		if ( xuiInventoryGridGetSlotRect(pWidget, iSlot, &tSlotRect) == XUI_OK ) {
			fX = tWorld.fX + tSlotRect.fX + tSlotRect.fW;
			fY = tWorld.fY + tSlotRect.fY;
		} else {
			fX = tWorld.fX;
			fY = tWorld.fY;
		}
	}
	iRet = xuiPopupSetOwner(pData->pSplitPopup, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetAnchor(pData->pSplitPopup, XUI_POPUP_ANCHOR_FIXED);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetDirection(pData->pSplitPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetGap(pData->pSplitPopup, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetAnchorRect(pData->pSplitPopup, (xui_rect_t){fX, fY, 0.0f, 0.0f});
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusPolicy(pData->pSplitPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pSplitInput);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusRestore(pData->pSplitPopup, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetOpen(pData->pSplitPopup, 1);
	if ( iRet == XUI_OK && pData->pSplitInput != NULL ) {
		xui_widget pInnerInput;
		pInnerInput = xuiNumericInputGetInputWidget(pData->pSplitInput);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), (pInnerInput != NULL) ? pInnerInput : pData->pSplitInput);
	}
	return iRet;
}

XUI_API int xuiInventoryGridCommitSplitPopup(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInventoryCommitSplitPopupInternal(pWidget, pData);
}

XUI_API int xuiInventoryGridCloseSplitPopup(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiInventoryCloseSplitPopupInternal(pWidget, pData);
}

XUI_API int xuiInventoryGridIsSplitPopupOpen(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL && pData->pSplitPopup != NULL) ? xuiPopupIsOpen(pData->pSplitPopup) : 0;
}

XUI_API xui_widget xuiInventoryGridGetSplitPopupWidget(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->pSplitPopup : NULL;
}

XUI_API xui_widget xuiInventoryGridGetSplitInputWidget(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->pSplitInput : NULL;
}

XUI_API int xuiInventoryGridGetSplitSlot(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iSplitSlot : -1;
}

XUI_API int xuiInventoryGridGetSplitCount(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iSplitCount : 0;
}

XUI_API int xuiInventoryGridSetSlotAnimation(xui_widget pWidget, int iSlot, xui_animation_object_t* pAnimation, uint32_t iFlags, float fScale, uint32_t iTint)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSlots[iSlot].pAnimation = pAnimation;
	pData->arrSlots[iSlot].iAnimationFlags = iFlags;
	pData->arrSlots[iSlot].fAnimationScale = (fScale > 0.0f) ? fScale : 1.0f;
	pData->arrSlots[iSlot].iAnimationTint = (iTint != 0u) ? iTint : XUI_COLOR_WHITE;
	if ( pAnimation != NULL ) pData->arrSlots[iSlot].iFlags |= XUI_INVENTORY_SLOT_ANIMATION;
	else pData->arrSlots[iSlot].iFlags &= ~XUI_INVENTORY_SLOT_ANIMATION;
	pData->iChangeCount++;
	return __xuiInventoryInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_animation_object_t* xuiInventoryGridGetSlotAnimation(xui_widget pWidget, int iSlot, uint32_t* pFlags, float* pScale, uint32_t* pTint)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	if ( (pData == NULL) || (iSlot < 0) || (iSlot >= pData->iSlotCount) ) return NULL;
	if ( pFlags != NULL ) *pFlags = pData->arrSlots[iSlot].iAnimationFlags;
	if ( pScale != NULL ) *pScale = pData->arrSlots[iSlot].fAnimationScale;
	if ( pTint != NULL ) *pTint = pData->arrSlots[iSlot].iAnimationTint;
	return pData->arrSlots[iSlot].pAnimation;
}

XUI_API int xuiInventoryGridClearSlotAnimation(xui_widget pWidget, int iSlot)
{
	return xuiInventoryGridSetSlotAnimation(pWidget, iSlot, NULL, 0u, 1.0f, XUI_COLOR_WHITE);
}

XUI_API int xuiInventoryGridGetTooltipSlot(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iTooltipSlot : -1;
}

XUI_API int xuiInventoryGridGetHoverSlot(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iHoverSlot : -1;
}

XUI_API int xuiInventoryGridGetActiveSlot(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iActiveSlot : -1;
}

XUI_API int xuiInventoryGridGetDragSource(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iDragSource : -1;
}

XUI_API int xuiInventoryGridGetDropTarget(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iDropTarget : -1;
}

XUI_API int xuiInventoryGridGetChangeCount(xui_widget pWidget)
{
	xui_inventory_grid_data_t* pData = __xuiInventoryGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiInventoryGridToXValue(xui_widget pWidget, xvalue* ppValue)
{
	xui_inventory_grid_data_t* pData;
	xvalue pRoot;
	xvalue pLayout;
	xvalue pColors;
	xvalue pState;
	xvalue pGamepad;
	xvalue pSlots;
	float fScrollX;
	float fScrollY;
	int i;
	int iRet;

	if ( ppValue == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppValue = NULL;
	pData = __xuiInventoryGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pRoot = xvoCreateTable();
	pLayout = xvoCreateTable();
	pColors = xvoCreateTable();
	pState = xvoCreateTable();
	pGamepad = xvoCreateTable();
	pSlots = xvoCreateArray();
	if ( (pRoot == NULL) || (pLayout == NULL) || (pColors == NULL) || (pState == NULL) || (pGamepad == NULL) || (pSlots == NULL) ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		if ( pLayout != NULL ) xvoUnref(pLayout);
		if ( pColors != NULL ) xvoUnref(pColors);
		if ( pState != NULL ) xvoUnref(pState);
		if ( pGamepad != NULL ) xvoUnref(pGamepad);
		if ( pSlots != NULL ) xvoUnref(pSlots);
		return XUI_ERROR_OUT_OF_MEMORY;
	}

	iRet = __xuiInventoryXsonSetText(pRoot, "kind", "xui.inventorygrid");
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pRoot, "version", 1);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetText(pRoot, "format", "xui.inventorygrid.v1");

	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pLayout, "columns", pData->tLayout.iColumns);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pLayout, "selectionMode", pData->tLayout.iSelectionMode);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "slotSize", pData->tLayout.fSlotSize);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "slotGap", pData->tLayout.fSlotGap);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "padding", pData->tLayout.fPadding);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "iconPadding", pData->tLayout.fIconPadding);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "borderWidth", pData->tLayout.fBorderWidth);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "wheelStep", pData->tLayout.fWheelStep);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pLayout, "dragThreshold", pData->tLayout.fDragThreshold);

	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "background", pData->tColors.iBackgroundColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "slot", pData->tColors.iSlotColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "empty", pData->tColors.iEmptyColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "hover", pData->tColors.iHoverColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "active", pData->tColors.iActiveColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "selected", pData->tColors.iSelectedColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "disabled", pData->tColors.iDisabledColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "locked", pData->tColors.iLockedColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "border", pData->tColors.iBorderColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "quality", pData->tColors.iQualityColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "focus", pData->tColors.iFocusColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "text", pData->tColors.iTextColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "mutedText", pData->tColors.iMutedTextColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "count", pData->tColors.iCountColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "hotkey", pData->tColors.iHotkeyColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "cooldown", pData->tColors.iCooldownColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "durability", pData->tColors.iDurabilityColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "drag", pData->tColors.iDragColor);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetColor(pColors, "drop", pData->tColors.iDropColor);

	fScrollX = 0.0f;
	fScrollY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fScrollX, &fScrollY);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "slotCount", pData->iSlotCount);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "current", pData->iCurrent);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "hoverSlot", pData->iHoverSlot);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "activeSlot", pData->iActiveSlot);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "anchorSlot", pData->iAnchorSlot);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "dragSource", pData->iDragSource);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "dropTarget", pData->iDropTarget);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "dropMode", pData->iDropMode);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "changeCount", pData->iChangeCount);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "resolvedColumns", pData->iResolvedColumns);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "resolvedRows", pData->iResolvedRows);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pState, "contentWidth", pData->fContentWidth);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pState, "contentHeight", pData->fContentHeight);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pState, "scrollX", fScrollX);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetFloat(pState, "scrollY", fScrollY);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetBool(pState, "tooltipVisible", pData->bTooltipVisible);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pState, "tooltipSlot", pData->iTooltipSlot);

	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "flags", (int)pData->tGamepadProfile.iFlags);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "accept", pData->tGamepadProfile.iAcceptButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "cancel", pData->tGamepadProfile.iCancelButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "context", pData->tGamepadProfile.iContextButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "left", pData->tGamepadProfile.iLeftButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "right", pData->tGamepadProfile.iRightButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "up", pData->tGamepadProfile.iUpButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "down", pData->tGamepadProfile.iDownButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "pageUp", pData->tGamepadProfile.iPageUpButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "pageDown", pData->tGamepadProfile.iPageDownButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "home", pData->tGamepadProfile.iHomeButton);
	if ( iRet == XUI_OK ) iRet = __xuiInventoryXsonSetInt(pGamepad, "end", pData->tGamepadProfile.iEndButton);

	for ( i = 0; (iRet == XUI_OK) && (i < pData->iSlotCount); i++ ) {
		iRet = __xuiInventoryAppendSlotXValue(pSlots, i, &pData->arrSlots[i]);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pRoot, "layout", pLayout);
		pLayout = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pRoot, "colors", pColors);
		pColors = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pRoot, "state", pState);
		pState = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pRoot, "gamepad", pGamepad);
		pGamepad = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInventoryXsonSetObject(pRoot, "slots", pSlots);
		pSlots = NULL;
	}
	if ( iRet == XUI_OK ) {
		*ppValue = pRoot;
		pRoot = NULL;
	}
	if ( pRoot != NULL ) xvoUnref(pRoot);
	if ( pLayout != NULL ) xvoUnref(pLayout);
	if ( pColors != NULL ) xvoUnref(pColors);
	if ( pState != NULL ) xvoUnref(pState);
	if ( pGamepad != NULL ) xvoUnref(pGamepad);
	if ( pSlots != NULL ) xvoUnref(pSlots);
	return iRet;
}

XUI_API int xuiInventoryGridExportXSON(xui_widget pWidget, char* sBuffer, int iCapacity)
{
	xvalue pValue;
	str sText;
	size_t iSize;
	size_t iCopy;
	int iNeed;
	int iRet;

	if ( iCapacity < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiInventoryGridToXValue(pWidget, &pValue);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iSize = 0u;
	sText = xrtStringifyXSON(pValue, TRUE, 0, &iSize);
	xvoUnref(pValue);
	if ( sText == NULL ) {
		return XUI_ERROR_RESOURCE_FAILED;
	}
	if ( iSize > (size_t)2147483646u ) {
		xrtFree(sText);
		return XUI_ERROR_BUFFER_TOO_SMALL;
	}
	iNeed = (int)iSize + 1;
	if ( (sBuffer != NULL) && (iCapacity > 0) ) {
		iCopy = (iSize < (size_t)(iCapacity - 1)) ? iSize : (size_t)(iCapacity - 1);
		if ( iCopy > 0u ) {
			memcpy(sBuffer, sText, iCopy);
		}
		sBuffer[iCopy] = '\0';
	}
	xrtFree(sText);
	return iNeed;
}

XUI_API int xuiInventoryGridSaveXSONFile(xui_widget pWidget, const char* sPath)
{
	xvalue pValue;
	int iRet;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiInventoryGridToXValue(pWidget, &pValue);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xrtStringifyXSON_File((str)(void*)sPath, pValue, TRUE, 0) ? XUI_OK : XUI_ERROR_RESOURCE_FAILED;
	xvoUnref(pValue);
	return iRet;
}
