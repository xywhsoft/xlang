static int __xgeRenderCommandAtomicCompareExchange(volatile int* pValue, int iExpected, int iValue)
{
	#if defined(_MSC_VER)
		return (int)_InterlockedCompareExchange((volatile long*)pValue, (long)iValue, (long)iExpected);
	#elif defined(__GNUC__) || defined(__clang__)
		return __sync_val_compare_and_swap(pValue, iExpected, iValue);
	#else
		int iOld;

		iOld = *pValue;
		if ( iOld == iExpected ) {
			*pValue = iValue;
		}
		return iOld;
	#endif
}

static void __xgeRenderCommandLock(void)
{
	while ( __xgeRenderCommandAtomicCompareExchange(&g_xge.iRenderCommandLock, 0, 1) != 0 ) {
	}
}

static void __xgeRenderCommandUnlock(void)
{
	#if defined(_MSC_VER)
		_InterlockedExchange((volatile long*)&g_xge.iRenderCommandLock, 0);
	#elif defined(__GNUC__) || defined(__clang__)
		__sync_lock_release(&g_xge.iRenderCommandLock);
	#else
		g_xge.iRenderCommandLock = 0;
	#endif
}

static void __xgeRenderCommandRelease(xge_render_command_t* pCommand)
{
	if ( pCommand == NULL ) {
		return;
	}
	switch ( pCommand->iType ) {
		case XGE_RENDER_COMMAND_DRAW:
			if ( pCommand->u.tDraw.pTexture != NULL ) {
				xgeTextureFree(pCommand->u.tDraw.pTexture);
				pCommand->u.tDraw.pTexture = NULL;
			}
			break;

		default:
			break;
	}
	memset(pCommand, 0, sizeof(*pCommand));
}

static void __xgeRenderCommandUnit(void)
{
	xge_render_command_t* pCommands;
	int iCount;
	int i;

	__xgeRenderCommandLock();
	pCommands = g_xge.pRenderCommands;
	iCount = g_xge.iRenderCommandCount;
	g_xge.pRenderCommands = NULL;
	g_xge.iRenderCommandCount = 0;
	g_xge.iRenderCommandCapacity = 0;
	__xgeRenderCommandUnlock();

	for ( i = 0; i < iCount; i++ ) {
		__xgeRenderCommandRelease(&pCommands[i]);
	}
	if ( pCommands != NULL ) {
		xrtFree(pCommands);
	}
}

static void __xgeRenderCommandReset(void)
{
	__xgeRenderCommandUnit();
	g_xge.bRenderActive = 0;
}

static int __xgeRenderCommandReserve(int iNeeded)
{
	xge_render_command_t* pCommands;
	int iCapacity;

	if ( iNeeded <= g_xge.iRenderCommandCapacity ) {
		return XGE_OK;
	}
	iCapacity = (g_xge.iRenderCommandCapacity > 0) ? g_xge.iRenderCommandCapacity : 64;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pCommands = (xge_render_command_t*)xrtRealloc(g_xge.pRenderCommands, sizeof(*pCommands) * (size_t)iCapacity);
	if ( pCommands == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	g_xge.pRenderCommands = pCommands;
	g_xge.iRenderCommandCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeRenderCommandPush(const xge_render_command_t* pCommand)
{
	int iRet;

	if ( pCommand == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeRenderCommandReserve(g_xge.iRenderCommandCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xge.pRenderCommands[g_xge.iRenderCommandCount] = *pCommand;
	g_xge.iRenderCommandCount++;
	return XGE_OK;
}

static int __xgeRenderCommandDraw(const xge_draw_t* pDraw)
{
	xge_render_command_t tCommand;
	int iRet;

	if ( (pDraw == NULL) || (pDraw->pTexture == NULL) || (pDraw->pTexture->iRefCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tCommand, 0, sizeof(tCommand));
	tCommand.iType = XGE_RENDER_COMMAND_DRAW;
	tCommand.u.tDraw = *pDraw;
	if ( tCommand.u.tDraw.pTexture->iBackendId == 0 ) {
		(void)xgeTextureUploadQueue(tCommand.u.tDraw.pTexture);
	}

	__xgeRenderCommandLock();
	iRet = xgeTextureAddRef(tCommand.u.tDraw.pTexture);
	if ( iRet <= 0 ) {
		__xgeRenderCommandUnlock();
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeRenderCommandPush(&tCommand);
	if ( iRet != XGE_OK ) {
		xgeTextureFree(tCommand.u.tDraw.pTexture);
	}
	__xgeRenderCommandUnlock();
	return iRet;
}

static int __xgeRenderCommandDrawRunEnd(xge_render_command_t* pCommands, int iCount, int iStart)
{
	xge_texture pTexture;
	int iEnd;

	if ( (pCommands == NULL) || (iStart < 0) || (iStart >= iCount) ) {
		return iStart;
	}
	pTexture = pCommands[iStart].u.tDraw.pTexture;
	iEnd = iStart + 1;
	while ( iEnd < iCount ) {
		if ( pCommands[iEnd].iType != XGE_RENDER_COMMAND_DRAW ) {
			break;
		}
		if ( pCommands[iEnd].u.tDraw.pTexture != pTexture ) {
			break;
		}
		iEnd++;
	}
	return iEnd;
}

static void __xgeRenderCommandFlushDrawRangeImmediate(xge_render_command_t* pCommands, int iStart, int iEnd)
{
	int i;

	for ( i = iStart; i < iEnd; i++ ) {
		__xgeDrawExImmediate(&pCommands[i].u.tDraw);
	}
}

static int __xgeRenderCommandFlushDrawRange(xge_render_command_t* pCommands, int iStart, int iEnd)
{
	xge_sprite_batch_t tBatch;
	xge_texture pTexture;
	int iRunCount;
	int i;
	int iRet;

	if ( (pCommands == NULL) || (iStart < 0) || (iEnd <= iStart) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRunCount = iEnd - iStart;
	if ( iRunCount <= 1 ) {
		__xgeRenderCommandFlushDrawRangeImmediate(pCommands, iStart, iEnd);
		return XGE_OK;
	}
	pTexture = pCommands[iStart].u.tDraw.pTexture;
	if ( (pTexture == NULL) || (pTexture->iRefCount <= 0) || (pTexture->iBackendId == 0) ) {
		__xgeRenderCommandFlushDrawRangeImmediate(pCommands, iStart, iEnd);
		return XGE_OK;
	}

	memset(&tBatch, 0, sizeof(tBatch));
	iRet = xgeSpriteBatchInit(&tBatch, pTexture, iRunCount, 0);
	if ( iRet != XGE_OK ) {
		__xgeRenderCommandFlushDrawRangeImmediate(pCommands, iStart, iEnd);
		return XGE_OK;
	}
	for ( i = iStart; i < iEnd; i++ ) {
		iRet = xgeSpriteBatchAdd(&tBatch, &pCommands[i].u.tDraw);
		if ( iRet != XGE_OK ) {
			xgeSpriteBatchFree(&tBatch);
			__xgeRenderCommandFlushDrawRangeImmediate(pCommands, iStart, iEnd);
			return XGE_OK;
		}
	}
	iRet = xgeSpriteBatchFlush(&tBatch);
	xgeSpriteBatchFree(&tBatch);
	if ( iRet != XGE_OK ) {
		__xgeRenderCommandFlushDrawRangeImmediate(pCommands, iStart, iEnd);
	}
	return XGE_OK;
}

static int __xgeRenderCommandFlush(void)
{
	xge_render_command_t* pCommands;
	int iCount;
	int i;
	int iEnd;

	__xgeRenderCommandLock();
	pCommands = g_xge.pRenderCommands;
	iCount = g_xge.iRenderCommandCount;
	g_xge.pRenderCommands = NULL;
	g_xge.iRenderCommandCount = 0;
	g_xge.iRenderCommandCapacity = 0;
	__xgeRenderCommandUnlock();

	for ( i = 0; i < iCount; i++ ) {
		switch ( pCommands[i].iType ) {
			case XGE_RENDER_COMMAND_DRAW:
				iEnd = __xgeRenderCommandDrawRunEnd(pCommands, iCount, i);
				(void)__xgeRenderCommandFlushDrawRange(pCommands, i, iEnd);
				while ( i < iEnd ) {
					__xgeRenderCommandRelease(&pCommands[i]);
					i++;
				}
				i--;
				break;

			default:
				__xgeRenderCommandRelease(&pCommands[i]);
				break;
		}
	}
	if ( pCommands != NULL ) {
		xrtFree(pCommands);
	}
	return XGE_OK;
}

static void __xgeRenderThreadEGLSnapshot(xge_context_t* pContext, const xge_egl_context_t* pEGL)
{
	if ( (pContext == NULL) || (pEGL == NULL) ) {
		return;
	}
	pContext->tRenderThreadLastEGL = *pEGL;
	pContext->tRenderThreadLastEGL.bInitialized = 0;
	pContext->tRenderThreadLastEGL.pDisplay = NULL;
	pContext->tRenderThreadLastEGL.pConfig = NULL;
	pContext->tRenderThreadLastEGL.pSurface = NULL;
	pContext->tRenderThreadLastEGL.pContext = NULL;
}

static int __xgeRenderThreadFlushWithEGL(xge_context_t* pContext)
{
#if defined(XGE_HAS_EGL)
	xge_egl_context_t tEGL;
	int iRet;

	memset(&tEGL, 0, sizeof(tEGL));
	tEGL.iWidth = pContext->tRenderThreadEGLDesc.iWidth;
	tEGL.iHeight = pContext->tRenderThreadEGLDesc.iHeight;
	tEGL.bPBuffer = pContext->tRenderThreadEGLDesc.bPBuffer ? 1 : 0;
	tEGL.bSurfaceless = pContext->tRenderThreadEGLDesc.bSurfaceless ? 1 : 0;
	tEGL.bBoardLinux = pContext->tRenderThreadEGLDesc.bBoardLinux ? 1 : 0;
	tEGL.pDisplay = pContext->tRenderThreadEGLDesc.pNativeDisplay;
	tEGL.pSurface = pContext->tRenderThreadEGLDesc.pNativeWindow;
	tEGL.pUser = pContext->tRenderThreadEGLDesc.pUser;
	iRet = __xgeEGLCreateContext(&tEGL, &pContext->tRenderThreadEGLDesc);
	__xgeRenderThreadEGLSnapshot(pContext, &tEGL);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( xge_gl_load((XgeGLLoadProc)__xgeGLGetProc) == 0 ) {
		__xgeRenderThreadEGLSnapshot(pContext, &tEGL);
		xgeEGLUnit(&tEGL);
		return XGE_ERROR_GPU_FAILED;
	}
	pContext->bRenderThreadGLCurrent = 1;
	(void)xgeTextureUploadFlush();
	iRet = __xgeRenderCommandFlush();
	if ( glFlush != NULL ) {
		glFlush();
	}
	pContext->bRenderThreadGLCurrent = 0;
	__xgeRenderThreadEGLSnapshot(pContext, &tEGL);
	xgeEGLUnit(&tEGL);
	return iRet;
#else
	(void)pContext;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

static uint32 __xgeRenderThreadProc(void* pUser)
{
	xge_context_t* pContext;

	pContext = (xge_context_t*)pUser;
	if ( pContext == NULL ) {
		return (uint32)XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->bRenderThreadOwnsGLContext != 0) && (pContext->bRenderThreadEGLConfigured != 0) ) {
		pContext->iRenderThreadResult = __xgeRenderThreadFlushWithEGL(pContext);
	} else {
		pContext->iRenderThreadResult = __xgeRenderCommandFlush();
	}
	pContext->bRenderThreadGLCurrent = 0;
	pContext->bRenderThreadBusy = 0;
	return (uint32)pContext->iRenderThreadResult;
}

static void __xgeRenderThreadJoin(void)
{
	if ( g_xge.pRenderThread == NULL ) {
		return;
	}
	xrtThreadWait((xthread)g_xge.pRenderThread);
	xrtThreadDestroy((xthread)g_xge.pRenderThread);
	g_xge.pRenderThread = NULL;
	g_xge.bRenderThreadBusy = 0;
}

static int __xgeRenderCommandFlushThreaded(void)
{
	if ( g_xge.pRenderThread != NULL ) {
		__xgeRenderThreadJoin();
	}
	g_xge.iRenderThreadResult = XGE_OK;
	g_xge.bRenderThreadBusy = 1;
	g_xge.pRenderThread = xrtThreadCreate((ptr)__xgeRenderThreadProc, &g_xge, 0);
	if ( g_xge.pRenderThread == NULL ) {
		g_xge.bRenderThreadBusy = 0;
		return XGE_ERROR_THREAD_FAILED;
	}
	__xgeRenderThreadJoin();
	return g_xge.iRenderThreadResult;
}
