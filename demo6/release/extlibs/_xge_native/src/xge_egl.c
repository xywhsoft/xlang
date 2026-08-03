#if defined(XGE_HAS_EGL)
	#include <EGL/egl.h>
	#ifndef EGL_OPENGL_ES3_BIT
		#define EGL_OPENGL_ES3_BIT 0x00000040
	#endif
#endif

static void __xgeEGLCapsNameCopy(char* sDst, int iDstSize, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstSize <= 0) ) {
		return;
	}
	if ( sSrc == NULL ) {
		sSrc = "";
	}
	snprintf(sDst, (size_t)iDstSize, "%s", sSrc);
}

static void __xgeEGLStageCopy(char* sDst, int iDstSize, const char* sStage)
{
	__xgeEGLCapsNameCopy(sDst, iDstSize, sStage);
}

int xgeEGLCapsGet(xge_egl_caps_t* pCaps)
{
	if ( pCaps == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pCaps, 0, sizeof(*pCaps));
#if defined(XGE_HAS_EGL)
	pCaps->bCompiled = 1;
#if defined(XGE_HAS_EGL_PBUFFER) || defined(XGE_HAS_EGL_SURFACELESS)
	pCaps->bOffscreen = 1;
#endif
#if defined(XGE_HAS_EGL_PBUFFER)
	pCaps->bPBuffer = 1;
#endif
#if defined(XGE_HAS_EGL_NATIVE_WINDOW)
	pCaps->bNativeWindow = 1;
#endif
#if defined(XGE_HAS_EGL_BOARD_LINUX)
	pCaps->bBoardLinux = 1;
#endif
#if defined(XGE_HAS_EGL_SURFACELESS)
	pCaps->bSurfaceless = 1;
#endif
	__xgeEGLCapsNameCopy(pCaps->sBackendName, (int)sizeof(pCaps->sBackendName), "egl");
	__xgeEGLStageCopy(pCaps->sLastStage, (int)sizeof(pCaps->sLastStage), "caps");
#else
	__xgeEGLCapsNameCopy(pCaps->sBackendName, (int)sizeof(pCaps->sBackendName), "egl-unavailable");
	__xgeEGLStageCopy(pCaps->sLastStage, (int)sizeof(pCaps->sLastStage), "not-compiled");
#endif
	return XGE_OK;
}

#if defined(XGE_HAS_EGL)
static int __xgeEGLFail(xge_egl_context_t* pContext, const char* sStage, int iResult)
{
	EGLint iError;
	char arrError[160];

	iError = eglGetError();
	if ( pContext != NULL ) {
		pContext->iLastError = (int)iError;
		__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), sStage);
	}
	snprintf(arrError, sizeof(arrError), "EGL failed at %s: 0x%04X", (sStage != NULL) ? sStage : "unknown", (unsigned int)iError);
	xrtSetError(arrError, false);
	__xgeLogFormat(XGE_LOG_ERROR, "egl", "%s", arrError);
	return iResult;
}

static int __xgeEGLHasExtension(EGLDisplay hDisplay, const char* sName)
{
	const char* sExtensions;
	const char* sMatch;
	size_t iNameLen;

	if ( sName == NULL ) {
		return 0;
	}
	sExtensions = eglQueryString(hDisplay, EGL_EXTENSIONS);
	if ( sExtensions == NULL ) {
		return 0;
	}
	iNameLen = strlen(sName);
	sMatch = sExtensions;
	while ( (sMatch = strstr(sMatch, sName)) != NULL ) {
		if ( ((sMatch == sExtensions) || (sMatch[-1] == ' ')) && ((sMatch[iNameLen] == 0) || (sMatch[iNameLen] == ' ')) ) {
			return 1;
		}
		sMatch += iNameLen;
	}
	return 0;
}

static int __xgeEGLCreateContext(xge_egl_context_t* pContext, const xge_egl_desc_t* pDesc)
{
	EGLDisplay hDisplay;
	EGLConfig hConfig;
	EGLSurface hSurface;
	EGLContext hContext;
	EGLint arrConfigAttribs[32];
	EGLint arrContextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	EGLint arrPBufferAttribs[] = {
		EGL_WIDTH, 1,
		EGL_HEIGHT, 1,
		EGL_NONE
	};
	EGLint iConfigCount;
	EGLint iSurfaceBits;
	int i;

	hDisplay = eglGetDisplay((pDesc->pNativeDisplay != NULL) ? (EGLNativeDisplayType)pDesc->pNativeDisplay : EGL_DEFAULT_DISPLAY);
	if ( hDisplay == EGL_NO_DISPLAY ) {
		return __xgeEGLFail(pContext, "eglGetDisplay", XGE_ERROR_BACKEND_FAILED);
	}
	if ( eglInitialize(hDisplay, NULL, NULL) == EGL_FALSE ) {
		return __xgeEGLFail(pContext, "eglInitialize", XGE_ERROR_BACKEND_FAILED);
	}
	if ( eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE ) {
		(void)__xgeEGLFail(pContext, "eglBindAPI", XGE_ERROR_BACKEND_FAILED);
		eglTerminate(hDisplay);
		return XGE_ERROR_BACKEND_FAILED;
	}
	iSurfaceBits = 0;
	if ( pDesc->bPBuffer != 0 ) {
		iSurfaceBits |= EGL_PBUFFER_BIT;
	}
	if ( pDesc->pNativeWindow != NULL ) {
		iSurfaceBits |= EGL_WINDOW_BIT;
	}
	if ( iSurfaceBits == 0 ) {
		iSurfaceBits = EGL_PBUFFER_BIT;
	}
	i = 0;
	arrConfigAttribs[i++] = EGL_SURFACE_TYPE;
	arrConfigAttribs[i++] = iSurfaceBits;
	arrConfigAttribs[i++] = EGL_RENDERABLE_TYPE;
	arrConfigAttribs[i++] = EGL_OPENGL_ES3_BIT;
	arrConfigAttribs[i++] = EGL_RED_SIZE;
	arrConfigAttribs[i++] = 8;
	arrConfigAttribs[i++] = EGL_GREEN_SIZE;
	arrConfigAttribs[i++] = 8;
	arrConfigAttribs[i++] = EGL_BLUE_SIZE;
	arrConfigAttribs[i++] = 8;
	arrConfigAttribs[i++] = EGL_ALPHA_SIZE;
	arrConfigAttribs[i++] = 8;
	arrConfigAttribs[i++] = EGL_DEPTH_SIZE;
	arrConfigAttribs[i++] = 16;
	arrConfigAttribs[i++] = EGL_STENCIL_SIZE;
	arrConfigAttribs[i++] = 0;
	arrConfigAttribs[i++] = EGL_NONE;
	if ( eglChooseConfig(hDisplay, arrConfigAttribs, &hConfig, 1, &iConfigCount) == EGL_FALSE || iConfigCount <= 0 ) {
		(void)__xgeEGLFail(pContext, "eglChooseConfig", XGE_ERROR_BACKEND_FAILED);
		eglTerminate(hDisplay);
		return XGE_ERROR_BACKEND_FAILED;
	}
	hSurface = EGL_NO_SURFACE;
	if ( pDesc->pNativeWindow != NULL ) {
		hSurface = eglCreateWindowSurface(hDisplay, hConfig, (EGLNativeWindowType)pDesc->pNativeWindow, NULL);
	} else if ( pDesc->bSurfaceless != 0 ) {
		if ( __xgeEGLHasExtension(hDisplay, "EGL_KHR_surfaceless_context") == 0 ) {
			(void)__xgeEGLFail(pContext, "EGL_KHR_surfaceless_context", XGE_ERROR_UNSUPPORTED);
			eglTerminate(hDisplay);
			return XGE_ERROR_UNSUPPORTED;
		}
	} else {
		arrPBufferAttribs[1] = pDesc->iWidth;
		arrPBufferAttribs[3] = pDesc->iHeight;
		hSurface = eglCreatePbufferSurface(hDisplay, hConfig, arrPBufferAttribs);
	}
	if ( (pDesc->bSurfaceless == 0) && (hSurface == EGL_NO_SURFACE) ) {
		(void)__xgeEGLFail(pContext, "eglCreateSurface", XGE_ERROR_BACKEND_FAILED);
		eglTerminate(hDisplay);
		return XGE_ERROR_BACKEND_FAILED;
	}
	hContext = eglCreateContext(hDisplay, hConfig, EGL_NO_CONTEXT, arrContextAttribs);
	if ( hContext == EGL_NO_CONTEXT ) {
		(void)__xgeEGLFail(pContext, "eglCreateContext", XGE_ERROR_BACKEND_FAILED);
		if ( hSurface != EGL_NO_SURFACE ) {
			eglDestroySurface(hDisplay, hSurface);
		}
		eglTerminate(hDisplay);
		return XGE_ERROR_BACKEND_FAILED;
	}
	if ( eglMakeCurrent(hDisplay, hSurface, hSurface, hContext) == EGL_FALSE ) {
		(void)__xgeEGLFail(pContext, "eglMakeCurrent", XGE_ERROR_BACKEND_FAILED);
		eglDestroyContext(hDisplay, hContext);
		if ( hSurface != EGL_NO_SURFACE ) {
			eglDestroySurface(hDisplay, hSurface);
		}
		eglTerminate(hDisplay);
		return XGE_ERROR_BACKEND_FAILED;
	}
	pContext->pDisplay = (void*)hDisplay;
	pContext->pConfig = (void*)hConfig;
	pContext->pSurface = (void*)hSurface;
	pContext->pContext = (void*)hContext;
	pContext->bInitialized = 1;
	pContext->iLastError = 0;
	__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "ready");
	return XGE_OK;
}
#endif

int xgeEGLInit(xge_egl_context_t* pContext, const xge_egl_desc_t* pDesc)
{
	xge_platform_backend_t tPlatform;
	xge_graphics_backend_t tGraphics;
	xge_egl_caps_t tCaps;

	if ( (pContext == NULL) || (pDesc == NULL) || (pDesc->iWidth <= 0) || (pDesc->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pContext, 0, sizeof(*pContext));
	pContext->iWidth = pDesc->iWidth;
	pContext->iHeight = pDesc->iHeight;
	pContext->bPBuffer = pDesc->bPBuffer ? 1 : 0;
	pContext->bSurfaceless = pDesc->bSurfaceless ? 1 : 0;
	pContext->bBoardLinux = pDesc->bBoardLinux ? 1 : 0;
	pContext->pDisplay = pDesc->pNativeDisplay;
	pContext->pSurface = pDesc->pNativeWindow;
	pContext->pUser = pDesc->pUser;
	memset(&tPlatform, 0, sizeof(tPlatform));
	tPlatform.iType = XGE_PLATFORM_BACKEND_EGL;
	tPlatform.sName = pContext->bBoardLinux ? "egl-board-linux" : "egl-offscreen";
	xgePlatformBackendSet(&tPlatform);
	memset(&tGraphics, 0, sizeof(tGraphics));
	tGraphics.iType = XGE_GPU_BACKEND_GLES30;
	tGraphics.sName = "gles30";
	xgeGraphicsBackendSet(&tGraphics);
	memset(&tCaps, 0, sizeof(tCaps));
	(void)xgeEGLCapsGet(&tCaps);
	if ( (pDesc->bSurfaceless != 0) && (tCaps.bSurfaceless == 0) ) {
		__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "surfaceless-not-compiled");
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->bPBuffer != 0) && (tCaps.bPBuffer == 0) ) {
		__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "pbuffer-not-compiled");
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->pNativeWindow != NULL) && (tCaps.bNativeWindow == 0) ) {
		__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "native-window-not-compiled");
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->bPBuffer == 0) && (pDesc->bSurfaceless == 0) && (pDesc->pNativeWindow == NULL) ) {
		__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "no-surface");
		return XGE_ERROR_UNSUPPORTED;
	}
#if defined(XGE_HAS_EGL)
	return __xgeEGLCreateContext(pContext, pDesc);
#else
	__xgeEGLStageCopy(pContext->sLastStage, (int)sizeof(pContext->sLastStage), "not-compiled");
	return XGE_ERROR_UNSUPPORTED;
#endif
}

void xgeEGLUnit(xge_egl_context_t* pContext)
{
#if defined(XGE_HAS_EGL)
	EGLDisplay hDisplay;
	EGLSurface hSurface;
	EGLContext hContext;
#endif
	if ( pContext == NULL ) {
		return;
	}
#if defined(XGE_HAS_EGL)
	if ( pContext->bInitialized != 0 ) {
		hDisplay = (EGLDisplay)pContext->pDisplay;
		hSurface = (EGLSurface)pContext->pSurface;
		hContext = (EGLContext)pContext->pContext;
		if ( hDisplay != EGL_NO_DISPLAY ) {
			eglMakeCurrent(hDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if ( hContext != EGL_NO_CONTEXT ) {
				eglDestroyContext(hDisplay, hContext);
			}
			if ( hSurface != EGL_NO_SURFACE ) {
				eglDestroySurface(hDisplay, hSurface);
			}
			eglTerminate(hDisplay);
		}
	}
#endif
	memset(pContext, 0, sizeof(*pContext));
}

int xgeEGLMakeCurrent(xge_egl_context_t* pContext)
{
	if ( pContext == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
#if defined(XGE_HAS_EGL)
	if ( eglMakeCurrent((EGLDisplay)pContext->pDisplay, (EGLSurface)pContext->pSurface, (EGLSurface)pContext->pSurface, (EGLContext)pContext->pContext) == EGL_FALSE ) {
		return __xgeEGLFail(pContext, "eglMakeCurrent", XGE_ERROR_BACKEND_FAILED);
	}
#endif
	return XGE_OK;
}

int xgeOffscreenInit(xge_offscreen pOffscreen, int iWidth, int iHeight)
{
	xge_egl_desc_t tDesc;
	int iRet;

	if ( (pOffscreen == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pOffscreen, 0, sizeof(*pOffscreen));
	pOffscreen->iWidth = iWidth;
	pOffscreen->iHeight = iHeight;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iWidth = iWidth;
	tDesc.iHeight = iHeight;
	tDesc.bPBuffer = 1;
	iRet = xgeEGLInit(&pOffscreen->tEGL, &tDesc);
	if ( (iRet != XGE_OK) && (iRet != XGE_ERROR_UNSUPPORTED) ) {
		memset(pOffscreen, 0, sizeof(*pOffscreen));
		return iRet;
	}
	if ( iRet == XGE_OK ) {
		pOffscreen->bEGLContext = 1;
	} else {
		pOffscreen->bFallbackRenderTarget = 1;
	}
	iRet = xgeRenderTargetCreate(&pOffscreen->tTarget, iWidth, iHeight);
	if ( iRet != XGE_OK ) {
		xgeEGLUnit(&pOffscreen->tEGL);
		memset(pOffscreen, 0, sizeof(*pOffscreen));
		return iRet;
	}
	pOffscreen->bActive = 1;
	return XGE_OK;
}

void xgeOffscreenUnit(xge_offscreen pOffscreen)
{
	if ( pOffscreen == NULL ) {
		return;
	}
	xgeRenderTargetFree(&pOffscreen->tTarget);
	xgeEGLUnit(&pOffscreen->tEGL);
	memset(pOffscreen, 0, sizeof(*pOffscreen));
}

xge_render_target xgeOffscreenRenderTarget(xge_offscreen pOffscreen)
{
	if ( (pOffscreen == NULL) || (pOffscreen->bActive == 0) ) {
		return NULL;
	}
	return &pOffscreen->tTarget;
}

int xgeOffscreenReadPixels(xge_offscreen pOffscreen, void* pPixels, int iStride)
{
	if ( (pOffscreen == NULL) || (pOffscreen->bActive == 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return xgeRenderTargetReadPixels(&pOffscreen->tTarget, pPixels, iStride);
}
