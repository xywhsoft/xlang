#ifndef XUI_INTERNAL_H
#define XUI_INTERNAL_H

#include "../xui.h"
#include "../lib/xlayout/xlayout.h"

#define XUI_CONTEXT_MAGIC 0x58554943u
#define XUI_CONTEXT_DAMAGE_INLINE 8
#define XUI_CONTEXT_EVENT_INLINE 32
#define XUI_CONTEXT_HOTKEY_INLINE 16
#define XUI_CONTEXT_FONT_INLINE 16
#define XUI_DRAG_ADORNER_MAX_PRIMITIVES 16
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
typedef int (*xui_internal_text_read_proc)(void* pUser, int iOffset, unsigned char* pByte);

enum {
	XUI_DRAG_ADORNER_RECT_FILL = 1,
	XUI_DRAG_ADORNER_RECT_STROKE = 2
};

typedef struct xui_drag_adorner_primitive_t {
	int iType;
	xui_rect_t tRect;
	float fWidth;
	uint32_t iColor;
} xui_drag_adorner_primitive_t;



/*
 * XUI 的图模型使用 1 基槽位作为稳定 ID，0 表示无效槽位。
 * 底层仍使用当前 XRT 的 0 基 xarray，此处集中完成索引换算。
 */
static inline size_t __xuiArraySlotAdd(xarray* pArray, size_t iCount)
{
	size_t iSlot;

	if ( (pArray == NULL) || (iCount == 0u) || (pArray->Count == SIZE_MAX) ) return 0u;
	iSlot = pArray->Count + 1u;
	return (xrtArrayAdd(pArray, iCount) != NULL) ? iSlot : 0u;
}



/* 返回指定 1 基槽位的元素地址。 */
static inline void* __xuiArraySlotGet(xarray* pArray, size_t iSlot)
{
	if ( (pArray == NULL) || (iSlot == 0u) ) return NULL;
	return xrtArrayGet(pArray, iSlot - 1u);
}



/* 删除从指定 1 基槽位开始的连续元素。 */
static inline bool __xuiArraySlotRemove(xarray* pArray, size_t iSlot, size_t iCount)
{
	if ( (pArray == NULL) || (iSlot == 0u) ) return false;
	return xrtArrayRemove(pArray, iSlot - 1u, iCount);
}



/* 将 XUI 的字符串键参数转换为当前 XRT 字节键视图。 */
static inline xbytesview __xuiMapKey(const void* pKey, size_t iKeySize)
{
	xbytesview Key;

	Key.Data = (const unsigned char*)pKey;
	Key.Size = iKeySize;
	return Key;
}



/* 查询字符串键对应的可写值槽。 */
static inline void* __xuiMapGet(xmap* pMap, const void* pKey, size_t iKeySize)
{
	if ( (pMap == NULL) || ((pKey == NULL) && (iKeySize != 0u)) ) return NULL;
	return xrtMapGet(pMap, __xuiMapKey(pKey, iKeySize));
}



/* 查询或创建字符串键对应的值槽。 */
static inline void* __xuiMapGetOrAdd(xmap* pMap, const void* pKey, size_t iKeySize, bool* pNew)
{
	if ( (pMap == NULL) || ((pKey == NULL) && (iKeySize != 0u)) ) return NULL;
	return xrtMapGetOrAdd(pMap, __xuiMapKey(pKey, iKeySize), pNew);
}



/* 删除字符串键及其值槽。 */
static inline bool __xuiMapRemove(xmap* pMap, const void* pKey, size_t iKeySize)
{
	if ( (pMap == NULL) || ((pKey == NULL) && (iKeySize != 0u)) ) return false;
	return xrtMapRemove(pMap, __xuiMapKey(pKey, iKeySize));
}



/* 读取指针值映射。 */
static inline void* __xuiMapGetPtr(xmap* pMap, const void* pKey, size_t iKeySize)
{
	if ( (pMap == NULL) || ((pKey == NULL) && (iKeySize != 0u)) ) return NULL;
	return xrtMapGetPtr(pMap, __xuiMapKey(pKey, iKeySize));
}



/* 设置指针值，并按需返回被替换的旧指针。 */
static inline bool __xuiMapSetPtr(
	xmap* pMap,
	const void* pKey,
	size_t iKeySize,
	void* pValue,
	void** ppOldValue
)
{
	xbytesview Key;

	if ( ppOldValue != NULL ) *ppOldValue = NULL;
	if ( (pMap == NULL) || ((pKey == NULL) && (iKeySize != 0u)) ) return false;
	Key = __xuiMapKey(pKey, iKeySize);
	if ( ppOldValue != NULL ) *ppOldValue = xrtMapGetPtr(pMap, Key);
	return xrtMapSetPtr(pMap, Key, pValue);
}

struct xui_language_t {
	uint32_t iMagic;
	xui_context pContext;
	int iLanguageId;
	int iFallbackLanguageId;
	char* sCode;
	char* sName;
	xarray arrTexts;
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
	int fPointerX;
	int fPointerY;
	float fContextPressTime;
	int fContextPressStartX;
	int fContextPressStartY;
	int fContextPressLastX;
	int fContextPressLastY;
	int fDragStartX;
	int fDragStartY;
	int fLastClickX;
	int fLastClickY;
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
	xlayout_context_t* pLayoutContext;
	xlayout_track_t* pLayoutTrackScratch;
	uint32_t iLayoutTrackScratchCapacity;
	xui_proxy_t tProxy;
	xui_proxy_caps_t tProxyCaps;
	int bHasProxy;
	int fViewportWidth;
	int fViewportHeight;
	float fDpiScale;
	uint32_t iGeneration;
	xui_widget pRoot;
	xui_widget pOverlayRoot;
	xui_widget pDragAdornerWidget;
	xui_widget pDragAdornerOwner;
	xui_rect_t tDragAdornerBounds;
	xui_drag_adorner_primitive_t arrDragAdornerPrimitives[XUI_DRAG_ADORNER_MAX_PRIMITIVES];
	int iDragAdornerPrimitiveCount;
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
	int fTooltipMouseX;
	int fTooltipMouseY;
	int bTooltipOpen;
	int fPointerX;
	int fPointerY;
	float fContextPressTime;
	int fContextPressStartX;
	int fContextPressStartY;
	int fContextPressLastX;
	int fContextPressLastY;
	int fDragStartX;
	int fDragStartY;
	int fLastClickX;
	int fLastClickY;
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
	xui_layout_stats_t tLayoutStats;
	xui_render_stats_t tRenderStats;
	xui_error_proc onError;
	void* pErrorUser;
	int bReportingError;
	xui_accessibility_event_proc onAccessibilityEvent;
	void* pAccessibilityEventUser;
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
	xmap mapIconCategories;
	xarray arrIconCategories;
	xui_icon_category pIconCategories;
	xui_icon pIcons;
	uint32_t iIconGeneration;
	void (*onDestroyIcons)(xui_context pContext);
	xarray arrLanguages;
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
	void (*onImeDetach)(xui_context pContext);
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
	xui_widget_layout_prepare_proc onLayoutPrepare;
	xui_widget_layout_children_proc onLayoutChildren;
	xui_widget_layout_complete_proc onLayoutComplete;
	xui_widget_cache_render_proc onCacheRender;
	xui_widget_update_proc onUpdate;
	xui_layout_t tLayout;
	xui_cache_policy_t tCachePolicy;
	int iWidgetCount;
	int bBuiltin;
};

struct xui_widget_t {
	uint32_t iMagic;
	xlayout_node_t iLayoutNode;
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
	uint32_t iLayoutSyncedVersion;
	uint32_t iLayoutSyncedParentVersion;
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
	xui_widget_layout_prepare_proc onLayoutPrepare;
	void* pLayoutPrepareUser;
	xui_widget_layout_children_proc onLayoutChildren;
	void* pLayoutChildrenUser;
	xui_widget_layout_complete_proc onLayoutComplete;
	void* pLayoutCompleteUser;
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
	xui_accessible_count_proc onAccessibleCount;
	xui_accessible_get_proc onAccessibleGet;
	xui_accessible_action_proc onAccessibleAction;
	void* pAccessibleUser;
	char* sAccessibleName;
	char* sAccessibleDescription;
	uint32_t iAccessibilityRevision;
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
int xuiInternalLayoutCreateWidget(xui_widget pWidget);
void xuiInternalLayoutDestroyWidget(xui_widget pWidget);
int xuiInternalLayoutAttach(xui_widget pParent, xui_widget pChild, xui_widget pBefore);
void xuiInternalLayoutDetach(xui_widget pWidget);
void xuiInternalLayoutInvalidate(xui_widget pWidget, int bMeasure);
int xuiInternalLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasured);
int xuiInternalLayoutArrange(xui_widget pWidget, xui_rect_t tRect);
int xuiInternalContextHasProxy(xui_context pContext);
xui_proxy xuiInternalContextGetProxy(xui_context pContext);
int xuiInternalContextInvalidateRect(xui_context pContext, xui_rect_i_t tRect);
int xuiInternalContextInvalidateAll(xui_context pContext);
void xuiInternalContextBumpGeneration(xui_context pContext);
void xuiInternalReportError(xui_context pContext, xui_widget pWidget, int iCode, int iStage,
	int bRecoverable, const char* sOperation, const char* sMessage);
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
XUI_API int xuiInternalContextSetImeDetach(xui_context pContext, void (*onDetach)(xui_context pContext));
int xuiInternalClipboardReadText(xui_context pContext, char** psText, int* pTextSize);
int xuiInternalTextGraphemeNextRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemePrevRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemeClampRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemeNext(const char* sText, int iLength, int iOffset);
int xuiInternalTextGraphemePrev(const char* sText, int iLength, int iOffset);
int xuiInternalTextGraphemeClamp(const char* sText, int iLength, int iOffset);
typedef void (*xui_internal_rich_change_proc)(xui_rich_document pDocument, const xui_rich_change_t* pChange, void* pUser);
int xuiInternalRichDocumentAddObserver(xui_rich_document pDocument, xui_internal_rich_change_proc onChange, void* pUser);
int xuiInternalRichDocumentRemoveObserver(xui_rich_document pDocument, xui_internal_rich_change_proc onChange, void* pUser);
int xuiInternalRichDocumentNodeOffset(xui_rich_document pDocument, xui_rich_node pNode, int* pOffset);
void xuiInternalContextPressCancel(xui_context pContext);
int xuiInternalContextPressUpdate(xui_context pContext, float fDelta);
int xuiInternalDrawPath(xui_proxy pProxy, xui_draw_context pDraw, xui_path pPath, const xui_path_style_t* pStyle, float fTolerance);
int xuiInternalTooltipPointerMove(xui_context pContext, xui_widget pHitWidget);
void xuiInternalTooltipCancel(xui_context pContext);
void xuiInternalTooltipDetachWidget(xui_context pContext, xui_widget pWidget);
int xuiInternalTooltipUpdate(xui_context pContext, float fDelta);
int xuiInternalDragAdornerSet(xui_context pContext, xui_widget pOwner,
	const xui_drag_adorner_primitive_t* pPrimitives, int iPrimitiveCount);
void xuiInternalDragAdornerHide(xui_context pContext, xui_widget pOwner);

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

static int xuiInternalSnapPixel(float fValue)
{
	return xuiInternalPixelFloor(fValue + 0.5f);
}

static int xuiInternalSnapSize(float fValue)
{
	if ( fValue <= 0.0f ) {
		return 0;
	}
	return xuiInternalSnapPixel(fValue) < 1 ? 1 : xuiInternalSnapPixel(fValue);
}

static xui_rect_t xuiInternalRectFromInt(xui_rect_i_t tRect)
{
	xui_rect_t tRet;

	tRet.fX = tRect.iX;
	tRet.fY = tRect.iY;
	tRet.fW = tRect.iW;
	tRet.fH = tRect.iH;
	return tRet;
}

static xui_rect_t xuiInternalRectFromFloatNearest(float fX, float fY, float fW, float fH)
{
	xui_rect_t tRect;
	int iRight;
	int iBottom;

	tRect.fX = xuiInternalSnapPixel(fX);
	tRect.fY = xuiInternalSnapPixel(fY);
	iRight = xuiInternalSnapPixel(fX + fW);
	iBottom = xuiInternalSnapPixel(fY + fH);
	tRect.fW = iRight > tRect.fX ? iRight - tRect.fX : 0;
	tRect.fH = iBottom > tRect.fY ? iBottom - tRect.fY : 0;
	return tRect;
}

static xui_rect_t xuiInternalSnapRect(xui_rect_t tRect)
{
	if ( tRect.fW < 0 ) {
		tRect.fW = 0;
	}
	if ( tRect.fH < 0 ) {
		tRect.fH = 0;
	}
	return tRect;
}

static xui_rect_t xuiInternalSnapRectOut(xui_rect_t tRect)
{
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t xuiInternalSnapRectIn(xui_rect_t tRect)
{
	return xuiInternalSnapRect(tRect);
}

static xui_rect_i_t xuiInternalRectToDamage(xui_rect_t tRect)
{
	xui_rect_i_t tDamage;
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;

	iLeft = tRect.fX;
	iTop = tRect.fY;
	iRight = tRect.fX + tRect.fW;
	iBottom = tRect.fY + tRect.fH;
	tDamage.iX = iLeft;
	tDamage.iY = iTop;
	tDamage.iW = iRight - iLeft;
	tDamage.iH = iBottom - iTop;
	return tDamage;
}

static xui_rect_t xuiInternalInsetRect(xui_rect_t tRect, float fInset)
{
	int iInset = xuiInternalSnapPixel(fInset);
	tRect.fX += iInset;
	tRect.fY += iInset;
	tRect.fW -= iInset * 2;
	tRect.fH -= iInset * 2;
	if ( tRect.fW < 0 ) {
		tRect.fW = 0;
	}
	if ( tRect.fH < 0 ) {
		tRect.fH = 0;
	}
	return tRect;
}

static xui_rect_t xuiInternalStrokeCenterRectInside(xui_rect_t tRect, float fWidth, float* pRadius)
{
	int iWidth;

	iWidth = xuiInternalSnapSize(fWidth);
	if ( pRadius != NULL ) {
		*pRadius = (float)xuiInternalSnapPixel(*pRadius - (float)iWidth);
		if ( *pRadius < 0.0f ) {
			*pRadius = 0.0f;
		}
	}
	return xuiInternalSnapRect(tRect);
}

#endif /* XUI_INTERNAL_H */
