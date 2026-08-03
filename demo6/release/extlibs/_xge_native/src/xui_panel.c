#include "xui_internal.h"

#include <string.h>

typedef struct xui_panel_data_t {
	xui_widget pHeader;
	xui_widget pIcon;
	xui_widget pTitle;
	xui_widget pClient;
	xui_font pFont;
	uint32_t iTitleColor;
	uint32_t iDisabledTitleColor;
	uint32_t iBackgroundColor;
	uint32_t iHeaderColor;
	uint32_t iClientColor;
	uint32_t iBorderColor;
	uint32_t iTitleFlags;
	float fHeaderHeight;
	float fHeaderGap;
	float fIconSize;
	float fBorderWidth;
	int bClipClient;
} xui_panel_data_t;

typedef struct xui_panel_resolved_t {
	xui_font pFont;
	uint32_t iTitleColor;
	uint32_t iDisabledTitleColor;
	uint32_t iBackgroundColor;
	uint32_t iHeaderColor;
	uint32_t iClientColor;
	uint32_t iBorderColor;
	uint32_t iTitleFlags;
	float fHeaderHeight;
	float fHeaderGap;
	float fIconSize;
	float fBorderWidth;
	int bClipClient;
} xui_panel_resolved_t;

static int __xuiPanelFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static xui_thickness_t __xuiPanelThickness(float fLeft, float fTop, float fRight, float fBottom)
{
	xui_thickness_t tValue;

	tValue.fLeft = fLeft;
	tValue.fTop = fTop;
	tValue.fRight = fRight;
	tValue.fBottom = fBottom;
	return tValue;
}

static int __xuiPanelDescValid(const xui_panel_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->tIconSrc.fW < 0.0f) || (pDesc->tIconSrc.fH < 0.0f) ) {
		return 0;
	}
	if ( ((pDesc->fHeaderHeight != 0.0f) && !__xuiPanelFloatValid(pDesc->fHeaderHeight)) ||
	     ((pDesc->fHeaderGap != 0.0f) && !__xuiPanelFloatValid(pDesc->fHeaderGap)) ||
	     ((pDesc->fIconSize != 0.0f) && !__xuiPanelFloatValid(pDesc->fIconSize)) ||
	     ((pDesc->fBorderWidth != 0.0f) && !__xuiPanelFloatValid(pDesc->fBorderWidth)) ) {
		return 0;
	}
	return 1;
}

static xui_panel_data_t* __xuiPanelGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "panel");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_panel_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiPanelStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiPanelStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiPanelFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static int __xuiPanelStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_font __xuiPanelStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiPanelResolve(xui_widget pWidget, const xui_panel_data_t* pData, xui_panel_resolved_t* pResolved)
{
	int iValue;

	memset(pResolved, 0, sizeof(*pResolved));
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTitleColor = pData->iTitleColor;
	pResolved->iDisabledTitleColor = pData->iDisabledTitleColor;
	pResolved->iBackgroundColor = pData->iBackgroundColor;
	pResolved->iHeaderColor = pData->iHeaderColor;
	pResolved->iClientColor = pData->iClientColor;
	pResolved->iBorderColor = pData->iBorderColor;
	pResolved->iTitleFlags = pData->iTitleFlags | XUI_TEXT_CLIP;
	pResolved->fHeaderHeight = pData->fHeaderHeight;
	pResolved->fHeaderGap = pData->fHeaderGap;
	pResolved->fIconSize = pData->fIconSize;
	pResolved->fBorderWidth = pData->fBorderWidth;
	pResolved->bClipClient = pData->bClipClient;

	(void)__xuiPanelStyleColor(pWidget, "text.color", &pResolved->iTitleColor);
	(void)__xuiPanelStyleColor(pWidget, "text.disabled_color", &pResolved->iDisabledTitleColor);
	(void)__xuiPanelStyleColor(pWidget, "panel.background.color", &pResolved->iBackgroundColor);
	(void)__xuiPanelStyleColor(pWidget, "panel.header.color", &pResolved->iHeaderColor);
	(void)__xuiPanelStyleColor(pWidget, "panel.client.color", &pResolved->iClientColor);
	(void)__xuiPanelStyleColor(pWidget, "panel.border.color", &pResolved->iBorderColor);
	(void)__xuiPanelStyleFloat(pWidget, "panel.header.height", &pResolved->fHeaderHeight);
	(void)__xuiPanelStyleFloat(pWidget, "panel.header.gap", &pResolved->fHeaderGap);
	(void)__xuiPanelStyleFloat(pWidget, "panel.icon.size", &pResolved->fIconSize);
	(void)__xuiPanelStyleFloat(pWidget, "panel.border.width", &pResolved->fBorderWidth);
	iValue = (int)pResolved->iTitleFlags;
	if ( __xuiPanelStyleInt(pWidget, "text.flags", &iValue) ) {
		pResolved->iTitleFlags = (uint32_t)iValue | XUI_TEXT_CLIP;
	}
	iValue = pResolved->bClipClient;
	if ( __xuiPanelStyleInt(pWidget, "panel.client.clip", &iValue) ) {
		pResolved->bClipClient = iValue ? 1 : 0;
	}
	pResolved->pFont = __xuiPanelStyleFont(pWidget, pResolved->pFont);
}

static int __xuiPanelSyncChildren(xui_widget pWidget, xui_panel_data_t* pData, const xui_panel_resolved_t* pResolved)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pHeader != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pHeader, pResolved->fHeaderHeight > 0.0f);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetPreferredSize(pData->pHeader, (xui_vec2_t){0.0f, pResolved->fHeaderHeight});
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetGap(pData->pHeader, pResolved->fHeaderGap);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pIcon != NULL ) {
		iRet = xuiWidgetSetPreferredSize(pData->pIcon, (xui_vec2_t){pResolved->fIconSize, pResolved->fIconSize});
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pTitle != NULL ) {
		iRet = xuiLabelSetFont(pData->pTitle, pResolved->pFont);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiLabelSetTextColor(pData->pTitle, pResolved->iTitleColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiLabelSetDisabledTextColor(pData->pTitle, pResolved->iDisabledTitleColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiLabelSetTextFlags(pData->pTitle, pResolved->iTitleFlags);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pClient != NULL ) {
		iRet = xuiWidgetSetOverflow(pData->pClient, pResolved->bClipClient ? XUI_OVERFLOW_CLIP : XUI_OVERFLOW_VISIBLE);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiPanelSyncResolved(xui_widget pWidget, xui_panel_data_t* pData, xui_panel_resolved_t* pResolved)
{
	__xuiPanelResolve(pWidget, pData, pResolved);
	return __xuiPanelSyncChildren(pWidget, pData, pResolved);
}

static int __xuiPanelDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (iColor & 0xffu) == 0u ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiPanelDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || ((iColor & 0xffu) == 0u) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectStroke != NULL) ) {
		return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiPanelCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_panel_data_t* pData;
	xui_panel_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tHeader;
	xui_rect_t tClient;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiPanelSyncResolved(pWidget, pData, &tResolved);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiPanelDrawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pData->pHeader != NULL) && xuiWidgetGetVisible(pData->pHeader) ) {
		tHeader = xuiInternalSnapRect(xuiWidgetGetRect(pData->pHeader));
		iRet = __xuiPanelDrawRectFill(pProxy, pDraw, tHeader, tResolved.iHeaderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pClient != NULL ) {
		tClient = xuiInternalSnapRect(xuiWidgetGetRect(pData->pClient));
		iRet = __xuiPanelDrawRectFill(pProxy, pDraw, tClient, tResolved.iClientColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return __xuiPanelDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iBorderColor);
}

static int __xuiPanelMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_panel_data_t* pData;
	xui_panel_resolved_t tResolved;
	xui_vec2_t tHeaderSize;
	xui_vec2_t tClientSize;
	float fClientConstraintH;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiPanelSyncResolved(pWidget, pData, &tResolved);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tHeaderSize, 0, sizeof(tHeaderSize));
	memset(&tClientSize, 0, sizeof(tClientSize));
	if ( (pData->pHeader != NULL) && (tResolved.fHeaderHeight > 0.0f) ) {
		iRet = xuiWidgetMeasure(pData->pHeader, (xui_vec2_t){tConstraint.fX, tResolved.fHeaderHeight}, &tHeaderSize);
		if ( iRet != XUI_OK ) return iRet;
		tHeaderSize.fY = tResolved.fHeaderHeight;
	}
	if ( pData->pClient != NULL ) {
		fClientConstraintH = (tConstraint.fY > tHeaderSize.fY) ? (tConstraint.fY - tHeaderSize.fY) : XUI_LAYOUT_UNBOUNDED;
		iRet = xuiWidgetMeasure(pData->pClient, (xui_vec2_t){tConstraint.fX, fClientConstraintH}, &tClientSize);
		if ( iRet != XUI_OK ) return iRet;
	}
	pSize->fX = (tHeaderSize.fX > tClientSize.fX) ? tHeaderSize.fX : tClientSize.fX;
	pSize->fY = tHeaderSize.fY + tClientSize.fY;
	if ( pSize->fX < 180.0f ) pSize->fX = 180.0f;
	if ( pSize->fY < tResolved.fHeaderHeight + 64.0f ) pSize->fY = tResolved.fHeaderHeight + 64.0f;
	return XUI_OK;
}

static int __xuiPanelArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_panel_data_t* pData;
	xui_panel_resolved_t tResolved;
	xui_rect_t tHeader;
	xui_rect_t tClient;
	float fHeaderHeight;
	int iRet;

	(void)pUser;
	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiPanelSyncResolved(pWidget, pData, &tResolved);
	if ( iRet != XUI_OK ) return iRet;
	fHeaderHeight = (tResolved.fHeaderHeight > tContentRect.fH) ? tContentRect.fH : tResolved.fHeaderHeight;
	if ( fHeaderHeight < 0.0f ) fHeaderHeight = 0.0f;
	if ( pData->pHeader != NULL ) {
		tHeader = (xui_rect_t){tContentRect.fX, tContentRect.fY, tContentRect.fW, fHeaderHeight};
		iRet = xuiWidgetArrange(pData->pHeader, tHeader);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pClient != NULL ) {
		tClient.fX = tContentRect.fX;
		tClient.fY = tContentRect.fY + fHeaderHeight;
		tClient.fW = tContentRect.fW;
		tClient.fH = (tContentRect.fH > fHeaderHeight) ? (tContentRect.fH - fHeaderHeight) : 0.0f;
		iRet = xuiWidgetArrange(pData->pClient, tClient);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static void __xuiPanelDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_COLUMN;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_HIDDEN;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 240.0f;
	pLayout->fPreferredHeight = 160.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiPanelDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiPanelDestroyInitChildren(xui_panel_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	if ( pData->pHeader != NULL ) {
		xuiWidgetDestroy(pData->pHeader);
		pData->pHeader = NULL;
	}
	if ( pData->pIcon != NULL ) {
		xuiWidgetDestroy(pData->pIcon);
		pData->pIcon = NULL;
	}
	if ( pData->pTitle != NULL ) {
		xuiWidgetDestroy(pData->pTitle);
		pData->pTitle = NULL;
	}
	if ( pData->pClient != NULL ) {
		xuiWidgetDestroy(pData->pClient);
		pData->pClient = NULL;
	}
}

static int __xuiPanelInitChildren(xui_widget pWidget, xui_panel_data_t* pData)
{
	xui_label_desc_t tLabelDesc;
	xui_image_desc_t tImageDesc;
	xui_widget pHeader;
	xui_widget pIcon;
	xui_widget pTitle;
	xui_widget pClient;
	int iRet;

	pHeader = NULL;
	pIcon = NULL;
	pTitle = NULL;
	pClient = NULL;
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pHeader);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pClient);
	memset(&tImageDesc, 0, sizeof(tImageDesc));
	tImageDesc.iSize = sizeof(tImageDesc);
	tImageDesc.iColor = XUI_COLOR_WHITE;
	tImageDesc.iMode = XUI_IMAGE_CONTAIN;
	tImageDesc.iAlignX = XUI_ALIGN_CENTER;
	tImageDesc.iAlignY = XUI_ALIGN_CENTER;
	if ( iRet == XUI_OK ) iRet = xuiImageCreate(xuiWidgetGetContext(pWidget), &pIcon, &tImageDesc);
	memset(&tLabelDesc, 0, sizeof(tLabelDesc));
	tLabelDesc.iSize = sizeof(tLabelDesc);
	tLabelDesc.pFont = pData->pFont;
	tLabelDesc.sText = "";
	tLabelDesc.iTextColor = pData->iTitleColor;
	tLabelDesc.iDisabledTextColor = pData->iDisabledTitleColor;
	tLabelDesc.iTextFlags = pData->iTitleFlags | XUI_TEXT_CLIP;
	if ( iRet == XUI_OK ) iRet = xuiLabelCreate(xuiWidgetGetContext(pWidget), &pTitle, &tLabelDesc);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pHeader);
		xuiWidgetDestroy(pClient);
		xuiWidgetDestroy(pIcon);
		xuiWidgetDestroy(pTitle);
		return iRet;
	}
	pData->pHeader = pHeader;
	pData->pIcon = pIcon;
	pData->pTitle = pTitle;
	pData->pClient = pClient;

	(void)xuiWidgetSetLayoutType(pHeader, XUI_LAYOUT_ROW);
	(void)xuiWidgetSetSizeMode(pHeader, XUI_SIZE_FILL, XUI_SIZE_FIXED);
	(void)xuiWidgetSetPreferredSize(pHeader, (xui_vec2_t){0.0f, pData->fHeaderHeight});
	(void)xuiWidgetSetPadding(pHeader, __xuiPanelThickness(8.0f, 0.0f, 8.0f, 0.0f));
	(void)xuiWidgetSetGap(pHeader, pData->fHeaderGap);
	(void)xuiWidgetSetAlign(pHeader, XUI_ALIGN_STRETCH, XUI_ALIGN_START);

	(void)xuiWidgetSetSizeMode(pIcon, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	(void)xuiWidgetSetPreferredSize(pIcon, (xui_vec2_t){pData->fIconSize, pData->fIconSize});
	(void)xuiWidgetSetAlign(pIcon, XUI_ALIGN_START, XUI_ALIGN_CENTER);
	(void)xuiWidgetSetVisible(pIcon, 0);

	(void)xuiWidgetSetSizeMode(pTitle, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetFlex(pTitle, 1.0f, 1.0f);
	(void)xuiWidgetSetAlign(pTitle, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);

	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_COLUMN);
	(void)xuiWidgetSetSizeMode(pClient, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetFlex(pClient, 1.0f, 1.0f);
	(void)xuiWidgetSetPadding(pClient, __xuiPanelThickness(8.0f, 8.0f, 8.0f, 8.0f));
	(void)xuiWidgetSetGap(pClient, 6.0f);
	(void)xuiWidgetSetOverflow(pClient, pData->bClipClient ? XUI_OVERFLOW_CLIP : XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetAlign(pClient, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);

	iRet = xuiWidgetAddChild(pWidget, pHeader);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pHeader, pIcon);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pHeader, pTitle);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pClient);
	if ( iRet != XUI_OK ) {
		__xuiPanelDestroyInitChildren(pData);
		return iRet;
	}
	return XUI_OK;
}

static int __xuiPanelInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_panel_data_t* pData;
	const xui_panel_desc_t* pDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || !__xuiPanelDescValid((const xui_panel_desc_t*)pCreateData) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_panel_data_t*)pTypeData;
	pDesc = (const xui_panel_desc_t*)pCreateData;
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->iTitleColor = (pDesc != NULL && pDesc->iTitleColor != 0) ? pDesc->iTitleColor : XUI_COLOR_RGBA(46, 70, 96, 255);
	pData->iDisabledTitleColor = (pDesc != NULL && pDesc->iDisabledTitleColor != 0) ? pDesc->iDisabledTitleColor : XUI_COLOR_RGBA(128, 141, 158, 255);
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iHeaderColor = (pDesc != NULL && pDesc->iHeaderColor != 0) ? pDesc->iHeaderColor : XUI_COLOR_RGBA(232, 243, 251, 255);
	pData->iClientColor = (pDesc != NULL && pDesc->iClientColor != 0) ? pDesc->iClientColor : XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(127, 196, 229, 255);
	pData->iTitleFlags = (pDesc != NULL && pDesc->iTitleFlags != 0) ? pDesc->iTitleFlags : (XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	pData->fHeaderHeight = (pDesc != NULL && pDesc->fHeaderHeight > 0.0f) ? pDesc->fHeaderHeight : 28.0f;
	pData->fHeaderGap = (pDesc != NULL && pDesc->fHeaderGap > 0.0f) ? pDesc->fHeaderGap : 6.0f;
	pData->fIconSize = (pDesc != NULL && pDesc->fIconSize > 0.0f) ? pDesc->fIconSize : 16.0f;
	pData->fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : 1.0f;
	pData->bClipClient = (pDesc == NULL) ? 1 : (pDesc->bClipClient ? 1 : 0);
	iRet = __xuiPanelInitChildren(pWidget, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pDesc != NULL ) {
		if ( pDesc->sTitle != NULL ) {
			iRet = xuiLabelSetText(pData->pTitle, pDesc->sTitle);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pDesc->pIconSurface != NULL ) {
			iRet = xuiPanelSetIcon(pWidget, pDesc->pIconSurface, pDesc->tIconSrc);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiPanelDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_panel_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_panel_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiPanelRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiPanelRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiPanelRegisterStyleProperty(pContext, pType, "text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiPanelRegisterStyleProperty(pContext, pType, "text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiPanelRegisterStyleProperty(pContext, pType, "text.flags", XUI_STYLE_VALUE_INT, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiPanelRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.header.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.client.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.header.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.header.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.icon.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPanelRegisterStyleProperty(pContext, pType, "panel.client.clip", XUI_STYLE_VALUE_BOOL, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiPanelGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "panel");
	if ( pType != NULL ) {
		__xuiPanelRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "panel";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_panel_data_t);
	tDesc.onInit = __xuiPanelInit;
	tDesc.onDestroy = __xuiPanelDestroy;
	tDesc.onLayoutMeasure = __xuiPanelMeasure;
	tDesc.onLayoutArrange = __xuiPanelArrange;
	tDesc.onCacheRender = __xuiPanelCacheRender;
	__xuiPanelDefaultLayout(&tDesc.tLayout);
	__xuiPanelDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiPanelRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiPanelCreate(xui_context pContext, xui_widget* ppWidget, const xui_panel_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiPanelDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiPanelGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_widget xuiPanelGetHeaderWidget(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->pHeader : NULL;
}

XUI_API xui_widget xuiPanelGetIconWidget(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->pIcon : NULL;
}

XUI_API xui_widget xuiPanelGetTitleWidget(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->pTitle : NULL;
}

XUI_API xui_widget xuiPanelGetClientWidget(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->pClient : NULL;
}

XUI_API int xuiPanelAddChild(xui_widget pWidget, xui_widget pChild)
{
	xui_widget pClient;

	pClient = xuiPanelGetClientWidget(pWidget);
	if ( pClient == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetAddChild(pClient, pChild);
}

XUI_API int xuiPanelInsertBefore(xui_widget pWidget, xui_widget pChild, xui_widget pBefore)
{
	xui_widget pClient;

	pClient = xuiPanelGetClientWidget(pWidget);
	if ( pClient == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetInsertBefore(pClient, pChild, pBefore);
}

XUI_API int xuiPanelSetTitle(xui_widget pWidget, const char* sTitle)
{
	xui_panel_data_t* pData;
	int iRet;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiLabelSetText(pData->pTitle, (sTitle != NULL) ? sTitle : "");
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiPanelGetTitle(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? xuiLabelGetText(pData->pTitle) : NULL;
}

XUI_API int xuiPanelSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pFont = pFont;
	(void)xuiLabelSetFont(pData->pTitle, pFont);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiPanelGetFont(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiPanelSetTitleColor(xui_widget pWidget, uint32_t iColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iTitleColor = iColor;
	(void)xuiLabelSetTextColor(pData->pTitle, iColor);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetTitleColor(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iTitleColor : 0u;
}

XUI_API int xuiPanelSetDisabledTitleColor(xui_widget pWidget, uint32_t iColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iDisabledTitleColor = iColor;
	(void)xuiLabelSetDisabledTextColor(pData->pTitle, iColor);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetDisabledTitleColor(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iDisabledTitleColor : 0u;
}

XUI_API int xuiPanelSetTitleAlign(xui_widget pWidget, uint32_t iTextFlags)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iTitleFlags = iTextFlags | XUI_TEXT_CLIP;
	(void)xuiLabelSetTextFlags(pData->pTitle, pData->iTitleFlags);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetTitleAlign(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iTitleFlags : 0u;
}

XUI_API int xuiPanelSetIcon(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_panel_data_t* pData;
	int iRet;

	pData = __xuiPanelGetData(pWidget);
	if ( (pData == NULL) || (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiImageSetSurface(pData->pIcon, pSurface);
	if ( iRet == XUI_OK ) iRet = xuiImageSetSource(pData->pIcon, tSrc);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pIcon, pSurface != NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_surface xuiPanelGetIconSurface(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? xuiImageGetSurface(pData->pIcon) : NULL;
}

XUI_API xui_rect_t xuiPanelGetIconSource(xui_widget pWidget)
{
	xui_panel_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? xuiImageGetSource(pData->pIcon) : tRect;
}

XUI_API int xuiPanelSetIconSize(xui_widget pWidget, float fSize)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiPanelFloatValid(fSize) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fSize < 1.0f ) {
		fSize = 1.0f;
	}
	pData->fIconSize = fSize;
	(void)xuiWidgetSetPreferredSize(pData->pIcon, (xui_vec2_t){fSize, fSize});
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiPanelGetIconSize(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->fIconSize : 0.0f;
}

XUI_API int xuiPanelSetBackgroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetBackgroundColor(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iBackgroundColor : 0u;
}

XUI_API int xuiPanelSetHeaderColor(xui_widget pWidget, uint32_t iColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iHeaderColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetHeaderColor(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iHeaderColor : 0u;
}

XUI_API int xuiPanelSetClientColor(xui_widget pWidget, uint32_t iColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iClientColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPanelGetClientColor(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->iClientColor : 0u;
}

XUI_API int xuiPanelSetBorder(xui_widget pWidget, float fBorderWidth, uint32_t iBorderColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiPanelFloatValid(fBorderWidth) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth;
	pData->iBorderColor = iBorderColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPanelGetBorder(xui_widget pWidget, float* pBorderWidth, uint32_t* pBorderColor)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pBorderColor != NULL ) *pBorderColor = pData->iBorderColor;
	return XUI_OK;
}

XUI_API int xuiPanelSetHeaderHeight(xui_widget pWidget, float fHeight)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiPanelFloatValid(fHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fHeaderHeight = fHeight;
	(void)xuiWidgetSetVisible(pData->pHeader, fHeight > 0.0f);
	(void)xuiWidgetSetPreferredSize(pData->pHeader, (xui_vec2_t){0.0f, fHeight});
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiPanelGetHeaderHeight(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->fHeaderHeight : 0.0f;
}

XUI_API int xuiPanelSetHeaderGap(xui_widget pWidget, float fGap)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiPanelFloatValid(fGap) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fHeaderGap = fGap;
	(void)xuiWidgetSetGap(pData->pHeader, fGap);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiPanelGetHeaderGap(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->fHeaderGap : 0.0f;
}

XUI_API int xuiPanelSetClientClip(xui_widget pWidget, int bClip)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bClipClient = bClip ? 1 : 0;
	(void)xuiWidgetSetOverflow(pData->pClient, pData->bClipClient ? XUI_OVERFLOW_CLIP : XUI_OVERFLOW_VISIBLE);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPanelGetClientClip(xui_widget pWidget)
{
	xui_panel_data_t* pData;

	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL) ? pData->bClipClient : 0;
}

XUI_API xui_rect_t xuiPanelGetHeaderRect(xui_widget pWidget)
{
	xui_panel_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL && pData->pHeader != NULL) ? xuiWidgetGetRect(pData->pHeader) : tRect;
}

XUI_API xui_rect_t xuiPanelGetIconRect(xui_widget pWidget)
{
	xui_panel_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL && pData->pIcon != NULL) ? xuiWidgetGetRect(pData->pIcon) : tRect;
}

XUI_API xui_rect_t xuiPanelGetTitleRect(xui_widget pWidget)
{
	xui_panel_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL && pData->pTitle != NULL) ? xuiWidgetGetRect(pData->pTitle) : tRect;
}

XUI_API xui_rect_t xuiPanelGetClientRect(xui_widget pWidget)
{
	xui_panel_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiPanelGetData(pWidget);
	return (pData != NULL && pData->pClient != NULL) ? xuiWidgetGetRect(pData->pClient) : tRect;
}

XUI_API uint32_t xuiPanelGetState(xui_widget pWidget)
{
	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return 0u;
	}
	return xuiWidgetGetInputState(pWidget) | (xuiWidgetGetEnabled(pWidget) ? 0u : XUI_WIDGET_STATE_DISABLED);
}
