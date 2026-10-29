/*
 * Curve-preserving stroke outline builder for ShapeEx.
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
 * This C implementation follows ThorVG's tvgSwStroke.cpp border construction.
 */

#define XGE_SHAPE_EX_STROKE_TAG_POINT 1
#define XGE_SHAPE_EX_STROKE_TAG_CUBIC 2
#define XGE_SHAPE_EX_STROKE_TAG_BEGIN 4
#define XGE_SHAPE_EX_STROKE_TAG_END 8
#define XGE_SHAPE_EX_STROKE_EPSILON 1.0e-6f
#define XGE_SHAPE_EX_STROKE_PI 3.14159265358979323846f
#define XGE_SHAPE_EX_STROKE_PI2 (XGE_SHAPE_EX_STROKE_PI * 0.5f)
#define XGE_SHAPE_EX_STROKE_2PI (XGE_SHAPE_EX_STROKE_PI * 2.0f)
#define XGE_SHAPE_EX_STROKE_DASH_THRESHOLD 0.001f
#define XGE_SHAPE_EX_STROKE_BEZIER_EPSILON 0.01f

typedef struct xge_shape_ex_stroke_input_t {
	xge_vec2_t* pPoints;
	uint8_t* pTypes;
	int* pContourEnds;
	uint8_t* pClosed;
	int iPointCount;
	int iPointCapacity;
	int iContourCount;
	int iContourCapacity;
} xge_shape_ex_stroke_input_t;

typedef struct xge_shape_ex_stroke_border_t {
	xge_vec2_t* pPoints;
	uint8_t* pTags;
	int iCount;
	int iCapacity;
	int iStart;
	int bMovable;
} xge_shape_ex_stroke_border_t;

typedef struct xge_shape_ex_stroker_t {
	float fAngleIn;
	float fAngleOut;
	xge_vec2_t tCenter;
	float fLength;
	float fSubPathAngle;
	xge_vec2_t tSubPathStart;
	float fSubPathLength;
	float fWidth;
	float fMiterLimit;
	xge_shape_ex_stroke_border_t arrBorders[2];
	int iCap;
	int iJoin;
	int bFirstPoint;
	int bClosedSubPath;
	int bHandleWideStrokes;
} xge_shape_ex_stroker_t;

typedef struct xge_shape_ex_stroke_path_t {
	uint8_t* pCommands;
	xge_vec2_t* pPoints;
	int iCommandCount;
	int iCommandCapacity;
	int iPointCount;
	int iPointCapacity;
} xge_shape_ex_stroke_path_t;

typedef struct xge_shape_ex_stroke_cubic_t {
	xge_vec2_t tStart;
	xge_vec2_t tControl1;
	xge_vec2_t tControl2;
	xge_vec2_t tEnd;
} xge_shape_ex_stroke_cubic_t;

typedef struct xge_shape_ex_stroke_dash_t {
	xge_shape_ex_stroke_path_t* pPath;
	const float* pPattern;
	int iPatternCount;
	int iLogicalCount;
	int iCurrentIndex;
	float fCurrentLength;
	float fOffset;
	xge_vec2_t tStart;
	xge_vec2_t tCurrent;
	int bCurrentGap;
	int bMove;
	int bValidPoint;
} xge_shape_ex_stroke_dash_t;

typedef struct xge_shape_ex_stroke_trim_segment_t {
	uint8_t iCommand;
	xge_shape_ex_stroke_cubic_t tCubic;
	float fLength;
	int iContour;
} xge_shape_ex_stroke_trim_segment_t;

typedef struct xge_shape_ex_stroke_trim_contour_t {
	int iSegmentStart;
	int iSegmentCount;
	float fLength;
	int bClosed;
} xge_shape_ex_stroke_trim_contour_t;

typedef struct xge_shape_ex_stroke_trim_input_t {
	xge_shape_ex_stroke_trim_segment_t* pSegments;
	xge_shape_ex_stroke_trim_contour_t* pContours;
	int iSegmentCount;
	int iSegmentCapacity;
	int iContourCount;
	int iContourCapacity;
} xge_shape_ex_stroke_trim_input_t;

static int __xgeShapeExStrokeFloatZero(float fValue)
{
	return fabsf(fValue) <= XGE_SHAPE_EX_STROKE_EPSILON;
}

static int __xgeShapeExStrokePointEqual(xge_vec2_t tA, xge_vec2_t tB)
{
	return (tA.fX == tB.fX) && (tA.fY == tB.fY);
}

static int __xgeShapeExStrokePointTiny(xge_vec2_t tPoint)
{
	const float fEpsilon = 2.0f / 64.0f;

	return (fabsf(tPoint.fX) < fEpsilon) && (fabsf(tPoint.fY) < fEpsilon);
}

static xge_vec2_t __xgeShapeExStrokePointAdd(xge_vec2_t tA, xge_vec2_t tB)
{
	return (xge_vec2_t){tA.fX + tB.fX, tA.fY + tB.fY};
}

static xge_vec2_t __xgeShapeExStrokePointSub(xge_vec2_t tA, xge_vec2_t tB)
{
	return (xge_vec2_t){tA.fX - tB.fX, tA.fY - tB.fY};
}

static xge_vec2_t __xgeShapeExStrokePointScale(xge_vec2_t tPoint, float fScale)
{
	return (xge_vec2_t){tPoint.fX * fScale, tPoint.fY * fScale};
}

static xge_vec2_t __xgeShapeExStrokePointMid(xge_vec2_t tA, xge_vec2_t tB)
{
	return (xge_vec2_t){(tA.fX + tB.fX) * 0.5f, (tA.fY + tB.fY) * 0.5f};
}

static xge_vec2_t __xgeShapeExStrokePointRotate(xge_vec2_t tPoint, float fRadians)
{
	float fCos;
	float fSin;

	if ( __xgeShapeExStrokeFloatZero(fRadians) ) return tPoint;
	fCos = cosf(fRadians);
	fSin = sinf(fRadians);
	return (xge_vec2_t){
		tPoint.fX * fCos - tPoint.fY * fSin,
		tPoint.fX * fSin + tPoint.fY * fCos
	};
}

static xge_vec2_t __xgeShapeExStrokePointTransform(
	xge_shape_ex_matrix_t tMatrix,
	xge_vec2_t tPoint
)
{
	return (xge_vec2_t){
		tPoint.fX * tMatrix.fA + tPoint.fY * tMatrix.fC + tMatrix.fE,
		tPoint.fX * tMatrix.fB + tPoint.fY * tMatrix.fD + tMatrix.fF
	};
}

static float __xgeShapeExStrokePointLength(xge_vec2_t tPoint)
{
	return hypotf(tPoint.fX, tPoint.fY);
}

static float __xgeShapeExStrokePointAngle(xge_vec2_t tPoint)
{
	float fAbsX;
	float fAbsY;
	float fRatio;
	float fRatioSquared;
	float fAngle;

	if ( (tPoint.fY == 0.0f) && (tPoint.fX == 0.0f) ) return 0.0f;
	fAbsX = fabsf(tPoint.fX);
	fAbsY = fabsf(tPoint.fY);
	fRatio = fminf(fAbsX, fAbsY) / fmaxf(fAbsX, fAbsY);
	fRatioSquared = fRatio * fRatio;
	fAngle = ((-0.0464964749f * fRatioSquared + 0.15931422f) * fRatioSquared -
		0.327622764f) * fRatioSquared * fRatio + fRatio;
	if ( fAbsY > fAbsX ) fAngle = 1.57079637f - fAngle;
	if ( tPoint.fX < 0.0f ) fAngle = 3.14159274f - fAngle;
	return tPoint.fY < 0.0f ? -fAngle : fAngle;
}

static float __xgeShapeExStrokeAngleDiff(float fAngle1, float fAngle2)
{
	float fDelta = fmodf(fAngle2 - fAngle1, XGE_SHAPE_EX_STROKE_2PI);

	if ( fDelta < 0.0f ) fDelta += XGE_SHAPE_EX_STROKE_2PI;
	if ( fDelta > XGE_SHAPE_EX_STROKE_PI ) fDelta -= XGE_SHAPE_EX_STROKE_2PI;
	return fDelta;
}

static float __xgeShapeExStrokeAngleMean(float fAngle1, float fAngle2)
{
	return fAngle1 + __xgeShapeExStrokeAngleDiff(fAngle1, fAngle2) * 0.5f;
}

static float __xgeShapeExStrokeSideRotation(int iSide)
{
	return XGE_SHAPE_EX_STROKE_PI2 - (float)iSide * XGE_SHAPE_EX_STROKE_PI;
}

static void __xgeShapeExStrokeInputFree(xge_shape_ex_stroke_input_t* pInput)
{
	if ( pInput == NULL ) return;
	xrtFree(pInput->pPoints);
	xrtFree(pInput->pTypes);
	xrtFree(pInput->pContourEnds);
	xrtFree(pInput->pClosed);
	memset(pInput, 0, sizeof(*pInput));
}

static int __xgeShapeExStrokeInputPointReserve(
	xge_shape_ex_stroke_input_t* pInput,
	int iNeeded
)
{
	xge_vec2_t* pPoints;
	uint8_t* pTypes;
	int iCapacity;

	if ( iNeeded <= pInput->iPointCapacity ) return XGE_OK;
	iCapacity = pInput->iPointCapacity > 0 ? pInput->iPointCapacity : 32;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	if ( ((size_t)iCapacity > (SIZE_MAX / sizeof(*pPoints))) ||
	     ((size_t)iCapacity > (SIZE_MAX / sizeof(*pTypes))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pPoints = (xge_vec2_t*)xrtMalloc((size_t)iCapacity * sizeof(*pPoints));
	pTypes = (uint8_t*)xrtMalloc((size_t)iCapacity * sizeof(*pTypes));
	if ( (pPoints == NULL) || (pTypes == NULL) ) {
		xrtFree(pPoints);
		xrtFree(pTypes);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pInput->iPointCount > 0 ) {
		memcpy(pPoints, pInput->pPoints, (size_t)pInput->iPointCount * sizeof(*pPoints));
		memcpy(pTypes, pInput->pTypes, (size_t)pInput->iPointCount * sizeof(*pTypes));
	}
	xrtFree(pInput->pPoints);
	xrtFree(pInput->pTypes);
	pInput->pPoints = pPoints;
	pInput->pTypes = pTypes;
	pInput->iPointCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExStrokeInputContourReserve(
	xge_shape_ex_stroke_input_t* pInput,
	int iNeeded
)
{
	int* pEnds;
	uint8_t* pClosed;
	int iCapacity;

	if ( iNeeded <= pInput->iContourCapacity ) return XGE_OK;
	iCapacity = pInput->iContourCapacity > 0 ? pInput->iContourCapacity : 8;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	if ( ((size_t)iCapacity > (SIZE_MAX / sizeof(*pEnds))) ||
	     ((size_t)iCapacity > (SIZE_MAX / sizeof(*pClosed))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pEnds = (int*)xrtMalloc((size_t)iCapacity * sizeof(*pEnds));
	pClosed = (uint8_t*)xrtMalloc((size_t)iCapacity * sizeof(*pClosed));
	if ( (pEnds == NULL) || (pClosed == NULL) ) {
		xrtFree(pEnds);
		xrtFree(pClosed);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pInput->iContourCount > 0 ) {
		memcpy(pEnds, pInput->pContourEnds, (size_t)pInput->iContourCount * sizeof(*pEnds));
		memcpy(pClosed, pInput->pClosed, (size_t)pInput->iContourCount * sizeof(*pClosed));
	}
	xrtFree(pInput->pContourEnds);
	xrtFree(pInput->pClosed);
	pInput->pContourEnds = pEnds;
	pInput->pClosed = pClosed;
	pInput->iContourCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExStrokeInputPointAppend(
	xge_shape_ex_stroke_input_t* pInput,
	xge_vec2_t tPoint,
	uint8_t iType
)
{
	int iRet = __xgeShapeExStrokeInputPointReserve(pInput, pInput->iPointCount + 1);

	if ( iRet != XGE_OK ) return iRet;
	pInput->pPoints[pInput->iPointCount] = tPoint;
	pInput->pTypes[pInput->iPointCount] = iType;
	pInput->iPointCount++;
	return XGE_OK;
}

static int __xgeShapeExStrokeInputContourEnd(
	xge_shape_ex_stroke_input_t* pInput,
	int bClosed
)
{
	int iRet;

	if ( pInput->iPointCount <= 0 ) return XGE_OK;
	if ( (pInput->iContourCount > 0) &&
	     (pInput->pContourEnds[pInput->iContourCount - 1] == (pInput->iPointCount - 1)) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeExStrokeInputContourReserve(pInput, pInput->iContourCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pInput->pContourEnds[pInput->iContourCount] = pInput->iPointCount - 1;
	pInput->pClosed[pInput->iContourCount] = (uint8_t)(bClosed != 0);
	pInput->iContourCount++;
	return XGE_OK;
}

static int __xgeShapeExStrokeInputBuild(
	xge_shape_ex_stroke_input_t* pInput,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount,
	xge_shape_ex_matrix_t tMatrix
)
{
	xge_vec2_t tCurrent = {0.0f, 0.0f};
	xge_vec2_t tStart = {0.0f, 0.0f};
	int iPointRead = 0;
	int bHasCurrent = 0;
	int bContourActive = 0;
	int i;
	int iRet = XGE_OK;

	if ( (pInput == NULL) || (pCommands == NULL) || (iCommandCount < 0) ||
	     (pPoints == NULL) || (iPointCount < 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(pInput, 0, sizeof(*pInput));
	for ( i = 0; (iRet == XGE_OK) && (i < iCommandCount); i++ ) {
		uint8_t iCommand = pCommands[i];

		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			if ( bContourActive ) {
				iRet = __xgeShapeExStrokeInputContourEnd(pInput, 0);
				if ( iRet != XGE_OK ) break;
			}
			tCurrent = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			tStart = tCurrent;
			iRet = __xgeShapeExStrokeInputPointAppend(
				pInput, tCurrent, XGE_SHAPE_EX_STROKE_TAG_POINT
			);
			bHasCurrent = 1;
			bContourActive = iRet == XGE_OK;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			xge_vec2_t tTo;

			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tTo = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			if ( !bContourActive ) {
				xge_vec2_t tBegin = bHasCurrent ? tCurrent : tTo;

				tStart = tBegin;
				iRet = __xgeShapeExStrokeInputPointAppend(
					pInput, tBegin, XGE_SHAPE_EX_STROKE_TAG_POINT
				);
				bContourActive = iRet == XGE_OK;
			}
			if ( (iRet == XGE_OK) && (!__xgeShapeExStrokePointEqual(tTo, tCurrent) || bHasCurrent) ) {
				iRet = __xgeShapeExStrokeInputPointAppend(
					pInput, tTo, XGE_SHAPE_EX_STROKE_TAG_POINT
				);
			}
			tCurrent = tTo;
			bHasCurrent = 1;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_QUAD_TO ) {
			xge_vec2_t tControl;
			xge_vec2_t tTo;
			xge_vec2_t tC1;
			xge_vec2_t tC2;

			if ( (iPointRead + 1) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tControl = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			tTo = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			if ( !bHasCurrent ) {
				tCurrent = tTo;
				bHasCurrent = 1;
				continue;
			}
			if ( !bContourActive ) {
				tStart = tCurrent;
				iRet = __xgeShapeExStrokeInputPointAppend(
					pInput, tCurrent, XGE_SHAPE_EX_STROKE_TAG_POINT
				);
				bContourActive = iRet == XGE_OK;
			}
			tC1 = __xgeShapeExStrokePointAdd(
				tCurrent,
				__xgeShapeExStrokePointScale(
					__xgeShapeExStrokePointSub(tControl, tCurrent), 2.0f / 3.0f
				)
			);
			tC2 = __xgeShapeExStrokePointAdd(
				tTo,
				__xgeShapeExStrokePointScale(
					__xgeShapeExStrokePointSub(tControl, tTo), 2.0f / 3.0f
				)
			);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tC1, XGE_SHAPE_EX_STROKE_TAG_CUBIC);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tC2, XGE_SHAPE_EX_STROKE_TAG_CUBIC);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tTo, XGE_SHAPE_EX_STROKE_TAG_POINT);
			tCurrent = tTo;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			xge_vec2_t tC1;
			xge_vec2_t tC2;
			xge_vec2_t tTo;

			if ( (iPointRead + 2) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tC1 = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			tC2 = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			tTo = __xgeShapeExStrokePointTransform(tMatrix, pPoints[iPointRead++]);
			if ( !bHasCurrent ) {
				tCurrent = tTo;
				bHasCurrent = 1;
				continue;
			}
			if ( !bContourActive ) {
				tStart = tCurrent;
				iRet = __xgeShapeExStrokeInputPointAppend(
					pInput, tCurrent, XGE_SHAPE_EX_STROKE_TAG_POINT
				);
				bContourActive = iRet == XGE_OK;
			}
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tC1, XGE_SHAPE_EX_STROKE_TAG_CUBIC);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tC2, XGE_SHAPE_EX_STROKE_TAG_CUBIC);
			if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputPointAppend(pInput, tTo, XGE_SHAPE_EX_STROKE_TAG_POINT);
			tCurrent = tTo;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			if ( bContourActive ) {
				iRet = __xgeShapeExStrokeInputPointAppend(
					pInput, tStart, XGE_SHAPE_EX_STROKE_TAG_POINT
				);
				if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeInputContourEnd(pInput, 1);
				bContourActive = 0;
				tCurrent = tStart;
				bHasCurrent = 1;
			}
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	if ( (iRet == XGE_OK) && bContourActive ) {
		iRet = __xgeShapeExStrokeInputContourEnd(pInput, 0);
	}
	if ( (iRet == XGE_OK) && (iPointRead != iPointCount) ) {
		iRet = XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet != XGE_OK ) __xgeShapeExStrokeInputFree(pInput);
	return iRet;
}

static void __xgeShapeExStrokeBorderFree(xge_shape_ex_stroke_border_t* pBorder)
{
	if ( pBorder == NULL ) return;
	xrtFree(pBorder->pPoints);
	xrtFree(pBorder->pTags);
	memset(pBorder, 0, sizeof(*pBorder));
}

static int __xgeShapeExStrokeBorderReserve(
	xge_shape_ex_stroke_border_t* pBorder,
	int iAdded
)
{
	xge_vec2_t* pPoints;
	uint8_t* pTags;
	int iNeeded;
	int iCapacity;

	if ( (pBorder == NULL) || (iAdded < 0) || (pBorder->iCount > (INT_MAX - iAdded)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iNeeded = pBorder->iCount + iAdded;
	if ( iNeeded <= pBorder->iCapacity ) return XGE_OK;
	iCapacity = pBorder->iCapacity > 0 ? pBorder->iCapacity : 32;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	if ( ((size_t)iCapacity > (SIZE_MAX / sizeof(*pPoints))) ||
	     ((size_t)iCapacity > (SIZE_MAX / sizeof(*pTags))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pPoints = (xge_vec2_t*)xrtMalloc((size_t)iCapacity * sizeof(*pPoints));
	pTags = (uint8_t*)xrtMalloc((size_t)iCapacity * sizeof(*pTags));
	if ( (pPoints == NULL) || (pTags == NULL) ) {
		xrtFree(pPoints);
		xrtFree(pTags);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pBorder->iCount > 0 ) {
		memcpy(pPoints, pBorder->pPoints, (size_t)pBorder->iCount * sizeof(*pPoints));
		memcpy(pTags, pBorder->pTags, (size_t)pBorder->iCount * sizeof(*pTags));
	}
	xrtFree(pBorder->pPoints);
	xrtFree(pBorder->pTags);
	pBorder->pPoints = pPoints;
	pBorder->pTags = pTags;
	pBorder->iCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExStrokeBorderLineTo(
	xge_shape_ex_stroke_border_t* pBorder,
	xge_vec2_t tTo,
	int bMovable
)
{
	int iRet;

	if ( pBorder->bMovable && (pBorder->iCount > 0) ) {
		pBorder->pPoints[pBorder->iCount - 1] = tTo;
	} else {
		if ( (pBorder->iCount > 0) && __xgeShapeExStrokePointTiny(
			__xgeShapeExStrokePointSub(pBorder->pPoints[pBorder->iCount - 1], tTo)
		) ) return XGE_OK;
		iRet = __xgeShapeExStrokeBorderReserve(pBorder, 1);
		if ( iRet != XGE_OK ) return iRet;
		pBorder->pPoints[pBorder->iCount] = tTo;
		pBorder->pTags[pBorder->iCount] = XGE_SHAPE_EX_STROKE_TAG_POINT;
		pBorder->iCount++;
	}
	pBorder->bMovable = bMovable;
	return XGE_OK;
}

static int __xgeShapeExStrokeBorderClose(
	xge_shape_ex_stroke_border_t* pBorder,
	int bReverse
)
{
	int iStart;
	int iCount;

	if ( pBorder == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	iStart = pBorder->iStart;
	iCount = pBorder->iCount;
	if ( iStart < 0 ) return XGE_OK;
	if ( iCount <= (iStart + 1) ) {
		pBorder->iCount = iStart;
	} else {
		int iLeft;
		int iRight;

		iCount--;
		pBorder->iCount = iCount;
		pBorder->pPoints[iStart] = pBorder->pPoints[iCount];
		if ( bReverse ) {
			iLeft = iStart + 1;
			iRight = iCount - 1;
			while ( iLeft < iRight ) {
				xge_vec2_t tPoint = pBorder->pPoints[iLeft];
				uint8_t iTag = pBorder->pTags[iLeft];

				pBorder->pPoints[iLeft] = pBorder->pPoints[iRight];
				pBorder->pTags[iLeft] = pBorder->pTags[iRight];
				pBorder->pPoints[iRight] = tPoint;
				pBorder->pTags[iRight] = iTag;
				iLeft++;
				iRight--;
			}
		}
		pBorder->pTags[iStart] |= XGE_SHAPE_EX_STROKE_TAG_BEGIN;
		pBorder->pTags[iCount - 1] |= XGE_SHAPE_EX_STROKE_TAG_END;
	}
	pBorder->iStart = -1;
	pBorder->bMovable = 0;
	return XGE_OK;
}

static int __xgeShapeExStrokeBorderMoveTo(
	xge_shape_ex_stroke_border_t* pBorder,
	xge_vec2_t tTo
)
{
	int iRet;

	if ( pBorder->iStart >= 0 ) {
		iRet = __xgeShapeExStrokeBorderClose(pBorder, 0);
		if ( iRet != XGE_OK ) return iRet;
	}
	pBorder->iStart = pBorder->iCount;
	pBorder->bMovable = 0;
	return __xgeShapeExStrokeBorderLineTo(pBorder, tTo, 0);
}

static int __xgeShapeExStrokeBorderCubicTo(
	xge_shape_ex_stroke_border_t* pBorder,
	xge_vec2_t tControl1,
	xge_vec2_t tControl2,
	xge_vec2_t tTo
)
{
	int iRet = __xgeShapeExStrokeBorderReserve(pBorder, 3);

	if ( iRet != XGE_OK ) return iRet;
	pBorder->pPoints[pBorder->iCount + 0] = tControl1;
	pBorder->pPoints[pBorder->iCount + 1] = tControl2;
	pBorder->pPoints[pBorder->iCount + 2] = tTo;
	pBorder->pTags[pBorder->iCount + 0] = XGE_SHAPE_EX_STROKE_TAG_CUBIC;
	pBorder->pTags[pBorder->iCount + 1] = XGE_SHAPE_EX_STROKE_TAG_CUBIC;
	pBorder->pTags[pBorder->iCount + 2] = XGE_SHAPE_EX_STROKE_TAG_POINT;
	pBorder->iCount += 3;
	pBorder->bMovable = 0;
	return XGE_OK;
}

static int __xgeShapeExStrokeBorderArcTo(xge_shape_ex_stroker_t* pStroke, int iSide)
{
	xge_shape_ex_stroke_border_t* pBorder = &pStroke->arrBorders[iSide];
	float fRotate = __xgeShapeExStrokeSideRotation(iSide);
	float fAngleStart = pStroke->fAngleIn + fRotate;
	float fAngleDiff = __xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut);
	xge_vec2_t tA;
	float fTotal;
	float fAngle;

	if ( fabsf(fAngleDiff - XGE_SHAPE_EX_STROKE_PI) <= XGE_SHAPE_EX_STROKE_EPSILON ) {
		fAngleDiff = -fRotate * 2.0f;
	}
	if ( fabsf(fAngleDiff + XGE_SHAPE_EX_STROKE_PI) <= XGE_SHAPE_EX_STROKE_EPSILON ) {
		fAngleDiff = -fRotate * 2.0f;
	}
	tA = __xgeShapeExStrokePointRotate((xge_vec2_t){pStroke->fWidth, 0.0f}, fAngleStart);
	tA = __xgeShapeExStrokePointAdd(tA, pStroke->tCenter);
	fTotal = fAngleDiff;
	fAngle = fAngleStart;
	fRotate = fAngleDiff >= 0.0f ? XGE_SHAPE_EX_STROKE_PI2 : -XGE_SHAPE_EX_STROKE_PI2;
	while ( !__xgeShapeExStrokeFloatZero(fTotal) ) {
		float fStep = fTotal;
		float fNext;
		float fTheta;
		float fLength;
		xge_vec2_t tB;
		xge_vec2_t tA2;
		xge_vec2_t tB2;
		int iRet;

		if ( fStep > XGE_SHAPE_EX_STROKE_PI2 ) fStep = XGE_SHAPE_EX_STROKE_PI2;
		else if ( fStep < -XGE_SHAPE_EX_STROKE_PI2 ) fStep = -XGE_SHAPE_EX_STROKE_PI2;
		fNext = fAngle + fStep;
		fTheta = fabsf(fStep) * 0.5f;
		tB = __xgeShapeExStrokePointRotate((xge_vec2_t){pStroke->fWidth, 0.0f}, fNext);
		tB = __xgeShapeExStrokePointAdd(tB, pStroke->tCenter);
		fLength = pStroke->fWidth * (4.0f / 3.0f) * tanf(fTheta * 0.5f);
		tA2 = __xgeShapeExStrokePointRotate((xge_vec2_t){fLength, 0.0f}, fAngle + fRotate);
		tA2 = __xgeShapeExStrokePointAdd(tA2, tA);
		tB2 = __xgeShapeExStrokePointRotate((xge_vec2_t){fLength, 0.0f}, fNext - fRotate);
		tB2 = __xgeShapeExStrokePointAdd(tB2, tB);
		iRet = __xgeShapeExStrokeBorderCubicTo(pBorder, tA2, tB2, tB);
		if ( iRet != XGE_OK ) return iRet;
		tA = tB;
		fTotal -= fStep;
		fAngle = fNext;
	}
	pBorder->bMovable = 0;
	return XGE_OK;
}

static int __xgeShapeExStrokeOutside(
	xge_shape_ex_stroker_t* pStroke,
	int iSide,
	float fLength
)
{
	xge_shape_ex_stroke_border_t* pBorder = &pStroke->arrBorders[iSide];
	float fRotate;
	xge_vec2_t tDelta;

	if ( pStroke->iJoin == XGE_SHAPE_EX_JOIN_ROUND ) {
		return __xgeShapeExStrokeBorderArcTo(pStroke, iSide);
	}
	fRotate = __xgeShapeExStrokeSideRotation(iSide);
	{
		int bBevel = pStroke->iJoin == XGE_SHAPE_EX_JOIN_BEVEL;
		float fPhi = 0.0f;
		float fThetaCos = 0.0f;

		if ( !bBevel ) {
			float fTheta = __xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut);

			if ( fabsf(fTheta - XGE_SHAPE_EX_STROKE_PI) <= XGE_SHAPE_EX_STROKE_EPSILON ||
			     fabsf(fTheta + XGE_SHAPE_EX_STROKE_PI) <= XGE_SHAPE_EX_STROKE_EPSILON ) {
				fTheta = fRotate;
				fPhi = pStroke->fAngleIn;
			} else {
				fTheta *= 0.5f;
				fPhi = pStroke->fAngleIn + fTheta + fRotate;
			}
			fThetaCos = cosf(fTheta);
			if ( pStroke->fMiterLimit * fThetaCos < 1.0f ) bBevel = 1;
		}
		if ( bBevel ) {
			tDelta = __xgeShapeExStrokePointRotate(
				(xge_vec2_t){pStroke->fWidth, 0.0f}, pStroke->fAngleOut + fRotate
			);
			tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
			pBorder->bMovable = 0;
			return __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
		}
		tDelta = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth / fThetaCos, 0.0f}, fPhi
		);
		tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
		if ( __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0) != XGE_OK ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( __xgeShapeExStrokeFloatZero(fLength) ) {
			tDelta = __xgeShapeExStrokePointRotate(
				(xge_vec2_t){pStroke->fWidth, 0.0f}, pStroke->fAngleOut + fRotate
			);
			tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
			return __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
		}
	}
	return XGE_OK;
}

static int __xgeShapeExStrokeInside(
	xge_shape_ex_stroker_t* pStroke,
	int iSide,
	float fLength
)
{
	xge_shape_ex_stroke_border_t* pBorder = &pStroke->arrBorders[iSide];
	float fTheta = __xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut) * 0.5f;
	int bIntersect = 0;
	float fRotate = __xgeShapeExStrokeSideRotation(iSide);
	xge_vec2_t tDelta;

	if ( pBorder->bMovable && (fLength > 0.0f) ) {
		float fMinLength = fabsf(pStroke->fWidth * tanf(fTheta));

		if ( (pStroke->fLength >= fMinLength) && (fLength >= fMinLength) ) bIntersect = 1;
	}
	if ( !bIntersect ) {
		tDelta = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth, 0.0f}, pStroke->fAngleOut + fRotate
		);
		tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
		pBorder->bMovable = 0;
	} else {
		float fPhi = pStroke->fAngleIn + fTheta;
		float fThetaCos = cosf(fTheta);

		tDelta = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth / fThetaCos, 0.0f}, fPhi + fRotate
		);
		tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
	}
	return __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
}

static int __xgeShapeExStrokeProcessCorner(xge_shape_ex_stroker_t* pStroke, float fLength)
{
	float fTurn = __xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut);
	int iInside;
	int iRet;

	if ( __xgeShapeExStrokeFloatZero(fTurn) ) return XGE_OK;
	iInside = fTurn < 0.0f ? 1 : 0;
	iRet = __xgeShapeExStrokeInside(pStroke, iInside, fLength);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeShapeExStrokeOutside(pStroke, 1 - iInside, fLength);
}

static int __xgeShapeExStrokeFirstSubPath(
	xge_shape_ex_stroker_t* pStroke,
	float fStartAngle,
	float fLength
)
{
	xge_vec2_t tDelta = __xgeShapeExStrokePointRotate(
		(xge_vec2_t){pStroke->fWidth, 0.0f}, fStartAngle + XGE_SHAPE_EX_STROKE_PI2
	);
	int iRet;

	iRet = __xgeShapeExStrokeBorderMoveTo(
		&pStroke->arrBorders[0], __xgeShapeExStrokePointAdd(pStroke->tCenter, tDelta)
	);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeExStrokeBorderMoveTo(
		&pStroke->arrBorders[1], __xgeShapeExStrokePointSub(pStroke->tCenter, tDelta)
	);
	if ( iRet != XGE_OK ) return iRet;
	pStroke->fSubPathAngle = fStartAngle;
	pStroke->bFirstPoint = 0;
	pStroke->fSubPathLength = fLength;
	return XGE_OK;
}

static int __xgeShapeExStrokeLineTo(xge_shape_ex_stroker_t* pStroke, xge_vec2_t tTo)
{
	xge_vec2_t tDirection = __xgeShapeExStrokePointSub(tTo, pStroke->tCenter);
	float fLength;
	float fAngle;
	xge_vec2_t tDelta;
	int iSide;
	int iRet;

	if ( __xgeShapeExStrokeFloatZero(tDirection.fX) &&
	     __xgeShapeExStrokeFloatZero(tDirection.fY) ) {
		if ( pStroke->bFirstPoint && (pStroke->iCap != XGE_SHAPE_EX_CAP_BUTT) ) {
			return __xgeShapeExStrokeFirstSubPath(pStroke, 0.0f, 0.0f);
		}
		return XGE_OK;
	}
	fLength = __xgeShapeExStrokePointLength(tDirection);
	fAngle = __xgeShapeExStrokePointAngle(tDirection);
	tDelta = __xgeShapeExStrokePointRotate(
		(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngle + XGE_SHAPE_EX_STROKE_PI2
	);
	if ( pStroke->bFirstPoint ) {
		iRet = __xgeShapeExStrokeFirstSubPath(pStroke, fAngle, fLength);
	} else {
		pStroke->fAngleOut = fAngle;
		iRet = __xgeShapeExStrokeProcessCorner(pStroke, fLength);
	}
	if ( iRet != XGE_OK ) return iRet;
	for ( iSide = 0; iSide < 2; iSide++ ) {
		iRet = __xgeShapeExStrokeBorderLineTo(
			&pStroke->arrBorders[iSide], __xgeShapeExStrokePointAdd(tTo, tDelta), 1
		);
		if ( iRet != XGE_OK ) return iRet;
		tDelta.fX = -tDelta.fX;
		tDelta.fY = -tDelta.fY;
	}
	pStroke->fAngleIn = fAngle;
	pStroke->tCenter = tTo;
	pStroke->fLength = fLength;
	return XGE_OK;
}

static int __xgeShapeExStrokeCubicAngles(
	const xge_vec2_t* pBase,
	float* pAngleIn,
	float* pAngleMid,
	float* pAngleOut
)
{
	xge_vec2_t tD1 = __xgeShapeExStrokePointSub(pBase[2], pBase[3]);
	xge_vec2_t tD2 = __xgeShapeExStrokePointSub(pBase[1], pBase[2]);
	xge_vec2_t tD3 = __xgeShapeExStrokePointSub(pBase[0], pBase[1]);
	float fTheta1;
	float fTheta2;

	if ( __xgeShapeExStrokePointTiny(tD1) ) {
		if ( __xgeShapeExStrokePointTiny(tD2) ) {
			if ( __xgeShapeExStrokePointTiny(tD3) ) {
				*pAngleIn = *pAngleMid = *pAngleOut = 0.0f;
				return -1;
			}
			*pAngleIn = *pAngleMid = *pAngleOut = __xgeShapeExStrokePointAngle(tD3);
		} else if ( __xgeShapeExStrokePointTiny(tD3) ) {
			*pAngleIn = *pAngleMid = *pAngleOut = __xgeShapeExStrokePointAngle(tD2);
		} else {
			*pAngleIn = *pAngleMid = __xgeShapeExStrokePointAngle(tD2);
			*pAngleOut = __xgeShapeExStrokePointAngle(tD3);
		}
	} else if ( __xgeShapeExStrokePointTiny(tD2) ) {
		if ( __xgeShapeExStrokePointTiny(tD3) ) {
			*pAngleIn = *pAngleMid = *pAngleOut = __xgeShapeExStrokePointAngle(tD1);
		} else {
			*pAngleIn = __xgeShapeExStrokePointAngle(tD1);
			*pAngleOut = __xgeShapeExStrokePointAngle(tD3);
			*pAngleMid = __xgeShapeExStrokeAngleMean(*pAngleIn, *pAngleOut);
		}
	} else if ( __xgeShapeExStrokePointTiny(tD3) ) {
		*pAngleIn = __xgeShapeExStrokePointAngle(tD1);
		*pAngleMid = *pAngleOut = __xgeShapeExStrokePointAngle(tD2);
	} else {
		*pAngleIn = __xgeShapeExStrokePointAngle(tD1);
		*pAngleMid = __xgeShapeExStrokePointAngle(tD2);
		*pAngleOut = __xgeShapeExStrokePointAngle(tD3);
	}
	fTheta1 = fabsf(__xgeShapeExStrokeAngleDiff(*pAngleIn, *pAngleMid));
	fTheta2 = fabsf(__xgeShapeExStrokeAngleDiff(*pAngleMid, *pAngleOut));
	if ( (fTheta1 < (XGE_SHAPE_EX_STROKE_PI / 8.0f)) &&
	     (fTheta2 < (XGE_SHAPE_EX_STROKE_PI / 8.0f)) ) return 0;
	return 1;
}

static void __xgeShapeExStrokeSplitCubic(xge_vec2_t* pBase)
{
	xge_vec2_t tP01 = __xgeShapeExStrokePointMid(pBase[0], pBase[1]);
	xge_vec2_t tP12 = __xgeShapeExStrokePointMid(pBase[1], pBase[2]);
	xge_vec2_t tP23 = __xgeShapeExStrokePointMid(pBase[2], pBase[3]);
	xge_vec2_t tP012 = __xgeShapeExStrokePointMid(tP01, tP12);
	xge_vec2_t tP123 = __xgeShapeExStrokePointMid(tP12, tP23);
	xge_vec2_t tMid = __xgeShapeExStrokePointMid(tP012, tP123);

	pBase[6] = pBase[3];
	pBase[1] = tP01;
	pBase[2] = tP012;
	pBase[3] = tMid;
	pBase[4] = tP123;
	pBase[5] = tP23;
}

static int __xgeShapeExStrokeCubicTo(
	xge_shape_ex_stroker_t* pStroke,
	xge_vec2_t tControl1,
	xge_vec2_t tControl2,
	xge_vec2_t tTo
)
{
	xge_vec2_t arrBezier[37];
	int iArc = 0;
	int bFirstArc = 1;
	int iJoin = pStroke->iJoin;

	arrBezier[0] = tTo;
	arrBezier[1] = tControl2;
	arrBezier[2] = tControl1;
	arrBezier[3] = pStroke->tCenter;
	while ( iArc >= 0 ) {
		float fAngleIn = pStroke->fAngleIn;
		float fAngleOut = pStroke->fAngleIn;
		float fAngleMid = pStroke->fAngleIn;
		int iValid = __xgeShapeExStrokeCubicAngles(
			arrBezier + iArc, &fAngleIn, &fAngleMid, &fAngleOut
		);
		int iRet;

		if ( (iValid > 0) && (iArc < 32) ) {
			if ( pStroke->bFirstPoint ) pStroke->fAngleIn = fAngleIn;
			__xgeShapeExStrokeSplitCubic(arrBezier + iArc);
			iArc += 3;
			continue;
		}
		if ( (iValid < 0) && (iArc == 0) ) {
			pStroke->tCenter = tTo;
			if ( pStroke->bFirstPoint && (pStroke->iCap != XGE_SHAPE_EX_CAP_BUTT) ) {
				return __xgeShapeExStrokeFirstSubPath(pStroke, 0.0f, 0.0f);
			}
			return XGE_OK;
		}
		if ( bFirstArc ) {
			bFirstArc = 0;
			if ( pStroke->bFirstPoint ) {
				iRet = __xgeShapeExStrokeFirstSubPath(pStroke, fAngleIn, 0.0f);
			} else {
				pStroke->fAngleOut = fAngleIn;
				iRet = __xgeShapeExStrokeProcessCorner(pStroke, 0.0f);
			}
			if ( iRet != XGE_OK ) return iRet;
		} else if ( fabsf(__xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, fAngleIn)) >
		            ((XGE_SHAPE_EX_STROKE_PI / 8.0f) / 4.0f) ) {
			pStroke->tCenter = arrBezier[iArc + 3];
			pStroke->fAngleOut = fAngleIn;
			pStroke->iJoin = XGE_SHAPE_EX_JOIN_ROUND;
			iRet = __xgeShapeExStrokeProcessCorner(pStroke, 0.0f);
			pStroke->iJoin = iJoin;
			if ( iRet != XGE_OK ) return iRet;
		}
		{
			float fTheta1 = __xgeShapeExStrokeAngleDiff(fAngleIn, fAngleMid) * 0.5f;
			float fTheta2 = __xgeShapeExStrokeAngleDiff(fAngleMid, fAngleOut) * 0.5f;
			float fPhi1 = __xgeShapeExStrokeAngleMean(fAngleIn, fAngleMid);
			float fPhi2 = __xgeShapeExStrokeAngleMean(fAngleMid, fAngleOut);
			float fLength1 = pStroke->fWidth / cosf(fTheta1);
			float fLength2 = pStroke->fWidth / cosf(fTheta2);
			float fAlpha0 = 0.0f;
			int iSide;

			if ( pStroke->bHandleWideStrokes ) {
				fAlpha0 = __xgeShapeExStrokePointAngle(
					__xgeShapeExStrokePointSub(arrBezier[iArc + 0], arrBezier[iArc + 3])
				);
			}
			for ( iSide = 0; iSide < 2; iSide++ ) {
				xge_shape_ex_stroke_border_t* pBorder = &pStroke->arrBorders[iSide];
				float fRotate = __xgeShapeExStrokeSideRotation(iSide);
				xge_vec2_t tC1 = __xgeShapeExStrokePointRotate(
					(xge_vec2_t){fLength1, 0.0f}, fPhi1 + fRotate
				);
				xge_vec2_t tC2 = __xgeShapeExStrokePointRotate(
					(xge_vec2_t){fLength2, 0.0f}, fPhi2 + fRotate
				);
				xge_vec2_t tEnd = __xgeShapeExStrokePointRotate(
					(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngleOut + fRotate
				);

				tC1 = __xgeShapeExStrokePointAdd(tC1, arrBezier[iArc + 2]);
				tC2 = __xgeShapeExStrokePointAdd(tC2, arrBezier[iArc + 1]);
				tEnd = __xgeShapeExStrokePointAdd(tEnd, arrBezier[iArc + 0]);
				if ( pStroke->bHandleWideStrokes && (pBorder->iCount > 0) ) {
					xge_vec2_t tStart = pBorder->pPoints[pBorder->iCount - 1];
					float fAlpha1 = __xgeShapeExStrokePointAngle(
						__xgeShapeExStrokePointSub(tEnd, tStart)
					);

					if ( fabsf(__xgeShapeExStrokeAngleDiff(fAlpha0, fAlpha1)) > XGE_SHAPE_EX_STROKE_PI2 ) {
						float fBeta = __xgeShapeExStrokePointAngle(
							__xgeShapeExStrokePointSub(arrBezier[iArc + 3], tStart)
						);
						float fGamma = __xgeShapeExStrokePointAngle(
							__xgeShapeExStrokePointSub(arrBezier[iArc + 0], tEnd)
						);
						xge_vec2_t tBVector = __xgeShapeExStrokePointSub(tEnd, tStart);
						float fBLength = __xgeShapeExStrokePointLength(tBVector);
						float fSinA = fabsf(sinf(fAlpha1 - fGamma));
						float fSinB = fabsf(sinf(fBeta - fGamma));
						float fALength = fSinB > XGE_SHAPE_EX_STROKE_EPSILON ?
							(fBLength * fSinA) / fSinB : 0.0f;
						xge_vec2_t tDelta = __xgeShapeExStrokePointRotate(
							(xge_vec2_t){fALength, 0.0f}, fBeta
						);

						tDelta = __xgeShapeExStrokePointAdd(tDelta, tStart);
						pBorder->bMovable = 0;
						iRet = __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
						if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeBorderLineTo(pBorder, tEnd, 0);
						if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeBorderCubicTo(pBorder, tC2, tC1, tStart);
						if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeBorderLineTo(pBorder, tEnd, 0);
						if ( iRet != XGE_OK ) return iRet;
						continue;
					}
				}
				iRet = __xgeShapeExStrokeBorderCubicTo(pBorder, tC1, tC2, tEnd);
				if ( iRet != XGE_OK ) return iRet;
			}
		}
		iArc -= 3;
		pStroke->fAngleIn = fAngleOut;
	}
	pStroke->tCenter = tTo;
	return XGE_OK;
}

static int __xgeShapeExStrokeAddCap(
	xge_shape_ex_stroker_t* pStroke,
	float fAngle,
	int iSide
)
{
	float fRotate = __xgeShapeExStrokeSideRotation(iSide);
	xge_shape_ex_stroke_border_t* pBorder = &pStroke->arrBorders[iSide];
	xge_vec2_t tDelta = {pStroke->fWidth, 0.0f};
	int iRet;

	if ( pStroke->iCap == XGE_SHAPE_EX_CAP_SQUARE ) {
		xge_vec2_t tDelta2;

		tDelta = __xgeShapeExStrokePointRotate(tDelta, fAngle);
		tDelta2 = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngle + fRotate
		);
		tDelta = __xgeShapeExStrokePointAdd(
			__xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter), tDelta2
		);
		iRet = __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
		if ( iRet != XGE_OK ) return iRet;
		tDelta = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngle
		);
		tDelta2 = __xgeShapeExStrokePointRotate(
			(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngle - fRotate
		);
		tDelta = __xgeShapeExStrokePointAdd(
			__xgeShapeExStrokePointAdd(tDelta, tDelta2), pStroke->tCenter
		);
		return __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
	}
	if ( pStroke->iCap == XGE_SHAPE_EX_CAP_ROUND ) {
		pStroke->fAngleIn = fAngle;
		pStroke->fAngleOut = fAngle + XGE_SHAPE_EX_STROKE_PI;
		return __xgeShapeExStrokeBorderArcTo(pStroke, iSide);
	}
	tDelta = __xgeShapeExStrokePointRotate(tDelta, fAngle + fRotate);
	tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
	iRet = __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
	if ( iRet != XGE_OK ) return iRet;
	tDelta = __xgeShapeExStrokePointRotate(
		(xge_vec2_t){pStroke->fWidth, 0.0f}, fAngle - fRotate
	);
	tDelta = __xgeShapeExStrokePointAdd(tDelta, pStroke->tCenter);
	return __xgeShapeExStrokeBorderLineTo(pBorder, tDelta, 0);
}

static int __xgeShapeExStrokeReverseLeft(
	xge_shape_ex_stroker_t* pStroke,
	int bOpened
)
{
	xge_shape_ex_stroke_border_t* pRight = &pStroke->arrBorders[0];
	xge_shape_ex_stroke_border_t* pLeft = &pStroke->arrBorders[1];
	int iNewPoints = pLeft->iCount - pLeft->iStart;
	int i;
	int iRet;

	if ( iNewPoints <= 0 ) return XGE_OK;
	iRet = __xgeShapeExStrokeBorderReserve(pRight, iNewPoints);
	if ( iRet != XGE_OK ) return iRet;
	for ( i = pLeft->iCount - 1; i >= pLeft->iStart; i-- ) {
		uint8_t iTag = pLeft->pTags[i];

		if ( bOpened ) {
			iTag &= (uint8_t)~(XGE_SHAPE_EX_STROKE_TAG_BEGIN | XGE_SHAPE_EX_STROKE_TAG_END);
		} else {
			uint8_t iEndTags = iTag & (XGE_SHAPE_EX_STROKE_TAG_BEGIN | XGE_SHAPE_EX_STROKE_TAG_END);

			if ( (iEndTags == XGE_SHAPE_EX_STROKE_TAG_BEGIN) ||
			     (iEndTags == XGE_SHAPE_EX_STROKE_TAG_END) ) {
				iTag ^= XGE_SHAPE_EX_STROKE_TAG_BEGIN | XGE_SHAPE_EX_STROKE_TAG_END;
			}
		}
		pRight->pPoints[pRight->iCount] = pLeft->pPoints[i];
		pRight->pTags[pRight->iCount] = iTag;
		pRight->iCount++;
	}
	pLeft->iCount = pLeft->iStart;
	pRight->bMovable = 0;
	pLeft->bMovable = 0;
	return XGE_OK;
}

static void __xgeShapeExStrokeBeginSubPath(
	xge_shape_ex_stroker_t* pStroke,
	xge_vec2_t tTo,
	int bClosed
)
{
	pStroke->bFirstPoint = 1;
	pStroke->tCenter = tTo;
	pStroke->bClosedSubPath = bClosed;
	pStroke->bHandleWideStrokes =
		(pStroke->iJoin != XGE_SHAPE_EX_JOIN_ROUND) ||
		(!pStroke->bClosedSubPath && (pStroke->iCap == XGE_SHAPE_EX_CAP_BUTT));
	pStroke->tSubPathStart = tTo;
	pStroke->fAngleIn = 0.0f;
}

static int __xgeShapeExStrokeEndSubPath(xge_shape_ex_stroker_t* pStroke)
{
	int iRet;

	if ( pStroke->bClosedSubPath ) {
		if ( !__xgeShapeExStrokePointEqual(pStroke->tCenter, pStroke->tSubPathStart) ) {
			iRet = __xgeShapeExStrokeLineTo(pStroke, pStroke->tSubPathStart);
			if ( iRet != XGE_OK ) return iRet;
		}
		pStroke->fAngleOut = pStroke->fSubPathAngle;
		if ( !__xgeShapeExStrokeFloatZero(
			__xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut)
		) ) {
			int iInside = __xgeShapeExStrokeAngleDiff(pStroke->fAngleIn, pStroke->fAngleOut) < 0.0f ? 1 : 0;

			iRet = __xgeShapeExStrokeInside(pStroke, iInside, pStroke->fSubPathLength);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeShapeExStrokeOutside(pStroke, 1 - iInside, pStroke->fSubPathLength);
			if ( iRet != XGE_OK ) return iRet;
		}
		iRet = __xgeShapeExStrokeBorderClose(&pStroke->arrBorders[0], 0);
		if ( iRet != XGE_OK ) return iRet;
		return __xgeShapeExStrokeBorderClose(&pStroke->arrBorders[1], 1);
	}
	iRet = __xgeShapeExStrokeAddCap(pStroke, pStroke->fAngleIn, 0);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeExStrokeReverseLeft(pStroke, 1);
	if ( iRet != XGE_OK ) return iRet;
	pStroke->tCenter = pStroke->tSubPathStart;
	iRet = __xgeShapeExStrokeAddCap(
		pStroke, pStroke->fSubPathAngle + XGE_SHAPE_EX_STROKE_PI, 0
	);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeShapeExStrokeBorderClose(&pStroke->arrBorders[0], 0);
}

static int __xgeShapeExStrokeParseInput(
	xge_shape_ex_stroker_t* pStroke,
	const xge_shape_ex_stroke_input_t* pInput
)
{
	int iFirst = 0;
	int iContour;

	for ( iContour = 0; iContour < pInput->iContourCount; iContour++ ) {
		int iLast = pInput->pContourEnds[iContour];
		int iPoint;
		int iRet = XGE_OK;

		if ( iLast <= iFirst ) {
			iFirst = iLast + 1;
			continue;
		}
		if ( pInput->pTypes[iFirst] == XGE_SHAPE_EX_STROKE_TAG_CUBIC ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		__xgeShapeExStrokeBeginSubPath(
			pStroke, pInput->pPoints[iFirst], pInput->pClosed[iContour] != 0
		);
		iPoint = iFirst;
		while ( iPoint < iLast ) {
			if ( pInput->pTypes[iPoint + 1] == XGE_SHAPE_EX_STROKE_TAG_POINT ) {
				iPoint++;
				iRet = __xgeShapeExStrokeLineTo(pStroke, pInput->pPoints[iPoint]);
			} else if ( (iPoint + 3) <= iLast ) {
				iRet = __xgeShapeExStrokeCubicTo(
					pStroke, pInput->pPoints[iPoint + 1], pInput->pPoints[iPoint + 2],
					pInput->pPoints[iPoint + 3]
				);
				iPoint += 3;
			} else {
				return XGE_ERROR_INVALID_ARGUMENT;
			}
			if ( iRet != XGE_OK ) return iRet;
		}
		if ( !pStroke->bFirstPoint ) {
			iRet = __xgeShapeExStrokeEndSubPath(pStroke);
			if ( iRet != XGE_OK ) return iRet;
		}
		iFirst = iLast + 1;
	}
	return XGE_OK;
}

static void __xgeShapeExStrokePathFree(xge_shape_ex_stroke_path_t* pPath)
{
	if ( pPath == NULL ) return;
	xrtFree(pPath->pCommands);
	xrtFree(pPath->pPoints);
	memset(pPath, 0, sizeof(*pPath));
}

static int __xgeShapeExStrokePathReserve(
	xge_shape_ex_stroke_path_t* pPath,
	int iAddedCommands,
	int iAddedPoints
)
{
	int iCommandNeeded;
	int iPointNeeded;
	int iCommandCapacity;
	int iPointCapacity;
	uint8_t* pCommands;
	xge_vec2_t* pPoints;

	if ( (iAddedCommands < 0) || (iAddedPoints < 0) ||
	     (pPath->iCommandCount > (INT_MAX - iAddedCommands)) ||
	     (pPath->iPointCount > (INT_MAX - iAddedPoints)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iCommandNeeded = pPath->iCommandCount + iAddedCommands;
	iPointNeeded = pPath->iPointCount + iAddedPoints;
	iCommandCapacity = pPath->iCommandCapacity > 0 ? pPath->iCommandCapacity : 32;
	iPointCapacity = pPath->iPointCapacity > 0 ? pPath->iPointCapacity : 64;
	while ( iCommandCapacity < iCommandNeeded ) {
		if ( iCommandCapacity > (INT_MAX / 2) ) {
			iCommandCapacity = iCommandNeeded;
			break;
		}
		iCommandCapacity *= 2;
	}
	while ( iPointCapacity < iPointNeeded ) {
		if ( iPointCapacity > (INT_MAX / 2) ) {
			iPointCapacity = iPointNeeded;
			break;
		}
		iPointCapacity *= 2;
	}
	if ( (iCommandCapacity != pPath->iCommandCapacity) ) {
		if ( (size_t)iCommandCapacity > (SIZE_MAX / sizeof(*pCommands)) ) return XGE_ERROR_OUT_OF_MEMORY;
		pCommands = (uint8_t*)xrtRealloc(
			pPath->pCommands, (size_t)iCommandCapacity * sizeof(*pCommands)
		);
		if ( pCommands == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pPath->pCommands = pCommands;
		pPath->iCommandCapacity = iCommandCapacity;
	}
	if ( iPointCapacity != pPath->iPointCapacity ) {
		if ( (size_t)iPointCapacity > (SIZE_MAX / sizeof(*pPoints)) ) return XGE_ERROR_OUT_OF_MEMORY;
		pPoints = (xge_vec2_t*)xrtRealloc(
			pPath->pPoints, (size_t)iPointCapacity * sizeof(*pPoints)
		);
		if ( pPoints == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pPath->pPoints = pPoints;
		pPath->iPointCapacity = iPointCapacity;
	}
	return XGE_OK;
}

static int __xgeShapeExStrokePathAppend(
	xge_shape_ex_stroke_path_t* pPath,
	uint8_t iCommand,
	const xge_vec2_t* pPoints,
	int iPointCount
)
{
	int iRet = __xgeShapeExStrokePathReserve(pPath, 1, iPointCount);

	if ( iRet != XGE_OK ) return iRet;
	pPath->pCommands[pPath->iCommandCount++] = iCommand;
	if ( iPointCount > 0 ) {
		memcpy(
			pPath->pPoints + pPath->iPointCount, pPoints,
			(size_t)iPointCount * sizeof(*pPoints)
		);
		pPath->iPointCount += iPointCount;
	}
	return XGE_OK;
}

static void __xgeShapeExStrokeCubicSplit(
	const xge_shape_ex_stroke_cubic_t* pCubic,
	float fT,
	xge_shape_ex_stroke_cubic_t* pLeft,
	xge_shape_ex_stroke_cubic_t* pRight
)
{
	xge_vec2_t tA;
	xge_vec2_t tB;
	xge_vec2_t tC;
	xge_vec2_t tD;
	xge_vec2_t tE;
	xge_vec2_t tMid;

	tA = __xgeShapeExStrokePointAdd(
		pCubic->tStart,
		__xgeShapeExStrokePointScale(
			__xgeShapeExStrokePointSub(pCubic->tControl1, pCubic->tStart), fT
		)
	);
	tB = __xgeShapeExStrokePointAdd(
		pCubic->tControl1,
		__xgeShapeExStrokePointScale(
			__xgeShapeExStrokePointSub(pCubic->tControl2, pCubic->tControl1), fT
		)
	);
	tC = __xgeShapeExStrokePointAdd(
		pCubic->tControl2,
		__xgeShapeExStrokePointScale(
			__xgeShapeExStrokePointSub(pCubic->tEnd, pCubic->tControl2), fT
		)
	);
	tD = __xgeShapeExStrokePointAdd(
		tA, __xgeShapeExStrokePointScale(__xgeShapeExStrokePointSub(tB, tA), fT)
	);
	tE = __xgeShapeExStrokePointAdd(
		tB, __xgeShapeExStrokePointScale(__xgeShapeExStrokePointSub(tC, tB), fT)
	);
	tMid = __xgeShapeExStrokePointAdd(
		tD, __xgeShapeExStrokePointScale(__xgeShapeExStrokePointSub(tE, tD), fT)
	);
	pLeft->tStart = pCubic->tStart;
	pLeft->tControl1 = tA;
	pLeft->tControl2 = tD;
	pLeft->tEnd = tMid;
	pRight->tStart = tMid;
	pRight->tControl1 = tE;
	pRight->tControl2 = tC;
	pRight->tEnd = pCubic->tEnd;
}

static float __xgeShapeExStrokePointDistance(xge_vec2_t tA, xge_vec2_t tB)
{
	return __xgeShapeExStrokePointLength(__xgeShapeExStrokePointSub(tB, tA));
}

static float __xgeShapeExStrokeCubicLengthDepth(
	const xge_shape_ex_stroke_cubic_t* pCubic,
	int iDepth
)
{
	float fPolygon;
	float fChord;

	fPolygon = __xgeShapeExStrokePointDistance(pCubic->tStart, pCubic->tControl1) +
	           __xgeShapeExStrokePointDistance(pCubic->tControl1, pCubic->tControl2) +
	           __xgeShapeExStrokePointDistance(pCubic->tControl2, pCubic->tEnd);
	fChord = __xgeShapeExStrokePointDistance(pCubic->tStart, pCubic->tEnd);
	if ( (fabsf(fPolygon - fChord) > XGE_SHAPE_EX_STROKE_BEZIER_EPSILON) &&
	     (iDepth < 32) ) {
		xge_shape_ex_stroke_cubic_t tLeft;
		xge_shape_ex_stroke_cubic_t tRight;

		__xgeShapeExStrokeCubicSplit(pCubic, 0.5f, &tLeft, &tRight);
		return __xgeShapeExStrokeCubicLengthDepth(&tLeft, iDepth + 1) +
		       __xgeShapeExStrokeCubicLengthDepth(&tRight, iDepth + 1);
	}
	return fPolygon;
}

static float __xgeShapeExStrokeCubicLength(const xge_shape_ex_stroke_cubic_t* pCubic)
{
	return __xgeShapeExStrokeCubicLengthDepth(pCubic, 0);
}

static float __xgeShapeExStrokeCubicParameterAt(
	const xge_shape_ex_stroke_cubic_t* pCubic,
	float fAt,
	float fLength
)
{
	float fLargest = 1.0f;
	float fSmallest = 0.0f;
	float fT = 0.5f;

	if ( fAt <= 0.0f ) return 0.0f;
	if ( fAt >= fLength ) return 1.0f;
	for (;;) {
		xge_shape_ex_stroke_cubic_t tLeft;
		xge_shape_ex_stroke_cubic_t tRight;
		float fLeftLength;

		__xgeShapeExStrokeCubicSplit(pCubic, fT, &tLeft, &tRight);
		fLeftLength = __xgeShapeExStrokeCubicLength(&tLeft);
		if ( (fabsf(fLeftLength - fAt) < XGE_SHAPE_EX_STROKE_BEZIER_EPSILON) ||
		     (fabsf(fSmallest - fLargest) < 0.001f) ) {
			break;
		}
		if ( fLeftLength < fAt ) {
			fSmallest = fT;
			fT = (fT + fLargest) * 0.5f;
		} else {
			fLargest = fT;
			fT = (fSmallest + fT) * 0.5f;
		}
	}
	return fT;
}

static void __xgeShapeExStrokeCubicSplitAtLength(
	const xge_shape_ex_stroke_cubic_t* pCubic,
	float fAt,
	float fLength,
	xge_shape_ex_stroke_cubic_t* pLeft,
	xge_shape_ex_stroke_cubic_t* pRight
)
{
	float fT = __xgeShapeExStrokeCubicParameterAt(pCubic, fAt, fLength);

	__xgeShapeExStrokeCubicSplit(pCubic, fT, pLeft, pRight);
}

static void __xgeShapeExStrokeTrimInputFree(
	xge_shape_ex_stroke_trim_input_t* pInput
)
{
	if ( pInput == NULL ) return;
	xrtFree(pInput->pSegments);
	xrtFree(pInput->pContours);
	memset(pInput, 0, sizeof(*pInput));
}

static int __xgeShapeExStrokeTrimSegmentReserve(
	xge_shape_ex_stroke_trim_input_t* pInput,
	int iNeeded
)
{
	xge_shape_ex_stroke_trim_segment_t* pSegments;
	int iCapacity;

	if ( iNeeded <= pInput->iSegmentCapacity ) return XGE_OK;
	iCapacity = pInput->iSegmentCapacity > 0 ? pInput->iSegmentCapacity : 32;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > (SIZE_MAX / sizeof(*pSegments)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSegments = (xge_shape_ex_stroke_trim_segment_t*)xrtRealloc(
		pInput->pSegments, (size_t)iCapacity * sizeof(*pSegments)
	);
	if ( pSegments == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pInput->pSegments = pSegments;
	pInput->iSegmentCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExStrokeTrimContourReserve(
	xge_shape_ex_stroke_trim_input_t* pInput,
	int iNeeded
)
{
	xge_shape_ex_stroke_trim_contour_t* pContours;
	int iCapacity;

	if ( iNeeded <= pInput->iContourCapacity ) return XGE_OK;
	iCapacity = pInput->iContourCapacity > 0 ? pInput->iContourCapacity : 8;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > (SIZE_MAX / sizeof(*pContours)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pContours = (xge_shape_ex_stroke_trim_contour_t*)xrtRealloc(
		pInput->pContours, (size_t)iCapacity * sizeof(*pContours)
	);
	if ( pContours == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pInput->pContours = pContours;
	pInput->iContourCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExStrokeTrimContourBegin(
	xge_shape_ex_stroke_trim_input_t* pInput
)
{
	xge_shape_ex_stroke_trim_contour_t* pContour;
	int iRet;

	iRet = __xgeShapeExStrokeTrimContourReserve(pInput, pInput->iContourCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pContour = &pInput->pContours[pInput->iContourCount++];
	pContour->iSegmentStart = pInput->iSegmentCount;
	pContour->iSegmentCount = 0;
	pContour->fLength = 0.0f;
	pContour->bClosed = 0;
	return XGE_OK;
}

static int __xgeShapeExStrokeTrimSegmentAppend(
	xge_shape_ex_stroke_trim_input_t* pInput,
	uint8_t iCommand,
	const xge_shape_ex_stroke_cubic_t* pCubic
)
{
	xge_shape_ex_stroke_trim_segment_t* pSegment;
	xge_shape_ex_stroke_trim_contour_t* pContour;
	int iRet;

	if ( (pInput == NULL) || (pCubic == NULL) ||
	     ((iCommand != XGE_SHAPE_EX_CMD_LINE_TO) &&
	      (iCommand != XGE_SHAPE_EX_CMD_CUBIC_TO)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pInput->iContourCount <= 0 ) {
		iRet = __xgeShapeExStrokeTrimContourBegin(pInput);
		if ( iRet != XGE_OK ) return iRet;
	}
	iRet = __xgeShapeExStrokeTrimSegmentReserve(pInput, pInput->iSegmentCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pSegment = &pInput->pSegments[pInput->iSegmentCount++];
	pSegment->iCommand = iCommand;
	pSegment->tCubic = *pCubic;
	pSegment->fLength = iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ?
		__xgeShapeExStrokeCubicLength(pCubic) :
		__xgeShapeExStrokePointDistance(pCubic->tStart, pCubic->tEnd);
	pSegment->iContour = pInput->iContourCount - 1;
	pContour = &pInput->pContours[pSegment->iContour];
	pContour->iSegmentCount++;
	pContour->fLength += pSegment->fLength;
	return XGE_OK;
}

static int __xgeShapeExStrokeTrimInputBuild(
	xge_shape_ex_stroke_trim_input_t* pInput,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount
)
{
	xge_vec2_t tCurrent = {0.0f, 0.0f};
	xge_vec2_t tStart = {0.0f, 0.0f};
	int iPointRead = 0;
	int bHasCurrent = 0;
	int bContourActive = 0;
	int bStartNewContour = 0;
	int iRet = XGE_OK;
	int i;

	if ( (pInput == NULL) || (pCommands == NULL) || (iCommandCount < 0) ||
	     (pPoints == NULL) || (iPointCount < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pInput, 0, sizeof(*pInput));
	for ( i = 0; (iRet == XGE_OK) && (i < iCommandCount); i++ ) {
		uint8_t iCommand = pCommands[i];
		xge_shape_ex_stroke_cubic_t tSegment;

		memset(&tSegment, 0, sizeof(tSegment));
		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tCurrent = pPoints[iPointRead++];
			tStart = tCurrent;
			bHasCurrent = 1;
			bContourActive = 0;
			bStartNewContour = 0;
			continue;
		}
		if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			if ( bHasCurrent ) {
				if ( !bContourActive ) {
					iRet = __xgeShapeExStrokeTrimContourBegin(pInput);
					if ( iRet != XGE_OK ) break;
					bContourActive = 1;
				}
				tSegment.tStart = tCurrent;
				tSegment.tEnd = tStart;
				iRet = __xgeShapeExStrokeTrimSegmentAppend(
					pInput, XGE_SHAPE_EX_CMD_LINE_TO, &tSegment
				);
				if ( iRet != XGE_OK ) break;
				pInput->pContours[pInput->iContourCount - 1].bClosed = 1;
				tCurrent = tStart;
				bContourActive = 0;
				bStartNewContour = 1;
			}
			continue;
		}
		if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tSegment.tEnd = pPoints[iPointRead++];
		} else if ( iCommand == XGE_SHAPE_EX_CMD_QUAD_TO ) {
			xge_vec2_t tControl;

			if ( (iPointRead + 1) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tControl = pPoints[iPointRead++];
			tSegment.tEnd = pPoints[iPointRead++];
			if ( bHasCurrent ) {
				tSegment.tControl1 = __xgeShapeExStrokePointAdd(
					tCurrent,
					__xgeShapeExStrokePointScale(
						__xgeShapeExStrokePointSub(tControl, tCurrent), 2.0f / 3.0f
					)
				);
				tSegment.tControl2 = __xgeShapeExStrokePointAdd(
					tSegment.tEnd,
					__xgeShapeExStrokePointScale(
						__xgeShapeExStrokePointSub(tControl, tSegment.tEnd), 2.0f / 3.0f
					)
				);
			}
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			if ( (iPointRead + 2) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tSegment.tControl1 = pPoints[iPointRead++];
			tSegment.tControl2 = pPoints[iPointRead++];
			tSegment.tEnd = pPoints[iPointRead++];
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
			break;
		}
		if ( !bHasCurrent ) {
			tCurrent = tStart = tSegment.tEnd;
			bHasCurrent = 1;
			bStartNewContour = 0;
			continue;
		}
		if ( bStartNewContour ) {
			tStart = tCurrent;
			bStartNewContour = 0;
		}
		if ( !bContourActive ) {
			iRet = __xgeShapeExStrokeTrimContourBegin(pInput);
			if ( iRet != XGE_OK ) break;
			bContourActive = 1;
		}
		tSegment.tStart = tCurrent;
		iRet = __xgeShapeExStrokeTrimSegmentAppend(
			pInput,
			iCommand == XGE_SHAPE_EX_CMD_LINE_TO ?
				XGE_SHAPE_EX_CMD_LINE_TO : XGE_SHAPE_EX_CMD_CUBIC_TO,
			&tSegment
		);
		if ( iRet == XGE_OK ) tCurrent = tSegment.tEnd;
	}
	if ( (iRet == XGE_OK) && (iPointRead != iPointCount) ) {
		iRet = XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet != XGE_OK ) __xgeShapeExStrokeTrimInputFree(pInput);
	return iRet;
}

static xge_vec2_t __xgeShapeExStrokeTrimLinePoint(
	xge_vec2_t tStart,
	xge_vec2_t tEnd,
	float fT
)
{
	return __xgeShapeExStrokePointAdd(
		tStart,
		__xgeShapeExStrokePointScale(
			__xgeShapeExStrokePointSub(tEnd, tStart), fT
		)
	);
}

static int __xgeShapeExStrokeTrimSegmentEmit(
	xge_shape_ex_stroke_path_t* pPath,
	const xge_shape_ex_stroke_trim_segment_t* pSegment,
	float fAt,
	float fSpan,
	int bStart
)
{
	xge_shape_ex_stroke_cubic_t tSelected;
	xge_vec2_t arrPoints[3];
	int iRet;

	if ( (pPath == NULL) || (pSegment == NULL) ||
	     (fSpan <= XGE_SHAPE_EX_STROKE_EPSILON) ||
	     (pSegment->fLength <= XGE_SHAPE_EX_STROKE_EPSILON) ) {
		return XGE_OK;
	}
	if ( fAt < 0.0f ) fAt = 0.0f;
	if ( fAt > pSegment->fLength ) fAt = pSegment->fLength;
	if ( fSpan > (pSegment->fLength - fAt) ) {
		fSpan = pSegment->fLength - fAt;
	}
	if ( fSpan <= XGE_SHAPE_EX_STROKE_EPSILON ) return XGE_OK;
	if ( pSegment->iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
		xge_vec2_t tStart = __xgeShapeExStrokeTrimLinePoint(
			pSegment->tCubic.tStart, pSegment->tCubic.tEnd,
			fAt / pSegment->fLength
		);
		xge_vec2_t tEnd = __xgeShapeExStrokeTrimLinePoint(
			pSegment->tCubic.tStart, pSegment->tCubic.tEnd,
			(fAt + fSpan) / pSegment->fLength
		);

		if ( bStart || (pPath->iCommandCount <= 0) ) {
			iRet = __xgeShapeExStrokePathAppend(
				pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &tStart, 1
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return __xgeShapeExStrokePathAppend(
			pPath, XGE_SHAPE_EX_CMD_LINE_TO, &tEnd, 1
		);
	}
	if ( pSegment->iCommand != XGE_SHAPE_EX_CMD_CUBIC_TO ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tSelected = pSegment->tCubic;
	if ( fAt > XGE_SHAPE_EX_STROKE_EPSILON ) {
		xge_shape_ex_stroke_cubic_t tLeft;
		xge_shape_ex_stroke_cubic_t tRight;

		__xgeShapeExStrokeCubicSplitAtLength(
			&tSelected, fAt, pSegment->fLength, &tLeft, &tRight
		);
		tSelected = tRight;
	}
	{
		float fSelectedLength = __xgeShapeExStrokeCubicLength(&tSelected);

		if ( fSpan < (fSelectedLength - XGE_SHAPE_EX_STROKE_EPSILON) ) {
			xge_shape_ex_stroke_cubic_t tLeft;
			xge_shape_ex_stroke_cubic_t tRight;

			__xgeShapeExStrokeCubicSplitAtLength(
				&tSelected, fSpan, fSelectedLength, &tLeft, &tRight
			);
			tSelected = tLeft;
		}
	}
	if ( bStart || (pPath->iCommandCount <= 0) ) {
		iRet = __xgeShapeExStrokePathAppend(
			pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &tSelected.tStart, 1
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	arrPoints[0] = tSelected.tControl1;
	arrPoints[1] = tSelected.tControl2;
	arrPoints[2] = tSelected.tEnd;
	return __xgeShapeExStrokePathAppend(
		pPath, XGE_SHAPE_EX_CMD_CUBIC_TO, arrPoints, 3
	);
}

static int __xgeShapeExStrokeTrimRangeEmit(
	xge_shape_ex_stroke_path_t* pPath,
	const xge_shape_ex_stroke_trim_input_t* pInput,
	int iSegmentStart,
	int iSegmentCount,
	float fStart,
	float fEnd,
	int bConnect
)
{
	float fPosition = 0.0f;
	int iPreviousContour = -1;
	int bFirstOutput = 1;
	int i;

	if ( (pPath == NULL) || (pInput == NULL) ||
	     (iSegmentStart < 0) || (iSegmentCount < 0) ||
	     (iSegmentStart > pInput->iSegmentCount) ||
	     (iSegmentCount > (pInput->iSegmentCount - iSegmentStart)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fStart < 0.0f ) fStart = 0.0f;
	if ( fEnd <= fStart + XGE_SHAPE_EX_STROKE_EPSILON ) return XGE_OK;
	for ( i = 0; i < iSegmentCount; i++ ) {
		const xge_shape_ex_stroke_trim_segment_t* pSegment =
			&pInput->pSegments[iSegmentStart + i];
		float fSegmentEnd = fPosition + pSegment->fLength;
		float fOverlapStart;
		float fOverlapEnd;
		int bStart;
		int iRet;

		if ( pSegment->fLength <= XGE_SHAPE_EX_STROKE_EPSILON ) {
			fPosition = fSegmentEnd;
			continue;
		}
		if ( fSegmentEnd <= fStart + XGE_SHAPE_EX_STROKE_EPSILON ) {
			fPosition = fSegmentEnd;
			continue;
		}
		if ( fPosition >= fEnd - XGE_SHAPE_EX_STROKE_EPSILON ) break;
		fOverlapStart = fmaxf(fStart, fPosition);
		fOverlapEnd = fminf(fEnd, fSegmentEnd);
		if ( fOverlapEnd <= fOverlapStart + XGE_SHAPE_EX_STROKE_EPSILON ) {
			fPosition = fSegmentEnd;
			continue;
		}
		bStart = (pPath->iCommandCount <= 0) ||
		         (bFirstOutput ? !bConnect :
		          (pSegment->iContour != iPreviousContour));
		iRet = __xgeShapeExStrokeTrimSegmentEmit(
			pPath, pSegment, fOverlapStart - fPosition,
			fOverlapEnd - fOverlapStart, bStart
		);
		if ( iRet != XGE_OK ) return iRet;
		bFirstOutput = 0;
		iPreviousContour = pSegment->iContour;
		fPosition = fSegmentEnd;
	}
	return XGE_OK;
}

static void __xgeShapeExStrokeTrimNormalize(float* pBegin, float* pEnd)
{
	float fBegin = *pBegin;
	float fEnd = *pEnd;
	int bLoop = 1;

	if ( (fBegin > 1.0f) && (fEnd > 1.0f) ) bLoop = 0;
	if ( (fBegin < 0.0f) && (fEnd < 0.0f) ) bLoop = 0;
	if ( (fBegin >= 0.0f) && (fBegin <= 1.0f) &&
	     (fEnd >= 0.0f) && (fEnd <= 1.0f) ) bLoop = 0;
	if ( fBegin > 1.0f ) fBegin -= 1.0f;
	if ( fBegin < 0.0f ) fBegin += 1.0f;
	if ( fEnd > 1.0f ) fEnd -= 1.0f;
	if ( fEnd < 0.0f ) fEnd += 1.0f;
	if ( (bLoop && (fBegin < fEnd)) || (!bLoop && (fBegin > fEnd)) ) {
		float fSwap = fBegin;

		fBegin = fEnd;
		fEnd = fSwap;
	}
	*pBegin = fBegin;
	*pEnd = fEnd;
}

static int __xgeShapeExStrokeTrimContourEmit(
	xge_shape_ex_stroke_path_t* pPath,
	const xge_shape_ex_stroke_trim_input_t* pInput,
	const xge_shape_ex_stroke_trim_contour_t* pContour,
	float fBegin,
	float fEnd
)
{
	int iRet;

	if ( pContour->fLength <= XGE_SHAPE_EX_STROKE_EPSILON ) return XGE_OK;
	if ( fBegin >= fEnd ) {
		iRet = __xgeShapeExStrokeTrimRangeEmit(
			pPath, pInput, pContour->iSegmentStart, pContour->iSegmentCount,
			fBegin * pContour->fLength, pContour->fLength, 0
		);
		if ( iRet != XGE_OK ) return iRet;
		return __xgeShapeExStrokeTrimRangeEmit(
			pPath, pInput, pContour->iSegmentStart, pContour->iSegmentCount,
			0.0f, fEnd * pContour->fLength, pContour->bClosed
		);
	}
	return __xgeShapeExStrokeTrimRangeEmit(
		pPath, pInput, pContour->iSegmentStart, pContour->iSegmentCount,
		fBegin * pContour->fLength, fEnd * pContour->fLength, 0
	);
}

static int __xgeShapeExTrimPathBuild(
	xge_shape_ex_stroke_path_t* pPath,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount,
	float fBegin,
	float fEnd,
	int bSimultaneous
)
{
	xge_shape_ex_stroke_trim_input_t tInput;
	float fTotalLength = 0.0f;
	int iRet;
	int i;

	if ( (pPath == NULL) || (pCommands == NULL) || (iCommandCount < 0) ||
	     (pPoints == NULL) || (iPointCount < 0) ||
	     !isfinite(fBegin) || !isfinite(fEnd) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pPath, 0, sizeof(*pPath));
	memset(&tInput, 0, sizeof(tInput));
	iRet = __xgeShapeExStrokeTrimInputBuild(
		&tInput, pCommands, iCommandCount, pPoints, iPointCount
	);
	if ( iRet != XGE_OK ) return iRet;
	if ( fabsf(fBegin - fEnd) <= XGE_SHAPE_EX_STROKE_EPSILON ) {
		__xgeShapeExStrokeTrimInputFree(&tInput);
		return XGE_OK;
	}
	__xgeShapeExStrokeTrimNormalize(&fBegin, &fEnd);
	if ( bSimultaneous ) {
		for ( i = 0; (iRet == XGE_OK) && (i < tInput.iContourCount); i++ ) {
			iRet = __xgeShapeExStrokeTrimContourEmit(
				pPath, &tInput, &tInput.pContours[i], fBegin, fEnd
			);
		}
	} else {
		for ( i = 0; i < tInput.iContourCount; i++ ) {
			fTotalLength += tInput.pContours[i].fLength;
		}
		if ( fTotalLength > XGE_SHAPE_EX_STROKE_EPSILON ) {
			if ( fBegin >= fEnd ) {
				iRet = __xgeShapeExStrokeTrimRangeEmit(
					pPath, &tInput, 0, tInput.iSegmentCount,
					fBegin * fTotalLength, fTotalLength, 0
				);
				if ( iRet == XGE_OK ) {
					iRet = __xgeShapeExStrokeTrimRangeEmit(
						pPath, &tInput, 0, tInput.iSegmentCount,
						0.0f, fEnd * fTotalLength, 0
					);
				}
			} else {
				iRet = __xgeShapeExStrokeTrimRangeEmit(
					pPath, &tInput, 0, tInput.iSegmentCount,
					fBegin * fTotalLength, fEnd * fTotalLength, 0
				);
			}
		}
	}
	__xgeShapeExStrokeTrimInputFree(&tInput);
	if ( iRet != XGE_OK ) __xgeShapeExStrokePathFree(pPath);
	return iRet;
}

static float __xgeShapeExStrokeDashPatternAt(
	const xge_shape_ex_stroke_dash_t* pDash,
	int iIndex
)
{
	return pDash->pPattern[iIndex % pDash->iPatternCount];
}

static int __xgeShapeExStrokeDashInitialize(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_shape_ex_stroke_path_t* pPath,
	const float* pPattern,
	int iPatternCount,
	float fOffset,
	int bValidPoint
)
{
	float fTotal = 0.0f;
	int i;

	if ( (pDash == NULL) || (pPath == NULL) || (pPattern == NULL) ||
	     (iPatternCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( ((iPatternCount & 1) != 0) && (iPatternCount > (INT_MAX / 2)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pDash, 0, sizeof(*pDash));
	pDash->pPath = pPath;
	pDash->pPattern = pPattern;
	pDash->iPatternCount = iPatternCount;
	pDash->iLogicalCount = (iPatternCount & 1) != 0 ? iPatternCount * 2 : iPatternCount;
	pDash->bValidPoint = bValidPoint != 0;
	for ( i = 0; i < pDash->iLogicalCount; i++ ) {
		float fValue = pPattern[i % iPatternCount];

		if ( !isfinite(fValue) || (fValue < 0.0f) ) return XGE_ERROR_INVALID_ARGUMENT;
		fTotal += fValue;
	}
	if ( fTotal <= XGE_SHAPE_EX_STROKE_DASH_THRESHOLD ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pDash->fOffset = fmodf(fOffset, fTotal);
	if ( pDash->fOffset < 0.0f ) pDash->fOffset += fTotal;
	return XGE_OK;
}

static void __xgeShapeExStrokeDashMoveTo(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tPoint
)
{
	float fOffset = pDash->fOffset;
	int iIndex = 0;
	int i;

	for ( i = 0; i < pDash->iLogicalCount; i++, iIndex++ ) {
		float fPattern = __xgeShapeExStrokeDashPatternAt(pDash, iIndex);

		if ( fOffset < fPattern ) break;
		fOffset -= fPattern;
	}
	pDash->iCurrentIndex = iIndex % pDash->iLogicalCount;
	pDash->fCurrentLength =
		__xgeShapeExStrokeDashPatternAt(pDash, pDash->iCurrentIndex) - fOffset;
	pDash->bCurrentGap = (pDash->iCurrentIndex & 1) != 0;
	pDash->tStart = tPoint;
	pDash->tCurrent = tPoint;
	pDash->bMove = 1;
}

static void __xgeShapeExStrokeDashNext(xge_shape_ex_stroke_dash_t* pDash)
{
	pDash->iCurrentIndex = (pDash->iCurrentIndex + 1) % pDash->iLogicalCount;
	pDash->fCurrentLength =
		__xgeShapeExStrokeDashPatternAt(pDash, pDash->iCurrentIndex);
	pDash->bCurrentGap = !pDash->bCurrentGap;
}

static int __xgeShapeExStrokeDashOutputMove(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tPoint
)
{
	int iRet = __xgeShapeExStrokePathAppend(
		pDash->pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &tPoint, 1
	);

	if ( iRet == XGE_OK ) pDash->bMove = 0;
	return iRet;
}

static int __xgeShapeExStrokeDashOutputLine(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tStart,
	xge_vec2_t tEnd
)
{
	int iRet = XGE_OK;

	if ( pDash->bMove ) iRet = __xgeShapeExStrokeDashOutputMove(pDash, tStart);
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExStrokePathAppend(
			pDash->pPath, XGE_SHAPE_EX_CMD_LINE_TO, &tEnd, 1
		);
	}
	return iRet;
}

static int __xgeShapeExStrokeDashOutputCubic(
	xge_shape_ex_stroke_dash_t* pDash,
	const xge_shape_ex_stroke_cubic_t* pCubic
)
{
	xge_vec2_t arrPoints[3];
	int iRet = XGE_OK;

	if ( pDash->bMove ) iRet = __xgeShapeExStrokeDashOutputMove(pDash, pCubic->tStart);
	arrPoints[0] = pCubic->tControl1;
	arrPoints[1] = pCubic->tControl2;
	arrPoints[2] = pCubic->tEnd;
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExStrokePathAppend(
			pDash->pPath, XGE_SHAPE_EX_CMD_CUBIC_TO, arrPoints, 3
		);
	}
	return iRet;
}

static int __xgeShapeExStrokeDashOutputPoint(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tPoint
)
{
	int iRet = XGE_OK;

	if ( pDash->bMove ||
	     (__xgeShapeExStrokeDashPatternAt(pDash, pDash->iCurrentIndex) <
	      XGE_SHAPE_EX_STROKE_EPSILON) ) {
		iRet = __xgeShapeExStrokeDashOutputMove(pDash, tPoint);
	}
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExStrokePathAppend(
			pDash->pPath, XGE_SHAPE_EX_CMD_LINE_TO, &tPoint, 1
		);
	}
	return iRet;
}

static int __xgeShapeExStrokeDashLineTo(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tTo
)
{
	xge_vec2_t tSegmentStart = pDash->tCurrent;
	xge_vec2_t tSegmentEnd = tTo;
	float fLength = __xgeShapeExStrokePointDistance(tSegmentStart, tSegmentEnd);
	int iGuard = 0;
	int iRet = XGE_OK;

	if ( __xgeShapeExStrokeFloatZero(fLength) ) {
		iRet = __xgeShapeExStrokePathAppend(
			pDash->pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &pDash->tCurrent, 1
		);
	} else if ( fLength <= pDash->fCurrentLength ) {
		pDash->fCurrentLength -= fLength;
		if ( !pDash->bCurrentGap ) {
			iRet = __xgeShapeExStrokeDashOutputLine(pDash, tSegmentStart, tSegmentEnd);
		}
	} else {
		while ( (fLength - pDash->fCurrentLength) > XGE_SHAPE_EX_STROKE_DASH_THRESHOLD ) {
			xge_vec2_t tLeftEnd;
			xge_vec2_t tRightStart;

			if ( ++iGuard > 2000000 ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			if ( pDash->fCurrentLength > 0.0f ) {
				float fRatio = pDash->fCurrentLength / fLength;

				tLeftEnd = __xgeShapeExStrokePointAdd(
					tSegmentStart,
					__xgeShapeExStrokePointScale(
						__xgeShapeExStrokePointSub(tSegmentEnd, tSegmentStart), fRatio
					)
				);
				tRightStart = tLeftEnd;
				fLength -= pDash->fCurrentLength;
				if ( !pDash->bCurrentGap ) {
					iRet = __xgeShapeExStrokeDashOutputLine(
						pDash, tSegmentStart, tLeftEnd
					);
					if ( iRet != XGE_OK ) break;
				}
			} else {
				tRightStart = tSegmentStart;
				if ( pDash->bValidPoint && !pDash->bCurrentGap ) {
					iRet = __xgeShapeExStrokeDashOutputPoint(pDash, tSegmentStart);
					if ( iRet != XGE_OK ) break;
				}
			}
			__xgeShapeExStrokeDashNext(pDash);
			tSegmentStart = tRightStart;
			pDash->tCurrent = tRightStart;
			pDash->bMove = 1;
		}
		if ( iRet == XGE_OK ) {
			pDash->fCurrentLength -= fLength;
			if ( !pDash->bCurrentGap ) {
				iRet = __xgeShapeExStrokeDashOutputLine(
					pDash, tSegmentStart, tSegmentEnd
				);
			}
			if ( (pDash->fCurrentLength < 1.0f) &&
			     !__xgeShapeExStrokeFloatZero(fLength) ) {
				__xgeShapeExStrokeDashNext(pDash);
			}
		}
	}
	pDash->tCurrent = tTo;
	return iRet;
}

static int __xgeShapeExStrokeDashCubicTo(
	xge_shape_ex_stroke_dash_t* pDash,
	xge_vec2_t tControl1,
	xge_vec2_t tControl2,
	xge_vec2_t tTo
)
{
	xge_shape_ex_stroke_cubic_t tCurrent;
	float fLength;
	int iGuard = 0;
	int iRet = XGE_OK;

	tCurrent.tStart = pDash->tCurrent;
	tCurrent.tControl1 = tControl1;
	tCurrent.tControl2 = tControl2;
	tCurrent.tEnd = tTo;
	fLength = __xgeShapeExStrokeCubicLength(&tCurrent);
	if ( __xgeShapeExStrokeFloatZero(fLength) ) {
		iRet = __xgeShapeExStrokePathAppend(
			pDash->pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &pDash->tCurrent, 1
		);
	} else if ( fLength <= pDash->fCurrentLength ) {
		pDash->fCurrentLength -= fLength;
		if ( !pDash->bCurrentGap ) {
			iRet = __xgeShapeExStrokeDashOutputCubic(pDash, &tCurrent);
		}
	} else {
		while ( (fLength - pDash->fCurrentLength) > XGE_SHAPE_EX_STROKE_DASH_THRESHOLD ) {
			xge_shape_ex_stroke_cubic_t tLeft;
			xge_shape_ex_stroke_cubic_t tRight;

			if ( ++iGuard > 2000000 ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			if ( pDash->fCurrentLength > 0.0f ) {
				float fSplitLength = pDash->fCurrentLength;

				__xgeShapeExStrokeCubicSplitAtLength(
					&tCurrent, fSplitLength, fLength, &tLeft, &tRight
				);
				fLength -= fSplitLength;
				if ( !pDash->bCurrentGap ) {
					iRet = __xgeShapeExStrokeDashOutputCubic(pDash, &tLeft);
					if ( iRet != XGE_OK ) break;
				}
			} else {
				tRight = tCurrent;
				if ( pDash->bValidPoint && !pDash->bCurrentGap ) {
					iRet = __xgeShapeExStrokeDashOutputPoint(pDash, tCurrent.tStart);
					if ( iRet != XGE_OK ) break;
				}
			}
			__xgeShapeExStrokeDashNext(pDash);
			tCurrent = tRight;
			pDash->tCurrent = tRight.tStart;
			pDash->bMove = 1;
		}
		if ( iRet == XGE_OK ) {
			pDash->fCurrentLength -= fLength;
			if ( !pDash->bCurrentGap ) {
				iRet = __xgeShapeExStrokeDashOutputCubic(pDash, &tCurrent);
			}
			if ( (pDash->fCurrentLength < 0.1f) &&
			     !__xgeShapeExStrokeFloatZero(fLength) ) {
				__xgeShapeExStrokeDashNext(pDash);
			}
		}
	}
	pDash->tCurrent = tTo;
	return iRet;
}

static int __xgeShapeExStrokeDashPathBuild(
	xge_shape_ex_stroke_path_t* pPath,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount,
	xge_shape_ex_matrix_t tPathMatrix,
	const float* pPattern,
	int iPatternCount,
	float fOffset,
	int iCap
)
{
	xge_shape_ex_stroke_dash_t tDash;
	xge_vec2_t tCurrent = {0.0f, 0.0f};
	xge_vec2_t tStart = {0.0f, 0.0f};
	int iPointRead = 0;
	int bHasCurrent = 0;
	int iRet;
	int i;

	if ( (pPath == NULL) || (pCommands == NULL) || (iCommandCount < 0) ||
	     (pPoints == NULL) || (iPointCount < 0) ||
	     (iCap < XGE_SHAPE_EX_CAP_BUTT) || (iCap > XGE_SHAPE_EX_CAP_SQUARE) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pPath, 0, sizeof(*pPath));
	iRet = __xgeShapeExStrokeDashInitialize(
		&tDash, pPath, pPattern, iPatternCount, fOffset,
		iCap != XGE_SHAPE_EX_CAP_BUTT
	);
	for ( i = 0; (iRet == XGE_OK) && (i < iCommandCount); i++ ) {
		uint8_t iCommand = pCommands[i];

		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tCurrent = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			tStart = tCurrent;
			bHasCurrent = 1;
			__xgeShapeExStrokeDashMoveTo(&tDash, tCurrent);
		} else if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			xge_vec2_t tTo;

			if ( iPointRead >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tTo = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			if ( !bHasCurrent ) {
				tCurrent = tStart = tTo;
				bHasCurrent = 1;
				__xgeShapeExStrokeDashMoveTo(&tDash, tCurrent);
			} else {
				iRet = __xgeShapeExStrokeDashLineTo(&tDash, tTo);
				tCurrent = tTo;
			}
		} else if ( iCommand == XGE_SHAPE_EX_CMD_QUAD_TO ) {
			xge_vec2_t tControl;
			xge_vec2_t tTo;
			xge_vec2_t tControl1;
			xge_vec2_t tControl2;

			if ( (iPointRead + 1) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tControl = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			tTo = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			if ( !bHasCurrent ) {
				tCurrent = tStart = tTo;
				bHasCurrent = 1;
				__xgeShapeExStrokeDashMoveTo(&tDash, tCurrent);
				continue;
			}
			tControl1 = __xgeShapeExStrokePointAdd(
				tCurrent,
				__xgeShapeExStrokePointScale(
					__xgeShapeExStrokePointSub(tControl, tCurrent), 2.0f / 3.0f
				)
			);
			tControl2 = __xgeShapeExStrokePointAdd(
				tTo,
				__xgeShapeExStrokePointScale(
					__xgeShapeExStrokePointSub(tControl, tTo), 2.0f / 3.0f
				)
			);
			iRet = __xgeShapeExStrokeDashCubicTo(
				&tDash, tControl1, tControl2, tTo
			);
			tCurrent = tTo;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			xge_vec2_t tControl1;
			xge_vec2_t tControl2;
			xge_vec2_t tTo;

			if ( (iPointRead + 2) >= iPointCount ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				break;
			}
			tControl1 = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			tControl2 = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			tTo = __xgeShapeExStrokePointTransform(
				tPathMatrix, pPoints[iPointRead++]
			);
			if ( !bHasCurrent ) {
				tCurrent = tStart = tTo;
				bHasCurrent = 1;
				__xgeShapeExStrokeDashMoveTo(&tDash, tCurrent);
				continue;
			}
			iRet = __xgeShapeExStrokeDashCubicTo(
				&tDash, tControl1, tControl2, tTo
			);
			tCurrent = tTo;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			if ( bHasCurrent ) {
				iRet = __xgeShapeExStrokeDashLineTo(&tDash, tStart);
				tCurrent = tStart;
			}
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	if ( (iRet == XGE_OK) && (iPointRead != iPointCount) ) {
		iRet = XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet != XGE_OK ) __xgeShapeExStrokePathFree(pPath);
	return iRet;
}

static int __xgeShapeExStrokeExportBorder(
	xge_shape_ex_stroke_path_t* pPath,
	const xge_shape_ex_stroke_border_t* pBorder
)
{
	xge_vec2_t tContourStart = {0.0f, 0.0f};
	int bHasContourStart = 0;
	int i = 0;

	while ( i < pBorder->iCount ) {
		uint8_t iTag = pBorder->pTags[i];
		int iRet;

		if ( (iTag & XGE_SHAPE_EX_STROKE_TAG_BEGIN) != 0 ) {
			tContourStart = pBorder->pPoints[i];
			bHasContourStart = 1;
			iRet = __xgeShapeExStrokePathAppend(
				pPath, XGE_SHAPE_EX_CMD_MOVE_TO, &pBorder->pPoints[i], 1
			);
			i++;
			if ( iRet != XGE_OK ) return iRet;
			continue;
		}
		if ( (iTag & XGE_SHAPE_EX_STROKE_TAG_CUBIC) != 0 ) {
			int bEnd;

			if ( bHasContourStart && ((i + 1) < pBorder->iCount) &&
			     ((pBorder->pTags[i + 1] & XGE_SHAPE_EX_STROKE_TAG_END) != 0) ) {
				xge_vec2_t arrClosingCubic[3];

				arrClosingCubic[0] = pBorder->pPoints[i];
				arrClosingCubic[1] = pBorder->pPoints[i + 1];
				arrClosingCubic[2] = tContourStart;
				iRet = __xgeShapeExStrokePathAppend(
					pPath, XGE_SHAPE_EX_CMD_CUBIC_TO, arrClosingCubic, 3
				);
				if ( iRet == XGE_OK ) {
					iRet = __xgeShapeExStrokePathAppend(
						pPath, XGE_SHAPE_EX_CMD_CLOSE, NULL, 0
					);
				}
				if ( iRet != XGE_OK ) return iRet;
				i += 2;
				bHasContourStart = 0;
				continue;
			}
			bEnd = (pBorder->pTags[i + 2] & XGE_SHAPE_EX_STROKE_TAG_END) != 0;
			iRet = __xgeShapeExStrokePathAppend(
				pPath, XGE_SHAPE_EX_CMD_CUBIC_TO, pBorder->pPoints + i, 3
			);
			i += 3;
			if ( iRet != XGE_OK ) return iRet;
			if ( bEnd ) {
				iRet = __xgeShapeExStrokePathAppend(pPath, XGE_SHAPE_EX_CMD_CLOSE, NULL, 0);
				if ( iRet != XGE_OK ) return iRet;
				bHasContourStart = 0;
			}
			continue;
		}
		{
			int bEnd = (iTag & XGE_SHAPE_EX_STROKE_TAG_END) != 0;

			iRet = __xgeShapeExStrokePathAppend(
				pPath, XGE_SHAPE_EX_CMD_LINE_TO, &pBorder->pPoints[i], 1
			);
			i++;
			if ( iRet != XGE_OK ) return iRet;
			if ( bEnd ) {
				iRet = __xgeShapeExStrokePathAppend(pPath, XGE_SHAPE_EX_CMD_CLOSE, NULL, 0);
				if ( iRet != XGE_OK ) return iRet;
				bHasContourStart = 0;
			}
		}
	}
	return XGE_OK;
}

static int __xgeShapeExStrokePathBuild(
	xge_shape_ex_stroke_path_t* pPath,
	const uint8_t* pCommands,
	int iCommandCount,
	const xge_vec2_t* pPoints,
	int iPointCount,
	xge_shape_ex_matrix_t tPathMatrix,
	float fHalfWidth,
	int iCap,
	int iJoin,
	float fMiterLimit
)
{
	xge_shape_ex_stroke_input_t tInput;
	xge_shape_ex_stroker_t tStroke;
	int iRet;

	if ( (pPath == NULL) || (fHalfWidth <= 0.0f) ||
	     (iCap < XGE_SHAPE_EX_CAP_BUTT) || (iCap > XGE_SHAPE_EX_CAP_SQUARE) ||
	     (iJoin < XGE_SHAPE_EX_JOIN_MITER) || (iJoin > XGE_SHAPE_EX_JOIN_BEVEL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pPath, 0, sizeof(*pPath));
	memset(&tInput, 0, sizeof(tInput));
	memset(&tStroke, 0, sizeof(tStroke));
	tStroke.fWidth = fHalfWidth;
	tStroke.fMiterLimit = fMiterLimit;
	tStroke.iCap = iCap;
	tStroke.iJoin = iJoin;
	tStroke.arrBorders[0].iStart = 0;
	tStroke.arrBorders[1].iStart = 0;
	iRet = __xgeShapeExStrokeInputBuild(
		&tInput, pCommands, iCommandCount, pPoints, iPointCount, tPathMatrix
	);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeParseInput(&tStroke, &tInput);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeExportBorder(pPath, &tStroke.arrBorders[0]);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeExportBorder(pPath, &tStroke.arrBorders[1]);
	__xgeShapeExStrokeInputFree(&tInput);
	__xgeShapeExStrokeBorderFree(&tStroke.arrBorders[0]);
	__xgeShapeExStrokeBorderFree(&tStroke.arrBorders[1]);
	if ( iRet != XGE_OK ) __xgeShapeExStrokePathFree(pPath);
	return iRet;
}
