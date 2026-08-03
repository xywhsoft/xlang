#ifndef XUI_INTERNAL_H
#define XUI_INTERNAL_H

/* TCC PE 链接器要求外部变量必须声明 dllimport (xCore) */
#if defined(_WIN32) && defined(__TINYC__)
#ifndef XXAPI
#define XXAPI __declspec(dllimport)
#endif
#endif

#include "../xui.h"

#define XUI_CONTEXT_MAGIC 0x58554943u
#define XUI_CONTEXT_DAMAGE_INLINE 8
#define XUI_CONTEXT_EVENT_INLINE 32
#define XUI_CONTEXT_HOTKEY_INLINE 16
#define XUI_CONTEXT_FONT_INLINE 16
#define XUI_CONTEXT_MAX_VIEWPORT 1073741824.0f

#define XUI_WIDGET_MAGIC 0x58554957u
#define XUI_WIDGET_TYPE_MAGIC 0x58554954u
#define XUI_STYLE_RULE_MAGIC 0x58554953u
#define XUI_PAINTER_MAGIC 0x58554950u
#define XUI_RESOURCE_MAGIC 0x58554952u
#define XUI_PATH_MAGIC 0x58554948u
#define XUI_LANGUAGE_MAGIC 0x5855494Cu

typedef struct xui_style_prop_t xui_style_prop_t;
typedef struct xui_style_rule_t xui_style_rule_t;
typedef struct xui_style_property_entry_t xui_style_property_entry_t;
typedef struct xui_style_token_t xui_style_token_t;
typedef struct xui_resource_t xui_resource_t;
typedef struct xui_resource_dependency_t xui_resource_dependency_t;
typedef struct xui_hotkey_t xui_hotkey_t;
typedef struct xui_font_entry_t xui_font_entry_t;
typedef struct xui_pointer_state_t xui_pointer_state_t;

struct xui_language_t {
	uint32_t iMagic;
	xui_context pContext;
	int iLanguageId;
	int iFallbackLanguageId;
	char* sCode;
	char* sName;
	xarray_struct arrTexts;
	uint32_t iRevision;
	int bBuiltin;
};

struct xui_pointer_state_t {
	int bAllocated;
	int bDown;
	uint64_t iPointerId;
	int iPointerType;
	xui_widget pHoverWidget;
	xui_widget pActiveWidget;
	xui_widget pPointerCaptureWidget;
	xui_widget pDragWidget;
	xui_widget pLastClickWidget;
	xui_widget pContextPressWidget;
	float fPointerX;
	float fPointerY;
	float fContextPressTime;
	float fContextPressStartX;
	float fContextPressStartY;
	float fContextPressLastX;
	float fContextPressLastY;
	float fDragStartX;
	float fDragStartY;
	float fLastClickX;
	float fLastClickY;
	uint32_t iPointerButtons;
	int iActiveButton;
	int iDragButton;
	int iLastClickButton;
	int bContextPressActive;
	int bContextPressMoved;
	int bContextPressFired;
	int bDragActive;
	double fLastClickTime;
};

typedef struct xui_widget_cache_slot_t {
	uint32_t iStateId;
	xui_surface pSurface;
	int iWidth;
	int iHeight;
	uint32_t iSurfaceGeneration;
	uint32_t iFlags;
} xui_widget_cache_slot_t;

struct xui_hotkey_t {
	xui_widget pWidget;
	int iKey;
	uint32_t iModifiers;
	xui_widget_event_proc onEvent;
	void* pUser;
	int iCommand;
	char* sCommand;
	void* pData;
};

struct xui_font_entry_t {
	char* sName;
	xui_font pFont;
};

struct xui_context_t {
	uint32_t iMagic;
	xui_proxy_t tProxy;
	xui_proxy_caps_t tProxyCaps;
	int bHasProxy;
	float fViewportWidth;
	float fViewportHeight;
	float fDpiScale;
	uint32_t iGeneration;
	xui_widget pRoot;
	xui_widget pOverlayRoot;
	xui_widget pHoverWidget;
	xui_widget pActiveWidget;
	xui_widget pFocusWidget;
	xui_widget pPointerCaptureWidget;
	xui_widget pDragWidget;
	xui_widget pLastClickWidget;
	xui_widget pTooltipOwner;
	xui_widget pTooltipPopupWidget;
	xui_tooltip_desc_t tActiveTooltip;
	xui_rect_t tTooltipRect;
	char* sActiveTooltipText;
	float fTooltipHoverTime;
	float fTooltipMouseX;
	float fTooltipMouseY;
	int bTooltipOpen;
	float fPointerX;
	float fPointerY;
	float fContextPressTime;
	float fContextPressStartX;
	float fContextPressStartY;
	float fContextPressLastX;
	float fContextPressLastY;
	float fDragStartX;
	float fDragStartY;
	float fLastClickX;
	float fLastClickY;
	uint32_t iPointerButtons;
	uint32_t iInputModifiers;
	int iActiveButton;
	int iDragButton;
	int iLastClickButton;
	int bContextPressActive;
	int bContextPressMoved;
	int bContextPressFired;
	int bDragActive;
	double fLastClickTime;
	xui_widget pContextPressWidget;
	xui_pointer_state_t arrPointerStates[XUI_POINTER_MAX];
	int iPointerStateCount;
	uint64_t iInputPointerId;
	int iInputPointerType;
	int iInputDispatchDepth;
	xui_rect_i_t arrInlineDamage[XUI_CONTEXT_DAMAGE_INLINE];
	xui_rect_i_t* pDamage;
	int iDamageCount;
	int iDamageCapacity;
	xui_event_t arrInlineEvents[XUI_CONTEXT_EVENT_INLINE];
	xui_event_t* pEvents;
	int iEventCount;
	int iEventCapacity;
	int iEventRead;
	xui_hotkey_t arrInlineHotkeys[XUI_CONTEXT_HOTKEY_INLINE];
	xui_hotkey_t* pHotkeys;
	int iHotkeyCount;
	int iHotkeyCapacity;
	xui_render_stats_t tRenderStats;
	xui_render_node_t* pRenderNodes;
	int iRenderNodeCount;
	int iRenderNodeCapacity;
	uint32_t iRenderTreeGeneration;
	size_t iCacheBudgetBytes;
	uint32_t iCacheGeneration;
	xui_widget_type pWidgetTypes;
	xui_style_rule_t* pStyleRules;
	xui_style_property_entry_t* pStyleProperties;
	xui_style_token_t* pStyleTokens;
	xui_style_prop_t* pDefaultStyle;
	int iDefaultStyleCount;
	uint32_t iNextStylePropertyId;
	uint32_t iStyleGeneration;
	uint32_t iTokenGeneration;
	int iStyleUpdateDepth;
	int bStyleDirty;
	xui_resource_t* pResources;
	uint32_t iNextResourceGeneration;
	xdict_struct mapIconCategories;
	xarray_struct arrIconCategories;
	xui_icon_category pIconCategories;
	xui_icon pIcons;
	uint32_t iIconGeneration;
	void (*onDestroyIcons)(xui_context pContext);
	xarray_struct arrLanguages;
	int iCurrentLanguageId;
	int iNextCustomLanguageId;
	uint32_t iLanguageRevision;
	xui_theme_t tTheme;
	xui_chrome_style_t tChromeStyle;
	xui_font pDefaultFont;
	xui_font_entry_t arrInlineFonts[XUI_CONTEXT_FONT_INLINE];
	xui_font_entry_t* pFonts;
	int iFontCount;
	int iFontCapacity;
	int bImeEnabled;
	int bHasImeCandidateRect;
	xui_rect_t tImeCandidateRect;
	float fXgeInputMouseX;
	float fXgeInputMouseY;
	uint32_t iXgeInputMouseButtons;
	int bXgeInputPointerReady;
};

struct xui_widget_type_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget_type pParent;
	xui_widget_type pNext;
	char* sName;
	uint32_t iFlags;
	size_t iTypeDataSize;
	void* pUser;
	xui_widget_type_init_proc onInit;
	xui_widget_type_destroy_proc onDestroy;
	xui_widget_content_measure_proc onContentMeasure;
	xui_widget_layout_measure_proc onLayoutMeasure;
	xui_widget_layout_arrange_proc onLayoutArrange;
	xui_widget_cache_render_proc onCacheRender;
	xui_widget_update_proc onUpdate;
	xui_layout_t tLayout;
	xui_cache_policy_t tCachePolicy;
	int iWidgetCount;
	int bBuiltin;
};

struct xui_widget_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget_type pType;
	xui_widget pParent;
	xui_widget pFirstChild;
	xui_widget pLastChild;
	xui_widget pPrevSibling;
	xui_widget pNextSibling;
	int iChildCount;
	xui_rect_t tRect;
	xui_layout_t tLayout;
	xui_vec2_t tMeasuredSize;
	xui_vec2_t tMeasureConstraint;
	int bVisible;
	int bEnabled;
	int bHitTestVisible;
	int bFocusable;
	int bTabStop;
	int iTabIndex;
	int bFocusScope;
	int bDragEnabled;
	int iImeMode;
	int bMeasureValid;
	int bArrangeValid;
	uint32_t iDirtyFlags;
	uint32_t iSubtreeDirtyFlags;
	uint32_t iInputState;
	uint64_t iEventInterestMask;
	uint64_t iEventHandlerMask;
	uint64_t iSubtreeEventMask;
	uint32_t iStateId;
	uint32_t iGeneration;
	uint32_t iLayoutVersion;
	uint32_t iStyleVersion;
	void* pUser;
	xui_widget pOverlayOwner;
	void* pTypeData;
	char* sStyleName;
	char** pStyleClasses;
	int iStyleClassCount;
	int iStyleClassCapacity;
	xui_style_prop_t* pInlineStyle;
	int iInlineStyleCount;
	xui_style_prop_t* pResolvedStyle;
	int iResolvedStyleCount;
	uint32_t iResolvedStyleHash;
	uint32_t iResolvedStyleGeneration;
	xui_widget_content_measure_proc onContentMeasure;
	void* pContentMeasureUser;
	xui_widget_layout_measure_proc onLayoutMeasure;
	xui_widget_layout_arrange_proc onLayoutArrange;
	void* pLayoutUser;
	xui_widget_event_proc onEvent;
	void* pEventUser;
	xui_widget_event_proc arrEventHandlers[XUI_WIDGET_EVENT_SLOT_COUNT];
	void* arrEventUsers[XUI_WIDGET_EVENT_SLOT_COUNT];
	xui_tooltip_desc_t tTooltip;
	char* sTooltipText;
	xui_tooltip_resolve_proc onTooltipResolve;
	void* pTooltipUser;
	xui_widget_action_proc onDefaultAction;
	void* pDefaultActionUser;
	xui_widget_action_proc onCancelAction;
	void* pCancelActionUser;
	xui_widget_ime_rect_proc onImeCandidateRect;
	void* pImeCandidateRectUser;
	xui_cache_policy_t tCachePolicy;
	xui_widget_cache_render_proc onCacheRender;
	void* pCacheRenderUser;
	xui_widget_update_proc onUpdate;
	void* pUpdateUser;
	xui_widget_cache_slot_t* pCacheSlots;
	int iCacheCount;
	xui_table_track_t* pTableRows;
	xui_table_track_t* pTableColumns;
	int iTableRowCount;
	int iTableColumnCount;
	xui_draw_context pActiveUpdateDraw;
	xui_widget_cache_slot_t* pActiveUpdateSlot;
	uint32_t iActiveUpdateStateId;
};

struct xui_painter_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_surface pTarget;
	xui_draw_context pDraw;
};

int xuiInternalContextIsValid(xui_context pContext);
int xuiInternalContextHasProxy(xui_context pContext);
xui_proxy xuiInternalContextGetProxy(xui_context pContext);
int xuiInternalContextInvalidateRect(xui_context pContext, xui_rect_i_t tRect);
int xuiInternalContextInvalidateAll(xui_context pContext);
void xuiInternalContextBumpGeneration(xui_context pContext);
void xuiInternalContextDetachWidget(xui_context pContext, xui_widget pWidget);
void xuiInternalContextDestroyWidgetTypes(xui_context pContext);
void xuiInternalContextDestroyStyles(xui_context pContext);
void xuiInternalContextDestroyResources(xui_context pContext);
void xuiInternalContextDestroyRenderTree(xui_context pContext);
void xuiInternalContextDestroyInput(xui_context pContext);
int xuiInternalInputSyncIme(xui_context pContext);
int xuiInternalInputRefreshIme(xui_context pContext);
int xuiInternalInputRefreshImePosition(xui_context pContext);
int xuiInternalClipboardReadProxy(xui_proxy pProxy, char** psText, int* pTextSize);
int xuiInternalClipboardReadText(xui_context pContext, char** psText, int* pTextSize);
void xuiInternalContextPressCancel(xui_context pContext);
int xuiInternalContextPressUpdate(xui_context pContext, float fDelta);
int xuiInternalDrawPath(xui_proxy pProxy, xui_draw_context pDraw, xui_path pPath, const xui_path_style_t* pStyle, float fTolerance);
int xuiInternalTooltipPointerMove(xui_context pContext, xui_widget pHitWidget);
void xuiInternalTooltipCancel(xui_context pContext);
void xuiInternalTooltipDetachWidget(xui_context pContext, xui_widget pWidget);
int xuiInternalTooltipUpdate(xui_context pContext, float fDelta);

int xuiInternalWidgetIsValid(xui_widget pWidget);

static int xuiInternalPixelFloor(float fValue)
{
	int iValue;

	iValue = (int)fValue;
	if ( (float)iValue > fValue ) {
		iValue--;
	}
	return iValue;
}

static int xuiInternalPixelCeil(float fValue)
{
	int iValue;

	iValue = (int)fValue;
	if ( (float)iValue < fValue ) {
		iValue++;
	}
	return iValue;
}

static float xuiInternalSnapPixel(float fValue)
{
	return (float)xuiInternalPixelFloor(fValue + 0.5f);
}

static float xuiInternalSnapSize(float fValue)
{
	if ( fValue <= 0.0f ) {
		return 0.0f;
	}
	fValue = xuiInternalSnapPixel(fValue);
	return (fValue < 1.0f) ? 1.0f : fValue;
}

static xui_rect_t xuiInternalRectFromInt(xui_rect_i_t tRect)
{
	xui_rect_t tRet;

	tRet.fX = (float)tRect.iX;
	tRet.fY = (float)tRect.iY;
	tRet.fW = (float)tRect.iW;
	tRet.fH = (float)tRect.iH;
	return tRet;
}

static xui_rect_t xuiInternalSnapRect(xui_rect_t tRect)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = xuiInternalSnapPixel(tRect.fX);
	fTop = xuiInternalSnapPixel(tRect.fY);
	fRight = xuiInternalSnapPixel(tRect.fX + tRect.fW);
	fBottom = xuiInternalSnapPixel(tRect.fY + tRect.fH);
	tRect.fX = fLeft;
	tRect.fY = fTop;
	tRect.fW = fRight - fLeft;
	tRect.fH = fBottom - fTop;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return tRect;
}

static xui_rect_t xuiInternalSnapRectOut(xui_rect_t tRect)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = (float)xuiInternalPixelFloor(tRect.fX);
	fTop = (float)xuiInternalPixelFloor(tRect.fY);
	fRight = (float)xuiInternalPixelCeil(tRect.fX + tRect.fW);
	fBottom = (float)xuiInternalPixelCeil(tRect.fY + tRect.fH);
	tRect.fX = fLeft;
	tRect.fY = fTop;
	tRect.fW = fRight - fLeft;
	tRect.fH = fBottom - fTop;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return tRect;
}

static xui_rect_t xuiInternalSnapRectIn(xui_rect_t tRect)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = (float)xuiInternalPixelCeil(tRect.fX);
	fTop = (float)xuiInternalPixelCeil(tRect.fY);
	fRight = (float)xuiInternalPixelFloor(tRect.fX + tRect.fW);
	fBottom = (float)xuiInternalPixelFloor(tRect.fY + tRect.fH);
	tRect.fX = fLeft;
	tRect.fY = fTop;
	tRect.fW = fRight - fLeft;
	tRect.fH = fBottom - fTop;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return tRect;
}

static xui_rect_i_t xuiInternalRectToDamage(xui_rect_t tRect)
{
	xui_rect_i_t tDamage;
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;

	iLeft = xuiInternalPixelFloor(tRect.fX);
	iTop = xuiInternalPixelFloor(tRect.fY);
	iRight = xuiInternalPixelCeil(tRect.fX + tRect.fW);
	iBottom = xuiInternalPixelCeil(tRect.fY + tRect.fH);
	tDamage.iX = iLeft;
	tDamage.iY = iTop;
	tDamage.iW = iRight - iLeft;
	tDamage.iH = iBottom - iTop;
	return tDamage;
}

static xui_rect_t xuiInternalInsetRect(xui_rect_t tRect, float fInset)
{
	tRect.fX += fInset;
	tRect.fY += fInset;
	tRect.fW -= fInset * 2.0f;
	tRect.fH -= fInset * 2.0f;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return tRect;
}

static xui_rect_t xuiInternalStrokeCenterRectInside(xui_rect_t tRect, float fWidth, float* pRadius)
{
	float fSnapWidth;
	float fInset;

	fSnapWidth = xuiInternalSnapSize(fWidth);
	fInset = fSnapWidth * 0.5f;
	tRect = xuiInternalSnapRect(tRect);
	tRect = xuiInternalInsetRect(tRect, fInset);
	if ( pRadius != NULL ) {
		*pRadius = xuiInternalSnapPixel(*pRadius - fInset);
		if ( *pRadius < 0.0f ) {
			*pRadius = 0.0f;
		}
	}
	return tRect;
}

#endif /* XUI_INTERNAL_H */
