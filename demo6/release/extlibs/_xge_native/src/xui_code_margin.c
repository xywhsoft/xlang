#include "../xui.h"

#include <string.h>

struct xui_code_margin_model_t {
	xui_code_margin_desc_t* pMargins;
	int iCount;
	int iCapacity;
};

static int __xuiCodeMarginReserve(xui_code_margin_model pModel, int iCapacity)
{
	xui_code_margin_desc_t* pNew;

	if ( iCapacity <= pModel->iCapacity ) return XUI_OK;
	if ( iCapacity < pModel->iCapacity * 2 ) iCapacity = pModel->iCapacity * 2;
	if ( iCapacity < 8 ) iCapacity = 8;
	pNew = (xui_code_margin_desc_t*)xrtRealloc(pModel->pMargins, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pModel->pMargins = pNew;
	pModel->iCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeMarginFind(xui_code_margin_model pModel, int iId)
{
	int i;

	for ( i = 0; i < pModel->iCount; i++ ) {
		if ( pModel->pMargins[i].iId == iId ) return i;
	}
	return -1;
}

static int __xuiCodeMarginVisible(const xui_code_margin_desc_t* pDesc)
{
	return (pDesc->iFlags & XUI_CODE_MARGIN_VISIBLE) != 0 && pDesc->fWidth > 0.0f;
}

static void __xuiCodeMarginCopyInfo(xui_code_margin_info_t* pInfo, const xui_code_margin_desc_t* pDesc, xui_rect_t tRect)
{
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iId = pDesc->iId;
	pInfo->iKind = pDesc->iKind;
	pInfo->fWidth = pDesc->fWidth;
	pInfo->iFlags = pDesc->iFlags;
	pInfo->tRect = tRect;
	pInfo->onRender = pDesc->onRender;
	pInfo->onEvent = pDesc->onEvent;
	pInfo->pUser = pDesc->pUser;
}

static xui_code_margin_desc_t __xuiCodeMarginDefault(int iId, int iKind, float fWidth, uint32_t iFlags)
{
	xui_code_margin_desc_t tDesc;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.iId = iId;
	tDesc.iKind = iKind;
	tDesc.fWidth = fWidth;
	tDesc.iFlags = iFlags | XUI_CODE_MARGIN_VISIBLE;
	return tDesc;
}

XUI_API int xuiCodeMarginModelCreate(xui_code_margin_model* ppModel)
{
	xui_code_margin_model pModel;

	if ( ppModel == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppModel = NULL;
	pModel = (xui_code_margin_model)xrtMalloc(sizeof(*pModel));
	if ( pModel == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pModel, 0, sizeof(*pModel));
	*ppModel = pModel;
	return XUI_OK;
}

XUI_API void xuiCodeMarginModelDestroy(xui_code_margin_model pModel)
{
	if ( pModel == NULL ) return;
	xrtFree(pModel->pMargins);
	xrtFree(pModel);
}

XUI_API void xuiCodeMarginModelClear(xui_code_margin_model pModel)
{
	if ( pModel == NULL ) return;
	pModel->iCount = 0;
}

XUI_API int xuiCodeMarginModelLoadDefaults(xui_code_margin_model pModel, int bLineNumber, int bMarker, int bFold, int bDiagnostic)
{
	xui_code_margin_desc_t tDesc;
	int iRet;

	if ( pModel == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xuiCodeMarginModelClear(pModel);
	tDesc = __xuiCodeMarginDefault(1, XUI_CODE_MARGIN_CHANGE, 4.0f, 0);
	iRet = xuiCodeMarginModelAdd(pModel, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	if ( bMarker ) {
		tDesc = __xuiCodeMarginDefault(2, XUI_CODE_MARGIN_MARKER, 16.0f, XUI_CODE_MARGIN_CLICKABLE);
		iRet = xuiCodeMarginModelAdd(pModel, &tDesc);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( bFold ) {
		tDesc = __xuiCodeMarginDefault(3, XUI_CODE_MARGIN_FOLD, 14.0f, XUI_CODE_MARGIN_CLICKABLE);
		iRet = xuiCodeMarginModelAdd(pModel, &tDesc);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( bLineNumber ) {
		tDesc = __xuiCodeMarginDefault(4, XUI_CODE_MARGIN_LINE_NUMBER, 44.0f, 0);
		iRet = xuiCodeMarginModelAdd(pModel, &tDesc);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( bDiagnostic ) {
		tDesc = __xuiCodeMarginDefault(5, XUI_CODE_MARGIN_DIAGNOSTIC, 14.0f, XUI_CODE_MARGIN_CLICKABLE);
		iRet = xuiCodeMarginModelAdd(pModel, &tDesc);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelAdd(xui_code_margin_model pModel, const xui_code_margin_desc_t* pDesc)
{
	int iIndex;
	int iRet;

	if ( (pModel == NULL) || (pDesc == NULL) || (pDesc->iId <= 0) || (pDesc->iKind <= 0) || (pDesc->fWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeMarginFind(pModel, pDesc->iId);
	if ( iIndex >= 0 ) {
		pModel->pMargins[iIndex] = *pDesc;
		pModel->pMargins[iIndex].iSize = sizeof(pModel->pMargins[iIndex]);
		return XUI_OK;
	}
	iRet = __xuiCodeMarginReserve(pModel, pModel->iCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pModel->pMargins[pModel->iCount] = *pDesc;
	pModel->pMargins[pModel->iCount].iSize = sizeof(pModel->pMargins[pModel->iCount]);
	pModel->iCount++;
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelRemove(xui_code_margin_model pModel, int iId)
{
	int iIndex;

	if ( (pModel == NULL) || (iId <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeMarginFind(pModel, iId);
	if ( iIndex < 0 ) return XUI_ERROR_UNSUPPORTED;
	memmove(&pModel->pMargins[iIndex], &pModel->pMargins[iIndex + 1], sizeof(pModel->pMargins[iIndex]) * (size_t)(pModel->iCount - iIndex - 1));
	pModel->iCount--;
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelSetWidth(xui_code_margin_model pModel, int iId, float fWidth)
{
	int iIndex;

	if ( (pModel == NULL) || (iId <= 0) || (fWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeMarginFind(pModel, iId);
	if ( iIndex < 0 ) return XUI_ERROR_UNSUPPORTED;
	pModel->pMargins[iIndex].fWidth = fWidth;
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelSetVisible(xui_code_margin_model pModel, int iId, int bVisible)
{
	int iIndex;

	if ( (pModel == NULL) || (iId <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiCodeMarginFind(pModel, iId);
	if ( iIndex < 0 ) return XUI_ERROR_UNSUPPORTED;
	if ( bVisible ) pModel->pMargins[iIndex].iFlags |= XUI_CODE_MARGIN_VISIBLE;
	else pModel->pMargins[iIndex].iFlags &= ~XUI_CODE_MARGIN_VISIBLE;
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelGetCount(xui_code_margin_model pModel)
{
	return (pModel != NULL) ? pModel->iCount : 0;
}

XUI_API int xuiCodeMarginModelGet(xui_code_margin_model pModel, int iIndex, xui_code_margin_info_t* pInfo)
{
	xui_rect_t tRect;

	if ( (pModel == NULL) || (pInfo == NULL) || (iIndex < 0) || (iIndex >= pModel->iCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tRect, 0, sizeof(tRect));
	__xuiCodeMarginCopyInfo(pInfo, &pModel->pMargins[iIndex], tRect);
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelGetTotalWidth(xui_code_margin_model pModel, float* pWidth)
{
	float fWidth;
	int i;

	if ( (pModel == NULL) || (pWidth == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fWidth = 0.0f;
	for ( i = 0; i < pModel->iCount; i++ ) {
		if ( __xuiCodeMarginVisible(&pModel->pMargins[i]) ) fWidth += pModel->pMargins[i].fWidth;
	}
	*pWidth = fWidth;
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelGetRect(xui_code_margin_model pModel, int iId, xui_rect_t tViewport, xui_rect_t* pRect)
{
	xui_rect_t tTextRect;
	xui_code_margin_info_t tInfo;
	int i;
	int iCount;
	int iRet;

	if ( (pModel == NULL) || (iId <= 0) || (pRect == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiCodeMarginModelLayout(pModel, tViewport, &tInfo, 1, &iCount, &tTextRect);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < pModel->iCount; i++ ) {
		if ( pModel->pMargins[i].iId == iId ) {
			float fX = tViewport.fX;
			int j;
			for ( j = 0; j < i; j++ ) {
				if ( __xuiCodeMarginVisible(&pModel->pMargins[j]) ) fX += pModel->pMargins[j].fWidth;
			}
			if ( !__xuiCodeMarginVisible(&pModel->pMargins[i]) ) return XUI_ERROR_UNSUPPORTED;
			*pRect = (xui_rect_t){ fX, tViewport.fY, pModel->pMargins[i].fWidth, tViewport.fH };
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeMarginModelLayout(xui_code_margin_model pModel, xui_rect_t tViewport, xui_code_margin_info_t* pInfos, int iInfoCapacity, int* pInfoCount, xui_rect_t* pTextRect)
{
	xui_rect_t tRect;
	float fX;
	float fTotal;
	int i;
	int iCount;

	if ( (pModel == NULL) || (pInfoCount == NULL) || (iInfoCapacity < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	fX = tViewport.fX;
	iCount = 0;
	for ( i = 0; i < pModel->iCount; i++ ) {
		if ( !__xuiCodeMarginVisible(&pModel->pMargins[i]) ) continue;
		tRect = (xui_rect_t){ fX, tViewport.fY, pModel->pMargins[i].fWidth, tViewport.fH };
		if ( pInfos != NULL && iCount < iInfoCapacity ) __xuiCodeMarginCopyInfo(&pInfos[iCount], &pModel->pMargins[i], tRect);
		fX += pModel->pMargins[i].fWidth;
		iCount++;
	}
	*pInfoCount = iCount;
	if ( pTextRect != NULL ) {
		fTotal = fX - tViewport.fX;
		*pTextRect = (xui_rect_t){ tViewport.fX + fTotal, tViewport.fY, tViewport.fW - fTotal, tViewport.fH };
		if ( pTextRect->fW < 0.0f ) pTextRect->fW = 0.0f;
	}
	return XUI_OK;
}

XUI_API int xuiCodeMarginModelHitTest(xui_code_margin_model pModel, xui_rect_t tViewport, float fX, float fY, float fLineHeight, float fScrollY, xui_code_margin_hit_t* pHit)
{
	xui_rect_t tRect;
	float fCurrentX;
	int i;

	if ( (pModel == NULL) || (pHit == NULL) || (fLineHeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pHit, 0, sizeof(*pHit));
	pHit->iSize = sizeof(*pHit);
	if ( fY < tViewport.fY || fY >= tViewport.fY + tViewport.fH ) return XUI_ERROR_UNSUPPORTED;
	fCurrentX = tViewport.fX;
	for ( i = 0; i < pModel->iCount; i++ ) {
		if ( !__xuiCodeMarginVisible(&pModel->pMargins[i]) ) continue;
		tRect = (xui_rect_t){ fCurrentX, tViewport.fY, pModel->pMargins[i].fWidth, tViewport.fH };
		if ( fX >= tRect.fX && fX < tRect.fX + tRect.fW ) {
			pHit->iId = pModel->pMargins[i].iId;
			pHit->iKind = pModel->pMargins[i].iKind;
			pHit->iIndex = i;
			pHit->iLine = (int)((fY - tViewport.fY + fScrollY) / fLineHeight);
			pHit->tRect = tRect;
			return XUI_OK;
		}
		fCurrentX += pModel->pMargins[i].fWidth;
	}
	return XUI_ERROR_UNSUPPORTED;
}
