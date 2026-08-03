#include "xui_internal.h"

#include <string.h>

#define XUI_ICON_CATEGORY_MAGIC 0x58494341u
#define XUI_ICON_MAGIC 0x5849434Fu
#define XUI_ICON_DEFAULT_SIZE 16.0f
#define XUI_ICON_DEFAULT_CACHE_CAPACITY 4
#define XUI_ICON_MAX_CACHE_CAPACITY 64
#define XUI_ICON_ALIAS_DEPTH_MAX 32

typedef struct xui_icon_cache_entry_t {
	xui_surface pSurface;
	int iPixelWidth;
	int iPixelHeight;
	uint32_t iLastUse;
} xui_icon_cache_entry_t;

struct xui_icon_category_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_icon_category pNextAll;
	char* sName;
	xdict_struct mapIcons;
	xarray_struct arrIcons;
	xui_icon_category_desc_t tDesc;
	uint32_t iGeneration;
	int iRefCount;
	int iIconCount;
	int iUpdateDepth;
	int bUpdateDirty;
	int bRegistered;
};

struct xui_icon_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_icon_category pCategory;
	xui_icon pNextAll;
	xui_icon_id iId;
	char* sName;
	char* sDisplayName;
	char* sTags;
	uint32_t iFlags;
	void* pUser;
	int iSourceType;
	uint32_t iGeneration;
	int iRefCount;
	int bRegistered;
	char* sPath;
	void* pMemory;
	int iMemorySize;
	xui_rect_t tSource;
	xui_rect_t tViewBox;
	xui_path_style_t tPathStyle;
	float* pDashPattern;
	xui_surface pSurface;
	int bOwnSurface;
	xui_icon pAlias;
	xui_icon_custom_desc_t tCustom;
	xui_icon_cache_entry_t* pCache;
	int iCacheCount;
	int iCacheCapacity;
	uint32_t iCacheClock;
};

static int __xuiIconContextValid(xui_context pContext)
{
	return (pContext != NULL) && (pContext->iMagic == XUI_CONTEXT_MAGIC);
}

static int __xuiIconCategoryValid(xui_icon_category pCategory)
{
	return (pCategory != NULL) &&
	       (pCategory->iMagic == XUI_ICON_CATEGORY_MAGIC) &&
	       __xuiIconContextValid(pCategory->pContext);
}

static int __xuiIconValid(xui_icon pIcon)
{
	return (pIcon != NULL) &&
	       (pIcon->iMagic == XUI_ICON_MAGIC) &&
	       __xuiIconContextValid(pIcon->pContext);
}

static int __xuiIconFloatPositive(float fValue)
{
	return (fValue == fValue) && (fValue > 0.0f) && (fValue < XUI_CONTEXT_MAX_VIEWPORT);
}

static char* __xuiIconStringDuplicate(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText) + 1u;
	sCopy = (char*)xrtMalloc(iSize);
	if ( sCopy != NULL ) {
		memcpy(sCopy, sText, iSize);
	}
	return sCopy;
}

static void* __xuiIconMemoryDuplicate(const void* pData, int iSize)
{
	void* pCopy;

	if ( (pData == NULL) || (iSize <= 0) ) {
		return NULL;
	}
	pCopy = xrtMalloc((size_t)iSize);
	if ( pCopy != NULL ) {
		memcpy(pCopy, pData, (size_t)iSize);
	}
	return pCopy;
}

static int __xuiIconFitModeValid(int iFitMode)
{
	return (iFitMode >= XUI_IMAGE_NATURAL) && (iFitMode <= XUI_IMAGE_CUSTOM);
}

static int __xuiIconCategoryDescValid(const xui_icon_category_desc_t* pDesc)
{
	if ( (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iSizeMode != XUI_ICON_SIZE_FIXED) &&
	     (pDesc->iSizeMode != XUI_ICON_SIZE_UNRESTRICTED) ) {
		return 0;
	}
	if ( (pDesc->iSizeMode == XUI_ICON_SIZE_FIXED) &&
	     (!__xuiIconFloatPositive(pDesc->fWidth) ||
	      !__xuiIconFloatPositive(pDesc->fHeight)) ) {
		return 0;
	}
	if ( !__xuiIconFitModeValid(pDesc->iFitMode) ) {
		return 0;
	}
	if ( (pDesc->iCacheCapacity < 0) ||
	     (pDesc->iCacheCapacity > XUI_ICON_MAX_CACHE_CAPACITY) ) {
		return 0;
	}
	return 1;
}

static int __xuiIconDescValid(const xui_icon_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiIconDrawDescValid(const xui_icon_draw_desc_t* pDesc)
{
	return (pDesc == NULL) ||
	       ((pDesc->iSize >= sizeof(*pDesc)) &&
	        ((pDesc->iFitMode == -1) || __xuiIconFitModeValid(pDesc->iFitMode)));
}

static int __xuiIconCacheCapacity(xui_icon pIcon)
{
	if ( __xuiIconCategoryValid(pIcon->pCategory) ) {
		if ( pIcon->pCategory->tDesc.iCacheCapacity > 0 ) {
			return pIcon->pCategory->tDesc.iCacheCapacity;
		}
	}
	return XUI_ICON_DEFAULT_CACHE_CAPACITY;
}

static void __xuiIconDestroySurface(xui_icon pIcon, xui_surface pSurface)
{
	xui_proxy pProxy;

	if ( (pIcon == NULL) || (pSurface == NULL) ||
	     !__xuiIconContextValid(pIcon->pContext) ||
	     !pIcon->pContext->bHasProxy ) {
		return;
	}
	pProxy = &pIcon->pContext->tProxy;
	if ( pProxy->surfaceDestroy != NULL ) {
		pProxy->surfaceDestroy(pProxy, pSurface);
	}
}

static void __xuiIconClearCache(xui_icon pIcon)
{
	int i;

	if ( pIcon == NULL ) {
		return;
	}
	for ( i = 0; i < pIcon->iCacheCount; i++ ) {
		__xuiIconDestroySurface(pIcon, pIcon->pCache[i].pSurface);
	}
	if ( pIcon->pCache != NULL ) {
		xrtFree(pIcon->pCache);
	}
	pIcon->pCache = NULL;
	pIcon->iCacheCount = 0;
	pIcon->iCacheCapacity = 0;
	pIcon->iCacheClock = 0;
}

static void __xuiIconClearLoadedSurface(xui_icon pIcon)
{
	if ( (pIcon != NULL) && (pIcon->pSurface != NULL) && pIcon->bOwnSurface ) {
		__xuiIconDestroySurface(pIcon, pIcon->pSurface);
	}
	if ( pIcon != NULL ) {
		pIcon->pSurface = NULL;
		pIcon->bOwnSurface = 0;
	}
}

static void __xuiIconUnlinkAll(xui_icon pIcon)
{
	xui_icon pScan;
	xui_icon pPrev;
	xui_context pContext;

	if ( (pIcon == NULL) || !__xuiIconContextValid(pIcon->pContext) ) {
		return;
	}
	pContext = pIcon->pContext;
	pPrev = NULL;
	for ( pScan = pContext->pIcons; pScan != NULL; pScan = pScan->pNextAll ) {
		if ( pScan == pIcon ) {
			if ( pPrev != NULL ) {
				pPrev->pNextAll = pScan->pNextAll;
			} else {
				pContext->pIcons = pScan->pNextAll;
			}
			return;
		}
		pPrev = pScan;
	}
}

static void __xuiIconCategoryUnlinkAll(xui_icon_category pCategory)
{
	xui_icon_category pScan;
	xui_icon_category pPrev;
	xui_context pContext;

	if ( (pCategory == NULL) || !__xuiIconContextValid(pCategory->pContext) ) {
		return;
	}
	pContext = pCategory->pContext;
	pPrev = NULL;
	for ( pScan = pContext->pIconCategories; pScan != NULL; pScan = pScan->pNextAll ) {
		if ( pScan == pCategory ) {
			if ( pPrev != NULL ) {
				pPrev->pNextAll = pScan->pNextAll;
			} else {
				pContext->pIconCategories = pScan->pNextAll;
			}
			return;
		}
		pPrev = pScan;
	}
}

static void __xuiIconDestroyOne(xui_icon pIcon)
{
	if ( pIcon == NULL ) {
		return;
	}
	__xuiIconUnlinkAll(pIcon);
	__xuiIconClearCache(pIcon);
	if ( (pIcon->iSourceType == XUI_ICON_SOURCE_SURFACE) &&
	     (pIcon->pSurface != NULL) && pIcon->bOwnSurface ) {
		__xuiIconDestroySurface(pIcon, pIcon->pSurface);
	}
	if ( (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_FILE) ||
	     (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_MEMORY) ) {
		__xuiIconClearLoadedSurface(pIcon);
	}
	if ( pIcon->pAlias != NULL ) {
		xui_icon pAlias;

		pAlias = pIcon->pAlias;
		pIcon->pAlias = NULL;
		if ( __xuiIconValid(pAlias) ) {
			pAlias->iRefCount--;
			if ( pAlias->iRefCount <= 0 ) {
				__xuiIconDestroyOne(pAlias);
			}
		}
	}
	if ( (pIcon->iSourceType == XUI_ICON_SOURCE_CUSTOM) &&
	     (pIcon->tCustom.onDestroy != NULL) ) {
		pIcon->tCustom.onDestroy(pIcon, pIcon->tCustom.pUser);
	}
	if ( pIcon->sName != NULL ) xrtFree(pIcon->sName);
	if ( pIcon->sDisplayName != NULL ) xrtFree(pIcon->sDisplayName);
	if ( pIcon->sTags != NULL ) xrtFree(pIcon->sTags);
	if ( pIcon->sPath != NULL ) xrtFree(pIcon->sPath);
	if ( pIcon->pMemory != NULL ) xrtFree(pIcon->pMemory);
	if ( pIcon->pDashPattern != NULL ) xrtFree(pIcon->pDashPattern);
	pIcon->iMagic = 0;
	xrtFree(pIcon);
}

static void __xuiIconReleaseOwner(xui_icon pIcon)
{
	if ( !__xuiIconValid(pIcon) ) {
		return;
	}
	pIcon->iRefCount--;
	if ( pIcon->iRefCount <= 0 ) {
		__xuiIconDestroyOne(pIcon);
	}
}

static void __xuiIconCategoryDestroyOne(xui_icon_category pCategory)
{
	uint32_t i;

	if ( pCategory == NULL ) {
		return;
	}
	for ( i = 1; i <= pCategory->arrIcons.Count; i++ ) {
		xui_icon* ppIcon;

		ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, i);
		if ( (ppIcon != NULL) && (*ppIcon != NULL) ) {
			xui_icon pIcon;

			pIcon = *ppIcon;
			*ppIcon = NULL;
			pIcon->bRegistered = 0;
			pIcon->pCategory = NULL;
			__xuiIconReleaseOwner(pIcon);
		}
	}
	pCategory->iIconCount = 0;
	__xuiIconCategoryUnlinkAll(pCategory);
	xrtDictUnit(&pCategory->mapIcons);
	xrtArrayUnit(&pCategory->arrIcons);
	if ( pCategory->sName != NULL ) xrtFree(pCategory->sName);
	pCategory->iMagic = 0;
	xrtFree(pCategory);
}

static void __xuiIconCategoryReleaseOwner(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) ) {
		return;
	}
	pCategory->iRefCount--;
	if ( pCategory->iRefCount <= 0 ) {
		__xuiIconCategoryDestroyOne(pCategory);
	}
}

static void __xuiIconCategoryChanged(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) ) {
		return;
	}
	if ( pCategory->iUpdateDepth > 0 ) {
		pCategory->bUpdateDirty = 1;
		return;
	}
	pCategory->iGeneration++;
	if ( pCategory->iGeneration == 0 ) pCategory->iGeneration = 1;
	pCategory->pContext->iIconGeneration++;
	if ( pCategory->pContext->iIconGeneration == 0 ) {
		pCategory->pContext->iIconGeneration = 1;
	}
	xuiInternalContextBumpGeneration(pCategory->pContext);
	(void)xuiInternalContextInvalidateAll(pCategory->pContext);
}

static void __xuiIconChanged(xui_icon pIcon, int bClearCache)
{
	if ( !__xuiIconValid(pIcon) ) {
		return;
	}
	if ( bClearCache ) {
		__xuiIconClearCache(pIcon);
		if ( (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_FILE) ||
		     (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_MEMORY) ) {
			__xuiIconClearLoadedSurface(pIcon);
		}
	}
	pIcon->iGeneration++;
	if ( pIcon->iGeneration == 0 ) pIcon->iGeneration = 1;
	if ( __xuiIconCategoryValid(pIcon->pCategory) ) {
		__xuiIconCategoryChanged(pIcon->pCategory);
	} else {
		xuiInternalContextBumpGeneration(pIcon->pContext);
		(void)xuiInternalContextInvalidateAll(pIcon->pContext);
	}
}

static uint32_t __xuiIconColorMultiply(uint32_t iA, uint32_t iB)
{
	uint32_t iR;
	uint32_t iG;
	uint32_t iBlue;
	uint32_t iAlpha;

	iR = ((((iA >> 24) & 0xFFu) * ((iB >> 24) & 0xFFu)) + 127u) / 255u;
	iG = ((((iA >> 16) & 0xFFu) * ((iB >> 16) & 0xFFu)) + 127u) / 255u;
	iBlue = ((((iA >> 8) & 0xFFu) * ((iB >> 8) & 0xFFu)) + 127u) / 255u;
	iAlpha = (((iA & 0xFFu) * (iB & 0xFFu)) + 127u) / 255u;
	return (iR << 24) | (iG << 16) | (iBlue << 8) | iAlpha;
}

static int __xuiIconGetSurfaceDesc(xui_icon pIcon, xui_surface pSurface, xui_surface_desc_t* pDesc)
{
	xui_proxy pProxy;

	if ( !__xuiIconValid(pIcon) || (pSurface == NULL) || (pDesc == NULL) ||
	     !pIcon->pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pProxy = &pIcon->pContext->tProxy;
	if ( pProxy->surfaceGetDesc == NULL ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	return pProxy->surfaceGetDesc(pProxy, pSurface, pDesc);
}

static int __xuiIconEnsureRasterSurface(xui_icon pIcon)
{
	xui_proxy pProxy;
	int iRet;

	if ( pIcon->pSurface != NULL ) {
		return XUI_OK;
	}
	if ( !pIcon->pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pProxy = &pIcon->pContext->tProxy;
	if ( pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_FILE ) {
		iRet = pProxy->surfaceLoadFile(pProxy, &pIcon->pSurface, pIcon->sPath, 0);
	} else {
		iRet = pProxy->surfaceLoadMemory(pProxy, &pIcon->pSurface, pIcon->pMemory, pIcon->iMemorySize, 0);
	}
	if ( (iRet == XUI_OK) && (pIcon->pSurface != NULL) ) {
		pIcon->bOwnSurface = 1;
	}
	return iRet;
}

static int __xuiIconPixelSize(xui_icon pIcon, float fWidth, float fHeight, int* pWidth, int* pHeight)
{
	float fDpi;

	if ( !__xuiIconFloatPositive(fWidth) || !__xuiIconFloatPositive(fHeight) ||
	     (pWidth == NULL) || (pHeight == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fDpi = pIcon->pContext->fDpiScale;
	if ( !__xuiIconFloatPositive(fDpi) ) fDpi = 1.0f;
	*pWidth = xuiInternalPixelCeil(fWidth * fDpi);
	*pHeight = xuiInternalPixelCeil(fHeight * fDpi);
	if ( *pWidth < 1 ) *pWidth = 1;
	if ( *pHeight < 1 ) *pHeight = 1;
	return XUI_OK;
}

static xui_icon_cache_entry_t* __xuiIconFindCache(xui_icon pIcon, int iWidth, int iHeight)
{
	int i;

	for ( i = 0; i < pIcon->iCacheCount; i++ ) {
		if ( (pIcon->pCache[i].iPixelWidth == iWidth) &&
		     (pIcon->pCache[i].iPixelHeight == iHeight) ) {
			pIcon->iCacheClock++;
			if ( pIcon->iCacheClock == 0 ) pIcon->iCacheClock = 1;
			pIcon->pCache[i].iLastUse = pIcon->iCacheClock;
			return &pIcon->pCache[i];
		}
	}
	return NULL;
}

static int __xuiIconLoadSvgSurface(xui_icon pIcon, int iWidth, int iHeight, xui_surface* ppSurface)
{
	xui_proxy pProxy;

	if ( (ppSurface == NULL) || !pIcon->pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	*ppSurface = NULL;
	if ( (pIcon->pContext->tProxyCaps.iCaps & XUI_PROXY_CAP_SVG_SURFACE) == 0 ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	pProxy = &pIcon->pContext->tProxy;
	if ( pIcon->iSourceType == XUI_ICON_SOURCE_SVG_FILE ) {
		if ( pProxy->surfaceLoadSvgFile == NULL ) return XUI_ERROR_UNSUPPORTED;
		return pProxy->surfaceLoadSvgFile(pProxy, ppSurface, pIcon->sPath, iWidth, iHeight, 0);
	}
	if ( pProxy->surfaceLoadSvgMemory == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProxy->surfaceLoadSvgMemory(pProxy, ppSurface, pIcon->pMemory, pIcon->iMemorySize, iWidth, iHeight, 0);
}

static int __xuiIconEnsureSvgCache(xui_icon pIcon, int iWidth, int iHeight, xui_icon_cache_entry_t** ppEntry)
{
	xui_icon_cache_entry_t* pEntry;
	xui_icon_cache_entry_t* pNewCache;
	xui_surface pSurface;
	int iCapacity;
	int iReplace;
	int i;
	int iRet;

	if ( ppEntry == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppEntry = NULL;
	pEntry = __xuiIconFindCache(pIcon, iWidth, iHeight);
	if ( pEntry != NULL ) {
		*ppEntry = pEntry;
		return XUI_OK;
	}
	iRet = __xuiIconLoadSvgSurface(pIcon, iWidth, iHeight, &pSurface);
	if ( iRet != XUI_OK ) return iRet;
	if ( pSurface == NULL ) return XUI_ERROR_BACKEND_FAILED;
	iCapacity = __xuiIconCacheCapacity(pIcon);
	if ( iCapacity <= 0 ) {
		__xuiIconDestroySurface(pIcon, pSurface);
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( pIcon->iCacheCapacity != iCapacity ) {
		pNewCache = (xui_icon_cache_entry_t*)xrtMalloc(sizeof(*pNewCache) * (size_t)iCapacity);
		if ( pNewCache == NULL ) {
			__xuiIconDestroySurface(pIcon, pSurface);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memset(pNewCache, 0, sizeof(*pNewCache) * (size_t)iCapacity);
		if ( pIcon->pCache != NULL ) {
			int iCopyCount;

			iCopyCount = pIcon->iCacheCount;
			if ( iCopyCount > iCapacity ) iCopyCount = iCapacity;
			memcpy(pNewCache, pIcon->pCache, sizeof(*pNewCache) * (size_t)iCopyCount);
			for ( i = iCopyCount; i < pIcon->iCacheCount; i++ ) {
				__xuiIconDestroySurface(pIcon, pIcon->pCache[i].pSurface);
			}
			xrtFree(pIcon->pCache);
			pIcon->iCacheCount = iCopyCount;
		}
		pIcon->pCache = pNewCache;
		pIcon->iCacheCapacity = iCapacity;
	}
	if ( pIcon->iCacheCount < pIcon->iCacheCapacity ) {
		iReplace = pIcon->iCacheCount++;
	} else {
		iReplace = 0;
		for ( i = 1; i < pIcon->iCacheCount; i++ ) {
			if ( pIcon->pCache[i].iLastUse < pIcon->pCache[iReplace].iLastUse ) {
				iReplace = i;
			}
		}
		__xuiIconDestroySurface(pIcon, pIcon->pCache[iReplace].pSurface);
	}
	pIcon->iCacheClock++;
	if ( pIcon->iCacheClock == 0 ) pIcon->iCacheClock = 1;
	pEntry = &pIcon->pCache[iReplace];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->pSurface = pSurface;
	pEntry->iPixelWidth = iWidth;
	pEntry->iPixelHeight = iHeight;
	pEntry->iLastUse = pIcon->iCacheClock;
	*ppEntry = pEntry;
	return XUI_OK;
}

static int __xuiIconIntrinsicSizeDepth(xui_icon pIcon, xui_vec2_t* pSize, int iDepth)
{
	xui_surface pSurface;
	xui_surface_desc_t tSurfaceDesc;
	int iRet;

	if ( !__xuiIconValid(pIcon) || (pSize == NULL) || (iDepth > XUI_ICON_ALIAS_DEPTH_MAX) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	switch ( pIcon->iSourceType ) {
	case XUI_ICON_SOURCE_SVG_PATH:
		if ( __xuiIconFloatPositive(pIcon->tViewBox.fW) &&
		     __xuiIconFloatPositive(pIcon->tViewBox.fH) ) {
			pSize->fX = pIcon->tViewBox.fW;
			pSize->fY = pIcon->tViewBox.fH;
			return XUI_OK;
		}
		break;
	case XUI_ICON_SOURCE_RASTER_FILE:
	case XUI_ICON_SOURCE_RASTER_MEMORY:
		iRet = __xuiIconEnsureRasterSurface(pIcon);
		if ( iRet != XUI_OK ) return iRet;
		pSurface = pIcon->pSurface;
		goto surface_size;
	case XUI_ICON_SOURCE_SURFACE:
		pSurface = pIcon->pSurface;
		goto surface_size;
	case XUI_ICON_SOURCE_RESOURCE:
		{
			xui_resource pResource;

			pResource = xuiResourceFind(pIcon->pContext, pIcon->sPath);
			if ( (pResource == NULL) ||
			     (xuiResourceGetKind(pResource) != XUI_RESOURCE_SURFACE) ) {
				return XUI_ERROR_RESOURCE_FAILED;
			}
			pSurface = (xui_surface)xuiResourceGetHandle(pResource);
		}
		if ( pSurface == NULL ) {
			return XUI_ERROR_RESOURCE_FAILED;
		}
surface_size:
		if ( __xuiIconFloatPositive(pIcon->tSource.fW) &&
		     __xuiIconFloatPositive(pIcon->tSource.fH) ) {
			pSize->fX = pIcon->tSource.fW;
			pSize->fY = pIcon->tSource.fH;
			return XUI_OK;
		}
		iRet = __xuiIconGetSurfaceDesc(pIcon, pSurface, &tSurfaceDesc);
		if ( iRet != XUI_OK ) return iRet;
		pSize->fX = (float)tSurfaceDesc.iWidth;
		pSize->fY = (float)tSurfaceDesc.iHeight;
		return XUI_OK;
	case XUI_ICON_SOURCE_SVG_FILE:
	case XUI_ICON_SOURCE_SVG_MEMORY:
		if ( pIcon->iCacheCount > 0 ) {
			pSize->fX = (float)pIcon->pCache[0].iPixelWidth / pIcon->pContext->fDpiScale;
			pSize->fY = (float)pIcon->pCache[0].iPixelHeight / pIcon->pContext->fDpiScale;
			return XUI_OK;
		}
		break;
	case XUI_ICON_SOURCE_ALIAS:
		return __xuiIconIntrinsicSizeDepth(pIcon->pAlias, pSize, iDepth + 1);
	case XUI_ICON_SOURCE_CUSTOM:
		if ( pIcon->tCustom.onMeasure != NULL ) {
			return pIcon->tCustom.onMeasure(pIcon, pSize, pIcon->tCustom.pUser);
		}
		break;
	default:
		break;
	}
	if ( __xuiIconCategoryValid(pIcon->pCategory) &&
	     (pIcon->pCategory->tDesc.iSizeMode == XUI_ICON_SIZE_FIXED) ) {
		pSize->fX = pIcon->pCategory->tDesc.fWidth;
		pSize->fY = pIcon->pCategory->tDesc.fHeight;
		return XUI_OK;
	}
	return XUI_ERROR_UNSUPPORTED;
}

static xui_rect_t __xuiIconCenteredRect(xui_rect_t tOuter, float fWidth, float fHeight)
{
	xui_rect_t tRect;

	tRect.fW = fWidth;
	tRect.fH = fHeight;
	tRect.fX = tOuter.fX + (tOuter.fW - fWidth) * 0.5f;
	tRect.fY = tOuter.fY + (tOuter.fH - fHeight) * 0.5f;
	return tRect;
}

static xui_rect_t __xuiIconFitRect(xui_rect_t tOuter, xui_vec2_t tSource, int iFitMode)
{
	float fScale;
	float fScaleX;
	float fScaleY;

	if ( !__xuiIconFloatPositive(tSource.fX) ||
	     !__xuiIconFloatPositive(tSource.fY) ||
	     (iFitMode == XUI_IMAGE_STRETCH) ||
	     (iFitMode == XUI_IMAGE_CUSTOM) ) {
		return tOuter;
	}
	if ( iFitMode == XUI_IMAGE_NATURAL ) {
		return __xuiIconCenteredRect(tOuter, tSource.fX, tSource.fY);
	}
	fScaleX = tOuter.fW / tSource.fX;
	fScaleY = tOuter.fH / tSource.fY;
	if ( iFitMode == XUI_IMAGE_COVER ) {
		fScale = (fScaleX > fScaleY) ? fScaleX : fScaleY;
	} else {
		fScale = (fScaleX < fScaleY) ? fScaleX : fScaleY;
		if ( (iFitMode == XUI_IMAGE_SCALE_DOWN) && (fScale > 1.0f) ) {
			fScale = 1.0f;
		}
	}
	return __xuiIconCenteredRect(tOuter, tSource.fX * fScale, tSource.fY * fScale);
}

static int __xuiIconResolveDrawRect(xui_icon pIcon, xui_rect_t tRect, const xui_icon_draw_desc_t* pDesc, xui_rect_t* pDrawRect)
{
	xui_vec2_t tSourceSize;
	xui_icon_category_desc_t* pCategoryDesc;
	uint32_t iFlags;
	int iFitMode;
	int iRet;

	if ( (pDrawRect == NULL) || !__xuiIconFloatPositive(tRect.fW) ||
	     !__xuiIconFloatPositive(tRect.fH) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iFlags = (pDesc != NULL) ? pDesc->iFlags : 0u;
	iFitMode = (pDesc != NULL) ? pDesc->iFitMode : -1;
	pCategoryDesc = __xuiIconCategoryValid(pIcon->pCategory) ? &pIcon->pCategory->tDesc : NULL;
	if ( (pCategoryDesc != NULL) &&
	     (pCategoryDesc->iSizeMode == XUI_ICON_SIZE_FIXED) &&
	     ((iFlags & XUI_ICON_DRAW_IGNORE_CATEGORY_SIZE) == 0) ) {
		tRect = __xuiIconCenteredRect(tRect, pCategoryDesc->fWidth, pCategoryDesc->fHeight);
	}
	if ( (iFlags & XUI_ICON_DRAW_USE_SOURCE_SIZE) != 0 ) {
		iRet = __xuiIconIntrinsicSizeDepth(pIcon, &tSourceSize, 0);
		if ( iRet == XUI_OK ) {
			tRect = __xuiIconCenteredRect(tRect, tSourceSize.fX, tSourceSize.fY);
		}
	}
	if ( iFitMode < 0 ) {
		iFitMode = (pCategoryDesc != NULL) ? pCategoryDesc->iFitMode : XUI_IMAGE_CONTAIN;
	}
	iRet = __xuiIconIntrinsicSizeDepth(pIcon, &tSourceSize, 0);
	if ( iRet != XUI_OK ) {
		tSourceSize.fX = tRect.fW;
		tSourceSize.fY = tRect.fH;
	}
	*pDrawRect = __xuiIconFitRect(tRect, tSourceSize, iFitMode);
	return XUI_OK;
}

static int __xuiIconPrepareDepth(xui_icon pIcon, float fWidth, float fHeight, int iDepth)
{
	xui_icon_cache_entry_t* pEntry;
	int iPixelWidth;
	int iPixelHeight;
	int iRet;

	if ( !__xuiIconValid(pIcon) || (iDepth > XUI_ICON_ALIAS_DEPTH_MAX) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiIconCategoryValid(pIcon->pCategory) &&
	     (pIcon->pCategory->tDesc.iSizeMode == XUI_ICON_SIZE_FIXED) ) {
		fWidth = pIcon->pCategory->tDesc.fWidth;
		fHeight = pIcon->pCategory->tDesc.fHeight;
	}
	if ( !__xuiIconFloatPositive(fWidth) || !__xuiIconFloatPositive(fHeight) ) {
		xui_vec2_t tSize;

		iRet = __xuiIconIntrinsicSizeDepth(pIcon, &tSize, 0);
		if ( iRet != XUI_OK ) {
			fWidth = XUI_ICON_DEFAULT_SIZE;
			fHeight = XUI_ICON_DEFAULT_SIZE;
		} else {
			fWidth = tSize.fX;
			fHeight = tSize.fY;
		}
	}
	switch ( pIcon->iSourceType ) {
	case XUI_ICON_SOURCE_RASTER_FILE:
	case XUI_ICON_SOURCE_RASTER_MEMORY:
		return __xuiIconEnsureRasterSurface(pIcon);
	case XUI_ICON_SOURCE_SVG_FILE:
	case XUI_ICON_SOURCE_SVG_MEMORY:
		iRet = __xuiIconPixelSize(pIcon, fWidth, fHeight, &iPixelWidth, &iPixelHeight);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiIconEnsureSvgCache(pIcon, iPixelWidth, iPixelHeight, &pEntry);
	case XUI_ICON_SOURCE_ALIAS:
		return __xuiIconPrepareDepth(pIcon->pAlias, fWidth, fHeight, iDepth + 1);
	case XUI_ICON_SOURCE_CUSTOM:
		if ( pIcon->tCustom.onPrepare != NULL ) {
			iRet = __xuiIconPixelSize(pIcon, fWidth, fHeight, &iPixelWidth, &iPixelHeight);
			if ( iRet != XUI_OK ) return iRet;
			return pIcon->tCustom.onPrepare(pIcon, iPixelWidth, iPixelHeight, pIcon->tCustom.pUser);
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiIconSurfaceAndSource(xui_icon pIcon, xui_surface* ppSurface, xui_rect_t* pSource)
{
	xui_surface_desc_t tDesc;
	xui_surface pSurface;
	int iRet;

	if ( (ppSurface == NULL) || (pSource == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_FILE) ||
	     (pIcon->iSourceType == XUI_ICON_SOURCE_RASTER_MEMORY) ) {
		iRet = __xuiIconEnsureRasterSurface(pIcon);
		if ( iRet != XUI_OK ) return iRet;
		pSurface = pIcon->pSurface;
	} else if ( pIcon->iSourceType == XUI_ICON_SOURCE_SURFACE ) {
		pSurface = pIcon->pSurface;
	} else {
		xui_resource pResource;

		pResource = xuiResourceFind(pIcon->pContext, pIcon->sPath);
		if ( (pResource == NULL) ||
		     (xuiResourceGetKind(pResource) != XUI_RESOURCE_SURFACE) ) {
			return XUI_ERROR_RESOURCE_FAILED;
		}
		pSurface = (xui_surface)xuiResourceGetHandle(pResource);
	}
	if ( pSurface == NULL ) return XUI_ERROR_RESOURCE_FAILED;
	*pSource = pIcon->tSource;
	if ( !__xuiIconFloatPositive(pSource->fW) || !__xuiIconFloatPositive(pSource->fH) ) {
		iRet = __xuiIconGetSurfaceDesc(pIcon, pSurface, &tDesc);
		if ( iRet != XUI_OK ) return iRet;
		pSource->fX = 0.0f;
		pSource->fY = 0.0f;
		pSource->fW = (float)tDesc.iWidth;
		pSource->fH = (float)tDesc.iHeight;
	}
	*ppSurface = pSurface;
	return XUI_OK;
}

static int __xuiIconDrawDepth(xui_painter pPainter, xui_icon pIcon, xui_rect_t tRect, const xui_icon_draw_desc_t* pDesc, int iDepth, int bResolveRect)
{
	xui_icon_draw_desc_t tAliasDesc;
	xui_icon_cache_entry_t* pEntry;
	xui_path_style_t tStyle;
	xui_surface pSurface;
	xui_surface_desc_t tSurfaceDesc;
	xui_rect_t tSource;
	xui_rect_t tDrawRect;
	uint32_t iColor;
	int iPixelWidth;
	int iPixelHeight;
	int iRet;

	if ( !__xuiIconValid(pIcon) || (pPainter == NULL) ||
	     (pPainter->iMagic != XUI_PAINTER_MAGIC) ||
	     (pPainter->pContext != pIcon->pContext) ||
	     (iDepth > XUI_ICON_ALIAS_DEPTH_MAX) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tDrawRect = tRect;
	if ( bResolveRect ) {
		iRet = __xuiIconResolveDrawRect(pIcon, tRect, pDesc, &tDrawRect);
		if ( iRet != XUI_OK ) return iRet;
	}
	iColor = (pDesc != NULL) ? pDesc->iColor : XUI_COLOR_WHITE;
	switch ( pIcon->iSourceType ) {
	case XUI_ICON_SOURCE_SVG_PATH:
		tStyle = pIcon->tPathStyle;
		tStyle.iFillColor = __xuiIconColorMultiply(tStyle.iFillColor, iColor);
		tStyle.iStrokeColor = __xuiIconColorMultiply(tStyle.iStrokeColor, iColor);
		return xuiPainterDrawSvgPath(pPainter, pIcon->sPath, pIcon->tViewBox, tDrawRect, &tStyle, 0.25f);
	case XUI_ICON_SOURCE_SVG_FILE:
	case XUI_ICON_SOURCE_SVG_MEMORY:
		iRet = __xuiIconPixelSize(pIcon, tDrawRect.fW, tDrawRect.fH, &iPixelWidth, &iPixelHeight);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiIconEnsureSvgCache(pIcon, iPixelWidth, iPixelHeight, &pEntry);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiIconGetSurfaceDesc(pIcon, pEntry->pSurface, &tSurfaceDesc);
		if ( iRet != XUI_OK ) return iRet;
		tSource = (xui_rect_t){0.0f, 0.0f, (float)tSurfaceDesc.iWidth, (float)tSurfaceDesc.iHeight};
		return xuiPainterDrawSurface(pPainter, pEntry->pSurface, tSource, tDrawRect, iColor, 0);
	case XUI_ICON_SOURCE_RASTER_FILE:
	case XUI_ICON_SOURCE_RASTER_MEMORY:
	case XUI_ICON_SOURCE_SURFACE:
	case XUI_ICON_SOURCE_RESOURCE:
		iRet = __xuiIconSurfaceAndSource(pIcon, &pSurface, &tSource);
		if ( iRet != XUI_OK ) return iRet;
		return xuiPainterDrawSurface(pPainter, pSurface, tSource, tDrawRect, iColor, 0);
	case XUI_ICON_SOURCE_ALIAS:
		if ( !__xuiIconValid(pIcon->pAlias) ) return XUI_ERROR_RESOURCE_FAILED;
		if ( pDesc != NULL ) {
			tAliasDesc = *pDesc;
		} else {
			xuiIconDrawDescDefault(&tAliasDesc);
		}
		tAliasDesc.iFlags |= XUI_ICON_DRAW_IGNORE_CATEGORY_SIZE;
		return __xuiIconDrawDepth(pPainter, pIcon->pAlias, tDrawRect, &tAliasDesc, iDepth + 1, 0);
	case XUI_ICON_SOURCE_CUSTOM:
		if ( pIcon->tCustom.onDraw == NULL ) return XUI_ERROR_UNSUPPORTED;
		return pIcon->tCustom.onDraw(pIcon, pPainter, tDrawRect, pDesc, pIcon->tCustom.pUser);
	default:
		return XUI_ERROR_UNSUPPORTED;
	}
}

static int __xuiIconRegister(xui_icon_category pCategory, const char* sName, int iSourceType, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	xui_icon* ppSlot;
	uint32_t* pExistingId;
	uint32_t* pStoredId;
	uint32_t iId;
	int iRet;

	if ( ppIcon != NULL ) *ppIcon = NULL;
	if ( !__xuiIconCategoryValid(pCategory) || (sName == NULL) || (sName[0] == '\0') ||
	     !__xuiIconDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pExistingId = (uint32_t*)xrtDictGet(&pCategory->mapIcons, (void*)sName, (uint32_t)strlen(sName));
	if ( pExistingId != NULL ) return XUI_ERROR_ALREADY_INITIALIZED;
	pIcon = (xui_icon)xrtMalloc(sizeof(*pIcon));
	if ( pIcon == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pIcon, 0, sizeof(*pIcon));
	pIcon->sName = __xuiIconStringDuplicate(sName);
	if ( pIcon->sName == NULL ) {
		xrtFree(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( (pDesc != NULL) && (pDesc->sDisplayName != NULL) ) {
		pIcon->sDisplayName = __xuiIconStringDuplicate(pDesc->sDisplayName);
		if ( pIcon->sDisplayName == NULL ) {
			xrtFree(pIcon->sName);
			xrtFree(pIcon);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( (pDesc != NULL) && (pDesc->sTags != NULL) ) {
		pIcon->sTags = __xuiIconStringDuplicate(pDesc->sTags);
		if ( pIcon->sTags == NULL ) {
			if ( pIcon->sDisplayName != NULL ) xrtFree(pIcon->sDisplayName);
			xrtFree(pIcon->sName);
			xrtFree(pIcon);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	pIcon->iMagic = XUI_ICON_MAGIC;
	pIcon->pContext = pCategory->pContext;
	pIcon->pCategory = pCategory;
	pIcon->iSourceType = iSourceType;
	pIcon->iFlags = (pDesc != NULL) ? pDesc->iFlags : 0u;
	pIcon->pUser = (pDesc != NULL) ? pDesc->pUser : NULL;
	pIcon->iGeneration = 1;
	pIcon->iRefCount = 1;
	pIcon->bRegistered = 1;
	iId = xrtArrayAppend(&pCategory->arrIcons, 1u);
	if ( iId == 0u ) {
		pIcon->iMagic = 0;
		if ( pIcon->sTags != NULL ) xrtFree(pIcon->sTags);
		if ( pIcon->sDisplayName != NULL ) xrtFree(pIcon->sDisplayName);
		xrtFree(pIcon->sName);
		xrtFree(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pIcon->iId = iId;
	ppSlot = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, iId);
	*ppSlot = pIcon;
	pStoredId = (uint32_t*)xrtDictSet(&pCategory->mapIcons, (void*)sName, (uint32_t)strlen(sName), NULL);
	if ( pStoredId == NULL ) {
		*ppSlot = NULL;
		pIcon->bRegistered = 0;
		pIcon->pCategory = NULL;
		pIcon->iMagic = 0;
		if ( pIcon->sTags != NULL ) xrtFree(pIcon->sTags);
		if ( pIcon->sDisplayName != NULL ) xrtFree(pIcon->sDisplayName);
		xrtFree(pIcon->sName);
		xrtFree(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*pStoredId = iId;
	pIcon->pNextAll = pIcon->pContext->pIcons;
	pIcon->pContext->pIcons = pIcon;
	pCategory->iIconCount++;
	__xuiIconCategoryChanged(pCategory);
	iRet = XUI_OK;
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return iRet;
}

static void __xuiIconRollbackRegistration(xui_icon pIcon)
{
	xui_icon_category pCategory;
	xui_icon* ppSlot;

	if ( !__xuiIconValid(pIcon) || !__xuiIconCategoryValid(pIcon->pCategory) ) return;
	pCategory = pIcon->pCategory;
	(void)xrtDictRemove(&pCategory->mapIcons, pIcon->sName, (uint32_t)strlen(pIcon->sName));
	ppSlot = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, pIcon->iId);
	if ( ppSlot != NULL ) *ppSlot = NULL;
	if ( pCategory->iIconCount > 0 ) pCategory->iIconCount--;
	pIcon->bRegistered = 0;
	pIcon->pCategory = NULL;
	__xuiIconDestroyOne(pIcon);
	__xuiIconCategoryChanged(pCategory);
}

XUI_API void xuiIconCategoryDescDefault(xui_icon_category_desc_t* pDesc)
{
	if ( pDesc == NULL ) return;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iSizeMode = XUI_ICON_SIZE_FIXED;
	pDesc->fWidth = XUI_ICON_DEFAULT_SIZE;
	pDesc->fHeight = XUI_ICON_DEFAULT_SIZE;
	pDesc->iFitMode = XUI_IMAGE_CONTAIN;
	pDesc->iCacheCapacity = XUI_ICON_DEFAULT_CACHE_CAPACITY;
}

XUI_API void xuiIconDescDefault(xui_icon_desc_t* pDesc)
{
	if ( pDesc == NULL ) return;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
}

XUI_API void xuiIconDrawDescDefault(xui_icon_draw_desc_t* pDesc)
{
	if ( pDesc == NULL ) return;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iColor = XUI_COLOR_WHITE;
	pDesc->iFitMode = -1;
}

static void __xuiIconContextDestroyObjects(xui_context pContext)
{
	if ( pContext == NULL ) return;
	while ( pContext->pIconCategories != NULL ) {
		xui_icon_category pCategory;

		pCategory = pContext->pIconCategories;
		pCategory->bRegistered = 0;
		__xuiIconCategoryDestroyOne(pCategory);
	}
	while ( pContext->pIcons != NULL ) {
		__xuiIconDestroyOne(pContext->pIcons);
	}
}

XUI_API int xuiIconCategoryCreate(xui_context pContext, const char* sName, const xui_icon_category_desc_t* pDesc, xui_icon_category* ppCategory)
{
	xui_icon_category_desc_t tDefaultDesc;
	xui_icon_category pCategory;
	xui_icon_category* ppSlot;
	void* pOld;
	uint32_t iSlot;

	if ( ppCategory != NULL ) *ppCategory = NULL;
	if ( !__xuiIconContextValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xrtDictGetPtr(&pContext->mapIconCategories, (void*)sName, (uint32_t)strlen(sName)) != NULL ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	if ( pDesc == NULL ) {
		xuiIconCategoryDescDefault(&tDefaultDesc);
		pDesc = &tDefaultDesc;
	}
	if ( !__xuiIconCategoryDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	pCategory = (xui_icon_category)xrtMalloc(sizeof(*pCategory));
	if ( pCategory == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pCategory, 0, sizeof(*pCategory));
	pCategory->sName = __xuiIconStringDuplicate(sName);
	if ( pCategory->sName == NULL ) {
		xrtFree(pCategory);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pCategory->iMagic = XUI_ICON_CATEGORY_MAGIC;
	pCategory->pContext = pContext;
	pCategory->tDesc = *pDesc;
	pCategory->tDesc.iSize = sizeof(pCategory->tDesc);
	if ( pCategory->tDesc.iCacheCapacity == 0 ) {
		pCategory->tDesc.iCacheCapacity = XUI_ICON_DEFAULT_CACHE_CAPACITY;
	}
	pCategory->iGeneration = 1;
	pCategory->iRefCount = 1;
	pCategory->bRegistered = 1;
	xrtDictInit(&pCategory->mapIcons, sizeof(uint32_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pCategory->arrIcons, sizeof(xui_icon), XRT_OBJMODE_LOCAL);
	iSlot = xrtArrayAppend(&pContext->arrIconCategories, 1u);
	if ( iSlot == 0u ) {
		pCategory->iMagic = 0;
		xrtDictUnit(&pCategory->mapIcons);
		xrtArrayUnit(&pCategory->arrIcons);
		xrtFree(pCategory->sName);
		xrtFree(pCategory);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	ppSlot = (xui_icon_category*)xrtArrayGet(&pContext->arrIconCategories, iSlot);
	*ppSlot = pCategory;
	pOld = NULL;
	if ( !xrtDictSetPtr(&pContext->mapIconCategories, (void*)sName, (uint32_t)strlen(sName), pCategory, &pOld) ) {
		*ppSlot = NULL;
		pCategory->iMagic = 0;
		xrtDictUnit(&pCategory->mapIcons);
		xrtArrayUnit(&pCategory->arrIcons);
		xrtFree(pCategory->sName);
		xrtFree(pCategory);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pCategory->pNextAll = pContext->pIconCategories;
	pContext->pIconCategories = pCategory;
	pContext->onDestroyIcons = __xuiIconContextDestroyObjects;
	pContext->iIconGeneration++;
	xuiInternalContextBumpGeneration(pContext);
	if ( ppCategory != NULL ) *ppCategory = pCategory;
	return XUI_OK;
}

XUI_API xui_icon_category xuiIconCategoryFind(xui_context pContext, const char* sName)
{
	if ( !__xuiIconContextValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	return (xui_icon_category)xrtDictGetPtr(&pContext->mapIconCategories, (void*)sName, (uint32_t)strlen(sName));
}

XUI_API int xuiIconCategoryRemove(xui_context pContext, const char* sName)
{
	xui_icon_category pCategory;
	uint32_t i;

	pCategory = xuiIconCategoryFind(pContext, sName);
	if ( pCategory == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xrtDictRemove(&pContext->mapIconCategories, (void*)sName, (uint32_t)strlen(sName));
	for ( i = 1; i <= pContext->arrIconCategories.Count; i++ ) {
		xui_icon_category* ppCategory;

		ppCategory = (xui_icon_category*)xrtArrayGet(&pContext->arrIconCategories, i);
		if ( (ppCategory != NULL) && (*ppCategory == pCategory) ) {
			*ppCategory = NULL;
			break;
		}
	}
	pCategory->bRegistered = 0;
	pContext->iIconGeneration++;
	xuiInternalContextBumpGeneration(pContext);
	(void)xuiInternalContextInvalidateAll(pContext);
	__xuiIconCategoryReleaseOwner(pCategory);
	return XUI_OK;
}

XUI_API void xuiIconCategoryClear(xui_context pContext)
{
	uint32_t i;

	if ( !__xuiIconContextValid(pContext) ) return;
	for ( i = 1; i <= pContext->arrIconCategories.Count; i++ ) {
		xui_icon_category* ppCategory;

		ppCategory = (xui_icon_category*)xrtArrayGet(&pContext->arrIconCategories, i);
		if ( (ppCategory != NULL) && (*ppCategory != NULL) ) {
			xui_icon_category pCategory;

			pCategory = *ppCategory;
			*ppCategory = NULL;
			pCategory->bRegistered = 0;
			__xuiIconCategoryReleaseOwner(pCategory);
		}
	}
	xrtDictUnit(&pContext->mapIconCategories);
	xrtDictInit(&pContext->mapIconCategories, sizeof(void*), XRT_OBJMODE_LOCAL);
	xrtArrayUnit(&pContext->arrIconCategories);
	xrtArrayInit(&pContext->arrIconCategories, sizeof(xui_icon_category), XRT_OBJMODE_LOCAL);
	pContext->iIconGeneration++;
	xuiInternalContextBumpGeneration(pContext);
	(void)xuiInternalContextInvalidateAll(pContext);
}

XUI_API int xuiIconCategoryGetCount(xui_context pContext)
{
	xui_icon_category pCategory;
	int iCount;

	if ( !__xuiIconContextValid(pContext) ) return 0;
	iCount = 0;
	for ( pCategory = pContext->pIconCategories; pCategory != NULL; pCategory = pCategory->pNextAll ) {
		if ( pCategory->bRegistered ) iCount++;
	}
	return iCount;
}

XUI_API xui_icon_category xuiIconCategoryGetAt(xui_context pContext, int iIndex)
{
	uint32_t i;
	int iActive;

	if ( !__xuiIconContextValid(pContext) || (iIndex < 0) ) return NULL;
	iActive = 0;
	for ( i = 1; i <= pContext->arrIconCategories.Count; i++ ) {
		xui_icon_category* ppCategory;

		ppCategory = (xui_icon_category*)xrtArrayGet(&pContext->arrIconCategories, i);
		if ( (ppCategory != NULL) && (*ppCategory != NULL) ) {
			if ( iActive == iIndex ) return *ppCategory;
			iActive++;
		}
	}
	return NULL;
}

XUI_API int xuiIconCategoryAddRef(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) ) return XUI_ERROR_INVALID_ARGUMENT;
	pCategory->iRefCount++;
	if ( pCategory->iRefCount <= 1 ) {
		pCategory->iRefCount = 1;
		return XUI_ERROR_UNSUPPORTED;
	}
	return XUI_OK;
}

XUI_API int xuiIconCategoryRelease(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pCategory->bRegistered && (pCategory->iRefCount <= 1) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiIconCategoryReleaseOwner(pCategory);
	return XUI_OK;
}

XUI_API int xuiIconCategoryGetRefCount(xui_icon_category pCategory)
{
	return __xuiIconCategoryValid(pCategory) ? pCategory->iRefCount : 0;
}

XUI_API const char* xuiIconCategoryGetName(xui_icon_category pCategory)
{
	return __xuiIconCategoryValid(pCategory) ? pCategory->sName : NULL;
}

XUI_API uint32_t xuiIconCategoryGetGeneration(xui_icon_category pCategory)
{
	return __xuiIconCategoryValid(pCategory) ? pCategory->iGeneration : 0u;
}

XUI_API int xuiIconCategorySetDesc(xui_icon_category pCategory, const xui_icon_category_desc_t* pDesc)
{
	uint32_t i;

	if ( !__xuiIconCategoryValid(pCategory) || !__xuiIconCategoryDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pCategory->tDesc = *pDesc;
	pCategory->tDesc.iSize = sizeof(pCategory->tDesc);
	if ( pCategory->tDesc.iCacheCapacity == 0 ) {
		pCategory->tDesc.iCacheCapacity = XUI_ICON_DEFAULT_CACHE_CAPACITY;
	}
	for ( i = 1; i <= pCategory->arrIcons.Count; i++ ) {
		xui_icon* ppIcon;

		ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, i);
		if ( (ppIcon != NULL) && (*ppIcon != NULL) ) {
			__xuiIconClearCache(*ppIcon);
		}
	}
	__xuiIconCategoryChanged(pCategory);
	return XUI_OK;
}

XUI_API int xuiIconCategoryGetDesc(xui_icon_category pCategory, xui_icon_category_desc_t* pDesc)
{
	if ( !__xuiIconCategoryValid(pCategory) || (pDesc == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pDesc = pCategory->tDesc;
	return XUI_OK;
}

XUI_API int xuiIconCategoryBeginUpdate(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) ) return XUI_ERROR_INVALID_ARGUMENT;
	pCategory->iUpdateDepth++;
	return XUI_OK;
}

XUI_API int xuiIconCategoryEndUpdate(xui_icon_category pCategory)
{
	if ( !__xuiIconCategoryValid(pCategory) || (pCategory->iUpdateDepth <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pCategory->iUpdateDepth--;
	if ( (pCategory->iUpdateDepth == 0) && pCategory->bUpdateDirty ) {
		pCategory->bUpdateDirty = 0;
		__xuiIconCategoryChanged(pCategory);
	}
	return XUI_OK;
}

XUI_API int xuiIconCategoryGetIconCount(xui_icon_category pCategory)
{
	return __xuiIconCategoryValid(pCategory) ? pCategory->iIconCount : 0;
}

XUI_API uint32_t xuiIconCategoryGetIconSlotCount(xui_icon_category pCategory)
{
	return __xuiIconCategoryValid(pCategory) ? pCategory->arrIcons.Count : 0u;
}

XUI_API xui_icon xuiIconCategoryGetIconAt(xui_icon_category pCategory, int iIndex)
{
	uint32_t i;
	int iActive;

	if ( !__xuiIconCategoryValid(pCategory) || (iIndex < 0) ) return NULL;
	iActive = 0;
	for ( i = 1; i <= pCategory->arrIcons.Count; i++ ) {
		xui_icon* ppIcon;

		ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, i);
		if ( (ppIcon != NULL) && (*ppIcon != NULL) ) {
			if ( iActive == iIndex ) return *ppIcon;
			iActive++;
		}
	}
	return NULL;
}

XUI_API int xuiIconCategoryPreload(xui_icon_category pCategory, float fWidth, float fHeight)
{
	uint32_t i;
	int iRet;

	if ( !__xuiIconCategoryValid(pCategory) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 1; i <= pCategory->arrIcons.Count; i++ ) {
		xui_icon* ppIcon;

		ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, i);
		if ( (ppIcon != NULL) && (*ppIcon != NULL) ) {
			iRet = xuiIconPrepare(*ppIcon, fWidth, fHeight);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiIconAddSvgPath(xui_icon_category pCategory, const char* sName, const char* sPath, xui_rect_t tViewBox, const xui_path_style_t* pStyle, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_path_style_t tDefaultStyle;
	xui_icon pIcon;
	int iRet;

	if ( (sPath == NULL) || (sPath[0] == '\0') ||
	     !__xuiIconFloatPositive(tViewBox.fW) ||
	     !__xuiIconFloatPositive(tViewBox.fH) ||
	     ((pStyle != NULL) &&
	      ((pStyle->iSize < sizeof(*pStyle)) ||
	       (pStyle->iDashCount < 0) ||
	       ((pStyle->iDashCount > 0) && (pStyle->pDashPattern == NULL)))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_SVG_PATH, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->sPath = __xuiIconStringDuplicate(sPath);
	if ( pIcon->sPath == NULL ) {
		__xuiIconRollbackRegistration(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pStyle == NULL ) {
		memset(&tDefaultStyle, 0, sizeof(tDefaultStyle));
		tDefaultStyle.iSize = sizeof(tDefaultStyle);
		tDefaultStyle.iFillColor = XUI_COLOR_WHITE;
		tDefaultStyle.iStrokeColor = XUI_COLOR_WHITE;
		tDefaultStyle.iFillRule = XUI_PATH_FILL_NON_ZERO;
		tDefaultStyle.iLineJoin = XUI_PATH_JOIN_MITER;
		tDefaultStyle.iLineCap = XUI_PATH_CAP_BUTT;
		pStyle = &tDefaultStyle;
	}
	pIcon->tViewBox = tViewBox;
	pIcon->tPathStyle = *pStyle;
	pIcon->tPathStyle.iSize = sizeof(pIcon->tPathStyle);
	if ( (pStyle->pDashPattern != NULL) && (pStyle->iDashCount > 0) ) {
		pIcon->pDashPattern = (float*)__xuiIconMemoryDuplicate(
			pStyle->pDashPattern,
			(int)(sizeof(float) * (size_t)pStyle->iDashCount));
		if ( pIcon->pDashPattern == NULL ) {
			__xuiIconRollbackRegistration(pIcon);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pIcon->tPathStyle.pDashPattern = pIcon->pDashPattern;
	}
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddSvgFile(xui_icon_category pCategory, const char* sName, const char* sPath, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_SVG_FILE, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->sPath = __xuiIconStringDuplicate(sPath);
	if ( pIcon->sPath == NULL ) {
		__xuiIconRollbackRegistration(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

static int __xuiIconAddMemory(xui_icon_category pCategory, const char* sName, const void* pData, int iSize, int iSourceType, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( (pData == NULL) || (iSize <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiIconRegister(pCategory, sName, iSourceType, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->pMemory = __xuiIconMemoryDuplicate(pData, iSize);
	if ( pIcon->pMemory == NULL ) {
		__xuiIconRollbackRegistration(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pIcon->iMemorySize = iSize;
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddSvgMemory(xui_icon_category pCategory, const char* sName, const void* pData, int iSize, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	return __xuiIconAddMemory(pCategory, sName, pData, iSize, XUI_ICON_SOURCE_SVG_MEMORY, pDesc, ppIcon);
}

XUI_API int xuiIconAddRasterFile(xui_icon_category pCategory, const char* sName, const char* sPath, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_RASTER_FILE, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->sPath = __xuiIconStringDuplicate(sPath);
	if ( pIcon->sPath == NULL ) {
		__xuiIconRollbackRegistration(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddRasterMemory(xui_icon_category pCategory, const char* sName, const void* pData, int iSize, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	return __xuiIconAddMemory(pCategory, sName, pData, iSize, XUI_ICON_SOURCE_RASTER_MEMORY, pDesc, ppIcon);
}

XUI_API int xuiIconAddSurface(xui_icon_category pCategory, const char* sName, xui_surface pSurface, xui_rect_t tSource, uint32_t iSurfaceFlags, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( pSurface == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_SURFACE, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->pSurface = pSurface;
	pIcon->tSource = tSource;
	pIcon->bOwnSurface = ((iSurfaceFlags & XUI_ICON_SURFACE_TAKE_OWNERSHIP) != 0);
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddResource(xui_icon_category pCategory, const char* sName, xui_resource pResource, xui_rect_t tSource, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	const char* sResourceName;
	int iRet;

	sResourceName = xuiResourceGetName(pResource);
	if ( !__xuiIconCategoryValid(pCategory) ||
	     (sResourceName == NULL) ||
	     (xuiResourceFind(pCategory->pContext, sResourceName) != pResource) ||
	     (xuiResourceGetKind(pResource) != XUI_RESOURCE_SURFACE) ||
	     (xuiResourceGetHandle(pResource) == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_RESOURCE, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->sPath = __xuiIconStringDuplicate(sResourceName);
	if ( pIcon->sPath == NULL ) {
		__xuiIconRollbackRegistration(pIcon);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pIcon->tSource = tSource;
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddAlias(xui_icon_category pCategory, const char* sName, xui_icon pTarget, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( !__xuiIconValid(pTarget) || !__xuiIconCategoryValid(pCategory) ||
	     (pTarget->pContext != pCategory->pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_ALIAS, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiIconAddRef(pTarget);
	if ( iRet != XUI_OK ) {
		__xuiIconRollbackRegistration(pIcon);
		return iRet;
	}
	pIcon->pAlias = pTarget;
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API int xuiIconAddCustom(xui_icon_category pCategory, const char* sName, const xui_icon_custom_desc_t* pCustom, const xui_icon_desc_t* pDesc, xui_icon* ppIcon)
{
	xui_icon pIcon;
	int iRet;

	if ( (pCustom == NULL) || (pCustom->iSize < sizeof(*pCustom)) ||
	     (pCustom->onDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiIconRegister(pCategory, sName, XUI_ICON_SOURCE_CUSTOM, pDesc, &pIcon);
	if ( iRet != XUI_OK ) return iRet;
	pIcon->tCustom = *pCustom;
	pIcon->tCustom.iSize = sizeof(pIcon->tCustom);
	if ( ppIcon != NULL ) *ppIcon = pIcon;
	return XUI_OK;
}

XUI_API xui_icon xuiIconFind(xui_icon_category pCategory, const char* sName)
{
	uint32_t* pId;

	if ( !__xuiIconCategoryValid(pCategory) || (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	pId = (uint32_t*)xrtDictGet(&pCategory->mapIcons, (void*)sName, (uint32_t)strlen(sName));
	return (pId != NULL) ? xuiIconFindById(pCategory, *pId) : NULL;
}

XUI_API xui_icon xuiIconFindById(xui_icon_category pCategory, xui_icon_id iId)
{
	xui_icon* ppIcon;

	if ( !__xuiIconCategoryValid(pCategory) || (iId == XUI_ICON_ID_INVALID) ||
	     (iId > pCategory->arrIcons.Count) ) {
		return NULL;
	}
	ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, iId);
	return (ppIcon != NULL) ? *ppIcon : NULL;
}

XUI_API int xuiIconRemoveById(xui_icon_category pCategory, xui_icon_id iId)
{
	xui_icon pIcon;
	xui_icon* ppIcon;

	pIcon = xuiIconFindById(pCategory, iId);
	if ( pIcon == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xrtDictRemove(&pCategory->mapIcons, pIcon->sName, (uint32_t)strlen(pIcon->sName));
	ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, iId);
	if ( ppIcon != NULL ) *ppIcon = NULL;
	if ( pCategory->iIconCount > 0 ) pCategory->iIconCount--;
	pIcon->bRegistered = 0;
	pIcon->pCategory = NULL;
	__xuiIconCategoryChanged(pCategory);
	__xuiIconReleaseOwner(pIcon);
	return XUI_OK;
}

XUI_API int xuiIconRemove(xui_icon_category pCategory, const char* sName)
{
	xui_icon pIcon;

	pIcon = xuiIconFind(pCategory, sName);
	return (pIcon != NULL) ? xuiIconRemoveById(pCategory, pIcon->iId) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API void xuiIconClear(xui_icon_category pCategory)
{
	uint32_t i;

	if ( !__xuiIconCategoryValid(pCategory) ) return;
	for ( i = 1; i <= pCategory->arrIcons.Count; i++ ) {
		xui_icon* ppIcon;

		ppIcon = (xui_icon*)xrtArrayGet(&pCategory->arrIcons, i);
		if ( (ppIcon != NULL) && (*ppIcon != NULL) ) {
			xui_icon pIcon;

			pIcon = *ppIcon;
			*ppIcon = NULL;
			pIcon->bRegistered = 0;
			pIcon->pCategory = NULL;
			__xuiIconReleaseOwner(pIcon);
		}
	}
	pCategory->iIconCount = 0;
	xrtDictUnit(&pCategory->mapIcons);
	xrtDictInit(&pCategory->mapIcons, sizeof(uint32_t), XRT_OBJMODE_LOCAL);
	__xuiIconCategoryChanged(pCategory);
}

XUI_API int xuiIconAddRef(xui_icon pIcon)
{
	if ( !__xuiIconValid(pIcon) ) return XUI_ERROR_INVALID_ARGUMENT;
	pIcon->iRefCount++;
	if ( pIcon->iRefCount <= 1 ) {
		pIcon->iRefCount = 1;
		return XUI_ERROR_UNSUPPORTED;
	}
	return XUI_OK;
}

XUI_API int xuiIconRelease(xui_icon pIcon)
{
	if ( !__xuiIconValid(pIcon) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pIcon->bRegistered && (pIcon->iRefCount <= 1) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiIconReleaseOwner(pIcon);
	return XUI_OK;
}

XUI_API int xuiIconGetRefCount(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->iRefCount : 0;
}

XUI_API xui_icon_id xuiIconGetId(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->iId : XUI_ICON_ID_INVALID;
}

XUI_API const char* xuiIconGetName(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->sName : NULL;
}

XUI_API const char* xuiIconGetDisplayName(xui_icon pIcon)
{
	if ( !__xuiIconValid(pIcon) ) return NULL;
	return (pIcon->sDisplayName != NULL) ? pIcon->sDisplayName : pIcon->sName;
}

XUI_API const char* xuiIconGetTags(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->sTags : NULL;
}

XUI_API int xuiIconGetSourceType(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->iSourceType : 0;
}

XUI_API uint32_t xuiIconGetGeneration(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->iGeneration : 0u;
}

XUI_API void* xuiIconGetUser(xui_icon pIcon)
{
	return __xuiIconValid(pIcon) ? pIcon->pUser : NULL;
}

XUI_API int xuiIconSetMetadata(xui_icon pIcon, const xui_icon_desc_t* pDesc)
{
	char* sDisplayName;
	char* sTags;

	if ( !__xuiIconValid(pIcon) || (pDesc == NULL) || !__xuiIconDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sDisplayName = __xuiIconStringDuplicate(pDesc->sDisplayName);
	if ( (pDesc->sDisplayName != NULL) && (sDisplayName == NULL) ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	sTags = __xuiIconStringDuplicate(pDesc->sTags);
	if ( (pDesc->sTags != NULL) && (sTags == NULL) ) {
		if ( sDisplayName != NULL ) xrtFree(sDisplayName);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pIcon->sDisplayName != NULL ) xrtFree(pIcon->sDisplayName);
	if ( pIcon->sTags != NULL ) xrtFree(pIcon->sTags);
	pIcon->sDisplayName = sDisplayName;
	pIcon->sTags = sTags;
	pIcon->iFlags = pDesc->iFlags;
	pIcon->pUser = pDesc->pUser;
	__xuiIconChanged(pIcon, 0);
	return XUI_OK;
}

XUI_API int xuiIconTouch(xui_icon pIcon)
{
	if ( !__xuiIconValid(pIcon) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiIconChanged(pIcon, 1);
	return XUI_OK;
}

XUI_API int xuiIconGetIntrinsicSize(xui_icon pIcon, xui_vec2_t* pSize)
{
	return __xuiIconIntrinsicSizeDepth(pIcon, pSize, 0);
}

XUI_API int xuiIconPrepare(xui_icon pIcon, float fWidth, float fHeight)
{
	return __xuiIconPrepareDepth(pIcon, fWidth, fHeight, 0);
}

XUI_API int xuiIconDraw(xui_painter pPainter, xui_icon pIcon, xui_rect_t tRect, const xui_icon_draw_desc_t* pDesc)
{
	if ( !__xuiIconDrawDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiIconDrawDepth(pPainter, pIcon, tRect, pDesc, 0, 1);
}

XUI_API int xuiIconDrawById(xui_painter pPainter, xui_icon_category pCategory, xui_icon_id iId, xui_rect_t tRect, const xui_icon_draw_desc_t* pDesc)
{
	xui_icon pIcon;

	pIcon = xuiIconFindById(pCategory, iId);
	return (pIcon != NULL) ? xuiIconDraw(pPainter, pIcon, tRect, pDesc) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiIconDrawByName(xui_painter pPainter, xui_icon_category pCategory, const char* sName, xui_rect_t tRect, const xui_icon_draw_desc_t* pDesc)
{
	xui_icon pIcon;

	pIcon = xuiIconFind(pCategory, sName);
	return (pIcon != NULL) ? xuiIconDraw(pPainter, pIcon, tRect, pDesc) : XUI_ERROR_INVALID_ARGUMENT;
}
