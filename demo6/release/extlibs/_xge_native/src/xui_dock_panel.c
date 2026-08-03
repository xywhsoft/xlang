#include "xui_internal.h"

#include <string.h>

#define XUI_DOCK_NODE_EMPTY 0
#define XUI_DOCK_NODE_SPLIT 1
#define XUI_DOCK_NODE_PANE 2
#define XUI_DOCK_ORIENTATION_VERTICAL 1
#define XUI_DOCK_ORIENTATION_HORIZONTAL 2
#define XUI_DOCK_DRAG_NONE 0
#define XUI_DOCK_DRAG_SPLITTER 1
#define XUI_DOCK_DRAG_FLOAT 2
#define XUI_DOCK_DRAG_DOCKED 3
#define XUI_DOCK_DRAG_FLOAT_RESIZE 4
#define XUI_DOCK_DRAG_THRESHOLD 6.0f
#define XUI_DOCK_RESIZE_LEFT 0x01
#define XUI_DOCK_RESIZE_TOP 0x02
#define XUI_DOCK_RESIZE_RIGHT 0x04
#define XUI_DOCK_RESIZE_BOTTOM 0x08
#define XUI_DOCK_PANEL_MENU_TITLE_COUNT 5
#define XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT 5

typedef struct xui_dock_panel_data_t xui_dock_panel_data_t;

typedef struct xui_dock_window_slot_t {
	int bUsed;
	int iWindow;
	int iPane;
	int iState;
	int iRegion;
	int iLastRegion;
	int iAutoHideRegion;
	int iLastSide;
	int iLastTabIndex;
	int bClosable;
	int bDockable;
	char sTitle[XUI_DOCK_PANEL_TITLE_CAPACITY];
	xui_widget pPanelWidget;
	xui_widget pHostWidget;
	xui_widget pClientWidget;
	xui_rect_t tFloatRect;
	xui_rect_t tLastDockRect;
	xui_rect_t tRect;
	xui_rect_t tClientRect;
	xui_rect_t tTabRect;
	xui_rect_t tTabCloseRect;
	xui_rect_t tAutoHideRect;
	void* pUser;
	xui_dock_panel_data_t* pOwner;
} xui_dock_window_slot_t;

typedef struct xui_dock_pane_slot_t {
	int bUsed;
	int iPane;
	int iNode;
	int iRegion;
	int arrWindows[XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY];
	int iWindowCount;
	int iActiveIndex;
	int iHoverPart;
	xui_rect_t tRect;
	xui_rect_t tCaptionRect;
	xui_rect_t tTabStripRect;
	xui_rect_t tClientRect;
	xui_rect_t tCloseRect;
	xui_rect_t tPinRect;
	xui_rect_t tOptionRect;
	xui_rect_t tOverflowRect;
	int iFirstVisibleTab;
	int iVisibleTabCount;
	int bOverflow;
} xui_dock_pane_slot_t;

typedef struct xui_dock_node_slot_t {
	int bUsed;
	int iNode;
	int iType;
	int iParent;
	int iFirst;
	int iSecond;
	int iPane;
	int iOrientation;
	float fRatio;
	xui_rect_t tRect;
	xui_rect_t tSplitterRect;
} xui_dock_node_slot_t;

typedef struct xui_dock_region_slot_t {
	int iRootNode;
	int iSizeMode;
	float fValue;
	float fMinSize;
	float fMaxSize;
	int bVisible;
	xui_rect_t tRect;
	xui_rect_t tSplitterRect;
} xui_dock_region_slot_t;

struct xui_dock_panel_data_t {
	xui_font pFont;
	xui_dock_panel_metrics_t tMetrics;
	xui_dock_panel_colors_t tColors;
	xui_dock_region_slot_t arrRegions[XUI_DOCK_PANEL_REGION_COUNT];
	xui_dock_window_slot_t arrWindows[XUI_DOCK_PANEL_WINDOW_CAPACITY];
	xui_dock_pane_slot_t arrPanes[XUI_DOCK_PANEL_PANE_CAPACITY];
	xui_dock_node_slot_t arrNodes[XUI_DOCK_PANEL_NODE_CAPACITY];
	int arrFloatOrder[XUI_DOCK_PANEL_WINDOW_CAPACITY];
	int iWindowCount;
	int iPaneCount;
	int iNodeCount;
	int iFloatCount;
	int iHoverType;
	int iHoverWindow;
	int iHoverPane;
	int iHoverNode;
	int iHoverRegion;
	int iDragType;
	int iDragNode;
	int iDragRegion;
	int iDragPane;
	int iDragWindow;
	int iDragSourceIndex;
	int iDragInsertIndex;
	int iDragSide;
	int bDragFloating;
	float fDragStartX;
	float fDragStartY;
	float fDragStartRatio;
	xui_rect_t tDragStartRect;
	xui_dock_drop_info_t tDragPreview;
	xui_dock_drop_info_t tDragIndicator;
	xui_dock_window_state_proc onStateChanged;
	void* pStateUser;
	xui_dock_active_proc onActiveChanged;
	void* pActiveUser;
	xui_dock_window_close_proc onClose;
	void* pCloseUser;
	xui_widget pOptionMenu;
	xui_widget pOverflowMenu;
	char* arrMenuTitle[XUI_DOCK_PANEL_MENU_TITLE_COUNT];
	char* arrTooltipText[XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT];
	xui_widget pDragOverlayWidget;
	int iMenuPane;
	int iPendingFocusWindow;
	int iAutoHideExpandWindow;
	xui_rect_t tAutoHideExpandRect;
	xui_rect_t tAutoHideExpandClientRect;
	xui_rect_t tAutoHidePinRect;
	xui_rect_t tAutoHideCloseRect;
	int iChangeCount;
	int iLayoutChangeCount;
	int iWindowChangeCount;
};

static xui_dock_panel_data_t* __xuiDockPanelGetData(xui_widget pWidget);
static int __xuiDockPanelArrangeNow(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_rect_t tRect);
static int __xuiDockOpenPaneMenu(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane);
static int __xuiDockOpenOverflowMenu(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane);
static xui_rect_t __xuiDockClampFloatRect(xui_widget pPanel, xui_rect_t r);
static void __xuiDockSetHostLayer(xui_dock_window_slot_t* pWin);
static void __xuiDockRefreshHostInputOrder(xui_dock_panel_data_t* pData);
static int __xuiDockPointerTargetIsFloatingHost(xui_dock_panel_data_t* pData, xui_widget pTarget);
static const char* __xuiDockTooltipTextForId(xui_context pContext, xui_dock_panel_data_t* pData, int iTooltip);
static int __xuiDockDragOverlayRender(xui_widget pOverlay, xui_draw_context pDraw, uint32_t iStateId, void* pUser);
static void __xuiDockSyncDragOverlay(xui_widget pWidget, xui_dock_panel_data_t* pData);

static float __xuiDockMin(float a, float b) { return (a < b) ? a : b; }
static float __xuiDockMax(float a, float b) { return (a > b) ? a : b; }
static float __xuiDockAbs(float a) { return (a < 0.0f) ? -a : a; }

static float __xuiDockClamp(float v, float mn, float mx)
{
	if ( mx < mn ) mx = mn;
	if ( v < mn ) return mn;
	if ( v > mx ) return mx;
	return v;
}

static int __xuiDockAlpha(uint32_t c)
{
	return (int)(c & 0xffu);
}

static char* __xuiDockStringDup(const char* sText)
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

static int __xuiDockRectRenderable(xui_rect_t r)
{
	return (r.fW > 0.0f) && (r.fH > 0.0f);
}

static int __xuiDockUtf8Continuation(const char* s, int i)
{
	unsigned char c;
	if ( s == NULL || i < 0 || s[i] == '\0' ) return 0;
	c = (unsigned char)s[i];
	return (c & 0xc0u) == 0x80u;
}

static int __xuiDockUtf8SequenceLength(const char* s)
{
	unsigned char c;
	if ( s == NULL || s[0] == '\0' ) return 0;
	c = (unsigned char)s[0];
	if ( c < 128u ) return 1;
	if ( (c & 0xe0u) == 0xc0u && __xuiDockUtf8Continuation(s, 1) ) return 2;
	if ( (c & 0xf0u) == 0xe0u && __xuiDockUtf8Continuation(s, 1) && __xuiDockUtf8Continuation(s, 2) ) return 3;
	if ( (c & 0xf8u) == 0xf0u && __xuiDockUtf8Continuation(s, 1) && __xuiDockUtf8Continuation(s, 2) && __xuiDockUtf8Continuation(s, 3) ) return 4;
	return 1;
}

static float __xuiDockStringWidthGuess(const char* s)
{
	float w = 0.0f;
	unsigned char c;
	if ( s == NULL ) return 0.0f;
	while ( *s != '\0' ) {
		c = (unsigned char)*s;
		if ( c < 128u ) {
			w += 7.0f;
			s++;
		} else {
			w += 12.0f;
			s += __xuiDockUtf8SequenceLength(s);
		}
	}
	return w;
}

static int __xuiDockRectContains(xui_rect_t r, float x, float y)
{
	return (x >= r.fX) && (y >= r.fY) && (x < r.fX + r.fW) && (y < r.fY + r.fH);
}

static xui_rect_t __xuiDockRect(float x, float y, float w, float h)
{
	xui_rect_t r;
	r.fX = x;
	r.fY = y;
	r.fW = (w > 0.0f) ? w : 0.0f;
	r.fH = (h > 0.0f) ? h : 0.0f;
	return xuiInternalSnapRect(r);
}

static xui_rect_t __xuiDockInset(xui_rect_t r, float x, float y)
{
	r.fX += x;
	r.fY += y;
	r.fW = __xuiDockMax(0.0f, r.fW - x * 2.0f);
	r.fH = __xuiDockMax(0.0f, r.fH - y * 2.0f);
	return xuiInternalSnapRect(r);
}

static xui_rect_t __xuiDockOffsetRect(xui_rect_t r, float x, float y)
{
	r.fX -= x;
	r.fY -= y;
	return xuiInternalSnapRect(r);
}

static void __xuiDockCopy(char* dst, int cap, const char* src)
{
	if ( (dst == NULL) || (cap <= 0) ) return;
	if ( src == NULL ) src = "";
	strncpy(dst, src, (size_t)cap - 1u);
	dst[cap - 1] = '\0';
}

static int __xuiDockRegionValid(int iRegion)
{
	return (iRegion >= 0) && (iRegion < XUI_DOCK_PANEL_REGION_COUNT);
}

static int __xuiDockSideValid(int iSide)
{
	return (iSide >= XUI_DOCK_PANEL_SIDE_NONE) && (iSide <= XUI_DOCK_PANEL_SIDE_FILL);
}

static int __xuiDockSideIsSplit(int iSide)
{
	return (iSide == XUI_DOCK_PANEL_SIDE_LEFT) ||
	       (iSide == XUI_DOCK_PANEL_SIDE_RIGHT) ||
	       (iSide == XUI_DOCK_PANEL_SIDE_TOP) ||
	       (iSide == XUI_DOCK_PANEL_SIDE_BOTTOM);
}

static int __xuiDockRegionFromSide(int iSide)
{
	switch ( iSide ) {
	case XUI_DOCK_PANEL_SIDE_LEFT: return XUI_DOCK_PANEL_REGION_LEFT;
	case XUI_DOCK_PANEL_SIDE_RIGHT: return XUI_DOCK_PANEL_REGION_RIGHT;
	case XUI_DOCK_PANEL_SIDE_TOP: return XUI_DOCK_PANEL_REGION_TOP;
	case XUI_DOCK_PANEL_SIDE_BOTTOM: return XUI_DOCK_PANEL_REGION_BOTTOM;
	default: return XUI_DOCK_PANEL_REGION_DOCUMENT;
	}
}

static int __xuiDockRegionIsAutoHideSide(int iRegion)
{
	return (iRegion == XUI_DOCK_PANEL_REGION_LEFT) ||
	       (iRegion == XUI_DOCK_PANEL_REGION_RIGHT) ||
	       (iRegion == XUI_DOCK_PANEL_REGION_TOP) ||
	       (iRegion == XUI_DOCK_PANEL_REGION_BOTTOM);
}

static int __xuiDockAutoHideRegionFromLast(int iLastRegion, int iLastSide)
{
	if ( __xuiDockRegionIsAutoHideSide(iLastRegion) ) return iLastRegion;
	if ( (iLastRegion == XUI_DOCK_PANEL_REGION_DOCUMENT) && __xuiDockSideIsSplit(iLastSide) ) {
		return __xuiDockRegionFromSide(iLastSide);
	}
	return -1;
}

static int __xuiDockWindowAutoHideRegion(const xui_dock_window_slot_t* pWin)
{
	int iRegion;
	if ( pWin == NULL ) return XUI_DOCK_PANEL_REGION_LEFT;
	if ( __xuiDockRegionIsAutoHideSide(pWin->iAutoHideRegion) ) return pWin->iAutoHideRegion;
	iRegion = __xuiDockAutoHideRegionFromLast(pWin->iLastRegion, pWin->iLastSide);
	return __xuiDockRegionIsAutoHideSide(iRegion) ? iRegion : XUI_DOCK_PANEL_REGION_LEFT;
}

static void __xuiDockDefaultMetrics(xui_dock_panel_metrics_t* m)
{
	memset(m, 0, sizeof(*m));
	m->iSize = sizeof(*m);
	m->fCaptionHeight = 24.0f;
	m->fTabStripHeight = 25.0f;
	m->fButtonSize = 16.0f;
	m->fButtonGap = 4.0f;
	m->fBorderWidth = 1.0f;
	m->fSplitterSize = 5.0f;
	m->fSplitterHitSize = 7.0f;
	m->fMinPaneWidth = 96.0f;
	m->fMinPaneHeight = 72.0f;
	m->fTabMinWidth = 68.0f;
	m->fTabMaxWidth = 150.0f;
	m->fTabPaddingX = 12.0f;
	m->fFloatTitleHeight = 27.0f;
	m->fFloatBorderWidth = 1.0f;
	m->fAutoHideStripSize = 24.0f;
}

static void __xuiDockDefaultColors(xui_dock_panel_colors_t* c)
{
	memset(c, 0, sizeof(*c));
	c->iBackgroundColor = XUI_COLOR_RGBA(232, 241, 250, 255);
	c->iPaneColor = XUI_COLOR_RGBA(246, 250, 254, 255);
	c->iClientColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	c->iCaptionColor = XUI_COLOR_RGBA(226, 238, 249, 255);
	c->iActiveCaptionColor = XUI_COLOR_RGBA(46, 125, 215, 255);
	c->iCaptionTextColor = XUI_COLOR_RGBA(45, 65, 88, 255);
	c->iActiveCaptionTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	c->iTabColor = XUI_COLOR_RGBA(235, 244, 252, 255);
	c->iTabHoverColor = XUI_COLOR_RGBA(219, 236, 250, 255);
	c->iActiveTabColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	c->iTabTextColor = XUI_COLOR_RGBA(55, 76, 99, 255);
	c->iActiveTabTextColor = XUI_COLOR_RGBA(35, 55, 76, 255);
	c->iBorderColor = XUI_COLOR_RGBA(145, 176, 207, 255);
	c->iFocusColor = XUI_COLOR_RGBA(47, 125, 214, 255);
	c->iSplitterColor = XUI_COLOR_RGBA(211, 226, 240, 255);
	c->iSplitterHoverColor = XUI_COLOR_RGBA(177, 207, 235, 255);
	c->iSplitterActiveColor = XUI_COLOR_RGBA(56, 138, 220, 255);
	c->iButtonColor = XUI_COLOR_RGBA(70, 99, 128, 255);
	c->iButtonHoverColor = XUI_COLOR_RGBA(41, 118, 200, 255);
	c->iButtonActiveColor = XUI_COLOR_RGBA(20, 88, 162, 255);
	c->iAutoHideColor = XUI_COLOR_RGBA(225, 239, 250, 255);
	c->iAutoHideHoverColor = XUI_COLOR_RGBA(202, 226, 247, 255);
	c->iFloatTitleColor = XUI_COLOR_RGBA(47, 125, 214, 255);
	c->iFloatBorderColor = XUI_COLOR_RGBA(47, 125, 214, 255);
}

static void __xuiDockDefaults(xui_dock_panel_data_t* pData)
{
	int i;
	memset(pData, 0, sizeof(*pData));
	__xuiDockDefaultMetrics(&pData->tMetrics);
	__xuiDockDefaultColors(&pData->tColors);
	for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		pData->arrRegions[i].iRootNode = -1;
		pData->arrRegions[i].iSizeMode = XUI_DOCK_PANEL_SIZE_PORTION;
		pData->arrRegions[i].fValue = 0.24f;
		pData->arrRegions[i].fMinSize = 96.0f;
		pData->arrRegions[i].fMaxSize = 0.0f;
		pData->arrRegions[i].bVisible = 1;
	}
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		pData->arrFloatOrder[i] = -1;
	}
	pData->arrRegions[XUI_DOCK_PANEL_REGION_DOCUMENT].fValue = 1.0f;
	pData->arrRegions[XUI_DOCK_PANEL_REGION_TOP].fValue = 0.28f;
	pData->arrRegions[XUI_DOCK_PANEL_REGION_BOTTOM].fValue = 0.28f;
	pData->iHoverWindow = -1;
	pData->iHoverPane = -1;
	pData->iHoverNode = -1;
	pData->iHoverRegion = -1;
	pData->iDragNode = -1;
	pData->iDragRegion = -1;
	pData->iDragPane = -1;
	pData->iDragWindow = -1;
	pData->iDragSourceIndex = -1;
	pData->iDragInsertIndex = -1;
	pData->iDragSide = 0;
	pData->iMenuPane = -1;
	pData->iPendingFocusWindow = -1;
	pData->iAutoHideExpandWindow = -1;
	pData->tDragPreview.iSize = sizeof(pData->tDragPreview);
	pData->tDragPreview.iWindow = -1;
	pData->tDragPreview.iPane = -1;
	pData->tDragPreview.iRegion = -1;
	pData->tDragPreview.iSide = XUI_DOCK_PANEL_SIDE_NONE;
	pData->tDragIndicator.iSize = sizeof(pData->tDragIndicator);
	pData->tDragIndicator.iWindow = -1;
	pData->tDragIndicator.iPane = -1;
	pData->tDragIndicator.iRegion = -1;
	pData->tDragIndicator.iSide = XUI_DOCK_PANEL_SIDE_NONE;
}

static void __xuiDockApplyDesc(xui_dock_panel_data_t* pData, const xui_dock_panel_desc_t* pDesc)
{
	if ( pDesc == NULL ) return;
	if ( pDesc->pFont != NULL ) pData->pFont = pDesc->pFont;
	if ( pDesc->bHasMetrics ) pData->tMetrics = pDesc->tMetrics;
	if ( pDesc->bHasColors ) pData->tColors = pDesc->tColors;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
}

static int __xuiDockDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c)
{
	if ( (__xuiDockAlpha(c) == 0) || !__xuiDockRectRenderable(r) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(r), c);
	}
	return XUI_OK;
}

static int __xuiDockDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, float w, uint32_t c)
{
	if ( (w <= 0.0f) || (__xuiDockAlpha(c) == 0) || !__xuiDockRectRenderable(r) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectStroke != NULL) ) {
		return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(r), w, c);
	}
	return XUI_OK;
}

static int __xuiDockDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c)
{
	if ( (__xuiDockAlpha(c) == 0) || !__xuiDockRectRenderable(r) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(r), c);
	}
	return __xuiDockDrawFill(pProxy, pDraw, r, c);
}

static int __xuiDockDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, float w, uint32_t c)
{
	if ( (w <= 0.0f) || (__xuiDockAlpha(c) == 0) || !__xuiDockRectRenderable(r) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawRectStroke != NULL) ) {
		return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(r), w, c);
	}
	return __xuiDockDrawStroke(pProxy, pDraw, r, w, c);
}

static int __xuiDockDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float x0, float y0, float x1, float y1, float w, uint32_t c)
{
	if ( (pProxy != NULL) && (pProxy->drawLine != NULL) && (__xuiDockAlpha(c) != 0) ) {
		return pProxy->drawLine(pProxy, pDraw, x0, y0, x1, y1, w, c);
	}
	return XUI_OK;
}

static int __xuiDockDrawEdgeRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c, int bLeft, int bTop, int bRight, int bBottom)
{
	int ret;
	if ( (__xuiDockAlpha(c) == 0) || !__xuiDockRectRenderable(r) ) return XUI_OK;
	if ( bTop ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(r.fX, r.fY, r.fW, 1.0f), c);
		if ( ret != XUI_OK ) return ret;
	}
	if ( bLeft ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(r.fX, r.fY, 1.0f, r.fH), c);
		if ( ret != XUI_OK ) return ret;
	}
	if ( bRight ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(r.fX + r.fW - 1.0f, r.fY, 1.0f, r.fH), c);
		if ( ret != XUI_OK ) return ret;
	}
	if ( bBottom ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(r.fX, r.fY + r.fH - 1.0f, r.fW, 1.0f), c);
		if ( ret != XUI_OK ) return ret;
	}
	return XUI_OK;
}

static int __xuiDockDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* text, xui_rect_t r, uint32_t c, uint32_t flags)
{
	if ( (pProxy != NULL) && (pProxy->drawText != NULL) && (pFont != NULL) && (text != NULL) && (__xuiDockAlpha(c) != 0) && __xuiDockRectRenderable(r) ) {
		return pProxy->drawText(pProxy, pDraw, pFont, text, xuiInternalSnapRect(r), c, flags);
	}
	return XUI_OK;
}

static int __xuiDockDrawBuiltinAsset(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, const char* sName, xui_rect_t tDst, uint32_t iColor)
{
	xui_surface pSurface;
	xui_rect_t tSrc;
	int iRet;
	if ( (pWidget == NULL) || (pProxy == NULL) || (pProxy->drawSurface == NULL) || (sName == NULL) || !__xuiDockRectRenderable(tDst) || (__xuiDockAlpha(iColor) == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSurface = NULL;
	iRet = xuiBuiltinAssetGetAtlas(xuiWidgetGetContext(pWidget), &pSurface);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiBuiltinAssetGetRect(sName, &tSrc);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, xuiInternalSnapRect(tDst), iColor, 0);
}

static int __xuiDockDrawCloseGlyph(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c)
{
	float x0 = r.fX + 4.0f;
	float y0 = r.fY + 4.0f;
	float x1 = r.fX + r.fW - 4.0f;
	float y1 = r.fY + r.fH - 4.0f;
	int ret = __xuiDockDrawLine(pProxy, pDraw, x0, y0, x1, y1, 1.3f, c);
	if ( ret != XUI_OK ) return ret;
	return __xuiDockDrawLine(pProxy, pDraw, x1, y0, x0, y1, 1.3f, c);
}

static int __xuiDockDrawPinGlyph(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c, int bAutoHide)
{
	float cx = r.fX + r.fW * 0.5f;
	float cy = r.fY + r.fH * 0.5f;
	int ret;
	ret = __xuiDockDrawLine(pProxy, pDraw, cx, r.fY + 4.0f, cx, r.fY + r.fH - 4.0f, 1.2f, c);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockDrawLine(pProxy, pDraw, cx - 4.0f, r.fY + 6.0f, cx + 4.0f, r.fY + 6.0f, 1.2f, c);
	if ( ret != XUI_OK ) return ret;
	if ( bAutoHide ) {
		ret = __xuiDockDrawLine(pProxy, pDraw, cx - 3.0f, cy + 4.0f, cx + 3.0f, cy + 4.0f, 1.2f, c);
	}
	return ret;
}

static int __xuiDockDrawOptionGlyph(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t r, uint32_t c)
{
	float y = r.fY + r.fH * 0.5f + 2.0f;
	float cx = r.fX + r.fW * 0.5f;
	if ( !__xuiDockRectRenderable(r) || (__xuiDockAlpha(c) == 0) ) return XUI_OK;
	if ( (pProxy != NULL) && (pProxy->drawTriangleFill != NULL) ) {
		xui_vec2_t a = {cx - 4.0f, y - 4.0f};
		xui_vec2_t b = {cx + 4.0f, y - 4.0f};
		xui_vec2_t d = {cx, y};
		return pProxy->drawTriangleFill(pProxy, pDraw, a, b, d, c);
	}
	return __xuiDockDrawLine(pProxy, pDraw, cx - 4.0f, y - 3.0f, cx, y, 1.0f, c);
}

static int __xuiDockDrawPaneIconFallback(xui_proxy pProxy, xui_draw_context pDraw, const char* sAsset, xui_rect_t r, uint32_t c)
{
	if ( sAsset == NULL ) return XUI_OK;
	if ( strcmp(sAsset, "dock_pane_close") == 0 ) return __xuiDockDrawCloseGlyph(pProxy, pDraw, r, c);
	if ( strcmp(sAsset, "dock_pane_auto_hide") == 0 ) return __xuiDockDrawPinGlyph(pProxy, pDraw, r, c, 0);
	if ( strcmp(sAsset, "dock_pane_dock") == 0 ) return __xuiDockDrawPinGlyph(pProxy, pDraw, r, c, 1);
	return __xuiDockDrawOptionGlyph(pProxy, pDraw, r, c);
}

static int __xuiDockDrawPaneButton(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_dock_panel_data_t* pData, xui_rect_t r, const char* sAsset, int bHot, int bEnabled)
{
	xui_rect_t icon;
	uint32_t iconColor;
	int ret;
	if ( !__xuiDockRectRenderable(r) || (pData == NULL) ) return XUI_OK;
	if ( bHot && bEnabled ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockInset(r, -1.0f, -1.0f), pData->tColors.iTabHoverColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, __xuiDockInset(r, -1.0f, -1.0f), 1.0f, pData->tColors.iFocusColor);
		if ( ret != XUI_OK ) return ret;
	}
	icon = __xuiDockRect(r.fX + (r.fW - 16.0f) * 0.5f, r.fY + (r.fH - 15.0f) * 0.5f, 16.0f, 15.0f);
	iconColor = bEnabled ? XUI_COLOR_WHITE : XUI_COLOR_RGBA(255, 255, 255, 120);
	ret = __xuiDockDrawBuiltinAsset(pWidget, pProxy, pDraw, sAsset, icon, iconColor);
	if ( ret == XUI_OK ) return XUI_OK;
	return __xuiDockDrawPaneIconFallback(pProxy, pDraw, sAsset, r, bEnabled ? pData->tColors.iButtonColor : XUI_COLOR_RGBA(120, 138, 153, 160));
}

static int __xuiDockDrawTabCloseButton(xui_proxy pProxy, xui_draw_context pDraw, xui_dock_panel_data_t* pData, xui_rect_t r, uint32_t iColor, int bHot)
{
	int ret;
	if ( !__xuiDockRectRenderable(r) || (pData == NULL) ) return XUI_OK;
	if ( bHot ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockInset(r, -1.0f, -1.0f), pData->tColors.iTabHoverColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, __xuiDockInset(r, -1.0f, -1.0f), 1.0f, pData->tColors.iFocusColor);
		if ( ret != XUI_OK ) return ret;
	}
	return __xuiDockDrawCloseGlyph(pProxy, pDraw, r, iColor);
}

static const char* __xuiDockPaneIndicatorAssetName(int iSide)
{
	switch ( iSide ) {
	case XUI_DOCK_PANEL_SIDE_LEFT: return "dock_indicator_pane_diamond_left";
	case XUI_DOCK_PANEL_SIDE_RIGHT: return "dock_indicator_pane_diamond_right";
	case XUI_DOCK_PANEL_SIDE_TOP: return "dock_indicator_pane_diamond_top";
	case XUI_DOCK_PANEL_SIDE_BOTTOM: return "dock_indicator_pane_diamond_bottom";
	case XUI_DOCK_PANEL_SIDE_FILL: return "dock_indicator_pane_diamond_fill";
	default: break;
	}
	return "dock_indicator_pane_diamond";
}

static const char* __xuiDockPanelIndicatorAssetName(int iRegion)
{
	switch ( iRegion ) {
	case XUI_DOCK_PANEL_REGION_LEFT: return "dock_indicator_panel_left_active";
	case XUI_DOCK_PANEL_REGION_RIGHT: return "dock_indicator_panel_right_active";
	case XUI_DOCK_PANEL_REGION_TOP: return "dock_indicator_panel_top_active";
	case XUI_DOCK_PANEL_REGION_BOTTOM: return "dock_indicator_panel_bottom_active";
	default: break;
	}
	return "dock_indicator_panel_fill_active";
}

static int __xuiDockAllocNode(xui_dock_panel_data_t* pData, int iType)
{
	int i;
	for ( i = 0; i < XUI_DOCK_PANEL_NODE_CAPACITY; i++ ) {
		if ( !pData->arrNodes[i].bUsed ) {
			memset(&pData->arrNodes[i], 0, sizeof(pData->arrNodes[i]));
			pData->arrNodes[i].bUsed = 1;
			pData->arrNodes[i].iNode = i;
			pData->arrNodes[i].iType = iType;
			pData->arrNodes[i].iParent = -1;
			pData->arrNodes[i].iFirst = -1;
			pData->arrNodes[i].iSecond = -1;
			pData->arrNodes[i].iPane = -1;
			pData->arrNodes[i].fRatio = 0.5f;
			pData->iNodeCount++;
			return i;
		}
	}
	return -1;
}

static int __xuiDockAllocPane(xui_dock_panel_data_t* pData, int iRegion)
{
	int i;
	for ( i = 0; i < XUI_DOCK_PANEL_PANE_CAPACITY; i++ ) {
		if ( !pData->arrPanes[i].bUsed ) {
			memset(&pData->arrPanes[i], 0, sizeof(pData->arrPanes[i]));
			pData->arrPanes[i].bUsed = 1;
			pData->arrPanes[i].iPane = i;
			pData->arrPanes[i].iNode = -1;
			pData->arrPanes[i].iRegion = iRegion;
			pData->arrPanes[i].iActiveIndex = -1;
			pData->iPaneCount++;
			return i;
		}
	}
	return -1;
}

static int __xuiDockAllocPaneNode(xui_dock_panel_data_t* pData, int iRegion)
{
	int pane;
	int node;
	pane = __xuiDockAllocPane(pData, iRegion);
	if ( pane < 0 ) return -1;
	node = __xuiDockAllocNode(pData, XUI_DOCK_NODE_PANE);
	if ( node < 0 ) {
		pData->arrPanes[pane].bUsed = 0;
		pData->iPaneCount--;
		return -1;
	}
	pData->arrPanes[pane].iNode = node;
	pData->arrNodes[node].iPane = pane;
	return node;
}

static void __xuiDockFreeNode(xui_dock_panel_data_t* pData, int iNode)
{
	if ( (iNode < 0) || (iNode >= XUI_DOCK_PANEL_NODE_CAPACITY) || !pData->arrNodes[iNode].bUsed ) return;
	memset(&pData->arrNodes[iNode], 0, sizeof(pData->arrNodes[iNode]));
	pData->iNodeCount--;
}

static void __xuiDockFreePane(xui_dock_panel_data_t* pData, int iPane)
{
	if ( (iPane < 0) || (iPane >= XUI_DOCK_PANEL_PANE_CAPACITY) || !pData->arrPanes[iPane].bUsed ) return;
	memset(&pData->arrPanes[iPane], 0, sizeof(pData->arrPanes[iPane]));
	pData->iPaneCount--;
}

static xui_dock_window_slot_t* __xuiDockWindowAt(xui_dock_panel_data_t* pData, int iWindow)
{
	if ( (pData == NULL) || (iWindow < 0) || (iWindow >= XUI_DOCK_PANEL_WINDOW_CAPACITY) || !pData->arrWindows[iWindow].bUsed ) return NULL;
	return &pData->arrWindows[iWindow];
}

static xui_dock_pane_slot_t* __xuiDockPaneAt(xui_dock_panel_data_t* pData, int iPane)
{
	if ( (pData == NULL) || (iPane < 0) || (iPane >= XUI_DOCK_PANEL_PANE_CAPACITY) || !pData->arrPanes[iPane].bUsed ) return NULL;
	return &pData->arrPanes[iPane];
}

static xui_dock_node_slot_t* __xuiDockNodeAt(xui_dock_panel_data_t* pData, int iNode)
{
	if ( (pData == NULL) || (iNode < 0) || (iNode >= XUI_DOCK_PANEL_NODE_CAPACITY) || !pData->arrNodes[iNode].bUsed ) return NULL;
	return &pData->arrNodes[iNode];
}

static void __xuiDockInvalidate(xui_widget pWidget, int bLayout)
{
	uint32_t flags = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	if ( bLayout ) flags |= XUI_WIDGET_DIRTY_LAYOUT;
	(void)xuiWidgetInvalidate(pWidget, flags);
}

static void __xuiDockDropInfoReset(xui_dock_drop_info_t* pInfo)
{
	if ( pInfo == NULL ) return;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iWindow = -1;
	pInfo->iPane = -1;
	pInfo->iRegion = -1;
	pInfo->iSide = XUI_DOCK_PANEL_SIDE_NONE;
}

static int __xuiDockSetDragPreview(xui_widget pWidget, xui_dock_panel_data_t* pData, const xui_dock_drop_info_t* pInfo)
{
	int bChanged;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bChanged = 0;
	if ( pInfo == NULL || !pInfo->bValid ) {
		bChanged = pData->tDragPreview.bValid ||
			(pData->tDragPreview.iWindow >= 0) ||
			(pData->tDragPreview.iPane >= 0) ||
			(pData->tDragPreview.iRegion >= 0) ||
			pData->tDragIndicator.bValid ||
			(pData->tDragIndicator.iWindow >= 0) ||
			(pData->tDragIndicator.iPane >= 0) ||
			(pData->tDragIndicator.iRegion >= 0);
		__xuiDockDropInfoReset(&pData->tDragPreview);
		__xuiDockDropInfoReset(&pData->tDragIndicator);
	} else {
		bChanged = (pData->tDragPreview.bValid != pInfo->bValid) ||
			(pData->tDragPreview.iWindow != pInfo->iWindow) ||
			(pData->tDragPreview.iPane != pInfo->iPane) ||
			(pData->tDragPreview.iRegion != pInfo->iRegion) ||
			(pData->tDragPreview.iSide != pInfo->iSide) ||
			(pData->tDragPreview.tRect.fX != pInfo->tRect.fX) ||
			(pData->tDragPreview.tRect.fY != pInfo->tRect.fY) ||
			(pData->tDragPreview.tRect.fW != pInfo->tRect.fW) ||
			(pData->tDragPreview.tRect.fH != pInfo->tRect.fH);
		pData->tDragPreview = *pInfo;
		pData->tDragPreview.iSize = sizeof(pData->tDragPreview);
	}
	if ( bChanged ) {
		if ( pWidget != NULL ) __xuiDockInvalidate(pWidget, 0);
		__xuiDockSyncDragOverlay(pWidget, pData);
	}
	return XUI_OK;
}

static int __xuiDockDragVisualActive(xui_dock_panel_data_t* pData)
{
	if ( pData == NULL ) return 0;
	if ( pData->tDragPreview.bValid && pData->tDragPreview.tRect.fW > 0.0f && pData->tDragPreview.tRect.fH > 0.0f ) return 1;
	if ( pData->tDragIndicator.bValid ) return 1;
	return 0;
}

static void __xuiDockSyncDragOverlay(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	int bVisible;
	(void)pWidget;
	if ( (pData == NULL) || (pData->pDragOverlayWidget == NULL) ) return;
	bVisible = __xuiDockDragVisualActive(pData);
	(void)xuiWidgetSetVisible(pData->pDragOverlayWidget, bVisible);
	(void)xuiWidgetSetLayer(pData->pDragOverlayWidget, XUI_LAYER_DRAG, XUI_WINDOW_Z_TOPMOST + 200);
	(void)xuiWidgetInvalidate(pData->pDragOverlayWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
}

static int __xuiDockSetDragIndicator(xui_widget pWidget, xui_dock_panel_data_t* pData, const xui_dock_drop_info_t* pInfo)
{
	int bChanged;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pInfo == NULL || !pInfo->bValid ) {
		bChanged = pData->tDragIndicator.bValid ||
			(pData->tDragIndicator.iWindow >= 0) ||
			(pData->tDragIndicator.iPane >= 0) ||
			(pData->tDragIndicator.iRegion >= 0);
		__xuiDockDropInfoReset(&pData->tDragIndicator);
	} else {
		bChanged = (pData->tDragIndicator.bValid != pInfo->bValid) ||
			(pData->tDragIndicator.iWindow != pInfo->iWindow) ||
			(pData->tDragIndicator.iPane != pInfo->iPane) ||
			(pData->tDragIndicator.iRegion != pInfo->iRegion) ||
			(pData->tDragIndicator.iSide != pInfo->iSide) ||
			(pData->tDragIndicator.tRect.fX != pInfo->tRect.fX) ||
			(pData->tDragIndicator.tRect.fY != pInfo->tRect.fY) ||
			(pData->tDragIndicator.tRect.fW != pInfo->tRect.fW) ||
			(pData->tDragIndicator.tRect.fH != pInfo->tRect.fH);
		pData->tDragIndicator = *pInfo;
		pData->tDragIndicator.iSize = sizeof(pData->tDragIndicator);
	}
	if ( bChanged ) {
		if ( pWidget != NULL ) __xuiDockInvalidate(pWidget, 0);
		__xuiDockSyncDragOverlay(pWidget, pData);
	}
	return XUI_OK;
}

static int __xuiDockFloatOrderIndex(xui_dock_panel_data_t* pData, int iWindow)
{
	int i;
	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iFloatCount && i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrFloatOrder[i] == iWindow ) return i;
	}
	return -1;
}

static void __xuiDockFloatOrderRefreshLayers(xui_dock_panel_data_t* pData)
{
	int i;
	if ( pData == NULL ) return;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed ) __xuiDockSetHostLayer(&pData->arrWindows[i]);
	}
	__xuiDockRefreshHostInputOrder(pData);
}

static int __xuiDockFloatOrderRemove(xui_dock_panel_data_t* pData, int iWindow)
{
	int idx;
	int i;
	if ( pData == NULL ) return 0;
	idx = __xuiDockFloatOrderIndex(pData, iWindow);
	if ( idx < 0 ) return 0;
	for ( i = idx; i < pData->iFloatCount - 1 && i < XUI_DOCK_PANEL_WINDOW_CAPACITY - 1; i++ ) {
		pData->arrFloatOrder[i] = pData->arrFloatOrder[i + 1];
	}
	if ( pData->iFloatCount > 0 ) pData->iFloatCount--;
	if ( pData->iFloatCount >= 0 && pData->iFloatCount < XUI_DOCK_PANEL_WINDOW_CAPACITY ) {
		pData->arrFloatOrder[pData->iFloatCount] = -1;
	}
	__xuiDockFloatOrderRefreshLayers(pData);
	return 1;
}

static int __xuiDockFloatOrderBringToFront(xui_dock_panel_data_t* pData, int iWindow)
{
	int oldIndex;
	if ( (pData == NULL) || (iWindow < 0) || (iWindow >= XUI_DOCK_PANEL_WINDOW_CAPACITY) || !pData->arrWindows[iWindow].bUsed ) return 0;
	oldIndex = __xuiDockFloatOrderIndex(pData, iWindow);
	if ( oldIndex == pData->iFloatCount - 1 && oldIndex >= 0 ) return 0;
	if ( oldIndex >= 0 ) {
		int i;
		for ( i = oldIndex; i < pData->iFloatCount - 1 && i < XUI_DOCK_PANEL_WINDOW_CAPACITY - 1; i++ ) {
			pData->arrFloatOrder[i] = pData->arrFloatOrder[i + 1];
		}
		if ( pData->iFloatCount > 0 ) pData->iFloatCount--;
	}
	if ( pData->iFloatCount >= XUI_DOCK_PANEL_WINDOW_CAPACITY ) return 0;
	pData->arrFloatOrder[pData->iFloatCount++] = iWindow;
	__xuiDockFloatOrderRefreshLayers(pData);
	return 1;
}

static void __xuiDockFloatOrderClear(xui_dock_panel_data_t* pData)
{
	int i;
	if ( pData == NULL ) return;
	pData->iFloatCount = 0;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) pData->arrFloatOrder[i] = -1;
}

static void __xuiDockNotifyState(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_window_slot_t* pWin, int iOldState, int iNewState)
{
	if ( iOldState == iNewState ) return;
	pData->iChangeCount++;
	pData->iWindowChangeCount++;
	if ( pData->onStateChanged != NULL ) {
		pData->onStateChanged(pWidget, pWin->iWindow, iOldState, iNewState, pData->pStateUser);
	}
}

static void __xuiDockSetWindowState(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_window_slot_t* pWin, int iState)
{
	int oldState;
	if ( pWin == NULL ) return;
	oldState = pWin->iState;
	pWin->iState = iState;
	if ( pData != NULL ) {
		if ( iState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
			(void)__xuiDockFloatOrderBringToFront(pData, pWin->iWindow);
		} else if ( oldState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
			(void)__xuiDockFloatOrderRemove(pData, pWin->iWindow);
		}
	}
	__xuiDockNotifyState(pWidget, pData, pWin, oldState, iState);
}

static void __xuiDockSetWidgetTreeLayer(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pChild;
	if ( pWidget == NULL ) return;
	(void)xuiWidgetSetLayer(pWidget, iLayer, iZIndex);
	for ( pChild = xuiWidgetGetFirstChild(pWidget); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		__xuiDockSetWidgetTreeLayer(pChild, iLayer, iZIndex);
	}
}

static void __xuiDockSetHostLayer(xui_dock_window_slot_t* pWin)
{
	xui_dock_panel_data_t* pData;
	int iLayer;
	int iZIndex;
	if ( pWin->pHostWidget == NULL ) return;
	pData = pWin->pOwner;
	if ( pWin->iState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
		int idx = __xuiDockFloatOrderIndex(pData, pWin->iWindow);
		if ( idx < 0 ) idx = pWin->iWindow;
		iLayer = XUI_LAYER_FLOATING;
		iZIndex = XUI_WINDOW_Z_NORMAL + idx + 1;
	} else if ( pData != NULL && pWin->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->iAutoHideExpandWindow == pWin->iWindow ) {
		iLayer = XUI_LAYER_FLOATING;
		iZIndex = XUI_WINDOW_Z_NORMAL + XUI_DOCK_PANEL_WINDOW_CAPACITY + pWin->iWindow + 1;
	} else {
		iLayer = XUI_LAYER_NORMAL;
		iZIndex = 0;
	}
	__xuiDockSetWidgetTreeLayer(pWin->pHostWidget, iLayer, iZIndex);
}

static void __xuiDockBringHostToSiblingFront(xui_dock_window_slot_t* pWin)
{
	xui_widget pParent;
	int iLayer;
	int iZIndex;
	if ( (pWin == NULL) || (pWin->pHostWidget == NULL) ) return;
	pParent = xuiWidgetGetParent(pWin->pHostWidget);
	if ( (pParent == NULL) || (xuiWidgetGetLastChild(pParent) == pWin->pHostWidget) ) return;
	iLayer = 0;
	iZIndex = 0;
	(void)xuiWidgetGetLayer(pWin->pHostWidget, &iLayer, &iZIndex);
	if ( xuiWidgetRemoveFromParent(pWin->pHostWidget) != XUI_OK ) return;
	if ( xuiWidgetAddChild(pParent, pWin->pHostWidget) != XUI_OK ) return;
	(void)xuiWidgetSetLayer(pWin->pHostWidget, iLayer, iZIndex);
}

static void __xuiDockRefreshHostInputOrder(xui_dock_panel_data_t* pData)
{
	xui_context pContext;
	xui_widget pCapture;
	xui_widget pParent;
	xui_widget pChild;
	int arrDesired[XUI_DOCK_PANEL_WINDOW_CAPACITY + 1];
	int iDesiredCount;
	int iDesiredIndex;
	int bElevatedSeen;
	int bOrderMatches;
	int i;
	if ( pData == NULL ) return;
	pContext = NULL;
	pParent = NULL;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed && pData->arrWindows[i].pPanelWidget != NULL ) {
			pContext = xuiWidgetGetContext(pData->arrWindows[i].pPanelWidget);
			pParent = pData->arrWindows[i].pPanelWidget;
			break;
		}
	}
	pCapture = (pContext != NULL) ? xuiGetPointerCapture(pContext) : NULL;
	if ( __xuiDockPointerTargetIsFloatingHost(pData, pCapture) ) return;
	iDesiredCount = 0;
	for ( i = 0; i < pData->iFloatCount && i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pData->arrFloatOrder[i]);
		if ( (w != NULL) && (w->iState == XUI_DOCK_PANEL_WINDOW_FLOATING) && xuiWidgetGetVisible(w->pHostWidget) ) {
			arrDesired[iDesiredCount++] = w->iWindow;
		}
	}
	if ( pData->iAutoHideExpandWindow >= 0 ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pData->iAutoHideExpandWindow);
		if ( (w != NULL) && (w->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE) && xuiWidgetGetVisible(w->pHostWidget) ) {
			arrDesired[iDesiredCount++] = w->iWindow;
		}
	}
	if ( pParent == NULL || iDesiredCount <= 0 ) return;
	iDesiredIndex = 0;
	bElevatedSeen = 0;
	bOrderMatches = 1;
	for ( pChild = xuiWidgetGetFirstChild(pParent); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		xui_dock_window_slot_t* w = NULL;
		for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
			if ( pData->arrWindows[i].bUsed && pData->arrWindows[i].pHostWidget == pChild ) {
				w = &pData->arrWindows[i];
				break;
			}
		}
		if ( w == NULL || !xuiWidgetGetVisible(pChild) ) continue;
		if ( (w->iState == XUI_DOCK_PANEL_WINDOW_FLOATING) ||
		     (w->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->iAutoHideExpandWindow == w->iWindow) ) {
			bElevatedSeen = 1;
			if ( iDesiredIndex >= iDesiredCount || arrDesired[iDesiredIndex] != w->iWindow ) {
				bOrderMatches = 0;
				break;
			}
			iDesiredIndex++;
		} else if ( bElevatedSeen ) {
			bOrderMatches = 0;
			break;
		}
	}
	if ( bOrderMatches && iDesiredIndex == iDesiredCount ) return;
	for ( i = 0; i < iDesiredCount; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, arrDesired[i]);
		if ( w != NULL ) __xuiDockBringHostToSiblingFront(w);
	}
}

static int __xuiDockPaneIndexOfWindow(xui_dock_pane_slot_t* pPane, int iWindow)
{
	int i;
	if ( pPane == NULL ) return -1;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		if ( pPane->arrWindows[i] == iWindow ) return i;
	}
	return -1;
}

static int __xuiDockPaneActiveWindow(xui_dock_pane_slot_t* pPane)
{
	if ( (pPane == NULL) || (pPane->iWindowCount <= 0) || (pPane->iActiveIndex < 0) || (pPane->iActiveIndex >= pPane->iWindowCount) ) return -1;
	return pPane->arrWindows[pPane->iActiveIndex];
}

static xui_widget __xuiDockFindFocusableWidget(xui_widget pWidget)
{
	xui_widget pChild;
	xui_widget pFocus;
	if ( (pWidget == NULL) || !pWidget->bVisible || !pWidget->bEnabled ) return NULL;
	if ( pWidget->bFocusable ) return pWidget;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		pFocus = __xuiDockFindFocusableWidget(pChild);
		if ( pFocus != NULL ) return pFocus;
	}
	return NULL;
}

static int __xuiDockApplyPendingFocus(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	xui_dock_window_slot_t* pWin;
	xui_widget pFocus;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->iPendingFocusWindow < 0) ) return XUI_OK;
	pWin = __xuiDockWindowAt(pData, pData->iPendingFocusWindow);
	if ( pWin == NULL ) {
		pData->iPendingFocusWindow = -1;
		return XUI_OK;
	}
	if ( (pWin->iState == XUI_DOCK_PANEL_WINDOW_HIDDEN) || (pWin->pHostWidget == NULL) || !xuiWidgetGetVisible(pWin->pHostWidget) ) {
		if ( pWin->iState == XUI_DOCK_PANEL_WINDOW_HIDDEN ) pData->iPendingFocusWindow = -1;
		return XUI_OK;
	}
	pFocus = __xuiDockFindFocusableWidget(pWin->pClientWidget);
	if ( pFocus == NULL ) return XUI_OK;
	if ( xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pFocus) == XUI_OK ) {
		pData->iPendingFocusWindow = -1;
	}
	return XUI_OK;
}

static int __xuiDockRequestFocusWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow)
{
	if ( (pWidget == NULL) || (pData == NULL) || (iWindow < 0) ) return XUI_OK;
	pData->iPendingFocusWindow = iWindow;
	return __xuiDockApplyPendingFocus(pWidget, pData);
}

static void __xuiDockPaneNotifyActiveChanged(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int oldWindow)
{
	int newWindow;
	if ( (pData == NULL) || (pPane == NULL) ) return;
	newWindow = __xuiDockPaneActiveWindow(pPane);
	if ( oldWindow == newWindow ) return;
	(void)__xuiDockRequestFocusWindow(pWidget, pData, newWindow);
	if ( pData->onActiveChanged != NULL ) {
		pData->onActiveChanged(pWidget, pPane->iPane, oldWindow, newWindow, pData->pActiveUser);
	}
}

static int __xuiDockWindowAutoHideStripRegionFromPane(xui_dock_panel_data_t* pData, xui_dock_window_slot_t* pWin)
{
	xui_dock_pane_slot_t* pPane;
	if ( (pData == NULL) || (pWin == NULL) || (pWin->iState != XUI_DOCK_PANEL_WINDOW_DOCKED) || !pWin->bDockable ) return -1;
	pPane = __xuiDockPaneAt(pData, pWin->iPane);
	if ( pPane == NULL ) return -1;
	if ( __xuiDockRegionIsAutoHideSide(pPane->iRegion) ) return pPane->iRegion;
	if ( pPane->iRegion == XUI_DOCK_PANEL_REGION_DOCUMENT && __xuiDockSideIsSplit(pWin->iLastSide) ) {
		return __xuiDockRegionFromSide(pWin->iLastSide);
	}
	return -1;
}

static int __xuiDockPaneCanAutoHide(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane)
{
	xui_dock_window_slot_t* pWin;
	if ( pPane == NULL ) return 0;
	pWin = __xuiDockWindowAt(pData, __xuiDockPaneActiveWindow(pPane));
	return __xuiDockRegionIsAutoHideSide(__xuiDockWindowAutoHideStripRegionFromPane(pData, pWin));
}

static int __xuiDockPaneUsesTabs(const xui_dock_pane_slot_t* pPane)
{
	return (pPane != NULL) && (pPane->iWindowCount > 1);
}

static int __xuiDockPaneSetActiveIndex(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iIndex)
{
	int oldWindow;
	if ( (pPane == NULL) || (iIndex < 0) || (iIndex >= pPane->iWindowCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	oldWindow = __xuiDockPaneActiveWindow(pPane);
	if ( pPane->iActiveIndex == iIndex ) return XUI_OK;
	pPane->iActiveIndex = iIndex;
	pData->iChangeCount++;
	__xuiDockPaneNotifyActiveChanged(pWidget, pData, pPane, oldWindow);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

static void __xuiDockPaneRefreshLastTabIndices(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane)
{
	int i;
	if ( (pData == NULL) || (pPane == NULL) ) return;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* pWin = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( pWin != NULL ) pWin->iLastTabIndex = i;
	}
}

static int __xuiDockPaneMoveWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iFrom, int iInsert)
{
	int iWindow;
	int oldWindow;
	int i;
	if ( (pData == NULL) || (pPane == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iFrom < 0) || (iFrom >= pPane->iWindowCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iInsert < 0 ) iInsert = 0;
	if ( iInsert > pPane->iWindowCount ) iInsert = pPane->iWindowCount;
	if ( (iInsert == iFrom) || (iInsert == iFrom + 1) ) return XUI_OK;
	oldWindow = __xuiDockPaneActiveWindow(pPane);
	iWindow = pPane->arrWindows[iFrom];
	if ( iInsert > iFrom ) iInsert--;
	for ( i = iFrom; i < pPane->iWindowCount - 1; i++ ) {
		pPane->arrWindows[i] = pPane->arrWindows[i + 1];
	}
	for ( i = pPane->iWindowCount - 1; i > iInsert; i-- ) {
		pPane->arrWindows[i] = pPane->arrWindows[i - 1];
	}
	pPane->arrWindows[iInsert] = iWindow;
	pPane->iActiveIndex = iInsert;
	__xuiDockPaneRefreshLastTabIndices(pData, pPane);
	pData->iChangeCount++;
	pData->iLayoutChangeCount++;
	__xuiDockPaneNotifyActiveChanged(pWidget, pData, pPane, oldWindow);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

static int __xuiDockPaneAddWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iWindow)
{
	xui_dock_window_slot_t* pWin;
	int iInsert;
	int oldWindow;
	int idx;
	int i;
	if ( pPane == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( pWin == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	idx = __xuiDockPaneIndexOfWindow(pPane, iWindow);
	if ( idx >= 0 ) {
		pWin->iLastTabIndex = idx;
		return __xuiDockPaneSetActiveIndex(pWidget, pData, pPane, idx);
	}
	if ( pPane->iWindowCount >= XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	oldWindow = __xuiDockPaneActiveWindow(pPane);
	iInsert = pPane->iWindowCount;
	if ( pWin->iLastTabIndex >= 0 && pWin->iLastTabIndex <= pPane->iWindowCount ) iInsert = pWin->iLastTabIndex;
	for ( i = pPane->iWindowCount; i > iInsert; i-- ) {
		pPane->arrWindows[i] = pPane->arrWindows[i - 1];
	}
	pPane->arrWindows[iInsert] = iWindow;
	pPane->iWindowCount++;
	pPane->iActiveIndex = iInsert;
	pWin->iPane = pPane->iPane;
	pWin->iRegion = pPane->iRegion;
	pWin->iLastRegion = pPane->iRegion;
	pWin->iLastSide = XUI_DOCK_PANEL_SIDE_FILL;
	if ( __xuiDockRectRenderable(pPane->tRect) ) pWin->tLastDockRect = pPane->tRect;
	__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_DOCKED);
	__xuiDockPaneRefreshLastTabIndices(pData, pPane);
	pData->iChangeCount++;
	pData->iLayoutChangeCount++;
	__xuiDockPaneNotifyActiveChanged(pWidget, pData, pPane, oldWindow);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

static void __xuiDockRememberLastDockRect(xui_dock_panel_data_t* pData, xui_dock_window_slot_t* pWin)
{
	xui_dock_pane_slot_t* pPane;
	if ( (pData == NULL) || (pWin == NULL) || (pWin->iPane < 0) ) return;
	pPane = __xuiDockPaneAt(pData, pWin->iPane);
	if ( (pPane != NULL) && __xuiDockRectRenderable(pPane->tRect) ) {
		pWin->tLastDockRect = pPane->tRect;
	}
}

static void __xuiDockReplaceRootOrChild(xui_dock_panel_data_t* pData, int iOldNode, int iNewNode)
{
	xui_dock_node_slot_t* oldNode;
	xui_dock_node_slot_t* parent;
	int parentIndex;
	int i;
	oldNode = __xuiDockNodeAt(pData, iOldNode);
	if ( oldNode == NULL ) return;
	parentIndex = oldNode->iParent;
	if ( parentIndex < 0 ) {
		for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
			if ( pData->arrRegions[i].iRootNode == iOldNode ) {
				pData->arrRegions[i].iRootNode = iNewNode;
				break;
			}
		}
	} else {
		parent = __xuiDockNodeAt(pData, parentIndex);
		if ( parent != NULL ) {
			if ( parent->iFirst == iOldNode ) parent->iFirst = iNewNode;
			if ( parent->iSecond == iOldNode ) parent->iSecond = iNewNode;
		}
	}
	if ( iNewNode >= 0 ) {
		pData->arrNodes[iNewNode].iParent = parentIndex;
	}
}

static void __xuiDockCollapsePaneIfEmpty(xui_dock_panel_data_t* pData, int iPane)
{
	xui_dock_pane_slot_t* pPane;
	xui_dock_node_slot_t* pNode;
	xui_dock_node_slot_t* pParent;
	int nodeIndex;
	int parentIndex;
	int siblingIndex;
	int grandIndex;
	pPane = __xuiDockPaneAt(pData, iPane);
	if ( (pPane == NULL) || (pPane->iWindowCount > 0) ) return;
	nodeIndex = pPane->iNode;
	pNode = __xuiDockNodeAt(pData, nodeIndex);
	parentIndex = (pNode != NULL) ? pNode->iParent : -1;
	if ( parentIndex < 0 ) {
		__xuiDockReplaceRootOrChild(pData, nodeIndex, -1);
		__xuiDockFreeNode(pData, nodeIndex);
		__xuiDockFreePane(pData, iPane);
		return;
	}
	pParent = __xuiDockNodeAt(pData, parentIndex);
	if ( pParent == NULL ) return;
	siblingIndex = (pParent->iFirst == nodeIndex) ? pParent->iSecond : pParent->iFirst;
	grandIndex = pParent->iParent;
	__xuiDockReplaceRootOrChild(pData, parentIndex, siblingIndex);
	if ( siblingIndex >= 0 ) pData->arrNodes[siblingIndex].iParent = grandIndex;
	__xuiDockFreeNode(pData, nodeIndex);
	__xuiDockFreeNode(pData, parentIndex);
	__xuiDockFreePane(pData, iPane);
}

static int __xuiDockPaneRemoveWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane, int iWindow)
{
	xui_dock_pane_slot_t* pPane;
	xui_dock_window_slot_t* pWin;
	int oldWindow;
	int idx;
	int i;
	pPane = __xuiDockPaneAt(pData, iPane);
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( (pPane == NULL) || (pWin == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	idx = __xuiDockPaneIndexOfWindow(pPane, iWindow);
	if ( idx < 0 ) return XUI_OK;
	oldWindow = __xuiDockPaneActiveWindow(pPane);
	pWin->iLastTabIndex = idx;
	for ( i = idx; i < pPane->iWindowCount - 1; i++ ) {
		pPane->arrWindows[i] = pPane->arrWindows[i + 1];
	}
	pPane->iWindowCount--;
	if ( pPane->iWindowCount <= 0 ) {
		pPane->iActiveIndex = -1;
	} else if ( pPane->iActiveIndex >= pPane->iWindowCount ) {
		pPane->iActiveIndex = pPane->iWindowCount - 1;
	} else if ( pPane->iActiveIndex > idx ) {
		pPane->iActiveIndex--;
	}
	pWin->iPane = -1;
	__xuiDockPaneRefreshLastTabIndices(pData, pPane);
	__xuiDockPaneNotifyActiveChanged(pWidget, pData, pPane, oldWindow);
	__xuiDockCollapsePaneIfEmpty(pData, iPane);
	pData->iChangeCount++;
	pData->iLayoutChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

static int __xuiDockDetachWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow)
{
	xui_dock_window_slot_t* pWin;
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( pWin == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWin->iPane >= 0 ) {
		return __xuiDockPaneRemoveWindow(pWidget, pData, pWin->iPane, iWindow);
	}
	return XUI_OK;
}

static int __xuiDockFirstPaneInNode(xui_dock_panel_data_t* pData, int iNode)
{
	xui_dock_node_slot_t* pNode = __xuiDockNodeAt(pData, iNode);
	int pane;
	if ( pNode == NULL ) return -1;
	if ( pNode->iType == XUI_DOCK_NODE_PANE ) return pNode->iPane;
	if ( pNode->iType == XUI_DOCK_NODE_SPLIT ) {
		pane = __xuiDockFirstPaneInNode(pData, pNode->iFirst);
		if ( pane >= 0 ) return pane;
		return __xuiDockFirstPaneInNode(pData, pNode->iSecond);
	}
	return -1;
}

static int __xuiDockDockWindowToRegion(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow, int iRegion, int iSide, float fRatio, int* pPane)
{
	xui_dock_region_slot_t* pRegion;
	xui_dock_node_slot_t* pSplit;
	xui_dock_window_slot_t* pWin;
	int oldRoot;
	int newPaneNode;
	int splitNode;
	int paneIndex;
	int oldFirst;
	int ret;
	if ( !__xuiDockRegionValid(iRegion) || !__xuiDockSideValid(iSide) ) return XUI_ERROR_INVALID_ARGUMENT;
	pRegion = &pData->arrRegions[iRegion];
	if ( iSide == XUI_DOCK_PANEL_SIDE_NONE ) iSide = XUI_DOCK_PANEL_SIDE_FILL;
	fRatio = __xuiDockClamp((fRatio > 0.0f) ? fRatio : 0.25f, 0.10f, 0.90f);
	ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
	if ( ret != XUI_OK ) return ret;
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( pWin == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pRegion = &pData->arrRegions[iRegion];
	if ( (iSide == XUI_DOCK_PANEL_SIDE_FILL) || (pRegion->iRootNode < 0) ) {
		if ( pRegion->iRootNode < 0 ) {
			pRegion->iRootNode = __xuiDockAllocPaneNode(pData, iRegion);
			if ( pRegion->iRootNode < 0 ) return XUI_ERROR_OUT_OF_MEMORY;
		}
		paneIndex = __xuiDockFirstPaneInNode(pData, pRegion->iRootNode);
		if ( paneIndex < 0 ) return XUI_ERROR_NOT_INITIALIZED;
		if ( pPane != NULL ) *pPane = paneIndex;
		ret = __xuiDockPaneAddWindow(pWidget, pData, &pData->arrPanes[paneIndex], iWindow);
		if ( ret == XUI_OK ) {
			pWin = __xuiDockWindowAt(pData, iWindow);
			if ( pWin != NULL ) pWin->iLastSide = XUI_DOCK_PANEL_SIDE_FILL;
		}
		return ret;
	}
	if ( !__xuiDockSideIsSplit(iSide) ) return XUI_ERROR_INVALID_ARGUMENT;
	oldRoot = pRegion->iRootNode;
	newPaneNode = __xuiDockAllocPaneNode(pData, iRegion);
	if ( newPaneNode < 0 ) return XUI_ERROR_OUT_OF_MEMORY;
	splitNode = __xuiDockAllocNode(pData, XUI_DOCK_NODE_SPLIT);
	if ( splitNode < 0 ) {
		paneIndex = pData->arrNodes[newPaneNode].iPane;
		__xuiDockFreeNode(pData, newPaneNode);
		__xuiDockFreePane(pData, paneIndex);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pSplit = &pData->arrNodes[splitNode];
	pSplit->iOrientation = ((iSide == XUI_DOCK_PANEL_SIDE_LEFT) || (iSide == XUI_DOCK_PANEL_SIDE_RIGHT)) ? XUI_DOCK_ORIENTATION_VERTICAL : XUI_DOCK_ORIENTATION_HORIZONTAL;
	if ( (iSide == XUI_DOCK_PANEL_SIDE_LEFT) || (iSide == XUI_DOCK_PANEL_SIDE_TOP) ) {
		pSplit->iFirst = newPaneNode;
		pSplit->iSecond = oldRoot;
		pSplit->fRatio = fRatio;
	} else {
		oldFirst = oldRoot;
		pSplit->iFirst = oldFirst;
		pSplit->iSecond = newPaneNode;
		pSplit->fRatio = 1.0f - fRatio;
	}
	pData->arrNodes[newPaneNode].iParent = splitNode;
	pData->arrNodes[oldRoot].iParent = splitNode;
	pRegion->iRootNode = splitNode;
	paneIndex = pData->arrNodes[newPaneNode].iPane;
	if ( pPane != NULL ) *pPane = paneIndex;
	ret = __xuiDockPaneAddWindow(pWidget, pData, &pData->arrPanes[paneIndex], iWindow);
	if ( ret == XUI_OK ) {
		pWin = __xuiDockWindowAt(pData, iWindow);
		if ( pWin != NULL ) pWin->iLastSide = iSide;
	}
	return ret;
}

static int __xuiDockDockWindowToPaneSide(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow, int iPane, int iSide, float fRatio, int* pNewPane)
{
	xui_dock_pane_slot_t* pTargetPane;
	xui_dock_node_slot_t* pTargetNode;
	xui_dock_node_slot_t* pSplit;
	xui_dock_window_slot_t* pWin;
	int targetNode;
	int parentNode;
	int newPaneNode;
	int splitNode;
	int newPane;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiDockSideValid(iSide) ) return XUI_ERROR_INVALID_ARGUMENT;
	pTargetPane = __xuiDockPaneAt(pData, iPane);
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( (pTargetPane == NULL) || (pWin == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iSide == XUI_DOCK_PANEL_SIDE_NONE ) iSide = XUI_DOCK_PANEL_SIDE_FILL;
	if ( iSide == XUI_DOCK_PANEL_SIDE_FILL ) {
		if ( pWin->iPane == iPane ) {
			if ( pNewPane != NULL ) *pNewPane = iPane;
			return __xuiDockPaneAddWindow(pWidget, pData, pTargetPane, iWindow);
		}
		ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
		if ( ret != XUI_OK ) return ret;
		pWin = __xuiDockWindowAt(pData, iWindow);
		pTargetPane = __xuiDockPaneAt(pData, iPane);
		if ( pTargetPane == NULL || pWin == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( pNewPane != NULL ) *pNewPane = iPane;
		return __xuiDockPaneAddWindow(pWidget, pData, pTargetPane, iWindow);
	}
	if ( !__xuiDockSideIsSplit(iSide) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pWin->iPane == iPane) && (pTargetPane->iWindowCount <= 1) ) return XUI_OK;
	targetNode = pTargetPane->iNode;
	pTargetNode = __xuiDockNodeAt(pData, targetNode);
	if ( (pTargetNode == NULL) || (pTargetNode->iType != XUI_DOCK_NODE_PANE) ) return XUI_ERROR_INVALID_ARGUMENT;
	parentNode = pTargetNode->iParent;
	fRatio = __xuiDockClamp((fRatio > 0.0f) ? fRatio : 0.25f, 0.10f, 0.90f);
	newPaneNode = __xuiDockAllocPaneNode(pData, pTargetPane->iRegion);
	splitNode = __xuiDockAllocNode(pData, XUI_DOCK_NODE_SPLIT);
	if ( (newPaneNode < 0) || (splitNode < 0) ) {
		if ( newPaneNode >= 0 ) {
			newPane = pData->arrNodes[newPaneNode].iPane;
			__xuiDockFreeNode(pData, newPaneNode);
			__xuiDockFreePane(pData, newPane);
		}
		if ( splitNode >= 0 ) __xuiDockFreeNode(pData, splitNode);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
	if ( ret != XUI_OK ) {
		newPane = pData->arrNodes[newPaneNode].iPane;
		__xuiDockFreeNode(pData, newPaneNode);
		__xuiDockFreePane(pData, newPane);
		__xuiDockFreeNode(pData, splitNode);
		return ret;
	}
	pWin = __xuiDockWindowAt(pData, iWindow);
	pTargetPane = __xuiDockPaneAt(pData, iPane);
	pTargetNode = __xuiDockNodeAt(pData, targetNode);
	if ( (pWin == NULL) || (pTargetPane == NULL) || (pTargetNode == NULL) || (pTargetNode->iType != XUI_DOCK_NODE_PANE) ) {
		newPane = pData->arrNodes[newPaneNode].iPane;
		__xuiDockFreeNode(pData, newPaneNode);
		__xuiDockFreePane(pData, newPane);
		__xuiDockFreeNode(pData, splitNode);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSplit = &pData->arrNodes[splitNode];
	pSplit->iParent = parentNode;
	pSplit->iOrientation = ((iSide == XUI_DOCK_PANEL_SIDE_LEFT) || (iSide == XUI_DOCK_PANEL_SIDE_RIGHT)) ? XUI_DOCK_ORIENTATION_VERTICAL : XUI_DOCK_ORIENTATION_HORIZONTAL;
	if ( (iSide == XUI_DOCK_PANEL_SIDE_LEFT) || (iSide == XUI_DOCK_PANEL_SIDE_TOP) ) {
		pSplit->iFirst = newPaneNode;
		pSplit->iSecond = targetNode;
		pSplit->fRatio = fRatio;
	} else {
		pSplit->iFirst = targetNode;
		pSplit->iSecond = newPaneNode;
		pSplit->fRatio = 1.0f - fRatio;
	}
	pData->arrNodes[newPaneNode].iParent = splitNode;
	__xuiDockReplaceRootOrChild(pData, targetNode, splitNode);
	pData->arrNodes[targetNode].iParent = splitNode;
	pData->arrNodes[splitNode].iParent = parentNode;
	newPane = pData->arrNodes[newPaneNode].iPane;
	if ( pNewPane != NULL ) *pNewPane = newPane;
	ret = __xuiDockPaneAddWindow(pWidget, pData, &pData->arrPanes[newPane], iWindow);
	if ( ret == XUI_OK ) {
		pWin = __xuiDockWindowAt(pData, iWindow);
		if ( pWin != NULL ) pWin->iLastSide = iSide;
	}
	return ret;
}

static xui_rect_t __xuiDockPreviewRectForSide(xui_rect_t r, int iSide)
{
	float w;
	float h;
	if ( iSide == XUI_DOCK_PANEL_SIDE_FILL || iSide == XUI_DOCK_PANEL_SIDE_NONE ) return __xuiDockInset(r, 4.0f, 4.0f);
	w = __xuiDockClamp(r.fW * 0.28f, 44.0f, __xuiDockMax(44.0f, r.fW - 8.0f));
	h = __xuiDockClamp(r.fH * 0.28f, 36.0f, __xuiDockMax(36.0f, r.fH - 8.0f));
	switch ( iSide ) {
	case XUI_DOCK_PANEL_SIDE_LEFT:
		return __xuiDockRect(r.fX + 4.0f, r.fY + 4.0f, w, __xuiDockMax(0.0f, r.fH - 8.0f));
	case XUI_DOCK_PANEL_SIDE_RIGHT:
		return __xuiDockRect(r.fX + r.fW - w - 4.0f, r.fY + 4.0f, w, __xuiDockMax(0.0f, r.fH - 8.0f));
	case XUI_DOCK_PANEL_SIDE_TOP:
		return __xuiDockRect(r.fX + 4.0f, r.fY + 4.0f, __xuiDockMax(0.0f, r.fW - 8.0f), h);
	case XUI_DOCK_PANEL_SIDE_BOTTOM:
		return __xuiDockRect(r.fX + 4.0f, r.fY + r.fH - h - 4.0f, __xuiDockMax(0.0f, r.fW - 8.0f), h);
	default:
		break;
	}
	return __xuiDockInset(r, 4.0f, 4.0f);
}

static int __xuiDockPaneTabInsertIndex(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, float x, float y)
{
	xui_rect_t last;
	int i;
	int lastIndex;
	if ( (pData == NULL) || (pPane == NULL) || (pPane->iWindowCount <= 1) ) return -1;
	if ( !__xuiDockRectContains(pPane->tTabStripRect, x, y) ) return -1;
	if ( __xuiDockRectContains(pPane->tCloseRect, x, y) || __xuiDockRectContains(pPane->tPinRect, x, y) ||
	     __xuiDockRectContains(pPane->tOptionRect, x, y) || __xuiDockRectContains(pPane->tOverflowRect, x, y) ) {
		return -1;
	}
	last = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	lastIndex = -1;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		xui_rect_t tab;
		if ( w == NULL ) continue;
		tab = w->tTabRect;
		if ( tab.fW <= 0.0f || tab.fH <= 0.0f ) continue;
		if ( x < tab.fX + tab.fW * 0.5f ) return i;
		if ( x < tab.fX + tab.fW ) return i + 1;
		last = tab;
		lastIndex = i;
	}
	if ( lastIndex >= 0 && x >= last.fX + last.fW ) return lastIndex + 1;
	return -1;
}

static xui_rect_t __xuiDockPaneTabInsertRect(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iInsert)
{
	xui_rect_t tab;
	float x;
	int i;
	if ( (pData == NULL) || (pPane == NULL) || (pPane->iWindowCount <= 0) ) return __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	if ( iInsert < 0 ) iInsert = 0;
	if ( iInsert > pPane->iWindowCount ) iInsert = pPane->iWindowCount;
	x = -1.0f;
	if ( iInsert < pPane->iWindowCount ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[iInsert]);
		if ( w != NULL && w->tTabRect.fW > 0.0f && w->tTabRect.fH > 0.0f ) x = w->tTabRect.fX;
	}
	if ( x < 0.0f ) {
		for ( i = pPane->iWindowCount - 1; i >= 0; i-- ) {
			xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
			if ( w == NULL || w->tTabRect.fW <= 0.0f || w->tTabRect.fH <= 0.0f ) continue;
			tab = w->tTabRect;
			x = tab.fX + tab.fW;
			break;
		}
	}
	if ( x < 0.0f ) x = pPane->tTabStripRect.fX + 4.0f;
	return __xuiDockRect(x - 1.0f, pPane->tTabStripRect.fY + 4.0f, 2.0f, __xuiDockMax(0.0f, pPane->tTabStripRect.fH - 8.0f));
}

static int __xuiDockSetTabInsertPreview(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iWindow, int iInsert)
{
	xui_dock_drop_info_t info;
	xui_rect_t r;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDragInsertIndex = -1;
	(void)__xuiDockSetDragIndicator(pWidget, pData, NULL);
	if ( (pPane == NULL) || (iInsert < 0) ) return __xuiDockSetDragPreview(pWidget, pData, NULL);
	r = __xuiDockPaneTabInsertRect(pData, pPane, iInsert);
	if ( r.fW <= 0.0f || r.fH <= 0.0f ) return __xuiDockSetDragPreview(pWidget, pData, NULL);
	__xuiDockDropInfoReset(&info);
	info.bValid = 1;
	info.iWindow = iWindow;
	info.iPane = pPane->iPane;
	info.iRegion = pPane->iRegion;
	info.iSide = XUI_DOCK_PANEL_SIDE_FILL;
	info.tRect = r;
	pData->iDragInsertIndex = iInsert;
	return __xuiDockSetDragPreview(pWidget, pData, &info);
}

static float __xuiDockEdgeSize(float fSize)
{
	if ( fSize < 120.0f ) return 24.0f;
	if ( fSize > 360.0f ) return 90.0f;
	return fSize * 0.25f;
}

static xui_rect_t __xuiDockIndicatorRect(xui_rect_t tBase, float fW, float fH)
{
	return __xuiDockRect(tBase.fX + (tBase.fW - fW) * 0.5f, tBase.fY + (tBase.fH - fH) * 0.5f, fW, fH);
}

static xui_rect_t __xuiDockPanelIndicatorRect(xui_rect_t tBase, int iRegion)
{
	switch ( iRegion ) {
	case XUI_DOCK_PANEL_REGION_LEFT:
		return __xuiDockRect(tBase.fX + 18.0f, tBase.fY + (tBase.fH - 29.0f) * 0.5f, 31.0f, 29.0f);
	case XUI_DOCK_PANEL_REGION_RIGHT:
		return __xuiDockRect(tBase.fX + tBase.fW - 49.0f, tBase.fY + (tBase.fH - 29.0f) * 0.5f, 31.0f, 29.0f);
	case XUI_DOCK_PANEL_REGION_TOP:
		return __xuiDockRect(tBase.fX + (tBase.fW - 29.0f) * 0.5f, tBase.fY + 18.0f, 29.0f, 32.0f);
	case XUI_DOCK_PANEL_REGION_BOTTOM:
		return __xuiDockRect(tBase.fX + (tBase.fW - 29.0f) * 0.5f, tBase.fY + tBase.fH - 49.0f, 29.0f, 31.0f);
	default:
		break;
	}
	return __xuiDockRect(tBase.fX + (tBase.fW - 31.0f) * 0.5f, tBase.fY + (tBase.fH - 31.0f) * 0.5f, 31.0f, 31.0f);
}

static int __xuiDockPaneIndicatorHitSide(xui_rect_t tIndicator, float x, float y)
{
	xui_rect_t tCenter;
	xui_rect_t tPart;
	float fCell;
	float fGap;
	if ( !__xuiDockRectContains(tIndicator, x, y) ) return XUI_DOCK_PANEL_SIDE_NONE;
	fCell = __xuiDockMin(tIndicator.fW, tIndicator.fH);
	fCell = __xuiDockClamp(fCell * 0.25f, 16.0f, 24.0f);
	fGap = 2.0f;
	tCenter = __xuiDockRect(tIndicator.fX + (tIndicator.fW - fCell) * 0.5f, tIndicator.fY + (tIndicator.fH - fCell) * 0.5f, fCell, fCell);
	if ( __xuiDockRectContains(tCenter, x, y) ) return XUI_DOCK_PANEL_SIDE_FILL;
	tPart = __xuiDockRect(tCenter.fX - fCell - fGap, tCenter.fY, fCell, fCell);
	if ( __xuiDockRectContains(tPart, x, y) ) return XUI_DOCK_PANEL_SIDE_LEFT;
	tPart = __xuiDockRect(tCenter.fX + fCell + fGap, tCenter.fY, fCell, fCell);
	if ( __xuiDockRectContains(tPart, x, y) ) return XUI_DOCK_PANEL_SIDE_RIGHT;
	tPart = __xuiDockRect(tCenter.fX, tCenter.fY - fCell - fGap, fCell, fCell);
	if ( __xuiDockRectContains(tPart, x, y) ) return XUI_DOCK_PANEL_SIDE_TOP;
	tPart = __xuiDockRect(tCenter.fX, tCenter.fY + fCell + fGap, fCell, fCell);
	if ( __xuiDockRectContains(tPart, x, y) ) return XUI_DOCK_PANEL_SIDE_BOTTOM;
	return XUI_DOCK_PANEL_SIDE_NONE;
}

static int __xuiDockPaneSplitSideFromPane(const xui_dock_pane_slot_t* pPane, float x, float y)
{
	xui_rect_t tHit;
	float fEdgeX;
	float fEdgeY;
	if ( (pPane == NULL) || !__xuiDockRectContains(pPane->tRect, x, y) ) return XUI_DOCK_PANEL_SIDE_NONE;
	tHit = pPane->tRect;
	if ( pPane->tClientRect.fW > 0.0f && pPane->tClientRect.fH > 0.0f ) {
		tHit = pPane->tClientRect;
		tHit.fY = pPane->tCaptionRect.fY;
		tHit.fH = (pPane->tClientRect.fY + pPane->tClientRect.fH) - tHit.fY;
	}
	fEdgeX = __xuiDockClamp(__xuiDockEdgeSize(tHit.fW), 18.0f, 48.0f);
	fEdgeY = __xuiDockClamp(__xuiDockEdgeSize(tHit.fH), 18.0f, 48.0f);
	if ( __xuiDockRectContains(pPane->tCaptionRect, x, y) || y < tHit.fY + fEdgeY ) return XUI_DOCK_PANEL_SIDE_TOP;
	if ( y >= tHit.fY + tHit.fH - fEdgeY ) return XUI_DOCK_PANEL_SIDE_BOTTOM;
	if ( x < tHit.fX + fEdgeX ) return XUI_DOCK_PANEL_SIDE_LEFT;
	if ( x >= tHit.fX + tHit.fW - fEdgeX ) return XUI_DOCK_PANEL_SIDE_RIGHT;
	return XUI_DOCK_PANEL_SIDE_NONE;
}

static int __xuiDockGlobalSideFromRect(xui_rect_t tRect, float x, float y)
{
	float fEdgeX;
	float fEdgeY;
	if ( !__xuiDockRectContains(tRect, x, y) ) return XUI_DOCK_PANEL_SIDE_NONE;
	fEdgeX = __xuiDockEdgeSize(tRect.fW);
	fEdgeY = __xuiDockEdgeSize(tRect.fH);
	if ( x < tRect.fX + fEdgeX ) return XUI_DOCK_PANEL_SIDE_LEFT;
	if ( x >= tRect.fX + tRect.fW - fEdgeX ) return XUI_DOCK_PANEL_SIDE_RIGHT;
	if ( y < tRect.fY + fEdgeY ) return XUI_DOCK_PANEL_SIDE_TOP;
	if ( y >= tRect.fY + tRect.fH - fEdgeY ) return XUI_DOCK_PANEL_SIDE_BOTTOM;
	return XUI_DOCK_PANEL_SIDE_NONE;
}

static int __xuiDockFindDropTargetEx(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow, float x, float y, xui_dock_drop_info_t* pInfo, xui_dock_drop_info_t* pIndicator)
{
	xui_dock_window_slot_t* pWin;
	xui_dock_pane_slot_t* pHitPane;
	xui_rect_t rPanel;
	xui_rect_t rIndicator;
	int side;
	int indicatorSide;
	int i;
	(void)pWidget;
	if ( pInfo != NULL ) __xuiDockDropInfoReset(pInfo);
	if ( pIndicator != NULL ) __xuiDockDropInfoReset(pIndicator);
	if ( (pData == NULL) || (pInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( (pWin == NULL) || !pWin->bDockable ) return XUI_OK;
	rPanel = xuiWidgetGetContentRect(pWin->pPanelWidget);
	rPanel.fX = 0.0f;
	rPanel.fY = 0.0f;
	if ( !__xuiDockRectContains(rPanel, x, y) ) return XUI_OK;
	pHitPane = NULL;
	for ( i = 0; i < XUI_DOCK_PANEL_PANE_CAPACITY; i++ ) {
		xui_dock_pane_slot_t* pPane = &pData->arrPanes[i];
		if ( !pPane->bUsed || !__xuiDockRectContains(pPane->tRect, x, y) ) continue;
		pHitPane = pPane;
		break;
	}
	if ( pHitPane != NULL ) {
		rIndicator = __xuiDockIndicatorRect(pHitPane->tRect, 88.0f, 88.0f);
		indicatorSide = __xuiDockPaneIndicatorHitSide(rIndicator, x, y);
		if ( pIndicator != NULL ) {
			pIndicator->bValid = 1;
			pIndicator->iWindow = iWindow;
			pIndicator->iRegion = pHitPane->iRegion;
			pIndicator->iPane = pHitPane->iPane;
			pIndicator->iSide = indicatorSide;
			pIndicator->tRect = pHitPane->tRect;
		}
		side = indicatorSide;
		if ( side == XUI_DOCK_PANEL_SIDE_NONE ) side = __xuiDockPaneSplitSideFromPane(pHitPane, x, y);
		if ( side == XUI_DOCK_PANEL_SIDE_NONE ) return XUI_OK;
		pInfo->bValid = 1;
		pInfo->iWindow = iWindow;
		pInfo->iRegion = pHitPane->iRegion;
		pInfo->iPane = pHitPane->iPane;
		pInfo->iSide = side;
		pInfo->tRect = __xuiDockPreviewRectForSide(pHitPane->tRect, side);
		return XUI_OK;
	}
	side = __xuiDockGlobalSideFromRect(rPanel, x, y);
	pInfo->bValid = 1;
	pInfo->iWindow = iWindow;
	pInfo->iPane = -1;
	pInfo->iSide = XUI_DOCK_PANEL_SIDE_FILL;
	if ( side == XUI_DOCK_PANEL_SIDE_LEFT ) {
		pInfo->iRegion = XUI_DOCK_PANEL_REGION_LEFT;
		pInfo->tRect = __xuiDockPreviewRectForSide(rPanel, XUI_DOCK_PANEL_SIDE_LEFT);
		if ( pIndicator != NULL ) *pIndicator = *pInfo;
		return XUI_OK;
	}
	if ( side == XUI_DOCK_PANEL_SIDE_RIGHT ) {
		pInfo->iRegion = XUI_DOCK_PANEL_REGION_RIGHT;
		pInfo->tRect = __xuiDockPreviewRectForSide(rPanel, XUI_DOCK_PANEL_SIDE_RIGHT);
		if ( pIndicator != NULL ) *pIndicator = *pInfo;
		return XUI_OK;
	}
	if ( side == XUI_DOCK_PANEL_SIDE_TOP ) {
		pInfo->iRegion = XUI_DOCK_PANEL_REGION_TOP;
		pInfo->tRect = __xuiDockPreviewRectForSide(rPanel, XUI_DOCK_PANEL_SIDE_TOP);
		if ( pIndicator != NULL ) *pIndicator = *pInfo;
		return XUI_OK;
	}
	if ( side == XUI_DOCK_PANEL_SIDE_BOTTOM ) {
		pInfo->iRegion = XUI_DOCK_PANEL_REGION_BOTTOM;
		pInfo->tRect = __xuiDockPreviewRectForSide(rPanel, XUI_DOCK_PANEL_SIDE_BOTTOM);
		if ( pIndicator != NULL ) *pIndicator = *pInfo;
		return XUI_OK;
	}
	if ( !__xuiDockRectContains(__xuiDockPanelIndicatorRect(rPanel, XUI_DOCK_PANEL_REGION_DOCUMENT), x, y) ) {
		__xuiDockDropInfoReset(pInfo);
		return XUI_OK;
	}
	pInfo->iRegion = XUI_DOCK_PANEL_REGION_DOCUMENT;
	pInfo->tRect = __xuiDockPreviewRectForSide(pData->arrRegions[XUI_DOCK_PANEL_REGION_DOCUMENT].tRect, XUI_DOCK_PANEL_SIDE_FILL);
	if ( pIndicator != NULL ) *pIndicator = *pInfo;
	return XUI_OK;
}

static int __xuiDockFindDropTarget(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow, float x, float y, xui_dock_drop_info_t* pInfo)
{
	return __xuiDockFindDropTargetEx(pWidget, pData, iWindow, x, y, pInfo, NULL);
}

static int __xuiDockCommitDropPreview(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	xui_dock_drop_info_t info;
	int pane;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	info = pData->tDragPreview;
	(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
	if ( !info.bValid || info.iWindow < 0 ) return XUI_OK;
	pane = -1;
	if ( info.iPane >= 0 ) {
		return __xuiDockDockWindowToPaneSide(pWidget, pData, info.iWindow, info.iPane, info.iSide, 0.25f, &pane);
	}
	if ( __xuiDockRegionValid(info.iRegion) ) {
		return __xuiDockDockWindowToRegion(pWidget, pData, info.iWindow, info.iRegion, XUI_DOCK_PANEL_SIDE_FILL, 0.25f, &pane);
	}
	return XUI_OK;
}

typedef struct xui_dock_state_window_t {
	int bInState;
	int bInTree;
	int bInFloating;
	int iState;
	int iLastRegion;
	int iAutoHideRegion;
	int iLastSide;
	int iLastTabIndex;
	int bClosable;
	int bDockable;
	xui_rect_t tFloatRect;
	xui_rect_t tLastDockRect;
} xui_dock_state_window_t;

static const char* __xuiDockStateRegionName(int iRegion)
{
	switch ( iRegion ) {
	case XUI_DOCK_PANEL_REGION_DOCUMENT: return "document";
	case XUI_DOCK_PANEL_REGION_LEFT: return "left";
	case XUI_DOCK_PANEL_REGION_RIGHT: return "right";
	case XUI_DOCK_PANEL_REGION_TOP: return "top";
	case XUI_DOCK_PANEL_REGION_BOTTOM: return "bottom";
	default: return "unknown";
	}
}

static const char* __xuiDockStateSideName(int iSide)
{
	switch ( iSide ) {
	case XUI_DOCK_PANEL_SIDE_NONE: return "none";
	case XUI_DOCK_PANEL_SIDE_LEFT: return "left";
	case XUI_DOCK_PANEL_SIDE_RIGHT: return "right";
	case XUI_DOCK_PANEL_SIDE_TOP: return "top";
	case XUI_DOCK_PANEL_SIDE_BOTTOM: return "bottom";
	case XUI_DOCK_PANEL_SIDE_FILL: return "fill";
	default: return "unknown";
	}
}

static const char* __xuiDockStateWindowStateName(int iState)
{
	switch ( iState ) {
	case XUI_DOCK_PANEL_WINDOW_DOCKED: return "docked";
	case XUI_DOCK_PANEL_WINDOW_FLOATING: return "floating";
	case XUI_DOCK_PANEL_WINDOW_AUTO_HIDE: return "autoHide";
	case XUI_DOCK_PANEL_WINDOW_HIDDEN:
	default:
		return "hidden";
	}
}

static const char* __xuiDockStateAxisName(int iOrientation)
{
	return (iOrientation == XUI_DOCK_ORIENTATION_HORIZONTAL) ? "horizontal" : "vertical";
}

static int __xuiDockStateSetText(xvalue pTable, const char* sKey, const char* sValue)
{
	if ( (pTable == NULL) || (sKey == NULL) || (sValue == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xvoTableSetText(pTable, sKey, 0, (ptr)(void*)sValue, 0, FALSE) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiDockStateSetObject(xvalue pTable, const char* sKey, xvalue pChild)
{
	if ( (pTable == NULL) || (sKey == NULL) || (pChild == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xvoTableSetValue(pTable, sKey, 0, pChild, TRUE) ) {
		xvoUnref(pChild);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static xvalue __xuiDockStateCreateRect(xui_rect_t r)
{
	xvalue pRect = xvoCreateArray();
	if ( pRect == NULL ) return NULL;
	if ( !xvoArrayAppendFloat(pRect, r.fX) ||
	     !xvoArrayAppendFloat(pRect, r.fY) ||
	     !xvoArrayAppendFloat(pRect, r.fW) ||
	     !xvoArrayAppendFloat(pRect, r.fH) ) {
		xvoUnref(pRect);
		return NULL;
	}
	return pRect;
}

static int __xuiDockStateSetRect(xvalue pTable, const char* sKey, xui_rect_t r)
{
	xvalue pRect = __xuiDockStateCreateRect(r);
	if ( pRect == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	return __xuiDockStateSetObject(pTable, sKey, pRect);
}

static xvalue __xuiDockStateCreateNode(xui_dock_panel_data_t* pData, int iNode)
{
	xui_dock_node_slot_t* pNode;
	xui_dock_pane_slot_t* pPane;
	xvalue pState;
	xvalue pChild;
	xvalue pTabs;
	int i;
	if ( iNode < 0 ) return xvoCreateNull();
	pNode = __xuiDockNodeAt(pData, iNode);
	if ( pNode == NULL ) return xvoCreateNull();
	pState = xvoCreateTable();
	if ( pState == NULL ) return NULL;
	if ( pNode->iType == XUI_DOCK_NODE_SPLIT ) {
		if ( (__xuiDockStateSetText(pState, "type", "split") != XUI_OK) ||
		     (__xuiDockStateSetText(pState, "axis", __xuiDockStateAxisName(pNode->iOrientation)) != XUI_OK) ||
		     !xvoTableSetFloat(pState, "ratio", 0, pNode->fRatio) ) {
			xvoUnref(pState);
			return NULL;
		}
		pChild = __xuiDockStateCreateNode(pData, pNode->iFirst);
		if ( (pChild == NULL) || (__xuiDockStateSetObject(pState, "first", pChild) != XUI_OK) ) {
			xvoUnref(pState);
			return NULL;
		}
		pChild = __xuiDockStateCreateNode(pData, pNode->iSecond);
		if ( (pChild == NULL) || (__xuiDockStateSetObject(pState, "second", pChild) != XUI_OK) ) {
			xvoUnref(pState);
			return NULL;
		}
		return pState;
	}
	if ( pNode->iType != XUI_DOCK_NODE_PANE ) {
		xvoUnref(pState);
		return xvoCreateNull();
	}
	if ( __xuiDockStateSetText(pState, "type", "pane") != XUI_OK ) {
		xvoUnref(pState);
		return NULL;
	}
	pPane = __xuiDockPaneAt(pData, pNode->iPane);
	if ( (pPane != NULL) && !xvoTableSetInt(pState, "active", 0, pPane->iActiveIndex) ) {
		xvoUnref(pState);
		return NULL;
	}
	pTabs = xvoCreateArray();
	if ( pTabs == NULL ) {
		xvoUnref(pState);
		return NULL;
	}
	if ( pPane != NULL ) {
		for ( i = 0; i < pPane->iWindowCount; i++ ) {
			if ( !xvoArrayAppendInt(pTabs, pPane->arrWindows[i]) ) {
				xvoUnref(pTabs);
				xvoUnref(pState);
				return NULL;
			}
		}
	}
	if ( __xuiDockStateSetObject(pState, "tabs", pTabs) != XUI_OK ) {
		xvoUnref(pState);
		return NULL;
	}
	return pState;
}

static int __xuiDockStateAppendRegion(xui_dock_panel_data_t* pData, xvalue pRegions, int iRegion)
{
	xui_dock_region_slot_t* pRegion;
	xvalue pItem;
	xvalue pRoot;
	int ret;
	if ( (pData == NULL) || (pRegions == NULL) || !__xuiDockRegionValid(iRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
	pRegion = &pData->arrRegions[iRegion];
	pItem = xvoCreateTable();
	if ( pItem == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	ret = __xuiDockStateSetText(pItem, "kind", __xuiDockStateRegionName(iRegion));
	if ( ret == XUI_OK && !xvoTableSetFloat(pItem, "value", 0, pRegion->fValue) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "sizeMode", (pRegion->iSizeMode == XUI_DOCK_PANEL_SIZE_PIXEL) ? "pixel" : "portion");
	if ( ret == XUI_OK && !xvoTableSetFloat(pItem, (pRegion->iSizeMode == XUI_DOCK_PANEL_SIZE_PIXEL) ? "pixelSize" : "portion", 0, pRegion->fValue) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK && !xvoTableSetBool(pItem, "visible", 0, pRegion->bVisible ? TRUE : FALSE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK ) {
		pRoot = __xuiDockStateCreateNode(pData, pRegion->iRootNode);
		if ( pRoot == NULL ) {
			ret = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			ret = __xuiDockStateSetObject(pItem, "root", pRoot);
		}
	}
	if ( ret == XUI_OK && !xvoArrayAppendValue(pRegions, pItem, TRUE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret != XUI_OK ) xvoUnref(pItem);
	return ret;
}

static int __xuiDockStateAppendWindow(xvalue pWindows, const xui_dock_window_slot_t* pWin)
{
	xvalue pItem;
	int ret;
	if ( (pWindows == NULL) || (pWin == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = xvoCreateTable();
	if ( pItem == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	ret = XUI_OK;
	if ( !xvoTableSetInt(pItem, "id", 0, pWin->iWindow) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "title", pWin->sTitle);
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "state", __xuiDockStateWindowStateName(pWin->iState));
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "lastRegion", __xuiDockStateRegionName(pWin->iLastRegion));
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "autoHideRegion", __xuiDockStateRegionName(__xuiDockWindowAutoHideRegion(pWin)));
	if ( ret == XUI_OK ) ret = __xuiDockStateSetText(pItem, "lastSide", __xuiDockStateSideName(pWin->iLastSide));
	if ( ret == XUI_OK && !xvoTableSetInt(pItem, "lastTabIndex", 0, pWin->iLastTabIndex) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK && !xvoTableSetBool(pItem, "closable", 0, pWin->bClosable ? TRUE : FALSE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK && !xvoTableSetBool(pItem, "dockable", 0, pWin->bDockable ? TRUE : FALSE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK ) ret = __xuiDockStateSetRect(pItem, "floatRect", pWin->tFloatRect);
	if ( ret == XUI_OK ) ret = __xuiDockStateSetRect(pItem, "lastDockRect", pWin->tLastDockRect);
	if ( ret == XUI_OK && !xvoArrayAppendValue(pWindows, pItem, TRUE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret != XUI_OK ) xvoUnref(pItem);
	return ret;
}

static int __xuiDockStateAppendFloating(xvalue pFloating, const xui_dock_window_slot_t* pWin, int iZ)
{
	xvalue pItem;
	int ret;
	if ( (pFloating == NULL) || (pWin == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = xvoCreateTable();
	if ( pItem == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	ret = XUI_OK;
	if ( !xvoTableSetInt(pItem, "id", 0, pWin->iWindow) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK && !xvoTableSetInt(pItem, "z", 0, iZ) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret == XUI_OK ) ret = __xuiDockStateSetRect(pItem, "rect", pWin->tFloatRect);
	if ( ret == XUI_OK && !xvoArrayAppendValue(pFloating, pItem, TRUE) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	if ( ret != XUI_OK ) xvoUnref(pItem);
	return ret;
}

static xvalue __xuiDockStateGet(xvalue pTable, const char* sKey)
{
	if ( (pTable == NULL) || (pTable->Type != XVO_DT_TABLE) || (sKey == NULL) ) return NULL;
	return xvoTableGetValue(pTable, sKey, 0);
}

static const char* __xuiDockStateGetText(xvalue pValue)
{
	return (pValue != NULL && pValue->Type == XVO_DT_TEXT) ? (const char*)xvoGetText(pValue) : NULL;
}

static int __xuiDockStateValueToFloat(xvalue pValue, float* pOut)
{
	if ( pOut == NULL || pValue == NULL ) return 0;
	if ( pValue->Type == XVO_DT_INT ) {
		*pOut = (float)xvoGetInt(pValue);
		return 1;
	}
	if ( pValue->Type == XVO_DT_FLOAT ) {
		*pOut = (float)xvoGetFloat(pValue);
		return 1;
	}
	return 0;
}

static int __xuiDockStateValueToInt(xvalue pValue, int* pOut)
{
	if ( pOut == NULL || pValue == NULL ) return 0;
	if ( pValue->Type == XVO_DT_INT ) {
		*pOut = (int)xvoGetInt(pValue);
		return 1;
	}
	if ( pValue->Type == XVO_DT_FLOAT ) {
		*pOut = (int)xvoGetFloat(pValue);
		return 1;
	}
	return 0;
}

static int __xuiDockStateValueToBool(xvalue pValue, int* pOut)
{
	if ( pOut == NULL || pValue == NULL ) return 0;
	if ( pValue->Type == XVO_DT_BOOL ) {
		*pOut = xvoGetBool(pValue) ? 1 : 0;
		return 1;
	}
	if ( pValue->Type == XVO_DT_INT ) {
		*pOut = (xvoGetInt(pValue) != 0) ? 1 : 0;
		return 1;
	}
	return 0;
}

static int __xuiDockStateReadRect(xvalue pValue, xui_rect_t* pRect)
{
	if ( (pRect == NULL) || (pValue == NULL) || (pValue->Type != XVO_DT_ARRAY) || (xvoArrayItemCount(pValue) != 4u) ) return 0;
	return __xuiDockStateValueToFloat(xvoArrayGetValue(pValue, 0), &pRect->fX) &&
	       __xuiDockStateValueToFloat(xvoArrayGetValue(pValue, 1), &pRect->fY) &&
	       __xuiDockStateValueToFloat(xvoArrayGetValue(pValue, 2), &pRect->fW) &&
	       __xuiDockStateValueToFloat(xvoArrayGetValue(pValue, 3), &pRect->fH);
}

static int __xuiDockStateRegionValue(const char* sText, int* pRegion)
{
	if ( (sText == NULL) || (pRegion == NULL) ) return 0;
	if ( strcmp(sText, "document") == 0 || strcmp(sText, "doc") == 0 || strcmp(sText, "center") == 0 || strcmp(sText, "fill") == 0 ) { *pRegion = XUI_DOCK_PANEL_REGION_DOCUMENT; return 1; }
	if ( strcmp(sText, "left") == 0 ) { *pRegion = XUI_DOCK_PANEL_REGION_LEFT; return 1; }
	if ( strcmp(sText, "right") == 0 ) { *pRegion = XUI_DOCK_PANEL_REGION_RIGHT; return 1; }
	if ( strcmp(sText, "top") == 0 ) { *pRegion = XUI_DOCK_PANEL_REGION_TOP; return 1; }
	if ( strcmp(sText, "bottom") == 0 ) { *pRegion = XUI_DOCK_PANEL_REGION_BOTTOM; return 1; }
	return 0;
}

static int __xuiDockStateSideValue(const char* sText, int* pSide)
{
	if ( (sText == NULL) || (pSide == NULL) ) return 0;
	if ( strcmp(sText, "none") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_NONE; return 1; }
	if ( strcmp(sText, "left") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_LEFT; return 1; }
	if ( strcmp(sText, "right") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_RIGHT; return 1; }
	if ( strcmp(sText, "top") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_TOP; return 1; }
	if ( strcmp(sText, "bottom") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_BOTTOM; return 1; }
	if ( strcmp(sText, "fill") == 0 || strcmp(sText, "center") == 0 || strcmp(sText, "tab") == 0 ) { *pSide = XUI_DOCK_PANEL_SIDE_FILL; return 1; }
	return 0;
}

static int __xuiDockStateWindowStateValue(const char* sText, int* pState)
{
	if ( (sText == NULL) || (pState == NULL) ) return 0;
	if ( strcmp(sText, "docked") == 0 || strcmp(sText, "dock") == 0 ) { *pState = XUI_DOCK_PANEL_WINDOW_DOCKED; return 1; }
	if ( strcmp(sText, "floating") == 0 || strcmp(sText, "float") == 0 ) { *pState = XUI_DOCK_PANEL_WINDOW_FLOATING; return 1; }
	if ( strcmp(sText, "autoHide") == 0 || strcmp(sText, "auto-hide") == 0 || strcmp(sText, "autohide") == 0 ) { *pState = XUI_DOCK_PANEL_WINDOW_AUTO_HIDE; return 1; }
	if ( strcmp(sText, "hidden") == 0 || strcmp(sText, "hide") == 0 ) { *pState = XUI_DOCK_PANEL_WINDOW_HIDDEN; return 1; }
	return 0;
}

static int __xuiDockStateAxisValue(const char* sText, int* pOrientation)
{
	if ( (sText == NULL) || (pOrientation == NULL) ) return 0;
	if ( strcmp(sText, "horizontal") == 0 ) { *pOrientation = XUI_DOCK_ORIENTATION_HORIZONTAL; return 1; }
	if ( strcmp(sText, "vertical") == 0 ) { *pOrientation = XUI_DOCK_ORIENTATION_VERTICAL; return 1; }
	return 0;
}

static int __xuiDockStateFindWindow(xui_dock_panel_data_t* pData, xvalue pValue, int* pWindow)
{
	const char* sText;
	int id;
	int i;
	if ( (pData == NULL) || (pValue == NULL) || (pWindow == NULL) ) return 0;
	if ( __xuiDockStateValueToInt(pValue, &id) ) {
		if ( __xuiDockWindowAt(pData, id) == NULL ) return 0;
		*pWindow = id;
		return 1;
	}
	sText = __xuiDockStateGetText(pValue);
	if ( sText == NULL ) return 0;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed && strcmp(pData->arrWindows[i].sTitle, sText) == 0 ) {
			*pWindow = i;
			return 1;
		}
	}
	return 0;
}

static int __xuiDockLoadPrepareWindows(xui_dock_panel_data_t* pData, xvalue pState, xui_dock_state_window_t* arrInfo)
{
	xvalue pWindows;
	uint32_t i;
	int w;
	if ( (pData == NULL) || (pState == NULL) || (arrInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( w = 0; w < XUI_DOCK_PANEL_WINDOW_CAPACITY; w++ ) {
		arrInfo[w].iState = XUI_DOCK_PANEL_WINDOW_HIDDEN;
		arrInfo[w].iLastRegion = pData->arrWindows[w].bUsed ? pData->arrWindows[w].iLastRegion : XUI_DOCK_PANEL_REGION_DOCUMENT;
		arrInfo[w].iAutoHideRegion = pData->arrWindows[w].bUsed ? pData->arrWindows[w].iAutoHideRegion : XUI_DOCK_PANEL_REGION_LEFT;
		arrInfo[w].iLastSide = pData->arrWindows[w].bUsed ? pData->arrWindows[w].iLastSide : XUI_DOCK_PANEL_SIDE_FILL;
		arrInfo[w].iLastTabIndex = pData->arrWindows[w].bUsed ? pData->arrWindows[w].iLastTabIndex : -1;
		arrInfo[w].bClosable = pData->arrWindows[w].bUsed ? pData->arrWindows[w].bClosable : 1;
		arrInfo[w].bDockable = pData->arrWindows[w].bUsed ? pData->arrWindows[w].bDockable : 1;
		arrInfo[w].tFloatRect = pData->arrWindows[w].bUsed ? pData->arrWindows[w].tFloatRect : __xuiDockRect(80.0f, 70.0f, 300.0f, 220.0f);
		arrInfo[w].tLastDockRect = pData->arrWindows[w].bUsed ? pData->arrWindows[w].tLastDockRect : __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
	pWindows = __xuiDockStateGet(pState, "windows");
	if ( (pWindows == NULL) || (pWindows->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0u; i < xvoArrayItemCount(pWindows); i++ ) {
		xvalue pItem = xvoArrayGetValue(pWindows, i);
		xvalue pValue;
		const char* sText;
		if ( (pItem == NULL) || (pItem->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !__xuiDockStateFindWindow(pData, __xuiDockStateGet(pItem, "id"), &w) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( arrInfo[w].bInState ) return XUI_ERROR_INVALID_ARGUMENT;
		arrInfo[w].bInState = 1;
		pValue = __xuiDockStateGet(pItem, "state");
		sText = __xuiDockStateGetText(pValue);
		if ( sText != NULL && !__xuiDockStateWindowStateValue(sText, &arrInfo[w].iState) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "lastRegion");
		sText = __xuiDockStateGetText(pValue);
		if ( sText != NULL && !__xuiDockStateRegionValue(sText, &arrInfo[w].iLastRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "autoHideRegion");
		sText = __xuiDockStateGetText(pValue);
		if ( sText != NULL && !__xuiDockStateRegionValue(sText, &arrInfo[w].iAutoHideRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "lastSide");
		sText = __xuiDockStateGetText(pValue);
		if ( sText != NULL && !__xuiDockStateSideValue(sText, &arrInfo[w].iLastSide) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "lastTabIndex");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToInt(pValue, &arrInfo[w].iLastTabIndex) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "closable");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToBool(pValue, &arrInfo[w].bClosable) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "dockable");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToBool(pValue, &arrInfo[w].bDockable) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "floatRect");
		if ( pValue == NULL ) pValue = __xuiDockStateGet(pItem, "lastFloatRect");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateReadRect(pValue, &arrInfo[w].tFloatRect) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pItem, "lastDockRect");
		if ( pValue == NULL ) pValue = __xuiDockStateGet(pItem, "dockRect");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateReadRect(pValue, &arrInfo[w].tLastDockRect) ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

static int __xuiDockLoadValidateNode(xui_dock_panel_data_t* pData, xvalue pNodeState, xui_dock_state_window_t* arrInfo, int* pNodeCount, int* pPaneCount)
{
	const char* sType;
	xvalue pValue;
	xvalue pTabs;
	uint32_t i;
	int w;
	(void)pData;
	if ( (pNodeState == NULL) || (pNodeState->Type == XVO_DT_NULL) ) return XUI_OK;
	if ( pNodeState->Type != XVO_DT_TABLE ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pNodeCount == NULL) || (pPaneCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(*pNodeCount)++;
	if ( *pNodeCount > XUI_DOCK_PANEL_NODE_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	sType = __xuiDockStateGetText(__xuiDockStateGet(pNodeState, "type"));
	if ( sType == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( strcmp(sType, "split") == 0 ) {
		int orientation = XUI_DOCK_ORIENTATION_VERTICAL;
		int ret;
		xvalue pFirst;
		xvalue pSecond;
		pValue = __xuiDockStateGet(pNodeState, "axis");
		if ( __xuiDockStateGetText(pValue) != NULL && !__xuiDockStateAxisValue(__xuiDockStateGetText(pValue), &orientation) ) return XUI_ERROR_INVALID_ARGUMENT;
		pValue = __xuiDockStateGet(pNodeState, "ratio");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) {
			float ratio;
			if ( !__xuiDockStateValueToFloat(pValue, &ratio) ) return XUI_ERROR_INVALID_ARGUMENT;
		}
		pFirst = __xuiDockStateGet(pNodeState, "first");
		pSecond = __xuiDockStateGet(pNodeState, "second");
		if ( (pFirst == NULL) || (pFirst->Type == XVO_DT_NULL) || (pSecond == NULL) || (pSecond->Type == XVO_DT_NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
		ret = __xuiDockLoadValidateNode(pData, pFirst, arrInfo, pNodeCount, pPaneCount);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockLoadValidateNode(pData, pSecond, arrInfo, pNodeCount, pPaneCount);
		if ( ret != XUI_OK ) return ret;
		return XUI_OK;
	}
	if ( strcmp(sType, "pane") != 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	(*pPaneCount)++;
	if ( *pPaneCount > XUI_DOCK_PANEL_PANE_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	pValue = __xuiDockStateGet(pNodeState, "active");
	if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) {
		int active;
		if ( !__xuiDockStateValueToInt(pValue, &active) ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTabs = __xuiDockStateGet(pNodeState, "tabs");
	if ( (pTabs == NULL) || (pTabs->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0u; i < xvoArrayItemCount(pTabs); i++ ) {
		if ( !__xuiDockStateFindWindow(pData, xvoArrayGetValue(pTabs, i), &w) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !arrInfo[w].bInState || arrInfo[w].iState != XUI_DOCK_PANEL_WINDOW_DOCKED || arrInfo[w].bInTree || arrInfo[w].bInFloating ) return XUI_ERROR_INVALID_ARGUMENT;
		arrInfo[w].bInTree = 1;
	}
	return XUI_OK;
}

static int __xuiDockLoadValidateRegions(xui_dock_panel_data_t* pData, xvalue pState, xui_dock_state_window_t* arrInfo, int* pRegionSeen)
{
	xvalue pRegions;
	uint32_t i;
	int nodeCount;
	int paneCount;
	if ( (pData == NULL) || (pState == NULL) || (arrInfo == NULL) || (pRegionSeen == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pRegions = __xuiDockStateGet(pState, "regions");
	if ( (pRegions == NULL) || (pRegions->Type != XVO_DT_ARRAY) || (xvoArrayItemCount(pRegions) != XUI_DOCK_PANEL_REGION_COUNT) ) return XUI_ERROR_INVALID_ARGUMENT;
	nodeCount = 0;
	paneCount = 0;
	for ( i = 0u; i < xvoArrayItemCount(pRegions); i++ ) {
		xvalue pItem = xvoArrayGetValue(pRegions, i);
		const char* sKind;
		int region = (int)i;
		if ( (pItem == NULL) || (pItem->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
		sKind = __xuiDockStateGetText(__xuiDockStateGet(pItem, "kind"));
		if ( sKind != NULL && !__xuiDockStateRegionValue(sKind, &region) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !__xuiDockRegionValid(region) || pRegionSeen[region] ) return XUI_ERROR_INVALID_ARGUMENT;
		{
			xvalue pValue;
			const char* sText;
			float fValue;
			int bValue;
			sText = __xuiDockStateGetText(__xuiDockStateGet(pItem, "sizeMode"));
			if ( sText != NULL && strcmp(sText, "pixel") != 0 && strcmp(sText, "portion") != 0 ) return XUI_ERROR_INVALID_ARGUMENT;
			pValue = __xuiDockStateGet(pItem, "value");
			if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToFloat(pValue, &fValue) ) return XUI_ERROR_INVALID_ARGUMENT;
			pValue = __xuiDockStateGet(pItem, "pixelSize");
			if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToFloat(pValue, &fValue) ) return XUI_ERROR_INVALID_ARGUMENT;
			pValue = __xuiDockStateGet(pItem, "portion");
			if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToFloat(pValue, &fValue) ) return XUI_ERROR_INVALID_ARGUMENT;
			pValue = __xuiDockStateGet(pItem, "visible");
			if ( pValue != NULL && pValue->Type != XVO_DT_NULL && !__xuiDockStateValueToBool(pValue, &bValue) ) return XUI_ERROR_INVALID_ARGUMENT;
		}
		pRegionSeen[region] = 1;
		{
			int ret = __xuiDockLoadValidateNode(pData, __xuiDockStateGet(pItem, "root"), arrInfo, &nodeCount, &paneCount);
			if ( ret != XUI_OK ) return ret;
		}
	}
	for ( i = 0u; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		if ( !pRegionSeen[i] ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

static int __xuiDockLoadValidateFloating(xui_dock_panel_data_t* pData, xvalue pState, xui_dock_state_window_t* arrInfo)
{
	xvalue pFloating;
	uint32_t i;
	int w;
	if ( (pData == NULL) || (pState == NULL) || (arrInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pFloating = __xuiDockStateGet(pState, "floating");
	if ( (pFloating == NULL) || (pFloating->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0u; i < xvoArrayItemCount(pFloating); i++ ) {
		xvalue pItem = xvoArrayGetValue(pFloating, i);
		xvalue pRect;
		if ( (pItem == NULL) || (pItem->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !__xuiDockStateFindWindow(pData, __xuiDockStateGet(pItem, "id"), &w) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !arrInfo[w].bInState || arrInfo[w].iState != XUI_DOCK_PANEL_WINDOW_FLOATING || arrInfo[w].bInTree || arrInfo[w].bInFloating ) return XUI_ERROR_INVALID_ARGUMENT;
		pRect = __xuiDockStateGet(pItem, "rect");
		if ( pRect != NULL && pRect->Type != XVO_DT_NULL && !__xuiDockStateReadRect(pRect, &arrInfo[w].tFloatRect) ) return XUI_ERROR_INVALID_ARGUMENT;
		arrInfo[w].bInFloating = 1;
	}
	for ( w = 0; w < XUI_DOCK_PANEL_WINDOW_CAPACITY; w++ ) {
		if ( !arrInfo[w].bInState ) continue;
		if ( !arrInfo[w].bInTree && !arrInfo[w].bInFloating &&
		     arrInfo[w].iState != XUI_DOCK_PANEL_WINDOW_HIDDEN &&
		     arrInfo[w].iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	return XUI_OK;
}

static void __xuiDockLoadClearLayout(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	int i;
	(void)xuiMenuClose(pData->pOptionMenu);
	(void)xuiMenuClose(pData->pOverflowMenu);
	(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) pData->arrRegions[i].iRootNode = -1;
	for ( i = 0; i < XUI_DOCK_PANEL_PANE_CAPACITY; i++ ) memset(&pData->arrPanes[i], 0, sizeof(pData->arrPanes[i]));
	for ( i = 0; i < XUI_DOCK_PANEL_NODE_CAPACITY; i++ ) memset(&pData->arrNodes[i], 0, sizeof(pData->arrNodes[i]));
	pData->iPaneCount = 0;
	pData->iNodeCount = 0;
	pData->iDragType = XUI_DOCK_DRAG_NONE;
	pData->iDragNode = -1;
	pData->iDragRegion = -1;
	pData->iDragPane = -1;
	pData->iDragWindow = -1;
	pData->iDragSourceIndex = -1;
	pData->iDragInsertIndex = -1;
	pData->iDragSide = 0;
	pData->bDragFloating = 0;
	pData->iHoverRegion = -1;
	pData->iMenuPane = -1;
	pData->iAutoHideExpandWindow = -1;
	__xuiDockFloatOrderClear(pData);
	(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( !pData->arrWindows[i].bUsed ) continue;
		pData->arrWindows[i].iPane = -1;
		pData->arrWindows[i].tTabRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
		pData->arrWindows[i].tTabCloseRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
		pData->arrWindows[i].tAutoHideRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

static int __xuiDockLoadBuildNode(xui_widget pWidget, xui_dock_panel_data_t* pData, xvalue pNodeState, int iRegion, xui_dock_state_window_t* arrInfo, int* pNode)
{
	const char* sType;
	if ( pNode == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pNode = -1;
	if ( (pNodeState == NULL) || (pNodeState->Type == XVO_DT_NULL) ) return XUI_OK;
	if ( pNodeState->Type != XVO_DT_TABLE ) return XUI_ERROR_INVALID_ARGUMENT;
	sType = __xuiDockStateGetText(__xuiDockStateGet(pNodeState, "type"));
	if ( sType == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( strcmp(sType, "split") == 0 ) {
		xui_dock_node_slot_t* pSplit;
		xvalue pValue;
		float ratio;
		int orientation;
		int first;
		int second;
		int split;
		int ret;
		orientation = XUI_DOCK_ORIENTATION_VERTICAL;
		pValue = __xuiDockStateGet(pNodeState, "axis");
		if ( __xuiDockStateGetText(pValue) != NULL ) (void)__xuiDockStateAxisValue(__xuiDockStateGetText(pValue), &orientation);
		ratio = 0.5f;
		pValue = __xuiDockStateGet(pNodeState, "ratio");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) (void)__xuiDockStateValueToFloat(pValue, &ratio);
		ret = __xuiDockLoadBuildNode(pWidget, pData, __xuiDockStateGet(pNodeState, "first"), iRegion, arrInfo, &first);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockLoadBuildNode(pWidget, pData, __xuiDockStateGet(pNodeState, "second"), iRegion, arrInfo, &second);
		if ( ret != XUI_OK ) return ret;
		if ( first < 0 || second < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
		split = __xuiDockAllocNode(pData, XUI_DOCK_NODE_SPLIT);
		if ( split < 0 ) return XUI_ERROR_OUT_OF_MEMORY;
		pSplit = &pData->arrNodes[split];
		pSplit->iOrientation = orientation;
		pSplit->iFirst = first;
		pSplit->iSecond = second;
		pSplit->fRatio = __xuiDockClamp(ratio, 0.10f, 0.90f);
		pData->arrNodes[first].iParent = split;
		pData->arrNodes[second].iParent = split;
		*pNode = split;
		return XUI_OK;
	}
	if ( strcmp(sType, "pane") == 0 ) {
		xui_dock_pane_slot_t* pPane;
		xvalue pTabs;
		xvalue pValue;
		uint32_t i;
		int node;
		int pane;
		int active;
		node = __xuiDockAllocPaneNode(pData, iRegion);
		if ( node < 0 ) return XUI_ERROR_OUT_OF_MEMORY;
		pane = pData->arrNodes[node].iPane;
		pPane = __xuiDockPaneAt(pData, pane);
		if ( pPane == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		active = -1;
		pValue = __xuiDockStateGet(pNodeState, "active");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) (void)__xuiDockStateValueToInt(pValue, &active);
		pTabs = __xuiDockStateGet(pNodeState, "tabs");
		if ( (pTabs == NULL) || (pTabs->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
		for ( i = 0u; i < xvoArrayItemCount(pTabs); i++ ) {
			xui_dock_window_slot_t* pWin;
			int w;
			if ( !__xuiDockStateFindWindow(pData, xvoArrayGetValue(pTabs, i), &w) ) return XUI_ERROR_INVALID_ARGUMENT;
			if ( pPane->iWindowCount >= XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
			pPane->arrWindows[pPane->iWindowCount++] = w;
			pWin = __xuiDockWindowAt(pData, w);
			if ( pWin != NULL ) {
				pWin->iPane = pane;
				pWin->iRegion = iRegion;
				pWin->iLastRegion = iRegion;
				pWin->iAutoHideRegion = arrInfo[w].iAutoHideRegion;
				pWin->iLastSide = arrInfo[w].iLastSide;
				pWin->iLastTabIndex = (int)i;
				pWin->tLastDockRect = arrInfo[w].tLastDockRect;
				__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_DOCKED);
			}
		}
		if ( pPane->iWindowCount <= 0 ) {
			pPane->iActiveIndex = -1;
		} else {
			pPane->iActiveIndex = (int)__xuiDockClamp((float)((active < 0) ? 0 : active), 0.0f, (float)(pPane->iWindowCount - 1));
		}
		__xuiDockPaneRefreshLastTabIndices(pData, pPane);
		*pNode = node;
		return XUI_OK;
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

static int __xuiDockLoadApplyRegions(xui_widget pWidget, xui_dock_panel_data_t* pData, xvalue pState, xui_dock_state_window_t* arrInfo)
{
	xvalue pRegions = __xuiDockStateGet(pState, "regions");
	uint32_t i;
	int ret;
	if ( (pRegions == NULL) || (pRegions->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0u; i < xvoArrayItemCount(pRegions); i++ ) {
		xvalue pItem = xvoArrayGetValue(pRegions, i);
		xvalue pValue;
		const char* sText;
		float value;
		int region;
		int root;
		int visible;
		int sizeMode;
		if ( (pItem == NULL) || (pItem->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
		region = (int)i;
		sText = __xuiDockStateGetText(__xuiDockStateGet(pItem, "kind"));
		if ( sText != NULL ) (void)__xuiDockStateRegionValue(sText, &region);
		sizeMode = pData->arrRegions[region].iSizeMode;
		sText = __xuiDockStateGetText(__xuiDockStateGet(pItem, "sizeMode"));
		if ( sText != NULL ) sizeMode = (strcmp(sText, "pixel") == 0) ? XUI_DOCK_PANEL_SIZE_PIXEL : XUI_DOCK_PANEL_SIZE_PORTION;
		value = pData->arrRegions[region].fValue;
		pValue = __xuiDockStateGet(pItem, "value");
		if ( pValue == NULL || pValue->Type == XVO_DT_NULL ) pValue = __xuiDockStateGet(pItem, sizeMode == XUI_DOCK_PANEL_SIZE_PIXEL ? "pixelSize" : "portion");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) (void)__xuiDockStateValueToFloat(pValue, &value);
		visible = 1;
		pValue = __xuiDockStateGet(pItem, "visible");
		if ( pValue != NULL && pValue->Type != XVO_DT_NULL ) (void)__xuiDockStateValueToBool(pValue, &visible);
		pData->arrRegions[region].iSizeMode = (region == XUI_DOCK_PANEL_REGION_DOCUMENT) ? XUI_DOCK_PANEL_SIZE_PORTION : sizeMode;
		pData->arrRegions[region].fValue = __xuiDockMax(0.0f, value);
		pData->arrRegions[region].bVisible = visible ? 1 : 0;
		ret = __xuiDockLoadBuildNode(pWidget, pData, __xuiDockStateGet(pItem, "root"), region, arrInfo, &root);
		if ( ret != XUI_OK ) return ret;
		pData->arrRegions[region].iRootNode = root;
	}
	return XUI_OK;
}

static int __xuiDockLoadApplyFloating(xui_widget pWidget, xui_dock_panel_data_t* pData, xvalue pState, xui_dock_state_window_t* arrInfo)
{
	xvalue pFloating = __xuiDockStateGet(pState, "floating");
	uint32_t i;
	if ( (pFloating == NULL) || (pFloating->Type != XVO_DT_ARRAY) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0u; i < xvoArrayItemCount(pFloating); i++ ) {
		xvalue pItem = xvoArrayGetValue(pFloating, i);
		xui_dock_window_slot_t* pWin;
		int w;
		if ( (pItem == NULL) || (pItem->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( !__xuiDockStateFindWindow(pData, __xuiDockStateGet(pItem, "id"), &w) ) return XUI_ERROR_INVALID_ARGUMENT;
		pWin = __xuiDockWindowAt(pData, w);
		if ( pWin == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		pWin->iPane = -1;
		pWin->tFloatRect = __xuiDockClampFloatRect(pWidget, arrInfo[w].tFloatRect);
		pWin->iAutoHideRegion = arrInfo[w].iAutoHideRegion;
		pWin->iLastTabIndex = arrInfo[w].iLastTabIndex;
		pWin->tLastDockRect = arrInfo[w].tLastDockRect;
		__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_FLOATING);
	}
	return XUI_OK;
}

static xui_rect_t __xuiDockHostClientLocal(xui_dock_window_slot_t* pWin, const xui_dock_panel_metrics_t* m)
{
	xui_dock_panel_data_t* pData;
	xui_rect_t r;
	if ( pWin->iState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
		r = __xuiDockRect(0.0f, m->fFloatTitleHeight, pWin->tRect.fW, __xuiDockMax(0.0f, pWin->tRect.fH - m->fFloatTitleHeight));
		r = __xuiDockInset(r, m->fFloatBorderWidth, m->fFloatBorderWidth);
		return r;
	}
	pData = pWin->pOwner;
	if ( pData != NULL && pWin->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->iAutoHideExpandWindow == pWin->iWindow ) {
		r = __xuiDockRect(0.0f, m->fCaptionHeight, pWin->tRect.fW, __xuiDockMax(0.0f, pWin->tRect.fH - m->fCaptionHeight));
		r = __xuiDockInset(r, m->fBorderWidth, m->fBorderWidth);
		return r;
	}
	return __xuiDockRect(0.0f, 0.0f, pWin->tRect.fW, pWin->tRect.fH);
}

static void __xuiDockArrangeWindowHost(xui_dock_window_slot_t* pWin, const xui_dock_panel_metrics_t* m, int bVisible)
{
	xui_rect_t local;
	if ( pWin->pHostWidget == NULL ) return;
	__xuiDockSetHostLayer(pWin);
	(void)xuiWidgetSetVisible(pWin->pHostWidget, bVisible);
	if ( !bVisible ) {
		if ( pWin->pClientWidget != NULL ) (void)xuiWidgetSetVisible(pWin->pClientWidget, 0);
		return;
	}
	(void)xuiWidgetSetRect(pWin->pHostWidget, pWin->tRect);
	(void)xuiWidgetArrange(pWin->pHostWidget, pWin->tRect);
	if ( pWin->pClientWidget != NULL ) {
		local = __xuiDockHostClientLocal(pWin, m);
		pWin->tClientRect = local;
		(void)xuiWidgetSetVisible(pWin->pClientWidget, 1);
		(void)xuiWidgetSetRect(pWin->pClientWidget, local);
		(void)xuiWidgetArrange(pWin->pClientWidget, local);
	}
	(void)xuiWidgetInvalidate(pWin->pHostWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiDockLayoutPane(xui_dock_panel_data_t* pData, int iPane, xui_rect_t r)
{
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	xui_dock_panel_metrics_t* m = &pData->tMetrics;
	xui_rect_t header;
	float headerH;
	float button;
	float right;
	int bTabs;
	xui_dock_window_slot_t* active;
	if ( pPane == NULL ) return;
	pPane->tRect = xuiInternalSnapRect(r);
	bTabs = __xuiDockPaneUsesTabs(pPane);
	headerH = 0.0f;
	if ( pPane->iWindowCount > 0 ) {
		headerH = __xuiDockMin(bTabs ? m->fTabStripHeight : m->fCaptionHeight, r.fH);
	}
	header = __xuiDockRect(r.fX + m->fBorderWidth, r.fY + m->fBorderWidth, __xuiDockMax(0.0f, r.fW - m->fBorderWidth * 2.0f), headerH);
	pPane->tCaptionRect = header;
	pPane->tTabStripRect = bTabs ? header : __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pPane->tClientRect = __xuiDockRect(r.fX + m->fBorderWidth, r.fY + headerH + m->fBorderWidth, __xuiDockMax(0.0f, r.fW - m->fBorderWidth * 2.0f), __xuiDockMax(0.0f, r.fH - headerH - m->fBorderWidth * 2.0f));
	pPane->tCloseRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pPane->tPinRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pPane->tOptionRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	if ( headerH > 0.0f ) {
		button = __xuiDockMin(m->fButtonSize, __xuiDockMax(0.0f, headerH - 4.0f));
		right = header.fX + header.fW - 4.0f;
		pPane->tOptionRect = __xuiDockRect(right - button, header.fY + (headerH - button) * 0.5f, button, button);
		right -= button + 2.0f;
		pPane->tPinRect = __xuiDockRect(right - button, header.fY + (headerH - button) * 0.5f, button, button);
		right -= button + 2.0f;
		active = __xuiDockWindowAt(pData, __xuiDockPaneActiveWindow(pPane));
		if ( !bTabs && (active != NULL) && active->bClosable ) {
			pPane->tCloseRect = __xuiDockRect(right - button, header.fY + (headerH - button) * 0.5f, button, button);
		}
	}
}

static void __xuiDockLayoutTabs(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane)
{
	xui_rect_t strip;
	xui_dock_panel_metrics_t* m;
	float tabW;
	float available;
	float x;
	float right;
	float natural;
	int first;
	int visible;
	int active;
	int i;
	int w;
	if ( pPane == NULL ) return;
	m = &pData->tMetrics;
	strip = pPane->tTabStripRect;
	pPane->tOverflowRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pPane->iFirstVisibleTab = 0;
	pPane->iVisibleTabCount = 0;
	pPane->bOverflow = 0;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		w = pPane->arrWindows[i];
		if ( (w >= 0) && (w < XUI_DOCK_PANEL_WINDOW_CAPACITY) ) {
			pData->arrWindows[w].tTabRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
			pData->arrWindows[w].tTabCloseRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	if ( !__xuiDockPaneUsesTabs(pPane) || (strip.fW <= 0.0f) || (strip.fH <= 0.0f) ) return;
	right = strip.fX + strip.fW - 3.0f;
	if ( pPane->tCloseRect.fW > 0.0f && pPane->tCloseRect.fX < right ) right = pPane->tCloseRect.fX - 3.0f;
	if ( pPane->tPinRect.fW > 0.0f && pPane->tPinRect.fX < right ) right = pPane->tPinRect.fX - 3.0f;
	if ( pPane->tOptionRect.fW > 0.0f && pPane->tOptionRect.fX < right ) right = pPane->tOptionRect.fX - 3.0f;
	x = strip.fX + 3.0f;
	available = __xuiDockMax(0.0f, right - x);
	natural = 0.0f;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* win = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		float closeW = (win != NULL && win->bClosable) ? (m->fButtonSize + 6.0f) : 0.0f;
		float nw = (win != NULL) ? (__xuiDockStringWidthGuess(win->sTitle) + m->fTabPaddingX * 2.0f + closeW + 8.0f) : m->fTabMinWidth;
		nw = __xuiDockClamp(nw, m->fTabMinWidth, m->fTabMaxWidth);
		natural += nw;
		if ( i > 0 ) natural -= 1.0f;
	}
	if ( natural > available ) {
		pPane->bOverflow = 1;
	}
	visible = pPane->iWindowCount;
	first = 0;
	active = pPane->iActiveIndex;
	if ( active < 0 || active >= pPane->iWindowCount ) active = 0;
	if ( pPane->bOverflow ) {
		visible = (m->fTabMinWidth > 0.0f) ? (int)(available / m->fTabMinWidth) : pPane->iWindowCount;
		if ( visible < 1 ) visible = 1;
		if ( visible > pPane->iWindowCount ) visible = pPane->iWindowCount;
		if ( active >= visible ) first = active - visible + 1;
		if ( first > pPane->iWindowCount - visible ) first = pPane->iWindowCount - visible;
		if ( first < 0 ) first = 0;
	}
	pPane->iFirstVisibleTab = first;
	pPane->iVisibleTabCount = visible;
	for ( i = 0; i < visible; i++ ) {
		w = pPane->arrWindows[first + i];
		if ( pPane->bOverflow ) {
			tabW = __xuiDockClamp(available / (float)visible + 1.0f, 8.0f, m->fTabMaxWidth);
		} else if ( (w >= 0) && (w < XUI_DOCK_PANEL_WINDOW_CAPACITY) && pData->arrWindows[w].bUsed ) {
			float closeW = pData->arrWindows[w].bClosable ? (m->fButtonSize + 6.0f) : 0.0f;
			tabW = __xuiDockStringWidthGuess(pData->arrWindows[w].sTitle) + m->fTabPaddingX * 2.0f + closeW + 8.0f;
			tabW = __xuiDockClamp(tabW, m->fTabMinWidth, m->fTabMaxWidth);
		} else {
			tabW = m->fTabMinWidth;
		}
		if ( (w >= 0) && (w < XUI_DOCK_PANEL_WINDOW_CAPACITY) && pData->arrWindows[w].bUsed ) {
			xui_dock_window_slot_t* pWin = &pData->arrWindows[w];
			xui_rect_t tab = __xuiDockRect(x, strip.fY + 2.0f, __xuiDockMin(tabW, __xuiDockMax(0.0f, right - x)), __xuiDockMax(0.0f, strip.fH - 2.0f));
			pWin->tTabRect = tab;
			if ( pWin->bClosable && tab.fW >= m->fButtonSize + 16.0f && tab.fH >= 8.0f ) {
				float button = __xuiDockMin(m->fButtonSize, __xuiDockMax(0.0f, tab.fH - 6.0f));
				pWin->tTabCloseRect = __xuiDockRect(tab.fX + tab.fW - button - 5.0f, tab.fY + (tab.fH - button) * 0.5f, button, button);
			}
		}
		x += tabW - 1.0f;
		if ( x >= right ) break;
	}
}

static void __xuiDockLayoutNode(xui_dock_panel_data_t* pData, int iNode, xui_rect_t r)
{
	xui_dock_node_slot_t* pNode = __xuiDockNodeAt(pData, iNode);
	float splitSize;
	float minA;
	float minB;
	float firstSize;
	xui_rect_t a;
	xui_rect_t b;
	if ( pNode == NULL ) return;
	pNode->tRect = xuiInternalSnapRect(r);
	if ( pNode->iType == XUI_DOCK_NODE_PANE ) {
		__xuiDockLayoutPane(pData, pNode->iPane, r);
		__xuiDockLayoutTabs(pData, __xuiDockPaneAt(pData, pNode->iPane));
		return;
	}
	if ( pNode->iType != XUI_DOCK_NODE_SPLIT ) return;
	splitSize = pData->tMetrics.fSplitterSize;
	minA = (pNode->iOrientation == XUI_DOCK_ORIENTATION_VERTICAL) ? pData->tMetrics.fMinPaneWidth : pData->tMetrics.fMinPaneHeight;
	minB = minA;
	if ( pNode->iOrientation == XUI_DOCK_ORIENTATION_VERTICAL ) {
		firstSize = (r.fW - splitSize) * pNode->fRatio;
		firstSize = __xuiDockClamp(firstSize, minA, __xuiDockMax(minA, r.fW - splitSize - minB));
		a = __xuiDockRect(r.fX, r.fY, firstSize, r.fH);
		pNode->tSplitterRect = __xuiDockRect(r.fX + firstSize, r.fY, splitSize, r.fH);
		b = __xuiDockRect(r.fX + firstSize + splitSize, r.fY, __xuiDockMax(0.0f, r.fW - firstSize - splitSize), r.fH);
	} else {
		firstSize = (r.fH - splitSize) * pNode->fRatio;
		firstSize = __xuiDockClamp(firstSize, minA, __xuiDockMax(minA, r.fH - splitSize - minB));
		a = __xuiDockRect(r.fX, r.fY, r.fW, firstSize);
		pNode->tSplitterRect = __xuiDockRect(r.fX, r.fY + firstSize, r.fW, splitSize);
		b = __xuiDockRect(r.fX, r.fY + firstSize + splitSize, r.fW, __xuiDockMax(0.0f, r.fH - firstSize - splitSize));
	}
	__xuiDockLayoutNode(pData, pNode->iFirst, a);
	__xuiDockLayoutNode(pData, pNode->iSecond, b);
}

static int __xuiDockRegionHasContent(xui_dock_panel_data_t* pData, int iRegion)
{
	return __xuiDockRegionValid(iRegion) && pData->arrRegions[iRegion].iRootNode >= 0;
}

static int __xuiDockRegionIsSide(int iRegion)
{
	return iRegion == XUI_DOCK_PANEL_REGION_LEFT ||
		iRegion == XUI_DOCK_PANEL_REGION_RIGHT ||
		iRegion == XUI_DOCK_PANEL_REGION_TOP ||
		iRegion == XUI_DOCK_PANEL_REGION_BOTTOM;
}

static float __xuiDockRegionSize(xui_dock_region_slot_t* r, float total)
{
	float v = (r->iSizeMode == XUI_DOCK_PANEL_SIZE_PIXEL) ? r->fValue : total * r->fValue;
	if ( r->fMaxSize > 0.0f ) v = __xuiDockMin(v, r->fMaxSize);
	v = __xuiDockMax(v, r->fMinSize);
	return v;
}

static float __xuiDockRegionDragBasis(xui_widget pWidget, int iRegion)
{
	xui_rect_t r;
	if ( pWidget == NULL ) return 0.0f;
	r = xuiWidgetGetContentRect(pWidget);
	if ( iRegion == XUI_DOCK_PANEL_REGION_LEFT || iRegion == XUI_DOCK_PANEL_REGION_RIGHT ) return r.fW;
	if ( iRegion == XUI_DOCK_PANEL_REGION_TOP || iRegion == XUI_DOCK_PANEL_REGION_BOTTOM ) return r.fH;
	return 0.0f;
}

static float __xuiDockRegionCurrentSize(xui_dock_panel_data_t* pData, int iRegion)
{
	xui_rect_t r;
	if ( (pData == NULL) || !__xuiDockRegionValid(iRegion) ) return 0.0f;
	r = pData->arrRegions[iRegion].tRect;
	if ( iRegion == XUI_DOCK_PANEL_REGION_LEFT || iRegion == XUI_DOCK_PANEL_REGION_RIGHT ) return r.fW;
	if ( iRegion == XUI_DOCK_PANEL_REGION_TOP || iRegion == XUI_DOCK_PANEL_REGION_BOTTOM ) return r.fH;
	return 0.0f;
}

static int __xuiDockSetRegionDragSize(xui_widget pWidget, xui_dock_panel_data_t* pData, int iRegion, float fSize)
{
	xui_dock_region_slot_t* r;
	float basis;
	float minSize;
	float maxSize;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiDockRegionIsSide(iRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
	r = &pData->arrRegions[iRegion];
	basis = __xuiDockRegionDragBasis(pWidget, iRegion);
	if ( basis <= 1.0f ) return XUI_OK;
	minSize = __xuiDockMax(r->fMinSize, (iRegion == XUI_DOCK_PANEL_REGION_LEFT || iRegion == XUI_DOCK_PANEL_REGION_RIGHT) ? pData->tMetrics.fMinPaneWidth : pData->tMetrics.fMinPaneHeight);
	maxSize = (r->fMaxSize > 0.0f) ? r->fMaxSize : basis * 0.80f;
	maxSize = __xuiDockMin(maxSize, basis * 0.80f);
	fSize = __xuiDockClamp(fSize, minSize, __xuiDockMax(minSize, maxSize));
	if ( r->iSizeMode == XUI_DOCK_PANEL_SIZE_PIXEL ) {
		r->fValue = fSize;
	} else {
		r->fValue = __xuiDockClamp(fSize / basis, 0.05f, 0.80f);
	}
	pData->iLayoutChangeCount++;
	pData->iChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

static void __xuiDockLayoutRegions(xui_dock_panel_data_t* pData, xui_rect_t r)
{
	xui_rect_t center = r;
	xui_dock_panel_metrics_t* m = &pData->tMetrics;
	float split = m->fSplitterSize;
	float size;
	xui_dock_region_slot_t* left = &pData->arrRegions[XUI_DOCK_PANEL_REGION_LEFT];
	xui_dock_region_slot_t* right = &pData->arrRegions[XUI_DOCK_PANEL_REGION_RIGHT];
	xui_dock_region_slot_t* top = &pData->arrRegions[XUI_DOCK_PANEL_REGION_TOP];
	xui_dock_region_slot_t* bottom = &pData->arrRegions[XUI_DOCK_PANEL_REGION_BOTTOM];
	if ( __xuiDockRegionHasContent(pData, XUI_DOCK_PANEL_REGION_LEFT) ) {
		size = __xuiDockMin(__xuiDockRegionSize(left, r.fW), __xuiDockMax(0.0f, center.fW - m->fMinPaneWidth - split));
		left->tRect = __xuiDockRect(center.fX, center.fY, size, center.fH);
		left->tSplitterRect = __xuiDockRect(center.fX + size, center.fY, split, center.fH);
		center.fX += size + split;
		center.fW = __xuiDockMax(0.0f, center.fW - size - split);
	} else {
		left->tRect = left->tSplitterRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if ( __xuiDockRegionHasContent(pData, XUI_DOCK_PANEL_REGION_RIGHT) ) {
		size = __xuiDockMin(__xuiDockRegionSize(right, r.fW), __xuiDockMax(0.0f, center.fW - m->fMinPaneWidth - split));
		right->tRect = __xuiDockRect(center.fX + center.fW - size, center.fY, size, center.fH);
		right->tSplitterRect = __xuiDockRect(right->tRect.fX - split, center.fY, split, center.fH);
		center.fW = __xuiDockMax(0.0f, center.fW - size - split);
	} else {
		right->tRect = right->tSplitterRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if ( __xuiDockRegionHasContent(pData, XUI_DOCK_PANEL_REGION_TOP) ) {
		size = __xuiDockMin(__xuiDockRegionSize(top, r.fH), __xuiDockMax(0.0f, center.fH - m->fMinPaneHeight - split));
		top->tRect = __xuiDockRect(center.fX, center.fY, center.fW, size);
		top->tSplitterRect = __xuiDockRect(center.fX, center.fY + size, center.fW, split);
		center.fY += size + split;
		center.fH = __xuiDockMax(0.0f, center.fH - size - split);
	} else {
		top->tRect = top->tSplitterRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if ( __xuiDockRegionHasContent(pData, XUI_DOCK_PANEL_REGION_BOTTOM) ) {
		size = __xuiDockMin(__xuiDockRegionSize(bottom, r.fH), __xuiDockMax(0.0f, center.fH - m->fMinPaneHeight - split));
		bottom->tRect = __xuiDockRect(center.fX, center.fY + center.fH - size, center.fW, size);
		bottom->tSplitterRect = __xuiDockRect(center.fX, bottom->tRect.fY - split, center.fW, split);
		center.fH = __xuiDockMax(0.0f, center.fH - size - split);
	} else {
		bottom->tRect = bottom->tSplitterRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	}
	pData->arrRegions[XUI_DOCK_PANEL_REGION_DOCUMENT].tRect = __xuiDockRect(center.fX, center.fY, center.fW, center.fH);
	pData->arrRegions[XUI_DOCK_PANEL_REGION_DOCUMENT].tSplitterRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
}

static float __xuiDockAutoHideExpandWidth(const xui_dock_window_slot_t* pWin, xui_rect_t r, float strip)
{
	float room = __xuiDockMax(0.0f, r.fW - strip);
	float target = ((pWin != NULL) && (pWin->tLastDockRect.fW > 0.0f)) ? pWin->tLastDockRect.fW : r.fW * 0.33f;
	float minSize = 160.0f;
	float maxSize;
	if ( room <= 0.0f ) return 0.0f;
	maxSize = __xuiDockMin(420.0f, __xuiDockMax(minSize, room - 8.0f));
	return __xuiDockMin(__xuiDockClamp(target, minSize, maxSize), room);
}

static float __xuiDockAutoHideExpandHeight(const xui_dock_window_slot_t* pWin, xui_rect_t r, float strip)
{
	float room = __xuiDockMax(0.0f, r.fH - strip);
	float target = ((pWin != NULL) && (pWin->tLastDockRect.fH > 0.0f)) ? pWin->tLastDockRect.fH : r.fH * 0.36f;
	float minSize = 120.0f;
	float maxSize;
	if ( room <= 0.0f ) return 0.0f;
	maxSize = __xuiDockMin(320.0f, __xuiDockMax(minSize, room - 8.0f));
	return __xuiDockMin(__xuiDockClamp(target, minSize, maxSize), room);
}

static void __xuiDockLayoutAutoHide(xui_dock_panel_data_t* pData, xui_rect_t r)
{
	float s = pData->tMetrics.fAutoHideStripSize;
	float panel;
	float yLeft = r.fY + 12.0f;
	float yRight = r.fY + 12.0f;
	float xTop = r.fX + 12.0f;
	float xBottom = r.fX + 12.0f;
	xui_dock_window_slot_t* pExpand;
	int i;
	pData->tAutoHideExpandRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pData->tAutoHideExpandClientRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pData->tAutoHidePinRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	pData->tAutoHideCloseRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = &pData->arrWindows[i];
		int iAutoHideRegion;
		w->tAutoHideRect = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
		if ( !w->bUsed || w->iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) continue;
		iAutoHideRegion = __xuiDockWindowAutoHideRegion(w);
		switch ( iAutoHideRegion ) {
		case XUI_DOCK_PANEL_REGION_LEFT:
			w->tAutoHideRect = __xuiDockRect(r.fX, yLeft, s, 92.0f);
			yLeft += 96.0f;
			break;
		case XUI_DOCK_PANEL_REGION_RIGHT:
			w->tAutoHideRect = __xuiDockRect(r.fX + r.fW - s, yRight, s, 92.0f);
			yRight += 96.0f;
			break;
		case XUI_DOCK_PANEL_REGION_TOP:
			w->tAutoHideRect = __xuiDockRect(xTop, r.fY, 118.0f, s);
			xTop += 122.0f;
			break;
		case XUI_DOCK_PANEL_REGION_BOTTOM:
		default:
			w->tAutoHideRect = __xuiDockRect(xBottom, r.fY + r.fH - s, 118.0f, s);
			xBottom += 122.0f;
			break;
		}
	}
	pExpand = __xuiDockWindowAt(pData, pData->iAutoHideExpandWindow);
	if ( pExpand == NULL || pExpand->iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) {
		pData->iAutoHideExpandWindow = -1;
		return;
	}
	switch ( __xuiDockWindowAutoHideRegion(pExpand) ) {
	case XUI_DOCK_PANEL_REGION_LEFT:
		panel = __xuiDockAutoHideExpandWidth(pExpand, r, s);
		pData->tAutoHideExpandRect = __xuiDockRect(r.fX + s, r.fY, panel, r.fH);
		break;
	case XUI_DOCK_PANEL_REGION_RIGHT:
		panel = __xuiDockAutoHideExpandWidth(pExpand, r, s);
		pData->tAutoHideExpandRect = __xuiDockRect(r.fX + r.fW - s - panel, r.fY, panel, r.fH);
		break;
	case XUI_DOCK_PANEL_REGION_TOP:
		panel = __xuiDockAutoHideExpandHeight(pExpand, r, s);
		pData->tAutoHideExpandRect = __xuiDockRect(r.fX, r.fY + s, r.fW, panel);
		break;
	case XUI_DOCK_PANEL_REGION_BOTTOM:
	default:
		panel = __xuiDockAutoHideExpandHeight(pExpand, r, s);
		pData->tAutoHideExpandRect = __xuiDockRect(r.fX, r.fY + r.fH - s - panel, r.fW, panel);
		break;
	}
	pData->tAutoHideExpandClientRect = __xuiDockRect(pData->tAutoHideExpandRect.fX + pData->tMetrics.fBorderWidth,
		pData->tAutoHideExpandRect.fY + pData->tMetrics.fCaptionHeight + pData->tMetrics.fBorderWidth,
		__xuiDockMax(0.0f, pData->tAutoHideExpandRect.fW - pData->tMetrics.fBorderWidth * 2.0f),
		__xuiDockMax(0.0f, pData->tAutoHideExpandRect.fH - pData->tMetrics.fCaptionHeight - pData->tMetrics.fBorderWidth * 2.0f));
	pData->tAutoHideCloseRect = __xuiDockRect(pData->tAutoHideExpandRect.fX + pData->tAutoHideExpandRect.fW - pData->tMetrics.fButtonGap - pData->tMetrics.fButtonSize,
		pData->tAutoHideExpandRect.fY + (pData->tMetrics.fCaptionHeight - pData->tMetrics.fButtonSize) * 0.5f,
		pData->tMetrics.fButtonSize, pData->tMetrics.fButtonSize);
	pData->tAutoHidePinRect = __xuiDockRect(pData->tAutoHideCloseRect.fX - pData->tMetrics.fButtonGap - pData->tMetrics.fButtonSize,
		pData->tAutoHideCloseRect.fY, pData->tMetrics.fButtonSize, pData->tMetrics.fButtonSize);
}

static void __xuiDockUpdateWindowHosts(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	int i;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = &pData->arrWindows[i];
		int bVisible = 0;
		if ( !w->bUsed ) continue;
		if ( w->iState == XUI_DOCK_PANEL_WINDOW_DOCKED ) {
			xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, w->iPane);
			if ( (pPane != NULL) && __xuiDockRectRenderable(pPane->tRect) ) {
				w->tLastDockRect = pPane->tRect;
			}
			if ( (pPane != NULL) && (__xuiDockPaneActiveWindow(pPane) == w->iWindow) ) {
				w->tRect = pPane->tClientRect;
				bVisible = 1;
			}
		} else if ( w->iState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
			w->tRect = w->tFloatRect;
			bVisible = 1;
		} else if ( w->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->iAutoHideExpandWindow == w->iWindow ) {
			w->tRect = pData->tAutoHideExpandRect;
			bVisible = 1;
		}
		__xuiDockArrangeWindowHost(w, &pData->tMetrics, bVisible);
	}
	__xuiDockRefreshHostInputOrder(pData);
	(void)__xuiDockApplyPendingFocus(pWidget, pData);
	(void)pWidget;
}

static void __xuiDockArrangeDragOverlay(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_rect_t tRect)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pDragOverlayWidget == NULL) ) return;
	(void)xuiWidgetSetRect(pData->pDragOverlayWidget, tRect);
	(void)xuiWidgetArrange(pData->pDragOverlayWidget, tRect);
	(void)xuiWidgetSetHitTestVisible(pData->pDragOverlayWidget, 0);
	(void)xuiWidgetSetLayer(pData->pDragOverlayWidget, XUI_LAYER_DRAG, XUI_WINDOW_Z_TOPMOST + 200);
	(void)xuiWidgetSetVisible(pData->pDragOverlayWidget, __xuiDockDragVisualActive(pData));
}

static int __xuiDockPanelArrangeNow(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_rect_t tRect)
{
	int i;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect = xuiInternalSnapRect(tRect);
	__xuiDockLayoutRegions(pData, tRect);
	for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		if ( pData->arrRegions[i].iRootNode >= 0 ) {
			__xuiDockLayoutNode(pData, pData->arrRegions[i].iRootNode, pData->arrRegions[i].tRect);
		}
	}
	__xuiDockLayoutAutoHide(pData, tRect);
	__xuiDockUpdateWindowHosts(pWidget, pData);
	__xuiDockArrangeDragOverlay(pWidget, pData, tRect);
	return XUI_OK;
}

static int __xuiDockPanelArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_dock_panel_data_t* pData;
	(void)pUser;
	pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockPanelArrangeNow(pWidget, pData, tContentRect);
}

static int __xuiDockHitLocal(xui_dock_panel_data_t* pData, float x, float y, xui_dock_hit_t* hit)
{
	int i;
	if ( hit != NULL ) {
		memset(hit, 0, sizeof(*hit));
		hit->iSize = sizeof(*hit);
		hit->iType = XUI_DOCK_PANEL_HIT_NONE;
		hit->iWindow = -1;
		hit->iPane = -1;
		hit->iNode = -1;
		hit->iRegion = -1;
	}
	if ( pData->iAutoHideExpandWindow >= 0 && pData->tAutoHideExpandRect.fW > 0.0f && pData->tAutoHideExpandRect.fH > 0.0f ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pData->iAutoHideExpandWindow);
		if ( w != NULL && __xuiDockRectContains(pData->tAutoHideCloseRect, x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_AUTO_HIDE_CLOSE;
				hit->iWindow = w->iWindow;
				hit->iRegion = __xuiDockWindowAutoHideRegion(w);
				hit->tRect = pData->tAutoHideCloseRect;
			}
			return 1;
		}
		if ( w != NULL && __xuiDockRectContains(pData->tAutoHidePinRect, x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_AUTO_HIDE_PIN;
				hit->iWindow = w->iWindow;
				hit->iRegion = __xuiDockWindowAutoHideRegion(w);
				hit->tRect = pData->tAutoHidePinRect;
			}
			return 1;
		}
		if ( w != NULL && __xuiDockRectContains(__xuiDockRect(pData->tAutoHideExpandRect.fX, pData->tAutoHideExpandRect.fY, pData->tAutoHideExpandRect.fW, pData->tMetrics.fCaptionHeight), x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_AUTO_HIDE_PANEL;
				hit->iWindow = w->iWindow;
				hit->iRegion = __xuiDockWindowAutoHideRegion(w);
				hit->tRect = pData->tAutoHideExpandRect;
			}
			return 1;
		}
	}
	for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		xui_dock_region_slot_t* r = &pData->arrRegions[i];
		if ( !__xuiDockRegionIsSide(i) || !__xuiDockRegionHasContent(pData, i) ) continue;
		if ( __xuiDockRectContains(r->tSplitterRect, x, y) && r->tSplitterRect.fW > 0.0f && r->tSplitterRect.fH > 0.0f ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_SPLITTER;
				hit->iNode = -1;
				hit->iRegion = i;
				hit->tRect = r->tSplitterRect;
			}
			return 1;
		}
	}
	for ( i = 0; i < XUI_DOCK_PANEL_NODE_CAPACITY; i++ ) {
		xui_dock_node_slot_t* n = &pData->arrNodes[i];
		if ( n->bUsed && n->iType == XUI_DOCK_NODE_SPLIT && __xuiDockRectContains(n->tSplitterRect, x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_SPLITTER;
				hit->iNode = i;
				hit->tRect = n->tSplitterRect;
			}
			return 1;
		}
	}
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = &pData->arrWindows[i];
		if ( w->bUsed && w->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && __xuiDockRectContains(w->tAutoHideRect, x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_AUTO_HIDE;
				hit->iWindow = i;
				hit->iRegion = __xuiDockWindowAutoHideRegion(w);
				hit->tRect = w->tAutoHideRect;
			}
			return 1;
		}
	}
	for ( i = 0; i < XUI_DOCK_PANEL_PANE_CAPACITY; i++ ) {
		xui_dock_pane_slot_t* p = &pData->arrPanes[i];
		int j;
		if ( !p->bUsed ) continue;
		if ( __xuiDockRectContains(p->tCloseRect, x, y) && p->tCloseRect.fW > 0.0f ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_PANE_CLOSE;
				hit->iPane = i;
				hit->iWindow = __xuiDockPaneActiveWindow(p);
				hit->iRegion = p->iRegion;
				hit->tRect = p->tCloseRect;
			}
			return 1;
		}
		if ( __xuiDockRectContains(p->tPinRect, x, y) && p->tPinRect.fW > 0.0f ) {
			if ( !__xuiDockPaneCanAutoHide(pData, p) ) return 1;
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_PANE_PIN;
				hit->iPane = i;
				hit->iWindow = __xuiDockPaneActiveWindow(p);
				hit->iRegion = p->iRegion;
				hit->tRect = p->tPinRect;
			}
			return 1;
		}
		if ( __xuiDockRectContains(p->tOptionRect, x, y) && p->tOptionRect.fW > 0.0f ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_PANE_OPTION;
				hit->iPane = i;
				hit->iWindow = __xuiDockPaneActiveWindow(p);
				hit->iRegion = p->iRegion;
				hit->tRect = p->tOptionRect;
			}
			return 1;
		}
		if ( p->bOverflow && __xuiDockRectContains(p->tOverflowRect, x, y) && p->tOverflowRect.fW > 0.0f ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_PANE_OVERFLOW;
				hit->iPane = i;
				hit->iWindow = __xuiDockPaneActiveWindow(p);
				hit->iRegion = p->iRegion;
				hit->tRect = p->tOverflowRect;
			}
			return 1;
		}
		for ( j = 0; j < p->iWindowCount; j++ ) {
			int widx = p->arrWindows[j];
			xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, widx);
			if ( (w != NULL) && __xuiDockRectContains(w->tTabCloseRect, x, y) && w->tTabCloseRect.fW > 0.0f ) {
				if ( hit != NULL ) {
					hit->iType = XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE;
					hit->iPane = i;
					hit->iWindow = widx;
					hit->iRegion = p->iRegion;
					hit->tRect = w->tTabCloseRect;
				}
				return 1;
			}
			if ( (w != NULL) && __xuiDockRectContains(w->tTabRect, x, y) ) {
				if ( hit != NULL ) {
					hit->iType = XUI_DOCK_PANEL_HIT_PANE_TAB;
					hit->iPane = i;
					hit->iWindow = widx;
					hit->iRegion = p->iRegion;
					hit->tRect = w->tTabRect;
				}
				return 1;
			}
		}
		if ( __xuiDockRectContains(p->tCaptionRect, x, y) ) {
			if ( hit != NULL ) {
				hit->iType = XUI_DOCK_PANEL_HIT_PANE_CAPTION;
				hit->iPane = i;
				hit->iWindow = __xuiDockPaneActiveWindow(p);
				hit->iRegion = p->iRegion;
				hit->tRect = p->tCaptionRect;
			}
			return 1;
		}
	}
	return 0;
}

static int __xuiDockTooltipFill(xui_tooltip_desc_t* pDesc, const char* sText)
{
	if ( (pDesc == NULL) || (sText == NULL) || (sText[0] == '\0') ) return 0;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_TEXT;
	pDesc->sText = sText;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_CURSOR;
	pDesc->fOffsetX = 8.0f;
	pDesc->fOffsetY = 10.0f;
	pDesc->fDelay = 0.0f;
	pDesc->bFollowCursor = 1;
	return 1;
}

static int __xuiDockTooltipResolve(xui_context pContext, xui_widget pWidget, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_dock_panel_data_t* pData;
	xui_dock_window_slot_t* pWin;
	xui_dock_hit_t tHit;
	xui_rect_t tWorld;
	const char* sText;
	float lx;
	float ly;
	(void)pUser;
	if ( (pContext == NULL) || (pWidget == NULL) || (pDesc == NULL) ) return 0;
	pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return 0;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	lx = pContext->fTooltipMouseX - tWorld.fX;
	ly = pContext->fTooltipMouseY - tWorld.fY;
	if ( !__xuiDockHitLocal(pData, lx, ly, &tHit) ) return 0;
	sText = NULL;
	pWin = __xuiDockWindowAt(pData, tHit.iWindow);
	switch ( tHit.iType ) {
	case XUI_DOCK_PANEL_HIT_PANE_TAB:
	case XUI_DOCK_PANEL_HIT_PANE_CAPTION:
	case XUI_DOCK_PANEL_HIT_AUTO_HIDE:
	case XUI_DOCK_PANEL_HIT_AUTO_HIDE_PANEL:
		sText = (pWin != NULL) ? pWin->sTitle : NULL;
		break;
	case XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE:
	case XUI_DOCK_PANEL_HIT_PANE_CLOSE:
	case XUI_DOCK_PANEL_HIT_AUTO_HIDE_CLOSE:
		sText = __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_CLOSE);
		break;
	case XUI_DOCK_PANEL_HIT_PANE_PIN:
		sText = __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_AUTO_HIDE);
		break;
	case XUI_DOCK_PANEL_HIT_PANE_OPTION:
		sText = __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_OPTIONS);
		break;
	case XUI_DOCK_PANEL_HIT_PANE_OVERFLOW:
		sText = __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_MORE_TABS);
		break;
	case XUI_DOCK_PANEL_HIT_AUTO_HIDE_PIN:
		sText = __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_DOCK);
		break;
	default:
		break;
	}
	return __xuiDockTooltipFill(pDesc, sText);
}

static int __xuiDockCloseWindow(xui_widget pWidget, xui_dock_panel_data_t* pData, int iWindow)
{
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !w->bClosable ) return XUI_OK;
	if ( pData->onClose != NULL ) {
		pData->onClose(pWidget, iWindow, pData->pCloseUser);
	}
	return xuiDockPanelHideWindow(pWidget, iWindow);
}

static int __xuiDockPaneClosableCount(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, int iExceptWindow)
{
	int count;
	int i;
	if ( (pData == NULL) || (pPane == NULL) ) return 0;
	count = 0;
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( (w != NULL) && (w->iWindow != iExceptWindow) && w->bClosable ) count++;
	}
	return count;
}

static int __xuiDockClosePaneWindows(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane, int iExceptWindow)
{
	xui_dock_pane_slot_t* pPane;
	int arrClose[XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY];
	int arrLastIndex[XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY];
	int count;
	int i;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pPane = __xuiDockPaneAt(pData, iPane);
	if ( pPane == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	count = 0;
	for ( i = 0; i < pPane->iWindowCount && count < XUI_DOCK_PANEL_PANE_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( (w != NULL) && (w->iWindow != iExceptWindow) && w->bClosable ) {
			arrClose[count] = w->iWindow;
			arrLastIndex[count] = i;
			count++;
		}
	}
	for ( i = 0; i < count; i++ ) {
		ret = __xuiDockCloseWindow(pWidget, pData, arrClose[i]);
		if ( ret != XUI_OK ) return ret;
		{
			xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, arrClose[i]);
			if ( w != NULL ) w->iLastTabIndex = arrLastIndex[i];
		}
	}
	return XUI_OK;
}

static xui_menu_item_t __xuiDockMenuItem(const char* sText, int iType, uint32_t iState, int iValue)
{
	xui_menu_item_t item;
	memset(&item, 0, sizeof(item));
	item.sText = sText;
	item.iType = iType;
	item.iState = iState;
	item.iValue = iValue;
	return item;
}

static int __xuiDockMenuTitleIndexForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_DOCK_PANEL_MENU_FLOAT: return 0;
	case XUI_DOCK_PANEL_MENU_AUTO_HIDE: return 1;
	case XUI_DOCK_PANEL_MENU_CLOSE: return 2;
	case XUI_DOCK_PANEL_MENU_CLOSE_OTHERS: return 3;
	case XUI_DOCK_PANEL_MENU_CLOSE_ALL: return 4;
	default: return -1;
	}
}

static int __xuiDockMenuTitleTranslationForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_DOCK_PANEL_MENU_FLOAT: return XUI_TR_DOCK_FLOAT;
	case XUI_DOCK_PANEL_MENU_AUTO_HIDE: return XUI_TR_DOCK_AUTO_HIDE;
	case XUI_DOCK_PANEL_MENU_CLOSE: return XUI_TR_DOCK_CLOSE;
	case XUI_DOCK_PANEL_MENU_CLOSE_OTHERS: return XUI_TR_DOCK_CLOSE_OTHERS;
	case XUI_DOCK_PANEL_MENU_CLOSE_ALL: return XUI_TR_DOCK_CLOSE_ALL;
	default: return 0;
	}
}

static const char* __xuiDockMenuTitleForCommand(xui_widget pWidget, xui_dock_panel_data_t* pData, int iCommand)
{
	xui_context pContext;
	int iIndex;
	int iTranslation;

	if ( pData == NULL ) return "";
	iIndex = __xuiDockMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_DOCK_PANEL_MENU_TITLE_COUNT ) return "";
	if ( pData->arrMenuTitle[iIndex] != NULL ) return pData->arrMenuTitle[iIndex];
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	iTranslation = __xuiDockMenuTitleTranslationForCommand(iCommand);
	return (iTranslation != 0) ? xuiTranslate(pContext, iTranslation) : "";
}

static int __xuiDockTooltipTextIndexForId(int iTooltip)
{
	switch ( iTooltip ) {
	case XUI_DOCK_PANEL_TOOLTIP_CLOSE: return 0;
	case XUI_DOCK_PANEL_TOOLTIP_AUTO_HIDE: return 1;
	case XUI_DOCK_PANEL_TOOLTIP_OPTIONS: return 2;
	case XUI_DOCK_PANEL_TOOLTIP_MORE_TABS: return 3;
	case XUI_DOCK_PANEL_TOOLTIP_DOCK: return 4;
	default: return -1;
	}
}

static int __xuiDockTooltipTextTranslationForId(int iTooltip)
{
	switch ( iTooltip ) {
	case XUI_DOCK_PANEL_TOOLTIP_CLOSE: return XUI_TR_DOCK_CLOSE;
	case XUI_DOCK_PANEL_TOOLTIP_AUTO_HIDE: return XUI_TR_DOCK_AUTO_HIDE;
	case XUI_DOCK_PANEL_TOOLTIP_OPTIONS: return XUI_TR_DOCK_OPTIONS;
	case XUI_DOCK_PANEL_TOOLTIP_MORE_TABS: return XUI_TR_DOCK_MORE_TABS;
	case XUI_DOCK_PANEL_TOOLTIP_DOCK: return XUI_TR_DOCK_DOCK;
	default: return 0;
	}
}

static const char* __xuiDockTooltipTextForId(xui_context pContext, xui_dock_panel_data_t* pData, int iTooltip)
{
	int iIndex;
	int iTranslation;

	if ( pData == NULL ) return "";
	iIndex = __xuiDockTooltipTextIndexForId(iTooltip);
	if ( iIndex < 0 || iIndex >= XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT ) return "";
	if ( pData->arrTooltipText[iIndex] != NULL ) return pData->arrTooltipText[iIndex];
	iTranslation = __xuiDockTooltipTextTranslationForId(iTooltip);
	return (iTranslation != 0) ? xuiTranslate(pContext, iTranslation) : "";
}

static int __xuiDockMenuOpenAtLocal(xui_widget pWidget, xui_widget pMenu, xui_rect_t tLocal)
{
	xui_rect_t world;
	if ( (pWidget == NULL) || (pMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	world = xuiWidgetGetWorldRect(pWidget);
	return xuiMenuOpenAt(pMenu, pWidget, world.fX + tLocal.fX, world.fY + tLocal.fY + tLocal.fH);
}

static void __xuiDockMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_pane_slot_t* pPane;
	xui_dock_window_slot_t* pWin;
	xui_rect_t tFloat;
	int iWindow;
	(void)pMenu;
	(void)iIndex;
	if ( pData == NULL ) return;
	pPane = __xuiDockPaneAt(pData, pData->iMenuPane);
	if ( pPane == NULL ) return;
	if ( iValue >= XUI_DOCK_PANEL_MENU_WINDOW_BASE ) {
		(void)xuiDockPanelSetPaneActiveWindow(pWidget, pPane->iPane, iValue - XUI_DOCK_PANEL_MENU_WINDOW_BASE);
		return;
	}
	iWindow = __xuiDockPaneActiveWindow(pPane);
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( pWin == NULL ) return;
	switch ( iValue ) {
	case XUI_DOCK_PANEL_MENU_FLOAT:
		if ( !pWin->bDockable ) break;
		tFloat = pPane->tRect;
		tFloat.fX += 28.0f;
		tFloat.fY += 28.0f;
		tFloat.fW = __xuiDockMax(180.0f, __xuiDockMin(320.0f, tFloat.fW));
		tFloat.fH = __xuiDockMax(120.0f, __xuiDockMin(220.0f, tFloat.fH));
		(void)xuiDockPanelFloatWindow(pWidget, iWindow, tFloat);
		break;
	case XUI_DOCK_PANEL_MENU_AUTO_HIDE:
		(void)xuiDockPanelAutoHideWindow(pWidget, iWindow);
		break;
	case XUI_DOCK_PANEL_MENU_CLOSE:
		(void)__xuiDockCloseWindow(pWidget, pData, iWindow);
		break;
	case XUI_DOCK_PANEL_MENU_CLOSE_OTHERS:
		(void)__xuiDockClosePaneWindows(pWidget, pData, pPane->iPane, iWindow);
		break;
	case XUI_DOCK_PANEL_MENU_CLOSE_ALL:
		(void)__xuiDockClosePaneWindows(pWidget, pData, pPane->iPane, -1);
		break;
	default:
		break;
	}
}

static int __xuiDockBuildPaneMenu(xui_widget pWidget, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane)
{
	xui_menu_item_t arrItems[XUI_MENU_ITEM_CAPACITY];
	xui_dock_window_slot_t* pActive;
	uint32_t enabled = XUI_MENU_ITEM_ENABLED;
	int activeWindow;
	int closableOthers;
	int activeClosable;
	int canAutoHide;
	int count;
	int i;
	if ( (pWidget == NULL) || (pData == NULL) || (pPane == NULL) || (pData->pOptionMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrItems, 0, sizeof(arrItems));
	count = 0;
	activeWindow = __xuiDockPaneActiveWindow(pPane);
	for ( i = 0; i < pPane->iWindowCount && count < XUI_MENU_ITEM_CAPACITY - 7; i++ ) {
		xui_dock_window_slot_t* pWin = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( pWin == NULL ) continue;
		arrItems[count++] = __xuiDockMenuItem(pWin->sTitle, XUI_MENU_ITEM_RADIO,
			enabled | ((pWin->iWindow == activeWindow) ? XUI_MENU_ITEM_CHECKED : 0u),
			XUI_DOCK_PANEL_MENU_WINDOW_BASE + pWin->iWindow);
	}
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(NULL, XUI_MENU_ITEM_SEPARATOR, 0u, 0);
	}
	pActive = __xuiDockWindowAt(pData, activeWindow);
	activeClosable = (pActive != NULL && pActive->bClosable) ? 1 : 0;
	canAutoHide = __xuiDockPaneCanAutoHide(pData, pPane);
	closableOthers = __xuiDockPaneClosableCount(pData, pPane, activeWindow);
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(__xuiDockMenuTitleForCommand(pWidget, pData, XUI_DOCK_PANEL_MENU_FLOAT), XUI_MENU_ITEM_NORMAL, (pActive != NULL && pActive->bDockable) ? enabled : 0u, XUI_DOCK_PANEL_MENU_FLOAT);
	}
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(__xuiDockMenuTitleForCommand(pWidget, pData, XUI_DOCK_PANEL_MENU_AUTO_HIDE), XUI_MENU_ITEM_NORMAL, canAutoHide ? enabled : 0u, XUI_DOCK_PANEL_MENU_AUTO_HIDE);
	}
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(__xuiDockMenuTitleForCommand(pWidget, pData, XUI_DOCK_PANEL_MENU_CLOSE), XUI_MENU_ITEM_NORMAL, activeClosable ? (enabled | XUI_MENU_ITEM_DANGER) : 0u, XUI_DOCK_PANEL_MENU_CLOSE);
	}
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(__xuiDockMenuTitleForCommand(pWidget, pData, XUI_DOCK_PANEL_MENU_CLOSE_OTHERS), XUI_MENU_ITEM_NORMAL, (closableOthers > 0) ? enabled : 0u, XUI_DOCK_PANEL_MENU_CLOSE_OTHERS);
	}
	if ( count < XUI_MENU_ITEM_CAPACITY ) {
		arrItems[count++] = __xuiDockMenuItem(__xuiDockMenuTitleForCommand(pWidget, pData, XUI_DOCK_PANEL_MENU_CLOSE_ALL), XUI_MENU_ITEM_NORMAL, (activeClosable || closableOthers > 0) ? (enabled | XUI_MENU_ITEM_DANGER) : 0u, XUI_DOCK_PANEL_MENU_CLOSE_ALL);
	}
	(void)pWidget;
	return xuiMenuSetItems(pData->pOptionMenu, arrItems, count);
}

static int __xuiDockBuildOverflowMenu(xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane)
{
	xui_menu_item_t arrItems[XUI_MENU_ITEM_CAPACITY];
	int activeWindow;
	int count;
	int i;
	if ( (pData == NULL) || (pPane == NULL) || (pData->pOverflowMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrItems, 0, sizeof(arrItems));
	activeWindow = __xuiDockPaneActiveWindow(pPane);
	count = 0;
	for ( i = 0; i < pPane->iWindowCount && count < XUI_MENU_ITEM_CAPACITY; i++ ) {
		xui_dock_window_slot_t* pWin = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( pWin == NULL ) continue;
		arrItems[count++] = __xuiDockMenuItem(pWin->sTitle, XUI_MENU_ITEM_RADIO,
			XUI_MENU_ITEM_ENABLED | ((pWin->iWindow == activeWindow) ? XUI_MENU_ITEM_CHECKED : 0u),
			XUI_DOCK_PANEL_MENU_WINDOW_BASE + pWin->iWindow);
	}
	return xuiMenuSetItems(pData->pOverflowMenu, arrItems, count);
}

static int __xuiDockOpenPaneMenu(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane)
{
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	xui_rect_t anchor;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) || (pPane == NULL) || (pData->pOptionMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = __xuiDockBuildPaneMenu(pWidget, pData, pPane);
	if ( ret != XUI_OK ) return ret;
	pData->iMenuPane = iPane;
	anchor = (pPane->tOptionRect.fW > 0.0f) ? pPane->tOptionRect : __xuiDockRect(pPane->tRect.fX + 6.0f, pPane->tRect.fY + 6.0f, 1.0f, pData->tMetrics.fCaptionHeight);
	return __xuiDockMenuOpenAtLocal(pWidget, pData->pOptionMenu, anchor);
}

static int __xuiDockOpenOverflowMenu(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane)
{
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	xui_rect_t anchor;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) || (pPane == NULL) || (pData->pOverflowMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = __xuiDockBuildOverflowMenu(pData, pPane);
	if ( ret != XUI_OK ) return ret;
	pData->iMenuPane = iPane;
	anchor = (pPane->tOverflowRect.fW > 0.0f) ? pPane->tOverflowRect : pPane->tTabStripRect;
	return __xuiDockMenuOpenAtLocal(pWidget, pData->pOverflowMenu, anchor);
}

static xui_rect_t __xuiDockDockedDragFloatRect(xui_dock_window_slot_t* pWin, float lx, float ly)
{
	xui_rect_t r;
	float w;
	float h;
	if ( pWin == NULL ) return __xuiDockRect(lx - 48.0f, ly - 12.0f, 220.0f, 150.0f);
	r = pWin->tRect;
	if ( r.fW <= 0.0f || r.fH <= 0.0f ) r = pWin->tFloatRect;
	w = __xuiDockClamp((r.fW > 0.0f) ? r.fW : 220.0f, 180.0f, 360.0f);
	h = __xuiDockClamp((r.fH > 0.0f) ? r.fH : 150.0f, 120.0f, 240.0f);
	return __xuiDockRect(lx - __xuiDockMin(72.0f, w * 0.35f), ly - 12.0f, w, h);
}

static int __xuiDockBeginDockedDrag(xui_widget pWidget, xui_dock_panel_data_t* pData, int iPane, int iWindow, float lx, float ly)
{
	xui_dock_window_slot_t* pWin;
	if ( (pWidget == NULL) || (pData == NULL) || (iWindow < 0) ) return XUI_OK;
	pWin = __xuiDockWindowAt(pData, iWindow);
	if ( (pWin == NULL) || !pWin->bDockable || (pWin->iState != XUI_DOCK_PANEL_WINDOW_DOCKED) ) return XUI_OK;
	pData->iDragType = XUI_DOCK_DRAG_DOCKED;
	pData->iDragNode = -1;
	pData->iDragRegion = -1;
	pData->iDragPane = iPane;
	pData->iDragWindow = iWindow;
	pData->iDragSourceIndex = __xuiDockPaneIndexOfWindow(__xuiDockPaneAt(pData, iPane), iWindow);
	pData->iDragInsertIndex = -1;
	pData->iDragSide = 0;
	pData->bDragFloating = 0;
	pData->fDragStartX = lx;
	pData->fDragStartY = ly;
	pData->tDragStartRect = __xuiDockDockedDragFloatRect(pWin, lx, ly);
	(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
	return xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
}

static int __xuiDockWidgetIsDescendantOf(xui_widget pWidget, xui_widget pAncestor)
{
	xui_widget pScan;
	if ( (pWidget == NULL) || (pAncestor == NULL) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = xuiWidgetGetParent(pScan) ) {
		if ( pScan == pAncestor ) return 1;
	}
	return 0;
}

static int __xuiDockPointerTargetIsFloatingHost(xui_dock_panel_data_t* pData, xui_widget pTarget)
{
	int i;
	if ( (pData == NULL) || (pTarget == NULL) ) return 0;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = &pData->arrWindows[i];
		if ( !w->bUsed || w->pHostWidget == NULL ) continue;
		if ( w->iState != XUI_DOCK_PANEL_WINDOW_FLOATING &&
		     !(w->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->iAutoHideExpandWindow == w->iWindow) ) {
			continue;
		}
		if ( __xuiDockWidgetIsDescendantOf(pTarget, w->pHostWidget) ) return 1;
	}
	return 0;
}

static int __xuiDockPanelEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_dock_panel_data_t* pData;
	xui_dock_hit_t hit;
	xui_rect_t world;
	float lx;
	float ly;
	int left;
	int middle;
	(void)pUser;
	pData = __xuiDockPanelGetData(pWidget);
	if ( (pWidget == NULL) || (pEvent == NULL) || (pData == NULL) ) return XUI_OK;
	if ( (pEvent->iType == XUI_EVENT_POINTER_DOWN ||
	      pEvent->iType == XUI_EVENT_POINTER_MOVE ||
	      pEvent->iType == XUI_EVENT_POINTER_UP ||
	      pEvent->iType == XUI_EVENT_POINTER_WHEEL ||
	      pEvent->iType == XUI_EVENT_POINTER_LEAVE) &&
	     (pEvent->pTarget != pWidget) &&
	     (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) != pWidget) &&
	     __xuiDockPointerTargetIsFloatingHost(pData, pEvent->pTarget) ) {
		return XUI_OK;
	}
	world = xuiWidgetGetWorldRect(pWidget);
	lx = pEvent->fX - world.fX;
	ly = pEvent->fY - world.fY;
	left = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	middle = (pEvent->iButton == XUI_POINTER_BUTTON_MIDDLE);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		if ( !left && !middle ) break;
		if ( __xuiDockHitLocal(pData, lx, ly, &hit) ) {
			if ( middle ) {
				if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_TAB || hit.iType == XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE ) {
					(void)__xuiDockCloseWindow(pWidget, pData, hit.iWindow);
					return XUI_EVENT_DISPATCH_STOP;
				}
				break;
			}
			(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
			if ( pData->iAutoHideExpandWindow >= 0 &&
			     hit.iType != XUI_DOCK_PANEL_HIT_AUTO_HIDE_PANEL &&
			     hit.iType != XUI_DOCK_PANEL_HIT_AUTO_HIDE_PIN &&
			     hit.iType != XUI_DOCK_PANEL_HIT_AUTO_HIDE_CLOSE &&
			     hit.iType != XUI_DOCK_PANEL_HIT_AUTO_HIDE ) {
				(void)xuiDockPanelCollapseAutoHide(pWidget);
			}
			if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE ) {
				(void)__xuiDockCloseWindow(pWidget, pData, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_TAB ) {
				xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, hit.iPane);
				int idx = __xuiDockPaneIndexOfWindow(pPane, hit.iWindow);
				if ( idx >= 0 ) (void)__xuiDockPaneSetActiveIndex(pWidget, pData, pPane, idx);
				{
					int ret = __xuiDockBeginDockedDrag(pWidget, pData, hit.iPane, hit.iWindow, lx, ly);
					if ( ret != XUI_OK ) return ret;
				}
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_CAPTION ) {
				int ret = __xuiDockBeginDockedDrag(pWidget, pData, hit.iPane, hit.iWindow, lx, ly);
				if ( ret != XUI_OK ) return ret;
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_CLOSE ) {
				(void)__xuiDockCloseWindow(pWidget, pData, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_PIN ) {
				(void)xuiDockPanelAutoHideWindow(pWidget, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_OPTION ) {
				(void)__xuiDockOpenPaneMenu(pWidget, pData, hit.iPane);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_PANE_OVERFLOW ) {
				(void)__xuiDockOpenOverflowMenu(pWidget, pData, hit.iPane);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_AUTO_HIDE ) {
				(void)xuiDockPanelExpandAutoHideWindow(pWidget, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_AUTO_HIDE_PIN ) {
				(void)xuiDockPanelDockAutoHideWindow(pWidget, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_AUTO_HIDE_CLOSE ) {
				(void)xuiDockPanelHideWindow(pWidget, hit.iWindow);
			} else if ( hit.iType == XUI_DOCK_PANEL_HIT_SPLITTER ) {
				pData->iDragType = XUI_DOCK_DRAG_SPLITTER;
				pData->iDragNode = -1;
				pData->iDragRegion = -1;
				pData->iDragSide = 0;
				pData->fDragStartX = lx;
				pData->fDragStartY = ly;
				if ( hit.iNode >= 0 ) {
					xui_dock_node_slot_t* n = __xuiDockNodeAt(pData, hit.iNode);
					pData->iDragNode = hit.iNode;
					pData->fDragStartRatio = (n != NULL) ? n->fRatio : 0.5f;
				} else if ( __xuiDockRegionIsSide(hit.iRegion) ) {
					pData->iDragRegion = hit.iRegion;
					pData->fDragStartRatio = __xuiDockRegionCurrentSize(pData, hit.iRegion);
				}
				(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			}
			return XUI_EVENT_DISPATCH_STOP;
		} else if ( pData->iAutoHideExpandWindow >= 0 ) {
			if ( !__xuiDockRectContains(pData->tAutoHideExpandRect, lx, ly) ) {
				(void)xuiDockPanelCollapseAutoHide(pWidget);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		if ( (pData->iDragType == XUI_DOCK_DRAG_SPLITTER) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
			xui_dock_node_slot_t* n = __xuiDockNodeAt(pData, pData->iDragNode);
			if ( pData->iDragRegion >= 0 ) {
				float delta;
				if ( pData->iDragRegion == XUI_DOCK_PANEL_REGION_LEFT || pData->iDragRegion == XUI_DOCK_PANEL_REGION_RIGHT ) {
					delta = lx - pData->fDragStartX;
					if ( pData->iDragRegion == XUI_DOCK_PANEL_REGION_RIGHT ) delta = -delta;
				} else {
					delta = ly - pData->fDragStartY;
					if ( pData->iDragRegion == XUI_DOCK_PANEL_REGION_BOTTOM ) delta = -delta;
				}
				(void)__xuiDockSetRegionDragSize(pWidget, pData, pData->iDragRegion, pData->fDragStartRatio + delta);
			} else if ( n != NULL ) {
				if ( n->iOrientation == XUI_DOCK_ORIENTATION_VERTICAL ) {
					n->fRatio = __xuiDockClamp((lx - n->tRect.fX) / __xuiDockMax(1.0f, n->tRect.fW), 0.10f, 0.90f);
				} else {
					n->fRatio = __xuiDockClamp((ly - n->tRect.fY) / __xuiDockMax(1.0f, n->tRect.fH), 0.10f, 0.90f);
				}
				pData->iLayoutChangeCount++;
				pData->iChangeCount++;
				__xuiDockInvalidate(pWidget, 1);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( (pData->iDragType == XUI_DOCK_DRAG_DOCKED) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
			xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pData->iDragWindow);
			xui_dock_drop_info_t drop;
			xui_dock_drop_info_t indicator;
			xui_rect_t r;
			float dx;
			float dy;
			int ret;
			if ( w == NULL ) return XUI_EVENT_DISPATCH_STOP;
			dx = lx - pData->fDragStartX;
			dy = ly - pData->fDragStartY;
			if ( !pData->bDragFloating ) {
				xui_dock_pane_slot_t* pSourcePane;
				int insert;
				if ( __xuiDockAbs(dx) < XUI_DOCK_DRAG_THRESHOLD && __xuiDockAbs(dy) < XUI_DOCK_DRAG_THRESHOLD ) {
					return XUI_EVENT_DISPATCH_STOP;
				}
				pSourcePane = __xuiDockPaneAt(pData, pData->iDragPane);
				insert = ((pEvent->iModifiers & XUI_MOD_CTRL) == 0u) ? __xuiDockPaneTabInsertIndex(pData, pSourcePane, lx, ly) : -1;
				if ( pSourcePane != NULL && __xuiDockRectContains(pSourcePane->tTabStripRect, lx, ly) ) {
					if ( insert == pData->iDragSourceIndex || insert == pData->iDragSourceIndex + 1 ) insert = -1;
					ret = __xuiDockSetTabInsertPreview(pWidget, pData, pSourcePane, w->iWindow, insert);
					if ( ret != XUI_OK ) return ret;
					__xuiDockInvalidate(pWidget, 0);
					return XUI_EVENT_DISPATCH_STOP;
				}
				r = pData->tDragStartRect;
				r.fX += dx;
				r.fY += dy;
				ret = xuiDockPanelFloatWindow(pWidget, w->iWindow, r);
				if ( ret != XUI_OK ) return ret;
				pData->bDragFloating = 1;
			}
			pData->iDragInsertIndex = -1;
			r = pData->tDragStartRect;
			r.fX += dx;
			r.fY += dy;
			w->tFloatRect = __xuiDockClampFloatRect(pWidget, r);
			if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0u ) {
				ret = __xuiDockSetDragPreview(pWidget, pData, NULL);
			} else {
				ret = __xuiDockFindDropTargetEx(pWidget, pData, w->iWindow, lx, ly, &drop, &indicator);
				if ( ret == XUI_OK ) ret = __xuiDockSetDragPreview(pWidget, pData, &drop);
				if ( ret == XUI_OK ) ret = __xuiDockSetDragIndicator(pWidget, pData, &indicator);
			}
			if ( ret != XUI_OK ) return ret;
			pData->iChangeCount++;
			pData->iLayoutChangeCount++;
			__xuiDockInvalidate(pWidget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		(void)__xuiDockHitLocal(pData, lx, ly, &hit);
		if ( (pData->iHoverType != hit.iType) || (pData->iHoverWindow != hit.iWindow) || (pData->iHoverPane != hit.iPane) || (pData->iHoverNode != hit.iNode) || (pData->iHoverRegion != hit.iRegion) ) {
			pData->iHoverType = hit.iType;
			pData->iHoverWindow = hit.iWindow;
			pData->iHoverPane = hit.iPane;
			pData->iHoverNode = hit.iNode;
			pData->iHoverRegion = hit.iRegion;
			__xuiDockInvalidate(pWidget, 0);
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( left && pData->iDragType != XUI_DOCK_DRAG_NONE ) {
			int ret = XUI_OK;
			if ( pData->iDragType == XUI_DOCK_DRAG_FLOAT || (pData->iDragType == XUI_DOCK_DRAG_DOCKED && pData->bDragFloating) ) {
				ret = __xuiDockCommitDropPreview(pWidget, pData);
			} else if ( pData->iDragType == XUI_DOCK_DRAG_DOCKED && pData->iDragInsertIndex >= 0 ) {
				ret = __xuiDockPaneMoveWindow(pWidget, pData, __xuiDockPaneAt(pData, pData->iDragPane), pData->iDragSourceIndex, pData->iDragInsertIndex);
			}
			pData->iDragType = XUI_DOCK_DRAG_NONE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = -1;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 0;
			(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			if ( ret != XUI_OK ) return ret;
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iDragType == XUI_DOCK_DRAG_NONE ) {
			pData->iHoverType = XUI_DOCK_PANEL_HIT_NONE;
			pData->iHoverWindow = -1;
			pData->iHoverPane = -1;
			pData->iHoverNode = -1;
			pData->iHoverRegion = -1;
			__xuiDockInvalidate(pWidget, 0);
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( pEvent->pTarget != pWidget ) break;
		if ( __xuiDockPointerTargetIsFloatingHost(pData, pEvent->pRelated) &&
		     (pData->iDragType == XUI_DOCK_DRAG_FLOAT || pData->iDragType == XUI_DOCK_DRAG_FLOAT_RESIZE) ) {
			break;
		}
		pData->iDragType = XUI_DOCK_DRAG_NONE;
		pData->iDragNode = -1;
		pData->iDragRegion = -1;
		pData->iDragPane = -1;
		pData->iDragWindow = -1;
		pData->iDragSourceIndex = -1;
		pData->iDragInsertIndex = -1;
		pData->iDragSide = 0;
		pData->bDragFloating = 0;
		(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
		break;
	case XUI_EVENT_KEY_DOWN:
		if ( (pEvent->iKey == XUI_KEY_ESCAPE) && (pData->iDragType != XUI_DOCK_DRAG_NONE) ) {
			pData->iDragType = XUI_DOCK_DRAG_NONE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = -1;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 0;
			(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		__xuiDockInvalidate(pWidget, 0);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static xui_rect_t __xuiDockClampFloatRect(xui_widget pPanel, xui_rect_t r)
{
	xui_rect_t pr = xuiWidgetGetRect(pPanel);
	float minW = 140.0f;
	float minH = 90.0f;
	r.fW = __xuiDockMax(minW, r.fW);
	r.fH = __xuiDockMax(minH, r.fH);
	if ( r.fW > pr.fW ) r.fW = pr.fW;
	if ( r.fH > pr.fH ) r.fH = pr.fH;
	r.fX = __xuiDockClamp(r.fX, 0.0f, __xuiDockMax(0.0f, pr.fW - r.fW));
	r.fY = __xuiDockClamp(r.fY, 0.0f, __xuiDockMax(0.0f, pr.fH - r.fH));
	return xuiInternalSnapRect(r);
}

static int __xuiDockFloatResizeSide(xui_dock_panel_data_t* pData, xui_rect_t r, float lx, float ly)
{
	float hit;
	int side;
	if ( (pData == NULL) || (r.fW <= 0.0f) || (r.fH <= 0.0f) ) return 0;
	hit = __xuiDockMax(5.0f, pData->tMetrics.fFloatBorderWidth + 5.0f);
	side = 0;
	if ( lx >= 0.0f && lx <= hit ) side |= XUI_DOCK_RESIZE_LEFT;
	if ( ly >= 0.0f && ly <= hit ) side |= XUI_DOCK_RESIZE_TOP;
	if ( lx >= r.fW - hit && lx < r.fW ) side |= XUI_DOCK_RESIZE_RIGHT;
	if ( ly >= r.fH - hit && ly < r.fH ) side |= XUI_DOCK_RESIZE_BOTTOM;
	return side;
}

static xui_rect_t __xuiDockResizeFloatRect(xui_widget pPanel, xui_rect_t r, int iSide, float dx, float dy)
{
	xui_rect_t pr;
	float left;
	float top;
	float right;
	float bottom;
	float minW;
	float minH;
	if ( pPanel == NULL ) return r;
	pr = xuiWidgetGetRect(pPanel);
	minW = 140.0f;
	minH = 90.0f;
	left = r.fX;
	top = r.fY;
	right = r.fX + r.fW;
	bottom = r.fY + r.fH;
	if ( (iSide & XUI_DOCK_RESIZE_LEFT) != 0 ) left = __xuiDockClamp(left + dx, 0.0f, right - minW);
	if ( (iSide & XUI_DOCK_RESIZE_TOP) != 0 ) top = __xuiDockClamp(top + dy, 0.0f, bottom - minH);
	if ( (iSide & XUI_DOCK_RESIZE_RIGHT) != 0 ) right = __xuiDockClamp(right + dx, left + minW, pr.fW);
	if ( (iSide & XUI_DOCK_RESIZE_BOTTOM) != 0 ) bottom = __xuiDockClamp(bottom + dy, top + minH, pr.fH);
	return xuiInternalSnapRect(__xuiDockRect(left, top, right - left, bottom - top));
}

static int __xuiDockHostEvent(xui_widget pHost, const xui_event_t* pEvent, void* pUser)
{
	xui_dock_window_slot_t* w = (xui_dock_window_slot_t*)pUser;
	xui_dock_panel_data_t* pData;
	xui_rect_t world;
	xui_rect_t title;
	xui_rect_t close;
	int resizeSide;
	float lx;
	float ly;
	int left;
	if ( (pHost == NULL) || (pEvent == NULL) || (w == NULL) || (w->pOwner == NULL) || (w->iState != XUI_DOCK_PANEL_WINDOW_FLOATING) ) return XUI_OK;
	pData = w->pOwner;
	world = xuiWidgetGetWorldRect(pHost);
	lx = pEvent->fX - world.fX;
	ly = pEvent->fY - world.fY;
	title = __xuiDockRect(0.0f, 0.0f, world.fW, pData->tMetrics.fFloatTitleHeight);
	close = __xuiDockRect(world.fW - pData->tMetrics.fFloatTitleHeight, 0.0f, pData->tMetrics.fFloatTitleHeight, pData->tMetrics.fFloatTitleHeight);
	resizeSide = __xuiDockFloatResizeSide(pData, __xuiDockRect(0.0f, 0.0f, world.fW, world.fH), lx, ly);
	left = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		if ( left ) {
			(void)__xuiDockFloatOrderBringToFront(pData, w->iWindow);
			(void)__xuiDockRequestFocusWindow(w->pPanelWidget, pData, w->iWindow);
			__xuiDockInvalidate(w->pPanelWidget, 1);
		}
		if ( left && __xuiDockRectContains(close, lx, ly) ) {
			(void)__xuiDockCloseWindow(w->pPanelWidget, pData, w->iWindow);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( left && __xuiDockRectContains(title, lx, ly) ) {
			pData->iDragType = XUI_DOCK_DRAG_FLOAT;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = w->iWindow;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 1;
			pData->fDragStartX = pEvent->fX;
			pData->fDragStartY = pEvent->fY;
			pData->tDragStartRect = w->tFloatRect;
			(void)__xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pHost), pHost);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( left && resizeSide != 0 ) {
			pData->iDragType = XUI_DOCK_DRAG_FLOAT_RESIZE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = w->iWindow;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = resizeSide;
			pData->bDragFloating = 1;
			pData->fDragStartX = pEvent->fX;
			pData->fDragStartY = pEvent->fY;
			pData->tDragStartRect = w->tFloatRect;
			(void)__xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pHost), pHost);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		if ( (pData->iDragType == XUI_DOCK_DRAG_FLOAT_RESIZE) && (pData->iDragWindow == w->iWindow) && (xuiGetPointerCapture(xuiWidgetGetContext(pHost)) == pHost) ) {
			w->tFloatRect = __xuiDockResizeFloatRect(w->pPanelWidget, pData->tDragStartRect, pData->iDragSide, pEvent->fX - pData->fDragStartX, pEvent->fY - pData->fDragStartY);
			(void)__xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
			pData->iChangeCount++;
			pData->iLayoutChangeCount++;
			__xuiDockInvalidate(w->pPanelWidget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( (pData->iDragType == XUI_DOCK_DRAG_FLOAT) && (pData->iDragWindow == w->iWindow) && (xuiGetPointerCapture(xuiWidgetGetContext(pHost)) == pHost) ) {
			xui_rect_t r = pData->tDragStartRect;
			xui_rect_t panelWorld;
			xui_dock_drop_info_t drop;
			xui_dock_drop_info_t indicator;
			int ret;
			r.fX += pEvent->fX - pData->fDragStartX;
			r.fY += pEvent->fY - pData->fDragStartY;
			w->tFloatRect = __xuiDockClampFloatRect(w->pPanelWidget, r);
			panelWorld = xuiWidgetGetWorldRect(w->pPanelWidget);
			if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0u ) {
				ret = __xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
			} else {
				ret = __xuiDockFindDropTargetEx(w->pPanelWidget, pData, w->iWindow, pEvent->fX - panelWorld.fX, pEvent->fY - panelWorld.fY, &drop, &indicator);
				if ( ret == XUI_OK ) ret = __xuiDockSetDragPreview(w->pPanelWidget, pData, &drop);
				if ( ret == XUI_OK ) ret = __xuiDockSetDragIndicator(w->pPanelWidget, pData, &indicator);
			}
			if ( ret != XUI_OK ) return ret;
			pData->iChangeCount++;
			pData->iLayoutChangeCount++;
			__xuiDockInvalidate(w->pPanelWidget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( left && (pData->iDragType == XUI_DOCK_DRAG_FLOAT_RESIZE) && (pData->iDragWindow == w->iWindow) ) {
			pData->iDragType = XUI_DOCK_DRAG_NONE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = -1;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 0;
			(void)__xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pHost), pHost);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( left && (pData->iDragType == XUI_DOCK_DRAG_FLOAT) && (pData->iDragWindow == w->iWindow) ) {
			int ret;
			ret = __xuiDockCommitDropPreview(w->pPanelWidget, pData);
			pData->iDragType = XUI_DOCK_DRAG_NONE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = -1;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 0;
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pHost), pHost);
			if ( ret != XUI_OK ) return ret;
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( (pEvent->pTarget == pHost) && (pData->iDragWindow == w->iWindow) &&
		     (pData->iDragType == XUI_DOCK_DRAG_FLOAT || pData->iDragType == XUI_DOCK_DRAG_FLOAT_RESIZE) ) {
			pData->iDragType = XUI_DOCK_DRAG_NONE;
			pData->iDragNode = -1;
			pData->iDragRegion = -1;
			pData->iDragPane = -1;
			pData->iDragWindow = -1;
			pData->iDragSourceIndex = -1;
			pData->iDragInsertIndex = -1;
			pData->iDragSide = 0;
			pData->bDragFloating = 0;
			(void)__xuiDockSetDragPreview(w->pPanelWidget, pData, NULL);
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiDockHostTooltipResolve(xui_context pContext, xui_widget pHost, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_dock_window_slot_t* w = (xui_dock_window_slot_t*)pUser;
	xui_dock_panel_data_t* pData;
	xui_rect_t world;
	xui_rect_t title;
	xui_rect_t close;
	float lx;
	float ly;
	if ( (pContext == NULL) || (pHost == NULL) || (pDesc == NULL) || (w == NULL) || (w->pOwner == NULL) ) return 0;
	if ( w->iState != XUI_DOCK_PANEL_WINDOW_FLOATING ) return 0;
	pData = w->pOwner;
	world = xuiWidgetGetWorldRect(pHost);
	lx = pContext->fTooltipMouseX - world.fX;
	ly = pContext->fTooltipMouseY - world.fY;
	title = __xuiDockRect(0.0f, 0.0f, world.fW, pData->tMetrics.fFloatTitleHeight);
	close = __xuiDockRect(world.fW - pData->tMetrics.fFloatTitleHeight, 0.0f, pData->tMetrics.fFloatTitleHeight, pData->tMetrics.fFloatTitleHeight);
	if ( __xuiDockRectContains(close, lx, ly) ) return __xuiDockTooltipFill(pDesc, __xuiDockTooltipTextForId(pContext, pData, XUI_DOCK_PANEL_TOOLTIP_CLOSE));
	if ( __xuiDockRectContains(title, lx, ly) ) return __xuiDockTooltipFill(pDesc, w->sTitle);
	return 0;
}

static int __xuiDockHostRender(xui_widget pHost, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_dock_window_slot_t* w = (xui_dock_window_slot_t*)pUser;
	xui_dock_panel_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t r;
	xui_rect_t title;
	xui_rect_t close;
	int ret;
	(void)iStateId;
	if ( (pHost == NULL) || (pDraw == NULL) || (w == NULL) || (w->pOwner == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( w->iState != XUI_DOCK_PANEL_WINDOW_FLOATING ) return XUI_OK;
	pData = w->pOwner;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pHost));
	r = xuiWidgetGetContentRect(pHost);
	r.fX = 0.0f;
	r.fY = 0.0f;
	ret = __xuiDockDrawRectFill(pProxy, pDraw, r, pData->tColors.iClientColor);
	if ( ret != XUI_OK ) return ret;
	title = __xuiDockRect(0.0f, 0.0f, r.fW, pData->tMetrics.fFloatTitleHeight);
	ret = __xuiDockDrawFill(pProxy, pDraw, title, pData->tColors.iFloatTitleColor);
	if ( ret != XUI_OK ) return ret;
	close = __xuiDockRect(r.fW - pData->tMetrics.fFloatTitleHeight, 0.0f, pData->tMetrics.fFloatTitleHeight, pData->tMetrics.fFloatTitleHeight);
	ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, w->sTitle, __xuiDockRect(8.0f, 0.0f, __xuiDockMax(0.0f, r.fW - pData->tMetrics.fFloatTitleHeight - 14.0f), pData->tMetrics.fFloatTitleHeight), pData->tColors.iActiveCaptionTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockDrawCloseGlyph(pProxy, pDraw, close, pData->tColors.iActiveCaptionTextColor);
	if ( ret != XUI_OK ) return ret;
	if ( r.fW >= 170.0f && r.fH >= 110.0f ) {
		float x0 = r.fX + r.fW - 17.0f;
		float y0 = r.fY + r.fH - 6.0f;
		ret = __xuiDockDrawLine(pProxy, pDraw, x0, y0, r.fX + r.fW - 6.0f, r.fY + r.fH - 17.0f, 1.0f, pData->tColors.iBorderColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawLine(pProxy, pDraw, x0 + 5.0f, y0, r.fX + r.fW - 6.0f, r.fY + r.fH - 12.0f, 1.0f, pData->tColors.iBorderColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawLine(pProxy, pDraw, x0 + 10.0f, y0, r.fX + r.fW - 6.0f, r.fY + r.fH - 7.0f, 1.0f, pData->tColors.iBorderColor);
		if ( ret != XUI_OK ) return ret;
	}
	return __xuiDockDrawRectStroke(pProxy, pDraw, r, pData->tMetrics.fFloatBorderWidth, pData->tColors.iFloatBorderColor);
}

static int __xuiDockDrawPane(xui_widget pWidget, xui_draw_context pDraw, xui_dock_panel_data_t* pData, xui_dock_pane_slot_t* pPane, xui_proxy pProxy)
{
	xui_dock_panel_colors_t* c = &pData->tColors;
	xui_dock_panel_metrics_t* m = &pData->tMetrics;
	xui_rect_t activeTab;
	xui_rect_t line;
	int activeWindow;
	int bTabs;
	int ret;
	int i;
	if ( (pPane == NULL) || !pPane->bUsed ) return XUI_OK;
	activeWindow = __xuiDockPaneActiveWindow(pPane);
	bTabs = __xuiDockPaneUsesTabs(pPane);
	ret = __xuiDockDrawFill(pProxy, pDraw, pPane->tRect, c->iPaneColor);
	if ( ret != XUI_OK ) return ret;
	if ( pPane->tClientRect.fW > 0.0f && pPane->tClientRect.fH > 0.0f ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, pPane->tClientRect, c->iClientColor);
		if ( ret != XUI_OK ) return ret;
	}
	if ( !bTabs && __xuiDockRectRenderable(pPane->tCaptionRect) ) {
		xui_dock_window_slot_t* active = __xuiDockWindowAt(pData, activeWindow);
		xui_rect_t caption = pPane->tCaptionRect;
		float textRight = caption.fX + caption.fW - 8.0f;
		if ( pPane->tOptionRect.fW > 0.0f && pPane->tOptionRect.fX < textRight ) textRight = pPane->tOptionRect.fX - 4.0f;
		if ( pPane->tPinRect.fW > 0.0f && pPane->tPinRect.fX < textRight ) textRight = pPane->tPinRect.fX - 4.0f;
		if ( pPane->tCloseRect.fW > 0.0f && pPane->tCloseRect.fX < textRight ) textRight = pPane->tCloseRect.fX - 4.0f;
		ret = __xuiDockDrawFill(pProxy, pDraw, caption, c->iCaptionColor);
		if ( ret != XUI_OK ) return ret;
		if ( active != NULL ) {
			ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, active->sTitle,
				__xuiDockRect(caption.fX + 8.0f, caption.fY, __xuiDockMax(0.0f, textRight - caption.fX - 8.0f), caption.fH),
				c->iCaptionTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( ret != XUI_OK ) return ret;
		}
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(caption.fX, caption.fY + caption.fH - 1.0f, caption.fW, 1.0f), c->iBorderColor);
		if ( ret != XUI_OK ) return ret;
	}
	if ( __xuiDockRectRenderable(pPane->tTabStripRect) ) {
		ret = __xuiDockDrawFill(pProxy, pDraw, pPane->tTabStripRect, c->iPaneColor);
		if ( ret != XUI_OK ) return ret;
	}
	activeTab = __xuiDockRect(0.0f, 0.0f, 0.0f, 0.0f);
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		if ( (w != NULL) && (w->iWindow == activeWindow) && __xuiDockRectRenderable(w->tTabRect) ) {
			activeTab = w->tTabRect;
			activeTab.fY -= 2.0f;
			activeTab.fH += 2.0f;
			activeTab = xuiInternalSnapRect(activeTab);
			break;
		}
	}
	if ( __xuiDockRectRenderable(pPane->tTabStripRect) ) {
		line = __xuiDockRect(pPane->tTabStripRect.fX, pPane->tTabStripRect.fY + pPane->tTabStripRect.fH - 1.0f, pPane->tTabStripRect.fW, 1.0f);
		if ( __xuiDockRectRenderable(activeTab) ) {
			if ( activeTab.fX > pPane->tTabStripRect.fX ) {
				ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(pPane->tTabStripRect.fX, line.fY, activeTab.fX - pPane->tTabStripRect.fX, 1.0f), c->iBorderColor);
				if ( ret != XUI_OK ) return ret;
			}
			if ( activeTab.fX + activeTab.fW < pPane->tTabStripRect.fX + pPane->tTabStripRect.fW ) {
				ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(activeTab.fX + activeTab.fW, line.fY, pPane->tTabStripRect.fX + pPane->tTabStripRect.fW - (activeTab.fX + activeTab.fW), 1.0f), c->iBorderColor);
				if ( ret != XUI_OK ) return ret;
			}
		} else {
			ret = __xuiDockDrawFill(pProxy, pDraw, line, c->iBorderColor);
			if ( ret != XUI_OK ) return ret;
		}
	}
	for ( i = 0; i < pPane->iWindowCount; i++ ) {
		xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, pPane->arrWindows[i]);
		xui_rect_t tab;
		uint32_t fill;
		uint32_t border;
		uint32_t text;
		xui_rect_t close;
		float textRight;
		if ( w == NULL || w->tTabRect.fW <= 0.0f || w->tTabRect.fH <= 0.0f ) continue;
		if ( w->iWindow == activeWindow ) continue;
		tab = w->tTabRect;
		fill = ((pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_TAB || pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE) && pData->iHoverWindow == w->iWindow) ? c->iTabHoverColor : c->iTabColor;
		border = c->iBorderColor;
		text = c->iTabTextColor;
		if ( !w->bDockable ) {
			border = XUI_COLOR_RGBA(150, 174, 198, 155);
			text = XUI_COLOR_RGBA(112, 132, 154, 185);
		}
		ret = __xuiDockDrawFill(pProxy, pDraw, tab, fill);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawEdgeRect(pProxy, pDraw, tab, border, 1, 1, 1, 1);
		if ( ret != XUI_OK ) return ret;
		close = w->tTabCloseRect;
		textRight = tab.fX + tab.fW - m->fTabPaddingX;
		if ( __xuiDockRectRenderable(close) && close.fX < textRight ) textRight = close.fX - 3.0f;
		ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, w->sTitle, __xuiDockRect(tab.fX + m->fTabPaddingX, tab.fY, __xuiDockMax(0.0f, textRight - tab.fX - m->fTabPaddingX), tab.fH), text, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawTabCloseButton(pProxy, pDraw, pData, close, text, pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE && pData->iHoverWindow == w->iWindow);
		if ( ret != XUI_OK ) return ret;
	}
	if ( __xuiDockRectRenderable(activeTab) ) {
		xui_dock_window_slot_t* active = __xuiDockWindowAt(pData, activeWindow);
		ret = __xuiDockDrawFill(pProxy, pDraw, activeTab, c->iActiveTabColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawEdgeRect(pProxy, pDraw, activeTab, c->iBorderColor, 1, 1, 1, 0);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockRect(activeTab.fX + 1.0f, activeTab.fY + 1.0f, __xuiDockMax(0.0f, activeTab.fW - 2.0f), 2.0f), XUI_COLOR_RGBA(238, 126, 24, 255));
		if ( ret != XUI_OK ) return ret;
		if ( active != NULL ) {
			xui_rect_t close = active->tTabCloseRect;
			float textRight = activeTab.fX + activeTab.fW - m->fTabPaddingX;
			if ( __xuiDockRectRenderable(close) && close.fX < textRight ) textRight = close.fX - 3.0f;
			ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, active->sTitle, __xuiDockRect(activeTab.fX + m->fTabPaddingX, activeTab.fY + 2.0f, __xuiDockMax(0.0f, textRight - activeTab.fX - m->fTabPaddingX), __xuiDockMax(0.0f, activeTab.fH - 2.0f)), c->iActiveTabTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( ret != XUI_OK ) return ret;
			ret = __xuiDockDrawTabCloseButton(pProxy, pDraw, pData, close, c->iActiveTabTextColor, pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE && pData->iHoverWindow == active->iWindow);
			if ( ret != XUI_OK ) return ret;
		}
	}
	if ( pPane->bOverflow && pPane->tOverflowRect.fW > 0.0f && pPane->tOverflowRect.fH > 0.0f ) {
		ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pPane->tOverflowRect, "dock_pane_option_overflow", pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_OVERFLOW && pData->iHoverPane == pPane->iPane, 1);
		if ( ret != XUI_OK ) return ret;
	}
	ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pPane->tCloseRect, "dock_pane_close", pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_CLOSE && pData->iHoverPane == pPane->iPane, 1);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pPane->tPinRect, "dock_pane_auto_hide", pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_PIN && pData->iHoverPane == pPane->iPane, __xuiDockPaneCanAutoHide(pData, pPane));
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pPane->tOptionRect, "dock_pane_option", pData->iHoverType == XUI_DOCK_PANEL_HIT_PANE_OPTION && pData->iHoverPane == pPane->iPane, 1);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockDrawStroke(pProxy, pDraw, pPane->tRect, m->fBorderWidth, c->iBorderColor);
	return ret;
}

static int __xuiDockDrawAutoHide(xui_widget pWidget, xui_draw_context pDraw, xui_dock_panel_data_t* pData, xui_proxy pProxy)
{
	xui_dock_window_slot_t* pExpand;
	int i;
	int ret;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* w = &pData->arrWindows[i];
		uint32_t fill;
		uint32_t border;
		xui_rect_t icon;
		xui_rect_t text;
		int hot;
		if ( !w->bUsed || w->iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) continue;
		hot = (pData->iHoverType == XUI_DOCK_PANEL_HIT_AUTO_HIDE && pData->iHoverWindow == i);
		fill = hot ? pData->tColors.iAutoHideHoverColor : pData->tColors.iAutoHideColor;
		border = hot ? XUI_COLOR_RGBA(229, 195, 101, 255) : pData->tColors.iBorderColor;
		ret = __xuiDockDrawFill(pProxy, pDraw, w->tAutoHideRect, fill);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, w->tAutoHideRect, 1.0f, border);
		if ( ret != XUI_OK ) return ret;
		icon = __xuiDockRect(w->tAutoHideRect.fX + 3.0f, w->tAutoHideRect.fY + 3.0f, 16.0f, 15.0f);
		if ( icon.fX + icon.fW <= w->tAutoHideRect.fX + w->tAutoHideRect.fW && icon.fY + icon.fH <= w->tAutoHideRect.fY + w->tAutoHideRect.fH ) {
			ret = __xuiDockDrawBuiltinAsset(pWidget, pProxy, pDraw, "dock_pane_dock", icon, XUI_COLOR_WHITE);
			if ( ret != XUI_OK ) ret = __xuiDockDrawPaneIconFallback(pProxy, pDraw, "dock_pane_dock", icon, pData->tColors.iButtonColor);
			if ( ret != XUI_OK ) return ret;
		}
		text = __xuiDockRect(w->tAutoHideRect.fX + 22.0f, w->tAutoHideRect.fY, __xuiDockMax(0.0f, w->tAutoHideRect.fW - 26.0f), w->tAutoHideRect.fH);
		if ( text.fW > 8.0f ) {
			ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, w->sTitle, text, pData->tColors.iTabTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( ret != XUI_OK ) return ret;
		}
	}
	pExpand = __xuiDockWindowAt(pData, pData->iAutoHideExpandWindow);
	if ( pExpand != NULL && pExpand->iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE && pData->tAutoHideExpandRect.fW > 0.0f && pData->tAutoHideExpandRect.fH > 0.0f ) {
		xui_rect_t caption;
		ret = __xuiDockDrawFill(pProxy, pDraw, pData->tAutoHideExpandRect, pData->tColors.iPaneColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawFill(pProxy, pDraw, pData->tAutoHideExpandClientRect, pData->tColors.iClientColor);
		if ( ret != XUI_OK ) return ret;
		caption = __xuiDockRect(pData->tAutoHideExpandRect.fX, pData->tAutoHideExpandRect.fY, pData->tAutoHideExpandRect.fW, pData->tMetrics.fCaptionHeight);
		ret = __xuiDockDrawFill(pProxy, pDraw, caption, pData->tColors.iCaptionColor);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawText(pProxy, pDraw, pData->pFont, pExpand->sTitle,
			__xuiDockRect(pData->tAutoHideExpandRect.fX + 8.0f, pData->tAutoHideExpandRect.fY, __xuiDockMax(0.0f, pData->tAutoHidePinRect.fX - pData->tAutoHideExpandRect.fX - 12.0f), pData->tMetrics.fCaptionHeight),
			pData->tColors.iCaptionTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pData->tAutoHidePinRect, "dock_pane_dock", pData->iHoverType == XUI_DOCK_PANEL_HIT_AUTO_HIDE_PIN, 1);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawPaneButton(pWidget, pProxy, pDraw, pData, pData->tAutoHideCloseRect, "dock_pane_close", pData->iHoverType == XUI_DOCK_PANEL_HIT_AUTO_HIDE_CLOSE, 1);
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, pData->tAutoHideExpandRect, pData->tMetrics.fBorderWidth, XUI_COLOR_RGBA(103, 152, 179, 255));
		if ( ret != XUI_OK ) return ret;
	}
	return XUI_OK;
}

static int __xuiDockDrawDragPreview(xui_widget pWidget, xui_draw_context pDraw, xui_dock_panel_data_t* pData, xui_proxy pProxy, float fOffsetX, float fOffsetY)
{
	xui_rect_t r;
	xui_dock_drop_info_t indicator;
	int bHasPreview;
	int ret;
	if ( pData == NULL ) return XUI_OK;
	bHasPreview = pData->tDragPreview.bValid && pData->tDragPreview.tRect.fW > 0.0f && pData->tDragPreview.tRect.fH > 0.0f;
	if ( !bHasPreview && !pData->tDragIndicator.bValid ) return XUI_OK;
	if ( bHasPreview ) {
		r = __xuiDockOffsetRect(pData->tDragPreview.tRect, fOffsetX, fOffsetY);
		if ( pData->iDragInsertIndex >= 0 ) {
			ret = __xuiDockDrawFill(pProxy, pDraw, __xuiDockInset(r, -1.0f, 0.0f), XUI_COLOR_RGBA(255, 255, 255, 210));
			if ( ret != XUI_OK ) return ret;
			return __xuiDockDrawFill(pProxy, pDraw, r, pData->tColors.iFocusColor);
		}
		ret = __xuiDockDrawFill(pProxy, pDraw, r, XUI_COLOR_RGBA(47, 125, 214, 54));
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, r, 2.0f, XUI_COLOR_RGBA(47, 125, 214, 220));
		if ( ret != XUI_OK ) return ret;
		ret = __xuiDockDrawStroke(pProxy, pDraw, __xuiDockInset(r, 3.0f, 3.0f), 1.0f, XUI_COLOR_RGBA(255, 255, 255, 150));
		if ( ret != XUI_OK ) return ret;
	}
	indicator = pData->tDragIndicator;
	if ( !indicator.bValid && bHasPreview ) indicator = pData->tDragPreview;
	if ( pWidget != NULL && indicator.bValid ) {
		xui_rect_t target;
		xui_rect_t dst;
		const char* asset;
		if ( indicator.iPane >= 0 && indicator.iPane < XUI_DOCK_PANEL_PANE_CAPACITY && pData->arrPanes[indicator.iPane].bUsed ) {
			target = __xuiDockOffsetRect(pData->arrPanes[indicator.iPane].tRect, fOffsetX, fOffsetY);
			dst = __xuiDockRect(target.fX + (target.fW - 88.0f) * 0.5f, target.fY + (target.fH - 88.0f) * 0.5f, 88.0f, 88.0f);
			asset = __xuiDockPaneIndicatorAssetName(indicator.iSide);
			(void)__xuiDockDrawBuiltinAsset(pWidget, pProxy, pDraw, asset, dst, XUI_COLOR_WHITE);
		} else {
			target = __xuiDockOffsetRect(xuiWidgetGetContentRect(pWidget), fOffsetX, fOffsetY);
			switch ( indicator.iRegion ) {
			case XUI_DOCK_PANEL_REGION_LEFT:
				dst = __xuiDockRect(target.fX + 18.0f, target.fY + (target.fH - 29.0f) * 0.5f, 31.0f, 29.0f);
				break;
			case XUI_DOCK_PANEL_REGION_RIGHT:
				dst = __xuiDockRect(target.fX + target.fW - 49.0f, target.fY + (target.fH - 29.0f) * 0.5f, 31.0f, 29.0f);
				break;
			case XUI_DOCK_PANEL_REGION_TOP:
				dst = __xuiDockRect(target.fX + (target.fW - 29.0f) * 0.5f, target.fY + 18.0f, 29.0f, 32.0f);
				break;
			case XUI_DOCK_PANEL_REGION_BOTTOM:
				dst = __xuiDockRect(target.fX + (target.fW - 29.0f) * 0.5f, target.fY + target.fH - 49.0f, 29.0f, 31.0f);
				break;
			default:
				dst = __xuiDockRect(target.fX + (target.fW - 31.0f) * 0.5f, target.fY + (target.fH - 31.0f) * 0.5f, 31.0f, 31.0f);
				break;
			}
			asset = __xuiDockPanelIndicatorAssetName(indicator.iRegion);
			(void)__xuiDockDrawBuiltinAsset(pWidget, pProxy, pDraw, asset, dst, XUI_COLOR_WHITE);
		}
	}
	return XUI_OK;
}

static int __xuiDockCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_dock_panel_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t r;
	int i;
	int ret;
	(void)iStateId;
	pData = (xui_dock_panel_data_t*)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	r = xuiWidgetGetContentRect(pWidget);
	ret = __xuiDockDrawFill(pProxy, pDraw, r, pData->tColors.iBackgroundColor);
	if ( ret != XUI_OK ) return ret;
	for ( i = 0; i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		if ( pData->arrRegions[i].iRootNode >= 0 ) {
			ret = __xuiDockDrawFill(pProxy, pDraw, pData->arrRegions[i].tRect, pData->tColors.iPaneColor);
			if ( ret != XUI_OK ) return ret;
		}
		if ( pData->arrRegions[i].tSplitterRect.fW > 0.0f && pData->arrRegions[i].tSplitterRect.fH > 0.0f ) {
			uint32_t color = (pData->iDragType == XUI_DOCK_DRAG_SPLITTER && pData->iDragRegion == i) ? pData->tColors.iSplitterActiveColor :
				((pData->iHoverType == XUI_DOCK_PANEL_HIT_SPLITTER && pData->iHoverRegion == i) ? pData->tColors.iSplitterHoverColor : pData->tColors.iSplitterColor);
			ret = __xuiDockDrawFill(pProxy, pDraw, pData->arrRegions[i].tSplitterRect, color);
			if ( ret != XUI_OK ) return ret;
		}
	}
	for ( i = 0; i < XUI_DOCK_PANEL_NODE_CAPACITY; i++ ) {
		xui_dock_node_slot_t* n = &pData->arrNodes[i];
		uint32_t color;
		if ( !n->bUsed || n->iType != XUI_DOCK_NODE_SPLIT ) continue;
		color = (pData->iDragType == XUI_DOCK_DRAG_SPLITTER && pData->iDragNode == i) ? pData->tColors.iSplitterActiveColor :
			((pData->iHoverType == XUI_DOCK_PANEL_HIT_SPLITTER && pData->iHoverNode == i) ? pData->tColors.iSplitterHoverColor : pData->tColors.iSplitterColor);
		ret = __xuiDockDrawFill(pProxy, pDraw, n->tSplitterRect, color);
		if ( ret != XUI_OK ) return ret;
	}
	for ( i = 0; i < XUI_DOCK_PANEL_PANE_CAPACITY; i++ ) {
		ret = __xuiDockDrawPane(pWidget, pDraw, pData, &pData->arrPanes[i], pProxy);
		if ( ret != XUI_OK ) return ret;
	}
	ret = __xuiDockDrawAutoHide(pWidget, pDraw, pData, pProxy);
	if ( ret != XUI_OK ) return ret;
	return XUI_OK;
}

static int __xuiDockDragOverlayRender(xui_widget pOverlay, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_dock_panel_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t r;
	(void)iStateId;
	pWidget = (xui_widget)pUser;
	if ( (pOverlay == NULL) || (pDraw == NULL) || (pWidget == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	r = xuiWidgetGetRect(pOverlay);
	return __xuiDockDrawDragPreview(pWidget, pDraw, pData, pProxy, r.fX, r.fY);
}

static void __xuiDockDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FILL;
	pLayout->iHeightMode = XUI_SIZE_FILL;
	pLayout->iFlowMode = XUI_FLOW_ABSOLUTE;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_STRETCH;
	pLayout->iAlignY = XUI_ALIGN_STRETCH;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 640.0f;
	pLayout->fPreferredHeight = 420.0f;
	pLayout->fMinWidth = 240.0f;
	pLayout->fMinHeight = 160.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fGrow = 1.0f;
	pLayout->fShrink = 1.0f;
}

static void __xuiDockDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiDockPanelInitEvents(xui_widget pWidget)
{
	int ret;
	ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiDockPanelEvent, NULL);
	if ( ret == XUI_OK ) ret = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiDockPanelEvent, NULL);
	return ret;
}

static int __xuiDockPanelInitMenus(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	xui_menu_desc_t desc;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pOwner = pWidget;
	desc.pFont = pData->pFont;
	ret = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pOptionMenu, &desc);
	if ( ret != XUI_OK ) return ret;
	ret = xuiMenuSetSelect(pData->pOptionMenu, __xuiDockMenuSelect, pWidget);
	if ( ret == XUI_OK ) ret = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pOverflowMenu, &desc);
	if ( ret == XUI_OK ) ret = xuiMenuSetSelect(pData->pOverflowMenu, __xuiDockMenuSelect, pWidget);
	if ( ret != XUI_OK ) {
		xuiWidgetDestroy(pData->pOptionMenu);
		xuiWidgetDestroy(pData->pOverflowMenu);
		pData->pOptionMenu = NULL;
		pData->pOverflowMenu = NULL;
		return ret;
	}
	return XUI_OK;
}

static int __xuiDockPanelInitDragOverlay(xui_widget pWidget, xui_dock_panel_data_t* pData)
{
	xui_cache_policy_t policy;
	xui_widget pOverlay;
	int ret;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pDragOverlayWidget != NULL ) return XUI_OK;
	pOverlay = NULL;
	ret = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pOverlay);
	if ( ret != XUI_OK ) return ret;
	__xuiDockDefaultCachePolicy(&policy);
	(void)xuiWidgetSetLayoutType(pOverlay, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pOverlay, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pOverlay, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pOverlay, 0);
	(void)xuiWidgetSetTabStop(pOverlay, 0);
	(void)xuiWidgetSetEnabled(pOverlay, 0);
	(void)xuiWidgetSetHitTestVisible(pOverlay, 0);
	(void)xuiWidgetSetLayer(pOverlay, XUI_LAYER_DRAG, XUI_WINDOW_Z_TOPMOST + 200);
	(void)xuiWidgetSetCachePolicy(pOverlay, &policy);
	(void)xuiWidgetSetCacheRenderCallback(pOverlay, __xuiDockDragOverlayRender, pWidget);
	(void)xuiWidgetSetVisible(pOverlay, 0);
	ret = xuiWidgetAddChild(pWidget, pOverlay);
	if ( ret != XUI_OK ) {
		xuiWidgetDestroy(pOverlay);
		return ret;
	}
	pData->pDragOverlayWidget = pOverlay;
	return XUI_OK;
}

static int __xuiDockPanelInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_dock_panel_data_t* pData;
	int ret;
	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_dock_panel_data_t*)pTypeData;
	__xuiDockDefaults(pData);
	__xuiDockApplyDesc(pData, (const xui_dock_panel_desc_t*)pCreateData);
	if ( pData->pFont == NULL ) pData->pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	ret = __xuiDockPanelInitMenus(pWidget, pData);
	if ( ret == XUI_OK ) ret = __xuiDockPanelInitDragOverlay(pWidget, pData);
	if ( ret == XUI_OK ) ret = __xuiDockPanelInitEvents(pWidget);
	if ( ret == XUI_OK ) ret = xuiWidgetSetTooltipResolver(pWidget, __xuiDockTooltipResolve, NULL);
	return ret;
}

static void __xuiDockPanelDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_dock_panel_data_t* pData = (xui_dock_panel_data_t*)pTypeData;
	int i;
	(void)pWidget;
	(void)pUser;
	if ( pData != NULL ) {
		xuiWidgetDestroy(pData->pOptionMenu);
		xuiWidgetDestroy(pData->pOverflowMenu);
		/* The drag overlay is an owned child and has already been destroyed by xuiWidgetDestroy(). */
		pData->pDragOverlayWidget = NULL;
		for ( i = 0; i < XUI_DOCK_PANEL_MENU_TITLE_COUNT; ++i ) {
			xrtFree(pData->arrMenuTitle[i]);
			pData->arrMenuTitle[i] = NULL;
		}
		for ( i = 0; i < XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT; ++i ) {
			xrtFree(pData->arrTooltipText[i]);
			pData->arrTooltipText[i] = NULL;
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static xui_dock_panel_data_t* __xuiDockPanelGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;
	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "dockpanel");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_dock_panel_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiDockPanelGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t desc;
	xui_layout_t layout;
	xui_cache_policy_t policy;
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "dockpanel");
	if ( pType != NULL ) return pType;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.sName = "dockpanel";
	desc.pParent = xuiWidgetGetBaseType();
	desc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	desc.iTypeDataSize = sizeof(xui_dock_panel_data_t);
	desc.onInit = __xuiDockPanelInit;
	desc.onDestroy = __xuiDockPanelDestroy;
	desc.onLayoutArrange = __xuiDockPanelArrange;
	desc.onCacheRender = __xuiDockCacheRender;
	__xuiDockDefaultLayout(&layout);
	__xuiDockDefaultCachePolicy(&policy);
	desc.tLayout = layout;
	desc.tCachePolicy = policy;
	if ( xuiWidgetRegisterType(pContext, &pType, &desc) != XUI_OK ) return NULL;
	return pType;
}

XUI_API int xuiDockPanelCreate(xui_context pContext, xui_widget* ppWidget, const xui_dock_panel_desc_t* pDesc)
{
	xui_widget_type pType;
	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiDockPanelGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiDockPanelClear(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_font pFont;
	xui_dock_panel_metrics_t metrics;
	xui_dock_panel_colors_t colors;
	xui_dock_window_state_proc onStateChanged;
	void* pStateUser;
	xui_dock_active_proc onActiveChanged;
	void* pActiveUser;
	xui_dock_window_close_proc onClose;
	void* pCloseUser;
	xui_widget pOptionMenu;
	xui_widget pOverflowMenu;
	xui_widget pDragOverlayWidget;
	char* arrMenuTitle[XUI_DOCK_PANEL_MENU_TITLE_COUNT];
	char* arrTooltipText[XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT];
	int i;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pFont = pData->pFont;
	metrics = pData->tMetrics;
	colors = pData->tColors;
	onStateChanged = pData->onStateChanged;
	pStateUser = pData->pStateUser;
	onActiveChanged = pData->onActiveChanged;
	pActiveUser = pData->pActiveUser;
	onClose = pData->onClose;
	pCloseUser = pData->pCloseUser;
	pOptionMenu = pData->pOptionMenu;
	pOverflowMenu = pData->pOverflowMenu;
	pDragOverlayWidget = pData->pDragOverlayWidget;
	memcpy(arrMenuTitle, pData->arrMenuTitle, sizeof(arrMenuTitle));
	memcpy(arrTooltipText, pData->arrTooltipText, sizeof(arrTooltipText));
	(void)xuiMenuClose(pOptionMenu);
	(void)xuiMenuClose(pOverflowMenu);
	(void)xuiMenuClear(pOptionMenu);
	(void)xuiMenuClear(pOverflowMenu);
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed ) {
			if ( pData->arrWindows[i].pClientWidget != NULL && xuiWidgetGetParent(pData->arrWindows[i].pClientWidget) == pData->arrWindows[i].pHostWidget ) {
				__xuiDockSetWidgetTreeLayer(pData->arrWindows[i].pClientWidget, XUI_LAYER_NORMAL, 0);
				(void)xuiWidgetRemoveFromParent(pData->arrWindows[i].pClientWidget);
			}
			xuiWidgetDestroy(pData->arrWindows[i].pHostWidget);
		}
	}
	__xuiDockDefaults(pData);
	pData->pFont = pFont;
	pData->tMetrics = metrics;
	pData->tColors = colors;
	pData->onStateChanged = onStateChanged;
	pData->pStateUser = pStateUser;
	pData->onActiveChanged = onActiveChanged;
	pData->pActiveUser = pActiveUser;
	pData->onClose = onClose;
	pData->pCloseUser = pCloseUser;
	pData->pOptionMenu = pOptionMenu;
	pData->pOverflowMenu = pOverflowMenu;
	pData->pDragOverlayWidget = pDragOverlayWidget;
	memcpy(pData->arrMenuTitle, arrMenuTitle, sizeof(pData->arrMenuTitle));
	memcpy(pData->arrTooltipText, arrTooltipText, sizeof(pData->arrTooltipText));
	(void)__xuiDockSetDragPreview(pWidget, pData, NULL);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelAddWindow(xui_widget pWidget, const char* sTitle, xui_widget pClient, int* pWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w;
	xui_widget host = NULL;
	xui_cache_policy_t policy;
	int i;
	int ret;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( !pData->arrWindows[i].bUsed ) break;
	}
	if ( i >= XUI_DOCK_PANEL_WINDOW_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	ret = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &host);
	if ( ret != XUI_OK ) return ret;
	__xuiDockDefaultCachePolicy(&policy);
	(void)xuiWidgetSetLayoutType(host, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(host, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(host, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(host, 0);
	(void)xuiWidgetSetTabStop(host, 0);
	(void)xuiWidgetSetCachePolicy(host, &policy);
	ret = xuiWidgetAddChild(pWidget, host);
	if ( ret != XUI_OK ) {
		xuiWidgetDestroy(host);
		return ret;
	}
	w = &pData->arrWindows[i];
	memset(w, 0, sizeof(*w));
	w->bUsed = 1;
	w->iWindow = i;
	w->iPane = -1;
	w->iState = XUI_DOCK_PANEL_WINDOW_HIDDEN;
	w->iRegion = XUI_DOCK_PANEL_REGION_DOCUMENT;
	w->iLastRegion = XUI_DOCK_PANEL_REGION_DOCUMENT;
	w->iAutoHideRegion = XUI_DOCK_PANEL_REGION_LEFT;
	w->iLastSide = XUI_DOCK_PANEL_SIDE_FILL;
	w->iLastTabIndex = -1;
	w->bClosable = 1;
	w->bDockable = 1;
	w->pPanelWidget = pWidget;
	w->pHostWidget = host;
	w->pOwner = pData;
	w->tFloatRect = __xuiDockRect(80.0f + (float)(i % 8) * 18.0f, 70.0f + (float)(i % 6) * 18.0f, 300.0f, 220.0f);
	__xuiDockCopy(w->sTitle, XUI_DOCK_PANEL_TITLE_CAPACITY, sTitle);
	(void)xuiWidgetSetCacheRenderCallback(host, __xuiDockHostRender, w);
	(void)xuiWidgetSetEventHandler(host, XUI_EVENT_POINTER_DOWN, __xuiDockHostEvent, w);
	(void)xuiWidgetSetEventHandler(host, XUI_EVENT_POINTER_MOVE, __xuiDockHostEvent, w);
	(void)xuiWidgetSetEventHandler(host, XUI_EVENT_POINTER_UP, __xuiDockHostEvent, w);
	(void)xuiWidgetSetEventHandler(host, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiDockHostEvent, w);
	(void)xuiWidgetSetTooltipResolver(host, __xuiDockHostTooltipResolve, w);
	(void)xuiWidgetSetVisible(host, 0);
	pData->iWindowCount++;
	if ( pWindow != NULL ) *pWindow = i;
	if ( pClient != NULL ) {
		ret = xuiDockPanelSetWindowClient(pWidget, i, pClient);
		if ( ret != XUI_OK ) {
			if ( xuiWidgetGetParent(host) == pWidget ) {
				(void)xuiWidgetRemoveFromParent(host);
			}
			xuiWidgetDestroy(host);
			memset(w, 0, sizeof(*w));
			pData->iWindowCount--;
			if ( pWindow != NULL ) *pWindow = -1;
			__xuiDockInvalidate(pWidget, 1);
			return ret;
		}
	}
	pData->iChangeCount++;
	pData->iWindowChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelSetWindowClient(xui_widget pWidget, int iWindow, xui_widget pClient)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	xui_widget pOldClient;
	xui_widget pNewParent;
	int bOldAttached;
	int ret;
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( w->pClientWidget == pClient ) return XUI_OK;
	if ( pClient != NULL &&
	     (!xuiInternalWidgetIsValid(pClient) ||
	      xuiWidgetGetContext(pClient) != xuiWidgetGetContext(pWidget) ||
	      pClient == w->pHostWidget ||
	      __xuiDockWidgetIsDescendantOf(w->pHostWidget, pClient)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pOldClient = w->pClientWidget;
	bOldAttached = pOldClient != NULL && xuiWidgetGetParent(pOldClient) == w->pHostWidget;
	pNewParent = (pClient != NULL) ? xuiWidgetGetParent(pClient) : NULL;
	if ( pClient != NULL ) {
		if ( pNewParent != NULL ) {
			ret = xuiWidgetRemoveFromParent(pClient);
			if ( ret != XUI_OK ) return ret;
		}
	}
	if ( bOldAttached ) {
		__xuiDockSetWidgetTreeLayer(pOldClient, XUI_LAYER_NORMAL, 0);
		ret = xuiWidgetRemoveFromParent(pOldClient);
		if ( ret != XUI_OK ) {
			if ( pClient != NULL && pNewParent != NULL ) (void)xuiWidgetAddChild(pNewParent, pClient);
			return ret;
		}
	}
	if ( pClient != NULL ) {
		(void)xuiWidgetSetFlowMode(pClient, XUI_FLOW_ABSOLUTE);
		(void)xuiWidgetSetSizeMode(pClient, XUI_SIZE_FILL, XUI_SIZE_FILL);
		ret = xuiWidgetAddChild(w->pHostWidget, pClient);
		if ( ret != XUI_OK ) {
			if ( bOldAttached ) {
				(void)xuiWidgetAddChild(w->pHostWidget, pOldClient);
				__xuiDockSetHostLayer(w);
			}
			if ( pNewParent != NULL ) (void)xuiWidgetAddChild(pNewParent, pClient);
			return ret;
		}
		__xuiDockSetHostLayer(w);
	}
	w->pClientWidget = pClient;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API xui_widget xuiDockPanelGetWindowClient(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	return (w != NULL) ? w->pClientWidget : NULL;
}

XUI_API int xuiDockPanelSetWindowTitle(xui_widget pWidget, int iWindow, const char* sTitle)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDockCopy(w->sTitle, XUI_DOCK_PANEL_TITLE_CAPACITY, sTitle);
	if ( w->pHostWidget != NULL ) (void)xuiWidgetInvalidate(w->pHostWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	__xuiDockInvalidate(pWidget, 0);
	return XUI_OK;
}

XUI_API const char* xuiDockPanelGetWindowTitle(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	return (w != NULL) ? w->sTitle : NULL;
}

XUI_API int xuiDockPanelSetWindowFlags(xui_widget pWidget, int iWindow, int bClosable, int bDockable)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	w->bClosable = bClosable ? 1 : 0;
	w->bDockable = bDockable ? 1 : 0;
	__xuiDockInvalidate(pWidget, 0);
	return XUI_OK;
}

XUI_API int xuiDockPanelSetWindowUserData(xui_widget pWidget, int iWindow, void* pUser)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	w->pUser = pUser;
	return XUI_OK;
}

XUI_API void* xuiDockPanelGetWindowUserData(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	return (w != NULL) ? w->pUser : NULL;
}

XUI_API int xuiDockPanelSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = (pFont != NULL) ? pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( pData->pOptionMenu != NULL ) (void)xuiMenuSetFont(pData->pOptionMenu, pData->pFont);
	if ( pData->pOverflowMenu != NULL ) (void)xuiMenuSetFont(pData->pOverflowMenu, pData->pFont);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API xui_font xuiDockPanelGetFont(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiDockPanelDockWindow(xui_widget pWidget, int iWindow, int iRegion, int iSide, float fRatio, int* pPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( (pData == NULL) || (w == NULL) || !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockDockWindowToRegion(pWidget, pData, iWindow, iRegion, iSide, fRatio, pPane);
}

XUI_API int xuiDockPanelDockWindowToPane(xui_widget pWidget, int iWindow, int iPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( (pData == NULL) || (w == NULL) || !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockDockWindowToPaneSide(pWidget, pData, iWindow, iPane, XUI_DOCK_PANEL_SIDE_FILL, 0.0f, NULL);
}

XUI_API int xuiDockPanelDockWindowToPaneSide(xui_widget pWidget, int iWindow, int iPane, int iSide, float fRatio, int* pPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( (pData == NULL) || (w == NULL) || !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockDockWindowToPaneSide(pWidget, pData, iWindow, iPane, iSide, fRatio, pPane);
}

XUI_API int xuiDockPanelFloatWindow(xui_widget pWidget, int iWindow, xui_rect_t tRect)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	int ret;
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iAutoHideExpandWindow == iWindow ) pData->iAutoHideExpandWindow = -1;
	__xuiDockRememberLastDockRect(pData, w);
	ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
	if ( ret != XUI_OK ) return ret;
	w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	w->tFloatRect = __xuiDockClampFloatRect(pWidget, tRect);
	w->iLastSide = XUI_DOCK_PANEL_SIDE_FILL;
	__xuiDockSetWindowState(pWidget, pData, w, XUI_DOCK_PANEL_WINDOW_FLOATING);
	(void)__xuiDockRequestFocusWindow(pWidget, pData, iWindow);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelHideWindow(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	int ret;
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iAutoHideExpandWindow == iWindow ) pData->iAutoHideExpandWindow = -1;
	__xuiDockRememberLastDockRect(pData, w);
	ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
	if ( ret != XUI_OK ) return ret;
	w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDockSetWindowState(pWidget, pData, w, XUI_DOCK_PANEL_WINDOW_HIDDEN);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelAutoHideWindow(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	int iStripRegion = -1;
	int ret;
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (w->iState != XUI_DOCK_PANEL_WINDOW_DOCKED) || !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	iStripRegion = __xuiDockWindowAutoHideStripRegionFromPane(pData, w);
	if ( w->iPane >= 0 ) {
		xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, w->iPane);
		if ( pPane != NULL ) {
			if ( pPane->iRegion != XUI_DOCK_PANEL_REGION_DOCUMENT ) {
				w->iLastRegion = pPane->iRegion;
			}
		}
	}
	if ( !__xuiDockRegionIsAutoHideSide(iStripRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
	w->iAutoHideRegion = iStripRegion;
	__xuiDockRememberLastDockRect(pData, w);
	ret = __xuiDockDetachWindow(pWidget, pData, iWindow);
	if ( ret != XUI_OK ) return ret;
	w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDockSetWindowState(pWidget, pData, w, XUI_DOCK_PANEL_WINDOW_AUTO_HIDE);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelExpandAutoHideWindow(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( w->iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) return XUI_OK;
	if ( pData->iAutoHideExpandWindow == iWindow ) return XUI_OK;
	pData->iAutoHideExpandWindow = iWindow;
	(void)__xuiDockRequestFocusWindow(pWidget, pData, iWindow);
	pData->iChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelCollapseAutoHide(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iAutoHideExpandWindow < 0 ) return XUI_OK;
	pData->iAutoHideExpandWindow = -1;
	pData->iChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelGetAutoHideExpandedWindow(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iAutoHideExpandWindow : -1;
}

XUI_API xui_rect_t xuiDockPanelGetAutoHideExpandRect(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->tAutoHideExpandRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiDockPanelDockAutoHideWindow(xui_widget pWidget, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	int region;
	int side;
	int pane = -1;
	if ( w == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !w->bDockable ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( w->iState != XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) return XUI_OK;
	if ( pData->iAutoHideExpandWindow == iWindow ) pData->iAutoHideExpandWindow = -1;
	region = __xuiDockRegionValid(w->iLastRegion) ? w->iLastRegion : XUI_DOCK_PANEL_REGION_DOCUMENT;
	side = __xuiDockSideIsSplit(w->iLastSide) ? w->iLastSide : XUI_DOCK_PANEL_SIDE_FILL;
	return __xuiDockDockWindowToRegion(pWidget, pData, iWindow, region, side, 0.25f, &pane);
}

XUI_API int xuiDockPanelSetPaneActiveWindow(xui_widget pWidget, int iPane, int iWindow)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	int idx = __xuiDockPaneIndexOfWindow(pPane, iWindow);
	if ( (pData == NULL) || (pPane == NULL) || (idx < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockPaneSetActiveIndex(pWidget, pData, pPane, idx);
}

XUI_API int xuiDockPanelGetPaneActiveWindow(xui_widget pWidget, int iPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return __xuiDockPaneActiveWindow(__xuiDockPaneAt(pData, iPane));
}

XUI_API int xuiDockPanelGetPaneWindowCount(xui_widget pWidget, int iPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	return (pPane != NULL) ? pPane->iWindowCount : 0;
}

XUI_API int xuiDockPanelGetPaneWindow(xui_widget pWidget, int iPane, int iIndex)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	if ( (pPane == NULL) || (iIndex < 0) || (iIndex >= pPane->iWindowCount) ) return -1;
	return pPane->arrWindows[iIndex];
}

XUI_API int xuiDockPanelGetWindowCount(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iWindowCount : 0;
}

XUI_API int xuiDockPanelGetPaneCount(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iPaneCount : 0;
}

XUI_API int xuiDockPanelGetWindowInfo(xui_widget pWidget, int iWindow, xui_dock_window_info_t* pInfo)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_window_slot_t* w = __xuiDockWindowAt(pData, iWindow);
	if ( (w == NULL) || (pInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iWindow = w->iWindow;
	pInfo->iPane = w->iPane;
	pInfo->iState = w->iState;
	pInfo->iRegion = w->iRegion;
	pInfo->iLastRegion = w->iLastRegion;
	pInfo->iLastSide = w->iLastSide;
	pInfo->iLastTabIndex = w->iLastTabIndex;
	pInfo->bClosable = w->bClosable;
	pInfo->bDockable = w->bDockable;
	__xuiDockCopy(pInfo->sTitle, XUI_DOCK_PANEL_TITLE_CAPACITY, w->sTitle);
	pInfo->pHostWidget = w->pHostWidget;
	pInfo->pClientWidget = w->pClientWidget;
	pInfo->tRect = w->tRect;
	pInfo->tClientRect = w->tClientRect;
	pInfo->tTabRect = w->tTabRect;
	pInfo->tAutoHideRect = w->tAutoHideRect;
	pInfo->pUser = w->pUser;
	return XUI_OK;
}

XUI_API int xuiDockPanelGetPaneInfo(xui_widget pWidget, int iPane, xui_dock_pane_info_t* pInfo)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_pane_slot_t* pPane = __xuiDockPaneAt(pData, iPane);
	if ( (pPane == NULL) || (pInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iPane = pPane->iPane;
	pInfo->iRegion = pPane->iRegion;
	pInfo->iWindowCount = pPane->iWindowCount;
	pInfo->iActiveIndex = pPane->iActiveIndex;
	pInfo->iActiveWindow = __xuiDockPaneActiveWindow(pPane);
	pInfo->iFirstVisibleTab = pPane->iFirstVisibleTab;
	pInfo->iVisibleTabCount = pPane->iVisibleTabCount;
	pInfo->bOverflow = pPane->bOverflow;
	pInfo->tRect = pPane->tRect;
	pInfo->tCaptionRect = pPane->tCaptionRect;
	pInfo->tTabStripRect = pPane->tTabStripRect;
	pInfo->tClientRect = pPane->tClientRect;
	pInfo->tOptionRect = pPane->tOptionRect;
	pInfo->tOverflowRect = pPane->tOverflowRect;
	return XUI_OK;
}

XUI_API int xuiDockPanelSetRegionSize(xui_widget pWidget, int iRegion, int iSizeMode, float fValue)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiDockRegionValid(iRegion) || (fValue < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iSizeMode != XUI_DOCK_PANEL_SIZE_PORTION) && (iSizeMode != XUI_DOCK_PANEL_SIZE_PIXEL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrRegions[iRegion].iSizeMode = iSizeMode;
	pData->arrRegions[iRegion].fValue = fValue;
	pData->iLayoutChangeCount++;
	pData->iChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelGetRegionSize(xui_widget pWidget, int iRegion, int* pSizeMode, float* pValue)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || !__xuiDockRegionValid(iRegion) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pSizeMode != NULL ) *pSizeMode = pData->arrRegions[iRegion].iSizeMode;
	if ( pValue != NULL ) *pValue = pData->arrRegions[iRegion].fValue;
	return XUI_OK;
}

XUI_API int xuiDockPanelSetMetrics(xui_widget pWidget, const xui_dock_panel_metrics_t* pMetrics)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API int xuiDockPanelGetMetrics(xui_widget pWidget, xui_dock_panel_metrics_t* pMetrics)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiDockPanelSetColors(xui_widget pWidget, const xui_dock_panel_colors_t* pColors)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	__xuiDockInvalidate(pWidget, 0);
	return XUI_OK;
}

XUI_API int xuiDockPanelGetColors(xui_widget pWidget, xui_dock_panel_colors_t* pColors)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	return XUI_OK;
}

XUI_API int xuiDockPanelSetWindowStateChanged(xui_widget pWidget, xui_dock_window_state_proc onState, void* pUser)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onStateChanged = onState;
	pData->pStateUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDockPanelSetActiveChanged(xui_widget pWidget, xui_dock_active_proc onActive, void* pUser)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onActiveChanged = onActive;
	pData->pActiveUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDockPanelSetWindowClose(xui_widget pWidget, xui_dock_window_close_proc onClose, void* pUser)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onClose = onClose;
	pData->pCloseUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDockPanelHitTest(xui_widget pWidget, float fX, float fY, xui_dock_hit_t* pHit)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pHit == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiDockHitLocal(pData, fX, fY, pHit);
	return XUI_OK;
}

XUI_API int xuiDockPanelFindDropTarget(xui_widget pWidget, int iWindow, float fX, float fY, xui_dock_drop_info_t* pInfo)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockFindDropTarget(pWidget, pData, iWindow, fX, fY, pInfo);
}

XUI_API int xuiDockPanelGetDragPreview(xui_widget pWidget, xui_dock_drop_info_t* pInfo)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( (pData == NULL) || (pInfo == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pInfo = pData->tDragPreview;
	pInfo->iSize = sizeof(*pInfo);
	return XUI_OK;
}

XUI_API xui_widget xuiDockPanelGetOptionMenu(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->pOptionMenu : NULL;
}

XUI_API xui_widget xuiDockPanelGetOverflowMenu(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->pOverflowMenu : NULL;
}

XUI_API int xuiDockPanelSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_dock_panel_data_t* pData;
	xui_dock_pane_slot_t* pPane;
	char* sNew;
	int iIndex;

	pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiDockMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_DOCK_PANEL_MENU_TITLE_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sTitle == NULL || sTitle[0] == '\0' ) {
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = NULL;
	} else {
		sNew = __xuiDockStringDup(sTitle);
		if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = sNew;
	}
	if ( pData->pOptionMenu != NULL ) {
		pPane = (pData->iMenuPane >= 0) ? __xuiDockPaneAt(pData, pData->iMenuPane) : NULL;
		if ( pPane != NULL ) (void)__xuiDockBuildPaneMenu(pWidget, pData, pPane);
		else (void)xuiMenuClear(pData->pOptionMenu);
	}
	return XUI_OK;
}

XUI_API const char* xuiDockPanelGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return __xuiDockMenuTitleForCommand(pWidget, pData, iCommand);
}

XUI_API int xuiDockPanelSetTooltipText(xui_widget pWidget, int iTooltip, const char* sText)
{
	xui_dock_panel_data_t* pData;
	char* sNew;
	int iIndex;

	pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiDockTooltipTextIndexForId(iTooltip);
	if ( iIndex < 0 || iIndex >= XUI_DOCK_PANEL_TOOLTIP_TEXT_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL || sText[0] == '\0' ) {
		xrtFree(pData->arrTooltipText[iIndex]);
		pData->arrTooltipText[iIndex] = NULL;
		return XUI_OK;
	}
	sNew = __xuiDockStringDup(sText);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	xrtFree(pData->arrTooltipText[iIndex]);
	pData->arrTooltipText[iIndex] = sNew;
	return XUI_OK;
}

XUI_API const char* xuiDockPanelGetTooltipText(xui_widget pWidget, int iTooltip)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_context pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	return __xuiDockTooltipTextForId(pContext, pData, iTooltip);
}

XUI_API int xuiDockPanelOpenPaneMenu(xui_widget pWidget, int iPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockOpenPaneMenu(pWidget, pData, iPane);
}

XUI_API int xuiDockPanelOpenOverflowMenu(xui_widget pWidget, int iPane)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiDockOpenOverflowMenu(pWidget, pData, iPane);
}

XUI_API int xuiDockPanelSaveState(xui_widget pWidget, xvalue* ppState)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xvalue pRoot;
	xvalue pRegions;
	xvalue pWindows;
	xvalue pFloating;
	int ret;
	int i;
	if ( (pData == NULL) || (ppState == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppState = NULL;
	pRoot = xvoCreateTable();
	pRegions = xvoCreateArray();
	pWindows = xvoCreateArray();
	pFloating = xvoCreateArray();
	if ( (pRoot == NULL) || (pRegions == NULL) || (pWindows == NULL) || (pFloating == NULL) ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		if ( pRegions != NULL ) xvoUnref(pRegions);
		if ( pWindows != NULL ) xvoUnref(pWindows);
		if ( pFloating != NULL ) xvoUnref(pFloating);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	ret = __xuiDockStateSetText(pRoot, "kind", "xui.dockpanel");
	if ( ret == XUI_OK && !xvoTableSetInt(pRoot, "version", 0, 1) ) ret = XUI_ERROR_OUT_OF_MEMORY;
	for ( i = 0; ret == XUI_OK && i < XUI_DOCK_PANEL_REGION_COUNT; i++ ) {
		ret = __xuiDockStateAppendRegion(pData, pRegions, i);
	}
	for ( i = 0; ret == XUI_OK && i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed ) ret = __xuiDockStateAppendWindow(pWindows, &pData->arrWindows[i]);
	}
	for ( i = 0; ret == XUI_OK && i < pData->iFloatCount && i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		int w = pData->arrFloatOrder[i];
		if ( w >= 0 && w < XUI_DOCK_PANEL_WINDOW_CAPACITY && pData->arrWindows[w].bUsed && pData->arrWindows[w].iState == XUI_DOCK_PANEL_WINDOW_FLOATING ) {
			ret = __xuiDockStateAppendFloating(pFloating, &pData->arrWindows[w], i);
		}
	}
	for ( i = 0; ret == XUI_OK && i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		if ( pData->arrWindows[i].bUsed &&
		     pData->arrWindows[i].iState == XUI_DOCK_PANEL_WINDOW_FLOATING &&
		     __xuiDockFloatOrderIndex(pData, i) < 0 ) {
			ret = __xuiDockStateAppendFloating(pFloating, &pData->arrWindows[i], pData->iFloatCount + i);
		}
	}
	if ( ret == XUI_OK ) {
		if ( !xvoTableSetValue(pRoot, "regions", 0, pRegions, TRUE) ) {
			ret = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			pRegions = NULL;
		}
	}
	if ( ret == XUI_OK ) {
		if ( !xvoTableSetValue(pRoot, "windows", 0, pWindows, TRUE) ) {
			ret = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			pWindows = NULL;
		}
	}
	if ( ret == XUI_OK ) {
		if ( !xvoTableSetValue(pRoot, "floating", 0, pFloating, TRUE) ) {
			ret = XUI_ERROR_OUT_OF_MEMORY;
		} else {
			pFloating = NULL;
		}
	}
	if ( ret != XUI_OK ) {
		if ( pRegions != NULL ) xvoUnref(pRegions);
		if ( pWindows != NULL ) xvoUnref(pWindows);
		if ( pFloating != NULL ) xvoUnref(pFloating);
		xvoUnref(pRoot);
		return ret;
	}
	*ppState = pRoot;
	return XUI_OK;
}

XUI_API int xuiDockPanelLoadState(xui_widget pWidget, xvalue pState)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	xui_dock_state_window_t arrInfo[XUI_DOCK_PANEL_WINDOW_CAPACITY];
	int arrRegionSeen[XUI_DOCK_PANEL_REGION_COUNT];
	xvalue pValue;
	const char* sKind;
	int version;
	int ret;
	int i;
	if ( (pData == NULL) || (pState == NULL) || (pState->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
	sKind = __xuiDockStateGetText(__xuiDockStateGet(pState, "kind"));
	if ( sKind != NULL && strcmp(sKind, "xui.dockpanel") != 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pValue = __xuiDockStateGet(pState, "version");
	if ( (pValue == NULL) || !__xuiDockStateValueToInt(pValue, &version) || (version != 1) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrInfo, 0, sizeof(arrInfo));
	memset(arrRegionSeen, 0, sizeof(arrRegionSeen));
	ret = __xuiDockLoadPrepareWindows(pData, pState, arrInfo);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockLoadValidateRegions(pData, pState, arrInfo, arrRegionSeen);
	if ( ret != XUI_OK ) return ret;
	ret = __xuiDockLoadValidateFloating(pData, pState, arrInfo);
	if ( ret != XUI_OK ) return ret;
	__xuiDockLoadClearLayout(pWidget, pData);
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* pWin;
		if ( !pData->arrWindows[i].bUsed ) continue;
		pWin = &pData->arrWindows[i];
		pWin->iRegion = XUI_DOCK_PANEL_REGION_DOCUMENT;
		pWin->iLastRegion = arrInfo[i].iLastRegion;
		pWin->iAutoHideRegion = arrInfo[i].iAutoHideRegion;
		pWin->iLastSide = arrInfo[i].iLastSide;
		pWin->iLastTabIndex = arrInfo[i].iLastTabIndex;
		pWin->bClosable = arrInfo[i].bClosable;
		pWin->bDockable = arrInfo[i].bDockable;
		pWin->tFloatRect = arrInfo[i].tFloatRect;
		pWin->tLastDockRect = arrInfo[i].tLastDockRect;
		__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_HIDDEN);
	}
	ret = __xuiDockLoadApplyRegions(pWidget, pData, pState, arrInfo);
	if ( ret == XUI_OK ) ret = __xuiDockLoadApplyFloating(pWidget, pData, pState, arrInfo);
	if ( ret != XUI_OK ) {
		__xuiDockLoadClearLayout(pWidget, pData);
		__xuiDockInvalidate(pWidget, 1);
		return ret;
	}
	for ( i = 0; i < XUI_DOCK_PANEL_WINDOW_CAPACITY; i++ ) {
		xui_dock_window_slot_t* pWin;
		if ( !pData->arrWindows[i].bUsed || !arrInfo[i].bInState || arrInfo[i].bInTree || arrInfo[i].bInFloating ) continue;
		pWin = &pData->arrWindows[i];
		if ( arrInfo[i].iState == XUI_DOCK_PANEL_WINDOW_AUTO_HIDE ) {
			if ( !__xuiDockRegionIsAutoHideSide(pWin->iAutoHideRegion) ) {
				pWin->iAutoHideRegion = __xuiDockAutoHideRegionFromLast(pWin->iLastRegion, pWin->iLastSide);
			}
			if ( !__xuiDockRegionIsAutoHideSide(pWin->iAutoHideRegion) ) {
				pWin->iAutoHideRegion = XUI_DOCK_PANEL_REGION_LEFT;
			}
			if ( pWin->iLastSide == XUI_DOCK_PANEL_SIDE_NONE ) pWin->iLastSide = XUI_DOCK_PANEL_SIDE_FILL;
			__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_AUTO_HIDE);
		} else {
			__xuiDockSetWindowState(pWidget, pData, pWin, XUI_DOCK_PANEL_WINDOW_HIDDEN);
		}
	}
	pData->iChangeCount++;
	pData->iLayoutChangeCount++;
	pData->iWindowChangeCount++;
	__xuiDockInvalidate(pWidget, 1);
	return XUI_OK;
}

XUI_API void xuiDockPanelStateFree(xvalue pState)
{
	if ( pState != NULL ) xvoUnref(pState);
}

XUI_API int xuiDockPanelStateGetCounts(xvalue pState, int* pRegionCount, int* pWindowCount, int* pFloatingCount)
{
	xvalue pRegions;
	xvalue pWindows;
	xvalue pFloating;
	if ( (pState == NULL) || (pState->Type != XVO_DT_TABLE) ) return XUI_ERROR_INVALID_ARGUMENT;
	pRegions = __xuiDockStateGet(pState, "regions");
	pWindows = __xuiDockStateGet(pState, "windows");
	pFloating = __xuiDockStateGet(pState, "floating");
	if ( (pRegions == NULL) || (pRegions->Type != XVO_DT_ARRAY) ||
	     (pWindows == NULL) || (pWindows->Type != XVO_DT_ARRAY) ||
	     (pFloating == NULL) || (pFloating->Type != XVO_DT_ARRAY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pRegionCount != NULL ) *pRegionCount = (int)xvoArrayItemCount(pRegions);
	if ( pWindowCount != NULL ) *pWindowCount = (int)xvoArrayItemCount(pWindows);
	if ( pFloatingCount != NULL ) *pFloatingCount = (int)xvoArrayItemCount(pFloating);
	return XUI_OK;
}

XUI_API int xuiDockPanelSaveXSONFile(xui_widget pWidget, const char* sPath)
{
	xvalue pState;
	int ret;
	if ( (sPath == NULL) || (sPath[0] == 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = xuiDockPanelSaveState(pWidget, &pState);
	if ( ret != XUI_OK ) return ret;
	ret = xrtStringifyXSON_File((str)(void*)sPath, pState, TRUE, 0) ? XUI_OK : XUI_ERROR_RESOURCE_FAILED;
	xvoUnref(pState);
	return ret;
}

XUI_API int xuiDockPanelLoadXSONFile(xui_widget pWidget, const char* sPath)
{
	xvalue pState;
	int ret;
	if ( (sPath == NULL) || (sPath[0] == 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pState = xrtParseXSON_File((str)(void*)sPath);
	if ( (pState == NULL) || (pState->Type == XVO_DT_NULL) ) {
		if ( pState != NULL ) xvoUnref(pState);
		return XUI_ERROR_RESOURCE_FAILED;
	}
	ret = xuiDockPanelLoadState(pWidget, pState);
	xvoUnref(pState);
	return ret;
}

XUI_API int xuiDockPanelGetChangeCount(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiDockPanelGetLayoutChangeCount(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iLayoutChangeCount : 0;
}

XUI_API int xuiDockPanelGetWindowChangeCount(xui_widget pWidget)
{
	xui_dock_panel_data_t* pData = __xuiDockPanelGetData(pWidget);
	return (pData != NULL) ? pData->iWindowChangeCount : 0;
}
