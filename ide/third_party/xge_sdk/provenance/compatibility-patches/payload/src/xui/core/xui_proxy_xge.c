#include "xui_config.h"
#if XGE_ENABLE_XUI
#include "xui_internal.h"
#include "xui_proxy_resource.h"
#include "xui_native_accessibility_internal.h"
#include "xge.h"
#include "xge_ime_owner_internal.h"
#include "xge_data_lifetime_internal.h"
#include "xge_transfer_internal_api.h"
#include "xui_text_item_internal.h"
#if XUI_ENABLE_DOCUMENT_EDITOR
#include "xui_document_ui.h"
#endif
#if defined(__linux__) && XUI_ENABLE_DOCUMENT
#include "xui_document_internal.h"
#endif

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
#include <stdlib.h>
#include <string.h>

#define XUI_PROXY_XGE_SURFACE_MAGIC	0x58554953u
#define XUI_PROXY_XGE_FONT_MAGIC	0x58554946u
#define XUI_PROXY_XGE_DRAW_MAGIC	0x58554944u
#define XUI_PROXY_XGE_PI		3.14159265358979323846f
#define XUI_PROXY_XGE_PI_HALF		1.57079632679489661923f
#define XUI_PROXY_XGE_PI_TWO		6.28318530717958647692f
#define XUI_PROXY_XGE_MESH_STACK_VERTICES	160
#define XUI_PROXY_XGE_PAINT_SPAN_STACK_CAPACITY	128

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
static xui_context g_xuiProxyXgeDragContext;
static xui_data_object g_xuiProxyXgeDragData;
static uint64_t g_xuiProxyXgeDragRevision;
static uint32_t __xuiProxyXgeMapModifiers(uint32_t iModifiers);

static int __xuiProxyXgeResult(int iRet)
{
	switch ( iRet ) {
	case XGE_OK: return XUI_OK;
	case XGE_STATUS_DEFERRED: return XUI_STATUS_DEFERRED;
	case XGE_ERROR_BUSY: return XUI_ERROR_BUSY;
	case XGE_ERROR_INVALID_STATE: return XUI_ERROR_INVALID_STATE;
	case XGE_ERROR_CALLBACK_FAILED: return XUI_ERROR_CALLBACK_FAILED;
	case XGE_ERROR_INVALID_ARGUMENT: return XUI_ERROR_INVALID_ARGUMENT;
	case XGE_ERROR_OUT_OF_MEMORY: return XUI_ERROR_OUT_OF_MEMORY;
	case XGE_ERROR_BUFFER_TOO_SMALL: return XUI_ERROR_BUFFER_TOO_SMALL;
	case XGE_ERROR_NOT_FOUND: return XUI_ERROR_NOT_FOUND;
	case XGE_ERROR_UNSUPPORTED: return XUI_ERROR_UNSUPPORTED;
	default: return XUI_ERROR;
	}
}

static int __xuiProxyXgeDragRead(const char* sFormat, void* pOutput,
	size_t iCapacity, size_t* pOutputSize, void* pUser)
{
	return __xuiProxyXgeResult(xgeDataObjectGet((xge_data_object)pUser,
		sFormat, pOutput, iCapacity, pOutputSize));
}

static void __xuiProxyXgeDragRelease(void* pUser)
{
	xgeDataObjectRelease((xge_data_object)pUser);
}

static xge_data_object __xuiProxyXgeDragSource(const void* reference)
{const xge_data_lifetime_t* root=reference;return (xge_data_object)((char*)reference-root->pType->iBodyBytes);}
static bool __xuiProxyXgeDragSourceHold(const void* reference)
{xge_data_object shared=NULL;return xgeDataObjectShareOwned(__xuiProxyXgeDragSource(reference),&shared)==XGE_OK;}
static bool __xuiProxyXgeDragSourceDrop(const void* reference)
{bool consumed=false;int status=xgeDataObjectReleaseOwnerChecked(__xuiProxyXgeDragSource(reference),&consumed);return consumed && status==XGE_OK;}
static int __xuiProxyXgeDragReadOwned(const xstrview* format,xui_data_output_t* output,void* source)
{
    size_t required=0;int status=xgeDataObjectGet(source,format->Data,output->_pDestination,output->iCapacity,&required);
    output->iRequired=required;
    /* Native Owned Get reserves this code for its checked capacity result;
     * user callback nonzero status was already normalized CALLBACK_FAILED. */
    if(status==XGE_ERROR_BUFFER_TOO_SMALL && output->_pDestination && required>output->iCapacity)return XUI_OK;
    return __xuiProxyXgeResult(status);
}
static int __xuiProxyXgeDragDataWrap(xge_data_object pSource,
	xui_data_object* ppData)
{
	xui_data_object pData;
	const char* sFormat;
	int iCount;
	int i;
	int iRet;

	if ( pSource == NULL || ppData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppData = NULL;
	bool owned=xgeDataObjectIsOwned(pSource);
	iRet=owned ? xuiDataObjectCreateOwned(&pData):xuiDataObjectCreate(&pData);
	if ( iRet != XUI_OK ) return iRet;
	iCount = xgeDataObjectFormatCount(pSource);
	iRet = XUI_OK;
	for ( i = 0; i < iCount; i++ ) {
		sFormat = xgeDataObjectFormatAt(pSource, i);
		if ( sFormat == NULL ) continue;
        if(owned){
            xui_callback_owner_t owner={xgeDataObjectOwnership(pSource),__xuiProxyXgeDragSourceHold,__xuiProxyXgeDragSourceDrop};
            iRet=xuiDataObjectSetProviderOwned(pData,sFormat,__xuiProxyXgeDragReadOwned,pSource,&owner);
        }else{
            if(xgeDataObjectAddRef(pSource)!=XGE_OK){iRet=XUI_ERROR_INVALID_ARGUMENT;break;}
            iRet=xuiDataObjectSetProvider(pData,sFormat,__xuiProxyXgeDragRead,__xuiProxyXgeDragRelease,pSource);
            if(iRet!=XUI_OK)xgeDataObjectRelease(pSource);
        }
        if(iRet!=XUI_OK)break;
	}
	if ( iRet != XUI_OK ) {
		xuiDataObjectRelease(pData);
		return iRet;
	}
	*ppData = pData;
	return XUI_OK;
}

static int __xuiProxyXgeDragDataClear(xui_context context,int status)
{
    xui_data_object previous=g_xuiProxyXgeDragData;g_xuiProxyXgeDragData=NULL;
    if(!previous)return status;
    if(g_xuiProxyXgeDragRevision!=UINT64_MAX)++g_xuiProxyXgeDragRevision;
    xerror* first=xrtTakeError();if(status==XUI_OK && first)status=XUI_ERROR_CALLBACK_FAILED;
    int dropped=XUI_OK;bool consumed=true;
    if(xuiDataObjectIsOwned(previous))dropped=xuiDataObjectReleaseOwnerChecked(previous,&consumed);
    else xuiDataObjectRelease(previous);
    xuiInternalContextCleanupOutcome(context,dropped,&status,&first);xrtSetErrorTake(first);
    /* Only caller-managed raw C misuse can violate creator/owner preflight.
     * Keep that actual owner visible instead of silently losing its carrier. */
    if(!consumed && !g_xuiProxyXgeDragData)g_xuiProxyXgeDragData=previous;
    return status;
}
static int __xuiProxyXgeDragEvent(const xge_drag_event_t* event,uint32_t* effect,void* user)
{
    xui_context context=user;xui_proxy_xge_input_transform_t* transform=&g_xuiProxyXgeInputTransform;
    if(!effect || !event)return XGE_ERROR_INVALID_ARGUMENT;
    *effect=XGE_DRAG_EFFECT_NONE;
    uint64_t uiThread=xgeInternalTransferThreadId();
    if(uiThread && uiThread!=xrtThreadCurrentId())return XGE_ERROR_BUSY;
    /* Default OS dispatch is creator-thread only. This non-owning native
     * back-link is removed by Detach before physical Context destruction. */
    if(!context || transform->pContext!=context || !transform->bValid ||
       context->bCloseRequested || context->bDestroyPending || context->bDestroying)return XGE_OK;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;
    int x=xuiInternalPixelFloor((event->fX-(float)transform->tWindowRect.fX)*(float)transform->tViewport.iW/(float)transform->tWindowRect.fW);
    int y=xuiInternalPixelFloor((event->fY-(float)transform->tWindowRect.fY)*(float)transform->tViewport.iH/(float)transform->tWindowRect.fH);
    int type=0;uint32_t negotiated=0;
    bool entering=event->iType==XGE_DRAG_EVENT_ENTER ||
        (event->iType==XGE_DRAG_EVENT_OVER && (!context->bTransferActive || !context->bTransferExternal));
    if(entering){
        status=__xuiProxyXgeDragDataClear(context,XUI_OK);
        if(status!=XUI_OK)goto finish;
        if(g_xuiProxyXgeDragContext!=context)goto finish;
        if(g_xuiProxyXgeDragRevision==UINT64_MAX){status=XUI_ERROR_INVALID_STATE;goto finish;}
        xui_data_object incoming=NULL;status=__xuiProxyXgeDragDataWrap(event->pData,&incoming);
        if(status!=XUI_OK)goto finish;
        g_xuiProxyXgeDragData=incoming;++g_xuiProxyXgeDragRevision;type=XUI_DRAG_EXTERNAL_ENTER;
    }else if(event->iType==XGE_DRAG_EVENT_OVER)type=XUI_DRAG_EXTERNAL_OVER;
    else if(event->iType==XGE_DRAG_EVENT_LEAVE)type=XUI_DRAG_EXTERNAL_LEAVE;
    else if(event->iType==XGE_DRAG_EVENT_DROP)type=XUI_DRAG_EXTERNAL_DROP;
    else if(event->iType==XGE_DRAG_EVENT_CANCEL)type=XUI_DRAG_EXTERNAL_CANCEL;
    else goto finish;
    if((type==XUI_DRAG_EXTERNAL_LEAVE || type==XUI_DRAG_EXTERNAL_DROP || type==XUI_DRAG_EXTERNAL_CANCEL) &&
       (!context->bTransferActive || !context->bTransferExternal)){
        status=__xuiProxyXgeDragDataClear(context,XUI_OK);goto finish;
    }
    uint64_t revision=g_xuiProxyXgeDragRevision;
    status=xuiDragExternalEvent(context,type,x,y,__xuiProxyXgeMapModifiers(event->iModifiers),
        type==XUI_DRAG_EXTERNAL_ENTER ? g_xuiProxyXgeDragData:NULL,event->iAllowedEffects,event->iSuggestedEffect,&negotiated);
    if((type==XUI_DRAG_EXTERNAL_LEAVE || type==XUI_DRAG_EXTERNAL_DROP || type==XUI_DRAG_EXTERNAL_CANCEL) &&
       g_xuiProxyXgeDragContext==context && g_xuiProxyXgeDragRevision==revision)status=__xuiProxyXgeDragDataClear(context,status);
finish:
    {xerror* first=xrtTakeError();status=xuiInternalCallScopeEndOutcome(&scope,status,&first);xrtSetErrorTake(first);}
    if(status==XUI_OK)*effect=negotiated;
    return status>0 ? XGE_ERROR_CALLBACK_FAILED:status;
}

static double __xuiProxyXgeClockSeconds(xui_proxy pProxy)
{
	(void)pProxy;
	return xgeTimer();
}

static void __xuiProxyXgeRequestFrame(xui_proxy pProxy, float fDelaySeconds)
{
	(void)pProxy;
	xgeRenderRequestAfter(fDelaySeconds);
}

struct xui_surface_t {
	xui_owned_resource_t tOwned;
	uint32_t iMagic;
	int iKind;
	int iFormat;
	uint32_t iFlags;
	uint32_t iGeneration;
	xge_texture_t tTexture;
	xge_render_target_t tTarget;
};

struct xui_font_t {
	xui_owned_resource_t tOwned;
	uint32_t iMagic;
	uint32_t iFlags;
	unsigned iPaintRefs;
	int bDestroyPending;
	xge_font_t tFont;
	xge_font_t tFallbackFont;
	int bHasFallback;
};
_Static_assert(offsetof(struct xui_font_t,iMagic)==sizeof(xui_owned_resource_t),"owned font payload offset");
_Static_assert(offsetof(struct xui_surface_t,iMagic)==sizeof(xui_owned_resource_t),"owned surface payload offset");

static int __xuiProxyXgeFontPayloadFinalize(xui_proxy proxy,void* payload)
{
	(void)proxy;xui_font font=(xui_font)((xui_owned_resource_t*)payload-1);
	xgeFontFree(&font->tFont);
	if(font->bHasFallback)xgeFontFree(&font->tFallbackFont);
	font->bHasFallback=0;font->iMagic=0;return XUI_OK;
}
static int __xuiProxyXgeSurfacePayloadFinalize(xui_proxy proxy,void* payload)
{
	(void)proxy;xui_surface surface=(xui_surface)((xui_owned_resource_t*)payload-1);
	if(surface->iFlags & XUI_SURFACE_USAGE_TARGET)xgeRenderTargetFree(&surface->tTarget);
	else xgeTextureFree(&surface->tTexture);
	surface->iMagic=0;return XUI_OK;
}
static xui_font __xuiProxyXgeFontAlloc(xui_proxy proxy,int* status)
{
	xui_font font=NULL;
	if(xuiProxyIsOwned(proxy)){
		void* payload=NULL;*status=xuiFontCreateOwned(proxy,sizeof(*font)-sizeof(font->tOwned),__xuiProxyXgeFontPayloadFinalize,&font,&payload);
	}else{font=xrtCalloc(1,sizeof(*font));*status=font ? XUI_OK:XGE_ERROR_OUT_OF_MEMORY;}
	return font;
}
/* Failure rollback keeps the operation's original diagnostic, even if a
 * provider owner's cleanup reports another failure. */
static void __xuiProxyXgeFontDiscard(xui_font font)
{
	xerror* primary=xrtTakeError();
	if(xuiInternalProxyResourceManaged(font,XUI_OWNED_FONT)){bool consumed=false;(void)xuiFontReleaseOwnerChecked(font,&consumed);}
	else{(void)__xuiProxyXgeFontPayloadFinalize(NULL,xuiInternalProxyResourcePayload(font));xrtFree(font);}
	xrtClearError();xrtSetErrorTake(primary);
}
static void __xuiProxyXgeSurfaceDiscard(xui_surface surface)
{
	xerror* primary=xrtTakeError();
	if(xuiInternalProxyResourceManaged(surface,XUI_OWNED_SURFACE)){bool consumed=false;(void)xuiSurfaceReleaseOwnerChecked(surface,&consumed);}
	else{(void)__xuiProxyXgeSurfacePayloadFinalize(NULL,xuiInternalProxyResourcePayload(surface));xrtFree(surface);}
	xrtClearError();xrtSetErrorTake(primary);
}

struct xui_draw_context_t {
	xui_owned_draw_t tOwned;
	uint32_t iMagic;
	xge_pass_t tPass;
	int bDirty;
};
static int __xuiProxyXgeDrawPayloadFinalize(xui_proxy,void*);

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
	if(xuiInternalProxyResourceManaged(pFont,XUI_OWNED_FONT) &&
	   (pFont->tOwned.onFinalize!=__xuiProxyXgeFontPayloadFinalize || !xuiInternalProxyResourceReady(pFont,XUI_OWNED_FONT)))return 0;
	return (pFont != NULL) &&
	       (pFont->iMagic == XUI_PROXY_XGE_FONT_MAGIC) &&
	       !pFont->bDestroyPending &&
	       (pFont->tFont.iRefCount > 0);
}

static int __xuiProxyXgeSurfaceValid(xui_surface pSurface)
{
	xge_texture pTexture;
	if(xuiInternalProxyResourceManaged(pSurface,XUI_OWNED_SURFACE) &&
	   (pSurface->tOwned.onFinalize!=__xuiProxyXgeSurfacePayloadFinalize || !xuiInternalProxyResourceReady(pSurface,XUI_OWNED_SURFACE)))return 0;

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

static void __xuiProxyXgePremultiplyRGBA(unsigned char* pDst, const unsigned char* pSrc, size_t iCount)
{
	unsigned int iA;
	size_t i;

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
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	/* The texture's packed CPU storage uses an int byte count. Validate
	 * before multiplication, input reads, allocation, or the borrowed fast path. */
	if ( iWidth > INT32_MAX / 4 || iHeight > INT32_MAX / (iWidth * 4) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pPixels == NULL ) {
		return XGE_OK;
	}
	iRowBytes = iWidth * 4;
	if ( iStride <= 0 ) {
		iStride = iRowBytes;
	}
	if ( iStride < iRowBytes ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (size_t)(iHeight - 1) > (SIZE_MAX - (size_t)iRowBytes) / (size_t)iStride ) {
		return XGE_ERROR_OUT_OF_MEMORY;
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
		__xuiProxyXgePremultiplyRGBA(pPacked, pPacked, (size_t)iWidth * (size_t)iHeight);
	}
	*ppPacked = pPacked;
	*ppOutPixels = pPacked;
	return XGE_OK;
}

static xui_surface __xuiProxyXgeSurfaceAlloc(xui_proxy pProxy,uint32_t iFlags,int* status)
{
	xui_surface pSurface;

	if(xuiProxyIsOwned(pProxy)){
		void* payload=NULL;pSurface=NULL;
		*status=xuiSurfaceCreateOwned(pProxy,sizeof(*pSurface)-sizeof(pSurface->tOwned),__xuiProxyXgeSurfacePayloadFinalize,&pSurface,&payload);
	}else{pSurface=(xui_surface)xrtCalloc(1,sizeof(*pSurface));*status=pSurface ? XUI_OK:XGE_ERROR_OUT_OF_MEMORY;}
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

static uint32_t __xuiProxyXgeSurfaceDrawFlags(xui_surface pSurface, uint32_t iFlags)
{
	uint32_t iDrawFlags;

	iDrawFlags = __xuiProxyXgeDrawFlags(iFlags);
	(void)pSurface;
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
	(void)pSurface;
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
	xgePassInit(pPass, &pTarget->tTarget, 0, 0);
	return xgePassBegin(pPass);
}

static int __xuiProxyXgeTargetEnd(xge_pass_t* pass,int primary)
{
    if(!pass)return XGE_ERROR_INVALID_ARGUMENT;
    /* PassEnd owns the single Flush under its ENDING reservation. Preserve
     * the original operation's first diagnostic, including a failed NULL. */
    xerror* first=primary!=XGE_OK ? xrtTakeError():NULL;
    int ended=xgePassEnd(pass);
    if(primary!=XGE_OK){xrtClearError();xrtSetErrorTake(first);return primary;}
    return ended;
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
	if(!pDraw)return false;
	if(xuiInternalProxyResourceManaged(pDraw,XUI_OWNED_DRAW)){
		if(pDraw->tOwned.tResource.onFinalize!=__xuiProxyXgeDrawPayloadFinalize ||
		   !xuiInternalProxyResourceReady(pDraw,XUI_OWNED_DRAW))return false;
	}else if(pDraw->tOwned.tResource.iMagic!=XUI_PROXY_XGE_DRAW_MAGIC)return false;
	return (pDraw->iMagic == XUI_PROXY_XGE_DRAW_MAGIC) &&
	       xgePassIsCurrent(&pDraw->tPass) &&
	       __xuiProxyXgeSurfaceTargetValid(pDraw->tOwned.pTarget);
}

static void __xuiProxyXgeDrawMarkDirty(xui_draw_context pDraw)
{
	/* Every caller admitted this live Draw before entering the renderer. A
	 * second Current query cannot make a reentrant raw-C free safe, and would
	 * miss completed drawing when a managed End became pending in Flush. */
	pDraw->bDirty = 1;
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
	               XUI_PROXY_CAP_TEXT | XUI_PROXY_CAP_TEXT_RANGE;
#if XGE_ENABLE_HARFBUZZ
	pCaps->iCaps |= XUI_PROXY_CAP_TEXT_CONTEXT | XUI_PROXY_CAP_TEXT_SCRIPT |
		XUI_PROXY_CAP_TEXT_LANGUAGE | XUI_PROXY_CAP_TEXT_RTL;
#endif
	pCaps->iSurfaceFormat = XUI_SURFACE_FORMAT_RGBA8;
	pCaps->iInternalAlpha = XUI_SURFACE_ALPHA_PREMULTIPLIED;
	tSampler = xgeSamplerDefault();
	pCaps->tDefaultSampler = __xuiProxyXgeSurfaceSampler(tSampler);
	return XGE_OK;
}

static int __xuiProxyXgeClipboardSetText(xui_proxy pProxy, const char* sText)
{
	xge_clipboard_item_t tItem;
	if ( pProxy == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	if ( sText == NULL ) sText = "";
	tItem.sFormat = XGE_CLIPBOARD_FORMAT_TEXT_UTF8;
	tItem.pData = sText;
	tItem.iDataSize = strlen(sText);
	/* Cut must not delete text when the native clipboard rejects the copy. */
	return xgeClipboardSetItems(&tItem, 1);
}

static int __xuiProxyXgeClipboardGetText(xui_proxy pProxy, char* sText, int iCapacity)
{
	int iLength;
	int iCopy;

	if ( (pProxy == NULL) || (sText == NULL) || (iCapacity <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	iLength = xgeClipboardGetData(XGE_CLIPBOARD_FORMAT_TEXT_UTF8,
		sText, (size_t)iCapacity - 1u);
	if ( iLength < 0 ) { sText[0] = 0; return iLength; }
	iCopy = iLength;
	if ( iCopy >= iCapacity ) iCopy = iCapacity - 1;
	sText[iCopy] = 0;
	return iLength;
}

static int __xuiProxyXgeClipboardPublish(xui_proxy proxy,const xui_clipboard_item_t* items,int count,const xn_transfer_services_v1* services)
{
    if(!proxy || !items || count<=0 || count>64)return XUI_ERROR_INVALID_ARGUMENT;
    /* Distinct public C record types are copied as typed fields. This bounded
     * cold stack projection needs no per-copy heap adapter allocation. */
    xge_clipboard_item_t native[64];
    for(int i=0;i<count;++i){native[i].sFormat=items[i].sFormat;native[i].pData=items[i].pData;native[i].iDataSize=items[i].iDataSize;}
    return __xuiProxyXgeResult(services ? xgeClipboardSetItemsWithTransfer(native,count,services):xgeClipboardSetItems(native,count));
}
static int __xuiProxyXgeClipboardSetItems(xui_proxy proxy,const xui_clipboard_item_t* items,int count)
{return __xuiProxyXgeClipboardPublish(proxy,items,count,NULL);}
static int __xuiProxyXgeClipboardSetItemsWithTransfer(xui_proxy proxy,const xui_clipboard_item_t* items,int count,const xn_transfer_services_v1* services)
{return __xuiProxyXgeClipboardPublish(proxy,items,count,services);}

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
	     (pDesc->iWidth <= 0) || (pDesc->iHeight <= 0) ||
	     ((pDesc->iFlags & XUI_SURFACE_COMPRESS_MASK) == XUI_SURFACE_COMPRESS_MASK) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,pDesc->iFlags,&iRet);
	if ( pSurface == NULL ) {
		return iRet;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		iRet = xgeRenderTargetCreate(&pSurface->tTarget, pDesc->iWidth, pDesc->iHeight);
	} else {
		iRet = xgeTextureCreateRGBAEx(&pSurface->tTexture, pDesc->iWidth, pDesc->iHeight, NULL, pDesc->iFlags & XUI_SURFACE_COMPRESS_MASK);
	}
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
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

	if ( (pProxy == NULL) || (ppSurface == NULL) || (iWidth <= 0) || (iHeight <= 0) ||
	     ((iFlags & XUI_SURFACE_COMPRESS_MASK) == XUI_SURFACE_COMPRESS_MASK) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	*ppSurface = NULL;
	iRet = __xuiProxyXgeSurfacePackRGBA(pPixels, iWidth, iHeight, iStride, iFlags, &pPacked, &pUploadPixels);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,iFlags,&iRet);
	if ( pSurface == NULL ) {
		if ( pPacked != NULL ) {
			xrtFree(pPacked);
		}
		return iRet;
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
		iRet = xgeTextureCreateRGBAEx(&pSurface->tTexture, iWidth, iHeight, pUploadPixels, iFlags & XUI_SURFACE_COMPRESS_MASK);
	}
	if ( pPacked != NULL ) {
		xrtFree(pPacked);
	}
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
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
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,iFlags,&iRet);
	if ( pSurface == NULL ) {
		return iRet;
	}
	iRet = xgeTextureLoadEx(&pSurface->tTexture, sPath, __xuiProxyXgeImageFlags(iFlags) | (iFlags & XUI_SURFACE_COMPRESS_MASK));
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
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
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,iFlags,&iRet);
	if ( pSurface == NULL ) {
		return iRet;
	}
	iRet = xgeTextureLoadMemoryEx(&pSurface->tTexture, pData, iSize, __xuiProxyXgeImageFlags(iFlags) | (iFlags & XUI_SURFACE_COMPRESS_MASK));
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}

#if XGE_ENABLE_SVG
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
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,iFlags,&iRet);
	if ( pSurface == NULL ) {
		return iRet;
	}
	iRet = xgeSvgTextureLoadEx(&pSurface->tTexture, sPath, iWidth, iHeight, iFlags & XUI_SURFACE_COMPRESS_MASK);
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
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
	pSurface = __xuiProxyXgeSurfaceAlloc(pProxy,iFlags,&iRet);
	if ( pSurface == NULL ) {
		return iRet;
	}
	iRet = xgeSvgTextureLoadMemoryEx(&pSurface->tTexture, pData, iSize, iWidth, iHeight, iFlags & XUI_SURFACE_COMPRESS_MASK);
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeSurfaceDiscard(pSurface);
		return iRet;
	}
	*ppSurface = pSurface;
	return XGE_OK;
}


#endif
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
	pFont = __xuiProxyXgeFontAlloc(pProxy,&iRet);
	if ( pFont == NULL ) {
		return iRet;
	}
	bUseXrf = __xuiProxyXgeFontUseXrf(sPath, iFlags);
	if ( bUseXrf ) {
		iRet = xgeFontLoadXRF(&pFont->tFont, sPath);
	} else {
		if ( fSize <= 0.0f ) {
			__xuiProxyXgeFontDiscard(pFont);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iRet = xgeFontLoad(&pFont->tFont, sPath, fSize);
	}
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeFontDiscard(pFont);
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
	pFont = __xuiProxyXgeFontAlloc(pProxy,&iRet);
	if ( pFont == NULL ) {
		return iRet;
	}
	if ( (iFlags & XUI_FONT_FORMAT_XRF) != 0 ) {
		iRet = xgeFontLoadXRFMemory(&pFont->tFont, pData, iSize);
	} else {
		if ( fSize <= 0.0f ) {
			__xuiProxyXgeFontDiscard(pFont);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iRet = xgeFontLoadMemory(&pFont->tFont, pData, iSize, fSize);
	}
	if ( iRet != XGE_OK ) {
		__xuiProxyXgeFontDiscard(pFont);
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

static int __xuiProxyXgeFontLoadSystem(xui_proxy pProxy,xui_font* ppFont,const xui_system_font_desc_t* pDesc)
{
	xge_system_font_desc_t desc={0};
	xui_font font;
	int status;
	if(!pProxy || !ppFont || !pDesc || pDesc->iSize<sizeof(*pDesc) || pDesc->iFlags!=0)
		return XGE_ERROR_INVALID_ARGUMENT;
	*ppFont=NULL;
	font=__xuiProxyXgeFontAlloc(pProxy,&status);
	if(!font)return status;
	desc.iSize=sizeof(desc);
	desc.iRole=XGE_SYSTEM_FONT_UI;
	desc.fPixelSize=pDesc->fSize;
	status=xgeFontLoadSystem(&font->tFont,&desc);
	if(status!=XGE_OK){__xuiProxyXgeFontDiscard(font);return status;}
	if(xgeFontFallbackGet(&font->tFallbackFont,font->tFont.fSize)==XGE_OK){
		xgeFontSetFallback(&font->tFont,&font->tFallbackFont);
		font->bHasFallback=1;
	}
	font->iMagic=XUI_PROXY_XGE_FONT_MAGIC;
	font->iFlags=XUI_FONT_FORMAT_TTF;
	*ppFont=font;
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
	pMetrics->fUnderlinePosition = tMetrics.fUnderlinePosition;
	pMetrics->fUnderlineThickness = tMetrics.fUnderlineThickness;
	pMetrics->fStrikePosition = tMetrics.fStrikePosition;
	pMetrics->fStrikeThickness = tMetrics.fStrikeThickness;
	return XGE_OK;
}

static void __xuiProxyXgeFontRelease(xui_font pFont)
{
	xgeFontFree(&pFont->tFont);
	if(pFont->bHasFallback)xgeFontFree(&pFont->tFallbackFont);
	pFont->iMagic=0;xrtFree(pFont);
}
static void __xuiProxyXgeFontDestroy(xui_proxy pProxy, xui_font pFont)
{
	if ( pProxy == NULL ) {
		return;
	}
	(void)pProxy;
	if(xuiInternalProxyResourceManaged(pFont,XUI_OWNED_FONT)){
		bool consumed=false;int status=xuiFontReleaseOwnerChecked(pFont,&consumed);
		if(status!=XUI_OK && !xrtGetError())xrtSetErrorKind(XERR_STATE);
		return;
	}
	if ( !__xuiProxyXgeFontValid(pFont) ) {
		return;
	}
	pFont->bDestroyPending=1;
	if(!pFont->iPaintRefs)__xuiProxyXgeFontRelease(pFont);
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
	pFont = __xuiProxyXgeFontAlloc(pProxy,&iRet);
	if ( pFont == NULL ) return iRet;
	iRet = xgeFontCreateSized(&pFont->tFont, &pSource->tFont, fSize);
	if ( iRet != XGE_OK ) { __xuiProxyXgeFontDiscard(pFont); return iRet; }
	if ( pSource->bHasFallback ) {
		iRet = xgeFontCreateSized(&pFont->tFallbackFont, &pSource->tFallbackFont, fSize);
		if ( iRet != XGE_OK ) {
			__xuiProxyXgeFontDiscard(pFont);
			return iRet;
		}
		xgeFontSetFallback(&pFont->tFont, &pFont->tFallbackFont);
		pFont->bHasFallback = 1;
	}
	pFont->iMagic = XUI_PROXY_XGE_FONT_MAGIC;
	pFont->iFlags = pSource->iFlags;
	*ppFont = pFont;
	return XGE_OK;
}

static int __xuiProxyXgeTextDesc(const xui_text_item_t* item, xge_text_shape_desc_t* desc)
{
    xui_text_item_t normalized;int result=__xuiTextItemNormalize(item,&normalized);
    if(result!=XUI_OK)return result;
    if(!__xuiProxyXgeFontValid(normalized.pFont))return XGE_ERROR_INVALID_ARGUMENT;
    memset(desc,0,sizeof(*desc));desc->iSize=sizeof(*desc);
    desc->pFont=&normalized.pFont->tFont;desc->sText=normalized.sText;desc->iTextSize=normalized.iTextSize;
    desc->sContext=normalized.sContext;desc->iContextSize=normalized.iContextSize;desc->iContextOffset=normalized.iContextOffset;
    desc->iScript=normalized.iScript;desc->sLanguage=normalized.sLanguage;
    if(normalized.iFlags & XUI_TEXT_SHAPE_KERNING)desc->iFlags|=XGE_TEXT_SHAPE_KERNING;
    if(normalized.iFlags & XUI_TEXT_SHAPE_EMOJI)desc->iFlags|=XGE_TEXT_SHAPE_EMOJI;
    if(normalized.iFlags & XUI_TEXT_SHAPE_RTL)desc->iFlags|=XGE_TEXT_SHAPE_RTL;
    desc->iEmojiPresentation=XGE_EMOJI_PRESENTATION_AUTO;desc->iEmojiLinePolicy=XGE_EMOJI_LINE_STABLE;desc->fEmojiScale=1;
    return XGE_OK;
}
static int __xuiProxyXgeTextRangeMeasure(xui_proxy,const xui_text_item_t*,xui_vec2_t*);
static int __xuiProxyXgeTextRangeShape(xui_proxy,const xui_text_item_t*,xui_text_shape_t*);
static int __xuiProxyXgeTextRangeDraw(xui_proxy,xui_draw_context,const xui_text_item_t*,xui_rect_t,uint32_t,uint32_t,const xui_text_paint_span_t*,int);
static int __xuiProxyXgeTextRangeSurfaceDraw(xui_proxy,xui_surface,const xui_text_item_t*,xui_rect_t,uint32_t,uint32_t);
static int __xuiProxyXgeTextMeasure(xui_proxy pProxy, const xui_text_item_t* pTextItem, xui_vec2_t* pSize)
{
    xge_text_shape_desc_t desc;xge_glyph_run_t run={0};int result;
    if(!pProxy || !pSize)return XGE_ERROR_INVALID_ARGUMENT;
    memset(pSize,0,sizeof(*pSize));result=__xuiProxyXgeTextDesc(pTextItem,&desc);
    if(result!=XGE_OK)return result;
    if(pTextItem->iFlags & XUI_TEXT_SHAPE_RANGE)return __xuiProxyXgeTextRangeMeasure(pProxy,pTextItem,pSize);
    result=xgeTextShape(&desc,&run);
    if(result==XGE_OK){pSize->fX=(float)xuiInternalPixelCeil(run.fWidth);pSize->fY=(float)xuiInternalPixelCeil(run.fHeight);}
    xgeGlyphRunFree(&run);return result;
}

static int __xuiProxyXgeDrawBegin(xui_proxy,xui_draw_context*,xui_surface);
static int __xuiProxyXgeDrawEnd(xui_proxy,xui_draw_context);
static int __xuiProxyXgeDrawEndOutcome(xui_proxy,xui_draw_context,int);
static int __xuiProxyXgeDrawText(xui_proxy, xui_draw_context, const xui_text_item_t* pTextItem, xui_rect_t, uint32_t, uint32_t);
static int __xuiProxyXgeTextDraw(xui_proxy pProxy, xui_surface pTarget, const xui_text_item_t* pTextItem, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
    xge_text_shape_desc_t desc;xui_draw_context draw;int result;
    if(!pProxy || !__xuiProxyXgeSurfaceTargetValid(pTarget))return XGE_ERROR_INVALID_ARGUMENT;
    result=__xuiProxyXgeTextDesc(pTextItem,&desc);if(result!=XGE_OK)return result;
    if(pTextItem->iFlags & XUI_TEXT_SHAPE_RANGE)return __xuiProxyXgeTextRangeSurfaceDraw(pProxy,pTarget,pTextItem,tRect,iColor,iFlags);
    if(tRect.fW<=0 || tRect.fH<=0)return XGE_OK;
    result=__xuiProxyXgeDrawBegin(pProxy,&draw,pTarget);if(result!=XGE_OK)return result;
    result=__xuiProxyXgeDrawText(pProxy,draw,pTextItem,tRect,iColor,iFlags);
    return __xuiProxyXgeDrawEndOutcome(pProxy,draw,result);
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
	pDraw->tOwned.tResource.iMagic = XUI_PROXY_XGE_DRAW_MAGIC;
	pDraw->tOwned.pTarget = pTarget;
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

	if(pProxy && xuiInternalProxyResourceManaged(pDraw,XUI_OWNED_DRAW)){
		if(pDraw->tOwned.tResource.onFinalize!=__xuiProxyXgeDrawPayloadFinalize)return XUI_ERROR_INVALID_ARGUMENT;
		return xuiDrawReleaseChecked(pDraw);
	}
	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
	pTarget = pDraw->tOwned.pTarget;
	bDirty = pDraw->bDirty;
	iRet = __xuiProxyXgeTargetEnd(&pDraw->tPass, XGE_OK);
	if ( (iRet == XGE_OK) && bDirty ) {
		__xuiProxyXgeSurfaceBump(pTarget);
	}
	/* A non-current End does not consume the raw C record. */
	if(pDraw->tPass.bActive)return iRet;
	pDraw->iMagic = 0;
	xrtFree(pDraw);
	return iRet;
}

static int __xuiProxyXgeDrawEndOutcome(xui_proxy proxy,xui_draw_context draw,int primary)
{
    xerror* first=primary!=XGE_OK ? xrtTakeError():NULL;
    int ended=__xuiProxyXgeDrawEnd(proxy,draw);
    if(primary!=XGE_OK){xrtClearError();xrtSetErrorTake(first);return primary;}
    return ended;
}
static bool __xuiProxyXgeDrawPayloadReady(const void* payload)
{
    const struct xui_draw_context_t* draw=(const void*)((const xui_owned_draw_t*)payload-1);
    /* Constructor rollback before Begin has no semantic pass to consume. */
    return !draw->tPass.bActive || xgePassIsCurrent(&draw->tPass);
}
static int __xuiProxyXgeDrawPayloadFinalize(xui_proxy proxy,void* payload)
{
	(void)proxy;
	xui_draw_context draw=(xui_draw_context)((xui_owned_draw_t*)payload-1);
	int status=XUI_OK;
	if(draw->tPass.bActive)status=__xuiProxyXgeTargetEnd(&draw->tPass,XGE_OK);
	/* Preparation completes End before target finalization. This privileged
	 * generation update uses the known owned payload, even while claimed. */
	xui_surface target=draw->tOwned.pTarget;
	if(status==XUI_OK && draw->bDirty && target && target->iMagic==XUI_PROXY_XGE_SURFACE_MAGIC){
		if(++target->iGeneration==0)target->iGeneration=1;
	}
	draw->iMagic=0;return status;
}
static int __xuiProxyXgeManualDrawBegin(xui_proxy proxy,xui_draw_context* output,xui_surface target)
{
	if(!xuiProxyIsOwned(proxy))return __xuiProxyXgeDrawBegin(proxy,output,target);
	if(!output || !__xuiProxyXgeSurfaceTargetValid(target) ||
	   !xuiInternalProxyResourceReady(target,XUI_OWNED_SURFACE) || xuiSurfaceGetProxy(target)!=proxy)return XUI_ERROR_INVALID_ARGUMENT;
	if(!xgePassIsIdle())return XUI_ERROR_BUSY;
	xui_draw_context draw=NULL;void* payload=NULL;
	int status=xuiDrawCreateOwnedEx(proxy,target,sizeof(*draw)-sizeof(draw->tOwned),__xuiProxyXgeDrawPayloadFinalize,__xuiProxyXgeDrawPayloadReady,&draw,&payload);
	if(status!=XUI_OK)return status;
	(void)payload;draw->iMagic=XUI_PROXY_XGE_DRAW_MAGIC;
	status=__xuiProxyXgeTargetBegin(proxy,target,&draw->tPass);
	if(status!=XUI_OK){
		xerror* first=xrtTakeError();(void)xuiDrawReleaseChecked(draw);xrtClearError();xrtSetErrorTake(first);return status;
	}
	*output=draw;return XUI_OK;
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
	xge_shape_vertex_t arrStackVertices[XUI_PROXY_XGE_MESH_STACK_VERTICES];
	xge_shape_vertex_t* pXgeVertices;
	int bHeapVertices;
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
	bHeapVertices = iVertexCount > XUI_PROXY_XGE_MESH_STACK_VERTICES;
	if ( bHeapVertices ) {
		pXgeVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pXgeVertices));
		if ( pXgeVertices == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	} else {
		pXgeVertices = arrStackVertices;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		pXgeVertices[i].fX = pVertices[i].fX;
		pXgeVertices[i].fY = pVertices[i].fY;
		pXgeVertices[i].iColor = pVertices[i].iColor;
	}
	iRet = xgeShapeMeshFillPx(pXgeVertices, iVertexCount, pIndices, iIndexCount);
	if ( bHeapVertices ) {
		xrtFree(pXgeVertices);
	}
	if ( iRet == XGE_OK ) {
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	return iRet;
}

#if XGE_ENABLE_SHAPE_EX
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
	fScaleX = (float)tTarget.fW / (float)tViewBox.fW;
	fScaleY = (float)tTarget.fH / (float)tViewBox.fH;
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


#endif
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

static int __xuiProxyXgeDrawTextSpans(xui_proxy, xui_draw_context, const xui_text_item_t* pTextItem, xui_rect_t, uint32_t, uint32_t, const xui_text_paint_span_t*, int);
static int __xuiProxyXgeDrawText(xui_proxy pProxy, xui_draw_context pDraw, const xui_text_item_t* pTextItem, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
    return __xuiProxyXgeDrawTextSpans(pProxy,pDraw,pTextItem,tRect,iColor,iFlags,NULL,0);
}

static int __xuiProxyXgeDrawTextSpans(xui_proxy pProxy, xui_draw_context pDraw, const xui_text_item_t* pTextItem, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags, const xui_text_paint_span_t* pSpans, int iSpanCount)
{

	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	xge_text_paint_span_t sSmall[XUI_PROXY_XGE_PAINT_SPAN_STACK_CAPACITY];
	xge_text_paint_span_t* pXgeSpans;
	xge_rect_t tOldClip;
	xge_rect_t tClip;
	int bOldClip = 0;
	int bClip = 0;
	int i;
	int iRet;
	int iTextWidth;
	int iTextHeight;
	float fTextX, fTextY;

    if(!pProxy || !__xuiProxyXgeDrawValid(pDraw) || iSpanCount<0 || (iSpanCount>0 && !pSpans))return XGE_ERROR_INVALID_ARGUMENT;
    iRet=__xuiProxyXgeTextDesc(pTextItem,&tDesc);if(iRet!=XGE_OK)return iRet;
    if(pTextItem->iFlags & XUI_TEXT_SHAPE_RANGE)return __xuiProxyXgeTextRangeDraw(pProxy,pDraw,pTextItem,tRect,iColor,iFlags,pSpans,iSpanCount);
    if(!tDesc.iTextSize || tRect.fW<=0 || tRect.fH<=0 || !XGE_COLOR_GET_A(iColor))return XGE_OK;
    /* Direction comes solely from the shared item, never from paint flags. */
    iFlags=(iFlags & ~XUI_TEXT_RTL) | ((pTextItem->iFlags & XUI_TEXT_SHAPE_RTL)?XUI_TEXT_RTL:0);
	pXgeSpans = sSmall;
	if ( iSpanCount > (int)(sizeof(sSmall) / sizeof(sSmall[0])) ) {
		pXgeSpans = (xge_text_paint_span_t*)xrtMalloc(sizeof(*pXgeSpans) * (size_t)iSpanCount);
		if ( pXgeSpans == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iSpanCount; i++ ) {
		pXgeSpans[i].iSize = sizeof(*pXgeSpans);
		pXgeSpans[i].iStart = pSpans[i].iStart;
		pXgeSpans[i].iEnd = pSpans[i].iEnd;
		pXgeSpans[i].iColor = pSpans[i].iColor;
	}
	memset(&tRun,0,sizeof(tRun));
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet == XGE_OK ) {
		iTextWidth = xuiInternalPixelCeil(tRun.fWidth);
		iTextHeight = xuiInternalPixelCeil(tRun.fHeight);
		if ( (iFlags & XUI_TEXT_CLIP) != 0 ) {
			tOldClip = xgeClipGet();
			bOldClip = tOldClip.fW > 0.0f && tOldClip.fH > 0.0f;
			tClip = __xuiProxyXgeRect(tRect);
			if ( bOldClip ) {
				float fLeft = fmaxf(tClip.fX, tOldClip.fX);
				float fTop = fmaxf(tClip.fY, tOldClip.fY);
				float fRight = fminf(tClip.fX + tClip.fW, tOldClip.fX + tOldClip.fW);
				float fBottom = fminf(tClip.fY + tClip.fH, tOldClip.fY + tOldClip.fH);
				tClip.fX = fLeft;
				tClip.fY = fTop;
				tClip.fW = fRight - fLeft;
				tClip.fH = fBottom - fTop;
			}
			if ( tClip.fW > 0.0f && tClip.fH > 0.0f ) {
				xgeFlush();
				xgeClipSet(tClip);
				bClip = 1;
			}
		}
		/* XUI rectangles are integer half-open pixel bounds. Align the rasterized
		 * pixel extent, not the fractional logical advance: snapping a fractional
		 * origin afterwards can otherwise push the last glyph column outside the
		 * unchanged right or bottom clip edge. */
		fTextX = (float)tRect.fX + pTextItem->fDrawOffsetX;
		fTextY = (float)tRect.fY;
		if ( (iFlags & XUI_TEXT_ALIGN_RIGHT) == XUI_TEXT_ALIGN_RIGHT )
			fTextX += (float)(tRect.fW - iTextWidth);
		else if ( iFlags & XUI_TEXT_ALIGN_CENTER )
			fTextX += (float)xuiInternalPixelFloor((float)(tRect.fW - iTextWidth) * 0.5f);
		if ( (iFlags & XUI_TEXT_ALIGN_BOTTOM) == XUI_TEXT_ALIGN_BOTTOM )
			fTextY += (float)(tRect.fH - iTextHeight);
		else if ( iFlags & XUI_TEXT_ALIGN_MIDDLE )
			fTextY += (float)xuiInternalPixelFloor((float)(tRect.fH - iTextHeight) * 0.5f);
		if ( (iFlags & XUI_TEXT_CLIP) == 0 || bClip ) {
			/* Match the existing clipped-text submission boundaries even
			 * when the caller's viewport clip needs no item-edge clip. */
			if ( (iFlags & XUI_TEXT_CLIP) == 0 ) xgeFlush();
			xgeGlyphRunDrawSpans(&tRun, fTextX, fTextY, iColor,
				XGE_DRAW_SCREEN_SPACE | (pTextItem->fDrawOffsetX!=0 ? XGE_DRAW_TEXT_SUBPIXEL_X : 0), pXgeSpans, iSpanCount);
			if ( iFlags & XUI_TEXT_UNDERLINE ) {
				xge_font_metrics_t metrics; int first = 0;
				if ( xgeFontGetMetrics(tDesc.pFont, &metrics) == XGE_OK ) {
					float baseline = fTextY + tRun.fAscent;
					while ( first < tRun.iGlyphCount ) {
						float width = 0;
						while ( first < tRun.iGlyphCount && !(tRun.pGlyphs[first].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) ) width += tRun.pGlyphs[first++].fAdvanceX;
						xgeShapeLinePx(fTextX,baseline+metrics.fUnderlinePosition,fTextX+width,baseline+metrics.fUnderlinePosition,metrics.fUnderlineThickness,iColor);
						if ( first < tRun.iGlyphCount ) first++;
						baseline += tRun.fLineHeight;
					}
				}
			}
			if ( (iFlags & XUI_TEXT_CLIP) == 0 ) xgeFlush();
		}
		if ( bClip ) {
			xgeFlush();
			if ( bOldClip ) xgeClipSet(tOldClip);
			else xgeClipClear();
		}
		xgeGlyphRunFree(&tRun);
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	if ( pXgeSpans != sSmall ) xrtFree(pXgeSpans);
	return iRet;
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
	if(!pProxy)return;
	if(xuiInternalProxyResourceManaged(pSurface,XUI_OWNED_SURFACE)){
		bool consumed=false;int status=xuiSurfaceReleaseOwnerChecked(pSurface,&consumed);
		if(status!=XUI_OK && !xrtGetError())xrtSetErrorKind(XERR_STATE);
		return;
	}
	if(!__xuiProxyXgeSurfaceValid(pSurface))return;
	(void)__xuiProxyXgeSurfacePayloadFinalize(pProxy,xuiInternalProxyResourcePayload(pSurface));
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

static int __xuiProxyXgeMapCursor(int iCursor)
{
	switch ( iCursor ) {
	case XUI_CURSOR_IBEAM: return XGE_CURSOR_IBEAM;
	case XUI_CURSOR_HAND: return XGE_CURSOR_HAND;
	case XUI_CURSOR_RESIZE_EW: return XGE_CURSOR_RESIZE_EW;
	case XUI_CURSOR_RESIZE_NS: return XGE_CURSOR_RESIZE_NS;
	case XUI_CURSOR_RESIZE_NESW: return XGE_CURSOR_RESIZE_NESW;
	case XUI_CURSOR_RESIZE_NWSE: return XGE_CURSOR_RESIZE_NWSE;
	case XUI_CURSOR_MOVE: return XGE_CURSOR_MOVE;
	case XUI_CURSOR_NOT_ALLOWED: return XGE_CURSOR_NOT_ALLOWED;
	case XUI_CURSOR_ARROW:
	default: return XGE_CURSOR_ARROW;
	}
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

static void __xuiProxyXgeImeFocusChanged(xui_context context)
{
	if (g_xuiProxyXgeImeTextContext == context)
		xgeInternalImeSetFocusOwner(context->iImeOwner);
}

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

static void __xuiProxyXgeApplyInteractionPolicy(xui_context pContext)
{
	if ( (pContext == NULL) || pContext->bInteractionPolicyUserSet || pContext->bInteractionPolicyPlatformSet ) return;
#ifdef _WIN32
	{
		xui_interaction_policy_t tPolicy;
		UINT iLines = 0;
		UINT iHoverTime = 0;
		UINT iBlinkTime;

		memset(&tPolicy, 0, sizeof(tPolicy));
		tPolicy.iSize = sizeof(tPolicy);
		tPolicy.fDoubleClickSeconds = 0.50f;
		tPolicy.iDoubleClickWidth = 4;
		tPolicy.iDoubleClickHeight = 4;
		tPolicy.iDragWidth = 4;
		tPolicy.iDragHeight = 4;
		tPolicy.iWheelScrollLines = 3;
		tPolicy.fTooltipInitialDelay = 0.35f;
		tPolicy.fCaretBlinkSeconds = 0.53f;
		tPolicy.fDoubleClickSeconds = (float)GetDoubleClickTime() / 1000.0f;
		tPolicy.iDoubleClickWidth = GetSystemMetrics(SM_CXDOUBLECLK);
		tPolicy.iDoubleClickHeight = GetSystemMetrics(SM_CYDOUBLECLK);
		tPolicy.iDragWidth = GetSystemMetrics(SM_CXDRAG);
		tPolicy.iDragHeight = GetSystemMetrics(SM_CYDRAG);
		if ( SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &iLines, 0) && (iLines != WHEEL_PAGESCROLL) ) {
			tPolicy.iWheelScrollLines = (iLines <= 100u) ? (int)iLines : 3;
		}
		if ( SystemParametersInfoA(SPI_GETMOUSEHOVERTIME, 0, &iHoverTime, 0) && (iHoverTime <= 60000u) ) {
			tPolicy.fTooltipInitialDelay = (float)iHoverTime / 1000.0f;
		}
		iBlinkTime = GetCaretBlinkTime();
		if ( iBlinkTime <= 10000u ) tPolicy.fCaretBlinkSeconds = (float)iBlinkTime / 1000.0f;
		pContext->tInteractionPolicy = tPolicy;
	}
#endif
	pContext->bInteractionPolicyPlatformSet = 1;
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

#if XUI_ENABLE_CODE_EDIT
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


#endif
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

#if XUI_ENABLE_POPUP && XUI_ENABLE_LIST_VIEW
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
		     pProxy->textMeasure(pProxy, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pFont, .sText=arrItems[i], .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT}, &tMeasure) == XUI_OK &&
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


#endif
static void __xuiProxyXgeDetachIme(xui_context pContext)
{
	if ( g_xuiProxyXgeImeTextContext == pContext ) {
		(void)xgeImeSetCandidatePresenterReady(0);
		(void)xgeImeSetTextClient(NULL);
		pContext->onImeFocusChanged = NULL;
		xgeInternalImeSetFocusOwner(0);
		g_xuiProxyXgeImeTextContext = NULL;
	}
	if ( g_xuiProxyXgeImeUi.pContext == pContext ) {
		memset(&g_xuiProxyXgeImeUi, 0, sizeof(g_xuiProxyXgeImeUi));
	}
	if ( g_xuiProxyXgeInputTransform.pContext == pContext ) {
		memset(&g_xuiProxyXgeInputTransform, 0, sizeof(g_xuiProxyXgeInputTransform));
	}
	if ( g_xuiProxyXgeDragContext == pContext ) {
		(void)xgeDragEventCallbackSet(NULL, NULL);
		g_xuiProxyXgeDragContext = NULL;
        (void)__xuiProxyXgeDragDataClear(pContext,XUI_OK);
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
#if XUI_ENABLE_CODE_EDIT
	xui_code_selection_model pSelection;
#endif
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
	uint64_t iDocumentRevision = 0;
	uint64_t iOwner;

	(void)pUser;
	pContext = g_xuiProxyXgeImeTextContext;
	if ( pContext == NULL || pSnapshot == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	pDocument = NULL;
#if XUI_ENABLE_CODE_EDIT
	pSelection = NULL;
#endif
	pFocus = xuiGetFocusWidget(pContext);
	iOwner = pContext->iImeOwner;
	if ( pFocus == NULL || xuiWidgetGetImeMode(pFocus) == XUI_IME_DISABLED ) return XGE_ERROR_NOT_INITIALIZED;
	sText = "";
	iDocumentLength = 0;
	iWindowStart = 0;
	iWindowEnd = 0;
	iLocalStart = 0;
	iLocalEnd = 0;
	iStart = 0;
	iEnd = 0;
	if ( 0 ) {}
#if XUI_ENABLE_INPUT
	else if ( xuiWidgetIsType(pFocus, xuiInputGetType(pContext)) ) {
		sText = xuiInputGetText(pFocus);
		if ( xuiInputGetSelection(pFocus, &iStart, &iEnd) != XUI_OK ) return XGE_ERROR;
		iDocumentLength = (sText != NULL) ? (int)strlen(sText) : 0;
	}
#endif
#if XUI_ENABLE_TEXT_EDIT
	else if ( xuiWidgetIsType(pFocus, xuiTextEditGetType(pContext)) ) {
		sText = xuiTextEditGetText(pFocus);
		if ( xuiTextEditGetSelection(pFocus, &iStart, &iEnd) != XUI_OK ) return XGE_ERROR;
		iDocumentLength = (sText != NULL) ? (int)strlen(sText) : 0;
	}
#endif
#if XUI_ENABLE_CODE_EDIT
	else if ( xuiWidgetIsType(pFocus, xuiCodeEditGetType(pContext)) ) {
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
	}
#endif
	#if XUI_ENABLE_DOCUMENT_EDITOR
	else if (xuiWidgetIsType(pFocus, xuiDocumentEditorGetType(pContext))) {
		iRet = xuiInternalDocumentImeText(pFocus, &sText, &iDocumentLength,
			&iStart, &iEnd, &iDocumentRevision);
		if (iRet != XUI_OK) return iRet == XUI_ERROR_OUT_OF_MEMORY ? XGE_ERROR_OUT_OF_MEMORY : XGE_ERROR_UNSUPPORTED;
	}
	#endif
	else {
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
	pSnapshot->iTextSize = iWindowEnd - iWindowStart;
	pSnapshot->iSelectionStart = iLocalStart;
	pSnapshot->iSelectionEnd = iLocalEnd;
	pSnapshot->iDocumentOffset = iWindowStart;
	pSnapshot->iDocumentSize = iDocumentLength;
#if XUI_ENABLE_CODE_EDIT
	pSnapshot->iRevision = (pDocument != NULL) ? xuiCodeDocumentGetVersion(pDocument) : 0u;
#endif
	if (iDocumentRevision) pSnapshot->iRevision = (uint32_t)iDocumentRevision;
	if (pContext->iImeOwner != iOwner || xuiGetFocusWidget(pContext) != pFocus)
		return XGE_ERROR_NOT_INITIALIZED;
	xgeInternalImeSetSnapshotOwner(iOwner);
	return XGE_OK;
}

#include "xui_proxy_xge_shape_paint.inl"

static int __xuiProxyXgeTextShape(xui_proxy pProxy, const xui_text_item_t* pTextItem, xui_text_shape_t* pShape)
{
    xge_text_shape_desc_t tDesc;xge_glyph_run_t tRun={0};xui_text_cluster_t* pCluster;
    const char* sText;
    int i,iCount,iRet;
    if(!pProxy || !pShape)return XGE_ERROR_INVALID_ARGUMENT;
    memset(pShape,0,sizeof(*pShape));iRet=__xuiProxyXgeTextDesc(pTextItem,&tDesc);if(iRet!=XGE_OK)return iRet;
    if(pTextItem->iFlags & XUI_TEXT_SHAPE_RANGE)return __xuiProxyXgeTextRangeShape(pProxy,pTextItem,pShape);
    sText=tDesc.sText;
    pShape->iSize=sizeof(*pShape);pShape->iFlags=pTextItem->iFlags;pShape->iTextSize=tDesc.iTextSize;
    iRet=xgeTextShape(&tDesc,&tRun);if(iRet!=XGE_OK)return iRet;
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
			/* XGE deliberately omits CR before LF. XUI caret clusters still
			 * cover the original CRLF bytes as one zero-width control. */
			if ((pGlyph->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) && pCluster->iTextStart > 0 &&
				sText[pCluster->iTextStart] == '\n' && sText[pCluster->iTextStart - 1] == '\r')
				pCluster->iTextStart--;
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
	if ( tRun.iCaretCount > 0 ) {
		pShape->pCarets = xrtCalloc((size_t)tRun.iCaretCount, sizeof(*pShape->pCarets));
		if ( !pShape->pCarets ) {
			xrtFree(pShape->pClusters); xgeGlyphRunFree(&tRun);
			memset(pShape, 0, sizeof(*pShape)); pShape->iSize = sizeof(*pShape);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pShape->iCaretCount = tRun.iCaretCount;
		for ( i = 0; i < tRun.iCaretCount; i++ ) {
			pShape->pCarets[i].iSize = sizeof(*pShape->pCarets);
			pShape->pCarets[i].iTextOffset = (int)tRun.pCarets[i].iTextOffset;
			pShape->pCarets[i].fAdvance = tRun.pCarets[i].fAdvance;
		}
	}
	if((pTextItem->iFlags & XUI_TEXT_SHAPE_RETAIN_PAINT) && tRun.iGlyphCount){
		xui_xge_shape_paint* paint=xrtCalloc(1,sizeof(*paint));
		bool managed=xuiInternalProxyResourceManaged(pTextItem->pFont,XUI_OWNED_FONT);
		int retain=paint && managed ? xuiFontAddRef(pTextItem->pFont):XUI_OK;
		if(!paint || retain!=XUI_OK || (!managed && pTextItem->pFont->iPaintRefs==UINT_MAX)){
			xrtFree(paint);xgeGlyphRunFree(&tRun);xuiTextShapeFree(pShape);
			return retain!=XUI_OK ? retain:XUI_ERROR_OUT_OF_MEMORY;
		}
		paint->run=tRun;paint->font=pTextItem->pFont;paint->iCreatorThread=xrtThreadCurrentId();
		if(!managed)paint->font->iPaintRefs++;
		pShape->pPaint=paint;pShape->paintFree=__xuiProxyXgeShapePaintFree;
		pShape->paintFreeChecked=__xuiProxyXgeShapePaintFreeChecked;
		pShape->pPaintOwnership=&__xuiProxyXgeShapePaintOwnership;
		pShape->iPaintBytes=sizeof(*paint)+xgeGlyphRunRetainedBytes(&tRun);
	}else xgeGlyphRunFree(&tRun);
	return XUI_OK;
}

#include "xui_proxy_xge_text_range.inl"

static int __xuiProxyXgeBindImeTextClient(xui_context pContext)
{
	xge_ime_text_client_t tClient;
	xui_context previous = NULL;
	int iRet;

	if ( pContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( g_xuiProxyXgeImeTextContext != pContext ) {
		if ( g_xuiProxyXgeImeTextContext != NULL ) {
			xui_ime_composition_t cancel = {0};
			previous = g_xuiProxyXgeImeTextContext;
			xuiInternalOperationEnter(previous);
			cancel.iSize = sizeof(cancel); cancel.sText = "";
			(void)xuiInputImeCompositionEx(previous, &cancel);
			xuiInternalInputInvalidateImeOwner(previous);
			previous->onImeFocusChanged = NULL;
			(void)xgeImeSetCandidatePresenterReady(0);
			(void)xuiInternalContextSetImeDetach(previous, NULL);
		}
		g_xuiProxyXgeImeTextContext = pContext;
		pContext->onImeFocusChanged = __xuiProxyXgeImeFocusChanged;
		xuiInternalInputInvalidateImeOwner(pContext);
		__xuiProxyXgeImeFocusChanged(pContext);
		(void)xuiInternalContextSetImeDetach(pContext, __xuiProxyXgeDetachIme);
		if (previous) xuiInternalOperationLeave(previous);
		if (xuiInternalContextDestroyPending(pContext)) return XUI_OK;
		memset(&tClient, 0, sizeof(tClient));
		tClient.iSize = sizeof(tClient);
		tClient.onSnapshot = __xuiProxyXgeImeTextSnapshot;
		iRet = xgeImeSetTextClient(&tClient);
		if ( iRet != XGE_OK && iRet != XGE_ERROR_UNSUPPORTED && iRet != XGE_ERROR_NOT_INITIALIZED ) return iRet;
	}
#if XUI_ENABLE_POPUP && XUI_ENABLE_LIST_VIEW
	if ( xgeImeGetMode() == XGE_IME_MODE_FULL ) {
		iRet = __xuiProxyXgeImeCandidateEnsure(pContext);
		(void)xgeImeSetCandidatePresenterReady(iRet == XUI_OK);
		if ( iRet != XUI_OK && iRet != XUI_ERROR_NOT_INITIALIZED ) return iRet;
	} else {
		(void)xgeImeSetCandidatePresenterReady(0);
	}

#endif
	return XUI_OK;
}

static int __xuiProxyXgeImeComposition(xui_context context,
    const xui_ime_composition_t* composition)
{
#if XUI_ENABLE_DOCUMENT_EDITOR
    xui_widget focus = xuiGetFocusWidget(context);
    if (focus && xuiWidgetIsType(focus, xuiDocumentEditorGetType(context)))
        return xuiInternalInputImeEditTextComposition(context, composition);
#endif
    return xuiInputImeCompositionEx(context, composition);
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
	int bUpdateCursor;
	int iRet;

	if ( pContext == NULL || tWindowRect.fW <= 0.0f || tWindowRect.fH <= 0.0f ||
	     tViewport.iW <= 0 || tViewport.iH <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiProxyXgeApplyInteractionPolicy(pContext);
	iRet = __xuiProxyXgeBindImeTextClient(pContext);
	if ( iRet != XUI_OK ) return iRet;
	if (xuiInternalContextDestroyPending(pContext)) return XUI_OK;
	iRet = xuiDispatchPendingEvents(pContext);
	if (iRet != XUI_OK || xuiInternalContextDestroyPending(pContext)) return iRet;
	iModifiers = __xuiProxyXgeInputModifiers();
	iRet = xuiInputSetModifiers(pContext, iModifiers);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tInput, 0, sizeof(tInput));
	while ( (iRet = xgeInputEventGet(&tInput)) > 0 ) {
		/* Copy provenance before callbacks can consume another engine event. */
		uint64_t owner = xgeInternalInputImeOwner();
		if ((tInput.iFlags & XGE_INPUT_EVENT_FLAG_NATIVE_IME) &&
			(tInput.iType == XGE_EVENT_IME_START || tInput.iType == XGE_EVENT_IME_UPDATE ||
			 tInput.iType == XGE_EVENT_IME_COMMIT || tInput.iType == XGE_EVENT_IME_END) &&
			(!owner || owner != pContext->iImeOwner)) continue;
		bUpdateCursor = 0;
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
			bUpdateCursor = 1;
			break;
		case XGE_EVENT_MOUSE_DOWN:
			iRet = xuiInputPointerDown(pContext, iX, iY, tInput.iButton, tInput.iButtons);
			bUpdateCursor = 1;
			break;
		case XGE_EVENT_MOUSE_UP:
			iRet = xuiInputPointerUp(pContext, iX, iY, tInput.iButton, tInput.iButtons);
			bUpdateCursor = 1;
			break;
		case XGE_EVENT_MOUSE_WHEEL:
			iRet = xuiInputPointerWheel(pContext, iX, iY, tInput.fDX, tInput.fDY, tInput.iButtons);
			bUpdateCursor = 1;
			break;
		case XGE_EVENT_MOUSE_LEAVE:
			iRet = xuiInputPointerLeave(pContext);
			break;
		case XGE_EVENT_WINDOW_BLUR:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = "";
			iRet = __xuiProxyXgeImeComposition(pContext, &tComposition);
			xuiInternalInputInvalidateImeOwner(pContext);
			if (iRet == XUI_OK) iRet = xuiInputCancelAllPointers(pContext);
			if ( iRet == XUI_OK ) iRet = xuiInputPointerLeave(pContext);
			break;
		case XGE_EVENT_WINDOW_FOCUS:
			iRet = xuiInputSetModifiers(pContext, 0u);
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
			tComposition.iAttribute = tInput.iAttribute;
			iRet = __xuiProxyXgeImeComposition(pContext, &tComposition);
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
			tComposition.iAttribute = tInput.iAttribute;
			iRet = __xuiProxyXgeImeComposition(pContext, &tComposition);
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
			tComposition.iAttribute = tInput.iAttribute;
			iRet = __xuiProxyXgeImeComposition(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_END:
			memset(&tComposition, 0, sizeof(tComposition));
			tComposition.iSize = sizeof(tComposition);
			tComposition.sText = "";
			iRet = __xuiProxyXgeImeComposition(pContext, &tComposition);
			break;
		case XGE_EVENT_IME_CANDIDATE_START:
		case XGE_EVENT_IME_CANDIDATE_UPDATE:
#if XUI_ENABLE_POPUP && XUI_ENABLE_LIST_VIEW
			iRet = __xuiProxyXgeImeCandidateSync(pContext);
			if ( iRet != XUI_OK ) return iRet;
#endif
			break;
		case XGE_EVENT_IME_CANDIDATE_END:
#if XUI_ENABLE_POPUP && XUI_ENABLE_LIST_VIEW
			__xuiProxyXgeImeCandidateClose(pContext);
#endif
			break;
		default:
			break;
		}
		if ( iRet != XUI_OK ) return iRet;
		if ( tInput.iType == XGE_EVENT_MOUSE_LEAVE || tInput.iType == XGE_EVENT_WINDOW_BLUR ) {
			(void)xgeSetCursor(XGE_CURSOR_ARROW);
		} else if ( bUpdateCursor != 0 ) {
			(void)xgeSetCursor(__xuiProxyXgeMapCursor(xuiQueryCursor(pContext, iX, iY)));
		}
		iRet = xuiDispatchPendingEvents(pContext);
		if ( iRet != XUI_OK ) return iRet;
		if (xuiInternalContextDestroyPending(pContext)) return XUI_OK;
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

XUI_API int xuiProxyXgePumpInputRect(xui_context context,xui_rect_t window)
{
    if(window.fW<=0 || window.fH<=0)return XUI_ERROR_INVALID_ARGUMENT;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;
    uint64_t uiThread=xgeInternalTransferThreadId(),current=xrtThreadCurrentId();
    /* Owned CPU contexts may measure/shape before a window exists, but must
     * not install a raw GUI back-link before its dispatch thread is known.
     * Legacy raw manual/offscreen callers keep their explicit same-thread C
     * registration contract; they are not a safe cross-thread weak handle. */
    if(uiThread && uiThread!=current){status=XUI_ERROR_BUSY;goto finish;}
    if(context->pOwnedProxy && !uiThread){status=XUI_ERROR_NOT_INITIALIZED;goto finish;}
    xui_size_t viewport=xuiGetViewportSize(context);
    if(g_xuiProxyXgeDragContext!=context){
        if(g_xuiProxyXgeDragRevision==UINT64_MAX){status=XUI_ERROR_INVALID_STATE;goto finish;}
        status=__xuiProxyXgeDragDataClear(context,XUI_OK);if(status!=XUI_OK)goto finish;
        if(context->bCloseRequested || context->bDestroyPending || context->bDestroying){status=XUI_ERROR_INVALID_STATE;goto finish;}
        status=xgeDragEventCallbackSetCheckedOwned(__xuiProxyXgeDragEvent,context,NULL);if(status!=XGE_OK)goto finish;
        g_xuiProxyXgeDragContext=context;++g_xuiProxyXgeDragRevision;
    }
    g_xuiProxyXgeInputTransform.pContext=context;g_xuiProxyXgeInputTransform.tWindowRect=window;
    g_xuiProxyXgeInputTransform.tViewport=viewport;g_xuiProxyXgeInputTransform.bValid=1;
	(void)xuiInternalNativeAccessibilityBind(context,xgePlatformNativeHandle(),window);
    status=xuiInternalContextDestroyPending(context) ? XUI_OK:__xuiProxyXgePumpQueuedInput(context,window,viewport);
finish:
    {xerror* first=xrtTakeError();status=xuiInternalCallScopeEndOutcome(&scope,status,&first);xrtSetErrorTake(first);}
    return status;
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
	tProxy.clipboardSetItemsWithTransfer=__xuiProxyXgeClipboardSetItemsWithTransfer;
	tProxy.clipboardGetData = __xuiProxyXgeClipboardGetData;
	tProxy.imeGetEnabled = __xuiProxyXgeImeGetEnabled;
	tProxy.imeSetEnabled = __xuiProxyXgeImeSetEnabled;
	tProxy.imeSetCandidateRect = __xuiProxyXgeImeSetCandidateRect;
	tProxy.surfaceCreate = __xuiProxyXgeSurfaceCreate;
	tProxy.surfaceCreateRGBA = __xuiProxyXgeSurfaceCreateRGBA;
	tProxy.zstdDecompress = __xuiProxyXgeZstdDecompress;
	tProxy.surfaceLoadFile = __xuiProxyXgeSurfaceLoadFile;
	tProxy.surfaceLoadMemory = __xuiProxyXgeSurfaceLoadMemory;
#if XGE_ENABLE_SVG
	tProxy.surfaceLoadSvgFile = __xuiProxyXgeSurfaceLoadSvgFile;
#endif
#if XGE_ENABLE_SVG
	tProxy.surfaceLoadSvgMemory = __xuiProxyXgeSurfaceLoadSvgMemory;
#endif
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
	tProxy.fontLoadSystem = __xuiProxyXgeFontLoadSystem;
	tProxy.fontGetMetrics = __xuiProxyXgeFontGetMetrics;
	tProxy.fontDestroy = __xuiProxyXgeFontDestroy;
	tProxy.textMeasure = __xuiProxyXgeTextMeasure;
	tProxy.textDraw = __xuiProxyXgeTextDraw;
	tProxy.drawBegin = __xuiProxyXgeManualDrawBegin;
	tProxy.drawEnd = __xuiProxyXgeDrawEnd;
	tProxy.drawClearRect = __xuiProxyXgeDrawClearRect;
	tProxy.drawSurface = __xuiProxyXgeDrawSurface;
	tProxy.drawSurfaceQuad = __xuiProxyXgeDrawSurfaceQuad;
	tProxy.drawMeshTriangles = __xuiProxyXgeDrawMeshTriangles;
#if XGE_ENABLE_SHAPE_EX
	tProxy.drawPath = __xuiProxyXgeDrawPath;
#endif
#if XGE_ENABLE_SHAPE_EX
	tProxy.drawSvgPath = __xuiProxyXgeDrawSvgPath;
#endif
	tProxy.drawPoint = __xuiProxyXgeDrawPoint;
	tProxy.drawLine = __xuiProxyXgeDrawLine;
	tProxy.drawTriangleFill = __xuiProxyXgeDrawTriangleFill;
	tProxy.drawTriangleStroke = __xuiProxyXgeDrawTriangleStroke;
	tProxy.drawRectFill = __xuiProxyXgeDrawRectFill;
	tProxy.drawRectStroke = __xuiProxyXgeDrawRectStroke;
	tProxy.drawCircleFill = __xuiProxyXgeDrawCircleFill;
	tProxy.drawCircleStroke = __xuiProxyXgeDrawCircleStroke;
	tProxy.drawText = __xuiProxyXgeDrawText;
	tProxy.drawTextSpans = __xuiProxyXgeDrawTextSpans;
	tProxy.drawClipGet = __xuiProxyXgeDrawClipGet;
	tProxy.drawClipSet = __xuiProxyXgeDrawClipSet;
	tProxy.drawClipClear = __xuiProxyXgeDrawClipClear;
	tProxy.textShape = __xuiProxyXgeTextShape;
	tProxy.drawTextShapeRange = __xuiProxyXgeDrawTextShapeRange;
	tProxy.textShapeRangeMeasure = __xuiProxyXgeTextShapeRangeMeasure;
	tProxy.drawTextShapeRangeSpans = __xuiProxyXgeDrawTextShapeRangeSpans;
	tProxy.fontCreateSized = __xuiProxyXgeFontCreateSized;
	tProxy.clockSeconds = __xuiProxyXgeClockSeconds;
	tProxy.requestFrame = __xuiProxyXgeRequestFrame;
	return tProxy;
}

int xuiProxyXgeCreate(xui_proxy* output)
{
	xui_proxy_t source=xuiProxyXge();
	return xuiInternalProxyCreateOwned(&source,NULL,true,0,output);
}

#endif
