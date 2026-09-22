#include "xui_internal.h"
#include "../xge.h"

#if defined(_WIN32) || defined(_WIN64)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <imm.h>
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#define XUI_PROXY_XGE_SURFACE_MAGIC	0x58554953u
#define XUI_PROXY_XGE_FONT_MAGIC	0x58554946u
#define XUI_PROXY_XGE_DRAW_MAGIC	0x58554944u
#define XUI_PROXY_XGE_PI		3.14159265358979323846f
#define XUI_PROXY_XGE_PI_HALF		1.57079632679489661923f
#define XUI_PROXY_XGE_PI_TWO		6.28318530717958647692f

#define XUI_PROXY_XGE_KEY_LEFT_SHIFT	340
#define XUI_PROXY_XGE_KEY_LEFT_CONTROL	341
#define XUI_PROXY_XGE_KEY_LEFT_ALT	342
#define XUI_PROXY_XGE_KEY_LEFT_SUPER	343
#define XUI_PROXY_XGE_KEY_RIGHT_SHIFT	344
#define XUI_PROXY_XGE_KEY_RIGHT_CONTROL	345
#define XUI_PROXY_XGE_KEY_RIGHT_ALT	346
#define XUI_PROXY_XGE_KEY_RIGHT_SUPER	347

typedef struct xui_proxy_xge_input_transform_t {
	xui_context pContext;
	xui_rect_t tWindowRect;
	xui_size_t tViewport;
	xui_rect_t tCandidateLocalRect;
	int bValid;
	int bHasCandidateLocalRect;
} xui_proxy_xge_input_transform_t;

static xui_proxy_xge_input_transform_t g_xuiProxyXgeInputTransform;

struct xui_surface_t {
	uint32_t iMagic;
	int iKind;
	int iFormat;
	uint32_t iFlags;
	uint32_t iGeneration;
	xge_texture_t tTexture;
	xge_render_target_t tTarget;
};

struct xui_font_t {
	uint32_t iMagic;
	uint32_t iFlags;
	xge_font_t tFont;
	xge_font_t tFallbackFont;
	int bHasFallback;
};

struct xui_draw_context_t {
	uint32_t iMagic;
	xui_surface pTarget;
	xge_pass_t tPass;
	int bDirty;
};

static uint32_t __xuiProxyXgeImageFlags(uint32_t iFlags)
{
	(void)iFlags;
	return XGE_IMAGE_PREMULTIPLIED;
}

static uint32_t __xuiProxyXgeDrawFlags(uint32_t iFlags)
{
	uint32_t iDrawFlags;

	iDrawFlags = 0;
	if ( (iFlags & XUI_SURFACE_DRAW_FLIP_X) != 0 ) {
		iDrawFlags |= XGE_DRAW_FLIP_X;
	}
	if ( (iFlags & XUI_SURFACE_DRAW_FLIP_Y) != 0 ) {
		iDrawFlags |= XGE_DRAW_FLIP_Y;
	}
	if ( (iFlags & XUI_SURFACE_DRAW_SCREEN_SPACE) != 0 ) {
		iDrawFlags |= XGE_DRAW_SCREEN_SPACE;
	}
	return iDrawFlags;
}

static uint32_t __xuiProxyXgeSurfaceFlags(uint32_t iFlags)
{
	iFlags &= ~XUI_SURFACE_ALPHA_STRAIGHT;
	iFlags |= XUI_SURFACE_ALPHA_PREMULTIPLIED;
	return iFlags;
}

static xge_sampler_t __xuiProxyXgeSampler(const xui_surface_sampler_t* pSampler)
{
	xge_sampler_t tSampler;

	tSampler = xgeSamplerDefault();
	if ( pSampler == NULL ) {
		return tSampler;
	}
	if ( pSampler->iMinFilter == XUI_SURFACE_FILTER_NEAREST ) {
		tSampler.iMinFilter = XGE_FILTER_NEAREST;
	} else if ( pSampler->iMinFilter == XUI_SURFACE_FILTER_LINEAR ) {
		tSampler.iMinFilter = XGE_FILTER_LINEAR;
	}
	if ( pSampler->iMagFilter == XUI_SURFACE_FILTER_NEAREST ) {
		tSampler.iMagFilter = XGE_FILTER_NEAREST;
	} else if ( pSampler->iMagFilter == XUI_SURFACE_FILTER_LINEAR ) {
		tSampler.iMagFilter = XGE_FILTER_LINEAR;
	}
	if ( pSampler->iWrapS == XUI_SURFACE_WRAP_REPEAT ) {
		tSampler.iWrapS = XGE_WRAP_REPEAT;
	} else if ( pSampler->iWrapS == XUI_SURFACE_WRAP_CLAMP ) {
		tSampler.iWrapS = XGE_WRAP_CLAMP;
	}
	if ( pSampler->iWrapT == XUI_SURFACE_WRAP_REPEAT ) {
		tSampler.iWrapT = XGE_WRAP_REPEAT;
	} else if ( pSampler->iWrapT == XUI_SURFACE_WRAP_CLAMP ) {
		tSampler.iWrapT = XGE_WRAP_CLAMP;
	}
	return tSampler;
}

static xui_surface_sampler_t __xuiProxyXgeSurfaceSampler(xge_sampler_t tSampler)
{
	xui_surface_sampler_t tRet;

	tRet.iMinFilter = (tSampler.iMinFilter == XGE_FILTER_NEAREST) ? XUI_SURFACE_FILTER_NEAREST : XUI_SURFACE_FILTER_LINEAR;
	tRet.iMagFilter = (tSampler.iMagFilter == XGE_FILTER_NEAREST) ? XUI_SURFACE_FILTER_NEAREST : XUI_SURFACE_FILTER_LINEAR;
	tRet.iWrapS = (tSampler.iWrapS == XGE_WRAP_REPEAT) ? XUI_SURFACE_WRAP_REPEAT : XUI_SURFACE_WRAP_CLAMP;
	tRet.iWrapT = (tSampler.iWrapT == XGE_WRAP_REPEAT) ? XUI_SURFACE_WRAP_REPEAT : XUI_SURFACE_WRAP_CLAMP;
	return tRet;
}

static int __xuiProxyXgeAsciiEqualNoCase(const char* sA, const char* sB)
{
	char cA;
	char cB;

	if ( (sA == NULL) || (sB == NULL) ) {
		return 0;
	}
	while ( (*sA != 0) && (*sB != 0) ) {
		cA = *sA++;
		cB = *sB++;
		if ( (cA >= 'A') && (cA <= 'Z') ) {
			cA = (char)(cA + ('a' - 'A'));
		}
		if ( (cB >= 'A') && (cB <= 'Z') ) {
			cB = (char)(cB + ('a' - 'A'));
		}
		if ( cA != cB ) {
			return 0;
		}
	}
	return (*sA == 0) && (*sB == 0);
}

static int __xuiProxyXgeFontUseXrf(const char* sPath, uint32_t iFlags)
{
	const char* sExt;

	if ( (iFlags & XUI_FONT_FORMAT_XRF) != 0 ) {
		return 1;
	}
	if ( (iFlags & XUI_FONT_FORMAT_TTF) != 0 ) {
		return 0;
	}
	if ( sPath == NULL ) {
		return 0;
	}
	sExt = strrchr(sPath, '.');
	return __xuiProxyXgeAsciiEqualNoCase(sExt, ".xrf");
}

static uint32_t __xuiProxyXgeTextFlags(uint32_t iFlags)
{
	uint32_t iTextFlags;

	iTextFlags = XGE_TEXT_SCREEN_SPACE;
	if ( (iFlags & XUI_TEXT_ALIGN_RIGHT) == XUI_TEXT_ALIGN_RIGHT ) {
		iTextFlags |= XGE_TEXT_ALIGN_RIGHT;
	} else if ( (iFlags & XUI_TEXT_ALIGN_CENTER) == XUI_TEXT_ALIGN_CENTER ) {
		iTextFlags |= XGE_TEXT_ALIGN_CENTER;
	}
	if ( (iFlags & XUI_TEXT_ALIGN_BOTTOM) == XUI_TEXT_ALIGN_BOTTOM ) {
		iTextFlags |= XGE_TEXT_ALIGN_BOTTOM;
	} else if ( (iFlags & XUI_TEXT_ALIGN_MIDDLE) == XUI_TEXT_ALIGN_MIDDLE ) {
		iTextFlags |= XGE_TEXT_ALIGN_MIDDLE;
	}
	if ( (iFlags & XUI_TEXT_CLIP) != 0 ) {
		iTextFlags |= XGE_TEXT_CLIP;
	}
	if ( (iFlags & XUI_TEXT_UNDERLINE) != 0 ) {
		iTextFlags |= XGE_TEXT_UNDERLINE;
	}
	return iTextFlags;
}

static int __xuiProxyXgeFontValid(xui_font pFont)
{
	return (pFont != NULL) &&
	       (pFont->iMagic == XUI_PROXY_XGE_FONT_MAGIC) &&
	       (pFont->tFont.iRefCount > 0);
}

static int __xuiProxyXgeSurfaceValid(xui_surface pSurface)
{
	xge_texture pTexture;

	if ( (pSurface == NULL) ||
	     (pSurface->iMagic != XUI_PROXY_XGE_SURFACE_MAGIC) ||
	     (pSurface->iKind != XUI_SURFACE_KIND_TEXTURE) ||
	     (pSurface->iFormat != XUI_SURFACE_FORMAT_RGBA8) ) {
		return 0;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		pTexture = xgeRenderTargetTexture(&pSurface->tTarget);
		return (pTexture != NULL) && (pTexture->iRefCount > 0);
	}
	return (pSurface != NULL) &&
	       (pSurface->iMagic == XUI_PROXY_XGE_SURFACE_MAGIC) &&
	       (pSurface->iKind == XUI_SURFACE_KIND_TEXTURE) &&
	       (pSurface->iFormat == XUI_SURFACE_FORMAT_RGBA8) &&
	       (pSurface->tTexture.iRefCount > 0);
}

static int __xuiProxyXgeSurfaceTargetValid(xui_surface pSurface)
{
	return __xuiProxyXgeSurfaceValid(pSurface) &&
	       ((pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0) &&
	       ((pSurface->tTarget.iFlags & XGE_RENDER_TARGET_TEXTURE) != 0);
}

static xge_texture __xuiProxyXgeSurfaceTexture(xui_surface pSurface)
{
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return NULL;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		return xgeRenderTargetTexture(&pSurface->tTarget);
	}
	return &pSurface->tTexture;
}

static int __xuiProxyXgeSurfaceSize(xui_surface pSurface, int* pWidth, int* pHeight)
{
	xge_texture pTexture;

	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidth != NULL ) {
		*pWidth = pTexture->iWidth;
	}
	if ( pHeight != NULL ) {
		*pHeight = pTexture->iHeight;
	}
	return XGE_OK;
}

static void __xuiProxyXgePremultiplyRGBA(unsigned char* pDst, const unsigned char* pSrc, int iCount)
{
	unsigned int iA;
	int i;

	for ( i = 0; i < iCount; i++ ) {
		iA = (unsigned int)pSrc[(i * 4) + 3];
		pDst[(i * 4) + 0] = (unsigned char)(((unsigned int)pSrc[(i * 4) + 0] * iA + 127u) / 255u);
		pDst[(i * 4) + 1] = (unsigned char)(((unsigned int)pSrc[(i * 4) + 1] * iA + 127u) / 255u);
		pDst[(i * 4) + 2] = (unsigned char)(((unsigned int)pSrc[(i * 4) + 2] * iA + 127u) / 255u);
		pDst[(i * 4) + 3] = pSrc[(i * 4) + 3];
	}
}

static int __xuiProxyXgeSurfacePackRGBA(const void* pPixels, int iWidth, int iHeight, int iStride, uint32_t iFlags, void** ppPacked, const void** ppOutPixels)
{
	unsigned char* pPacked;
	const unsigned char* pSrc;
	int iRowBytes;
	int bPremultiply;
	int i;

	if ( (ppPacked == NULL) || (ppOutPixels == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppPacked = NULL;
	*ppOutPixels = NULL;
	if ( pPixels == NULL ) {
		return XGE_OK;
	}
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRowBytes = iWidth * 4;
	if ( iStride <= 0 ) {
		iStride = iRowBytes;
	}
	if ( iStride < iRowBytes ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	bPremultiply = ((iFlags & XUI_SURFACE_ALPHA_STRAIGHT) != 0);
	if ( (iStride == iRowBytes) && !bPremultiply ) {
		*ppOutPixels = pPixels;
		return XGE_OK;
	}
	pPacked = (unsigned char*)xrtMalloc((size_t)iRowBytes * (size_t)iHeight);
	if ( pPacked == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSrc = (const unsigned char*)pPixels;
	for ( i = 0; i < iHeight; i++ ) {
		memcpy(pPacked + ((size_t)i * (size_t)iRowBytes), pSrc + ((size_t)i * (size_t)iStride), (size_t)iRowBytes);
	}
	if ( bPremultiply ) {
		__xuiProxyXgePremultiplyRGBA(pPacked, pPacked, iWidth * iHeight);
	}
	*ppPacked = pPacked;
	*ppOutPixels = pPacked;
	return XGE_OK;
}

static xui_surface __xuiProxyXgeSurfaceAlloc(uint32_t iFlags)
{
	xui_surface pSurface;

	pSurface = (xui_surface)xrtCalloc(1, sizeof(*pSurface));
	if ( pSurface == NULL ) {
		return NULL;
	}
	pSurface->iMagic = XUI_PROXY_XGE_SURFACE_MAGIC;
	pSurface->iKind = XUI_SURFACE_KIND_TEXTURE;
	pSurface->iFormat = XUI_SURFACE_FORMAT_RGBA8;
	pSurface->iFlags = __xuiProxyXgeSurfaceFlags(iFlags);
	pSurface->iGeneration = 1u;
	return pSurface;
}

static void __xuiProxyXgeSurfaceBump(xui_surface pSurface)
{
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return;
	}
	pSurface->iGeneration++;
	if ( pSurface->iGeneration == 0 ) {
		pSurface->iGeneration = 1u;
	}
}

static void __xuiProxyXgeSurfaceVertexCopy(xge_vertex_t* pDst, const xui_surface_vertex_t* pSrc)
{
	int i;

	for ( i = 0; i < 4; i++ ) {
		pDst[i].fX = pSrc[i].fX;
		pDst[i].fY = pSrc[i].fY;
		pDst[i].fZ = pSrc[i].fZ;
		pDst[i].fW = pSrc[i].fW;
		pDst[i].fU = pSrc[i].fU;
		pDst[i].fV = pSrc[i].fV;
		pDst[i].iColor = pSrc[i].iColor;
	}
}

static int __xuiProxyXgeSurfaceIsTarget(xui_surface pSurface)
{
	return __xuiProxyXgeSurfaceValid(pSurface) &&
	       ((pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0);
}

static uint32_t __xuiProxyXgeSurfaceDrawFlags(xui_surface pSurface, uint32_t iFlags)
{
	uint32_t iDrawFlags;

	iDrawFlags = __xuiProxyXgeDrawFlags(iFlags);
	if ( __xuiProxyXgeSurfaceIsTarget(pSurface) ) {
		iDrawFlags ^= XGE_DRAW_FLIP_Y;
	}
	return iDrawFlags;
}

static xge_rect_t __xuiProxyXgeSurfaceSourceRect(xui_surface pSurface, xui_rect_t tSrc)
{
	xge_rect_t tRet;
	int iWidth;
	int iHeight;

	tRet.fX = tSrc.fX;
	tRet.fY = tSrc.fY;
	tRet.fW = tSrc.fW;
	tRet.fH = tSrc.fH;
	if ( __xuiProxyXgeSurfaceSize(pSurface, &iWidth, &iHeight) != XGE_OK ) {
		return tRet;
	}
	if ( tRet.fW == 0.0f ) {
		tRet.fW = (float)iWidth;
	}
	if ( tRet.fH == 0.0f ) {
		tRet.fH = (float)iHeight;
	}
	if ( __xuiProxyXgeSurfaceIsTarget(pSurface) ) {
		tRet.fY = (float)iHeight - (tRet.fY + tRet.fH);
	}
	return tRet;
}

static void __xuiProxyXgeSurfaceVertexMapUV(xui_surface pSurface, xge_vertex_t* pVertices, uint32_t iFlags)
{
	int bFlipX;
	int bFlipY;
	int i;

	if ( pVertices == NULL ) {
		return;
	}
	bFlipX = ((iFlags & XUI_SURFACE_DRAW_FLIP_X) != 0);
	bFlipY = ((iFlags & XUI_SURFACE_DRAW_FLIP_Y) != 0);
	if ( __xuiProxyXgeSurfaceIsTarget(pSurface) ) {
		bFlipY = !bFlipY;
	}
	if ( !bFlipX && !bFlipY ) {
		return;
	}
	for ( i = 0; i < 4; i++ ) {
		if ( bFlipX ) {
			pVertices[i].fU = 1.0f - pVertices[i].fU;
		}
		if ( bFlipY ) {
			pVertices[i].fV = 1.0f - pVertices[i].fV;
		}
	}
}

static xge_vec2_t __xuiProxyXgeVec2(xui_vec2_t tPoint)
{
	xge_vec2_t tRet;

	tRet.fX = tPoint.fX;
	tRet.fY = tPoint.fY;
	return tRet;
}

static xge_rect_t __xuiProxyXgeRect(xui_rect_t tRect)
{
	xge_rect_t tRet;

	tRet.fX = tRect.fX;
	tRet.fY = tRect.fY;
	tRet.fW = tRect.fW;
	tRet.fH = tRect.fH;
	return tRet;
}

static xge_rect_i_t __xuiProxyXgePixelRect(xui_rect_t tRect)
{
	return (xge_rect_i_t){tRect.fX, tRect.fY, tRect.fW, tRect.fH};
}

static int __xuiProxyXgeTargetBegin(xui_proxy pProxy, xui_surface pTarget, xge_pass_t* pPass)
{
	if ( (pProxy == NULL) || (pPass == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)xgeFlush();
	xgePassInit(pPass, &pTarget->tTarget, 0, 0);
	return xgePassBegin(pPass);
}

static int __xuiProxyXgeTargetEnd(xge_pass_t* pPass, int iRet)
{
	int iEndRet;

	if ( pPass == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeFlush();
	}
	iEndRet = xgePassEnd(pPass);
	if ( (iEndRet != XGE_OK) && (iRet == XGE_OK) ) {
		iRet = iEndRet;
	}
	return iRet;
}

static int __xuiProxyXgeTargetEndDirty(xge_pass_t* pPass, xui_surface pTarget, int iRet)
{
	iRet = __xuiProxyXgeTargetEnd(pPass, iRet);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	return iRet;
}

static int __xuiProxyXgeDrawValid(xui_draw_context pDraw)
{
	return (pDraw != NULL) &&
	       (pDraw->iMagic == XUI_PROXY_XGE_DRAW_MAGIC) &&
	       (pDraw->tPass.bActive != 0) &&
	       __xuiProxyXgeSurfaceTargetValid(pDraw->pTarget);
}

static void __xuiProxyXgeDrawMarkDirty(xui_draw_context pDraw)
{
	if ( __xuiProxyXgeDrawValid(pDraw) ) {
		pDraw->bDirty = 1;
	}
}

static int __xuiProxyXgeClearRectLocal(xui_rect_t tRect, uint32_t iColor)
{
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) {
		return XGE_OK;
	}
	(void)xgeFlush();
	xgeClipSetPixels(__xuiProxyXgePixelRect(tRect));
	xgeClear(iColor);
	xgeClipClear();
	return XGE_OK;
}

static xge_rect_t __xuiProxyXgeIntersectClip(xge_rect_t tA, xge_rect_t tB)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = (tA.fX > tB.fX) ? tA.fX : tB.fX;
	fTop = (tA.fY > tB.fY) ? tA.fY : tB.fY;
	fRight = ((tA.fX + tA.fW) < (tB.fX + tB.fW)) ? (tA.fX + tA.fW) : (tB.fX + tB.fW);
	fBottom = ((tA.fY + tA.fH) < (tB.fY + tB.fH)) ? (tA.fY + tA.fH) : (tB.fY + tB.fH);
	if ( (fRight <= fLeft) || (fBottom <= fTop) ) {
		return (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return (xge_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop};
}

static int __xuiProxyXgeGetCaps(xui_proxy pProxy, xui_proxy_caps_t* pCaps)
{
	xge_sampler_t tSampler;

	if ( (pProxy == NULL) || (pCaps == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	memset(pCaps, 0, sizeof(*pCaps));
	pCaps->iSize = (uint32_t)sizeof(*pCaps);
	pCaps->iCaps = XUI_PROXY_CAP_SURFACE_TARGET |
	               XUI_PROXY_CAP_SURFACE_READ |
	               XUI_PROXY_CAP_SURFACE_QUAD |
	               XUI_PROXY_CAP_SURFACE_CLEAR_RECT |
	               XUI_PROXY_CAP_SURFACE_SAMPLER |
	               XUI_PROXY_CAP_DRAW_CONTEXT |
	               XUI_PROXY_CAP_SHAPE |
	               XUI_PROXY_CAP_MESH_TRIANGLES |
	               XUI_PROXY_CAP_PATH_FILL |
	               XUI_PROXY_CAP_PATH_STROKE |
	               XUI_PROXY_CAP_PATH_DASH |
	               XUI_PROXY_CAP_PATH_AA |
	               XUI_PROXY_CAP_SVG_SURFACE |
	               XUI_PROXY_CAP_FONT_TTF |
	               XUI_PROXY_CAP_FONT_XRF |
	               XUI_PROXY_CAP_TEXT;
	pCaps->iSurfaceFormat = XUI_SURFACE_FORMAT_RGBA8;
	pCaps->iInternalAlpha = XUI_SURFACE_ALPHA_PREMULTIPLIED;
	tSampler = xgeSamplerDefault();
	pCaps->tDefaultSampler = __xuiProxyXgeSurfaceSampler(tSampler);
	return XGE_OK;
}

static int __xuiProxyXgeClipboardSetText(xui_proxy pProxy, const char* sText)
{
	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	xgeClipboardSetText(sText);
	return XGE_OK;
}

static int __xuiProxyXgeClipboardGetText(xui_proxy pProxy, char* sText, int iCapacity)
{
	const char* sSource;
	size_t iLength;
	size_t iCopy;

	if ( (pProxy == NULL) || (sText == NULL) || (iCapacity <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	sSource = xgeClipboardGetText();
	if ( sSource == NULL ) {
		sSource = "";
	}
	iLength = strlen(sSource);
	iCopy = iLength;
	if ( iCopy >= (size_t)iCapacity ) {
		iCopy = (size_t)iCapacity - 1u;
	}
	if ( iCopy > 0u ) {
		memcpy(sText, sSource, iCopy);
	}
	sText[iCopy] = 0;
	return (int)iLength;
}

static int __xuiProxyXgeClipboardSetItems(xui_proxy pProxy,
	const xui_clipboard_item_t* pItems, int iItemCount)
{
	xge_clipboard_item_t* pXgeItems;
	int i;
	int iRet;
	if ( pProxy == NULL || pItems == NULL || iItemCount <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	pXgeItems = (xge_clipboard_item_t*)xrtMalloc((size_t)iItemCount * sizeof(*pXgeItems));
	if ( pXgeItems == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	for ( i = 0; i < iItemCount; i++ ) {
		pXgeItems[i].sFormat = pItems[i].sFormat;
		pXgeItems[i].pData = pItems[i].pData;
		pXgeItems[i].iDataSize = pItems[i].iDataSize;
	}
	iRet = xgeClipboardSetItems(pXgeItems, iItemCount);
	xrtFree(pXgeItems);
	return iRet;
}

static int __xuiProxyXgeClipboardGetData(xui_proxy pProxy, const char* sFormat,
	void* pData, size_t iCapacity)
{
	if ( pProxy == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	return xgeClipboardGetData(sFormat, pData, iCapacity);
}

static int __xuiProxyXgeImeGetEnabled(xui_proxy pProxy)
{
	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	return xgeImeGetEnabled();
}

static int __xuiProxyXgeImeSetEnabled(xui_proxy pProxy, int bEnabled)
{
	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	return xgeImeSetEnabled(bEnabled);
}

static int __xuiProxyXgeImeSetCandidateRect(xui_proxy pProxy, xui_rect_t tRect)
{
	xge_rect_t tXgeRect;
	xui_proxy_xge_input_transform_t* pTransform;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pTransform = &g_xuiProxyXgeInputTransform;
	pTransform->tCandidateLocalRect = tRect;
	pTransform->bHasCandidateLocalRect = 1;
	if ( pTransform->bValid && pTransform->tViewport.iW > 0 &&
	     pTransform->tViewport.iH > 0 ) {
		tXgeRect.fX = pTransform->tWindowRect.fX +
			(float)tRect.fX * (float)pTransform->tWindowRect.fW / (float)pTransform->tViewport.iW;
		tXgeRect.fY = pTransform->tWindowRect.fY +
			(float)tRect.fY * (float)pTransform->tWindowRect.fH / (float)pTransform->tViewport.iH;
		tXgeRect.fW = (float)tRect.fW * (float)pTransform->tWindowRect.fW / (float)pTransform->tViewport.iW;
		tXgeRect.fH = (float)tRect.fH * (float)pTransform->tWindowRect.fH / (float)pTransform->tViewport.iH;
	} else {
		tXgeRect.fX = tRect.fX;
		tXgeRect.fY = tRect.fY;
		tXgeRect.fW = tRect.fW;
		tXgeRect.fH = tRect.fH;
	}
	return xgeImeSetCandidateRect(tXgeRect);
}

static int __xuiProxyXgeSurfaceCreate(xui_proxy pProxy, xui_surface* ppSurface, const xui_surface_desc_t* pDesc)
{
	xui_surface pSurface;
	int iRet;
	int iKind;
	int iFormat;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (pDesc == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	iKind = (pDesc->iKind != 0) ? pDesc->iKind : XUI_SURFACE_KIND_TEXTURE;
	iFormat = (pDesc->iFormat != 0) ? pDesc->iFormat : XUI_SURFACE_FORMAT_RGBA8;
	if ( (iKind != XUI_SURFACE_KIND_TEXTURE) || (iFormat != XUI_SURFACE_FORMAT_RGBA8) ||
	     (pDesc->iWidth <= 0) || (pDesc->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSurface = __xuiProxyXgeSurfaceAlloc(pDesc->iFlags);
	if ( pSurface == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		iRet = xgeRenderTargetCreate(&pSurface->tTarget, pDesc->iWidth, pDesc->iHeight);
	} else {
		iRet = xgeTextureCreateRGBA(&pSurface->tTexture, pDesc->iWidth, pDesc->iHeight, NULL);
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceCreateRGBA(xui_proxy pProxy, xui_surface* ppSurface, int iWidth, int iHeight, const void* pPixels, int iStride, uint32_t iFlags)
{
	xui_surface pSurface;
	void* pPacked;
	const void* pUploadPixels;
	int iRet;
	xui_rect_i_t tRect;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	iRet = __xuiProxyXgeSurfacePackRGBA(pPixels, iWidth, iHeight, iStride, iFlags, &pPacked, &pUploadPixels);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSurface = __xuiProxyXgeSurfaceAlloc(iFlags);
	if ( pSurface == NULL ) {
		if ( pPacked != NULL ) {
			xrtFree(pPacked);
		}
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		iRet = xgeRenderTargetCreate(&pSurface->tTarget, iWidth, iHeight);
		if ( (iRet == XGE_OK) && (pUploadPixels != NULL) ) {
			tRect.iX = 0;
			tRect.iY = 0;
			tRect.iW = iWidth;
			tRect.iH = iHeight;
			iRet = xgeTextureUpdateRGBA(&pSurface->tTarget.tTexture, tRect.iX, tRect.iY, tRect.iW, tRect.iH, pUploadPixels, iWidth * 4);
		}
	} else {
		iRet = xgeTextureCreateRGBA(&pSurface->tTexture, iWidth, iHeight, pUploadPixels);
	}
	if ( pPacked != NULL ) {
		xrtFree(pPacked);
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceLoadFile(xui_proxy pProxy, xui_surface* ppSurface, const char* sPath, uint32_t iFlags)
{
	xui_surface pSurface;
	int iRet;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	pSurface = __xuiProxyXgeSurfaceAlloc(iFlags);
	if ( pSurface == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeTextureLoadEx(&pSurface->tTexture, sPath, __xuiProxyXgeImageFlags(iFlags));
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceLoadMemory(xui_proxy pProxy, xui_surface* ppSurface, const void* pData, int iSize, uint32_t iFlags)
{
	xui_surface pSurface;
	int iRet;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (pData == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	pSurface = __xuiProxyXgeSurfaceAlloc(iFlags);
	if ( pSurface == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeTextureLoadMemoryEx(&pSurface->tTexture, pData, iSize, __xuiProxyXgeImageFlags(iFlags));
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceLoadSvgFile(xui_proxy pProxy, xui_surface* ppSurface, const char* sPath, int iWidth, int iHeight, uint32_t iFlags)
{
	xui_surface pSurface;
	int iRet;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (sPath == NULL) ||
	     (iWidth <= 0) || (iHeight <= 0) ||
	     ((iFlags & XUI_SURFACE_USAGE_TARGET) != 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	pSurface = __xuiProxyXgeSurfaceAlloc(iFlags);
	if ( pSurface == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeSvgTextureLoad(&pSurface->tTexture, sPath, iWidth, iHeight);
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceLoadSvgMemory(xui_proxy pProxy, xui_surface* ppSurface, const void* pData, int iSize, int iWidth, int iHeight, uint32_t iFlags)
{
	xui_surface pSurface;
	int iRet;

	if ( (pProxy == NULL) || (ppSurface == NULL) || (pData == NULL) ||
	     (iSize <= 0) || (iWidth <= 0) || (iHeight <= 0) ||
	     ((iFlags & XUI_SURFACE_USAGE_TARGET) != 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	pSurface = __xuiProxyXgeSurfaceAlloc(iFlags);
	if ( pSurface == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeSvgTextureLoadMemory(&pSurface->tTexture, pData, iSize, iWidth, iHeight);
	if ( iRet != XGE_OK ) {
		xrtFree(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceUpdateRGBA(xui_proxy pProxy, xui_surface pSurface, xui_rect_i_t tRect, const void* pPixels, int iStride)
{
	xge_texture pTexture;
	int iRet;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeTextureUpdateRGBA(pTexture, tRect.iX, tRect.iY, tRect.iW, tRect.iH, pPixels, iStride);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pSurface);
	}
	return iRet;
}

static int __xuiProxyXgeSurfaceReadRGBA(xui_proxy pProxy, xui_surface pSurface, void* pPixels, int iStride)
{
	xge_texture pTexture;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		return xgeRenderTargetReadPixels(&pSurface->tTarget, pPixels, iStride);
	}
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return xgeTextureReadPixels(pTexture, pPixels, iStride);
}

static int __xuiProxyXgeSurfaceGetDesc(xui_proxy pProxy, xui_surface pSurface, xui_surface_desc_t* pDesc)
{
	if ( (pProxy == NULL) || (pDesc == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iKind = pSurface->iKind;
	pDesc->iFormat = pSurface->iFormat;
	(void)__xuiProxyXgeSurfaceSize(pSurface, &pDesc->iWidth, &pDesc->iHeight);
	pDesc->iFlags = pSurface->iFlags;
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceDraw(xui_proxy pProxy, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{
	xge_draw_t tDraw;
	xge_texture pTexture;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tDst.fW <= 0.0f || tDst.fH <= 0.0f ) {
		return XGE_OK;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = pTexture;
	tDraw.tSrc = __xuiProxyXgeSurfaceSourceRect(pSurface, tSrc);
	tDraw.tDst.fX = tDst.fX;
	tDraw.tDst.fY = tDst.fY;
	tDraw.tDst.fW = tDst.fW;
	tDraw.tDst.fH = tDst.fH;
	tDraw.iColor = iColor;
	tDraw.iFlags = __xuiProxyXgeSurfaceDrawFlags(pSurface, iFlags);
	xgeDrawEx(&tDraw);
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceClear(xui_proxy pProxy, xui_surface pTarget, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)xgeFlush();
	xgePassInit(&tPass, &pTarget->tTarget, XGE_PASS_CLEAR_COLOR, iColor);
	iRet = xgePassBegin(&tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgePassEnd(&tPass);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	return iRet;
}

static int __xuiProxyXgeSurfaceClearRect(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xuiProxyXgeClearRectLocal(tRect, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, iRet);
}

static int __xuiProxyXgeSurfaceDrawTo(xui_proxy pProxy, xui_surface pTarget, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{
	xge_pass_t tPass;
	int iRet;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiProxyXgeSurfaceTargetValid(pTarget) || !__xuiProxyXgeSurfaceValid(pSurface) || (pTarget == pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tDst.fW <= 0.0f || tDst.fH <= 0.0f ) {
		return XGE_OK;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)xgeFlush();
	xgePassInit(&tPass, &pTarget->tTarget, 0, 0);
	iRet = xgePassBegin(&tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xuiProxyXgeSurfaceDraw(pProxy, pSurface, tSrc, tDst, iColor, iFlags | XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( iRet == XGE_OK ) {
		iRet = xgeFlush();
	}
	if ( xgePassEnd(&tPass) != XGE_OK && iRet == XGE_OK ) {
		iRet = XGE_ERROR_BACKEND_FAILED;
	}
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	return iRet;
}

static int __xuiProxyXgeSurfaceDrawQuad(xui_proxy pProxy, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{
	xge_vertex_t arrVertices[4];
	xge_texture pTexture;

	if ( (pProxy == NULL) || (pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (XGE_COLOR_GET_A(pVertices[0].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[1].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[2].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[3].iColor) == 0) ) {
		return XGE_OK;
	}
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)xgeFlush();
	if ( pTexture->iBackendId == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	__xuiProxyXgeSurfaceVertexCopy(arrVertices, pVertices);
	__xuiProxyXgeSurfaceVertexMapUV(pSurface, arrVertices, iFlags);
	xgeDrawQuad3D(pTexture, arrVertices, __xuiProxyXgeDrawFlags(iFlags & XUI_SURFACE_DRAW_SCREEN_SPACE));
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceDrawQuadTo(xui_proxy pProxy, xui_surface pTarget, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{
	xge_pass_t tPass;
	int iRet;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiProxyXgeSurfaceTargetValid(pTarget) || !__xuiProxyXgeSurfaceValid(pSurface) || (pTarget == pSurface) || (pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (XGE_COLOR_GET_A(pVertices[0].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[1].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[2].iColor) == 0) &&
	     (XGE_COLOR_GET_A(pVertices[3].iColor) == 0) ) {
		return XGE_OK;
	}
	(void)xgeFlush();
	xgePassInit(&tPass, &pTarget->tTarget, 0, 0);
	iRet = xgePassBegin(&tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xuiProxyXgeSurfaceDrawQuad(pProxy, pSurface, pVertices, iFlags | XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( xgePassEnd(&tPass) != XGE_OK && iRet == XGE_OK ) {
		iRet = XGE_ERROR_BACKEND_FAILED;
	}
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	return iRet;
}

static int __xuiProxyXgeSurfaceGetSampler(xui_proxy pProxy, xui_surface pSurface, xui_surface_sampler_t* pSampler)
{
	xge_texture pTexture;

	if ( (pProxy == NULL) || (pSampler == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pSampler = __xuiProxyXgeSurfaceSampler(xgeTextureGetSampler(pTexture));
	return XGE_OK;
}

static int __xuiProxyXgeSurfaceSetSampler(xui_proxy pProxy, xui_surface pSurface, const xui_surface_sampler_t* pSampler)
{
	xge_texture pTexture;
	xge_sampler_t tSampler;
	int iRet;

	if ( (pProxy == NULL) || (pSampler == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pTexture = __xuiProxyXgeSurfaceTexture(pSurface);
	if ( pTexture == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tSampler = __xuiProxyXgeSampler(pSampler);
	iRet = xgeTextureSetSampler(pTexture, &tSampler);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeSurfaceBump(pSurface);
	}
	return iRet;
}

static int __xuiProxyXgeSurfaceGetGeneration(xui_proxy pProxy, xui_surface pSurface, uint32_t* pGeneration)
{
	if ( (pProxy == NULL) || (pGeneration == NULL) || !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*pGeneration = pSurface->iGeneration;
	return XGE_OK;
}

static int __xuiProxyXgeShapePoint(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fSize, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fSize <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapePointPx(fX, fY, fSize, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeLine(xui_proxy pProxy, xui_surface pTarget, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeLinePx(fX0, fY0, fX1, fY1, fWidth, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeTriangleFill(xui_proxy pProxy, xui_surface pTarget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeTriangleFillPx(__xuiProxyXgeVec2(tA), __xuiProxyXgeVec2(tB), __xuiProxyXgeVec2(tC), iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeTriangleStroke(xui_proxy pProxy, xui_surface pTarget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeTriangleStrokePx(__xuiProxyXgeVec2(tA), __xuiProxyXgeVec2(tB), __xuiProxyXgeVec2(tC), fWidth, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeRectFill(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeRectFillPixels(__xuiProxyXgePixelRect(tRect), iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeRectStroke(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	{
		int iWidth = xuiInternalSnapSize(fWidth);
		xgeShapeRectBorderPixels(__xuiProxyXgePixelRect(tRect),
			(xge_edges_i_t){iWidth, iWidth, iWidth, iWidth}, iColor);
	}
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeCircleFill(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fRadius <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeCircleFillPx(fX, fY, fRadius, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeCircleStroke(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeSurfaceTargetValid(pTarget) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fRadius <= 0.0f ) return XGE_OK;
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeShapeCircleStrokePx(fX, fY, fRadius, fWidth, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeFontLoadFile(xui_proxy pProxy, xui_font* ppFont, const char* sPath, float fSize, uint32_t iFlags)
{
	xui_font pFont;
	int iRet;
	int bUseXrf;

	if ( (pProxy == NULL) || (ppFont == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppFont = NULL;
	pFont = (xui_font)xrtCalloc(1, sizeof(*pFont));
	if ( pFont == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	bUseXrf = __xuiProxyXgeFontUseXrf(sPath, iFlags);
	if ( bUseXrf ) {
		iRet = xgeFontLoadXRF(&pFont->tFont, sPath);
	} else {
		if ( fSize <= 0.0f ) {
			xrtFree(pFont);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iRet = xgeFontLoad(&pFont->tFont, sPath, fSize);
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pFont);
		return iRet;
	}
	if ( !bUseXrf && xgeFontFallbackGet(&pFont->tFallbackFont, fSize) == XGE_OK ) {
		xgeFontSetFallback(&pFont->tFont, &pFont->tFallbackFont);
		pFont->bHasFallback = 1;
	}
	pFont->iMagic = XUI_PROXY_XGE_FONT_MAGIC;
	pFont->iFlags = iFlags;
	*ppFont = pFont;
	return XGE_OK;
}

static int __xuiProxyXgeFontLoadMemory(xui_proxy pProxy, xui_font* ppFont, const void* pData, int iSize, float fSize, uint32_t iFlags)
{
	xui_font pFont;
	int iRet;

	if ( (pProxy == NULL) || (ppFont == NULL) || (pData == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppFont = NULL;
	pFont = (xui_font)xrtCalloc(1, sizeof(*pFont));
	if ( pFont == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( (iFlags & XUI_FONT_FORMAT_XRF) != 0 ) {
		iRet = xgeFontLoadXRFMemory(&pFont->tFont, pData, iSize);
	} else {
		if ( fSize <= 0.0f ) {
			xrtFree(pFont);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iRet = xgeFontLoadMemory(&pFont->tFont, pData, iSize, fSize);
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pFont);
		return iRet;
	}
	if ( (iFlags & XUI_FONT_FORMAT_XRF) == 0 && xgeFontFallbackGet(&pFont->tFallbackFont, fSize) == XGE_OK ) {
		xgeFontSetFallback(&pFont->tFont, &pFont->tFallbackFont);
		pFont->bHasFallback = 1;
	}
	pFont->iMagic = XUI_PROXY_XGE_FONT_MAGIC;
	pFont->iFlags = iFlags;
	*ppFont = pFont;
	return XGE_OK;
}

static int __xuiProxyXgeFontGetMetrics(xui_proxy pProxy, xui_font pFont, xui_font_metrics_t* pMetrics)
{
	xge_font_metrics_t tMetrics;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (pMetrics == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	memset(&tMetrics, 0, sizeof(tMetrics));
	tMetrics.iSize = sizeof(tMetrics);
	iRet = xgeFontGetMetrics(&pFont->tFont, &tMetrics);
	if ( iRet != XGE_OK ) return iRet;
	pMetrics->fSize = tMetrics.fPixelSize;
	pMetrics->fAscent = tMetrics.fAscent;
	pMetrics->fDescent = tMetrics.fDescent;
	pMetrics->fLineGap = tMetrics.fLineGap;
	pMetrics->fLineHeight = tMetrics.fLineHeight;
	return XGE_OK;
}

static void __xuiProxyXgeFontDestroy(xui_proxy pProxy, xui_font pFont)
{
	if ( pProxy == NULL ) {
		return;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeFontValid(pFont) ) {
		return;
	}
	xgeFontFree(&pFont->tFont);
	if ( pFont->bHasFallback ) {
		xgeFontFree(&pFont->tFallbackFont);
	}
	pFont->iMagic = 0;
	xrtFree(pFont);
}

static int __xuiProxyXgeZstdDecompress(xui_proxy pProxy, void* pOutput, int iOutputCapacity, const void* pInput, int iInputSize, int* pOutputSize)
{
	(void)pProxy;
	return xgeZstdDecompress(pOutput, iOutputCapacity, pInput, iInputSize, pOutputSize);
}

static int __xuiProxyXgeFontCreateSized(xui_proxy pProxy, xui_font* ppFont, xui_font pSource, float fSize)
{
	xui_font pFont;
	int iRet;
	if ( pProxy == NULL || ppFont == NULL || !__xuiProxyXgeFontValid(pSource) || fSize <= 0.0f )
		return XGE_ERROR_INVALID_ARGUMENT;
	*ppFont = NULL;
	pFont = (xui_font)xrtCalloc(1, sizeof(*pFont));
	if ( pFont == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	iRet = xgeFontCreateSized(&pFont->tFont, &pSource->tFont, fSize);
	if ( iRet != XGE_OK ) { xrtFree(pFont); return iRet; }
	if ( pSource->bHasFallback && xgeFontCreateSized(&pFont->tFallbackFont, &pSource->tFallbackFont, fSize) == XGE_OK ) {
		xgeFontSetFallback(&pFont->tFont, &pFont->tFallbackFont);
		pFont->bHasFallback = 1;
	}
	pFont->iMagic = XUI_PROXY_XGE_FONT_MAGIC;
	pFont->iFlags = pSource->iFlags;
	*ppFont = pFont;
	return XGE_OK;
}

static int __xuiProxyXgeTextMeasure(xui_proxy pProxy, xui_font pFont, const char* sText, xui_vec2_t* pSize)
{
	xge_vec2_t tSize;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	tSize = xgeTextMeasure(&pFont->tFont, sText);
	pSize->fX = (float)xuiInternalPixelCeil(tSize.fX);
	pSize->fY = (float)xuiInternalPixelCeil(tSize.fY);
	return XGE_OK;
}

static int __xuiProxyXgeTextDraw(xui_proxy pProxy, xui_surface pTarget, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	xge_pass_t tPass;
	int iRet;

	if ( pProxy == NULL || !__xuiProxyXgeSurfaceTargetValid(pTarget) ||
	     !__xuiProxyXgeFontValid(pFont) || sText == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeTextDrawRect(&pFont->tFont, sText, __xuiProxyXgeRect(tRect), iColor, __xuiProxyXgeTextFlags(iFlags));
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeDrawBegin(xui_proxy pProxy, xui_draw_context* ppDraw, xui_surface pTarget)
{
	xui_draw_context pDraw;
	int iRet;

	if ( (pProxy == NULL) || (ppDraw == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppDraw = NULL;
	pDraw = (xui_draw_context)xrtCalloc(1, sizeof(*pDraw));
	if ( pDraw == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pDraw->iMagic = XUI_PROXY_XGE_DRAW_MAGIC;
	pDraw->pTarget = pTarget;
	iRet = __xuiProxyXgeTargetBegin(pProxy, pTarget, &pDraw->tPass);
	if ( iRet != XGE_OK ) {
		xrtFree(pDraw);
		return iRet;
	}
	*ppDraw = pDraw;
	return XGE_OK;
}

static int __xuiProxyXgeDrawEnd(xui_proxy pProxy, xui_draw_context pDraw)
{
	xui_surface pTarget;
	int bDirty;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pTarget = pDraw->pTarget;
	bDirty = pDraw->bDirty;
	iRet = __xuiProxyXgeTargetEnd(&pDraw->tPass, XGE_OK);
	if ( (iRet == XGE_OK) && bDirty ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	pDraw->iMagic = 0;
	xrtFree(pDraw);
	return iRet;
}

static int __xuiProxyXgeDrawClearRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	iRet = __xuiProxyXgeClearRectLocal(tRect, iColor);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	return iRet;
}

static int __xuiProxyXgeDrawSurface(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiProxyXgeSurfaceDraw(pProxy, pSurface, tSrc, tDst, iColor, iFlags | XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	return iRet;
}

static int __xuiProxyXgeDrawSurfaceQuad(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiProxyXgeSurfaceDrawQuad(pProxy, pSurface, pVertices, iFlags | XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	return iRet;
}

static int __xuiProxyXgeDrawMeshTriangles(xui_proxy pProxy, xui_draw_context pDraw, const xui_mesh_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iFlags)
{
	xge_shape_vertex_t* pXgeVertices;
	int iRet;
	int i;

	(void)iFlags;
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (pVertices == NULL) ||
	     (iVertexCount <= 0) || (pIndices == NULL) || (iIndexCount <= 0) || ((iIndexCount % 3) != 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iVertexCount > (INT32_MAX / (int)sizeof(*pXgeVertices)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		if ( XGE_COLOR_GET_A(pVertices[i].iColor) != 0 ) {
			break;
		}
	}
	if ( i == iVertexCount ) {
		return XGE_OK;
	}
	pXgeVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pXgeVertices));
	if ( pXgeVertices == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		pXgeVertices[i].fX = pVertices[i].fX;
		pXgeVertices[i].fY = pVertices[i].fY;
		pXgeVertices[i].iColor = pVertices[i].iColor;
	}
	iRet = xgeShapeMeshFillPx(pXgeVertices, iVertexCount, pIndices, iIndexCount);
	xrtFree(pXgeVertices);
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	return iRet;
}

static int __xuiProxyXgePathAppend(xge_shape_ex pShape, const xui_path_command_t* pCommands, int iCommandCount)
{
	int i;
	int iRet;

	if ( (pShape == NULL) || (pCommands == NULL) || (iCommandCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = XGE_OK;
	for ( i = 0; (i < iCommandCount) && (iRet == XGE_OK); i++ ) {
		const xui_path_command_t* pCommand = &pCommands[i];
		if ( pCommand->iCommand == XUI_PATH_CMD_MOVE ) {
			iRet = xgeShapeExMoveTo(pShape, pCommand->arrPoints[0].fX, pCommand->arrPoints[0].fY);
		} else if ( pCommand->iCommand == XUI_PATH_CMD_LINE ) {
			iRet = xgeShapeExLineTo(pShape, pCommand->arrPoints[0].fX, pCommand->arrPoints[0].fY);
		} else if ( pCommand->iCommand == XUI_PATH_CMD_QUAD ) {
			iRet = xgeShapeExQuadTo(pShape,
				pCommand->arrPoints[0].fX, pCommand->arrPoints[0].fY,
				pCommand->arrPoints[1].fX, pCommand->arrPoints[1].fY);
		} else if ( pCommand->iCommand == XUI_PATH_CMD_CUBIC ) {
			iRet = xgeShapeExCubicTo(pShape,
				pCommand->arrPoints[0].fX, pCommand->arrPoints[0].fY,
				pCommand->arrPoints[1].fX, pCommand->arrPoints[1].fY,
				pCommand->arrPoints[2].fX, pCommand->arrPoints[2].fY);
		} else if ( pCommand->iCommand == XUI_PATH_CMD_CLOSE ) {
			iRet = xgeShapeExClose(pShape);
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	return iRet;
}

static int __xuiProxyXgePathStyle(xge_shape_ex pShape, const xui_path_style_t* pStyle)
{
	int iJoin;
	int iCap;
	int iRet;

	if ( (pShape == NULL) || (pStyle == NULL) || (pStyle->iSize < sizeof(*pStyle)) ||
	     ((pStyle->iFillRule != XUI_PATH_FILL_NON_ZERO) && (pStyle->iFillRule != XUI_PATH_FILL_EVEN_ODD)) ||
	     ((pStyle->iLineJoin != XUI_PATH_JOIN_MITER) && (pStyle->iLineJoin != XUI_PATH_JOIN_BEVEL) && (pStyle->iLineJoin != XUI_PATH_JOIN_ROUND)) ||
	     ((pStyle->iLineCap != XUI_PATH_CAP_BUTT) && (pStyle->iLineCap != XUI_PATH_CAP_SQUARE) && (pStyle->iLineCap != XUI_PATH_CAP_ROUND)) ||
	     (pStyle->fStrokeWidth < 0.0f) || (pStyle->iDashCount < 0) ||
	     ((pStyle->iDashCount > 0) && (pStyle->pDashPattern == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iJoin = XGE_SHAPE_EX_JOIN_MITER;
	if ( pStyle->iLineJoin == XUI_PATH_JOIN_BEVEL ) iJoin = XGE_SHAPE_EX_JOIN_BEVEL;
	else if ( pStyle->iLineJoin == XUI_PATH_JOIN_ROUND ) iJoin = XGE_SHAPE_EX_JOIN_ROUND;
	iCap = XGE_SHAPE_EX_CAP_BUTT;
	if ( pStyle->iLineCap == XUI_PATH_CAP_SQUARE ) iCap = XGE_SHAPE_EX_CAP_SQUARE;
	else if ( pStyle->iLineCap == XUI_PATH_CAP_ROUND ) iCap = XGE_SHAPE_EX_CAP_ROUND;
	iRet = xgeShapeExFillColor(pShape, pStyle->iFillColor);
	if ( iRet == XGE_OK ) iRet = xgeShapeExFillRule(pShape, pStyle->iFillRule == XUI_PATH_FILL_EVEN_ODD ? XGE_SHAPE_EX_FILL_EVEN_ODD : XGE_SHAPE_EX_FILL_NON_ZERO);
	if ( iRet == XGE_OK ) iRet = xgeShapeExStrokeColor(pShape, pStyle->iStrokeColor);
	if ( iRet == XGE_OK ) iRet = xgeShapeExStrokeWidth(pShape, pStyle->fStrokeWidth);
	if ( iRet == XGE_OK ) iRet = xgeShapeExStrokeJoin(pShape, iJoin);
	if ( iRet == XGE_OK ) iRet = xgeShapeExStrokeCap(pShape, iCap);
	if ( iRet == XGE_OK ) iRet = xgeShapeExStrokeDash(pShape, pStyle->pDashPattern, pStyle->iDashCount, pStyle->fDashOffset);
	return iRet;
}

static int __xuiProxyXgeDrawPath(xui_proxy pProxy, xui_draw_context pDraw, const xui_path_command_t* pCommands, int iCommandCount, const xui_path_style_t* pStyle, float fTolerance)
{
	xge_shape_ex pShape;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (pCommands == NULL) ||
	     (iCommandCount <= 0) || (pStyle == NULL) || (fTolerance <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pShape = NULL;
	iRet = xgeShapeExCreate(&pShape);
	if ( iRet == XGE_OK ) iRet = __xuiProxyXgePathAppend(pShape, pCommands, iCommandCount);
	if ( iRet == XGE_OK ) iRet = __xuiProxyXgePathStyle(pShape, pStyle);
	if ( iRet == XGE_OK ) iRet = xgeShapeExDrawPx(pShape, fTolerance);
	xgeShapeExDestroy(pShape);
	if ( iRet == XGE_OK ) __xuiProxyXgeDrawMarkDirty(pDraw);
	return iRet;
}

static int __xuiProxyXgeDrawSvgPath(xui_proxy pProxy, xui_draw_context pDraw, const char* sPath, xui_rect_t tViewBox, xui_rect_t tTarget, const xui_path_style_t* pStyle, float fTolerance)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex pShape;
	float fScaleX;
	float fScaleY;
	float fScale;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (sPath == NULL) ||
	     (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) ||
	     (pStyle == NULL) || (fTolerance <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tTarget.fW <= 0.0f || tTarget.fH <= 0.0f ) return XGE_OK;
	(void)pProxy;
	fScaleX = tTarget.fW / tViewBox.fW;
	fScaleY = tTarget.fH / tViewBox.fH;
	fScale = (fScaleX < fScaleY) ? fScaleX : fScaleY;
	tMatrix.fA = fScale;
	tMatrix.fB = 0.0f;
	tMatrix.fC = 0.0f;
	tMatrix.fD = fScale;
	tMatrix.fE = tTarget.fX + ((tTarget.fW - (tViewBox.fW * fScale)) * 0.5f) - (tViewBox.fX * fScale);
	tMatrix.fF = tTarget.fY + ((tTarget.fH - (tViewBox.fH * fScale)) * 0.5f) - (tViewBox.fY * fScale);
	pShape = NULL;
	iRet = xgeShapeExCreate(&pShape);
	if ( iRet == XGE_OK ) iRet = xgeShapeExAppendSvgPath(pShape, sPath);
	if ( iRet == XGE_OK ) iRet = __xuiProxyXgePathStyle(pShape, pStyle);
	if ( iRet == XGE_OK ) iRet = xgeShapeExTransformSet(pShape, &tMatrix);
	if ( iRet == XGE_OK ) iRet = xgeShapeExDrawPx(pShape, fTolerance / fScale);
	xgeShapeExDestroy(pShape);
	if ( iRet == XGE_OK ) __xuiProxyXgeDrawMarkDirty(pDraw);
	return iRet;
}

static int __xuiProxyXgeDrawPoint(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fSize, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fSize <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapePointPx(fX, fY, fSize, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeLinePx(fX0, fY0, fX1, fY1, fWidth, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawTriangleFill(xui_proxy pProxy, xui_draw_context pDraw, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeTriangleFillPx(__xuiProxyXgeVec2(tA), __xuiProxyXgeVec2(tB), __xuiProxyXgeVec2(tC), iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawTriangleStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeTriangleStrokePx(__xuiProxyXgeVec2(tA), __xuiProxyXgeVec2(tB), __xuiProxyXgeVec2(tC), fWidth, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeRectFillPixels(__xuiProxyXgePixelRect(tRect), iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	{
		int iWidth = xuiInternalSnapSize(fWidth);
		xgeShapeRectBorderPixels(__xuiProxyXgePixelRect(tRect),
			(xge_edges_i_t){iWidth, iWidth, iWidth, iWidth}, iColor);
	}
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawCircleFill(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fRadius <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeCircleFillPx(fX, fY, fRadius, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawCircleStroke(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fRadius <= 0.0f ) return XGE_OK;
	if ( fWidth <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeCircleStrokePx(fX, fY, fRadius, fWidth, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeTextDrawRect(&pFont->tFont, sText, __xuiProxyXgeRect(tRect), iColor, __xuiProxyXgeTextFlags(iFlags));
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawClipGet(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t* pRect, int* pHasClip)
{
	xge_rect_i_t tClip;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (pRect == NULL) || (pHasClip == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	tClip = xgeClipGetPixels();
	*pHasClip = ((tClip.iW > 0) && (tClip.iH > 0)) ? 1 : 0;
	pRect->fX = tClip.iX;
	pRect->fY = tClip.iY;
	pRect->fW = tClip.iW;
	pRect->fH = tClip.iH;
	return XGE_OK;
}

static int __xuiProxyXgeDrawClipSet(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	(void)xgeFlush();
	xgeClipSetPixels(__xuiProxyXgePixelRect(tRect));
	return XGE_OK;
}

static int __xuiProxyXgeDrawClipClear(xui_proxy pProxy, xui_draw_context pDraw)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	(void)xgeFlush();
	xgeClipClear();
	return XGE_OK;
}

static void __xuiProxyXgeSurfaceDestroy(xui_proxy pProxy, xui_surface pSurface)
{
	if ( pProxy == NULL ) {
		return;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeSurfaceValid(pSurface) ) {
		return;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		xgeRenderTargetFree(&pSurface->tTarget);
	} else {
		xgeTextureFree(&pSurface->tTexture);
	}
	pSurface->iMagic = 0;
	xrtFree(pSurface);
}

static uint32_t __xuiProxyXgeInputModifiers(void)
{
	uint32_t iModifiers = 0u;
	if ( xgeKeyDown(XUI_PROXY_XGE_KEY_LEFT_SHIFT) || xgeKeyDown(XUI_PROXY_XGE_KEY_RIGHT_SHIFT) ) iModifiers |= XUI_MOD_SHIFT;
	if ( xgeKeyDown(XUI_PROXY_XGE_KEY_LEFT_CONTROL) || xgeKeyDown(XUI_PROXY_XGE_KEY_RIGHT_CONTROL) ) iModifiers |= XUI_MOD_CTRL;
	if ( xgeKeyDown(XUI_PROXY_XGE_KEY_LEFT_ALT) || xgeKeyDown(XUI_PROXY_XGE_KEY_RIGHT_ALT) ) iModifiers |= XUI_MOD_ALT;
	if ( xgeKeyDown(XUI_PROXY_XGE_KEY_LEFT_SUPER) || xgeKeyDown(XUI_PROXY_XGE_KEY_RIGHT_SUPER) ) iModifiers |= XUI_MOD_SUPER;
	return iModifiers;
}

static uint32_t __xuiProxyXgeMapModifiers(uint32_t iModifiers)
{
	uint32_t iResult;

	iResult = 0u;
	if ( (iModifiers & XGE_KEY_MOD_SHIFT) != 0u ) iResult |= XUI_MOD_SHIFT;
	if ( (iModifiers & XGE_KEY_MOD_CTRL) != 0u ) iResult |= XUI_MOD_CTRL;
	if ( (iModifiers & XGE_KEY_MOD_ALT) != 0u ) iResult |= XUI_MOD_ALT;
	if ( (iModifiers & XGE_KEY_MOD_SUPER) != 0u ) iResult |= XUI_MOD_SUPER;
	return iResult;
}

static int __xuiProxyXgeMapKey(int iKey)
{
	if ( iKey >= XGE_KEY_F1 && iKey <= XGE_KEY_F25 ) return XUI_KEY_F1 + (iKey - XGE_KEY_F1);
	switch ( iKey ) {
	case XGE_KEY_ESCAPE: return XUI_KEY_ESCAPE;
	case XGE_KEY_ENTER: return XUI_KEY_ENTER;
	case XGE_KEY_TAB: return XUI_KEY_TAB;
	case XGE_KEY_BACKSPACE: return XUI_KEY_BACKSPACE;
	case XGE_KEY_INSERT: return XUI_KEY_INSERT;
	case XGE_KEY_DELETE: return XUI_KEY_DELETE;
	case XGE_KEY_RIGHT: return XUI_KEY_RIGHT;
	case XGE_KEY_LEFT: return XUI_KEY_LEFT;
	case XGE_KEY_DOWN: return XUI_KEY_DOWN;
	case XGE_KEY_UP: return XUI_KEY_UP;
	case XGE_KEY_PAGE_UP: return XUI_KEY_PAGE_UP;
	case XGE_KEY_PAGE_DOWN: return XUI_KEY_PAGE_DOWN;
	case XGE_KEY_HOME: return XUI_KEY_HOME;
	case XGE_KEY_END: return XUI_KEY_END;
	case XGE_KEY_MENU: return XUI_KEY_CONTEXT_MENU;
	default: break;
	}
	return (iKey >= 32 && iKey <= 126) ? iKey : 0;
}

typedef struct xui_proxy_xge_ime_ui_t {
	xui_context pContext;
	xui_widget pPopup;
	xui_widget pList;
	int iPageStart;
	int bSyncing;
	int bCanSelect;
} xui_proxy_xge_ime_ui_t;

#define XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES (64 * 1024)

static xui_context g_xuiProxyXgeImeTextContext;
static xui_proxy_xge_ime_ui_t g_xuiProxyXgeImeUi;
static char* g_xuiProxyXgeImeSnapshotText;
static int g_xuiProxyXgeImeSnapshotCapacity;

static int __xuiProxyXgeImeSnapshotReserve(int iCapacity)
{
	char* sNew;
	int iNewCapacity;

	if ( iCapacity <= g_xuiProxyXgeImeSnapshotCapacity ) return XUI_OK;
	iNewCapacity = (g_xuiProxyXgeImeSnapshotCapacity > 0) ?
		g_xuiProxyXgeImeSnapshotCapacity : 4096;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	sNew = (char*)xrtRealloc(g_xuiProxyXgeImeSnapshotText, (size_t)iNewCapacity);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	g_xuiProxyXgeImeSnapshotText = sNew;
	g_xuiProxyXgeImeSnapshotCapacity = iNewCapacity;
	return XUI_OK;
}

static void __xuiProxyXgeImeTextWindow(int iLength, int iSelectionStart,
	int iSelectionEnd, int* pStart, int* pEnd, int* pLocalStart, int* pLocalEnd)
{
	int iSelectionLength;
	int iWindowStart;
	int iWindowEnd;
	int iBefore;

	if ( iSelectionStart < 0 ) iSelectionStart = 0;
	if ( iSelectionEnd < iSelectionStart ) iSelectionEnd = iSelectionStart;
	if ( iSelectionStart > iLength ) iSelectionStart = iLength;
	if ( iSelectionEnd > iLength ) iSelectionEnd = iLength;
	iSelectionLength = iSelectionEnd - iSelectionStart;
	if ( iLength <= XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES ) {
		iWindowStart = 0;
		iWindowEnd = iLength;
	} else if ( iSelectionLength > XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES ) {
		iWindowStart = iSelectionEnd - XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES / 2;
		if ( iWindowStart < 0 ) iWindowStart = 0;
		iWindowEnd = iWindowStart + XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES;
		if ( iWindowEnd > iLength ) {
			iWindowEnd = iLength;
			iWindowStart = iWindowEnd - XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES;
		}
		iSelectionStart = iSelectionEnd;
	} else {
		iBefore = (XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES - iSelectionLength) / 2;
		iWindowStart = iSelectionStart - iBefore;
		if ( iWindowStart < 0 ) iWindowStart = 0;
		iWindowEnd = iWindowStart + XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES;
		if ( iWindowEnd > iLength ) {
			iWindowEnd = iLength;
			iWindowStart = iWindowEnd - XUI_PROXY_XGE_IME_TEXT_WINDOW_BYTES;
		}
	}
	*pStart = iWindowStart;
	*pEnd = iWindowEnd;
	*pLocalStart = iSelectionStart - iWindowStart;
	*pLocalEnd = iSelectionEnd - iWindowStart;
	if ( *pLocalStart < 0 || *pLocalStart > iWindowEnd - iWindowStart ) {
		*pLocalStart = iSelectionEnd - iWindowStart;
	}
	if ( *pLocalEnd < *pLocalStart || *pLocalEnd > iWindowEnd - iWindowStart ) {
		*pLocalEnd = *pLocalStart;
	}
}

static int __xuiProxyXgeCodeDocumentBoundary(xui_code_document pDocument,
	int iOffset, int iLength, int bForward)
{
	char cByte;

	if ( iOffset <= 0 ) return 0;
	if ( iOffset >= iLength ) return iLength;
	while ( iOffset > 0 && iOffset < iLength &&
	        xuiCodeDocumentGetByte(pDocument, iOffset, &cByte) == XUI_OK &&
	        (((unsigned char)cByte & 0xC0u) == 0x80u) ) {
		iOffset += bForward ? 1 : -1;
	}
	return iOffset;
}

static int __xuiProxyXgeTextBoundary(const char* sText, int iOffset, int iLength,
	int bForward)
{
	if ( sText == NULL || iOffset <= 0 ) return 0;
	if ( iOffset >= iLength ) return iLength;
	while ( iOffset > 0 && iOffset < iLength &&
	        (((unsigned char)sText[iOffset] & 0xC0u) == 0x80u) ) {
		iOffset += bForward ? 1 : -1;
	}
	return iOffset;
}

static void __xuiProxyXgeImeCandidateSelected(xui_widget pWidget, int iIndex, void* pUser)
{
	xui_proxy_xge_ime_ui_t* pUi;
	int iCandidate;

	(void)pWidget;
	pUi = (xui_proxy_xge_ime_ui_t*)pUser;
	if ( pUi == NULL || pUi->bSyncing || !pUi->bCanSelect || iIndex < 0 ) return;
	iCandidate = pUi->iPageStart + iIndex;
	if ( xgeImeCandidateSelect(iCandidate) == XGE_OK ) {
		(void)xgeImeCandidateFinalize();
	}
}

static void __xuiProxyXgeImeCandidateClose(xui_context pContext)
{
	if ( g_xuiProxyXgeImeUi.pContext != pContext ) return;
	if ( g_xuiProxyXgeImeUi.pPopup != NULL ) {
		(void)xuiPopupSetOpen(g_xuiProxyXgeImeUi.pPopup, 0);
	}
}

static int __xuiProxyXgeImeCandidateEnsure(xui_context pContext)
{
	xui_popup_desc_t tPopup;
	xui_list_view_desc_t tList;
	xui_widget pContent;
	xui_widget pFocus;
	int iRet;

	if ( pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pFocus = xuiGetFocusWidget(pContext);
	if ( pFocus == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	if ( g_xuiProxyXgeImeUi.pContext == pContext &&
	     g_xuiProxyXgeImeUi.pPopup != NULL && g_xuiProxyXgeImeUi.pList != NULL ) return XUI_OK;
	if ( g_xuiProxyXgeImeUi.pContext != NULL &&
	     g_xuiProxyXgeImeUi.pContext != pContext &&
	     g_xuiProxyXgeImeUi.pPopup != NULL ) {
		xuiWidgetDestroy(g_xuiProxyXgeImeUi.pPopup);
	}
	memset(&g_xuiProxyXgeImeUi, 0, sizeof(g_xuiProxyXgeImeUi));
	g_xuiProxyXgeImeUi.pContext = pContext;
	memset(&tPopup, 0, sizeof(tPopup));
	tPopup.iSize = sizeof(tPopup);
	tPopup.pOwner = pFocus;
	tPopup.fContentWidth = 280.0f;
	tPopup.fContentHeight = 28.0f;
	tPopup.fMaxHeight = 280.0f;
	tPopup.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tPopup.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopup.iOutsidePolicy = XUI_POPUP_OUTSIDE_IGNORE;
	tPopup.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tPopup.iEscapePolicy = XUI_POPUP_ESCAPE_IGNORE;
	tPopup.iFocusPolicy = XUI_POPUP_FOCUS_NONE;
	tPopup.bConsumeInside = 1;
	tPopup.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopup.fPadding = 0.0f;
	tPopup.fBorderWidth = 1.0f;
	tPopup.fShadowSize = 5.0f;
	tPopup.iPanelColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	tPopup.iBorderColor = XUI_COLOR_RGBA(148, 163, 184, 255);
	tPopup.iShadowColor = XUI_COLOR_RGBA(15, 23, 42, 70);
	iRet = xuiPopupCreate(pContext, &g_xuiProxyXgeImeUi.pPopup, &tPopup);
	if ( iRet != XUI_OK ) goto fail;
	pContent = xuiPopupGetContentWidget(g_xuiProxyXgeImeUi.pPopup);
	if ( pContent == NULL ) {
		iRet = XUI_ERROR_NOT_INITIALIZED;
		goto fail;
	}
	memset(&tList, 0, sizeof(tList));
	tList.iSize = sizeof(tList);
	tList.pFont = xuiGetDefaultFont(pContext);
	tList.fItemHeight = 28.0f;
	tList.fPadding = 8.0f;
	tList.iSelectionMode = XUI_SELECTION_SINGLE;
	tList.iSelected = -1;
	tList.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tList.iBackgroundColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	tList.iBorderColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tList.iHoverColor = XUI_COLOR_RGBA(226, 232, 240, 255);
	tList.iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	tList.iTextColor = XUI_COLOR_RGBA(30, 41, 59, 255);
	iRet = xuiListViewCreate(pContext, &g_xuiProxyXgeImeUi.pList, &tList);
	if ( iRet != XUI_OK ) goto fail;
	iRet = xuiWidgetAddChild(pContent, g_xuiProxyXgeImeUi.pList);
	if ( iRet != XUI_OK ) goto fail;
	(void)xuiWidgetSetFocusable(g_xuiProxyXgeImeUi.pList, 0);
	(void)xuiWidgetSetTabStop(g_xuiProxyXgeImeUi.pList, 0);
	iRet = xuiListViewSetSelect(g_xuiProxyXgeImeUi.pList,
		__xuiProxyXgeImeCandidateSelected, &g_xuiProxyXgeImeUi);
	if ( iRet != XUI_OK ) goto fail;
	return XUI_OK;

fail:
	if ( g_xuiProxyXgeImeUi.pPopup != NULL ) {
		xuiWidgetDestroy(g_xuiProxyXgeImeUi.pPopup);
	}
	memset(&g_xuiProxyXgeImeUi, 0, sizeof(g_xuiProxyXgeImeUi));
	return iRet;
}

static int __xuiProxyXgeImeCandidateSync(xui_context pContext)
{
	xge_ime_candidate_info_t tInfo;
	xge_rect_t tCandidateRect;
	xui_proxy_t tProxy;
	xui_proxy pProxy;
	xui_font pFont;
	xui_widget pFocus;
	xui_rect_t tAnchor;
	xui_vec2_t tMeasure;
	char** arrText;
	const char** arrItems;
	char* sCandidate;
	int iCandidateLength;
	int iFirst;
	int iCount;
	int iRows;
	int i;
	int iRet;
	float fWidth;

	if ( xgeImeGetMode() != XGE_IME_MODE_FULL ) {
		__xuiProxyXgeImeCandidateClose(pContext);
		return XUI_OK;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	iRet = xgeImeCandidateGetInfo(&tInfo);
	if ( iRet != XGE_OK || !tInfo.bVisible || tInfo.iCount <= 0 ) {
		__xuiProxyXgeImeCandidateClose(pContext);
		return (iRet == XGE_OK || iRet == XGE_ERROR_UNSUPPORTED) ? XUI_OK : iRet;
	}
	iRet = __xuiProxyXgeImeCandidateEnsure(pContext);
	if ( iRet != XUI_OK ) return iRet;
	pFocus = xuiGetFocusWidget(pContext);
	if ( pFocus == NULL ) {
		__xuiProxyXgeImeCandidateClose(pContext);
		return XUI_OK;
	}
	iFirst = tInfo.iPageStart;
	iCount = tInfo.iPageSize;
	if ( iFirst < 0 || iFirst >= tInfo.iCount ) iFirst = 0;
	if ( iCount <= 0 || iFirst + iCount > tInfo.iCount ) iCount = tInfo.iCount - iFirst;
	if ( iCount > 64 ) iCount = 64;
	arrText = (char**)xrtMalloc(sizeof(*arrText) * (size_t)iCount);
	arrItems = (const char**)xrtMalloc(sizeof(*arrItems) * (size_t)iCount);
	if ( arrText == NULL || arrItems == NULL ) {
		if ( arrText != NULL ) xrtFree(arrText);
		if ( arrItems != NULL ) xrtFree(arrItems);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(arrText, 0, sizeof(*arrText) * (size_t)iCount);
	memset(&tProxy, 0, sizeof(tProxy));
	pProxy = (xuiGetProxy(pContext, &tProxy) == XUI_OK) ? &tProxy : NULL;
	pFont = xuiGetDefaultFont(pContext);
	fWidth = 180.0f;
	for ( i = 0; i < iCount; i++ ) {
		iCandidateLength = xgeImeCandidateGetText(iFirst + i, NULL, 0);
		if ( iCandidateLength < 0 ) iCandidateLength = 0;
		arrText[i] = (char*)xrtMalloc((size_t)iCandidateLength + 24u);
		if ( arrText[i] == NULL ) {
			iRet = XUI_ERROR_OUT_OF_MEMORY;
			goto cleanup;
		}
		sCandidate = arrText[i] + 16;
		(void)xgeImeCandidateGetText(iFirst + i, sCandidate, iCandidateLength + 1);
		snprintf(arrText[i], 16u, "%d  ", i + 1);
		memmove(arrText[i] + strlen(arrText[i]), sCandidate, (size_t)iCandidateLength + 1u);
		arrItems[i] = arrText[i];
		memset(&tMeasure, 0, sizeof(tMeasure));
		if ( pProxy != NULL && pProxy->textMeasure != NULL && pFont != NULL &&
		     pProxy->textMeasure(pProxy, pFont, arrItems[i], &tMeasure) == XUI_OK &&
		     tMeasure.fX + 28.0f > fWidth ) fWidth = tMeasure.fX + 28.0f;
	}
	if ( fWidth > 480.0f ) fWidth = 480.0f;
	iRows = (iCount < 10) ? iCount : 10;
	g_xuiProxyXgeImeUi.bSyncing = 1;
	g_xuiProxyXgeImeUi.iPageStart = iFirst;
	g_xuiProxyXgeImeUi.bCanSelect = tInfo.bCanSelect;
	iRet = xuiListViewSetItems(g_xuiProxyXgeImeUi.pList, arrItems, iCount);
	if ( iRet == XUI_OK ) {
		(void)xuiWidgetSetEnabled(g_xuiProxyXgeImeUi.pList, tInfo.bCanSelect);
		(void)xuiListViewSetSelected(g_xuiProxyXgeImeUi.pList,
			(tInfo.iSelection >= iFirst && tInfo.iSelection < iFirst + iCount) ?
			tInfo.iSelection - iFirst : -1);
		(void)xuiWidgetSetRect(g_xuiProxyXgeImeUi.pList,
			(xui_rect_t){0.0f, 0.0f, fWidth, (float)iRows * 28.0f});
		(void)xuiPopupSetOwner(g_xuiProxyXgeImeUi.pPopup, pFocus);
		(void)xuiPopupSetContentSize(g_xuiProxyXgeImeUi.pPopup, fWidth, (float)iRows * 28.0f);
		memset(&tCandidateRect, 0, sizeof(tCandidateRect));
		if ( g_xuiProxyXgeInputTransform.pContext == pContext &&
		     g_xuiProxyXgeInputTransform.bHasCandidateLocalRect ) {
			tAnchor = g_xuiProxyXgeInputTransform.tCandidateLocalRect;
		} else if ( xgeImeGetCandidateRect(&tCandidateRect) == XGE_OK ) {
			tAnchor = (xui_rect_t){tCandidateRect.fX, tCandidateRect.fY,
				tCandidateRect.fW, tCandidateRect.fH};
		} else {
			tAnchor = xuiWidgetGetWorldRect(pFocus);
		}
		(void)xuiPopupSetAnchorRect(g_xuiProxyXgeImeUi.pPopup, tAnchor);
		(void)xuiPopupSetOpen(g_xuiProxyXgeImeUi.pPopup, 1);
	}
	g_xuiProxyXgeImeUi.bSyncing = 0;

cleanup:
	for ( i = 0; i < iCount; i++ ) {
		if ( arrText[i] != NULL ) xrtFree(arrText[i]);
	}
	xrtFree(arrItems);
	xrtFree(arrText);
	return iRet;
}

static void __xuiProxyXgeDetachIme(xui_context pContext)
{
	if ( g_xuiProxyXgeImeTextContext == pContext ) {
		(void)xgeImeSetCandidatePresenterReady(0);
		(void)xgeImeSetTextClient(NULL);
		g_xuiProxyXgeImeTextContext = NULL;
	}
	if ( g_xuiProxyXgeImeUi.pContext == pContext ) {
		memset(&g_xuiProxyXgeImeUi, 0, sizeof(g_xuiProxyXgeImeUi));
	}
	if ( g_xuiProxyXgeInputTransform.pContext == pContext ) {
		memset(&g_xuiProxyXgeInputTransform, 0, sizeof(g_xuiProxyXgeInputTransform));
	}
	if ( g_xuiProxyXgeImeSnapshotText != NULL ) {
		xrtFree(g_xuiProxyXgeImeSnapshotText);
		g_xuiProxyXgeImeSnapshotText = NULL;
		g_xuiProxyXgeImeSnapshotCapacity = 0;
	}
}

static int __xuiProxyXgeImeTextSnapshot(void* pUser, xge_ime_text_snapshot_t* pSnapshot)
{
	xui_context pContext;
	xui_widget pFocus;
	xui_code_document pDocument;
	xui_code_selection_model pSelection;
	const char* sText;
	int iDocumentLength;
	int iWindowStart;
	int iWindowEnd;
	int iLocalStart;
	int iLocalEnd;
	int iCopyLength;
	int iRet;
	int iStart;
	int iEnd;

	(void)pUser;
	pContext = g_xuiProxyXgeImeTextContext;
	if ( pContext == NULL || pSnapshot == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	pDocument = NULL;
	pSelection = NULL;
	pFocus = xuiGetFocusWidget(pContext);
	if ( pFocus == NULL || xuiWidgetGetImeMode(pFocus) == XUI_IME_DISABLED ) return XGE_ERROR_NOT_INITIALIZED;
	sText = "";
	iDocumentLength = 0;
	iWindowStart = 0;
	iWindowEnd = 0;
	iLocalStart = 0;
	iLocalEnd = 0;
	iStart = 0;
	iEnd = 0;
	if ( xuiWidgetIsType(pFocus, xuiInputGetType(pContext)) ) {
		sText = xuiInputGetText(pFocus);
		if ( xuiInputGetSelection(pFocus, &iStart, &iEnd) != XUI_OK ) return XGE_ERROR;
		iDocumentLength = (sText != NULL) ? (int)strlen(sText) : 0;
	} else if ( xuiWidgetIsType(pFocus, xuiTextEditGetType(pContext)) ) {
		sText = xuiTextEditGetText(pFocus);
		if ( xuiTextEditGetSelection(pFocus, &iStart, &iEnd) != XUI_OK ) return XGE_ERROR;
		iDocumentLength = (sText != NULL) ? (int)strlen(sText) : 0;
	} else if ( xuiWidgetIsType(pFocus, xuiCodeEditGetType(pContext)) ) {
		pDocument = xuiCodeEditGetDocument(pFocus);
		pSelection = xuiCodeEditGetSelection(pFocus);
		if ( pDocument == NULL || pSelection == NULL ) return XGE_ERROR_NOT_INITIALIZED;
		if ( xuiCodeSelectionGetRange(pSelection, &iStart, &iEnd) != XUI_OK ) return XGE_ERROR;
		iDocumentLength = xuiCodeDocumentGetLength(pDocument);
		__xuiProxyXgeImeTextWindow(iDocumentLength, iStart, iEnd,
			&iWindowStart, &iWindowEnd, &iLocalStart, &iLocalEnd);
		iWindowStart = __xuiProxyXgeCodeDocumentBoundary(pDocument,
			iWindowStart, iDocumentLength, 1);
		iWindowEnd = __xuiProxyXgeCodeDocumentBoundary(pDocument,
			iWindowEnd, iDocumentLength, 0);
		iLocalStart = iStart - iWindowStart;
		iLocalEnd = iEnd - iWindowStart;
		if ( iLocalStart < 0 || iLocalEnd > iWindowEnd - iWindowStart ) {
			iLocalStart = iEnd - iWindowStart;
			iLocalEnd = iLocalStart;
		}
		iCopyLength = iWindowEnd - iWindowStart;
		iRet = __xuiProxyXgeImeSnapshotReserve(iCopyLength + 1);
		if ( iRet != XUI_OK ) return XGE_ERROR_OUT_OF_MEMORY;
		iRet = xuiCodeDocumentCopyRange(pDocument, iWindowStart, iWindowEnd,
			g_xuiProxyXgeImeSnapshotText, g_xuiProxyXgeImeSnapshotCapacity, NULL);
		if ( iRet != XUI_OK ) return XGE_ERROR;
		sText = g_xuiProxyXgeImeSnapshotText;
	} else {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( pDocument == NULL ) {
		__xuiProxyXgeImeTextWindow(iDocumentLength, iStart, iEnd,
			&iWindowStart, &iWindowEnd, &iLocalStart, &iLocalEnd);
		if ( iWindowStart != 0 || iWindowEnd != iDocumentLength ) {
			iWindowStart = __xuiProxyXgeTextBoundary(sText, iWindowStart, iDocumentLength, 0);
			iWindowEnd = __xuiProxyXgeTextBoundary(sText, iWindowEnd, iDocumentLength, 1);
			iLocalStart = iStart - iWindowStart;
			iLocalEnd = iEnd - iWindowStart;
			if ( iLocalStart < 0 || iLocalEnd > iWindowEnd - iWindowStart ) {
				iLocalStart = iEnd - iWindowStart;
				iLocalEnd = iLocalStart;
			}
			iCopyLength = iWindowEnd - iWindowStart;
			iRet = __xuiProxyXgeImeSnapshotReserve(iCopyLength + 1);
			if ( iRet != XUI_OK ) return XGE_ERROR_OUT_OF_MEMORY;
			if ( iCopyLength > 0 ) memcpy(g_xuiProxyXgeImeSnapshotText,
				sText + iWindowStart, (size_t)iCopyLength);
			g_xuiProxyXgeImeSnapshotText[iCopyLength] = '\0';
			sText = g_xuiProxyXgeImeSnapshotText;
		}
	}
	memset(pSnapshot, 0, sizeof(*pSnapshot));
	pSnapshot->iSize = sizeof(*pSnapshot);
	pSnapshot->sText = (sText != NULL) ? sText : "";
	pSnapshot->iTextSize = (int)strlen(pSnapshot->sText);
	pSnapshot->iSelectionStart = iLocalStart;
	pSnapshot->iSelectionEnd = iLocalEnd;
	pSnapshot->iDocumentOffset = iWindowStart;
	pSnapshot->iDocumentSize = iDocumentLength;
	pSnapshot->iRevision = (pDocument != NULL) ? xuiCodeDocumentGetVersion(pDocument) : 0u;
	return XGE_OK;
}

static int __xuiProxyXgeTextShape(xui_proxy pProxy, xui_font pFont, const char* sText,
	int iTextSize, uint32_t iFlags, xui_text_shape_t* pShape)
{
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	xui_text_cluster_t* pCluster;
	uint32_t iXgeFlags = 0;
	int i;
	int iCount;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) ||
	     (iTextSize < -1) || (pShape == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	(void)pProxy;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	memset(pShape, 0, sizeof(*pShape));
	pShape->iSize = sizeof(*pShape);
	pShape->iFlags = iFlags;
	pShape->iTextSize = iTextSize;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = &pFont->tFont;
	tDesc.sText = sText;
	tDesc.iTextSize = iTextSize;
	if ( (iFlags & XUI_TEXT_SHAPE_KERNING) != 0 ) iXgeFlags |= XGE_TEXT_SHAPE_KERNING;
	if ( (iFlags & XUI_TEXT_SHAPE_EMOJI) != 0 ) iXgeFlags |= XGE_TEXT_SHAPE_EMOJI;
	tDesc.iFlags = iXgeFlags;
	tDesc.iEmojiPresentation = XGE_EMOJI_PRESENTATION_AUTO;
	tDesc.iEmojiLinePolicy = XGE_EMOJI_LINE_STABLE;
	tDesc.fEmojiScale = 1.0f;
	memset(&tRun, 0, sizeof(tRun));
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet != XGE_OK ) return iRet;
	pShape->fWidth = tRun.fWidth;
	pShape->fHeight = tRun.fHeight;
	pShape->fAscent = tRun.fAscent;
	pShape->fDescent = tRun.fDescent;
	pShape->fLineHeight = tRun.fLineHeight;
	iCount = 0;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		if ( i == 0 || tRun.pGlyphs[i].iCluster != tRun.pGlyphs[i - 1].iCluster ) iCount++;
	}
	if ( iCount > 0 ) {
		pShape->pClusters = (xui_text_cluster_t*)xrtCalloc((size_t)iCount, sizeof(*pShape->pClusters));
		if ( pShape->pClusters == NULL ) {
			xgeGlyphRunFree(&tRun);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	pShape->iClusterCount = iCount;
	iCount = -1;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		xge_glyph_position_t* pGlyph = &tRun.pGlyphs[i];
		if ( i == 0 || pGlyph->iCluster != tRun.pGlyphs[i - 1].iCluster ) {
			iCount++;
			pCluster = &pShape->pClusters[iCount];
			pCluster->iSize = sizeof(*pCluster);
			pCluster->iTextStart = (int)pGlyph->iCluster;
			pCluster->iTextEnd = (int)pGlyph->iClusterEnd;
			pCluster->fOffsetX = pGlyph->fOffsetX;
			pCluster->fOffsetY = pGlyph->fOffsetY;
		} else {
			pCluster = &pShape->pClusters[iCount];
			if ((int)pGlyph->iClusterEnd > pCluster->iTextEnd) pCluster->iTextEnd = (int)pGlyph->iClusterEnd;
		}
		pCluster->fAdvance += pGlyph->fAdvanceX;
		if ( (pGlyph->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) pCluster->iFlags |= XUI_TEXT_CLUSTER_LINE_BREAK;
		if ( pGlyph->iItemKind == XGE_TEXT_ITEM_EMOJI ) pCluster->iFlags |= XUI_TEXT_CLUSTER_EMOJI;
	}
	xgeGlyphRunFree(&tRun);
	return XUI_OK;
}

static int __xuiProxyXgeBindImeTextClient(xui_context pContext)
{
	xge_ime_text_client_t tClient;
	int iRet;

	if ( pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( g_xuiProxyXgeImeTextContext != pContext ) {
		if ( g_xuiProxyXgeImeTextContext != NULL ) {
			(void)xgeImeSetCandidatePresenterReady(0);
			(void)xuiInternalContextSetImeDetach(g_xuiProxyXgeImeTextContext, NULL);
		}
		g_xuiProxyXgeImeTextContext = pContext;
		(void)xuiInternalContextSetImeDetach(pContext, __xuiProxyXgeDetachIme);
		memset(&tClient, 0, sizeof(tClient));
		tClient.iSize = sizeof(tClient);
		tClient.onSnapshot = __xuiProxyXgeImeTextSnapshot;
		iRet = xgeImeSetTextClient(&tClient);
		if ( iRet != XGE_OK && iRet != XGE_ERROR_UNSUPPORTED && iRet != XGE_ERROR_NOT_INITIALIZED ) return iRet;
	}
	if ( xgeImeGetMode() == XGE_IME_MODE_FULL ) {
		iRet = __xuiProxyXgeImeCandidateEnsure(pContext);
		(void)xgeImeSetCandidatePresenterReady(iRet == XUI_OK);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_NOT_INITIALIZED ) return iRet;
	} else {
		(void)xgeImeSetCandidatePresenterReady(0);
	}
	return XUI_OK;
}

static int __xuiProxyXgePumpQueuedInput(xui_context pContext,
	xui_rect_t tWindowRect, xui_size_t tViewport)
{
	xge_input_event_t tInput;
	xui_ime_composition_t tComposition;
	uint32_t iModifiers;
	uint32_t iResult;
	int iX;
	int iY;
	int iMapped;
	int iRet;

	if ( pContext == NULL || tWindowRect.fW <= 0.0f || tWindowRect.fH <= 0.0f ||
	     tViewport.iW <= 0 || tViewport.iH <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiProxyXgeBindImeTextClient(pContext);
	if ( iRet != XUI_OK ) return iRet;
	iModifiers = __xuiProxyXgeInputModifiers();
	iRet = xuiInputSetModifiers(pContext, iModifiers);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tInput, 0, sizeof(tInput));
	while ( (iRet = xgeInputEventGet(&tInput)) > 0 ) {
		iModifiers = __xuiProxyXgeMapModifiers(tInput.iModifiers);
		iRet = xuiInputSetModifiers(pContext, iModifiers);
		if ( iRet != XUI_OK ) return iRet;
		iX = xuiInternalPixelFloor((tInput.fX - (float)tWindowRect.fX) *
			(float)tViewport.iW / (float)tWindowRect.fW);
		iY = xuiInternalPixelFloor((tInput.fY - (float)tWindowRect.fY) *
			(float)tViewport.iH / (float)tWindowRect.fH);
		switch ( tInput.iType ) {
		case XGE_EVENT_MOUSE_MOVE:
			iRet = xuiInputPointerMove(pContext, iX, iY, tInput.iButtons);
			break;
		case XGE_EVENT_MOUSE_DOWN:
			iRet = xuiInputPointerDown(pContext, iX, iY, tInput.iButton, tInput.iButtons);
			break;
		case XGE_EVENT_MOUSE_UP:
			iRet = xuiInputPointerUp(pContext, iX, iY, tInput.iButton, tInput.iButtons);
			break;
		case XGE_EVENT_MOUSE_WHEEL:
			iRet = xuiInputPointerWheel(pContext, iX, iY, tInput.fDX, tInput.fDY, tInput.iButtons);
			break;
		case XGE_EVENT_TOUCH_BEGIN:
			iRet = xuiInputPointerDownEx(pContext, tInput.iPointerId, XUI_POINTER_TYPE_TOUCH,
				iX, iY, XUI_POINTER_BUTTON_LEFT, XUI_POINTER_BUTTON_LEFT);
			break;
		case XGE_EVENT_TOUCH_MOVE:
			iRet = xuiInputPointerMoveEx(pContext, tInput.iPointerId, XUI_POINTER_TYPE_TOUCH,
				iX, iY, XUI_POINTER_BUTTON_LEFT);
			break;
		case XGE_EVENT_TOUCH_END:
			iRet = xuiInputPointerUpEx(pContext, tInput.iPointerId, XUI_POINTER_TYPE_TOUCH,
				iX, iY, XUI_POINTER_BUTTON_LEFT, 0u);
			break;
		case XGE_EVENT_TOUCH_CANCEL:
			iRet = xuiInputPointerCancelEx(pContext, tInput.iPointerId, XUI_POINTER_TYPE_TOUCH);
			break;
		case XGE_EVENT_KEY_DOWN:
			iMapped = __xuiProxyXgeMapKey(tInput.iKey);
			if ( iMapped == 0 ) break;
			iResult = 0u;
			iRet = xuiInputKeyDownEx(pContext, iMapped, iModifiers, &iResult);
			if ( iRet != XUI_OK ) return iRet;
			if ( (iResult & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(tInput.iKey);
			break;
		case XGE_EVENT_KEY_UP:
			iMapped = __xuiProxyXgeMapKey(tInput.iKey);
			if ( iMapped == 0 ) break;
			iResult = 0u;
			iRet = xuiInputKeyUpEx(pContext, iMapped, iModifiers, &iResult);
			if ( iRet != XUI_OK ) return iRet;
			if ( (iResult & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(tInput.iKey);
			break;
		case XGE_EVENT_TEXT:
			if ( (iModifiers & (XUI_MOD_CTRL | XUI_MOD_ALT)) != 0u ) break;
			iRet = xuiInputTextEx(pContext, tInput.iCodepoint, NULL);
			if ( iRet != XUI_OK ) return iRet;
			break;
		case XGE_EVENT_IME_START:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = "";
			tComposition.bActive = 1;
			tComposition.bReplacementRange = tInput.bReplacementRange;
			tComposition.iReplacementStart = tInput.iReplacementStart;
			tComposition.iReplacementEnd = tInput.iReplacementEnd;
			iRet = xuiInputImeCompositionEx(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_UPDATE:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = (tInput.sText != NULL) ? tInput.sText : "";
			tComposition.iTextSize = tInput.iTextSize;
			tComposition.bActive = 1;
			tComposition.iCursor = tInput.iCursor;
			tComposition.iSelectionStart = tInput.iSelectStart;
			tComposition.iSelectionEnd = tInput.iSelectEnd;
			tComposition.bReplacementRange = tInput.bReplacementRange;
			tComposition.iReplacementStart = tInput.iReplacementStart;
			tComposition.iReplacementEnd = tInput.iReplacementEnd;
			iRet = xuiInputImeCompositionEx(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_COMMIT:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = (tInput.sText != NULL) ? tInput.sText : "";
			tComposition.iTextSize = tInput.iTextSize;
			tComposition.iCursor = tInput.iCursor;
			tComposition.iSelectionStart = tInput.iSelectStart;
			tComposition.iSelectionEnd = tInput.iSelectEnd;
			tComposition.bReplacementRange = tInput.bReplacementRange;
			tComposition.iReplacementStart = tInput.iReplacementStart;
			tComposition.iReplacementEnd = tInput.iReplacementEnd;
			iRet = xuiInputImeCompositionEx(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_END:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = "";
			iRet = xuiInputImeCompositionEx(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_CANDIDATE_START:
		case XGE_EVENT_IME_CANDIDATE_UPDATE:
			iRet = __xuiProxyXgeImeCandidateSync(pContext);
			if ( iRet != XUI_OK ) return iRet;
			break;
		case XGE_EVENT_IME_CANDIDATE_END:
			__xuiProxyXgeImeCandidateClose(pContext);
			break;
		default:
			break;
		}
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiDispatchPendingEvents(pContext);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( iRet < 0 ) return iRet;
	iModifiers = __xuiProxyXgeInputModifiers();
	iRet = xuiInputSetModifiers(pContext, iModifiers);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xgeImeRefreshTextClient();
	if ( iRet != XGE_OK && iRet != XGE_ERROR_UNSUPPORTED && iRet != XGE_ERROR_NOT_INITIALIZED ) return iRet;
	return XUI_OK;
}

XUI_API int xuiProxyXgePumpKeyboard(xui_context pContext)
{
	return xuiProxyXgePumpInput(pContext);
}

XUI_API int xuiProxyXgePumpInputRect(xui_context pContext, xui_rect_t tWindowRect)
{
	xui_size_t tViewport;

	if ( pContext == NULL || tWindowRect.fW <= 0.0f || tWindowRect.fH <= 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	tViewport = xuiGetViewportSize(pContext);
	g_xuiProxyXgeInputTransform.pContext = pContext;
	g_xuiProxyXgeInputTransform.tWindowRect = tWindowRect;
	g_xuiProxyXgeInputTransform.tViewport = tViewport;
	g_xuiProxyXgeInputTransform.bValid = 1;
	return __xuiProxyXgePumpQueuedInput(pContext, tWindowRect, tViewport);
}

XUI_API int xuiProxyXgePumpInput(xui_context pContext)
{
	int iFramebufferWidth;
	int iFramebufferHeight;

	if ( pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iFramebufferWidth = xgeGetWidth();
	iFramebufferHeight = xgeGetHeight();
	if ( iFramebufferWidth <= 0 || iFramebufferHeight <= 0 ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiProxyXgePumpInputRect(pContext,
		(xui_rect_t){0.0f, 0.0f, (float)iFramebufferWidth, (float)iFramebufferHeight});
}

XUI_API xui_proxy_t xuiProxyXge(void)
{
	xui_proxy_t tProxy;

	memset(&tProxy, 0, sizeof(tProxy));
	tProxy.iSize = (uint32_t)sizeof(tProxy);
	tProxy.iVersion = XUI_PROXY_VERSION;
	tProxy.getCaps = __xuiProxyXgeGetCaps;
	tProxy.clipboardSetText = __xuiProxyXgeClipboardSetText;
	tProxy.clipboardGetText = __xuiProxyXgeClipboardGetText;
	tProxy.clipboardSetItems = __xuiProxyXgeClipboardSetItems;
	tProxy.clipboardGetData = __xuiProxyXgeClipboardGetData;
	tProxy.imeGetEnabled = __xuiProxyXgeImeGetEnabled;
	tProxy.imeSetEnabled = __xuiProxyXgeImeSetEnabled;
	tProxy.imeSetCandidateRect = __xuiProxyXgeImeSetCandidateRect;
	tProxy.surfaceCreate = __xuiProxyXgeSurfaceCreate;
	tProxy.surfaceCreateRGBA = __xuiProxyXgeSurfaceCreateRGBA;
	tProxy.zstdDecompress = __xuiProxyXgeZstdDecompress;
	tProxy.surfaceLoadFile = __xuiProxyXgeSurfaceLoadFile;
	tProxy.surfaceLoadMemory = __xuiProxyXgeSurfaceLoadMemory;
	tProxy.surfaceLoadSvgFile = __xuiProxyXgeSurfaceLoadSvgFile;
	tProxy.surfaceLoadSvgMemory = __xuiProxyXgeSurfaceLoadSvgMemory;
	tProxy.surfaceUpdateRGBA = __xuiProxyXgeSurfaceUpdateRGBA;
	tProxy.surfaceReadRGBA = __xuiProxyXgeSurfaceReadRGBA;
	tProxy.surfaceGetDesc = __xuiProxyXgeSurfaceGetDesc;
	tProxy.surfaceDraw = __xuiProxyXgeSurfaceDraw;
	tProxy.surfaceClear = __xuiProxyXgeSurfaceClear;
	tProxy.surfaceClearRect = __xuiProxyXgeSurfaceClearRect;
	tProxy.surfaceDrawTo = __xuiProxyXgeSurfaceDrawTo;
	tProxy.surfaceDrawQuad = __xuiProxyXgeSurfaceDrawQuad;
	tProxy.surfaceDrawQuadTo = __xuiProxyXgeSurfaceDrawQuadTo;
	tProxy.surfaceGetSampler = __xuiProxyXgeSurfaceGetSampler;
	tProxy.surfaceSetSampler = __xuiProxyXgeSurfaceSetSampler;
	tProxy.surfaceGetGeneration = __xuiProxyXgeSurfaceGetGeneration;
	tProxy.surfaceDestroy = __xuiProxyXgeSurfaceDestroy;
	tProxy.shapePoint = __xuiProxyXgeShapePoint;
	tProxy.shapeLine = __xuiProxyXgeShapeLine;
	tProxy.shapeTriangleFill = __xuiProxyXgeShapeTriangleFill;
	tProxy.shapeTriangleStroke = __xuiProxyXgeShapeTriangleStroke;
	tProxy.shapeRectFill = __xuiProxyXgeShapeRectFill;
	tProxy.shapeRectStroke = __xuiProxyXgeShapeRectStroke;
	tProxy.shapeCircleFill = __xuiProxyXgeShapeCircleFill;
	tProxy.shapeCircleStroke = __xuiProxyXgeShapeCircleStroke;
	tProxy.fontLoadFile = __xuiProxyXgeFontLoadFile;
	tProxy.fontLoadMemory = __xuiProxyXgeFontLoadMemory;
	tProxy.fontGetMetrics = __xuiProxyXgeFontGetMetrics;
	tProxy.fontDestroy = __xuiProxyXgeFontDestroy;
	tProxy.textMeasure = __xuiProxyXgeTextMeasure;
	tProxy.textDraw = __xuiProxyXgeTextDraw;
	tProxy.drawBegin = __xuiProxyXgeDrawBegin;
	tProxy.drawEnd = __xuiProxyXgeDrawEnd;
	tProxy.drawClearRect = __xuiProxyXgeDrawClearRect;
	tProxy.drawSurface = __xuiProxyXgeDrawSurface;
	tProxy.drawSurfaceQuad = __xuiProxyXgeDrawSurfaceQuad;
	tProxy.drawMeshTriangles = __xuiProxyXgeDrawMeshTriangles;
	tProxy.drawPath = __xuiProxyXgeDrawPath;
	tProxy.drawSvgPath = __xuiProxyXgeDrawSvgPath;
	tProxy.drawPoint = __xuiProxyXgeDrawPoint;
	tProxy.drawLine = __xuiProxyXgeDrawLine;
	tProxy.drawTriangleFill = __xuiProxyXgeDrawTriangleFill;
	tProxy.drawTriangleStroke = __xuiProxyXgeDrawTriangleStroke;
	tProxy.drawRectFill = __xuiProxyXgeDrawRectFill;
	tProxy.drawRectStroke = __xuiProxyXgeDrawRectStroke;
	tProxy.drawCircleFill = __xuiProxyXgeDrawCircleFill;
	tProxy.drawCircleStroke = __xuiProxyXgeDrawCircleStroke;
	tProxy.drawText = __xuiProxyXgeDrawText;
	tProxy.drawClipGet = __xuiProxyXgeDrawClipGet;
	tProxy.drawClipSet = __xuiProxyXgeDrawClipSet;
	tProxy.drawClipClear = __xuiProxyXgeDrawClipClear;
	tProxy.textShape = __xuiProxyXgeTextShape;
	tProxy.fontCreateSized = __xuiProxyXgeFontCreateSized;
	return tProxy;
}
