int xgeSpriteBatchInit(xge_sprite_batch pBatch, xge_texture pTexture, int iCapacity, uint32_t iFlags)
{
	size_t iVertexSize;

	if ( (pBatch == NULL) || (pTexture == NULL) || (pTexture->iRefCount <= 0) || (iCapacity <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iCapacity > (INT32_MAX / (6 * 10 * (int)sizeof(float))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pBatch, 0, sizeof(*pBatch));
	iVertexSize = (size_t)iCapacity * 6u * 10u * sizeof(float);
	pBatch->pVertices = xrtMalloc(iVertexSize);
	if ( pBatch->pVertices == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pBatch->pTexture = pTexture;
	pBatch->iCapacity = iCapacity;
	pBatch->iFlags = iFlags;
	xgeTextureAddRef(pTexture);
	return XGE_OK;
}

void xgeSpriteBatchFree(xge_sprite_batch pBatch)
{
	if ( pBatch == NULL ) {
		return;
	}
	if ( pBatch->pVertices != NULL ) {
		xrtFree(pBatch->pVertices);
	}
	if ( pBatch->pTexture != NULL ) {
		xgeTextureFree(pBatch->pTexture);
	}
	memset(pBatch, 0, sizeof(*pBatch));
}

void xgeSpriteBatchClear(xge_sprite_batch pBatch)
{
	if ( pBatch != NULL ) {
		pBatch->iCount = 0;
	}
}

static void __xgeSpriteBatchSetVertex(float* pVertices, int iIndex, float fX, float fY, float fU, float fV, float fR, float fG, float fB, float fA)
{
	pVertices[(iIndex * 10) + 0] = fX;
	pVertices[(iIndex * 10) + 1] = fY;
	pVertices[(iIndex * 10) + 2] = 0.0f;
	pVertices[(iIndex * 10) + 3] = 1.0f;
	pVertices[(iIndex * 10) + 4] = fU;
	pVertices[(iIndex * 10) + 5] = fV;
	pVertices[(iIndex * 10) + 6] = fR;
	pVertices[(iIndex * 10) + 7] = fG;
	pVertices[(iIndex * 10) + 8] = fB;
	pVertices[(iIndex * 10) + 9] = fA;
}

int xgeSpriteBatchAdd(xge_sprite_batch pBatch, const xge_draw_t* pDraw)
{
	float fSrcX;
	float fSrcY;
	float fSrcW;
	float fSrcH;
	float fU0;
	float fV0;
	float fU1;
	float fV1;
	float fLocalX[4];
	float fLocalY[4];
	float fX[4];
	float fY[4];
	float fCos;
	float fSin;
	float fR;
	float fG;
	float fB;
	float fA;
	float* pVertices;
	int iBase;
	int i;
	xge_texture pTexture;
	xge_vec2_t tScreen;
	xge_vec2_t tWorld;

	if ( (pBatch == NULL) || (pDraw == NULL) || (pBatch->pTexture == NULL) || (pBatch->pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBatch->iCount >= pBatch->iCapacity ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pDraw->pTexture != pBatch->pTexture ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pTexture = pBatch->pTexture;
	if ( (pTexture->iWidth <= 0) || (pTexture->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}

	fSrcX = pDraw->tSrc.fX;
	fSrcY = pDraw->tSrc.fY;
	fSrcW = pDraw->tSrc.fW;
	fSrcH = pDraw->tSrc.fH;
	if ( fSrcW == 0.0f ) {
		fSrcW = (float)pTexture->iWidth;
	}
	if ( fSrcH == 0.0f ) {
		fSrcH = (float)pTexture->iHeight;
	}
	fU0 = fSrcX / (float)pTexture->iWidth;
	fV0 = fSrcY / (float)pTexture->iHeight;
	fU1 = (fSrcX + fSrcW) / (float)pTexture->iWidth;
	fV1 = (fSrcY + fSrcH) / (float)pTexture->iHeight;
	if ( (pDraw->iFlags & XGE_DRAW_FLIP_X) != 0 ) {
		float fTmp = fU0;
		fU0 = fU1;
		fU1 = fTmp;
	}
	if ( (pDraw->iFlags & XGE_DRAW_FLIP_Y) != 0 ) {
		float fTmp = fV0;
		fV0 = fV1;
		fV1 = fTmp;
	}

	fLocalX[0] = -pDraw->tOrigin.fX;
	fLocalY[0] = -pDraw->tOrigin.fY;
	fLocalX[1] = pDraw->tDst.fW - pDraw->tOrigin.fX;
	fLocalY[1] = -pDraw->tOrigin.fY;
	fLocalX[2] = -pDraw->tOrigin.fX;
	fLocalY[2] = pDraw->tDst.fH - pDraw->tOrigin.fY;
	fLocalX[3] = pDraw->tDst.fW - pDraw->tOrigin.fX;
	fLocalY[3] = pDraw->tDst.fH - pDraw->tOrigin.fY;
	fCos = cosf(pDraw->fRotation);
	fSin = sinf(pDraw->fRotation);

	for ( i = 0; i < 4; i++ ) {
		tWorld.fX = pDraw->tDst.fX + (fLocalX[i] * fCos) - (fLocalY[i] * fSin);
		tWorld.fY = pDraw->tDst.fY + (fLocalX[i] * fSin) + (fLocalY[i] * fCos);
		if ( (pDraw->iFlags & XGE_DRAW_SCREEN_SPACE) != 0 ) {
			tScreen = tWorld;
		} else {
			tScreen = xgeWorldToScreen(tWorld);
		}
		fX[i] = tScreen.fX;
		fY[i] = tScreen.fY;
	}

	__xgeColorToFloat(pDraw->iColor, &fR, &fG, &fB, &fA);
	pVertices = (float*)pBatch->pVertices;
	iBase = pBatch->iCount * 6;
	__xgeSpriteBatchSetVertex(pVertices, iBase + 0, fX[0], fY[0], fU0, fV0, fR, fG, fB, fA);
	__xgeSpriteBatchSetVertex(pVertices, iBase + 1, fX[1], fY[1], fU1, fV0, fR, fG, fB, fA);
	__xgeSpriteBatchSetVertex(pVertices, iBase + 2, fX[2], fY[2], fU0, fV1, fR, fG, fB, fA);
	__xgeSpriteBatchSetVertex(pVertices, iBase + 3, fX[2], fY[2], fU0, fV1, fR, fG, fB, fA);
	__xgeSpriteBatchSetVertex(pVertices, iBase + 4, fX[1], fY[1], fU1, fV0, fR, fG, fB, fA);
	__xgeSpriteBatchSetVertex(pVertices, iBase + 5, fX[3], fY[3], fU1, fV1, fR, fG, fB, fA);
	pBatch->iCount++;
	return XGE_OK;
}

int xgeSpriteBatchFlush(xge_sprite_batch pBatch)
{
	int iVertexCount;

	if ( (pBatch == NULL) || (pBatch->pTexture == NULL) || (pBatch->pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBatch->iCount <= 0 ) {
		return XGE_OK;
	}
	if ( (g_xge.bSokolRunning == 0) || (pBatch->pTexture->iBackendId == 0) ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( __xgeQuad3DRendererInit() != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	iVertexCount = pBatch->iCount * 6;
	glUseProgram(g_xgeQuad3DRenderer.iProgram);
	glUniform2f(g_xgeQuad3DRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform1i(g_xgeQuad3DRenderer.iLocTexture, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (GLuint)pBatch->pTexture->iBackendId);
	glBindVertexArray(g_xgeQuad3DRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeQuad3DRenderer.iVBO);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBufferData(GL_ARRAY_BUFFER, (size_t)iVertexCount * 10u * sizeof(float), pBatch->pVertices, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, iVertexCount);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
	__xgeFrameStatsAddBatch();
	xgeSpriteBatchClear(pBatch);
	return XGE_OK;
}
