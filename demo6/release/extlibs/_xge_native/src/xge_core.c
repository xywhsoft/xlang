static int __xgeSceneFrame(void);
static void __xgeSceneClear(void);
static void __xgeTextureUploadQueueFree(void);
static void __xgeFrameStatsRecordTime(double fSeconds);
static void __xgeRenderCommandReset(void);
static void __xgeRenderCommandUnit(void);
static int __xgeRenderCommandFlush(void);

int xgeInit(const xge_desc_t* pDesc)
{
	xge_desc_t objDesc;

	if ( g_xge.bInitialized ) {
		return XGE_ERROR_ALREADY_INITIALIZED;
	}

	__xgeConfigureGraphicsProcessStartup();
	memset(&g_xge, 0, sizeof(g_xge));
	memset(&objDesc, 0, sizeof(objDesc));

	if ( pDesc != NULL ) {
		objDesc = *pDesc;
	}
	if ( objDesc.iWidth <= 0 ) {
		objDesc.iWidth = 800;
	}
	if ( objDesc.iHeight <= 0 ) {
		objDesc.iHeight = 600;
	}
	if ( objDesc.sTitle == NULL ) {
		objDesc.sTitle = "XGE";
	}
	if ( (objDesc.iRunMode != XGE_RUN_GAME_LOOP) && (objDesc.iRunMode != XGE_RUN_MANUAL) ) {
		objDesc.iRunMode = XGE_RUN_GAME_LOOP;
	}
	if ( objDesc.iTargetFPS <= 0 ) {
		objDesc.iTargetFPS = 60;
	}

	if ( xrtInit() == NULL ) {
		__xgeLogError("core", "xrtInit failed.");
		return XGE_ERROR_BACKEND_FAILED;
	}

	g_xge.objDesc = objDesc;
	g_xge.iWidth = objDesc.iWidth;
	g_xge.iHeight = objDesc.iHeight;
	g_xge.iWindowWidth = objDesc.iWidth;
	g_xge.iWindowHeight = objDesc.iHeight;
	g_xge.iFramebufferWidth = objDesc.iWidth;
	g_xge.iFramebufferHeight = objDesc.iHeight;
	g_xge.fDpiScale = 1.0f;
	g_xge.tPlatformRuntime.iWindowWidth = objDesc.iWidth;
	g_xge.tPlatformRuntime.iWindowHeight = objDesc.iHeight;
	g_xge.tPlatformRuntime.iFramebufferWidth = objDesc.iWidth;
	g_xge.tPlatformRuntime.iFramebufferHeight = objDesc.iHeight;
	g_xge.tPlatformRuntime.fDpiScale = 1.0f;
	g_xge.iClearColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	g_xge.iBlend = XGE_BLEND_ALPHA;
	g_xge.fDelta = 1.0f / 60.0f;
	g_xge.fStartTime = xrtTimer();
	g_xge.fFPSLastTime = g_xge.fStartTime;
	g_xge.iFPS = objDesc.iTargetFPS;
	g_xge.tPlatformBackend = xgePlatformBackendDefault();
	g_xge.tGraphicsBackend = xgeGraphicsBackendDefault();
	g_xge.tGpuCaps.iBackend = XGE_GPU_BACKEND_OPENGL33;
	g_xge.iSceneUpdateMode = XGE_UPDATE_VARIABLE;
	g_xge.iSceneMaxUpdates = 1;
	g_xge.fSceneFixedStep = 1.0f / 60.0f;
	g_xge.tCamera = xgeCameraDefault((float)objDesc.iWidth, (float)objDesc.iHeight);
#ifndef XGE_NO_AUDIO
	g_xge.tAudioListener.tForward.fZ = -1.0f;
	g_xge.tAudioListener.tUp.fY = 1.0f;
#endif
	g_xge.bInitialized = 1;
	g_xge.bRunning = 1;
	g_xge.bRenderRequested = 1;
	return XGE_OK;
}

void xgeUnit(void)
{
	int i;

	if ( g_xge.bInitialized == 0 ) {
		return;
	}
	g_xge.bInitialized = 0;
	g_xge.bRunning = 0;
	__xgeSceneClear();
	__xgeRenderThreadJoin();
	__xgeRenderCommandUnit();
	__xgeShapeAutoBatchReset();
	__xgeTextureUploadQueueFree();
	xgeSvgCacheClear();
	xgeTextureFallbackClear();
	xgeFontFallbackClear();
	xgeSoundFallbackClear();
	xgeAudioUnit();
	if ( g_xge.sClipboardText != NULL ) {
		xrtFree(g_xge.sClipboardText);
		g_xge.sClipboardText = NULL;
		g_xge.iClipboardTextCapacity = 0u;
		g_xge.iClipboardSequence = 0u;
		g_xge.iClipboardCacheValid = 0;
	}
	for ( i = 0; i < g_xge.iImeQueueCount; i++ ) {
		if ( g_xge.pImeQueue[i].sText != NULL ) xrtFree(g_xge.pImeQueue[i].sText);
	}
	if ( g_xge.pImeQueue != NULL ) {
		xrtFree(g_xge.pImeQueue);
		g_xge.pImeQueue = NULL;
		g_xge.iImeQueueCount = 0;
		g_xge.iImeQueueCapacity = 0;
	}
	if ( g_xge.sImeEventText != NULL ) {
		xrtFree(g_xge.sImeEventText);
		g_xge.sImeEventText = NULL;
	}
	(void)xgeLogFlush();
	xrtUnit();
}

void xgeMemoryFree(void* pData)
{
	if ( pData != NULL ) {
		xrtFree(pData);
	}
}

int xgeRun(xge_scene_proc procFrame, void* pUser)
{
#if !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	sapp_desc objDesc;
#endif
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	UINT iOldErrorMode;
#endif

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}

	g_xge.procFrame = procFrame;
	g_xge.pFrameUser = pUser;
	if ( g_xge.objDesc.iRunMode == XGE_RUN_MANUAL ) {
		return XGE_OK;
	}
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	return XGE_OK;
#else
	objDesc = __xgeMakeSokolDesc();
#if defined(_WIN32) || defined(_WIN64)
	iOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
	SetErrorMode(iOldErrorMode | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
#endif
	sapp_run(&objDesc);
#if defined(_WIN32) || defined(_WIN64)
	SetErrorMode(iOldErrorMode);
#endif
	return XGE_OK;
#endif
}

void xgeQuit(void)
{
	g_xge.bRunning = 0;
	if ( g_xge.bSokolRunning ) {
		sapp_quit();
	}
}

void xgeRenderRequest(void)
{
	__xgeRenderRequestInternal();
}

int xgeFrame(void)
{
	int iRet;
	double fFrameStart;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.objDesc.iRunMode != XGE_RUN_MANUAL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( g_xge.bRunning == 0 ) {
		return 0;
	}
	fFrameStart = xrtTimer();
	__xgeRenderCommandReset();
	__xgeShapeAutoBatchReset();
	__xgeFrameStatsBeginFrame();
	g_xge.iFrameCount++;
	g_xge.tFrameStats.iFrameCount++;
	g_xge.fDelta = 1.0f / (float)g_xge.objDesc.iTargetFPS;
	g_xge.iFPS = g_xge.objDesc.iTargetFPS;
	xgeTextureUploadFlush();
	if ( g_xge.procFrame != NULL ) {
		iRet = g_xge.procFrame(g_xge.pFrameUser);
		if ( iRet != 0 ) {
			xgeQuit();
			return iRet;
		}
	} else {
		iRet = __xgeSceneFrame();
		if ( iRet != 0 ) {
			xgeQuit();
			return iRet;
		}
	}
	__xgeInputBeginFrame();
	if ( g_xge.bRenderActive != 0 ) {
		(void)xgeFlush();
		g_xge.bRenderActive = 0;
	}
	__xgeFrameStatsRecordTime(xrtTimer() - fFrameStart);
	return 1;
}

void xgeFrameStatsReset(void)
{
	memset(&g_xge.tFrameStats, 0, sizeof(g_xge.tFrameStats));
}

xge_frame_stats_t xgeFrameStatsGet(void)
{
	xge_frame_stats_t tStats;

	memset(&tStats, 0, sizeof(tStats));
	if ( g_xge.bInitialized == 0 ) {
		return tStats;
	}
	tStats = g_xge.tFrameStats;
	tStats.iDirtyRectCount = g_xge.iDirtyRectCount;
	return tStats;
}

static int __xgeSurfaceDirtyGenerationGet(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return 0;
	}
	return g_xge.iSurfaceDirtyGeneration;
}

int xgeLogSetLevel(int iLevel)
{
	xlogger* pLogger;

	if ( __xgeLogLevelValid(iLevel) == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pLogger = xlogDefault();
	if ( pLogger == NULL ) {
		return XGE_ERROR_BACKEND_FAILED;
	}
	xlogSetLevel(pLogger, __xgeLogLevelToXrt(iLevel));
	return XGE_OK;
}

int xgeLogGetLevel(void)
{
	xlogger* pLogger;

	pLogger = xlogDefault();
	if ( pLogger == NULL ) {
		return XGE_LOG_OFF;
	}
	return __xgeLogLevelFromXrt(xlogGetLevel(pLogger));
}

int xgeLogWrite(int iLevel, const char* sTag, const char* sMessage)
{
	if ( (__xgeLogLevelValid(iLevel) == 0) || (sMessage == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeLogFormat(iLevel, sTag, "%s", sMessage);
	return XGE_OK;
}

int xgeLogFlush(void)
{
	xlogger* pLogger;

	pLogger = xlogDefault();
	if ( pLogger == NULL ) {
		return XGE_ERROR_BACKEND_FAILED;
	}
	xlogFlush(pLogger);
	return XGE_OK;
}

xge_platform_backend_t xgePlatformBackendDefault(void)
{
	xge_platform_backend_t tBackend;

	memset(&tBackend, 0, sizeof(tBackend));
	tBackend.iType = XGE_PLATFORM_BACKEND_SOKOL;
	tBackend.sName = "sokol";
	return tBackend;
}

int xgePlatformBackendSet(const xge_platform_backend_t* pBackend)
{
	if ( pBackend == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	g_xge.tPlatformBackend = *pBackend;
	if ( g_xge.tPlatformBackend.iType == XGE_PLATFORM_BACKEND_NONE ) {
		g_xge.tPlatformBackend.iType = XGE_PLATFORM_BACKEND_CUSTOM;
	}
	return XGE_OK;
}

xge_platform_backend_t xgePlatformBackendGet(void)
{
	if ( g_xge.tPlatformBackend.iType == XGE_PLATFORM_BACKEND_NONE ) {
		return xgePlatformBackendDefault();
	}
	return g_xge.tPlatformBackend;
}

static void __xgePlatformCapsCopy(char* sDst, int iDstSize, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstSize <= 0) ) {
		return;
	}
	if ( sSrc == NULL ) {
		sSrc = "";
	}
	snprintf(sDst, (size_t)iDstSize, "%s", sSrc);
}

static void __xgePlatformCapsSokolTarget(xge_platform_caps_t* pCaps)
{
	if ( pCaps == NULL ) {
		return;
	}
	pCaps->bSokol = 1;
#if defined(SOKOL_GLCORE)
	pCaps->bSokolGLCore = 1;
#endif
#if defined(SOKOL_GLES3)
	pCaps->bSokolGLES3 = 1;
#endif
#if defined(SOKOL_D3D11)
	pCaps->bSokolD3D11 = 1;
#endif
#if defined(SOKOL_METAL)
	pCaps->bSokolMetal = 1;
#endif
#if defined(SOKOL_DUMMY_BACKEND)
	pCaps->bSokolDummy = 1;
#endif
#if defined(_WIN32)
	pCaps->bSokolWindows = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "windows");
#elif defined(__EMSCRIPTEN__)
	pCaps->bSokolWeb = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "web-emscripten");
#elif defined(__ANDROID__)
	pCaps->bSokolAndroid = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "android");
#elif defined(__APPLE__)
	#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	pCaps->bSokolIOS = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "ios");
	#else
	pCaps->bSokolMacOS = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "macos");
	#endif
#elif defined(__linux__)
	#if defined(SOKOL_WAYLAND)
	pCaps->bSokolLinuxWayland = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "linux-wayland");
	#else
	pCaps->bSokolLinuxX11 = 1;
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "linux-x11");
	#endif
#else
	__xgePlatformCapsCopy(pCaps->sSokolTargetName, (int)sizeof(pCaps->sSokolTargetName), "unknown");
#endif
}

int xgePlatformCapsGet(xge_platform_caps_t* pCaps)
{
	xge_platform_backend_t tPlatform;
	xge_graphics_backend_t tGraphics;
	xge_egl_caps_t tEGLCaps;

	if ( pCaps == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pCaps, 0, sizeof(*pCaps));
	tPlatform = xgePlatformBackendGet();
	tGraphics = xgeGraphicsBackendGet();
	pCaps->iPlatformBackend = tPlatform.iType;
	pCaps->iGraphicsBackend = tGraphics.iType;
	__xgePlatformCapsCopy(pCaps->sPlatformName, (int)sizeof(pCaps->sPlatformName), tPlatform.sName);
	__xgePlatformCapsCopy(pCaps->sGraphicsName, (int)sizeof(pCaps->sGraphicsName), tGraphics.sName);
	pCaps->bRenderTarget = 1;
	pCaps->bResourceProvider = 1;
	pCaps->bThreadSafeSubmit = 1;
	pCaps->bAudio = 1;
	if ( tPlatform.iType == XGE_PLATFORM_BACKEND_SOKOL ) {
		__xgePlatformCapsSokolTarget(pCaps);
		pCaps->bWindow = 1;
		pCaps->bMouse = 1;
		pCaps->bHighDPI = 1;
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		pCaps->bTouch = 1;
#else
		pCaps->bTouch = 0;
#endif
		pCaps->bKeyboard = 1;
		pCaps->bTextInput = 1;
		pCaps->bGamepad = 1;
		return XGE_OK;
	}
	if ( tPlatform.iType == XGE_PLATFORM_BACKEND_MINIPROGRAM ) {
		pCaps->bWindow = 1;
		pCaps->bOffscreen = 0;
		pCaps->bMiniProgram = 1;
		pCaps->bWASM = 1;
		pCaps->bHighDPI = 1;
		pCaps->bTouch = 1;
		pCaps->bMouse = 0;
		pCaps->bKeyboard = 1;
		pCaps->bTextInput = 1;
		pCaps->bGamepad = 0;
		return XGE_OK;
	}
	if ( tPlatform.iType == XGE_PLATFORM_BACKEND_EGL ) {
		memset(&tEGLCaps, 0, sizeof(tEGLCaps));
		(void)xgeEGLCapsGet(&tEGLCaps);
		pCaps->bWindow = 0;
		pCaps->bOffscreen = tEGLCaps.bOffscreen;
		pCaps->bPBuffer = tEGLCaps.bPBuffer;
		pCaps->bSurfaceless = tEGLCaps.bSurfaceless;
		pCaps->bBoardLinux = (tEGLCaps.bBoardLinux != 0) && (tPlatform.sName != NULL) && (strcmp(tPlatform.sName, "egl-board-linux") == 0);
		pCaps->bTouch = 0;
		pCaps->bMouse = 0;
		pCaps->bKeyboard = 0;
		pCaps->bTextInput = 0;
		pCaps->bGamepad = 0;
		return XGE_OK;
	}
	return XGE_OK;
}

int xgePlatformRuntimeGet(xge_platform_runtime_t* pRuntime)
{
	if ( pRuntime == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pRuntime, 0, sizeof(*pRuntime));
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	*pRuntime = g_xge.tPlatformRuntime;
	pRuntime->bRunning = g_xge.bSokolRunning ? 1 : 0;
	pRuntime->iWindowWidth = (g_xge.iWindowWidth > 0) ? g_xge.iWindowWidth : g_xge.iWidth;
	pRuntime->iWindowHeight = (g_xge.iWindowHeight > 0) ? g_xge.iWindowHeight : g_xge.iHeight;
	pRuntime->iFramebufferWidth = (g_xge.iFramebufferWidth > 0) ? g_xge.iFramebufferWidth : g_xge.iWidth;
	pRuntime->iFramebufferHeight = (g_xge.iFramebufferHeight > 0) ? g_xge.iFramebufferHeight : g_xge.iHeight;
	pRuntime->fDpiScale = (g_xge.fDpiScale > 0.0f) ? g_xge.fDpiScale : 1.0f;
	return XGE_OK;
}

xge_graphics_backend_t xgeGraphicsBackendDefault(void)
{
	xge_graphics_backend_t tBackend;

	memset(&tBackend, 0, sizeof(tBackend));
	tBackend.iType = XGE_GPU_BACKEND_OPENGL33;
	tBackend.sName = "opengl33";
	return tBackend;
}

int xgeGraphicsBackendSet(const xge_graphics_backend_t* pBackend)
{
	if ( pBackend == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	g_xge.tGraphicsBackend = *pBackend;
	if ( g_xge.tGraphicsBackend.iType == XGE_GPU_BACKEND_NONE ) {
		g_xge.tGraphicsBackend.iType = XGE_GPU_BACKEND_CUSTOM;
	}
	g_xge.tGpuCaps.iBackend = g_xge.tGraphicsBackend.iType;
	return XGE_OK;
}

xge_graphics_backend_t xgeGraphicsBackendGet(void)
{
	if ( g_xge.tGraphicsBackend.iType == XGE_GPU_BACKEND_NONE ) {
		return xgeGraphicsBackendDefault();
	}
	return g_xge.tGraphicsBackend;
}

static void __xgeGpuCapsCopy(char* sDst, int iDstSize, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstSize <= 0) ) {
		return;
	}
	if ( sSrc == NULL ) {
		sSrc = "";
	}
	snprintf(sDst, (size_t)iDstSize, "%s", sSrc);
}

int xgeGpuCapsGet(xge_gpu_caps_t* pCaps)
{
	const char* sVendor;
	const char* sRenderer;
	const char* sVersion;
	const char* sShading;
	int iMaxTextureSize;
	int iBackend;

	if ( pCaps == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pCaps, 0, sizeof(*pCaps));
	iBackend = g_xge.tGpuCaps.iBackend != XGE_GPU_BACKEND_NONE ? g_xge.tGpuCaps.iBackend : XGE_GPU_BACKEND_OPENGL33;
	pCaps->iBackend = iBackend;
	if ( iBackend == XGE_GPU_BACKEND_WEBGL2 ) {
		pCaps->iMajorVersion = 2;
		pCaps->iMinorVersion = 0;
	} else if ( iBackend == XGE_GPU_BACKEND_GLES30 ) {
		pCaps->iMajorVersion = 3;
		pCaps->iMinorVersion = 0;
	} else {
		pCaps->iMajorVersion = 3;
		pCaps->iMinorVersion = 3;
	}
	sVendor = "";
	sRenderer = "";
	sVersion = "";
	sShading = "";
	iMaxTextureSize = 0;
	if ( (g_xge.bSokolRunning != 0) && (glGetString != NULL) ) {
		sVendor = (const char*)glGetString(GL_VENDOR);
		sRenderer = (const char*)glGetString(GL_RENDERER);
		sVersion = (const char*)glGetString(GL_VERSION);
		sShading = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		if ( glGetIntegerv != NULL ) {
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		}
	}
	pCaps->iMaxTextureSize = iMaxTextureSize;
	__xgeGpuCapsCopy(pCaps->sVendor, (int)sizeof(pCaps->sVendor), sVendor);
	__xgeGpuCapsCopy(pCaps->sRenderer, (int)sizeof(pCaps->sRenderer), sRenderer);
	__xgeGpuCapsCopy(pCaps->sVersion, (int)sizeof(pCaps->sVersion), sVersion);
	__xgeGpuCapsCopy(pCaps->sShadingLanguage, (int)sizeof(pCaps->sShadingLanguage), sShading);
	return XGE_OK;
}

int xgeGraphicsShaderHeaderGet(int iBackend, char* sBuffer, int iSize)
{
	return __xgeGraphicsShaderHeaderGet(iBackend, sBuffer, iSize);
}

int xgeGraphicsLibraryNameGet(int iBackend, int iIndex, char* sBuffer, int iSize)
{
	const char* sName;

	if ( (sBuffer == NULL) || (iSize <= 0) || (iIndex < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iBackend == XGE_GPU_BACKEND_NONE ) {
		iBackend = xgeGraphicsBackendGet().iType;
	}
	sName = __xgeGraphicsLibraryName(iBackend, iIndex);
	if ( sName == NULL ) {
		sBuffer[0] = 0;
		return XGE_ERROR_FILE_NOT_FOUND;
	}
	return snprintf(sBuffer, (size_t)iSize, "%s", sName);
}

int xgeGraphicsMappingGet(int iBackend, xge_graphics_mapping_t* pMapping)
{
	if ( pMapping == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iBackend == XGE_GPU_BACKEND_NONE ) {
		iBackend = xgeGraphicsBackendGet().iType;
	}
	memset(pMapping, 0, sizeof(*pMapping));
	pMapping->iBackend = iBackend;
	pMapping->bVAO = 1;
	pMapping->bFramebufferObject = 1;
	pMapping->bDepthTexture = 1;
	pMapping->bClientSideVertexArray = 0;
	pMapping->iRGBA8InternalFormat = GL_RGBA8;
	pMapping->iRGBAFormat = GL_RGBA;
	pMapping->iUnsignedByteType = GL_UNSIGNED_BYTE;
	if ( iBackend == XGE_GPU_BACKEND_WEBGL2 ) {
		pMapping->bWebGL = 1;
		pMapping->bGLES = 1;
		pMapping->bDepthTexture = 1;
	} else if ( iBackend == XGE_GPU_BACKEND_GLES30 ) {
		pMapping->bGLES = 1;
	} else {
		pMapping->bOpenGLCore = 1;
	}
	return xgeGraphicsShaderHeaderGet(iBackend, pMapping->sShaderHeader, (int)sizeof(pMapping->sShaderHeader));
}

static void __xgeFrameStatsAddDrawCall(void)
{
	g_xge.tFrameStats.iDrawCallCount++;
}

static void __xgeFrameStatsAddBatch(void)
{
	g_xge.tFrameStats.iBatchCount++;
}

static void __xgeFrameStatsBeginFrame(void)
{
	g_xge.tFrameStats.iDrawCallCount = 0;
	g_xge.tFrameStats.iBatchCount = 0;
}

static void __xgeFrameStatsRecordTime(double fSeconds)
{
	float fMs;
	int iFrameCount;
	double fNow;
	double fElapsed;

	if ( fSeconds < 0.0 ) {
		fSeconds = 0.0;
	}
	g_xge.iFPSFrameCount++;
	fNow = xrtTimer();
	fElapsed = fNow - g_xge.fFPSLastTime;
	if ( fElapsed >= 1.0 ) {
		g_xge.iFPS = (int)((double)g_xge.iFPSFrameCount / fElapsed + 0.5);
		g_xge.iFPSFrameCount = 0;
		g_xge.fFPSLastTime = fNow;
	}
	fMs = (float)(fSeconds * 1000.0);
	iFrameCount = g_xge.tFrameStats.iFrameCount;
	g_xge.tFrameStats.fFrameTimeMs = fMs;
	if ( iFrameCount <= 1 ) {
		g_xge.tFrameStats.fFrameTimeAvgMs = fMs;
		g_xge.tFrameStats.fFrameTimeMaxMs = fMs;
		return;
	}
	g_xge.tFrameStats.fFrameTimeAvgMs = ((g_xge.tFrameStats.fFrameTimeAvgMs * (float)(iFrameCount - 1)) + fMs) / (float)iFrameCount;
	if ( fMs > g_xge.tFrameStats.fFrameTimeMaxMs ) {
		g_xge.tFrameStats.fFrameTimeMaxMs = fMs;
	}
}

static int __xgeSceneCall(xge_scene pScene, xge_scene_lifecycle_proc proc)
{
	if ( (pScene == NULL) || (proc == NULL) ) {
		return XGE_OK;
	}
	return proc(pScene);
}

static int __xgeSceneEnter(xge_scene pScene)
{
	int iRet;

	if ( pScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pScene->bEntered ) {
		pScene->bPaused = 0;
		return XGE_OK;
	}
	iRet = __xgeSceneCall(pScene, pScene->onEnter);
	if ( iRet != 0 ) {
		return iRet;
	}
	pScene->bEntered = 1;
	pScene->bPaused = 0;
	return XGE_OK;
}

static int __xgeSceneLeave(xge_scene pScene)
{
	int iRet;

	if ( pScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pScene->bEntered == 0 ) {
		pScene->bPaused = 0;
		return XGE_OK;
	}
	iRet = __xgeSceneCall(pScene, pScene->onLeave);
	if ( iRet != 0 ) {
		return iRet;
	}
	pScene->bEntered = 0;
	pScene->bPaused = 0;
	return XGE_OK;
}

static int __xgeScenePause(xge_scene pScene)
{
	int iRet;

	if ( (pScene == NULL) || (pScene->bPaused != 0) ) {
		return XGE_OK;
	}
	iRet = __xgeSceneCall(pScene, pScene->onPause);
	if ( iRet != 0 ) {
		return iRet;
	}
	pScene->bPaused = 1;
	return XGE_OK;
}

static int __xgeSceneResume(xge_scene pScene)
{
	int iRet;

	if ( (pScene == NULL) || (pScene->bPaused == 0) ) {
		return XGE_OK;
	}
	iRet = __xgeSceneCall(pScene, pScene->onResume);
	if ( iRet != 0 ) {
		return iRet;
	}
	pScene->bPaused = 0;
	return XGE_OK;
}

static int __xgeSceneFree(xge_scene pScene)
{
	return __xgeSceneCall(pScene, pScene ? pScene->onFree : NULL);
}

static void __xgeSceneClear(void)
{
	xge_scene pScene;

	while ( g_xge.iSceneStackCount > 0 ) {
		pScene = g_xge.arrSceneStack[g_xge.iSceneStackCount - 1];
		g_xge.arrSceneStack[g_xge.iSceneStackCount - 1] = NULL;
		g_xge.iSceneStackCount--;
		(void)__xgeSceneLeave(pScene);
		(void)__xgeSceneFree(pScene);
	}
}

static int __xgeSceneFrame(void)
{
	xge_scene pScene;
	int iRet;
	int iUpdateCount;
	float fOldDelta;

	pScene = xgeSceneCurrent();
	if ( pScene == NULL ) {
		return XGE_OK;
	}
	if ( pScene->onUpdate != NULL ) {
		if ( g_xge.iSceneUpdateMode == XGE_UPDATE_FIXED ) {
			fOldDelta = g_xge.fDelta;
			g_xge.fSceneAccumulator += fOldDelta;
			iUpdateCount = 0;
			while ( (g_xge.fSceneAccumulator >= g_xge.fSceneFixedStep) && (iUpdateCount < g_xge.iSceneMaxUpdates) ) {
				g_xge.fDelta = g_xge.fSceneFixedStep;
				iRet = pScene->onUpdate(pScene, g_xge.fSceneFixedStep);
				if ( iRet != 0 ) {
					g_xge.fDelta = fOldDelta;
					return iRet;
				}
				g_xge.fSceneAccumulator -= g_xge.fSceneFixedStep;
				iUpdateCount++;
			}
			if ( iUpdateCount >= g_xge.iSceneMaxUpdates ) {
				g_xge.fSceneAccumulator = 0.0f;
			}
			g_xge.fDelta = fOldDelta;
		} else {
			iRet = pScene->onUpdate(pScene, g_xge.fDelta);
			if ( iRet != 0 ) {
				return iRet;
			}
		}
	}
	if ( pScene->onDraw != NULL ) {
		iRet = pScene->onDraw(pScene);
		if ( iRet != 0 ) {
			return iRet;
		}
	}
	return XGE_OK;
}

int xgeSceneSet(xge_scene pScene)
{
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	__xgeSceneClear();
	if ( pScene == NULL ) {
		return XGE_OK;
	}
	g_xge.fSceneAccumulator = 0.0f;
	g_xge.arrSceneStack[0] = pScene;
	g_xge.iSceneStackCount = 1;
	return __xgeSceneEnter(pScene);
}

int xgeScenePush(xge_scene pScene)
{
	xge_scene pCurrent;
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( pScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.iSceneStackCount >= XGE_SCENE_STACK_MAX ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pCurrent = xgeSceneCurrent();
	iRet = __xgeScenePause(pCurrent);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xge.fSceneAccumulator = 0.0f;
	g_xge.arrSceneStack[g_xge.iSceneStackCount++] = pScene;
	iRet = __xgeSceneEnter(pScene);
	if ( iRet != XGE_OK ) {
		g_xge.iSceneStackCount--;
		g_xge.arrSceneStack[g_xge.iSceneStackCount] = NULL;
		(void)__xgeSceneResume(pCurrent);
		return iRet;
	}
	return XGE_OK;
}

int xgeScenePop(void)
{
	xge_scene pScene;
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.iSceneStackCount <= 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene = g_xge.arrSceneStack[g_xge.iSceneStackCount - 1];
	g_xge.arrSceneStack[g_xge.iSceneStackCount - 1] = NULL;
	g_xge.iSceneStackCount--;
	iRet = __xgeSceneLeave(pScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSceneFree(pScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xge.fSceneAccumulator = 0.0f;
	return __xgeSceneResume(xgeSceneCurrent());
}

int xgeSceneReplace(xge_scene pScene)
{
	xge_scene pOld;
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( pScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.iSceneStackCount <= 0 ) {
		return xgeScenePush(pScene);
	}
	pOld = g_xge.arrSceneStack[g_xge.iSceneStackCount - 1];
	iRet = __xgeSceneLeave(pOld);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSceneFree(pOld);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xge.arrSceneStack[g_xge.iSceneStackCount - 1] = pScene;
	g_xge.fSceneAccumulator = 0.0f;
	return __xgeSceneEnter(pScene);
}

xge_scene xgeSceneCurrent(void)
{
	if ( (g_xge.bInitialized == 0) || (g_xge.iSceneStackCount <= 0) ) {
		return NULL;
	}
	return g_xge.arrSceneStack[g_xge.iSceneStackCount - 1];
}

int xgeSceneCount(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return 0;
	}
	return g_xge.iSceneStackCount;
}

int xgeSceneDispatchEvent(const xge_event_t* pEvent)
{
	xge_scene pScene;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( pEvent == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene = xgeSceneCurrent();
	if ( (pScene == NULL) || (pScene->onEvent == NULL) ) {
		return XGE_OK;
	}
	return pScene->onEvent(pScene, pEvent);
}

int xgeSceneUpdateStrategySet(int iMode, float fFixedStep, int iMaxUpdates)
{
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( (iMode != XGE_UPDATE_VARIABLE) && (iMode != XGE_UPDATE_FIXED) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fFixedStep <= 0.0f ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iMaxUpdates <= 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	g_xge.iSceneUpdateMode = iMode;
	g_xge.fSceneFixedStep = fFixedStep;
	g_xge.iSceneMaxUpdates = iMaxUpdates;
	g_xge.fSceneAccumulator = 0.0f;
	return XGE_OK;
}

void xgeSceneUpdateStrategyGet(int* pMode, float* pFixedStep, int* pMaxUpdates)
{
	if ( pMode != NULL ) {
		*pMode = g_xge.iSceneUpdateMode;
	}
	if ( pFixedStep != NULL ) {
		*pFixedStep = g_xge.fSceneFixedStep;
	}
	if ( pMaxUpdates != NULL ) {
		*pMaxUpdates = g_xge.iSceneMaxUpdates;
	}
}

int xgeRender(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.objDesc.iRunMode != XGE_RUN_MANUAL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( g_xge.bRunning == 0 ) {
		return 0;
	}
	return 1;
}

static xge_rect_t __xgeRectNormalize(xge_rect_t tRect)
{
	if ( tRect.fW < 0.0f ) {
		tRect.fX += tRect.fW;
		tRect.fW = -tRect.fW;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fY += tRect.fH;
		tRect.fH = -tRect.fH;
	}
	return tRect;
}

static xge_rect_t __xgeRectClampToWindow(xge_rect_t tRect)
{
	float fRight;
	float fBottom;
	float fMaxW;
	float fMaxH;

	tRect = __xgeRectNormalize(tRect);
	fMaxW = (float)g_xge.iWidth;
	fMaxH = (float)g_xge.iHeight;
	fRight = tRect.fX + tRect.fW;
	fBottom = tRect.fY + tRect.fH;
	if ( tRect.fX < 0.0f ) {
		tRect.fX = 0.0f;
	}
	if ( tRect.fY < 0.0f ) {
		tRect.fY = 0.0f;
	}
	if ( fRight > fMaxW ) {
		fRight = fMaxW;
	}
	if ( fBottom > fMaxH ) {
		fBottom = fMaxH;
	}
	tRect.fW = fRight - tRect.fX;
	tRect.fH = fBottom - tRect.fY;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return tRect;
}

static xge_rect_t __xgeRectUnion(xge_rect_t tA, xge_rect_t tB)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	xge_rect_t tRect;

	tA = __xgeRectNormalize(tA);
	tB = __xgeRectNormalize(tB);
	fLeft = (tA.fX < tB.fX) ? tA.fX : tB.fX;
	fTop = (tA.fY < tB.fY) ? tA.fY : tB.fY;
	fRight = ((tA.fX + tA.fW) > (tB.fX + tB.fW)) ? (tA.fX + tA.fW) : (tB.fX + tB.fW);
	fBottom = ((tA.fY + tA.fH) > (tB.fY + tB.fH)) ? (tA.fY + tA.fH) : (tB.fY + tB.fH);
	tRect.fX = fLeft;
	tRect.fY = fTop;
	tRect.fW = fRight - fLeft;
	tRect.fH = fBottom - fTop;
	return tRect;
}

static xge_rect_t __xgeDirtyRectUnionAll(void)
{
	xge_rect_t tRect;
	int i;

	memset(&tRect, 0, sizeof(tRect));
	if ( g_xge.iDirtyRectCount <= 0 ) {
		return tRect;
	}
	tRect = g_xge.arrDirtyRects[0];
	for ( i = 1; i < g_xge.iDirtyRectCount; i++ ) {
		tRect = __xgeRectUnion(tRect, g_xge.arrDirtyRects[i]);
	}
	return tRect;
}

static void __xgeDirtyRectClear(void)
{
	g_xge.iDirtyRectCount = 0;
	memset(g_xge.arrDirtyRects, 0, sizeof(g_xge.arrDirtyRects));
}

int xgeGetWidth(void)
{
	return g_xge.iWidth;
}

int xgeGetHeight(void)
{
	return g_xge.iHeight;
}

float xgeGetDelta(void)
{
	return g_xge.fDelta;
}

int xgeGetFPS(void)
{
	return g_xge.iFPS;
}

double xgeTimer(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return 0.0;
	}
	return xrtTimer() - g_xge.fStartTime;
}

int64_t xgeTimeNow(void)
{
	return (int64_t)xrtNow();
}

void xgeSleep(uint32_t iMilliseconds)
{
	xrtSleep(iMilliseconds);
}

void xgeSetTitle(const char* sTitle)
{
	if ( sTitle != NULL ) {
		sapp_set_window_title(sTitle);
	}
}

int xgeBegin(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.bRenderActive != 0 ) {
		return XGE_ERROR_ALREADY_INITIALIZED;
	}
	g_xge.bRenderActive = 1;
	return XGE_OK;
}

int xgeFlush(void)
{
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	xgeTextureUploadFlush();
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( (g_xge.bRenderThreadEnabled != 0) && (g_xge.bSokolRunning == 0) ) {
		iRet = __xgeRenderCommandFlushThreaded();
	} else {
		iRet = __xgeRenderCommandFlush();
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( (g_xge.bSokolRunning != 0) && (glFlush != NULL) ) {
		glFlush();
	}
	return XGE_OK;
}

int xgeRenderThreadSet(int bEnabled)
{
	xge_render_thread_caps_t tCaps;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( bEnabled == 0 ) {
		__xgeRenderThreadJoin();
		g_xge.bRenderThreadEnabled = 0;
		g_xge.bRenderThreadOwnsGLContext = 0;
		return XGE_OK;
	}
	memset(&tCaps, 0, sizeof(tCaps));
	(void)xgeRenderThreadCapsGet(&tCaps);
	if ( tCaps.bCanUseWithCurrentContext == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	g_xge.bRenderThreadEnabled = 1;
	g_xge.bRenderThreadOwnsGLContext = g_xge.bRenderThreadEGLConfigured ? 1 : 0;
	return XGE_OK;
}

int xgeRenderThreadGet(void)
{
	return g_xge.bRenderThreadEnabled;
}

static int __xgeRenderThreadEGLDescValidate(const xge_egl_desc_t* pDesc)
{
	xge_egl_caps_t tCaps;

	if ( pDesc == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pDesc->iWidth <= 0) || (pDesc->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pDesc->bPBuffer == 0) && (pDesc->bSurfaceless == 0) && (pDesc->pNativeWindow == NULL) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	memset(&tCaps, 0, sizeof(tCaps));
	(void)xgeEGLCapsGet(&tCaps);
	if ( tCaps.bCompiled == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->bPBuffer != 0) && (tCaps.bPBuffer == 0) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->bSurfaceless != 0) && (tCaps.bSurfaceless == 0) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (pDesc->pNativeWindow != NULL) && (tCaps.bNativeWindow == 0) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	return XGE_OK;
}

int xgeRenderThreadEGLSet(const xge_egl_desc_t* pDesc)
{
	xge_platform_backend_t tPlatform;
	xge_graphics_backend_t tGraphics;
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.bRenderThreadEnabled != 0 ) {
		return XGE_ERROR_ALREADY_INITIALIZED;
	}
	if ( pDesc == NULL ) {
		memset(&g_xge.tRenderThreadEGLDesc, 0, sizeof(g_xge.tRenderThreadEGLDesc));
		memset(&g_xge.tRenderThreadLastEGL, 0, sizeof(g_xge.tRenderThreadLastEGL));
		g_xge.bRenderThreadEGLConfigured = 0;
		g_xge.bRenderThreadOwnsGLContext = 0;
		return XGE_OK;
	}
	if ( g_xge.bSokolRunning != 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iRet = __xgeRenderThreadEGLDescValidate(pDesc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xge.tRenderThreadEGLDesc = *pDesc;
	memset(&g_xge.tRenderThreadLastEGL, 0, sizeof(g_xge.tRenderThreadLastEGL));
	g_xge.bRenderThreadEGLConfigured = 1;
	g_xge.bRenderThreadOwnsGLContext = 0;
	memset(&tPlatform, 0, sizeof(tPlatform));
	tPlatform.iType = XGE_PLATFORM_BACKEND_EGL;
	tPlatform.sName = pDesc->bBoardLinux ? "egl-board-linux-render-thread" : "egl-render-thread";
	xgePlatformBackendSet(&tPlatform);
	memset(&tGraphics, 0, sizeof(tGraphics));
	tGraphics.iType = XGE_GPU_BACKEND_GLES30;
	tGraphics.sName = "gles30";
	xgeGraphicsBackendSet(&tGraphics);
	return XGE_OK;
}

int xgeRenderThreadCapsGet(xge_render_thread_caps_t* pCaps)
{
	if ( pCaps == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pCaps, 0, sizeof(*pCaps));
	pCaps->bSupported = 1;
	pCaps->bEnabled = g_xge.bRenderThreadEnabled;
	pCaps->bWorkerDrain = 1;
	pCaps->bGLContextOwned = (g_xge.bRenderThreadOwnsGLContext != 0) ? 1 : 0;
	pCaps->bAsyncFlush = 0;
	pCaps->bCanUseWithCurrentContext = (g_xge.bSokolRunning == 0) ? 1 : 0;
	return XGE_OK;
}

int xgeEnd(void)
{
	int iRet;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( g_xge.bRenderActive == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeFlush();
	g_xge.bRenderActive = 0;
	return iRet;
}

void xgeClear(uint32_t iColor)
{
	float fR;
	float fG;
	float fB;
	float fA;
	xge_rect_t tRect;

	g_xge.iClearColor = iColor;
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect.fW = (float)g_xge.iWidth;
	tRect.fH = (float)g_xge.iHeight;
	xgeInvalidateRect(tRect);
	g_xge.iSurfaceDirtyGeneration++;
	if ( g_xge.bSokolRunning ) {
		(void)__xgeShapeAutoBatchFlush();
		__xgeColorToFloat(iColor, &fR, &fG, &fB, &fA);
		glClearColor(fR, fG, fB, fA);
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void xgePresent(void)
{
	if ( g_xge.bInitialized != 0 ) {
		(void)xgeFlush();
		g_xge.bRenderActive = 0;
	}
	__xgeDirtyRectClear();
}

void xgeInvalidateRect(xge_rect_t tRect)
{
	if ( g_xge.bInitialized == 0 ) {
		return;
	}
	if ( g_xge.bRenderActive == 0 ) {
		xgeRenderRequest();
	}
	tRect = __xgeRectClampToWindow(tRect);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return;
	}
	if ( g_xge.iDirtyRectCount >= XGE_DIRTY_RECT_MAX ) {
		g_xge.arrDirtyRects[0] = __xgeRectUnion(__xgeDirtyRectUnionAll(), tRect);
		g_xge.iDirtyRectCount = 1;
		return;
	}
	g_xge.arrDirtyRects[g_xge.iDirtyRectCount++] = tRect;
}

uint32_t xgeColorRGBA(int iR, int iG, int iB, int iA)
{
	return XGE_COLOR_RGBA(iR, iG, iB, iA);
}

xge_color_t xgeColorUnpack(uint32_t iColor)
{
	xge_color_t tColor;

	tColor.fR = (float)XGE_COLOR_GET_R(iColor) / 255.0f;
	tColor.fG = (float)XGE_COLOR_GET_G(iColor) / 255.0f;
	tColor.fB = (float)XGE_COLOR_GET_B(iColor) / 255.0f;
	tColor.fA = (float)XGE_COLOR_GET_A(iColor) / 255.0f;
	return tColor;
}

void xgeBlendSet(int iBlend)
{
	g_xge.iBlend = iBlend;
	__xgeBlendApply(iBlend);
}

int xgeBlendGet(void)
{
	return g_xge.iBlend;
}

void xgeDepthTestSet(int bEnabled)
{
	g_xge.bDepthTestEnabled = bEnabled ? 1 : 0;
	__xgeDepthTestApply();
}

int xgeDepthTestGet(void)
{
	return g_xge.bDepthTestEnabled;
}

xge_camera_t xgeCameraDefault(float fWidth, float fHeight)
{
	xge_camera_t tCamera;

	memset(&tCamera, 0, sizeof(tCamera));
	tCamera.tScale.fX = 1.0f;
	tCamera.tScale.fY = 1.0f;
	tCamera.tViewport.fX = 0.0f;
	tCamera.tViewport.fY = 0.0f;
	tCamera.tViewport.fW = fWidth;
	tCamera.tViewport.fH = fHeight;
	tCamera.iCoordinateMode = XGE_COORD_Y_DOWN;
	tCamera.iProjectionMode = XGE_CAMERA_ORTHO;
	return tCamera;
}

xge_camera_t xgeCameraPerspective(float fWidth, float fHeight, float fFovY, float fNearZ, float fFarZ)
{
	xge_camera_t tCamera;
	float fRadians;

	tCamera = xgeCameraDefault(fWidth, fHeight);
	tCamera.iProjectionMode = XGE_CAMERA_PERSPECTIVE;
	if ( fFovY <= 0.0f ) {
		fFovY = 60.0f;
	}
	if ( fNearZ <= 0.0f ) {
		fNearZ = 0.1f;
	}
	if ( fFarZ <= fNearZ ) {
		fFarZ = 1000.0f;
	}
	tCamera.fFovY = fFovY;
	tCamera.fNearZ = fNearZ;
	tCamera.fFarZ = fFarZ;
	fRadians = fFovY * 0.017453292519943295f;
	tCamera.fPerspectiveDistance = (fHeight * 0.5f) / tanf(fRadians * 0.5f);
	if ( tCamera.fPerspectiveDistance <= 0.0f ) {
		tCamera.fPerspectiveDistance = fHeight;
	}
	return tCamera;
}

void xgeCameraSet(const xge_camera_t* pCamera)
{
	if ( pCamera == NULL ) {
		return;
	}
	g_xge.tCamera = *pCamera;
	if ( g_xge.tCamera.tScale.fX == 0.0f ) {
		g_xge.tCamera.tScale.fX = 1.0f;
	}
	if ( g_xge.tCamera.tScale.fY == 0.0f ) {
		g_xge.tCamera.tScale.fY = 1.0f;
	}
	if ( g_xge.tCamera.iProjectionMode == XGE_CAMERA_PERSPECTIVE ) {
		if ( g_xge.tCamera.fFovY <= 0.0f ) {
			g_xge.tCamera.fFovY = 60.0f;
		}
		if ( g_xge.tCamera.fNearZ <= 0.0f ) {
			g_xge.tCamera.fNearZ = 0.1f;
		}
		if ( g_xge.tCamera.fFarZ <= g_xge.tCamera.fNearZ ) {
			g_xge.tCamera.fFarZ = 1000.0f;
		}
		if ( g_xge.tCamera.fPerspectiveDistance <= 0.0f ) {
			g_xge.tCamera.fPerspectiveDistance = (g_xge.tCamera.tViewport.fH * 0.5f) / tanf((g_xge.tCamera.fFovY * 0.017453292519943295f) * 0.5f);
		}
	}
}

xge_camera_t xgeCameraGet(void)
{
	return g_xge.tCamera;
}

xge_vec2_t xgeWorldToScreen(xge_vec2_t tPoint)
{
	xge_vec2_t tRet;
	xge_camera_t* pCamera;

	pCamera = &g_xge.tCamera;
	tRet.fX = (tPoint.fX - pCamera->tPosition.fX) * pCamera->tScale.fX;
	tRet.fY = (tPoint.fY - pCamera->tPosition.fY) * pCamera->tScale.fY;
	if ( pCamera->iCoordinateMode == XGE_COORD_CENTER ) {
		tRet.fX += pCamera->tViewport.fX + (pCamera->tViewport.fW * 0.5f);
		tRet.fY += pCamera->tViewport.fY + (pCamera->tViewport.fH * 0.5f);
	} else {
		tRet.fX += pCamera->tViewport.fX;
		tRet.fY += pCamera->tViewport.fY;
	}
	return tRet;
}

xge_vec2_t xgeScreenToWorld(xge_vec2_t tPoint)
{
	xge_vec2_t tRet;
	xge_camera_t* pCamera;
	float fX;
	float fY;

	pCamera = &g_xge.tCamera;
	fX = tPoint.fX - pCamera->tViewport.fX;
	fY = tPoint.fY - pCamera->tViewport.fY;
	if ( pCamera->iCoordinateMode == XGE_COORD_CENTER ) {
		fX -= pCamera->tViewport.fW * 0.5f;
		fY -= pCamera->tViewport.fH * 0.5f;
	}
	tRet.fX = (fX / pCamera->tScale.fX) + pCamera->tPosition.fX;
	tRet.fY = (fY / pCamera->tScale.fY) + pCamera->tPosition.fY;
	return tRet;
}

static void __xgeCameraProjectVertex(float fX, float fY, float fZ, uint32_t iFlags, float* pOutX, float* pOutY)
{
	xge_vec2_t tPoint;
	xge_camera_t* pCamera;
	float fCenterX;
	float fCenterY;
	float fDistance;
	float fScale;

	tPoint.fX = fX;
	tPoint.fY = fY;
	if ( (iFlags & XGE_DRAW_SCREEN_SPACE) == 0 ) {
		tPoint = xgeWorldToScreen(tPoint);
		pCamera = &g_xge.tCamera;
		if ( pCamera->iProjectionMode == XGE_CAMERA_PERSPECTIVE ) {
			fDistance = pCamera->fPerspectiveDistance;
			if ( fDistance <= 0.0f ) {
				fDistance = pCamera->tViewport.fH;
			}
			if ( (fDistance + fZ) > 0.0001f ) {
				fScale = fDistance / (fDistance + fZ);
				fCenterX = pCamera->tViewport.fX + (pCamera->tViewport.fW * 0.5f);
				fCenterY = pCamera->tViewport.fY + (pCamera->tViewport.fH * 0.5f);
				tPoint.fX = fCenterX + ((tPoint.fX - fCenterX) * fScale);
				tPoint.fY = fCenterY + ((tPoint.fY - fCenterY) * fScale);
			}
		}
	}
	if ( pOutX != NULL ) {
		*pOutX = tPoint.fX;
	}
	if ( pOutY != NULL ) {
		*pOutY = tPoint.fY;
	}
}

