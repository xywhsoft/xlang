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
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)xgeFlush();
	xgeClipSet(__xuiProxyXgeRect(tRect));
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

#if defined(_WIN32) || defined(_WIN64)
typedef HIMC (WINAPI *xui_imm_get_context_proc)(HWND);
typedef BOOL (WINAPI *xui_imm_release_context_proc)(HWND, HIMC);
typedef BOOL (WINAPI *xui_imm_set_candidate_window_proc)(HIMC, LPCANDIDATEFORM);
typedef BOOL (WINAPI *xui_imm_set_composition_window_proc)(HIMC, LPCOMPOSITIONFORM);

static HMODULE g_xuiProxyXgeImm32;
static xui_imm_get_context_proc g_xuiProxyXgeImmGetContext;
static xui_imm_release_context_proc g_xuiProxyXgeImmReleaseContext;
static xui_imm_set_candidate_window_proc g_xuiProxyXgeImmSetCandidateWindow;
static xui_imm_set_composition_window_proc g_xuiProxyXgeImmSetCompositionWindow;

static int __xuiProxyXgeImeEnsureWin32(void)
{
	if ( g_xuiProxyXgeImm32 != NULL ) {
		return (g_xuiProxyXgeImmGetContext != NULL) &&
		       (g_xuiProxyXgeImmReleaseContext != NULL) &&
		       (g_xuiProxyXgeImmSetCandidateWindow != NULL) &&
		       (g_xuiProxyXgeImmSetCompositionWindow != NULL);
	}
	g_xuiProxyXgeImm32 = LoadLibraryA("imm32.dll");
	if ( g_xuiProxyXgeImm32 == NULL ) {
		return 0;
	}
	g_xuiProxyXgeImmGetContext = (xui_imm_get_context_proc)GetProcAddress(g_xuiProxyXgeImm32, "ImmGetContext");
	g_xuiProxyXgeImmReleaseContext = (xui_imm_release_context_proc)GetProcAddress(g_xuiProxyXgeImm32, "ImmReleaseContext");
	g_xuiProxyXgeImmSetCandidateWindow = (xui_imm_set_candidate_window_proc)GetProcAddress(g_xuiProxyXgeImm32, "ImmSetCandidateWindow");
	g_xuiProxyXgeImmSetCompositionWindow = (xui_imm_set_composition_window_proc)GetProcAddress(g_xuiProxyXgeImm32, "ImmSetCompositionWindow");
	return (g_xuiProxyXgeImmGetContext != NULL) &&
	       (g_xuiProxyXgeImmReleaseContext != NULL) &&
	       (g_xuiProxyXgeImmSetCandidateWindow != NULL) &&
	       (g_xuiProxyXgeImmSetCompositionWindow != NULL);
}

static HWND __xuiProxyXgeImeWindowGet(void)
{
	HWND hWnd;

	hWnd = GetFocus();
	if ( hWnd != NULL ) {
		HWND hRoot = GetAncestor(hWnd, GA_ROOT);
		if ( hRoot != NULL ) hWnd = hRoot;
	}
	if ( hWnd == NULL ) {
		hWnd = GetActiveWindow();
	}
	if ( hWnd == NULL ) {
		hWnd = GetForegroundWindow();
	}
	return hWnd;
}
#endif

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
#if defined(_WIN32) || defined(_WIN64)
	HWND hWnd;
	HIMC hImc;
	RECT tClient;
	CANDIDATEFORM tCandidate;
	COMPOSITIONFORM tComposition;
	xge_platform_runtime_t tRuntime;
	float fScaleX;
	float fScaleY;
	LONG iLeft;
	LONG iTop;
	LONG iRight;
	LONG iBottom;

	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( __xuiProxyXgeImeEnsureWin32() == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	hWnd = __xuiProxyXgeImeWindowGet();
	if ( hWnd == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	hImc = g_xuiProxyXgeImmGetContext(hWnd);
	if ( hImc == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	memset(&tRuntime, 0, sizeof(tRuntime));
	memset(&tClient, 0, sizeof(tClient));
	fScaleX = 1.0f;
	fScaleY = 1.0f;
	if ( GetClientRect(hWnd, &tClient) && xgePlatformRuntimeGet(&tRuntime) == XGE_OK ) {
		if ( tRuntime.iFramebufferWidth > 0 ) {
			fScaleX = (float)(tClient.right - tClient.left) / (float)tRuntime.iFramebufferWidth;
		}
		if ( tRuntime.iFramebufferHeight > 0 ) {
			fScaleY = (float)(tClient.bottom - tClient.top) / (float)tRuntime.iFramebufferHeight;
		}
	}
	iLeft = (LONG)floorf(tRect.fX * fScaleX + 0.5f);
	iTop = (LONG)floorf(tRect.fY * fScaleY + 0.5f);
	iRight = (LONG)floorf((tRect.fX + ((tRect.fW > 1.0f) ? tRect.fW : 1.0f)) * fScaleX + 0.5f);
	iBottom = (LONG)floorf((tRect.fY + ((tRect.fH > 1.0f) ? tRect.fH : 1.0f)) * fScaleY + 0.5f);

	memset(&tComposition, 0, sizeof(tComposition));
	tComposition.dwStyle = CFS_POINT;
	tComposition.ptCurrentPos.x = iLeft;
	tComposition.ptCurrentPos.y = iTop;
	(void)g_xuiProxyXgeImmSetCompositionWindow(hImc, &tComposition);
	memset(&tCandidate, 0, sizeof(tCandidate));
	tCandidate.dwIndex = 0;
	tCandidate.dwStyle = CFS_EXCLUDE;
	tCandidate.ptCurrentPos.x = iLeft;
	tCandidate.ptCurrentPos.y = iBottom;
	tCandidate.rcArea.left = iLeft;
	tCandidate.rcArea.top = iTop;
	tCandidate.rcArea.right = iRight;
	tCandidate.rcArea.bottom = iBottom;
	(void)g_xuiProxyXgeImmSetCandidateWindow(hImc, &tCandidate);
	(void)g_xuiProxyXgeImmReleaseContext(hWnd, hImc);
	return XGE_OK;
#else
	(void)tRect;
	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	return XGE_ERROR_UNSUPPORTED;
#endif
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
	if ( !__xuiProxyXgeSurfaceValid(pSurface) || (tDst.fW == 0.0f) || (tDst.fH == 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
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

	if ( fSize <= 0.0f ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xgeShapePointPx(fX, fY, fSize, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeLine(xui_proxy pProxy, xui_surface pTarget, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( fWidth <= 0.0f ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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

	if ( fWidth <= 0.0f ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xgeShapeTriangleStrokePx(__xuiProxyXgeVec2(tA), __xuiProxyXgeVec2(tB), __xuiProxyXgeVec2(tC), fWidth, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeRectFill(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xgeShapeRectFillPx(__xuiProxyXgeRect(tRect), iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeRectStroke(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xgeShapeRectStrokePx(__xuiProxyXgeRect(tRect), fWidth, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeCircleFill(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( fRadius <= 0.0f ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xgeShapeCircleFillPx(fX, fY, fRadius, iColor);
	return __xuiProxyXgeTargetEndDirty(&tPass, pTarget, XGE_OK);
}

static int __xuiProxyXgeShapeCircleStroke(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	xge_pass_t tPass;
	int iRet;

	if ( (fRadius <= 0.0f) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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

static int __xuiProxyXgeTextMeasure(xui_proxy pProxy, xui_font pFont, const char* sText, xui_vec2_t* pSize)
{
	xge_vec2_t tSize;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	tSize = xgeTextMeasure(&pFont->tFont, sText);
	pSize->fX = tSize.fX;
	pSize->fY = tSize.fY;
	return XGE_OK;
}

static int __xuiProxyXgeTextDraw(xui_proxy pProxy, xui_surface pTarget, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	xge_pass_t tPass;
	int iRet;

	if ( !__xuiProxyXgeFontValid(pFont) || (sText == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	     (tTarget.fW <= 0.0f) || (tTarget.fH <= 0.0f) ||
	     (pStyle == NULL) || (fTolerance <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeRectFillPx(__xuiProxyXgeRect(tRect), iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return XGE_OK;
	}
	(void)pProxy;
	xgeShapeRectStrokePx(__xuiProxyXgeRect(tRect), fWidth, iColor);
	__xuiProxyXgeDrawMarkDirty(pDraw);
	return XGE_OK;
}

static int __xuiProxyXgeDrawCircleFill(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, uint32_t iColor)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (fRadius <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (fRadius <= 0.0f) || (fWidth <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
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
	xge_rect_t tClip;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || (pRect == NULL) || (pHasClip == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	tClip = xgeClipGet();
	*pHasClip = ((tClip.fW > 0.0f) && (tClip.fH > 0.0f)) ? 1 : 0;
	pRect->fX = tClip.fX;
	pRect->fY = tClip.fY;
	pRect->fW = tClip.fW;
	pRect->fH = tClip.fH;
	return XGE_OK;
}

static int __xuiProxyXgeDrawClipSet(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect)
{
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	(void)xgeFlush();
	xgeClipSet(__xuiProxyXgeRect(xuiInternalSnapRect(tRect)));
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

static int __xuiProxyXgeMapKey(int iKey)
{
	if ( iKey >= 32 && iKey <= 126 ) return iKey;
	if ( iKey >= 290 && iKey <= 314 ) return 112 + (iKey - 290);
	switch ( iKey ) {
	case XGE_KEY_ESCAPE: return XUI_KEY_ESCAPE;
	case XGE_KEY_ENTER: return XUI_KEY_ENTER;
	case XGE_KEY_TAB: return XUI_KEY_TAB;
	case XGE_KEY_BACKSPACE: return XUI_KEY_BACKSPACE;
	case 260: return 45;
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
	default: return 0;
	}
}

static uint32_t __xuiProxyXgeMouseButtons(void)
{
	uint32_t iButtons = 0u;
	if ( xgeMouseDown(XGE_MOUSE_LEFT) ) iButtons |= XUI_POINTER_BUTTON_LEFT;
	if ( xgeMouseDown(XGE_MOUSE_RIGHT) ) iButtons |= XUI_POINTER_BUTTON_RIGHT;
	if ( xgeMouseDown(XGE_MOUSE_MIDDLE) ) iButtons |= XUI_POINTER_BUTTON_MIDDLE;
	return iButtons;
}

XUI_API int xuiProxyXgePumpInputRect(xui_context pContext, xui_rect_t tWindowRect)
{
	xge_ime_event_t tIme;
	uint32_t iButtons;
	uint32_t iChanged;
	uint32_t iModifiers;
	uint32_t iResult;
	uint32_t iCodepoint;
	float fMouseX;
	float fMouseY;
	float fWheelX;
	float fWheelY;
	xui_vec2_t tViewport;
	int iPressed;
	int iRepeated;
	int iReleased;
	int iMapped;
	int iRet;
	int iKey;

	if ( pContext == NULL || tWindowRect.fW <= 0.0f || tWindowRect.fH <= 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	xgeMouseGet(&fMouseX, &fMouseY);
	tViewport = xuiGetViewportSize(pContext);
	fMouseX = (fMouseX - tWindowRect.fX) * tViewport.fX / tWindowRect.fW;
	fMouseY = (fMouseY - tWindowRect.fY) * tViewport.fY / tWindowRect.fH;
	iButtons = __xuiProxyXgeMouseButtons();
	if ( !pContext->bXgeInputPointerReady || fMouseX != pContext->fXgeInputMouseX || fMouseY != pContext->fXgeInputMouseY ) {
		iRet = xuiInputPointerMove(pContext, fMouseX, fMouseY, iButtons);
		if ( iRet != XUI_OK ) return iRet;
	}
	iChanged = pContext->iXgeInputMouseButtons ^ iButtons;
	if ( (iChanged & XUI_POINTER_BUTTON_LEFT) != 0u ) {
		iRet = (iButtons & XUI_POINTER_BUTTON_LEFT) ? xuiInputPointerDown(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_LEFT, iButtons) : xuiInputPointerUp(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_LEFT, iButtons);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (iChanged & XUI_POINTER_BUTTON_RIGHT) != 0u ) {
		iRet = (iButtons & XUI_POINTER_BUTTON_RIGHT) ? xuiInputPointerDown(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_RIGHT, iButtons) : xuiInputPointerUp(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_RIGHT, iButtons);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (iChanged & XUI_POINTER_BUTTON_MIDDLE) != 0u ) {
		iRet = (iButtons & XUI_POINTER_BUTTON_MIDDLE) ? xuiInputPointerDown(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_MIDDLE, iButtons) : xuiInputPointerUp(pContext, fMouseX, fMouseY, XUI_POINTER_BUTTON_MIDDLE, iButtons);
		if ( iRet != XUI_OK ) return iRet;
	}
	xgeMouseGetWheel(&fWheelX, &fWheelY);
	if ( fWheelX != 0.0f || fWheelY != 0.0f ) {
		iRet = xuiInputPointerWheel(pContext, fMouseX, fMouseY, fWheelX, fWheelY, iButtons);
		if ( iRet != XUI_OK ) return iRet;
	}
	pContext->fXgeInputMouseX = fMouseX;
	pContext->fXgeInputMouseY = fMouseY;
	pContext->iXgeInputMouseButtons = iButtons;
	pContext->bXgeInputPointerReady = 1;
	iRet = xuiDispatchPendingEvents(pContext);
	if ( iRet != XUI_OK ) return iRet;

	iModifiers = __xuiProxyXgeInputModifiers();
	iRet = xuiInputSetModifiers(pContext, iModifiers);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tIme, 0, sizeof(tIme));
	while ( (iRet = xgeImeEventGet(&tIme)) > 0 ) {
		if ( tIme.iType == XGE_EVENT_IME_UPDATE ) {
			iRet = xuiInputImeComposition(pContext, tIme.sText, tIme.iTextSize, 0, (tIme.iTextSize > 0) ? tIme.iTextSize : 1);
		} else if ( tIme.iType == XGE_EVENT_IME_COMMIT ) {
			iRet = xuiInputImeComposition(pContext, tIme.sText, tIme.iTextSize, 0, 0);
		} else if ( tIme.iType == XGE_EVENT_IME_END ) {
			iRet = xuiInputImeComposition(pContext, "", 0, 0, 0);
		} else {
			iRet = XUI_OK;
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( iRet < 0 ) return iRet;
	iRet = xuiDispatchPendingEvents(pContext);
	if ( iRet != XUI_OK ) return iRet;
	for ( iKey = 0; iKey < XGE_KEY_COUNT; iKey++ ) {
		iMapped = __xuiProxyXgeMapKey(iKey);
		if ( iMapped == 0 ) continue;
		iPressed = xgeKeyPressed(iKey);
		iRepeated = xgeKeyRepeated(iKey);
		iReleased = xgeKeyReleased(iKey);
		if ( iPressed || iRepeated ) {
			iResult = 0u;
			iRet = xuiInputKeyDownEx(pContext, iMapped, iModifiers, &iResult);
			if ( iRet != XUI_OK ) return iRet;
			if ( (iResult & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(iKey);
		}
		if ( iReleased ) {
			iResult = 0u;
			iRet = xuiInputKeyUpEx(pContext, iMapped, iModifiers, &iResult);
			if ( iRet != XUI_OK ) return iRet;
			if ( (iResult & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(iKey);
		}
	}
	while ( (iCodepoint = xgeTextGet()) != 0u ) {
		if ( (iModifiers & (XUI_MOD_CTRL | XUI_MOD_ALT)) == 0u ) {
			iRet = xuiInputTextEx(pContext, iCodepoint, NULL);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiProxyXgePumpInput(xui_context pContext)
{
	xui_vec2_t tViewport;

	if ( pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tViewport = xuiGetViewportSize(pContext);
	return xuiProxyXgePumpInputRect(pContext, (xui_rect_t){0.0f, 0.0f, tViewport.fX, tViewport.fY});
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
	tProxy.imeGetEnabled = __xuiProxyXgeImeGetEnabled;
	tProxy.imeSetEnabled = __xuiProxyXgeImeSetEnabled;
	tProxy.imeSetCandidateRect = __xuiProxyXgeImeSetCandidateRect;
	tProxy.surfaceCreate = __xuiProxyXgeSurfaceCreate;
	tProxy.surfaceCreateRGBA = __xuiProxyXgeSurfaceCreateRGBA;
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
	return tProxy;
}
