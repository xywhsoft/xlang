#define XGE_SHAPE_AUTO_VERTEX_FLOATS 6
#define XGE_SHAPE_CIRCLE_SEGMENTS 48
#define XGE_SHAPE_PI 3.14159265358979323846f
#define XGE_SHAPE_TAU 6.28318530717958647692f
#define XGE_SHAPE_HALF_PI 1.57079632679489661923f
#define XGE_SHAPE_MAX_SEGMENTS 512
#define XGE_SHAPE_CURVE_TOLERANCE 0.25f
#define XGE_SHAPE_AA_FRINGE 1.0f
#define XGE_SHAPE_SDF_FRINGE 2.0f

static int g_iXgeShapeRenderMode = XGE_SHAPE_RENDER_SDF;
static int g_iXgeShapeRoundRectMode = XGE_SHAPE_ROUND_RECT_AUTO;

static int __xgeShapeMeshFill(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, int bScreenSpace);

static float g_arrShapeCircleCos[XGE_SHAPE_CIRCLE_SEGMENTS];
static float g_arrShapeCircleSin[XGE_SHAPE_CIRCLE_SEGMENTS];
static int g_bShapeCircleTableReady;

static void __xgeShapeCircleTableInit(void)
{
	float fStep;
	int i;

	if ( g_bShapeCircleTableReady != 0 ) {
		return;
	}
	fStep = 6.28318530718f / (float)XGE_SHAPE_CIRCLE_SEGMENTS;
	for ( i = 0; i < XGE_SHAPE_CIRCLE_SEGMENTS; i++ ) {
		g_arrShapeCircleCos[i] = cosf(fStep * (float)i);
		g_arrShapeCircleSin[i] = sinf(fStep * (float)i);
	}
	g_bShapeCircleTableReady = 1;
}

static void __xgeShapeAutoBatchReset(void)
{
	g_xge.iShapeAutoVertexCount = 0;
	g_xge.iShapeAutoIndexCount = 0;
	if ( (g_xge.bInitialized == 0) && (g_xge.pShapeAutoVertices != NULL) ) {
		xrtFree(g_xge.pShapeAutoVertices);
		g_xge.pShapeAutoVertices = NULL;
		g_xge.iShapeAutoVertexCapacity = 0;
	}
	if ( (g_xge.bInitialized == 0) && (g_xge.pShapeAutoIndices != NULL) ) {
		xrtFree(g_xge.pShapeAutoIndices);
		g_xge.pShapeAutoIndices = NULL;
		g_xge.iShapeAutoIndexCapacity = 0;
	}
}

static int __xgeShapeAutoBatchReserveVertices(int iNeeded)
{
	void* pVertices;
	int iCapacity;

	if ( iNeeded <= g_xge.iShapeAutoVertexCapacity ) {
		return XGE_OK;
	}
	iCapacity = (g_xge.iShapeAutoVertexCapacity > 0) ? g_xge.iShapeAutoVertexCapacity : 4096;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	if ( iCapacity > (INT32_MAX / ((int)sizeof(float) * XGE_SHAPE_AUTO_VERTEX_FLOATS)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pVertices = xrtRealloc(g_xge.pShapeAutoVertices, (size_t)iCapacity * XGE_SHAPE_AUTO_VERTEX_FLOATS * sizeof(float));
	if ( pVertices == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	g_xge.pShapeAutoVertices = pVertices;
	g_xge.iShapeAutoVertexCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeAutoBatchReserveIndices(int iNeeded)
{
	void* pIndices;
	int iCapacity;

	if ( iNeeded <= g_xge.iShapeAutoIndexCapacity ) {
		return XGE_OK;
	}
	iCapacity = (g_xge.iShapeAutoIndexCapacity > 0) ? g_xge.iShapeAutoIndexCapacity : 4096;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	if ( iCapacity > (INT32_MAX / (int)sizeof(uint32_t)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pIndices = xrtRealloc(g_xge.pShapeAutoIndices, (size_t)iCapacity * sizeof(uint32_t));
	if ( pIndices == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	g_xge.pShapeAutoIndices = pIndices;
	g_xge.iShapeAutoIndexCapacity = iCapacity;
	return XGE_OK;
}

static void __xgeShapeAutoBatchSetVertex(float* pVertices, int iIndex, float fX, float fY, uint32_t iColor, int bScreenSpace)
{
	float fSX;
	float fSY;

	__xgeCameraProjectVertex(fX, fY, 0.0f, bScreenSpace ? XGE_DRAW_SCREEN_SPACE : 0, &fSX, &fSY);
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 0] = fSX;
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 1] = fSY;
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 5] = (float)XGE_COLOR_GET_A(iColor) / 255.0f;
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 2] = ((float)XGE_COLOR_GET_R(iColor) / 255.0f) * pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 5];
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 3] = ((float)XGE_COLOR_GET_G(iColor) / 255.0f) * pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 5];
	pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 4] = ((float)XGE_COLOR_GET_B(iColor) / 255.0f) * pVertices[(iIndex * XGE_SHAPE_AUTO_VERTEX_FLOATS) + 5];
}

static int __xgeShapeAutoBatchFlush(void)
{
	if ( g_xge.iShapeAutoVertexCount <= 0 ) {
		return XGE_OK;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		g_xge.iShapeAutoVertexCount = 0;
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		g_xge.iShapeAutoVertexCount = 0;
		g_xge.iShapeAutoIndexCount = 0;
		return XGE_ERROR_GPU_FAILED;
	}
	glUseProgram(g_xgeShapeRenderer.iProgram);
	glUniform2f(g_xgeShapeRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iLocColor, 1.0f, 1.0f, 1.0f, 1.0f);
	glUniform1i(g_xgeShapeRenderer.iLocUseVertexColor, 1);
	glBindVertexArray(g_xgeShapeRenderer.iColorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iColorVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_xgeShapeRenderer.iColorEBO);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, XGE_SHAPE_AUTO_VERTEX_FLOATS * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, XGE_SHAPE_AUTO_VERTEX_FLOATS * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBufferData(GL_ARRAY_BUFFER, (size_t)g_xge.iShapeAutoVertexCount * XGE_SHAPE_AUTO_VERTEX_FLOATS * sizeof(float), g_xge.pShapeAutoVertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (size_t)g_xge.iShapeAutoIndexCount * sizeof(uint32_t), g_xge.pShapeAutoIndices, GL_DYNAMIC_DRAW);
	glDrawElements(GL_TRIANGLES, g_xge.iShapeAutoIndexCount, GL_UNSIGNED_INT, (void*)0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
	__xgeFrameStatsAddBatch();
	g_xge.iShapeAutoVertexCount = 0;
	g_xge.iShapeAutoIndexCount = 0;
	return XGE_OK;
}

static int __xgeShapeAutoBatchCircleFill(float fX, float fY, float fRadius, uint32_t iColor, int bScreenSpace)
{
	float* pVertices;
	uint32_t* pIndices;
	int iVertexBase;
	int iIndexBase;
	int i;
	int iRet;

	if ( (fRadius <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeAutoBatchReserveVertices(g_xge.iShapeAutoVertexCount + XGE_SHAPE_CIRCLE_SEGMENTS + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeAutoBatchReserveIndices(g_xge.iShapeAutoIndexCount + (XGE_SHAPE_CIRCLE_SEGMENTS * 3));
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	__xgeShapeCircleTableInit();
	pVertices = (float*)g_xge.pShapeAutoVertices;
	pIndices = (uint32_t*)g_xge.pShapeAutoIndices;
	iVertexBase = g_xge.iShapeAutoVertexCount;
	iIndexBase = g_xge.iShapeAutoIndexCount;
	__xgeShapeAutoBatchSetVertex(pVertices, iVertexBase, fX, fY, iColor, bScreenSpace);
	for ( i = 0; i < XGE_SHAPE_CIRCLE_SEGMENTS; i++ ) {
		__xgeShapeAutoBatchSetVertex(pVertices, iVertexBase + 1 + i, fX + g_arrShapeCircleCos[i] * fRadius, fY + g_arrShapeCircleSin[i] * fRadius, iColor, bScreenSpace);
		pIndices[iIndexBase + (i * 3) + 0] = (uint32_t)iVertexBase;
		pIndices[iIndexBase + (i * 3) + 1] = (uint32_t)(iVertexBase + 1 + i);
		pIndices[iIndexBase + (i * 3) + 2] = (uint32_t)(iVertexBase + 1 + ((i + 1) % XGE_SHAPE_CIRCLE_SEGMENTS));
	}
	g_xge.iShapeAutoVertexCount += XGE_SHAPE_CIRCLE_SEGMENTS + 1;
	g_xge.iShapeAutoIndexCount += XGE_SHAPE_CIRCLE_SEGMENTS * 3;
	return XGE_OK;
}

static xge_shape_round_rect_t __xgeShapeRoundRectRadii(float fRadius)
{
	xge_shape_round_rect_t tRadii;

	tRadii.fTopLeft = fRadius;
	tRadii.fTopRight = fRadius;
	tRadii.fBottomRight = fRadius;
	tRadii.fBottomLeft = fRadius;
	return tRadii;
}

static float __xgeShapeClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static float __xgeShapeMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xgeShapeMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static int __xgeShapeColorVisible(uint32_t iColor)
{
	return XGE_COLOR_GET_A(iColor) != 0;
}

static int __xgeShapeArcSegmentCount(float fRadius, float fSweep)
{
	float fTolerance;
	float fCosValue;
	float fMaxAngle;
	int iSegments;

	fRadius = fabsf(fRadius);
	fSweep = fabsf(fSweep);
	if ( (fRadius <= 0.0f) || (fSweep <= 0.0f) ) {
		return 1;
	}
	fTolerance = XGE_SHAPE_CURVE_TOLERANCE;
	if ( fRadius <= fTolerance ) {
		return 4;
	}
	fCosValue = 1.0f - (fTolerance / fRadius);
	fCosValue = __xgeShapeClampFloat(fCosValue, -1.0f, 1.0f);
	fMaxAngle = 2.0f * acosf(fCosValue);
	if ( fMaxAngle <= 0.001f ) {
		fMaxAngle = XGE_SHAPE_PI / 64.0f;
	}
	iSegments = (int)ceilf(fSweep / fMaxAngle);
	if ( iSegments < 1 ) {
		iSegments = 1;
	}
	if ( iSegments > XGE_SHAPE_MAX_SEGMENTS ) {
		iSegments = XGE_SHAPE_MAX_SEGMENTS;
	}
	return iSegments;
}

static int __xgeShapeEllipseSegmentCount(float fRadiusX, float fRadiusY, float fSweep)
{
	return __xgeShapeArcSegmentCount(__xgeShapeMaxFloat(fabsf(fRadiusX), fabsf(fRadiusY)), fSweep);
}

static void __xgeShapeNormalizeRoundRectRadii(xge_rect_t tRect, xge_shape_round_rect_t* pRadii)
{
	float fScale;
	float fSum;

	if ( pRadii == NULL ) {
		return;
	}
	pRadii->fTopLeft = __xgeShapeMaxFloat(0.0f, pRadii->fTopLeft);
	pRadii->fTopRight = __xgeShapeMaxFloat(0.0f, pRadii->fTopRight);
	pRadii->fBottomRight = __xgeShapeMaxFloat(0.0f, pRadii->fBottomRight);
	pRadii->fBottomLeft = __xgeShapeMaxFloat(0.0f, pRadii->fBottomLeft);
	fScale = 1.0f;
	fSum = pRadii->fTopLeft + pRadii->fTopRight;
	if ( fSum > tRect.fW && fSum > 0.0f ) {
		fScale = __xgeShapeMinFloat(fScale, tRect.fW / fSum);
	}
	fSum = pRadii->fBottomLeft + pRadii->fBottomRight;
	if ( fSum > tRect.fW && fSum > 0.0f ) {
		fScale = __xgeShapeMinFloat(fScale, tRect.fW / fSum);
	}
	fSum = pRadii->fTopLeft + pRadii->fBottomLeft;
	if ( fSum > tRect.fH && fSum > 0.0f ) {
		fScale = __xgeShapeMinFloat(fScale, tRect.fH / fSum);
	}
	fSum = pRadii->fTopRight + pRadii->fBottomRight;
	if ( fSum > tRect.fH && fSum > 0.0f ) {
		fScale = __xgeShapeMinFloat(fScale, tRect.fH / fSum);
	}
	if ( fScale < 1.0f ) {
		pRadii->fTopLeft *= fScale;
		pRadii->fTopRight *= fScale;
		pRadii->fBottomRight *= fScale;
		pRadii->fBottomLeft *= fScale;
	}
}

static int __xgeShapeBuildArcPoints(xge_vec2_t* pPoints, int iCapacity, int* pCount, float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, int iSegments)
{
	float fStep;
	float fAngle;
	int i;

	if ( (pPoints == NULL) || (pCount == NULL) || (iSegments < 1) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (*pCount + iSegments + 1) > iCapacity ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fStep = (fEndRadians - fStartRadians) / (float)iSegments;
	for ( i = 0; i <= iSegments; i++ ) {
		fAngle = fStartRadians + fStep * (float)i;
		pPoints[*pCount].fX = fX + cosf(fAngle) * fRadiusX;
		pPoints[*pCount].fY = fY + sinf(fAngle) * fRadiusY;
		(*pCount)++;
	}
	return XGE_OK;
}

static int __xgeShapeRoundRectCornerSegments(xge_shape_round_rect_t tRadii, int* pSegments)
{
	if ( pSegments == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSegments[0] = (tRadii.fTopRight > 0.0f) ? __xgeShapeArcSegmentCount(tRadii.fTopRight, XGE_SHAPE_HALF_PI) : 0;
	pSegments[1] = (tRadii.fBottomRight > 0.0f) ? __xgeShapeArcSegmentCount(tRadii.fBottomRight, XGE_SHAPE_HALF_PI) : 0;
	pSegments[2] = (tRadii.fBottomLeft > 0.0f) ? __xgeShapeArcSegmentCount(tRadii.fBottomLeft, XGE_SHAPE_HALF_PI) : 0;
	pSegments[3] = (tRadii.fTopLeft > 0.0f) ? __xgeShapeArcSegmentCount(tRadii.fTopLeft, XGE_SHAPE_HALF_PI) : 0;
	return XGE_OK;
}

static int __xgeShapeRoundRectOutlineCount(const int* pSegments)
{
	return (pSegments[0] + 1) + (pSegments[1] + 1) + (pSegments[2] + 1) + (pSegments[3] + 1);
}

static int __xgeShapeBuildRoundRectCorner(xge_vec2_t* pPoints, int iCapacity, int* pCount, float fCenterX, float fCenterY, float fRadius, float fStartRadians, float fEndRadians, int iSegments, float fCornerX, float fCornerY)
{
	if ( (pPoints == NULL) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (fRadius <= 0.0f) || (iSegments <= 0) ) {
		if ( (*pCount + 1) > iCapacity ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pPoints[*pCount].fX = fCornerX;
		pPoints[*pCount].fY = fCornerY;
		(*pCount)++;
		return XGE_OK;
	}
	return __xgeShapeBuildArcPoints(pPoints, iCapacity, pCount, fCenterX, fCenterY, fRadius, fRadius, fStartRadians, fEndRadians, iSegments);
}

static int __xgeShapeCleanClosedPoints(xge_vec2_t* pPoints, int iCount)
{
	int iWrite;
	int i;

	if ( (pPoints == NULL) || (iCount <= 1) ) {
		return iCount;
	}
	iWrite = 1;
	for ( i = 1; i < iCount; i++ ) {
		float fDX = pPoints[i].fX - pPoints[iWrite - 1].fX;
		float fDY = pPoints[i].fY - pPoints[iWrite - 1].fY;
		if ( ((fDX * fDX) + (fDY * fDY)) > 0.0001f ) {
			pPoints[iWrite++] = pPoints[i];
		}
	}
	if ( iWrite > 1 ) {
		float fDX = pPoints[0].fX - pPoints[iWrite - 1].fX;
		float fDY = pPoints[0].fY - pPoints[iWrite - 1].fY;
		if ( ((fDX * fDX) + (fDY * fDY)) <= 0.0001f ) {
			iWrite--;
		}
	}
	return iWrite;
}

static int __xgeShapeBuildRoundRectOutline(xge_rect_t tRect, xge_shape_round_rect_t tRadii, const int* pSegments, xge_vec2_t* pPoints, int iCapacity, int* pCount)
{
	int iRet;

	if ( (pSegments == NULL) || (pPoints == NULL) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCount = 0;
	iRet = __xgeShapeBuildRoundRectCorner(pPoints, iCapacity, pCount, tRect.fX + tRect.fW - tRadii.fTopRight, tRect.fY + tRadii.fTopRight, tRadii.fTopRight, -XGE_SHAPE_HALF_PI, 0.0f, pSegments[0], tRect.fX + tRect.fW, tRect.fY);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeBuildRoundRectCorner(pPoints, iCapacity, pCount, tRect.fX + tRect.fW - tRadii.fBottomRight, tRect.fY + tRect.fH - tRadii.fBottomRight, tRadii.fBottomRight, 0.0f, XGE_SHAPE_HALF_PI, pSegments[1], tRect.fX + tRect.fW, tRect.fY + tRect.fH);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeBuildRoundRectCorner(pPoints, iCapacity, pCount, tRect.fX + tRadii.fBottomLeft, tRect.fY + tRect.fH - tRadii.fBottomLeft, tRadii.fBottomLeft, XGE_SHAPE_HALF_PI, XGE_SHAPE_PI, pSegments[2], tRect.fX, tRect.fY + tRect.fH);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeBuildRoundRectCorner(pPoints, iCapacity, pCount, tRect.fX + tRadii.fTopLeft, tRect.fY + tRadii.fTopLeft, tRadii.fTopLeft, XGE_SHAPE_PI, XGE_SHAPE_PI + XGE_SHAPE_HALF_PI, pSegments[3], tRect.fX, tRect.fY);
	if ( iRet == XGE_OK ) {
		*pCount = __xgeShapeCleanClosedPoints(pPoints, *pCount);
	}
	return iRet;
}

static uint32_t __xgeShapeTransparentColor(void)
{
	return XGE_COLOR_RGBA(0, 0, 0, 0);
}

static float __xgeShapePolygonSignedArea(const xge_vec2_t* pPoints, int iCount)
{
	float fArea;
	int i;

	fArea = 0.0f;
	if ( (pPoints == NULL) || (iCount < 3) ) {
		return 0.0f;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xge_vec2_t* pA = &pPoints[i];
		const xge_vec2_t* pB = &pPoints[(i + 1) % iCount];
		fArea += (pA->fX * pB->fY) - (pB->fX * pA->fY);
	}
	return fArea * 0.5f;
}

static void __xgeShapeEdgeOutwardNormal(xge_vec2_t tA, xge_vec2_t tB, int bPositiveArea, float* pNX, float* pNY)
{
	float fDX;
	float fDY;
	float fLen;

	fDX = tB.fX - tA.fX;
	fDY = tB.fY - tA.fY;
	fLen = sqrtf((fDX * fDX) + (fDY * fDY));
	if ( fLen <= 0.00001f ) {
		*pNX = 0.0f;
		*pNY = 0.0f;
		return;
	}
	if ( bPositiveArea != 0 ) {
		*pNX = fDY / fLen;
		*pNY = -fDX / fLen;
	} else {
		*pNX = -fDY / fLen;
		*pNY = fDX / fLen;
	}
}

static void __xgeShapeBuildOffsetPoints(const xge_vec2_t* pPoints, int iCount, float fOffset, xge_vec2_t* pOut)
{
	float fArea;
	int bPositiveArea;
	int i;

	if ( (pPoints == NULL) || (pOut == NULL) || (iCount <= 0) ) {
		return;
	}
	fArea = __xgeShapePolygonSignedArea(pPoints, iCount);
	bPositiveArea = (fArea >= 0.0f) ? 1 : 0;
	for ( i = 0; i < iCount; i++ ) {
		int iPrev = (i + iCount - 1) % iCount;
		int iNext = (i + 1) % iCount;
		float fN0X;
		float fN0Y;
		float fN1X;
		float fN1Y;
		float fMX;
		float fMY;
		float fMLen;
		float fDenom;
		float fScale;

		__xgeShapeEdgeOutwardNormal(pPoints[iPrev], pPoints[i], bPositiveArea, &fN0X, &fN0Y);
		__xgeShapeEdgeOutwardNormal(pPoints[i], pPoints[iNext], bPositiveArea, &fN1X, &fN1Y);
		fMX = fN0X + fN1X;
		fMY = fN0Y + fN1Y;
		fMLen = sqrtf((fMX * fMX) + (fMY * fMY));
		if ( fMLen <= 0.00001f ) {
			fMX = fN1X;
			fMY = fN1Y;
			fMLen = sqrtf((fMX * fMX) + (fMY * fMY));
		}
		if ( fMLen <= 0.00001f ) {
			pOut[i] = pPoints[i];
			continue;
		}
		fMX /= fMLen;
		fMY /= fMLen;
		fDenom = (fMX * fN1X) + (fMY * fN1Y);
		if ( fabsf(fDenom) < 0.25f ) {
			fDenom = (fDenom < 0.0f) ? -0.25f : 0.25f;
		}
		fScale = fOffset / fDenom;
		if ( fScale > (fabsf(fOffset) * 4.0f) ) {
			fScale = fabsf(fOffset) * 4.0f;
		} else if ( fScale < (-fabsf(fOffset) * 4.0f) ) {
			fScale = -fabsf(fOffset) * 4.0f;
		}
		pOut[i].fX = pPoints[i].fX + fMX * fScale;
		pOut[i].fY = pPoints[i].fY + fMY * fScale;
	}
}

static int __xgeShapeConvexFill(const xge_vec2_t* pPoints, int iCount, uint32_t iColor, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	xge_vec2_t tCenter;
	int i;
	int iRet;

	if ( (pPoints == NULL) || (iCount < 3) || (__xgeShapeColorVisible(iColor) == 0) ) {
		return XGE_OK;
	}
	pVertices = (xge_shape_vertex_t*)xrtMalloc(sizeof(*pVertices) * (size_t)(iCount + 1));
	pIndices = (uint32_t*)xrtMalloc(sizeof(*pIndices) * (size_t)iCount * 3u);
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	tCenter.fX = 0.0f;
	tCenter.fY = 0.0f;
	for ( i = 0; i < iCount; i++ ) {
		tCenter.fX += pPoints[i].fX;
		tCenter.fY += pPoints[i].fY;
	}
	tCenter.fX /= (float)iCount;
	tCenter.fY /= (float)iCount;
	pVertices[0].fX = tCenter.fX;
	pVertices[0].fY = tCenter.fY;
	pVertices[0].iColor = iColor;
	for ( i = 0; i < iCount; i++ ) {
		pVertices[i + 1].fX = pPoints[i].fX;
		pVertices[i + 1].fY = pPoints[i].fY;
		pVertices[i + 1].iColor = iColor;
		pIndices[(i * 3) + 0] = 0u;
		pIndices[(i * 3) + 1] = (uint32_t)(i + 1);
		pIndices[(i * 3) + 2] = (uint32_t)(((i + 1) % iCount) + 1);
	}
	iRet = __xgeShapeMeshFill(pVertices, iCount + 1, pIndices, iCount * 3, bScreenSpace);
	xrtFree(pVertices);
	xrtFree(pIndices);
	return iRet;
}

static int __xgeShapeConvexFillAA(const xge_vec2_t* pPoints, int iCount, uint32_t iColor, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	xge_vec2_t* pOuter;
	xge_vec2_t tCenter;
	int iVertexCount;
	int iIndexCount;
	int i;
	int iRet;

	if ( (pPoints == NULL) || (iCount < 3) || (__xgeShapeColorVisible(iColor) == 0) ) {
		return XGE_OK;
	}
	pOuter = (xge_vec2_t*)xrtMalloc(sizeof(*pOuter) * (size_t)iCount);
	if ( pOuter == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	__xgeShapeBuildOffsetPoints(pPoints, iCount, XGE_SHAPE_AA_FRINGE, pOuter);
	iVertexCount = 1 + iCount + iCount;
	iIndexCount = (iCount * 3) + (iCount * 6);
	pVertices = (xge_shape_vertex_t*)xrtMalloc(sizeof(*pVertices) * (size_t)iVertexCount);
	pIndices = (uint32_t*)xrtMalloc(sizeof(*pIndices) * (size_t)iIndexCount);
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		xrtFree(pOuter);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	tCenter.fX = 0.0f;
	tCenter.fY = 0.0f;
	for ( i = 0; i < iCount; i++ ) {
		tCenter.fX += pPoints[i].fX;
		tCenter.fY += pPoints[i].fY;
	}
	tCenter.fX /= (float)iCount;
	tCenter.fY /= (float)iCount;
	pVertices[0].fX = tCenter.fX;
	pVertices[0].fY = tCenter.fY;
	pVertices[0].iColor = iColor;
	for ( i = 0; i < iCount; i++ ) {
		pVertices[1 + i].fX = pPoints[i].fX;
		pVertices[1 + i].fY = pPoints[i].fY;
		pVertices[1 + i].iColor = iColor;
		pVertices[1 + iCount + i].fX = pOuter[i].fX;
		pVertices[1 + iCount + i].fY = pOuter[i].fY;
		pVertices[1 + iCount + i].iColor = __xgeShapeTransparentColor();
	}
	for ( i = 0; i < iCount; i++ ) {
		int iNext = (i + 1) % iCount;
		pIndices[(i * 3) + 0] = 0u;
		pIndices[(i * 3) + 1] = (uint32_t)(1 + i);
		pIndices[(i * 3) + 2] = (uint32_t)(1 + iNext);
	}
	for ( i = 0; i < iCount; i++ ) {
		int iNext = (i + 1) % iCount;
		int iBase = (iCount * 3) + (i * 6);
		pIndices[iBase + 0] = (uint32_t)(1 + i);
		pIndices[iBase + 1] = (uint32_t)(1 + iNext);
		pIndices[iBase + 2] = (uint32_t)(1 + iCount + i);
		pIndices[iBase + 3] = (uint32_t)(1 + iCount + i);
		pIndices[iBase + 4] = (uint32_t)(1 + iNext);
		pIndices[iBase + 5] = (uint32_t)(1 + iCount + iNext);
	}
	iRet = __xgeShapeMeshFill(pVertices, iVertexCount, pIndices, iIndexCount, bScreenSpace);
	xrtFree(pVertices);
	xrtFree(pIndices);
	xrtFree(pOuter);
	return iRet;
}

static int __xgeShapeEllipseFillMesh(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, int bPie, uint32_t iColor, int bScreenSpace)
{
	xge_vec2_t* pPoints;
	float fSweep;
	float fStep;
	int iSegments;
	int iCount;
	int bClosed;
	int i;
	int iRet;

	if ( (fRadiusX <= 0.0f) || (fRadiusY <= 0.0f) || (__xgeShapeColorVisible(iColor) == 0) ) {
		return XGE_OK;
	}
	fSweep = fEndRadians - fStartRadians;
	bClosed = ((bPie == 0) && (fabsf(fSweep) >= (XGE_SHAPE_TAU - 0.001f))) ? 1 : 0;
	iSegments = __xgeShapeEllipseSegmentCount(fRadiusX, fRadiusY, fSweep);
	pPoints = (xge_vec2_t*)xrtMalloc(sizeof(*pPoints) * (size_t)(iSegments + 3));
	if ( pPoints == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iCount = 0;
	if ( bPie != 0 ) {
		pPoints[iCount].fX = fX;
		pPoints[iCount].fY = fY;
		iCount++;
	}
	if ( bClosed != 0 ) {
		fStep = fSweep / (float)iSegments;
		for ( i = 0; i < iSegments; i++ ) {
			float fAngle = fStartRadians + fStep * (float)i;
			pPoints[iCount].fX = fX + cosf(fAngle) * fRadiusX;
			pPoints[iCount].fY = fY + sinf(fAngle) * fRadiusY;
			iCount++;
		}
		iRet = XGE_OK;
	} else {
		iRet = __xgeShapeBuildArcPoints(pPoints, iSegments + 3, &iCount, fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, iSegments);
	}
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeConvexFillAA(pPoints, iCount, iColor, bScreenSpace);
	}
	xrtFree(pPoints);
	return iRet;
}

static int __xgeShapeEllipseStrokeMesh(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, float fWidth, int bClosed, uint32_t iColor, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	float fOuterX;
	float fOuterY;
	float fInnerX;
	float fInnerY;
	float fOuterFadeX;
	float fOuterFadeY;
	float fInnerFadeX;
	float fInnerFadeY;
	float fStep;
	float fAngle;
	int iSegments;
	int iSampleCount;
	int iVertexCount;
	int iIndexCount;
	int iSegmentCount;
	int i;
	int iRet;

	if ( (fRadiusX <= 0.0f) || (fRadiusY <= 0.0f) || (fWidth <= 0.0f) || (__xgeShapeColorVisible(iColor) == 0) ) {
		return XGE_OK;
	}
	fOuterX = fRadiusX + fWidth * 0.5f;
	fOuterY = fRadiusY + fWidth * 0.5f;
	fInnerX = fRadiusX - fWidth * 0.5f;
	fInnerY = fRadiusY - fWidth * 0.5f;
	if ( (fInnerX <= 0.0f) || (fInnerY <= 0.0f) ) {
		return __xgeShapeEllipseFillMesh(fX, fY, fOuterX, fOuterY, fStartRadians, fEndRadians, bClosed ? 0 : 1, iColor, bScreenSpace);
	}
	fOuterFadeX = fOuterX + XGE_SHAPE_AA_FRINGE;
	fOuterFadeY = fOuterY + XGE_SHAPE_AA_FRINGE;
	fInnerFadeX = __xgeShapeMaxFloat(0.0f, fInnerX - XGE_SHAPE_AA_FRINGE);
	fInnerFadeY = __xgeShapeMaxFloat(0.0f, fInnerY - XGE_SHAPE_AA_FRINGE);
	iSegments = __xgeShapeEllipseSegmentCount(fOuterX, fOuterY, fEndRadians - fStartRadians);
	iSampleCount = (bClosed != 0) ? iSegments : (iSegments + 1);
	iVertexCount = iSampleCount * 4;
	iSegmentCount = (bClosed != 0) ? iSampleCount : (iSampleCount - 1);
	iIndexCount = iSegmentCount * 18;
	pVertices = (xge_shape_vertex_t*)xrtMalloc(sizeof(*pVertices) * (size_t)iVertexCount);
	pIndices = (uint32_t*)xrtMalloc(sizeof(*pIndices) * (size_t)iIndexCount);
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fStep = (fEndRadians - fStartRadians) / (float)iSegments;
	for ( i = 0; i < iSampleCount; i++ ) {
		fAngle = fStartRadians + fStep * (float)i;
		pVertices[(i * 4) + 0].fX = fX + cosf(fAngle) * fOuterFadeX;
		pVertices[(i * 4) + 0].fY = fY + sinf(fAngle) * fOuterFadeY;
		pVertices[(i * 4) + 0].iColor = __xgeShapeTransparentColor();
		pVertices[(i * 4) + 1].fX = fX + cosf(fAngle) * fOuterX;
		pVertices[(i * 4) + 1].fY = fY + sinf(fAngle) * fOuterY;
		pVertices[(i * 4) + 1].iColor = iColor;
		pVertices[(i * 4) + 2].fX = fX + cosf(fAngle) * fInnerX;
		pVertices[(i * 4) + 2].fY = fY + sinf(fAngle) * fInnerY;
		pVertices[(i * 4) + 2].iColor = iColor;
		pVertices[(i * 4) + 3].fX = fX + cosf(fAngle) * fInnerFadeX;
		pVertices[(i * 4) + 3].fY = fY + sinf(fAngle) * fInnerFadeY;
		pVertices[(i * 4) + 3].iColor = __xgeShapeTransparentColor();
	}
	for ( i = 0; i < iSegmentCount; i++ ) {
		int iNext = (i + 1) % iSampleCount;
		int iBase = i * 18;
		pIndices[iBase + 0] = (uint32_t)((i * 4) + 0);
		pIndices[iBase + 1] = (uint32_t)((iNext * 4) + 0);
		pIndices[iBase + 2] = (uint32_t)((i * 4) + 1);
		pIndices[iBase + 3] = (uint32_t)((i * 4) + 1);
		pIndices[iBase + 4] = (uint32_t)((iNext * 4) + 0);
		pIndices[iBase + 5] = (uint32_t)((iNext * 4) + 1);
		pIndices[iBase + 6] = (uint32_t)((i * 4) + 1);
		pIndices[iBase + 7] = (uint32_t)((iNext * 4) + 1);
		pIndices[iBase + 8] = (uint32_t)((i * 4) + 2);
		pIndices[iBase + 9] = (uint32_t)((i * 4) + 2);
		pIndices[iBase + 10] = (uint32_t)((iNext * 4) + 1);
		pIndices[iBase + 11] = (uint32_t)((iNext * 4) + 2);
		pIndices[iBase + 12] = (uint32_t)((i * 4) + 2);
		pIndices[iBase + 13] = (uint32_t)((iNext * 4) + 2);
		pIndices[iBase + 14] = (uint32_t)((i * 4) + 3);
		pIndices[iBase + 15] = (uint32_t)((i * 4) + 3);
		pIndices[iBase + 16] = (uint32_t)((iNext * 4) + 2);
		pIndices[iBase + 17] = (uint32_t)((iNext * 4) + 3);
	}
	iRet = __xgeShapeMeshFill(pVertices, iVertexCount, pIndices, iIndexCount, bScreenSpace);
	xrtFree(pVertices);
	xrtFree(pIndices);
	return iRet;
}

static xge_rect_t __xgeShapeProjectRect(xge_rect_t tRect, int bScreenSpace, float* pScaleX, float* pScaleY)
{
	xge_vec2_t tA;
	xge_vec2_t tB;
	xge_rect_t tOut;
	float fX0;
	float fY0;
	float fX1;
	float fY1;

	if ( bScreenSpace != 0 ) {
		if ( pScaleX != NULL ) {
			*pScaleX = 1.0f;
		}
		if ( pScaleY != NULL ) {
			*pScaleY = 1.0f;
		}
		return tRect;
	}
	tA.fX = tRect.fX;
	tA.fY = tRect.fY;
	tB.fX = tRect.fX + tRect.fW;
	tB.fY = tRect.fY + tRect.fH;
	tA = xgeWorldToScreen(tA);
	tB = xgeWorldToScreen(tB);
	fX0 = __xgeShapeMinFloat(tA.fX, tB.fX);
	fY0 = __xgeShapeMinFloat(tA.fY, tB.fY);
	fX1 = __xgeShapeMaxFloat(tA.fX, tB.fX);
	fY1 = __xgeShapeMaxFloat(tA.fY, tB.fY);
	tOut.fX = fX0;
	tOut.fY = fY0;
	tOut.fW = fX1 - fX0;
	tOut.fH = fY1 - fY0;
	if ( pScaleX != NULL ) {
		*pScaleX = (tRect.fW != 0.0f) ? (tOut.fW / fabsf(tRect.fW)) : 1.0f;
	}
	if ( pScaleY != NULL ) {
		*pScaleY = (tRect.fH != 0.0f) ? (tOut.fH / fabsf(tRect.fH)) : 1.0f;
	}
	return tOut;
}

static int __xgeShapeRoundRectSdfDraw(xge_rect_t tRect, xge_shape_round_rect_t tRadii, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor, int bScreenSpace)
{
	xge_rect_t tScreenRect;
	xge_rect_t tDrawRect;
	float fScaleX;
	float fScaleY;
	float fRadiusScale;
	float fFillR;
	float fFillG;
	float fFillB;
	float fFillA;
	float fStrokeR;
	float fStrokeG;
	float fStrokeB;
	float fStrokeA;
	float arrVertices[8];

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (__xgeShapeColorVisible(iFillColor) == 0) && ((fStrokeWidth <= 0.0f) || (__xgeShapeColorVisible(iStrokeColor) == 0)) ) {
		return XGE_OK;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	__xgeShapeNormalizeRoundRectRadii(tRect, &tRadii);
	tScreenRect = __xgeShapeProjectRect(tRect, bScreenSpace, &fScaleX, &fScaleY);
	if ( (tScreenRect.fW <= 0.0f) || (tScreenRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	fRadiusScale = __xgeShapeMinFloat(fScaleX, fScaleY);
	tRadii.fTopLeft *= fRadiusScale;
	tRadii.fTopRight *= fRadiusScale;
	tRadii.fBottomRight *= fRadiusScale;
	tRadii.fBottomLeft *= fRadiusScale;
	__xgeShapeNormalizeRoundRectRadii(tScreenRect, &tRadii);
	if ( (fStrokeWidth <= 0.0f) || (__xgeShapeColorVisible(iStrokeColor) == 0) ) {
		fStrokeWidth = 0.0f;
		iStrokeColor = 0;
	}
	fStrokeWidth *= fRadiusScale;
	if ( fStrokeWidth > __xgeShapeMinFloat(tScreenRect.fW, tScreenRect.fH) * 0.5f ) {
		fStrokeWidth = __xgeShapeMinFloat(tScreenRect.fW, tScreenRect.fH) * 0.5f;
	}
	tDrawRect = tScreenRect;
	tDrawRect.fX -= XGE_SHAPE_SDF_FRINGE;
	tDrawRect.fY -= XGE_SHAPE_SDF_FRINGE;
	tDrawRect.fW += XGE_SHAPE_SDF_FRINGE * 2.0f;
	tDrawRect.fH += XGE_SHAPE_SDF_FRINGE * 2.0f;
	arrVertices[0] = tDrawRect.fX;
	arrVertices[1] = tDrawRect.fY;
	arrVertices[2] = tDrawRect.fX + tDrawRect.fW;
	arrVertices[3] = tDrawRect.fY;
	arrVertices[4] = tDrawRect.fX;
	arrVertices[5] = tDrawRect.fY + tDrawRect.fH;
	arrVertices[6] = tDrawRect.fX + tDrawRect.fW;
	arrVertices[7] = tDrawRect.fY + tDrawRect.fH;
	__xgeColorToFloat(iFillColor, &fFillR, &fFillG, &fFillB, &fFillA);
	__xgeColorToFloat(iStrokeColor, &fStrokeR, &fStrokeG, &fStrokeB, &fStrokeA);
	(void)__xgeShapeAutoBatchFlush();
	glUseProgram(g_xgeShapeRenderer.iSdfRoundRectProgram);
	glUniform2f(g_xgeShapeRenderer.iSdfRoundRectLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iSdfRoundRectLocRect, tScreenRect.fX, tScreenRect.fY, tScreenRect.fW, tScreenRect.fH);
	glUniform4f(g_xgeShapeRenderer.iSdfRoundRectLocRadii, tRadii.fTopLeft, tRadii.fTopRight, tRadii.fBottomRight, tRadii.fBottomLeft);
	glUniform4f(g_xgeShapeRenderer.iSdfRoundRectLocFillColor, fFillR, fFillG, fFillB, fFillA);
	glUniform4f(g_xgeShapeRenderer.iSdfRoundRectLocStrokeColor, fStrokeR, fStrokeG, fStrokeB, fStrokeA);
	glUniform1f(g_xgeShapeRenderer.iSdfRoundRectLocStrokeWidth, fStrokeWidth);
	glBindVertexArray(g_xgeShapeRenderer.iSdfRoundRectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iSdfRoundRectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arrVertices), arrVertices, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
	return XGE_OK;
}

static int __xgeShapeRoundRectMeshDraw(xge_rect_t tRect, xge_shape_round_rect_t tRadii, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	xge_vec2_t* pOuter;
	xge_vec2_t* pInner;
	xge_vec2_t* pOuterAA;
	xge_vec2_t* pInnerAA;
	xge_rect_t tInnerRect;
	xge_shape_round_rect_t tInnerRadii;
	int arrSegments[4];
	int iOutlineCapacity;
	int iOuterCount;
	int iInnerCount;
	int iFillVertexBase;
	int iStrokeVertexBase;
	int iOuterFringeBase;
	int iInnerFringeBase;
	int iVertexCount;
	int iIndexCount;
	int iVertexWrite;
	int iIndexWrite;
	int bFillVisible;
	int i;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (__xgeShapeColorVisible(iFillColor) == 0) && ((fStrokeWidth <= 0.0f) || (__xgeShapeColorVisible(iStrokeColor) == 0)) ) {
		return XGE_OK;
	}
	__xgeShapeNormalizeRoundRectRadii(tRect, &tRadii);
	iRet = __xgeShapeRoundRectCornerSegments(tRadii, arrSegments);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iOutlineCapacity = __xgeShapeRoundRectOutlineCount(arrSegments);
	iOuterCount = iOutlineCapacity;
	pOuter = (xge_vec2_t*)xrtMalloc(sizeof(*pOuter) * (size_t)iOutlineCapacity);
	if ( pOuter == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xgeShapeBuildRoundRectOutline(tRect, tRadii, arrSegments, pOuter, iOuterCount, &iOuterCount);
	if ( iRet != XGE_OK ) {
		xrtFree(pOuter);
		return iRet;
	}
	if ( (fStrokeWidth <= 0.0f) || (__xgeShapeColorVisible(iStrokeColor) == 0) ) {
		iRet = __xgeShapeConvexFillAA(pOuter, iOuterCount, iFillColor, bScreenSpace);
		xrtFree(pOuter);
		return iRet;
	}
	if ( fStrokeWidth > __xgeShapeMinFloat(tRect.fW, tRect.fH) * 0.5f ) {
		fStrokeWidth = __xgeShapeMinFloat(tRect.fW, tRect.fH) * 0.5f;
	}
	tInnerRect.fX = tRect.fX + fStrokeWidth;
	tInnerRect.fY = tRect.fY + fStrokeWidth;
	tInnerRect.fW = tRect.fW - fStrokeWidth * 2.0f;
	tInnerRect.fH = tRect.fH - fStrokeWidth * 2.0f;
	if ( (tInnerRect.fW <= 0.0f) || (tInnerRect.fH <= 0.0f) ) {
		iRet = __xgeShapeConvexFillAA(pOuter, iOuterCount, iStrokeColor, bScreenSpace);
		xrtFree(pOuter);
		return iRet;
	}
	tInnerRadii.fTopLeft = __xgeShapeMaxFloat(0.0f, tRadii.fTopLeft - fStrokeWidth);
	tInnerRadii.fTopRight = __xgeShapeMaxFloat(0.0f, tRadii.fTopRight - fStrokeWidth);
	tInnerRadii.fBottomRight = __xgeShapeMaxFloat(0.0f, tRadii.fBottomRight - fStrokeWidth);
	tInnerRadii.fBottomLeft = __xgeShapeMaxFloat(0.0f, tRadii.fBottomLeft - fStrokeWidth);
	pInner = (xge_vec2_t*)xrtMalloc(sizeof(*pInner) * (size_t)iOutlineCapacity);
	if ( pInner == NULL ) {
		xrtFree(pOuter);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xgeShapeBuildRoundRectOutline(tInnerRect, tInnerRadii, arrSegments, pInner, iOutlineCapacity, &iInnerCount);
	if ( (iRet != XGE_OK) || (iInnerCount != iOuterCount) ) {
		xrtFree(pOuter);
		xrtFree(pInner);
		return (iRet != XGE_OK) ? iRet : XGE_ERROR_RESOURCE_FAILED;
	}
	pOuterAA = (xge_vec2_t*)xrtMalloc(sizeof(*pOuterAA) * (size_t)iOuterCount);
	pInnerAA = NULL;
	if ( pOuterAA == NULL ) {
		xrtFree(pOuter);
		xrtFree(pInner);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	__xgeShapeBuildOffsetPoints(pOuter, iOuterCount, XGE_SHAPE_AA_FRINGE, pOuterAA);
	bFillVisible = __xgeShapeColorVisible(iFillColor);
	if ( bFillVisible == 0 ) {
		pInnerAA = (xge_vec2_t*)xrtMalloc(sizeof(*pInnerAA) * (size_t)iInnerCount);
		if ( pInnerAA == NULL ) {
			xrtFree(pOuterAA);
			xrtFree(pOuter);
			xrtFree(pInner);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		__xgeShapeBuildOffsetPoints(pInner, iInnerCount, -XGE_SHAPE_AA_FRINGE, pInnerAA);
	}
	iVertexCount = (iOuterCount * 2) + (iOuterCount * 2);
	iIndexCount = (iOuterCount * 6) + (iOuterCount * 6);
	if ( bFillVisible != 0 ) {
		iVertexCount += iInnerCount + 1;
		iIndexCount += iInnerCount * 3;
	} else {
		iVertexCount += iInnerCount * 2;
		iIndexCount += iInnerCount * 6;
	}
	pVertices = (xge_shape_vertex_t*)xrtMalloc(sizeof(*pVertices) * (size_t)iVertexCount);
	pIndices = (uint32_t*)xrtMalloc(sizeof(*pIndices) * (size_t)iIndexCount);
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		xrtFree(pOuterAA);
		xrtFree(pInnerAA);
		xrtFree(pOuter);
		xrtFree(pInner);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iVertexWrite = 0;
	iIndexWrite = 0;
	if ( bFillVisible != 0 ) {
		xge_vec2_t tCenter;
		tCenter.fX = 0.0f;
		tCenter.fY = 0.0f;
		for ( i = 0; i < iInnerCount; i++ ) {
			tCenter.fX += pInner[i].fX;
			tCenter.fY += pInner[i].fY;
		}
		tCenter.fX /= (float)iInnerCount;
		tCenter.fY /= (float)iInnerCount;
		iFillVertexBase = iVertexWrite;
		pVertices[iVertexWrite].fX = tCenter.fX;
		pVertices[iVertexWrite].fY = tCenter.fY;
		pVertices[iVertexWrite].iColor = iFillColor;
		iVertexWrite++;
		for ( i = 0; i < iInnerCount; i++ ) {
			pVertices[iVertexWrite + i].fX = pInner[i].fX;
			pVertices[iVertexWrite + i].fY = pInner[i].fY;
			pVertices[iVertexWrite + i].iColor = iFillColor;
			pIndices[iIndexWrite++] = (uint32_t)iFillVertexBase;
			pIndices[iIndexWrite++] = (uint32_t)(iVertexWrite + i);
			pIndices[iIndexWrite++] = (uint32_t)(iVertexWrite + ((i + 1) % iInnerCount));
		}
		iVertexWrite += iInnerCount;
	}
	iStrokeVertexBase = iVertexWrite;
	for ( i = 0; i < iOuterCount; i++ ) {
		pVertices[iStrokeVertexBase + (i * 2) + 0].fX = pOuter[i].fX;
		pVertices[iStrokeVertexBase + (i * 2) + 0].fY = pOuter[i].fY;
		pVertices[iStrokeVertexBase + (i * 2) + 0].iColor = iStrokeColor;
		pVertices[iStrokeVertexBase + (i * 2) + 1].fX = pInner[i].fX;
		pVertices[iStrokeVertexBase + (i * 2) + 1].fY = pInner[i].fY;
		pVertices[iStrokeVertexBase + (i * 2) + 1].iColor = iStrokeColor;
	}
	iVertexWrite += iOuterCount * 2;
	for ( i = 0; i < iOuterCount; i++ ) {
		int iNext = (i + 1) % iOuterCount;
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (i * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (iNext * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (i * 2) + 1);
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (i * 2) + 1);
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (iNext * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iStrokeVertexBase + (iNext * 2) + 1);
	}
	iOuterFringeBase = iVertexWrite;
	for ( i = 0; i < iOuterCount; i++ ) {
		pVertices[iOuterFringeBase + (i * 2) + 0].fX = pOuterAA[i].fX;
		pVertices[iOuterFringeBase + (i * 2) + 0].fY = pOuterAA[i].fY;
		pVertices[iOuterFringeBase + (i * 2) + 0].iColor = __xgeShapeTransparentColor();
		pVertices[iOuterFringeBase + (i * 2) + 1].fX = pOuter[i].fX;
		pVertices[iOuterFringeBase + (i * 2) + 1].fY = pOuter[i].fY;
		pVertices[iOuterFringeBase + (i * 2) + 1].iColor = iStrokeColor;
	}
	iVertexWrite += iOuterCount * 2;
	for ( i = 0; i < iOuterCount; i++ ) {
		int iNext = (i + 1) % iOuterCount;
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (i * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (iNext * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (i * 2) + 1);
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (i * 2) + 1);
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (iNext * 2));
		pIndices[iIndexWrite++] = (uint32_t)(iOuterFringeBase + (iNext * 2) + 1);
	}
	if ( bFillVisible == 0 ) {
		iInnerFringeBase = iVertexWrite;
		for ( i = 0; i < iInnerCount; i++ ) {
			pVertices[iInnerFringeBase + (i * 2) + 0].fX = pInner[i].fX;
			pVertices[iInnerFringeBase + (i * 2) + 0].fY = pInner[i].fY;
			pVertices[iInnerFringeBase + (i * 2) + 0].iColor = iStrokeColor;
			pVertices[iInnerFringeBase + (i * 2) + 1].fX = pInnerAA[i].fX;
			pVertices[iInnerFringeBase + (i * 2) + 1].fY = pInnerAA[i].fY;
			pVertices[iInnerFringeBase + (i * 2) + 1].iColor = __xgeShapeTransparentColor();
		}
		iVertexWrite += iInnerCount * 2;
		for ( i = 0; i < iInnerCount; i++ ) {
			int iNext = (i + 1) % iInnerCount;
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (i * 2));
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (iNext * 2));
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (i * 2) + 1);
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (i * 2) + 1);
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (iNext * 2));
			pIndices[iIndexWrite++] = (uint32_t)(iInnerFringeBase + (iNext * 2) + 1);
		}
	}
	iRet = __xgeShapeMeshFill(pVertices, iVertexWrite, pIndices, iIndexWrite, bScreenSpace);
	xrtFree(pVertices);
	xrtFree(pIndices);
	xrtFree(pOuterAA);
	xrtFree(pInnerAA);
	xrtFree(pOuter);
	xrtFree(pInner);
	return iRet;
}

static void __xgeShapeRectFill(xge_rect_t tRect, uint32_t iColor, int bScreenSpace)
{
	float fR;
	float fG;
	float fB;
	float fA;
	float arrVertices[8];
	xge_vec2_t tPoint;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		return;
	}
	(void)__xgeShapeAutoBatchFlush();

	if ( bScreenSpace ) {
		arrVertices[0] = tRect.fX;
		arrVertices[1] = tRect.fY;
		arrVertices[2] = tRect.fX + tRect.fW;
		arrVertices[3] = tRect.fY;
		arrVertices[4] = tRect.fX;
		arrVertices[5] = tRect.fY + tRect.fH;
		arrVertices[6] = tRect.fX + tRect.fW;
		arrVertices[7] = tRect.fY + tRect.fH;
	} else {
		tPoint.fX = tRect.fX;
		tPoint.fY = tRect.fY;
		tPoint = xgeWorldToScreen(tPoint);
		arrVertices[0] = tPoint.fX;
		arrVertices[1] = tPoint.fY;
		tPoint.fX = tRect.fX + tRect.fW;
		tPoint.fY = tRect.fY;
		tPoint = xgeWorldToScreen(tPoint);
		arrVertices[2] = tPoint.fX;
		arrVertices[3] = tPoint.fY;
		tPoint.fX = tRect.fX;
		tPoint.fY = tRect.fY + tRect.fH;
		tPoint = xgeWorldToScreen(tPoint);
		arrVertices[4] = tPoint.fX;
		arrVertices[5] = tPoint.fY;
		tPoint.fX = tRect.fX + tRect.fW;
		tPoint.fY = tRect.fY + tRect.fH;
		tPoint = xgeWorldToScreen(tPoint);
		arrVertices[6] = tPoint.fX;
		arrVertices[7] = tPoint.fY;
	}

	__xgeColorToFloat(iColor, &fR, &fG, &fB, &fA);
	glUseProgram(g_xgeShapeRenderer.iProgram);
	glUniform2f(g_xgeShapeRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iLocColor, fR, fG, fB, fA);
	glUniform1i(g_xgeShapeRenderer.iLocUseVertexColor, 0);
	glBindVertexArray(g_xgeShapeRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arrVertices), arrVertices, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
}

static void __xgeShapeQuadFill(const xge_vec2_t* pPoints, uint32_t iColor, int bScreenSpace)
{
	float fR;
	float fG;
	float fB;
	float fA;
	float arrVertices[8];
	xge_vec2_t tPoint;
	int i;

	if ( (pPoints == NULL) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		return;
	}
	(void)__xgeShapeAutoBatchFlush();
	for ( i = 0; i < 4; i++ ) {
		tPoint = pPoints[i];
		if ( bScreenSpace == 0 ) {
			tPoint = xgeWorldToScreen(tPoint);
		}
		arrVertices[(i * 2) + 0] = tPoint.fX;
		arrVertices[(i * 2) + 1] = tPoint.fY;
	}
	__xgeColorToFloat(iColor, &fR, &fG, &fB, &fA);
	glUseProgram(g_xgeShapeRenderer.iProgram);
	glUniform2f(g_xgeShapeRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iLocColor, fR, fG, fB, fA);
	glUniform1i(g_xgeShapeRenderer.iLocUseVertexColor, 0);
	glBindVertexArray(g_xgeShapeRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arrVertices), arrVertices, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
}

static void __xgeShapePointsFill(const xge_vec2_t* pPoints, int iCount, uint32_t iColor, int iMode, int bScreenSpace)
{
	float fR;
	float fG;
	float fB;
	float fA;
	float* pVertices;
	xge_vec2_t tPoint;
	int i;

	if ( (pPoints == NULL) || (iCount < 3) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		return;
	}
	(void)__xgeShapeAutoBatchFlush();
	pVertices = (float*)xrtMalloc(sizeof(float) * 2 * (size_t)iCount);
	if ( pVertices == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		tPoint = pPoints[i];
		if ( bScreenSpace == 0 ) {
			tPoint = xgeWorldToScreen(tPoint);
		}
		pVertices[(i * 2) + 0] = tPoint.fX;
		pVertices[(i * 2) + 1] = tPoint.fY;
	}
	__xgeColorToFloat(iColor, &fR, &fG, &fB, &fA);
	glUseProgram(g_xgeShapeRenderer.iProgram);
	glUniform2f(g_xgeShapeRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iLocColor, fR, fG, fB, fA);
	glUniform1i(g_xgeShapeRenderer.iLocUseVertexColor, 0);
	glBindVertexArray(g_xgeShapeRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * (size_t)iCount, pVertices, GL_DYNAMIC_DRAW);
	glDrawArrays((GLenum)iMode, 0, iCount);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	xrtFree(pVertices);
	__xgeFrameStatsAddDrawCall();
}

static void __xgeShapePoint(float fX, float fY, float fSize, uint32_t iColor, int bScreenSpace)
{
	xge_rect_t tRect;
	float fHalf;

	if ( fSize <= 0.0f ) {
		return;
	}
	fHalf = fSize * 0.5f;
	tRect.fX = fX - fHalf;
	tRect.fY = fY - fHalf;
	tRect.fW = fSize;
	tRect.fH = fSize;
	__xgeShapeRectFill(tRect, iColor, bScreenSpace);
}

void xgeShapePoint(float fX, float fY, float fSize, uint32_t iColor)
{
	__xgeShapePoint(fX, fY, fSize, iColor, 0);
}

void xgeShapePointPx(float fX, float fY, float fSize, uint32_t iColor)
{
	__xgeShapePoint(fX, fY, fSize, iColor, 1);
}

static void __xgeShapeLine(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor, int bScreenSpace)
{
	xge_shape_vertex_t arrVertices[8];
	uint32_t arrIndices[30];
	float fDX;
	float fDY;
	float fLen;
	float fNX;
	float fNY;
	float fUX;
	float fUY;
	float fHalf;
	float fOuter;
	uint32_t iTransparent;

	if ( (fWidth <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	fDX = fX1 - fX0;
	fDY = fY1 - fY0;
	fLen = sqrtf((fDX * fDX) + (fDY * fDY));
	if ( fLen <= 0.0f ) {
		return;
	}
	fUX = fDX / fLen;
	fUY = fDY / fLen;
	fHalf = fWidth * 0.5f;
	fNX = -fUY;
	fNY = fUX;
	fOuter = fHalf + XGE_SHAPE_AA_FRINGE;
	iTransparent = __xgeShapeTransparentColor();
	arrVertices[0] = (xge_shape_vertex_t){fX0 + fNX * fHalf, fY0 + fNY * fHalf, iColor};
	arrVertices[1] = (xge_shape_vertex_t){fX1 + fNX * fHalf, fY1 + fNY * fHalf, iColor};
	arrVertices[2] = (xge_shape_vertex_t){fX0 - fNX * fHalf, fY0 - fNY * fHalf, iColor};
	arrVertices[3] = (xge_shape_vertex_t){fX1 - fNX * fHalf, fY1 - fNY * fHalf, iColor};
	arrVertices[4] = (xge_shape_vertex_t){fX0 - fUX * XGE_SHAPE_AA_FRINGE + fNX * fOuter, fY0 - fUY * XGE_SHAPE_AA_FRINGE + fNY * fOuter, iTransparent};
	arrVertices[5] = (xge_shape_vertex_t){fX1 + fUX * XGE_SHAPE_AA_FRINGE + fNX * fOuter, fY1 + fUY * XGE_SHAPE_AA_FRINGE + fNY * fOuter, iTransparent};
	arrVertices[6] = (xge_shape_vertex_t){fX1 + fUX * XGE_SHAPE_AA_FRINGE - fNX * fOuter, fY1 + fUY * XGE_SHAPE_AA_FRINGE - fNY * fOuter, iTransparent};
	arrVertices[7] = (xge_shape_vertex_t){fX0 - fUX * XGE_SHAPE_AA_FRINGE - fNX * fOuter, fY0 - fUY * XGE_SHAPE_AA_FRINGE - fNY * fOuter, iTransparent};
	arrIndices[0] = 0; arrIndices[1] = 1; arrIndices[2] = 2;
	arrIndices[3] = 2; arrIndices[4] = 1; arrIndices[5] = 3;
	arrIndices[6] = 4; arrIndices[7] = 5; arrIndices[8] = 0;
	arrIndices[9] = 0; arrIndices[10] = 5; arrIndices[11] = 1;
	arrIndices[12] = 1; arrIndices[13] = 5; arrIndices[14] = 3;
	arrIndices[15] = 3; arrIndices[16] = 5; arrIndices[17] = 6;
	arrIndices[18] = 2; arrIndices[19] = 3; arrIndices[20] = 7;
	arrIndices[21] = 7; arrIndices[22] = 3; arrIndices[23] = 6;
	arrIndices[24] = 4; arrIndices[25] = 0; arrIndices[26] = 7;
	arrIndices[27] = 7; arrIndices[28] = 0; arrIndices[29] = 2;
	(void)__xgeShapeMeshFill(arrVertices, 8, arrIndices, 30, bScreenSpace);
}

void xgeShapeLine(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	__xgeShapeLine(fX0, fY0, fX1, fY1, fWidth, iColor, 0);
}

void xgeShapeLinePx(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	__xgeShapeLine(fX0, fY0, fX1, fY1, fWidth, iColor, 1);
}

void xgeShapeRectFill(xge_rect_t tRect, uint32_t iColor)
{
	__xgeShapeRectFill(tRect, iColor, 0);
}

void xgeShapeRectFillPx(xge_rect_t tRect, uint32_t iColor)
{
	__xgeShapeRectFill(tRect, iColor, 1);
}

static void __xgeShapeRectStroke(xge_rect_t tRect, float fWidth, uint32_t iColor, int bScreenSpace)
{
	xge_rect_t tEdge;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	if ( fWidth > tRect.fW ) {
		fWidth = tRect.fW;
	}
	if ( fWidth > tRect.fH ) {
		fWidth = tRect.fH;
	}
	tEdge = tRect;
	tEdge.fH = fWidth;
	__xgeShapeRectFill(tEdge, iColor, bScreenSpace);
	tEdge.fY = tRect.fY + tRect.fH - fWidth;
	__xgeShapeRectFill(tEdge, iColor, bScreenSpace);
	tEdge.fX = tRect.fX;
	tEdge.fY = tRect.fY + fWidth;
	tEdge.fW = fWidth;
	tEdge.fH = tRect.fH - (fWidth * 2.0f);
	__xgeShapeRectFill(tEdge, iColor, bScreenSpace);
	tEdge.fX = tRect.fX + tRect.fW - fWidth;
	__xgeShapeRectFill(tEdge, iColor, bScreenSpace);
}

void xgeShapeRectStroke(xge_rect_t tRect, float fWidth, uint32_t iColor)
{
	__xgeShapeRectStroke(tRect, fWidth, iColor, 0);
}

void xgeShapeRectStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor)
{
	__xgeShapeRectStroke(tRect, fWidth, iColor, 1);
}

static void __xgeShapeCircleFill(float fX, float fY, float fRadius, uint32_t iColor, int bScreenSpace)
{
	(void)__xgeShapeEllipseFillMesh(fX, fY, fRadius, fRadius, 0.0f, XGE_SHAPE_TAU, 0, iColor, bScreenSpace);
}

void xgeShapeCircleFill(float fX, float fY, float fRadius, uint32_t iColor)
{
	__xgeShapeCircleFill(fX, fY, fRadius, iColor, 0);
}

void xgeShapeCircleFillPx(float fX, float fY, float fRadius, uint32_t iColor)
{
	__xgeShapeCircleFill(fX, fY, fRadius, iColor, 1);
}

static void __xgeShapeArc(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor, int bScreenSpace)
{
	float fSweep;

	if ( (fRadius <= 0.0f) || (fWidth <= 0.0f) || (XGE_COLOR_GET_A(iColor) == 0) ) {
		return;
	}
	fSweep = fEndRadians - fStartRadians;
	if ( fSweep == 0.0f ) {
		return;
	}
	(void)__xgeShapeEllipseStrokeMesh(fX, fY, fRadius, fRadius, fStartRadians, fEndRadians, fWidth, fabsf(fSweep) >= (XGE_SHAPE_TAU - 0.001f), iColor, bScreenSpace);
}

void xgeShapeArc(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor)
{
	__xgeShapeArc(fX, fY, fRadius, fStartRadians, fEndRadians, fWidth, iColor, 0);
}

void xgeShapeArcPx(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor)
{
	__xgeShapeArc(fX, fY, fRadius, fStartRadians, fEndRadians, fWidth, iColor, 1);
}

void xgeShapeCircleStroke(float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	__xgeShapeArc(fX, fY, fRadius, 0.0f, XGE_SHAPE_TAU, fWidth, iColor, 0);
}

void xgeShapeCircleStrokePx(float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	__xgeShapeArc(fX, fY, fRadius, 0.0f, XGE_SHAPE_TAU, fWidth, iColor, 1);
}

static void __xgeShapeTriangleFill(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor, int bScreenSpace)
{
	xge_vec2_t arrPoints[3];

	arrPoints[0] = tA;
	arrPoints[1] = tB;
	arrPoints[2] = tC;
	(void)__xgeShapeConvexFillAA(arrPoints, 3, iColor, bScreenSpace);
}

void xgeShapeTriangleFill(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor)
{
	__xgeShapeTriangleFill(tA, tB, tC, iColor, 0);
}

void xgeShapeTriangleFillPx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor)
{
	__xgeShapeTriangleFill(tA, tB, tC, iColor, 1);
}

static void __xgeShapeTriangleStroke(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor, int bScreenSpace)
{
	__xgeShapeLine(tA.fX, tA.fY, tB.fX, tB.fY, fWidth, iColor, bScreenSpace);
	__xgeShapeLine(tB.fX, tB.fY, tC.fX, tC.fY, fWidth, iColor, bScreenSpace);
	__xgeShapeLine(tC.fX, tC.fY, tA.fX, tA.fY, fWidth, iColor, bScreenSpace);
}

void xgeShapeTriangleStroke(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor)
{
	__xgeShapeTriangleStroke(tA, tB, tC, fWidth, iColor, 0);
}

void xgeShapeTriangleStrokePx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor)
{
	__xgeShapeTriangleStroke(tA, tB, tC, fWidth, iColor, 1);
}

static void __xgeShapeEllipseFill(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor, int bScreenSpace)
{
	(void)__xgeShapeEllipseFillMesh(fX, fY, fRadiusX, fRadiusY, 0.0f, XGE_SHAPE_TAU, 0, iColor, bScreenSpace);
}

void xgeShapeEllipseFill(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor)
{
	__xgeShapeEllipseFill(fX, fY, fRadiusX, fRadiusY, iColor, 0);
}

void xgeShapeEllipseFillPx(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor)
{
	__xgeShapeEllipseFill(fX, fY, fRadiusX, fRadiusY, iColor, 1);
}

static void __xgeShapeEllipseStroke(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor, int bScreenSpace)
{
	(void)__xgeShapeEllipseStrokeMesh(fX, fY, fRadiusX, fRadiusY, 0.0f, XGE_SHAPE_TAU, fWidth, 1, iColor, bScreenSpace);
}

void xgeShapeEllipseStroke(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor)
{
	__xgeShapeEllipseStroke(fX, fY, fRadiusX, fRadiusY, fWidth, iColor, 0);
}

void xgeShapeEllipseStrokePx(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor)
{
	__xgeShapeEllipseStroke(fX, fY, fRadiusX, fRadiusY, fWidth, iColor, 1);
}

static void __xgeShapePieFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor, int bScreenSpace)
{
	(void)__xgeShapeEllipseFillMesh(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, 1, iColor, bScreenSpace);
}

void xgeShapePieFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor)
{
	__xgeShapePieFill(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, iColor, 0);
}

void xgeShapePieFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor)
{
	__xgeShapePieFill(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, iColor, 1);
}

static void __xgeShapeChordFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor, int bScreenSpace)
{
	(void)__xgeShapeEllipseFillMesh(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, 0, iColor, bScreenSpace);
}

void xgeShapeChordFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor)
{
	__xgeShapeChordFill(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, iColor, 0);
}

void xgeShapeChordFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor)
{
	__xgeShapeChordFill(fX, fY, fRadiusX, fRadiusY, fStartRadians, fEndRadians, iColor, 1);
}

static void __xgeShapeRoundRectDrawInternal(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor, int bScreenSpace)
{
	int iRet;

	if ( iMode == XGE_SHAPE_ROUND_RECT_AUTO ) {
		iMode = g_iXgeShapeRoundRectMode;
	}
	if ( iMode == XGE_SHAPE_ROUND_RECT_AUTO ) {
		iMode = (g_iXgeShapeRenderMode == XGE_SHAPE_RENDER_SDF) ? XGE_SHAPE_ROUND_RECT_SDF : XGE_SHAPE_ROUND_RECT_MESH;
	}
	if ( iMode == XGE_SHAPE_ROUND_RECT_SDF ) {
		iRet = __xgeShapeRoundRectSdfDraw(tRect, tRadii, iFillColor, fStrokeWidth, iStrokeColor, bScreenSpace);
		if ( iRet == XGE_OK ) {
			return;
		}
	}
	(void)__xgeShapeRoundRectMeshDraw(tRect, tRadii, iFillColor, fStrokeWidth, iStrokeColor, bScreenSpace);
}

void xgeShapeRenderModeSet(int iMode)
{
	if ( (iMode == XGE_SHAPE_RENDER_AA_MESH) || (iMode == XGE_SHAPE_RENDER_SDF) ) {
		g_iXgeShapeRenderMode = iMode;
	}
}

int xgeShapeRenderModeGet(void)
{
	return g_iXgeShapeRenderMode;
}

void xgeShapeRoundRectModeSet(int iMode)
{
	if ( (iMode == XGE_SHAPE_ROUND_RECT_AUTO) || (iMode == XGE_SHAPE_ROUND_RECT_SDF) || (iMode == XGE_SHAPE_ROUND_RECT_MESH) ) {
		g_iXgeShapeRoundRectMode = iMode;
	}
}

int xgeShapeRoundRectModeGet(void)
{
	return g_iXgeShapeRoundRectMode;
}

void xgeShapeRoundRectFill(xge_rect_t tRect, float fRadius, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, iColor, 0.0f, 0, 0);
}

void xgeShapeRoundRectFillPx(xge_rect_t tRect, float fRadius, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, iColor, 0.0f, 0, 1);
}

void xgeShapeRoundRectFillEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, iColor, 0.0f, 0, 0);
}

void xgeShapeRoundRectFillExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, iColor, 0.0f, 0, 1);
}

void xgeShapeRoundRectStroke(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, 0, fWidth, iColor, 0);
}

void xgeShapeRoundRectStrokePx(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, 0, fWidth, iColor, 1);
}

void xgeShapeRoundRectStrokeEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, 0, fWidth, iColor, 0);
}

void xgeShapeRoundRectStrokeExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, 0, fWidth, iColor, 1);
}

void xgeShapeRoundRectDraw(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, iFillColor, fStrokeWidth, iStrokeColor, 0);
}

void xgeShapeRoundRectDrawPx(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, iFillColor, fStrokeWidth, iStrokeColor, 1);
}

void xgeShapeRoundRectDrawEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, iFillColor, fStrokeWidth, iStrokeColor, 0);
}

void xgeShapeRoundRectDrawExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor)
{
	__xgeShapeRoundRectDrawInternal(tRect, tRadii, iMode, iFillColor, fStrokeWidth, iStrokeColor, 1);
}

static void __xgeShapeCapsuleFill(xge_rect_t tRect, uint32_t iColor, int bScreenSpace)
{
	float fRadius;

	fRadius = __xgeShapeMinFloat(tRect.fW, tRect.fH) * 0.5f;
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, iColor, 0.0f, 0, bScreenSpace);
}

void xgeShapeCapsuleFill(xge_rect_t tRect, uint32_t iColor)
{
	__xgeShapeCapsuleFill(tRect, iColor, 0);
}

void xgeShapeCapsuleFillPx(xge_rect_t tRect, uint32_t iColor)
{
	__xgeShapeCapsuleFill(tRect, iColor, 1);
}

static void __xgeShapeCapsuleStroke(xge_rect_t tRect, float fWidth, uint32_t iColor, int bScreenSpace)
{
	float fRadius;

	fRadius = __xgeShapeMinFloat(tRect.fW, tRect.fH) * 0.5f;
	__xgeShapeRoundRectDrawInternal(tRect, __xgeShapeRoundRectRadii(fRadius), XGE_SHAPE_ROUND_RECT_AUTO, 0, fWidth, iColor, bScreenSpace);
}

void xgeShapeCapsuleStroke(xge_rect_t tRect, float fWidth, uint32_t iColor)
{
	__xgeShapeCapsuleStroke(tRect, fWidth, iColor, 0);
}

void xgeShapeCapsuleStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor)
{
	__xgeShapeCapsuleStroke(tRect, fWidth, iColor, 1);
}

void xgeShapePolygonFill(const xge_vec2_t* pPoints, int iCount, uint32_t iColor)
{
	(void)__xgeShapeConvexFillAA(pPoints, iCount, iColor, 0);
}

void xgeShapePolygonFillPx(const xge_vec2_t* pPoints, int iCount, uint32_t iColor)
{
	(void)__xgeShapeConvexFillAA(pPoints, iCount, iColor, 1);
}

static int __xgeShapeMeshFill(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, int bScreenSpace)
{
	float* pBatchVertices;
	uint32_t* pBatchIndices;
	int iVertexBase;
	int iIndexBase;
	int i;
	int iRet;

	if ( (pVertices == NULL) || (iVertexCount <= 0) || (pIndices == NULL) ||
	     (iIndexCount <= 0) || ((iIndexCount % 3) != 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iVertexCount > (INT32_MAX - g_xge.iShapeAutoVertexCount)) ||
	     (iIndexCount > (INT32_MAX - g_xge.iShapeAutoIndexCount)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iIndexCount; i++ ) {
		if ( pIndices[i] >= (uint32_t)iVertexCount ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = __xgeShapeAutoBatchReserveVertices(g_xge.iShapeAutoVertexCount + iVertexCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeAutoBatchReserveIndices(g_xge.iShapeAutoIndexCount + iIndexCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pBatchVertices = (float*)g_xge.pShapeAutoVertices;
	pBatchIndices = (uint32_t*)g_xge.pShapeAutoIndices;
	iVertexBase = g_xge.iShapeAutoVertexCount;
	iIndexBase = g_xge.iShapeAutoIndexCount;
	for ( i = 0; i < iVertexCount; i++ ) {
		__xgeShapeAutoBatchSetVertex(pBatchVertices, iVertexBase + i, pVertices[i].fX, pVertices[i].fY, pVertices[i].iColor, bScreenSpace);
	}
	for ( i = 0; i < iIndexCount; i++ ) {
		pBatchIndices[iIndexBase + i] = (uint32_t)iVertexBase + pIndices[i];
	}
	g_xge.iShapeAutoVertexCount += iVertexCount;
	g_xge.iShapeAutoIndexCount += iIndexCount;
	return XGE_OK;
}

int xgeShapeMeshFill(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount)
{
	return __xgeShapeMeshFill(pVertices, iVertexCount, pIndices, iIndexCount, 0);
}

int xgeShapeMeshFillPx(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount)
{
	return __xgeShapeMeshFill(pVertices, iVertexCount, pIndices, iIndexCount, 1);
}

int xgeShapeBatchInit(xge_shape_batch pBatch, uint32_t iColor, int iTriangleCapacity, uint32_t iFlags)
{
	size_t iVertexSize;

	if ( (pBatch == NULL) || (iTriangleCapacity <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iTriangleCapacity > (INT32_MAX / (6 * (int)sizeof(float))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pBatch, 0, sizeof(*pBatch));
	iVertexSize = (size_t)iTriangleCapacity * 3u * 2u * sizeof(float);
	pBatch->pVertices = xrtMalloc(iVertexSize);
	if ( pBatch->pVertices == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pBatch->iColor = iColor;
	pBatch->iFlags = iFlags;
	pBatch->iTriangleCapacity = iTriangleCapacity;
	return XGE_OK;
}

void xgeShapeBatchFree(xge_shape_batch pBatch)
{
	if ( pBatch == NULL ) {
		return;
	}
	if ( pBatch->pVertices != NULL ) {
		xrtFree(pBatch->pVertices);
	}
	memset(pBatch, 0, sizeof(*pBatch));
}

void xgeShapeBatchClear(xge_shape_batch pBatch)
{
	if ( pBatch != NULL ) {
		pBatch->iTriangleCount = 0;
	}
}

static void __xgeShapeBatchSetVertex(float* pVertices, int iIndex, xge_vec2_t tPoint, uint32_t iFlags)
{
	float fX;
	float fY;

	__xgeCameraProjectVertex(tPoint.fX, tPoint.fY, 0.0f, iFlags, &fX, &fY);
	pVertices[(iIndex * 2) + 0] = fX;
	pVertices[(iIndex * 2) + 1] = fY;
}

int xgeShapeBatchTriangleFill(xge_shape_batch pBatch, xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC)
{
	float* pVertices;
	int iBase;

	if ( (pBatch == NULL) || (pBatch->pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBatch->iTriangleCount >= pBatch->iTriangleCapacity ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pVertices = (float*)pBatch->pVertices;
	iBase = pBatch->iTriangleCount * 3;
	__xgeShapeBatchSetVertex(pVertices, iBase + 0, tA, pBatch->iFlags);
	__xgeShapeBatchSetVertex(pVertices, iBase + 1, tB, pBatch->iFlags);
	__xgeShapeBatchSetVertex(pVertices, iBase + 2, tC, pBatch->iFlags);
	pBatch->iTriangleCount++;
	return XGE_OK;
}

int xgeShapeBatchRectFill(xge_shape_batch pBatch, xge_rect_t tRect)
{
	xge_vec2_t tA;
	xge_vec2_t tB;
	xge_vec2_t tC;
	xge_vec2_t tD;
	int iRet;

	if ( (pBatch == NULL) || (pBatch->pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pBatch->iTriangleCapacity - pBatch->iTriangleCount) < 2 ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	tA.fX = tRect.fX;
	tA.fY = tRect.fY;
	tB.fX = tRect.fX + tRect.fW;
	tB.fY = tRect.fY;
	tC.fX = tRect.fX;
	tC.fY = tRect.fY + tRect.fH;
	tD.fX = tRect.fX + tRect.fW;
	tD.fY = tRect.fY + tRect.fH;
	iRet = xgeShapeBatchTriangleFill(pBatch, tA, tB, tC);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return xgeShapeBatchTriangleFill(pBatch, tC, tB, tD);
}

int xgeShapeBatchFlush(xge_shape_batch pBatch)
{
	float fR;
	float fG;
	float fB;
	float fA;
	int iVertexCount;

	if ( (pBatch == NULL) || (pBatch->pVertices == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBatch->iTriangleCount <= 0 ) {
		return XGE_OK;
	}
	if ( XGE_COLOR_GET_A(pBatch->iColor) == 0 ) {
		xgeShapeBatchClear(pBatch);
		return XGE_OK;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	if ( __xgeShapeRendererInit() != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	iVertexCount = pBatch->iTriangleCount * 3;
	__xgeColorToFloat(pBatch->iColor, &fR, &fG, &fB, &fA);
	glUseProgram(g_xgeShapeRenderer.iProgram);
	glUniform2f(g_xgeShapeRenderer.iLocResolution, (float)g_xge.iWidth, (float)g_xge.iHeight);
	glUniform4f(g_xgeShapeRenderer.iLocColor, fR, fG, fB, fA);
	glUniform1i(g_xgeShapeRenderer.iLocUseVertexColor, 0);
	glBindVertexArray(g_xgeShapeRenderer.iVAO);
	glBindBuffer(GL_ARRAY_BUFFER, g_xgeShapeRenderer.iVBO);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBufferData(GL_ARRAY_BUFFER, (size_t)iVertexCount * 2u * sizeof(float), pBatch->pVertices, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, iVertexCount);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	__xgeFrameStatsAddDrawCall();
	__xgeFrameStatsAddBatch();
	xgeShapeBatchClear(pBatch);
	return XGE_OK;
}
