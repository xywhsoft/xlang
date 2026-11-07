#ifndef XUI_INTERNAL_H
#define XUI_INTERNAL_H

#include "xui.h"
#include "xui_xrt_port.h"
#include "xui_unicode_core.h"
#include "xui_owned_manager.h"
#include "../modules/xlayout/xlayout.h"

/* Private destroy-call policy, independent of native owner consumption.
 * Checked entry points never wait for a worker; legacy void destruction may.
 * Frames belong to a single creator-thread Context and are invalidated before
 * that Context is physically freed. They are not public call-scope ABI. */
typedef enum xui_internal_destroy_mode_t {
    XUI_DESTROY_NONBLOCKING = 0,
    XUI_DESTROY_SYNC_WORKER = 1
} xui_internal_destroy_mode_t;
typedef struct xui_internal_destroy_scope_t {
    xui_context pContext;
    struct xui_internal_destroy_scope_t* pPrevious;
    xui_internal_destroy_mode_t iMode;
} xui_internal_destroy_scope_t;
void xuiInternalDestroyScopeBegin(xui_context,xui_internal_destroy_mode_t,xui_internal_destroy_scope_t*);
void xuiInternalDestroyScopeEnd(xui_internal_destroy_scope_t*);
xui_internal_destroy_mode_t xuiInternalDestroyModeInherited(xui_context);
bool xuiInternalDestroyWaitAllowed(xui_context);

/* Private composite ownership. A live child retains its identity across
 * internal moves, including parent-change callbacks. No thread/global bypass. */
int xuiInternalWidgetRequestDestroy(xui_widget,const void* authority);
int xuiInternalWidgetDestroyReady(xui_widget);
int xuiInternalContextPrepareDestroy(xui_context,bool* complete);
int xuiInternalTreeViewCreatePart(xui_widget,xui_widget*,const xui_tree_view_desc_t*);
int xuiInternalBreadcrumbCreatePart(xui_widget,xui_widget*,const xui_breadcrumb_desc_t*);
int xuiInternalWidgetCreatePart(xui_widget pOwner,xui_widget_type pType,xui_widget* ppPart,const void* pCreateData);
int xuiInternalWidgetOverlayAttachPart(xui_widget pOwner,xui_widget pOverlayOwner,xui_widget pPart,int iLayer,int iZIndex);
int xuiInternalWidgetBindPartSlot(xui_widget pOwner,xui_widget pPart,xui_widget* ppStableDataSlot);
int xuiInternalWidgetClaimPart(xui_widget pOwner,xui_widget pPart);
int xuiInternalWidgetDestroyPart(xui_widget pOwner,xui_widget pPart);
int xuiInternalWidgetDetachPart(xui_widget pOwner,xui_widget pPart);
int xuiInternalWidgetInsertPart(xui_widget pOwner,xui_widget pParent,xui_widget pPart,xui_widget pBefore);

/* Private factories preserve each control validation and post-construction. */
int xuiInternalScrollBarCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_scrollbar_desc_t* pDesc);
int xuiInternalScrollFrameCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_scroll_frame_desc_t* pDesc);
int xuiInternalScrollViewCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_scroll_view_desc_t* pDesc);
int xuiInternalInputCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_input_desc_t* pDesc);
int xuiInternalNumericInputCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_numeric_input_desc_t* pDesc);
int xuiInternalComboBoxCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_combobox_desc_t* pDesc);
int xuiInternalColorPickerCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_color_picker_desc_t* pDesc);
int xuiInternalDatePickerCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_date_picker_desc_t* pDesc);
int xuiInternalPopupCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_popup_desc_t* pDesc);
int xuiInternalTextEditCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_text_edit_desc_t* pDesc);
int xuiInternalButtonCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_button_desc_t* pDesc);
int xuiInternalTableViewCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_table_view_desc_t* pDesc);
int xuiInternalTableGridCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_table_grid_desc_t* pDesc);
int xuiInternalLabelCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_label_desc_t* pDesc);
int xuiInternalImageCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_image_desc_t* pDesc);
int xuiInternalMenuCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_menu_desc_t* pDesc);
int xuiInternalWindowCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_window_desc_t* pDesc);
int xuiInternalCheckBoxCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_checkbox_desc_t* pDesc);

int xuiInternalListViewCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_list_view_desc_t* pDesc);

bool xuiInternalTextShapeTraceOwnership(const xui_text_shape_t* pShape,xrtownershipvisitor onVisit,void* pUser);
int xuiInternalTextShapeReady(const xui_text_shape_t* pShape,bool bManaged);
int xuiInternalTextLayoutDestroy(xui_context pContext,xui_text_layout pLayout);
int xuiInternalTextLayoutDestroyReady(xui_text_layout pLayout);
bool xuiInternalTextMeasureCacheTrace(xui_context,xrtownershipvisitor,void*);
int xuiInternalTextMeasureCacheReady(xui_context);
int xuiInternalTextMeasureCachePrepare(xui_context,bool* complete);
int xuiInternalCleanupCallScopeBegin(xui_context,xui_call_scope_t*);
/* Stack-only native slot transaction. No language bridge or per-widget map. */
typedef struct xui_resource_slot_update_t {
    xui_call_scope_t scope;
    xui_context context;
    xui_widget widget;
    void* slot;
    void* previous;
    void* current;
    void* guarded;
    struct xui_resource_slot_update_t* next;
    bool prepared,published,restored,preserveOld,ownsSlots,newHeld,guardHeld;
    uint8_t kind;
} xui_resource_slot_update_t;
typedef xui_resource_slot_update_t xui_font_slot_update_t;
typedef xui_resource_slot_update_t xui_surface_slot_update_t;
int xuiInternalResourceSlotPublish(xui_resource_slot_update_t* update);
int xuiInternalResourceSlotRestore(xui_resource_slot_update_t* update);
int xuiInternalResourceSlotEnd(xui_resource_slot_update_t* update,bool commit,int status);
#define xuiInternalFontSlotPublish xuiInternalResourceSlotPublish
#define xuiInternalFontSlotRestore xuiInternalResourceSlotRestore
#define xuiInternalFontSlotEnd xuiInternalResourceSlotEnd
#define xuiInternalSurfaceSlotPublish xuiInternalResourceSlotPublish
#define xuiInternalSurfaceSlotRestore xuiInternalResourceSlotRestore
#define xuiInternalSurfaceSlotEnd xuiInternalResourceSlotEnd
int xuiInternalFontSlotPrepare(xui_context,xui_font*,xui_font,bool,bool,xui_font_slot_update_t*);
int xuiInternalFontSlotBegin(xui_context,xui_font*,xui_font,bool,xui_font_slot_update_t*);
int xuiInternalSurfaceSlotPrepare(xui_context,xui_surface*,xui_surface,bool,bool,xui_surface_slot_update_t*);
int xuiInternalSurfaceSlotBegin(xui_context,xui_surface*,xui_surface,bool,xui_surface_slot_update_t*);
int xuiInternalSurfaceSlotsPrepare(xui_widget,size_t,xui_surface* const*,const xui_surface*,xui_surface_slot_update_t*);
int xuiInternalSurfaceSlotsPublish(xui_surface_slot_update_t*,size_t);
int xuiInternalSurfaceSlotsEnd(xui_surface_slot_update_t*,size_t,bool,int);
int xuiInternalFontAdmit(xui_context context,xui_font font);
int xuiInternalFontRelease(xui_context context,xui_font font);
int xuiInternalSurfaceAdmit(xui_context context,xui_surface surface);
int xuiInternalSurfaceRelease(xui_context context,xui_surface surface);
xui_proxy xuiInternalProxyCanonical(const xui_proxy_t* proxy);
bool xuiInternalProxyReady(const xui_proxy_t* proxy);
bool xuiInternalProxyCreator(const xui_proxy_t* proxy);
bool xuiInternalContextTraceRegistry(xui_context,xrtownershipvisitor,void*);
bool xuiInternalContextTraceIcons(xui_context,xrtownershipvisitor,void*);
/* An explicitly declared primary outcome across implicit void cleanup.
 * Caller owns error for the entire LIFO scope and already protects Context
 * activity on its UI thread. This is neither TLS nor an inferred parent error. */
typedef struct xui_cleanup_prefix_t {
    const void* self;
    xui_context context;
    struct xui_cleanup_prefix_t* previous;
    uint64_t thread;
    int status;
    const xerror* error;
} xui_cleanup_prefix_t;
int xuiInternalCleanupPrefixBegin(xui_context,xui_cleanup_prefix_t*,int,const xerror*);
int xuiInternalCleanupPrefixEnd(xui_cleanup_prefix_t*);
void xuiInternalContextRecordFailure(xui_context context,int status);
/* Borrows error, retaining one reference in each still-successful receipt.
 * status is independent of nullable error; callers that took TLS pass it here. */
void xuiInternalContextRecordOutcome(xui_context context,int status,const xerror* error);
/* Consuming cleanup is sticky in enclosing receipts; an ordinary returned
 * operation failure is local and can be handled by a native caller. */
static inline void xuiInternalContextCleanupOutcome(xui_context context,int next,int* status,xerror** first)
{
    xerror* error=xrtTakeError();if(next==XUI_OK && !error)return;
    if(next==XUI_OK)next=XUI_ERROR_CALLBACK_FAILED;
    xuiInternalContextRecordOutcome(context,next,*status!=XUI_OK ? *first:error);
    if(*status==XUI_OK){*status=next;*first=error;}else xrtErrorFree(error);
}

enum { XUI_CALL_SCOPE_ENDING=1, XUI_CALL_SCOPE_INHERITED=2, XUI_CALL_SCOPE_PROPAGATED=4 };
static inline void xuiInternalCallScopeRecordOutcome(xui_call_scope_t* scope,int status,const xerror* error)
{
    if(!scope || scope->pSelf!=scope || scope->iResult!=XUI_OK || (status==XUI_OK && !error))return;
    xerror* next=xrtErrorRef(error);
    /* While successful this tagged slot owns the entry ambient diagnostic.
     * The first actual failure replaces it, including failed-with-NULL. */
    xrtErrorFree(scope->_pFirstError);
    scope->_pFirstError=next;
    scope->iResult=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;
}

/* Finish an owned local outcome through the authoritative enclosing receipt.
 * The caller has detached *first from TLS and owns its reference. */
static inline int xuiInternalCallScopeEndOutcome(xui_call_scope_t* scope,int status,xerror** first)
{
    const xerror* ambient=scope && scope->pSelf==scope && scope->iResult==XUI_OK ? scope->_pFirstError:NULL;
    bool inherited=scope && scope->pSelf==scope && (scope->bEnding&XUI_CALL_SCOPE_INHERITED)!=0;
    xuiInternalCallScopeRecordOutcome(scope,status,*first);
    int ended=xuiCallScopeEnd(scope);xerror* error=xrtTakeError();
    if(status!=XUI_OK && !inherited){xrtErrorFree(error);return status;}
    if(ended!=XUI_OK){xrtErrorFree(*first);*first=error;return ended;}
    /* End preserves the old ambient diagnostic on success. Carry its owned
     * reference through the caller's final SetErrorTake without calling it a
     * new failure. status remains the authoritative failure tag. */
    if(error==ambient){*first=error;return XUI_OK;}
    if(error){*first=error;return XUI_ERROR_CALLBACK_FAILED;}
    return status;
}

bool xuiInternalContextTraceResources(xui_context context,xrtownershipvisitor visit,void* user);
/* C-stack receipt, not a TLS stack. Preserve an explicit failed bit even when
 * no diagnostic could allocate, and keep the first void-hook diagnostic. */
typedef struct xui_cleanup_receipt_t {int status;xerror* error;} xui_cleanup_receipt_t;
static inline void xuiInternalCleanupStart(xui_cleanup_receipt_t* receipt,int status)
{
    receipt->error=xrtTakeError();
    receipt->status=status!=XUI_OK ? status:receipt->error ? XUI_ERROR_CALLBACK_FAILED:XUI_OK;
}
static inline void xuiInternalCleanupAdd(xui_cleanup_receipt_t* receipt,int status)
{
    xerror* error=xrtTakeError();
    if(status==XUI_OK && !error)return;
    if(receipt->status==XUI_OK){receipt->status=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;receipt->error=error;}
    else xrtErrorFree(error);
}
static inline int xuiInternalCleanupEnd(xui_context context,xui_cleanup_receipt_t* receipt)
{
    xuiInternalContextRecordOutcome(context,receipt->status,receipt->error);
    xrtSetErrorTake(receipt->error);receipt->error=NULL;return receipt->status;
}


void xuiInternalTableGridConfigProject(const xui_table_grid_editor_config_output_t* pOutput,xui_table_grid_editor_config_t* pConfig);

/* Native Cell output read protection; last consumer ends it. */
int xuiInternalTableCellMove(xui_table_view_cell_output_t* pTarget, xui_table_view_cell_output_t* pSource);
int xuiInternalTableCellReadBegin(xui_table_view_cell_output_t* pCell);
int xuiInternalTableCellReadEnd(xui_table_view_cell_output_t* pCell);
const xui_table_view_cell_output_t* xuiInternalTableCellSource(const xui_table_view_cell_output_t* pCell);

/* Caller holds a Cell read pin; only scalar/owned text projections escape. */
static inline void xuiInternalTableCellProject(xui_table_view_cell_t* raw,const xui_table_view_cell_output_t* cell)
{
    const xui_table_view_cell_output_t* source=cell->_pSource ? cell->_pSource:cell;
    raw->sText=source->_tText.Data;
    raw->sTooltip=source->_tTooltip.Data;
    raw->iType=cell->iType;raw->iRowSpan=cell->iRowSpan;raw->iColSpan=cell->iColSpan;
    raw->bDisabled=cell->bDisabled;raw->bEditing=cell->bEditing;raw->bInvalid=cell->bInvalid;
    raw->bDirty=cell->bDirty;raw->bHasStyle=cell->bHasStyle;raw->bReadOnly=cell->bReadOnly;
    raw->iBackgroundColor=cell->iBackgroundColor;raw->iTextColor=cell->iTextColor;raw->iGridColor=cell->iGridColor;
    /* A raw pValue cannot carry a language xvalue or its ownership contract. */
    raw->pValue=NULL;raw->onFormat=NULL;raw->pFormatUser=NULL;raw->onRender=NULL;raw->pRenderUser=NULL;
}

/* Format a read-pinned Cell under an existing full native Context operation.
 * End follows the last text consumer and frees owned output before owner Drop. */
typedef struct xui_table_cell_format_t {
    xstrview text;
    xui_callback_owner_t owner;
    xui_context context;
    bool held,owned;
} xui_table_cell_format_t;
int xuiInternalTableCellFormatBegin(xui_widget pWidget,int iRow,int iColumn,const xui_table_view_cell_output_t* pCell,xui_table_cell_format_t* pFormat);
int xuiInternalTableCellFormatEnd(xui_table_cell_format_t* pFormat,int iResult);
/* Consumes the detached primary reference. */
int xuiInternalTableCellFormatEndOutcome(xui_table_cell_format_t* pFormat,int iResult,xerror* pFirst);

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
struct xui_menu_data_t;
/* Sparse ownership only: callback functions and userdata remain in their
 * original, exactly typed native slots. Keys identify widget-lifetime slots. */
typedef struct xui_callback_binding_t {
    struct xui_callback_binding_t* pNext;
    const void* pSlot;
    void* pUser; /* Independent of the ownership reference's Data. */
    xui_callback_owner_t tOwner;
} xui_callback_binding_t;
typedef struct xui_event_owners_t {
	xui_callback_owner_t tPrimary;
	xui_callback_owner_t* pHandlers;
    xui_callback_binding_t* pBindings;
} xui_event_owners_t;
typedef struct xui_callback_update_t {
    xui_widget pWidget;
    xui_context pContext;
    xui_call_scope_t tScope;
    xrtownershipscope tMutation;
    xui_callback_owner_t tOld;
    xui_callback_binding_t* pRetired;
    bool bPublished;
} xui_callback_update_t;
/* Multi-slot publications retain and allocate all candidates before one
 * ownership mutation. Entries stay in caller-owned stable stack storage. */
typedef struct xui_callback_group_entry_t {
    const void* pSlot;
    bool bHasCallback;
    void* pUser;
    const xui_callback_owner_t* pOwner;
    xui_callback_owner_t tCandidate;
    xui_callback_owner_t tOld;
    xui_callback_binding_t* pAllocated;
    xui_callback_binding_t* pRetired;
    bool bHeld;
} xui_callback_group_entry_t;
typedef struct xui_callback_group_update_t {
    xui_widget pWidget;
    xui_context pContext;
    xui_call_scope_t tScope;
    xrtownershipscope tMutation;
    xui_callback_group_entry_t* pEntries;
    size_t iCount;
    bool bPublished;
} xui_callback_group_update_t;

typedef struct xui_callback_snapshot_t {
    xui_context pContext;
    xui_callback_owner_t tOwner;
} xui_callback_snapshot_t;

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

typedef struct xui_surface_dependency_t {
	xui_surface pSurface;
	uint32_t iGeneration;
	int bGenerationValid;
} xui_surface_dependency_t;

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
	xui_lifetime_link_t* pLifetimeLinks;
	xlayout_context_t* pLayoutContext;
	xlayout_track_t* pLayoutTrackScratch;
	uint32_t iLayoutTrackScratchCapacity;
    xui_internal_destroy_mode_t iPendingDestroyMode; /* Uses pointer-alignment padding. */
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
	/* Actual context ownership, including unattached widgets. Tree membership
	 * never determines whether the context must reclaim an instance. */
	xui_widget pAllWidgets;
	xui_widget pDragAdornerWidget;
	xui_widget pDragAdornerOwner;
	xui_rect_t tDragAdornerBounds;
	xui_drag_adorner_primitive_t arrDragAdornerPrimitives[XUI_DRAG_ADORNER_MAX_PRIMITIVES];
	int iDragAdornerPrimitiveCount;
	xui_widget pHoverWidget;
	xui_widget pActiveWidget;
	xui_widget pFocusWidget;
	xui_widget pActiveWindow;
	/* MenuBar menu-mode focus is context-owned so widget detachment can clear
	 * both borrowed handles before deferred destruction releases memory. */
	xui_widget pMenuBarModeOwner;
	xui_widget pMenuBarFocusRestore;
	struct xui_popup_data_t* pPopupList;
	struct xui_menu_data_t* pMenuList;
	struct xui_menubar_data_t* pMenuBarList;
	void (*onPopupSubtreeUnavailable)(xui_context pContext, xui_widget pWidget);
	int (*onPopupLayoutChanged)(xui_context pContext);
	void (*onMenuUnavailable)(xui_context pContext, xui_widget pMenu);
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
	uint32_t iDestroyVisitEpoch;
	int iDestroyFlushDepth;
	int iWidgetCallbackDepth;
	int iOperationDepth;
	xui_call_scope_t* pCallScopes;
	int bDestroyPending;
	int bCloseRequested;
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
	xui_widget pRenderHead;
	xui_widget pRenderTail;
	int iRenderLinkedNodeCount;
	int bRenderIndexDirty;
	uint32_t iRootStructureRevision;
	uint32_t iOverlayStructureRevision;
	uint64_t iDirtyPropagationTotal;
	uint64_t iEventMaskPropagationTotal;
	uint64_t iDirtyPropagationSnapshot;
	uint64_t iEventMaskPropagationSnapshot;
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
	uint32_t iResourceCascadeMark;
	uint64_t iResourceRegistryGeneration;
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
	xui_font pSystemFont; /* Context-owned lazy platform font, separate from the active default slot. */
	xui_context_desc_t tCreateDesc;
	int iSystemFontStatus; /* 0 not attempted, XUI_OK loaded, or a stable negative result. */
	int bSystemFontLoading;
    xui_proxy pOwnedProxy; /* one real capability owner; tProxy is a borrowed table copy */
    xui_owned_manager_t* pOwnedManagers;
    xui_owned_manager_t* pPendingManagers;
    xui_cleanup_prefix_t* pCleanupPrefixes;
    xui_text_layout pMeasureLayout;
    int bMeasureLayoutActive;
    int iDestroyOwnership; /* 0 caller-owned, 1 void-close pending, 2 preparing; uses padding. */
    xui_resource_slot_update_t* pActiveResourceUpdates; /* stack nodes, no allocation */
    xui_internal_destroy_scope_t* pDestroyScopes; /* creator-thread stack, no allocation */
	xui_font_entry_t arrInlineFonts[XUI_CONTEXT_FONT_INLINE];
	xui_font_entry_t* pFonts;
	int iFontCount;
	int iFontCapacity;
	int bImeEnabled;
	int bImeCompositionActive;
	uint64_t iImeOwner;
	void (*onImeFocusChanged)(xui_context pContext);
	int bHasImeCandidateRect;
	xui_rect_t tImeCandidateRect;
	void (*onImeDetach)(xui_context pContext);
};

struct xui_internal_accessible_text_request;
struct xui_internal_accessible_text_range;
typedef struct xui_internal_accessibility_adapter_t {
	int (*getNode)(xui_widget pWidget, xui_accessible_node_t* pNode);
	int (*performAction)(xui_widget pWidget, int iAction, const void* pData);
	int (*createTextRange)(xui_widget, uint64_t,
		const struct xui_internal_accessible_text_request*,
		struct xui_internal_accessible_text_range**);
} xui_internal_accessibility_adapter_t;
typedef int (*xui_internal_accessible_focus_proc)(xui_widget pWidget,
	uint64_t* pNodeId, void* pUser);

typedef int (*xui_internal_prepare_paint_proc)(xui_widget pWidget);
typedef void (*xui_internal_parent_changed_proc)(xui_widget pWidget,
	xui_widget pOldParent, xui_widget pNewParent);
/* Pure metadata for actual Surface fields in a variable item array. The
 * borrowed layout is read only under the Context activity/ownership gate. */
typedef struct xui_surface_slot_array_t {
    void* data;
    size_t count,stride;
    const size_t* offsets;
    size_t fields;
} xui_surface_slot_array_t;
typedef void (*xui_internal_surface_slots_proc)(xui_widget,void*,xui_surface_slot_array_t*);

struct xui_widget_type_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget_type pParent;
	xui_widget_type pNext;
	char* sName;
	uint32_t iFlags;
	size_t iTypeDataSize;
    const size_t* pFontOffsets;
    size_t iFontSlotCount;
    const size_t* pSurfaceOffsets;
    size_t iSurfaceSlotCount;
    xui_internal_surface_slots_proc onSurfaceSlots;
    int (*onDestroyReady)(xui_widget,const void*);
    int (*onDestroyPrepare)(xui_widget,void*,bool* complete);
	void* pUser;
	xui_widget_type_init_proc onInit;
	xui_widget_type_destroy_proc onDestroy;
	xui_widget_type_ownership_trace_proc onTraceOwnership;
	xui_widget_content_measure_proc onContentMeasure;
	xui_widget_layout_prepare_proc onLayoutPrepare;
	xui_widget_layout_children_proc onLayoutChildren;
	xui_widget_layout_complete_proc onLayoutComplete;
	/* Sync dependent paint state before child caches; never draw or run layout here. */
	xui_internal_prepare_paint_proc onPreparePaint;
	xui_widget_cache_render_proc onCacheRender;
	xui_widget_update_proc onUpdate;
	xui_widget_cursor_proc onQueryCursor;
	/* Internal structural hook. Implementations must not mutate the widget tree. */
	xui_internal_parent_changed_proc onParentChanged;
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

typedef int (*xui_internal_edit_prepare_proc)(xui_widget pHost, xui_widget pSource,
	int iType, int* pSkipDefault, void* pUser);

struct xui_widget_t {
	uint32_t iMagic;
    uint32_t iDestroyVisitEpoch; /* Reuses the natural pointer alignment gap. */
	xui_lifetime_link_t* pLifetimeLinks;
	xlayout_node_t iLayoutNode;
	xui_context pContext;
	xui_widget_type pType;
	xui_widget pContextPrev;
	xui_widget pContextNext;
	xui_widget pParent;
    xui_widget pFirstOwnedPart;
    xui_widget pOwnedPartNext;
    xui_widget* ppOwnedPartPrevious;
    xui_widget_type pInitializedType; /* Last successful init stage; also on failed construction. */
    const void* pInternalOwner; /* Native construction identity; never a public bypass flag. */
    xui_widget* ppInternalOwnerSlot; /* Exact fixed TypeData slot; never a stack/relocatable slot. */
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
	int bTypeInitialized;
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
	xui_render_node_t tRenderNode;
	xui_widget pRenderPrev;
	xui_widget pRenderNext;
	xui_widget pRenderSubtreeLast;
	int bRenderNodeLinked;
	int bRenderGeometryDirty;
	int bRenderStructureDirty;
	int bSubtreeRenderStructureDirty;
	int bSubtreePrepareFrameDependent;
	int bSubtreeHasUpdate;
	int bSubtreeHasInactiveUpdate;
	uint32_t iStateId;
	uint32_t iGeneration;
	uint32_t iLayoutVersion;
	uint32_t iContentMeasureGeneration;
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
	xui_internal_edit_prepare_proc onEditPrepare;
	void* pEditPrepareUser;
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
	xui_event_owners_t* pEventOwners;
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
	xui_internal_accessible_focus_proc onAccessibleFocus;
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
    int bCacheMutationActive; /* Uses the pointer-alignment gap after count. */
	xui_table_track_t* pTableRows;
	xui_table_track_t* pTableColumns;
	int iTableRowCount;
	int iTableColumnCount;
	xui_draw_context pActiveUpdateDraw;
	xui_widget_cache_slot_t* pActiveUpdateSlot;
	uint32_t iActiveUpdateStateId;
    uint32_t iCacheEpoch; /* Nonwrapping identity; uses the trailing alignment gap. */
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
int xuiInternalCallbackOwnerValid(const xui_callback_owner_t*);
/* Pure native preflight runs after reentrant Hold, before allocation/mutation.
 * Widget publishers use the existing serialized UI-thread contract. */
typedef int (*xui_callback_preflight_proc)(xui_widget pWidget,void* pData);
int xuiInternalCallbackUpdateBeginValidated(xui_widget pWidget,const void* pSlot,bool bHasCallback,void* pUser,
    const xui_callback_owner_t* pOwner,xui_callback_preflight_proc onPreflight,void* pPreflightData,xui_callback_update_t* pUpdate);
int xuiInternalCallbackUpdateBegin(xui_widget,const void*,bool,void*,const xui_callback_owner_t*,xui_callback_update_t*);
void xuiInternalCallbackUpdatePublish(xui_callback_update_t*);
int xuiInternalCallbackUpdateEnd(xui_callback_update_t*,int);
int xuiInternalCallbackGroupUpdateBegin(xui_widget,xui_callback_group_entry_t*,size_t,xui_callback_group_update_t*);
void xuiInternalCallbackGroupUpdatePublish(xui_callback_group_update_t*);
int xuiInternalCallbackGroupUpdateEnd(xui_callback_group_update_t*,int);
/* Returned: the consumer propagates negative status. Reported: a void/value/
 * veto consumer has no status channel, so admission failure reaches receipts. */
int xuiInternalCallbackSnapshotBeginReturned(xui_widget,const void*,void**,xui_callback_snapshot_t*);
int xuiInternalCallbackSnapshotBeginReported(xui_widget,const void*,void**,xui_callback_snapshot_t*);
int xuiInternalCallbackSnapshotEnd(xui_callback_snapshot_t*,int);
/* Consumes the detached primary error reference, including NULL on failure. */
int xuiInternalCallbackSnapshotEndOutcome(xui_callback_snapshot_t* snapshot,int result,xerror* first);
bool xuiInternalCallbackSlotOwned(xui_widget,const void*);
int xuiInternalCallbackOwnerHold(const xui_callback_owner_t* pOwner);
int xuiInternalCallbackOwnerDrop(xui_context pContext, const xui_callback_owner_t* pOwner);
int xuiInternalCallbackOwnerDropOutcome(xui_context pContext,const xui_callback_owner_t* pOwner,int iPrimaryStatus,const xerror* pPrimaryError);
static inline int xuiInternalInvokeContentMeasure(xui_widget widget,xui_vec2_t constraint,xui_vec2_t* output)
{
    xui_widget_content_measure_proc callback=widget->onContentMeasure;void* user=widget->pContentMeasureUser;
    if(!callback)return XUI_OK;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return callback(widget,constraint,output,user);
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onContentMeasure,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,constraint,output,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

static inline int xuiInternalInvokeLayoutPrepare(xui_widget widget)
{
    xui_widget_layout_prepare_proc callback=widget->onLayoutPrepare;void* user=widget->pLayoutPrepareUser;
    if(!callback)return XUI_OK;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return callback(widget,user);
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onLayoutPrepare,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

static inline int xuiInternalInvokeLayoutChildren(xui_widget widget,xui_rect_t content)
{
    xui_widget_layout_children_proc callback=widget->onLayoutChildren;void* user=widget->pLayoutChildrenUser;
    if(!callback)return XUI_OK;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return callback(widget,content,user);
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onLayoutChildren,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,content,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

static inline int xuiInternalInvokeLayoutComplete(xui_widget widget,xui_rect_t content)
{
    xui_widget_layout_complete_proc callback=widget->onLayoutComplete;void* user=widget->pLayoutCompleteUser;
    if(!callback)return XUI_OK;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return callback(widget,content,user);
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onLayoutComplete,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,content,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

static inline int xuiInternalInvokeDefaultAction(xui_widget widget)
{
    xui_widget_action_proc callback=widget->onDefaultAction;void* user=widget->pDefaultActionUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){callback(widget,user);return XUI_OK;}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReported(widget,&widget->onDefaultAction,&user,&snapshot);
    if(result!=XUI_OK)return XUI_ERROR_INVALID_STATE;
    callback(widget,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,XUI_OK);
}

static inline int xuiInternalInvokeCancelAction(xui_widget widget)
{
    xui_widget_action_proc callback=widget->onCancelAction;void* user=widget->pCancelActionUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){callback(widget,user);return XUI_OK;}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReported(widget,&widget->onCancelAction,&user,&snapshot);
    if(result!=XUI_OK)return XUI_ERROR_INVALID_STATE;
    callback(widget,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,XUI_OK);
}

static inline xui_rect_t xuiInternalInvokeImeCandidateRect(xui_widget widget)
{
    xui_widget_ime_rect_proc callback=widget->onImeCandidateRect;void* user=widget->pImeCandidateRectUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){return callback(widget,user);}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReported(widget,&widget->onImeCandidateRect,&user,&snapshot);
    if(result!=XUI_OK)return (xui_rect_t){0};
    xui_rect_t rect=callback(widget,user);
    (void)xuiInternalCallbackSnapshotEnd(&snapshot,XUI_OK);return rect;
}

static inline int xuiInternalInvokeCursorQueryCallback(xui_widget widget,int x,int y)
{
    xui_widget_cursor_proc callback=widget->onQueryCursor;void* user=widget->pQueryCursorUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){return callback(widget,x,y,user);}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReported(widget,&widget->onQueryCursor,&user,&snapshot);
    if(result!=XUI_OK)return -1;
    int cursor=callback(widget,x,y,user);
    if(xuiInternalCallbackSnapshotEnd(&snapshot,XUI_OK)!=XUI_OK)return -1;
    return cursor;
}

static inline int xuiInternalInvokeCacheRenderCallback(xui_widget widget,xui_draw_context draw,uint32_t state)
{
    xui_widget_cache_render_proc callback=widget->onCacheRender;void* user=widget->pCacheRenderUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){return callback(widget,draw,state,user);}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onCacheRender,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,draw,state,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

static inline int xuiInternalInvokeUpdateCallback(xui_widget widget,float delta)
{
    xui_widget_update_proc callback=widget->onUpdate;void* user=widget->pUpdateUser;
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings){return callback(widget,delta,user);}
    xui_callback_snapshot_t snapshot={0};
    int result=xuiInternalCallbackSnapshotBeginReturned(widget,&widget->onUpdate,&user,&snapshot);
    if(result!=XUI_OK)return result;
    result=callback(widget,delta,user);
    return xuiInternalCallbackSnapshotEnd(&snapshot,result);
}

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
int xuiInternalWidgetInvalidatePaintIfNeeded(xui_widget pWidget);
void xuiInternalWidgetMarkDirtyFlags(xui_widget pWidget, uint32_t iFlags);
int xuiInternalListViewSetVirtualItemsResetSelection(xui_widget pWidget, int iCount,
	xui_list_view_text_proc onText, void* pUser);
int xuiInternalLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasured);
int xuiInternalLayoutArrange(xui_widget pWidget, xui_rect_t tRect);
int xuiInternalLayoutArrangeChild(xui_widget pParent, xui_widget pChild, xui_rect_t tRect);
int xuiInternalEnsureLayout(xui_context pContext);
int xuiInternalContextHasProxy(xui_context pContext);
xui_proxy xuiInternalContextGetProxy(xui_context pContext);
int xuiInternalSurfaceDependencySync(xui_proxy pProxy, xui_surface pSurface,
	xui_surface_dependency_t* pDependencies, int iIndex);
void xuiInternalUtf8TrimIncomplete(char* sText);
size_t xuiInternalUtf8CopyGraphemes(char* sDst, size_t iCapacity, const char* sSource);
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
XUI_API int xuiInternalAccessibilitySetFocusResolver(xui_widget pWidget,
	xui_internal_accessible_focus_proc onFocus);
XUI_API int xuiInternalAccessibilityGetFocusedNode(xui_widget pWidget,
	uint64_t* pNodeId);
int xuiInternalAccessibilityEditNode(xui_widget pWidget, xui_accessible_node_t* pNode);
int xuiInternalAccessibilityEditAction(xui_widget pWidget, int iAction, const void* pData);
xui_widget xuiInternalInputModalRoot(xui_context pContext);
xui_widget xuiInternalInputFocusParent(xui_widget pWidget);
int xuiInternalInputFocusContains(xui_widget pRoot, xui_widget pWidget);
int xuiInternalInputFocusAllowed(xui_context pContext, xui_widget pWidget);
#if XUI_ENABLE_POPUP
xui_widget xuiInternalPopupPointerPassthroughOwner(xui_context pContext,
	xui_widget pHit, float fX, float fY);
int xuiInternalPopupPrepareOwnerPress(xui_context pContext,
	xui_widget pTarget, float fX, float fY);
int xuiInternalPopupSetSizing(xui_widget pWidget, float fContentWidth,
	float fContentHeight, float fMaxWidth, float fMaxHeight);
#endif
#if XUI_ENABLE_MENU
int xuiInternalMenuIsLive(xui_context pContext, xui_widget pWidget);
#endif
bool xuiInternalWidgetDestroyFlush(xui_context pContext);
void xuiInternalContextDestroyWidgets(xui_context pContext);
/* Caller holds an operation/physical-destruction scope across callbacks. */
void xuiInternalLifetimeInvalidateAll(xui_lifetime_link_t** ppHead);
void xuiInternalContextInvalidateLifetimes(xui_context pContext);
void xuiInternalContextDestroyWidgetTypes(xui_context pContext);
void xuiInternalContextDestroyStyles(xui_context pContext);
void xuiInternalContextDestroyResources(xui_context pContext);
void xuiInternalContextDestroyRenderTree(xui_context pContext);
int xuiInternalStackingChildren(xui_widget pParent,
	const xui_widget** ppChildren, int* pCount);
void xuiInternalContextDestroyInput(xui_context pContext);
int xuiInternalInputSyncIme(xui_context pContext);
void xuiInternalInputInvalidateImeOwner(xui_context pContext);
int xuiInternalInputRefreshIme(xui_context pContext);
int xuiInternalInputRefreshImePosition(xui_context pContext);
/* Native document snapshots use the generic edit projection, whereas direct
 * DocumentEditor composition calls retain paragraph-local rich offsets. */
#define XUI_INTERNAL_IME_EDIT_TEXT_RANGE 0x00000002u
int xuiInternalInputImeEditTextComposition(xui_context pContext,
    const xui_ime_composition_t* pComposition);
#if XUI_ENABLE_DOCUMENT_EDITOR
int xuiInternalDocumentImeText(xui_widget pWidget, const char** psText,
    int* pLength, int* pStart, int* pEnd, uint64_t* pRevision);
#endif
int xuiInternalClipboardReadProxy(xui_proxy pProxy, char** psText, int* pTextSize);
XUI_API int xuiInternalContextSetImeDetach(xui_context pContext, void (*onDetach)(xui_context pContext));
int xuiInternalClipboardReadText(xui_context pContext, char** psText, int* pTextSize);
int xuiInternalEditRegister(xui_widget pWidget, const xui_internal_edit_adapter_t* pAdapter,
	const xui_edit_behavior_t* pBehavior);
int xuiInternalEditDelegate(xui_widget pWidget, xui_widget pDelegate);
void xuiInternalEditUndelegate(xui_widget pWidget);
xui_widget xuiInternalEditHost(xui_widget pWidget);
int xuiInternalEditSetPrepare(xui_widget pWidget, xui_internal_edit_prepare_proc onPrepare, void* pUser);
int xuiInternalEditPrepare(xui_widget pWidget, int iType, int* pSkipDefault);
const xui_edit_behavior_t* xuiInternalEditBehavior(xui_widget pWidget);
int xuiInternalEditEmit(xui_widget pWidget, int iType, const char* sText,
	int iSelectionStart, int iSelectionEnd, int iCompositionStart, int iCompositionEnd, int bValid);
int xuiInternalEditEmitSized(xui_widget pWidget, int iType, const char* sText, int iTextSize,
	int iSelectionStart, int iSelectionEnd, int iCompositionStart, int iCompositionEnd, int bValid);
/* Commit an in-panel ColorPicker field without closing its popup. Returns
 * INVALID_ARGUMENT when the draft is malformed so composite editors can keep focus. */
int xuiInternalColorPickerCommitPendingEdit(xui_widget pWidget);
void xuiInternalContextPressCancel(xui_context pContext);
int xuiInternalContextPressUpdate(xui_context pContext, float fDelta);
int xuiInternalDrawPath(xui_proxy pProxy, xui_draw_context pDraw, xui_path pPath, const xui_path_style_t* pStyle, float fTolerance);
int xuiInternalDrawDisclosureChevron(xui_proxy pProxy, xui_draw_context pDraw,
	xui_rect_t tRect, int bExpanded, uint32_t iColor);
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
int xuiInternalIconCategoryBelongsToContext(xui_icon_category pCategory, xui_context pContext);
void* xuiInternalCodeDocumentGetLexerCCache(xui_code_document pDocument);
int xuiInternalCodeDocumentSetLexerCCache(xui_code_document pDocument,
	void* pCache, void (*onDestroy)(void* pCache));
typedef void (*xui_internal_code_document_edit_proc)(xui_code_document pDocument,
	int iStartOffset, int iEndOffset, int iNewEndOffset,
	int iStartLine, int iEndLine, int iNewEndLine, int bReset, void* pUser);
int xuiInternalCodeDocumentSetEditObserver(xui_code_document pDocument,
	xui_internal_code_document_edit_proc onEdit, void* pUser);
int xuiInternalCodeDocumentClampOffset(xui_code_document pDocument,
	int iOffset, int bForward, int* pClamped);
int xuiInternalCodeLanguageUsesBuiltinC(const xui_code_language_t* pLanguage);
uint32_t xuiInternalFlowGraphGetGeometryRevision(xui_flow_graph pGraph);
void xuiInternalFlowGraphSwapContents(xui_flow_graph pFirst, xui_flow_graph pSecond);
int xuiInternalFlowGraphUpdateNodeConfiguration(xui_flow_graph pGraph,
	const char* sNodeId, xvalue* pConfig, const xui_flow_port_desc_t* pPorts,
	int iPortCount, int bReplaceDynamicPorts, const char* sSummary);
int xuiInternalWidgetSetTreeLayer(xui_widget pWidget, int iLayer, int iZIndex);
int xuiInternalOverlayNextZ(xui_context pContext, int iLayer);

static char* xuiInternalStringDuplicateNullable(const char* sText)
{
	return sText != NULL ? xrtStrDup(sText) : NULL;
}

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

/* Invoke an exactly typed void publisher while retaining its selected owner.
 * Payload expressions are evaluated AFTER snapshot acquisition, because Hold
 * may reenter and replace payload storage. The enclosing native call scope
 * protects widget/context storage until the entire public operation returns. */
#define XUI_INTERNAL_CALLBACK_VOID(widget, slot, proc_type, user, ...) \
    do { \
        proc_type xui_selected_proc = (slot); \
        void* xui_selected_user = (user); \
        if ( xui_selected_proc != NULL ) { \
            if ( !(widget)->pEventOwners || !(widget)->pEventOwners->pBindings ) { \
                xui_selected_proc(__VA_ARGS__, xui_selected_user); \
            } else { \
                xui_callback_snapshot_t xui_selected_snapshot = {0}; \
                if ( xuiInternalCallbackSnapshotBeginReported((widget), &(slot), &xui_selected_user, &xui_selected_snapshot) == XUI_OK ) { \
                    xui_selected_proc(__VA_ARGS__, xui_selected_user); \
                    (void)xuiInternalCallbackSnapshotEnd(&xui_selected_snapshot, XUI_OK); \
                } \
            } \
        } \
    } while (0)

/* Statement form preserves ordinary C11 and exact callback signatures. Its
 * payload expressions run after Hold, just like the void publisher above. */
#define XUI_INTERNAL_CALLBACK_RESULT_AT(widget, slot_address, proc_type, proc, user, output, failure, ...) \
    do { \
        xui_widget xui_selected_widget = (widget); \
        const void* xui_selected_slot = (slot_address); \
        proc_type xui_selected_proc = (proc); \
        void* xui_selected_user = (user); \
        (output) = (failure); \
        if ( xui_selected_proc != NULL ) { \
            if ( !xui_selected_widget->pEventOwners || !xui_selected_widget->pEventOwners->pBindings ) { \
                (output) = xui_selected_proc(__VA_ARGS__, xui_selected_user); \
            } else { \
                xui_callback_snapshot_t xui_selected_snapshot = {0}; \
                if ( xuiInternalCallbackSnapshotBeginReported(xui_selected_widget, xui_selected_slot, &xui_selected_user, &xui_selected_snapshot) == XUI_OK ) { \
                    (output) = xui_selected_proc(__VA_ARGS__, xui_selected_user); \
                    if ( xuiInternalCallbackSnapshotEnd(&xui_selected_snapshot, XUI_OK) != XUI_OK ) (output) = (failure); \
                } \
            } \
        } \
    } while (0)
#define XUI_INTERNAL_CALLBACK_RESULT(widget, slot, proc_type, user, output, failure, ...) \
    XUI_INTERNAL_CALLBACK_RESULT_AT(widget, &(slot), proc_type, (slot), user, output, failure, __VA_ARGS__)

#if XUI_ENABLE_MENUBAR
xui_widget xuiInternalMenuBarResolveSelect(xui_context pContext, xui_widget pMenu,
    xui_menu_select_proc* pOnSelect, void** ppUser, const void** ppSlot);
#endif

#endif /* XUI_INTERNAL_H */
