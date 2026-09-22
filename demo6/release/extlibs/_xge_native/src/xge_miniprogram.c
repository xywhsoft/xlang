static int __xgeMiniProgramResourceLoad(const char* sURI, void** ppData, int* pSize, void* pUser)
{
	xge_miniprogram_bridge_t* pBridge;

	(void)pUser;
	if ( (sURI == NULL) || (ppData == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pBridge = &g_xge.tMiniProgramBridge;
	if ( pBridge->load_resource == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	return pBridge->load_resource(sURI, ppData, pSize, pBridge->pUser);
}

static void __xgeMiniProgramResourceFree(void* pData, void* pUser)
{
	xge_miniprogram_bridge_t* pBridge;

	(void)pUser;
	if ( pData == NULL ) {
		return;
	}
	pBridge = &g_xge.tMiniProgramBridge;
	if ( pBridge->free_resource != NULL ) {
		pBridge->free_resource(pData, pBridge->pUser);
	} else {
		xrtFree(pData);
	}
}

static void __xgeMiniProgramResourceProviderAdd(void)
{
	xge_resource_provider_t tProvider;

	if ( g_xge.tMiniProgramBridge.load_resource == NULL ) {
		return;
	}
	memset(&tProvider, 0, sizeof(tProvider));
	tProvider.sScheme = "res";
	tProvider.load = __xgeMiniProgramResourceLoad;
	tProvider.free = __xgeMiniProgramResourceFree;
	(void)xgeResourceProviderAdd(&tProvider);
}

static void __xgeMiniProgramSetDimensions(int iWidth, int iHeight, float fDevicePixelRatio)
{
	int iFramebufferWidth;
	int iFramebufferHeight;

	iFramebufferWidth = (int)floorf((float)iWidth * fDevicePixelRatio);
	iFramebufferHeight = (int)floorf((float)iHeight * fDevicePixelRatio);
	if ( iFramebufferWidth < 1 ) iFramebufferWidth = 1;
	if ( iFramebufferHeight < 1 ) iFramebufferHeight = 1;
	g_xge.iWindowWidth = iWidth;
	g_xge.iWindowHeight = iHeight;
	g_xge.iFramebufferWidth = iFramebufferWidth;
	g_xge.iFramebufferHeight = iFramebufferHeight;
	g_xge.fDpiScale = fDevicePixelRatio;
	g_xge.iWidth = iFramebufferWidth;
	g_xge.iHeight = iFramebufferHeight;
	g_xge.tPlatformRuntime.iWindowWidth = iWidth;
	g_xge.tPlatformRuntime.iWindowHeight = iHeight;
	g_xge.tPlatformRuntime.iFramebufferWidth = iFramebufferWidth;
	g_xge.tPlatformRuntime.iFramebufferHeight = iFramebufferHeight;
	g_xge.tPlatformRuntime.fDpiScale = fDevicePixelRatio;
	g_xge.tCamera.tViewport.fW = (float)iFramebufferWidth;
	g_xge.tCamera.tViewport.fH = (float)iFramebufferHeight;
}

int xgeMiniProgramInit(const xge_miniprogram_desc_t* pDesc)
{
	xge_platform_backend_t tPlatform;
	xge_graphics_backend_t tGraphics;

	if ( pDesc == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	g_xge.tMiniProgramDesc = *pDesc;
	if ( g_xge.tMiniProgramDesc.fDevicePixelRatio <= 0.0f ) {
		g_xge.tMiniProgramDesc.fDevicePixelRatio = 1.0f;
	}
	if ( g_xge.tMiniProgramDesc.iWidth > 0 && g_xge.tMiniProgramDesc.iHeight > 0 ) {
		__xgeMiniProgramSetDimensions(g_xge.tMiniProgramDesc.iWidth,
			g_xge.tMiniProgramDesc.iHeight, g_xge.tMiniProgramDesc.fDevicePixelRatio);
	}
	memset(&tPlatform, 0, sizeof(tPlatform));
	tPlatform.iType = XGE_PLATFORM_BACKEND_MINIPROGRAM;
	tPlatform.sName = "miniprogram";
	xgePlatformBackendSet(&tPlatform);
	memset(&tGraphics, 0, sizeof(tGraphics));
	tGraphics.iType = XGE_GPU_BACKEND_WEBGL2;
	tGraphics.sName = "webgl2";
	xgeGraphicsBackendSet(&tGraphics);
	__xgeMiniProgramResourceProviderAdd();
	g_xge.bMiniProgramInitialized = 1;
	return XGE_OK;
}

int xgeMiniProgramInitSimple(int iWidth, int iHeight, float fDevicePixelRatio)
{
	xge_miniprogram_desc_t tDesc;

	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iWidth = iWidth;
	tDesc.iHeight = iHeight;
	tDesc.fDevicePixelRatio = fDevicePixelRatio;
	return xgeMiniProgramInit(&tDesc);
}

void xgeMiniProgramUnit(void)
{
	memset(&g_xge.tMiniProgramDesc, 0, sizeof(g_xge.tMiniProgramDesc));
	g_xge.bMiniProgramInitialized = 0;
}

int xgeMiniProgramSetBridge(const xge_miniprogram_bridge_t* pBridge)
{
	if ( pBridge == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	g_xge.tMiniProgramBridge = *pBridge;
	if ( g_xge.bMiniProgramInitialized != 0 ) {
		__xgeMiniProgramResourceProviderAdd();
	}
	return XGE_OK;
}

int xgeMiniProgramFrame(double fTimeSeconds)
{
	(void)fTimeSeconds;
	if ( g_xge.bMiniProgramInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	return xgeFrame();
}

int xgeMiniProgramResize(int iWidth, int iHeight, float fDevicePixelRatio)
{
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fDevicePixelRatio <= 0.0f ) {
		fDevicePixelRatio = 1.0f;
	}
	g_xge.tMiniProgramDesc.iWidth = iWidth;
	g_xge.tMiniProgramDesc.iHeight = iHeight;
	g_xge.tMiniProgramDesc.fDevicePixelRatio = fDevicePixelRatio;
	__xgeMiniProgramSetDimensions(iWidth, iHeight, fDevicePixelRatio);
	return XGE_OK;
}

int xgeMiniProgramTouch(int iPhase, const xge_miniprogram_touch_t* pTouches, int iCount)
{
	xge_event_t tEvent;
	xge_touch_event_t tTouch;
	xge_touch_point_t* pPoint;
	float fDevicePixelRatio;
	float fX;
	float fY;
	int i;
	int iIndex;

	if ( (pTouches == NULL) || (iCount < 0) || (iCount > XGE_TOUCH_MAX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fDevicePixelRatio = (g_xge.tMiniProgramDesc.fDevicePixelRatio > 0.0f) ?
		g_xge.tMiniProgramDesc.fDevicePixelRatio : 1.0f;
	__xgeTouchResetStationary();
	for ( i = 0; i < iCount; i++ ) {
		iIndex = __xgeTouchFindIndex(pTouches[i].iId);
		if ( iIndex < 0 ) {
			if ( g_xge.iTouchCount >= XGE_TOUCH_MAX ) {
				continue;
			}
			iIndex = g_xge.iTouchCount++;
			memset(&g_xge.arrTouches[iIndex], 0, sizeof(g_xge.arrTouches[iIndex]));
			g_xge.arrTouches[iIndex].iId = pTouches[i].iId;
		}
		pPoint = &g_xge.arrTouches[iIndex];
		fX = pTouches[i].fX * fDevicePixelRatio;
		fY = pTouches[i].fY * fDevicePixelRatio;
		pPoint->fDX = fX - pPoint->fX;
		pPoint->fDY = fY - pPoint->fY;
		pPoint->fX = fX;
		pPoint->fY = fY;
		pPoint->iPhase = iPhase;
		pPoint->bChanged = 1;
		pPoint->bDown = ((iPhase == XGE_TOUCH_END) || (iPhase == XGE_TOUCH_CANCEL)) ? 0 : 1;
	}
	memset(&tEvent, 0, sizeof(tEvent));
	memset(&tTouch, 0, sizeof(tTouch));
	if ( iPhase == XGE_TOUCH_BEGIN ) {
		tEvent.iType = XGE_EVENT_TOUCH_BEGIN;
	} else if ( iPhase == XGE_TOUCH_MOVE ) {
		tEvent.iType = XGE_EVENT_TOUCH_MOVE;
	} else if ( iPhase == XGE_TOUCH_END ) {
		tEvent.iType = XGE_EVENT_TOUCH_END;
	} else if ( iPhase == XGE_TOUCH_CANCEL ) {
		tEvent.iType = XGE_EVENT_TOUCH_CANCEL;
	} else {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tTouch.iPhase = iPhase;
	tTouch.iCount = g_xge.iTouchCount;
	if ( tTouch.iCount > XGE_TOUCH_MAX ) {
		tTouch.iCount = XGE_TOUCH_MAX;
	}
	for ( i = 0; i < tTouch.iCount; i++ ) {
		tTouch.arrPoints[i] = g_xge.arrTouches[i];
	}
	tEvent.pData = &tTouch;
	for ( i = 0; i < tTouch.iCount; i++ ) {
		if ( tTouch.arrPoints[i].bChanged != 0 ) {
			tEvent.iPointerId = tTouch.arrPoints[i].iId;
			tEvent.fX = tTouch.arrPoints[i].fX;
			tEvent.fY = tTouch.arrPoints[i].fY;
			tEvent.fDX = tTouch.arrPoints[i].fDX;
			tEvent.fDY = tTouch.arrPoints[i].fDY;
			break;
		}
	}
	if ( (i >= tTouch.iCount) && (tTouch.iCount > 0) ) {
		tEvent.iPointerId = tTouch.arrPoints[0].iId;
		tEvent.fX = tTouch.arrPoints[0].fX;
		tEvent.fY = tTouch.arrPoints[0].fY;
		tEvent.fDX = tTouch.arrPoints[0].fDX;
		tEvent.fDY = tTouch.arrPoints[0].fDY;
	}
	for ( i = 0; i < tTouch.iCount; i++ ) {
		if ( tTouch.arrPoints[i].bChanged == 0 ) continue;
		__xgeInputQueuePointerEvent(tEvent.iType, tTouch.arrPoints[i].iId,
			XGE_MOUSE_LEFT, tTouch.arrPoints[i].bDown ? XGE_MOUSE_LEFT : 0u,
			0u, tTouch.arrPoints[i].fX, tTouch.arrPoints[i].fY,
			tTouch.arrPoints[i].fDX, tTouch.arrPoints[i].fDY);
	}
	xgeSceneDispatchEvent(&tEvent);
	return XGE_OK;
}

int xgeMiniProgramTouchOne(int iPhase, int iId, float fX, float fY, float fForce)
{
	xge_miniprogram_touch_t tTouch;

	if ( iId < 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tTouch, 0, sizeof(tTouch));
	tTouch.iId = (uint64_t)iId;
	tTouch.fX = fX;
	tTouch.fY = fY;
	tTouch.fForce = fForce;
	return xgeMiniProgramTouch(iPhase, &tTouch, 1);
}

int xgeMiniProgramText(uint32_t iCodepoint)
{
	xge_event_t tEvent;
	xge_input_event_t tInput;

	if ( iCodepoint == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeTextPush(iCodepoint);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.iType = XGE_EVENT_TEXT;
	tInput.iCodepoint = iCodepoint;
	if ( !__xgeInputEventQueuePush(&tInput) ) return XGE_ERROR_OUT_OF_MEMORY;
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iType = XGE_EVENT_TEXT;
	tEvent.iCodepoint = iCodepoint;
	xgeSceneDispatchEvent(&tEvent);
	return XGE_OK;
}

int xgeMiniProgramRequestFrame(void)
{
	xge_miniprogram_bridge_t* pBridge;

	pBridge = &g_xge.tMiniProgramBridge;
	if ( pBridge->request_frame == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	return pBridge->request_frame(pBridge->pUser);
}

int xgeMiniProgramAudioCommand(int iCommand, int iHandle, const void* pData, int iSize)
{
	xge_miniprogram_bridge_t* pBridge;

	if ( iSize < 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pBridge = &g_xge.tMiniProgramBridge;
	if ( pBridge->audio_command == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	return pBridge->audio_command(iCommand, iHandle, pData, iSize, pBridge->pUser);
}
