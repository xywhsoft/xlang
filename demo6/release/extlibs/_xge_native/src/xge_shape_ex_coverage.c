/*
 * Fixed-point area coverage rasterizer for ShapeEx.
 *
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * The cell accumulation model is adapted from the FreeType gray raster used by
 * ThorVG. Portions are copyright 1996-2002, 2006 The FreeType Project
 * (www.freetype.org). Full upstream terms remain in the vendored reference
 * source and its license files under dev/; no reference source is compiled.
 *
 * ShapeEx feeds both flattened contours and curve-preserving command paths.
 * This C adaptation owns fixed-point line/cubic coverage and RLE span generation.
 */

#define XGE_SHAPE_EX_COVERAGE_PIXEL_BITS 8
#define XGE_SHAPE_EX_COVERAGE_ONE_PIXEL (1 << XGE_SHAPE_EX_COVERAGE_PIXEL_BITS)
#define XGE_SHAPE_EX_COVERAGE_CUBIC_STACK_SIZE (32 * 3 + 1)

typedef struct xge_shape_ex_coverage_point_t {
	int32_t iX;
	int32_t iY;
} xge_shape_ex_coverage_point_t;

typedef struct xge_shape_ex_coverage_cell_t {
	int iX;
	int iY;
	int64_t iArea;
	int64_t iCover;
} xge_shape_ex_coverage_cell_t;

typedef struct xge_shape_ex_coverage_span_t {
	int iX;
	int iY;
	int iLength;
	uint8_t iCoverage;
} xge_shape_ex_coverage_span_t;

typedef struct xge_shape_ex_coverage_raster_t {
	xge_shape_ex_coverage_span_t* pSpans;
	int iSpanCount;
	int iSpanCapacity;
} xge_shape_ex_coverage_raster_t;

typedef struct xge_shape_ex_coverage_contour_t {
	const xge_vec2_t* pPoints;
	int iPointCount;
} xge_shape_ex_coverage_contour_t;

typedef struct xge_shape_ex_coverage_worker_t {
	xge_shape_ex_coverage_cell_t* pCells;
	int iCellCount;
	int iCellCapacity;
	int iMinX;
	int iMinY;
	int iMaxX;
	int iMaxY;
	int iCellX;
	int iCellY;
	int64_t iArea;
	int64_t iCover;
	int bInvalid;
	xge_shape_ex_coverage_point_t tPosition;
} xge_shape_ex_coverage_worker_t;

static void __xgeShapeExCoverageRasterFree(xge_shape_ex_coverage_raster_t* pRaster)
{
	if ( pRaster == NULL ) return;
	xrtFree(pRaster->pSpans);
	memset(pRaster, 0, sizeof(*pRaster));
}

static int __xgeShapeExCoverageCellEnsure(xge_shape_ex_coverage_worker_t* pWorker, int iRequired)
{
	xge_shape_ex_coverage_cell_t* pCells;
	int iCapacity;

	if ( iRequired <= pWorker->iCellCapacity ) return XGE_OK;
	iCapacity = pWorker->iCellCapacity > 0 ? pWorker->iCellCapacity : 256;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > (SIZE_MAX / sizeof(*pCells)) ) return XGE_ERROR_OUT_OF_MEMORY;
	pCells = (xge_shape_ex_coverage_cell_t*)xrtRealloc(
		pWorker->pCells, (size_t)iCapacity * sizeof(*pCells)
	);
	if ( pCells == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pWorker->pCells = pCells;
	pWorker->iCellCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExCoverageSpanEnsure(xge_shape_ex_coverage_raster_t* pRaster, int iRequired)
{
	xge_shape_ex_coverage_span_t* pSpans;
	int iCapacity;

	if ( iRequired <= pRaster->iSpanCapacity ) return XGE_OK;
	iCapacity = pRaster->iSpanCapacity > 0 ? pRaster->iSpanCapacity : 256;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > (SIZE_MAX / sizeof(*pSpans)) ) return XGE_ERROR_OUT_OF_MEMORY;
	pSpans = (xge_shape_ex_coverage_span_t*)xrtRealloc(
		pRaster->pSpans, (size_t)iCapacity * sizeof(*pSpans)
	);
	if ( pSpans == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pRaster->pSpans = pSpans;
	pRaster->iSpanCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExCoverageRecordCell(xge_shape_ex_coverage_worker_t* pWorker)
{
	xge_shape_ex_coverage_cell_t* pCell;
	int iRet;

	if ( (pWorker->iArea == 0) && (pWorker->iCover == 0) ) return XGE_OK;
	iRet = __xgeShapeExCoverageCellEnsure(pWorker, pWorker->iCellCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pCell = &pWorker->pCells[pWorker->iCellCount++];
	pCell->iX = pWorker->iCellX;
	pCell->iY = pWorker->iCellY;
	pCell->iArea = pWorker->iArea;
	pCell->iCover = pWorker->iCover;
	return XGE_OK;
}

static int __xgeShapeExCoverageSetCell(xge_shape_ex_coverage_worker_t* pWorker, int iX, int iY)
{
	int iLocalX = iX - pWorker->iMinX;
	int iLocalY = iY - pWorker->iMinY;
	int iWidth = pWorker->iMaxX - pWorker->iMinX;
	int iHeight = pWorker->iMaxY - pWorker->iMinY;
	int iRet;

	if ( iLocalX < 0 ) iLocalX = -1;
	else if ( iLocalX > iWidth ) iLocalX = iWidth;
	if ( (iLocalX != pWorker->iCellX) || (iLocalY != pWorker->iCellY) ) {
		if ( !pWorker->bInvalid ) {
			iRet = __xgeShapeExCoverageRecordCell(pWorker);
			if ( iRet != XGE_OK ) return iRet;
		}
		pWorker->iArea = 0;
		pWorker->iCover = 0;
		pWorker->iCellX = iLocalX;
		pWorker->iCellY = iLocalY;
	}
	pWorker->bInvalid = ((unsigned int)iLocalY >= (unsigned int)iHeight) || (iLocalX >= iWidth);
	return XGE_OK;
}

static int __xgeShapeExCoverageStartCell(xge_shape_ex_coverage_worker_t* pWorker, int iX, int iY)
{
	if ( iX > pWorker->iMaxX ) iX = pWorker->iMaxX;
	if ( iX < pWorker->iMinX ) iX = pWorker->iMinX - 1;
	pWorker->iArea = 0;
	pWorker->iCover = 0;
	pWorker->iCellX = iX - pWorker->iMinX;
	pWorker->iCellY = iY - pWorker->iMinY;
	pWorker->bInvalid = 0;
	return __xgeShapeExCoverageSetCell(pWorker, iX, iY);
}

static int __xgeShapeExCoverageTrunc(int32_t iValue)
{
	return iValue >> XGE_SHAPE_EX_COVERAGE_PIXEL_BITS;
}

static int __xgeShapeExCoverageFract(int32_t iValue)
{
	return iValue & (XGE_SHAPE_EX_COVERAGE_ONE_PIXEL - 1);
}

static int __xgeShapeExCoverageMoveTo(xge_shape_ex_coverage_worker_t* pWorker, xge_shape_ex_coverage_point_t tPoint)
{
	int iRet;

	if ( !pWorker->bInvalid ) {
		iRet = __xgeShapeExCoverageRecordCell(pWorker);
		if ( iRet != XGE_OK ) return iRet;
	}
	iRet = __xgeShapeExCoverageStartCell(
		pWorker, __xgeShapeExCoverageTrunc(tPoint.iX), __xgeShapeExCoverageTrunc(tPoint.iY)
	);
	if ( iRet != XGE_OK ) return iRet;
	pWorker->tPosition = tPoint;
	return XGE_OK;
}

static int32_t __xgeShapeExCoverageUnsignedDivide(int64_t iA, int64_t iB)
{
	return (int32_t)(((uint64_t)iA * (uint64_t)iB) >> 32);
}

static int __xgeShapeExCoverageLineToDirect(xge_shape_ex_coverage_worker_t* pWorker, xge_shape_ex_coverage_point_t tTo)
{
	xge_shape_ex_coverage_point_t tFrom = pWorker->tPosition;
	int iE1X = __xgeShapeExCoverageTrunc(tFrom.iX);
	int iE1Y = __xgeShapeExCoverageTrunc(tFrom.iY);
	int iE2X = __xgeShapeExCoverageTrunc(tTo.iX);
	int iE2Y = __xgeShapeExCoverageTrunc(tTo.iY);
	int32_t iDX = tTo.iX - tFrom.iX;
	int32_t iDY = tTo.iY - tFrom.iY;
	int iF1X = __xgeShapeExCoverageFract(tFrom.iX);
	int iF1Y = __xgeShapeExCoverageFract(tFrom.iY);
	int iF2X;
	int iF2Y;
	int iRet;

	if ( ((iE1Y >= pWorker->iMaxY) && (iE2Y >= pWorker->iMaxY)) ||
	     ((iE1Y < pWorker->iMinY) && (iE2Y < pWorker->iMinY)) ) {
		pWorker->tPosition = tTo;
		return XGE_OK;
	}
	if ( (iE1X == iE2X) && (iE1Y == iE2Y) ) {
		/* The final fragment below owns this cell. */
	} else if ( iDY == 0 ) {
		iRet = __xgeShapeExCoverageSetCell(pWorker, iE2X, iE1Y);
		if ( iRet != XGE_OK ) return iRet;
	} else if ( iDX == 0 ) {
		if ( iDY > 0 ) {
			do {
				iF2Y = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * iF1X * 2;
				iF1Y = 0;
				iE1Y++;
				iRet = __xgeShapeExCoverageSetCell(pWorker, iE1X, iE1Y);
				if ( iRet != XGE_OK ) return iRet;
			} while ( iE1Y != iE2Y );
		} else {
			do {
				iF2Y = 0;
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * iF1X * 2;
				iF1Y = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				iE1Y--;
				iRet = __xgeShapeExCoverageSetCell(pWorker, iE1X, iE1Y);
				if ( iRet != XGE_OK ) return iRet;
			} while ( iE1Y != iE2Y );
		}
	} else {
		int64_t iProduct = (int64_t)iDX * iF1Y - (int64_t)iDY * iF1X;
		int64_t iDXReciprocal = (iE1X != iE2X) ? (int64_t)0xffffffffLL / iDX : 0;
		int64_t iDYReciprocal = (iE1Y != iE2Y) ? (int64_t)0xffffffffLL / iDY : 0;
		int64_t iPixelDX = (int64_t)iDX * XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
		int64_t iPixelDY = (int64_t)iDY * XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
		int iGuard = 0;
		int iGuardLimit = abs(iE2X - iE1X) + abs(iE2Y - iE1Y) + 8;

		do {
			if ( ++iGuard > iGuardLimit ) return XGE_ERROR_BACKEND_FAILED;
			if ( (iProduct <= 0) && ((iProduct - iPixelDX) > 0) ) {
				iF2X = 0;
				iF2Y = __xgeShapeExCoverageUnsignedDivide(-iProduct, -iDXReciprocal);
				iProduct -= iPixelDY;
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * (iF1X + iF2X);
				iF1X = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				iF1Y = iF2Y;
				iE1X--;
			} else if ( ((iProduct - iPixelDX) <= 0) && ((iProduct - iPixelDX + iPixelDY) > 0) ) {
				iProduct -= iPixelDX;
				iF2X = __xgeShapeExCoverageUnsignedDivide(-iProduct, iDYReciprocal);
				iF2Y = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * (iF1X + iF2X);
				iF1X = iF2X;
				iF1Y = 0;
				iE1Y++;
			} else if ( ((iProduct - iPixelDX + iPixelDY) <= 0) && ((iProduct + iPixelDY) >= 0) ) {
				iProduct += iPixelDY;
				iF2X = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				iF2Y = __xgeShapeExCoverageUnsignedDivide(iProduct, iDXReciprocal);
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * (iF1X + iF2X);
				iF1X = 0;
				iF1Y = iF2Y;
				iE1X++;
			} else {
				iF2X = __xgeShapeExCoverageUnsignedDivide(iProduct, -iDYReciprocal);
				iF2Y = 0;
				iProduct += iPixelDX;
				pWorker->iCover += iF2Y - iF1Y;
				pWorker->iArea += (int64_t)(iF2Y - iF1Y) * (iF1X + iF2X);
				iF1X = iF2X;
				iF1Y = XGE_SHAPE_EX_COVERAGE_ONE_PIXEL;
				iE1Y--;
			}
			iRet = __xgeShapeExCoverageSetCell(pWorker, iE1X, iE1Y);
			if ( iRet != XGE_OK ) return iRet;
		} while ( (iE1X != iE2X) || (iE1Y != iE2Y) );
	}
	iF2X = __xgeShapeExCoverageFract(tTo.iX);
	iF2Y = __xgeShapeExCoverageFract(tTo.iY);
	pWorker->iCover += iF2Y - iF1Y;
	pWorker->iArea += (int64_t)(iF2Y - iF1Y) * (iF1X + iF2X);
	pWorker->tPosition = tTo;
	return XGE_OK;
}

static int __xgeShapeExCoverageLineTo(xge_shape_ex_coverage_worker_t* pWorker, xge_shape_ex_coverage_point_t tTo, int iDepth)
{
	int32_t iDX = tTo.iX - pWorker->tPosition.iX;
	int32_t iDY = tTo.iY - pWorker->tPosition.iY;
	int32_t iAbsX = iDX < 0 ? -iDX : iDX;
	int32_t iAbsY = iDY < 0 ? -iDY : iDY;
	int32_t iLength = iAbsX > iAbsY ? iAbsX + ((3 * iAbsY) >> 3) : iAbsY + ((3 * iAbsX) >> 3);

	if ( iLength > SHRT_MAX ) {
		xge_shape_ex_coverage_point_t tMiddle;
		int iRet;

		if ( iDepth >= 32 ) return XGE_ERROR_BACKEND_FAILED;
		tMiddle.iX = (pWorker->tPosition.iX >> 1) + (tTo.iX >> 1);
		tMiddle.iY = (pWorker->tPosition.iY >> 1) + (tTo.iY >> 1);
		if ( (tMiddle.iX == pWorker->tPosition.iX) && (tMiddle.iY == pWorker->tPosition.iY) ) {
			return __xgeShapeExCoverageLineToDirect(pWorker, tTo);
		}
		iRet = __xgeShapeExCoverageLineTo(pWorker, tMiddle, iDepth + 1);
		if ( iRet != XGE_OK ) return iRet;
		return __xgeShapeExCoverageLineTo(pWorker, tTo, iDepth + 1);
	}
	return __xgeShapeExCoverageLineToDirect(pWorker, tTo);
}

static int __xgeShapeExCoverageCellCompare(const void* pA, const void* pB)
{
	const xge_shape_ex_coverage_cell_t* pCellA = (const xge_shape_ex_coverage_cell_t*)pA;
	const xge_shape_ex_coverage_cell_t* pCellB = (const xge_shape_ex_coverage_cell_t*)pB;

	if ( pCellA->iY != pCellB->iY ) return pCellA->iY < pCellB->iY ? -1 : 1;
	if ( pCellA->iX != pCellB->iX ) return pCellA->iX < pCellB->iX ? -1 : 1;
	return 0;
}

static int __xgeShapeExCoverageAppendSpan(xge_shape_ex_coverage_raster_t* pRaster, int iX, int iY, int iLength, int iCoverage)
{
	xge_shape_ex_coverage_span_t* pLast;
	int iRet;

	if ( (iLength <= 0) || (iCoverage <= 0) ) return XGE_OK;
	if ( iCoverage > 255 ) iCoverage = 255;
	if ( pRaster->iSpanCount > 0 ) {
		pLast = &pRaster->pSpans[pRaster->iSpanCount - 1];
		if ( (pLast->iY == iY) && (pLast->iCoverage == (uint8_t)iCoverage) &&
		     ((pLast->iX + pLast->iLength) == iX) ) {
			if ( pLast->iLength > (INT_MAX - iLength) ) return XGE_ERROR_OUT_OF_MEMORY;
			pLast->iLength += iLength;
			return XGE_OK;
		}
	}
	iRet = __xgeShapeExCoverageSpanEnsure(pRaster, pRaster->iSpanCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pLast = &pRaster->pSpans[pRaster->iSpanCount++];
	pLast->iX = iX;
	pLast->iY = iY;
	pLast->iLength = iLength;
	pLast->iCoverage = (uint8_t)iCoverage;
	return XGE_OK;
}

static int __xgeShapeExCoverageAreaToAlpha(int64_t iArea, int bEvenOdd)
{
	int iCoverage = (int)(iArea >> (XGE_SHAPE_EX_COVERAGE_PIXEL_BITS * 2 + 1 - 8));

	if ( iCoverage < 0 ) iCoverage = -iCoverage;
	if ( bEvenOdd ) {
		iCoverage &= 511;
		if ( iCoverage > 255 ) iCoverage = 511 - iCoverage;
	} else if ( iCoverage > 255 ) {
		iCoverage = 255;
	}
	return iCoverage;
}

static int __xgeShapeExCoverageSweep(xge_shape_ex_coverage_worker_t* pWorker, xge_shape_ex_coverage_raster_t* pRaster, int bEvenOdd)
{
	int iWidth = pWorker->iMaxX - pWorker->iMinX;
	int iHeight = pWorker->iMaxY - pWorker->iMinY;
	int iCell = 0;
	int iY;

	if ( pWorker->iCellCount <= 0 ) return XGE_OK;
	qsort(
		pWorker->pCells, (size_t)pWorker->iCellCount, sizeof(*pWorker->pCells),
		__xgeShapeExCoverageCellCompare
	);
	for ( iY = 0; iY < iHeight; iY++ ) {
		int64_t iCover = 0;
		int iX = 0;

		while ( (iCell < pWorker->iCellCount) && (pWorker->pCells[iCell].iY < iY) ) iCell++;
		while ( (iCell < pWorker->iCellCount) && (pWorker->pCells[iCell].iY == iY) ) {
			int iCellX = pWorker->pCells[iCell].iX;
			int64_t iArea = 0;
			int64_t iCellCover = 0;
			int iCoverage;
			int iRet;

			while ( (iCell < pWorker->iCellCount) && (pWorker->pCells[iCell].iY == iY) &&
			        (pWorker->pCells[iCell].iX == iCellX) ) {
				iArea += pWorker->pCells[iCell].iArea;
				iCellCover += pWorker->pCells[iCell].iCover;
				iCell++;
			}
			if ( (iCellX > iX) && (iCover != 0) ) {
				iCoverage = __xgeShapeExCoverageAreaToAlpha(
					iCover * (XGE_SHAPE_EX_COVERAGE_ONE_PIXEL * 2), bEvenOdd
				);
				iRet = __xgeShapeExCoverageAppendSpan(
					pRaster, pWorker->iMinX + iX, pWorker->iMinY + iY,
					(iCellX < iWidth ? iCellX : iWidth) - iX, iCoverage
				);
				if ( iRet != XGE_OK ) return iRet;
			}
			iCover += iCellCover;
			iCoverage = __xgeShapeExCoverageAreaToAlpha(
				iCover * (XGE_SHAPE_EX_COVERAGE_ONE_PIXEL * 2) - iArea, bEvenOdd
			);
			if ( (iCoverage > 0) && (iCellX >= 0) && (iCellX < iWidth) ) {
				iRet = __xgeShapeExCoverageAppendSpan(
					pRaster, pWorker->iMinX + iCellX, pWorker->iMinY + iY, 1, iCoverage
				);
				if ( iRet != XGE_OK ) return iRet;
			}
			iX = iCellX + 1;
		}
		if ( (iCover != 0) && (iX < iWidth) ) {
			int iCoverage = __xgeShapeExCoverageAreaToAlpha(
				iCover * (XGE_SHAPE_EX_COVERAGE_ONE_PIXEL * 2), bEvenOdd
			);
			int iRet = __xgeShapeExCoverageAppendSpan(
				pRaster, pWorker->iMinX + iX, pWorker->iMinY + iY, iWidth - iX, iCoverage
			);
			if ( iRet != XGE_OK ) return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeShapeExCoveragePointFromFloat(xge_vec2_t tPoint, xge_shape_ex_coverage_point_t* pOut)
{
	double fX;
	double fY;

	if ( pOut == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	fX = (double)tPoint.fX * 64.0;
	fY = (double)tPoint.fY * 64.0;
	if ( !isfinite(fX) || !isfinite(fY) ||
	     (fX < ((double)INT32_MIN / 8.0)) || (fX > ((double)INT32_MAX / 8.0)) ||
	     (fY < ((double)INT32_MIN / 8.0)) || (fY > ((double)INT32_MAX / 8.0)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pOut->iX = (int32_t)fX * 4;
	pOut->iY = (int32_t)fY * 4;
	return XGE_OK;
}

static xge_vec2_t __xgeShapeExCoverageTransformPoint(xge_shape_ex_matrix_t tMatrix, xge_vec2_t tPoint)
{
	xge_vec2_t tOut;

	tOut.fX = (tMatrix.fA * tPoint.fX) + (tMatrix.fC * tPoint.fY) + tMatrix.fE;
	tOut.fY = (tMatrix.fB * tPoint.fX) + (tMatrix.fD * tPoint.fY) + tMatrix.fF;
	return tOut;
}

static int32_t __xgeShapeExCoverageHypot(xge_shape_ex_coverage_point_t tPoint)
{
	int32_t iX = tPoint.iX < 0 ? -tPoint.iX : tPoint.iX;
	int32_t iY = tPoint.iY < 0 ? -tPoint.iY : tPoint.iY;

	return iX > iY ? iX + ((3 * iY) >> 3) : iY + ((3 * iX) >> 3);
}

static void __xgeShapeExCoverageSplitCubic(xge_shape_ex_coverage_point_t* pBase)
{
	int32_t a;
	int32_t b;
	int32_t c;
	int32_t d;

	pBase[6].iX = pBase[3].iX;
	c = pBase[1].iX;
	d = pBase[2].iX;
	pBase[1].iX = a = (pBase[0].iX + c) >> 1;
	pBase[5].iX = b = (pBase[3].iX + d) >> 1;
	c = (c + d) >> 1;
	pBase[2].iX = a = (a + c) >> 1;
	pBase[4].iX = b = (b + c) >> 1;
	pBase[3].iX = (a + b) >> 1;

	pBase[6].iY = pBase[3].iY;
	c = pBase[1].iY;
	d = pBase[2].iY;
	pBase[1].iY = a = (pBase[0].iY + c) >> 1;
	pBase[5].iY = b = (pBase[3].iY + d) >> 1;
	c = (c + d) >> 1;
	pBase[2].iY = a = (a + c) >> 1;
	pBase[4].iY = b = (b + c) >> 1;
	pBase[3].iY = (a + b) >> 1;
}

static int __xgeShapeExCoverageCubicTo(
	xge_shape_ex_coverage_worker_t* pWorker,
	xge_shape_ex_coverage_point_t tControl1,
	xge_shape_ex_coverage_point_t tControl2,
	xge_shape_ex_coverage_point_t tTo
)
{
	xge_shape_ex_coverage_point_t arrStack[XGE_SHAPE_EX_COVERAGE_CUBIC_STACK_SIZE];
	int iArc = 0;
	int iRet;
	int i;
	int32_t iMinY;
	int32_t iMaxY;

	arrStack[0] = tTo;
	arrStack[1] = tControl2;
	arrStack[2] = tControl1;
	arrStack[3] = pWorker->tPosition;
	iMinY = arrStack[0].iY;
	iMaxY = arrStack[0].iY;
	for ( i = 1; i < 4; i++ ) {
		if ( arrStack[i].iY < iMinY ) iMinY = arrStack[i].iY;
		if ( arrStack[i].iY > iMaxY ) iMaxY = arrStack[i].iY;
	}
	if ( (__xgeShapeExCoverageTrunc(iMinY) >= pWorker->iMaxY) ||
	     (__xgeShapeExCoverageTrunc(iMaxY) < pWorker->iMinY) ) {
		return __xgeShapeExCoverageLineTo(pWorker, tTo, 0);
	}
	while ( iArc < XGE_SHAPE_EX_COVERAGE_CUBIC_STACK_SIZE ) {
		xge_shape_ex_coverage_point_t* pArc = arrStack + iArc;
		xge_shape_ex_coverage_point_t tDiff;
		xge_shape_ex_coverage_point_t tDiff1;
		xge_shape_ex_coverage_point_t tDiff2;
		int32_t iLength;
		int64_t iLimit;
		int bSplit;

		tDiff.iX = pArc[3].iX - pArc[0].iX;
		tDiff.iY = pArc[3].iY - pArc[0].iY;
		iLength = __xgeShapeExCoverageHypot(tDiff);
		bSplit = iLength > SHRT_MAX;
		if ( !bSplit ) {
			int64_t iCross;
			int64_t iDot;

			iLimit = (int64_t)iLength * (XGE_SHAPE_EX_COVERAGE_ONE_PIXEL / 6);
			tDiff1.iX = pArc[1].iX - pArc[0].iX;
			tDiff1.iY = pArc[1].iY - pArc[0].iY;
			iCross = (int64_t)tDiff.iY * tDiff1.iX - (int64_t)tDiff.iX * tDiff1.iY;
			if ( iCross < 0 ) iCross = -iCross;
			if ( iCross > iLimit ) bSplit = 1;
			tDiff2.iX = pArc[2].iX - pArc[0].iX;
			tDiff2.iY = pArc[2].iY - pArc[0].iY;
			iCross = (int64_t)tDiff.iY * tDiff2.iX - (int64_t)tDiff.iX * tDiff2.iY;
			if ( iCross < 0 ) iCross = -iCross;
			if ( iCross > iLimit ) bSplit = 1;
			iDot = (int64_t)tDiff1.iX * (tDiff1.iX - tDiff.iX) +
			       (int64_t)tDiff1.iY * (tDiff1.iY - tDiff.iY);
			if ( iDot > 0 ) bSplit = 1;
			iDot = (int64_t)tDiff2.iX * (tDiff2.iX - tDiff.iX) +
			       (int64_t)tDiff2.iY * (tDiff2.iY - tDiff.iY);
			if ( iDot > 0 ) bSplit = 1;
		}
		if ( bSplit && ((iArc + 6) < XGE_SHAPE_EX_COVERAGE_CUBIC_STACK_SIZE) ) {
			__xgeShapeExCoverageSplitCubic(pArc);
			iArc += 3;
			continue;
		}
		iRet = __xgeShapeExCoverageLineTo(pWorker, pArc[0], 0);
		if ( iRet != XGE_OK ) return iRet;
		if ( iArc == 0 ) return XGE_OK;
		iArc -= 3;
	}
	return XGE_ERROR_BACKEND_FAILED;
}

static int __xgeShapeExCoverageRasterizePath(
	xge_shape_ex_coverage_raster_t* pRaster,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount,
	xge_shape_ex_matrix_t tMatrix,
	int iMinX,
	int iMinY,
	int iMaxX,
	int iMaxY,
	int bEvenOdd
)
{
	xge_shape_ex_coverage_worker_t tWorker;
	xge_shape_ex_coverage_point_t tCurrent;
	xge_shape_ex_coverage_point_t tStart;
	xge_vec2_t tCurrentFloat;
	xge_vec2_t tStartFloat;
	int iPointRead = 0;
	int iContourCount = 0;
	int bHasCurrent = 0;
	int bContourOpen = 0;
	int iRet = XGE_OK;
	int i;

	if ( (pRaster == NULL) || (pCommands == NULL) || (iCommandCount <= 0) ||
	     (pPoints == NULL) || (iPointCount <= 0) ||
	     (iMaxX <= iMinX) || (iMaxY <= iMinY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tWorker, 0, sizeof(tWorker));
	tWorker.iMinX = iMinX;
	tWorker.iMinY = iMinY;
	tWorker.iMaxX = iMaxX;
	tWorker.iMaxY = iMaxY;
	tWorker.bInvalid = 1;
	memset(&tCurrentFloat, 0, sizeof(tCurrentFloat));
	memset(&tStartFloat, 0, sizeof(tStartFloat));
	for ( i = 0; i < iCommandCount; i++ ) {
		uint8_t iCommand = pCommands[i];

		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto xge_shape_ex_coverage_path_done;
			}
			if ( bContourOpen ) {
				iRet = __xgeShapeExCoverageLineTo(&tWorker, tStart, 0);
				if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
				iContourCount++;
			}
			tCurrentFloat = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			iRet = __xgeShapeExCoveragePointFromFloat(tCurrentFloat, &tCurrent);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
			iRet = __xgeShapeExCoverageMoveTo(&tWorker, tCurrent);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
			tStart = tCurrent;
			tStartFloat = tCurrentFloat;
			bHasCurrent = 1;
			bContourOpen = 1;
		} else if ( (iCommand == XGE_SHAPE_EX_CMD_LINE_TO) && bHasCurrent ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto xge_shape_ex_coverage_path_done;
			}
			if ( !bContourOpen ) {
				iRet = __xgeShapeExCoverageMoveTo(&tWorker, tCurrent);
				if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
				tStart = tCurrent;
				tStartFloat = tCurrentFloat;
				bContourOpen = 1;
			}
			tCurrentFloat = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			iRet = __xgeShapeExCoveragePointFromFloat(tCurrentFloat, &tCurrent);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoverageLineTo(&tWorker, tCurrent, 0);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
		} else if ( (iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO) && bHasCurrent ) {
			xge_shape_ex_coverage_point_t tControl1;
			xge_shape_ex_coverage_point_t tControl2;
			xge_vec2_t tControl1Float;
			xge_vec2_t tControl2Float;

			if ( (iPointRead + 3) > iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto xge_shape_ex_coverage_path_done;
			}
			if ( !bContourOpen ) {
				iRet = __xgeShapeExCoverageMoveTo(&tWorker, tCurrent);
				if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
				tStart = tCurrent;
				tStartFloat = tCurrentFloat;
				bContourOpen = 1;
			}
			tControl1Float = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			tControl2Float = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			tCurrentFloat = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			iRet = __xgeShapeExCoveragePointFromFloat(tControl1Float, &tControl1);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoveragePointFromFloat(tControl2Float, &tControl2);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoveragePointFromFloat(tCurrentFloat, &tCurrent);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoverageCubicTo(&tWorker, tControl1, tControl2, tCurrent);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
		} else if ( (iCommand == XGE_SHAPE_EX_CMD_QUAD_TO) && bHasCurrent ) {
			xge_shape_ex_coverage_point_t tControl1;
			xge_shape_ex_coverage_point_t tControl2;
			xge_vec2_t tControl;
			xge_vec2_t tEnd;
			xge_vec2_t tControl1Float;
			xge_vec2_t tControl2Float;

			if ( (iPointRead + 2) > iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto xge_shape_ex_coverage_path_done;
			}
			if ( !bContourOpen ) {
				iRet = __xgeShapeExCoverageMoveTo(&tWorker, tCurrent);
				if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
				tStart = tCurrent;
				tStartFloat = tCurrentFloat;
				bContourOpen = 1;
			}
			tControl = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			tEnd = __xgeShapeExCoverageTransformPoint(tMatrix, pPoints[iPointRead++]);
			tControl1Float.fX = tCurrentFloat.fX + (tControl.fX - tCurrentFloat.fX) * (2.0f / 3.0f);
			tControl1Float.fY = tCurrentFloat.fY + (tControl.fY - tCurrentFloat.fY) * (2.0f / 3.0f);
			tControl2Float.fX = tEnd.fX + (tControl.fX - tEnd.fX) * (2.0f / 3.0f);
			tControl2Float.fY = tEnd.fY + (tControl.fY - tEnd.fY) * (2.0f / 3.0f);
			tCurrentFloat = tEnd;
			iRet = __xgeShapeExCoveragePointFromFloat(tControl1Float, &tControl1);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoveragePointFromFloat(tControl2Float, &tControl2);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoveragePointFromFloat(tCurrentFloat, &tCurrent);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExCoverageCubicTo(&tWorker, tControl1, tControl2, tCurrent);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			if ( bContourOpen ) {
				iRet = __xgeShapeExCoverageLineTo(&tWorker, tStart, 0);
				if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
				iContourCount++;
				bContourOpen = 0;
				tCurrent = tStart;
				tCurrentFloat = tStartFloat;
			}
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
			goto xge_shape_ex_coverage_path_done;
		}
	}
	if ( iPointRead != iPointCount ) {
		iRet = XGE_ERROR_INVALID_ARGUMENT;
		goto xge_shape_ex_coverage_path_done;
	}
	if ( bContourOpen ) {
		iRet = __xgeShapeExCoverageLineTo(&tWorker, tStart, 0);
		if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
		iContourCount++;
	}
	if ( iContourCount > 0 ) {
		if ( !tWorker.bInvalid ) {
			iRet = __xgeShapeExCoverageRecordCell(&tWorker);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_path_done;
		}
		iRet = __xgeShapeExCoverageSweep(&tWorker, pRaster, bEvenOdd);
	}

xge_shape_ex_coverage_path_done:
	xrtFree(tWorker.pCells);
	return iRet;
}

static int __xgeShapeExCoverageRasterizeContours(
	xge_shape_ex_coverage_raster_t* pRaster,
	const xge_shape_ex_coverage_contour_t* pContours,
	int iContourCount,
	int iMinX,
	int iMinY,
	int iMaxX,
	int iMaxY,
	int bEvenOdd
)
{
	xge_shape_ex_coverage_worker_t tWorker;
	xge_shape_ex_coverage_point_t tFirst;
	xge_shape_ex_coverage_point_t tPoint;
	int iRet;
	int i;
	int j;
	int iValidCount;

	if ( (pRaster == NULL) || (pContours == NULL) || (iContourCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iMaxX <= iMinX) || (iMaxY <= iMinY) ) return XGE_OK;
	memset(&tWorker, 0, sizeof(tWorker));
	tWorker.iMinX = iMinX;
	tWorker.iMinY = iMinY;
	tWorker.iMaxX = iMaxX;
	tWorker.iMaxY = iMaxY;
	tWorker.bInvalid = 1;
	iRet = XGE_OK;
	iValidCount = 0;
	for ( i = 0; i < iContourCount; i++ ) {
		const xge_shape_ex_coverage_contour_t* pContour = &pContours[i];

		if ( (pContour->pPoints == NULL) || (pContour->iPointCount < 3) ) continue;
		iRet = __xgeShapeExCoveragePointFromFloat(pContour->pPoints[0], &tFirst);
		if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
		iRet = __xgeShapeExCoverageMoveTo(&tWorker, tFirst);
		if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
		for ( j = 1; j < pContour->iPointCount; j++ ) {
			iRet = __xgeShapeExCoveragePointFromFloat(pContour->pPoints[j], &tPoint);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
			iRet = __xgeShapeExCoverageLineTo(&tWorker, tPoint, 0);
			if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
		}
		iRet = __xgeShapeExCoverageLineTo(&tWorker, tFirst, 0);
		if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
		iValidCount++;
	}
	if ( iValidCount <= 0 ) goto xge_shape_ex_coverage_rasterize_done;
	if ( !tWorker.bInvalid ) {
		iRet = __xgeShapeExCoverageRecordCell(&tWorker);
		if ( iRet != XGE_OK ) goto xge_shape_ex_coverage_rasterize_done;
	}
	iRet = __xgeShapeExCoverageSweep(&tWorker, pRaster, bEvenOdd);

xge_shape_ex_coverage_rasterize_done:
	xrtFree(tWorker.pCells);
	return iRet;
}

static int __xgeShapeExCoverageRasterizeContour(
	xge_shape_ex_coverage_raster_t* pRaster,
	const xge_vec2_t* pPoints,
	int iPointCount,
	int iMinX,
	int iMinY,
	int iMaxX,
	int iMaxY,
	int bEvenOdd
)
{
	xge_shape_ex_coverage_contour_t tContour;

	tContour.pPoints = pPoints;
	tContour.iPointCount = iPointCount;
	return __xgeShapeExCoverageRasterizeContours(
		pRaster, &tContour, 1, iMinX, iMinY, iMaxX, iMaxY, bEvenOdd
	);
}
