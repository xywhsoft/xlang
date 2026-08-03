static GLenum __xgeBufferTarget(int iType)
{
	if ( iType == XGE_BUFFER_VERTEX ) {
		return GL_ARRAY_BUFFER;
	}
	if ( iType == XGE_BUFFER_INDEX ) {
		return GL_ELEMENT_ARRAY_BUFFER;
	}
	return 0;
}

static GLenum __xgeBufferUsage(int iUsage)
{
	if ( iUsage == XGE_BUFFER_DYNAMIC ) {
		return GL_DYNAMIC_DRAW;
	}
	return GL_STATIC_DRAW;
}

int xgeBufferUpload(xge_buffer pBuffer)
{
	GLenum iTarget;
	GLuint iBuffer;

	if ( (pBuffer == NULL) || (pBuffer->iSize <= 0) || (pBuffer->pData == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iTarget = __xgeBufferTarget(pBuffer->iType);
	if ( iTarget == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( (glGenBuffers == NULL) || (glBindBuffer == NULL) || (glBufferData == NULL) ) {
		return XGE_ERROR_GPU_FAILED;
	}
	if ( pBuffer->iBackendId == 0 ) {
		glGenBuffers(1, &iBuffer);
		if ( iBuffer == 0 ) {
			return XGE_ERROR_GPU_FAILED;
		}
		pBuffer->iBackendId = iBuffer;
	}
	glBindBuffer(iTarget, (GLuint)pBuffer->iBackendId);
	glBufferData(iTarget, pBuffer->iSize, pBuffer->pData, __xgeBufferUsage(pBuffer->iUsage));
	glBindBuffer(iTarget, 0);
	return XGE_OK;
}

int xgeBufferCreate(xge_buffer pBuffer, int iType, int iUsage, const void* pData, int iSize)
{
	int iRet;

	if ( (pBuffer == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iType != XGE_BUFFER_VERTEX) && (iType != XGE_BUFFER_INDEX) && (iType != XGE_BUFFER_UNIFORM) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iUsage != XGE_BUFFER_STATIC) && (iUsage != XGE_BUFFER_DYNAMIC) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBuffer, 0, sizeof(*pBuffer));
	pBuffer->pData = xrtMalloc((size_t)iSize);
	if ( pBuffer->pData == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pBuffer->iType = iType;
	pBuffer->iUsage = iUsage;
	pBuffer->iSize = iSize;
	if ( pData != NULL ) {
		memcpy(pBuffer->pData, pData, (size_t)iSize);
	} else {
		memset(pBuffer->pData, 0, (size_t)iSize);
	}
	if ( (g_xge.bSokolRunning != 0) && (iType != XGE_BUFFER_UNIFORM) ) {
		iRet = xgeBufferUpload(pBuffer);
		if ( iRet != XGE_OK ) {
			xgeBufferFree(pBuffer);
			return iRet;
		}
	}
	return XGE_OK;
}

int xgeBufferUpdate(xge_buffer pBuffer, int iOffset, const void* pData, int iSize)
{
	GLenum iTarget;

	if ( (pBuffer == NULL) || (pData == NULL) || (iOffset < 0) || (iSize <= 0) || (pBuffer->pData == NULL) || (pBuffer->iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iOffset > (pBuffer->iSize - iSize) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memcpy(((unsigned char*)pBuffer->pData) + iOffset, pData, (size_t)iSize);
	if ( (g_xge.bSokolRunning != 0) && (pBuffer->iType != XGE_BUFFER_UNIFORM) ) {
		if ( pBuffer->iBackendId == 0 ) {
			return xgeBufferUpload(pBuffer);
		}
		iTarget = __xgeBufferTarget(pBuffer->iType);
		if ( iTarget == 0 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		if ( (glBindBuffer == NULL) || (glBufferSubData == NULL) ) {
			return XGE_ERROR_GPU_FAILED;
		}
		glBindBuffer(iTarget, (GLuint)pBuffer->iBackendId);
		glBufferSubData(iTarget, iOffset, iSize, pData);
		glBindBuffer(iTarget, 0);
	}
	return XGE_OK;
}

void xgeBufferFree(xge_buffer pBuffer)
{
	GLuint iBuffer;

	if ( pBuffer == NULL ) {
		return;
	}
	iBuffer = (GLuint)pBuffer->iBackendId;
	if ( (iBuffer != 0) && (g_xge.bSokolRunning != 0) && (glDeleteBuffers != NULL) ) {
		glDeleteBuffers(1, &iBuffer);
	}
	if ( pBuffer->pData != NULL ) {
		xrtFree(pBuffer->pData);
	}
	memset(pBuffer, 0, sizeof(*pBuffer));
}
