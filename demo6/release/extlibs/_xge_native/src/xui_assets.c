#include "xui_internal.h"

#include <string.h>

typedef struct xui_builtin_asset_t {
	const char* sName;
	int iX;
	int iY;
	int iW;
	int iH;
} xui_builtin_asset_t;

#include "xui_builtin_assets.inc"

#define XUI_BUILTIN_ATLAS_RESOURCE_NAME "__xui_builtin_atlas"

static int __xuiBuiltinAssetValidIndex(int iIndex)
{
	return (iIndex >= 0) && (iIndex < XUI_BUILTIN_ASSET_COUNT);
}

static xui_rect_t __xuiBuiltinAssetRect(const xui_builtin_asset_t* pAsset)
{
	if ( pAsset == NULL ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return (xui_rect_t){(float)pAsset->iX, (float)pAsset->iY, (float)pAsset->iW, (float)pAsset->iH};
}

static void __xuiBuiltinAssetAtlasDestroy(xui_context pContext, void* pHandle, void* pUser)
{
	xui_proxy pProxy;

	(void)pUser;
	if ( (pHandle == NULL) || !xuiInternalContextIsValid(pContext) ) {
		return;
	}
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( (pProxy != NULL) && (pProxy->surfaceDestroy != NULL) ) {
		pProxy->surfaceDestroy(pProxy, (xui_surface)pHandle);
	}
}

XUI_API int xuiBuiltinAssetGetCount(void)
{
	return XUI_BUILTIN_ASSET_COUNT;
}

XUI_API const char* xuiBuiltinAssetGetName(int iIndex)
{
	return __xuiBuiltinAssetValidIndex(iIndex) ? g_arrXuiBuiltinAssets[iIndex].sName : NULL;
}

XUI_API int xuiBuiltinAssetGetAtlasSize(int* pWidth, int* pHeight)
{
	if ( (pWidth == NULL) && (pHeight == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidth != NULL ) {
		*pWidth = XUI_BUILTIN_ATLAS_WIDTH;
	}
	if ( pHeight != NULL ) {
		*pHeight = XUI_BUILTIN_ATLAS_HEIGHT;
	}
	return XUI_OK;
}

XUI_API int xuiBuiltinAssetGetRectByIndex(int iIndex, xui_rect_t* pRect)
{
	if ( pRect == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiBuiltinAssetValidIndex(iIndex) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pRect = __xuiBuiltinAssetRect(&g_arrXuiBuiltinAssets[iIndex]);
	return XUI_OK;
}

XUI_API int xuiBuiltinAssetGetRect(const char* sName, xui_rect_t* pRect)
{
	int i;

	if ( (sName == NULL) || (pRect == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < XUI_BUILTIN_ASSET_COUNT; i++ ) {
		if ( strcmp(g_arrXuiBuiltinAssets[i].sName, sName) == 0 ) {
			*pRect = __xuiBuiltinAssetRect(&g_arrXuiBuiltinAssets[i]);
			return XUI_OK;
		}
	}
	return XUI_ERROR_FILE_NOT_FOUND;
}

XUI_API int xuiBuiltinAssetGetAtlas(xui_context pContext, xui_surface* ppSurface)
{
	xui_proxy pProxy;
	xui_resource pResource;
	xui_surface pSurface;
	xui_resource_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppSurface == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppSurface = NULL;
	pResource = xuiResourceFind(pContext, XUI_BUILTIN_ATLAS_RESOURCE_NAME);
	if ( pResource != NULL ) {
		*ppSurface = (xui_surface)xuiResourceGetHandle(pResource);
		return (*ppSurface != NULL) ? XUI_OK : XUI_ERROR_RESOURCE_FAILED;
	}
	if ( !xuiInternalContextHasProxy(pContext) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( (pProxy == NULL) || (pProxy->surfaceLoadMemory == NULL) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	pSurface = NULL;
	iRet = pProxy->surfaceLoadMemory(pProxy, &pSurface, g_arrXuiBuiltinAtlasPng, g_iXuiBuiltinAtlasPngSize, XUI_SURFACE_ALPHA_PREMULTIPLIED);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = XUI_BUILTIN_ATLAS_RESOURCE_NAME;
	tDesc.iKind = XUI_RESOURCE_SURFACE;
	tDesc.pHandle = pSurface;
	tDesc.onDestroy = __xuiBuiltinAssetAtlasDestroy;
	iRet = xuiResourceSet(pContext, &pResource, &tDesc);
	if ( iRet != XUI_OK ) {
		if ( pSurface != NULL ) {
			pProxy->surfaceDestroy(pProxy, pSurface);
		}
		return iRet;
	}
	*ppSurface = pSurface;
	return XUI_OK;
}
