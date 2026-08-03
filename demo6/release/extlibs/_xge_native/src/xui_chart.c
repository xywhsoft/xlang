#include "xui_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define XUI_CHART_MAX_SERIES 16
#define XUI_CHART_LABEL_CAPACITY 64
#define XUI_CHART_NAME_CAPACITY 64
#define XUI_CHART_DASH_CAPACITY 8
#define XUI_CHART_PI 3.14159265358979323846

typedef struct xui_chart_series_t {
	int iType;
	int bVisible;
	int iSymbol;
	float fSymbolSize;
	uint32_t iColor;
	int bValueRadius;
	float fValueRadiusMin;
	float fValueRadiusMax;
	int bValueColor;
	uint32_t iValueColorMin;
	uint32_t iValueColorMax;
	int bAreaFill;
	uint32_t iAreaColor;
	int bSmooth;
	float arrDash[XUI_CHART_DASH_CAPACITY];
	int iDashCount;
	char sName[XUI_CHART_NAME_CAPACITY];
	xui_chart_point_t* pPoints;
	int iCount;
} xui_chart_series_t;

typedef struct xui_chart_data_t {
	xui_font pFont;
	char* sTitle;
	int iXAxisType;
	int iYAxisType;
	int iBarMode;
	int iBarDirection;
	int iPieMode;
	int bLegendVisible;
	int bTooltipVisible;
	xui_chart_tooltip_proc onTooltip;
	void* pTooltipUser;
	xui_thickness_t tPadding;
	uint32_t iDirtyFlags;
	uint32_t iBackgroundColor;
	uint32_t iPlotColor;
	uint32_t iGridColor;
	uint32_t iAxisColor;
	uint32_t iTextColor;
	uint32_t iTooltipColor;
	uint32_t iTooltipTextColor;
	float fPieInnerRadius;
	int iLodThreshold;
	int bAnimationEnabled;
	int bAnimationActive;
	float fAnimationDuration;
	float fAnimationProgress;
	xui_rect_t tPlotRect;
	int bViewRange;
	double fViewMinX;
	double fViewMaxX;
	double fViewMinY;
	double fViewMaxY;
	int bBrushRange;
	double fBrushMinX;
	double fBrushMaxX;
	double fBrushMinY;
	double fBrushMaxY;
	int iLastLodStride;
	double fMinX;
	double fMaxX;
	double fMinY;
	double fMaxY;
	xui_chart_series_t arrSeries[XUI_CHART_MAX_SERIES];
	int iSeriesCount;
	xui_chart_hit_t tHover;
	xui_chart_hit_t tSelected;
	int bPanning;
	float fPanStartX;
	float fPanStartY;
	double fPanMinX;
	double fPanMaxX;
	double fPanMinY;
	double fPanMaxY;
	int bBrushing;
	float fBrushStartX;
	float fBrushStartY;
	float fBrushCurrentX;
	float fBrushCurrentY;
} xui_chart_data_t;

static int __xuiChartInvalidate(xui_widget pWidget, xui_chart_data_t* pData, uint32_t iChartDirty, uint32_t iWidgetDirty)
{
	if ( pData != NULL ) {
		pData->iDirtyFlags |= iChartDirty;
	}
	return xuiWidgetInvalidate(pWidget, iWidgetDirty);
}

static uint32_t __xuiChartPalette(int iIndex)
{
	static const uint32_t arrPalette[] = {
		XUI_COLOR_RGBA(42, 124, 221, 255),
		XUI_COLOR_RGBA(28, 164, 115, 255),
		XUI_COLOR_RGBA(238, 145, 42, 255),
		XUI_COLOR_RGBA(211, 75, 98, 255),
		XUI_COLOR_RGBA(112, 92, 210, 255),
		XUI_COLOR_RGBA(38, 154, 180, 255),
		XUI_COLOR_RGBA(185, 118, 42, 255),
		XUI_COLOR_RGBA(82, 104, 134, 255)
	};
	return arrPalette[iIndex % (int)(sizeof(arrPalette) / sizeof(arrPalette[0]))];
}

static uint32_t __xuiChartColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiChartBlendComponent(int iMin, int iMax, float fRate)
{
	float fValue;

	fValue = (float)iMin + (float)(iMax - iMin) * fRate;
	return (int)(fValue + ((fValue >= 0.0f) ? 0.5f : -0.5f));
}

static int __xuiChartColorR(uint32_t iColor)
{
	return (int)((iColor >> 24) & 0xffu);
}

static int __xuiChartColorG(uint32_t iColor)
{
	return (int)((iColor >> 16) & 0xffu);
}

static int __xuiChartColorB(uint32_t iColor)
{
	return (int)((iColor >> 8) & 0xffu);
}

static uint32_t __xuiChartBlendColor(uint32_t iMinColor, uint32_t iMaxColor, float fRate)
{
	int r;
	int g;
	int b;
	int a;

	if ( fRate < 0.0f ) fRate = 0.0f;
	if ( fRate > 1.0f ) fRate = 1.0f;
	r = __xuiChartBlendComponent(__xuiChartColorR(iMinColor), __xuiChartColorR(iMaxColor), fRate);
	g = __xuiChartBlendComponent(__xuiChartColorG(iMinColor), __xuiChartColorG(iMaxColor), fRate);
	b = __xuiChartBlendComponent(__xuiChartColorB(iMinColor), __xuiChartColorB(iMaxColor), fRate);
	a = __xuiChartBlendComponent((int)__xuiChartColorAlpha(iMinColor), (int)__xuiChartColorAlpha(iMaxColor), fRate);
	return XUI_COLOR_RGBA(r, g, b, a);
}

static uint32_t __xuiChartColorWithAlphaScale(uint32_t iColor, float fScale)
{
	int iAlpha;

	if ( fScale < 0.0f ) fScale = 0.0f;
	if ( fScale > 1.0f ) fScale = 1.0f;
	iAlpha = (int)((float)__xuiChartColorAlpha(iColor) * fScale + 0.5f);
	return (iColor & 0xffffff00u) | (uint32_t)(iAlpha & 0xff);
}

static float __xuiChartAnimationAlpha(const xui_chart_data_t* pData)
{
	if ( (pData == NULL) || !pData->bAnimationEnabled || !pData->bAnimationActive ) {
		return 1.0f;
	}
	if ( pData->fAnimationProgress < 0.0f ) return 0.0f;
	if ( pData->fAnimationProgress > 1.0f ) return 1.0f;
	return pData->fAnimationProgress;
}

static uint32_t __xuiChartAnimatedColor(const xui_chart_data_t* pData, uint32_t iColor)
{
	return __xuiChartColorWithAlphaScale(iColor, __xuiChartAnimationAlpha(pData));
}

static void __xuiChartStartAnimation(xui_chart_data_t* pData)
{
	if ( (pData != NULL) && pData->bAnimationEnabled && (pData->fAnimationDuration > 0.0f) ) {
		pData->bAnimationActive = 1;
		pData->fAnimationProgress = 0.0f;
	}
}

static int __xuiChartSeriesTypeValid(int iType)
{
	return (iType == XUI_CHART_SERIES_LINE) ||
	       (iType == XUI_CHART_SERIES_BAR) ||
	       (iType == XUI_CHART_SERIES_PIE) ||
	       (iType == XUI_CHART_SERIES_SCATTER);
}

static int __xuiChartAxisTypeValid(int iType)
{
	return (iType == XUI_CHART_AXIS_VALUE) || (iType == XUI_CHART_AXIS_CATEGORY);
}

static int __xuiChartBarModeValid(int iMode)
{
	return (iMode == XUI_CHART_BAR_GROUPED) || (iMode == XUI_CHART_BAR_STACKED);
}

static int __xuiChartBarDirectionValid(int iDirection)
{
	return (iDirection == XUI_CHART_BAR_VERTICAL) || (iDirection == XUI_CHART_BAR_HORIZONTAL);
}

static int __xuiChartPieModeValid(int iMode)
{
	return (iMode == XUI_CHART_PIE_NORMAL) || (iMode == XUI_CHART_PIE_ROSE);
}

static int __xuiChartSymbolValid(int iSymbol)
{
	return (iSymbol == XUI_CHART_SYMBOL_CIRCLE) ||
	       (iSymbol == XUI_CHART_SYMBOL_RECT) ||
	       (iSymbol == XUI_CHART_SYMBOL_TRIANGLE) ||
	       (iSymbol == XUI_CHART_SYMBOL_DIAMOND);
}

static char* __xuiChartCopyString(const char* sText)
{
	char* sCopy;

	if ( (sText == NULL) || (sText[0] == 0) ) {
		return NULL;
	}
	sCopy = (char*)xrtCopyStr((str)(void*)sText, 0);
	if ( (sCopy == NULL) || (sCopy[0] == 0) ) {
		return NULL;
	}
	return sCopy;
}

static void __xuiChartSetFixedString(char* sDst, int iCapacity, const char* sText)
{
	if ( (sDst == NULL) || (iCapacity <= 0) ) {
		return;
	}
	sDst[0] = 0;
	if ( sText != NULL ) {
		strncpy(sDst, sText, (size_t)iCapacity - 1u);
		sDst[iCapacity - 1] = 0;
	}
}

static xui_chart_data_t* __xuiChartGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "chart");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_chart_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiChartStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( pColor == NULL ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static void __xuiChartSaveColors(const xui_chart_data_t* pData, uint32_t arrColors[7])
{
	if ( (pData == NULL) || (arrColors == NULL) ) {
		return;
	}
	arrColors[0] = pData->iBackgroundColor;
	arrColors[1] = pData->iPlotColor;
	arrColors[2] = pData->iGridColor;
	arrColors[3] = pData->iAxisColor;
	arrColors[4] = pData->iTextColor;
	arrColors[5] = pData->iTooltipColor;
	arrColors[6] = pData->iTooltipTextColor;
}

static void __xuiChartRestoreColors(xui_chart_data_t* pData, const uint32_t arrColors[7])
{
	if ( (pData == NULL) || (arrColors == NULL) ) {
		return;
	}
	pData->iBackgroundColor = arrColors[0];
	pData->iPlotColor = arrColors[1];
	pData->iGridColor = arrColors[2];
	pData->iAxisColor = arrColors[3];
	pData->iTextColor = arrColors[4];
	pData->iTooltipColor = arrColors[5];
	pData->iTooltipTextColor = arrColors[6];
}

static void __xuiChartApplyStyleColors(xui_widget pWidget, xui_chart_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiChartStyleColor(pWidget, "chart.background.color", &pData->iBackgroundColor);
	(void)__xuiChartStyleColor(pWidget, "chart.plot.color", &pData->iPlotColor);
	(void)__xuiChartStyleColor(pWidget, "chart.grid.color", &pData->iGridColor);
	(void)__xuiChartStyleColor(pWidget, "chart.axis.color", &pData->iAxisColor);
	(void)__xuiChartStyleColor(pWidget, "chart.text.color", &pData->iTextColor);
	(void)__xuiChartStyleColor(pWidget, "chart.tooltip.color", &pData->iTooltipColor);
	(void)__xuiChartStyleColor(pWidget, "chart.tooltip.text_color", &pData->iTooltipTextColor);
}

static void __xuiChartFreeSeries(xui_chart_series_t* pSeries)
{
	if ( pSeries == NULL ) {
		return;
	}
	if ( pSeries->pPoints != NULL ) {
		xrtFree(pSeries->pPoints);
	}
	memset(pSeries, 0, sizeof(*pSeries));
}

static xui_rect_t __xuiChartContentRect(xui_widget pWidget)
{
	xui_rect_t tRect;

	tRect = xuiWidgetGetContentRect(pWidget);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		tRect = xuiWidgetGetRect(pWidget);
		tRect.fX = 0.0f;
		tRect.fY = 0.0f;
	}
	return xuiInternalSnapRect(tRect);
}

static void __xuiChartComputeRanges(xui_chart_data_t* pData)
{
	double fMinX;
	double fMaxX;
	double fMinY;
	double fMaxY;
	int bAny;
	int iMaxBarCount;
	int bHorizontalBar;
	int bVerticalBar;
	int bValueSeries;
	int i;
	int j;

	fMinX = 0.0;
	fMaxX = 1.0;
	fMinY = 0.0;
	fMaxY = 1.0;
	bAny = 0;
	iMaxBarCount = 0;
	bHorizontalBar = 0;
	bVerticalBar = 0;
	bValueSeries = 0;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		xui_chart_series_t* pSeries = &pData->arrSeries[i];
		if ( !pSeries->bVisible || (pSeries->iType == XUI_CHART_SERIES_PIE) ) {
			continue;
		}
		if ( pSeries->iType != XUI_CHART_SERIES_BAR ) {
			bValueSeries = 1;
		}
		if ( pSeries->iType == XUI_CHART_SERIES_BAR && pData->iBarMode == XUI_CHART_BAR_STACKED ) {
			if ( pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) bHorizontalBar = 1;
			else bVerticalBar = 1;
			if ( pSeries->iCount > iMaxBarCount ) iMaxBarCount = pSeries->iCount;
			continue;
		}
		for ( j = 0; j < pSeries->iCount; j++ ) {
			double x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)j : pSeries->pPoints[j].x;
			double y = pSeries->pPoints[j].y;
			if ( pSeries->iType == XUI_CHART_SERIES_BAR && pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) {
				x = pSeries->pPoints[j].y;
				y = (double)j;
				bHorizontalBar = 1;
			} else if ( pSeries->iType == XUI_CHART_SERIES_BAR ) {
				bVerticalBar = 1;
			}
			if ( !bAny ) {
				fMinX = fMaxX = x;
				fMinY = fMaxY = y;
				bAny = 1;
			} else {
				if ( x < fMinX ) fMinX = x;
				if ( x > fMaxX ) fMaxX = x;
				if ( y < fMinY ) fMinY = y;
				if ( y > fMaxY ) fMaxY = y;
			}
		}
	}
	if ( pData->iBarMode == XUI_CHART_BAR_STACKED ) {
		for ( j = 0; j < iMaxBarCount; j++ ) {
			double fPositive;
			double fNegative;
			double x;
			int bHasX;

			fPositive = 0.0;
			fNegative = 0.0;
			x = (double)j;
			bHasX = 0;
			for ( i = 0; i < pData->iSeriesCount; i++ ) {
				xui_chart_series_t* pSeries = &pData->arrSeries[i];
				double y;
				if ( !pSeries->bVisible || (pSeries->iType != XUI_CHART_SERIES_BAR) || (j >= pSeries->iCount) ) {
					continue;
				}
				if ( !bHasX ) {
					x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)j : pSeries->pPoints[j].x;
					bHasX = 1;
				}
				y = pSeries->pPoints[j].y;
				if ( y >= 0.0 ) {
					fPositive += y;
				} else {
					fNegative += y;
				}
			}
			if ( bHasX ) {
				if ( !bAny ) {
					if ( pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) {
						fMinX = fNegative;
						fMaxX = fPositive;
						fMinY = fMaxY = x;
					} else {
						fMinX = fMaxX = x;
						fMinY = fNegative;
						fMaxY = fPositive;
					}
					bAny = 1;
				} else {
					if ( pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) {
						if ( fNegative < fMinX ) fMinX = fNegative;
						if ( fPositive > fMaxX ) fMaxX = fPositive;
						if ( x < fMinY ) fMinY = x;
						if ( x > fMaxY ) fMaxY = x;
					} else {
						if ( x < fMinX ) fMinX = x;
						if ( x > fMaxX ) fMaxX = x;
						if ( fNegative < fMinY ) fMinY = fNegative;
						if ( fPositive > fMaxY ) fMaxY = fPositive;
					}
				}
			}
		}
	}
	if ( !bAny ) {
		pData->fMinX = 0.0;
		pData->fMaxX = 1.0;
		pData->fMinY = 0.0;
		pData->fMaxY = 1.0;
		if ( pData->bViewRange ) {
			pData->fMinX = pData->fViewMinX;
			pData->fMaxX = pData->fViewMaxX;
			pData->fMinY = pData->fViewMinY;
			pData->fMaxY = pData->fViewMaxY;
		}
		return;
	}
	if ( pData->iYAxisType == XUI_CHART_AXIS_VALUE && fMinY > 0.0 ) {
		fMinY = 0.0;
	}
	if ( bHorizontalBar && fMinX > 0.0 ) {
		fMinX = 0.0;
	}
	if ( fMinX == fMaxX ) {
		fMinX -= 1.0;
		fMaxX += 1.0;
	}
	if ( fMinY == fMaxY ) {
		fMinY -= 1.0;
		fMaxY += 1.0;
	}
	if ( bHorizontalBar ) {
		fMinY -= 0.5;
		fMaxY += 0.5;
	} else if ( bVerticalBar ) {
		fMinX -= 0.5;
		fMaxX += 0.5;
	}
	if ( bValueSeries ) {
		double fPadX = (fMaxX - fMinX) * 0.05;
		double fPadY = (fMaxY - fMinY) * 0.05;
		if ( pData->iXAxisType == XUI_CHART_AXIS_VALUE ) {
			fMinX -= fPadX;
			fMaxX += fPadX;
		}
		if ( pData->iYAxisType == XUI_CHART_AXIS_VALUE ) {
			if ( fMinY == 0.0 ) {
				fMaxY += fPadY;
			} else {
				fMinY -= fPadY;
				fMaxY += fPadY;
			}
		}
	}
	pData->fMinX = fMinX;
	pData->fMaxX = fMaxX;
	pData->fMinY = fMinY;
	pData->fMaxY = fMaxY;
	if ( pData->bViewRange ) {
		pData->fMinX = pData->fViewMinX;
		pData->fMaxX = pData->fViewMaxX;
		pData->fMinY = pData->fViewMinY;
		pData->fMaxY = pData->fViewMaxY;
	}
}

static xui_rect_t __xuiChartComputePlotRect(xui_widget pWidget, xui_chart_data_t* pData)
{
	xui_rect_t tRect;
	float fTop;
	float fBottom;
	float fLeft;
	float fRight;

	tRect = __xuiChartContentRect(pWidget);
	fTop = (pData->sTitle != NULL && pData->sTitle[0] != 0) ? 30.0f : 12.0f;
	fBottom = 34.0f;
	fLeft = 48.0f;
	fRight = pData->bLegendVisible ? 116.0f : 16.0f;
	fTop += pData->tPadding.fTop;
	fBottom += pData->tPadding.fBottom;
	fLeft += pData->tPadding.fLeft;
	fRight += pData->tPadding.fRight;
	if ( tRect.fW < 240.0f ) {
		fRight = 12.0f;
	}
	tRect.fX += fLeft;
	tRect.fY += fTop;
	tRect.fW -= fLeft + fRight;
	tRect.fH -= fTop + fBottom;
	if ( tRect.fW < 10.0f ) tRect.fW = 10.0f;
	if ( tRect.fH < 10.0f ) tRect.fH = 10.0f;
	return xuiInternalSnapRect(tRect);
}

static float __xuiChartMapX(const xui_chart_data_t* pData, double x)
{
	double fRate;

	fRate = (x - pData->fMinX) / (pData->fMaxX - pData->fMinX);
	return pData->tPlotRect.fX + (float)(fRate * pData->tPlotRect.fW);
}

static float __xuiChartMapY(const xui_chart_data_t* pData, double y)
{
	double fRate;

	fRate = (y - pData->fMinY) / (pData->fMaxY - pData->fMinY);
	return pData->tPlotRect.fY + pData->tPlotRect.fH - (float)(fRate * pData->tPlotRect.fH);
}

static double __xuiChartUnmapX(const xui_chart_data_t* pData, float fX)
{
	double fRate;

	if ( pData->tPlotRect.fW == 0.0f ) {
		return pData->fMinX;
	}
	fRate = ((double)fX - (double)pData->tPlotRect.fX) / (double)pData->tPlotRect.fW;
	return pData->fMinX + fRate * (pData->fMaxX - pData->fMinX);
}

static double __xuiChartUnmapY(const xui_chart_data_t* pData, float fY)
{
	double fRate;

	if ( pData->tPlotRect.fH == 0.0f ) {
		return pData->fMinY;
	}
	fRate = ((double)(pData->tPlotRect.fY + pData->tPlotRect.fH) - (double)fY) / (double)pData->tPlotRect.fH;
	return pData->fMinY + fRate * (pData->fMaxY - pData->fMinY);
}

static int __xuiChartPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fX <= tRect.fX + tRect.fW) && (fY >= tRect.fY) && (fY <= tRect.fY + tRect.fH);
}

static int __xuiChartHasVisiblePie(const xui_chart_data_t* pData)
{
	int i;

	if ( pData == NULL ) {
		return 0;
	}
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_PIE) ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiChartHasVisibleBar(const xui_chart_data_t* pData, int iDirection)
{
	int i;

	if ( pData == NULL ) {
		return 0;
	}
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_BAR) &&
		     ((iDirection == 0) || (pData->iBarDirection == iDirection)) ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiChartHasVisibleNonBarValueSeries(const xui_chart_data_t* pData)
{
	int i;

	if ( pData == NULL ) {
		return 0;
	}
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		int iType = pData->arrSeries[i].iType;
		if ( pData->arrSeries[i].bVisible && (iType != XUI_CHART_SERIES_PIE) && (iType != XUI_CHART_SERIES_BAR) ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiChartSetViewRangeData(xui_chart_data_t* pData, double fMinX, double fMaxX, double fMinY, double fMaxY)
{
	if ( (pData == NULL) || (fMaxX <= fMinX) || (fMaxY <= fMinY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->bViewRange = 1;
	pData->fViewMinX = fMinX;
	pData->fViewMaxX = fMaxX;
	pData->fViewMinY = fMinY;
	pData->fViewMaxY = fMaxY;
	pData->fMinX = fMinX;
	pData->fMaxX = fMaxX;
	pData->fMinY = fMinY;
	pData->fMaxY = fMaxY;
	return XUI_OK;
}

static int __xuiChartSetBrushRangeData(xui_chart_data_t* pData, double fMinX, double fMaxX, double fMinY, double fMaxY)
{
	double fTemp;

	if ( (pData == NULL) || (fMinX == fMaxX) || (fMinY == fMaxY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fMinX > fMaxX ) {
		fTemp = fMinX;
		fMinX = fMaxX;
		fMaxX = fTemp;
	}
	if ( fMinY > fMaxY ) {
		fTemp = fMinY;
		fMinY = fMaxY;
		fMaxY = fTemp;
	}
	pData->bBrushRange = 1;
	pData->fBrushMinX = fMinX;
	pData->fBrushMaxX = fMaxX;
	pData->fBrushMinY = fMinY;
	pData->fBrushMaxY = fMaxY;
	return XUI_OK;
}

static xui_rect_t __xuiChartRangeToRect(const xui_chart_data_t* pData, double fMinX, double fMaxX, double fMinY, double fMaxY)
{
	float x0 = __xuiChartMapX(pData, fMinX);
	float x1 = __xuiChartMapX(pData, fMaxX);
	float y0 = __xuiChartMapY(pData, fMinY);
	float y1 = __xuiChartMapY(pData, fMaxY);
	xui_rect_t tRect;

	tRect.fX = (x0 < x1) ? x0 : x1;
	tRect.fY = (y0 < y1) ? y0 : y1;
	tRect.fW = (x0 < x1) ? (x1 - x0) : (x0 - x1);
	tRect.fH = (y0 < y1) ? (y1 - y0) : (y0 - y1);
	return xuiInternalSnapRect(tRect);
}

static int __xuiChartSeriesLodStride(xui_chart_data_t* pData, const xui_chart_series_t* pSeries)
{
	int iStride;

	if ( (pData == NULL) || (pSeries == NULL) || (pData->iLodThreshold <= 0) || (pSeries->iCount <= pData->iLodThreshold) ) {
		return 1;
	}
	iStride = (pSeries->iCount + pData->iLodThreshold - 1) / pData->iLodThreshold;
	if ( iStride < 1 ) iStride = 1;
	if ( iStride > pData->iLastLodStride ) pData->iLastLodStride = iStride;
	return iStride;
}

static int __xuiChartDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pFont == NULL) || (sText == NULL) || (sText[0] == 0) || ((iColor & 0xffu) == 0u) ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect(tRect);
	if ( (tRect.fX != tRect.fX) || (tRect.fY != tRect.fY) || (tRect.fW != tRect.fW) || (tRect.fH != tRect.fH) ||
	     (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, tRect, iColor, iFlags);
}

static int __xuiChartDrawablePoint(float fX, float fY)
{
	return (fX == fX) && (fY == fY) && (fX > -100000000.0f) && (fX < 100000000.0f) && (fY > -100000000.0f) && (fY < 100000000.0f);
}

static int __xuiChartDrawSymbol(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fSize, int iSymbol, uint32_t iColor)
{
	float r;

	if ( (pProxy == NULL) || (pDraw == NULL) || (fSize <= 0.0f) || (__xuiChartColorAlpha(iColor) == 0) || !__xuiChartDrawablePoint(fX, fY) ) {
		return XUI_OK;
	}
	r = fSize * 0.5f;
	if ( iSymbol == XUI_CHART_SYMBOL_RECT ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect((xui_rect_t){fX - r, fY - r, fSize, fSize}), iColor);
	}
	if ( iSymbol == XUI_CHART_SYMBOL_TRIANGLE ) {
		return pProxy->drawTriangleFill(pProxy, pDraw, (xui_vec2_t){fX, fY - r}, (xui_vec2_t){fX + r, fY + r}, (xui_vec2_t){fX - r, fY + r}, iColor);
	}
	if ( iSymbol == XUI_CHART_SYMBOL_DIAMOND ) {
		int iRet;
		iRet = pProxy->drawTriangleFill(pProxy, pDraw, (xui_vec2_t){fX, fY - r}, (xui_vec2_t){fX + r, fY}, (xui_vec2_t){fX, fY + r}, iColor);
		if ( iRet == XUI_OK ) iRet = pProxy->drawTriangleFill(pProxy, pDraw, (xui_vec2_t){fX, fY - r}, (xui_vec2_t){fX, fY + r}, (xui_vec2_t){fX - r, fY}, iColor);
		return iRet;
	}
	return pProxy->drawCircleFill(pProxy, pDraw, fX, fY, r, iColor);
}

static int __xuiChartSeriesValueRange(const xui_chart_series_t* pSeries, double* pMinValue, double* pMaxValue)
{
	double fMin;
	double fMax;
	int i;

	if ( (pSeries == NULL) || (pSeries->iCount <= 0) || (pSeries->pPoints == NULL) || (pMinValue == NULL) || (pMaxValue == NULL) ) {
		return 0;
	}
	fMin = pSeries->pPoints[0].value;
	fMax = pSeries->pPoints[0].value;
	for ( i = 1; i < pSeries->iCount; i++ ) {
		if ( pSeries->pPoints[i].value < fMin ) fMin = pSeries->pPoints[i].value;
		if ( pSeries->pPoints[i].value > fMax ) fMax = pSeries->pPoints[i].value;
	}
	*pMinValue = fMin;
	*pMaxValue = fMax;
	return 1;
}

static float __xuiChartValueRate(double fValue, double fMinValue, double fMaxValue)
{
	if ( fMaxValue == fMinValue ) {
		return 0.5f;
	}
	return (float)((fValue - fMinValue) / (fMaxValue - fMinValue));
}

static int __xuiChartDrawAxes(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData)
{
	xui_rect_t tPlot;
	int i;
	int iRet;
	int iLabelSeries;

	tPlot = pData->tPlotRect;
	iLabelSeries = -1;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType != XUI_CHART_SERIES_PIE) && (pData->arrSeries[i].iCount > 0) ) {
			iLabelSeries = i;
			break;
		}
	}
	iRet = pProxy->drawRectFill(pProxy, pDraw, tPlot, pData->iPlotColor);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i <= 4; i++ ) {
		float x = tPlot.fX + tPlot.fW * (float)i / 4.0f;
		float y = tPlot.fY + tPlot.fH * (float)i / 4.0f;
		char sText[48];
		iRet = pProxy->drawLine(pProxy, pDraw, x, tPlot.fY, x, tPlot.fY + tPlot.fH, 1.0f, pData->iGridColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawLine(pProxy, pDraw, tPlot.fX, y, tPlot.fX + tPlot.fW, y, 1.0f, pData->iGridColor);
		if ( iRet != XUI_OK ) return iRet;
		if ( pData->pFont != NULL ) {
			if ( __xuiChartHasVisibleBar(pData, XUI_CHART_BAR_HORIZONTAL) && (iLabelSeries >= 0) ) {
				int iPoint = (int)((double)(pData->arrSeries[iLabelSeries].iCount - 1) * (double)i / 4.0 + 0.5);
				const char* sLabel = NULL;
				double fXValue;
				if ( (iPoint >= 0) && (iPoint < pData->arrSeries[iLabelSeries].iCount) ) {
					sLabel = pData->arrSeries[iLabelSeries].pPoints[iPoint].label;
				}
				if ( sLabel == NULL ) {
					snprintf(sText, sizeof(sText), "%d", iPoint + 1);
					sLabel = sText;
				}
				iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sLabel, (xui_rect_t){tPlot.fX - 46.0f, __xuiChartMapY(pData, (double)iPoint) - 8.0f, 40.0f, 16.0f}, pData->iTextColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
				fXValue = pData->fMinX + (pData->fMaxX - pData->fMinX) * (double)i / 4.0;
				snprintf(sText, sizeof(sText), "%.1f", fXValue);
				iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sText, (xui_rect_t){x - 28.0f, tPlot.fY + tPlot.fH + 4.0f, 56.0f, 18.0f}, pData->iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			} else {
				double fYValue = pData->fMaxY - (pData->fMaxY - pData->fMinY) * (double)i / 4.0;
				snprintf(sText, sizeof(sText), "%.1f", fYValue);
				iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sText, (xui_rect_t){tPlot.fX - 46.0f, y - 8.0f, 40.0f, 16.0f}, pData->iTextColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
				if ( pData->iXAxisType == XUI_CHART_AXIS_CATEGORY && iLabelSeries >= 0 ) {
					int iPoint = (int)((double)(pData->arrSeries[iLabelSeries].iCount - 1) * (double)i / 4.0 + 0.5);
					const char* sLabel = NULL;
					if ( (iPoint >= 0) && (iPoint < pData->arrSeries[iLabelSeries].iCount) ) {
						sLabel = pData->arrSeries[iLabelSeries].pPoints[iPoint].label;
					}
					if ( sLabel == NULL ) {
						snprintf(sText, sizeof(sText), "%d", iPoint + 1);
						sLabel = sText;
					}
					iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sLabel, (xui_rect_t){x - 28.0f, tPlot.fY + tPlot.fH + 4.0f, 56.0f, 18.0f}, pData->iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
					if ( iRet != XUI_OK ) return iRet;
				} else {
					double fXValue = pData->fMinX + (pData->fMaxX - pData->fMinX) * (double)i / 4.0;
					snprintf(sText, sizeof(sText), "%.1f", fXValue);
					iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sText, (xui_rect_t){x - 28.0f, tPlot.fY + tPlot.fH + 4.0f, 56.0f, 18.0f}, pData->iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
		}
	}
	iRet = pProxy->drawLine(pProxy, pDraw, tPlot.fX, tPlot.fY + tPlot.fH, tPlot.fX + tPlot.fW, tPlot.fY + tPlot.fH, 1.0f, pData->iAxisColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, tPlot.fX, tPlot.fY, tPlot.fX, tPlot.fY + tPlot.fH, 1.0f, pData->iAxisColor);
}

static int __xuiChartDrawLineSeries(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, xui_chart_series_t* pSeries)
{
	int i;
	int iRet;
	int iStride;
	int iDrawn;
	int bSmoothDrawn;
	float fPrevX;
	float fPrevY;

	bSmoothDrawn = 0;
	if ( (pSeries->bSmooth || (pSeries->iDashCount > 0)) && (pSeries->iCount >= 2) ) {
		xui_path pPath;
		xui_vec2_t* pPoints;
		xui_path_style_t tStyle;
		int iPointCount;
		iRet = xuiPathCreate(&pPath);
		if ( iRet != XUI_OK ) return iRet;
		pPoints = (xui_vec2_t*)xrtMalloc(sizeof(*pPoints) * (size_t)pSeries->iCount);
		if ( pPoints == NULL ) {
			xuiPathDestroy(pPath);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		iStride = __xuiChartSeriesLodStride(pData, pSeries);
		iPointCount = 0;
		for ( i = 0; i < pSeries->iCount; i++ ) {
			double x;
			if ( (iStride > 1) && (i != 0) && (i != pSeries->iCount - 1) && ((i % iStride) != 0) ) {
				continue;
			}
			x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)i : pSeries->pPoints[i].x;
			pPoints[iPointCount++] = (xui_vec2_t){__xuiChartMapX(pData, x), __xuiChartMapY(pData, pSeries->pPoints[i].y)};
		}
		if ( iPointCount >= 2 ) {
			iRet = xuiPathMoveTo(pPath, pPoints[0].fX, pPoints[0].fY);
			for ( i = 1; (iRet == XUI_OK) && (i < iPointCount); i++ ) {
				xui_vec2_t p0 = pPoints[(i > 1) ? (i - 2) : (i - 1)];
				xui_vec2_t p1 = pPoints[i - 1];
				xui_vec2_t p2 = pPoints[i];
				xui_vec2_t p3 = pPoints[(i + 1 < iPointCount) ? (i + 1) : i];
				xui_vec2_t c1 = {p1.fX + (p2.fX - p0.fX) / 6.0f, p1.fY + (p2.fY - p0.fY) / 6.0f};
				xui_vec2_t c2 = {p2.fX - (p3.fX - p1.fX) / 6.0f, p2.fY - (p3.fY - p1.fY) / 6.0f};
				iRet = xuiPathCubicTo(pPath, c1.fX, c1.fY, c2.fX, c2.fY, p2.fX, p2.fY);
			}
			if ( iRet == XUI_OK ) {
				memset(&tStyle, 0, sizeof(tStyle));
				tStyle.iSize = (uint32_t)sizeof(tStyle);
				tStyle.iStrokeColor = __xuiChartAnimatedColor(pData, pSeries->iColor);
				tStyle.fStrokeWidth = 2.0f;
				tStyle.iLineJoin = XUI_PATH_JOIN_MITER;
				tStyle.iLineCap = XUI_PATH_CAP_BUTT;
				tStyle.pDashPattern = pSeries->iDashCount > 0 ? pSeries->arrDash : NULL;
				tStyle.iDashCount = pSeries->iDashCount;
				iRet = xuiInternalDrawPath(pProxy, pDraw, pPath, &tStyle, 1.0f);
				if ( iRet == XUI_OK ) bSmoothDrawn = 1;
			}
		}
		xrtFree(pPoints);
		xuiPathDestroy(pPath);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	fPrevX = 0.0f;
	fPrevY = 0.0f;
	iStride = __xuiChartSeriesLodStride(pData, pSeries);
	iDrawn = 0;
	for ( i = 0; i < pSeries->iCount; i++ ) {
		double x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)i : pSeries->pPoints[i].x;
		float fX = __xuiChartMapX(pData, x);
		float fY = __xuiChartMapY(pData, pSeries->pPoints[i].y);
		if ( (iStride > 1) && (i != 0) && (i != pSeries->iCount - 1) && ((i % iStride) != 0) ) {
			continue;
		}
		if ( (iDrawn > 0) && !bSmoothDrawn ) {
			iRet = pProxy->drawLine(pProxy, pDraw, fPrevX, fPrevY, fX, fY, 2.0f, __xuiChartAnimatedColor(pData, pSeries->iColor));
			if ( iRet != XUI_OK ) return iRet;
		}
		iRet = __xuiChartDrawSymbol(pProxy, pDraw, fX, fY, pSeries->fSymbolSize, pSeries->iSymbol, __xuiChartAnimatedColor(pData, pSeries->iColor));
		if ( iRet != XUI_OK ) return iRet;
		fPrevX = fX;
		fPrevY = fY;
		iDrawn++;
	}
	return XUI_OK;
}

static int __xuiChartDrawLineArea(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, xui_chart_series_t* pSeries)
{
	xui_path pPath;
	xui_path_style_t tStyle;
	uint32_t iColor;
	int iStride;
	int iDrawn;
	int iRet;
	int i;

	if ( (pProxy == NULL) || (pProxy->drawMeshTriangles == NULL) || (pSeries == NULL) ||
	     !pSeries->bAreaFill || (pSeries->iCount < 2) ) {
		return XUI_OK;
	}
	iRet = xuiPathCreate(&pPath);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iStride = __xuiChartSeriesLodStride(pData, pSeries);
	iDrawn = 0;
	for ( i = 0; i < pSeries->iCount; i++ ) {
		double x;
		float fX;
		float fY;
		if ( (iStride > 1) && (i != 0) && (i != pSeries->iCount - 1) && ((i % iStride) != 0) ) {
			continue;
		}
		x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)i : pSeries->pPoints[i].x;
		fX = __xuiChartMapX(pData, x);
		fY = __xuiChartMapY(pData, pSeries->pPoints[i].y);
		if ( iDrawn == 0 ) {
			iRet = xuiPathMoveTo(pPath, fX, __xuiChartMapY(pData, 0.0));
			if ( iRet == XUI_OK ) iRet = xuiPathLineTo(pPath, fX, fY);
		} else {
			iRet = xuiPathLineTo(pPath, fX, fY);
		}
		if ( iRet != XUI_OK ) {
			xuiPathDestroy(pPath);
			return iRet;
		}
		iDrawn++;
	}
	if ( iDrawn < 2 ) {
		xuiPathDestroy(pPath);
		return XUI_OK;
	}
	iRet = xuiPathLineTo(pPath, __xuiChartMapX(pData, (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)(pSeries->iCount - 1) : pSeries->pPoints[pSeries->iCount - 1].x), __xuiChartMapY(pData, 0.0));
	if ( iRet == XUI_OK ) iRet = xuiPathClose(pPath);
	if ( iRet != XUI_OK ) {
		xuiPathDestroy(pPath);
		return iRet;
	}
	iColor = __xuiChartAnimatedColor(pData, pSeries->iAreaColor);
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = (uint32_t)sizeof(tStyle);
	tStyle.iFillColor = iColor;
	tStyle.iFillRule = XUI_PATH_FILL_NON_ZERO;
	iRet = xuiInternalDrawPath(pProxy, pDraw, pPath, &tStyle, 1.0f);
	xuiPathDestroy(pPath);
	return iRet;
}

static int __xuiChartVisibleBarSeries(const xui_chart_data_t* pData)
{
	int i;
	int iCount;

	iCount = 0;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_BAR) ) {
			iCount++;
		}
	}
	return iCount;
}

static int __xuiChartBarVisibleIndex(const xui_chart_data_t* pData, int iSeries)
{
	int i;
	int iIndex;

	iIndex = 0;
	for ( i = 0; i < iSeries; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_BAR) ) {
			iIndex++;
		}
	}
	return iIndex;
}

static void __xuiChartStackedBarRange(const xui_chart_data_t* pData, int iSeries, int iItem, double* pStart, double* pEnd)
{
	double fStart;
	double fValue;
	int i;

	fStart = 0.0;
	fValue = pData->arrSeries[iSeries].pPoints[iItem].y;
	for ( i = 0; i < iSeries; i++ ) {
		const xui_chart_series_t* pSeries = &pData->arrSeries[i];
		double y;
		if ( !pSeries->bVisible || (pSeries->iType != XUI_CHART_SERIES_BAR) || (iItem >= pSeries->iCount) ) {
			continue;
		}
		y = pSeries->pPoints[iItem].y;
		if ( ((fValue >= 0.0) && (y >= 0.0)) || ((fValue < 0.0) && (y < 0.0)) ) {
			fStart += y;
		}
	}
	if ( pStart != NULL ) *pStart = fStart;
	if ( pEnd != NULL ) *pEnd = fStart + fValue;
}

static int __xuiChartDrawBarSeries(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, int iSeries)
{
	xui_chart_series_t* pSeries;
	int i;
	int iRet;
	int iBarCount;
	int iBarIndex;
	float fSlot;
	float fBarW;
	float fBaseY;
	float fBaseX;

	pSeries = &pData->arrSeries[iSeries];
	iBarCount = __xuiChartVisibleBarSeries(pData);
	if ( iBarCount <= 0 ) {
		iBarCount = 1;
	}
	if ( pData->iBarMode == XUI_CHART_BAR_STACKED ) {
		iBarCount = 1;
	}
	iBarIndex = __xuiChartBarVisibleIndex(pData, iSeries);
	fSlot = ((pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL) ? pData->tPlotRect.fH : pData->tPlotRect.fW) / (float)((pSeries->iCount > 1) ? pSeries->iCount : 1);
	fBarW = fSlot * 0.72f / (float)iBarCount;
	fBaseY = __xuiChartMapY(pData, 0.0);
	fBaseX = __xuiChartMapX(pData, 0.0);
	for ( i = 0; i < pSeries->iCount; i++ ) {
		double x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)i : pSeries->pPoints[i].x;
		float fX = __xuiChartMapX(pData, x) - (fSlot * 0.36f) + fBarW * (float)iBarIndex;
		float fY = __xuiChartMapY(pData, pSeries->pPoints[i].y);
		xui_rect_t tBar;
		if ( pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) {
			float fValueX = __xuiChartMapX(pData, pSeries->pPoints[i].y);
			float fItemY = __xuiChartMapY(pData, (double)i) - (fSlot * 0.36f) + fBarW * (float)iBarIndex;
			if ( pData->iBarMode == XUI_CHART_BAR_STACKED ) {
				double fStart;
				double fEnd;
				float fStartX;
				float fEndX;
				__xuiChartStackedBarRange(pData, iSeries, i, &fStart, &fEnd);
				fStartX = __xuiChartMapX(pData, fStart);
				fEndX = __xuiChartMapX(pData, fEnd);
				fItemY = __xuiChartMapY(pData, (double)i) - (fSlot * 0.36f);
				if ( fEndX >= fStartX ) {
					tBar = (xui_rect_t){fStartX, fItemY, fEndX - fStartX, fBarW};
				} else {
					tBar = (xui_rect_t){fEndX, fItemY, fStartX - fEndX, fBarW};
				}
			} else if ( fValueX >= fBaseX ) {
				tBar = (xui_rect_t){fBaseX, fItemY, fValueX - fBaseX, fBarW};
			} else {
				tBar = (xui_rect_t){fValueX, fItemY, fBaseX - fValueX, fBarW};
			}
			tBar = xuiInternalSnapRect(tBar);
			if ( (tBar.fW > 0.0f) && (tBar.fH > 0.0f) ) {
				iRet = pProxy->drawRectFill(pProxy, pDraw, tBar, __xuiChartAnimatedColor(pData, (pSeries->pPoints[i].color != 0) ? pSeries->pPoints[i].color : pSeries->iColor));
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		if ( pData->iBarMode == XUI_CHART_BAR_STACKED ) {
			double fStart;
			double fEnd;
			float fStartY;
			float fEndY;
			__xuiChartStackedBarRange(pData, iSeries, i, &fStart, &fEnd);
			fX = __xuiChartMapX(pData, x) - (fSlot * 0.36f);
			fStartY = __xuiChartMapY(pData, fStart);
			fEndY = __xuiChartMapY(pData, fEnd);
			if ( fEndY <= fStartY ) {
				tBar = (xui_rect_t){fX, fEndY, fBarW, fStartY - fEndY};
			} else {
				tBar = (xui_rect_t){fX, fStartY, fBarW, fEndY - fStartY};
			}
			tBar = xuiInternalSnapRect(tBar);
			if ( (tBar.fW > 0.0f) && (tBar.fH > 0.0f) ) {
				iRet = pProxy->drawRectFill(pProxy, pDraw, tBar, (pSeries->pPoints[i].color != 0) ? pSeries->pPoints[i].color : pSeries->iColor);
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		if ( fY <= fBaseY ) {
			tBar = (xui_rect_t){fX, fY, fBarW, fBaseY - fY};
		} else {
			tBar = (xui_rect_t){fX, fBaseY, fBarW, fY - fBaseY};
		}
		tBar = xuiInternalSnapRect(tBar);
		if ( (tBar.fW > 0.0f) && (tBar.fH > 0.0f) ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tBar, __xuiChartAnimatedColor(pData, (pSeries->pPoints[i].color != 0) ? pSeries->pPoints[i].color : pSeries->iColor));
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiChartDrawScatterSeries(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, xui_chart_series_t* pSeries)
{
	double fMinValue;
	double fMaxValue;
	int iStride;
	int i;
	int iRet;

	fMinValue = 0.0;
	fMaxValue = 0.0;
	iStride = __xuiChartSeriesLodStride(pData, pSeries);
	(void)__xuiChartSeriesValueRange(pSeries, &fMinValue, &fMaxValue);
	for ( i = 0; i < pSeries->iCount; i++ ) {
		float fRate = __xuiChartValueRate(pSeries->pPoints[i].value, fMinValue, fMaxValue);
		float fSize = pSeries->bValueRadius ? (pSeries->fValueRadiusMin + (pSeries->fValueRadiusMax - pSeries->fValueRadiusMin) * fRate) : pSeries->fSymbolSize;
		uint32_t iColor = pSeries->iColor;
		float fX = __xuiChartMapX(pData, pSeries->pPoints[i].x);
		float fY = __xuiChartMapY(pData, pSeries->pPoints[i].y);
		if ( (iStride > 1) && (i != 0) && (i != pSeries->iCount - 1) && ((i % iStride) != 0) ) {
			continue;
		}
		if ( pSeries->bValueColor ) {
			iColor = __xuiChartBlendColor(pSeries->iValueColorMin, pSeries->iValueColorMax, fRate);
		}
		if ( pSeries->pPoints[i].color != 0 ) {
			iColor = pSeries->pPoints[i].color;
		}
		iColor = __xuiChartAnimatedColor(pData, iColor);
		iRet = __xuiChartDrawSymbol(pProxy, pDraw, fX, fY, fSize, pSeries->iSymbol, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiChartVisiblePieSeries(const xui_chart_data_t* pData)
{
	int i;
	int iCount;

	iCount = 0;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_PIE) ) {
			iCount++;
		}
	}
	return iCount;
}

static int __xuiChartPieVisibleIndex(const xui_chart_data_t* pData, int iSeries)
{
	int i;
	int iIndex;

	iIndex = 0;
	for ( i = 0; i < iSeries; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_PIE) ) {
			iIndex++;
		}
	}
	return iIndex;
}

static void __xuiChartPieRing(const xui_chart_data_t* pData, int iSeries, float fRadius, float* pInnerRadius, float* pOuterRadius)
{
	int iRingCount;
	int iRingIndex;
	float fInnerRate;
	float fOuterRate;
	float fSpanRate;

	iRingCount = __xuiChartVisiblePieSeries(pData);
	if ( iRingCount <= 0 ) iRingCount = 1;
	iRingIndex = __xuiChartPieVisibleIndex(pData, iSeries);
	if ( iRingIndex < 0 ) iRingIndex = 0;
	if ( iRingIndex >= iRingCount ) iRingIndex = iRingCount - 1;
	fSpanRate = (1.0f - pData->fPieInnerRadius) / (float)iRingCount;
	fInnerRate = pData->fPieInnerRadius + fSpanRate * (float)iRingIndex;
	fOuterRate = pData->fPieInnerRadius + fSpanRate * (float)(iRingIndex + 1);
	if ( pInnerRadius != NULL ) *pInnerRadius = fRadius * fInnerRate;
	if ( pOuterRadius != NULL ) *pOuterRadius = fRadius * fOuterRate;
}

static double __xuiChartPieMaxValue(const xui_chart_series_t* pSeries)
{
	double fMax;
	int i;

	fMax = 0.0;
	if ( pSeries == NULL ) {
		return 0.0;
	}
	for ( i = 0; i < pSeries->iCount; i++ ) {
		if ( pSeries->pPoints[i].value > fMax ) {
			fMax = pSeries->pPoints[i].value;
		}
	}
	return fMax;
}

static int __xuiChartDrawPieSliceMesh(xui_proxy pProxy, xui_draw_context pDraw, float fCx, float fCy, float fInnerRadius, float fOuterRadius, double fAngle, double fNext, int iSteps, uint32_t iColor)
{
	xui_mesh_vertex_t* pVertices;
	uint32_t* pIndices;
	int iVertexCount;
	int iIndexCount;
	int s;
	int iRet;

	if ( (pProxy == NULL) || (pProxy->drawMeshTriangles == NULL) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( iSteps < 2 ) {
		iSteps = 2;
	}
	if ( fInnerRadius <= 0.0f ) {
		iVertexCount = iSteps + 2;
		iIndexCount = iSteps * 3;
	} else {
		iVertexCount = (iSteps + 1) * 2;
		iIndexCount = iSteps * 6;
	}
	pVertices = (xui_mesh_vertex_t*)xrtMalloc(sizeof(*pVertices) * (size_t)iVertexCount);
	pIndices = (uint32_t*)xrtMalloc(sizeof(*pIndices) * (size_t)iIndexCount);
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		if ( pVertices != NULL ) xrtFree(pVertices);
		if ( pIndices != NULL ) xrtFree(pIndices);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( fInnerRadius <= 0.0f ) {
		pVertices[0] = (xui_mesh_vertex_t){fCx, fCy, iColor};
		for ( s = 0; s <= iSteps; s++ ) {
			double a = fAngle + (fNext - fAngle) * (double)s / (double)iSteps;
			pVertices[s + 1] = (xui_mesh_vertex_t){fCx + (float)cos(a) * fOuterRadius, fCy + (float)sin(a) * fOuterRadius, iColor};
		}
		for ( s = 0; s < iSteps; s++ ) {
			pIndices[(s * 3) + 0] = 0u;
			pIndices[(s * 3) + 1] = (uint32_t)(s + 1);
			pIndices[(s * 3) + 2] = (uint32_t)(s + 2);
		}
	} else {
		for ( s = 0; s <= iSteps; s++ ) {
			double a = fAngle + (fNext - fAngle) * (double)s / (double)iSteps;
			pVertices[(s * 2) + 0] = (xui_mesh_vertex_t){fCx + (float)cos(a) * fOuterRadius, fCy + (float)sin(a) * fOuterRadius, iColor};
			pVertices[(s * 2) + 1] = (xui_mesh_vertex_t){fCx + (float)cos(a) * fInnerRadius, fCy + (float)sin(a) * fInnerRadius, iColor};
		}
		for ( s = 0; s < iSteps; s++ ) {
			int iBase = s * 2;
			int iIndex = s * 6;
			pIndices[iIndex + 0] = (uint32_t)iBase;
			pIndices[iIndex + 1] = (uint32_t)(iBase + 2);
			pIndices[iIndex + 2] = (uint32_t)(iBase + 3);
			pIndices[iIndex + 3] = (uint32_t)iBase;
			pIndices[iIndex + 4] = (uint32_t)(iBase + 3);
			pIndices[iIndex + 5] = (uint32_t)(iBase + 1);
		}
	}
	iRet = pProxy->drawMeshTriangles(pProxy, pDraw, pVertices, iVertexCount, pIndices, iIndexCount, 0);
	xrtFree(pIndices);
	xrtFree(pVertices);
	return iRet;
}

static int __xuiChartDrawPieSeries(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, int iSeries)
{
	xui_chart_series_t* pSeries;
	double fTotal;
	double fMaxValue;
	double fAngle;
	float fCx;
	float fCy;
	float fRadius;
	float fInnerRadius;
	float fOuterRadius;
	int i;
	int s;
	int iRet;

	pSeries = &pData->arrSeries[iSeries];
	fTotal = 0.0;
	fMaxValue = __xuiChartPieMaxValue(pSeries);
	for ( i = 0; i < pSeries->iCount; i++ ) {
		if ( pSeries->pPoints[i].value > 0.0 ) {
			fTotal += pSeries->pPoints[i].value;
		}
	}
	if ( (pData->iPieMode == XUI_CHART_PIE_NORMAL && fTotal <= 0.0) || (pData->iPieMode == XUI_CHART_PIE_ROSE && fMaxValue <= 0.0) ) {
		return XUI_OK;
	}
	fCx = pData->tPlotRect.fX + pData->tPlotRect.fW * 0.5f;
	fCy = pData->tPlotRect.fY + pData->tPlotRect.fH * 0.5f;
	fRadius = ((pData->tPlotRect.fW < pData->tPlotRect.fH) ? pData->tPlotRect.fW : pData->tPlotRect.fH) * 0.42f;
	__xuiChartPieRing(pData, iSeries, fRadius, &fInnerRadius, &fOuterRadius);
	fAngle = -XUI_CHART_PI * 0.5;
	for ( i = 0; i < pSeries->iCount; i++ ) {
		double fNext;
		float fSliceOuterRadius;
		int iSteps;
		uint32_t iColor;
		if ( pSeries->pPoints[i].value <= 0.0 ) {
			continue;
		}
		if ( pData->iPieMode == XUI_CHART_PIE_ROSE ) {
			fAngle = -XUI_CHART_PI * 0.5 + XUI_CHART_PI * 2.0 * (double)i / (double)pSeries->iCount;
			fNext = -XUI_CHART_PI * 0.5 + XUI_CHART_PI * 2.0 * (double)(i + 1) / (double)pSeries->iCount;
			fSliceOuterRadius = fInnerRadius + (fOuterRadius - fInnerRadius) * (float)(pSeries->pPoints[i].value / fMaxValue);
		} else {
			fNext = fAngle + (pSeries->pPoints[i].value / fTotal) * XUI_CHART_PI * 2.0;
			fSliceOuterRadius = fOuterRadius;
		}
		iSteps = (int)((fNext - fAngle) / (XUI_CHART_PI * 2.0) * 72.0) + 1;
		if ( iSteps < 2 ) iSteps = 2;
		iColor = (pSeries->pPoints[i].color != 0) ? pSeries->pPoints[i].color : __xuiChartPalette(i);
		iColor = __xuiChartAnimatedColor(pData, iColor);
		iRet = __xuiChartDrawPieSliceMesh(pProxy, pDraw, fCx, fCy, fInnerRadius, fSliceOuterRadius, fAngle, fNext, iSteps, iColor);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) {
			for ( s = 0; s < iSteps; s++ ) {
				double a0 = fAngle + (fNext - fAngle) * (double)s / (double)iSteps;
				double a1 = fAngle + (fNext - fAngle) * (double)(s + 1) / (double)iSteps;
				xui_vec2_t tOuter0 = {fCx + (float)cos(a0) * fSliceOuterRadius, fCy + (float)sin(a0) * fSliceOuterRadius};
				xui_vec2_t tOuter1 = {fCx + (float)cos(a1) * fSliceOuterRadius, fCy + (float)sin(a1) * fSliceOuterRadius};
				xui_vec2_t tInner0 = {fCx + (float)cos(a0) * fInnerRadius, fCy + (float)sin(a0) * fInnerRadius};
				xui_vec2_t tInner1 = {fCx + (float)cos(a1) * fInnerRadius, fCy + (float)sin(a1) * fInnerRadius};
				if ( fInnerRadius <= 0.0f ) {
					iRet = pProxy->drawTriangleFill(pProxy, pDraw, (xui_vec2_t){fCx, fCy}, tOuter0, tOuter1, iColor);
					if ( iRet != XUI_OK ) return iRet;
				} else {
					iRet = pProxy->drawTriangleFill(pProxy, pDraw, tOuter0, tOuter1, tInner1, iColor);
					if ( iRet != XUI_OK ) return iRet;
					iRet = pProxy->drawTriangleFill(pProxy, pDraw, tOuter0, tInner1, tInner0, iColor);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
		} else if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pData->iPieMode == XUI_CHART_PIE_NORMAL ) {
			fAngle = fNext;
		}
	}
	return XUI_OK;
}

static int __xuiChartDrawLegend(xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData, xui_rect_t tContent)
{
	int i;
	float fX;
	float fY;

	if ( !pData->bLegendVisible ) {
		return XUI_OK;
	}
	fX = tContent.fX + tContent.fW - 108.0f;
	fY = tContent.fY + 36.0f;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		xui_rect_t tSwatch = {fX, fY + (float)i * 20.0f + 4.0f, 10.0f, 10.0f};
		xui_rect_t tText = {fX + 16.0f, fY + (float)i * 20.0f, 90.0f, 18.0f};
		uint32_t iColor = pData->arrSeries[i].bVisible ? pData->arrSeries[i].iColor : XUI_COLOR_RGBA(120, 128, 136, 120);
		int iRet = pProxy->drawRectFill(pProxy, pDraw, tSwatch, iColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, pData->arrSeries[i].sName, tText, pData->iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiChartDrawSelection(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_chart_data_t* pData)
{
	xui_chart_hit_t* pHit;
	xui_rect_t tBrush;
	int iRet;

	if ( pData->bBrushRange && !__xuiChartHasVisiblePie(pData) ) {
		tBrush = __xuiChartRangeToRect(pData, pData->fBrushMinX, pData->fBrushMaxX, pData->fBrushMinY, pData->fBrushMaxY);
		if ( (tBrush.fW > 0.0f) && (tBrush.fH > 0.0f) ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tBrush, XUI_COLOR_RGBA(42, 124, 221, 38));
			if ( iRet != XUI_OK ) return iRet;
			if ( pProxy->drawRectStroke != NULL ) {
				iRet = pProxy->drawRectStroke(pProxy, pDraw, tBrush, 1.0f, XUI_COLOR_RGBA(42, 124, 221, 180));
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	pHit = (pData->tSelected.iPart != XUI_CHART_HIT_NONE) ? &pData->tSelected : &pData->tHover;
	if ( pHit->iPart != XUI_CHART_HIT_SERIES ) {
		return XUI_OK;
	}
	if ( !__xuiChartDrawablePoint(pHit->fX, pHit->fY) ) {
		return XUI_OK;
	}
	iRet = pProxy->drawCircleStroke(pProxy, pDraw, pHit->fX, pHit->fY, 7.0f, 2.0f, XUI_COLOR_RGBA(30, 40, 52, 220));
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bTooltipVisible && (pData->pFont != NULL) && (pHit->iSeries >= 0) && (pHit->iSeries < pData->iSeriesCount) &&
	     (pHit->iItem >= 0) && (pHit->iItem < pData->arrSeries[pHit->iSeries].iCount) ) {
		char sText[128];
		xui_chart_point_t* pPoint = &pData->arrSeries[pHit->iSeries].pPoints[pHit->iItem];
		xui_rect_t tTip = {pHit->fX + 10.0f, pHit->fY - 28.0f, 118.0f, 24.0f};
		sText[0] = 0;
		if ( pData->onTooltip != NULL ) {
			iRet = pData->onTooltip(pWidget, pHit->iSeries, pHit->iItem, sText, (int)sizeof(sText), pData->pTooltipUser);
			if ( iRet != XUI_OK ) {
				sText[0] = 0;
			}
		}
		if ( sText[0] == 0 ) {
			snprintf(sText, sizeof(sText), "%s %.2f", (pPoint->label != NULL) ? pPoint->label : pData->arrSeries[pHit->iSeries].sName, pPoint->value != 0.0 ? pPoint->value : pPoint->y);
		}
		tTip = xuiInternalSnapRect(tTip);
		if ( (tTip.fW <= 0.0f) || (tTip.fH <= 0.0f) || (tTip.fX != tTip.fX) || (tTip.fY != tTip.fY) ) {
			return XUI_OK;
		}
		iRet = pProxy->drawRectFill(pProxy, pDraw, tTip, pData->iTooltipColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, sText, tTip, pData->iTooltipTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiChartCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_chart_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tContent;
	uint32_t arrSavedColors[7];
	int i;
	int iRet;
	int bHasPie;

	(void)iStateId;
	(void)pUser;
	pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) || (pProxy->drawLine == NULL) ||
	     (pProxy->drawCircleFill == NULL) || (pProxy->drawTriangleFill == NULL) || (pProxy->drawText == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiChartSaveColors(pData, arrSavedColors);
	__xuiChartApplyStyleColors(pWidget, pData);
	tContent = __xuiChartContentRect(pWidget);
	__xuiChartComputeRanges(pData);
	pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
	pData->iLastLodStride = 1;
	iRet = pProxy->drawRectFill(pProxy, pDraw, tContent, pData->iBackgroundColor);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiChartDrawText(pProxy, pDraw, pData->pFont, pData->sTitle, (xui_rect_t){tContent.fX + 8.0f, tContent.fY + 4.0f, tContent.fW - 16.0f, 24.0f}, pData->iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	if ( iRet != XUI_OK ) goto cleanup;
	bHasPie = 0;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		if ( pData->arrSeries[i].bVisible && (pData->arrSeries[i].iType == XUI_CHART_SERIES_PIE) ) {
			bHasPie = 1;
			break;
		}
	}
	if ( !bHasPie ) {
		iRet = __xuiChartDrawAxes(pProxy, pDraw, pData);
		if ( iRet != XUI_OK ) goto cleanup;
	} else {
		iRet = pProxy->drawRectFill(pProxy, pDraw, pData->tPlotRect, pData->iPlotColor);
		if ( iRet != XUI_OK ) goto cleanup;
	}
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		xui_chart_series_t* pSeries = &pData->arrSeries[i];
		if ( !pSeries->bVisible ) {
			continue;
		}
		if ( pSeries->iType == XUI_CHART_SERIES_LINE ) {
			iRet = __xuiChartDrawLineArea(pProxy, pDraw, pData, pSeries);
			if ( iRet != XUI_OK ) return iRet;
			iRet = __xuiChartDrawLineSeries(pProxy, pDraw, pData, pSeries);
		} else if ( pSeries->iType == XUI_CHART_SERIES_BAR ) {
			iRet = __xuiChartDrawBarSeries(pProxy, pDraw, pData, i);
		} else if ( pSeries->iType == XUI_CHART_SERIES_SCATTER ) {
			iRet = __xuiChartDrawScatterSeries(pProxy, pDraw, pData, pSeries);
		} else {
			iRet = __xuiChartDrawPieSeries(pProxy, pDraw, pData, i);
		}
		if ( iRet != XUI_OK ) goto cleanup;
	}
	iRet = __xuiChartDrawLegend(pProxy, pDraw, pData, tContent);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiChartDrawSelection(pWidget, pProxy, pDraw, pData);
	if ( iRet == XUI_OK ) {
		pData->iDirtyFlags = 0;
	}
cleanup:
	__xuiChartRestoreColors(pData, arrSavedColors);
	return iRet;
}

static int __xuiChartContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 320.0f;
	pSize->fY = 220.0f;
	return XUI_OK;
}

static int __xuiChartHitLegend(xui_chart_data_t* pData, xui_rect_t tContent, float fX, float fY, xui_chart_hit_t* pHit)
{
	int i;
	float fLeft;
	float fTop;

	if ( !pData->bLegendVisible ) {
		return 0;
	}
	fLeft = tContent.fX + tContent.fW - 108.0f;
	fTop = tContent.fY + 36.0f;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		xui_rect_t tRect = {fLeft, fTop + (float)i * 20.0f, 106.0f, 18.0f};
		if ( (fX >= tRect.fX) && (fX <= tRect.fX + tRect.fW) && (fY >= tRect.fY) && (fY <= tRect.fY + tRect.fH) ) {
			if ( pHit != NULL ) {
				pHit->iPart = XUI_CHART_HIT_LEGEND;
				pHit->iSeries = i;
				pHit->iItem = -1;
				pHit->tRect = tRect;
				pHit->fX = fX;
				pHit->fY = fY;
			}
			return 1;
		}
	}
	return 0;
}

static int __xuiChartHitSeries(xui_chart_data_t* pData, float fX, float fY, xui_chart_hit_t* pHit)
{
	double fBest;
	int iBestSeries;
	int iBestItem;
	int i;
	int j;

	fBest = 144.0;
	iBestSeries = -1;
	iBestItem = -1;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		xui_chart_series_t* pSeries = &pData->arrSeries[i];
		if ( !pSeries->bVisible ) {
			continue;
		}
		if ( pSeries->iType == XUI_CHART_SERIES_PIE ) {
			double fTotal = 0.0;
			double fMaxValue;
			double fAngle = -XUI_CHART_PI * 0.5;
			double fPointAngle;
			float fCx = pData->tPlotRect.fX + pData->tPlotRect.fW * 0.5f;
			float fCy = pData->tPlotRect.fY + pData->tPlotRect.fH * 0.5f;
			float fRadius = ((pData->tPlotRect.fW < pData->tPlotRect.fH) ? pData->tPlotRect.fW : pData->tPlotRect.fH) * 0.42f;
			float fInnerRadius;
			float fOuterRadius;
			float fHitRadius;
			double dx = (double)fX - (double)fCx;
			double dy = (double)fY - (double)fCy;
			double dist = sqrt(dx * dx + dy * dy);
			__xuiChartPieRing(pData, i, fRadius, &fInnerRadius, &fOuterRadius);
			fMaxValue = __xuiChartPieMaxValue(pSeries);
			if ( (dist > (double)fOuterRadius) || (dist < (double)fInnerRadius) ) {
				continue;
			}
			fPointAngle = atan2(dy, dx);
			if ( fPointAngle < -XUI_CHART_PI * 0.5 ) {
				fPointAngle += XUI_CHART_PI * 2.0;
			}
			for ( j = 0; j < pSeries->iCount; j++ ) {
				if ( pSeries->pPoints[j].value > 0.0 ) fTotal += pSeries->pPoints[j].value;
			}
			for ( j = 0; j < pSeries->iCount; j++ ) {
				double fNext;
				float fSliceOuterRadius;
				if ( pSeries->pPoints[j].value <= 0.0 ) continue;
				if ( pData->iPieMode == XUI_CHART_PIE_ROSE ) {
					if ( fMaxValue <= 0.0 ) break;
					fAngle = -XUI_CHART_PI * 0.5 + XUI_CHART_PI * 2.0 * (double)j / (double)pSeries->iCount;
					fNext = -XUI_CHART_PI * 0.5 + XUI_CHART_PI * 2.0 * (double)(j + 1) / (double)pSeries->iCount;
					fSliceOuterRadius = fInnerRadius + (fOuterRadius - fInnerRadius) * (float)(pSeries->pPoints[j].value / fMaxValue);
				} else {
					if ( fTotal <= 0.0 ) break;
					fNext = fAngle + (pSeries->pPoints[j].value / fTotal) * XUI_CHART_PI * 2.0;
					fSliceOuterRadius = fOuterRadius;
				}
				if ( (fPointAngle >= fAngle) && (fPointAngle <= fNext) ) {
					if ( dist > (double)fSliceOuterRadius ) {
						if ( pData->iPieMode == XUI_CHART_PIE_NORMAL ) fAngle = fNext;
						continue;
					}
					if ( pHit != NULL ) {
						fHitRadius = (fInnerRadius + fSliceOuterRadius) * 0.5f;
						pHit->iPart = XUI_CHART_HIT_SERIES;
						pHit->iSeries = i;
						pHit->iItem = j;
						pHit->fX = fCx + (float)cos((fAngle + fNext) * 0.5) * fHitRadius;
						pHit->fY = fCy + (float)sin((fAngle + fNext) * 0.5) * fHitRadius;
						pHit->tRect = (xui_rect_t){pHit->fX - 5.0f, pHit->fY - 5.0f, 10.0f, 10.0f};
					}
					return 1;
				}
				if ( pData->iPieMode == XUI_CHART_PIE_NORMAL ) {
					fAngle = fNext;
				}
			}
			continue;
		}
		for ( j = 0; j < pSeries->iCount; j++ ) {
			double x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)j : pSeries->pPoints[j].x;
			float px = __xuiChartMapX(pData, x);
			float py = __xuiChartMapY(pData, pSeries->pPoints[j].y);
			double dx = (double)fX - (double)px;
			double dy = (double)fY - (double)py;
			double d2 = dx * dx + dy * dy;
			if ( (pSeries->iType == XUI_CHART_SERIES_BAR) ) {
				float fSlot = ((pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL) ? pData->tPlotRect.fH : pData->tPlotRect.fW) / (float)((pSeries->iCount > 1) ? pSeries->iCount : 1);
				float fBaseY = __xuiChartMapY(pData, 0.0);
				float fBaseX = __xuiChartMapX(pData, 0.0);
				float fBarW = fSlot * 0.72f / (float)((__xuiChartVisibleBarSeries(pData) > 0) ? __xuiChartVisibleBarSeries(pData) : 1);
				float bx = px - (fSlot * 0.36f) + fBarW * (float)__xuiChartBarVisibleIndex(pData, i);
				float by = py < fBaseY ? py : fBaseY;
				float bh = (py < fBaseY) ? (fBaseY - py) : (py - fBaseY);
				float bw = fBarW;
				if ( pData->iBarDirection == XUI_CHART_BAR_HORIZONTAL ) {
					float fValueX = __xuiChartMapX(pData, pSeries->pPoints[j].y);
					float fItemY = __xuiChartMapY(pData, (double)j) - (fSlot * 0.36f) + fBarW * (float)__xuiChartBarVisibleIndex(pData, i);
					if ( pData->iBarMode == XUI_CHART_BAR_STACKED ) {
						double fStart;
						double fEnd;
						float fStartX;
						float fEndX;
						fBarW = fSlot * 0.72f;
						fItemY = __xuiChartMapY(pData, (double)j) - (fSlot * 0.36f);
						__xuiChartStackedBarRange(pData, i, j, &fStart, &fEnd);
						fStartX = __xuiChartMapX(pData, fStart);
						fEndX = __xuiChartMapX(pData, fEnd);
						bx = fEndX < fStartX ? fEndX : fStartX;
						bw = (fEndX < fStartX) ? (fStartX - fEndX) : (fEndX - fStartX);
					} else if ( fValueX >= fBaseX ) {
						bx = fBaseX;
						bw = fValueX - fBaseX;
					} else {
						bx = fValueX;
						bw = fBaseX - fValueX;
					}
					by = fItemY;
					bh = fBarW;
				}
				if ( (pData->iBarDirection == XUI_CHART_BAR_VERTICAL) && (pData->iBarMode == XUI_CHART_BAR_STACKED) ) {
					double fStart;
					double fEnd;
					float fStartY;
					float fEndY;
					fBarW = fSlot * 0.72f;
					bx = px - (fSlot * 0.36f);
					__xuiChartStackedBarRange(pData, i, j, &fStart, &fEnd);
					fStartY = __xuiChartMapY(pData, fStart);
					fEndY = __xuiChartMapY(pData, fEnd);
					by = fEndY < fStartY ? fEndY : fStartY;
					bh = (fEndY < fStartY) ? (fStartY - fEndY) : (fEndY - fStartY);
				}
				if ( (fX >= bx) && (fX <= bx + bw) && (fY >= by) && (fY <= by + bh) ) {
					fBest = 0.0;
					iBestSeries = i;
					iBestItem = j;
				}
			} else if ( d2 < fBest ) {
				fBest = d2;
				iBestSeries = i;
				iBestItem = j;
			}
		}
	}
	if ( iBestSeries >= 0 ) {
		xui_chart_series_t* pSeries = &pData->arrSeries[iBestSeries];
		double x = (pData->iXAxisType == XUI_CHART_AXIS_CATEGORY) ? (double)iBestItem : pSeries->pPoints[iBestItem].x;
		float px = __xuiChartMapX(pData, x);
		float py = __xuiChartMapY(pData, pSeries->pPoints[iBestItem].y);
		if ( pHit != NULL ) {
			pHit->iPart = XUI_CHART_HIT_SERIES;
			pHit->iSeries = iBestSeries;
			pHit->iItem = iBestItem;
			pHit->fX = px;
			pHit->fY = py;
			pHit->tRect = (xui_rect_t){px - 6.0f, py - 6.0f, 12.0f, 12.0f};
		}
		return 1;
	}
	return 0;
}

static int __xuiChartEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_chart_data_t* pData;
	xui_chart_hit_t tHit;

	(void)pUser;
	pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tHit, 0, sizeof(tHit));
	tHit.iSize = sizeof(tHit);
	if ( pEvent->iType == XUI_EVENT_POINTER_CAPTURE_LOST ) {
		pData->bPanning = 0;
		pData->bBrushing = 0;
		return XUI_OK;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_LEAVE ) {
		if ( !pData->bPanning ) {
			memset(&pData->tHover, 0, sizeof(pData->tHover));
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_OK;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_WHEEL ) {
		double fAnchorX;
		double fAnchorY;
		double fFactor;
		double fMinX;
		double fMaxX;
		double fMinY;
		double fMaxY;
		if ( __xuiChartHasVisiblePie(pData) ) return XUI_OK;
		__xuiChartComputeRanges(pData);
		pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
		if ( !__xuiChartPointInRect(pData->tPlotRect, pEvent->fX, pEvent->fY) || (pEvent->fWheelY == 0.0f) ) return XUI_OK;
		fAnchorX = __xuiChartUnmapX(pData, pEvent->fX);
		fAnchorY = __xuiChartUnmapY(pData, pEvent->fY);
		fFactor = (pEvent->fWheelY > 0.0f) ? 0.8 : 1.25;
		fMinX = fAnchorX - (fAnchorX - pData->fMinX) * fFactor;
		fMaxX = fAnchorX + (pData->fMaxX - fAnchorX) * fFactor;
		fMinY = fAnchorY - (fAnchorY - pData->fMinY) * fFactor;
		fMaxY = fAnchorY + (pData->fMaxY - fAnchorY) * fFactor;
		(void)__xuiChartSetViewRangeData(pData, fMinX, fMaxX, fMinY, fMaxY);
		(void)__xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_DOWN ) {
		if ( __xuiChartHasVisiblePie(pData) ) return XUI_OK;
		__xuiChartComputeRanges(pData);
		pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
		if ( !__xuiChartPointInRect(pData->tPlotRect, pEvent->fX, pEvent->fY) ) return XUI_OK;
		if ( (pEvent->iModifiers & XUI_MOD_SHIFT) != 0 ) {
			pData->bBrushing = 1;
			pData->fBrushStartX = pEvent->fX;
			pData->fBrushStartY = pEvent->fY;
			pData->fBrushCurrentX = pEvent->fX;
			pData->fBrushCurrentY = pEvent->fY;
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		pData->bPanning = 1;
		pData->fPanStartX = pEvent->fX;
		pData->fPanStartY = pEvent->fY;
		pData->fPanMinX = pData->fMinX;
		pData->fPanMaxX = pData->fMaxX;
		pData->fPanMinY = pData->fMinY;
		pData->fPanMaxY = pData->fMaxY;
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_UP ) {
		if ( pData->bBrushing ) {
			double fDataX0;
			double fDataY0;
			double fDataX1;
			double fDataY1;
			pData->bBrushing = 0;
			pData->fBrushCurrentX = pEvent->fX;
			pData->fBrushCurrentY = pEvent->fY;
			fDataX0 = __xuiChartUnmapX(pData, pData->fBrushStartX);
			fDataY0 = __xuiChartUnmapY(pData, pData->fBrushStartY);
			fDataX1 = __xuiChartUnmapX(pData, pData->fBrushCurrentX);
			fDataY1 = __xuiChartUnmapY(pData, pData->fBrushCurrentY);
			if ( ((pData->fBrushStartX - pData->fBrushCurrentX) * (pData->fBrushStartX - pData->fBrushCurrentX) +
			      (pData->fBrushStartY - pData->fBrushCurrentY) * (pData->fBrushStartY - pData->fBrushCurrentY)) >= 16.0f ) {
				(void)__xuiChartSetBrushRangeData(pData, fDataX0, fDataX1, fDataY0, fDataY1);
			}
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			(void)__xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bPanning ) {
			pData->bPanning = 0;
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_MOVE) && pData->bBrushing ) {
		double fDataX0;
		double fDataY0;
		double fDataX1;
		double fDataY1;
		pData->fBrushCurrentX = pEvent->fX;
		pData->fBrushCurrentY = pEvent->fY;
		fDataX0 = __xuiChartUnmapX(pData, pData->fBrushStartX);
		fDataY0 = __xuiChartUnmapY(pData, pData->fBrushStartY);
		fDataX1 = __xuiChartUnmapX(pData, pData->fBrushCurrentX);
		fDataY1 = __xuiChartUnmapY(pData, pData->fBrushCurrentY);
		(void)__xuiChartSetBrushRangeData(pData, fDataX0, fDataX1, fDataY0, fDataY1);
		(void)__xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_MOVE) && pData->bPanning ) {
		double fRangeX;
		double fRangeY;
		double fDx;
		double fDy;
		fRangeX = pData->fPanMaxX - pData->fPanMinX;
		fRangeY = pData->fPanMaxY - pData->fPanMinY;
		if ( (pData->tPlotRect.fW > 0.0f) && (pData->tPlotRect.fH > 0.0f) ) {
			fDx = -((double)pEvent->fX - (double)pData->fPanStartX) / (double)pData->tPlotRect.fW * fRangeX;
			fDy = ((double)pEvent->fY - (double)pData->fPanStartY) / (double)pData->tPlotRect.fH * fRangeY;
			(void)__xuiChartSetViewRangeData(pData, pData->fPanMinX + fDx, pData->fPanMaxX + fDx, pData->fPanMinY + fDy, pData->fPanMaxY + fDy);
			(void)__xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_MOVE) || (pEvent->iType == XUI_EVENT_POINTER_CLICK) ) {
		(void)xuiChartHitTest(pWidget, pEvent->fX, pEvent->fY, &tHit);
		if ( pEvent->iType == XUI_EVENT_POINTER_CLICK ) {
			if ( tHit.iPart == XUI_CHART_HIT_LEGEND ) {
				(void)xuiChartSetSeriesVisible(pWidget, tHit.iSeries, !pData->arrSeries[tHit.iSeries].bVisible);
				return XUI_EVENT_DISPATCH_STOP;
			}
			pData->tSelected = tHit;
		} else {
			pData->tHover = tHit;
		}
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_OK;
	}
	return XUI_OK;
}

static int __xuiChartInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_chart_data_t* pData;
	const xui_chart_desc_t* pDesc;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_chart_data_t*)pTypeData;
	pDesc = (const xui_chart_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->sTitle = __xuiChartCopyString((pDesc != NULL) ? pDesc->sTitle : NULL);
	if ( (pDesc != NULL) && (pDesc->sTitle != NULL) && (pData->sTitle == NULL) ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->iXAxisType = XUI_CHART_AXIS_CATEGORY;
	pData->iYAxisType = XUI_CHART_AXIS_VALUE;
	pData->iBarMode = XUI_CHART_BAR_GROUPED;
	pData->iBarDirection = XUI_CHART_BAR_VERTICAL;
	pData->iPieMode = XUI_CHART_PIE_NORMAL;
	pData->bLegendVisible = 1;
	pData->bTooltipVisible = 1;
	pData->tPadding = (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->iDirtyFlags = XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY;
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(248, 250, 252, 255);
	pData->iPlotColor = (pDesc != NULL && pDesc->iPlotColor != 0) ? pDesc->iPlotColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iGridColor = (pDesc != NULL && pDesc->iGridColor != 0) ? pDesc->iGridColor : XUI_COLOR_RGBA(218, 226, 236, 255);
	pData->iAxisColor = (pDesc != NULL && pDesc->iAxisColor != 0) ? pDesc->iAxisColor : XUI_COLOR_RGBA(108, 122, 140, 255);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(38, 48, 62, 255);
	pData->iTooltipColor = (pDesc != NULL && pDesc->iTooltipColor != 0) ? pDesc->iTooltipColor : XUI_COLOR_RGBA(25, 35, 48, 230);
	pData->iTooltipTextColor = (pDesc != NULL && pDesc->iTooltipTextColor != 0) ? pDesc->iTooltipTextColor : XUI_COLOR_WHITE;
	pData->fPieInnerRadius = 0.0f;
	pData->iLodThreshold = 0;
	pData->iLastLodStride = 1;
	pData->bAnimationEnabled = 0;
	pData->fAnimationDuration = 0.25f;
	pData->fAnimationProgress = 1.0f;
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiChartEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiChartEvent, NULL);
	return XUI_OK;
}

static void __xuiChartDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_chart_data_t* pData;
	int i;

	(void)pWidget;
	(void)pUser;
	pData = (xui_chart_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		__xuiChartFreeSeries(&pData->arrSeries[i]);
	}
	if ( pData->sTitle != NULL ) {
		xrtFree(pData->sTitle);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiChartDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 320.0f;
	pLayout->fPreferredHeight = 220.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiChartDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiChartRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) {
		return;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiChartRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.plot.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.grid.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.axis.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.tooltip.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiChartRegisterStyleProperty(pContext, pType, "chart.tooltip.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiChartGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "chart");
	if ( pType != NULL ) {
		__xuiChartRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "chart";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_chart_data_t);
	tDesc.onInit = __xuiChartInit;
	tDesc.onDestroy = __xuiChartDestroy;
	tDesc.onContentMeasure = __xuiChartContentMeasure;
	tDesc.onCacheRender = __xuiChartCacheRender;
	__xuiChartDefaultLayout(&tDesc.tLayout);
	__xuiChartDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiChartRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiChartCreate(xui_context pContext, xui_widget* ppWidget, const xui_chart_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( ppWidget == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	if ( (pDesc != NULL) && (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = xuiChartGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiChartSetTitle(xui_widget pWidget, const char* sTitle)
{
	xui_chart_data_t* pData;
	char* sNew;

	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sNew = __xuiChartCopyString(sTitle);
	if ( (sTitle != NULL) && (sNew == NULL) ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( pData->sTitle != NULL ) xrtFree(pData->sTitle);
	pData->sTitle = sNew;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiChartGetTitle(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->sTitle : NULL;
}

XUI_API int xuiChartSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiChartGetFont(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiChartSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iPlot, uint32_t iGrid, uint32_t iAxis, uint32_t iText, uint32_t iTooltip, uint32_t iTooltipText)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iPlotColor = iPlot;
	pData->iGridColor = iGrid;
	pData->iAxisColor = iAxis;
	pData->iTextColor = iText;
	pData->iTooltipColor = iTooltip;
	pData->iTooltipTextColor = iTooltipText;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pPlot, uint32_t* pGrid, uint32_t* pAxis, uint32_t* pText, uint32_t* pTooltip, uint32_t* pTooltipText)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pPlot != NULL ) *pPlot = pData->iPlotColor;
	if ( pGrid != NULL ) *pGrid = pData->iGridColor;
	if ( pAxis != NULL ) *pAxis = pData->iAxisColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pTooltip != NULL ) *pTooltip = pData->iTooltipColor;
	if ( pTooltipText != NULL ) *pTooltipText = pData->iTooltipTextColor;
	return XUI_OK;
}

XUI_API int xuiChartSetXAxis(xui_widget pWidget, int iAxisType)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartAxisTypeValid(iAxisType) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iXAxisType = iAxisType;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartSetYAxis(xui_widget pWidget, int iAxisType)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartAxisTypeValid(iAxisType) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iYAxisType = iAxisType;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetXAxis(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iXAxisType : 0;
}

XUI_API int xuiChartGetYAxis(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iYAxisType : 0;
}

XUI_API int xuiChartSetBarMode(xui_widget pWidget, int iMode)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartBarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBarMode = iMode;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetBarMode(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iBarMode : 0;
}

XUI_API int xuiChartSetBarDirection(xui_widget pWidget, int iDirection)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartBarDirectionValid(iDirection) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBarDirection = iDirection;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetBarDirection(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iBarDirection : 0;
}

XUI_API int xuiChartSetLegendVisible(xui_widget pWidget, int bVisible)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bLegendVisible = bVisible ? 1 : 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetLegendVisible(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->bLegendVisible : 0;
}

XUI_API int xuiChartSetTooltipVisible(xui_widget pWidget, int bVisible)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bTooltipVisible = bVisible ? 1 : 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetTooltipVisible(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->bTooltipVisible : 0;
}

XUI_API int xuiChartSetTooltipCallback(xui_widget pWidget, xui_chart_tooltip_proc onTooltip, void* pUser)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onTooltip = onTooltip;
	pData->pTooltipUser = pUser;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartSetPadding(xui_widget pWidget, xui_thickness_t tPadding)
{
	xui_chart_data_t* pData;

	if ( (tPadding.fLeft < 0.0f) || (tPadding.fTop < 0.0f) || (tPadding.fRight < 0.0f) || (tPadding.fBottom < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tPadding = tPadding;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_thickness_t xuiChartGetPadding(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_thickness_t tPadding;

	memset(&tPadding, 0, sizeof(tPadding));
	return (pData != NULL) ? pData->tPadding : tPadding;
}

XUI_API uint32_t xuiChartGetDirtyFlags(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iDirtyFlags : 0u;
}

XUI_API int xuiChartSetViewRange(xui_widget pWidget, double fMinX, double fMaxX, double fMinY, double fMaxY)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiChartSetViewRangeData(pData, fMinX, fMaxX, fMinY, fMaxY);
	if ( iRet != XUI_OK ) return iRet;
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetViewRange(xui_widget pWidget, double* pMinX, double* pMaxX, double* pMinY, double* pMaxY)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiChartComputeRanges(pData);
	if ( pMinX != NULL ) *pMinX = pData->fMinX;
	if ( pMaxX != NULL ) *pMaxX = pData->fMaxX;
	if ( pMinY != NULL ) *pMinY = pData->fMinY;
	if ( pMaxY != NULL ) *pMaxY = pData->fMaxY;
	return pData->bViewRange ? 1 : 0;
}

XUI_API int xuiChartResetViewRange(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bViewRange = 0;
	pData->bPanning = 0;
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartSetBrushRange(xui_widget pWidget, double fMinX, double fMaxX, double fMinY, double fMaxY)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiChartSetBrushRangeData(pData, fMinX, fMaxX, fMinY, fMaxY);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetBrushRange(xui_widget pWidget, double* pMinX, double* pMaxX, double* pMinY, double* pMaxY)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMinX != NULL ) *pMinX = pData->fBrushMinX;
	if ( pMaxX != NULL ) *pMaxX = pData->fBrushMaxX;
	if ( pMinY != NULL ) *pMinY = pData->fBrushMinY;
	if ( pMaxY != NULL ) *pMaxY = pData->fBrushMaxY;
	return pData->bBrushRange ? 1 : 0;
}

XUI_API int xuiChartClearBrushRange(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bBrushRange = 0;
	pData->bBrushing = 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartSetLodThreshold(xui_widget pWidget, int iThreshold)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iThreshold < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iLodThreshold = iThreshold;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetLodThreshold(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iLodThreshold : 0;
}

XUI_API int xuiChartGetLastLodStride(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iLastLodStride : 0;
}

XUI_API int xuiChartSetAnimation(xui_widget pWidget, int bEnabled, float fDuration)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (fDuration < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bAnimationEnabled = bEnabled ? 1 : 0;
	pData->fAnimationDuration = fDuration;
	if ( !pData->bAnimationEnabled || (pData->fAnimationDuration == 0.0f) ) {
		pData->bAnimationActive = 0;
		pData->fAnimationProgress = 1.0f;
	} else {
		__xuiChartStartAnimation(pData);
	}
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetAnimation(xui_widget pWidget, int* pEnabled, float* pDuration, float* pProgress)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEnabled != NULL ) *pEnabled = pData->bAnimationEnabled;
	if ( pDuration != NULL ) *pDuration = pData->fAnimationDuration;
	if ( pProgress != NULL ) *pProgress = pData->fAnimationProgress;
	return pData->bAnimationActive ? 1 : 0;
}

XUI_API int xuiChartStepAnimation(xui_widget pWidget, float fDeltaSeconds)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (fDeltaSeconds < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pData->bAnimationEnabled || !pData->bAnimationActive ) return XUI_OK;
	if ( pData->fAnimationDuration <= 0.0f ) {
		pData->fAnimationProgress = 1.0f;
		pData->bAnimationActive = 0;
	} else {
		pData->fAnimationProgress += fDeltaSeconds / pData->fAnimationDuration;
		if ( pData->fAnimationProgress >= 1.0f ) {
			pData->fAnimationProgress = 1.0f;
			pData->bAnimationActive = 0;
		}
	}
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartAddSeries(xui_widget pWidget, int iType, const char* sName, int* pIndex)
{
	xui_chart_data_t* pData;
	xui_chart_series_t* pSeries;

	if ( !__xuiChartSeriesTypeValid(iType) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSeriesCount >= XUI_CHART_MAX_SERIES ) return XUI_ERROR_OUT_OF_MEMORY;
	pSeries = &pData->arrSeries[pData->iSeriesCount];
	memset(pSeries, 0, sizeof(*pSeries));
	pSeries->iType = iType;
	pSeries->bVisible = 1;
	pSeries->iSymbol = XUI_CHART_SYMBOL_CIRCLE;
	pSeries->fSymbolSize = (iType == XUI_CHART_SERIES_SCATTER) ? 8.0f : 6.0f;
	pSeries->iColor = __xuiChartPalette(pData->iSeriesCount);
	pSeries->iAreaColor = __xuiChartColorWithAlphaScale(pSeries->iColor, 0.18f);
	__xuiChartSetFixedString(pSeries->sName, XUI_CHART_NAME_CAPACITY, sName != NULL ? sName : "Series");
	if ( pIndex != NULL ) *pIndex = pData->iSeriesCount;
	pData->iSeriesCount++;
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartClearSeries(xui_widget pWidget)
{
	xui_chart_data_t* pData;
	int i;

	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iSeriesCount; i++ ) {
		__xuiChartFreeSeries(&pData->arrSeries[i]);
	}
	pData->iSeriesCount = 0;
	memset(&pData->tHover, 0, sizeof(pData->tHover));
	memset(&pData->tSelected, 0, sizeof(pData->tSelected));
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesCount(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iSeriesCount : 0;
}

XUI_API int xuiChartGetSeriesType(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return -1;
	return pData->arrSeries[iSeries].iType;
}

XUI_API int xuiChartSetSeriesData(xui_widget pWidget, int iSeries, const xui_chart_point_t* pPoints, int iCount)
{
	xui_chart_data_t* pData;
	xui_chart_point_t* pNew;

	if ( (iCount < 0) || ((iCount > 0) && (pPoints == NULL)) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pNew = NULL;
	if ( iCount > 0 ) {
		pNew = (xui_chart_point_t*)xrtMalloc(sizeof(xui_chart_point_t) * (size_t)iCount);
		if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		memcpy(pNew, pPoints, sizeof(xui_chart_point_t) * (size_t)iCount);
	}
	if ( pData->arrSeries[iSeries].pPoints != NULL ) xrtFree(pData->arrSeries[iSeries].pPoints);
	pData->arrSeries[iSeries].pPoints = pNew;
	pData->arrSeries[iSeries].iCount = iCount;
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartSetSeriesColor(xui_widget pWidget, int iSeries, uint32_t iColor)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSeries[iSeries].iColor = iColor;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiChartGetSeriesColor(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	return pData->arrSeries[iSeries].iColor;
}

XUI_API int xuiChartSetSeriesAreaFill(xui_widget pWidget, int iSeries, int bEnabled, uint32_t iColor)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bAreaFill = bEnabled ? 1 : 0;
	pSeries->iAreaColor = (iColor != 0u) ? iColor : __xuiChartColorWithAlphaScale(pSeries->iColor, 0.18f);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesAreaFill(xui_widget pWidget, int iSeries, uint32_t* pColor)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return 0;
	if ( pColor != NULL ) {
		*pColor = pSeries->iAreaColor;
	}
	return pSeries->bAreaFill ? 1 : 0;
}

XUI_API int xuiChartSetSeriesSmooth(xui_widget pWidget, int iSeries, int bSmooth)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bSmooth = bSmooth ? 1 : 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesSmooth(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	if ( pData->arrSeries[iSeries].iType != XUI_CHART_SERIES_LINE ) return 0;
	return pData->arrSeries[iSeries].bSmooth ? 1 : 0;
}

XUI_API int xuiChartSetSeriesDash(xui_widget pWidget, int iSeries, const float* pDashPattern, int iDashCount)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	int i;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ||
	     (pDashPattern == NULL) || (iDashCount <= 0) || (iDashCount > XUI_CHART_DASH_CAPACITY) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return XUI_ERROR_UNSUPPORTED;
	for ( i = 0; i < iDashCount; i++ ) {
		if ( pDashPattern[i] <= 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
		pSeries->arrDash[i] = pDashPattern[i];
	}
	pSeries->iDashCount = iDashCount;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartClearSeriesDash(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return XUI_ERROR_UNSUPPORTED;
	pSeries->iDashCount = 0;
	memset(pSeries->arrDash, 0, sizeof(pSeries->arrDash));
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesDash(xui_widget pWidget, int iSeries, float* pDashPattern, int iCapacity)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	int i;

	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_LINE ) return 0;
	if ( (pDashPattern != NULL) && (iCapacity > 0) ) {
		for ( i = 0; (i < pSeries->iDashCount) && (i < iCapacity); i++ ) {
			pDashPattern[i] = pSeries->arrDash[i];
		}
	}
	return pSeries->iDashCount;
}

XUI_API int xuiChartSetSeriesVisible(xui_widget pWidget, int iSeries, int bVisible)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSeries[iSeries].bVisible = bVisible ? 1 : 0;
	__xuiChartStartAnimation(pData);
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_STATIC | XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesVisible(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	return pData->arrSeries[iSeries].bVisible;
}

XUI_API int xuiChartSetSeriesSymbol(xui_widget pWidget, int iSeries, int iSymbol)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartSymbolValid(iSymbol) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSeries[iSeries].iSymbol = iSymbol;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesSymbol(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0;
	return pData->arrSeries[iSeries].iSymbol;
}

XUI_API int xuiChartSetSeriesSymbolSize(xui_widget pWidget, int iSeries, float fSize)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) || (fSize <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSeries[iSeries].fSymbolSize = fSize;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiChartGetSeriesSymbolSize(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return 0.0f;
	return pData->arrSeries[iSeries].fSymbolSize;
}

XUI_API int xuiChartSetSeriesValueRadius(xui_widget pWidget, int iSeries, float fMinSize, float fMaxSize)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) || (fMinSize <= 0.0f) || (fMaxSize <= 0.0f) || (fMaxSize < fMinSize) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bValueRadius = 1;
	pSeries->fValueRadiusMin = fMinSize;
	pSeries->fValueRadiusMax = fMaxSize;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartClearSeriesValueRadius(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bValueRadius = 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesValueRadius(xui_widget pWidget, int iSeries, float* pMinSize, float* pMaxSize)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	if ( pMinSize != NULL ) *pMinSize = pSeries->fValueRadiusMin;
	if ( pMaxSize != NULL ) *pMaxSize = pSeries->fValueRadiusMax;
	return pSeries->bValueRadius ? 1 : 0;
}

XUI_API int xuiChartSetSeriesValueColor(xui_widget pWidget, int iSeries, uint32_t iMinColor, uint32_t iMaxColor)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bValueColor = 1;
	pSeries->iValueColorMin = iMinColor;
	pSeries->iValueColorMax = iMaxColor;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartClearSeriesValueColor(xui_widget pWidget, int iSeries)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	pSeries->bValueColor = 0;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetSeriesValueColor(xui_widget pWidget, int iSeries, uint32_t* pMinColor, uint32_t* pMaxColor)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_chart_series_t* pSeries;
	if ( (pData == NULL) || (iSeries < 0) || (iSeries >= pData->iSeriesCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pSeries = &pData->arrSeries[iSeries];
	if ( pSeries->iType != XUI_CHART_SERIES_SCATTER ) return XUI_ERROR_UNSUPPORTED;
	if ( pMinColor != NULL ) *pMinColor = pSeries->iValueColorMin;
	if ( pMaxColor != NULL ) *pMaxColor = pSeries->iValueColorMax;
	return pSeries->bValueColor ? 1 : 0;
}

XUI_API int xuiChartSetPieMode(xui_widget pWidget, int iMode)
{
	xui_chart_data_t* pData;
	if ( !__xuiChartPieModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPieMode = iMode;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiChartGetPieMode(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->iPieMode : 0;
}

XUI_API int xuiChartSetPieInnerRadius(xui_widget pWidget, float fRate)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	if ( (pData == NULL) || (fRate < 0.0f) || (fRate >= 1.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPieInnerRadius = fRate;
	return __xuiChartInvalidate(pWidget, pData, XUI_CHART_DIRTY_PLOT | XUI_CHART_DIRTY_OVERLAY, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiChartGetPieInnerRadius(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	return (pData != NULL) ? pData->fPieInnerRadius : 0.0f;
}

XUI_API xui_rect_t xuiChartGetPlotRect(xui_widget pWidget)
{
	xui_chart_data_t* pData = __xuiChartGetData(pWidget);
	xui_rect_t tRect;
	memset(&tRect, 0, sizeof(tRect));
	if ( pData == NULL ) return tRect;
	__xuiChartComputeRanges(pData);
	pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
	return pData->tPlotRect;
}

XUI_API int xuiChartDataToPixel(xui_widget pWidget, double fX, double fY, xui_vec2_t* pPoint)
{
	xui_chart_data_t* pData;

	if ( pPoint == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiChartComputeRanges(pData);
	pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
	pPoint->fX = __xuiChartMapX(pData, fX);
	pPoint->fY = __xuiChartMapY(pData, fY);
	return XUI_OK;
}

XUI_API int xuiChartPixelToData(xui_widget pWidget, float fX, float fY, double* pDataX, double* pDataY)
{
	xui_chart_data_t* pData;

	if ( (pDataX == NULL) || (pDataY == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiChartComputeRanges(pData);
	pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
	*pDataX = __xuiChartUnmapX(pData, fX);
	*pDataY = __xuiChartUnmapY(pData, fY);
	return XUI_OK;
}

XUI_API int xuiChartHitTest(xui_widget pWidget, float fX, float fY, xui_chart_hit_t* pHit)
{
	xui_chart_data_t* pData;
	xui_rect_t tContent;

	pData = __xuiChartGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pHit != NULL ) {
		memset(pHit, 0, sizeof(*pHit));
		pHit->iSize = sizeof(*pHit);
	}
	__xuiChartComputeRanges(pData);
	pData->tPlotRect = __xuiChartComputePlotRect(pWidget, pData);
	tContent = __xuiChartContentRect(pWidget);
	if ( __xuiChartHitLegend(pData, tContent, fX, fY, pHit) ) return XUI_OK;
	if ( __xuiChartHitSeries(pData, fX, fY, pHit) ) return XUI_OK;
	return XUI_OK;
}
