#include "xui_internal.h"

#include <stdio.h>
#include <string.h>

#define XUI_CASCADER_DEFAULT_ITEM_H	32.0f
#define XUI_CASCADER_DEFAULT_COLUMN_W	120.0f
#define XUI_CASCADER_DEFAULT_POPUP_MAX_H	224.0f

typedef struct xui_cascader_data_t {
	xui_widget pPopup;
	xui_widget pPanel;
	xui_font pFont;
	xui_cascader_item_t arrItems[XUI_CASCADER_ITEM_CAPACITY];
	xui_cascader_change_proc onChange;
	void* pChangeUser;
	int iItemCount;
	int arrSelected[XUI_CASCADER_PATH_CAPACITY];
	int iSelectedDepth;
	int iSelectedLeaf;
	int arrActive[XUI_CASCADER_PATH_CAPACITY];
	int iActiveDepth;
	int arrColumnScroll[XUI_CASCADER_PATH_CAPACITY];
	int iHoverColumn;
	int iHoverItem;
	int bShowAllLevels;
	int bClearable;
	int bSelectAnyLevel;
	int iExpandTrigger;
	int iPopupPlacement;
	int iChangeCount;
	char sDisplay[512];
	char sPlaceholder[128];
	char sSeparator[24];
	xui_rect_t tTextRect;
	xui_rect_t tButtonRect;
	xui_rect_t tClearRect;
	float fItemHeight;
	float fColumnWidth;
	float fPopupHeight;
	float fPopupMaxHeight;
	float fResolvedPopupWidth;
	float fResolvedPopupHeight;
	uint32_t iTextColor;
	uint32_t iPlaceholderColor;
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
	uint32_t iPopupTextColor;
	uint32_t iPopupMutedTextColor;
	uint32_t iPopupHoverColor;
	uint32_t iPopupActiveColor;
	uint32_t iPopupSelectedColor;
	uint32_t iPopupActiveTextColor;
	uint32_t iPopupDisabledTextColor;
	uint32_t iPopupSeparatorColor;
	float fBorderWidth;
} xui_cascader_data_t;

static xui_cascader_data_t* __xuiCascaderGetData(xui_widget pWidget);
static int __xuiCascaderApplyPopupStyle(xui_widget pWidget, xui_cascader_data_t* pData);
static int __xuiCascaderSyncState(xui_widget pWidget, xui_cascader_data_t* pData);
static int __xuiCascaderCloseInternal(xui_widget pWidget, xui_cascader_data_t* pData);

static int __xuiCascaderAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiCascaderColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static float __xuiCascaderMax(float a, float b)
{
	return (a > b) ? a : b;
}

static float __xuiCascaderMin(float a, float b)
{
	return (a < b) ? a : b;
}

static const char* __xuiCascaderText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static int __xuiCascaderRectContains(xui_rect_t r, float x, float y)
{
	return (x >= r.fX) && (y >= r.fY) && (x < r.fX + r.fW) && (y < r.fY + r.fH);
}

static void __xuiCascaderCopyText(char* sDst, int iCapacity, const char* sSrc)
{
	if ( (sDst == NULL) || (iCapacity <= 0) ) return;
	if ( sSrc == NULL ) sSrc = "";
	snprintf(sDst, (size_t)iCapacity, "%s", sSrc);
	sDst[iCapacity - 1] = '\0';
}

static int __xuiCascaderDescValid(const xui_cascader_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( pDesc->iItemCount < 0 ) return 0;
	if ( (pDesc->iSelectedDepth < 0) || (pDesc->iSelectedDepth > XUI_CASCADER_PATH_CAPACITY) ) return 0;
	if ( (pDesc->fItemHeight < 0.0f) || (pDesc->fColumnWidth < 0.0f) ||
	     (pDesc->fPopupHeight < 0.0f) || (pDesc->fPopupMaxHeight < 0.0f) ||
	     (pDesc->fBorderWidth < 0.0f) ) return 0;
	return 1;
}

static int __xuiCascaderPlacementValid(int iPlacement)
{
	return (iPlacement == XUI_CASCADER_POPUP_AUTO) ||
	       (iPlacement == XUI_CASCADER_POPUP_BOTTOM) ||
	       (iPlacement == XUI_CASCADER_POPUP_TOP);
}

static int __xuiCascaderExpandTriggerValid(int iTrigger)
{
	return (iTrigger == XUI_CASCADER_EXPAND_CLICK) || (iTrigger == XUI_CASCADER_EXPAND_HOVER);
}

static int __xuiCascaderStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiCascaderStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiCascaderStyleFont(xui_widget pWidget, xui_font pBaseFont)
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
		if ( pFont != NULL ) return pFont;
	}
	return pBaseFont;
}

static void __xuiCascaderDefaults(xui_cascader_data_t* pData)
{
	int i;

	memset(pData, 0, sizeof(*pData));
	for ( i = 0; i < XUI_CASCADER_PATH_CAPACITY; i++ ) {
		pData->arrSelected[i] = -1;
		pData->arrActive[i] = -1;
	}
	pData->iSelectedLeaf = -1;
	pData->iHoverColumn = -1;
	pData->iHoverItem = -1;
	pData->bShowAllLevels = 1;
	pData->iExpandTrigger = XUI_CASCADER_EXPAND_CLICK;
	pData->iPopupPlacement = XUI_CASCADER_POPUP_AUTO;
	pData->fItemHeight = XUI_CASCADER_DEFAULT_ITEM_H;
	pData->fColumnWidth = XUI_CASCADER_DEFAULT_COLUMN_W;
	pData->fPopupMaxHeight = XUI_CASCADER_DEFAULT_POPUP_MAX_H;
	pData->iTextColor = XUI_COLOR_RGBA(36, 52, 72, 255);
	pData->iPlaceholderColor = XUI_COLOR_RGBA(132, 146, 162, 225);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 190);
	pData->iBackgroundColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pData->iHoverBackgroundColor = XUI_COLOR_RGBA(238, 247, 252, 255);
	pData->iOpenBackgroundColor = XUI_COLOR_RGBA(246, 252, 250, 255);
	pData->iDisabledBackgroundColor = XUI_COLOR_RGBA(235, 240, 245, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(182, 204, 224, 255);
	pData->iHoverBorderColor = XUI_COLOR_RGBA(94, 180, 142, 255);
	pData->iFocusBorderColor = XUI_COLOR_RGBA(16, 185, 129, 255);
	pData->iArrowColor = XUI_COLOR_RGBA(62, 88, 112, 255);
	pData->iDisabledArrowColor = XUI_COLOR_RGBA(148, 160, 174, 180);
	pData->iButtonColor = XUI_COLOR_RGBA(246, 250, 253, 255);
	pData->iButtonHoverColor = XUI_COLOR_RGBA(230, 246, 238, 255);
	pData->iButtonOpenColor = XUI_COLOR_RGBA(220, 242, 232, 255);
	pData->iPopupPanelColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iPopupBorderColor = XUI_COLOR_RGBA(219, 228, 236, 255);
	pData->iPopupShadowColor = XUI_COLOR_RGBA(16, 35, 54, 38);
	pData->iPopupTextColor = XUI_COLOR_RGBA(28, 42, 58, 255);
	pData->iPopupMutedTextColor = XUI_COLOR_RGBA(86, 106, 126, 255);
	pData->iPopupHoverColor = XUI_COLOR_RGBA(238, 249, 244, 255);
	pData->iPopupActiveColor = XUI_COLOR_RGBA(16, 185, 129, 255);
	pData->iPopupSelectedColor = XUI_COLOR_RGBA(226, 247, 238, 255);
	pData->iPopupActiveTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iPopupDisabledTextColor = XUI_COLOR_RGBA(150, 162, 174, 185);
	pData->iPopupSeparatorColor = XUI_COLOR_RGBA(226, 233, 240, 255);
	pData->fBorderWidth = 1.0f;
	__xuiCascaderCopyText(pData->sPlaceholder, (int)sizeof(pData->sPlaceholder), "Select");
	__xuiCascaderCopyText(pData->sSeparator, (int)sizeof(pData->sSeparator), " / ");
}

static void __xuiCascaderApplyDesc(xui_cascader_data_t* pData, const xui_cascader_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) return;
	pData->pFont = pDesc->pFont;
	if ( pDesc->bShowLastLevelOnly ) pData->bShowAllLevels = 0;
	if ( pDesc->bClearable ) pData->bClearable = 1;
	if ( pDesc->bSelectAnyLevel ) pData->bSelectAnyLevel = 1;
	if ( __xuiCascaderExpandTriggerValid(pDesc->iExpandTrigger) ) pData->iExpandTrigger = pDesc->iExpandTrigger;
	if ( __xuiCascaderPlacementValid(pDesc->iPopupPlacement) ) pData->iPopupPlacement = pDesc->iPopupPlacement;
	if ( pDesc->sPlaceholder != NULL ) __xuiCascaderCopyText(pData->sPlaceholder, (int)sizeof(pData->sPlaceholder), pDesc->sPlaceholder);
	if ( pDesc->sSeparator != NULL ) __xuiCascaderCopyText(pData->sSeparator, (int)sizeof(pData->sSeparator), pDesc->sSeparator);
	if ( pDesc->fItemHeight > 0.0f ) pData->fItemHeight = pDesc->fItemHeight;
	if ( pDesc->fColumnWidth > 0.0f ) pData->fColumnWidth = pDesc->fColumnWidth;
	if ( pDesc->fPopupHeight > 0.0f ) pData->fPopupHeight = pDesc->fPopupHeight;
	if ( pDesc->fPopupMaxHeight > 0.0f ) pData->fPopupMaxHeight = pDesc->fPopupMaxHeight;
	if ( __xuiCascaderAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiCascaderAlpha(pDesc->iPlaceholderColor) != 0 ) pData->iPlaceholderColor = pDesc->iPlaceholderColor;
	if ( __xuiCascaderAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiCascaderAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiCascaderAlpha(pDesc->iHoverBackgroundColor) != 0 ) pData->iHoverBackgroundColor = pDesc->iHoverBackgroundColor;
	if ( __xuiCascaderAlpha(pDesc->iOpenBackgroundColor) != 0 ) pData->iOpenBackgroundColor = pDesc->iOpenBackgroundColor;
	if ( __xuiCascaderAlpha(pDesc->iDisabledBackgroundColor) != 0 ) pData->iDisabledBackgroundColor = pDesc->iDisabledBackgroundColor;
	if ( __xuiCascaderAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiCascaderAlpha(pDesc->iHoverBorderColor) != 0 ) pData->iHoverBorderColor = pDesc->iHoverBorderColor;
	if ( __xuiCascaderAlpha(pDesc->iFocusBorderColor) != 0 ) pData->iFocusBorderColor = pDesc->iFocusBorderColor;
	if ( __xuiCascaderAlpha(pDesc->iArrowColor) != 0 ) pData->iArrowColor = pDesc->iArrowColor;
	if ( __xuiCascaderAlpha(pDesc->iDisabledArrowColor) != 0 ) pData->iDisabledArrowColor = pDesc->iDisabledArrowColor;
	if ( __xuiCascaderAlpha(pDesc->iButtonColor) != 0 ) pData->iButtonColor = pDesc->iButtonColor;
	if ( __xuiCascaderAlpha(pDesc->iButtonHoverColor) != 0 ) pData->iButtonHoverColor = pDesc->iButtonHoverColor;
	if ( __xuiCascaderAlpha(pDesc->iButtonOpenColor) != 0 ) pData->iButtonOpenColor = pDesc->iButtonOpenColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupPanelColor) != 0 ) pData->iPopupPanelColor = pDesc->iPopupPanelColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupBorderColor) != 0 ) pData->iPopupBorderColor = pDesc->iPopupBorderColor;
	if ( pDesc->iPopupShadowColor != 0 ) pData->iPopupShadowColor = pDesc->iPopupShadowColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupTextColor) != 0 ) pData->iPopupTextColor = pDesc->iPopupTextColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupMutedTextColor) != 0 ) pData->iPopupMutedTextColor = pDesc->iPopupMutedTextColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupHoverColor) != 0 ) pData->iPopupHoverColor = pDesc->iPopupHoverColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupActiveColor) != 0 ) pData->iPopupActiveColor = pDesc->iPopupActiveColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupSelectedColor) != 0 ) pData->iPopupSelectedColor = pDesc->iPopupSelectedColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupActiveTextColor) != 0 ) pData->iPopupActiveTextColor = pDesc->iPopupActiveTextColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupDisabledTextColor) != 0 ) pData->iPopupDisabledTextColor = pDesc->iPopupDisabledTextColor;
	if ( __xuiCascaderAlpha(pDesc->iPopupSeparatorColor) != 0 ) pData->iPopupSeparatorColor = pDesc->iPopupSeparatorColor;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
}

static void __xuiCascaderResolve(xui_widget pWidget, xui_cascader_data_t* pData, xui_cascader_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->pFont = __xuiCascaderStyleFont(pWidget, pResolved->pFont);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.text.color", &pResolved->iTextColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.placeholder.color", &pResolved->iPlaceholderColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.background.color", &pResolved->iBackgroundColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.background.open_color", &pResolved->iOpenBackgroundColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.border.color", &pResolved->iBorderColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiCascaderStyleColor(pWidget, "cascader.arrow.color", &pResolved->iArrowColor);
	(void)__xuiCascaderStyleFloat(pWidget, "cascader.border.width", &pResolved->fBorderWidth);
}

static int __xuiCascaderItemEnabled(const xui_cascader_data_t* pData, int iItem)
{
	if ( (pData == NULL) || (iItem < 0) || (iItem >= pData->iItemCount) ) return 0;
	return (pData->arrItems[iItem].iFlags & XUI_CASCADER_ITEM_DISABLED) == 0u;
}

static int __xuiCascaderItemHasChildren(const xui_cascader_data_t* pData, int iItem)
{
	int i;

	if ( (pData == NULL) || (iItem < 0) || (iItem >= pData->iItemCount) ) return 0;
	if ( (pData->arrItems[iItem].iFlags & XUI_CASCADER_ITEM_LEAF) != 0u ) return 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iParent == iItem ) return 1;
	}
	return 0;
}

static int __xuiCascaderChildCount(const xui_cascader_data_t* pData, int iParent)
{
	int i;
	int n;

	if ( pData == NULL ) return 0;
	n = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iParent == iParent ) n++;
	}
	return n;
}

static int __xuiCascaderChildAt(const xui_cascader_data_t* pData, int iParent, int iOrdinal)
{
	int i;
	int n;

	if ( (pData == NULL) || (iOrdinal < 0) ) return -1;
	n = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iParent == iParent ) {
			if ( n == iOrdinal ) return i;
			n++;
		}
	}
	return -1;
}

static int __xuiCascaderChildOrdinal(const xui_cascader_data_t* pData, int iParent, int iItem)
{
	int i;
	int n;

	if ( pData == NULL ) return -1;
	n = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iParent == iParent ) {
			if ( i == iItem ) return n;
			n++;
		}
	}
	return -1;
}

static int __xuiCascaderFindChildByValue(const xui_cascader_data_t* pData, int iParent, int iValue)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( (pData->arrItems[i].iParent == iParent) && (pData->arrItems[i].iValue == iValue) ) return i;
	}
	return -1;
}

static int __xuiCascaderBuildPathToItem(const xui_cascader_data_t* pData, int iItem, int* arrPath, int iCapacity)
{
	int arrReverse[XUI_CASCADER_PATH_CAPACITY];
	int iDepth;
	int i;

	if ( (pData == NULL) || (iItem < 0) || (iItem >= pData->iItemCount) || (arrPath == NULL) || (iCapacity <= 0) ) return 0;
	iDepth = 0;
	while ( (iItem >= 0) && (iItem < pData->iItemCount) && (iDepth < XUI_CASCADER_PATH_CAPACITY) ) {
		arrReverse[iDepth++] = iItem;
		iItem = pData->arrItems[iItem].iParent;
	}
	if ( iDepth > iCapacity ) return 0;
	for ( i = 0; i < iDepth; i++ ) {
		arrPath[i] = arrReverse[iDepth - i - 1];
	}
	return iDepth;
}

static int __xuiCascaderColumnParent(const xui_cascader_data_t* pData, int iColumn)
{
	if ( iColumn <= 0 ) return -1;
	if ( (pData == NULL) || (iColumn > pData->iActiveDepth) ) return -2;
	return pData->arrActive[iColumn - 1];
}

static int __xuiCascaderColumnCount(const xui_cascader_data_t* pData)
{
	int i;
	int n;

	if ( pData == NULL ) return 0;
	n = 1;
	for ( i = 0; i < pData->iActiveDepth && i < XUI_CASCADER_PATH_CAPACITY; i++ ) {
		if ( (pData->arrActive[i] < 0) || (pData->arrActive[i] >= pData->iItemCount) ) break;
		n = i + 1;
		if ( __xuiCascaderItemHasChildren(pData, pData->arrActive[i]) && (n < XUI_CASCADER_PATH_CAPACITY) ) {
			n = i + 2;
		} else {
			break;
		}
	}
	if ( n < 1 ) n = 1;
	if ( n > XUI_CASCADER_PATH_CAPACITY ) n = XUI_CASCADER_PATH_CAPACITY;
	return n;
}

static int __xuiCascaderVisibleRows(const xui_cascader_data_t* pData)
{
	int n;

	if ( (pData == NULL) || (pData->fItemHeight <= 0.0f) ) return 1;
	n = (int)(pData->fResolvedPopupHeight / pData->fItemHeight);
	if ( n < 1 ) n = 1;
	return n;
}

static void __xuiCascaderResolvePopupSize(xui_cascader_data_t* pData)
{
	int i;
	int columns;
	int rows;
	int maxRows;
	float h;

	if ( pData == NULL ) return;
	columns = __xuiCascaderColumnCount(pData);
	maxRows = 1;
	for ( i = 0; i < columns; i++ ) {
		rows = __xuiCascaderChildCount(pData, __xuiCascaderColumnParent(pData, i));
		if ( rows > maxRows ) maxRows = rows;
	}
	pData->fResolvedPopupWidth = __xuiCascaderMax(1.0f, pData->fColumnWidth * (float)columns);
	if ( pData->fPopupHeight > 0.0f ) {
		h = pData->fPopupHeight;
	} else {
		h = (float)maxRows * pData->fItemHeight;
		if ( pData->fPopupMaxHeight > 0.0f ) h = __xuiCascaderMin(h, pData->fPopupMaxHeight);
	}
	if ( h < pData->fItemHeight ) h = pData->fItemHeight;
	pData->fResolvedPopupHeight = h;
}

static void __xuiCascaderClampScrolls(xui_cascader_data_t* pData)
{
	int i;
	int columns;
	int count;
	int visible;
	int maxScroll;

	if ( pData == NULL ) return;
	__xuiCascaderResolvePopupSize(pData);
	columns = __xuiCascaderColumnCount(pData);
	visible = __xuiCascaderVisibleRows(pData);
	for ( i = 0; i < XUI_CASCADER_PATH_CAPACITY; i++ ) {
		if ( i >= columns ) {
			pData->arrColumnScroll[i] = 0;
			continue;
		}
		count = __xuiCascaderChildCount(pData, __xuiCascaderColumnParent(pData, i));
		maxScroll = count - visible;
		if ( maxScroll < 0 ) maxScroll = 0;
		if ( pData->arrColumnScroll[i] < 0 ) pData->arrColumnScroll[i] = 0;
		if ( pData->arrColumnScroll[i] > maxScroll ) pData->arrColumnScroll[i] = maxScroll;
	}
}

static void __xuiCascaderEnsureItemVisible(xui_cascader_data_t* pData, int iColumn, int iItem)
{
	int parent;
	int ordinal;
	int visible;

	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= XUI_CASCADER_PATH_CAPACITY) || (iItem < 0) ) return;
	__xuiCascaderClampScrolls(pData);
	parent = __xuiCascaderColumnParent(pData, iColumn);
	ordinal = __xuiCascaderChildOrdinal(pData, parent, iItem);
	if ( ordinal < 0 ) return;
	visible = __xuiCascaderVisibleRows(pData);
	if ( ordinal < pData->arrColumnScroll[iColumn] ) pData->arrColumnScroll[iColumn] = ordinal;
	if ( ordinal >= pData->arrColumnScroll[iColumn] + visible ) pData->arrColumnScroll[iColumn] = ordinal - visible + 1;
	__xuiCascaderClampScrolls(pData);
}

static xui_rect_t __xuiCascaderPanelItemRect(const xui_cascader_data_t* pData, int iColumn, int iItem)
{
	xui_rect_t r;
	int parent;
	int ordinal;

	r = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= __xuiCascaderColumnCount(pData)) || (iItem < 0) ) return r;
	parent = __xuiCascaderColumnParent(pData, iColumn);
	ordinal = __xuiCascaderChildOrdinal(pData, parent, iItem);
	if ( ordinal < 0 ) return r;
	r.fX = pData->fColumnWidth * (float)iColumn;
	r.fY = pData->fItemHeight * (float)(ordinal - pData->arrColumnScroll[iColumn]);
	r.fW = pData->fColumnWidth;
	r.fH = pData->fItemHeight;
	return xuiInternalSnapRect(r);
}

static int __xuiCascaderInvalidatePanelItem(xui_widget pPanel, xui_cascader_data_t* pData, int iColumn, int iItem)
{
	xui_rect_t tRect;

	if ( (pPanel == NULL) || (pData == NULL) || (iColumn < 0) || (iItem < 0) ) {
		return XUI_OK;
	}
	tRect = __xuiCascaderPanelItemRect(pData, iColumn, iItem);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect.fX -= 2.0f;
	tRect.fY -= 2.0f;
	tRect.fW += 4.0f;
	tRect.fH += 4.0f;
	return xuiWidgetInvalidateRect(pPanel, tRect, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiCascaderSyncDisplay(xui_cascader_data_t* pData)
{
	int i;
	int offset;
	const char* sText;
	const char* sSep;

	if ( pData == NULL ) return;
	pData->sDisplay[0] = '\0';
	if ( (pData->iSelectedDepth <= 0) || (pData->iSelectedLeaf < 0) ) return;
	if ( !pData->bShowAllLevels ) {
		__xuiCascaderCopyText(pData->sDisplay, (int)sizeof(pData->sDisplay), pData->arrItems[pData->iSelectedLeaf].sText);
		return;
	}
	offset = 0;
	sSep = __xuiCascaderText(pData->sSeparator);
	for ( i = 0; i < pData->iSelectedDepth; i++ ) {
		if ( (pData->arrSelected[i] < 0) || (pData->arrSelected[i] >= pData->iItemCount) ) break;
		if ( i > 0 ) {
			offset += snprintf(pData->sDisplay + offset, sizeof(pData->sDisplay) - (size_t)offset, "%s", sSep);
			if ( offset >= (int)sizeof(pData->sDisplay) - 1 ) break;
		}
		sText = __xuiCascaderText(pData->arrItems[pData->arrSelected[i]].sText);
		offset += snprintf(pData->sDisplay + offset, sizeof(pData->sDisplay) - (size_t)offset, "%s", sText);
		if ( offset >= (int)sizeof(pData->sDisplay) - 1 ) break;
	}
	pData->sDisplay[sizeof(pData->sDisplay) - 1] = '\0';
}

static void __xuiCascaderSetActivePath(xui_cascader_data_t* pData, const int* arrPath, int iDepth)
{
	int i;

	if ( pData == NULL ) return;
	if ( iDepth < 0 ) iDepth = 0;
	if ( iDepth > XUI_CASCADER_PATH_CAPACITY ) iDepth = XUI_CASCADER_PATH_CAPACITY;
	for ( i = 0; i < XUI_CASCADER_PATH_CAPACITY; i++ ) {
		pData->arrActive[i] = (i < iDepth && arrPath != NULL) ? arrPath[i] : -1;
	}
	pData->iActiveDepth = iDepth;
	__xuiCascaderClampScrolls(pData);
	for ( i = 0; i < pData->iActiveDepth; i++ ) __xuiCascaderEnsureItemVisible(pData, i, pData->arrActive[i]);
}

static void __xuiCascaderSetActiveToItem(xui_cascader_data_t* pData, int iItem)
{
	int arrPath[XUI_CASCADER_PATH_CAPACITY];
	int depth;

	if ( pData == NULL ) return;
	depth = __xuiCascaderBuildPathToItem(pData, iItem, arrPath, XUI_CASCADER_PATH_CAPACITY);
	__xuiCascaderSetActivePath(pData, arrPath, depth);
}

static void __xuiCascaderSyncActiveFromSelected(xui_cascader_data_t* pData)
{
	if ( pData == NULL ) return;
	if ( pData->iSelectedDepth > 0 ) {
		__xuiCascaderSetActivePath(pData, pData->arrSelected, pData->iSelectedDepth);
		pData->iHoverColumn = pData->iSelectedDepth - 1;
		pData->iHoverItem = pData->iSelectedLeaf;
		__xuiCascaderEnsureItemVisible(pData, pData->iHoverColumn, pData->iHoverItem);
	} else {
		__xuiCascaderSetActivePath(pData, NULL, 0);
		pData->iHoverColumn = 0;
		pData->iHoverItem = __xuiCascaderChildAt(pData, -1, 0);
	}
}

static int __xuiCascaderSetSelectedItem(xui_widget pWidget, xui_cascader_data_t* pData, int iItem, int bNotify)
{
	xui_cascader_change_proc onChange;
	void* pUser;
	int arrValues[XUI_CASCADER_PATH_CAPACITY];
	int arrPath[XUI_CASCADER_PATH_CAPACITY];
	int depth;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iItem < 0) || (iItem >= pData->iItemCount) || !__xuiCascaderItemEnabled(pData, iItem) ) return XUI_ERROR_INVALID_ARGUMENT;
	depth = __xuiCascaderBuildPathToItem(pData, iItem, arrPath, XUI_CASCADER_PATH_CAPACITY);
	if ( depth <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < XUI_CASCADER_PATH_CAPACITY; i++ ) {
		pData->arrSelected[i] = (i < depth) ? arrPath[i] : -1;
	}
	pData->iSelectedDepth = depth;
	pData->iSelectedLeaf = iItem;
	__xuiCascaderSyncDisplay(pData);
	__xuiCascaderSetActivePath(pData, arrPath, depth);
	pData->iHoverColumn = depth - 1;
	pData->iHoverItem = iItem;
	pData->iChangeCount++;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) {
		for ( i = 0; i < depth; i++ ) arrValues[i] = pData->arrItems[arrPath[i]].iValue;
		onChange = pData->onChange;
		pUser = pData->pChangeUser;
		if ( onChange != NULL ) onChange(pWidget, iItem, arrValues, depth, pUser);
	}
	return XUI_OK;
}

static int __xuiCascaderClearInternal(xui_widget pWidget, xui_cascader_data_t* pData, int bNotify)
{
	xui_cascader_change_proc onChange;
	void* pUser;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSelectedDepth <= 0 && pData->iSelectedLeaf < 0 ) return XUI_OK;
	for ( i = 0; i < XUI_CASCADER_PATH_CAPACITY; i++ ) pData->arrSelected[i] = -1;
	pData->iSelectedDepth = 0;
	pData->iSelectedLeaf = -1;
	pData->sDisplay[0] = '\0';
	__xuiCascaderSetActivePath(pData, NULL, 0);
	pData->iChangeCount++;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify ) {
		onChange = pData->onChange;
		pUser = pData->pChangeUser;
		if ( onChange != NULL ) onChange(pWidget, -1, NULL, 0, pUser);
	}
	return XUI_OK;
}

static int __xuiCascaderSetSelectedPathInternal(xui_widget pWidget, xui_cascader_data_t* pData, const int* arrValues, int iDepth, int bNotify)
{
	int parent;
	int item;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (iDepth < 0) || (iDepth > XUI_CASCADER_PATH_CAPACITY) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iDepth == 0 ) return __xuiCascaderClearInternal(pWidget, pData, bNotify);
	if ( arrValues == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	parent = -1;
	item = -1;
	for ( i = 0; i < iDepth; i++ ) {
		item = __xuiCascaderFindChildByValue(pData, parent, arrValues[i]);
		if ( (item < 0) || !__xuiCascaderItemEnabled(pData, item) ) return XUI_ERROR_INVALID_ARGUMENT;
		parent = item;
	}
	return __xuiCascaderSetSelectedItem(pWidget, pData, item, bNotify);
}

static void __xuiCascaderUpdateOwnerRects(xui_widget pWidget, xui_cascader_data_t* pData)
{
	xui_rect_t r;
	float button;
	float clear;

	if ( (pWidget == NULL) || (pData == NULL) ) return;
	r = xuiWidgetGetRect(pWidget);
	r.fX = 0.0f;
	r.fY = 0.0f;
	button = __xuiCascaderMin(30.0f, __xuiCascaderMax(22.0f, r.fH));
	pData->tButtonRect = xuiInternalSnapRect((xui_rect_t){__xuiCascaderMax(0.0f, r.fW - button), 0.0f, button, r.fH});
	pData->tClearRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	clear = 0.0f;
	if ( pData->bClearable && pData->iSelectedDepth > 0 && xuiWidgetGetEnabled(pWidget) ) {
		clear = 22.0f;
		pData->tClearRect = xuiInternalSnapRect((xui_rect_t){__xuiCascaderMax(0.0f, pData->tButtonRect.fX - clear), 0.0f, clear, r.fH});
	}
	pData->tTextRect = xuiInternalSnapRect((xui_rect_t){10.0f, 0.0f, __xuiCascaderMax(0.0f, r.fW - button - clear - 14.0f), r.fH});
}

static uint32_t __xuiCascaderState(xui_widget pWidget, xui_cascader_data_t* pData)
{
	xui_context pContext;
	xui_widget pFocus;
	uint32_t iState;

	if ( pWidget == NULL ) return 0u;
	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	if ( (pData != NULL) && (pData->pPopup != NULL) && xuiPopupIsOpen(pData->pPopup) ) iState |= XUI_CASCADER_STATE_OPEN;
	if ( pData != NULL ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFocus = xuiGetFocusWidget(pContext);
		if ( (pFocus == pWidget) || (pFocus == pData->pPanel) || (pFocus == pData->pPopup) ) iState |= XUI_WIDGET_STATE_FOCUS;
	}
	return iState;
}

static int __xuiCascaderSyncState(xui_widget pWidget, xui_cascader_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiCascaderState(pWidget, pData));
}

static int __xuiCascaderApplyPopupStyle(xui_widget pWidget, xui_cascader_data_t* pData)
{
	xui_cascader_data_t tResolved;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pPopup == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCascaderResolve(pWidget, pData, &tResolved);
	__xuiCascaderClampScrolls(pData);
	iRet = xuiPopupSetOwner(pData->pPopup, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusRestore(pData->pPopup, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetContentSize(pData->pPopup, pData->fResolvedPopupWidth, pData->fResolvedPopupHeight);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetColors(pData->pPopup, tResolved.iPopupPanelColor, tResolved.iPopupBorderColor, tResolved.iPopupShadowColor, XUI_COLOR_RGBA(0, 0, 0, 0));
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetMetrics(pData->pPopup, 0.0f, 1.0f, 5.0f);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetClosePolicy(pData->pPopup, XUI_POPUP_OUTSIDE_CLOSE, XUI_POPUP_OWNER_PASSTHROUGH, XUI_POPUP_ESCAPE_CLOSE);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pPanel);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetGap(pData->pPopup, 2.0f);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiPopupClearAnchorRect(pData->pPopup);
	if ( pData->iPopupPlacement == XUI_CASCADER_POPUP_TOP ) {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_TOP_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_UP);
	} else {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	}
	if ( pData->pPanel != NULL ) {
		(void)xuiWidgetSetRect(pData->pPanel, (xui_rect_t){0.0f, 0.0f, pData->fResolvedPopupWidth, pData->fResolvedPopupHeight});
		(void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiCascaderOpenInternal(xui_widget pWidget, xui_cascader_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pPopup == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		if ( xuiPopupIsOpen(pData->pPopup) ) return __xuiCascaderCloseInternal(pWidget, pData);
		return XUI_OK;
	}
	__xuiCascaderSyncActiveFromSelected(pData);
	iRet = __xuiCascaderApplyPopupStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetOpen(pData->pPopup, 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->pPanel != NULL ) (void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pPanel);
	(void)__xuiCascaderSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCascaderCloseInternal(xui_widget pWidget, xui_cascader_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pPopup != NULL ) {
		iRet = xuiPopupSetOpen(pData->pPopup, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)__xuiCascaderSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCascaderOwnerPointerDown(xui_widget pWidget, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t w;
	float x;
	float y;

	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	__xuiCascaderUpdateOwnerRects(pWidget, pData);
	w = xuiWidgetGetWorldRect(pWidget);
	x = pEvent->fX - w.fX;
	y = pEvent->fY - w.fY;
	if ( pData->bClearable && pData->iSelectedDepth > 0 && __xuiCascaderRectContains(pData->tClearRect, x, y) ) {
		(void)__xuiCascaderClearInternal(pWidget, pData, 1);
		(void)__xuiCascaderCloseInternal(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( (pData->pPopup != NULL) && xuiPopupIsOpen(pData->pPopup) ) {
		(void)__xuiCascaderCloseInternal(pWidget, pData);
	} else {
		(void)__xuiCascaderOpenInternal(pWidget, pData);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiCascaderOwnerKeyDown(xui_widget pWidget, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE || !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) {
			(void)__xuiCascaderCloseInternal(pWidget, pData);
		} else {
			(void)__xuiCascaderOpenInternal(pWidget, pData);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
		(void)__xuiCascaderOpenInternal(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_BACKSPACE:
	case XUI_KEY_DELETE:
		if ( pData->bClearable ) {
			(void)__xuiCascaderClearInternal(pWidget, pData, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_ESCAPE:
		if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) {
			(void)__xuiCascaderCloseInternal(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCascaderOwnerEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_cascader_data_t* pData;

	(void)pUser;
	pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiCascaderOwnerPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) return XUI_EVENT_DISPATCH_STOP;
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiCascaderOwnerKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiCascaderSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) (void)__xuiCascaderApplyPopupStyle(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) (void)__xuiCascaderCloseInternal(pWidget, pData);
		(void)__xuiCascaderSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCascaderHitPanel(xui_cascader_data_t* pData, float x, float y, int* pColumn, int* pItem)
{
	int column;
	int row;
	int parent;
	int item;

	if ( pColumn != NULL ) *pColumn = -1;
	if ( pItem != NULL ) *pItem = -1;
	if ( (pData == NULL) || (x < 0.0f) || (y < 0.0f) || (pData->fColumnWidth <= 0.0f) || (pData->fItemHeight <= 0.0f) ) return 0;
	__xuiCascaderClampScrolls(pData);
	column = (int)(x / pData->fColumnWidth);
	if ( (column < 0) || (column >= __xuiCascaderColumnCount(pData)) ) return 0;
	row = (int)(y / pData->fItemHeight) + pData->arrColumnScroll[column];
	parent = __xuiCascaderColumnParent(pData, column);
	item = __xuiCascaderChildAt(pData, parent, row);
	if ( item < 0 ) return 0;
	if ( pColumn != NULL ) *pColumn = column;
	if ( pItem != NULL ) *pItem = item;
	return 1;
}

static int __xuiCascaderPanelActivateItem(xui_widget pOwner, xui_cascader_data_t* pData, int iColumn, int iItem, int bCommit)
{
	int hasChildren;
	int iRet;

	if ( (pOwner == NULL) || (pData == NULL) || (iItem < 0) || (iItem >= pData->iItemCount) ) return XUI_OK;
	pData->iHoverColumn = iColumn;
	pData->iHoverItem = iItem;
	if ( !__xuiCascaderItemEnabled(pData, iItem) ) return XUI_OK;
	hasChildren = __xuiCascaderItemHasChildren(pData, iItem);
	if ( hasChildren ) {
		__xuiCascaderSetActiveToItem(pData, iItem);
		__xuiCascaderEnsureItemVisible(pData, iColumn, iItem);
		(void)__xuiCascaderApplyPopupStyle(pOwner, pData);
	}
	if ( bCommit && (!hasChildren || pData->bSelectAnyLevel) ) {
		iRet = __xuiCascaderSetSelectedItem(pOwner, pData, iItem, 1);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiCascaderCloseInternal(pOwner, pData);
	}
	if ( pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

static int __xuiCascaderPanelPointerDown(xui_widget pPanel, xui_widget pOwner, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t w;
	float x;
	float y;
	int column;
	int item;

	if ( (pEvent->iButton != 0) && (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) ) return XUI_OK;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pPanel), pPanel);
	w = xuiWidgetGetWorldRect(pPanel);
	x = pEvent->fX - w.fX;
	y = pEvent->fY - w.fY;
	if ( __xuiCascaderHitPanel(pData, x, y, &column, &item) ) {
		(void)__xuiCascaderPanelActivateItem(pOwner, pData, column, item, 1);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiCascaderPanelPointerMove(xui_widget pPanel, xui_widget pOwner, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t w;
	float x;
	float y;
	int column;
	int item;
	int oldColumn;
	int oldItem;
	int bExpanded;

	w = xuiWidgetGetWorldRect(pPanel);
	x = pEvent->fX - w.fX;
	y = pEvent->fY - w.fY;
	if ( __xuiCascaderHitPanel(pData, x, y, &column, &item) ) {
		if ( (pData->iHoverColumn != column) || (pData->iHoverItem != item) ) {
			oldColumn = pData->iHoverColumn;
			oldItem = pData->iHoverItem;
			bExpanded = 0;
			pData->iHoverColumn = column;
			pData->iHoverItem = item;
			if ( pData->iExpandTrigger == XUI_CASCADER_EXPAND_HOVER && __xuiCascaderItemEnabled(pData, item) && __xuiCascaderItemHasChildren(pData, item) ) {
				(void)__xuiCascaderPanelActivateItem(pOwner, pData, column, item, 0);
				bExpanded = 1;
			}
			if ( bExpanded ) {
				(void)xuiWidgetInvalidate(pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			} else {
				(void)__xuiCascaderInvalidatePanelItem(pPanel, pData, oldColumn, oldItem);
				(void)__xuiCascaderInvalidatePanelItem(pPanel, pData, column, item);
			}
		}
	} else if ( pData->iHoverItem >= 0 ) {
		oldColumn = pData->iHoverColumn;
		oldItem = pData->iHoverItem;
		pData->iHoverColumn = -1;
		pData->iHoverItem = -1;
		(void)__xuiCascaderInvalidatePanelItem(pPanel, pData, oldColumn, oldItem);
	}
	return XUI_OK;
}

static int __xuiCascaderMoveHover(xui_cascader_data_t* pData, int iDelta)
{
	int column;
	int parent;
	int ordinal;
	int count;
	int item;
	int i;

	if ( pData == NULL ) return 0;
	column = pData->iHoverColumn;
	if ( column < 0 || column >= __xuiCascaderColumnCount(pData) ) column = 0;
	parent = __xuiCascaderColumnParent(pData, column);
	ordinal = __xuiCascaderChildOrdinal(pData, parent, pData->iHoverItem);
	count = __xuiCascaderChildCount(pData, parent);
	if ( count <= 0 ) return 0;
	for ( i = 0; i < count; i++ ) {
		ordinal += iDelta;
		if ( ordinal < 0 ) ordinal = count - 1;
		if ( ordinal >= count ) ordinal = 0;
		item = __xuiCascaderChildAt(pData, parent, ordinal);
		if ( item >= 0 ) {
			pData->iHoverColumn = column;
			pData->iHoverItem = item;
			__xuiCascaderEnsureItemVisible(pData, column, item);
			return 1;
		}
	}
	return 0;
}

static int __xuiCascaderPanelKeyDown(xui_widget pPanel, xui_widget pOwner, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	int child;
	int parentItem;
	int column;

	(void)pPanel;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ESCAPE:
		(void)__xuiCascaderCloseInternal(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP:
		if ( __xuiCascaderMoveHover(pData, -1) && pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
		if ( __xuiCascaderMoveHover(pData, 1) && pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
		if ( pData->iHoverItem >= 0 && __xuiCascaderItemEnabled(pData, pData->iHoverItem) && __xuiCascaderItemHasChildren(pData, pData->iHoverItem) ) {
			(void)__xuiCascaderPanelActivateItem(pOwner, pData, pData->iHoverColumn, pData->iHoverItem, 0);
			child = __xuiCascaderChildAt(pData, pData->iHoverItem, 0);
			if ( child >= 0 ) {
				pData->iHoverColumn++;
				pData->iHoverItem = child;
				__xuiCascaderEnsureItemVisible(pData, pData->iHoverColumn, child);
			}
		}
		if ( pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
		column = pData->iHoverColumn;
		if ( column > 0 ) {
			parentItem = pData->arrActive[column - 1];
			pData->iHoverColumn = column - 1;
			pData->iHoverItem = parentItem;
			__xuiCascaderEnsureItemVisible(pData, pData->iHoverColumn, pData->iHoverItem);
			if ( pData->pPanel != NULL ) (void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( pData->iHoverItem >= 0 ) (void)__xuiCascaderPanelActivateItem(pOwner, pData, pData->iHoverColumn, pData->iHoverItem, 1);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCascaderPanelWheel(xui_widget pPanel, xui_cascader_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t w;
	float x;
	int column;
	int before;

	if ( (pPanel == NULL) || (pData == NULL) ) return XUI_OK;
	w = xuiWidgetGetWorldRect(pPanel);
	x = pEvent->fX - w.fX;
	if ( pData->fColumnWidth <= 0.0f ) return XUI_OK;
	column = (int)(x / pData->fColumnWidth);
	if ( column < 0 || column >= __xuiCascaderColumnCount(pData) ) return XUI_OK;
	before = pData->arrColumnScroll[column];
	if ( pEvent->fWheelY > 0.0f ) pData->arrColumnScroll[column] -= 1;
	if ( pEvent->fWheelY < 0.0f ) pData->arrColumnScroll[column] += 1;
	__xuiCascaderClampScrolls(pData);
	if ( before != pData->arrColumnScroll[column] ) {
		(void)xuiWidgetInvalidate(pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiCascaderPanelEvent(xui_widget pPanel, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pOwner;
	xui_cascader_data_t* pData;

	pOwner = (xui_widget)pUser;
	pData = __xuiCascaderGetData(pOwner);
	if ( (pOwner == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiCascaderPanelPointerDown(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_POINTER_MOVE:
		return __xuiCascaderPanelPointerMove(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiCascaderPanelWheel(pPanel, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiCascaderPanelKeyDown(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiCascaderSyncState(pOwner, pData);
		return xuiWidgetInvalidate(pOwner, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiCascaderPopupChanged(xui_widget pPopup, int bOpen, void* pUser)
{
	xui_widget pOwner;
	xui_cascader_data_t* pData;

	(void)pPopup;
	pOwner = (xui_widget)pUser;
	pData = __xuiCascaderGetData(pOwner);
	if ( (pOwner == NULL) || (pData == NULL) ) return;
	if ( !bOpen ) {
		pData->iHoverColumn = -1;
		pData->iHoverItem = -1;
	}
	(void)__xuiCascaderSyncState(pOwner, pData);
	(void)xuiWidgetInvalidate(pOwner, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCascaderDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t iColor)
{
	if ( __xuiCascaderAlpha(iColor) == 0 ) return XUI_OK;
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, r, iColor) : XUI_OK;
}

static int __xuiCascaderDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiCascaderAlpha(iColor) == 0) ) return XUI_OK;
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, r, fWidth, iColor) : XUI_OK;
}

static int __xuiCascaderDrawChevron(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, int iDirection, uint32_t iColor)
{
	float cx;
	float cy;
	int iRet;

	if ( pProxy->drawLine == NULL || __xuiCascaderAlpha(iColor) == 0 ) return XUI_OK;
	cx = r.fX + r.fW * 0.5f;
	cy = r.fY + r.fH * 0.5f;
	if ( iDirection > 0 ) {
		iRet = pProxy->drawLine(pProxy, pDraw, cx - 2.5f, cy - 4.0f, cx + 2.5f, cy, 1.6f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, cx + 2.5f, cy, cx - 2.5f, cy + 4.0f, 1.6f, iColor);
	}
	if ( iDirection < 0 ) {
		iRet = pProxy->drawLine(pProxy, pDraw, cx - 4.0f, cy + 2.0f, cx, cy - 2.0f, 1.6f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, cx, cy - 2.0f, cx + 4.0f, cy + 2.0f, 1.6f, iColor);
	}
	iRet = pProxy->drawLine(pProxy, pDraw, cx - 4.0f, cy - 2.0f, cx, cy + 2.0f, 1.6f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, cx, cy + 2.0f, cx + 4.0f, cy - 2.0f, 1.6f, iColor);
}

static int __xuiCascaderDrawClear(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t iColor)
{
	float cx;
	float cy;

	if ( pProxy->drawLine == NULL || __xuiCascaderAlpha(iColor) == 0 ) return XUI_OK;
	cx = r.fX + r.fW * 0.5f;
	cy = r.fY + r.fH * 0.5f;
	(void)pProxy->drawLine(pProxy, pDraw, cx - 4.0f, cy - 4.0f, cx + 4.0f, cy + 4.0f, 1.5f, iColor);
	return pProxy->drawLine(pProxy, pDraw, cx + 4.0f, cy - 4.0f, cx - 4.0f, cy + 4.0f, 1.5f, iColor);
}

static int __xuiCascaderOwnerRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_cascader_data_t* pData;
	xui_cascader_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t r;
	const char* sText;
	uint32_t state;
	uint32_t bg;
	uint32_t border;
	uint32_t text;
	uint32_t arrow;
	uint32_t button;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCascaderResolve(pWidget, pData, &tResolved);
	__xuiCascaderUpdateOwnerRects(pWidget, pData);
	state = __xuiCascaderState(pWidget, pData);
	r = xuiWidgetGetRect(pWidget);
	r.fX = 0.0f;
	r.fY = 0.0f;
	r = xuiInternalSnapRect(r);
	bg = tResolved.iBackgroundColor;
	border = tResolved.iBorderColor;
	text = tResolved.iTextColor;
	arrow = tResolved.iArrowColor;
	button = tResolved.iButtonColor;
	if ( (state & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		bg = tResolved.iDisabledBackgroundColor;
		border = __xuiCascaderColorWithAlpha(tResolved.iBorderColor, 130);
		text = tResolved.iDisabledTextColor;
		arrow = tResolved.iDisabledArrowColor;
	} else if ( (state & XUI_CASCADER_STATE_OPEN) != 0 ) {
		bg = tResolved.iOpenBackgroundColor;
		border = tResolved.iFocusBorderColor;
		button = tResolved.iButtonOpenColor;
	} else if ( (state & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		border = tResolved.iFocusBorderColor;
	} else if ( (state & XUI_WIDGET_STATE_HOVER) != 0 ) {
		bg = tResolved.iHoverBackgroundColor;
		border = tResolved.iHoverBorderColor;
		button = tResolved.iButtonHoverColor;
	}
	iRet = __xuiCascaderDrawRectFill(pProxy, pDraw, r, bg);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCascaderDrawRectFill(pProxy, pDraw, pData->tButtonRect, button);
	if ( iRet != XUI_OK ) return iRet;
	if ( pProxy->drawLine != NULL ) {
		iRet = pProxy->drawLine(pProxy, pDraw, pData->tButtonRect.fX, pData->tButtonRect.fY + 3.0f, pData->tButtonRect.fX, pData->tButtonRect.fY + pData->tButtonRect.fH - 3.0f, 1.0f, __xuiCascaderColorWithAlpha(border, 130));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->iSelectedDepth > 0 ) {
		sText = pData->sDisplay;
	} else {
		sText = tResolved.sPlaceholder;
		text = tResolved.iPlaceholderColor;
	}
	if ( pProxy->drawText != NULL ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, sText, pData->tTextRect, text, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->tClearRect.fW > 0.0f ) {
		iRet = __xuiCascaderDrawClear(pProxy, pDraw, pData->tClearRect, __xuiCascaderColorWithAlpha(tResolved.iArrowColor, 185));
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiCascaderDrawChevron(pProxy, pDraw, pData->tButtonRect, ((state & XUI_CASCADER_STATE_OPEN) != 0) ? -1 : 0, arrow);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCascaderDrawRectStroke(pProxy, pDraw, r, tResolved.fBorderWidth, border);
}

static int __xuiCascaderPanelRender(xui_widget pPanel, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pOwner;
	xui_cascader_data_t* pData;
	xui_cascader_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t r;
	xui_rect_t textRect;
	xui_rect_t arrowRect;
	xui_rect_t thumb;
	int columns;
	int column;
	int parent;
	int count;
	int visible;
	int start;
	int end;
	int ord;
	int item;
	int active;
	int selected;
	int hover;
	int disabled;
	int hasChildren;
	uint32_t fill;
	uint32_t text;
	int iRet;

	(void)iStateId;
	pOwner = (xui_widget)pUser;
	if ( (pPanel == NULL) || (pDraw == NULL) || (pOwner == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiCascaderGetData(pOwner);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pOwner));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCascaderResolve(pOwner, pData, &tResolved);
	__xuiCascaderClampScrolls(pData);
	columns = __xuiCascaderColumnCount(pData);
	visible = __xuiCascaderVisibleRows(pData);
	if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){0.0f, 0.0f, pData->fResolvedPopupWidth, pData->fResolvedPopupHeight}, tResolved.iPopupPanelColor);
	}
	for ( column = 0; column < columns; column++ ) {
		parent = __xuiCascaderColumnParent(pData, column);
		count = __xuiCascaderChildCount(pData, parent);
		if ( column > 0 && __xuiCascaderAlpha(tResolved.iPopupSeparatorColor) != 0 && pProxy->drawLine != NULL ) {
			(void)pProxy->drawLine(pProxy, pDraw, pData->fColumnWidth * (float)column, 0.0f, pData->fColumnWidth * (float)column, pData->fResolvedPopupHeight, 1.0f, tResolved.iPopupSeparatorColor);
		}
		start = pData->arrColumnScroll[column];
		end = start + visible;
		if ( end > count ) end = count;
		for ( ord = start; ord < end; ord++ ) {
			item = __xuiCascaderChildAt(pData, parent, ord);
			if ( item < 0 ) continue;
			r = __xuiCascaderPanelItemRect(pData, column, item);
			active = (column < pData->iActiveDepth) && (pData->arrActive[column] == item);
			selected = (column < pData->iSelectedDepth) && (pData->arrSelected[column] == item);
			hover = (pData->iHoverColumn == column) && (pData->iHoverItem == item);
			disabled = !__xuiCascaderItemEnabled(pData, item);
			hasChildren = __xuiCascaderItemHasChildren(pData, item);
			fill = 0u;
			text = tResolved.iPopupTextColor;
			if ( disabled ) {
				text = tResolved.iPopupDisabledTextColor;
			} else if ( active ) {
				fill = tResolved.iPopupActiveColor;
				text = tResolved.iPopupActiveTextColor;
			} else if ( selected ) {
				fill = tResolved.iPopupSelectedColor;
			} else if ( hover ) {
				fill = tResolved.iPopupHoverColor;
			}
			if ( __xuiCascaderAlpha(fill) != 0 && pProxy->drawRectFill != NULL ) {
				iRet = pProxy->drawRectFill(pProxy, pDraw, r, fill);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( pProxy->drawText != NULL ) {
				textRect = r;
				textRect.fX += 14.0f;
				textRect.fW = __xuiCascaderMax(0.0f, textRect.fW - 34.0f);
				iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, __xuiCascaderText(pData->arrItems[item].sText), textRect, text, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( hasChildren ) {
				arrowRect = r;
				arrowRect.fX = r.fX + r.fW - 24.0f;
				arrowRect.fW = 20.0f;
				iRet = __xuiCascaderDrawChevron(pProxy, pDraw, arrowRect, 1, active ? tResolved.iPopupActiveTextColor : tResolved.iPopupMutedTextColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( count > visible && pProxy->drawRectFill != NULL ) {
			thumb.fW = 4.0f;
			thumb.fX = pData->fColumnWidth * (float)(column + 1) - 7.0f;
			thumb.fH = __xuiCascaderMax(18.0f, pData->fResolvedPopupHeight * ((float)visible / (float)count));
			thumb.fY = (pData->fResolvedPopupHeight - thumb.fH) * ((float)pData->arrColumnScroll[column] / (float)(count - visible));
			(void)pProxy->drawRectFill(pProxy, pDraw, thumb, __xuiCascaderColorWithAlpha(tResolved.iPopupMutedTextColor, 95));
		}
	}
	return XUI_OK;
}

static int __xuiCascaderContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_cascader_data_t* pData;
	xui_cascader_data_t tResolved;
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;

	(void)tConstraint;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_cascader_data_t*)pUser;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCascaderResolve(pWidget, pData, &tResolved);
	pSize->fX = 190.0f;
	pSize->fY = 30.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (tResolved.pFont != NULL) &&
		(pProxy->fontGetMetrics(pProxy, tResolved.pFont, &tMetrics) == XUI_OK) && (tMetrics.fLineHeight > 0.0f) ) {
		pSize->fY = __xuiCascaderMax(30.0f, tMetrics.fLineHeight + 12.0f);
	}
	pSize->fX = xuiInternalSnapSize(pSize->fX);
	pSize->fY = xuiInternalSnapSize(pSize->fY);
	return XUI_OK;
}

static void __xuiCascaderDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiCascaderDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCascaderInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiCascaderOwnerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiCascaderOwnerEvent, NULL);
	return iRet;
}

static int __xuiCascaderInitPanelEvents(xui_widget pPanel, xui_widget pOwner)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_DOWN, __xuiCascaderPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_MOVE, __xuiCascaderPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_WHEEL, __xuiCascaderPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_KEY_DOWN, __xuiCascaderPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_FOCUS, __xuiCascaderPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_BLUR, __xuiCascaderPanelEvent, pOwner);
	return iRet;
}

static int __xuiCascaderCreatePopup(xui_widget pWidget, xui_cascader_data_t* pData)
{
	xui_popup_desc_t tDesc;
	xui_cache_policy_t tPolicy;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tDesc.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tDesc.iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	tDesc.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tDesc.iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	tDesc.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	__xuiCascaderResolvePopupSize(pData);
	tDesc.fContentWidth = pData->fResolvedPopupWidth;
	tDesc.fContentHeight = pData->fResolvedPopupHeight;
	tDesc.fGap = 2.0f;
	tDesc.fPadding = 0.0f;
	tDesc.fBorderWidth = 1.0f;
	tDesc.fShadowSize = 5.0f;
	tDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tDesc.iPanelColor = pData->iPopupPanelColor;
	tDesc.iBorderColor = pData->iPopupBorderColor;
	tDesc.iShadowColor = pData->iPopupShadowColor;
	tDesc.iBackdropColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pPopup, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	pData->pPanel = xuiPopupGetContentWidget(pData->pPopup);
	if ( pData->pPanel == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCascaderDefaultCachePolicy(&tPolicy);
	(void)xuiWidgetSetFocusable(pData->pPanel, 1);
	(void)xuiWidgetSetTabStop(pData->pPanel, 0);
	(void)xuiWidgetSetOverflow(pData->pPanel, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetCachePolicy(pData->pPanel, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pData->pPanel, __xuiCascaderPanelRender, pWidget);
	iRet = __xuiCascaderInitPanelEvents(pData->pPanel, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetChange(pData->pPopup, __xuiCascaderPopupChanged, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pPanel);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCascaderApplyPopupStyle(pWidget, pData);
}

static int __xuiCascaderSetItemsInternal(xui_widget pWidget, xui_cascader_data_t* pData, const xui_cascader_item_t* pItems, int iCount)
{
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (iCount < 0) || ((iCount > 0) && (pItems == NULL)) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCount > XUI_CASCADER_ITEM_CAPACITY ) iCount = XUI_CASCADER_ITEM_CAPACITY;
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	for ( i = 0; i < iCount; i++ ) {
		pData->arrItems[i] = pItems[i];
		if ( pData->arrItems[i].iParent >= iCount ) pData->arrItems[i].iParent = -1;
	}
	pData->iItemCount = iCount;
	(void)__xuiCascaderClearInternal(pWidget, pData, 0);
	__xuiCascaderSyncDisplay(pData);
	(void)__xuiCascaderApplyPopupStyle(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCascaderInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_cascader_data_t* pData;
	const xui_cascader_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_cascader_data_t*)pTypeData;
	pDesc = (const xui_cascader_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiCascaderDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCascaderDefaults(pData);
	__xuiCascaderApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiCascaderInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCascaderCreatePopup(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->arrItems != NULL) && (pDesc->iItemCount > 0) ) {
		iRet = __xuiCascaderSetItemsInternal(pWidget, pData, pDesc->arrItems, pDesc->iItemCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( (pDesc->arrSelectedValues != NULL) && (pDesc->iSelectedDepth > 0) ) {
			iRet = __xuiCascaderSetSelectedPathInternal(pWidget, pData, pDesc->arrSelectedValues, pDesc->iSelectedDepth, 0);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	(void)__xuiCascaderSyncState(pWidget, pData);
	return XUI_OK;
}

static void __xuiCascaderDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_cascader_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_cascader_data_t*)pTypeData;
	if ( pData != NULL ) {
		if ( pData->pPopup != NULL ) xuiWidgetDestroy(pData->pPopup);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiCascaderRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiCascaderRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t paintDirty;
	uint32_t layoutDirty;

	paintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	layoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.text.color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.placeholder.color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.background.color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.background.hover_color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.background.open_color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.border.color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.border.hover_color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.border.focus_color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.arrow.color", XUI_STYLE_VALUE_COLOR, paintDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "cascader.border.width", XUI_STYLE_VALUE_FLOAT, layoutDirty, 0);
	__xuiCascaderRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, layoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_cascader_data_t* __xuiCascaderGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "cascader");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_cascader_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiCascaderGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "cascader");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "cascader";
	tDesc.iTypeDataSize = sizeof(xui_cascader_data_t);
	tDesc.onInit = __xuiCascaderInit;
	tDesc.onDestroy = __xuiCascaderDestroy;
	tDesc.onContentMeasure = __xuiCascaderContentMeasure;
	tDesc.onCacheRender = __xuiCascaderOwnerRender;
	__xuiCascaderDefaultLayout(&tLayout);
	__xuiCascaderDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiCascaderRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCascaderCreate(xui_context pContext, xui_widget* ppWidget, const xui_cascader_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiCascaderDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiCascaderGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiCascaderSetChange(xui_widget pWidget, xui_cascader_change_proc onChange, void* pUser)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCascaderSetItems(xui_widget pWidget, const xui_cascader_item_t* pItems, int iCount)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCascaderSetItemsInternal(pWidget, pData, pItems, iCount);
}

XUI_API int xuiCascaderGetItemCount(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_cascader_item_t* xuiCascaderGetItem(xui_widget pWidget, int iIndex)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return &pData->arrItems[iIndex];
}

XUI_API int xuiCascaderSetSelectedPath(xui_widget pWidget, const int* arrValues, int iDepth)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCascaderSetSelectedPathInternal(pWidget, pData, arrValues, iDepth, 0);
}

XUI_API int xuiCascaderGetSelectedPath(xui_widget pWidget, int* arrValues, int iCapacity)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	int i;
	if ( (pData == NULL) || (iCapacity < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( arrValues != NULL ) {
		for ( i = 0; i < pData->iSelectedDepth && i < iCapacity; i++ ) arrValues[i] = pData->arrItems[pData->arrSelected[i]].iValue;
	}
	return pData->iSelectedDepth;
}

XUI_API int xuiCascaderGetSelectedDepth(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedDepth : 0;
}

XUI_API int xuiCascaderGetSelectedLeaf(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedLeaf : -1;
}

XUI_API const char* xuiCascaderGetSelectedText(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->sDisplay : "";
}

XUI_API int xuiCascaderClear(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCascaderClearInternal(pWidget, pData, 0);
}

XUI_API int xuiCascaderOpen(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCascaderOpenInternal(pWidget, pData);
}

XUI_API int xuiCascaderClose(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCascaderCloseInternal(pWidget, pData);
}

XUI_API int xuiCascaderToggle(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return (pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup)) ? xuiCascaderClose(pWidget) : xuiCascaderOpen(pWidget);
}

XUI_API int xuiCascaderIsOpen(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL && pData->pPopup != NULL) ? xuiPopupIsOpen(pData->pPopup) : 0;
}

XUI_API int xuiCascaderSetShowAllLevels(xui_widget pWidget, int bShowAllLevels)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowAllLevels = bShowAllLevels ? 1 : 0;
	__xuiCascaderSyncDisplay(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetShowAllLevels(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->bShowAllLevels : 0;
}

XUI_API int xuiCascaderSetClearable(xui_widget pWidget, int bClearable)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bClearable = bClearable ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetClearable(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->bClearable : 0;
}

XUI_API int xuiCascaderSetSelectAnyLevel(xui_widget pWidget, int bSelectAnyLevel)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bSelectAnyLevel = bSelectAnyLevel ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiCascaderGetSelectAnyLevel(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->bSelectAnyLevel : 0;
}

XUI_API int xuiCascaderSetExpandTrigger(xui_widget pWidget, int iExpandTrigger)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || !__xuiCascaderExpandTriggerValid(iExpandTrigger) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iExpandTrigger = iExpandTrigger;
	return XUI_OK;
}

XUI_API int xuiCascaderGetExpandTrigger(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iExpandTrigger : XUI_CASCADER_EXPAND_CLICK;
}

XUI_API int xuiCascaderSetPopupSize(xui_widget pWidget, float fColumnWidth, float fPopupHeight, float fPopupMaxHeight)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || (fColumnWidth < 0.0f) || (fPopupHeight < 0.0f) || (fPopupMaxHeight < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fColumnWidth > 0.0f ) pData->fColumnWidth = fColumnWidth;
	pData->fPopupHeight = fPopupHeight;
	if ( fPopupMaxHeight > 0.0f ) pData->fPopupMaxHeight = fPopupMaxHeight;
	if ( pData->pPopup != NULL ) (void)__xuiCascaderApplyPopupStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiCascaderGetPopupSize(xui_widget pWidget, float* pColumnWidth, float* pPopupHeight, float* pPopupMaxHeight)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pColumnWidth != NULL ) *pColumnWidth = pData->fColumnWidth;
	if ( pPopupHeight != NULL ) *pPopupHeight = pData->fPopupHeight;
	if ( pPopupMaxHeight != NULL ) *pPopupMaxHeight = pData->fPopupMaxHeight;
	return XUI_OK;
}

XUI_API int xuiCascaderSetPopupPlacement(xui_widget pWidget, int iPlacement)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || !__xuiCascaderPlacementValid(iPlacement) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPlacement = iPlacement;
	if ( pData->pPopup != NULL ) return __xuiCascaderApplyPopupStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiCascaderGetPopupPlacement(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iPopupPlacement : XUI_CASCADER_POPUP_AUTO;
}

XUI_API int xuiCascaderSetMetrics(xui_widget pWidget, float fItemHeight, float fBorderWidth)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( (pData == NULL) || (fItemHeight <= 0.0f) || (fBorderWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fItemHeight = fItemHeight;
	pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetMetrics(xui_widget pWidget, float* pItemHeight, float* pBorderWidth)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiCascaderSetColors(xui_widget pWidget, uint32_t iText, uint32_t iPlaceholder, uint32_t iDisabledText, uint32_t iBackground, uint32_t iHoverBackground, uint32_t iOpenBackground, uint32_t iDisabledBackground)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iText;
	pData->iPlaceholderColor = iPlaceholder;
	pData->iDisabledTextColor = iDisabledText;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iOpenBackgroundColor = iOpenBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetColors(xui_widget pWidget, uint32_t* pText, uint32_t* pPlaceholder, uint32_t* pDisabledText, uint32_t* pBackground, uint32_t* pHoverBackground, uint32_t* pOpenBackground, uint32_t* pDisabledBackground)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pPlaceholder != NULL ) *pPlaceholder = pData->iPlaceholderColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pOpenBackground != NULL ) *pOpenBackground = pData->iOpenBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	return XUI_OK;
}

XUI_API int xuiCascaderSetBorderColors(xui_widget pWidget, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocusBorder)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iFocusBorderColor = iFocusBorder;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetBorderColors(xui_widget pWidget, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocusBorder)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pFocusBorder != NULL ) *pFocusBorder = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiCascaderSetArrowColors(xui_widget pWidget, uint32_t iArrow, uint32_t iDisabledArrow)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iArrowColor = iArrow;
	pData->iDisabledArrowColor = iDisabledArrow;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetArrowColors(xui_widget pWidget, uint32_t* pArrow, uint32_t* pDisabledArrow)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pArrow != NULL ) *pArrow = pData->iArrowColor;
	if ( pDisabledArrow != NULL ) *pDisabledArrow = pData->iDisabledArrowColor;
	return XUI_OK;
}

XUI_API int xuiCascaderSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iButtonHover, uint32_t iButtonOpen)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iButtonColor = iButton;
	pData->iButtonHoverColor = iButtonHover;
	pData->iButtonOpenColor = iButtonOpen;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCascaderGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pButtonHover, uint32_t* pButtonOpen)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pButton != NULL ) *pButton = pData->iButtonColor;
	if ( pButtonHover != NULL ) *pButtonHover = pData->iButtonHoverColor;
	if ( pButtonOpen != NULL ) *pButtonOpen = pData->iButtonOpenColor;
	return XUI_OK;
}

XUI_API int xuiCascaderSetPopupColors(xui_widget pWidget, uint32_t iPanel, uint32_t iBorder, uint32_t iShadow, uint32_t iText, uint32_t iMutedText, uint32_t iHover, uint32_t iActive, uint32_t iSelected, uint32_t iActiveText, uint32_t iDisabledText, uint32_t iSeparator)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPanelColor = iPanel;
	pData->iPopupBorderColor = iBorder;
	pData->iPopupShadowColor = iShadow;
	pData->iPopupTextColor = iText;
	pData->iPopupMutedTextColor = iMutedText;
	pData->iPopupHoverColor = iHover;
	pData->iPopupActiveColor = iActive;
	pData->iPopupSelectedColor = iSelected;
	pData->iPopupActiveTextColor = iActiveText;
	pData->iPopupDisabledTextColor = iDisabledText;
	pData->iPopupSeparatorColor = iSeparator;
	if ( pData->pPopup != NULL ) (void)__xuiCascaderApplyPopupStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiCascaderGetPopupColors(xui_widget pWidget, uint32_t* pPanel, uint32_t* pBorder, uint32_t* pShadow, uint32_t* pText, uint32_t* pMutedText, uint32_t* pHover, uint32_t* pActive, uint32_t* pSelected, uint32_t* pActiveText, uint32_t* pDisabledText, uint32_t* pSeparator)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPanel != NULL ) *pPanel = pData->iPopupPanelColor;
	if ( pBorder != NULL ) *pBorder = pData->iPopupBorderColor;
	if ( pShadow != NULL ) *pShadow = pData->iPopupShadowColor;
	if ( pText != NULL ) *pText = pData->iPopupTextColor;
	if ( pMutedText != NULL ) *pMutedText = pData->iPopupMutedTextColor;
	if ( pHover != NULL ) *pHover = pData->iPopupHoverColor;
	if ( pActive != NULL ) *pActive = pData->iPopupActiveColor;
	if ( pSelected != NULL ) *pSelected = pData->iPopupSelectedColor;
	if ( pActiveText != NULL ) *pActiveText = pData->iPopupActiveTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iPopupDisabledTextColor;
	if ( pSeparator != NULL ) *pSeparator = pData->iPopupSeparatorColor;
	return XUI_OK;
}

XUI_API int xuiCascaderSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiCascaderGetFont(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiCascaderSetPlaceholder(xui_widget pWidget, const char* sPlaceholder)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCascaderCopyText(pData->sPlaceholder, (int)sizeof(pData->sPlaceholder), sPlaceholder);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiCascaderGetPlaceholder(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->sPlaceholder : "";
}

XUI_API int xuiCascaderSetSeparator(xui_widget pWidget, const char* sSeparator)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCascaderCopyText(pData->sSeparator, (int)sizeof(pData->sSeparator), sSeparator);
	__xuiCascaderSyncDisplay(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiCascaderGetSeparator(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->sSeparator : "";
}

XUI_API xui_widget xuiCascaderGetPopupWidget(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->pPopup : NULL;
}

XUI_API xui_widget xuiCascaderGetPanelWidget(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->pPanel : NULL;
}

XUI_API int xuiCascaderGetColumnCount(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? __xuiCascaderColumnCount(pData) : 0;
}

XUI_API xui_rect_t xuiCascaderGetItemRect(xui_widget pWidget, int iColumn, int iItem)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiCascaderClampScrolls(pData);
	return __xuiCascaderPanelItemRect(pData, iColumn, iItem);
}

XUI_API xui_rect_t xuiCascaderGetButtonRect(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiCascaderUpdateOwnerRects(pWidget, pData);
	return pData->tButtonRect;
}

XUI_API xui_rect_t xuiCascaderGetClearRect(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiCascaderUpdateOwnerRects(pWidget, pData);
	return pData->tClearRect;
}

XUI_API xui_rect_t xuiCascaderGetTextRect(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiCascaderUpdateOwnerRects(pWidget, pData);
	return pData->tTextRect;
}

XUI_API uint32_t xuiCascaderGetState(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return __xuiCascaderState(pWidget, pData);
}

XUI_API int xuiCascaderGetChangeCount(xui_widget pWidget)
{
	xui_cascader_data_t* pData = __xuiCascaderGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
