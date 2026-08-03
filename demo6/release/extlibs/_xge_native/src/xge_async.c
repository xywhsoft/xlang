static int __xgeAsyncBegin(xge_async_request pRequest, int iType, void* pTarget, const char* sPath, xge_async_proc onComplete, void* pUser)
{
	if ( (pRequest == NULL) || (pTarget == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	xgeAsyncRequestFree(pRequest);
	memset(pRequest, 0, sizeof(*pRequest));
	pRequest->sURI = __xgeStrDup(sPath);
	if ( pRequest->sURI == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pRequest->iType = iType;
	pRequest->iStatus = XGE_ASYNC_LOADING;
	pRequest->iResult = XGE_OK;
	pRequest->fProgress = 0.0f;
	pRequest->pTarget = pTarget;
	pRequest->onComplete = onComplete;
	pRequest->pUser = pUser;
	return XGE_OK;
}

static int __xgeAsyncFinishEx(xge_async_request pRequest, int iResult, int bInvokeCallback)
{
	pRequest->iResult = iResult;
	pRequest->fProgress = 1.0f;
	if ( pRequest->bCancel != 0 ) {
		pRequest->iStatus = XGE_ASYNC_CANCELLED;
		pRequest->iResult = XGE_ERROR;
	} else if ( iResult == XGE_OK ) {
		pRequest->iStatus = XGE_ASYNC_READY;
	} else {
		pRequest->iStatus = XGE_ASYNC_FAILED;
	}
	if ( bInvokeCallback && (pRequest->onComplete != NULL) ) {
		pRequest->onComplete(pRequest, pRequest->pUser);
	} else if ( !bInvokeCallback ) {
		pRequest->bCallbackPending = 1;
	}
	return pRequest->iResult;
}

static int __xgeAsyncFinish(xge_async_request pRequest, int iResult)
{
	return __xgeAsyncFinishEx(pRequest, iResult, 1);
}

static uint32 __xgeAsyncThreadProc(ptr pParam)
{
	xge_async_request pRequest;
	int iRet;

	pRequest = (xge_async_request)pParam;
	if ( pRequest == NULL ) {
		return 1;
	}
	iRet = XGE_ERROR_INVALID_ARGUMENT;
	if ( pRequest->bCancel == 0 ) {
		if ( pRequest->iType == XGE_ASYNC_IMAGE ) {
			iRet = xgeImageLoadEx((xge_image)pRequest->pTarget, pRequest->sURI, pRequest->iFlags);
		} else if ( pRequest->iType == XGE_ASYNC_TEXTURE ) {
			iRet = xgeTextureLoadEx((xge_texture)pRequest->pTarget, pRequest->sURI, pRequest->iFlags);
		} else if ( pRequest->iType == XGE_ASYNC_FONT ) {
			iRet = xgeFontLoad((xge_font)pRequest->pTarget, pRequest->sURI, pRequest->fSize);
		} else if ( pRequest->iType == XGE_ASYNC_SOUND ) {
			iRet = xgeSoundLoad((xge_sound)pRequest->pTarget, pRequest->sURI);
		}
	}
	(void)__xgeAsyncFinishEx(pRequest, iRet, 0);
	return (uint32)(iRet == XGE_OK ? 0 : 1);
}

static int __xgeAsyncStartThread(xge_async_request pRequest)
{
	xthread pThread;

	pThread = xrtThreadCreate((ptr)__xgeAsyncThreadProc, pRequest, 0);
	if ( pThread == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pRequest->pThread = pThread;
	pRequest->bThreaded = 1;
	return XGE_OK;
}

void xgeAsyncRequestInit(xge_async_request pRequest)
{
	if ( pRequest == NULL ) {
		return;
	}
	memset(pRequest, 0, sizeof(*pRequest));
	pRequest->iStatus = XGE_ASYNC_PENDING;
}

void xgeAsyncRequestFree(xge_async_request pRequest)
{
	if ( pRequest == NULL ) {
		return;
	}
	if ( pRequest->pThread != NULL ) {
		xrtThreadWait((xthread)pRequest->pThread);
		xrtThreadDestroy((xthread)pRequest->pThread);
		pRequest->pThread = NULL;
	}
	if ( pRequest->sURI != NULL ) {
		xrtFree(pRequest->sURI);
	}
	memset(pRequest, 0, sizeof(*pRequest));
	pRequest->iStatus = XGE_ASYNC_PENDING;
}

int xgeAsyncRequestCancel(xge_async_request pRequest)
{
	if ( pRequest == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pRequest->iStatus == XGE_ASYNC_READY) || (pRequest->iStatus == XGE_ASYNC_FAILED) || (pRequest->iStatus == XGE_ASYNC_CANCELLED) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	pRequest->bCancel = 1;
	pRequest->iStatus = XGE_ASYNC_CANCELLED;
	pRequest->iResult = XGE_ERROR;
	return XGE_OK;
}

int xgeAsyncThreadingSet(int bEnabled)
{
	g_xge.bAsyncThreadingEnabled = bEnabled ? 1 : 0;
	return XGE_OK;
}

int xgeAsyncThreadingGet(void)
{
	return g_xge.bAsyncThreadingEnabled;
}

int xgeAsyncPoll(xge_async_request pRequest)
{
	if ( pRequest == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pRequest->pThread != NULL ) {
		if ( xrtThreadWaitTimeout((xthread)pRequest->pThread, 0) == XRT_WAIT_TIMEOUT ) {
			return XGE_ASYNC_LOADING;
		}
		xrtThreadDestroy((xthread)pRequest->pThread);
		pRequest->pThread = NULL;
	}
	if ( pRequest->bCallbackPending != 0 ) {
		pRequest->bCallbackPending = 0;
		if ( pRequest->onComplete != NULL ) {
			pRequest->onComplete(pRequest, pRequest->pUser);
		}
	}
	return pRequest->iStatus;
}

int xgeAsyncImageLoad(xge_async_request pRequest, xge_image pImage, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser)
{
	int iRet;

	iRet = __xgeAsyncBegin(pRequest, XGE_ASYNC_IMAGE, pImage, sPath, onComplete, pUser);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pRequest->iFlags = iFlags;
	if ( g_xge.bAsyncThreadingEnabled != 0 ) {
		iRet = __xgeAsyncStartThread(pRequest);
		if ( iRet == XGE_OK ) {
			return XGE_OK;
		}
	}
	iRet = xgeImageLoadEx(pImage, sPath, iFlags);
	return __xgeAsyncFinish(pRequest, iRet);
}

int xgeAsyncTextureLoad(xge_async_request pRequest, xge_texture pTexture, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser)
{
	int iRet;

	iRet = __xgeAsyncBegin(pRequest, XGE_ASYNC_TEXTURE, pTexture, sPath, onComplete, pUser);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pRequest->iFlags = iFlags;
	if ( g_xge.bAsyncThreadingEnabled != 0 ) {
		iRet = __xgeAsyncStartThread(pRequest);
		if ( iRet == XGE_OK ) {
			return XGE_OK;
		}
	}
	iRet = xgeTextureLoadEx(pTexture, sPath, iFlags);
	return __xgeAsyncFinish(pRequest, iRet);
}

int xgeAsyncFontLoad(xge_async_request pRequest, xge_font pFont, const char* sPath, float fSize, xge_async_proc onComplete, void* pUser)
{
	int iRet;

	iRet = __xgeAsyncBegin(pRequest, XGE_ASYNC_FONT, pFont, sPath, onComplete, pUser);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pRequest->fSize = fSize;
	if ( g_xge.bAsyncThreadingEnabled != 0 ) {
		iRet = __xgeAsyncStartThread(pRequest);
		if ( iRet == XGE_OK ) {
			return XGE_OK;
		}
	}
	iRet = xgeFontLoad(pFont, sPath, fSize);
	return __xgeAsyncFinish(pRequest, iRet);
}

int xgeAsyncSoundLoad(xge_async_request pRequest, xge_sound pSound, const char* sPath, xge_async_proc onComplete, void* pUser)
{
	int iRet;

	iRet = __xgeAsyncBegin(pRequest, XGE_ASYNC_SOUND, pSound, sPath, onComplete, pUser);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( g_xge.bAsyncThreadingEnabled != 0 ) {
		iRet = __xgeAsyncStartThread(pRequest);
		if ( iRet == XGE_OK ) {
			return XGE_OK;
		}
	}
	iRet = xgeSoundLoad(pSound, sPath);
	return __xgeAsyncFinish(pRequest, iRet);
}
