#include "xui_internal.h"

#include <string.h>

typedef struct xui_separator_data_t {
	uint32_t iColor;
	float fThickness;
	int iOrientation;
	int iAlign;
	int iLineStyle;
} xui_separator_data_t;

static uint32_t __xuiSeparatorColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static float __xuiSeparatorMaxFloat(float a, float b)
{
	return (a > b) ? a : b;
}

static int __xuiSeparatorOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_SEPARATOR_HORIZONTAL) ||
	       (iOrientation == XUI_SEPARATOR_VERTICAL);
}

static int __xuiSeparatorAlignValid(int iAlign)
{
	return (iAlign == XUI_ALIGN_START) ||
	       (iAlign == XUI_ALIGN_CENTER) ||
	       (iAlign == XUI_ALIGN_END);
}

static int __xuiSeparatorLineStyleValid(int iLineStyle)
{
	return (iLineStyle == XUI_SEPARATOR_SOLID) ||
	       (iLineStyle == XUI_SEPARATOR_DOT) ||
	       (iLineStyle == XUI_SEPARATOR_DASH) ||
	       (iLineStyle == XUI_SEPARATOR_DASH_DOT);
}

static int __xuiSeparatorDescValid(const xui_separator_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( pDesc->fThickness < 0.0f ) {
		return 0;
	}
	if ( !__xuiSeparatorOrientationValid(pDesc->iOrientation) ||
	     !__xuiSeparatorLineStyleValid(pDesc->iLineStyle) ) {
		return 0;
	}
	if ( (pDesc->iAlign != 0) && !__xuiSeparatorAlignValid(pDesc->iAlign) ) {
		return 0;
	}
	return 1;
}

static xui_separator_data_t* __xuiSeparatorGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "separator");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_separator_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiSeparatorStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiSeparatorStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static int __xuiSeparatorStyleInt(xui_widget pWidget, const char* sName, int* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_INT) ) {
		*pValue = tProperty.tValue.iInt;
		return 1;
	}
	return 0;
}

static void __xuiSeparatorResolve(xui_widget pWidget, const xui_separator_data_t* pData, xui_separator_data_t* pResolved)
{
	int iValue;

	*pResolved = *pData;
	(void)__xuiSeparatorStyleColor(pWidget, "separator.color", &pResolved->iColor);
	(void)__xuiSeparatorStyleFloat(pWidget, "separator.thickness", &pResolved->fThickness);
	iValue = pResolved->iOrientation;
	if ( __xuiSeparatorStyleInt(pWidget, "separator.orientation", &iValue) && __xuiSeparatorOrientationValid(iValue) ) {
		pResolved->iOrientation = iValue;
	}
	iValue = pResolved->iAlign;
	if ( __xuiSeparatorStyleInt(pWidget, "separator.align", &iValue) && __xuiSeparatorAlignValid(iValue) ) {
		pResolved->iAlign = iValue;
	}
	iValue = pResolved->iLineStyle;
	if ( __xuiSeparatorStyleInt(pWidget, "separator.line_style", &iValue) && __xuiSeparatorLineStyleValid(iValue) ) {
		pResolved->iLineStyle = iValue;
	}
	if ( pResolved->fThickness < 0.0f ) {
		pResolved->fThickness = 0.0f;
	}
}

static xui_rect_t __xuiSeparatorLineRectFromData(xui_widget pWidget, const xui_separator_data_t* pData)
{
	xui_rect_t tRect;
	float fThickness;

	memset(&tRect, 0, sizeof(tRect));
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return tRect;
	}
	tRect = xuiInternalSnapRect(xuiWidgetGetContentRect(pWidget));
	fThickness = xuiInternalSnapSize(pData->fThickness);
	if ( pData->iOrientation == XUI_SEPARATOR_VERTICAL ) {
		if ( fThickness > tRect.fW ) {
			fThickness = tRect.fW;
		}
		if ( pData->iAlign == XUI_ALIGN_CENTER ) {
			tRect.fX += (tRect.fW - fThickness) * 0.5f;
		} else if ( pData->iAlign == XUI_ALIGN_END ) {
			tRect.fX += tRect.fW - fThickness;
		}
		tRect.fW = fThickness;
	} else {
		if ( fThickness > tRect.fH ) {
			fThickness = tRect.fH;
		}
		if ( pData->iAlign == XUI_ALIGN_CENTER ) {
			tRect.fY += (tRect.fH - fThickness) * 0.5f;
		} else if ( pData->iAlign == XUI_ALIGN_END ) {
			tRect.fY += tRect.fH - fThickness;
		}
		tRect.fH = fThickness;
	}
	return xuiInternalSnapRect(tRect);
}

static int __xuiSeparatorDrawSegment(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tLine, int bVertical, float fStart, float fLength, uint32_t iColor)
{
	xui_rect_t tPart;
	float fLimit;

	if ( fLength <= 0.0f ) {
		return XUI_OK;
	}
	tPart = tLine;
	if ( bVertical ) {
		fLimit = tLine.fY + tLine.fH;
		tPart.fY = tLine.fY + fStart;
		if ( tPart.fY >= fLimit ) {
			return XUI_OK;
		}
		tPart.fH = fLength;
		if ( (tPart.fY + tPart.fH) > fLimit ) {
			tPart.fH = fLimit - tPart.fY;
		}
	} else {
		fLimit = tLine.fX + tLine.fW;
		tPart.fX = tLine.fX + fStart;
		if ( tPart.fX >= fLimit ) {
			return XUI_OK;
		}
		tPart.fW = fLength;
		if ( (tPart.fX + tPart.fW) > fLimit ) {
			tPart.fW = fLimit - tPart.fX;
		}
	}
	tPart = xuiInternalSnapRect(tPart);
	if ( (tPart.fW <= 0.0f) || (tPart.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tPart, iColor);
}

static int __xuiSeparatorDrawPattern(xui_proxy pProxy, xui_draw_context pDraw, const xui_separator_data_t* pData, xui_rect_t tLine)
{
	float fThickness;
	float fCursor;
	float fMain;
	float fDash;
	float fDot;
	float fGap;
	int bVertical;
	int iRet;

	bVertical = (pData->iOrientation == XUI_SEPARATOR_VERTICAL);
	fMain = bVertical ? tLine.fH : tLine.fW;
	if ( pData->iLineStyle == XUI_SEPARATOR_SOLID ) {
		return pProxy->drawRectFill(pProxy, pDraw, tLine, pData->iColor);
	}
	fThickness = __xuiSeparatorMaxFloat(xuiInternalSnapSize(pData->fThickness), 1.0f);
	fDot = fThickness;
	fDash = fThickness * 4.0f;
	fGap = fThickness * 2.0f;
	fCursor = 0.0f;
	while ( fCursor < fMain ) {
		if ( pData->iLineStyle == XUI_SEPARATOR_DOT ) {
			iRet = __xuiSeparatorDrawSegment(pProxy, pDraw, tLine, bVertical, fCursor, fDot, pData->iColor);
			if ( iRet != XUI_OK ) return iRet;
			fCursor += fDot + fThickness;
		} else if ( pData->iLineStyle == XUI_SEPARATOR_DASH ) {
			iRet = __xuiSeparatorDrawSegment(pProxy, pDraw, tLine, bVertical, fCursor, fDash, pData->iColor);
			if ( iRet != XUI_OK ) return iRet;
			fCursor += fDash + fGap;
		} else {
			iRet = __xuiSeparatorDrawSegment(pProxy, pDraw, tLine, bVertical, fCursor, fDash, pData->iColor);
			if ( iRet != XUI_OK ) return iRet;
			fCursor += fDash + fGap;
			iRet = __xuiSeparatorDrawSegment(pProxy, pDraw, tLine, bVertical, fCursor, fDot, pData->iColor);
			if ( iRet != XUI_OK ) return iRet;
			fCursor += fDot + fGap;
		}
	}
	return XUI_OK;
}

static int __xuiSeparatorContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_separator_data_t tResolved;
	float fThickness;

	(void)tConstraint;
	if ( (pWidget == NULL) || (pSize == NULL) || (pUser == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSeparatorResolve(pWidget, (xui_separator_data_t*)pUser, &tResolved);
	fThickness = xuiInternalSnapSize(tResolved.fThickness);
	pSize->fX = (tResolved.iOrientation == XUI_SEPARATOR_VERTICAL) ? fThickness : 0.0f;
	pSize->fY = (tResolved.iOrientation == XUI_SEPARATOR_VERTICAL) ? 0.0f : fThickness;
	return XUI_OK;
}

static int __xuiSeparatorCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_separator_data_t* pData;
	xui_separator_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tLine;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSeparatorResolve(pWidget, pData, &tResolved);
	if ( (__xuiSeparatorColorAlpha(tResolved.iColor) == 0) || (tResolved.fThickness <= 0.0f) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tLine = __xuiSeparatorLineRectFromData(pWidget, &tResolved);
	if ( (tLine.fW <= 0.0f) || (tLine.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return __xuiSeparatorDrawPattern(pProxy, pDraw, &tResolved, tLine);
}

static int __xuiSeparatorInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_separator_data_t* pData;
	const xui_separator_desc_t* pDesc;

	(void)pUser;
	pData = (xui_separator_data_t*)pTypeData;
	pDesc = (const xui_separator_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiSeparatorDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->iColor = XUI_COLOR_RGBA(90, 104, 122, 180);
	pData->fThickness = 1.0f;
	pData->iOrientation = XUI_SEPARATOR_HORIZONTAL;
	pData->iAlign = XUI_ALIGN_CENTER;
	pData->iLineStyle = XUI_SEPARATOR_SOLID;
	if ( pDesc != NULL ) {
		pData->iColor = (pDesc->iColor != 0) ? pDesc->iColor : pData->iColor;
		pData->fThickness = (pDesc->fThickness > 0.0f) ? pDesc->fThickness : pData->fThickness;
		pData->iOrientation = pDesc->iOrientation;
		if ( pDesc->iAlign != 0 ) {
			pData->iAlign = pDesc->iAlign;
		}
		pData->iLineStyle = pDesc->iLineStyle;
	}
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	return XUI_OK;
}

static void __xuiSeparatorDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	(void)pWidget;
	(void)pUser;
	if ( pTypeData != NULL ) {
		memset(pTypeData, 0, sizeof(xui_separator_data_t));
	}
}

static void __xuiSeparatorDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiSeparatorDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiSeparatorRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags)
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
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiSeparatorRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	__xuiSeparatorRegisterStyleProperty(pContext, pType, "separator.color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiSeparatorRegisterStyleProperty(pContext, pType, "separator.thickness", XUI_STYLE_VALUE_FLOAT, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiSeparatorRegisterStyleProperty(pContext, pType, "separator.orientation", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiSeparatorRegisterStyleProperty(pContext, pType, "separator.align", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiSeparatorRegisterStyleProperty(pContext, pType, "separator.line_style", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_widget_type xuiSeparatorGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "separator");
	if ( pType != NULL ) {
		__xuiSeparatorRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "separator";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_separator_data_t);
	tDesc.onInit = __xuiSeparatorInit;
	tDesc.onDestroy = __xuiSeparatorDestroy;
	tDesc.onContentMeasure = __xuiSeparatorContentMeasure;
	tDesc.onCacheRender = __xuiSeparatorCacheRender;
	__xuiSeparatorDefaultLayout(&tDesc.tLayout);
	__xuiSeparatorDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiSeparatorRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiSeparatorCreate(xui_context pContext, xui_widget* ppWidget, const xui_separator_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiSeparatorDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiSeparatorGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiSeparatorSetColor(xui_widget pWidget, uint32_t iColor)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iColor == iColor ) {
		return XUI_OK;
	}
	pData->iColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiSeparatorGetColor(xui_widget pWidget)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	return (pData != NULL) ? pData->iColor : 0;
}

XUI_API int xuiSeparatorSetThickness(xui_widget pWidget, float fThickness)
{
	xui_separator_data_t* pData;

	if ( fThickness < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->fThickness == fThickness ) {
		return XUI_OK;
	}
	pData->fThickness = fThickness;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiSeparatorGetThickness(xui_widget pWidget)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	return (pData != NULL) ? pData->fThickness : 0.0f;
}

XUI_API int xuiSeparatorSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_separator_data_t* pData;

	if ( !__xuiSeparatorOrientationValid(iOrientation) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iOrientation == iOrientation ) {
		return XUI_OK;
	}
	pData->iOrientation = iOrientation;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSeparatorGetOrientation(xui_widget pWidget)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	return (pData != NULL) ? pData->iOrientation : XUI_SEPARATOR_HORIZONTAL;
}

XUI_API int xuiSeparatorSetAlign(xui_widget pWidget, int iAlign)
{
	xui_separator_data_t* pData;

	if ( !__xuiSeparatorAlignValid(iAlign) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iAlign == iAlign ) {
		return XUI_OK;
	}
	pData->iAlign = iAlign;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSeparatorGetAlign(xui_widget pWidget)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	return (pData != NULL) ? pData->iAlign : XUI_ALIGN_START;
}

XUI_API int xuiSeparatorSetLineStyle(xui_widget pWidget, int iLineStyle)
{
	xui_separator_data_t* pData;

	if ( !__xuiSeparatorLineStyleValid(iLineStyle) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iLineStyle == iLineStyle ) {
		return XUI_OK;
	}
	pData->iLineStyle = iLineStyle;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSeparatorGetLineStyle(xui_widget pWidget)
{
	xui_separator_data_t* pData;

	pData = __xuiSeparatorGetData(pWidget);
	return (pData != NULL) ? pData->iLineStyle : XUI_SEPARATOR_SOLID;
}

XUI_API xui_rect_t xuiSeparatorGetLineRect(xui_widget pWidget)
{
	xui_separator_data_t* pData;
	xui_separator_data_t tResolved;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiSeparatorGetData(pWidget);
	if ( pData == NULL ) {
		return tRect;
	}
	__xuiSeparatorResolve(pWidget, pData, &tResolved);
	return __xuiSeparatorLineRectFromData(pWidget, &tResolved);
}
