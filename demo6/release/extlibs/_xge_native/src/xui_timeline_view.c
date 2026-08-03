#include "xui_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define XUI_TIMELINE_DEFAULT_FRAME_COUNT 120
#define XUI_TIMELINE_DEFAULT_FRAME_RATE 24.0f
#define XUI_TIMELINE_DEFAULT_LAYER_WIDTH 160.0f
#define XUI_TIMELINE_DEFAULT_FRAME_WIDTH 12.0f
#define XUI_TIMELINE_DEFAULT_MIN_FRAME_WIDTH 6.0f
#define XUI_TIMELINE_DEFAULT_MAX_FRAME_WIDTH 40.0f
#define XUI_TIMELINE_DEFAULT_ROW_HEIGHT 22.0f
#define XUI_TIMELINE_DEFAULT_RULER_HEIGHT 24.0f
#define XUI_TIMELINE_MENU_TITLE_COUNT 14

typedef struct xui_timeline_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_widget pLayerMenu;
	xui_widget pFrameMenu;
	char* arrMenuTitle[XUI_TIMELINE_MENU_TITLE_COUNT];
	xui_font pFont;
	xui_timeline_layer_t arrLayers[XUI_TIMELINE_LAYER_CAPACITY];
	xui_timeline_frame_t arrFrames[XUI_TIMELINE_FRAME_CAPACITY];
	xui_timeline_span_t arrSpans[XUI_TIMELINE_SPAN_CAPACITY];
	xui_timeline_selection_t arrSelection[XUI_TIMELINE_SELECTION_CAPACITY];
	xui_timeline_selection_t arrDragBaseSelection[XUI_TIMELINE_SELECTION_CAPACITY];
	int iLayerCount;
	int iFrameRecordCount;
	int iSpanCount;
	int iSelectionCount;
	int iDragBaseSelectionCount;
	int iNextLayerId;
	int iNextSpanId;
	int iTotalFrames;
	float fFrameRate;
	int iCurrentFrame;
	int iActiveLayer;
	int iAnchorLayer;
	int iAnchorFrame;
	int iHoverLayer;
	int iHoverFrame;
	int iHoverSpanId;
	int bDraggingPlayhead;
	int bDraggingSelection;
	int bDragMoved;
	int bDragSelectingState;
	int iDragLayer;
	int iDragFrame;
	uint32_t iDragModifiers;
	float fDragStartX;
	float fDragStartY;
	xui_timeline_hit_t tHoverHit;
	xui_timeline_hit_t tContextHit;
	float fLayerHeaderWidth;
	float fFrameWidth;
	float fMinFrameWidth;
	float fMaxFrameWidth;
	float fRowHeight;
	float fRulerHeight;
	int iScrollbarMode;
	int bShowVisibilityFeature;
	int bShowLockFeature;
	xui_timeline_view_colors_t tColors;
	xui_timeline_current_frame_changing_proc onCurrentChanging;
	xui_timeline_current_frame_changed_proc onCurrentChanged;
	void* pCurrentUser;
	xui_timeline_layer_changing_proc onLayerChanging;
	xui_timeline_layer_changed_proc onLayerChanged;
	void* pLayerUser;
	xui_timeline_frame_changing_proc onFrameChanging;
	xui_timeline_frame_changed_proc onFrameChanged;
	void* pFrameUser;
	xui_timeline_span_changing_proc onSpanChanging;
	xui_timeline_span_changed_proc onSpanChanged;
	void* pSpanUser;
	xui_timeline_layer_selected_proc onLayerSelected;
	void* pLayerSelectedUser;
	xui_timeline_context_opening_proc onContextOpening;
	xui_timeline_context_command_proc onContextCommand;
	void* pContextUser;
	xui_timeline_frame_click_proc onFrameClick;
	xui_timeline_frame_click_proc onFrameDoubleClick;
	void* pFrameClickUser;
	xui_timeline_selection_proc onSelection;
	void* pSelectionUser;
	xui_timeline_layer_renderer_proc onLayerRender;
	xui_timeline_ruler_renderer_proc onRulerRender;
	xui_timeline_frame_renderer_proc onFrameRender;
	xui_timeline_span_renderer_proc onSpanRender;
	void* pRenderUser;
	int iChangeCount;
	int iCurrentFrameChangeCount;
	int iLayerChangeCount;
	int iFrameChangeCount;
	int iSpanChangeCount;
	int iSelectionChangeCount;
	int iClickCount;
} xui_timeline_view_data_t;

static xui_timeline_view_data_t* __xuiTimeLineViewGetData(xui_widget pWidget);
static void __xuiTimeLineMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser);
static int __xuiTimeLineRunContextCommand(xui_widget pWidget, xui_timeline_view_data_t* pData, int iCommand);

static float __xuiTimeLineMax(float a, float b) { return (a > b) ? a : b; }
static float __xuiTimeLineMin(float a, float b) { return (a < b) ? a : b; }
static int __xuiTimeLineClampInt(int v, int mn, int mx) { if ( v < mn ) return mn; if ( v > mx ) return mx; return v; }
static float __xuiTimeLineClampFloat(float v, float mn, float mx) { if ( v < mn ) return mn; if ( v > mx ) return mx; return v; }
static int __xuiTimeLineAlpha(uint32_t c) { return (int)((c >> 24) & 0xffu); }

static char* __xuiTimeLineStringDup(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) sText = "";
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) return NULL;
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static int __xuiTimeLineFrameTypeNormalize(int iType)
{
	if ( (iType < XUI_TIMELINE_FRAME_EMPTY) || (iType > XUI_TIMELINE_FRAME_BLANK_KEY) ) return XUI_TIMELINE_FRAME_EMPTY;
	return iType;
}

static int __xuiTimeLineSpanTypeNormalize(int iType)
{
	if ( (iType < XUI_TIMELINE_SPAN_CUSTOM) || (iType > XUI_TIMELINE_SPAN_HOLD) ) return XUI_TIMELINE_SPAN_CUSTOM;
	return iType;
}

static int __xuiTimeLineScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static void __xuiTimeLineCopy(char* sDst, int iCapacity, const char* sSrc)
{
	if ( (sDst == NULL) || (iCapacity <= 0) ) return;
	if ( sSrc == NULL ) sSrc = "";
	strncpy(sDst, sSrc, (size_t)iCapacity - 1u);
	sDst[iCapacity - 1] = '\0';
}

static void __xuiTimeLineDefaultColors(xui_timeline_view_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iBackgroundColor = XUI_COLOR_RGBA(248, 251, 255, 255);
	pColors->iCornerColor = XUI_COLOR_RGBA(235, 242, 250, 255);
	pColors->iRulerColor = XUI_COLOR_RGBA(241, 247, 253, 255);
	pColors->iLayerColor = XUI_COLOR_RGBA(246, 250, 254, 255);
	pColors->iLayerAltColor = XUI_COLOR_RGBA(239, 246, 253, 255);
	pColors->iGridColor = XUI_COLOR_RGBA(217, 229, 241, 255);
	pColors->iGridStrongColor = XUI_COLOR_RGBA(190, 211, 230, 255);
	pColors->iTextColor = XUI_COLOR_RGBA(36, 52, 72, 255);
	pColors->iMutedTextColor = XUI_COLOR_RGBA(112, 133, 156, 255);
	pColors->iSelectedColor = XUI_COLOR_RGBA(48, 126, 215, 72);
	pColors->iHoverColor = XUI_COLOR_RGBA(111, 178, 239, 42);
	pColors->iCurrentColor = XUI_COLOR_RGBA(225, 66, 80, 255);
	pColors->iFrameColor = XUI_COLOR_RGBA(94, 143, 188, 255);
	pColors->iKeyFrameColor = XUI_COLOR_RGBA(42, 116, 204, 255);
	pColors->iBlankKeyFrameColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iSpanColor = XUI_COLOR_RGBA(72, 151, 215, 185);
	pColors->iSpanTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iIconColor = XUI_COLOR_RGBA(78, 106, 135, 255);
	pColors->iHiddenIconColor = XUI_COLOR_RGBA(172, 190, 207, 255);
	pColors->iLockedIconColor = XUI_COLOR_RGBA(212, 118, 74, 255);
	pColors->iBorderColor = XUI_COLOR_RGBA(151, 178, 205, 255);
	pColors->iFocusColor = XUI_COLOR_RGBA(47, 125, 214, 255);
	pColors->iDisabledColor = XUI_COLOR_RGBA(154, 172, 190, 255);
	pColors->iTrackColor = XUI_COLOR_RGBA(220, 232, 244, 255);
	pColors->iThumbColor = XUI_COLOR_RGBA(128, 166, 202, 255);
	pColors->iScrollbarHoverColor = XUI_COLOR_RGBA(105, 152, 195, 255);
	pColors->iScrollbarActiveColor = XUI_COLOR_RGBA(62, 126, 184, 255);
	pColors->iScrollbarFocusColor = XUI_COLOR_RGBA(47, 125, 214, 255);
	pColors->iScrollbarDisabledColor = XUI_COLOR_RGBA(210, 220, 230, 255);
}

static void __xuiTimeLineDefaults(xui_timeline_view_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iNextLayerId = 1;
	pData->iNextSpanId = 1;
	pData->iTotalFrames = XUI_TIMELINE_DEFAULT_FRAME_COUNT;
	pData->fFrameRate = XUI_TIMELINE_DEFAULT_FRAME_RATE;
	pData->iCurrentFrame = 0;
	pData->iActiveLayer = -1;
	pData->iAnchorLayer = -1;
	pData->iAnchorFrame = -1;
	pData->iDragBaseSelectionCount = 0;
	pData->bDragSelectingState = 0;
	pData->iHoverLayer = -1;
	pData->iHoverFrame = -1;
	pData->iHoverSpanId = -1;
	pData->iDragLayer = -1;
	pData->iDragFrame = -1;
	pData->iDragModifiers = 0;
	pData->fLayerHeaderWidth = XUI_TIMELINE_DEFAULT_LAYER_WIDTH;
	pData->fFrameWidth = XUI_TIMELINE_DEFAULT_FRAME_WIDTH;
	pData->fMinFrameWidth = XUI_TIMELINE_DEFAULT_MIN_FRAME_WIDTH;
	pData->fMaxFrameWidth = XUI_TIMELINE_DEFAULT_MAX_FRAME_WIDTH;
	pData->fRowHeight = XUI_TIMELINE_DEFAULT_ROW_HEIGHT;
	pData->fRulerHeight = XUI_TIMELINE_DEFAULT_RULER_HEIGHT;
	pData->iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	pData->bShowVisibilityFeature = 1;
	pData->bShowLockFeature = 1;
	__xuiTimeLineDefaultColors(&pData->tColors);
}

static int __xuiTimeLineDescValid(const xui_timeline_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(xui_timeline_view_desc_t)) ) return 0;
	return 1;
}

static void __xuiTimeLineApplyDesc(xui_timeline_view_data_t* pData, const xui_timeline_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) return;
	if ( pDesc->pFont != NULL ) pData->pFont = pDesc->pFont;
	if ( pDesc->iFrameCount > 0 ) pData->iTotalFrames = pDesc->iFrameCount;
	if ( pDesc->fFrameRate > 0.0f ) pData->fFrameRate = pDesc->fFrameRate;
	if ( pDesc->iCurrentFrame >= 0 ) pData->iCurrentFrame = __xuiTimeLineClampInt(pDesc->iCurrentFrame, 0, pData->iTotalFrames - 1);
	if ( pDesc->fLayerHeaderWidth > 0.0f ) pData->fLayerHeaderWidth = pDesc->fLayerHeaderWidth;
	if ( pDesc->fFrameWidth > 0.0f ) pData->fFrameWidth = pDesc->fFrameWidth;
	if ( pDesc->fMinFrameWidth > 0.0f ) pData->fMinFrameWidth = pDesc->fMinFrameWidth;
	if ( pDesc->fMaxFrameWidth > 0.0f ) pData->fMaxFrameWidth = pDesc->fMaxFrameWidth;
	if ( pData->fMinFrameWidth > pData->fMaxFrameWidth ) pData->fMaxFrameWidth = pData->fMinFrameWidth;
	pData->fFrameWidth = __xuiTimeLineClampFloat(pData->fFrameWidth, pData->fMinFrameWidth, pData->fMaxFrameWidth);
	if ( pDesc->fRowHeight > 0.0f ) pData->fRowHeight = pDesc->fRowHeight;
	if ( pDesc->fRulerHeight > 0.0f ) pData->fRulerHeight = pDesc->fRulerHeight;
	if ( pDesc->iScrollbarMode == XUI_SCROLLBAR_MODE_COMPACT ) pData->iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	if ( pDesc->bShowVisibilityFeature != 0 ) pData->bShowVisibilityFeature = 1;
	if ( pDesc->bShowLockFeature != 0 ) pData->bShowLockFeature = 1;
	if ( pDesc->tColors.iBackgroundColor != 0 ) pData->tColors = pDesc->tColors;
}

static xui_rect_t __xuiTimeLineLocalRect(xui_widget pWidget)
{
	xui_rect_t tRect;
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	return tRect;
}

static xui_rect_t __xuiTimeLineFrameRect(xui_widget pWidget, const xui_timeline_view_data_t* pData)
{
	xui_rect_t tRect;
	tRect = __xuiTimeLineLocalRect(pWidget);
	tRect.fX += pData->fLayerHeaderWidth;
	tRect.fY += pData->fRulerHeight;
	tRect.fW = __xuiTimeLineMax(1.0f, tRect.fW - pData->fLayerHeaderWidth);
	tRect.fH = __xuiTimeLineMax(1.0f, tRect.fH - pData->fRulerHeight);
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiTimeLineRulerRect(xui_widget pWidget, const xui_timeline_view_data_t* pData)
{
	xui_rect_t tRect;
	tRect = __xuiTimeLineLocalRect(pWidget);
	tRect.fX = pData->fLayerHeaderWidth;
	tRect.fY = 0.0f;
	tRect.fW = __xuiTimeLineMax(0.0f, tRect.fW - pData->fLayerHeaderWidth);
	tRect.fH = pData->fRulerHeight;
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiTimeLineLayerRect(xui_widget pWidget, const xui_timeline_view_data_t* pData)
{
	xui_rect_t tRect;
	tRect = __xuiTimeLineLocalRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = pData->fRulerHeight;
	tRect.fW = pData->fLayerHeaderWidth;
	tRect.fH = __xuiTimeLineMax(0.0f, tRect.fH - pData->fRulerHeight);
	return xuiInternalSnapRect(tRect);
}

static float __xuiTimeLineLayerHeight(const xui_timeline_view_data_t* pData, int iLayer)
{
	if ( (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return pData->fRowHeight;
	return (pData->arrLayers[iLayer].fHeight > 0.0f) ? pData->arrLayers[iLayer].fHeight : pData->fRowHeight;
}

static float __xuiTimeLineLayerTop(const xui_timeline_view_data_t* pData, int iLayer)
{
	float fY;
	int i;
	fY = 0.0f;
	for ( i = 0; i < iLayer && i < pData->iLayerCount; i++ ) {
		fY += __xuiTimeLineLayerHeight(pData, i);
	}
	return fY;
}

static float __xuiTimeLineContentHeight(const xui_timeline_view_data_t* pData)
{
	return __xuiTimeLineLayerTop(pData, pData->iLayerCount);
}

static int __xuiTimeLineLayerAt(const xui_timeline_view_data_t* pData, float fContentY)
{
	float fY;
	float fH;
	int i;
	fY = 0.0f;
	for ( i = 0; i < pData->iLayerCount; i++ ) {
		fH = __xuiTimeLineLayerHeight(pData, i);
		if ( (fContentY >= fY) && (fContentY < fY + fH) ) return i;
		fY += fH;
	}
	return -1;
}

static int __xuiTimeLineFrameAt(const xui_timeline_view_data_t* pData, float fContentX)
{
	int iFrame;
	if ( pData->fFrameWidth <= 0.0f ) return -1;
	iFrame = (int)floorf(fContentX / pData->fFrameWidth);
	if ( (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return -1;
	return iFrame;
}

static int __xuiTimeLineFindFrame(const xui_timeline_view_data_t* pData, int iLayer, int iFrame)
{
	int i;
	for ( i = 0; i < pData->iFrameRecordCount; i++ ) {
		if ( (pData->arrFrames[i].iLayer == iLayer) && (pData->arrFrames[i].iFrame == iFrame) ) return i;
	}
	return -1;
}

static int __xuiTimeLineFindSpan(const xui_timeline_view_data_t* pData, int iSpanId)
{
	int i;
	for ( i = 0; i < pData->iSpanCount; i++ ) {
		if ( pData->arrSpans[i].iId == iSpanId ) return i;
	}
	return -1;
}

static int __xuiTimeLineFindSelection(const xui_timeline_view_data_t* pData, int iLayer, int iFrame)
{
	int i;
	for ( i = 0; i < pData->iSelectionCount; i++ ) {
		if ( (pData->arrSelection[i].iLayer == iLayer) && (pData->arrSelection[i].iFrame == iFrame) ) return i;
	}
	return -1;
}

static int __xuiTimeLineInvalidate(xui_widget pWidget, xui_timeline_view_data_t* pData, uint32_t iFlags)
{
	if ( pData != NULL && pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pWidget, iFlags);
}

static int __xuiTimeLineUpdateContentSize(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	float fWidth;
	float fHeight;
	int iRet;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fWidth = __xuiTimeLineMax(0.0f, (float)pData->iTotalFrames * pData->fFrameWidth);
	fHeight = __xuiTimeLineMax(0.0f, __xuiTimeLineContentHeight(pData));
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, fWidth, fHeight);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelStep(pData->pFrame, pData->fRowHeight * 3.0f);
	return iRet;
}

static int __xuiTimeLineApplyFrameStyle(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	int iRet;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameSetScrollbarMode(pData->pFrame, pData->iScrollbarMode);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetMetrics(pData->pFrame, 8.0f, 18.0f, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame,
		pData->tColors.iTrackColor, pData->tColors.iThumbColor,
		pData->tColors.iScrollbarHoverColor, pData->tColors.iScrollbarActiveColor,
		pData->tColors.iScrollbarFocusColor, pData->tColors.iScrollbarDisabledColor);
	return iRet;
}

static void __xuiTimeLineScrollOffset(xui_timeline_view_data_t* pData, float* pX, float* pY)
{
	float fX;
	float fY;
	fX = 0.0f;
	fY = 0.0f;
	if ( (pData != NULL) && (pData->pFrame != NULL) ) {
		(void)xuiScrollFrameGetOffset(pData->pFrame, &fX, &fY);
	}
	if ( pX != NULL ) *pX = fX;
	if ( pY != NULL ) *pY = fY;
}

static int __xuiTimeLineSetActiveLayer(xui_widget pWidget, xui_timeline_view_data_t* pData, int iLayer)
{
	int iOld;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iLayerCount <= 0 ) iLayer = -1;
	else iLayer = __xuiTimeLineClampInt(iLayer, 0, pData->iLayerCount - 1);
	iOld = pData->iActiveLayer;
	if ( iOld == iLayer ) return XUI_OK;
	if ( iOld >= 0 && iOld < pData->iLayerCount ) pData->arrLayers[iOld].bSelected = 0;
	pData->iActiveLayer = iLayer;
	if ( iLayer >= 0 && iLayer < pData->iLayerCount ) {
		pData->arrLayers[iLayer].bSelected = 1;
		if ( pData->onLayerSelected != NULL ) pData->onLayerSelected(pWidget, iLayer, pData->pLayerSelectedUser);
	}
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTimeLineSelectionNotify(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSelectionChangeCount++;
	pData->iChangeCount++;
	if ( pData->onSelection != NULL ) pData->onSelection(pWidget, pData->pSelectionUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTimeLineSelectFrameRaw(xui_timeline_view_data_t* pData, int iLayer, int iFrame, int bSelected)
{
	int iIndex;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return 0;
	iIndex = __xuiTimeLineFindSelection(pData, iLayer, iFrame);
	if ( bSelected ) {
		if ( iIndex >= 0 ) return 0;
		if ( pData->iSelectionCount >= XUI_TIMELINE_SELECTION_CAPACITY ) return 0;
		pData->arrSelection[pData->iSelectionCount].iLayer = iLayer;
		pData->arrSelection[pData->iSelectionCount].iFrame = iFrame;
		pData->iSelectionCount++;
		return 1;
	}
	if ( iIndex < 0 ) return 0;
	pData->arrSelection[iIndex] = pData->arrSelection[pData->iSelectionCount - 1];
	pData->iSelectionCount--;
	return 1;
}

static int __xuiTimeLineSelectRangeRaw(xui_timeline_view_data_t* pData, int iLayer0, int iFrame0, int iLayer1, int iFrame1, int bSelected)
{
	int iLayerMin;
	int iLayerMax;
	int iFrameMin;
	int iFrameMax;
	int iLayer;
	int iFrame;
	int bChanged;
	if ( pData == NULL ) return 0;
	iLayerMin = (iLayer0 < iLayer1) ? iLayer0 : iLayer1;
	iLayerMax = (iLayer0 > iLayer1) ? iLayer0 : iLayer1;
	iFrameMin = (iFrame0 < iFrame1) ? iFrame0 : iFrame1;
	iFrameMax = (iFrame0 > iFrame1) ? iFrame0 : iFrame1;
	iLayerMin = __xuiTimeLineClampInt(iLayerMin, 0, pData->iLayerCount - 1);
	iLayerMax = __xuiTimeLineClampInt(iLayerMax, 0, pData->iLayerCount - 1);
	iFrameMin = __xuiTimeLineClampInt(iFrameMin, 0, pData->iTotalFrames - 1);
	iFrameMax = __xuiTimeLineClampInt(iFrameMax, 0, pData->iTotalFrames - 1);
	bChanged = 0;
	for ( iLayer = iLayerMin; iLayer <= iLayerMax; iLayer++ ) {
		for ( iFrame = iFrameMin; iFrame <= iFrameMax; iFrame++ ) {
			bChanged |= __xuiTimeLineSelectFrameRaw(pData, iLayer, iFrame, bSelected);
			if ( pData->iSelectionCount >= XUI_TIMELINE_SELECTION_CAPACITY ) return bChanged;
		}
	}
	return bChanged;
}

static int __xuiTimeLineClearSelectionRaw(xui_timeline_view_data_t* pData)
{
	if ( (pData == NULL) || (pData->iSelectionCount <= 0) ) return 0;
	pData->iSelectionCount = 0;
	return 1;
}

static int __xuiTimeLineFindSelectionArray(const xui_timeline_selection_t* pSelection, int iCount, int iLayer, int iFrame)
{
	int i;
	if ( pSelection == NULL ) return -1;
	for ( i = 0; i < iCount; i++ ) {
		if ( (pSelection[i].iLayer == iLayer) && (pSelection[i].iFrame == iFrame) ) return i;
	}
	return -1;
}

static void __xuiTimeLineStoreSelection(const xui_timeline_view_data_t* pData, xui_timeline_selection_t* pSelection, int* pCount)
{
	int i;
	int iCount;
	if ( pCount != NULL ) *pCount = 0;
	if ( (pData == NULL) || (pSelection == NULL) || (pCount == NULL) ) return;
	iCount = pData->iSelectionCount;
	if ( iCount > XUI_TIMELINE_SELECTION_CAPACITY ) iCount = XUI_TIMELINE_SELECTION_CAPACITY;
	for ( i = 0; i < iCount; i++ ) pSelection[i] = pData->arrSelection[i];
	*pCount = iCount;
}

static int __xuiTimeLineSelectionMatchesArray(const xui_timeline_view_data_t* pData, const xui_timeline_selection_t* pSelection, int iCount)
{
	int i;
	if ( pData == NULL ) return (iCount == 0);
	if ( iCount < 0 ) iCount = 0;
	if ( iCount > XUI_TIMELINE_SELECTION_CAPACITY ) iCount = XUI_TIMELINE_SELECTION_CAPACITY;
	if ( pData->iSelectionCount != iCount ) return 0;
	for ( i = 0; i < pData->iSelectionCount; i++ ) {
		if ( __xuiTimeLineFindSelectionArray(pSelection, iCount, pData->arrSelection[i].iLayer, pData->arrSelection[i].iFrame) < 0 ) return 0;
	}
	return 1;
}

static int __xuiTimeLineRestoreSelectionRaw(xui_timeline_view_data_t* pData, const xui_timeline_selection_t* pSelection, int iCount)
{
	int i;
	int bChanged;
	if ( pData == NULL ) return 0;
	if ( (pSelection == NULL) || (iCount < 0) ) iCount = 0;
	if ( iCount > XUI_TIMELINE_SELECTION_CAPACITY ) iCount = XUI_TIMELINE_SELECTION_CAPACITY;
	bChanged = !__xuiTimeLineSelectionMatchesArray(pData, pSelection, iCount);
	pData->iSelectionCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		(void)__xuiTimeLineSelectFrameRaw(pData, pSelection[i].iLayer, pSelection[i].iFrame, 1);
	}
	return bChanged;
}

static void __xuiTimeLineResetDragSelection(xui_timeline_view_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->bDraggingSelection = 0;
	pData->bDragMoved = 0;
	pData->bDragSelectingState = 0;
	pData->iDragLayer = -1;
	pData->iDragFrame = -1;
	pData->iDragModifiers = 0;
	pData->iDragBaseSelectionCount = 0;
}

static int __xuiTimeLineApplyDragSelection(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	int bChanged;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->iAnchorLayer < 0) || (pData->iAnchorFrame < 0) || (pData->iDragLayer < 0) || (pData->iDragFrame < 0) ) return XUI_OK;
	bChanged = 0;
	if ( (pData->iDragModifiers & XUI_MOD_SHIFT) != 0 ) {
		bChanged |= __xuiTimeLineClearSelectionRaw(pData);
		bChanged |= __xuiTimeLineSelectRangeRaw(pData, pData->iAnchorLayer, pData->iAnchorFrame, pData->iDragLayer, pData->iDragFrame, 1);
	} else if ( (pData->iDragModifiers & XUI_MOD_CTRL) != 0 ) {
		bChanged |= __xuiTimeLineRestoreSelectionRaw(pData, pData->arrDragBaseSelection, pData->iDragBaseSelectionCount);
		bChanged |= __xuiTimeLineSelectRangeRaw(pData, pData->iAnchorLayer, pData->iAnchorFrame, pData->iDragLayer, pData->iDragFrame, pData->bDragSelectingState);
	} else {
		bChanged |= __xuiTimeLineClearSelectionRaw(pData);
		bChanged |= __xuiTimeLineSelectRangeRaw(pData, pData->iAnchorLayer, pData->iAnchorFrame, pData->iDragLayer, pData->iDragFrame, 1);
	}
	if ( bChanged ) return __xuiTimeLineSelectionNotify(pWidget, pData);
	return XUI_OK;
}

static int __xuiTimeLineSelectionRangeForLayer(const xui_timeline_view_data_t* pData, int iLayer, int* pStartFrame, int* pEndFrame)
{
	int i;
	int iStart;
	int iEnd;
	int bFound;

	if ( pStartFrame != NULL ) *pStartFrame = -1;
	if ( pEndFrame != NULL ) *pEndFrame = -1;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return 0;
	iStart = 0;
	iEnd = 0;
	bFound = 0;
	for ( i = 0; i < pData->iSelectionCount; i++ ) {
		if ( pData->arrSelection[i].iLayer != iLayer ) continue;
		if ( !bFound ) {
			iStart = pData->arrSelection[i].iFrame;
			iEnd = pData->arrSelection[i].iFrame;
			bFound = 1;
		} else {
			if ( pData->arrSelection[i].iFrame < iStart ) iStart = pData->arrSelection[i].iFrame;
			if ( pData->arrSelection[i].iFrame > iEnd ) iEnd = pData->arrSelection[i].iFrame;
		}
	}
	if ( !bFound ) return 0;
	if ( pStartFrame != NULL ) *pStartFrame = iStart;
	if ( pEndFrame != NULL ) *pEndFrame = iEnd;
	return 1;
}

static int __xuiTimeLineSetCurrentFrameInternal(xui_widget pWidget, xui_timeline_view_data_t* pData, int iFrame)
{
	int iOld;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->iTotalFrames <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iFrame = __xuiTimeLineClampInt(iFrame, 0, pData->iTotalFrames - 1);
	iOld = pData->iCurrentFrame;
	if ( iOld == iFrame ) return XUI_OK;
	if ( (pData->onCurrentChanging != NULL) && !pData->onCurrentChanging(pWidget, iOld, iFrame, pData->pCurrentUser) ) {
		return XUI_OK;
	}
	pData->iCurrentFrame = iFrame;
	pData->iCurrentFrameChangeCount++;
	pData->iChangeCount++;
	if ( pData->onCurrentChanged != NULL ) pData->onCurrentChanged(pWidget, iOld, iFrame, pData->pCurrentUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTimeLineDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTimeLineAlpha(iColor) == 0 ) return XUI_OK;
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) return XUI_OK;
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
}

static int __xuiTimeLineDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTimeLineAlpha(iColor) == 0) ) return XUI_OK;
	if ( (pProxy == NULL) || (pProxy->drawRectStroke == NULL) ) return XUI_OK;
	return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), fWidth, iColor);
}

static int __xuiTimeLineDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTimeLineAlpha(iColor) == 0 ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
	}
	return __xuiTimeLineDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiTimeLineDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTimeLineAlpha(iColor) == 0) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectStroke != NULL) ) {
		return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), fWidth, iColor);
	}
	return __xuiTimeLineDrawStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiTimeLineDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float x0, float y0, float x1, float y1, float fWidth, uint32_t iColor)
{
	if ( (pProxy == NULL) || (pProxy->drawLine == NULL) || (__xuiTimeLineAlpha(iColor) == 0) ) return XUI_OK;
	return pProxy->drawLine(pProxy, pDraw, x0, y0, x1, y1, fWidth, iColor);
}

static int __xuiTimeLineDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pFont == NULL) || (sText == NULL) || (sText[0] == '\0') || (__xuiTimeLineAlpha(iColor) == 0) ||
	     (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XUI_OK;
	return pProxy->drawText(pProxy, pDraw, pFont, sText, xuiInternalSnapRect(tRect), iColor, iFlags);
}

static xui_rect_t __xuiTimeLineLayerIconRect(const xui_timeline_view_data_t* pData, int iIcon, float fRowY, float fRowH)
{
	float fRight;
	float fSize;
	fSize = 14.0f;
	fRight = pData->fLayerHeaderWidth - 8.0f;
	if ( iIcon == XUI_TIMELINE_HIT_LAYER_LOCK ) fRight -= 20.0f;
	return xuiInternalSnapRect((xui_rect_t){fRight - fSize, fRowY + (fRowH - fSize) * 0.5f, fSize, fSize});
}

static int __xuiTimeLineDrawEyeIcon(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor, int bVisible)
{
	int iRet;
	if ( !bVisible ) {
		iRet = __xuiTimeLineDrawStroke(pProxy, pDraw, tRect, 1.0f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTimeLineDrawLine(pProxy, pDraw, tRect.fX + 2.0f, tRect.fY + tRect.fH - 2.0f, tRect.fX + tRect.fW - 2.0f, tRect.fY + 2.0f, 1.2f, iColor);
	}
	iRet = __xuiTimeLineDrawRectStroke(pProxy, pDraw, (xui_rect_t){tRect.fX + 1.0f, tRect.fY + 3.0f, tRect.fW - 2.0f, tRect.fH - 6.0f}, 1.0f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pProxy != NULL) && (pProxy->drawCircleFill != NULL) ) {
		return pProxy->drawCircleFill(pProxy, pDraw, tRect.fX + tRect.fW * 0.5f, tRect.fY + tRect.fH * 0.5f, 2.2f, iColor);
	}
	return XUI_OK;
}

static int __xuiTimeLineDrawLockIcon(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor, int bLocked)
{
	int iRet;
	if ( bLocked ) {
		iRet = __xuiTimeLineDrawRectStroke(pProxy, pDraw, (xui_rect_t){tRect.fX + 3.0f, tRect.fY + 1.0f, tRect.fW - 6.0f, 8.0f}, 1.2f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTimeLineDrawRectFill(pProxy, pDraw, (xui_rect_t){tRect.fX + 2.0f, tRect.fY + 6.0f, tRect.fW - 4.0f, 7.0f}, iColor);
	}
	iRet = __xuiTimeLineDrawRectStroke(pProxy, pDraw, (xui_rect_t){tRect.fX + 5.0f, tRect.fY + 1.0f, tRect.fW - 6.0f, 7.0f}, 1.2f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTimeLineDrawRectStroke(pProxy, pDraw, (xui_rect_t){tRect.fX + 2.0f, tRect.fY + 6.0f, tRect.fW - 4.0f, 7.0f}, 1.0f, iColor);
}

static int __xuiTimeLineLayerState(const xui_timeline_view_data_t* pData, int iLayer)
{
	int iState;
	iState = 0;
	if ( pData->iHoverLayer == iLayer ) iState |= XUI_TIMELINE_STATE_HOVER;
	if ( pData->iActiveLayer == iLayer || pData->arrLayers[iLayer].bSelected ) iState |= XUI_TIMELINE_STATE_SELECTED;
	if ( pData->arrLayers[iLayer].bLocked ) iState |= XUI_TIMELINE_STATE_LOCKED;
	if ( !pData->arrLayers[iLayer].bVisible ) iState |= XUI_TIMELINE_STATE_HIDDEN;
	return iState;
}

static int __xuiTimeLineFrameState(const xui_timeline_view_data_t* pData, int iLayer, int iFrame, int iType, int iSpanId)
{
	int iState;
	iState = 0;
	if ( (pData->iHoverLayer == iLayer) && (pData->iHoverFrame == iFrame) ) iState |= XUI_TIMELINE_STATE_HOVER;
	if ( __xuiTimeLineFindSelection(pData, iLayer, iFrame) >= 0 ) iState |= XUI_TIMELINE_STATE_SELECTED;
	if ( pData->iCurrentFrame == iFrame ) iState |= XUI_TIMELINE_STATE_CURRENT;
	if ( iType == XUI_TIMELINE_FRAME_KEY ) iState |= XUI_TIMELINE_STATE_KEY;
	if ( iType == XUI_TIMELINE_FRAME_BLANK_KEY ) iState |= XUI_TIMELINE_STATE_BLANK_KEY;
	if ( iSpanId >= 0 ) iState |= XUI_TIMELINE_STATE_SPAN;
	if ( (iLayer >= 0) && (iLayer < pData->iLayerCount) ) {
		if ( pData->arrLayers[iLayer].bLocked ) iState |= XUI_TIMELINE_STATE_LOCKED;
		if ( !pData->arrLayers[iLayer].bVisible ) iState |= XUI_TIMELINE_STATE_HIDDEN;
	}
	return iState;
}

static int __xuiTimeLineRenderLayers(xui_widget pWidget, xui_draw_context pDraw, xui_timeline_view_data_t* pData, xui_proxy pProxy)
{
	xui_rect_t tLayerRect;
	xui_rect_t tRow;
	xui_rect_t tText;
	xui_rect_t tIcon;
	float fOffsetY;
	float fY;
	float fH;
	uint32_t iFill;
	int i;
	int iRet;
	int iState;

	tLayerRect = __xuiTimeLineLayerRect(pWidget, pData);
	__xuiTimeLineScrollOffset(pData, NULL, &fOffsetY);
	iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tLayerRect, pData->tColors.iLayerColor);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < pData->iLayerCount; i++ ) {
		fH = __xuiTimeLineLayerHeight(pData, i);
		fY = tLayerRect.fY + __xuiTimeLineLayerTop(pData, i) - fOffsetY;
		if ( (fY + fH < tLayerRect.fY) || (fY > tLayerRect.fY + tLayerRect.fH) ) continue;
		tRow = xuiInternalSnapRect((xui_rect_t){tLayerRect.fX, fY, tLayerRect.fW, fH});
		iState = __xuiTimeLineLayerState(pData, i);
		iFill = ((i & 1) != 0) ? pData->tColors.iLayerAltColor : pData->tColors.iLayerColor;
		if ( (iState & XUI_TIMELINE_STATE_SELECTED) != 0 ) iFill = pData->tColors.iSelectedColor;
		else if ( (iState & XUI_TIMELINE_STATE_HOVER) != 0 ) iFill = pData->tColors.iHoverColor;
		if ( pData->onLayerRender != NULL && pData->onLayerRender(pWidget, i, &pData->arrLayers[i], pDraw, tRow, iState, pData->pRenderUser) ) {
			continue;
		}
		iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tRow, iFill);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTimeLineDrawFill(pProxy, pDraw, (xui_rect_t){tRow.fX, tRow.fY, 3.0f, tRow.fH}, pData->arrLayers[i].iColor);
		if ( iRet != XUI_OK ) return iRet;
		tText = xuiInternalSnapRect((xui_rect_t){tRow.fX + 10.0f, tRow.fY, __xuiTimeLineMax(0.0f, pData->fLayerHeaderWidth - 58.0f), tRow.fH});
		iRet = __xuiTimeLineDrawText(pProxy, pDraw, pData->pFont, pData->arrLayers[i].sName, tText,
			((iState & XUI_TIMELINE_STATE_HIDDEN) != 0) ? pData->tColors.iMutedTextColor : pData->tColors.iTextColor,
			XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
		if ( pData->bShowLockFeature ) {
			tIcon = __xuiTimeLineLayerIconRect(pData, XUI_TIMELINE_HIT_LAYER_LOCK, tRow.fY, tRow.fH);
			iRet = __xuiTimeLineDrawLockIcon(pProxy, pDraw, tIcon, pData->arrLayers[i].bLocked ? pData->tColors.iLockedIconColor : pData->tColors.iIconColor, pData->arrLayers[i].bLocked);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pData->bShowVisibilityFeature ) {
			tIcon = __xuiTimeLineLayerIconRect(pData, XUI_TIMELINE_HIT_LAYER_VISIBLE, tRow.fY, tRow.fH);
			iRet = __xuiTimeLineDrawEyeIcon(pProxy, pDraw, tIcon, pData->arrLayers[i].bVisible ? pData->tColors.iIconColor : pData->tColors.iHiddenIconColor, pData->arrLayers[i].bVisible);
			if ( iRet != XUI_OK ) return iRet;
		}
		iRet = __xuiTimeLineDrawLine(pProxy, pDraw, tRow.fX, tRow.fY + tRow.fH - 0.5f, tRow.fX + tRow.fW, tRow.fY + tRow.fH - 0.5f, 1.0f, pData->tColors.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiTimeLineDrawLine(pProxy, pDraw, tLayerRect.fX + tLayerRect.fW - 0.5f, tLayerRect.fY, tLayerRect.fX + tLayerRect.fW - 0.5f, tLayerRect.fY + tLayerRect.fH, 1.0f, pData->tColors.iBorderColor);
	return iRet;
}

static int __xuiTimeLineRenderRuler(xui_widget pWidget, xui_draw_context pDraw, xui_timeline_view_data_t* pData, xui_proxy pProxy)
{
	xui_rect_t tRuler;
	xui_rect_t tTick;
	float fOffsetX;
	float fX;
	int iFrame0;
	int iFrame1;
	int iFrame;
	int iStep;
	int iRet;
	char sText[32];
	tRuler = __xuiTimeLineRulerRect(pWidget, pData);
	__xuiTimeLineScrollOffset(pData, &fOffsetX, NULL);
	iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tRuler, pData->tColors.iRulerColor);
	if ( iRet != XUI_OK ) return iRet;
	iFrame0 = __xuiTimeLineClampInt((int)floorf(fOffsetX / pData->fFrameWidth) - 1, 0, pData->iTotalFrames - 1);
	iFrame1 = __xuiTimeLineClampInt((int)ceilf((fOffsetX + tRuler.fW) / pData->fFrameWidth) + 1, 0, pData->iTotalFrames - 1);
	iStep = (pData->fFrameWidth < 8.0f) ? 10 : ((pData->fFrameWidth < 14.0f) ? 5 : 1);
	for ( iFrame = iFrame0; iFrame <= iFrame1; iFrame++ ) {
		fX = tRuler.fX + (float)iFrame * pData->fFrameWidth - fOffsetX;
		if ( (iFrame % iStep) == 0 ) {
			tTick = xuiInternalSnapRect((xui_rect_t){fX, tRuler.fY + 1.0f, pData->fFrameWidth, tRuler.fH - 2.0f});
			if ( pData->onRulerRender != NULL && pData->onRulerRender(pWidget, iFrame, pDraw, tTick, (iFrame == pData->iCurrentFrame) ? XUI_TIMELINE_STATE_CURRENT : 0, pData->pRenderUser) ) {
				continue;
			}
			snprintf(sText, sizeof(sText), "%d", iFrame + 1);
			iRet = __xuiTimeLineDrawText(pProxy, pDraw, pData->pFont, sText, tTick, pData->tColors.iMutedTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		iRet = __xuiTimeLineDrawLine(pProxy, pDraw, fX, tRuler.fY + tRuler.fH - ((iFrame % 5) == 0 ? 9.0f : 5.0f), fX, tRuler.fY + tRuler.fH, 1.0f, ((iFrame % 5) == 0) ? pData->tColors.iGridStrongColor : pData->tColors.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	fX = tRuler.fX + (float)pData->iCurrentFrame * pData->fFrameWidth - fOffsetX;
	if ( (fX >= tRuler.fX - 4.0f) && (fX <= tRuler.fX + tRuler.fW + 4.0f) ) {
		iRet = __xuiTimeLineDrawLine(pProxy, pDraw, fX + 0.5f, tRuler.fY, fX + 0.5f, tRuler.fY + tRuler.fH, 2.0f, pData->tColors.iCurrentColor);
		if ( iRet != XUI_OK ) return iRet;
		if ( pProxy->drawTriangleFill != NULL ) {
			xui_vec2_t a = {fX + 0.5f, tRuler.fY + tRuler.fH - 2.0f};
			xui_vec2_t b = {fX - 5.0f, tRuler.fY + tRuler.fH - 9.0f};
			xui_vec2_t c = {fX + 6.0f, tRuler.fY + tRuler.fH - 9.0f};
			iRet = pProxy->drawTriangleFill(pProxy, pDraw, a, b, c, pData->tColors.iCurrentColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	iRet = __xuiTimeLineDrawLine(pProxy, pDraw, tRuler.fX, tRuler.fY + tRuler.fH - 0.5f, tRuler.fX + tRuler.fW, tRuler.fY + tRuler.fH - 0.5f, 1.0f, pData->tColors.iBorderColor);
	return iRet;
}

static int __xuiTimeLineCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_timeline_view_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tCorner;
	uint32_t iBorder;
	int iRet;
	(void)iStateId;
	pData = (xui_timeline_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tRect = __xuiTimeLineLocalRect(pWidget);
	tCorner = xuiInternalSnapRect((xui_rect_t){0.0f, 0.0f, pData->fLayerHeaderWidth, pData->fRulerHeight});
	iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tRect, pData->tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tCorner, pData->tColors.iCornerColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTimeLineRenderRuler(pWidget, pDraw, pData, pProxy);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTimeLineRenderLayers(pWidget, pDraw, pData, pProxy);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTimeLineDrawLine(pProxy, pDraw, tCorner.fX, tCorner.fY + tCorner.fH - 0.5f, tCorner.fX + tCorner.fW, tCorner.fY + tCorner.fH - 0.5f, 1.0f, pData->tColors.iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	iBorder = (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) ? pData->tColors.iFocusColor : pData->tColors.iBorderColor;
	return __xuiTimeLineDrawStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
}

static int __xuiTimeLineDrawFrameMarker(xui_widget pWidget, xui_draw_context pDraw, xui_timeline_view_data_t* pData, xui_proxy pProxy, int iLayer, int iFrame, const xui_timeline_frame_t* pFrame, xui_rect_t tCell)
{
	xui_rect_t tMark;
	uint32_t iColor;
	int iState;
	int iType;
	int iRet;
	iType = (pFrame != NULL) ? pFrame->iType : XUI_TIMELINE_FRAME_EMPTY;
	iState = __xuiTimeLineFrameState(pData, iLayer, iFrame, iType, -1);
	if ( pData->onFrameRender != NULL && pData->onFrameRender(pWidget, iLayer, iFrame, pFrame, pDraw, tCell, iState, pData->pRenderUser) ) {
		return XUI_OK;
	}
	if ( iType == XUI_TIMELINE_FRAME_EMPTY ) return XUI_OK;
	iColor = (iType == XUI_TIMELINE_FRAME_KEY) ? pData->tColors.iKeyFrameColor : ((iType == XUI_TIMELINE_FRAME_BLANK_KEY) ? pData->tColors.iBlankKeyFrameColor : pData->tColors.iFrameColor);
	tMark = xuiInternalSnapRect((xui_rect_t){tCell.fX + __xuiTimeLineMax(2.0f, (tCell.fW - 8.0f) * 0.5f), tCell.fY + __xuiTimeLineMax(2.0f, (tCell.fH - 8.0f) * 0.5f), __xuiTimeLineMin(8.0f, tCell.fW - 3.0f), __xuiTimeLineMin(8.0f, tCell.fH - 4.0f)});
	if ( iType == XUI_TIMELINE_FRAME_BLANK_KEY ) {
		iRet = __xuiTimeLineDrawRectFill(pProxy, pDraw, tMark, XUI_COLOR_RGBA(255, 255, 255, 255));
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTimeLineDrawRectStroke(pProxy, pDraw, tMark, 1.0f, pData->tColors.iKeyFrameColor);
	}
	return __xuiTimeLineDrawRectFill(pProxy, pDraw, tMark, iColor);
}

static int __xuiTimeLineViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_timeline_view_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tView;
	xui_rect_t tRow;
	xui_rect_t tCell;
	xui_rect_t tSpan;
	float fOffsetX;
	float fOffsetY;
	float fY;
	float fH;
	float fX;
	int iLayer;
	int iFrame0;
	int iFrame1;
	int iFrame;
	int i;
	int iRet;
	int iState;
	uint32_t iRowColor;
	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pViewport == NULL) || (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tView = xuiWidgetGetRect(pViewport);
	tView.fX = 0.0f;
	tView.fY = 0.0f;
	__xuiTimeLineScrollOffset(pData, &fOffsetX, &fOffsetY);
	iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tView, pData->tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iFrame0 = __xuiTimeLineClampInt((int)floorf(fOffsetX / pData->fFrameWidth) - 1, 0, pData->iTotalFrames - 1);
	iFrame1 = __xuiTimeLineClampInt((int)ceilf((fOffsetX + tView.fW) / pData->fFrameWidth) + 1, 0, pData->iTotalFrames - 1);
	for ( iLayer = 0; iLayer < pData->iLayerCount; iLayer++ ) {
		fH = __xuiTimeLineLayerHeight(pData, iLayer);
		fY = __xuiTimeLineLayerTop(pData, iLayer) - fOffsetY;
		if ( (fY + fH < 0.0f) || (fY > tView.fH) ) continue;
		tRow = xuiInternalSnapRect((xui_rect_t){0.0f, fY, tView.fW, fH});
		iRowColor = ((iLayer & 1) != 0) ? pData->tColors.iLayerAltColor : pData->tColors.iBackgroundColor;
		iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tRow, iRowColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTimeLineDrawLine(pProxy, pDraw, 0.0f, tRow.fY + tRow.fH - 0.5f, tView.fW, tRow.fY + tRow.fH - 0.5f, 1.0f, pData->tColors.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
		for ( iFrame = iFrame0; iFrame <= iFrame1; iFrame++ ) {
			fX = (float)iFrame * pData->fFrameWidth - fOffsetX;
			iRet = __xuiTimeLineDrawLine(pProxy, pDraw, fX, tRow.fY, fX, tRow.fY + tRow.fH, 1.0f, ((iFrame % 5) == 0) ? pData->tColors.iGridStrongColor : pData->tColors.iGridColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	for ( i = 0; i < pData->iSpanCount; i++ ) {
		xui_timeline_span_t* pSpan = &pData->arrSpans[i];
		if ( (pSpan->iLayer < 0) || (pSpan->iLayer >= pData->iLayerCount) ) continue;
		fH = __xuiTimeLineLayerHeight(pData, pSpan->iLayer);
		fY = __xuiTimeLineLayerTop(pData, pSpan->iLayer) - fOffsetY;
		if ( (fY + fH < 0.0f) || (fY > tView.fH) ) continue;
		tSpan = xuiInternalSnapRect((xui_rect_t){
			(float)pSpan->iStartFrame * pData->fFrameWidth - fOffsetX + 2.0f,
			fY + 4.0f,
			__xuiTimeLineMax(1.0f, (float)(pSpan->iEndFrame - pSpan->iStartFrame + 1) * pData->fFrameWidth - 4.0f),
			__xuiTimeLineMax(1.0f, fH - 8.0f)
		});
		if ( (tSpan.fX + tSpan.fW < 0.0f) || (tSpan.fX > tView.fW) ) continue;
		iState = __xuiTimeLineFrameState(pData, pSpan->iLayer, pSpan->iStartFrame, XUI_TIMELINE_FRAME_EMPTY, pSpan->iId);
		if ( pData->onSpanRender != NULL && pData->onSpanRender(pWidget, pSpan->iLayer, pSpan->iId, pSpan, pDraw, tSpan, iState, pData->pRenderUser) ) {
			continue;
		}
		iRet = __xuiTimeLineDrawRectFill(pProxy, pDraw, tSpan, (pSpan->iColor != 0) ? pSpan->iColor : pData->tColors.iSpanColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTimeLineDrawText(pProxy, pDraw, pData->pFont, pSpan->sLabel, (xui_rect_t){tSpan.fX + 6.0f, tSpan.fY, __xuiTimeLineMax(0.0f, tSpan.fW - 12.0f), tSpan.fH}, pData->tColors.iSpanTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pData->iSelectionCount; i++ ) {
		xui_timeline_selection_t* pSel = &pData->arrSelection[i];
		if ( (pSel->iLayer < 0) || (pSel->iLayer >= pData->iLayerCount) || (pSel->iFrame < iFrame0) || (pSel->iFrame > iFrame1) ) continue;
		fH = __xuiTimeLineLayerHeight(pData, pSel->iLayer);
		tCell = xuiInternalSnapRect((xui_rect_t){(float)pSel->iFrame * pData->fFrameWidth - fOffsetX + 1.0f, __xuiTimeLineLayerTop(pData, pSel->iLayer) - fOffsetY + 2.0f, __xuiTimeLineMax(1.0f, pData->fFrameWidth - 2.0f), __xuiTimeLineMax(1.0f, fH - 4.0f)});
		iRet = __xuiTimeLineDrawFill(pProxy, pDraw, tCell, pData->tColors.iSelectedColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pData->iFrameRecordCount; i++ ) {
		xui_timeline_frame_t* pFrame = &pData->arrFrames[i];
		if ( (pFrame->iLayer < 0) || (pFrame->iLayer >= pData->iLayerCount) || (pFrame->iFrame < iFrame0) || (pFrame->iFrame > iFrame1) ) continue;
		fH = __xuiTimeLineLayerHeight(pData, pFrame->iLayer);
		tCell = xuiInternalSnapRect((xui_rect_t){(float)pFrame->iFrame * pData->fFrameWidth - fOffsetX, __xuiTimeLineLayerTop(pData, pFrame->iLayer) - fOffsetY, pData->fFrameWidth, fH});
		iRet = __xuiTimeLineDrawFrameMarker(pWidget, pDraw, pData, pProxy, pFrame->iLayer, pFrame->iFrame, pFrame, tCell);
		if ( iRet != XUI_OK ) return iRet;
	}
	fX = (float)pData->iCurrentFrame * pData->fFrameWidth - fOffsetX;
	if ( (fX >= -4.0f) && (fX <= tView.fW + 4.0f) ) {
		iRet = __xuiTimeLineDrawLine(pProxy, pDraw, fX + 0.5f, 0.0f, fX + 0.5f, tView.fH, 2.0f, pData->tColors.iCurrentColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTimeLineHitLocal(xui_widget pWidget, xui_timeline_view_data_t* pData, float fX, float fY, xui_timeline_hit_t* pHit)
{
	xui_rect_t tRect;
	xui_rect_t tRuler;
	xui_rect_t tLayer;
	xui_rect_t tFrame;
	xui_rect_t tBar;
	float fOffsetX;
	float fOffsetY;
	float fContentX;
	float fContentY;
	float fRowY;
	float fRowH;
	float fPlayheadX;
	int iLayer;
	int iFrame;
	int i;
	if ( (pWidget == NULL) || (pData == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pHit, 0, sizeof(*pHit));
	pHit->iType = XUI_TIMELINE_HIT_NONE;
	pHit->iLayer = -1;
	pHit->iLayerId = -1;
	pHit->iFrame = -1;
	pHit->iSpanId = -1;
	tRect = __xuiTimeLineLocalRect(pWidget);
	if ( (fX < 0.0f) || (fY < 0.0f) || (fX > tRect.fW) || (fY > tRect.fH) ) return XUI_OK;
	__xuiTimeLineScrollOffset(pData, &fOffsetX, &fOffsetY);
	tRuler = __xuiTimeLineRulerRect(pWidget, pData);
	tLayer = __xuiTimeLineLayerRect(pWidget, pData);
	tFrame = __xuiTimeLineFrameRect(pWidget, pData);
	if ( pData->pFrame != NULL ) {
		if ( xuiScrollFrameIsHScrollBarVisible(pData->pFrame) ) {
			xui_widget pBar = xuiScrollFrameGetHScrollBarWidget(pData->pFrame);
			if ( pBar != NULL ) {
				tBar = xuiWidgetGetRect(pBar);
				tBar.fX += tFrame.fX;
				tBar.fY += tFrame.fY;
				if ( (fX >= tBar.fX) && (fY >= tBar.fY) && (fX <= tBar.fX + tBar.fW) && (fY <= tBar.fY + tBar.fH) ) {
					pHit->iType = XUI_TIMELINE_HIT_HSCROLLBAR;
					pHit->tRect = tBar;
					return XUI_OK;
				}
			}
		}
		if ( xuiScrollFrameIsVScrollBarVisible(pData->pFrame) ) {
			xui_widget pBar = xuiScrollFrameGetVScrollBarWidget(pData->pFrame);
			if ( pBar != NULL ) {
				tBar = xuiWidgetGetRect(pBar);
				tBar.fX += tFrame.fX;
				tBar.fY += tFrame.fY;
				if ( (fX >= tBar.fX) && (fY >= tBar.fY) && (fX <= tBar.fX + tBar.fW) && (fY <= tBar.fY + tBar.fH) ) {
					pHit->iType = XUI_TIMELINE_HIT_VSCROLLBAR;
					pHit->tRect = tBar;
					return XUI_OK;
				}
			}
		}
	}
	if ( (fX < pData->fLayerHeaderWidth) && (fY < pData->fRulerHeight) ) {
		pHit->iType = XUI_TIMELINE_HIT_CORNER;
		pHit->tRect = (xui_rect_t){0.0f, 0.0f, pData->fLayerHeaderWidth, pData->fRulerHeight};
		return XUI_OK;
	}
	if ( (fX >= tRuler.fX) && (fY >= tRuler.fY) && (fX <= tRuler.fX + tRuler.fW) && (fY <= tRuler.fY + tRuler.fH) ) {
		iFrame = __xuiTimeLineFrameAt(pData, fX - tRuler.fX + fOffsetX);
		fPlayheadX = tRuler.fX + (float)pData->iCurrentFrame * pData->fFrameWidth - fOffsetX;
		pHit->iType = (fabsf(fX - fPlayheadX) <= 5.0f) ? XUI_TIMELINE_HIT_PLAYHEAD : XUI_TIMELINE_HIT_RULER;
		pHit->iFrame = iFrame;
		pHit->tRect = (xui_rect_t){tRuler.fX + (float)iFrame * pData->fFrameWidth - fOffsetX, tRuler.fY, pData->fFrameWidth, tRuler.fH};
		return XUI_OK;
	}
	if ( (fX >= tLayer.fX) && (fY >= tLayer.fY) && (fX <= tLayer.fX + tLayer.fW) && (fY <= tLayer.fY + tLayer.fH) ) {
		fContentY = fY - tLayer.fY + fOffsetY;
		iLayer = __xuiTimeLineLayerAt(pData, fContentY);
		if ( iLayer >= 0 ) {
			fRowY = tLayer.fY + __xuiTimeLineLayerTop(pData, iLayer) - fOffsetY;
			fRowH = __xuiTimeLineLayerHeight(pData, iLayer);
			pHit->iLayer = iLayer;
			pHit->iLayerId = pData->arrLayers[iLayer].iId;
			pHit->tRect = (xui_rect_t){tLayer.fX, fRowY, tLayer.fW, fRowH};
			if ( pData->bShowVisibilityFeature ) {
				xui_rect_t tIcon = __xuiTimeLineLayerIconRect(pData, XUI_TIMELINE_HIT_LAYER_VISIBLE, fRowY, fRowH);
				if ( (fX >= tIcon.fX) && (fY >= tIcon.fY) && (fX <= tIcon.fX + tIcon.fW) && (fY <= tIcon.fY + tIcon.fH) ) {
					pHit->iType = XUI_TIMELINE_HIT_LAYER_VISIBLE;
					pHit->tRect = tIcon;
					return XUI_OK;
				}
			}
			if ( pData->bShowLockFeature ) {
				xui_rect_t tIcon = __xuiTimeLineLayerIconRect(pData, XUI_TIMELINE_HIT_LAYER_LOCK, fRowY, fRowH);
				if ( (fX >= tIcon.fX) && (fY >= tIcon.fY) && (fX <= tIcon.fX + tIcon.fW) && (fY <= tIcon.fY + tIcon.fH) ) {
					pHit->iType = XUI_TIMELINE_HIT_LAYER_LOCK;
					pHit->tRect = tIcon;
					return XUI_OK;
				}
			}
			pHit->iType = (fX < tLayer.fX + pData->fLayerHeaderWidth - 46.0f) ? XUI_TIMELINE_HIT_LAYER_NAME : XUI_TIMELINE_HIT_LAYER_ROW;
			return XUI_OK;
		}
	}
	if ( (fX >= tFrame.fX) && (fY >= tFrame.fY) && (fX <= tFrame.fX + tFrame.fW) && (fY <= tFrame.fY + tFrame.fH) ) {
		fContentX = fX - tFrame.fX + fOffsetX;
		fContentY = fY - tFrame.fY + fOffsetY;
		iLayer = __xuiTimeLineLayerAt(pData, fContentY);
		iFrame = __xuiTimeLineFrameAt(pData, fContentX);
		if ( (iLayer >= 0) && (iFrame >= 0) ) {
			pHit->iType = XUI_TIMELINE_HIT_FRAME;
			pHit->iLayer = iLayer;
			pHit->iLayerId = pData->arrLayers[iLayer].iId;
			pHit->iFrame = iFrame;
			pHit->tRect = (xui_rect_t){tFrame.fX + (float)iFrame * pData->fFrameWidth - fOffsetX, tFrame.fY + __xuiTimeLineLayerTop(pData, iLayer) - fOffsetY, pData->fFrameWidth, __xuiTimeLineLayerHeight(pData, iLayer)};
			for ( i = pData->iSpanCount - 1; i >= 0; i-- ) {
				xui_timeline_span_t* pSpan = &pData->arrSpans[i];
				if ( (pSpan->iLayer == iLayer) && (iFrame >= pSpan->iStartFrame) && (iFrame <= pSpan->iEndFrame) ) {
					pHit->iType = XUI_TIMELINE_HIT_SPAN;
					pHit->iSpanId = pSpan->iId;
					break;
				}
			}
			if ( (pHit->iType == XUI_TIMELINE_HIT_FRAME) && (__xuiTimeLineFindSelection(pData, iLayer, iFrame) >= 0) ) {
				pHit->iType = XUI_TIMELINE_HIT_SELECTION;
			}
			return XUI_OK;
		}
	}
	return XUI_OK;
}

static int __xuiTimeLineHitWorld(xui_widget pWidget, xui_timeline_view_data_t* pData, float fWorldX, float fWorldY, xui_timeline_hit_t* pHit)
{
	xui_rect_t tWorld;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	return __xuiTimeLineHitLocal(pWidget, pData, fWorldX - tWorld.fX, fWorldY - tWorld.fY, pHit);
}

static int __xuiTimeLinePointerMove(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_timeline_hit_t tHit;
	int iFrame;
	int iRet;
	int bTargetChanged;
	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	if ( (pData->bDraggingPlayhead || pData->bDraggingSelection) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		(void)__xuiTimeLineHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
		if ( pData->bDraggingPlayhead ) {
			iFrame = (tHit.iFrame >= 0) ? tHit.iFrame : __xuiTimeLineFrameAt(pData, (pEvent->fX - xuiWidgetGetWorldRect(pWidget).fX - pData->fLayerHeaderWidth));
			if ( iFrame >= 0 ) (void)__xuiTimeLineSetCurrentFrameInternal(pWidget, pData, iFrame);
			return XUI_EVENT_DISPATCH_STOP;
		}
		bTargetChanged = 0;
		if ( tHit.iLayer >= 0 && tHit.iFrame >= 0 ) {
			bTargetChanged = (pData->iDragLayer != tHit.iLayer) || (pData->iDragFrame != tHit.iFrame);
			pData->iDragLayer = tHit.iLayer;
			pData->iDragFrame = tHit.iFrame;
		}
		if ( fabsf(pEvent->fX - pData->fDragStartX) > 3.0f || fabsf(pEvent->fY - pData->fDragStartY) > 3.0f ) pData->bDragMoved = 1;
		if ( pData->bDragMoved && bTargetChanged ) {
			iRet = __xuiTimeLineApplyDragSelection(pWidget, pData);
			if ( iRet != XUI_OK ) return iRet;
		}
		(void)__xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	(void)__xuiTimeLineHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( (tHit.iLayer != pData->iHoverLayer) || (tHit.iFrame != pData->iHoverFrame) || (tHit.iSpanId != pData->iHoverSpanId) || (tHit.iType != pData->tHoverHit.iType) ) {
		pData->tHoverHit = tHit;
		pData->iHoverLayer = tHit.iLayer;
		pData->iHoverFrame = tHit.iFrame;
		pData->iHoverSpanId = tHit.iSpanId;
		(void)__xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiTimeLinePointerDown(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_timeline_hit_t tHit;
	int bSelected;
	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	(void)__xuiTimeLineHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	pData->tContextHit = tHit;
	if ( tHit.iType == XUI_TIMELINE_HIT_RULER || tHit.iType == XUI_TIMELINE_HIT_PLAYHEAD ) {
		if ( tHit.iFrame >= 0 ) (void)__xuiTimeLineSetCurrentFrameInternal(pWidget, pData, tHit.iFrame);
		pData->bDraggingPlayhead = 1;
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( tHit.iType == XUI_TIMELINE_HIT_LAYER_VISIBLE ) {
		(void)xuiTimeLineViewSetLayerVisible(pWidget, tHit.iLayer, !pData->arrLayers[tHit.iLayer].bVisible);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( tHit.iType == XUI_TIMELINE_HIT_LAYER_LOCK ) {
		(void)xuiTimeLineViewSetLayerLocked(pWidget, tHit.iLayer, !pData->arrLayers[tHit.iLayer].bLocked);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( tHit.iType == XUI_TIMELINE_HIT_LAYER_ROW || tHit.iType == XUI_TIMELINE_HIT_LAYER_NAME ) {
		(void)__xuiTimeLineSetActiveLayer(pWidget, pData, tHit.iLayer);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( tHit.iLayer >= 0 && tHit.iFrame >= 0 ) {
		__xuiTimeLineStoreSelection(pData, pData->arrDragBaseSelection, &pData->iDragBaseSelectionCount);
		pData->iDragModifiers = pEvent->iModifiers;
		pData->bDragSelectingState = 1;
		(void)__xuiTimeLineSetCurrentFrameInternal(pWidget, pData, tHit.iFrame);
		(void)__xuiTimeLineSetActiveLayer(pWidget, pData, tHit.iLayer);
		if ( (pEvent->iModifiers & XUI_MOD_SHIFT) != 0 && pData->iAnchorLayer >= 0 && pData->iAnchorFrame >= 0 ) {
			if ( __xuiTimeLineClearSelectionRaw(pData) | __xuiTimeLineSelectRangeRaw(pData, pData->iAnchorLayer, pData->iAnchorFrame, tHit.iLayer, tHit.iFrame, 1) ) {
				(void)__xuiTimeLineSelectionNotify(pWidget, pData);
			}
		} else if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 ) {
			bSelected = !xuiTimeLineViewIsFrameSelected(pWidget, tHit.iLayer, tHit.iFrame);
			pData->bDragSelectingState = bSelected;
			if ( __xuiTimeLineSelectFrameRaw(pData, tHit.iLayer, tHit.iFrame, bSelected) ) {
				(void)__xuiTimeLineSelectionNotify(pWidget, pData);
			}
			pData->iAnchorLayer = tHit.iLayer;
			pData->iAnchorFrame = tHit.iFrame;
		} else {
			if ( __xuiTimeLineClearSelectionRaw(pData) | __xuiTimeLineSelectFrameRaw(pData, tHit.iLayer, tHit.iFrame, 1) ) {
				(void)__xuiTimeLineSelectionNotify(pWidget, pData);
			}
			pData->iAnchorLayer = tHit.iLayer;
			pData->iAnchorFrame = tHit.iFrame;
		}
		if ( pData->onFrameClick != NULL ) pData->onFrameClick(pWidget, tHit.iLayer, tHit.iFrame, pEvent->iButton, pEvent->iModifiers, pData->pFrameClickUser);
		pData->iClickCount++;
		pData->bDraggingSelection = 1;
		pData->bDragMoved = 0;
		pData->iDragLayer = tHit.iLayer;
		pData->iDragFrame = tHit.iFrame;
		pData->fDragStartX = pEvent->fX;
		pData->fDragStartY = pEvent->fY;
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTimeLinePointerUp(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	int iRet;
	int iInvalidateRet;
	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) != pWidget ) return XUI_OK;
	iRet = XUI_OK;
	if ( pData->bDraggingSelection && pData->bDragMoved && pData->iAnchorLayer >= 0 && pData->iAnchorFrame >= 0 && pData->iDragLayer >= 0 && pData->iDragFrame >= 0 ) {
		iRet = __xuiTimeLineApplyDragSelection(pWidget, pData);
	}
	pData->bDraggingPlayhead = 0;
	__xuiTimeLineResetDragSelection(pData);
	(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	iInvalidateRet = __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	if ( iInvalidateRet != XUI_OK ) return iInvalidateRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTimeLineDoubleClick(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_timeline_hit_t tHit;
	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	(void)__xuiTimeLineHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( tHit.iLayer >= 0 && tHit.iFrame >= 0 ) {
		if ( pData->onFrameDoubleClick != NULL ) pData->onFrameDoubleClick(pWidget, tHit.iLayer, tHit.iFrame, pEvent->iButton, pEvent->iModifiers, pData->pFrameClickUser);
		pData->iClickCount++;
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static void __xuiTimeLineMenuItem(xui_widget pMenu, const char* sText, int iType, uint32_t iState, int iValue)
{
	xui_menu_item_t tItem;

	memset(&tItem, 0, sizeof(tItem));
	tItem.sText = sText;
	tItem.iType = iType;
	tItem.iState = iState;
	tItem.iValue = iValue;
	(void)xuiMenuAddItem(pMenu, &tItem);
}

static uint32_t __xuiTimeLineMenuState(int bEnabled)
{
	return bEnabled ? XUI_MENU_ITEM_ENABLED : 0u;
}

static int __xuiTimeLineMenuTitleIndexForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_TIMELINE_MENU_LAYER_RENAME: return 0;
	case XUI_TIMELINE_MENU_LAYER_SHOW_HIDE: return 1;
	case XUI_TIMELINE_MENU_LAYER_LOCK_UNLOCK: return 2;
	case XUI_TIMELINE_MENU_LAYER_ADD: return 3;
	case XUI_TIMELINE_MENU_LAYER_DELETE: return 4;
	case XUI_TIMELINE_MENU_LAYER_MOVE_UP: return 5;
	case XUI_TIMELINE_MENU_LAYER_MOVE_DOWN: return 6;
	case XUI_TIMELINE_MENU_FRAME_INSERT: return 7;
	case XUI_TIMELINE_MENU_FRAME_KEY: return 8;
	case XUI_TIMELINE_MENU_FRAME_BLANK_KEY: return 9;
	case XUI_TIMELINE_MENU_FRAME_CLEAR: return 10;
	case XUI_TIMELINE_MENU_SPAN_CREATE: return 11;
	case XUI_TIMELINE_MENU_SPAN_CLEAR: return 12;
	case XUI_TIMELINE_MENU_SPAN_CREATE_FROM_SELECTION: return 13;
	default: return -1;
	}
}

static int __xuiTimeLineMenuTitleTranslationForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_TIMELINE_MENU_LAYER_RENAME: return XUI_TR_TIMELINE_RENAME;
	case XUI_TIMELINE_MENU_LAYER_SHOW_HIDE: return XUI_TR_TIMELINE_VISIBLE;
	case XUI_TIMELINE_MENU_LAYER_LOCK_UNLOCK: return XUI_TR_TIMELINE_LOCKED;
	case XUI_TIMELINE_MENU_LAYER_ADD: return XUI_TR_TIMELINE_ADD_LAYER;
	case XUI_TIMELINE_MENU_LAYER_DELETE: return XUI_TR_TIMELINE_DELETE_LAYER;
	case XUI_TIMELINE_MENU_LAYER_MOVE_UP: return XUI_TR_TIMELINE_MOVE_UP;
	case XUI_TIMELINE_MENU_LAYER_MOVE_DOWN: return XUI_TR_TIMELINE_MOVE_DOWN;
	case XUI_TIMELINE_MENU_FRAME_INSERT: return XUI_TR_TIMELINE_INSERT_FRAME;
	case XUI_TIMELINE_MENU_FRAME_KEY: return XUI_TR_TIMELINE_INSERT_KEYFRAME;
	case XUI_TIMELINE_MENU_FRAME_BLANK_KEY: return XUI_TR_TIMELINE_INSERT_BLANK_KEYFRAME;
	case XUI_TIMELINE_MENU_FRAME_CLEAR: return XUI_TR_TIMELINE_CLEAR_KEYFRAME;
	case XUI_TIMELINE_MENU_SPAN_CREATE: return XUI_TR_TIMELINE_CREATE_SPAN;
	case XUI_TIMELINE_MENU_SPAN_CLEAR: return XUI_TR_TIMELINE_CLEAR_SPAN;
	case XUI_TIMELINE_MENU_SPAN_CREATE_FROM_SELECTION: return XUI_TR_TIMELINE_CREATE_SPAN_FROM_SELECTION;
	default: return 0;
	}
}

static const char* __xuiTimeLineMenuTitleForCommand(xui_widget pWidget, xui_timeline_view_data_t* pData, int iCommand)
{
	xui_context pContext;
	int iIndex;
	int iTranslation;

	if ( pData == NULL ) return "";
	iIndex = __xuiTimeLineMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_TIMELINE_MENU_TITLE_COUNT ) return "";
	if ( pData->arrMenuTitle[iIndex] != NULL ) return pData->arrMenuTitle[iIndex];
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	iTranslation = __xuiTimeLineMenuTitleTranslationForCommand(iCommand);
	return (iTranslation != 0) ? xuiTranslate(pContext, iTranslation) : "";
}

static int __xuiTimeLineCloseMenus(xui_timeline_view_data_t* pData)
{
	int bClosed;

	if ( pData == NULL ) return 0;
	bClosed = 0;
	if ( (pData->pLayerMenu != NULL) && xuiMenuIsOpen(pData->pLayerMenu) ) {
		(void)xuiMenuClose(pData->pLayerMenu);
		bClosed = 1;
	}
	if ( (pData->pFrameMenu != NULL) && xuiMenuIsOpen(pData->pFrameMenu) ) {
		(void)xuiMenuClose(pData->pFrameMenu);
		bClosed = 1;
	}
	return bClosed;
}

static int __xuiTimeLineBuildLayerMenu(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_timeline_hit_t* pHit)
{
	xui_timeline_layer_t* pLayer;
	uint32_t iState;

	if ( (pData == NULL) || (pData->pLayerMenu == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiMenuClear(pData->pLayerMenu);
	if ( (pHit->iLayer < 0) || (pHit->iLayer >= pData->iLayerCount) ) return XUI_OK;
	pLayer = &pData->arrLayers[pHit->iLayer];
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_RENAME), XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, XUI_TIMELINE_MENU_LAYER_RENAME);
	iState = __xuiTimeLineMenuState(pData->bShowVisibilityFeature);
	if ( pLayer->bVisible ) iState |= XUI_MENU_ITEM_CHECKED;
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_SHOW_HIDE), XUI_MENU_ITEM_CHECK, iState, XUI_TIMELINE_MENU_LAYER_SHOW_HIDE);
	iState = __xuiTimeLineMenuState(pData->bShowLockFeature);
	if ( pLayer->bLocked ) iState |= XUI_MENU_ITEM_CHECKED;
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_LOCK_UNLOCK), XUI_MENU_ITEM_CHECK, iState, XUI_TIMELINE_MENU_LAYER_LOCK_UNLOCK);
	(void)xuiMenuAddSeparator(pData->pLayerMenu);
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_ADD), XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, XUI_TIMELINE_MENU_LAYER_ADD);
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_DELETE), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(pData->iLayerCount > 1), XUI_TIMELINE_MENU_LAYER_DELETE);
	(void)xuiMenuAddSeparator(pData->pLayerMenu);
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_MOVE_UP), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(pHit->iLayer > 0), XUI_TIMELINE_MENU_LAYER_MOVE_UP);
	__xuiTimeLineMenuItem(pData->pLayerMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_LAYER_MOVE_DOWN), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState((pHit->iLayer + 1) < pData->iLayerCount), XUI_TIMELINE_MENU_LAYER_MOVE_DOWN);
	return XUI_OK;
}

static int __xuiTimeLineBuildFrameMenu(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_timeline_hit_t* pHit)
{
	xui_timeline_layer_t* pLayer;
	int bEditable;
	int bSpanSelection;
	int iStart;
	int iEnd;

	if ( (pData == NULL) || (pData->pFrameMenu == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiMenuClear(pData->pFrameMenu);
	pLayer = (pHit->iLayer >= 0 && pHit->iLayer < pData->iLayerCount) ? &pData->arrLayers[pHit->iLayer] : NULL;
	bEditable = (pLayer != NULL) && (pLayer->bLocked == 0) && (pHit->iFrame >= 0) && (pHit->iFrame < pData->iTotalFrames);
	bSpanSelection = bEditable &&
		(__xuiTimeLineFindSelection(pData, pHit->iLayer, pHit->iFrame) >= 0) &&
		__xuiTimeLineSelectionRangeForLayer(pData, pHit->iLayer, &iStart, &iEnd) &&
		(iEnd > iStart);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_FRAME_INSERT), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable), XUI_TIMELINE_MENU_FRAME_INSERT);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_FRAME_KEY), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable), XUI_TIMELINE_MENU_FRAME_KEY);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_FRAME_BLANK_KEY), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable), XUI_TIMELINE_MENU_FRAME_BLANK_KEY);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_FRAME_CLEAR), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable), XUI_TIMELINE_MENU_FRAME_CLEAR);
	(void)xuiMenuAddSeparator(pData->pFrameMenu);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, bSpanSelection ? XUI_TIMELINE_MENU_SPAN_CREATE_FROM_SELECTION : XUI_TIMELINE_MENU_SPAN_CREATE), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable), XUI_TIMELINE_MENU_SPAN_CREATE);
	__xuiTimeLineMenuItem(pData->pFrameMenu, __xuiTimeLineMenuTitleForCommand(pWidget, pData, XUI_TIMELINE_MENU_SPAN_CLEAR), XUI_MENU_ITEM_NORMAL, __xuiTimeLineMenuState(bEditable && pHit->iSpanId > 0), XUI_TIMELINE_MENU_SPAN_CLEAR);
	return XUI_OK;
}

static xui_widget __xuiTimeLineMenuForHit(const xui_timeline_view_data_t* pData, const xui_timeline_hit_t* pHit)
{
	if ( (pData == NULL) || (pHit == NULL) ) return NULL;
	if ( (pHit->iType == XUI_TIMELINE_HIT_LAYER_ROW) ||
	     (pHit->iType == XUI_TIMELINE_HIT_LAYER_NAME) ||
	     (pHit->iType == XUI_TIMELINE_HIT_LAYER_VISIBLE) ||
	     (pHit->iType == XUI_TIMELINE_HIT_LAYER_LOCK) ) {
		return pData->pLayerMenu;
	}
	if ( (pHit->iType == XUI_TIMELINE_HIT_FRAME) ||
	     (pHit->iType == XUI_TIMELINE_HIT_SELECTION) ||
	     (pHit->iType == XUI_TIMELINE_HIT_SPAN) ) {
		return pData->pFrameMenu;
	}
	return NULL;
}

static int __xuiTimeLineOpenContextMenuAt(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_timeline_hit_t* pHit, float fX, float fY)
{
	xui_widget pMenu;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pMenu = __xuiTimeLineMenuForHit(pData, pHit);
	(void)__xuiTimeLineCloseMenus(pData);
	if ( pMenu == NULL ) return XUI_OK;
	pData->tContextHit = *pHit;
	if ( pMenu == pData->pLayerMenu ) {
		iRet = __xuiTimeLineBuildLayerMenu(pWidget, pData, pHit);
	} else {
		iRet = __xuiTimeLineBuildFrameMenu(pWidget, pData, pHit);
	}
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->onContextOpening != NULL && !pData->onContextOpening(pWidget, &pData->tContextHit, pData->pContextUser) ) {
		return XUI_OK;
	}
	return xuiMenuOpenAt(pMenu, pWidget, fX, fY);
}

static void __xuiTimeLineNotifyContextCommand(xui_widget pWidget, xui_timeline_view_data_t* pData, int iCommand)
{
	if ( (pData != NULL) && (pData->onContextCommand != NULL) ) {
		pData->onContextCommand(pWidget, iCommand, &pData->tContextHit, pData->pContextUser);
	}
}

static int __xuiTimeLineRunContextCommand(xui_widget pWidget, xui_timeline_view_data_t* pData, int iCommand)
{
	xui_timeline_layer_t* pLayer;
	int iLayer;
	int iFrame;
	int iRet;
	int iNewLayer;
	int iStart;
	int iEnd;
	int bEditable;
	int bUseSelectionRange;
	char sName[64];

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = XUI_OK;
	iLayer = pData->tContextHit.iLayer;
	iFrame = pData->tContextHit.iFrame;
	pLayer = (iLayer >= 0 && iLayer < pData->iLayerCount) ? &pData->arrLayers[iLayer] : NULL;
	bEditable = (pLayer != NULL) && (pLayer->bLocked == 0) && (iFrame >= 0) && (iFrame < pData->iTotalFrames);
	switch ( iCommand ) {
	case XUI_TIMELINE_MENU_LAYER_RENAME:
		break;
	case XUI_TIMELINE_MENU_LAYER_SHOW_HIDE:
		if ( (pLayer != NULL) && pData->bShowVisibilityFeature ) {
			iRet = xuiTimeLineViewSetLayerVisible(pWidget, iLayer, !pLayer->bVisible);
		}
		break;
	case XUI_TIMELINE_MENU_LAYER_LOCK_UNLOCK:
		if ( (pLayer != NULL) && pData->bShowLockFeature ) {
			iRet = xuiTimeLineViewSetLayerLocked(pWidget, iLayer, !pLayer->bLocked);
		}
		break;
	case XUI_TIMELINE_MENU_LAYER_ADD:
		snprintf(sName, sizeof(sName), "Layer %d", pData->iLayerCount + 1);
		iRet = xuiTimeLineViewAddLayer(pWidget, sName, &iNewLayer);
		if ( (iRet == XUI_OK) && (iNewLayer >= 0) && (iLayer >= 0) && (iLayer + 1 < pData->iLayerCount) ) {
			iRet = xuiTimeLineViewMoveLayer(pWidget, iNewLayer, iLayer + 1);
			iNewLayer = iLayer + 1;
		}
		if ( (iRet == XUI_OK) && (iNewLayer >= 0) ) {
			iRet = __xuiTimeLineSetActiveLayer(pWidget, pData, iNewLayer);
		}
		break;
	case XUI_TIMELINE_MENU_LAYER_DELETE:
		if ( (pLayer != NULL) && (pData->iLayerCount > 1) ) {
			iRet = xuiTimeLineViewRemoveLayer(pWidget, iLayer);
			if ( iRet == XUI_OK ) {
				iRet = __xuiTimeLineSetActiveLayer(pWidget, pData, __xuiTimeLineClampInt(iLayer, 0, pData->iLayerCount - 1));
			}
		}
		break;
	case XUI_TIMELINE_MENU_LAYER_MOVE_UP:
		if ( (pLayer != NULL) && (iLayer > 0) ) {
			iRet = xuiTimeLineViewMoveLayer(pWidget, iLayer, iLayer - 1);
			if ( iRet == XUI_OK ) iRet = __xuiTimeLineSetActiveLayer(pWidget, pData, iLayer - 1);
		}
		break;
	case XUI_TIMELINE_MENU_LAYER_MOVE_DOWN:
		if ( (pLayer != NULL) && (iLayer + 1 < pData->iLayerCount) ) {
			iRet = xuiTimeLineViewMoveLayer(pWidget, iLayer, iLayer + 1);
			if ( iRet == XUI_OK ) iRet = __xuiTimeLineSetActiveLayer(pWidget, pData, iLayer + 1);
		}
		break;
	case XUI_TIMELINE_MENU_FRAME_INSERT:
		if ( bEditable ) iRet = xuiTimeLineViewSetFrame(pWidget, iLayer, iFrame, XUI_TIMELINE_FRAME_NORMAL, NULL);
		break;
	case XUI_TIMELINE_MENU_FRAME_KEY:
		if ( bEditable ) iRet = xuiTimeLineViewSetFrame(pWidget, iLayer, iFrame, XUI_TIMELINE_FRAME_KEY, NULL);
		break;
	case XUI_TIMELINE_MENU_FRAME_BLANK_KEY:
		if ( bEditable ) iRet = xuiTimeLineViewSetFrame(pWidget, iLayer, iFrame, XUI_TIMELINE_FRAME_BLANK_KEY, NULL);
		break;
	case XUI_TIMELINE_MENU_FRAME_CLEAR:
		if ( bEditable ) iRet = xuiTimeLineViewClearFrame(pWidget, iLayer, iFrame);
		break;
	case XUI_TIMELINE_MENU_SPAN_CREATE:
	case XUI_TIMELINE_MENU_SPAN_CREATE_FROM_SELECTION:
		if ( bEditable ) {
			bUseSelectionRange = (__xuiTimeLineFindSelection(pData, iLayer, iFrame) >= 0) &&
				__xuiTimeLineSelectionRangeForLayer(pData, iLayer, &iStart, &iEnd) &&
				(iEnd > iStart);
			if ( !bUseSelectionRange ) {
				iStart = iFrame;
				iEnd = __xuiTimeLineClampInt(iFrame + 5, 0, pData->iTotalFrames - 1);
			}
			iRet = xuiTimeLineViewAddSpan(pWidget, iLayer, iStart, iEnd, XUI_TIMELINE_SPAN_HOLD, "Span", NULL);
		}
		break;
	case XUI_TIMELINE_MENU_SPAN_CLEAR:
		if ( pData->tContextHit.iSpanId > 0 ) iRet = xuiTimeLineViewRemoveSpan(pWidget, pData->tContextHit.iSpanId);
		break;
	default:
		break;
	}
	__xuiTimeLineNotifyContextCommand(pWidget, pData, iCommand);
	return iRet;
}

static void __xuiTimeLineMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pWidget;
	xui_timeline_view_data_t* pData;

	(void)pMenu;
	(void)iIndex;
	pWidget = (xui_widget)pUser;
	pData = __xuiTimeLineViewGetData(pWidget);
	(void)__xuiTimeLineRunContextCommand(pWidget, pData, iValue);
}

static int __xuiTimeLineContextMenu(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_timeline_hit_t tHit;
	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	(void)__xuiTimeLineHitWorld(pWidget, pData, pEvent->fX, pEvent->fY, &tHit);
	if ( __xuiTimeLineOpenContextMenuAt(pWidget, pData, &tHit, pEvent->fX, pEvent->fY) != XUI_OK ) return XUI_OK;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTimeLineKeyDown(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	int iLayer;
	int iFrame;
	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) != pWidget ) return XUI_OK;
	iLayer = (pData->iActiveLayer >= 0) ? pData->iActiveLayer : 0;
	iFrame = pData->iCurrentFrame;
	switch ( pEvent->iKey ) {
	case XUI_KEY_LEFT:
		iFrame = pData->iCurrentFrame - 1;
		break;
	case XUI_KEY_RIGHT:
		iFrame = pData->iCurrentFrame + 1;
		break;
	case XUI_KEY_HOME:
		iFrame = 0;
		break;
	case XUI_KEY_END:
		iFrame = pData->iTotalFrames - 1;
		break;
	case XUI_KEY_UP:
		iLayer = iLayer - 1;
		break;
	case XUI_KEY_DOWN:
		iLayer = iLayer + 1;
		break;
	case XUI_KEY_CONTEXT_MENU:
		if ( pData->iLayerCount > 0 ) {
			xui_timeline_hit_t tHit;
			xui_rect_t tWorld;
			float fOffsetX;
			float fOffsetY;
			memset(&tHit, 0, sizeof(tHit));
			iLayer = __xuiTimeLineClampInt(iLayer, 0, pData->iLayerCount - 1);
			iFrame = __xuiTimeLineClampInt(iFrame, 0, pData->iTotalFrames - 1);
			__xuiTimeLineScrollOffset(pData, &fOffsetX, &fOffsetY);
			tHit.iType = XUI_TIMELINE_HIT_FRAME;
			tHit.iLayer = iLayer;
			tHit.iLayerId = pData->arrLayers[iLayer].iId;
			tHit.iFrame = iFrame;
			tHit.iSpanId = -1;
			tHit.tRect = (xui_rect_t){
				pData->fLayerHeaderWidth + iFrame * pData->fFrameWidth - fOffsetX,
				pData->fRulerHeight + __xuiTimeLineLayerTop(pData, iLayer) - fOffsetY,
				pData->fFrameWidth,
				__xuiTimeLineLayerHeight(pData, iLayer)
			};
			tWorld = xuiWidgetGetWorldRect(pWidget);
			(void)__xuiTimeLineOpenContextMenuAt(pWidget, pData, &tHit,
				tWorld.fX + tHit.tRect.fX + tHit.tRect.fW * 0.5f,
				tWorld.fY + tHit.tRect.fY + tHit.tRect.fH * 0.5f);
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_OK;
	default:
		return XUI_OK;
	}
	if ( iFrame != pData->iCurrentFrame ) {
		(void)__xuiTimeLineSetCurrentFrameInternal(pWidget, pData, iFrame);
		if ( (pEvent->iModifiers & XUI_MOD_SHIFT) != 0 && pData->iAnchorLayer >= 0 && pData->iAnchorFrame >= 0 ) {
			if ( __xuiTimeLineClearSelectionRaw(pData) | __xuiTimeLineSelectRangeRaw(pData, pData->iAnchorLayer, pData->iAnchorFrame, iLayer, pData->iCurrentFrame, 1) ) {
				(void)__xuiTimeLineSelectionNotify(pWidget, pData);
			}
		}
	}
	if ( iLayer != pData->iActiveLayer && pData->iLayerCount > 0 ) {
		(void)__xuiTimeLineSetActiveLayer(pWidget, pData, iLayer);
	}
	(void)xuiTimeLineViewEnsureFrameVisible(pWidget, pData->iActiveLayer, pData->iCurrentFrame);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTimeLineWheel(xui_widget pWidget, xui_timeline_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fOffsetX;
	float fOffsetY;
	float fLocalX;
	float fAnchorFrame;
	float fNewWidth;
	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) return XUI_OK;
	if ( (pEvent->iModifiers & XUI_MOD_CTRL) == 0 ) {
		pData->iHoverLayer = -1;
		pData->iHoverFrame = -1;
		pData->iHoverSpanId = -1;
		return XUI_OK;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX - pData->fLayerHeaderWidth;
	if ( fLocalX < 0.0f ) fLocalX = 0.0f;
	__xuiTimeLineScrollOffset(pData, &fOffsetX, &fOffsetY);
	fAnchorFrame = (fOffsetX + fLocalX) / pData->fFrameWidth;
	fNewWidth = pData->fFrameWidth * ((pEvent->fWheelY > 0.0f) ? 1.12f : 0.88f);
	fNewWidth = __xuiTimeLineClampFloat(fNewWidth, pData->fMinFrameWidth, pData->fMaxFrameWidth);
	if ( fNewWidth == pData->fFrameWidth ) return XUI_EVENT_DISPATCH_STOP;
	pData->fFrameWidth = fNewWidth;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	(void)xuiScrollFrameSetOffset(pData->pFrame, fAnchorFrame * pData->fFrameWidth - fLocalX, fOffsetY);
	(void)__xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTimeLineEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_timeline_view_data_t* pData;
	(void)pUser;
	pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_OK;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiTimeLinePointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( !pData->bDraggingSelection && !pData->bDraggingPlayhead ) {
			pData->iHoverLayer = -1;
			pData->iHoverFrame = -1;
			pData->iHoverSpanId = -1;
			(void)__xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDraggingPlayhead = 0;
		__xuiTimeLineResetDragSelection(pData);
		return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTimeLinePointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTimeLinePointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		return __xuiTimeLineDoubleClick(pWidget, pData, pEvent);
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiTimeLineContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiTimeLineKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiTimeLineWheel(pWidget, pData, pEvent);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
		return XUI_OK;
	default:
		return XUI_OK;
	}
}

static void __xuiTimeLineFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_timeline_view_data_t* pData;
	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return;
	pData->iHoverLayer = -1;
	pData->iHoverFrame = -1;
	pData->iHoverSpanId = -1;
	pData->iChangeCount++;
	(void)__xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTimeLineContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_timeline_view_data_t* pData;
	float fRows;
	(void)pWidget;
	(void)tConstraint;
	pData = (xui_timeline_view_data_t*)pUser;
	if ( (pData == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fRows = (float)((pData->iLayerCount > 0 && pData->iLayerCount < 8) ? pData->iLayerCount : 8);
	if ( fRows < 3.0f ) fRows = 3.0f;
	pSize->fX = xuiInternalSnapSize(pData->fLayerHeaderWidth + 360.0f);
	pSize->fY = xuiInternalSnapSize(pData->fRulerHeight + pData->fRowHeight * fRows + 12.0f);
	return XUI_OK;
}

static int __xuiTimeLineArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_timeline_view_data_t* pData;
	xui_rect_t tFrame;
	int iRet;
	(void)tContentRect;
	pData = (xui_timeline_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tFrame = __xuiTimeLineFrameRect(pWidget, pData);
	iRet = __xuiTimeLineUpdateContentSize(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pFrame, tFrame);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameLayout(pData->pFrame);
	return iRet;
}

static void __xuiTimeLineDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
}

static void __xuiTimeLineDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTimeLineInitViewport(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	(void)xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetFocusable(pData->pViewport, 0);
	(void)xuiWidgetSetTabStop(pData->pViewport, 0);
	(void)xuiWidgetSetCachePolicy(pData->pViewport, &tPolicy);
	return xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiTimeLineViewportRender, pWidget);
}

static int __xuiTimeLineCreateFrame(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;
	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = pData->iScrollbarMode;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_AUTO;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fRowHeight * 3.0f;
	tFrameDesc.iTrackColor = pData->tColors.iTrackColor;
	tFrameDesc.iThumbColor = pData->tColors.iThumbColor;
	tFrameDesc.iHoverColor = pData->tColors.iScrollbarHoverColor;
	tFrameDesc.iActiveColor = pData->tColors.iScrollbarActiveColor;
	tFrameDesc.iFocusColor = pData->tColors.iScrollbarFocusColor;
	tFrameDesc.iDisabledColor = pData->tColors.iScrollbarDisabledColor;
	iRet = xuiScrollFrameCreate(xuiWidgetGetContext(pWidget), &pData->pFrame, &tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pFrame);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	(void)xuiWidgetSetFlowMode(pData->pFrame, XUI_FLOW_ABSOLUTE);
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiTimeLineFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiTimeLineInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTimeLineApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
	}
	return iRet;
}

static void __xuiTimeLineDestroyMenu(xui_widget* ppMenu)
{
	xui_widget pPopup;

	if ( (ppMenu == NULL) || (*ppMenu == NULL) ) return;
	pPopup = xuiMenuGetPopupWidget(*ppMenu);
	if ( pPopup != NULL ) {
		xuiWidgetDestroy(pPopup);
	} else {
		xuiWidgetDestroy(*ppMenu);
	}
	*ppMenu = NULL;
}

static int __xuiTimeLineInitMenus(xui_widget pWidget, xui_timeline_view_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pLayerMenu, &tDesc);
	if ( iRet == XUI_OK ) iRet = xuiMenuSetSelect(pData->pLayerMenu, __xuiTimeLineMenuSelect, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pFrameMenu, &tDesc);
	if ( iRet == XUI_OK ) iRet = xuiMenuSetSelect(pData->pFrameMenu, __xuiTimeLineMenuSelect, pWidget);
	if ( iRet != XUI_OK ) {
		__xuiTimeLineDestroyMenu(&pData->pLayerMenu);
		__xuiTimeLineDestroyMenu(&pData->pFrameMenu);
	}
	return iRet;
}

static int __xuiTimeLineInitEvents(xui_widget pWidget)
{
	int iRet;
	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTimeLineEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTimeLineEvent, NULL);
	return iRet;
}

static int __xuiTimeLineInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_timeline_view_data_t* pData;
	const xui_timeline_view_desc_t* pDesc;
	int iRet;
	(void)pUser;
	pData = (xui_timeline_view_data_t*)pTypeData;
	pDesc = (const xui_timeline_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTimeLineDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTimeLineDefaults(pData);
	__xuiTimeLineApplyDesc(pData, pDesc);
	if ( pData->pFont == NULL ) pData->pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiTimeLineCreateFrame(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTimeLineInitMenus(pWidget, pData);
	if ( iRet != XUI_OK ) {
		if ( pData->pFrame != NULL ) xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	iRet = __xuiTimeLineInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		__xuiTimeLineDestroyMenu(&pData->pLayerMenu);
		__xuiTimeLineDestroyMenu(&pData->pFrameMenu);
		if ( pData->pFrame != NULL ) xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	return __xuiTimeLineUpdateContentSize(pWidget, pData);
}

static void __xuiTimeLineDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_timeline_view_data_t* pData;
	int i;
	(void)pWidget;
	(void)pUser;
	pData = (xui_timeline_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiTimeLineDestroyMenu(&pData->pLayerMenu);
		__xuiTimeLineDestroyMenu(&pData->pFrameMenu);
		for ( i = 0; i < XUI_TIMELINE_MENU_TITLE_COUNT; ++i ) {
			xrtFree(pData->arrMenuTitle[i]);
			pData->arrMenuTitle[i] = NULL;
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static xui_timeline_view_data_t* __xuiTimeLineViewGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;
	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "timelineview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_timeline_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiTimeLineViewGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "timelineview");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "timelineview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_timeline_view_data_t);
	tDesc.onInit = __xuiTimeLineInit;
	tDesc.onDestroy = __xuiTimeLineDestroy;
	tDesc.onContentMeasure = __xuiTimeLineContentMeasure;
	tDesc.onLayoutArrange = __xuiTimeLineArrange;
	tDesc.onCacheRender = __xuiTimeLineCacheRender;
	__xuiTimeLineDefaultLayout(&tLayout);
	__xuiTimeLineDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	return pType;
}

XUI_API int xuiTimeLineViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_timeline_view_desc_t* pDesc)
{
	xui_widget_type pType;
	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiTimeLineDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiTimeLineViewGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_widget xuiTimeLineViewGetFrameWidget(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiTimeLineViewGetViewportWidget(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_scroll_model_t* xuiTimeLineViewGetModel(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API int xuiTimeLineViewClear(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iLayerCount = 0;
	pData->iFrameRecordCount = 0;
	pData->iSpanCount = 0;
	pData->iSelectionCount = 0;
	pData->iDragBaseSelectionCount = 0;
	pData->iActiveLayer = -1;
	pData->iAnchorLayer = -1;
	pData->iAnchorFrame = -1;
	pData->iHoverLayer = -1;
	pData->iHoverFrame = -1;
	pData->iHoverSpanId = -1;
	__xuiTimeLineResetDragSelection(pData);
	pData->iNextLayerId = 1;
	pData->iNextSpanId = 1;
	pData->iChangeCount++;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiTimeLineViewGetFont(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiTimeLineViewSetFrameCount(xui_widget pWidget, int iFrameCount)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int i;
	if ( (pData == NULL) || (iFrameCount <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTotalFrames = iFrameCount;
	pData->iCurrentFrame = __xuiTimeLineClampInt(pData->iCurrentFrame, 0, pData->iTotalFrames - 1);
	for ( i = 0; i < pData->iFrameRecordCount; ) {
		if ( pData->arrFrames[i].iFrame >= pData->iTotalFrames ) pData->arrFrames[i] = pData->arrFrames[--pData->iFrameRecordCount];
		else i++;
	}
	for ( i = 0; i < pData->iSpanCount; ) {
		if ( pData->arrSpans[i].iStartFrame >= pData->iTotalFrames ) pData->arrSpans[i] = pData->arrSpans[--pData->iSpanCount];
		else {
			if ( pData->arrSpans[i].iEndFrame >= pData->iTotalFrames ) pData->arrSpans[i].iEndFrame = pData->iTotalFrames - 1;
			i++;
		}
	}
	for ( i = 0; i < pData->iSelectionCount; ) {
		if ( pData->arrSelection[i].iFrame >= pData->iTotalFrames ) pData->arrSelection[i] = pData->arrSelection[--pData->iSelectionCount];
		else i++;
	}
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	pData->iChangeCount++;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetFrameCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iTotalFrames : 0;
}

XUI_API int xuiTimeLineViewSetFrameRate(xui_widget pWidget, float fFrameRate)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (fFrameRate <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fFrameRate = fFrameRate;
	pData->iChangeCount++;
	return XUI_OK;
}

XUI_API float xuiTimeLineViewGetFrameRate(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->fFrameRate : 0.0f;
}

XUI_API int xuiTimeLineViewSetCurrentFrame(xui_widget pWidget, int iFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTimeLineSetCurrentFrameInternal(pWidget, pData, iFrame);
}

XUI_API int xuiTimeLineViewGetCurrentFrame(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iCurrentFrame : -1;
}

XUI_API int xuiTimeLineViewSetMetrics(xui_widget pWidget, float fLayerHeaderWidth, float fFrameWidth, float fRowHeight, float fRulerHeight)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fLayerHeaderWidth > 0.0f ) pData->fLayerHeaderWidth = fLayerHeaderWidth;
	if ( fFrameWidth > 0.0f ) pData->fFrameWidth = __xuiTimeLineClampFloat(fFrameWidth, pData->fMinFrameWidth, pData->fMaxFrameWidth);
	if ( fRowHeight > 0.0f ) pData->fRowHeight = fRowHeight;
	if ( fRulerHeight > 0.0f ) pData->fRulerHeight = fRulerHeight;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetMetrics(xui_widget pWidget, float* pLayerHeaderWidth, float* pFrameWidth, float* pRowHeight, float* pRulerHeight)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLayerHeaderWidth != NULL ) *pLayerHeaderWidth = pData->fLayerHeaderWidth;
	if ( pFrameWidth != NULL ) *pFrameWidth = pData->fFrameWidth;
	if ( pRowHeight != NULL ) *pRowHeight = pData->fRowHeight;
	if ( pRulerHeight != NULL ) *pRulerHeight = pData->fRulerHeight;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetFrameWidthRange(xui_widget pWidget, float fMinFrameWidth, float fMaxFrameWidth)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (fMinFrameWidth <= 0.0f) || (fMaxFrameWidth < fMinFrameWidth) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fMinFrameWidth = fMinFrameWidth;
	pData->fMaxFrameWidth = fMaxFrameWidth;
	return xuiTimeLineViewSetFrameWidth(pWidget, pData->fFrameWidth);
}

XUI_API int xuiTimeLineViewGetFrameWidthRange(xui_widget pWidget, float* pMinFrameWidth, float* pMaxFrameWidth)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMinFrameWidth != NULL ) *pMinFrameWidth = pData->fMinFrameWidth;
	if ( pMaxFrameWidth != NULL ) *pMaxFrameWidth = pData->fMaxFrameWidth;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetFrameWidth(xui_widget pWidget, float fFrameWidth)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (fFrameWidth <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fFrameWidth = __xuiTimeLineClampFloat(fFrameWidth, pData->fMinFrameWidth, pData->fMaxFrameWidth);
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiTimeLineViewGetFrameWidth(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->fFrameWidth : 0.0f;
}

XUI_API int xuiTimeLineViewSetFeatureFlags(xui_widget pWidget, int bShowVisibility, int bShowLock)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowVisibilityFeature = bShowVisibility ? 1 : 0;
	pData->bShowLockFeature = bShowLock ? 1 : 0;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetFeatureFlags(xui_widget pWidget, int* pShowVisibility, int* pShowLock)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pShowVisibility != NULL ) *pShowVisibility = pData->bShowVisibilityFeature;
	if ( pShowLock != NULL ) *pShowLock = pData->bShowLockFeature;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || !__xuiTimeLineScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iScrollbarMode = iMode;
	if ( pData->pFrame != NULL ) (void)xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetScrollbarMode(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiTimeLineViewAddLayer(xui_widget pWidget, const char* sName, int* pLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	xui_timeline_layer_t* pNew;
	int iLayer;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iLayerCount >= XUI_TIMELINE_LAYER_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	iLayer = pData->iLayerCount++;
	pNew = &pData->arrLayers[iLayer];
	memset(pNew, 0, sizeof(*pNew));
	pNew->iId = pData->iNextLayerId++;
	__xuiTimeLineCopy(pNew->sName, XUI_TIMELINE_NAME_CAPACITY, (sName != NULL) ? sName : "Layer");
	pNew->bVisible = 1;
	pNew->bLocked = 0;
	pNew->bSelected = (pData->iActiveLayer < 0);
	pNew->fHeight = pData->fRowHeight;
	pNew->iColor = XUI_COLOR_RGBA(74, 144, 226, 255);
	if ( pData->iActiveLayer < 0 ) pData->iActiveLayer = iLayer;
	if ( pLayer != NULL ) *pLayer = iLayer;
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewRemoveLayer(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int i;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iFrameRecordCount; ) {
		if ( pData->arrFrames[i].iLayer == iLayer ) pData->arrFrames[i] = pData->arrFrames[--pData->iFrameRecordCount];
		else {
			if ( pData->arrFrames[i].iLayer > iLayer ) pData->arrFrames[i].iLayer--;
			i++;
		}
	}
	for ( i = 0; i < pData->iSpanCount; ) {
		if ( pData->arrSpans[i].iLayer == iLayer ) pData->arrSpans[i] = pData->arrSpans[--pData->iSpanCount];
		else {
			if ( pData->arrSpans[i].iLayer > iLayer ) pData->arrSpans[i].iLayer--;
			i++;
		}
	}
	for ( i = 0; i < pData->iSelectionCount; ) {
		if ( pData->arrSelection[i].iLayer == iLayer ) pData->arrSelection[i] = pData->arrSelection[--pData->iSelectionCount];
		else {
			if ( pData->arrSelection[i].iLayer > iLayer ) pData->arrSelection[i].iLayer--;
			i++;
		}
	}
	for ( i = iLayer; i < pData->iLayerCount - 1; i++ ) pData->arrLayers[i] = pData->arrLayers[i + 1];
	pData->iLayerCount--;
	if ( pData->iActiveLayer >= pData->iLayerCount ) pData->iActiveLayer = pData->iLayerCount - 1;
	if ( pData->iActiveLayer >= 0 ) pData->arrLayers[pData->iActiveLayer].bSelected = 1;
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewMoveLayer(xui_widget pWidget, int iLayer, int iToLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	xui_timeline_layer_t tMove;
	int i;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iToLayer < 0) || (iToLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iLayer == iToLayer ) return XUI_OK;
	tMove = pData->arrLayers[iLayer];
	if ( iLayer < iToLayer ) {
		for ( i = iLayer; i < iToLayer; i++ ) pData->arrLayers[i] = pData->arrLayers[i + 1];
	} else {
		for ( i = iLayer; i > iToLayer; i-- ) pData->arrLayers[i] = pData->arrLayers[i - 1];
	}
	pData->arrLayers[iToLayer] = tMove;
#define XUI_TIMELINE_REMAP_LAYER(v) do { if ((v) == iLayer) (v) = iToLayer; else if (iLayer < iToLayer && (v) > iLayer && (v) <= iToLayer) (v)--; else if (iToLayer < iLayer && (v) >= iToLayer && (v) < iLayer) (v)++; } while (0)
	for ( i = 0; i < pData->iFrameRecordCount; i++ ) XUI_TIMELINE_REMAP_LAYER(pData->arrFrames[i].iLayer);
	for ( i = 0; i < pData->iSpanCount; i++ ) XUI_TIMELINE_REMAP_LAYER(pData->arrSpans[i].iLayer);
	for ( i = 0; i < pData->iSelectionCount; i++ ) XUI_TIMELINE_REMAP_LAYER(pData->arrSelection[i].iLayer);
	XUI_TIMELINE_REMAP_LAYER(pData->iActiveLayer);
	XUI_TIMELINE_REMAP_LAYER(pData->iAnchorLayer);
#undef XUI_TIMELINE_REMAP_LAYER
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetLayerCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iLayerCount : 0;
}

XUI_API int xuiTimeLineViewGetLayer(xui_widget pWidget, int iLayer, xui_timeline_layer_t* pLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pLayer == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pLayer = pData->arrLayers[iLayer];
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetLayerName(xui_widget pWidget, int iLayer, const char* sName)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->onLayerChanging != NULL && !pData->onLayerChanging(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_NAME, pData->pLayerUser) ) return XUI_OK;
	__xuiTimeLineCopy(pData->arrLayers[iLayer].sName, XUI_TIMELINE_NAME_CAPACITY, sName);
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	if ( pData->onLayerChanged != NULL ) pData->onLayerChanged(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_NAME, pData->pLayerUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiTimeLineViewGetLayerName(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? pData->arrLayers[iLayer].sName : NULL;
}

XUI_API int xuiTimeLineViewSetLayerVisible(xui_widget pWidget, int iLayer, int bVisible)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	bVisible = bVisible ? 1 : 0;
	if ( pData->arrLayers[iLayer].bVisible == bVisible ) return XUI_OK;
	if ( pData->onLayerChanging != NULL && !pData->onLayerChanging(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_VISIBLE, pData->pLayerUser) ) return XUI_OK;
	pData->arrLayers[iLayer].bVisible = bVisible;
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	if ( pData->onLayerChanged != NULL ) pData->onLayerChanged(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_VISIBLE, pData->pLayerUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetLayerVisible(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? pData->arrLayers[iLayer].bVisible : 0;
}

XUI_API int xuiTimeLineViewSetLayerLocked(xui_widget pWidget, int iLayer, int bLocked)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	bLocked = bLocked ? 1 : 0;
	if ( pData->arrLayers[iLayer].bLocked == bLocked ) return XUI_OK;
	if ( pData->onLayerChanging != NULL && !pData->onLayerChanging(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_LOCKED, pData->pLayerUser) ) return XUI_OK;
	pData->arrLayers[iLayer].bLocked = bLocked;
	pData->iLayerChangeCount++;
	pData->iChangeCount++;
	if ( pData->onLayerChanged != NULL ) pData->onLayerChanged(pWidget, iLayer, XUI_TIMELINE_LAYER_CHANGE_LOCKED, pData->pLayerUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetLayerLocked(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? pData->arrLayers[iLayer].bLocked : 0;
}

XUI_API int xuiTimeLineViewSetLayerHeight(xui_widget pWidget, int iLayer, float fHeight)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (fHeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrLayers[iLayer].fHeight = fHeight;
	(void)__xuiTimeLineUpdateContentSize(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiTimeLineViewGetLayerHeight(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? __xuiTimeLineLayerHeight(pData, iLayer) : 0.0f;
}

XUI_API int xuiTimeLineViewSetLayerColor(xui_widget pWidget, int iLayer, uint32_t iColor)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrLayers[iLayer].iColor = iColor;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiTimeLineViewGetLayerColor(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? pData->arrLayers[iLayer].iColor : 0;
}

XUI_API int xuiTimeLineViewSetLayerUserData(xui_widget pWidget, int iLayer, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrLayers[iLayer].pUser = pUser;
	return XUI_OK;
}

XUI_API void* xuiTimeLineViewGetLayerUserData(xui_widget pWidget, int iLayer)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && iLayer >= 0 && iLayer < pData->iLayerCount) ? pData->arrLayers[iLayer].pUser : NULL;
}

XUI_API int xuiTimeLineViewSetFrame(xui_widget pWidget, int iLayer, int iFrame, int iType, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	int iOldType;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	iType = __xuiTimeLineFrameTypeNormalize(iType);
	if ( iType == XUI_TIMELINE_FRAME_EMPTY ) return xuiTimeLineViewClearFrame(pWidget, iLayer, iFrame);
	iIndex = __xuiTimeLineFindFrame(pData, iLayer, iFrame);
	iOldType = (iIndex >= 0) ? pData->arrFrames[iIndex].iType : XUI_TIMELINE_FRAME_EMPTY;
	if ( pData->onFrameChanging != NULL && !pData->onFrameChanging(pWidget, iLayer, iFrame, iOldType, iType, pData->pFrameUser) ) return XUI_OK;
	if ( iIndex < 0 ) {
		if ( pData->iFrameRecordCount >= XUI_TIMELINE_FRAME_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
		iIndex = pData->iFrameRecordCount++;
	}
	pData->arrFrames[iIndex].iLayer = iLayer;
	pData->arrFrames[iIndex].iFrame = iFrame;
	pData->arrFrames[iIndex].iType = iType;
	pData->arrFrames[iIndex].pUser = pUser;
	pData->iFrameChangeCount++;
	pData->iChangeCount++;
	if ( pData->onFrameChanged != NULL ) pData->onFrameChanged(pWidget, iLayer, iFrame, iOldType, iType, pData->pFrameUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetFrame(xui_widget pWidget, int iLayer, int iFrame, xui_timeline_frame_t* pFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( (pData == NULL) || (pFrame == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindFrame(pData, iLayer, iFrame);
	if ( iIndex >= 0 ) *pFrame = pData->arrFrames[iIndex];
	else {
		memset(pFrame, 0, sizeof(*pFrame));
		pFrame->iLayer = iLayer;
		pFrame->iFrame = iFrame;
		pFrame->iType = XUI_TIMELINE_FRAME_EMPTY;
	}
	return XUI_OK;
}

XUI_API int xuiTimeLineViewClearFrame(xui_widget pWidget, int iLayer, int iFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	int iOldType;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindFrame(pData, iLayer, iFrame);
	if ( iIndex < 0 ) return XUI_OK;
	iOldType = pData->arrFrames[iIndex].iType;
	if ( pData->onFrameChanging != NULL && !pData->onFrameChanging(pWidget, iLayer, iFrame, iOldType, XUI_TIMELINE_FRAME_EMPTY, pData->pFrameUser) ) return XUI_OK;
	pData->arrFrames[iIndex] = pData->arrFrames[--pData->iFrameRecordCount];
	pData->iFrameChangeCount++;
	pData->iChangeCount++;
	if ( pData->onFrameChanged != NULL ) pData->onFrameChanged(pWidget, iLayer, iFrame, iOldType, XUI_TIMELINE_FRAME_EMPTY, pData->pFrameUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewSetFrameUserData(xui_widget pWidget, int iLayer, int iFrame, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindFrame(pData, iLayer, iFrame);
	if ( iIndex < 0 ) return xuiTimeLineViewSetFrame(pWidget, iLayer, iFrame, XUI_TIMELINE_FRAME_EMPTY, pUser);
	pData->arrFrames[iIndex].pUser = pUser;
	return XUI_OK;
}

XUI_API void* xuiTimeLineViewGetFrameUserData(xui_widget pWidget, int iLayer, int iFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return NULL;
	iIndex = __xuiTimeLineFindFrame(pData, iLayer, iFrame);
	return (iIndex >= 0) ? pData->arrFrames[iIndex].pUser : NULL;
}

XUI_API int xuiTimeLineViewAddSpan(xui_widget pWidget, int iLayer, int iStartFrame, int iEndFrame, int iType, const char* sLabel, int* pSpanId)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	xui_timeline_span_t* pSpan;
	int iTmp;
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSpanCount >= XUI_TIMELINE_SPAN_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iStartFrame > iEndFrame ) { iTmp = iStartFrame; iStartFrame = iEndFrame; iEndFrame = iTmp; }
	iStartFrame = __xuiTimeLineClampInt(iStartFrame, 0, pData->iTotalFrames - 1);
	iEndFrame = __xuiTimeLineClampInt(iEndFrame, 0, pData->iTotalFrames - 1);
	if ( pData->onSpanChanging != NULL && !pData->onSpanChanging(pWidget, -1, XUI_TIMELINE_SPAN_CHANGE_ADD, pData->pSpanUser) ) return XUI_OK;
	pSpan = &pData->arrSpans[pData->iSpanCount++];
	memset(pSpan, 0, sizeof(*pSpan));
	pSpan->iLayer = iLayer;
	pSpan->iId = pData->iNextSpanId++;
	pSpan->iStartFrame = iStartFrame;
	pSpan->iEndFrame = iEndFrame;
	pSpan->iType = __xuiTimeLineSpanTypeNormalize(iType);
	__xuiTimeLineCopy(pSpan->sLabel, XUI_TIMELINE_LABEL_CAPACITY, sLabel);
	pSpan->iColor = pData->tColors.iSpanColor;
	if ( pSpanId != NULL ) *pSpanId = pSpan->iId;
	pData->iSpanChangeCount++;
	pData->iChangeCount++;
	if ( pData->onSpanChanged != NULL ) pData->onSpanChanged(pWidget, pSpan->iId, XUI_TIMELINE_SPAN_CHANGE_ADD, pData->pSpanUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewRemoveSpan(xui_widget pWidget, int iSpanId)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->onSpanChanging != NULL && !pData->onSpanChanging(pWidget, iSpanId, XUI_TIMELINE_SPAN_CHANGE_REMOVE, pData->pSpanUser) ) return XUI_OK;
	pData->arrSpans[iIndex] = pData->arrSpans[--pData->iSpanCount];
	pData->iSpanChangeCount++;
	pData->iChangeCount++;
	if ( pData->onSpanChanged != NULL ) pData->onSpanChanged(pWidget, iSpanId, XUI_TIMELINE_SPAN_CHANGE_REMOVE, pData->pSpanUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewSetSpan(xui_widget pWidget, int iSpanId, int iStartFrame, int iEndFrame, int iType, const char* sLabel)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	xui_timeline_span_t* pSpan;
	int iIndex;
	int iTmp;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iStartFrame > iEndFrame ) { iTmp = iStartFrame; iStartFrame = iEndFrame; iEndFrame = iTmp; }
	iStartFrame = __xuiTimeLineClampInt(iStartFrame, 0, pData->iTotalFrames - 1);
	iEndFrame = __xuiTimeLineClampInt(iEndFrame, 0, pData->iTotalFrames - 1);
	if ( pData->onSpanChanging != NULL && !pData->onSpanChanging(pWidget, iSpanId, XUI_TIMELINE_SPAN_CHANGE_SET, pData->pSpanUser) ) return XUI_OK;
	pSpan = &pData->arrSpans[iIndex];
	pSpan->iStartFrame = iStartFrame;
	pSpan->iEndFrame = iEndFrame;
	pSpan->iType = __xuiTimeLineSpanTypeNormalize(iType);
	__xuiTimeLineCopy(pSpan->sLabel, XUI_TIMELINE_LABEL_CAPACITY, sLabel);
	pData->iSpanChangeCount++;
	pData->iChangeCount++;
	if ( pData->onSpanChanged != NULL ) pData->onSpanChanged(pWidget, iSpanId, XUI_TIMELINE_SPAN_CHANGE_SET, pData->pSpanUser);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetSpan(xui_widget pWidget, int iSpanId, xui_timeline_span_t* pSpan)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( (pData == NULL) || (pSpan == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	*pSpan = pData->arrSpans[iIndex];
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetSpanColor(xui_widget pWidget, int iSpanId, uint32_t iColor)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSpans[iIndex].iColor = iColor;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewSetSpanCustomType(xui_widget pWidget, int iSpanId, const char* sCustomType)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTimeLineCopy(pData->arrSpans[iIndex].sCustomType, XUI_TIMELINE_TYPE_CAPACITY, sCustomType);
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetSpanUserData(xui_widget pWidget, int iSpanId, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSpans[iIndex].pUser = pUser;
	return XUI_OK;
}

XUI_API void* xuiTimeLineViewGetSpanUserData(xui_widget pWidget, int iSpanId)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	int iIndex;
	if ( pData == NULL ) return NULL;
	iIndex = __xuiTimeLineFindSpan(pData, iSpanId);
	return (iIndex >= 0) ? pData->arrSpans[iIndex].pUser : NULL;
}

XUI_API int xuiTimeLineViewClearSelection(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTimeLineClearSelectionRaw(pData) ) return XUI_OK;
	return __xuiTimeLineSelectionNotify(pWidget, pData);
}

XUI_API int xuiTimeLineViewSelectFrame(xui_widget pWidget, int iLayer, int iFrame, int bSelected)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTimeLineSelectFrameRaw(pData, iLayer, iFrame, bSelected) ) return XUI_OK;
	return __xuiTimeLineSelectionNotify(pWidget, pData);
}

XUI_API int xuiTimeLineViewSelectRange(xui_widget pWidget, int iLayer0, int iFrame0, int iLayer1, int iFrame1, int bSelected)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pData->iLayerCount <= 0) || (pData->iTotalFrames <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTimeLineSelectRangeRaw(pData, iLayer0, iFrame0, iLayer1, iFrame1, bSelected) ) return XUI_OK;
	return __xuiTimeLineSelectionNotify(pWidget, pData);
}

XUI_API int xuiTimeLineViewIsFrameSelected(xui_widget pWidget, int iLayer, int iFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL && __xuiTimeLineFindSelection(pData, iLayer, iFrame) >= 0) ? 1 : 0;
}

XUI_API int xuiTimeLineViewGetSelectionCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectionCount : 0;
}

XUI_API int xuiTimeLineViewGetSelection(xui_widget pWidget, int iIndex, xui_timeline_selection_t* pSelection)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pSelection == NULL) || (iIndex < 0) || (iIndex >= pData->iSelectionCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pSelection = pData->arrSelection[iIndex];
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetCurrentFrameCallbacks(xui_widget pWidget, xui_timeline_current_frame_changing_proc onChanging, xui_timeline_current_frame_changed_proc onChanged, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCurrentChanging = onChanging;
	pData->onCurrentChanged = onChanged;
	pData->pCurrentUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetLayerCallbacks(xui_widget pWidget, xui_timeline_layer_changing_proc onChanging, xui_timeline_layer_changed_proc onChanged, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onLayerChanging = onChanging;
	pData->onLayerChanged = onChanged;
	pData->pLayerUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetFrameCallbacks(xui_widget pWidget, xui_timeline_frame_changing_proc onChanging, xui_timeline_frame_changed_proc onChanged, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onFrameChanging = onChanging;
	pData->onFrameChanged = onChanged;
	pData->pFrameUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetSpanCallbacks(xui_widget pWidget, xui_timeline_span_changing_proc onChanging, xui_timeline_span_changed_proc onChanged, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSpanChanging = onChanging;
	pData->onSpanChanged = onChanged;
	pData->pSpanUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetLayerSelected(xui_widget pWidget, xui_timeline_layer_selected_proc onSelected, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onLayerSelected = onSelected;
	pData->pLayerSelectedUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetContextMenu(xui_widget pWidget, xui_timeline_context_opening_proc onOpening, xui_timeline_context_command_proc onCommand, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onContextOpening = onOpening;
	pData->onContextCommand = onCommand;
	pData->pContextUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_timeline_view_data_t* pData;
	char* sNew;
	int iIndex;

	pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTimeLineMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_TIMELINE_MENU_TITLE_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sTitle == NULL || sTitle[0] == '\0' ) {
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = NULL;
	} else {
		sNew = __xuiTimeLineStringDup(sTitle);
		if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = sNew;
	}
	if ( (pData->pLayerMenu != NULL) && xuiMenuIsOpen(pData->pLayerMenu) ) {
		(void)__xuiTimeLineBuildLayerMenu(pWidget, pData, &pData->tContextHit);
	}
	if ( (pData->pFrameMenu != NULL) && xuiMenuIsOpen(pData->pFrameMenu) ) {
		(void)__xuiTimeLineBuildFrameMenu(pWidget, pData, &pData->tContextHit);
	}
	return XUI_OK;
}

XUI_API const char* xuiTimeLineViewGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return __xuiTimeLineMenuTitleForCommand(pWidget, pData, iCommand);
}

XUI_API int xuiTimeLineViewRunContextCommand(xui_widget pWidget, int iCommand)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTimeLineRunContextCommand(pWidget, pData, iCommand);
}

XUI_API int xuiTimeLineViewSetFrameClick(xui_widget pWidget, xui_timeline_frame_click_proc onClick, xui_timeline_frame_click_proc onDoubleClick, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onFrameClick = onClick;
	pData->onFrameDoubleClick = onDoubleClick;
	pData->pFrameClickUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetSelectionChange(xui_widget pWidget, xui_timeline_selection_proc onSelection, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelection = onSelection;
	pData->pSelectionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewSetRenderers(xui_widget pWidget, xui_timeline_layer_renderer_proc onLayer, xui_timeline_ruler_renderer_proc onRuler, xui_timeline_frame_renderer_proc onFrame, xui_timeline_span_renderer_proc onSpan, void* pUser)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onLayerRender = onLayer;
	pData->onRulerRender = onRuler;
	pData->onFrameRender = onFrame;
	pData->onSpanRender = onSpan;
	pData->pRenderUser = pUser;
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewSetColors(xui_widget pWidget, const xui_timeline_view_colors_t* pColors)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	(void)__xuiTimeLineApplyFrameStyle(pWidget, pData);
	return __xuiTimeLineInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTimeLineViewGetColors(xui_widget pWidget, xui_timeline_view_colors_t* pColors)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	return XUI_OK;
}

XUI_API int xuiTimeLineViewEnsureFrameVisible(xui_widget pWidget, int iLayer, int iFrame)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	xui_rect_t tRect;
	if ( (pData == NULL) || (pData->pFrame == NULL) || (iLayer < 0) || (iLayer >= pData->iLayerCount) || (iFrame < 0) || (iFrame >= pData->iTotalFrames) ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect.fX = (float)iFrame * pData->fFrameWidth;
	tRect.fY = __xuiTimeLineLayerTop(pData, iLayer);
	tRect.fW = pData->fFrameWidth;
	tRect.fH = __xuiTimeLineLayerHeight(pData, iLayer);
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

XUI_API int xuiTimeLineViewHitTest(xui_widget pWidget, float fX, float fY, xui_timeline_hit_t* pHit)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTimeLineHitLocal(pWidget, pData, fX, fY, pHit);
}

XUI_API int xuiTimeLineViewSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetOffset(pData->pFrame, fOffsetX, fOffsetY);
}

XUI_API int xuiTimeLineViewGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetOffset(pData->pFrame, pOffsetX, pOffsetY);
}

XUI_API int xuiTimeLineViewGetChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetCurrentFrameChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iCurrentFrameChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetLayerChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iLayerChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetFrameChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iFrameChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetSpanChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iSpanChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetSelectionChangeCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectionChangeCount : 0;
}

XUI_API int xuiTimeLineViewGetClickCount(xui_widget pWidget)
{
	xui_timeline_view_data_t* pData = __xuiTimeLineViewGetData(pWidget);
	return (pData != NULL) ? pData->iClickCount : 0;
}
