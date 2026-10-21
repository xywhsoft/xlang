#ifndef XUI_INTERNAL_H
#define XUI_INTERNAL_H

#include "../xui.h"
#include "xui_xrt_port.h"
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

typedef enum xui_internal_word_policy_t {
	XUI_INTERNAL_WORD_NATURAL = 0,
	XUI_INTERNAL_WORD_IDENTIFIER,
	XUI_INTERNAL_WORD_TERMINAL
} xui_internal_word_policy_t;

typedef enum xui_internal_word_kind_t {
	XUI_INTERNAL_WORD_SPACE = 0,
	XUI_INTERNAL_WORD_TEXT,
	XUI_INTERNAL_WORD_SYMBOL
} xui_internal_word_kind_t;

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

typedef struct xui_internal_window_frame_metrics_t {
	float fTitleBarHeight;
	float fBorderWidth;
	float fResizeGrip;
	float fButtonSize;
	float fButtonGap;
	float fButtonInset;
	uint32_t iResizeEdges;
	int bShowTitleBar;
	int bResizable;
} xui_internal_window_frame_metrics_t;

typedef struct xui_internal_window_frame_layout_t {
	xui_rect_t tFrameRect;
	xui_rect_t tTitleBarRect;
	xui_rect_t tClientRect;
} xui_internal_window_frame_layout_t;

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
	int fClickStartX;
	int fClickStartY;
	int fLastClickX;
	int fLastClickY;
	uint32_t iPointerButtons;
	int iActiveButton;
	int iDragButton;
	int iLastClickButton;
	int iClickCount;
	int bContextPressActive;
	int bContextPressMoved;
	int bContextPressFired;
	int bDragActive;
	int bClickMoved;
	double fLastClickTime;
};

typedef struct xui_widget_cache_slot_t {
	uint32_t iStateId;
	xui_surface pSurface;
	int iWidth;
	int iHeight;
	uint32_t iSurfaceGeneration;
	uint32_t iDpiGeneration;
	int iContentPolicy;
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
	uint32_t iDpiGeneration;
	xui_interaction_policy_t tInteractionPolicy;
	int bInteractionPolicyUserSet;
	int bInteractionPolicyPlatformSet;
	float fCaretBlinkElapsed;
	double fCaretBlinkClockSeconds;
	int bCaretBlinkClockValid;
	int bCaretBlinkVisible;
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
	struct xui_popup_data_t* pPopupList;
	void (*onPopupSubtreeUnavailable)(xui_context pContext, xui_widget pWidget);
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
	int fClickStartX;
	int fClickStartY;
	int fLastClickX;
	int fLastClickY;
	uint32_t iPointerButtons;
	uint32_t iInputModifiers;
	int iActiveButton;
	int iDragButton;
	int iLastClickButton;
	int iClickCount;
	int bContextPressActive;
	int bContextPressMoved;
	int bContextPressFired;
	int bDragActive;
	int bClickMoved;
	double fLastClickTime;
	xui_data_object pTransferData;
	xui_widget pDragSource;
	xui_widget pDropTarget;
	uint64_t iDragSessionId;
	uint64_t iDragRevision;
	struct xui_drag_dispatch_t* pDragDispatch;
	struct xui_drag_detach_t* pDragDetach;
	uint32_t iDragAllowedEffects;
	uint32_t iDragSuggestedEffect;
	uint32_t iDragEffect;
	uint32_t iDragLastEffect;
	int bTransferActive;
	int bTransferExternal;
	int bTransferDropping;
	xui_widget pContextPressWidget;
	xui_pointer_state_t arrPointerStates[XUI_POINTER_MAX];
	int iPointerStateCount;
	uint64_t iInputPointerId;
	int iInputPointerType;
	xui_widget pDeferredDestroyHead;
	xui_widget pStateChangeHead;
	xui_widget pStateChangeTail;
	int iDestroyFlushDepth;
	int iWidgetCallbackDepth;
	int iOperationDepth;
	int bDestroyPending;
	int bDestroying;
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
	xui_widget pAccessibilityHead;
	xui_widget pAccessibilityTail;
	uint32_t iAccessibilityFlushEpoch;
	int bAccessibilityFlushing;
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

typedef struct xui_internal_accessibility_adapter_t {
	int (*getNode)(xui_widget pWidget, xui_accessible_node_t* pNode);
	int (*performAction)(xui_widget pWidget, int iAction, const void* pData);
} xui_internal_accessibility_adapter_t;

typedef int (*xui_internal_prepare_paint_proc)(xui_widget pWidget);

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
	/* Sync dependent paint state before child caches; never draw or run layout here. */
	xui_internal_prepare_paint_proc onPreparePaint;
	xui_widget_cache_render_proc onCacheRender;
	xui_widget_update_proc onUpdate;
	xui_widget_cursor_proc onQueryCursor;
	const xui_internal_accessibility_adapter_t* pAccessibleAdapter;
	xui_layout_t tLayout;
	xui_cache_policy_t tCachePolicy;
	int iWidgetCount;
	int bBuiltin;
};

typedef struct xui_internal_edit_adapter_t {
	uint32_t iCapabilities;
	int (*setText)(xui_widget pWidget, const char* sText);
	const char* (*getText)(xui_widget pWidget);
	int (*setSelection)(xui_widget pWidget, int iStart, int iEnd);
	int (*getSelection)(xui_widget pWidget, int* pStart, int* pEnd);
	int (*hasSelection)(xui_widget pWidget);
	int (*selectAll)(xui_widget pWidget);
	int (*copy)(xui_widget pWidget);
	int (*cut)(xui_widget pWidget);
	int (*paste)(xui_widget pWidget);
	int (*deleteSelection)(xui_widget pWidget);
	int (*undo)(xui_widget pWidget);
	int (*redo)(xui_widget pWidget);
	int (*canUndo)(xui_widget pWidget);
	int (*canRedo)(xui_widget pWidget);
	int (*setReadonly)(xui_widget pWidget, int bReadonly);
	int (*isReadonly)(xui_widget pWidget);
	xui_rect_t (*getCaretRect)(xui_widget pWidget);
	int (*openContextMenu)(xui_widget pWidget, float fX, float fY);
} xui_internal_edit_adapter_t;

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
	xui_widget* pStackingChildren;
	int iStackingChildCount;
	int iStackingChildCapacity;
	int bStackingChildrenDirty;
	xui_widget pDeferredDestroyNext;
	xui_widget pStateChangeNext;
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
	int bModalFocus;
	int bDragEnabled;
	int bDropEnabled;
	int iImeMode;
	int bMeasureValid;
	int bArrangeValid;
	int bDestroyPending;
	int bInteractionCancelling;
	uint32_t iPendingStateChanges;
	xui_rect_t tPendingStateOldWorldRect;
	xui_rect_t tPendingDamageWorldRect;
	int bPendingDamageVisible;
	int bPendingStateOldVisible;
	int bPendingStateOldEnabled;
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
	uint32_t iDpiGeneration;
	uint32_t iStyleVersion;
	void* pUser;
	xui_widget pOverlayOwner;
	void* pTypeData;
	const xui_internal_edit_adapter_t* pEditAdapter;
	xui_widget pEditDelegate;
	xui_widget pEditOwner;
	xui_edit_behavior_t tEditBehavior;
	xui_edit_event_proc onEditEvent;
	void* pEditEventUser;
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
	uint32_t iAccessibleProviderGeneration;
	uint32_t iAccessibleMutationGeneration;
	uint32_t iPendingAccessibilityEvents;
	uint32_t iActiveAccessibilityEvents;
	uint32_t iAccessibilitySeenEpoch;
	uint32_t iAccessibilitySeenEvents;
	xui_widget pAccessibilityOldParent;
	xui_widget pAccessibilityPrev;
	xui_widget pAccessibilityNext;
	int bAccessibleQueryActive;
	int bAccessibleActionActive;
	char sAccessibleValue[64];
	xui_cache_policy_t tCachePolicy;
	xui_widget_cache_render_proc onCacheRender;
	void* pCacheRenderUser;
	xui_widget_update_proc onUpdate;
	void* pUpdateUser;
	xui_widget_cursor_proc onQueryCursor;
	void* pQueryCursorUser;
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
void xuiInternalOperationEnter(xui_context pContext);
void xuiInternalOperationLeave(xui_context pContext);
int xuiInternalContextDestroyPending(xui_context pContext);
void xuiInternalStateRecordBoundsTree(xui_widget pWidget);
void xuiInternalStateDamageFlush(xui_context pContext);
void xuiInternalStateRecordVisible(xui_widget pWidget);
void xuiInternalStateRecordEnabled(xui_widget pWidget);
void xuiInternalStateChangeFlush(xui_context pContext);
int xuiInternalLayoutCreateWidget(xui_widget pWidget);
void xuiInternalLayoutDestroyWidget(xui_widget pWidget);
int xuiInternalLayoutAttach(xui_widget pParent, xui_widget pChild, xui_widget pBefore);
void xuiInternalLayoutDetach(xui_widget pWidget);
void xuiInternalLayoutInvalidate(xui_widget pWidget, int bMeasure);
void xuiInternalWidgetSyncDpi(xui_widget pWidget);
int xuiInternalWidgetInvalidateArrange(xui_widget pWidget, uint32_t iPaintFlags);
int xuiInternalLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasured);
int xuiInternalLayoutArrange(xui_widget pWidget, xui_rect_t tRect);
int xuiInternalLayoutArrangeChild(xui_widget pParent, xui_widget pChild, xui_rect_t tRect);
int xuiInternalEnsureLayout(xui_context pContext);
int xuiInternalContextHasProxy(xui_context pContext);
xui_proxy xuiInternalContextGetProxy(xui_context pContext);
int xuiInternalSetInteractionPolicy(xui_context pContext, const xui_interaction_policy_t* pPolicy, int bUserSet);
void xuiInternalCaretBlinkUpdate(xui_context pContext, float fDelta);
void xuiInternalCaretBlinkReset(xui_context pContext);
int xuiInternalCaretBlinkVisible(xui_widget pWidget);
int xuiInternalContextInvalidateRect(xui_context pContext, xui_rect_i_t tRect);
int xuiInternalContextInvalidateAll(xui_context pContext);
void xuiInternalContextBumpGeneration(xui_context pContext);
void xuiInternalReportError(xui_context pContext, xui_widget pWidget, int iCode, int iStage,
	int bRecoverable, const char* sOperation, const char* sMessage);
void xuiInternalContextDetachWidget(xui_context pContext, xui_widget pWidget);
int xuiInternalInputCancelSubtree(xui_context pContext, xui_widget pWidget);
int xuiInternalInputCancelFocusSubtree(xui_context pContext, xui_widget pWidget);
void xuiInternalAccessibilityQueue(xui_widget pWidget, int iEventType);
void xuiInternalAccessibilityFlush(xui_context pContext);
void xuiInternalAccessibilityDetach(xui_widget pWidget);
int xuiInternalAccessibilityEditNode(xui_widget pWidget, xui_accessible_node_t* pNode);
int xuiInternalAccessibilityEditAction(xui_widget pWidget, int iAction, const void* pData);
xui_widget xuiInternalInputModalRoot(xui_context pContext);
xui_widget xuiInternalInputFocusParent(xui_widget pWidget);
int xuiInternalInputFocusContains(xui_widget pRoot, xui_widget pWidget);
int xuiInternalInputFocusAllowed(xui_context pContext, xui_widget pWidget);
void xuiInternalWidgetDestroyFlush(xui_context pContext);
void xuiInternalContextDestroyWidgetTypes(xui_context pContext);
void xuiInternalContextDestroyStyles(xui_context pContext);
void xuiInternalContextDestroyResources(xui_context pContext);
void xuiInternalContextDestroyRenderTree(xui_context pContext);
int xuiInternalStackingChildren(xui_widget pParent,
	const xui_widget** ppChildren, int* pCount);
void xuiInternalContextDestroyInput(xui_context pContext);
int xuiInternalInputSyncIme(xui_context pContext);
int xuiInternalInputRefreshIme(xui_context pContext);
int xuiInternalInputRefreshImePosition(xui_context pContext);
int xuiInternalClipboardReadProxy(xui_proxy pProxy, char** psText, int* pTextSize);
XUI_API int xuiInternalContextSetImeDetach(xui_context pContext, void (*onDetach)(xui_context pContext));
int xuiInternalClipboardReadText(xui_context pContext, char** psText, int* pTextSize);
int xuiInternalEditRegister(xui_widget pWidget, const xui_internal_edit_adapter_t* pAdapter,
	const xui_edit_behavior_t* pBehavior);
int xuiInternalEditDelegate(xui_widget pWidget, xui_widget pDelegate);
void xuiInternalEditUndelegate(xui_widget pWidget);
xui_widget xuiInternalEditHost(xui_widget pWidget);
const xui_edit_behavior_t* xuiInternalEditBehavior(xui_widget pWidget);
int xuiInternalEditEmit(xui_widget pWidget, int iType, const char* sText,
	int iSelectionStart, int iSelectionEnd, int iCompositionStart, int iCompositionEnd, int bValid);
int xuiInternalEditEmitSized(xui_widget pWidget, int iType, const char* sText, int iTextSize,
	int iSelectionStart, int iSelectionEnd, int iCompositionStart, int iCompositionEnd, int bValid);
/* Built-in XGE-only streaming path. Custom proxies retain textShape. */
typedef int (*xui_internal_text_cluster_visit_proc)(const xui_text_cluster_t*, void*);
int xuiInternalProxyXgeVisitTextShape(xui_proxy, xui_font, const char*, int, uint32_t,
    xui_internal_text_cluster_visit_proc, void*, xui_text_shape_t*);

int xuiInternalTextGraphemeNextRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemePrevRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemeClampRead(xui_internal_text_read_proc onRead, void* pUser, int iLength, int iOffset);
int xuiInternalTextGraphemeNext(const char* sText, int iLength, int iOffset);
int xuiInternalTextGraphemePrev(const char* sText, int iLength, int iOffset);
int xuiInternalTextGraphemeClamp(const char* sText, int iLength, int iOffset);
int xuiInternalTextWordBoundaryRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy);
xui_internal_word_kind_t xuiInternalTextWordRangeRead(xui_internal_text_read_proc onRead,
	void* pUser, int iLength, int iOffset, xui_internal_word_policy_t iPolicy,
	int* pStart, int* pEnd);
int xuiInternalTextWordPrevRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy);
int xuiInternalTextWordNextRead(xui_internal_text_read_proc onRead, void* pUser,
	int iLength, int iOffset, xui_internal_word_policy_t iPolicy);
int xuiInternalTextWordBoundary(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy);
xui_internal_word_kind_t xuiInternalTextWordRange(const char* sText, int iLength,
	int iOffset, xui_internal_word_policy_t iPolicy, int* pStart, int* pEnd);
int xuiInternalTextWordPrev(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy);
int xuiInternalTextWordNext(const char* sText, int iLength, int iOffset,
	xui_internal_word_policy_t iPolicy);
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
int xuiInternalDragAdornerSetColor(xui_context pContext, xui_widget pOwner, int iIndex, uint32_t iColor);
void xuiInternalDragAdornerHide(xui_context pContext, xui_widget pOwner);
int xuiInternalDragTransferMove(xui_context pContext, int iX, int iY,
	uint32_t iModifiers);
int xuiInternalDragTransferDrop(xui_context pContext, int iX, int iY,
	uint32_t iModifiers);
void xuiInternalDragTransferCancel(xui_context pContext);
void xuiInternalDragTransferDetachWidget(xui_context pContext, xui_widget pWidget);
void xuiInternalDragTransferShutdown(xui_context pContext);
void xuiInternalWindowFrameLayout(xui_rect_t tFrameRect,
	const xui_internal_window_frame_metrics_t* pMetrics,
	xui_internal_window_frame_layout_t* pLayout);
xui_rect_t xuiInternalWindowFrameTrailingButton(const xui_internal_window_frame_layout_t* pLayout,
	const xui_internal_window_frame_metrics_t* pMetrics, int iTrailingIndex);
uint32_t xuiInternalWindowFrameResizeEdgesAt(xui_rect_t tFrameRect,
	const xui_internal_window_frame_metrics_t* pMetrics, float fX, float fY);
int xuiInternalWindowFrameResizeCursor(uint32_t iEdges);
xui_rect_t xuiInternalWindowFrameClamp(xui_rect_t tRect, xui_rect_t tBounds,
	float fMinWidth, float fMinHeight);
xui_rect_t xuiInternalWindowFrameMove(xui_rect_t tRect, xui_rect_t tBounds,
	float fDX, float fDY);
xui_rect_t xuiInternalWindowFrameResize(xui_rect_t tRect, xui_rect_t tBounds,
	uint32_t iEdges, float fDX, float fDY, float fMinWidth, float fMinHeight);

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

/* Disjoint pieces of rect outside cover, in the same pixel coordinate space. */
static int xuiInternalRectSubtract(xui_rect_t tRect, xui_rect_t tCover, xui_rect_t* pParts)
{
	int64_t l = tRect.fX > tCover.fX ? tRect.fX : tCover.fX;
	int64_t t = tRect.fY > tCover.fY ? tRect.fY : tCover.fY;
	int64_t rr = (int64_t)tRect.fX + tRect.fW;
	int64_t rb = (int64_t)tRect.fY + tRect.fH;
	int64_t cr = (int64_t)tCover.fX + tCover.fW;
	int64_t cb = (int64_t)tCover.fY + tCover.fH;
	int64_t r = rr < cr ? rr : cr;
	int64_t b = rb < cb ? rb : cb;
	int n = 0;
	if ( tRect.fW <= 0 || tRect.fH <= 0 ) return 0;
	if ( tCover.fW <= 0 || tCover.fH <= 0 || r <= l || b <= t ) {
		pParts[0] = tRect;
		return 1;
	}
	if ( t > tRect.fY ) pParts[n++] = (xui_rect_t){tRect.fX, tRect.fY, tRect.fW, (int)(t - tRect.fY)};
	if ( b < rb ) pParts[n++] = (xui_rect_t){tRect.fX, (int)b, tRect.fW, (int)(rb - b)};
	if ( l > tRect.fX ) pParts[n++] = (xui_rect_t){tRect.fX, (int)t, (int)(l - tRect.fX), (int)(b - t)};
	if ( r < rr ) pParts[n++] = (xui_rect_t){(int)r, (int)t, (int)(rr - r), (int)(b - t)};
	return n;
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

static void xuiInternalContextMenuPoint(const xui_event_t* pEvent, xui_rect_t tKeyboardAnchor,
	float* pX, float* pY)
{
	float fX = 0.0f;
	float fY = 0.0f;
	if ( pEvent != NULL && pEvent->iKey != XUI_KEY_CONTEXT_MENU ) {
		fX = (float)pEvent->fX;
		fY = (float)pEvent->fY;
	} else {
		fX = (float)tKeyboardAnchor.fX;
		fY = (float)(tKeyboardAnchor.fY + tKeyboardAnchor.fH);
	}
	if ( pX != NULL ) *pX = fX;
	if ( pY != NULL ) *pY = fY;
}

#endif /* XUI_INTERNAL_H */
