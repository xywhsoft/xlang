#include "xui_internal.h"

#include <string.h>

typedef struct xui_image_data_t {
	xui_surface pSurface;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	uint32_t iColor;
	int iMode;
	int iAlignX;
	int iAlignY;
} xui_image_data_t;

static uint32_t __xuiImageColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiImageModeValid(int iMode)
{
	return (iMode == XUI_IMAGE_NATURAL) ||
	       (iMode == XUI_IMAGE_STRETCH) ||
	       (iMode == XUI_IMAGE_CONTAIN) ||
	       (iMode == XUI_IMAGE_COVER) ||
	       (iMode == XUI_IMAGE_SCALE_DOWN) ||
	       (iMode == XUI_IMAGE_CUSTOM);
}

static int __xuiImageAlignValid(int iAlign)
{
	return (iAlign == XUI_ALIGN_START) ||
	       (iAlign == XUI_ALIGN_CENTER) ||
	       (iAlign == XUI_ALIGN_END);
}

static int __xuiImageDescValid(const xui_image_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->tSrc.fW < 0.0f) || (pDesc->tSrc.fH < 0.0f) ||
	     (pDesc->tDst.fW < 0.0f) || (pDesc->tDst.fH < 0.0f) ) {
		return 0;
	}
	if ( !__xuiImageModeValid(pDesc->iMode) ) {
		return 0;
	}
	if ( !__xuiImageAlignValid(pDesc->iAlignX) || !__xuiImageAlignValid(pDesc->iAlignY) ) {
		return 0;
	}
	return 1;
}

static xui_image_data_t* __xuiImageGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( pWidget == NULL ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "image");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_image_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiImageStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiImageStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_proxy __xuiImageProxy(xui_widget pWidget)
{
	return xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
}

static int __xuiImageSurfaceDesc(xui_widget pWidget, xui_surface pSurface, xui_surface_desc_t* pDesc)
{
	xui_proxy pProxy;

	if ( pDesc == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	if ( pSurface == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = __xuiImageProxy(pWidget);
	if ( (pProxy == NULL) || (pProxy->surfaceGetDesc == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return pProxy->surfaceGetDesc(pProxy, pSurface, pDesc);
}

static xui_rect_t __xuiImageSourceRect(xui_widget pWidget, const xui_image_data_t* pData)
{
	xui_surface_desc_t tDesc;
	xui_rect_t tSrc;

	memset(&tSrc, 0, sizeof(tSrc));
	if ( pData == NULL ) {
		return tSrc;
	}
	if ( (pData->tSrc.fW > 0.0f) && (pData->tSrc.fH > 0.0f) ) {
		return xuiInternalSnapRect(pData->tSrc);
	}
	if ( __xuiImageSurfaceDesc(pWidget, pData->pSurface, &tDesc) == XUI_OK ) {
		tSrc.fW = (float)tDesc.iWidth;
		tSrc.fH = (float)tDesc.iHeight;
	}
	return xuiInternalSnapRect(tSrc);
}

static xui_rect_t __xuiImageAlignRect(xui_rect_t tContent, float fW, float fH, int iAlignX, int iAlignY)
{
	xui_rect_t tDst;

	tDst.fX = tContent.fX;
	tDst.fY = tContent.fY;
	tDst.fW = fW;
	tDst.fH = fH;
	if ( iAlignX == XUI_ALIGN_CENTER ) {
		tDst.fX = tContent.fX + (tContent.fW - fW) * 0.5f;
	} else if ( iAlignX == XUI_ALIGN_END ) {
		tDst.fX = tContent.fX + tContent.fW - fW;
	}
	if ( iAlignY == XUI_ALIGN_CENTER ) {
		tDst.fY = tContent.fY + (tContent.fH - fH) * 0.5f;
	} else if ( iAlignY == XUI_ALIGN_END ) {
		tDst.fY = tContent.fY + tContent.fH - fH;
	}
	return xuiInternalSnapRect(tDst);
}

static void __xuiImageResolve(xui_widget pWidget, const xui_image_data_t* pData, xui_image_data_t* pResolved)
{
	int iValue;

	*pResolved = *pData;
	(void)__xuiImageStyleColor(pWidget, "image.color", &pResolved->iColor);
	(void)__xuiImageStyleColor(pWidget, "image.tint", &pResolved->iColor);
	iValue = pResolved->iMode;
	if ( __xuiImageStyleInt(pWidget, "image.mode", &iValue) && __xuiImageModeValid(iValue) ) {
		pResolved->iMode = iValue;
	}
	iValue = pResolved->iAlignX;
	if ( __xuiImageStyleInt(pWidget, "image.align_x", &iValue) && __xuiImageAlignValid(iValue) ) {
		pResolved->iAlignX = iValue;
	}
	iValue = pResolved->iAlignY;
	if ( __xuiImageStyleInt(pWidget, "image.align_y", &iValue) && __xuiImageAlignValid(iValue) ) {
		pResolved->iAlignY = iValue;
	}
}

static xui_rect_t __xuiImageDrawRectFromData(xui_widget pWidget, const xui_image_data_t* pData)
{
	xui_rect_t tContent;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	float fSrcW;
	float fSrcH;
	float fScale;

	memset(&tDst, 0, sizeof(tDst));
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return tDst;
	}
	tContent = xuiWidgetGetContentRect(pWidget);
	tSrc = __xuiImageSourceRect(pWidget, pData);
	fSrcW = tSrc.fW;
	fSrcH = tSrc.fH;
	if ( (fSrcW <= 0.0f) || (fSrcH <= 0.0f) || (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		return tDst;
	}
	tDst = tContent;
	switch ( pData->iMode ) {
	case XUI_IMAGE_CUSTOM:
		tDst = pData->tDst;
		tDst.fX += tContent.fX;
		tDst.fY += tContent.fY;
		return xuiInternalSnapRect(tDst);
	case XUI_IMAGE_STRETCH:
		return xuiInternalSnapRect(tContent);
	case XUI_IMAGE_CONTAIN:
		fScale = tContent.fW / fSrcW;
		if ( (fSrcH * fScale) > tContent.fH ) {
			fScale = tContent.fH / fSrcH;
		}
		return __xuiImageAlignRect(tContent, fSrcW * fScale, fSrcH * fScale, pData->iAlignX, pData->iAlignY);
	case XUI_IMAGE_COVER:
		fScale = tContent.fW / fSrcW;
		if ( (fSrcH * fScale) < tContent.fH ) {
			fScale = tContent.fH / fSrcH;
		}
		return __xuiImageAlignRect(tContent, fSrcW * fScale, fSrcH * fScale, pData->iAlignX, pData->iAlignY);
	case XUI_IMAGE_SCALE_DOWN:
		if ( (fSrcW <= tContent.fW) && (fSrcH <= tContent.fH) ) {
			return __xuiImageAlignRect(tContent, fSrcW, fSrcH, pData->iAlignX, pData->iAlignY);
		}
		fScale = tContent.fW / fSrcW;
		if ( (fSrcH * fScale) > tContent.fH ) {
			fScale = tContent.fH / fSrcH;
		}
		return __xuiImageAlignRect(tContent, fSrcW * fScale, fSrcH * fScale, pData->iAlignX, pData->iAlignY);
	case XUI_IMAGE_NATURAL:
	default:
		return __xuiImageAlignRect(tContent, fSrcW, fSrcH, pData->iAlignX, pData->iAlignY);
	}
}

static int __xuiImageContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_image_data_t* pData;
	xui_rect_t tSrc;

	(void)tConstraint;
	if ( (pWidget == NULL) || (pSize == NULL) || (pUser == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_image_data_t*)pUser;
	tSrc = __xuiImageSourceRect(pWidget, pData);
	pSize->fX = tSrc.fW;
	pSize->fY = tSrc.fH;
	return XUI_OK;
}

static int __xuiImageCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_image_data_t* pData;
	xui_image_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tSrc;
	xui_rect_t tDst;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pSurface == NULL ) {
		return XUI_OK;
	}
	pProxy = __xuiImageProxy(pWidget);
	if ( (pProxy == NULL) || (pProxy->drawSurface == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiImageResolve(pWidget, pData, &tResolved);
	if ( (tResolved.pSurface == NULL) || (__xuiImageColorAlpha(tResolved.iColor) == 0) ) {
		return XUI_OK;
	}
	tSrc = __xuiImageSourceRect(pWidget, &tResolved);
	tDst = __xuiImageDrawRectFromData(pWidget, &tResolved);
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return pProxy->drawSurface(pProxy, pDraw, tResolved.pSurface, tSrc, tDst, tResolved.iColor, 0);
}

static int __xuiImageInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_image_data_t* pData;
	const xui_image_desc_t* pDesc;

	(void)pUser;
	pData = (xui_image_data_t*)pTypeData;
	pDesc = (const xui_image_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiImageDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->iColor = XUI_COLOR_WHITE;
	pData->iMode = XUI_IMAGE_NATURAL;
	pData->iAlignX = XUI_ALIGN_CENTER;
	pData->iAlignY = XUI_ALIGN_CENTER;
	if ( pDesc != NULL ) {
		pData->pSurface = pDesc->pSurface;
		pData->tSrc = pDesc->tSrc;
		pData->tDst = pDesc->tDst;
		pData->iColor = (pDesc->iColor != 0) ? pDesc->iColor : XUI_COLOR_WHITE;
		pData->iMode = pDesc->iMode;
		pData->iAlignX = pDesc->iAlignX;
		pData->iAlignY = pDesc->iAlignY;
	}
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	return XUI_OK;
}

static void __xuiImageDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	(void)pWidget;
	(void)pUser;
	if ( pTypeData != NULL ) {
		memset(pTypeData, 0, sizeof(xui_image_data_t));
	}
}

static void __xuiImageDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiImageDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiImageRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags)
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

static void __xuiImageRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	__xuiImageRegisterStyleProperty(pContext, pType, "image.color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiImageRegisterStyleProperty(pContext, pType, "image.tint", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiImageRegisterStyleProperty(pContext, pType, "image.mode", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiImageRegisterStyleProperty(pContext, pType, "image.align_x", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiImageRegisterStyleProperty(pContext, pType, "image.align_y", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_widget_type xuiImageGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "image");
	if ( pType != NULL ) {
		__xuiImageRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "image";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_image_data_t);
	tDesc.onInit = __xuiImageInit;
	tDesc.onDestroy = __xuiImageDestroy;
	tDesc.onContentMeasure = __xuiImageContentMeasure;
	tDesc.onCacheRender = __xuiImageCacheRender;
	__xuiImageDefaultLayout(&tDesc.tLayout);
	__xuiImageDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiImageRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiImageCreate(xui_context pContext, xui_widget* ppWidget, const xui_image_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiImageDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiImageGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiImageSetSurface(xui_widget pWidget, xui_surface pSurface)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pSurface == pSurface ) {
		return XUI_OK;
	}
	pData->pSurface = pSurface;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_surface xuiImageGetSurface(xui_widget pWidget)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	return (pData != NULL) ? pData->pSurface : NULL;
}

XUI_API int xuiImageSetSource(xui_widget pWidget, xui_rect_t tSrc)
{
	xui_image_data_t* pData;

	if ( (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->tSrc = tSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiImageSetSourceRect(xui_widget pWidget, float fX1, float fY1, float fX2, float fY2)
{
	return xuiImageSetSource(pWidget, (xui_rect_t){fX1, fY1, fX2 - fX1, fY2 - fY1});
}

XUI_API int xuiImageClearSource(xui_widget pWidget)
{
	return xuiImageSetSource(pWidget, (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f});
}

XUI_API xui_rect_t xuiImageGetSource(xui_widget pWidget)
{
	xui_image_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiImageGetData(pWidget);
	return (pData != NULL) ? pData->tSrc : tRect;
}

XUI_API int xuiImageSetColor(xui_widget pWidget, uint32_t iColor)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iColor == iColor ) {
		return XUI_OK;
	}
	pData->iColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiImageSetTint(xui_widget pWidget, uint32_t iColor)
{
	return xuiImageSetColor(pWidget, iColor);
}

XUI_API uint32_t xuiImageGetColor(xui_widget pWidget)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	return (pData != NULL) ? pData->iColor : 0;
}

XUI_API int xuiImageSetMode(xui_widget pWidget, int iMode)
{
	xui_image_data_t* pData;

	if ( !__xuiImageModeValid(iMode) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iMode == iMode ) {
		return XUI_OK;
	}
	pData->iMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiImageGetMode(xui_widget pWidget)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	return (pData != NULL) ? pData->iMode : XUI_IMAGE_NATURAL;
}

XUI_API int xuiImageSetAlign(xui_widget pWidget, int iAlignX, int iAlignY)
{
	xui_image_data_t* pData;

	if ( !__xuiImageAlignValid(iAlignX) || !__xuiImageAlignValid(iAlignY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pData->iAlignX == iAlignX) && (pData->iAlignY == iAlignY) ) {
		return XUI_OK;
	}
	pData->iAlignX = iAlignX;
	pData->iAlignY = iAlignY;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiImageGetAlign(xui_widget pWidget, int* pAlignX, int* pAlignY)
{
	xui_image_data_t* pData;

	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pAlignX != NULL ) {
		*pAlignX = pData->iAlignX;
	}
	if ( pAlignY != NULL ) {
		*pAlignY = pData->iAlignY;
	}
	return XUI_OK;
}

XUI_API int xuiImageSetCustomRect(xui_widget pWidget, float fX1, float fY1, float fX2, float fY2)
{
	xui_image_data_t* pData;
	xui_rect_t tDst;

	tDst = (xui_rect_t){fX1, fY1, fX2 - fX1, fY2 - fY1};
	if ( (tDst.fW < 0.0f) || (tDst.fH < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->tDst = tDst;
	pData->iMode = XUI_IMAGE_CUSTOM;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_rect_t xuiImageGetCustomRect(xui_widget pWidget)
{
	xui_image_data_t* pData;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiImageGetData(pWidget);
	return (pData != NULL) ? pData->tDst : tRect;
}

XUI_API xui_rect_t xuiImageGetDrawRect(xui_widget pWidget)
{
	xui_image_data_t* pData;
	xui_image_data_t tResolved;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	pData = __xuiImageGetData(pWidget);
	if ( pData == NULL ) {
		return tRect;
	}
	__xuiImageResolve(pWidget, pData, &tResolved);
	return __xuiImageDrawRectFromData(pWidget, &tResolved);
}
