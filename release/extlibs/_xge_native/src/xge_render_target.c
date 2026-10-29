static int __xgeRenderTargetEnsureFramebuffer(xge_render_target pTarget)
{
	GLuint iFramebuffer;
	GLuint iStencilRenderbuffer;

	if ( (pTarget == NULL) || (pTarget->iWidth <= 0) || (pTarget->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pTarget->iFlags & XGE_RENDER_TARGET_WINDOW) != 0 ) {
		return XGE_OK;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( pTarget->tTexture.iBackendId == 0 ) {
		if ( __xgeTextureUploadNow(&pTarget->tTexture) != XGE_OK ) {
			return XGE_ERROR_GPU_FAILED;
		}
	}
	if ( pTarget->iFramebufferId != 0 ) {
		return XGE_OK;
	}
	if ( (glGenFramebuffers == NULL) || (glDeleteFramebuffers == NULL) || (glBindFramebuffer == NULL) || (glFramebufferTexture2D == NULL) || (glCheckFramebufferStatus == NULL) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iStencilRenderbuffer = 0;
	glGenFramebuffers(1, &iFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, iFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)pTarget->tTexture.iBackendId, 0);
	if ( (glGenRenderbuffers != NULL) && (glDeleteRenderbuffers != NULL) && (glBindRenderbuffer != NULL) && (glRenderbufferStorage != NULL) && (glFramebufferRenderbuffer != NULL) ) {
		glGenRenderbuffers(1, &iStencilRenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, iStencilRenderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, pTarget->iWidth, pTarget->iHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, iStencilRenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ) {
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_xge.iCurrentFramebufferId);
		if ( iStencilRenderbuffer != 0 ) {
			glDeleteRenderbuffers(1, &iStencilRenderbuffer);
		}
		glDeleteFramebuffers(1, &iFramebuffer);
		return XGE_ERROR_GPU_FAILED;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_xge.iCurrentFramebufferId);
	pTarget->iFramebufferId = iFramebuffer;
	pTarget->iStencilRenderbufferId = iStencilRenderbuffer;
	return XGE_OK;
}

int xgeRenderTargetWindow(xge_render_target pTarget)
{
	if ( pTarget == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pTarget, 0, sizeof(*pTarget));
	pTarget->iWidth = g_xge.iWidth;
	pTarget->iHeight = g_xge.iHeight;
	pTarget->iFlags = XGE_RENDER_TARGET_WINDOW;
	return XGE_OK;
}

int xgeRenderTargetCreate(xge_render_target pTarget, int iWidth, int iHeight)
{
	int iRet;

	if ( (pTarget == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pTarget, 0, sizeof(*pTarget));
	iRet = xgeTextureCreateRGBA(&pTarget->tTexture, iWidth, iHeight, NULL);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pTarget->iWidth = iWidth;
	pTarget->iHeight = iHeight;
	pTarget->iFlags = XGE_RENDER_TARGET_TEXTURE;
	if ( g_xge.bSokolRunning != 0 ) {
		iRet = __xgeRenderTargetEnsureFramebuffer(pTarget);
		if ( iRet != XGE_OK ) {
			xgeRenderTargetFree(pTarget);
			return iRet;
		}
	}
	return XGE_OK;
}

int xgeRenderTargetResize(xge_render_target pTarget, int iWidth, int iHeight)
{
	uint32_t iFlags;

	if ( (pTarget == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pTarget->iFlags & XGE_RENDER_TARGET_WINDOW) != 0 ) {
		pTarget->iWidth = iWidth;
		pTarget->iHeight = iHeight;
		return XGE_OK;
	}
	iFlags = pTarget->iFlags;
	xgeRenderTargetFree(pTarget);
	if ( (iFlags & XGE_RENDER_TARGET_TEXTURE) == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return xgeRenderTargetCreate(pTarget, iWidth, iHeight);
}

int xgeRenderTargetCaptureCurrent(xge_render_target pTarget, int iSrcX, int iSrcY)
{
	int iGLY;
	int iRet;

	if ( (pTarget == NULL) || ((pTarget->iFlags & XGE_RENDER_TARGET_TEXTURE) == 0) ||
	     (pTarget->iWidth <= 0) || (pTarget->iHeight <= 0) || (iSrcX < 0) || (iSrcY < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSrcX > (g_xge.iWidth - pTarget->iWidth)) ||
	     (iSrcY > (g_xge.iHeight - pTarget->iHeight)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (g_xge.bSokolRunning == 0) || (glCopyTexSubImage2D == NULL) || (glActiveTexture == NULL) || (glBindTexture == NULL) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iRet = xgeFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeRenderTargetEnsureFramebuffer(pTarget);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iGLY = g_xge.iHeight - iSrcY - pTarget->iHeight;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (GLuint)pTarget->tTexture.iBackendId);
	glCopyTexSubImage2D(
		GL_TEXTURE_2D, 0, 0, 0, iSrcX, iGLY,
		pTarget->iWidth, pTarget->iHeight
	);
	glBindTexture(GL_TEXTURE_2D, 0);
	return XGE_OK;
}

int xgeRenderTargetReadPixels(xge_render_target pTarget, void* pPixels, int iStride)
{
	unsigned char* pTemp;
	unsigned char* pDst;
	unsigned char* pSrc;
	GLuint iFramebuffer;
	int iRow;
	int iCopyStride;
	int iSize;

	if ( (pTarget == NULL) || (pPixels == NULL) || (pTarget->iWidth <= 0) || (pTarget->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iStride <= 0 ) {
		iStride = pTarget->iWidth * 4;
	}
	if ( iStride < (pTarget->iWidth * 4) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		if ( (pTarget->iFlags & XGE_RENDER_TARGET_TEXTURE) != 0 ) {
			return xgeTextureReadPixels(&pTarget->tTexture, pPixels, iStride);
		}
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( glReadPixels == NULL ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( __xgeRenderTargetEnsureFramebuffer(pTarget) != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	if ( pTarget->iWidth > (INT32_MAX / 4) || pTarget->iHeight > (INT32_MAX / (pTarget->iWidth * 4)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iSize = pTarget->iWidth * pTarget->iHeight * 4;
	pTemp = (unsigned char*)xrtMalloc((size_t)iSize);
	if ( pTemp == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iFramebuffer = (GLuint)(((pTarget->iFlags & XGE_RENDER_TARGET_WINDOW) != 0) ? 0 : pTarget->iFramebufferId);
	glBindFramebuffer(GL_FRAMEBUFFER, iFramebuffer);
	glReadPixels(0, 0, pTarget->iWidth, pTarget->iHeight, GL_RGBA, GL_UNSIGNED_BYTE, pTemp);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_xge.iCurrentFramebufferId);
	pDst = (unsigned char*)pPixels;
	iCopyStride = pTarget->iWidth * 4;
	for ( iRow = 0; iRow < pTarget->iHeight; iRow++ ) {
		pSrc = pTemp + ((pTarget->iHeight - 1 - iRow) * iCopyStride);
		memcpy(pDst + (iRow * iStride), pSrc, (size_t)iCopyStride);
	}
	xrtFree(pTemp);
	return XGE_OK;
}

xge_texture xgeRenderTargetTexture(xge_render_target pTarget)
{
	if ( (pTarget == NULL) || ((pTarget->iFlags & XGE_RENDER_TARGET_TEXTURE) == 0) ) {
		return NULL;
	}
	return &pTarget->tTexture;
}

void xgeRenderTargetFree(xge_render_target pTarget)
{
	GLuint iFramebuffer;
	GLuint iStencilRenderbuffer;

	if ( pTarget == NULL ) {
		return;
	}
	iFramebuffer = (GLuint)pTarget->iFramebufferId;
	iStencilRenderbuffer = (GLuint)pTarget->iStencilRenderbufferId;
	if ( (iFramebuffer != 0) && (g_xge.bSokolRunning != 0) && (glDeleteFramebuffers != NULL) ) {
		if ( g_xge.iCurrentFramebufferId == pTarget->iFramebufferId ) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			g_xge.iCurrentFramebufferId = 0;
		}
		glDeleteFramebuffers(1, &iFramebuffer);
	}
	if ( (iStencilRenderbuffer != 0) && (g_xge.bSokolRunning != 0) && (glDeleteRenderbuffers != NULL) ) {
		glDeleteRenderbuffers(1, &iStencilRenderbuffer);
	}
	xgeTextureFree(&pTarget->tTexture);
	memset(pTarget, 0, sizeof(*pTarget));
}

void xgePassInit(xge_pass pPass, xge_render_target pTarget, uint32_t iClearFlags, uint32_t iClearColor)
{
	if ( pPass == NULL ) {
		return;
	}
	memset(pPass, 0, sizeof(*pPass));
	pPass->pTarget = pTarget;
	pPass->iClearFlags = iClearFlags;
	pPass->iClearColor = iClearColor;
}

int xgePassBegin(xge_pass pPass)
{
	xge_render_target pTarget;
	uint32_t iFramebuffer;

	if ( (pPass == NULL) || (pPass->pTarget == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPass->bActive ) {
		return XGE_ERROR_ALREADY_INITIALIZED;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( xgeFlush() != XGE_OK ) {
		return XGE_ERROR_BACKEND_FAILED;
	}
	pTarget = pPass->pTarget;
	if ( __xgeRenderTargetEnsureFramebuffer(pTarget) != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	pPass->iPrevWidth = g_xge.iWidth;
	pPass->iPrevHeight = g_xge.iHeight;
	pPass->iPrevFramebufferId = g_xge.iCurrentFramebufferId;
	pPass->tPrevCamera = g_xge.tCamera;
	pPass->bPrevViewportEnabled = g_xge.bViewportEnabled;
	pPass->tPrevViewportRect = g_xge.tViewportRect;
	iFramebuffer = ((pTarget->iFlags & XGE_RENDER_TARGET_WINDOW) != 0) ? 0 : pTarget->iFramebufferId;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)iFramebuffer);
	g_xge.iCurrentFramebufferId = iFramebuffer;
	g_xge.iWidth = pTarget->iWidth;
	g_xge.iHeight = pTarget->iHeight;
	g_xge.tCamera = xgeCameraDefault((float)pTarget->iWidth, (float)pTarget->iHeight);
	g_xge.bViewportEnabled = 0;
	memset(&g_xge.tViewportRect, 0, sizeof(g_xge.tViewportRect));
	glViewport(0, 0, pTarget->iWidth, pTarget->iHeight);
	if ( (pPass->iClearFlags & XGE_PASS_CLEAR_COLOR) != 0 ) {
		xgeClear(pPass->iClearColor);
	}
	pPass->bActive = 1;
	return XGE_OK;
}

int xgePassEnd(xge_pass pPass)
{
	int iRet;

	if ( (pPass == NULL) || (pPass->bActive == 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	iRet = xgeFlush();
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)pPass->iPrevFramebufferId);
	g_xge.iCurrentFramebufferId = pPass->iPrevFramebufferId;
	g_xge.iWidth = pPass->iPrevWidth;
	g_xge.iHeight = pPass->iPrevHeight;
	g_xge.tCamera = pPass->tPrevCamera;
	g_xge.bViewportEnabled = pPass->bPrevViewportEnabled;
	g_xge.tViewportRect = pPass->tPrevViewportRect;
	if ( g_xge.bViewportEnabled ) {
		xgeViewportSet(g_xge.tViewportRect);
	} else {
		xgeViewportClear();
	}
	pPass->bActive = 0;
	return (iRet == XGE_OK) ? XGE_OK : iRet;
}
