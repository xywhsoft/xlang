void xgeDraw(xge_texture pTexture, float fX, float fY)
{
	xge_draw_t tDraw;

	if ( pTexture == NULL ) {
		return;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = pTexture;
	tDraw.tDst.fX = fX;
	tDraw.tDst.fY = fY;
	tDraw.tDst.fW = (float)pTexture->iWidth;
	tDraw.tDst.fH = (float)pTexture->iHeight;
	tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	xgeDrawEx(&tDraw);
}

void xgeDrawPx(xge_texture pTexture, int iX, int iY)
{
	xge_draw_t tDraw;

	if ( pTexture == NULL ) {
		return;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = pTexture;
	tDraw.tDst.fX = (float)iX;
	tDraw.tDst.fY = (float)iY;
	tDraw.tDst.fW = (float)pTexture->iWidth;
	tDraw.tDst.fH = (float)pTexture->iHeight;
	tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tDraw.iFlags = XGE_DRAW_SCREEN_SPACE;
	xgeDrawEx(&tDraw);
}

void xgeDrawEx(const xge_draw_t* pDraw)
{
	(void)__xgeRenderCommandDraw(pDraw);
}

typedef void (*xge_nine_patch_draw_proc)(const xge_draw_t* pDraw, void* pUser);

static float __xgeNinePatchClamp01(float fValue)
{
	if ( fValue < 0.0f ) {
		return 0.0f;
	}
	if ( fValue > 1.0f ) {
		return 1.0f;
	}
	return fValue;
}

static void __xgeNinePatchNormalize(xge_nine_patch pPatch)
{
	float fSwap;

	if ( pPatch == NULL ) {
		return;
	}
	pPatch->fX1 = __xgeNinePatchClamp01(pPatch->fX1);
	pPatch->fY1 = __xgeNinePatchClamp01(pPatch->fY1);
	pPatch->fX2 = __xgeNinePatchClamp01(pPatch->fX2);
	pPatch->fY2 = __xgeNinePatchClamp01(pPatch->fY2);
	if ( pPatch->fX2 < pPatch->fX1 ) {
		fSwap = pPatch->fX1;
		pPatch->fX1 = pPatch->fX2;
		pPatch->fX2 = fSwap;
	}
	if ( pPatch->fY2 < pPatch->fY1 ) {
		fSwap = pPatch->fY1;
		pPatch->fY1 = pPatch->fY2;
		pPatch->fY2 = fSwap;
	}
	if ( pPatch->iMode != XGE_NINE_PATCH_TILE ) {
		pPatch->iMode = XGE_NINE_PATCH_STRETCH;
	}
	pPatch->bEasyMode = (pPatch->fX1 == 0.0f && pPatch->fY1 == 0.0f && pPatch->fX2 == 1.0f && pPatch->fY2 == 1.0f) ? 1 : 0;
}

void xgeNinePatchInitSimple(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc)
{
	if ( pPatch == NULL ) {
		return;
	}
	memset(pPatch, 0, sizeof(*pPatch));
	pPatch->pTexture = pTexture;
	pPatch->tSrc = tSrc;
	pPatch->fX1 = 0.0f;
	pPatch->fY1 = 0.0f;
	pPatch->fX2 = 1.0f;
	pPatch->fY2 = 1.0f;
	pPatch->iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	pPatch->iMode = XGE_NINE_PATCH_STRETCH;
	pPatch->bEasyMode = 1;
}

void xgeNinePatchInit(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc, float fX1, float fY1, float fX2, float fY2)
{
	if ( pPatch == NULL ) {
		return;
	}
	memset(pPatch, 0, sizeof(*pPatch));
	pPatch->pTexture = pTexture;
	pPatch->tSrc = tSrc;
	pPatch->fX1 = fX1;
	pPatch->fY1 = fY1;
	pPatch->fX2 = fX2;
	pPatch->fY2 = fY2;
	pPatch->iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	pPatch->iMode = XGE_NINE_PATCH_STRETCH;
	__xgeNinePatchNormalize(pPatch);
}

void xgeNinePatchSetMode(xge_nine_patch pPatch, int iMode)
{
	if ( pPatch == NULL ) {
		return;
	}
	pPatch->iMode = (iMode == XGE_NINE_PATCH_TILE) ? XGE_NINE_PATCH_TILE : XGE_NINE_PATCH_STRETCH;
	__xgeNinePatchNormalize(pPatch);
}

void xgeNinePatchSetColor(xge_nine_patch pPatch, uint32_t iColor)
{
	if ( pPatch == NULL ) {
		return;
	}
	pPatch->iColor = iColor;
}

static uint32_t __xgeNinePatchMulColor(uint32_t iA, uint32_t iB)
{
	uint32_t r;
	uint32_t g;
	uint32_t b;
	uint32_t a;

	r = (uint32_t)XGE_COLOR_GET_R(iA) * (uint32_t)XGE_COLOR_GET_R(iB) / 255u;
	g = (uint32_t)XGE_COLOR_GET_G(iA) * (uint32_t)XGE_COLOR_GET_G(iB) / 255u;
	b = (uint32_t)XGE_COLOR_GET_B(iA) * (uint32_t)XGE_COLOR_GET_B(iB) / 255u;
	a = (uint32_t)XGE_COLOR_GET_A(iA) * (uint32_t)XGE_COLOR_GET_A(iB) / 255u;
	return XGE_COLOR_RGBA(r, g, b, a);
}

static void __xgeNinePatchDrawStretchPiece(const xge_nine_patch_t* pPatch, xge_rect_t tSrc, xge_rect_t tDst, uint32_t iColor, uint32_t iFlags, xge_nine_patch_draw_proc procDraw, void* pUser)
{
	xge_draw_t tDraw;

	if ( (pPatch == NULL) || (procDraw == NULL) || (pPatch->pTexture == NULL) || (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = pPatch->pTexture;
	tDraw.tSrc = tSrc;
	tDraw.tDst = tDst;
	tDraw.iColor = iColor;
	tDraw.iFlags = iFlags;
	procDraw(&tDraw, pUser);
}

static void __xgeNinePatchDrawTilePiece(const xge_nine_patch_t* pPatch, xge_rect_t tSrc, xge_rect_t tDst, uint32_t iColor, uint32_t iFlags, xge_nine_patch_draw_proc procDraw, void* pUser)
{
	xge_rect_t tTileSrc;
	xge_rect_t tTileDst;
	float fY;
	float fX;
	float fTileW;
	float fTileH;

	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return;
	}
	for ( fY = 0.0f; fY < tDst.fH; fY += tSrc.fH ) {
		fTileH = tSrc.fH;
		if ( fY + fTileH > tDst.fH ) {
			fTileH = tDst.fH - fY;
		}
		for ( fX = 0.0f; fX < tDst.fW; fX += tSrc.fW ) {
			fTileW = tSrc.fW;
			if ( fX + fTileW > tDst.fW ) {
				fTileW = tDst.fW - fX;
			}
			tTileSrc = tSrc;
			tTileSrc.fW = fTileW;
			tTileSrc.fH = fTileH;
			tTileDst.fX = tDst.fX + fX;
			tTileDst.fY = tDst.fY + fY;
			tTileDst.fW = fTileW;
			tTileDst.fH = fTileH;
			__xgeNinePatchDrawStretchPiece(pPatch, tTileSrc, tTileDst, iColor, iFlags, procDraw, pUser);
		}
	}
}

static void __xgeNinePatchDrawPiece(const xge_nine_patch_t* pPatch, xge_rect_t tSrc, xge_rect_t tDst, uint32_t iColor, uint32_t iFlags, int bTile, xge_nine_patch_draw_proc procDraw, void* pUser)
{
	if ( bTile ) {
		__xgeNinePatchDrawTilePiece(pPatch, tSrc, tDst, iColor, iFlags, procDraw, pUser);
	} else {
		__xgeNinePatchDrawStretchPiece(pPatch, tSrc, tDst, iColor, iFlags, procDraw, pUser);
	}
}

static void __xgeNinePatchScaleFixed(float fTotal, float* pA, float* pB, float* pMid)
{
	float fFixed;
	float fScale;

	fFixed = *pA + *pB;
	if ( (fTotal < fFixed) && (fFixed > 0.0f) ) {
		fScale = fTotal / fFixed;
		*pA *= fScale;
		*pB *= fScale;
		*pMid = 0.0f;
	} else {
		*pMid = fTotal - fFixed;
	}
	if ( *pMid < 0.0f ) {
		*pMid = 0.0f;
	}
}

static void __xgeNinePatchDrawInternal(const xge_nine_patch_t* pPatch, xge_rect_t tDst, uint32_t iFlags, uint32_t iDrawColor, xge_nine_patch_draw_proc procDraw, void* pUser)
{
	xge_nine_patch_t tPatch;
	xge_rect_t tSrc;
	xge_rect_t arrSrc[3][3];
	xge_rect_t arrDst[3][3];
	float arrSrcX[4];
	float arrSrcY[4];
	float arrDstX[4];
	float arrDstY[4];
	float fLeftW;
	float fRightW;
	float fMidW;
	float fTopH;
	float fBottomH;
	float fMidH;
	uint32_t iColor;
	int i;
	int j;

	if ( (pPatch == NULL) || (pPatch->pTexture == NULL) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) || (procDraw == NULL) ) {
		return;
	}
	tPatch = *pPatch;
	__xgeNinePatchNormalize(&tPatch);
	if ( XGE_COLOR_GET_A(tPatch.iColor) == 0 ) {
		return;
	}
	iColor = __xgeNinePatchMulColor(tPatch.iColor, iDrawColor);
	if ( XGE_COLOR_GET_A(iColor) == 0 ) {
		return;
	}
	tSrc = tPatch.tSrc;
	if ( tSrc.fW == 0.0f ) {
		tSrc.fW = (float)tPatch.pTexture->iWidth;
	}
	if ( tSrc.fH == 0.0f ) {
		tSrc.fH = (float)tPatch.pTexture->iHeight;
	}
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		return;
	}
	if ( tPatch.bEasyMode ) {
		__xgeNinePatchDrawPiece(&tPatch, tSrc, tDst, iColor, iFlags, tPatch.iMode == XGE_NINE_PATCH_TILE, procDraw, pUser);
		return;
	}
	arrSrcX[0] = tSrc.fX;
	arrSrcX[1] = tSrc.fX + tSrc.fW * tPatch.fX1;
	arrSrcX[2] = tSrc.fX + tSrc.fW * tPatch.fX2;
	arrSrcX[3] = tSrc.fX + tSrc.fW;
	arrSrcY[0] = tSrc.fY;
	arrSrcY[1] = tSrc.fY + tSrc.fH * tPatch.fY1;
	arrSrcY[2] = tSrc.fY + tSrc.fH * tPatch.fY2;
	arrSrcY[3] = tSrc.fY + tSrc.fH;
	fLeftW = arrSrcX[1] - arrSrcX[0];
	fRightW = arrSrcX[3] - arrSrcX[2];
	fTopH = arrSrcY[1] - arrSrcY[0];
	fBottomH = arrSrcY[3] - arrSrcY[2];
	__xgeNinePatchScaleFixed(tDst.fW, &fLeftW, &fRightW, &fMidW);
	__xgeNinePatchScaleFixed(tDst.fH, &fTopH, &fBottomH, &fMidH);
	arrDstX[0] = tDst.fX;
	arrDstX[1] = tDst.fX + fLeftW;
	arrDstX[2] = arrDstX[1] + fMidW;
	arrDstX[3] = tDst.fX + tDst.fW;
	arrDstY[0] = tDst.fY;
	arrDstY[1] = tDst.fY + fTopH;
	arrDstY[2] = arrDstY[1] + fMidH;
	arrDstY[3] = tDst.fY + tDst.fH;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			arrSrc[j][i].fX = arrSrcX[i];
			arrSrc[j][i].fY = arrSrcY[j];
			arrSrc[j][i].fW = arrSrcX[i + 1] - arrSrcX[i];
			arrSrc[j][i].fH = arrSrcY[j + 1] - arrSrcY[j];
			arrDst[j][i].fX = arrDstX[i];
			arrDst[j][i].fY = arrDstY[j];
			arrDst[j][i].fW = arrDstX[i + 1] - arrDstX[i];
			arrDst[j][i].fH = arrDstY[j + 1] - arrDstY[j];
			__xgeNinePatchDrawPiece(&tPatch, arrSrc[j][i], arrDst[j][i], iColor, iFlags, (tPatch.iMode == XGE_NINE_PATCH_TILE) && !((i == 0 || i == 2) && (j == 0 || j == 2)), procDraw, pUser);
		}
	}
}

static void __xgeNinePatchDrawEngineProc(const xge_draw_t* pDraw, void* pUser)
{
	(void)pUser;
	xgeDrawEx(pDraw);
}

void xgeNinePatchDraw(const xge_nine_patch_t* pPatch, xge_rect_t tDst, uint32_t iFlags)
{
	__xgeNinePatchDrawInternal(pPatch, tDst, iFlags, XGE_COLOR_RGBA(255, 255, 255, 255), __xgeNinePatchDrawEngineProc, NULL);
}

static void __xgeDrawExImmediate(const xge_draw_t* pDraw)
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
	float fCos;
	float fSin;
	float fR;
	float fG;
	float fB;
	float fA;
	float arrVertices[16];
	int i;
	xge_texture pTexture;
	xge_vec2_t tScreen;
	xge_texture_yuv420p_t* pYUV;

	if ( (pDraw == NULL) || (pDraw->pTexture == NULL) || (pDraw->pTexture->iBackendId == 0) ) {
		return;
	}
	pTexture = pDraw->pTexture;
	if ( __xgeTextureRendererInit() != XGE_OK ) {
		return;
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
		xge_vec2_t tWorld;
		tWorld.fX = pDraw->tDst.fX + (fLocalX[i] * fCos) - (fLocalY[i] * fSin);
		tWorld.fY = pDraw->tDst.fY + (fLocalX[i] * fSin) + (fLocalY[i] * fCos);
		if ( (pDraw->iFlags & XGE_DRAW_SCREEN_SPACE) != 0 ) {
			tScreen = tWorld;
		} else {
			tScreen = xgeWorldToScreen(tWorld);
		}
		arrVertices[(i * 4) + 0] = tScreen.fX;
		arrVertices[(i * 4) + 1] = tScreen.fY;
	}
	arrVertices[2] = fU0; arrVertices[3] = fV0;
	arrVertices[6] = fU1; arrVertices[7] = fV0;
	arrVertices[10] = fU0; arrVertices[11] = fV1;
	arrVertices[14] = fU1; arrVertices[15] = fV1;

	__xgeColorToFloat(pDraw->iColor, &fR, &fG, &fB, &fA);

	if ( pTexture->iFormat == XGE_PIXEL_YUV420P ) {
		if ( !__xgeTextureHasYUV420P(pTexture) ) {
			return;
		}
		pYUV = (xge_texture_yuv420p_t*)pTexture->pBackend;
		glUseProgram(g_xgeTextureRenderer.iYUVProgram);
		glUniform2f(g_xgeTextureRenderer.iYUVLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
		glUniform1i(g_xgeTextureRenderer.iYUVLocTexY, 0);
		glUniform1i(g_xgeTextureRenderer.iYUVLocTexU, 1);
		glUniform1i(g_xgeTextureRenderer.iYUVLocTexV, 2);
		glUniform4f(g_xgeTextureRenderer.iYUVLocColor, fR, fG, fB, fA);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, pYUV->iPlaneTexture[0]);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, pYUV->iPlaneTexture[1]);
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, pYUV->iPlaneTexture[2]);
	} else {
		glUseProgram(g_xgeTextureRenderer.iProgram);
		glUniform2f(g_xgeTextureRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
		glUniform1i(g_xgeTextureRenderer.iLocTexture, 0);
		glUniform4f(g_xgeTextureRenderer.iLocColor, fR, fG, fB, fA);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, (GLuint)pTexture->iBackendId);
	}
	glBindVertexArray(g_xgeTextureRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeTextureRenderer.iVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(arrVertices), arrVertices);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	if ( pTexture->iFormat == XGE_PIXEL_YUV420P ) {
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
}

void xgeViewportSet(xge_rect_t tRect)
{
	GLint iX;
	GLint iY;
	GLsizei iW;
	GLsizei iH;

	(void)__xgeShapeAutoBatchFlush();
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	g_xge.bViewportEnabled = 1;
	g_xge.tViewportRect = tRect;
	if ( (g_xge.bSokolRunning == 0) || (glViewport == NULL) ) {
		return;
	}
	iX = (GLint)tRect.fX;
	iY = (GLint)((float)g_xge.iHeight - tRect.fY - tRect.fH);
	iW = (GLsizei)tRect.fW;
	iH = (GLsizei)tRect.fH;
	glViewport(iX, iY, iW, iH);
}

xge_rect_t xgeViewportGet(void)
{
	xge_rect_t tRect;

	if ( g_xge.bViewportEnabled ) {
		return g_xge.tViewportRect;
	}
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect.fW = (float)g_xge.iWidth;
	tRect.fH = (float)g_xge.iHeight;
	return tRect;
}

void xgeViewportClear(void)
{
	xge_rect_t tRect;

	(void)__xgeShapeAutoBatchFlush();
	g_xge.bViewportEnabled = 0;
	memset(&g_xge.tViewportRect, 0, sizeof(g_xge.tViewportRect));
	tRect = xgeViewportGet();
	if ( (g_xge.bSokolRunning == 0) || (glViewport == NULL) ) {
		return;
	}
	glViewport((GLint)tRect.fX, (GLint)tRect.fY, (GLsizei)tRect.fW, (GLsizei)tRect.fH);
}

void xgeClipSet(xge_rect_t tRect)
{
	GLint iX;
	GLint iY;
	GLsizei iW;
	GLsizei iH;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fScissorBottom;

	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	g_xge.bClipEnabled = 1;
	g_xge.tClipRect = tRect;
	if ( (g_xge.bSokolRunning == 0) || (glScissor == NULL) ) {
		return;
	}
	fLeft = floorf(tRect.fX);
	fTop = floorf(tRect.fY);
	fRight = ceilf(tRect.fX + tRect.fW);
	fBottom = ceilf(tRect.fY + tRect.fH);
	if ( fRight < fLeft ) {
		fRight = fLeft;
	}
	if ( fBottom < fTop ) {
		fBottom = fTop;
	}
	if ( fLeft < 0.0f ) {
		fLeft = 0.0f;
	}
	if ( fTop < 0.0f ) {
		fTop = 0.0f;
	}
	if ( fRight > (float)g_xge.iWidth ) {
		fRight = (float)g_xge.iWidth;
	}
	if ( fBottom > (float)g_xge.iHeight ) {
		fBottom = (float)g_xge.iHeight;
	}
	if ( fRight < fLeft ) {
		fRight = fLeft;
	}
	if ( fBottom < fTop ) {
		fBottom = fTop;
	}
	fScissorBottom = floorf((float)g_xge.iHeight - fBottom);
	iX = (GLint)fLeft;
	iY = (GLint)fScissorBottom;
	iW = (GLsizei)(fRight - fLeft);
	iH = (GLsizei)(fBottom - fTop);
	glEnable(GL_SCISSOR_TEST);
	glScissor(iX, iY, iW, iH);
}

xge_rect_t xgeClipGet(void)
{
	xge_rect_t tRect;

	if ( g_xge.bClipEnabled ) {
		return g_xge.tClipRect;
	}
	memset(&tRect, 0, sizeof(tRect));
	return tRect;
}

void xgeClipClear(void)
{
	g_xge.bClipEnabled = 0;
	memset(&g_xge.tClipRect, 0, sizeof(g_xge.tClipRect));
	if ( (g_xge.bSokolRunning == 0) || (glDisable == NULL) ) {
		return;
	}
	glDisable(GL_SCISSOR_TEST);
}

#if defined(_WIN32) || defined(_WIN64)
#define XGE_WIN32_CLIPBOARD_RETRY_COUNT 10
#define XGE_WIN32_CLIPBOARD_RETRY_DELAY_MS 1

static int __xgeWin32ClipboardCacheReserve(size_t iCapacity)
{
	char* sNew;

	if ( iCapacity <= g_xge.iClipboardTextCapacity ) {
		return 1;
	}
	sNew = (char*)xrtRealloc(g_xge.sClipboardText, iCapacity);
	if ( sNew == NULL ) {
		return 0;
	}
	g_xge.sClipboardText = sNew;
	g_xge.iClipboardTextCapacity = iCapacity;
	return 1;
}

static HWND __xgeWin32ClipboardOwner(void)
{
	if ( g_xge.bSokolRunning ) {
		return (HWND)sapp_win32_get_hwnd();
	}
	return NULL;
}

static int __xgeWin32ClipboardOpen(void)
{
	int iAttempt;

	for ( iAttempt = 0; iAttempt < XGE_WIN32_CLIPBOARD_RETRY_COUNT; iAttempt++ ) {
		if ( OpenClipboard(__xgeWin32ClipboardOwner()) ) {
			return 1;
		}
		if ( iAttempt + 1 < XGE_WIN32_CLIPBOARD_RETRY_COUNT ) {
			Sleep(XGE_WIN32_CLIPBOARD_RETRY_DELAY_MS);
		}
	}
	return 0;
}

static const char* __xgeWin32ClipboardCacheEmpty(DWORD iSequence)
{
	if ( __xgeWin32ClipboardCacheReserve(1u) ) {
		g_xge.sClipboardText[0] = '\0';
		g_xge.iClipboardSequence = (uint32_t)iSequence;
		g_xge.iClipboardCacheValid = 1;
		return g_xge.sClipboardText;
	}
	g_xge.iClipboardSequence = 0u;
	g_xge.iClipboardCacheValid = 0;
	return "";
}

static const char* __xgeWin32ClipboardCacheInvalidate(void)
{
	if ( __xgeWin32ClipboardCacheReserve(1u) ) g_xge.sClipboardText[0] = '\0';
	g_xge.iClipboardSequence = 0u;
	g_xge.iClipboardCacheValid = 0;
	return NULL;
}

static int __xgeWin32ClipboardSetText(const char* sText)
{
	HGLOBAL hMemory;
	WCHAR* sWide;
	int iWideCount;
	int bSuccess;
	size_t iTextSize;
	DWORD iSequence;

	iWideCount = MultiByteToWideChar(CP_UTF8, 0, sText, -1, NULL, 0);
	if ( iWideCount <= 0 ) {
		return 0;
	}
	hMemory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)iWideCount * sizeof(WCHAR));
	if ( hMemory == NULL ) {
		return 0;
	}
	sWide = (WCHAR*)GlobalLock(hMemory);
	if ( sWide == NULL ) {
		GlobalFree(hMemory);
		return 0;
	}
	if ( MultiByteToWideChar(CP_UTF8, 0, sText, -1, sWide, iWideCount) <= 0 ) {
		GlobalUnlock(hMemory);
		GlobalFree(hMemory);
		return 0;
	}
	GlobalUnlock(hMemory);
	if ( !__xgeWin32ClipboardOpen() ) {
		GlobalFree(hMemory);
		return 0;
	}
	bSuccess = 0;
	iSequence = 0u;
	if ( EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, hMemory) != NULL ) {
		bSuccess = 1;
		hMemory = NULL;
		iSequence = GetClipboardSequenceNumber();
	}
	CloseClipboard();
	if ( hMemory != NULL ) {
		GlobalFree(hMemory);
	}
	if ( !bSuccess ) {
		return 0;
	}
	iTextSize = strlen(sText) + 1u;
	if ( __xgeWin32ClipboardCacheReserve(iTextSize) ) {
		memcpy(g_xge.sClipboardText, sText, iTextSize);
		g_xge.iClipboardSequence = (uint32_t)iSequence;
		g_xge.iClipboardCacheValid = 1;
	} else {
		g_xge.iClipboardSequence = 0u;
		g_xge.iClipboardCacheValid = 0;
	}
	return 1;
}

static const char* __xgeWin32ClipboardGetText(void)
{
	HANDLE hData;
	const WCHAR* sWide;
	DWORD iSequence;
	DWORD iReadSequence;
	int iUtf8Size;
	int iConverted;
	int iAttempt;

	iSequence = GetClipboardSequenceNumber();
	if ( g_xge.iClipboardCacheValid && iSequence != 0u && g_xge.sClipboardText != NULL && g_xge.iClipboardSequence == (uint32_t)iSequence ) {
		return g_xge.sClipboardText;
	}
	if ( !IsClipboardFormatAvailable(CF_UNICODETEXT) ) {
		return __xgeWin32ClipboardCacheEmpty(iSequence);
	}
	hData = NULL;
	iReadSequence = 0u;
	for ( iAttempt = 0; iAttempt < XGE_WIN32_CLIPBOARD_RETRY_COUNT; iAttempt++ ) {
		if ( OpenClipboard(__xgeWin32ClipboardOwner()) ) {
			iReadSequence = GetClipboardSequenceNumber();
			hData = GetClipboardData(CF_UNICODETEXT);
			if ( hData != NULL ) {
				break;
			}
			CloseClipboard();
		}
		if ( iAttempt + 1 < XGE_WIN32_CLIPBOARD_RETRY_COUNT ) {
			Sleep(XGE_WIN32_CLIPBOARD_RETRY_DELAY_MS);
		}
	}
	if ( hData == NULL ) {
		return __xgeWin32ClipboardCacheInvalidate();
	}
	sWide = (const WCHAR*)GlobalLock(hData);
	if ( sWide == NULL ) {
		CloseClipboard();
		return __xgeWin32ClipboardCacheInvalidate();
	}
	iUtf8Size = WideCharToMultiByte(CP_UTF8, 0, sWide, -1, NULL, 0, NULL, NULL);
	if ( iUtf8Size <= 0 || !__xgeWin32ClipboardCacheReserve((size_t)iUtf8Size) ) {
		GlobalUnlock(hData);
		CloseClipboard();
		return __xgeWin32ClipboardCacheInvalidate();
	}
	iConverted = WideCharToMultiByte(CP_UTF8, 0, sWide, -1, g_xge.sClipboardText, iUtf8Size, NULL, NULL);
	GlobalUnlock(hData);
	CloseClipboard();
	if ( iConverted <= 0 ) {
		return __xgeWin32ClipboardCacheInvalidate();
	}
	g_xge.iClipboardSequence = (uint32_t)iReadSequence;
	g_xge.iClipboardCacheValid = 1;
	return g_xge.sClipboardText;
}
#endif

void xgeClipboardSetText(const char* sText)
{
	if ( sText == NULL ) {
		sText = "";
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( g_xge.bInitialized ) {
			(void)__xgeWin32ClipboardSetText(sText);
			return;
		}
	#endif
	if ( g_xge.bSokolRunning == 0 ) {
		return;
	}
	sapp_set_clipboard_string(sText);
}

const char* xgeClipboardGetText(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		const char* sText;
		if ( g_xge.bInitialized ) {
			sText = __xgeWin32ClipboardGetText();
			return (sText != NULL) ? sText : "";
		}
	#endif
	if ( g_xge.bSokolRunning == 0 ) {
		return "";
	}
	return sapp_get_clipboard_string();
}

