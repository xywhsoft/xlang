static xge_debug_stats_t __xgeDebugStatsGetValue(void)
{
	xge_debug_stats_t tStats;

	memset(&tStats, 0, sizeof(tStats));
	if ( g_xge.bInitialized == 0 ) {
		return tStats;
	}
	tStats.tFrame = xgeFrameStatsGet();
	tStats.iTextureCount = g_xge.iTextureCount;
	tStats.iTextureMemoryBytes = g_xge.iTextureMemoryBytes;
	tStats.iFontCount = g_xge.iFontCount;
	tStats.iAudioCount = g_xge.iAudioCount;
	if ( (g_xge.bSokolRunning != 0) && (glGetError != NULL) ) {
		g_xge.iLastGLError = (int)glGetError();
	}
	tStats.iLastGLError = g_xge.iLastGLError;
	return tStats;
}

int xgeDebugGetStats(xge_debug_stats_t* pStats)
{
	if ( pStats == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pStats = __xgeDebugStatsGetValue();
	return XGE_OK;
}

int xgeDebugDumpCaps(char* sBuffer, int iSize)
{
	xge_gpu_caps_t tCaps;
	xge_platform_caps_t tPlatformCaps;
	const char* sPlatform;
	const char* sGraphics;
	const char* sVendor;
	const char* sRenderer;
	const char* sVersion;
	const char* sShading;
	int iNeed;

	if ( (sBuffer == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tCaps, 0, sizeof(tCaps));
	memset(&tPlatformCaps, 0, sizeof(tPlatformCaps));
	(void)xgeGpuCapsGet(&tCaps);
	(void)xgePlatformCapsGet(&tPlatformCaps);
	sPlatform = tPlatformCaps.sPlatformName;
	sGraphics = tPlatformCaps.sGraphicsName;
	sVendor = tCaps.sVendor;
	sRenderer = tCaps.sRenderer;
	sVersion = tCaps.sVersion;
	sShading = tCaps.sShadingLanguage;
	iNeed = snprintf(sBuffer, (size_t)iSize,
		"XGE %d.%d.%d\n"
		"Platform Backend: %s\n"
		"Sokol Target: %s\n"
		"Graphics Backend: %s\n"
		"Window: %d\n"
		"Offscreen: %d\n"
		"RenderTarget: %d\n"
		"ThreadSafeSubmit: %d\n"
		"RenderThread: %d\n"
		"Platform Running: %d\n"
		"Window Size: %dx%d\n"
		"Framebuffer Size: %dx%d\n"
		"DPI Scale: %.2f\n"
		"HighDPI: %d\n"
		"Touch: %d\n"
		"Mouse: %d\n"
		"TextInput: %d\n"
		"Gamepad: %d\n"
		"Sokol GLCore: %d\n"
		"Sokol GLES3: %d\n"
		"Sokol D3D11: %d\n"
		"Sokol Metal: %d\n"
		"Sokol Dummy: %d\n"
		"Audio: %d\n"
		"OpenGL Vendor: %s\n"
		"OpenGL Renderer: %s\n"
		"OpenGL Version: %s\n"
		"GLSL Version: %s\n"
		"Max Texture Size: %d\n",
		XGE_VERSION_MAJOR, XGE_VERSION_MINOR, XGE_VERSION_PATCH,
		sPlatform, tPlatformCaps.sSokolTargetName, sGraphics,
		tPlatformCaps.bWindow, tPlatformCaps.bOffscreen, tPlatformCaps.bRenderTarget,
		tPlatformCaps.bThreadSafeSubmit, xgeRenderThreadGet(),
		g_xge.tPlatformRuntime.bRunning,
		g_xge.tPlatformRuntime.iWindowWidth, g_xge.tPlatformRuntime.iWindowHeight,
		g_xge.tPlatformRuntime.iFramebufferWidth, g_xge.tPlatformRuntime.iFramebufferHeight,
		g_xge.tPlatformRuntime.fDpiScale,
		tPlatformCaps.bHighDPI, tPlatformCaps.bTouch, tPlatformCaps.bMouse,
		tPlatformCaps.bTextInput, tPlatformCaps.bGamepad,
		tPlatformCaps.bSokolGLCore, tPlatformCaps.bSokolGLES3, tPlatformCaps.bSokolD3D11,
		tPlatformCaps.bSokolMetal, tPlatformCaps.bSokolDummy, tPlatformCaps.bAudio,
		sVendor, sRenderer, sVersion, sShading, tCaps.iMaxTextureSize);
	if ( iNeed < 0 ) {
		sBuffer[0] = 0;
		return XGE_ERROR;
	}
	return iNeed;
}

int xgedbgDirtyRectCount(void)
{
	if ( g_xge.bInitialized == 0 ) {
		return 0;
	}
	return g_xge.iDirtyRectCount;
}

int xgedbgDirtyRectGet(int iIndex, xge_rect_t* pRect)
{
	if ( (g_xge.bInitialized == 0) || (pRect == NULL) || (iIndex < 0) || (iIndex >= g_xge.iDirtyRectCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pRect = g_xge.arrDirtyRects[iIndex];
	return XGE_OK;
}

void xgedbgDirtyRectClear(void)
{
	__xgeDirtyRectClear();
}

