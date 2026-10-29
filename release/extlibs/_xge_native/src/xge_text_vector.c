#ifndef XGE_NO_TEXT

static float __xgeTextOutlineX(float fPenX, float fScale, short iX)
{
	return fPenX + (float)iX * fScale;
}

static float __xgeTextOutlineY(float fBaselineY, float fScale, short iY)
{
	return fBaselineY - (float)iY * fScale;
}

int xgeFontGlyphOutlineAppendShapeEx(xge_font pFont, int iGlyph, xge_shape_ex pShape, float fPenX, float fBaselineY)
{
	const stbtt_vertex* pVertices;
	const stbtt_vertex* pVertex;
	float fScale;
	int iVertexCount;
	int bContourOpen;
	int iRet;
	int i;

	if ( (pFont == NULL) || (pShape == NULL) || (iGlyph < 0) ||
	     !isfinite(fPenX) || !isfinite(fBaselineY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pVertices = NULL;
	iVertexCount = 0;
	iRet = __xgeFontGlyphOutlineGet(pFont, iGlyph, &pVertices, &iVertexCount);
	if ( iRet != XGE_OK ) return iRet;
	if ( (pVertices == NULL) || (iVertexCount <= 0) ) return XGE_OK;
	fScale = pFont->fScale;
	bContourOpen = 0;
	for ( i = 0; i < iVertexCount; i++ ) {
		pVertex = &pVertices[i];
		switch ( pVertex->type ) {
		case STBTT_vmove:
			if ( bContourOpen ) {
				iRet = xgeShapeExClose(pShape);
				if ( iRet != XGE_OK ) return iRet;
			}
			iRet = xgeShapeExMoveTo(
				pShape,
				__xgeTextOutlineX(fPenX, fScale, pVertex->x),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->y)
			);
			bContourOpen = 1;
			break;
		case STBTT_vline:
			iRet = xgeShapeExLineTo(
				pShape,
				__xgeTextOutlineX(fPenX, fScale, pVertex->x),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->y)
			);
			break;
		case STBTT_vcurve:
			iRet = xgeShapeExQuadTo(
				pShape,
				__xgeTextOutlineX(fPenX, fScale, pVertex->cx),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->cy),
				__xgeTextOutlineX(fPenX, fScale, pVertex->x),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->y)
			);
			break;
		case STBTT_vcubic:
			iRet = xgeShapeExCubicTo(
				pShape,
				__xgeTextOutlineX(fPenX, fScale, pVertex->cx),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->cy),
				__xgeTextOutlineX(fPenX, fScale, pVertex->cx1),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->cy1),
				__xgeTextOutlineX(fPenX, fScale, pVertex->x),
				__xgeTextOutlineY(fBaselineY, fScale, pVertex->y)
			);
			break;
		default:
			return XGE_ERROR_RESOURCE_FAILED;
		}
		if ( iRet != XGE_OK ) return iRet;
	}
	return bContourOpen ? xgeShapeExClose(pShape) : XGE_OK;
}

int xgeGlyphRunAppendShapeEx(const xge_glyph_run_t* pRun, xge_shape_ex pShape, float fX, float fY)
{
	const xge_glyph_position_t* pPosition;
	float fPenX;
	float fBaselineY;
	int iRet;
	int i;

	if ( (pRun == NULL) || (pShape == NULL) || !isfinite(fX) || !isfinite(fY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fPenX = fX;
	fBaselineY = fY + pRun->fAscent;
	for ( i = 0; i < pRun->iGlyphCount; i++ ) {
		pPosition = &pRun->pGlyphs[i];
		if ( (pPosition->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) {
			fPenX = fX;
			fBaselineY += pRun->fLineHeight;
			continue;
		}
		if ( (pPosition->pFont != NULL) && (pPosition->iGlyph >= 0) ) {
			iRet = xgeFontGlyphOutlineAppendShapeEx(
				pPosition->pFont, pPosition->iGlyph, pShape,
				fPenX + pPosition->fOffsetX,
				fBaselineY + pPosition->fOffsetY
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		fPenX += pPosition->fAdvanceX;
	}
	return XGE_OK;
}

#else

int xgeFontGlyphOutlineAppendShapeEx(xge_font pFont, int iGlyph, xge_shape_ex pShape, float fPenX, float fBaselineY)
{
	(void)pFont; (void)iGlyph; (void)pShape; (void)fPenX; (void)fBaselineY;
	return XGE_ERROR_UNSUPPORTED;
}

int xgeGlyphRunAppendShapeEx(const xge_glyph_run_t* pRun, xge_shape_ex pShape, float fX, float fY)
{
	(void)pRun; (void)pShape; (void)fX; (void)fY;
	return XGE_ERROR_UNSUPPORTED;
}

#endif
