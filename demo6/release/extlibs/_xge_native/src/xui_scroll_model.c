#include "xui_internal.h"

#include <string.h>

static int __xuiScrollModelFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_LAYOUT_UNBOUNDED) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiScrollModelSizeValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiScrollModelRectValid(xui_rect_t tRect)
{
	return __xuiScrollModelFloatValid(tRect.fX) &&
	       __xuiScrollModelFloatValid(tRect.fY) &&
	       __xuiScrollModelSizeValid(tRect.fW) &&
	       __xuiScrollModelSizeValid(tRect.fH);
}

static float __xuiScrollModelClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static float __xuiScrollModelMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiScrollModelValid(const xui_scroll_model_t* pModel)
{
	return (pModel != NULL) && ((pModel->iSize == 0) || (pModel->iSize >= sizeof(*pModel)));
}

static void __xuiScrollModelGetMaxOffsetValue(const xui_scroll_model_t* pModel, float* pMaxX, float* pMaxY)
{
	float fMaxX;
	float fMaxY;

	fMaxX = 0.0f;
	fMaxY = 0.0f;
	if ( pModel != NULL ) {
		fMaxX = __xuiScrollModelMaxFloat(0.0f, pModel->fContentWidth - pModel->tViewportRect.fW);
		fMaxY = __xuiScrollModelMaxFloat(0.0f, pModel->fContentHeight - pModel->tViewportRect.fH);
	}
	if ( pMaxX != NULL ) *pMaxX = fMaxX;
	if ( pMaxY != NULL ) *pMaxY = fMaxY;
}

static int __xuiScrollModelClampOffset(xui_scroll_model_t* pModel)
{
	float fMaxX;
	float fMaxY;
	float fOldX;
	float fOldY;

	if ( pModel == NULL ) {
		return 0;
	}
	fOldX = pModel->fScrollX;
	fOldY = pModel->fScrollY;
	__xuiScrollModelGetMaxOffsetValue(pModel, &fMaxX, &fMaxY);
	pModel->fScrollX = __xuiScrollModelClampFloat(pModel->fScrollX, 0.0f, fMaxX);
	pModel->fScrollY = __xuiScrollModelClampFloat(pModel->fScrollY, 0.0f, fMaxY);
	return (pModel->fScrollX != fOldX) || (pModel->fScrollY != fOldY);
}

XUI_API void xuiScrollModelInit(xui_scroll_model_t* pModel)
{
	if ( pModel == NULL ) {
		return;
	}
	memset(pModel, 0, sizeof(*pModel));
	pModel->iSize = sizeof(*pModel);
}

XUI_API int xuiScrollModelSetViewport(xui_scroll_model_t* pModel, xui_rect_t tViewport)
{
	if ( !__xuiScrollModelValid(pModel) || !__xuiScrollModelRectValid(tViewport) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pModel->iSize = sizeof(*pModel);
	pModel->tViewportRect = tViewport;
	(void)__xuiScrollModelClampOffset(pModel);
	return XUI_OK;
}

XUI_API xui_rect_t xuiScrollModelGetViewport(const xui_scroll_model_t* pModel)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( __xuiScrollModelValid(pModel) ) {
		tRect = pModel->tViewportRect;
	}
	return tRect;
}

XUI_API int xuiScrollModelSetContentSize(xui_scroll_model_t* pModel, float fWidth, float fHeight)
{
	if ( !__xuiScrollModelValid(pModel) || !__xuiScrollModelSizeValid(fWidth) || !__xuiScrollModelSizeValid(fHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pModel->iSize = sizeof(*pModel);
	pModel->fContentWidth = fWidth;
	pModel->fContentHeight = fHeight;
	(void)__xuiScrollModelClampOffset(pModel);
	return XUI_OK;
}

XUI_API int xuiScrollModelGetContentSize(const xui_scroll_model_t* pModel, float* pWidth, float* pHeight)
{
	if ( !__xuiScrollModelValid(pModel) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidth != NULL ) *pWidth = pModel->fContentWidth;
	if ( pHeight != NULL ) *pHeight = pModel->fContentHeight;
	return XUI_OK;
}

XUI_API int xuiScrollModelSetOffset(xui_scroll_model_t* pModel, float fOffsetX, float fOffsetY)
{
	if ( !__xuiScrollModelValid(pModel) || !__xuiScrollModelFloatValid(fOffsetX) || !__xuiScrollModelFloatValid(fOffsetY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pModel->iSize = sizeof(*pModel);
	pModel->fScrollX = fOffsetX;
	pModel->fScrollY = fOffsetY;
	(void)__xuiScrollModelClampOffset(pModel);
	return XUI_OK;
}

XUI_API int xuiScrollModelScrollBy(xui_scroll_model_t* pModel, float fDeltaX, float fDeltaY)
{
	if ( !__xuiScrollModelValid(pModel) || !__xuiScrollModelFloatValid(fDeltaX) || !__xuiScrollModelFloatValid(fDeltaY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiScrollModelSetOffset(pModel, pModel->fScrollX + fDeltaX, pModel->fScrollY + fDeltaY);
}

XUI_API int xuiScrollModelGetOffset(const xui_scroll_model_t* pModel, float* pOffsetX, float* pOffsetY)
{
	if ( !__xuiScrollModelValid(pModel) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pOffsetX != NULL ) *pOffsetX = pModel->fScrollX;
	if ( pOffsetY != NULL ) *pOffsetY = pModel->fScrollY;
	return XUI_OK;
}

XUI_API int xuiScrollModelGetMaxOffset(const xui_scroll_model_t* pModel, float* pMaxX, float* pMaxY)
{
	if ( !__xuiScrollModelValid(pModel) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollModelGetMaxOffsetValue(pModel, pMaxX, pMaxY);
	return XUI_OK;
}

XUI_API int xuiScrollModelEnsureRectVisible(xui_scroll_model_t* pModel, xui_rect_t tContentRect)
{
	float fOffsetX;
	float fOffsetY;

	if ( !__xuiScrollModelValid(pModel) || !__xuiScrollModelRectValid(tContentRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fOffsetX = pModel->fScrollX;
	fOffsetY = pModel->fScrollY;
	if ( tContentRect.fX < fOffsetX ) {
		fOffsetX = tContentRect.fX;
	} else if ( (tContentRect.fX + tContentRect.fW) > (fOffsetX + pModel->tViewportRect.fW) ) {
		fOffsetX = tContentRect.fX + tContentRect.fW - pModel->tViewportRect.fW;
	}
	if ( tContentRect.fY < fOffsetY ) {
		fOffsetY = tContentRect.fY;
	} else if ( (tContentRect.fY + tContentRect.fH) > (fOffsetY + pModel->tViewportRect.fH) ) {
		fOffsetY = tContentRect.fY + tContentRect.fH - pModel->tViewportRect.fH;
	}
	return xuiScrollModelSetOffset(pModel, fOffsetX, fOffsetY);
}

XUI_API xui_vec2_t xuiScrollModelScreenToViewport(const xui_scroll_model_t* pModel, float fX, float fY)
{
	xui_vec2_t tPoint;

	tPoint.fX = 0.0f;
	tPoint.fY = 0.0f;
	if ( __xuiScrollModelValid(pModel) ) {
		tPoint.fX = fX - pModel->tViewportRect.fX;
		tPoint.fY = fY - pModel->tViewportRect.fY;
	}
	return tPoint;
}

XUI_API xui_vec2_t xuiScrollModelViewportToContent(const xui_scroll_model_t* pModel, float fX, float fY)
{
	xui_vec2_t tPoint;

	tPoint.fX = 0.0f;
	tPoint.fY = 0.0f;
	if ( __xuiScrollModelValid(pModel) ) {
		tPoint.fX = fX + pModel->fScrollX;
		tPoint.fY = fY + pModel->fScrollY;
	}
	return tPoint;
}

XUI_API xui_vec2_t xuiScrollModelScreenToContent(const xui_scroll_model_t* pModel, float fX, float fY)
{
	xui_vec2_t tPoint;

	tPoint = xuiScrollModelScreenToViewport(pModel, fX, fY);
	return xuiScrollModelViewportToContent(pModel, tPoint.fX, tPoint.fY);
}

XUI_API xui_vec2_t xuiScrollModelContentToViewport(const xui_scroll_model_t* pModel, float fX, float fY)
{
	xui_vec2_t tPoint;

	tPoint.fX = 0.0f;
	tPoint.fY = 0.0f;
	if ( __xuiScrollModelValid(pModel) ) {
		tPoint.fX = fX - pModel->fScrollX;
		tPoint.fY = fY - pModel->fScrollY;
	}
	return tPoint;
}

XUI_API xui_vec2_t xuiScrollModelContentToScreen(const xui_scroll_model_t* pModel, float fX, float fY)
{
	xui_vec2_t tPoint;

	tPoint = xuiScrollModelContentToViewport(pModel, fX, fY);
	if ( __xuiScrollModelValid(pModel) ) {
		tPoint.fX += pModel->tViewportRect.fX;
		tPoint.fY += pModel->tViewportRect.fY;
	}
	return tPoint;
}
