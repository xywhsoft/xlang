#include "xui_config.h"
#if XGE_ENABLE_XUI
#include "xui_internal.h"
#include "xui_callback_hold_internal.h"
#include "xui_native_accessibility_internal.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_RENDER_CACHE_DIRTY_FLAGS \
	(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE)
#define XUI_RENDER_SUBTREE_DIRTY_FLAGS \
	(XUI_RENDER_CACHE_DIRTY_FLAGS | XUI_WIDGET_DIRTY_RENDER)

static int __xuiWidgetRemoveFromParentOperation(xui_widget pWidget,const void* pAuthority);
static void __xuiWidgetSyncInactiveTree(xui_widget pWidget);
static int __xuiWidgetDestroyPrepareClosure(xui_widget,bool*);
static int __xuiWidgetDestroyReadySelf(xui_widget);
static int __xuiWidgetDestroyPrepareSelf(xui_widget,bool*);
static int __xuiWidgetDestroyInMode(xui_widget,xui_internal_destroy_mode_t);

int xuiInternalCallbackOwnerValid(const xui_callback_owner_t* pOwner)
{
	return xuiInternalCallbackOwnerRawValid(pOwner);
}

int xuiInternalCallbackOwnerHold(const xui_callback_owner_t* pOwner)
{
	return xuiInternalCallbackOwnerRawHold(pOwner);
}

int xuiInternalCallbackOwnerDropOutcome(xui_context context,const xui_callback_owner_t* owner,int primaryStatus,const xerror* primaryError)
{
    if(!owner || !owner->onDrop)return XUI_OK;
    /* The caller has detached its exact primary from TLS. Every return from
     * Drop consumes this owner; a later failure cannot replace that primary. */
    bool success=owner->onDrop(owner->tReference.Data);
    int result=success && !xrtGetError() ? XUI_OK:XUI_ERROR_CALLBACK_FAILED;
    if(result!=XUI_OK)xuiInternalContextRecordOutcome(context,result,primaryStatus!=XUI_OK ? primaryError:xrtGetError());
    return result;
}
int xuiInternalCallbackOwnerDrop(xui_context context,const xui_callback_owner_t* owner)
{
    if(!owner || !owner->onDrop)return XUI_OK;
    xerror* first=xrtTakeError();int primary=first ? XUI_ERROR_CALLBACK_FAILED:XUI_OK;
    int result=xuiInternalCallbackOwnerDropOutcome(context,owner,primary,first);
    if(primary!=XUI_OK){xrtClearError();xrtSetErrorTake(first);return primary;}
    return result;
}

static int __xuiWidgetMemoryValid(xui_widget pWidget)
{
	return (pWidget != NULL) && (pWidget->iMagic == XUI_WIDGET_MAGIC);
}

static int __xuiWidgetValid(xui_widget pWidget)
{
	return __xuiWidgetMemoryValid(pWidget) && !pWidget->bDestroyPending;
}

XUI_API void xuiLifetimeUnwatch(xui_lifetime_link_t* pLink)
{
	if ( pLink == NULL ) return;
	if ( pLink->ppPrevious != NULL ) {
		*pLink->ppPrevious = pLink->pNext;
		if ( pLink->pNext != NULL ) pLink->pNext->ppPrevious = pLink->ppPrevious;
	}
	pLink->pNext = NULL; pLink->ppPrevious = NULL;
	pLink->onInvalidate = NULL; pLink->pUser = NULL;
}

static int __xuiLifetimeWatch(xui_lifetime_link_t** ppHead, xui_lifetime_link_t* pLink,
	xui_lifetime_invalidate_proc onInvalidate, void* pUser)
{
	if ( pLink == NULL || onInvalidate == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLink->ppPrevious != NULL || pLink->pNext != NULL || pLink->onInvalidate != NULL || pLink->pUser != NULL )
		return XUI_ERROR_INVALID_STATE;
	pLink->pNext = *ppHead; pLink->ppPrevious = ppHead;
	pLink->onInvalidate = onInvalidate; pLink->pUser = pUser;
	if ( *ppHead != NULL ) (*ppHead)->ppPrevious = &pLink->pNext;
	*ppHead = pLink;
	return XUI_OK;
}

XUI_API int xuiContextLifetimeWatch(xui_context pContext, xui_lifetime_link_t* pLink,
	xui_lifetime_invalidate_proc onInvalidate, void* pUser)
{
	if ( !xuiInternalContextIsValid(pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pContext->bDestroying || pContext->bDestroyPending ) return XUI_ERROR_INVALID_STATE;
	return __xuiLifetimeWatch(&pContext->pLifetimeLinks, pLink, onInvalidate, pUser);
}

XUI_API int xuiWidgetLifetimeWatch(xui_widget pWidget, xui_lifetime_link_t* pLink,
	xui_lifetime_invalidate_proc onInvalidate, void* pUser)
{
	if ( !__xuiWidgetMemoryValid(pWidget) ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_context pContext = pWidget->pContext;
	if ( pWidget->bDestroyPending || !xuiInternalContextIsValid(pContext) ||
		pContext->bDestroying || pContext->bDestroyPending ) return XUI_ERROR_INVALID_STATE;
	return __xuiLifetimeWatch(&pWidget->pLifetimeLinks, pLink, onInvalidate, pUser);
}

void xuiInternalLifetimeInvalidateAll(xui_lifetime_link_t** ppHead)
{
	/* Read the live head after each callback: a callback may detach/free its
	 * own registration or a pending peer. Never touch the detached node again. */
	while ( *ppHead != NULL ) {
		xui_lifetime_link_t* pLink = *ppHead;
		xui_lifetime_invalidate_proc onInvalidate = pLink->onInvalidate;
		void* pUser = pLink->pUser;
		xuiLifetimeUnwatch(pLink);
		if ( onInvalidate != NULL ) onInvalidate(pUser);
	}
}

void xuiInternalContextInvalidateLifetimes(xui_context pContext)
{
	xuiInternalLifetimeInvalidateAll(&pContext->pLifetimeLinks);
	for ( xui_widget pWidget = pContext->pAllWidgets; pWidget != NULL; pWidget = pWidget->pContextNext )
		xuiInternalLifetimeInvalidateAll(&pWidget->pLifetimeLinks);
}

static int __xuiWidgetEffectiveVisible(xui_widget pWidget)
{
	xui_widget pScan;

	if ( !__xuiWidgetValid(pWidget) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( !pScan->bVisible ) return 0;
	}
	return 1;
}

static int __xuiWidgetEffectiveEnabled(xui_widget pWidget)
{
	xui_widget pScan;

	if ( !__xuiWidgetValid(pWidget) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( !pScan->bEnabled ) return 0;
	}
	return 1;
}

static int __xuiWidgetAttachedToContext(xui_widget pWidget)
{
	xui_widget pScan;
	xui_context pContext;

	if ( !__xuiWidgetValid(pWidget) ) return 0;
	pContext = pWidget->pContext;
	pScan = pWidget;
	while ( pScan->pParent != NULL ) pScan = pScan->pParent;
	return (pScan == pContext->pRoot) || (pScan == pContext->pOverlayRoot);
}

#define XUI_STATE_CHANGE_BOUNDS  0x00000001u
#define XUI_STATE_CHANGE_VISIBLE 0x00000002u
#define XUI_STATE_CHANGE_ENABLED 0x00000004u

static int __xuiWidgetRectEqual(xui_rect_t tA, xui_rect_t tB)
{
	return (tA.fX == tB.fX) && (tA.fY == tB.fY) &&
	       (tA.fW == tB.fW) && (tA.fH == tB.fH);
}

static void __xuiWidgetStateEnsureQueued(xui_widget pWidget, const xui_rect_t* pOldWorldRect)
{
	xui_context pContext;

	if ( !__xuiWidgetValid(pWidget) || (pWidget->iPendingStateChanges != 0u) ) return;
	pContext = pWidget->pContext;
	pWidget->tPendingStateOldWorldRect = (pOldWorldRect != NULL) ?
		*pOldWorldRect : xuiWidgetGetWorldRect(pWidget);
	pWidget->bPendingStateOldVisible = __xuiWidgetEffectiveVisible(pWidget);
	pWidget->bPendingStateOldEnabled = __xuiWidgetEffectiveEnabled(pWidget);
	pWidget->pAccessibilityOldParent = pWidget->pParent;
	pWidget->tPendingDamageWorldRect = pWidget->tPendingStateOldWorldRect;
	pWidget->bPendingDamageVisible = pWidget->bPendingStateOldVisible &&
		__xuiWidgetAttachedToContext(pWidget) &&
		pWidget->tPendingDamageWorldRect.fW > 0 && pWidget->tPendingDamageWorldRect.fH > 0;
	pWidget->pStateChangeNext = NULL;
	if ( pContext->pStateChangeTail != NULL ) {
		pContext->pStateChangeTail->pStateChangeNext = pWidget;
	} else {
		pContext->pStateChangeHead = pWidget;
	}
	pContext->pStateChangeTail = pWidget;
}

/* A subtype destructor may destroy another widget while the outer widget is
 * being freed. Remove each freed node from the pending state queue before its
 * storage can be recycled; the next operation leave may still flush events. */
static void __xuiWidgetStateUnqueue(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget* ppScan;
	xui_widget pPrevious = NULL;
	if ( !__xuiWidgetMemoryValid(pWidget) || pWidget->iPendingStateChanges == 0u ) return;
	pContext = pWidget->pContext;
	if ( !xuiInternalContextIsValid(pContext) ) return;
	ppScan = &pContext->pStateChangeHead;
	while ( *ppScan != NULL ) {
		if ( *ppScan == pWidget ) {
			*ppScan = pWidget->pStateChangeNext;
			if ( pContext->pStateChangeTail == pWidget ) pContext->pStateChangeTail = pPrevious;
			break;
		}
		pPrevious = *ppScan;
		ppScan = &pPrevious->pStateChangeNext;
	}
	pWidget->pStateChangeNext = NULL;
	pWidget->iPendingStateChanges = 0u;
}

static void __xuiWidgetStateRecordBoundsTreeAt(xui_widget pWidget, xui_rect_t tOldWorldRect)
{
	xui_widget pChild;
	xui_rect_t tChildWorldRect;

	if ( !__xuiWidgetValid(pWidget) ||
	     ((pWidget->iPendingStateChanges & XUI_STATE_CHANGE_BOUNDS) != 0u) ) return;
	__xuiWidgetStateEnsureQueued(pWidget, &tOldWorldRect);
	pWidget->iPendingStateChanges |= XUI_STATE_CHANGE_BOUNDS;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		tChildWorldRect = pChild->tRect;
		tChildWorldRect.fX += tOldWorldRect.fX;
		tChildWorldRect.fY += tOldWorldRect.fY;
		__xuiWidgetStateRecordBoundsTreeAt(pChild, tChildWorldRect);
	}
}

void xuiInternalStateRecordBoundsTree(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) return;
	__xuiWidgetStateRecordBoundsTreeAt(pWidget, xuiWidgetGetWorldRect(pWidget));
}

static void __xuiWidgetStateDamageBounds(xui_widget pWidget)
{
	xui_rect_t tWorldRect;
	int bVisible;
	int iRet = XUI_OK;
	if ( !__xuiWidgetMemoryValid(pWidget) || pWidget->iPendingStateChanges == 0u ) return;
	tWorldRect = xuiWidgetGetWorldRect(pWidget);
	bVisible = __xuiWidgetEffectiveVisible(pWidget) && __xuiWidgetAttachedToContext(pWidget) &&
		tWorldRect.fW > 0 && tWorldRect.fH > 0;
	if ( pWidget->bPendingDamageVisible == bVisible &&
		(!bVisible || __xuiWidgetRectEqual(pWidget->tPendingDamageWorldRect, tWorldRect)) ) return;
	if ( pWidget->bPendingDamageVisible ) {
		iRet = xuiInternalContextInvalidateRect(pWidget->pContext,
			xuiInternalRectToDamage(pWidget->tPendingDamageWorldRect));
	}
	if ( bVisible && iRet == XUI_OK ) {
		iRet = xuiInternalContextInvalidateRect(pWidget->pContext, xuiInternalRectToDamage(tWorldRect));
	}
	if ( iRet != XUI_OK ) (void)xuiInternalContextInvalidateAll(pWidget->pContext);
	/* Keep event history intact, but do not re-damage a layout already composed. */
	pWidget->tPendingDamageWorldRect = tWorldRect;
	pWidget->bPendingDamageVisible = bVisible;
}

void xuiInternalStateDamageFlush(xui_context pContext)
{
	xui_widget pWidget;
	if ( !xuiInternalContextIsValid(pContext) ) return;
	for ( pWidget = pContext->pStateChangeHead; pWidget != NULL; pWidget = pWidget->pStateChangeNext ) {
		__xuiWidgetStateDamageBounds(pWidget);
	}
}

static void __xuiWidgetDamagePaintTree(xui_widget pWidget)
{
	xui_widget pScan;
	xui_rect_t tRect;
	if ( !__xuiWidgetEffectiveVisible(pWidget) || !__xuiWidgetAttachedToContext(pWidget) ) return;
	/* A clip-policy change can reveal or remove pixels outside this widget. */
	for ( pScan = pWidget; pScan != NULL; ) {
		if ( pScan->bVisible ) {
			tRect = xuiWidgetGetWorldRect(pScan);
			if ( tRect.fW > 0 && tRect.fH > 0 &&
				xuiInternalContextInvalidateRect(pWidget->pContext, xuiInternalRectToDamage(tRect)) != XUI_OK ) {
				(void)xuiInternalContextInvalidateAll(pWidget->pContext);
				return;
			}
			if ( pScan->pFirstChild != NULL ) {
				pScan = pScan->pFirstChild;
				continue;
			}
		}
		while ( pScan != pWidget && pScan->pNextSibling == NULL ) pScan = pScan->pParent;
		if ( pScan == pWidget ) break;
		pScan = pScan->pNextSibling;
	}
}

void xuiInternalStateRecordVisible(xui_widget pWidget)
{
	xui_widget pChild;
	if ( !__xuiWidgetValid(pWidget) ||
	     ((pWidget->iPendingStateChanges & XUI_STATE_CHANGE_VISIBLE) != 0u) ) return;
	__xuiWidgetStateEnsureQueued(pWidget, NULL);
	pWidget->iPendingStateChanges |= XUI_STATE_CHANGE_VISIBLE;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		xuiInternalStateRecordVisible(pChild);
	}
}

void xuiInternalStateRecordEnabled(xui_widget pWidget)
{
	xui_widget pChild;
	if ( !__xuiWidgetValid(pWidget) ||
	     ((pWidget->iPendingStateChanges & XUI_STATE_CHANGE_ENABLED) != 0u) ) return;
	__xuiWidgetStateEnsureQueued(pWidget, NULL);
	pWidget->iPendingStateChanges |= XUI_STATE_CHANGE_ENABLED;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		xuiInternalStateRecordEnabled(pChild);
	}
}

static void __xuiWidgetDispatchStateEvent(xui_context pContext, xui_widget pWidget,
	int iEventType, const char* sOperation)
{
	xui_event_t tEvent;
	int iRet;

	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iSize = sizeof(tEvent);
	tEvent.iType = iEventType;
	tEvent.pTarget = pWidget;
	iRet = xuiDispatchEvent(pContext, &tEvent);
	if ( iRet != XUI_OK && xuiInternalContextIsValid(pContext) ) {
		xuiInternalReportError(pContext, pWidget, iRet, XUI_ERROR_STAGE_USER, 1,
			sOperation, "A widget state-change handler failed.");
	}
}

void xuiInternalStateChangeFlush(xui_context pContext)
{
	xui_widget pWidget;
	xui_rect_t tOldWorldRect;
	uint32_t iChanges;
	uint32_t iAccessibilityEvents;
	int iEventType;
	int bOldVisible;
	int bOldEnabled;

	if ( !xuiInternalContextIsValid(pContext) ) return;
	while ( (pWidget = pContext->pStateChangeHead) != NULL ) {
		if ( xuiInternalContextDestroyPending(pContext) ) break;
		pContext->pStateChangeHead = pWidget->pStateChangeNext;
		if ( pContext->pStateChangeHead == NULL ) pContext->pStateChangeTail = NULL;
		pWidget->pStateChangeNext = NULL;
		__xuiWidgetStateDamageBounds(pWidget);
		iChanges = pWidget->iPendingStateChanges;
		iAccessibilityEvents = 0;
		tOldWorldRect = pWidget->tPendingStateOldWorldRect;
		bOldVisible = pWidget->bPendingStateOldVisible;
		bOldEnabled = pWidget->bPendingStateOldEnabled;
		pWidget->iPendingStateChanges = 0u;
		if ( pWidget->pAccessibilityOldParent != pWidget->pParent ) {
			xuiInternalAccessibilityQueue(pWidget->pAccessibilityOldParent, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
			xuiInternalAccessibilityQueue(pWidget->pParent, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
		}
		if ( !__xuiWidgetValid(pWidget) ) continue;
		if ( bOldVisible != __xuiWidgetEffectiveVisible(pWidget) || bOldEnabled != __xuiWidgetEffectiveEnabled(pWidget) )
			iAccessibilityEvents |= 1u << (XUI_ACCESSIBLE_EVENT_STATE_CHANGED - 1);
		if ( (iChanges & XUI_STATE_CHANGE_BOUNDS) != 0u &&
		     !__xuiWidgetRectEqual(tOldWorldRect, xuiWidgetGetWorldRect(pWidget)) ) {
			iAccessibilityEvents |= 1u << (XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED - 1);
			__xuiWidgetDispatchStateEvent(pContext, pWidget, XUI_EVENT_BOUNDS_CHANGED,
				"state.bounds_changed");
		}
		if ( xuiInternalContextDestroyPending(pContext) || !__xuiWidgetValid(pWidget) ) continue;
		if ( (iChanges & XUI_STATE_CHANGE_VISIBLE) != 0u &&
		     (bOldVisible != __xuiWidgetEffectiveVisible(pWidget)) ) {
			iAccessibilityEvents |= 1u << (XUI_ACCESSIBLE_EVENT_STATE_CHANGED - 1);
			__xuiWidgetDispatchStateEvent(pContext, pWidget, XUI_EVENT_VISIBLE_CHANGED,
				"state.visible_changed");
		}
		if ( xuiInternalContextDestroyPending(pContext) || !__xuiWidgetValid(pWidget) ) continue;
		if ( (iChanges & XUI_STATE_CHANGE_ENABLED) != 0u &&
		     (bOldEnabled != __xuiWidgetEffectiveEnabled(pWidget)) ) {
			iAccessibilityEvents |= 1u << (XUI_ACCESSIBLE_EVENT_STATE_CHANGED - 1);
			__xuiWidgetDispatchStateEvent(pContext, pWidget, XUI_EVENT_ENABLED_CHANGED,
				"state.enabled_changed");
		}
		if ( xuiInternalContextDestroyPending(pContext) ) break;
		for ( iEventType = XUI_ACCESSIBLE_EVENT_TREE_CHANGED; iEventType <= XUI_ACCESSIBLE_EVENT_STATE_CHANGED; iEventType++ ) {
			if ( (iAccessibilityEvents & (1u << (iEventType - 1))) != 0 )
				xuiInternalAccessibilityQueue(pWidget, iEventType);
		}
	}
}

int xuiInternalWidgetIsValid(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget);
}

static uint64_t __xuiWidgetEventTypeMask(int iType)
{
	switch ( iType ) {
	case XUI_EVENT_POINTER_ENTER: return XUI_EVENT_MASK_POINTER_ENTER;
	case XUI_EVENT_POINTER_LEAVE: return XUI_EVENT_MASK_POINTER_LEAVE;
	case XUI_EVENT_POINTER_MOVE: return XUI_EVENT_MASK_POINTER_MOVE;
	case XUI_EVENT_POINTER_DOWN: return XUI_EVENT_MASK_POINTER_DOWN;
	case XUI_EVENT_POINTER_UP: return XUI_EVENT_MASK_POINTER_UP;
	case XUI_EVENT_POINTER_CLICK: return XUI_EVENT_MASK_POINTER_CLICK;
	case XUI_EVENT_POINTER_WHEEL: return XUI_EVENT_MASK_POINTER_WHEEL;
	case XUI_EVENT_POINTER_DOUBLE_CLICK: return XUI_EVENT_MASK_DOUBLE_CLICK;
	case XUI_EVENT_CONTEXT_MENU: return XUI_EVENT_MASK_CONTEXT_MENU;
	case XUI_EVENT_KEY_DOWN: return XUI_EVENT_MASK_KEY_DOWN;
	case XUI_EVENT_KEY_UP: return XUI_EVENT_MASK_KEY_UP;
	case XUI_EVENT_TEXT: return XUI_EVENT_MASK_TEXT;
	case XUI_EVENT_HOTKEY: return XUI_EVENT_MASK_HOTKEY;
	case XUI_EVENT_COMMAND: return XUI_EVENT_MASK_COMMAND;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return XUI_EVENT_MASK_FOCUS;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		return XUI_EVENT_MASK_CAPTURE;
	case XUI_EVENT_DRAG_BEGIN:
	case XUI_EVENT_DRAG_MOVE:
	case XUI_EVENT_DRAG_END:
	case XUI_EVENT_DRAG_CANCEL:
	case XUI_EVENT_DRAG_ENTER:
	case XUI_EVENT_DRAG_OVER:
	case XUI_EVENT_DRAG_LEAVE:
	case XUI_EVENT_DROP:
		return XUI_EVENT_MASK_DRAG;
	case XUI_EVENT_IME_COMPOSITION:
		return XUI_EVENT_MASK_IME;
	case XUI_EVENT_BOUNDS_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
	case XUI_EVENT_ENABLED_CHANGED:
		return XUI_EVENT_MASK_STATE;
	default:
		return 0ull;
	}
}

static uint64_t __xuiWidgetOwnEventMask(xui_widget pWidget)
{
	return pWidget->iEventInterestMask | pWidget->iEventHandlerMask;
}

static xui_widget_type_t g_xuiWidgetBaseType = {
	.iMagic = XUI_WIDGET_TYPE_MAGIC,
	.sName = "widget",
	.bBuiltin = 1
};

struct xui_style_prop_t {
	uint32_t iPropertyId;
	char* sName;
	uint32_t iDirtyFlags;
	xui_style_value_t tValue;
};

struct xui_style_rule_t {
	uint32_t iMagic;
	xui_context pContext;
	int iKind;
	xui_widget_type pType;
	uint32_t iStateMask;
	char* sName;
	char* sParent;
	xui_style_prop_t* pProperties;
	int iPropertyCount;
	xui_style_rule_t* pNext;
};

struct xui_style_property_entry_t {
	uint32_t iPropertyId;
	char* sName;
	int iValueType;
	uint32_t iDirtyFlags;
	uint32_t iFlags;
	xui_widget_type pWidgetType;
	xui_style_value_t tDefaultValue;
	xui_style_property_entry_t* pNext;
	xui_style_property_entry_t* pIdNext;
	xui_style_property_entry_t* pNameNext;
};

struct xui_style_token_t {
	char* sName;
	xui_style_value_t tValue;
	xui_style_token_t* pNext;
};

#define XUI_STYLE_RULE_NAMED 1
#define XUI_STYLE_RULE_CLASS 2
#define XUI_STYLE_RULE_TYPE  3
#define XUI_STYLE_RULE_STATE_CLASS 4
#define XUI_STYLE_HASH_OFFSET 2166136261u
#define XUI_STYLE_HASH_PRIME 16777619u
#define XUI_STYLE_MAX_PARENT_DEPTH 64

static int __xuiWidgetTypeValid(xui_widget_type pType)
{
	return (pType != NULL) && (pType->iMagic == XUI_WIDGET_TYPE_MAGIC);
}

static int __xuiWidgetCreateInternal(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData, const void* pNativeOwner);

static int __xuiFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_CONTEXT_MAX_VIEWPORT) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiRectValid(xui_rect_t tRect)
{
	return (tRect.fX >= -XUI_CONTEXT_MAX_VIEWPORT) &&
	       (tRect.fY >= -XUI_CONTEXT_MAX_VIEWPORT) &&
	       (tRect.fX <= XUI_CONTEXT_MAX_VIEWPORT) &&
	       (tRect.fY <= XUI_CONTEXT_MAX_VIEWPORT) &&
	       (tRect.fW >= 0) &&
	       (tRect.fH >= 0) &&
	       (tRect.fW <= XUI_CONTEXT_MAX_VIEWPORT) &&
	       (tRect.fH <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiNonNegativeFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiLayoutTypeValid(int iLayoutType)
{
	return (iLayoutType == XUI_LAYOUT_MANUAL) ||
	       (iLayoutType == XUI_LAYOUT_OVERLAY) ||
	       (iLayoutType == XUI_LAYOUT_ROW) ||
	       (iLayoutType == XUI_LAYOUT_COLUMN) ||
	       (iLayoutType == XUI_LAYOUT_FLOW) ||
	       (iLayoutType == XUI_LAYOUT_TABLE) ||
	       (iLayoutType == XUI_LAYOUT_DOCK) ||
	       (iLayoutType == XUI_LAYOUT_GRID);
}

static int __xuiSizeModeValid(int iSizeMode)
{
	return (iSizeMode == XUI_SIZE_FIXED) ||
	       (iSizeMode == XUI_SIZE_CONTENT) ||
	       (iSizeMode == XUI_SIZE_FILL);
}

static int __xuiAlignValid(int iAlign)
{
	return (iAlign == XUI_ALIGN_START) ||
	       (iAlign == XUI_ALIGN_CENTER) ||
	       (iAlign == XUI_ALIGN_END) ||
	       (iAlign == XUI_ALIGN_STRETCH);
}

static int __xuiFlowModeValid(int iFlowMode)
{
	return (iFlowMode == XUI_FLOW_BLOCK) ||
	       (iFlowMode == XUI_FLOW_INLINE) ||
	       (iFlowMode == XUI_FLOW_INLINE_BLOCK) ||
	       (iFlowMode == XUI_FLOW_NONE) ||
	       (iFlowMode == XUI_FLOW_ABSOLUTE);
}

static int __xuiDockValid(int iDock)
{
	return (iDock == 0) ||
	       (iDock == XUI_DOCK_LEFT) ||
	       (iDock == XUI_DOCK_TOP) ||
	       (iDock == XUI_DOCK_RIGHT) ||
	       (iDock == XUI_DOCK_BOTTOM) ||
	       (iDock == XUI_DOCK_FILL);
}

static int __xuiOverflowValid(int iOverflow)
{
	return (iOverflow == XUI_OVERFLOW_VISIBLE) ||
	       (iOverflow == XUI_OVERFLOW_HIDDEN) ||
	       (iOverflow == XUI_OVERFLOW_CLIP) ||
	       (iOverflow == XUI_OVERFLOW_REPORT);
}

static int __xuiMeasureContainmentValid(int iContainment)
{
	return iContainment >= 0 && (iContainment & ~XUI_MEASURE_CONTAIN_BOTH) == 0;
}

static int __xuiThicknessValid(xui_thickness_t tValue)
{
	return __xuiNonNegativeFloatValid(tValue.fLeft) &&
	       __xuiNonNegativeFloatValid(tValue.fTop) &&
	       __xuiNonNegativeFloatValid(tValue.fRight) &&
	       __xuiNonNegativeFloatValid(tValue.fBottom);
}

static int __xuiTableTrackValid(const xui_table_track_t* pTrack)
{
	if ( pTrack == NULL ) {
		return 0;
	}
	return (__xuiSizeModeValid(pTrack->iSizeMode) || pTrack->iSizeMode == XUI_TRACK_CROSS) &&
	       __xuiNonNegativeFloatValid(pTrack->fValue) &&
	       __xuiNonNegativeFloatValid(pTrack->fMin) &&
	       __xuiNonNegativeFloatValid(pTrack->fMax) &&
	       __xuiNonNegativeFloatValid(pTrack->fWeight) &&
	       __xuiNonNegativeFloatValid(pTrack->fShrink) &&
	       (pTrack->fMin <= pTrack->fMax);
}

static int __xuiCachePolicyValid(const xui_cache_policy_t* pPolicy)
{
	if ( pPolicy == NULL ) {
		return 0;
	}
	if ( (pPolicy->iSize != 0) && (pPolicy->iSize < sizeof(*pPolicy)) ) {
		return 0;
	}
	if ( (pPolicy->iPolicy != XUI_CACHE_POLICY_NONE) &&
	     (pPolicy->iPolicy != XUI_CACHE_POLICY_SELF) &&
	     (pPolicy->iPolicy != XUI_CACHE_POLICY_SUBTREE) &&
	     (pPolicy->iPolicy != XUI_CACHE_POLICY_SUBTREE_TILED) &&
	     (pPolicy->iPolicy != XUI_CACHE_POLICY_DISPLAY_LIST) &&
	     (pPolicy->iPolicy != XUI_CACHE_POLICY_AUTO) ) {
		return 0;
	}
	if ( (pPolicy->iTileWidth < 0) || (pPolicy->iTileHeight < 0) ) {
		return 0;
	}
	return 1;
}

static int __xuiCachePolicySupported(int iPolicy)
{
	return iPolicy == XUI_CACHE_POLICY_NONE || iPolicy == XUI_CACHE_POLICY_SELF ||
		iPolicy == XUI_CACHE_POLICY_SUBTREE || iPolicy == XUI_CACHE_POLICY_AUTO;
}

static int __xuiTooltipTypeValid(int iType)
{
	return (iType == XUI_TOOLTIP_NONE) ||
	       (iType == XUI_TOOLTIP_TEXT) ||
	       (iType == XUI_TOOLTIP_CUSTOM);
}

static int __xuiTooltipAnchorValid(int iAnchor)
{
	return (iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_BOTTOM) ||
	       (iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_TOP) ||
	       (iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_RIGHT) ||
	       (iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_LEFT) ||
	       (iAnchor == XUI_TOOLTIP_ANCHOR_CURSOR);
}

static void __xuiTooltipDefaultDesc(xui_tooltip_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_NONE;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_WIDGET_BOTTOM;
	pDesc->fOffsetX = 0.0f;
	pDesc->fOffsetY = 6.0f;
	/* Negative delay means use the context's platform interaction policy. */
	pDesc->fDelay = -1.0f;
}

static int __xuiTooltipDescStructValid(const xui_tooltip_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 0;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( !__xuiTooltipTypeValid(pDesc->iType) ||
	     !__xuiTooltipAnchorValid(pDesc->iAnchor) ||
	     !__xuiRectValid(pDesc->tAnchorRect) ||
	     !__xuiFloatValid(pDesc->fOffsetX) ||
	     !__xuiFloatValid(pDesc->fOffsetY) ||
	     (pDesc->fDelay != pDesc->fDelay) || (pDesc->fDelay < -1.0f) ||
	     (pDesc->fDelay > XUI_LAYOUT_UNBOUNDED) ) {
		return 0;
	}
	return 1;
}

static char* __xuiStringDuplicate(const char* sText)
{
	return xuiInternalStringDuplicateNullable(sText);
}

static int __xuiStringEqual(const char* sA, const char* sB)
{
	if ( (sA == NULL) || (sB == NULL) ) {
		return 0;
	}
	return strcmp(sA, sB) == 0;
}

static void __xuiStyleValueReset(xui_style_value_t* pValue)
{
	if ( pValue == NULL ) {
		return;
	}
	if ( ((pValue->iType == XUI_STYLE_VALUE_STRING) ||
	      (pValue->iType == XUI_STYLE_VALUE_TOKEN)) &&
	     (pValue->sText != NULL) ) {
		xrtFree((void*)pValue->sText);
	}
	memset(pValue, 0, sizeof(*pValue));
}

static int __xuiStyleValueCopy(xui_style_value_t* pDst, const xui_style_value_t* pSrc)
{
	char* sText;

	memset(pDst, 0, sizeof(*pDst));
	*pDst = *pSrc;
	pDst->iSize = sizeof(*pDst);
	if ( (pSrc->iType == XUI_STYLE_VALUE_STRING) || (pSrc->iType == XUI_STYLE_VALUE_TOKEN) ) {
		sText = __xuiStringDuplicate(pSrc->sText);
		if ( sText == NULL ) {
			memset(pDst, 0, sizeof(*pDst));
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pDst->sText = sText;
	}
	return XUI_OK;
}

static unsigned __xuiStylePropertyNameBucket(const char* name)
{
	uint32_t hash = XUI_STYLE_HASH_OFFSET;
	for (const unsigned char* p = (const unsigned char*)name; *p; ++p)
		hash = (hash ^ *p) * XUI_STYLE_HASH_PRIME;
	return hash & 255u;
}

static xui_style_property_entry_t* __xuiStyleFindPropertyById(xui_context pContext, uint32_t iPropertyId)
{
	xui_style_property_entry_t* pEntry;

	if ( iPropertyId == 0 ) {
		return NULL;
	}
	for ( pEntry = pContext->pStylePropertiesById[iPropertyId & 255u]; pEntry != NULL; pEntry = pEntry->pIdNext ) {
		if ( pEntry->iPropertyId == iPropertyId ) {
			return pEntry;
		}
	}
	return NULL;
}

static xui_style_property_entry_t* __xuiStyleFindPropertyByName(xui_context pContext, const char* sName)
{
	xui_style_property_entry_t* pEntry;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	for ( pEntry = pContext->pStylePropertiesByName[__xuiStylePropertyNameBucket(sName)]; pEntry != NULL; pEntry = pEntry->pNameNext ) {
		if ( __xuiStringEqual(pEntry->sName, sName) ) {
			return pEntry;
		}
	}
	return NULL;
}

static xui_style_token_t* __xuiStyleFindToken(xui_context pContext, const char* sName)
{
	xui_style_token_t* pToken;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	for ( pToken = pContext->pStyleTokens; pToken != NULL; pToken = pToken->pNext ) {
		if ( __xuiStringEqual(pToken->sName, sName) ) {
			return pToken;
		}
	}
	return NULL;
}

static int __xuiStylePropIsInherited(xui_context pContext, const xui_style_prop_t* pProp)
{
	xui_style_property_entry_t* pEntry;

	pEntry = NULL;
	if ( pProp->iPropertyId != 0 ) {
		pEntry = __xuiStyleFindPropertyById(pContext, pProp->iPropertyId);
	}
	if ( (pEntry == NULL) && (pProp->sName != NULL) ) {
		pEntry = __xuiStyleFindPropertyByName(pContext, pProp->sName);
	}
	return (pEntry != NULL) && ((pEntry->iFlags & XUI_STYLE_PROPERTY_INHERITED) != 0);
}

static int __xuiStyleValueTypeValid(int iType)
{
	return (iType == XUI_STYLE_VALUE_NONE) ||
	       (iType == XUI_STYLE_VALUE_INT) ||
	       (iType == XUI_STYLE_VALUE_FLOAT) ||
	       (iType == XUI_STYLE_VALUE_COLOR) ||
	       (iType == XUI_STYLE_VALUE_BOOL) ||
	       (iType == XUI_STYLE_VALUE_STRING) ||
	       (iType == XUI_STYLE_VALUE_TOKEN);
}

static uint32_t __xuiStyleDirtyFlags(uint32_t iFlags)
{
	if ( iFlags == 0 ) {
		return XUI_STYLE_DIRTY_DEFAULT;
	}
	return iFlags;
}

static int __xuiStylePropertyValid(const xui_style_property_t* pProperty)
{
	if ( (pProperty == NULL) || (pProperty->iSize < sizeof(*pProperty)) ||
	     (((pProperty->sName == NULL) || (pProperty->sName[0] == '\0')) && (pProperty->iPropertyId == 0)) ||
	     ((pProperty->iDirtyFlags & ~XUI_WIDGET_DIRTY_ALL) != 0) ||
	     (pProperty->tValue.iSize < sizeof(pProperty->tValue)) ||
	     !__xuiStyleValueTypeValid(pProperty->tValue.iType) ) {
		return 0;
	}
	if ( ((pProperty->tValue.iType == XUI_STYLE_VALUE_STRING) ||
	      (pProperty->tValue.iType == XUI_STYLE_VALUE_TOKEN)) &&
	     (pProperty->tValue.sText == NULL) ) {
		return 0;
	}
	return 1;
}

static int __xuiStyleDescValid(const xui_style_desc_t* pStyle, int bNameRequired)
{
	int i;

	if ( (pStyle == NULL) || (pStyle->iSize < sizeof(*pStyle)) ||
	     (pStyle->iPropertyCount < 0) ||
	     ((pStyle->iPropertyCount > 0) && (pStyle->pProperties == NULL)) ) {
		return 0;
	}
	if ( bNameRequired && ((pStyle->sName == NULL) || (pStyle->sName[0] == '\0')) ) {
		return 0;
	}
	for ( i = 0; i < pStyle->iPropertyCount; i++ ) {
		if ( !__xuiStylePropertyValid(&pStyle->pProperties[i]) ) {
			return 0;
		}
	}
	return 1;
}

static void __xuiStylePropReset(xui_style_prop_t* pProp)
{
	if ( pProp == NULL ) {
		return;
	}
	if ( pProp->sName != NULL ) {
		xrtFree(pProp->sName);
	}
	if ( ((pProp->tValue.iType == XUI_STYLE_VALUE_STRING) ||
	      (pProp->tValue.iType == XUI_STYLE_VALUE_TOKEN)) &&
	     (pProp->tValue.sText != NULL) ) {
		xrtFree((void*)pProp->tValue.sText);
	}
	memset(pProp, 0, sizeof(*pProp));
}

static void __xuiStylePropArrayClear(xui_style_prop_t* pProps, int iCount)
{
	int i;

	if ( pProps == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xuiStylePropReset(&pProps[i]);
	}
	xrtFree(pProps);
}

static int __xuiStylePropCopy(xui_context pContext, xui_style_prop_t* pDst, const xui_style_property_t* pSrc)
{
	xui_style_property_entry_t* pEntry;
	char* sText;
	const char* sName;

	memset(pDst, 0, sizeof(*pDst));
	pEntry = NULL;
	if ( pSrc->iPropertyId != 0 ) {
		pEntry = __xuiStyleFindPropertyById(pContext, pSrc->iPropertyId);
	}
	if ( (pEntry == NULL) && (pSrc->sName != NULL) ) {
		pEntry = __xuiStyleFindPropertyByName(pContext, pSrc->sName);
	}
	if ( pEntry != NULL ) {
		if ( (pEntry->iValueType != XUI_STYLE_VALUE_NONE) &&
		     (pSrc->tValue.iType != XUI_STYLE_VALUE_TOKEN) &&
		     (pSrc->tValue.iType != pEntry->iValueType) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		pDst->iPropertyId = pEntry->iPropertyId;
		sName = pEntry->sName;
		pDst->iDirtyFlags = __xuiStyleDirtyFlags((pSrc->iDirtyFlags != 0) ? pSrc->iDirtyFlags : pEntry->iDirtyFlags);
	} else {
		pDst->iPropertyId = pSrc->iPropertyId;
		sName = pSrc->sName;
		pDst->iDirtyFlags = __xuiStyleDirtyFlags(pSrc->iDirtyFlags);
	}
	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDst->sName = __xuiStringDuplicate(sName);
	if ( pDst->sName == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst->tValue = pSrc->tValue;
	pDst->tValue.iSize = sizeof(pDst->tValue);
	if ( pSrc->tValue.iType == XUI_STYLE_VALUE_STRING || pSrc->tValue.iType == XUI_STYLE_VALUE_TOKEN ) {
		sText = __xuiStringDuplicate(pSrc->tValue.sText);
		if ( sText == NULL ) {
			__xuiStylePropReset(pDst);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pDst->tValue.sText = sText;
	}
	return XUI_OK;
}

static int __xuiStylePropArrayCopy(xui_context pContext, xui_style_prop_t** ppDst, int* pDstCount, const xui_style_property_t* pProps, int iCount)
{
	xui_style_prop_t* pDst;
	int i;
	int iRet;

	*ppDst = NULL;
	*pDstCount = 0;
	if ( iCount == 0 ) {
		return XUI_OK;
	}
	pDst = (xui_style_prop_t*)xrtCalloc((size_t)iCount, sizeof(*pDst));
	if ( pDst == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xuiStylePropCopy(pContext, &pDst[i], &pProps[i]);
		if ( iRet != XUI_OK ) {
			__xuiStylePropArrayClear(pDst, iCount);
			return iRet;
		}
	}
	*ppDst = pDst;
	*pDstCount = iCount;
	return XUI_OK;
}

static void __xuiStylePropExport(const xui_style_prop_t* pProp, xui_style_property_t* pOut)
{
	memset(pOut, 0, sizeof(*pOut));
	pOut->iSize = sizeof(*pOut);
	pOut->iPropertyId = pProp->iPropertyId;
	pOut->sName = pProp->sName;
	pOut->iDirtyFlags = pProp->iDirtyFlags;
	pOut->tValue = pProp->tValue;
	pOut->tValue.iSize = sizeof(pOut->tValue);
}

static int __xuiStyleValueEqual(const xui_style_value_t* pA, const xui_style_value_t* pB)
{
	if ( pA->iType != pB->iType ) {
		return 0;
	}
	switch ( pA->iType ) {
	case XUI_STYLE_VALUE_INT:
	case XUI_STYLE_VALUE_BOOL:
		return pA->iInt == pB->iInt;
	case XUI_STYLE_VALUE_FLOAT:
		return pA->fFloat == pB->fFloat;
	case XUI_STYLE_VALUE_COLOR:
		return pA->iColor == pB->iColor;
	case XUI_STYLE_VALUE_STRING:
	case XUI_STYLE_VALUE_TOKEN:
		return __xuiStringEqual(pA->sText, pB->sText);
	case XUI_STYLE_VALUE_NONE:
	default:
		return 1;
	}
}

static int __xuiStylePropEqual(const xui_style_prop_t* pA, const xui_style_prop_t* pB)
{
	return __xuiStringEqual(pA->sName, pB->sName) &&
	       (pA->iPropertyId == pB->iPropertyId) &&
	       (pA->iDirtyFlags == pB->iDirtyFlags) &&
	       __xuiStyleValueEqual(&pA->tValue, &pB->tValue);
}

static xui_style_prop_t* __xuiStyleFindProp(xui_style_prop_t* pProps, int iCount, const char* sName)
{
	int i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xuiStringEqual(pProps[i].sName, sName) ) {
			return &pProps[i];
		}
	}
	return NULL;
}

static const xui_style_prop_t* __xuiStyleFindPropConst(const xui_style_prop_t* pProps, int iCount, const char* sName)
{
	int i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xuiStringEqual(pProps[i].sName, sName) ) {
			return &pProps[i];
		}
	}
	return NULL;
}

static uint32_t __xuiStyleHashBytes(uint32_t iHash, const void* pData, size_t iSize)
{
	const unsigned char* pBytes;
	size_t i;

	pBytes = (const unsigned char*)pData;
	for ( i = 0; i < iSize; i++ ) {
		iHash ^= pBytes[i];
		iHash *= XUI_STYLE_HASH_PRIME;
	}
	return iHash;
}

static uint32_t __xuiStyleHashString(uint32_t iHash, const char* sText)
{
	if ( sText == NULL ) {
		return __xuiStyleHashBytes(iHash, "", 1u);
	}
	return __xuiStyleHashBytes(iHash, sText, strlen(sText) + 1u);
}

static uint32_t __xuiStyleHashProps(const xui_style_prop_t* pProps, int iCount)
{
	uint32_t iHash;
	int i;

	iHash = XUI_STYLE_HASH_OFFSET;
	for ( i = 0; i < iCount; i++ ) {
		iHash = __xuiStyleHashBytes(iHash, &pProps[i].iPropertyId, sizeof(pProps[i].iPropertyId));
		iHash = __xuiStyleHashString(iHash, pProps[i].sName);
		iHash = __xuiStyleHashBytes(iHash, &pProps[i].iDirtyFlags, sizeof(pProps[i].iDirtyFlags));
		iHash = __xuiStyleHashBytes(iHash, &pProps[i].tValue.iType, sizeof(pProps[i].tValue.iType));
		switch ( pProps[i].tValue.iType ) {
		case XUI_STYLE_VALUE_INT:
		case XUI_STYLE_VALUE_BOOL:
			iHash = __xuiStyleHashBytes(iHash, &pProps[i].tValue.iInt, sizeof(pProps[i].tValue.iInt));
			break;
		case XUI_STYLE_VALUE_FLOAT:
			iHash = __xuiStyleHashBytes(iHash, &pProps[i].tValue.fFloat, sizeof(pProps[i].tValue.fFloat));
			break;
		case XUI_STYLE_VALUE_COLOR:
			iHash = __xuiStyleHashBytes(iHash, &pProps[i].tValue.iColor, sizeof(pProps[i].tValue.iColor));
			break;
		case XUI_STYLE_VALUE_STRING:
		case XUI_STYLE_VALUE_TOKEN:
			iHash = __xuiStyleHashString(iHash, pProps[i].tValue.sText);
			break;
		default:
			break;
		}
	}
	if ( iHash == 0 ) {
		iHash = 1;
	}
	return iHash;
}

static int __xuiStylePropCopyInternal(xui_context pContext, xui_style_prop_t* pDst, const xui_style_prop_t* pSrc)
{
	xui_style_property_t tProperty;

	__xuiStylePropExport(pSrc, &tProperty);
	return __xuiStylePropCopy(pContext, pDst, &tProperty);
}

static int __xuiStyleBuilderUpsert(xui_context pContext, xui_style_prop_t** ppProps, int* pCount, const xui_style_prop_t* pSrc)
{
	xui_style_prop_t* pProp;
	xui_style_prop_t tResolved;
	xui_style_prop_t* pNewProps;
	int iNewCount;
	int iRet;

	memset(&tResolved, 0, sizeof(tResolved));
	if ( (pSrc->tValue.iType == XUI_STYLE_VALUE_TOKEN) && (pSrc->tValue.sText != NULL) ) {
		xui_style_token_t* pToken;
		pToken = __xuiStyleFindToken(pContext, pSrc->tValue.sText);
		if ( pToken != NULL ) {
			tResolved = *pSrc;
			iRet = __xuiStyleValueCopy(&tResolved.tValue, &pToken->tValue);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
			pSrc = &tResolved;
		}
	}
	pProp = __xuiStyleFindProp(*ppProps, *pCount, pSrc->sName);
	if ( pProp != NULL ) {
		__xuiStylePropReset(pProp);
		iRet = __xuiStylePropCopyInternal(pContext, pProp, pSrc);
		__xuiStyleValueReset(&tResolved.tValue);
		return iRet;
	}
	iNewCount = *pCount + 1;
	pNewProps = (xui_style_prop_t*)xrtRealloc(*ppProps, sizeof(*pNewProps) * (size_t)iNewCount);
	if ( pNewProps == NULL ) {
		__xuiStyleValueReset(&tResolved.tValue);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*ppProps = pNewProps;
	memset(&(*ppProps)[*pCount], 0, sizeof((*ppProps)[*pCount]));
	iRet = __xuiStylePropCopyInternal(pContext, &(*ppProps)[*pCount], pSrc);
	__xuiStyleValueReset(&tResolved.tValue);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	*pCount = iNewCount;
	return XUI_OK;
}

static xui_style_rule_t* __xuiStyleFindRuleEx(xui_context pContext, int iKind, const char* sName, xui_widget_type pType, uint32_t iStateMask)
{
	xui_style_rule_t* pRule;

	for ( pRule = pContext->pStyleRules; pRule != NULL; pRule = pRule->pNext ) {
		if ( (pRule->iKind != iKind) ||
		     (pRule->iMagic != XUI_STYLE_RULE_MAGIC) ||
		     (pRule->iStateMask != iStateMask) ) {
			continue;
		}
		if ( (iKind == XUI_STYLE_RULE_TYPE) && (pRule->pType == pType) ) {
			return pRule;
		}
		if ( (iKind != XUI_STYLE_RULE_TYPE) && __xuiStringEqual(pRule->sName, sName) ) {
			return pRule;
		}
	}
	return NULL;
}

static xui_style_rule_t* __xuiStyleFindRule(xui_context pContext, int iKind, const char* sName, xui_widget_type pType)
{
	return __xuiStyleFindRuleEx(pContext, iKind, sName, pType, 0);
}

static void __xuiStyleRuleClear(xui_style_rule_t* pRule)
{
	if ( pRule == NULL ) {
		return;
	}
	if ( pRule->sName != NULL ) {
		xrtFree(pRule->sName);
	}
	if ( pRule->sParent != NULL ) {
		xrtFree(pRule->sParent);
	}
	__xuiStylePropArrayClear(pRule->pProperties, pRule->iPropertyCount);
	memset(pRule, 0, sizeof(*pRule));
}

static void __xuiStyleRuleDestroy(xui_style_rule_t* pRule)
{
	if ( pRule == NULL ) {
		return;
	}
	__xuiStyleRuleClear(pRule);
	xrtFree(pRule);
}

static int __xuiStyleRuleSetEx(xui_context pContext, int iKind, const char* sName, xui_widget_type pType, const xui_style_desc_t* pStyle, uint32_t iStateMask)
{
	xui_style_rule_t* pRule;
	xui_style_prop_t* pProperties;
	char* sRuleName;
	char* sParent;
	int iPropertyCount;
	int iRet;

	pProperties = NULL;
	iPropertyCount = 0;
	sRuleName = NULL;
	sParent = NULL;
	if ( !__xuiStyleDescValid(pStyle, 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iKind != XUI_STYLE_RULE_TYPE ) {
		if ( (sName == NULL) || (sName[0] == '\0') ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		sRuleName = __xuiStringDuplicate(sName);
		if ( sRuleName == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( (pStyle->sParent != NULL) && (pStyle->sParent[0] != '\0') ) {
		sParent = __xuiStringDuplicate(pStyle->sParent);
		if ( sParent == NULL ) {
			if ( sRuleName != NULL ) {
				xrtFree(sRuleName);
			}
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	iRet = __xuiStylePropArrayCopy(pContext, &pProperties, &iPropertyCount, pStyle->pProperties, pStyle->iPropertyCount);
	if ( iRet != XUI_OK ) {
		if ( sRuleName != NULL ) {
			xrtFree(sRuleName);
		}
		if ( sParent != NULL ) {
			xrtFree(sParent);
		}
		return iRet;
	}
	pRule = __xuiStyleFindRuleEx(pContext, iKind, sName, pType, iStateMask);
	if ( pRule == NULL ) {
		pRule = (xui_style_rule_t*)xrtCalloc(1, sizeof(*pRule));
		if ( pRule == NULL ) {
			if ( sRuleName != NULL ) {
				xrtFree(sRuleName);
			}
			if ( sParent != NULL ) {
				xrtFree(sParent);
			}
			__xuiStylePropArrayClear(pProperties, iPropertyCount);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pRule->pNext = pContext->pStyleRules;
		pContext->pStyleRules = pRule;
	} else {
		if ( pRule->sName != NULL ) {
			xrtFree(pRule->sName);
		}
		if ( pRule->sParent != NULL ) {
			xrtFree(pRule->sParent);
		}
		__xuiStylePropArrayClear(pRule->pProperties, pRule->iPropertyCount);
	}
	pRule->iMagic = XUI_STYLE_RULE_MAGIC;
	pRule->pContext = pContext;
	pRule->iKind = iKind;
	pRule->pType = pType;
	pRule->iStateMask = iStateMask;
	pRule->sName = sRuleName;
	pRule->sParent = sParent;
	pRule->pProperties = pProperties;
	pRule->iPropertyCount = iPropertyCount;
	return XUI_OK;
}

static int __xuiStyleRuleSet(xui_context pContext, int iKind, const char* sName, xui_widget_type pType, const xui_style_desc_t* pStyle)
{
	return __xuiStyleRuleSetEx(pContext, iKind, sName, pType, pStyle, 0);
}

static int __xuiStyleRuleRemoveEx(xui_context pContext, int iKind, const char* sName, xui_widget_type pType, uint32_t iStateMask)
{
	xui_style_rule_t* pRule;
	xui_style_rule_t* pPrev;

	pPrev = NULL;
	for ( pRule = pContext->pStyleRules; pRule != NULL; pRule = pRule->pNext ) {
		if ( (pRule->iStateMask == iStateMask) &&
		     (((iKind == XUI_STYLE_RULE_TYPE) && (pRule->iKind == iKind) && (pRule->pType == pType)) ||
		      ((iKind != XUI_STYLE_RULE_TYPE) && (pRule->iKind == iKind) && __xuiStringEqual(pRule->sName, sName))) ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pRule->pNext;
			} else {
				pContext->pStyleRules = pRule->pNext;
			}
			__xuiStyleRuleDestroy(pRule);
			return XUI_OK;
		}
		pPrev = pRule;
	}
	return XUI_OK;
}

static int __xuiStyleRuleRemove(xui_context pContext, int iKind, const char* sName, xui_widget_type pType)
{
	return __xuiStyleRuleRemoveEx(pContext, iKind, sName, pType, 0);
}

static xui_table_track_t __xuiTableTrackDefault(void)
{
	xui_table_track_t tTrack;

	tTrack.iSizeMode = XUI_SIZE_CONTENT;
	tTrack.fValue = 0.0f;
	tTrack.fMin = 0.0f;
	tTrack.fMax = XUI_LAYOUT_UNBOUNDED;
	tTrack.fWeight = 1.0f;
	tTrack.fShrink = 0.0f;
	return tTrack;
}

static float __xuiMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static float __xuiFiniteConstraint(float fValue)
{
	if ( (fValue == fValue) && (fValue >= 0.0f) && (fValue < XUI_LAYOUT_UNBOUNDED) ) {
		return fValue;
	}
	return XUI_LAYOUT_UNBOUNDED;
}

static int __xuiConstraintIsFinite(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue < XUI_LAYOUT_UNBOUNDED);
}

static xui_vec2_t __xuiVec2(float fX, float fY)
{
	xui_vec2_t tRet;

	tRet.fX = fX;
	tRet.fY = fY;
	return tRet;
}

static xui_vec2_t __xuiWidgetClampSize(xui_widget pWidget, xui_vec2_t tSize)
{
	tSize.fX = __xuiClampFloat(tSize.fX, pWidget->tLayout.fMinWidth, pWidget->tLayout.fMaxWidth);
	tSize.fY = __xuiClampFloat(tSize.fY, pWidget->tLayout.fMinHeight, pWidget->tLayout.fMaxHeight);
	return tSize;
}

static xui_rect_i_t __xuiWidgetRectToDamage(xui_rect_t tRect)
{
	return xuiInternalRectToDamage(tRect);
}

static void __xuiWidgetBumpGeneration(xui_widget pWidget)
{
	pWidget->iGeneration++;
	if ( pWidget->iGeneration == 0 ) {
		pWidget->iGeneration = 1;
	}
}

static void __xuiWidgetInitLayout(xui_widget pWidget)
{
	memset(&pWidget->tLayout, 0, sizeof(pWidget->tLayout));
	pWidget->tLayout.iLayoutType = XUI_LAYOUT_MANUAL;
	pWidget->tLayout.iWidthMode = XUI_SIZE_FIXED;
	pWidget->tLayout.iHeightMode = XUI_SIZE_FIXED;
	pWidget->tLayout.iFlowMode = XUI_FLOW_BLOCK;
	pWidget->tLayout.iDock = XUI_DOCK_FILL;
	pWidget->tLayout.iOverflow = XUI_OVERFLOW_CLIP;
	pWidget->tLayout.iMeasureContainment = XUI_MEASURE_CONTAIN_NONE;
	pWidget->tLayout.iAlignX = XUI_ALIGN_START;
	pWidget->tLayout.iAlignY = XUI_ALIGN_START;
	pWidget->tLayout.iTableRow = 0;
	pWidget->tLayout.iTableColumn = 0;
	pWidget->tLayout.iTableRowSpan = 1;
	pWidget->tLayout.iTableColumnSpan = 1;
	pWidget->tLayout.iGridColumnCount = 1;
	pWidget->tLayout.iLayer = 0;
	pWidget->tLayout.iZIndex = 0;
	pWidget->tLayout.fPreferredWidth = 0.0f;
	pWidget->tLayout.fPreferredHeight = 0.0f;
	pWidget->tLayout.fMinWidth = 0.0f;
	pWidget->tLayout.fMinHeight = 0.0f;
	pWidget->tLayout.fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pWidget->tLayout.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pWidget->tLayout.fGrow = 0.0f;
	pWidget->tLayout.fShrink = 0.0f;
	pWidget->tLayout.fGap = 0.0f;
	pWidget->tLayout.fGridItemWidth = 0.0f;
	pWidget->tLayout.fGridItemHeight = 0.0f;
	pWidget->tLayout.fBaseline = 0.0f;
}

static int __xuiWidgetIsAttached(xui_widget pWidget)
{
	xui_widget pRoot;

	if ( !__xuiWidgetValid(pWidget) ) {
		return 0;
	}
	pRoot = pWidget;
	while ( pRoot->pParent != NULL ) {
		pRoot = pRoot->pParent;
	}
	return (pWidget->pContext != NULL) &&
	       ((pWidget->pContext->pRoot == pRoot) || (pWidget->pContext->pOverlayRoot == pRoot));
}

static uint64_t __xuiWidgetAggregateEventMask(xui_widget pWidget)
{
	xui_widget pChild;
	uint64_t iMask;

	/* Destruction marks a whole subtree pending before physically unlinking it.
	 * Aggregates remain exact for those still-linked carriers until removal. */
	if ( !__xuiWidgetMemoryValid(pWidget) ) {
		return 0ull;
	}
	iMask = __xuiWidgetOwnEventMask(pWidget);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iMask |= pChild->iSubtreeEventMask;
	}
	return iMask;
}

#if XUI_ENABLE_INCREMENTAL_VERIFY
static uint64_t __xuiWidgetVerifyEventMaskRecursive(xui_widget pWidget, int* pValid)
{
	xui_widget pChild;
	uint64_t iMask = __xuiWidgetOwnEventMask(pWidget);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling )
		iMask |= __xuiWidgetVerifyEventMaskRecursive(pChild, pValid);
	if ( pWidget->iSubtreeEventMask != iMask ) {
		fprintf(stderr, "XUI event mask mismatch: type=%s stored=%llu expected=%llu\n",
			pWidget->pType != NULL ? pWidget->pType->sName : "?",
			(unsigned long long)pWidget->iSubtreeEventMask, (unsigned long long)iMask);
		*pValid = 0;
	}
	return iMask;
}

static uint32_t __xuiWidgetVerifyDirtyMaskRecursive(xui_widget pWidget, int* pValid)
{
	xui_widget pChild;
	uint32_t iFlags = pWidget->iDirtyFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling )
		iFlags |= __xuiWidgetVerifyDirtyMaskRecursive(pChild, pValid);
	if ( pWidget->iSubtreeDirtyFlags != iFlags ) *pValid = 0;
	return iFlags;
}

static void __xuiWidgetVerifyEventMasks(xui_widget pWidget)
{
	int bValid = 1;
	xui_widget pTrigger = pWidget;
	if ( pWidget == NULL ) return;
	while ( pWidget->pParent != NULL ) pWidget = pWidget->pParent;
	(void)__xuiWidgetVerifyEventMaskRecursive(pWidget, &bValid);
	if ( !bValid ) {
		xui_widget pScan;
		fprintf(stderr, "XUI incremental verification failed: event mask aggregate trigger=%s\n",
			pTrigger->pType != NULL ? pTrigger->pType->sName : "?");
		for ( pScan = pTrigger; pScan != NULL; pScan = pScan->pParent )
			fprintf(stderr, "  chain type=%s stored=%llu own=%llu\n",
				pScan->pType != NULL ? pScan->pType->sName : "?",
				(unsigned long long)pScan->iSubtreeEventMask,
				(unsigned long long)__xuiWidgetOwnEventMask(pScan));
		abort();
	}
}
#endif

static void __xuiWidgetUpdateEventMasksToRoot(xui_widget pWidget)
{
	xui_widget pScan;
	uint64_t iOldMask;
	uint64_t iNewMask;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		iOldMask = pScan->iSubtreeEventMask;
		iNewMask = __xuiWidgetAggregateEventMask(pScan);
		pScan->iSubtreeEventMask = iNewMask;
		pScan->pContext->iEventMaskPropagationTotal++;
		if ( iNewMask == iOldMask ) break;
	}
#if XUI_ENABLE_INCREMENTAL_VERIFY
	__xuiWidgetVerifyEventMasks(pWidget);
#endif
}

static void __xuiWidgetPropagateEventBitsToRoot(xui_widget pWidget, uint64_t iBits)
{
	xui_widget pScan;
	uint64_t iOldMask;

	if ( iBits == 0ull ) return;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		iOldMask = pScan->iSubtreeEventMask;
		pScan->iSubtreeEventMask |= iBits;
		pScan->pContext->iEventMaskPropagationTotal++;
		if ( pScan->iSubtreeEventMask == iOldMask ) break;
	}
#if XUI_ENABLE_INCREMENTAL_VERIFY
	__xuiWidgetVerifyEventMasks(pWidget);
#endif
}

static void __xuiWidgetApplyOwnEventMaskChange(xui_widget pWidget, uint64_t iOldOwnMask)
{
	uint64_t iNewOwnMask = __xuiWidgetOwnEventMask(pWidget);
	uint64_t iRemoved = iOldOwnMask & ~iNewOwnMask;
	if ( iRemoved != 0ull ) __xuiWidgetUpdateEventMasksToRoot(pWidget);
	else __xuiWidgetPropagateEventBitsToRoot(pWidget, iNewOwnMask & ~iOldOwnMask);
}

static void __xuiWidgetInvalidateMeasureTree(xui_widget pWidget)
{
	xui_widget pParent;

	if ( ++pWidget->iContentMeasureGeneration == 0u ) pWidget->iContentMeasureGeneration = 1u;
	pWidget->bMeasureValid = 0;
	pWidget->bArrangeValid = 0;
	xuiInternalLayoutInvalidate(pWidget, 1);
	for ( pParent = pWidget->pParent; pParent != NULL; pParent = pParent->pParent ) {
		pParent->bMeasureValid = 0;
		pParent->bArrangeValid = 0;
	}
}

static void __xuiWidgetMarkDirtyOnly(xui_widget pWidget, uint32_t iFlags)
{
	xui_widget pScan;
	xui_widget pRoot;
	uint32_t iOldFlags;

	if ( iFlags == 0 ) {
		iFlags = XUI_WIDGET_DIRTY_RENDER;
	}
	if ( (iFlags & (XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE)) != 0 ) {
		__xuiWidgetInvalidateMeasureTree(pWidget);
	}
	pWidget->iDirtyFlags |= iFlags;
	if ( (iFlags & (XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE)) != 0 ) {
		pWidget->bRenderGeometryDirty = 1;
	}
	if ( (iFlags & XUI_WIDGET_DIRTY_TREE) != 0 ) {
		pWidget->bRenderStructureDirty = 1;
		for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
			if ( pScan->bSubtreeRenderStructureDirty ) break;
			pScan->bSubtreeRenderStructureDirty = 1;
		}
		pRoot = pWidget;
		while ( pRoot->pParent != NULL ) pRoot = pRoot->pParent;
		if ( pRoot == pWidget->pContext->pRoot ) {
			if ( ++pWidget->pContext->iRootStructureRevision == 0 )
				pWidget->pContext->iRootStructureRevision = 1;
		} else if ( pRoot == pWidget->pContext->pOverlayRoot ) {
			if ( ++pWidget->pContext->iOverlayStructureRevision == 0 )
				pWidget->pContext->iOverlayStructureRevision = 1;
		}
	}
	__xuiWidgetBumpGeneration(pWidget);
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		iOldFlags = pScan->iSubtreeDirtyFlags;
		pScan->iSubtreeDirtyFlags |= iFlags;
		pScan->pContext->iDirtyPropagationTotal++;
		if ( pScan != pWidget ) __xuiWidgetBumpGeneration(pScan);
		if ( pScan->iSubtreeDirtyFlags == iOldFlags ) break;
	}
	xuiInternalContextBumpGeneration(pWidget->pContext);
}

void xuiInternalWidgetMarkDirtyFlags(xui_widget pWidget, uint32_t iFlags)
{
	if ( __xuiWidgetValid(pWidget) ) __xuiWidgetMarkDirtyOnly(pWidget, iFlags);
}

static void __xuiWidgetMarkDescendantPaintDirty(xui_widget pWidget, uint32_t iFlags)
{
	xui_widget pChild;

	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		pChild->iDirtyFlags |= iFlags;
		pChild->iSubtreeDirtyFlags |= iFlags;
		__xuiWidgetBumpGeneration(pChild);
		__xuiWidgetMarkDescendantPaintDirty(pChild, iFlags);
	}
}

void xuiInternalWidgetSyncDpi(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) || pWidget->iDpiGeneration == pWidget->pContext->iDpiGeneration ) return;
	pWidget->iDpiGeneration = pWidget->pContext->iDpiGeneration;
	__xuiWidgetMarkDirtyOnly(pWidget,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static uint32_t __xuiWidgetAggregateDirtyFlags(xui_widget pWidget)
{
	xui_widget pChild;
	uint32_t iFlags;

	if ( !__xuiWidgetMemoryValid(pWidget) ) return 0;
	iFlags = pWidget->iDirtyFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iFlags |= pChild->iSubtreeDirtyFlags;
	}
	return iFlags;
}

static void __xuiWidgetRepairDirtyToRoot(xui_widget pWidget)
{
	xui_widget pScan;
	uint32_t iOldFlags;
	uint32_t iNewFlags;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		iOldFlags = pScan->iSubtreeDirtyFlags;
		iNewFlags = __xuiWidgetAggregateDirtyFlags(pScan);
		pScan->iSubtreeDirtyFlags = iNewFlags;
		pScan->pContext->iDirtyPropagationTotal++;
		if ( iNewFlags == iOldFlags ) break;
	}
}

static int __xuiWidgetAggregateRenderStructureDirty(xui_widget pWidget)
{
	xui_widget pChild;
	if ( pWidget->bRenderStructureDirty ) return 1;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( pChild->bSubtreeRenderStructureDirty ) return 1;
	}
	return 0;
}

static void __xuiWidgetRepairRenderStructureToRoot(xui_widget pWidget)
{
	xui_widget pScan;
	int bOld;
	int bNew;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		bOld = pScan->bSubtreeRenderStructureDirty;
		bNew = __xuiWidgetAggregateRenderStructureDirty(pScan);
		pScan->bSubtreeRenderStructureDirty = bNew;
		if ( bNew == bOld ) break;
	}
}

static void __xuiWidgetClearDirtyRecursive(xui_widget pWidget, uint32_t iFlags)
{
	xui_widget pChild;

	if ( (pWidget->iSubtreeDirtyFlags & iFlags) == 0 ) return;
	pWidget->iDirtyFlags &= ~iFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetClearDirtyRecursive(pChild, iFlags);
	}
	pWidget->iSubtreeDirtyFlags = __xuiWidgetAggregateDirtyFlags(pWidget);
}

static int __xuiWidgetCountDescendants(xui_widget pWidget)
{
	xui_widget pChild;
	int iCount;

	iCount = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iCount++;
		iCount += __xuiWidgetCountDescendants(pChild);
	}
	return iCount;
}

static int __xuiWidgetInvalidateWorldRect(xui_widget pWidget, xui_rect_t tWorldRect, uint32_t iFlags)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWidgetMarkDirtyOnly(pWidget, iFlags);
	if ( (tWorldRect.fW <= 0.0f) || (tWorldRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( !__xuiWidgetIsAttached(pWidget) ) {
		return XUI_OK;
	}
	return xuiInternalContextInvalidateRect(pWidget->pContext, __xuiWidgetRectToDamage(tWorldRect));
}

static int __xuiWidgetIndexOfState(xui_widget pWidget, uint32_t iStateId)
{
	int i;

	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		if ( pWidget->pCacheSlots[i].iStateId == iStateId ) {
			return i;
		}
	}
	return -1;
}

static int __xuiWidgetSlotsHaveState(const xui_widget_cache_slot_t* pSlots, int iCount, uint32_t iStateId)
{
	int i;

	for ( i = 0; i < iCount; i++ ) {
		if ( pSlots[i].iStateId == iStateId ) {
			return 1;
		}
	}
	return 0;
}

static uint32_t __xuiWidgetNextFreeStateId(const xui_widget_cache_slot_t* pSlots, int iCount)
{
	uint32_t iStateId;

	iStateId = 0;
	while ( __xuiWidgetSlotsHaveState(pSlots, iCount, iStateId) ) {
		iStateId++;
	}
	return iStateId;
}

static xui_widget_cache_slot_t* __xuiWidgetFindCacheSlot(xui_widget pWidget, uint32_t iStateId)
{
	int iIndex;

	iIndex = __xuiWidgetIndexOfState(pWidget, iStateId);
	if ( iIndex < 0 ) {
		return NULL;
	}
	return &pWidget->pCacheSlots[iIndex];
}

/* Producer-owned cache records are represented by their actual fields. A
 * managed Native Drop can fail without consuming its owner; preserve that
 * exact field, and keep the enclosing cache array alive for checked retry. */
static int __xuiWidgetReleaseCacheSlot(xui_widget widget,xui_widget_cache_slot_t* slot,bool* complete)
{
    *complete=true;
    if(!slot->pSurface){memset(slot,0,sizeof(*slot));return XUI_OK;}
    xui_surface surface=slot->pSurface;slot->pSurface=NULL;
    int result=XUI_OK;
    if(widget->pContext->pOwnedProxy){
        bool consumed=false;result=xuiSurfaceReleaseOwnerChecked(surface,&consumed);
        if(!consumed){slot->pSurface=surface;*complete=false;}
    }else{
        xui_proxy proxy=xuiInternalContextGetProxy(widget->pContext);
        if(proxy && proxy->surfaceDestroy)proxy->surfaceDestroy(proxy,surface);
        else {slot->pSurface=surface;*complete=false;result=XUI_ERROR_NOT_INITIALIZED;}
    }
    if(*complete)memset(slot,0,sizeof(*slot));
    if(result<0 || xrtGetError())xuiInternalContextRecordOutcome(widget->pContext,result<0 ? result:XUI_ERROR_CALLBACK_FAILED,xrtGetError());
    return result;
}
static int __xuiWidgetDestroyCacheSlot(xui_widget widget,xui_widget_cache_slot_t* slot)
{
    if(widget->bCacheMutationActive)return XUI_ERROR_BUSY;
    widget->bCacheMutationActive=1;bool complete=false;
    int result=__xuiWidgetReleaseCacheSlot(widget,slot,&complete);
    widget->bCacheMutationActive=0;
    return complete ? result:(result<0 ? result:XUI_ERROR_BUSY);
}
static int __xuiWidgetDestroyAllCaches(xui_widget widget)
{
    if(widget->bCacheMutationActive)return XUI_ERROR_BUSY;
    widget->bCacheMutationActive=1;
    bool all=true;int result=XUI_OK;xerror* first=xrtTakeError();
    if(first)result=XUI_ERROR_CALLBACK_FAILED;
    for(int i=0;i<widget->iCacheCount;++i){
        bool complete=false;int status=__xuiWidgetReleaseCacheSlot(widget,&widget->pCacheSlots[i],&complete);
        xerror* error=xrtTakeError();
        if(status<0 || error){if(result==XUI_OK){result=status<0 ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}else xrtErrorFree(error);}
        if(!complete){all=false;break;}
    }
    if(all){xrtFree(widget->pCacheSlots);widget->pCacheSlots=NULL;widget->iCacheCount=0;}
    widget->bCacheMutationActive=0;xrtSetErrorTake(first);
    return all ? result:(result<0 ? result:XUI_STATUS_DEFERRED);
}

static size_t __xuiWidgetCacheSlotBytes(const xui_widget_cache_slot_t* pSlot)
{
	if ( (pSlot == NULL) || (pSlot->pSurface == NULL) || (pSlot->iWidth <= 0) || (pSlot->iHeight <= 0) ) {
		return 0u;
	}
	return (size_t)pSlot->iWidth * (size_t)pSlot->iHeight * 4u;
}

static void __xuiWidgetCollectCacheStats(xui_widget pWidget, size_t* pUsedBytes, int* pSurfaceCount)
{
	xui_widget pChild;
	int i;

	if ( pWidget == NULL ) {
		return;
	}
	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		if ( pWidget->pCacheSlots[i].pSurface != NULL ) {
			*pUsedBytes += __xuiWidgetCacheSlotBytes(&pWidget->pCacheSlots[i]);
			(*pSurfaceCount)++;
		}
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetCollectCacheStats(pChild, pUsedBytes, pSurfaceCount);
	}
}

static void __xuiWidgetPurgeCacheSlotSurface(xui_widget widget,xui_widget_cache_slot_t* slot)
{
    if(!widget || !slot || !slot->pSurface)return;
    uint32_t state=slot->iStateId;
    (void)__xuiWidgetDestroyCacheSlot(widget,slot);
    slot->iStateId=state;
    if(slot->pSurface)return;
    slot->iFlags|=XUI_WIDGET_DIRTY_CACHE;
    widget->iDirtyFlags|=XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER;
    widget->iSubtreeDirtyFlags|=XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER;
}

static void __xuiWidgetPurgeCachesToTarget(xui_widget pWidget, size_t* pUsedBytes, size_t iTargetBytes)
{
	xui_widget pChild;
	size_t iBytes;
	int i;

	if ( (pWidget == NULL) || (*pUsedBytes <= iTargetBytes) ) {
		return;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetPurgeCachesToTarget(pChild, pUsedBytes, iTargetBytes);
		if ( *pUsedBytes <= iTargetBytes ) {
			return;
		}
	}
	if ( (pWidget->tCachePolicy.iFlags & XUI_CACHE_PINNED) != 0 ) {
		return;
	}
	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		iBytes = __xuiWidgetCacheSlotBytes(&pWidget->pCacheSlots[i]);
		if ( iBytes == 0u ) {
			continue;
		}
		__xuiWidgetPurgeCacheSlotSurface(pWidget, &pWidget->pCacheSlots[i]);
        if(pWidget->pCacheSlots[i].pSurface)continue;
		*pUsedBytes = (*pUsedBytes > iBytes) ? (*pUsedBytes - iBytes) : 0u;
		if ( *pUsedBytes <= iTargetBytes ) {
			return;
		}
	}
}

static void __xuiWidgetDestroyTableTracks(xui_widget pWidget)
{
	if ( pWidget->pTableRows != NULL ) {
		xrtFree(pWidget->pTableRows);
		pWidget->pTableRows = NULL;
	}
	if ( pWidget->pTableColumns != NULL ) {
		xrtFree(pWidget->pTableColumns);
		pWidget->pTableColumns = NULL;
	}
	pWidget->iTableRowCount = 0;
	pWidget->iTableColumnCount = 0;
}

static void __xuiWidgetDestroyStyleData(xui_widget pWidget)
{
	int i;

	if ( pWidget->sStyleName != NULL ) {
		xrtFree(pWidget->sStyleName);
		pWidget->sStyleName = NULL;
	}
	if ( pWidget->pStyleClasses != NULL ) {
		for ( i = 0; i < pWidget->iStyleClassCount; i++ ) {
			if ( pWidget->pStyleClasses[i] != NULL ) {
				xrtFree(pWidget->pStyleClasses[i]);
			}
		}
		xrtFree(pWidget->pStyleClasses);
		pWidget->pStyleClasses = NULL;
	}
	pWidget->iStyleClassCount = 0;
	pWidget->iStyleClassCapacity = 0;
	__xuiStylePropArrayClear(pWidget->pInlineStyle, pWidget->iInlineStyleCount);
	pWidget->pInlineStyle = NULL;
	pWidget->iInlineStyleCount = 0;
	__xuiStylePropArrayClear(pWidget->pResolvedStyle, pWidget->iResolvedStyleCount);
	pWidget->pResolvedStyle = NULL;
	pWidget->iResolvedStyleCount = 0;
	pWidget->iResolvedStyleHash = 0;
	pWidget->iResolvedStyleGeneration = 0;
}

static void __xuiWidgetDestroyTooltipData(xui_widget pWidget)
{
	if ( pWidget->sTooltipText != NULL ) {
		xrtFree(pWidget->sTooltipText);
		pWidget->sTooltipText = NULL;
	}
	memset(&pWidget->tTooltip, 0, sizeof(pWidget->tTooltip));
	pWidget->onTooltipResolve = NULL;
	pWidget->pTooltipUser = NULL;
}

static int __xuiWidgetEndActiveUpdate(xui_widget widget)
{
    if(!widget->pActiveUpdateDraw){widget->pActiveUpdateSlot=NULL;widget->iActiveUpdateStateId=0;return XUI_OK;}
    if(widget->bCacheMutationActive)return XUI_ERROR_BUSY;
    widget->bCacheMutationActive=1;
    xui_draw_context draw=widget->pActiveUpdateDraw;
    bool consumed=false;int status=XUI_OK;
    if(widget->pContext->pOwnedProxy){
        status=xuiDrawEndChecked(draw);xerror* first=xrtTakeError();
        if(status==XUI_OK || xuiDrawEnded(draw)){
            int released=xuiDrawReleaseOwnerChecked(draw,&consumed);xerror* error=xrtTakeError();
            if(status==XUI_OK && (released!=XUI_OK || error)){status=released!=XUI_OK ? released:XUI_ERROR_CALLBACK_FAILED;first=error;}
            else xrtErrorFree(error);
        }
        xrtSetErrorTake(first);
    }else {
        xui_proxy proxy=xuiInternalContextGetProxy(widget->pContext);
        if(proxy && proxy->drawEnd){status=proxy->drawEnd(proxy,draw);consumed=status!=XUI_ERROR_BUSY;}
        else status=XUI_ERROR_NOT_INITIALIZED;
    }
    if(consumed){widget->pActiveUpdateDraw=NULL;widget->pActiveUpdateSlot=NULL;widget->iActiveUpdateStateId=0;}
    widget->bCacheMutationActive=0;
    if(status<0 || xrtGetError())xuiInternalContextRecordOutcome(widget->pContext,status<0 ? status:XUI_ERROR_CALLBACK_FAILED,xrtGetError());
    return consumed ? status:(status<0 ? status:XUI_STATUS_DEFERRED);
}

static int __xuiWidgetEnsureSingleCacheSlot(xui_widget pWidget, uint32_t iStateId)
{
	int iRet;

	if ( pWidget->iCacheCount > 0 ) {
		return XUI_OK;
	}
	iRet = xuiWidgetSetCacheStateCount(pWidget, 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pWidget->pCacheSlots[0].iStateId = iStateId;
	return XUI_OK;
}

static int __xuiWidgetEffectiveCachePolicy(xui_widget pWidget)
{
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_AUTO ) {
		return (pWidget->iChildCount > 0) ? XUI_CACHE_POLICY_SUBTREE : XUI_CACHE_POLICY_SELF;
	}
	return pWidget->tCachePolicy.iPolicy;
}

static xui_rect_t __xuiWidgetIntersectRect(xui_rect_t tA, xui_rect_t tB)
{
	xui_rect_t tRect;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = __xuiMaxFloat(tA.fX, tB.fX);
	fTop = __xuiMaxFloat(tA.fY, tB.fY);
	fRight = __xuiMinFloat(tA.fX + tA.fW, tB.fX + tB.fW);
	fBottom = __xuiMinFloat(tA.fY + tA.fH, tB.fY + tB.fH);
	tRect.fX = fLeft;
	tRect.fY = fTop;
	tRect.fW = __xuiMaxFloat(0.0f, fRight - fLeft);
	tRect.fH = __xuiMaxFloat(0.0f, fBottom - fTop);
	return tRect;
}

static int __xuiWidgetReserveRenderNodes(xui_context pContext, int iCapacity)
{
	xui_render_node_t* pNodes;

	if ( iCapacity <= pContext->iRenderNodeCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pContext->iRenderNodeCapacity * 2) ) {
		iCapacity = pContext->iRenderNodeCapacity * 2;
	}
	if ( iCapacity < 16 ) {
		iCapacity = 16;
	}
	pNodes = (xui_render_node_t*)xrtRealloc(pContext->pRenderNodes, sizeof(*pNodes) * (size_t)iCapacity);
	if ( pNodes == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pContext->pRenderNodes = pNodes;
	pContext->iRenderNodeCapacity = iCapacity;
	return XUI_OK;
}

static void __xuiWidgetFillRenderNode(xui_widget pWidget, xui_rect_t tPaintRect)
{
	xui_render_node_t* pNode = &pWidget->tRenderNode;
	memset(pNode, 0, sizeof(*pNode));
	pNode->iSize = sizeof(*pNode);
	pNode->pWidget = pWidget;
	pNode->tWorldRect = xuiInternalSnapRect(xuiWidgetGetWorldRect(pWidget));
	pNode->tPaintRect = xuiInternalSnapRect(tPaintRect);
	pNode->iLayer = pWidget->tLayout.iLayer;
	pNode->iZIndex = pWidget->tLayout.iZIndex;
	pNode->iStateId = pWidget->iStateId;
	pNode->iCachePolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	pNode->iFlags = pWidget->iDirtyFlags;
	pWidget->bRenderGeometryDirty = 0;
}

static void __xuiWidgetUnlinkRenderNodeOnDestroy(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget pScan;
	xui_widget pNext;
	xui_widget pEnd;
	xui_widget pAfter;
	xui_widget pPrevious;
	int iRemoved = 0;

	if ( pWidget == NULL || !pWidget->bRenderNodeLinked ) return;
	pContext = pWidget->pContext;
	pPrevious = pWidget->pRenderPrev;
	pEnd = pWidget->pRenderSubtreeLast != NULL ? pWidget->pRenderSubtreeLast : pWidget;
	pAfter = pEnd->pRenderNext;
	if ( pPrevious != NULL ) pPrevious->pRenderNext = pAfter;
	else pContext->pRenderHead = pAfter;
	if ( pAfter != NULL ) pAfter->pRenderPrev = pPrevious;
	else pContext->pRenderTail = pPrevious;
	for ( pScan = pWidget->pParent; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan->pRenderSubtreeLast != pEnd ) break;
		pScan->pRenderSubtreeLast = pPrevious;
	}
	for ( pScan = pWidget; pScan != pAfter; pScan = pNext ) {
		pNext = pScan->pRenderNext;
		pScan->pRenderPrev = NULL;
		pScan->pRenderNext = NULL;
		pScan->pRenderSubtreeLast = NULL;
		pScan->bRenderNodeLinked = 0;
		iRemoved++;
	}
	pContext->iRenderLinkedNodeCount -= iRemoved;
	if ( pContext->iRenderLinkedNodeCount < 0 ) pContext->iRenderLinkedNodeCount = 0;
	pContext->bRenderIndexDirty = 1;
}

static int __xuiWidgetStackingGreater(xui_widget pA, xui_widget pB)
{
	if ( pA->tLayout.iLayer != pB->tLayout.iLayer ) {
		return pA->tLayout.iLayer > pB->tLayout.iLayer;
	}
	return pA->tLayout.iZIndex > pB->tLayout.iZIndex;
}

static void __xuiWidgetStackingDirty(xui_widget pParent)
{
	if ( __xuiWidgetMemoryValid(pParent) ) {
		pParent->bStackingChildrenDirty = 1;
	}
}

static int __xuiWidgetReserveStackingChildren(xui_widget pParent, int iCapacity)
{
	xui_widget* pChildren;
	int iNewCapacity;

	if ( iCapacity <= pParent->iStackingChildCapacity ) return XUI_OK;
	iNewCapacity = pParent->iStackingChildCapacity * 2;
	if ( iNewCapacity < iCapacity ) iNewCapacity = iCapacity;
	if ( iNewCapacity < 8 ) iNewCapacity = 8;
	pChildren = (xui_widget*)xrtRealloc(pParent->pStackingChildren,
		sizeof(*pChildren) * (size_t)iNewCapacity);
	if ( pChildren == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pParent->pStackingChildren = pChildren;
	pParent->iStackingChildCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiWidgetSortStackingChildren(xui_widget pParent)
{
	xui_widget pChild;
	xui_widget pEntry;
	xui_widget* pSource;
	xui_widget* pTarget;
	xui_widget* pSwap;
	xui_widget* pMerge;
	int iCount;
	int iWidth;
	int iStart;
	int iMiddle;
	int iEnd;
	int iLeft;
	int iRight;
	int iWrite;
	int i;
	int j;

	iCount = pParent->iChildCount;
	if ( __xuiWidgetReserveStackingChildren(pParent, iCount) != XUI_OK ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	i = 0;
	for ( pChild = pParent->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		pParent->pStackingChildren[i++] = pChild;
	}
	iCount = i;
	if ( iCount <= 32 ) {
		for ( i = 1; i < iCount; i++ ) {
			pEntry = pParent->pStackingChildren[i];
			j = i - 1;
			while ( (j >= 0) && __xuiWidgetStackingGreater(
				pParent->pStackingChildren[j], pEntry) ) {
				pParent->pStackingChildren[j + 1] = pParent->pStackingChildren[j];
				j--;
			}
			pParent->pStackingChildren[j + 1] = pEntry;
		}
	} else {
		pMerge = (xui_widget*)xrtMalloc(sizeof(*pMerge) * (size_t)iCount);
		if ( pMerge == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		pSource = pParent->pStackingChildren;
		pTarget = pMerge;
		for ( iWidth = 1; iWidth < iCount; ) {
			for ( iStart = 0; iStart < iCount; iStart += iWidth * 2 ) {
				iMiddle = iStart + iWidth;
				if ( iMiddle > iCount ) iMiddle = iCount;
				iEnd = iMiddle + iWidth;
				if ( iEnd > iCount ) iEnd = iCount;
				iLeft = iStart;
				iRight = iMiddle;
				iWrite = iStart;
				while ( (iLeft < iMiddle) && (iRight < iEnd) ) {
					if ( __xuiWidgetStackingGreater(pSource[iLeft], pSource[iRight]) ) {
						pTarget[iWrite++] = pSource[iRight++];
					} else {
						pTarget[iWrite++] = pSource[iLeft++];
					}
				}
				while ( iLeft < iMiddle ) pTarget[iWrite++] = pSource[iLeft++];
				while ( iRight < iEnd ) pTarget[iWrite++] = pSource[iRight++];
			}
			pSwap = pSource;
			pSource = pTarget;
			pTarget = pSwap;
			if ( iWidth > iCount / 2 ) iWidth = iCount;
			else iWidth *= 2;
		}
		if ( pSource != pParent->pStackingChildren ) {
			memcpy(pParent->pStackingChildren, pSource,
				sizeof(*pSource) * (size_t)iCount);
		}
		xrtFree(pMerge);
	}
	pParent->iStackingChildCount = iCount;
	pParent->bStackingChildrenDirty = 0;
	return XUI_OK;
}

int xuiInternalStackingChildren(xui_widget pParent,
	const xui_widget** ppChildren, int* pCount)
{
	int iRet;

	if ( !xuiInternalWidgetIsValid(pParent) ||
	     (ppChildren == NULL) || (pCount == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pParent->bStackingChildrenDirty ||
	     (pParent->iStackingChildCount != pParent->iChildCount) ) {
		iRet = __xuiWidgetSortStackingChildren(pParent);
		if ( iRet != XUI_OK ) return iRet;
	}
	*ppChildren = pParent->pStackingChildren;
	*pCount = pParent->iStackingChildCount;
	return XUI_OK;
}

/* Keep the cache valid for empty widgets without requiring an allocation. */
static void __xuiWidgetInitStackingChildren(xui_widget pWidget)
{
	pWidget->pStackingChildren = NULL;
	pWidget->iStackingChildCount = 0;
	pWidget->iStackingChildCapacity = 0;
	pWidget->bStackingChildrenDirty = 1;
}

static void __xuiWidgetDestroyStackingChildren(xui_widget pWidget)
{
	if ( pWidget->pStackingChildren != NULL ) {
		xrtFree(pWidget->pStackingChildren);
	}
	pWidget->pStackingChildren = NULL;
	pWidget->iStackingChildCount = 0;
	pWidget->iStackingChildCapacity = 0;
	pWidget->bStackingChildrenDirty = 1;
}

static int __xuiWidgetBuildRenderChainRecursive(xui_context pContext, xui_widget pWidget,
	xui_rect_t tParentClip, int bHasClip, xui_widget* ppHead, xui_widget* ppTail, int* pCount)
{
	xui_widget pChild;
	const xui_widget* ppChildren;
	xui_rect_t tWorldRect;
	xui_rect_t tPaintRect;
	xui_rect_t tChildClip;
	int bChildHasClip;
	int iChildCount;
	int i;
	int iRet;

	if ( !pWidget->bVisible ) {
		pWidget->pRenderSubtreeLast = NULL;
		return XUI_OK;
	}
	tWorldRect = xuiInternalSnapRect(xuiWidgetGetWorldRect(pWidget));
	tPaintRect = bHasClip ? __xuiWidgetIntersectRect(tWorldRect, tParentClip) : tWorldRect;
	__xuiWidgetFillRenderNode(pWidget, tPaintRect);
	pWidget->pRenderPrev = *ppTail;
	pWidget->pRenderNext = NULL;
	if ( *ppTail != NULL ) (*ppTail)->pRenderNext = pWidget;
	else *ppHead = pWidget;
	*ppTail = pWidget;
	pWidget->bRenderNodeLinked = 1;
	(*pCount)++;
	tChildClip = tParentClip;
	bChildHasClip = bHasClip;
	if ( (pWidget->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN) ||
	     (pWidget->tLayout.iOverflow == XUI_OVERFLOW_CLIP) ) {
		tChildClip = bHasClip ? __xuiWidgetIntersectRect(tPaintRect, tParentClip) : tPaintRect;
		bChildHasClip = 1;
	}
	iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 0; i < iChildCount; i++ ) {
		pChild = ppChildren[i];
		iRet = __xuiWidgetBuildRenderChainRecursive(pContext, pChild, tChildClip, bChildHasClip,
			ppHead, ppTail, pCount);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pWidget->pRenderSubtreeLast = *ppTail;
	return XUI_OK;
}

static int __xuiWidgetEnsureCacheSurface(xui_widget pWidget, xui_widget_cache_slot_t* pSlot)
{
	xui_surface_desc_t tDesc;
	xui_proxy pProxy;
	int iWidth;
	int iHeight;
	int iRet;

	iWidth = xuiInternalPixelCeil(pWidget->tRect.fW);
	iHeight = xuiInternalPixelCeil(pWidget->tRect.fH);
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pSlot->pSurface != NULL) && (pSlot->iWidth == iWidth) && (pSlot->iHeight == iHeight) ) {
		return XUI_OK;
	}
    /* Epoch never wraps: a reused allocator address cannot resurrect an old
     * cache identity. Exhaustion refuses a rebuild while preserving its slot. */
    if(pWidget->iCacheEpoch==UINT32_MAX)return XUI_ERROR_INVALID_STATE;
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
    if(pSlot->pSurface){
        int released=__xuiWidgetDestroyCacheSlot(pWidget,pSlot);
        if(released!=XUI_OK)return released;
        pSlot->iStateId=pWidget->iActiveUpdateStateId;
    }
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iKind = XUI_SURFACE_KIND_TEXTURE;
	tDesc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
	tDesc.iWidth = iWidth;
	tDesc.iHeight = iHeight;
	tDesc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
    ++pWidget->iCacheEpoch;
    pWidget->bCacheMutationActive=1;
	iRet = pProxy->surfaceCreate(pProxy, &pSlot->pSurface, &tDesc);
    pWidget->bCacheMutationActive=0;
    if(iRet==XUI_OK && (!__xuiWidgetValid(pWidget) || pWidget->pContext->bCloseRequested))iRet=XUI_ERROR_INVALID_STATE;
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pSlot->iWidth = iWidth;
	pSlot->iHeight = iHeight;
	pSlot->iFlags = XUI_WIDGET_DIRTY_CACHE;
	return XUI_OK;
}

/* Native ownership and visual parentage are different relations. A menu
 * owns its Popup while the Popup content visually contains that same menu.
 * Each relation is acyclic on its own; their union may contain such cycles. */
#define XUI_DESTROY_EXECUTING UINT32_C(0x80000000)
#define XUI_DESTROY_MEMBER UINT32_C(0x40000000)
#define XUI_DESTROY_PREPARING UINT32_C(0x20000000)
#define XUI_DESTROY_EPOCH_MASK UINT32_C(0x1fffffff)
static uint32_t __xuiWidgetVisitBegin(xui_context context)
{
    uint32_t epoch=(++context->iDestroyVisitEpoch)&XUI_DESTROY_EPOCH_MASK;
    if(!epoch){
        for(xui_widget value=context->pAllWidgets;value;value=value->pContextNext)
            value->iDestroyVisitEpoch&=~XUI_DESTROY_EPOCH_MASK;
        context->iDestroyVisitEpoch=epoch=1;
    }
    return epoch;
}
static int __xuiWidgetVisitSeen(xui_widget value,uint32_t epoch)
{return (value->iDestroyVisitEpoch&XUI_DESTROY_EPOCH_MASK)==epoch;}
static void __xuiWidgetVisitMark(xui_widget value,uint32_t epoch)
{value->iDestroyVisitEpoch=(value->iDestroyVisitEpoch&~XUI_DESTROY_EPOCH_MASK)|epoch;}
static int __xuiWidgetNativeOwnerContains(xui_widget value,xui_widget target)
{
    for(;__xuiWidgetMemoryValid(value);value=(xui_widget)value->pInternalOwner)
        if(value==target)return 1;
    return 0;
}
static void __xuiWidgetLinkPart(xui_widget owner,xui_widget part)
{
    part->pInternalOwner=owner;
    part->pOwnedPartNext=owner->pFirstOwnedPart;
    part->ppOwnedPartPrevious=&owner->pFirstOwnedPart;
    if(part->pOwnedPartNext)part->pOwnedPartNext->ppOwnedPartPrevious=&part->pOwnedPartNext;
    owner->pFirstOwnedPart=part;
}
static void __xuiWidgetUnlinkPart(xui_widget part)
{
    if(part->ppOwnedPartPrevious){
        *part->ppOwnedPartPrevious=part->pOwnedPartNext;
        if(part->pOwnedPartNext)part->pOwnedPartNext->ppOwnedPartPrevious=part->ppOwnedPartPrevious;
    }
    part->ppOwnedPartPrevious=NULL;part->pOwnedPartNext=NULL;part->pInternalOwner=NULL;
}
static int __xuiWidgetCanAttachChild(xui_widget pParent, xui_widget pChild, const void* pAuthority)
{

	if ( !__xuiWidgetValid(pParent) || !__xuiWidgetValid(pChild) || (pParent == pChild) ) {
		return 0;
	}
	if ( pParent->pContext != pChild->pContext ) {
		return 0;
	}
    if(pChild->pInternalOwner && (pChild->pInternalOwner!=pAuthority || !__xuiWidgetValid((xui_widget)pAuthority)))return 0;
	if ( (pChild->pParent != NULL) || (pChild->pPrevSibling != NULL) || (pChild->pNextSibling != NULL) ) {
		return 0;
	}
	if ( (pChild->pContext->pRoot == pChild) || (pChild->pContext->pOverlayRoot == pChild) ) {
		return 0;
	}
    for(xui_widget scan=pParent;scan;scan=scan->pParent)if(scan==pChild)return 0;
	return 1;
}

static xui_rect_t __xuiWidgetContentRect(xui_widget pWidget)
{
	xui_rect_t tRect;
	xui_thickness_t tPadding;

	tPadding = pWidget->tLayout.tPadding;
	tRect.fX = tPadding.fLeft;
	tRect.fY = tPadding.fTop;
	tRect.fW = pWidget->tRect.fW - tPadding.fLeft - tPadding.fRight;
	tRect.fH = pWidget->tRect.fH - tPadding.fTop - tPadding.fBottom;
	if ( tRect.fW < 0.0f ) {
		tRect.fW = 0.0f;
	}
	if ( tRect.fH < 0.0f ) {
		tRect.fH = 0.0f;
	}
	return xuiInternalSnapRect(tRect);
}

static int __xuiWidgetSizeValid(xui_vec2_t tSize)
{
	return __xuiNonNegativeFloatValid(tSize.fX) && __xuiNonNegativeFloatValid(tSize.fY);
}

static int __xuiWidgetMeasureOwnContent(xui_widget widget,xui_vec2_t constraint,xui_vec2_t* output)
{
    xui_vec2_t size=__xuiVec2(0.0f,0.0f);
    if(widget->onContentMeasure){
        constraint.fX=__xuiFiniteConstraint(constraint.fX);
        constraint.fY=__xuiFiniteConstraint(constraint.fY);
        int status=xuiInternalInvokeContentMeasure(widget,constraint,&size);
        if(status!=XUI_OK)return status;
        if(!__xuiWidgetSizeValid(size))return XUI_ERROR_INVALID_STATE;
    }
    if(!__xuiWidgetValid(widget) || widget->pContext->bCloseRequested)return XUI_ERROR_INVALID_STATE;
    *output=__xuiWidgetClampSize(widget,size);return XUI_OK;
}

static int __xuiLayoutStructValid(const xui_layout_t* pLayout)
{
	if ( pLayout == NULL ) {
		return 0;
	}
	if ( !__xuiLayoutTypeValid(pLayout->iLayoutType) ||
	     !__xuiSizeModeValid(pLayout->iWidthMode) ||
	     !__xuiSizeModeValid(pLayout->iHeightMode) ||
	     !__xuiFlowModeValid(pLayout->iFlowMode) ||
	     !__xuiDockValid(pLayout->iDock) ||
	     !__xuiOverflowValid(pLayout->iOverflow) ||
	     !__xuiMeasureContainmentValid(pLayout->iMeasureContainment) ||
	     !__xuiAlignValid(pLayout->iAlignX) ||
	     !__xuiAlignValid(pLayout->iAlignY) ) {
		return 0;
	}
	if ( (pLayout->iTableRow < 0) || (pLayout->iTableColumn < 0) ||
	     (pLayout->iTableRowSpan <= 0) || (pLayout->iTableColumnSpan <= 0) ) {
		return 0;
	}
	if ( pLayout->iGridColumnCount < 0 ) {
		return 0;
	}
	if ( !__xuiNonNegativeFloatValid(pLayout->fPreferredWidth) ||
	     !__xuiNonNegativeFloatValid(pLayout->fPreferredHeight) ||
	     !__xuiNonNegativeFloatValid(pLayout->fMinWidth) ||
	     !__xuiNonNegativeFloatValid(pLayout->fMinHeight) ||
	     !__xuiNonNegativeFloatValid(pLayout->fMaxWidth) ||
	     !__xuiNonNegativeFloatValid(pLayout->fMaxHeight) ||
	     !__xuiNonNegativeFloatValid(pLayout->fGrow) ||
	     !__xuiNonNegativeFloatValid(pLayout->fShrink) ||
	     !__xuiNonNegativeFloatValid(pLayout->fGap) ||
	     !__xuiNonNegativeFloatValid(pLayout->fGridItemWidth) ||
	     !__xuiNonNegativeFloatValid(pLayout->fGridItemHeight) ||
	     !__xuiThicknessValid(pLayout->tMargin) ||
	     !__xuiThicknessValid(pLayout->tPadding) ) {
		return 0;
	}
	if ( (pLayout->fMinWidth > pLayout->fMaxWidth) || (pLayout->fMinHeight > pLayout->fMaxHeight) ) {
		return 0;
	}
	return 1;
}

static int __xuiWidgetTypeBelongsToContext(xui_context pContext, xui_widget_type pType)
{
	return __xuiWidgetTypeValid(pType) &&
	       ((pType == &g_xuiWidgetBaseType) || (pType->pContext == pContext));
}

static int __xuiWidgetTypeInherits(xui_widget_type pType, xui_widget_type pBase)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType == pBase ) {
			return 1;
		}
	}
	return 0;
}

static xui_widget_type __xuiWidgetFindTypeInContext(xui_context pContext, const char* sName)
{
	xui_widget_type pType;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	if ( __xuiStringEqual(g_xuiWidgetBaseType.sName, sName) ) {
		return &g_xuiWidgetBaseType;
	}
	for ( pType = pContext->pWidgetTypes; pType != NULL; pType = pType->pNext ) {
		if ( __xuiStringEqual(pType->sName, sName) ) {
			return pType;
		}
	}
	return NULL;
}

static int __xuiWidgetTypeDescValid(xui_context pContext, const xui_widget_type_desc_t* pDesc)
{
	xui_widget_type pParent;

	if ( !xuiInternalContextIsValid(pContext) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sName == NULL) || (pDesc->sName[0] == '\0') ) {
		return 0;
	}
	if ( (pDesc->iFlags & ~(XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY |
		XUI_WIDGET_TYPE_UPDATE_ON_INACTIVE | XUI_WIDGET_TYPE_PREPARE_FRAME_DEPENDENT)) != 0 ) {
		return 0;
	}
	pParent = (pDesc->pParent != NULL) ? pDesc->pParent : &g_xuiWidgetBaseType;
	if ( !__xuiWidgetTypeBelongsToContext(pContext, pParent) ) {
		return 0;
	}
	if ( pDesc->iTypeDataSize < pParent->iTypeDataSize ) {
		return 0;
	}
	if ( (pDesc->iFlags & XUI_WIDGET_TYPE_DEFAULT_LAYOUT) != 0 ) {
		if ( !__xuiLayoutStructValid(&pDesc->tLayout) ) {
			return 0;
		}
	}
	if ( (pDesc->iFlags & XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY) != 0 ) {
		if ( !__xuiCachePolicyValid(&pDesc->tCachePolicy) ) {
			return 0;
		}
	}
	return 1;
}

static xui_widget_content_measure_proc __xuiWidgetTypeContentMeasure(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onContentMeasure != NULL ) {
			return pType->onContentMeasure;
		}
	}
	return NULL;
}

static xui_widget_cache_render_proc __xuiWidgetTypeCacheRender(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onCacheRender != NULL ) {
			return pType->onCacheRender;
		}
	}
	return NULL;
}

static xui_internal_prepare_paint_proc __xuiWidgetTypePreparePaint(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onPreparePaint != NULL ) {
			return pType->onPreparePaint;
		}
	}
	return NULL;
}

static int __xuiWidgetTypePrepareFrameDependent(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( (pType->iFlags & XUI_WIDGET_TYPE_PREPARE_FRAME_DEPENDENT) != 0 ) return 1;
	}
	return 0;
}

static int __xuiWidgetAggregatePrepareFrameDependent(xui_widget pWidget)
{
	xui_widget pChild;
	if ( __xuiWidgetTypePrepareFrameDependent(pWidget->pType) ) return 1;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( pChild->bSubtreePrepareFrameDependent ) return 1;
	}
	return 0;
}

static void __xuiWidgetRepairPrepareFrameDependentToRoot(xui_widget pWidget)
{
	xui_widget pScan;
	int bOld;
	int bNew;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		bOld = pScan->bSubtreePrepareFrameDependent;
		bNew = __xuiWidgetAggregatePrepareFrameDependent(pScan);
		pScan->bSubtreePrepareFrameDependent = bNew;
		if ( bNew == bOld ) break;
	}
}

static xui_internal_parent_changed_proc __xuiWidgetTypeParentChanged(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onParentChanged != NULL ) return pType->onParentChanged;
	}
	return NULL;
}

static void __xuiWidgetNotifyParentChanged(xui_widget pWidget,
	xui_widget pOldParent, xui_widget pNewParent)
{
	xui_internal_parent_changed_proc onParentChanged;

	if ( !__xuiWidgetValid(pWidget) || pOldParent == pNewParent ) return;
	onParentChanged = __xuiWidgetTypeParentChanged(pWidget->pType);
	if ( onParentChanged != NULL ) onParentChanged(pWidget, pOldParent, pNewParent);
}

static xui_widget_update_proc __xuiWidgetTypeUpdate(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onUpdate != NULL ) {
			return pType->onUpdate;
		}
	}
	return NULL;
}

static int __xuiWidgetAggregateHasUpdate(xui_widget pWidget, int bInactive)
{
	xui_widget pChild;

	if ( pWidget->onUpdate != NULL &&
		(!bInactive || (pWidget->pType->iFlags & XUI_WIDGET_TYPE_UPDATE_ON_INACTIVE) != 0) ) {
		return 1;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( bInactive ? pChild->bSubtreeHasInactiveUpdate : pChild->bSubtreeHasUpdate ) return 1;
	}
	return 0;
}

#if XUI_ENABLE_INCREMENTAL_VERIFY
static unsigned __xuiWidgetVerifyUpdateStateRecursive(xui_widget pWidget, int* pValid)
{
	xui_widget pChild;
	unsigned iState = pWidget->onUpdate != NULL ? 1u : 0u;
	if ( pWidget->onUpdate != NULL &&
		(pWidget->pType->iFlags & XUI_WIDGET_TYPE_UPDATE_ON_INACTIVE) != 0 ) iState |= 2u;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling )
		iState |= __xuiWidgetVerifyUpdateStateRecursive(pChild, pValid);
	if ( pWidget->bSubtreeHasUpdate != ((iState & 1u) != 0) ||
		pWidget->bSubtreeHasInactiveUpdate != ((iState & 2u) != 0) ) *pValid = 0;
	return iState;
}

static void __xuiWidgetVerifyUpdateState(xui_widget pWidget)
{
	int bValid = 1;
	if ( pWidget == NULL ) return;
	while ( pWidget->pParent != NULL ) pWidget = pWidget->pParent;
	(void)__xuiWidgetVerifyUpdateStateRecursive(pWidget, &bValid);
	if ( !bValid ) {
		fprintf(stderr, "XUI incremental verification failed: update aggregate\n");
		abort();
	}
}
#endif

static void __xuiWidgetRepairUpdateStateToRoot(xui_widget pWidget)
{
	xui_widget pScan;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		int bOldUpdate = pScan->bSubtreeHasUpdate;
		int bOldInactive = pScan->bSubtreeHasInactiveUpdate;
		pScan->bSubtreeHasUpdate = __xuiWidgetAggregateHasUpdate(pScan, 0);
		pScan->bSubtreeHasInactiveUpdate = __xuiWidgetAggregateHasUpdate(pScan, 1);
		if ( pScan->bSubtreeHasUpdate == bOldUpdate &&
			pScan->bSubtreeHasInactiveUpdate == bOldInactive ) break;
	}
#if XUI_ENABLE_INCREMENTAL_VERIFY
	__xuiWidgetVerifyUpdateState(pWidget);
#endif
}

static xui_widget_type __xuiWidgetTypeDefaultLayoutOwner(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( (pType->iFlags & XUI_WIDGET_TYPE_DEFAULT_LAYOUT) != 0 ) {
			return pType;
		}
	}
	return NULL;
}

static xui_widget_type __xuiWidgetTypeDefaultCacheOwner(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( (pType->iFlags & XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY) != 0 ) {
			return pType;
		}
	}
	return NULL;
}

static int __xuiStyleApplyRuleProperties(xui_style_rule_t* pRule, xui_style_prop_t** ppProps, int* pCount)
{
	int i;
	int iRet;

	if ( pRule == NULL ) {
		return XUI_OK;
	}
	for ( i = 0; i < pRule->iPropertyCount; i++ ) {
		iRet = __xuiStyleBuilderUpsert(pRule->pContext, ppProps, pCount, &pRule->pProperties[i]);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiStyleApplyNamedStyle(xui_context pContext, const char* sName, xui_style_prop_t** ppProps, int* pCount, int iDepth);

static int __xuiStyleApplyRuleWithParent(xui_context pContext, xui_style_rule_t* pRule, xui_style_prop_t** ppProps, int* pCount, int iDepth)
{
	int iRet;

	if ( pRule == NULL ) {
		return XUI_OK;
	}
	if ( iDepth > XUI_STYLE_MAX_PARENT_DEPTH ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pRule->sParent != NULL) && (pRule->sParent[0] != '\0') ) {
		iRet = __xuiStyleApplyNamedStyle(pContext, pRule->sParent, ppProps, pCount, iDepth + 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return __xuiStyleApplyRuleProperties(pRule, ppProps, pCount);
}

static int __xuiStyleApplyNamedStyle(xui_context pContext, const char* sName, xui_style_prop_t** ppProps, int* pCount, int iDepth)
{
	xui_style_rule_t* pRule;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return XUI_OK;
	}
	pRule = __xuiStyleFindRule(pContext, XUI_STYLE_RULE_NAMED, sName, NULL);
	return __xuiStyleApplyRuleWithParent(pContext, pRule, ppProps, pCount, iDepth);
}

static int __xuiStyleApplyTypeChain(xui_context pContext, xui_widget_type pType, xui_style_prop_t** ppProps, int* pCount)
{
	xui_style_rule_t* pRule;
	int iRet;

	if ( !__xuiWidgetTypeValid(pType) ) {
		return XUI_OK;
	}
	iRet = __xuiStyleApplyTypeChain(pContext, pType->pParent, ppProps, pCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pRule = __xuiStyleFindRule(pContext, XUI_STYLE_RULE_TYPE, NULL, pType);
	return __xuiStyleApplyRuleWithParent(pContext, pRule, ppProps, pCount, 0);
}

static int __xuiWidgetBuildResolvedStyle(xui_widget pWidget, xui_style_prop_t** ppProps, int* pCount, uint32_t* pHash)
{
	xui_context pContext;
	xui_style_rule_t* pRule;
	int i;
	int iRet;

	*ppProps = NULL;
	*pCount = 0;
	*pHash = 0;
	pContext = pWidget->pContext;
	{
		xui_style_property_entry_t* pEntry;
		xui_style_prop_t tDefaultProp;

		for ( pEntry = pContext->pStyleProperties; pEntry != NULL; pEntry = pEntry->pNext ) {
			if ( (pEntry->tDefaultValue.iType == XUI_STYLE_VALUE_NONE) ||
			     ((pEntry->pWidgetType != NULL) && !__xuiWidgetTypeInherits(pWidget->pType, pEntry->pWidgetType)) ) {
				continue;
			}
			memset(&tDefaultProp, 0, sizeof(tDefaultProp));
			tDefaultProp.iPropertyId = pEntry->iPropertyId;
			tDefaultProp.sName = pEntry->sName;
			tDefaultProp.iDirtyFlags = pEntry->iDirtyFlags;
			tDefaultProp.tValue = pEntry->tDefaultValue;
			iRet = __xuiStyleBuilderUpsert(pContext, ppProps, pCount, &tDefaultProp);
			if ( iRet != XUI_OK ) {
				__xuiStylePropArrayClear(*ppProps, *pCount);
				*ppProps = NULL;
				*pCount = 0;
				return iRet;
			}
		}
	}
	for ( i = 0; i < pContext->iDefaultStyleCount; i++ ) {
		iRet = __xuiStyleBuilderUpsert(pWidget->pContext, ppProps, pCount, &pContext->pDefaultStyle[i]);
		if ( iRet != XUI_OK ) {
			__xuiStylePropArrayClear(*ppProps, *pCount);
			*ppProps = NULL;
			*pCount = 0;
			return iRet;
		}
	}
	if ( pWidget->pParent != NULL ) {
		if ( pWidget->pParent->iResolvedStyleGeneration != pContext->iStyleGeneration ) {
			iRet = xuiWidgetResolveStyle(pWidget->pParent);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
		for ( i = 0; i < pWidget->pParent->iResolvedStyleCount; i++ ) {
			if ( __xuiStylePropIsInherited(pContext, &pWidget->pParent->pResolvedStyle[i]) ) {
				iRet = __xuiStyleBuilderUpsert(pContext, ppProps, pCount, &pWidget->pParent->pResolvedStyle[i]);
				if ( iRet != XUI_OK ) {
					__xuiStylePropArrayClear(*ppProps, *pCount);
					*ppProps = NULL;
					*pCount = 0;
					return iRet;
				}
			}
		}
	}
	iRet = __xuiStyleApplyTypeChain(pContext, pWidget->pType, ppProps, pCount);
	if ( iRet != XUI_OK ) {
		__xuiStylePropArrayClear(*ppProps, *pCount);
		*ppProps = NULL;
		*pCount = 0;
		return iRet;
	}
	iRet = __xuiStyleApplyNamedStyle(pContext, pWidget->sStyleName, ppProps, pCount, 0);
	if ( iRet != XUI_OK ) {
		__xuiStylePropArrayClear(*ppProps, *pCount);
		*ppProps = NULL;
		*pCount = 0;
		return iRet;
	}
	for ( i = 0; i < pWidget->iStyleClassCount; i++ ) {
		pRule = __xuiStyleFindRule(pContext, XUI_STYLE_RULE_CLASS, pWidget->pStyleClasses[i], NULL);
		iRet = __xuiStyleApplyRuleWithParent(pContext, pRule, ppProps, pCount, 0);
		if ( iRet != XUI_OK ) {
			__xuiStylePropArrayClear(*ppProps, *pCount);
			*ppProps = NULL;
			*pCount = 0;
			return iRet;
		}
	}
	for ( pRule = pContext->pStyleRules; pRule != NULL; pRule = pRule->pNext ) {
		if ( (pRule->iMagic != XUI_STYLE_RULE_MAGIC) ||
		     (pRule->iKind != XUI_STYLE_RULE_STATE_CLASS) ||
		     ((xuiWidgetGetInputState(pWidget) & pRule->iStateMask) != pRule->iStateMask) ||
		     !xuiWidgetHasStyleClass(pWidget, pRule->sName) ) {
			continue;
		}
		iRet = __xuiStyleApplyRuleWithParent(pContext, pRule, ppProps, pCount, 0);
		if ( iRet != XUI_OK ) {
			__xuiStylePropArrayClear(*ppProps, *pCount);
			*ppProps = NULL;
			*pCount = 0;
			return iRet;
		}
	}
	for ( i = 0; i < pWidget->iInlineStyleCount; i++ ) {
		iRet = __xuiStyleBuilderUpsert(pWidget->pContext, ppProps, pCount, &pWidget->pInlineStyle[i]);
		if ( iRet != XUI_OK ) {
			__xuiStylePropArrayClear(*ppProps, *pCount);
			*ppProps = NULL;
			*pCount = 0;
			return iRet;
		}
	}
	*pHash = __xuiStyleHashProps(*ppProps, *pCount);
	return XUI_OK;
}

static uint32_t __xuiWidgetStyleDiffDirty(const xui_style_prop_t* pOldProps, int iOldCount, const xui_style_prop_t* pNewProps, int iNewCount, uint32_t iOldHash, uint32_t iNewHash)
{
	const xui_style_prop_t* pOld;
	const xui_style_prop_t* pNew;
	uint32_t iFlags;
	int i;

	if ( (iOldHash == iNewHash) && (iOldCount == iNewCount) ) {
		return 0;
	}
	iFlags = 0;
	for ( i = 0; i < iNewCount; i++ ) {
		pNew = &pNewProps[i];
		pOld = __xuiStyleFindPropConst(pOldProps, iOldCount, pNew->sName);
		if ( (pOld == NULL) || !__xuiStylePropEqual(pOld, pNew) ) {
			iFlags |= pNew->iDirtyFlags;
			if ( pOld != NULL ) {
				iFlags |= pOld->iDirtyFlags;
			}
		}
	}
	for ( i = 0; i < iOldCount; i++ ) {
		pOld = &pOldProps[i];
		pNew = __xuiStyleFindPropConst(pNewProps, iNewCount, pOld->sName);
		if ( pNew == NULL ) {
			iFlags |= pOld->iDirtyFlags;
		}
	}
	if ( (iFlags == 0) && (iOldHash != iNewHash) ) {
		iFlags = XUI_STYLE_DIRTY_DEFAULT;
	}
	return iFlags;
}

static int __xuiStyleResolveWidgetTree(xui_widget pWidget)
{
	xui_widget pChild;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_OK;
	}
	iRet = xuiWidgetResolveStyle(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiStyleResolveWidgetTree(pChild);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiStyleBumpGeneration(xui_context pContext)
{
	pContext->iStyleGeneration++;
	if ( pContext->iStyleGeneration == 0 ) {
		pContext->iStyleGeneration = 1;
	}
	xuiInternalContextBumpGeneration(pContext);
}

static int __xuiStyleCommitChange(xui_context pContext)
{
	if ( pContext->iStyleUpdateDepth > 0 ) {
		pContext->bStyleDirty = 1;
		return XUI_OK;
	}
	__xuiStyleBumpGeneration(pContext);
	pContext->bStyleDirty = 0;
	return xuiStyleRefresh(pContext);
}

static xui_font __xuiTypeFont(xui_widget widget,xui_widget_type type,size_t index)
{return *(xui_font*)((char*)widget->pTypeData+type->pFontOffsets[index]);}
static xui_surface __xuiTypeSurface(xui_widget widget,xui_widget_type type,size_t index)
{return *(xui_surface*)((char*)widget->pTypeData+type->pSurfaceOffsets[index]);}
typedef struct xui_type_resource_view_t {
    xui_widget_type type;
    xui_surface_slot_array_t array;
    size_t count;
} xui_type_resource_view_t;
static bool __xuiTypeResourceView(xui_widget widget,xui_widget_type type,xui_type_resource_view_t* view)
{
    memset(view,0,sizeof(*view));view->type=type;
    if(type->iFontSlotCount>SIZE_MAX-type->iSurfaceSlotCount)return false;
    view->count=type->iFontSlotCount+type->iSurfaceSlotCount;
    if(!type->onSurfaceSlots)return true;
    type->onSurfaceSlots(widget,widget->pTypeData,&view->array);
    const xui_surface_slot_array_t* array=&view->array;
    if(!array->count)return true;
    if(!array->data || !array->offsets || !array->fields || array->stride<sizeof(xui_surface) ||
       array->count>SIZE_MAX/array->fields || array->count*array->fields>SIZE_MAX-view->count ||
       array->count>SIZE_MAX/array->stride || (uintptr_t)array->data>UINTPTR_MAX-array->count*array->stride)return false;
    for(size_t i=0;i<array->fields;++i)
        if(array->offsets[i]>array->stride-sizeof(xui_surface) ||
           ((uintptr_t)array->data+array->offsets[i])%_Alignof(xui_surface) || array->stride%_Alignof(xui_surface))return false;
    view->count+=array->count*array->fields;return true;
}
static void* __xuiTypeResource(xui_widget widget,const xui_type_resource_view_t* view,size_t index,bool* surface)
{
    xui_widget_type type=view->type;*surface=index>=type->iFontSlotCount;
    if(!*surface)return __xuiTypeFont(widget,type,index);
    index-=type->iFontSlotCount;
    if(index<type->iSurfaceSlotCount)return __xuiTypeSurface(widget,type,index);
    index-=type->iSurfaceSlotCount;
    const xui_surface_slot_array_t* array=&view->array;
    return *(xui_surface*)((char*)array->data+(index/array->fields)*array->stride+array->offsets[index%array->fields]);
}
static int __xuiTypeResourceRelease(bool surface,void* value)
{return surface ? xuiSurfaceReleaseChecked(value):xuiFontReleaseChecked(value);}
static void __xuiTypeResourceRollback(xui_widget widget,xui_widget_type type,int* status,xerror** first)
{
    if(!__xuiWidgetTypeValid(type))return;
    xui_type_resource_view_t view;
    if(!__xuiTypeResourceView(widget,type,&view)){xuiInternalContextRecordOutcome(widget->pContext,XUI_ERROR_INVALID_STATE,NULL);return;}
    for(size_t i=view.count;i>0;--i){
        bool surface=false;void* value=__xuiTypeResource(widget,&view,i-1,&surface);
        if(value)xuiInternalContextCleanupOutcome(widget->pContext,__xuiTypeResourceRelease(surface,value),status,first);
    }
    __xuiTypeResourceRollback(widget,type->pParent,status,first);
}
static int __xuiTypeResourceAdmit(xui_widget widget,xui_widget_type type)
{
    if(!__xuiWidgetTypeValid(type) || !widget->pContext->pOwnedProxy)return XUI_OK;
    int status=__xuiTypeResourceAdmit(widget,type->pParent);if(status!=XUI_OK)return status;
    xui_type_resource_view_t view;
    if(!__xuiTypeResourceView(widget,type,&view)){
        xerror* first=NULL;status=XUI_ERROR_INVALID_STATE;__xuiTypeResourceRollback(widget,type->pParent,&status,&first);xrtSetErrorTake(first);return status;
    }
    for(size_t i=0;i<view.count;++i){
        bool surface=false;void* value=__xuiTypeResource(widget,&view,i,&surface);
        status=surface ? xuiInternalSurfaceAdmit(widget->pContext,value):xuiInternalFontAdmit(widget->pContext,value);
        if(status!=XUI_OK){
            xerror* first=xrtTakeError();xui_cleanup_prefix_t prefix={0};
            int begun=xuiInternalCleanupPrefixBegin(widget->pContext,&prefix,status,first);
            for(size_t j=i;j>0;--j){
                value=__xuiTypeResource(widget,&view,j-1,&surface);
                if(value)xuiInternalContextCleanupOutcome(widget->pContext,__xuiTypeResourceRelease(surface,value),&status,&first);
            }
            __xuiTypeResourceRollback(widget,type->pParent,&status,&first);
            if(begun==XUI_OK)begun=xuiInternalCleanupPrefixEnd(&prefix);
            if(begun!=XUI_OK)xuiInternalContextRecordOutcome(widget->pContext,begun,first);
            xrtSetErrorTake(first);return status;
        }
    }
    return XUI_OK;
}
static void __xuiTypeDestroyCallbacks(xui_widget widget,xui_widget_type type)
{
    if(!__xuiWidgetTypeValid(type))return;
    if(type->onDestroy)type->onDestroy(widget,widget->pTypeData,type->pUser);
    __xuiTypeDestroyCallbacks(widget,type->pParent);
}
typedef struct xui_type_resource_frame_t {
    xui_type_resource_view_t view;
    const struct xui_type_resource_frame_t* next;
} xui_type_resource_frame_t;
/* All inheritance frames and actual references are captured on the C stack
 * before the first destructor can clear shared TypeData or free item arrays.
 * Parent owners are captured first, and released last. No heap directory or
 * additional retained copy of the owned resources is required. */
static void __xuiTypeDestroyCaptured(xui_widget widget,xui_widget_type leaf,const xui_type_resource_frame_t* frame,size_t index)
{
    if(index<frame->view.count){
        bool surface=false;void* value=__xuiTypeResource(widget,&frame->view,index,&surface);
        __xuiTypeDestroyCaptured(widget,leaf,frame,index+1);
        if(value){
            if(surface)(void)xuiInternalSurfaceRelease(widget->pContext,value);
            else (void)xuiInternalFontRelease(widget->pContext,value);
        }
    }else if(frame->next)__xuiTypeDestroyCaptured(widget,leaf,frame->next,0);
    else __xuiTypeDestroyCallbacks(widget,leaf);
}
static void __xuiTypeDestroyFrames(xui_widget widget,xui_widget_type leaf,xui_widget_type type,const xui_type_resource_frame_t* children)
{
    if(!__xuiWidgetTypeValid(type)){
        if(children)__xuiTypeDestroyCaptured(widget,leaf,children,0);
        else __xuiTypeDestroyCallbacks(widget,leaf);
        return;
    }
    xui_type_resource_frame_t frame={.next=children};
    if(!__xuiTypeResourceView(widget,type,&frame.view)){
        xuiInternalContextRecordOutcome(widget->pContext,XUI_ERROR_INVALID_STATE,NULL);
        frame.view.count=0;
    }
    __xuiTypeDestroyFrames(widget,leaf,type->pParent,&frame);
}
static void __xuiWidgetTypeDestroyChain(xui_widget pWidget,xui_widget_type pType);
static int __xuiWidgetTypeInitChain(xui_widget pWidget, xui_widget_type pType, const void* pCreateData)
{
	int iRet;

	if ( !__xuiWidgetTypeValid(pType) ) {
		return XUI_OK;
	}
	iRet = __xuiWidgetTypeInitChain(pWidget, pType->pParent, pCreateData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pType->onInit != NULL ) {
		iRet = pType->onInit(pWidget, pWidget->pTypeData, pCreateData, pType->pUser);
		if ( iRet != XUI_OK ) return iRet;
		/* A successful native return completed this stage even if its semantic
		 * diagnostic refuses construction; rollback must destroy that stage. */
		pWidget->pInitializedType=pType;
		if(xrtGetError())return XUI_ERROR_CALLBACK_FAILED;
	}
    pWidget->pInitializedType=pType;
	return XUI_OK;
}

static void __xuiWidgetTypeDestroyChain(xui_widget pWidget, xui_widget_type pType)
{
	if ( !__xuiWidgetTypeValid(pType) ) {
		return;
	}
    if(pWidget->bTypeInitialized && pWidget->pContext->pOwnedProxy)
        __xuiTypeDestroyFrames(pWidget,pType,pType,NULL);
    else __xuiTypeDestroyCallbacks(pWidget,pType);
}

void xuiInternalContextDestroyWidgetTypes(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type pNext;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return;
	}
	pType = pContext->pWidgetTypes;
	while ( pType != NULL ) {
		pNext = pType->pNext;
		pType->iMagic = 0;
		if ( pType->sName != NULL ) {
			xrtFree(pType->sName);
		}
		xrtFree(pType);
		pType = pNext;
	}
	pContext->pWidgetTypes = NULL;
}

void xuiInternalContextDestroyStyles(xui_context pContext)
{
	xui_style_rule_t* pRule;
	xui_style_rule_t* pNext;
	xui_style_property_entry_t* pProperty;
	xui_style_property_entry_t* pNextProperty;
	xui_style_token_t* pToken;
	xui_style_token_t* pNextToken;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return;
	}
	pRule = pContext->pStyleRules;
	while ( pRule != NULL ) {
		pNext = pRule->pNext;
		__xuiStyleRuleDestroy(pRule);
		pRule = pNext;
	}
	pContext->pStyleRules = NULL;
	pProperty = pContext->pStyleProperties;
	while ( pProperty != NULL ) {
		pNextProperty = pProperty->pNext;
		if ( pProperty->sName != NULL ) {
			xrtFree(pProperty->sName);
		}
		__xuiStyleValueReset(&pProperty->tDefaultValue);
		xrtFree(pProperty);
		pProperty = pNextProperty;
	}
	pContext->pStyleProperties = NULL;
	memset(pContext->pStylePropertiesById, 0, sizeof(pContext->pStylePropertiesById));
	memset(pContext->pStylePropertiesByName, 0, sizeof(pContext->pStylePropertiesByName));
	pToken = pContext->pStyleTokens;
	while ( pToken != NULL ) {
		pNextToken = pToken->pNext;
		if ( pToken->sName != NULL ) {
			xrtFree(pToken->sName);
		}
		__xuiStyleValueReset(&pToken->tValue);
		xrtFree(pToken);
		pToken = pNextToken;
	}
	pContext->pStyleTokens = NULL;
	__xuiStylePropArrayClear(pContext->pDefaultStyle, pContext->iDefaultStyleCount);
	pContext->pDefaultStyle = NULL;
	pContext->iDefaultStyleCount = 0;
	pContext->iStyleUpdateDepth = 0;
	pContext->bStyleDirty = 0;
}

static void __xuiWidgetBumpLayoutVersion(xui_widget pWidget)
{
	pWidget->iLayoutVersion++;
	if ( pWidget->iLayoutVersion == 0 ) {
		pWidget->iLayoutVersion = 1;
	}
}

static void __xuiWidgetDestroyAccessibilityData(xui_widget pWidget)
{
	xuiInternalAccessibilityDetach(pWidget);
	if ( pWidget->sAccessibleName != NULL ) xrtFree(pWidget->sAccessibleName);
	if ( pWidget->sAccessibleDescription != NULL ) xrtFree(pWidget->sAccessibleDescription);
	pWidget->sAccessibleName = NULL;
	pWidget->sAccessibleDescription = NULL;
	pWidget->onAccessibleCount = NULL;
	pWidget->onAccessibleGet = NULL;
	pWidget->onAccessibleAction = NULL;
	pWidget->onAccessibleFocus = NULL;
	pWidget->pAccessibleUser = NULL;
}

static xui_widget_layout_complete_proc __xuiWidgetTypeLayoutComplete(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onLayoutComplete != NULL ) {
			return pType->onLayoutComplete;
		}
	}
	return NULL;
}

static xui_widget_cursor_proc __xuiWidgetTypeQueryCursor(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onQueryCursor != NULL ) {
			return pType->onQueryCursor;
		}
	}
	return NULL;
}

static xui_widget_layout_children_proc __xuiWidgetTypeLayoutChildren(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onLayoutChildren != NULL ) {
			return pType->onLayoutChildren;
		}
	}
	return NULL;
}

static xui_widget_layout_prepare_proc __xuiWidgetTypeLayoutPrepare(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onLayoutPrepare != NULL ) {
			return pType->onLayoutPrepare;
		}
	}
	return NULL;
}

static int __xuiWidgetLayoutChanged(xui_widget pWidget)
{
	__xuiWidgetBumpLayoutVersion(pWidget);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiWidgetBuildTrackArray(
	xui_table_track_t* pOldTracks,
	int iOldCount,
	int iNewCount,
	xui_table_track_t** ppNewTracks)
{
	xui_table_track_t* pTracks;
	int i;
	int iCopyCount;

	if ( iOldCount < 0 || iNewCount < 0 || ppNewTracks == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppNewTracks = NULL;
	if ( iNewCount == 0 ) {
		return XUI_OK;
	}
#if SIZE_MAX <= UINT32_MAX
	if ( (uint32_t)iNewCount > (uint32_t)(SIZE_MAX / sizeof(*pTracks)) ) return XUI_ERROR_LIMIT_EXCEEDED;
#endif
	pTracks = (xui_table_track_t*)xrtMalloc(sizeof(*pTracks) * (size_t)iNewCount);
	if ( pTracks == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iCopyCount = iOldCount;
	if ( iCopyCount > iNewCount ) {
		iCopyCount = iNewCount;
	}
	for ( i = 0; i < iCopyCount; i++ ) {
		pTracks[i] = pOldTracks[i];
	}
	for ( i = iCopyCount; i < iNewCount; i++ ) {
		pTracks[i] = __xuiTableTrackDefault();
	}
	*ppNewTracks = pTracks;
	return XUI_OK;
}

static int __xuiLayoutRootNeedsArrange(xui_widget root, xui_rect_t rect)
{
	return root != NULL && (((root->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) != 0) ||
		!root->bArrangeValid || root->tRect.fX != rect.fX || root->tRect.fY != rect.fY ||
		root->tRect.fW != rect.fW || root->tRect.fH != rect.fH);
}

static int __xuiLayoutOperation(xui_context pContext)
{
	xui_widget pRoot;
	xui_widget pOverlayRoot;
	xui_size_t tViewport;
	xui_rect_t tRootRect;
	int bNeedRoot;
	int bNeedOverlay;
	int bStable;
	int iPass;
	int iRet;
	uint32_t iGeneration;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pRoot = pContext->pRoot;
	pOverlayRoot = pContext->pOverlayRoot;
	iGeneration = pContext->tLayoutStats.iGeneration + 1u;
	memset(&pContext->tLayoutStats, 0, sizeof(pContext->tLayoutStats));
	pContext->tLayoutStats.iSize = sizeof(pContext->tLayoutStats);
	pContext->tLayoutStats.iGeneration = iGeneration != 0u ? iGeneration : 1u;
	pContext->tLayoutStats.iMaxPassCount = XUI_LAYOUT_MAX_PASSES;
	pContext->tLayoutStats.bStabilized = 1;
	tViewport = xuiGetViewportSize(pContext);
	if ( (tViewport.iW <= 0) || (tViewport.iH <= 0) ) {
		return XUI_OK;
	}
	tRootRect.fX = 0.0f;
	tRootRect.fY = 0.0f;
	tRootRect.fW = tViewport.iW;
	tRootRect.fH = tViewport.iH;
	tRootRect = xuiInternalSnapRect(tRootRect);
	for ( iPass = 0; iPass < XUI_LAYOUT_MAX_PASSES; iPass++ ) {
		bNeedRoot = __xuiLayoutRootNeedsArrange(pRoot, tRootRect);
		bNeedOverlay = __xuiLayoutRootNeedsArrange(pOverlayRoot, tRootRect);
		if ( !bNeedRoot && !bNeedOverlay ) break;
		pContext->tLayoutStats.iPassCount++;
		if ( bNeedRoot ) {
			iRet = xuiInternalLayoutArrange(pRoot, tRootRect);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( bNeedOverlay ) {
			iRet = xuiInternalLayoutArrange(pOverlayRoot, tRootRect);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pContext->onPopupLayoutChanged != NULL ) {
			iRet = pContext->onPopupLayoutChanged(pContext);
			if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
			if ( iRet != XUI_OK ) return iRet;
			pRoot = pContext->pRoot;
			pOverlayRoot = pContext->pOverlayRoot;
		}
	}
	bStable = ((pRoot == NULL) || ((pRoot->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) == 0)) &&
		((pOverlayRoot == NULL) || ((pOverlayRoot->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) == 0));
	pContext->tLayoutStats.bStabilized = bStable;
	if ( !bStable ) {
		xuiInternalReportError(pContext, pRoot, XUI_ERROR_LAYOUT_UNSTABLE, XUI_ERROR_STAGE_LAYOUT, 1,
			"layout.stabilize", "Layout did not stabilize; the last completed pass was preserved.");
	}
	return xuiInternalInputRefreshIme(pContext);
}

XUI_API int xuiLayout(xui_context pContext)
{
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiLayoutOperation(pContext);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiGetLayoutStats(xui_context pContext, xui_layout_stats_t* pStats)
{
	if ( !xuiInternalContextIsValid(pContext) || pStats == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pStats = pContext->tLayoutStats;
	pStats->iSize = sizeof(*pStats);
	return XUI_OK;
}

static int __xuiSetRootWidgetOperation(xui_context pContext, xui_widget pWidget)
{
	xui_widget pOldRoot;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget != NULL ) {
		if ( !__xuiWidgetValid(pWidget) || (pWidget->pContext != pContext) || (pWidget->pParent != NULL) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	if ( pContext->pRoot == pWidget ) {
		return XUI_OK;
	}
	pOldRoot = pContext->pRoot;
	if ( pOldRoot != NULL ) {
		xuiInternalContextDetachWidget(pContext, pOldRoot);
		if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
		if ( __xuiWidgetValid(pOldRoot) ) {
			__xuiWidgetMarkDirtyOnly(pOldRoot, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
		}
	}
	if ( pWidget != NULL && !__xuiWidgetValid(pWidget) ) pWidget = NULL;
	pContext->pRoot = pWidget;
	if ( pWidget != NULL ) {
		__xuiWidgetMarkDirtyOnly(pWidget, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( __xuiWidgetValid(pOldRoot) ) __xuiWidgetSyncInactiveTree(pOldRoot);
	return xuiInternalContextInvalidateAll(pContext);
}

XUI_API int xuiSetRootWidget(xui_context pContext, xui_widget pWidget)
{
    if(__xuiWidgetValid(pWidget) && pWidget->pInternalOwner)return XUI_ERROR_INVALID_STATE;
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiSetRootWidgetOperation(pContext, pWidget);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API xui_widget xuiGetRootWidget(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	return pContext->pRoot;
}

XUI_API xui_widget_type xuiWidgetGetBaseType(void)
{
	return &g_xuiWidgetBaseType;
}

XUI_API int xuiWidgetRegisterType(xui_context pContext, xui_widget_type* ppType, const xui_widget_type_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !__xuiWidgetTypeDescValid(pContext, pDesc) || (ppType == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppType = NULL;
	if ( (pDesc->iFlags & XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY) != 0 &&
	     !__xuiCachePolicySupported(pDesc->tCachePolicy.iPolicy) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( __xuiWidgetFindTypeInContext(pContext, pDesc->sName) != NULL ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	pType = (xui_widget_type)xrtMalloc(sizeof(*pType));
	if ( pType == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pType, 0, sizeof(*pType));
	pType->sName = __xuiStringDuplicate(pDesc->sName);
	if ( pType->sName == NULL ) {
		xrtFree(pType);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pType->iMagic = XUI_WIDGET_TYPE_MAGIC;
	pType->pContext = pContext;
	pType->pParent = (pDesc->pParent != NULL) ? pDesc->pParent : &g_xuiWidgetBaseType;
	pType->iFlags = pDesc->iFlags;
	pType->iTypeDataSize = pDesc->iTypeDataSize;
	pType->pUser = pDesc->pUser;
	pType->onInit = pDesc->onInit;
	pType->onDestroy = pDesc->onDestroy;
	pType->onContentMeasure = pDesc->onContentMeasure;
	pType->onLayoutPrepare = pDesc->onLayoutPrepare;
	pType->onLayoutChildren = pDesc->onLayoutChildren;
	pType->onLayoutComplete = pDesc->onLayoutComplete;
	pType->onCacheRender = pDesc->onCacheRender;
	pType->onUpdate = pDesc->onUpdate;
	pType->onQueryCursor = pDesc->onQueryCursor;
	pType->tLayout = pDesc->tLayout;
	pType->tCachePolicy = pDesc->tCachePolicy;
	pType->pNext = pContext->pWidgetTypes;
	pContext->pWidgetTypes = pType;
	*ppType = pType;
	return XUI_OK;
}

XUI_API int xuiWidgetTypeSetOwnershipTrace(xui_widget_type pType, xui_widget_type_ownership_trace_proc onTrace)
{
	if ( !__xuiWidgetTypeValid(pType) || pType->bBuiltin ||
	     !xuiInternalContextIsValid(pType->pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
	/* iWidgetCount covers only exact instances, not live descendants. */
	for ( xui_widget pWidget = pType->pContext->pAllWidgets; pWidget != NULL; pWidget = pWidget->pContextNext )
		for ( xui_widget_type pScan = pWidget->pType; pScan != NULL; pScan = pScan->pParent )
			if ( pScan == pType ) return XUI_ERROR_ALREADY_INITIALIZED;
	pType->onTraceOwnership = onTrace;
	return XUI_OK;
}

XUI_API int xuiWidgetUnregisterType(xui_widget_type pType)
{
	xui_context pContext;
	xui_widget_type pScan;
	xui_widget_type pPrev;

	if ( !__xuiWidgetTypeValid(pType) || pType->bBuiltin || (pType->pContext == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pType->iWidgetCount > 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	pContext = pType->pContext;
	for ( pScan = pContext->pWidgetTypes; pScan != NULL; pScan = pScan->pNext ) {
		if ( pScan->pParent == pType ) {
			return XUI_ERROR_ALREADY_INITIALIZED;
		}
	}
	pPrev = NULL;
	for ( pScan = pContext->pWidgetTypes; pScan != NULL; pScan = pScan->pNext ) {
		if ( pScan == pType ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pScan->pNext;
			} else {
				pContext->pWidgetTypes = pScan->pNext;
			}
			pType->iMagic = 0;
			xrtFree(pType->sName);
			xrtFree(pType);
			return XUI_OK;
		}
		pPrev = pScan;
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API xui_widget_type xuiWidgetFindType(xui_context pContext, const char* sName)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	return __xuiWidgetFindTypeInContext(pContext, sName);
}

XUI_API const char* xuiWidgetTypeGetName(xui_widget_type pType)
{
	return __xuiWidgetTypeValid(pType) ? pType->sName : NULL;
}

XUI_API xui_widget_type xuiWidgetTypeGetParent(xui_widget_type pType)
{
	return __xuiWidgetTypeValid(pType) ? pType->pParent : NULL;
}

XUI_API void* xuiWidgetTypeGetUserData(xui_widget_type pType)
{
	return __xuiWidgetTypeValid(pType) ? pType->pUser : NULL;
}

XUI_API int xuiStyleBeginUpdate(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iStyleUpdateDepth++;
	return XUI_OK;
}

XUI_API int xuiStyleEndUpdate(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) || (pContext->iStyleUpdateDepth <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iStyleUpdateDepth--;
	if ( (pContext->iStyleUpdateDepth == 0) && pContext->bStyleDirty ) {
		__xuiStyleBumpGeneration(pContext);
		pContext->bStyleDirty = 0;
		return xuiStyleRefresh(pContext);
	}
	return XUI_OK;
}

XUI_API uint32_t xuiStyleGetGeneration(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return 0;
	}
	return pContext->iStyleGeneration;
}

XUI_API int xuiStyleRefresh(xui_context pContext)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiStyleResolveWidgetTree(pContext->pRoot);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleResolveWidgetTree(pContext->pOverlayRoot);
}

XUI_API int xuiStyleSetNamed(xui_context pContext, const xui_style_desc_t* pStyle)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiStyleDescValid(pStyle, 1) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiStyleRuleSet(pContext, XUI_STYLE_RULE_NAMED, pStyle->sName, NULL, pStyle);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRemoveNamed(xui_context pContext, const char* sName)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiStyleFindRule(pContext, XUI_STYLE_RULE_NAMED, sName, NULL) == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiStyleRuleRemove(pContext, XUI_STYLE_RULE_NAMED, sName, NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleSetClass(xui_context pContext, const char* sClass, const xui_style_desc_t* pStyle)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sClass == NULL) || (sClass[0] == '\0') ||
	     !__xuiStyleDescValid(pStyle, 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiStyleRuleSet(pContext, XUI_STYLE_RULE_CLASS, sClass, NULL, pStyle);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRemoveClass(xui_context pContext, const char* sClass)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sClass == NULL) || (sClass[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiStyleFindRule(pContext, XUI_STYLE_RULE_CLASS, sClass, NULL) == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiStyleRuleRemove(pContext, XUI_STYLE_RULE_CLASS, sClass, NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleSetType(xui_context pContext, xui_widget_type pType, const xui_style_desc_t* pStyle)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiWidgetTypeBelongsToContext(pContext, pType) ||
	     !__xuiStyleDescValid(pStyle, 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiStyleRuleSet(pContext, XUI_STYLE_RULE_TYPE, NULL, pType, pStyle);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRemoveType(xui_context pContext, xui_widget_type pType)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiWidgetTypeBelongsToContext(pContext, pType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiStyleFindRule(pContext, XUI_STYLE_RULE_TYPE, NULL, pType) == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiStyleRuleRemove(pContext, XUI_STYLE_RULE_TYPE, NULL, pType);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleSetStateClass(xui_context pContext, const xui_state_style_desc_t* pStyle)
{
	xui_style_desc_t tStyle;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (pStyle == NULL) ||
	     (pStyle->iSize < sizeof(*pStyle)) ||
	     (pStyle->sClass == NULL) ||
	     (pStyle->sClass[0] == '\0') ||
	     (pStyle->iStateMask == 0) ||
	     (pStyle->iPropertyCount < 0) ||
	     ((pStyle->iPropertyCount > 0) && (pStyle->pProperties == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = sizeof(tStyle);
	tStyle.pProperties = pStyle->pProperties;
	tStyle.iPropertyCount = pStyle->iPropertyCount;
	iRet = __xuiStyleRuleSetEx(pContext, XUI_STYLE_RULE_STATE_CLASS, pStyle->sClass, NULL, &tStyle, pStyle->iStateMask);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRemoveStateClass(xui_context pContext, const char* sClass, uint32_t iStateMask)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (sClass == NULL) ||
	     (sClass[0] == '\0') ||
	     (iStateMask == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiStyleFindRuleEx(pContext, XUI_STYLE_RULE_STATE_CLASS, sClass, NULL, iStateMask) == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiStyleRuleRemoveEx(pContext, XUI_STYLE_RULE_STATE_CLASS, sClass, NULL, iStateMask);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleSetDefault(xui_context pContext, const xui_style_property_t* pProperties, int iPropertyCount)
{
	xui_style_prop_t* pCopy;
	int iCopyCount;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (iPropertyCount < 0) ||
	     ((iPropertyCount > 0) && (pProperties == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pCopy = NULL;
	iCopyCount = 0;
	iRet = __xuiStylePropArrayCopy(pContext, &pCopy, &iCopyCount, pProperties, iPropertyCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiStylePropArrayClear(pContext->pDefaultStyle, pContext->iDefaultStyleCount);
	pContext->pDefaultStyle = pCopy;
	pContext->iDefaultStyleCount = iCopyCount;
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleClearDefault(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->iDefaultStyleCount == 0 ) {
		return XUI_OK;
	}
	__xuiStylePropArrayClear(pContext->pDefaultStyle, pContext->iDefaultStyleCount);
	pContext->pDefaultStyle = NULL;
	pContext->iDefaultStyleCount = 0;
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRegisterProperty(xui_context pContext, const xui_style_property_info_t* pInfo, uint32_t* pPropertyId)
{
	xui_style_property_entry_t* pEntry;
	char* sName;
	xui_style_value_t tDefaultValue;
	xui_style_value_t tRequestedDefault;
	int bNew;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (pInfo == NULL) || (pInfo->iSize < sizeof(*pInfo)) ||
	     (pInfo->sName == NULL) || (pInfo->sName[0] == '\0') ||
	     !__xuiStyleValueTypeValid(pInfo->iValueType) ||
	     ((pInfo->iDirtyFlags & ~XUI_WIDGET_DIRTY_ALL) != 0) ||
	     ((pInfo->iFlags & ~(XUI_STYLE_PROPERTY_INHERITED | XUI_STYLE_PROPERTY_PRIVATE)) != 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pInfo->pWidgetType != NULL) && !__xuiWidgetTypeBelongsToContext(pContext, pInfo->pWidgetType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tRequestedDefault, 0, sizeof(tRequestedDefault));
	tRequestedDefault.iSize = sizeof(tRequestedDefault);
	memset(&tDefaultValue, 0, sizeof(tDefaultValue));
	if ( (pInfo->tDefaultValue.iSize >= sizeof(pInfo->tDefaultValue)) &&
	     (pInfo->tDefaultValue.iType != XUI_STYLE_VALUE_NONE) ) {
		if ( !__xuiStyleValueTypeValid(pInfo->tDefaultValue.iType) ||
		     (pInfo->tDefaultValue.iType == XUI_STYLE_VALUE_TOKEN) ||
		     ((pInfo->iValueType != XUI_STYLE_VALUE_NONE) && (pInfo->tDefaultValue.iType != pInfo->iValueType)) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		tRequestedDefault = pInfo->tDefaultValue;
	} else {
		tDefaultValue.iSize = sizeof(tDefaultValue);
		tDefaultValue.iType = XUI_STYLE_VALUE_NONE;
	}
	pEntry = __xuiStyleFindPropertyByName(pContext, pInfo->sName);
	/* Compare semantic fields before allocating, including borrowed GetInfo
	 * strings. Existing IDs stay stable, as on a real definition update. */
	if (pEntry != NULL && pEntry->iValueType == pInfo->iValueType &&
	    pEntry->iDirtyFlags == __xuiStyleDirtyFlags(pInfo->iDirtyFlags) &&
	    pEntry->iFlags == pInfo->iFlags && pEntry->pWidgetType == pInfo->pWidgetType &&
	    (tRequestedDefault.iType == XUI_STYLE_VALUE_FLOAT
	        ? pEntry->tDefaultValue.iType == XUI_STYLE_VALUE_FLOAT &&
	          memcmp(&pEntry->tDefaultValue.fFloat, &tRequestedDefault.fFloat, sizeof(float)) == 0
	        : __xuiStyleValueEqual(&pEntry->tDefaultValue, &tRequestedDefault))) {
		if (pPropertyId != NULL) *pPropertyId = pEntry->iPropertyId;
		return XUI_OK;
	}
	iRet = __xuiStyleValueCopy(&tDefaultValue, &tRequestedDefault);
	if (iRet != XUI_OK) return iRet;
	bNew = pEntry == NULL;
	sName = __xuiStringDuplicate(pInfo->sName);
	if ( sName == NULL ) {
		__xuiStyleValueReset(&tDefaultValue);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( bNew ) {
		pEntry = (xui_style_property_entry_t*)xrtCalloc(1, sizeof(*pEntry));
		if ( pEntry == NULL ) {
			xrtFree(sName);
			__xuiStyleValueReset(&tDefaultValue);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pEntry->iPropertyId = (pInfo->iPropertyId != 0) ? pInfo->iPropertyId : pContext->iNextStylePropertyId++;
		if ( pContext->iNextStylePropertyId == 0 ) {
			pContext->iNextStylePropertyId = 1;
		}
	} else {
		if ( pEntry->sName != NULL ) {
			xrtFree(pEntry->sName);
		}
		__xuiStyleValueReset(&pEntry->tDefaultValue);
	}
	pEntry->sName = sName;
	pEntry->iValueType = pInfo->iValueType;
	pEntry->iDirtyFlags = __xuiStyleDirtyFlags(pInfo->iDirtyFlags);
	pEntry->iFlags = pInfo->iFlags;
	pEntry->pWidgetType = pInfo->pWidgetType;
	pEntry->tDefaultValue = tDefaultValue;
	if (bNew) {
		unsigned byId = pEntry->iPropertyId & 255u;
		unsigned byName = __xuiStylePropertyNameBucket(pEntry->sName);
		pEntry->pNext = pContext->pStyleProperties;
		pContext->pStyleProperties = pEntry;
		pEntry->pIdNext = pContext->pStylePropertiesById[byId];
		pContext->pStylePropertiesById[byId] = pEntry;
		pEntry->pNameNext = pContext->pStylePropertiesByName[byName];
		pContext->pStylePropertiesByName[byName] = pEntry;
	}
	if ( pPropertyId != NULL ) {
		*pPropertyId = pEntry->iPropertyId;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API uint32_t xuiStyleFindProperty(xui_context pContext, const char* sName)
{
	xui_style_property_entry_t* pEntry;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return 0;
	}
	pEntry = __xuiStyleFindPropertyByName(pContext, sName);
	return (pEntry != NULL) ? pEntry->iPropertyId : 0;
}

XUI_API int xuiStyleGetPropertyInfo(xui_context pContext, uint32_t iPropertyId, xui_style_property_info_t* pInfo)
{
	xui_style_property_entry_t* pEntry;

	if ( !xuiInternalContextIsValid(pContext) || (pInfo == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pEntry = __xuiStyleFindPropertyById(pContext, iPropertyId);
	if ( pEntry == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iPropertyId = pEntry->iPropertyId;
	pInfo->sName = pEntry->sName;
	pInfo->iValueType = pEntry->iValueType;
	pInfo->iDirtyFlags = pEntry->iDirtyFlags;
	pInfo->iFlags = pEntry->iFlags;
	pInfo->pWidgetType = pEntry->pWidgetType;
	pInfo->tDefaultValue = pEntry->tDefaultValue;
	pInfo->tDefaultValue.iSize = sizeof(pInfo->tDefaultValue);
	return XUI_OK;
}

XUI_API int xuiStyleSetToken(xui_context pContext, const char* sName, const xui_style_value_t* pValue)
{
	xui_style_token_t* pToken;
	char* sCopy;
	xui_style_value_t tValue;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sName == NULL) || (sName[0] == '\0') ||
	     (pValue == NULL) || (pValue->iSize < sizeof(*pValue)) ||
	     !__xuiStyleValueTypeValid(pValue->iType) || (pValue->iType == XUI_STYLE_VALUE_TOKEN) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pValue->iType == XUI_STYLE_VALUE_STRING) && (pValue->sText == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tValue, 0, sizeof(tValue));
	iRet = __xuiStyleValueCopy(&tValue, pValue);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pToken = __xuiStyleFindToken(pContext, sName);
	if ( pToken == NULL ) {
		sCopy = __xuiStringDuplicate(sName);
		if ( sCopy == NULL ) {
			__xuiStyleValueReset(&tValue);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pToken = (xui_style_token_t*)xrtCalloc(1, sizeof(*pToken));
		if ( pToken == NULL ) {
			xrtFree(sCopy);
			__xuiStyleValueReset(&tValue);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pToken->sName = sCopy;
		pToken->pNext = pContext->pStyleTokens;
		pContext->pStyleTokens = pToken;
	} else {
		__xuiStyleValueReset(&pToken->tValue);
	}
	pToken->tValue = tValue;
	pContext->iTokenGeneration++;
	if ( pContext->iTokenGeneration == 0 ) {
		pContext->iTokenGeneration = 1;
	}
	return __xuiStyleCommitChange(pContext);
}

XUI_API int xuiStyleRemoveToken(xui_context pContext, const char* sName)
{
	xui_style_token_t* pToken;
	xui_style_token_t* pPrev;

	if ( !xuiInternalContextIsValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPrev = NULL;
	for ( pToken = pContext->pStyleTokens; pToken != NULL; pToken = pToken->pNext ) {
		if ( __xuiStringEqual(pToken->sName, sName) ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pToken->pNext;
			} else {
				pContext->pStyleTokens = pToken->pNext;
			}
			xrtFree(pToken->sName);
			__xuiStyleValueReset(&pToken->tValue);
			xrtFree(pToken);
			pContext->iTokenGeneration++;
			if ( pContext->iTokenGeneration == 0 ) {
				pContext->iTokenGeneration = 1;
			}
			return __xuiStyleCommitChange(pContext);
		}
		pPrev = pToken;
	}
	return XUI_OK;
}

XUI_API int xuiStyleGetToken(xui_context pContext, const char* sName, xui_style_value_t* pValue)
{
	xui_style_token_t* pToken;

	if ( !xuiInternalContextIsValid(pContext) || (sName == NULL) || (sName[0] == '\0') || (pValue == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToken = __xuiStyleFindToken(pContext, sName);
	if ( pToken == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	*pValue = pToken->tValue;
	pValue->iSize = sizeof(*pValue);
	return XUI_OK;
}

XUI_API uint32_t xuiStyleGetTokenGeneration(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->iTokenGeneration : 0;
}

static int __xuiWidgetStablePartSlot(xui_widget owner,xui_widget* slot)
{
    if(!owner || !owner->pTypeData || !owner->pType || !slot)return 0;
    size_t size=owner->pType->iTypeDataSize;
    uintptr_t first=(uintptr_t)owner->pTypeData,address=(uintptr_t)slot;
    return size>=sizeof(*slot) && address>=first && address-first<=size-sizeof(*slot) &&
        address%_Alignof(xui_widget)==0;
}
static void __xuiWidgetClearPartSlot(xui_widget part)
{
    xui_widget* slot=part->ppInternalOwnerSlot;
    part->ppInternalOwnerSlot=NULL;
    if(slot && *slot==part)*slot=NULL;
}
int xuiInternalWidgetBindPartSlot(xui_widget owner,xui_widget part,xui_widget* slot)
{
    if(!__xuiWidgetValid(owner) || !__xuiWidgetValid(part) || part->pInternalOwner!=owner ||
       !__xuiWidgetStablePartSlot(owner,slot) || *slot!=part)return XUI_ERROR_INVALID_ARGUMENT;
    if(part->ppInternalOwnerSlot && part->ppInternalOwnerSlot!=slot)return XUI_ERROR_INVALID_STATE;
    part->ppInternalOwnerSlot=slot;return XUI_OK;
}

static int __xuiWidgetCreateInternal(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData, const void* pNativeOwner)
{
	xui_widget pWidget;
	xui_widget_type pOwner;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	if ( pContext->bDestroying || pContext->bDestroyPending || pContext->bCloseRequested ) return XUI_ERROR_INVALID_STATE;
	if ( pType == NULL ) {
		pType = &g_xuiWidgetBaseType;
	}
	if ( !__xuiWidgetTypeBelongsToContext(pContext, pType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pWidget = (xui_widget)xrtMalloc(sizeof(*pWidget));
	if ( pWidget == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pWidget, 0, sizeof(*pWidget));
	pWidget->iMagic = XUI_WIDGET_MAGIC;
	pWidget->pContext = pContext;
	pWidget->pType = pType;
	pWidget->bSubtreePrepareFrameDependent = __xuiWidgetTypePrepareFrameDependent(pType);

    if(pNativeOwner && __xuiWidgetStablePartSlot((xui_widget)pNativeOwner,ppWidget))pWidget->ppInternalOwnerSlot=ppWidget;
	__xuiWidgetInitStackingChildren(pWidget);
	__xuiWidgetInitLayout(pWidget);
	pWidget->bVisible = 1;
	pWidget->bEnabled = 1;
	pWidget->bHitTestVisible = 1;
	pWidget->bTabStop = 1;
	pWidget->iImeMode = XUI_IME_DISABLED;
	pWidget->iDirtyFlags = XUI_WIDGET_DIRTY_ALL;
	pWidget->iSubtreeDirtyFlags = XUI_WIDGET_DIRTY_ALL;
	pWidget->iGeneration = 1;
	pWidget->iDpiGeneration = pContext->iDpiGeneration;
	pWidget->iResolvedStyleHash = __xuiStyleHashProps(NULL, 0);
	pWidget->iResolvedStyleGeneration = 0;
	pWidget->tCachePolicy.iSize = sizeof(pWidget->tCachePolicy);
	pWidget->tCachePolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	pWidget->tCachePolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pWidget->tCachePolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	__xuiTooltipDefaultDesc(&pWidget->tTooltip);
	if ( pType->iTypeDataSize > 0 ) {
		pWidget->pTypeData = xrtCalloc(1, pType->iTypeDataSize);
		if ( pWidget->pTypeData == NULL ) {
			pWidget->iMagic = 0;
			xrtFree(pWidget);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	pOwner = __xuiWidgetTypeDefaultLayoutOwner(pType);
	if ( pOwner != NULL ) {
		pWidget->tLayout = pOwner->tLayout;
	}
	pOwner = __xuiWidgetTypeDefaultCacheOwner(pType);
	if ( pOwner != NULL ) {
		pWidget->tCachePolicy = pOwner->tCachePolicy;
		if ( pWidget->tCachePolicy.iSize == 0 ) {
			pWidget->tCachePolicy.iSize = sizeof(pWidget->tCachePolicy);
		}
	}
	pWidget->onContentMeasure = __xuiWidgetTypeContentMeasure(pType);
	pWidget->pContentMeasureUser = pWidget->pTypeData;
	pWidget->onLayoutPrepare = __xuiWidgetTypeLayoutPrepare(pType);
	pWidget->pLayoutPrepareUser = pWidget->pTypeData;
	pWidget->onLayoutChildren = __xuiWidgetTypeLayoutChildren(pType);
	pWidget->pLayoutChildrenUser = pWidget->pTypeData;
	pWidget->onLayoutComplete = __xuiWidgetTypeLayoutComplete(pType);
	pWidget->pLayoutCompleteUser = pWidget->pTypeData;
	pWidget->onCacheRender = __xuiWidgetTypeCacheRender(pType);
	pWidget->pCacheRenderUser = pWidget->pTypeData;
	pWidget->onUpdate = __xuiWidgetTypeUpdate(pType);
	pWidget->pUpdateUser = pWidget->pTypeData;
	pWidget->bSubtreeHasUpdate = pWidget->onUpdate != NULL;
	pWidget->bSubtreeHasInactiveUpdate = pWidget->onUpdate != NULL &&
		(pType->iFlags & XUI_WIDGET_TYPE_UPDATE_ON_INACTIVE) != 0;
	pWidget->onQueryCursor = __xuiWidgetTypeQueryCursor(pType);
	pWidget->pQueryCursorUser = pWidget->pTypeData;
	iRet = xuiInternalLayoutCreateWidget(pWidget);
	if ( iRet != XUI_OK ) {
		if ( pWidget->pTypeData != NULL ) {
			xrtFree(pWidget->pTypeData);
		}
		pWidget->iMagic = 0;
		xrtFree(pWidget);
		return iRet;
	}
	/* Hold the type and context registration throughout arbitrary init hooks.
	 * A hook may request widget/context destruction; the outer operation owns
	 * the physical lifetime until it returns. */
	pType->iWidgetCount++;
	pWidget->pContextNext = pContext->pAllWidgets;
	if ( pContext->pAllWidgets != NULL ) pContext->pAllWidgets->pContextPrev = pWidget;
	pContext->pAllWidgets = pWidget;
    if(pNativeOwner)__xuiWidgetLinkPart((xui_widget)pNativeOwner,pWidget);
	iRet = __xuiWidgetTypeInitChain(pWidget, pType, pCreateData);
	if ( iRet != XUI_OK ) {
        /* Completed parent stages remain on this real Context-owned carrier.
         * A busy cache cannot be discarded by immediate constructor rollback. */
		(void)xuiInternalWidgetRequestDestroy(pWidget,pNativeOwner);
		return iRet;
	}
    /* Init setters only borrow. Admit the final fields exactly once after the
     * full inheritance chain succeeds; partial failure rolls back held slots. */
    iRet=pContext->pOwnedProxy && !xuiInternalWidgetIsValid(pWidget) ? XUI_ERROR_INVALID_STATE:__xuiTypeResourceAdmit(pWidget,pType);
    if(iRet!=XUI_OK){
        xerror* first=xrtTakeError();
        (void)xuiInternalWidgetRequestDestroy(pWidget,pNativeOwner);xrtSetErrorTake(first);return iRet;
    }
	pWidget->bTypeInitialized = 1;
	*ppWidget = pWidget;
	return XUI_OK;
}

static void __xuiCreatedWidgetGone(void* value)
{*(xui_widget*)value=NULL;}
typedef struct xui_created_owner_output_t {xui_widget owner; xui_widget* output;} xui_created_owner_output_t;
static void __xuiCreatedOwnerGone(void* value)
{
    xui_created_owner_output_t* state=value;
    /* A native part slot may live in this owner's type data. Clear it during
     * logical invalidation, while that storage still exists. */
    state->owner=NULL;*state->output=NULL;
}

/* Constructor callbacks see only this operation's diagnostics. Keep a weak
 * output through End, which may run deferred cleanup and destroy that output. */
static int __xuiWidgetCreateScoped(xui_context context,xui_widget_type type,
    xui_widget* output,const void* data,const void* owner)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(context,&scope);
    if(status!=XUI_OK)return status;
    xui_lifetime_link_t ownerLife={0};
    xui_created_owner_output_t ownerOutput={(xui_widget)owner,output};
    if(owner)status=xuiWidgetLifetimeWatch((xui_widget)owner,&ownerLife,__xuiCreatedOwnerGone,&ownerOutput);
    if(status==XUI_OK)status=__xuiWidgetCreateInternal(context,type,output,data,owner);
    xui_widget candidate=status==XUI_OK ? *output:NULL;
    xui_lifetime_link_t life={0};
    if(status==XUI_OK && (!__xuiWidgetValid(candidate) ||
        (owner && !__xuiWidgetValid((xui_widget)owner)) || context->bCloseRequested ||
        context->bDestroyPending || context->bDestroying))status=owner ? XUI_ERROR_INVALID_STATE:XUI_ERROR;
    if(candidate){
        int watched=xuiWidgetLifetimeWatch(candidate,&life,__xuiCreatedWidgetGone,&candidate);
        if(status==XUI_OK && watched!=XUI_OK)status=watched;
        if(watched!=XUI_OK){
            /* A closing Context can refuse the weak watch. Retire while the
             * construction scope still pins memory, then forget the pointer. */
            xerror* before=xrtTakeError();
            (void)xuiInternalWidgetRequestDestroy(candidate,owner);candidate=NULL;
            xrtErrorFree(xrtTakeError());xrtSetErrorTake(before);
        }
    }
    if(status!=XUI_OK)*output=NULL;
    xerror* first=xrtTakeError();
    status=xuiInternalCallScopeEndOutcome(&scope,status,&first);
    if(status==XUI_OK && !candidate){
        /* A deferred close is a new integer-only failure, not the entry error. */
        xrtErrorFree(first);first=NULL;status=XUI_ERROR_INVALID_STATE;
    }
    if(status!=XUI_OK && candidate){
        (void)xuiInternalWidgetRequestDestroy(candidate,owner);
        xrtErrorFree(xrtTakeError()); /* cleanup cannot replace the first failure */
    }
    xuiLifetimeUnwatch(&life);
    if(!owner || ownerOutput.owner)*output=status==XUI_OK ? candidate:NULL;
    xuiLifetimeUnwatch(&ownerLife);
    xrtSetErrorTake(first);return status;
}

XUI_API int xuiWidgetCreate(xui_context pContext, xui_widget* ppWidget)
{return __xuiWidgetCreateScoped(pContext,&g_xuiWidgetBaseType,ppWidget,NULL,NULL);}

XUI_API int xuiWidgetCreateTyped(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData)
{return __xuiWidgetCreateScoped(pContext,pType,ppWidget,pCreateData,NULL);}

/* Construction identity is installed before any native/type init callback. */
int xuiInternalWidgetCreatePart(xui_widget owner,xui_widget_type type,xui_widget* output,const void* data)
{
    if(output)*output=NULL;
    if(!__xuiWidgetValid(owner) || !output)return XUI_ERROR_INVALID_ARGUMENT;
    return __xuiWidgetCreateScoped(owner->pContext,type,output,data,owner);
}

static int __xuiDragAdornerRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_rect_t tRect;
	int i;
	int iRet;

	(void)pWidget;
	(void)iStateId;
	pContext = (xui_context)pUser;
	if ( !xuiInternalContextIsValid(pContext) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	for ( i = 0; i < pContext->iDragAdornerPrimitiveCount; i++ ) {
		const xui_drag_adorner_primitive_t* pPrimitive = &pContext->arrDragAdornerPrimitives[i];
		tRect = pPrimitive->tRect;
		tRect.fX -= pContext->tDragAdornerBounds.fX;
		tRect.fY -= pContext->tDragAdornerBounds.fY;
		if ( pPrimitive->iType == XUI_DRAG_ADORNER_RECT_FILL ) {
			if ( pProxy->drawRectFill == NULL ) continue;
			iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, pPrimitive->iColor);
		} else if ( pPrimitive->iType == XUI_DRAG_ADORNER_RECT_STROKE ) {
			if ( pProxy->drawRectStroke == NULL ) continue;
			iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect,
				(pPrimitive->fWidth > 0.0f) ? pPrimitive->fWidth : 1.0f, pPrimitive->iColor);
		} else {
			continue;
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiDragAdornerEnsure(xui_context pContext)
{
	xui_cache_policy_t tPolicy;
	xui_widget pWidget;
	int iRet;

	if ( pContext->pDragAdornerWidget != NULL ) return XUI_OK;
	iRet = xuiWidgetCreate(pContext, &pWidget);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCacheRenderCallback(pWidget, __xuiDragAdornerRender, pContext);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEnabled(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetHitTestVisible(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiOverlayAttach(pContext, NULL, pWidget, XUI_LAYER_DRAG, 100);
	if ( iRet != XUI_OK ) {
		int cleanup=__xuiWidgetDestroyInMode(pWidget,xuiInternalDestroyModeInherited(pContext));
        if(cleanup!=XUI_OK && !xrtGetError())xrtSetErrorInfo(XERR_STATE,"xui",cleanup,"internal widget is owned by its composite");
		return iRet;
	}
	pContext->pDragAdornerWidget = pWidget;
	return XUI_OK;
}

int xuiInternalDragAdornerSet(xui_context pContext, xui_widget pOwner,
	const xui_drag_adorner_primitive_t* pPrimitives, int iPrimitiveCount)
{
	xui_rect_t tBounds;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fPad;
	int i;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !xuiInternalWidgetIsValid(pOwner) ||
	     (xuiWidgetGetContext(pOwner) != pContext) || (pPrimitives == NULL) ||
	     (iPrimitiveCount <= 0) || (iPrimitiveCount > XUI_DRAG_ADORNER_MAX_PRIMITIVES) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiDragAdornerEnsure(pContext);
	if ( iRet != XUI_OK ) return iRet;
	fLeft = pPrimitives[0].tRect.fX;
	fTop = pPrimitives[0].tRect.fY;
	fRight = fLeft + pPrimitives[0].tRect.fW;
	fBottom = fTop + pPrimitives[0].tRect.fH;
	for ( i = 0; i < iPrimitiveCount; i++ ) {
		const xui_drag_adorner_primitive_t* pPrimitive = &pPrimitives[i];
		fPad = (pPrimitive->iType == XUI_DRAG_ADORNER_RECT_STROKE) ?
			(((pPrimitive->fWidth > 0.0f) ? pPrimitive->fWidth : 1.0f) + 2.0f) : 1.0f;
		if ( pPrimitive->tRect.fX - fPad < fLeft ) fLeft = pPrimitive->tRect.fX - fPad;
		if ( pPrimitive->tRect.fY - fPad < fTop ) fTop = pPrimitive->tRect.fY - fPad;
		if ( pPrimitive->tRect.fX + pPrimitive->tRect.fW + fPad > fRight ) fRight = pPrimitive->tRect.fX + pPrimitive->tRect.fW + fPad;
		if ( pPrimitive->tRect.fY + pPrimitive->tRect.fH + fPad > fBottom ) fBottom = pPrimitive->tRect.fY + pPrimitive->tRect.fH + fPad;
	}
	tBounds = xuiInternalSnapRect((xui_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop});
	memcpy(pContext->arrDragAdornerPrimitives, pPrimitives,
		(size_t)iPrimitiveCount * sizeof(pContext->arrDragAdornerPrimitives[0]));
	pContext->iDragAdornerPrimitiveCount = iPrimitiveCount;
	pContext->pDragAdornerOwner = pOwner;
	pContext->pDragAdornerWidget->pOverlayOwner = pOwner;
	pContext->tDragAdornerBounds = tBounds;
	iRet = xuiWidgetSetRect(pContext->pDragAdornerWidget, tBounds);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pContext->pDragAdornerWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiOverlayBringToFront(pContext->pDragAdornerWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetInvalidate(pContext->pDragAdornerWidget,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
	return iRet;
}

int xuiInternalDragAdornerSetColor(xui_context pContext, xui_widget pOwner, int iIndex, uint32_t iColor)
{
	if ( !xuiInternalContextIsValid(pContext) || !__xuiWidgetValid(pOwner) ||
		pContext->pDragAdornerOwner != pOwner || !__xuiWidgetValid(pContext->pDragAdornerWidget) ||
		iIndex < 0 || iIndex >= pContext->iDragAdornerPrimitiveCount ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pContext->arrDragAdornerPrimitives[iIndex].iColor == iColor ) return XUI_OK;
	pContext->arrDragAdornerPrimitives[iIndex].iColor = iColor;
	return xuiWidgetInvalidate(pContext->pDragAdornerWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

void xuiInternalDragAdornerHide(xui_context pContext, xui_widget pOwner)
{
	if ( !xuiInternalContextIsValid(pContext) ) return;
	if ( (pOwner != NULL) && (pContext->pDragAdornerOwner != pOwner) ) return;
	pContext->pDragAdornerOwner = NULL;
	pContext->iDragAdornerPrimitiveCount = 0;
	memset(&pContext->tDragAdornerBounds, 0, sizeof(pContext->tDragAdornerBounds));
	if ( pContext->pDragAdornerWidget != NULL ) {
		pContext->pDragAdornerWidget->pOverlayOwner = NULL;
		(void)xuiWidgetSetVisible(pContext->pDragAdornerWidget, 0);
	}
}

static void __xuiWidgetMarkDestroyPending(xui_widget pWidget)
{
	xui_widget pChild;

	if ( !__xuiWidgetMemoryValid(pWidget) || pWidget->bDestroyPending ) return;
    __xuiWidgetClearPartSlot(pWidget);
	pWidget->bDestroyPending = 1;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetMarkDestroyPending(pChild);
	}
    for(pChild=pWidget->pFirstOwnedPart;pChild;pChild=pChild->pOwnedPartNext)__xuiWidgetMarkDestroyPending(pChild);
	return;
}

static void __xuiWidgetInvalidateLifetimesVisit(xui_widget widget,uint32_t epoch)
{
    if(__xuiWidgetVisitSeen(widget,epoch))return;
    __xuiWidgetVisitMark(widget,epoch);
    xuiInternalLifetimeInvalidateAll(&widget->pLifetimeLinks);
    for(xui_widget child=widget->pFirstChild;child;child=child->pNextSibling)__xuiWidgetInvalidateLifetimesVisit(child,epoch);
    for(xui_widget part=widget->pFirstOwnedPart;part;part=part->pOwnedPartNext)__xuiWidgetInvalidateLifetimesVisit(part,epoch);
}
static void __xuiWidgetInvalidateLifetimes(xui_widget widget)
{__xuiWidgetInvalidateLifetimesVisit(widget,__xuiWidgetVisitBegin(widget->pContext));}

static void __xuiWidgetDestroyQueue(xui_context pContext, xui_widget pWidget)
{
	pWidget->pDeferredDestroyNext = pContext->pDeferredDestroyHead;
	pContext->pDeferredDestroyHead = pWidget;
}

static void __xuiWidgetDestroyNow(xui_widget pWidget, bool bVisualDetached)
{
	xui_widget pChild;

    if(!__xuiWidgetMemoryValid(pWidget) || (pWidget->iDestroyVisitEpoch&XUI_DESTROY_EXECUTING))return;
    /* The exclusive native owner must stay alive until all its parts have
     * retired. A visual traversal entering a part first promotes to that owner.
     * The execution bit breaks a legitimate mixed visual/owner cycle without
     * reading freed records or duplicating any destructor. */
    xui_widget nativeOwner=(xui_widget)pWidget->pInternalOwner;
    if(nativeOwner && nativeOwner->bDestroyPending &&
       (nativeOwner->iDestroyVisitEpoch&XUI_DESTROY_MEMBER) &&
       !(nativeOwner->iDestroyVisitEpoch&XUI_DESTROY_EXECUTING)){
        __xuiWidgetDestroyNow(nativeOwner, false);return;
    }
    pWidget->iDestroyVisitEpoch|=XUI_DESTROY_EXECUTING;
	if ( !pWidget->bDestroyPending ) {
		__xuiWidgetMarkDestroyPending(pWidget);
		__xuiWidgetInvalidateLifetimes(pWidget);
	}
	/* Native-owned parts can live outside their owner's visual subtree. The
	 * owner's logical detach cannot clear interaction records for those parts.
	 * Retire each part's context references while its visual ancestry is still
	 * intact, before unlinking or freeing anything. Pending widgets do not
	 * dispatch new cancellation callbacks from ContextDetachWidget. Visual
	 * children share their parent's sweep; only a separate native-owned visual
	 * subtree needs another sweep. */
	if (!bVisualDetached) xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
	/* Disarm the exact owner slot before parent TypeData can be released. */
    __xuiWidgetClearPartSlot(pWidget);
    __xuiWidgetUnlinkPart(pWidget);
    xui_widget visualParent=pWidget->pParent;
	__xuiWidgetUnlinkRenderNodeOnDestroy(pWidget);
    if(visualParent){
        xuiInternalLayoutDetach(pWidget);
        if(pWidget->pPrevSibling)pWidget->pPrevSibling->pNextSibling=pWidget->pNextSibling;
        else visualParent->pFirstChild=pWidget->pNextSibling;
        if(pWidget->pNextSibling)pWidget->pNextSibling->pPrevSibling=pWidget->pPrevSibling;
        else visualParent->pLastChild=pWidget->pPrevSibling;
        if(visualParent->iChildCount>0)--visualParent->iChildCount;
        __xuiWidgetStackingDirty(visualParent);
        pWidget->pParent=NULL;pWidget->pPrevSibling=NULL;pWidget->pNextSibling=NULL;
		__xuiWidgetUpdateEventMasksToRoot(visualParent);
		__xuiWidgetRepairDirtyToRoot(visualParent);
		__xuiWidgetRepairPrepareFrameDependentToRoot(visualParent);
		__xuiWidgetRepairUpdateStateToRoot(visualParent);
		__xuiWidgetMarkDirtyOnly(visualParent,
			XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
    }
	/* Detach before user destructors can request more context cleanup. */
	xui_context pContext = pWidget->pContext;
	if ( pWidget->pContextPrev != NULL ) pWidget->pContextPrev->pContextNext = pWidget->pContextNext;
	else if ( pContext != NULL && pContext->pAllWidgets == pWidget ) pContext->pAllWidgets = pWidget->pContextNext;
	if ( pWidget->pContextNext != NULL ) pWidget->pContextNext->pContextPrev = pWidget->pContextPrev;
	pWidget->pContextPrev = pWidget->pContextNext = NULL;
	xuiInternalNativeAccessibilityResetWidget(pWidget);
	__xuiWidgetStateUnqueue(pWidget);
	if ( __xuiWidgetMemoryValid(pWidget->pEditDelegate) &&
	     pWidget->pEditDelegate->pEditOwner == pWidget ) {
		pWidget->pEditDelegate->pEditOwner = NULL;
	}
	if ( __xuiWidgetMemoryValid(pWidget->pEditOwner) &&
	     pWidget->pEditOwner->pEditDelegate == pWidget ) {
		pWidget->pEditOwner->pEditDelegate = NULL;
	}
	pWidget->pEditDelegate = NULL;
	pWidget->pEditOwner = NULL;
	while ( (pChild = pWidget->pFirstChild) != NULL ) {
		pWidget->pFirstChild = pChild->pNextSibling;
		if ( pWidget->pFirstChild != NULL ) pWidget->pFirstChild->pPrevSibling = NULL;
		else pWidget->pLastChild = NULL;
		if ( pWidget->iChildCount > 0 ) pWidget->iChildCount--;
		pChild->pParent = NULL;
		pChild->pPrevSibling = NULL;
		pChild->pNextSibling = NULL;
		__xuiWidgetDestroyNow(pChild, true);
	}
    /* All native-owned parts are physically retired while their owner's data
     * and callback targets are still alive, even outside its visual subtree. */
    while(pWidget->pFirstOwnedPart)__xuiWidgetDestroyNow(pWidget->pFirstOwnedPart, false);
	__xuiWidgetEndActiveUpdate(pWidget);
    if(pWidget->pInitializedType)__xuiWidgetTypeDestroyChain(pWidget,pWidget->pInitializedType);
	/* Empty the physical slots before arbitrary consuming cleanup reenters. */
	xui_event_owners_t* pOwners = pWidget->pEventOwners;
	pWidget->pEventOwners = NULL;
	pWidget->onEvent = NULL; pWidget->pEventUser = NULL;
	memset(pWidget->arrEventHandlers, 0, sizeof(pWidget->arrEventHandlers));
	memset(pWidget->arrEventUsers, 0, sizeof(pWidget->arrEventUsers));
	if ( pOwners != NULL ) {
        while(pOwners->pBindings){
            xui_callback_binding_t* binding=pOwners->pBindings;
            pOwners->pBindings=binding->pNext;
            xui_callback_owner_t owner=binding->tOwner;xrtFree(binding);
            (void)xuiInternalCallbackOwnerDrop(pContext,&owner);
        }
		(void)xuiInternalCallbackOwnerDrop(pContext, &pOwners->tPrimary);
		if ( pOwners->pHandlers != NULL ) {
			for ( int i = 0; i < XUI_WIDGET_EVENT_SLOT_COUNT; ++i )
				(void)xuiInternalCallbackOwnerDrop(pContext, &pOwners->pHandlers[i]);
			xrtFree(pOwners->pHandlers);
		}
		xrtFree(pOwners);
	}
	__xuiWidgetDestroyAllCaches(pWidget);
	__xuiWidgetDestroyTableTracks(pWidget);
	__xuiWidgetDestroyStyleData(pWidget);
	__xuiWidgetDestroyTooltipData(pWidget);
	__xuiWidgetDestroyAccessibilityData(pWidget);
	__xuiWidgetDestroyStackingChildren(pWidget);
	if ( pWidget->pTypeData != NULL ) {
		xrtFree(pWidget->pTypeData);
	}
	if ( __xuiWidgetTypeValid(pWidget->pType) && (pWidget->pType->iWidgetCount > 0) ) {
		pWidget->pType->iWidgetCount--;
	}
	xuiInternalLayoutDestroyWidget(pWidget);
	pWidget->iMagic = 0;
	xrtFree(pWidget);
}

void xuiInternalContextDestroyWidgets(xui_context pContext)
{
	/* Roots have already been destroyed. Destroying a remaining instance also
	 * removes any descendants from this registry; no parallel tree is built. */
	while(pContext->pAllWidgets!=NULL){xui_widget widget=pContext->pAllWidgets;(void)xuiInternalWidgetRequestDestroy(widget,widget->pInternalOwner);}
}

static void __xuiWidgetMarkClosure(xui_widget widget,uint32_t epoch)
{
    if(__xuiWidgetVisitSeen(widget,epoch))return;
    __xuiWidgetVisitMark(widget,epoch);
    widget->iDestroyVisitEpoch|=XUI_DESTROY_MEMBER;
    for(xui_widget child=widget->pFirstChild;child;child=child->pNextSibling)__xuiWidgetMarkClosure(child,epoch);
    for(xui_widget part=widget->pFirstOwnedPart;part;part=part->pOwnedPartNext)__xuiWidgetMarkClosure(part,epoch);
}
bool xuiInternalWidgetDestroyFlush(xui_context context)
{
    if(!xuiInternalContextIsValid(context) || context->iDestroyFlushDepth>0 ||
       context->iWidgetCallbackDepth>0 || context->iOperationDepth>0)return false;
    ++context->iDestroyFlushDepth;
    bool progressed=false;
    xui_widget candidates=context->pDeferredDestroyHead;
    context->pDeferredDestroyHead=NULL;
    while(candidates){
        xui_widget widget=candidates;candidates=widget->pDeferredDestroyNext;widget->pDeferredDestroyNext=NULL;
        if(!__xuiWidgetMemoryValid(widget) || !widget->bDestroyPending)continue;
        /* An independently queued part can stay pending without delaying its
         * native owner; the owner retries the complete closure below. */
        bool complete=false;
        int status=__xuiWidgetDestroyPrepareClosure(widget,&complete);
        if(status<0)xuiInternalContextRecordFailure(context,status);
        if(!complete){__xuiWidgetDestroyQueue(context,widget);continue;}
        uint32_t epoch=__xuiWidgetVisitBegin(context);__xuiWidgetMarkClosure(widget,epoch);
        xui_widget* lists[2]={&candidates,&context->pDeferredDestroyHead};
        for(size_t i=0;i<2;++i){
            for(xui_widget* link=lists[i];*link;){
                xui_widget pending=*link;
                if(__xuiWidgetVisitSeen(pending,epoch)){*link=pending->pDeferredDestroyNext;pending->pDeferredDestroyNext=NULL;}
                else link=&pending->pDeferredDestroyNext;
            }
        }
        __xuiWidgetDestroyNow(widget, false);progressed=true;
    }
    --context->iDestroyFlushDepth;return progressed;
}

int xuiInternalWidgetRequestDestroy(xui_widget pWidget,const void* pAuthority)
{
	xui_context pContext;
	int bDestroying;

	if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
    if(pWidget->pInternalOwner && pWidget->pInternalOwner!=pAuthority)return XUI_ERROR_INVALID_STATE;
	pContext = pWidget->pContext;
	if ( !xuiInternalContextIsValid(pContext) ) {
		__xuiWidgetDestroyNow(pWidget, false);
		return XUI_OK;
	}
	xuiInternalOperationEnter(pContext);
    __xuiWidgetClearPartSlot(pWidget);
	if ( pWidget->pParent != NULL ) {
		(void)__xuiWidgetRemoveFromParentOperation(pWidget,pAuthority);
	} else if ( pContext->pRoot == pWidget ) {
		xuiInternalContextDetachWidget(pContext, pWidget);
		pContext->pRoot = NULL;
		(void)xuiInternalContextInvalidateAll(pContext);
	} else if ( pContext->pOverlayRoot == pWidget ) {
		xuiInternalContextDetachWidget(pContext, pWidget);
		pContext->pOverlayRoot = NULL;
		(void)xuiInternalContextInvalidateAll(pContext);
	} else {
		xuiInternalContextDetachWidget(pContext, pWidget);
	}
	if ( __xuiWidgetValid(pWidget) ) {
		__xuiWidgetMarkDestroyPending(pWidget);
		__xuiWidgetInvalidateLifetimes(pWidget);
		__xuiWidgetDestroyQueue(pContext, pWidget);
	}
	bDestroying = pContext->bDestroying;
	xuiInternalOperationLeave(pContext);
	/* When already destroying, Leave never frees the context. In the normal
	 * case it may do so; no access to pContext follows that path. */
	if ( bDestroying ) xuiInternalWidgetDestroyFlush(pContext);
    return XUI_OK;
}
static int __xuiWidgetDestroy(xui_widget widget)
{
    if(!__xuiWidgetValid(widget))return XUI_OK;
    if(widget->pInternalOwner)return XUI_ERROR_INVALID_STATE;
    xui_context context=widget->pContext;
    if(!xuiInternalContextIsValid(context) || context->bDestroying)return xuiInternalWidgetRequestDestroy(widget,NULL);
    xui_call_scope_t scope={0};int result=xuiCallScopeBegin(context,&scope);if(result!=XUI_OK)return result;
    result=xuiInternalWidgetRequestDestroy(widget,NULL);
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    result=xuiInternalCallScopeEndOutcome(&scope,result,&first);
    xrtSetErrorTake(first);return result;
}
static int __xuiWidgetDestroyInMode(xui_widget widget,xui_internal_destroy_mode_t mode)
{
    if(!__xuiWidgetValid(widget))return XUI_OK;
    xui_internal_destroy_scope_t scope={0};xuiInternalDestroyScopeBegin(widget->pContext,mode,&scope);
    int result=__xuiWidgetDestroy(widget);
    xuiInternalDestroyScopeEnd(&scope);return result;
}
XUI_API int xuiWidgetDestroyChecked(xui_widget widget)
{return __xuiWidgetDestroyInMode(widget,XUI_DESTROY_NONBLOCKING);}
XUI_API void xuiWidgetDestroy(xui_widget widget)
{
    int result=__xuiWidgetDestroyInMode(widget,XUI_DESTROY_SYNC_WORKER);
    if(result!=XUI_OK && !xrtGetError())xrtSetErrorInfo(XERR_STATE,"xui",result,"internal widget is owned by its composite");
}
int xuiInternalWidgetClaimPart(xui_widget owner,xui_widget part)
{
    if(!__xuiWidgetValid(owner) || !__xuiWidgetValid(part) || owner==part || owner->pContext!=part->pContext)return XUI_ERROR_INVALID_ARGUMENT;
    if(part->pInternalOwner)return part->pInternalOwner==owner ? XUI_OK:XUI_ERROR_INVALID_STATE;
    if(__xuiWidgetNativeOwnerContains(owner,part))return XUI_ERROR_INVALID_STATE;
    __xuiWidgetLinkPart(owner,part);return XUI_OK;
}
int xuiInternalWidgetDestroyPart(xui_widget owner,xui_widget part)
{
    if(!part)return XUI_OK;
    if(!__xuiWidgetMemoryValid(owner) || !__xuiWidgetMemoryValid(part) ||
       owner->pContext!=part->pContext || part->pInternalOwner!=owner)return XUI_ERROR_INVALID_STATE;
    return xuiInternalWidgetRequestDestroy(part,owner);
}

XUI_API xui_context xuiWidgetGetContext(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return NULL;
	}
	return pWidget->pContext;
}

XUI_API xui_widget_type xuiWidgetGetType(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return NULL;
	}
	return pWidget->pType;
}

XUI_API int xuiWidgetIsType(xui_widget pWidget, xui_widget_type pType)
{
	xui_widget_type pScan;

	if ( !__xuiWidgetValid(pWidget) || !__xuiWidgetTypeValid(pType) ) {
		return 0;
	}
	for ( pScan = pWidget->pType; __xuiWidgetTypeValid(pScan); pScan = pScan->pParent ) {
		if ( pScan == pType ) {
			return 1;
		}
	}
	return 0;
}

XUI_API void* xuiWidgetGetTypeData(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return NULL;
	}
	return pWidget->pTypeData;
}

XUI_API void xuiWidgetSetUserData(xui_widget pWidget, void* pUser)
{
	if ( __xuiWidgetValid(pWidget) ) {
		pWidget->pUser = pUser;
	}
}

XUI_API void* xuiWidgetGetUserData(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return NULL;
	}
	return pWidget->pUser;
}

XUI_API int xuiWidgetAddChild(xui_widget pParent, xui_widget pChild)
{
	return xuiWidgetInsertBefore(pParent, pChild, NULL);
}

static int __xuiWidgetInsertBeforeOperation(xui_widget pParent, xui_widget pChild, xui_widget pBefore, const void* pAuthority)
{
	int iRet;

	if ( !__xuiWidgetCanAttachChild(pParent, pChild,pAuthority) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pBefore != NULL ) {
		if ( !__xuiWidgetValid(pBefore) || (pBefore->pParent != pParent) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = xuiInternalLayoutAttach(pParent, pChild, pBefore);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalStateRecordBoundsTree(pChild);
	if ( pBefore != NULL ) {
		pChild->pNextSibling = pBefore;
		pChild->pPrevSibling = pBefore->pPrevSibling;
		if ( pBefore->pPrevSibling != NULL ) {
			pBefore->pPrevSibling->pNextSibling = pChild;
		} else {
			pParent->pFirstChild = pChild;
		}
		pBefore->pPrevSibling = pChild;
	} else {
		pChild->pPrevSibling = pParent->pLastChild;
		if ( pParent->pLastChild != NULL ) {
			pParent->pLastChild->pNextSibling = pChild;
		} else {
			pParent->pFirstChild = pChild;
		}
		pParent->pLastChild = pChild;
	}
	pChild->pParent = pParent;
	pParent->iChildCount++;
	if ( pChild->bSubtreePrepareFrameDependent ) {
		xui_widget pScan;
		for ( pScan = pParent; pScan != NULL; pScan = pScan->pParent ) {
			if ( pScan->bSubtreePrepareFrameDependent ) break;
			pScan->bSubtreePrepareFrameDependent = 1;
		}
	}
	if ( pChild->bSubtreeHasUpdate || pChild->bSubtreeHasInactiveUpdate ) {
		xui_widget pScan;
		for ( pScan = pParent; pScan != NULL; pScan = pScan->pParent ) {
			int bOldUpdate = pScan->bSubtreeHasUpdate;
			int bOldInactive = pScan->bSubtreeHasInactiveUpdate;
			pScan->bSubtreeHasUpdate |= pChild->bSubtreeHasUpdate;
			pScan->bSubtreeHasInactiveUpdate |= pChild->bSubtreeHasInactiveUpdate;
			if ( pScan->bSubtreeHasUpdate == bOldUpdate &&
				pScan->bSubtreeHasInactiveUpdate == bOldInactive ) break;
		}
	}
#if XUI_ENABLE_INCREMENTAL_VERIFY
	__xuiWidgetVerifyUpdateState(pParent);
#endif
	__xuiWidgetStackingDirty(pParent);
	__xuiWidgetBumpLayoutVersion(pParent);
	__xuiWidgetPropagateEventBitsToRoot(pParent, pChild->iSubtreeEventMask);
	if ( pChild->iSubtreeDirtyFlags != 0 ) {
		xui_widget pScan;
		uint32_t iBits = pChild->iSubtreeDirtyFlags;
		for ( pScan = pParent; pScan != NULL; pScan = pScan->pParent ) {
			uint32_t iOldFlags = pScan->iSubtreeDirtyFlags;
			pScan->iSubtreeDirtyFlags |= iBits;
			pScan->pContext->iDirtyPropagationTotal++;
			if ( pScan->iSubtreeDirtyFlags == iOldFlags ) break;
		}
	}
	(void)xuiWidgetInvalidate(pParent, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
	__xuiWidgetNotifyParentChanged(pChild, NULL, pParent);
	return XUI_OK;
}

XUI_API int xuiWidgetInsertBefore(xui_widget pParent, xui_widget pChild, xui_widget pBefore)
{
    if(__xuiWidgetValid(pChild) && pChild->pInternalOwner)return XUI_ERROR_INVALID_STATE;
	xui_context pContext = __xuiWidgetValid(pParent) ? pParent->pContext : NULL;
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetInsertBeforeOperation(pParent, pChild, pBefore,NULL);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

int xuiInternalWidgetInsertPart(xui_widget owner,xui_widget parent,xui_widget part,xui_widget before)
{
    if(!__xuiWidgetValid(owner) || !__xuiWidgetValid(parent) || !__xuiWidgetValid(part) ||
       part->pInternalOwner!=owner || owner->pContext!=part->pContext || parent->pContext!=part->pContext)return XUI_ERROR_INVALID_STATE;
    xui_context context=part->pContext;xuiInternalOperationEnter(context);
    int result=__xuiWidgetInsertBeforeOperation(parent,part,before,owner);
    xuiInternalOperationLeave(context);return result;
}
int xuiInternalWidgetDetachPart(xui_widget owner,xui_widget part)
{
    if(!__xuiWidgetMemoryValid(owner) || !__xuiWidgetValid(part) || part->pInternalOwner!=owner || owner->pContext!=part->pContext)return XUI_ERROR_INVALID_STATE;
    xui_context context=part->pContext;xuiInternalOperationEnter(context);
    int result=__xuiWidgetRemoveFromParentOperation(part,owner);
    xuiInternalOperationLeave(context);return result;
}
int xuiInternalEnsureLayout(xui_context context)
{
	xui_size_t viewport;
	xui_rect_t rect;
	if ( !xuiInternalContextIsValid(context) ) return XUI_ERROR_INVALID_ARGUMENT;
	viewport = xuiGetViewportSize(context);
	if ( viewport.iW <= 0 || viewport.iH <= 0 ) return XUI_OK;
	rect = (xui_rect_t){0, 0, viewport.iW, viewport.iH};
	if ( context->tLayoutStats.iGeneration != 0 && context->tLayoutStats.bStabilized &&
	     !__xuiLayoutRootNeedsArrange(context->pRoot, rect) &&
	     !__xuiLayoutRootNeedsArrange(context->pOverlayRoot, rect) ) return XUI_OK;
	return xuiLayout(context);
}

static int __xuiWidgetRemoveFromParentOperation(xui_widget pWidget,const void* pAuthority)
{
	xui_widget pParent;
	xui_rect_t tWorldRect;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    if(pWidget->pInternalOwner && pWidget->pInternalOwner!=pAuthority)return XUI_ERROR_INVALID_STATE;
	pParent = pWidget->pParent;
	if ( pParent == NULL ) {
		return XUI_OK;
	}
	xuiInternalStateRecordBoundsTree(pWidget);
	xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
	if ( !__xuiWidgetValid(pWidget) || pWidget->pParent != pParent ) return XUI_OK;
    if(pWidget->pInternalOwner && pWidget->pInternalOwner!=pAuthority)return XUI_ERROR_INVALID_STATE;
	tWorldRect = xuiWidgetGetWorldRect(pWidget);
	/* Remove the stable paint-order range while the old ancestor chain is
	 * still available. A detached overlay may be destroyed before the next
	 * RenderTree pass, so deferring this would leave stale subtree endpoints. */
	__xuiWidgetUnlinkRenderNodeOnDestroy(pWidget);
	xuiInternalLayoutDetach(pWidget);
	if ( pWidget->pPrevSibling != NULL ) {
		pWidget->pPrevSibling->pNextSibling = pWidget->pNextSibling;
	} else {
		pParent->pFirstChild = pWidget->pNextSibling;
	}
	if ( pWidget->pNextSibling != NULL ) {
		pWidget->pNextSibling->pPrevSibling = pWidget->pPrevSibling;
	} else {
		pParent->pLastChild = pWidget->pPrevSibling;
	}
	pWidget->pParent = NULL;
	pWidget->pPrevSibling = NULL;
	pWidget->pNextSibling = NULL;
	if ( pParent->iChildCount > 0 ) {
		pParent->iChildCount--;
	}
	__xuiWidgetStackingDirty(pParent);
	__xuiWidgetBumpLayoutVersion(pParent);
	__xuiWidgetUpdateEventMasksToRoot(pParent);
	__xuiWidgetRepairDirtyToRoot(pParent);
	__xuiWidgetRepairPrepareFrameDependentToRoot(pParent);
	__xuiWidgetRepairUpdateStateToRoot(pParent);
	(void)__xuiWidgetInvalidateWorldRect(pParent, tWorldRect, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
	__xuiWidgetNotifyParentChanged(pWidget, pParent, NULL);
	if ( __xuiWidgetValid(pWidget) ) __xuiWidgetSyncInactiveTree(pWidget);
	return XUI_OK;
}

XUI_API xui_widget xuiWidgetGetParent(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->pParent : NULL;
}

XUI_API xui_widget xuiWidgetGetFirstChild(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->pFirstChild : NULL;
}

XUI_API xui_widget xuiWidgetGetLastChild(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->pLastChild : NULL;
}

XUI_API xui_widget xuiWidgetGetPrevSibling(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->pPrevSibling : NULL;
}

XUI_API xui_widget xuiWidgetGetNextSibling(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->pNextSibling : NULL;
}

XUI_API int xuiWidgetGetChildCount(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iChildCount : 0;
}

static int __xuiWidgetSetRectOperation(xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tOldRect;
	xui_rect_t tNewRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValid(tRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tRect = xuiInternalSnapRect(tRect);
	if ( (pWidget->tRect.fX == tRect.fX) &&
	     (pWidget->tRect.fY == tRect.fY) &&
	     (pWidget->tRect.fW == tRect.fW) &&
	     (pWidget->tRect.fH == tRect.fH) ) {
		return XUI_OK;
	}
	tOldRect = xuiWidgetGetWorldRect(pWidget);
	xuiInternalStateRecordBoundsTree(pWidget);
	pWidget->tRect = tRect;
	__xuiWidgetBumpLayoutVersion(pWidget);
	tNewRect = xuiWidgetGetWorldRect(pWidget);
	iRet = __xuiWidgetInvalidateWorldRect(pWidget, tOldRect, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, tNewRect, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetRect(xui_widget pWidget, xui_rect_t tRect)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetSetRectOperation(pWidget, tRect);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API xui_rect_t xuiWidgetGetRect(xui_widget pWidget)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( __xuiWidgetValid(pWidget) ) {
		tRect = pWidget->tRect;
	}
	return tRect;
}

XUI_API xui_rect_t xuiWidgetGetWorldRect(xui_widget pWidget)
{
	xui_rect_t tRect;
	xui_widget pParent;

	memset(&tRect, 0, sizeof(tRect));
	if ( !__xuiWidgetValid(pWidget) ) {
		return tRect;
	}
	tRect = pWidget->tRect;
	for ( pParent = pWidget->pParent; pParent != NULL; pParent = pParent->pParent ) {
		tRect.fX += pParent->tRect.fX;
		tRect.fY += pParent->tRect.fY;
	}
	return tRect;
}

XUI_API int xuiWidgetIsAttachedToContext(xui_widget pWidget)
{
	return __xuiWidgetAttachedToContext(pWidget);
}

XUI_API xui_rect_t xuiWidgetGetContentRect(xui_widget pWidget)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( __xuiWidgetValid(pWidget) ) {
		tRect = __xuiWidgetContentRect(pWidget);
	}
	return tRect;
}

XUI_API int xuiWidgetSetLayout(xui_widget pWidget, const xui_layout_t* pLayout)
{
	int bStackingChanged;
	int iRet;
	if ( !__xuiWidgetValid(pWidget) || !__xuiLayoutStructValid(pLayout) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iOverflow != pLayout->iOverflow ) __xuiWidgetDamagePaintTree(pWidget);
	bStackingChanged = pWidget->tLayout.iLayer != pLayout->iLayer ||
		pWidget->tLayout.iZIndex != pLayout->iZIndex;
	if ( bStackingChanged ) {
		__xuiWidgetStackingDirty(pWidget->pParent);
	}
	pWidget->tLayout = *pLayout;
	iRet = __xuiWidgetLayoutChanged(pWidget);
	if ( iRet != XUI_OK || !bStackingChanged ) return iRet;
	return xuiWidgetInvalidate(pWidget->pParent != NULL ? pWidget->pParent : pWidget,
		XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_layout_t xuiWidgetGetLayout(xui_widget pWidget)
{
	xui_layout_t tLayout;

	memset(&tLayout, 0, sizeof(tLayout));
	if ( __xuiWidgetValid(pWidget) ) {
		tLayout = pWidget->tLayout;
	}
	return tLayout;
}

XUI_API int xuiWidgetSetContentMeasureCallbackOwned(xui_widget pWidget,xui_widget_content_measure_proc onMeasure,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onContentMeasure,onMeasure!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onContentMeasure=onMeasure;pWidget->pContentMeasureUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=__xuiWidgetLayoutChanged(pWidget);
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetContentMeasureCallback(xui_widget pWidget, xui_widget_content_measure_proc onMeasure, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onContentMeasure))return xuiWidgetSetContentMeasureCallbackOwned(pWidget,onMeasure,pUser,NULL);
	if ( (pWidget->onContentMeasure == onMeasure) && (pWidget->pContentMeasureUser == pUser) ) {
		return XUI_OK;
	}
	pWidget->onContentMeasure = onMeasure;
	pWidget->pContentMeasureUser = pUser;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetContentMeasureCallback(xui_widget pWidget, xui_widget_content_measure_proc* pMeasure, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pMeasure != NULL ) {
		*pMeasure = pWidget->onContentMeasure;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pContentMeasureUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetLayoutPrepareCallbackOwned(xui_widget pWidget,xui_widget_layout_prepare_proc onPrepare,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onLayoutPrepare,onPrepare!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onLayoutPrepare=onPrepare;pWidget->pLayoutPrepareUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=__xuiWidgetLayoutChanged(pWidget);
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetLayoutPrepareCallback(xui_widget pWidget, xui_widget_layout_prepare_proc onPrepare, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onLayoutPrepare))return xuiWidgetSetLayoutPrepareCallbackOwned(pWidget,onPrepare,pUser,NULL);
	if ( pWidget->onLayoutPrepare == onPrepare && pWidget->pLayoutPrepareUser == pUser ) {
		return XUI_OK;
	}
	pWidget->onLayoutPrepare = onPrepare;
	pWidget->pLayoutPrepareUser = pUser;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetLayoutPrepareCallback(xui_widget pWidget, xui_widget_layout_prepare_proc* pPrepare, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPrepare != NULL ) {
		*pPrepare = pWidget->onLayoutPrepare;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pLayoutPrepareUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetRemoveFromParent(xui_widget pWidget)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetRemoveFromParentOperation(pWidget,NULL);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetSetLayoutChildrenCallbackOwned(xui_widget pWidget,xui_widget_layout_children_proc onChildren,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onLayoutChildren,onChildren!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onLayoutChildren=onChildren;pWidget->pLayoutChildrenUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=__xuiWidgetLayoutChanged(pWidget);
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetLayoutChildrenCallback(xui_widget pWidget, xui_widget_layout_children_proc onChildren, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onLayoutChildren))return xuiWidgetSetLayoutChildrenCallbackOwned(pWidget,onChildren,pUser,NULL);
	if ( pWidget->onLayoutChildren == onChildren && pWidget->pLayoutChildrenUser == pUser ) {
		return XUI_OK;
	}
	pWidget->onLayoutChildren = onChildren;
	pWidget->pLayoutChildrenUser = pUser;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetLayoutChildrenCallback(xui_widget pWidget, xui_widget_layout_children_proc* pChildren, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pChildren != NULL ) {
		*pChildren = pWidget->onLayoutChildren;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pLayoutChildrenUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetLayoutCompleteCallbackOwned(xui_widget pWidget,xui_widget_layout_complete_proc onComplete,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onLayoutComplete,onComplete!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onLayoutComplete=onComplete;pWidget->pLayoutCompleteUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=__xuiWidgetLayoutChanged(pWidget);
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetLayoutCompleteCallback(xui_widget pWidget, xui_widget_layout_complete_proc onComplete, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onLayoutComplete))return xuiWidgetSetLayoutCompleteCallbackOwned(pWidget,onComplete,pUser,NULL);
	if ( pWidget->onLayoutComplete == onComplete && pWidget->pLayoutCompleteUser == pUser ) {
		return XUI_OK;
	}
	pWidget->onLayoutComplete = onComplete;
	pWidget->pLayoutCompleteUser = pUser;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetLayoutCompleteCallback(xui_widget pWidget, xui_widget_layout_complete_proc* pComplete, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pComplete != NULL ) {
		*pComplete = pWidget->onLayoutComplete;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pLayoutCompleteUser;
	}
	return XUI_OK;
}

static int __xuiWidgetMeasureContentOperation(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pContentSize)
{
	if ( !__xuiWidgetValid(pWidget) || (pContentSize == NULL) ||
	     !__xuiNonNegativeFloatValid(tConstraint.fX) ||
	     !__xuiNonNegativeFloatValid(tConstraint.fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
    return __xuiWidgetMeasureOwnContent(pWidget,tConstraint,pContentSize);
}

XUI_API int xuiWidgetMeasureContent(xui_widget widget,xui_vec2_t constraint,xui_vec2_t* output)
{
    if(!__xuiWidgetValid(widget) || !output)return XUI_ERROR_INVALID_ARGUMENT;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(widget->pContext,&scope);if(status!=XUI_OK)return status;
    status=__xuiWidgetMeasureContentOperation(widget,constraint,output);
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    status=xuiInternalCallScopeEndOutcome(&scope,status,&first);xrtSetErrorTake(first);return status;
}

XUI_API int xuiWidgetSetLayoutType(xui_widget pWidget, int iLayoutType)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiLayoutTypeValid(iLayoutType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iLayoutType == iLayoutType ) {
		return XUI_OK;
	}
	pWidget->tLayout.iLayoutType = iLayoutType;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetLayoutType(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iLayoutType : XUI_LAYOUT_MANUAL;
}

XUI_API int xuiWidgetSetSizeMode(xui_widget pWidget, int iWidthMode, int iHeightMode)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiSizeModeValid(iWidthMode) || !__xuiSizeModeValid(iHeightMode) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->tLayout.iWidthMode == iWidthMode) && (pWidget->tLayout.iHeightMode == iHeightMode) ) {
		return XUI_OK;
	}
	pWidget->tLayout.iWidthMode = iWidthMode;
	pWidget->tLayout.iHeightMode = iHeightMode;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetSizeMode(xui_widget pWidget, int* pWidthMode, int* pHeightMode)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidthMode != NULL ) {
		*pWidthMode = pWidget->tLayout.iWidthMode;
	}
	if ( pHeightMode != NULL ) {
		*pHeightMode = pWidget->tLayout.iHeightMode;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetFlowMode(xui_widget pWidget, int iFlowMode)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiFlowModeValid(iFlowMode) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iFlowMode == iFlowMode ) {
		return XUI_OK;
	}
	pWidget->tLayout.iFlowMode = iFlowMode;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetFlowMode(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iFlowMode : XUI_FLOW_BLOCK;
}

XUI_API int xuiWidgetSetDock(xui_widget pWidget, int iDock)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiDockValid(iDock) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iDock == 0 ) {
		iDock = XUI_DOCK_FILL;
	}
	if ( pWidget->tLayout.iDock == iDock ) {
		return XUI_OK;
	}
	pWidget->tLayout.iDock = iDock;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetDock(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iDock : 0;
}

XUI_API int xuiWidgetSetOverflow(xui_widget pWidget, int iOverflow)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiOverflowValid(iOverflow) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iOverflow == iOverflow ) {
		return XUI_OK;
	}
	__xuiWidgetDamagePaintTree(pWidget);
	pWidget->tLayout.iOverflow = iOverflow;
	pWidget->bRenderGeometryDirty = 1;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetOverflow(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iOverflow : XUI_OVERFLOW_VISIBLE;
}

XUI_API int xuiWidgetSetMeasureContainment(xui_widget pWidget, int iContainment)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiMeasureContainmentValid(iContainment) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iMeasureContainment == iContainment ) {
		return XUI_OK;
	}
	pWidget->tLayout.iMeasureContainment = iContainment;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetMeasureContainment(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iMeasureContainment : XUI_MEASURE_CONTAIN_NONE;
}

XUI_API int xuiWidgetSetGridMetrics(xui_widget pWidget, int iColumnCount, float fItemWidth, float fItemHeight)
{
	if ( !__xuiWidgetValid(pWidget) || (iColumnCount <= 0) ||
	     !__xuiNonNegativeFloatValid(fItemWidth) || !__xuiNonNegativeFloatValid(fItemHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->tLayout.iGridColumnCount == iColumnCount) &&
	     (pWidget->tLayout.fGridItemWidth == fItemWidth) &&
	     (pWidget->tLayout.fGridItemHeight == fItemHeight) ) {
		return XUI_OK;
	}
	pWidget->tLayout.iGridColumnCount = iColumnCount;
	pWidget->tLayout.fGridItemWidth = fItemWidth;
	pWidget->tLayout.fGridItemHeight = fItemHeight;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetGridMetrics(xui_widget pWidget, int* pColumnCount, float* pItemWidth, float* pItemHeight)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pColumnCount != NULL ) {
		*pColumnCount = pWidget->tLayout.iGridColumnCount;
	}
	if ( pItemWidth != NULL ) {
		*pItemWidth = pWidget->tLayout.fGridItemWidth;
	}
	if ( pItemHeight != NULL ) {
		*pItemHeight = pWidget->tLayout.fGridItemHeight;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetLayer(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pStructuralRoot;
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->tLayout.iLayer == iLayer) && (pWidget->tLayout.iZIndex == iZIndex) ) {
		return XUI_OK;
	}
	pWidget->tLayout.iLayer = iLayer;
	pWidget->tLayout.iZIndex = iZIndex;
	__xuiWidgetStackingDirty(pWidget->pParent);
	pStructuralRoot = pWidget->pParent != NULL ? pWidget->pParent : pWidget;
	return xuiWidgetInvalidate(pStructuralRoot, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetLayer(xui_widget pWidget, int* pLayer, int* pZIndex)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pLayer != NULL ) {
		*pLayer = pWidget->tLayout.iLayer;
	}
	if ( pZIndex != NULL ) {
		*pZIndex = pWidget->tLayout.iZIndex;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetPreferredSize(xui_widget pWidget, xui_vec2_t tSize)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiNonNegativeFloatValid(tSize.fX) || !__xuiNonNegativeFloatValid(tSize.fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->tLayout.fPreferredWidth == tSize.fX) && (pWidget->tLayout.fPreferredHeight == tSize.fY) ) {
		return XUI_OK;
	}
	pWidget->tLayout.fPreferredWidth = tSize.fX;
	pWidget->tLayout.fPreferredHeight = tSize.fY;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API xui_vec2_t xuiWidgetGetPreferredSize(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	return __xuiVec2(pWidget->tLayout.fPreferredWidth, pWidget->tLayout.fPreferredHeight);
}

XUI_API int xuiWidgetSetMinSize(xui_widget pWidget, xui_vec2_t tSize)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiNonNegativeFloatValid(tSize.fX) || !__xuiNonNegativeFloatValid(tSize.fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tSize.fX > pWidget->tLayout.fMaxWidth) || (tSize.fY > pWidget->tLayout.fMaxHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.fMinWidth == tSize.fX && pWidget->tLayout.fMinHeight == tSize.fY ) {
		return XUI_OK;
	}
	pWidget->tLayout.fMinWidth = tSize.fX;
	pWidget->tLayout.fMinHeight = tSize.fY;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API xui_vec2_t xuiWidgetGetMinSize(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	return __xuiVec2(pWidget->tLayout.fMinWidth, pWidget->tLayout.fMinHeight);
}

XUI_API int xuiWidgetSetMaxSize(xui_widget pWidget, xui_vec2_t tSize)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiNonNegativeFloatValid(tSize.fX) || !__xuiNonNegativeFloatValid(tSize.fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tSize.fX < pWidget->tLayout.fMinWidth) || (tSize.fY < pWidget->tLayout.fMinHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.fMaxWidth == tSize.fX && pWidget->tLayout.fMaxHeight == tSize.fY ) {
		return XUI_OK;
	}
	pWidget->tLayout.fMaxWidth = tSize.fX;
	pWidget->tLayout.fMaxHeight = tSize.fY;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API xui_vec2_t xuiWidgetGetMaxSize(xui_widget pWidget)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	return __xuiVec2(pWidget->tLayout.fMaxWidth, pWidget->tLayout.fMaxHeight);
}

XUI_API int xuiWidgetSetMargin(xui_widget pWidget, xui_thickness_t tMargin)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiThicknessValid(tMargin) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( memcmp(&pWidget->tLayout.tMargin, &tMargin, sizeof(tMargin)) == 0 ) {
		return XUI_OK;
	}
	pWidget->tLayout.tMargin = tMargin;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API xui_thickness_t xuiWidgetGetMargin(xui_widget pWidget)
{
	xui_thickness_t tValue;

	memset(&tValue, 0, sizeof(tValue));
	if ( __xuiWidgetValid(pWidget) ) {
		tValue = pWidget->tLayout.tMargin;
	}
	return tValue;
}

XUI_API int xuiWidgetSetPadding(xui_widget pWidget, xui_thickness_t tPadding)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiThicknessValid(tPadding) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( memcmp(&pWidget->tLayout.tPadding, &tPadding, sizeof(tPadding)) == 0 ) {
		return XUI_OK;
	}
	pWidget->tLayout.tPadding = tPadding;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API xui_thickness_t xuiWidgetGetPadding(xui_widget pWidget)
{
	xui_thickness_t tValue;

	memset(&tValue, 0, sizeof(tValue));
	if ( __xuiWidgetValid(pWidget) ) {
		tValue = pWidget->tLayout.tPadding;
	}
	return tValue;
}

XUI_API int xuiWidgetSetGap(xui_widget pWidget, float fGap)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiNonNegativeFloatValid(fGap) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.fGap == fGap ) {
		return XUI_OK;
	}
	pWidget->tLayout.fGap = fGap;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API float xuiWidgetGetGap(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.fGap : 0.0f;
}

XUI_API int xuiWidgetSetFlex(xui_widget pWidget, float fGrow, float fShrink)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiNonNegativeFloatValid(fGrow) || !__xuiNonNegativeFloatValid(fShrink) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.fGrow == fGrow && pWidget->tLayout.fShrink == fShrink ) {
		return XUI_OK;
	}
	pWidget->tLayout.fGrow = fGrow;
	pWidget->tLayout.fShrink = fShrink;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetFlex(xui_widget pWidget, float* pGrow, float* pShrink)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pGrow != NULL ) {
		*pGrow = pWidget->tLayout.fGrow;
	}
	if ( pShrink != NULL ) {
		*pShrink = pWidget->tLayout.fShrink;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetAlign(xui_widget pWidget, int iAlignX, int iAlignY)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiAlignValid(iAlignX) || !__xuiAlignValid(iAlignY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iAlignX == iAlignX && pWidget->tLayout.iAlignY == iAlignY ) {
		return XUI_OK;
	}
	pWidget->tLayout.iAlignX = iAlignX;
	pWidget->tLayout.iAlignY = iAlignY;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetAlign(xui_widget pWidget, int* pAlignX, int* pAlignY)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pAlignX != NULL ) {
		*pAlignX = pWidget->tLayout.iAlignX;
	}
	if ( pAlignY != NULL ) {
		*pAlignY = pWidget->tLayout.iAlignY;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetTableSize(xui_widget pWidget, int iRows, int iColumns)
{
	xui_table_track_t* pNewRows;
	xui_table_track_t* pNewColumns;
	int bRowsChanged;
	int bColumnsChanged;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (iRows < 0) || (iColumns < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (uint64_t)(uint32_t)iRows + (uint64_t)(uint32_t)iColumns > (uint64_t)UINT32_MAX / 2u ) {
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	bRowsChanged = iRows != pWidget->iTableRowCount;
	bColumnsChanged = iColumns != pWidget->iTableColumnCount;
	if ( !bRowsChanged && !bColumnsChanged ) return XUI_OK;
	pNewRows = pWidget->pTableRows;
	pNewColumns = pWidget->pTableColumns;
	if ( bRowsChanged ) {
		iRet = __xuiWidgetBuildTrackArray(pWidget->pTableRows, pWidget->iTableRowCount, iRows, &pNewRows);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( bColumnsChanged ) {
		iRet = __xuiWidgetBuildTrackArray(pWidget->pTableColumns, pWidget->iTableColumnCount,
			iColumns, &pNewColumns);
		if ( iRet != XUI_OK ) {
			if ( bRowsChanged && pNewRows != NULL ) xrtFree(pNewRows);
			return iRet;
		}
	}
	if ( bRowsChanged ) {
		if ( pWidget->pTableRows != NULL ) xrtFree(pWidget->pTableRows);
		pWidget->pTableRows = pNewRows;
		pWidget->iTableRowCount = iRows;
	}
	if ( bColumnsChanged ) {
		if ( pWidget->pTableColumns != NULL ) xrtFree(pWidget->pTableColumns);
		pWidget->pTableColumns = pNewColumns;
		pWidget->iTableColumnCount = iColumns;
	}
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetTableSize(xui_widget pWidget, int* pRows, int* pColumns)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pRows != NULL ) {
		*pRows = pWidget->iTableRowCount;
	}
	if ( pColumns != NULL ) {
		*pColumns = pWidget->iTableColumnCount;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetTableRow(xui_widget pWidget, int iRow, const xui_table_track_t* pTrack)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiTableTrackValid(pTrack) || (iRow < 0) || (iRow >= pWidget->iTableRowCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( memcmp(&pWidget->pTableRows[iRow], pTrack, sizeof(*pTrack)) == 0 ) {
		return XUI_OK;
	}
	pWidget->pTableRows[iRow] = *pTrack;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetTableRow(xui_widget pWidget, int iRow, xui_table_track_t* pTrack)
{
	if ( !__xuiWidgetValid(pWidget) || (pTrack == NULL) || (iRow < 0) || (iRow >= pWidget->iTableRowCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pTrack = pWidget->pTableRows[iRow];
	return XUI_OK;
}

XUI_API int xuiWidgetSetTableColumn(xui_widget pWidget, int iColumn, const xui_table_track_t* pTrack)
{
	if ( !__xuiWidgetValid(pWidget) || !__xuiTableTrackValid(pTrack) || (iColumn < 0) || (iColumn >= pWidget->iTableColumnCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( memcmp(&pWidget->pTableColumns[iColumn], pTrack, sizeof(*pTrack)) == 0 ) {
		return XUI_OK;
	}
	pWidget->pTableColumns[iColumn] = *pTrack;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetTableColumn(xui_widget pWidget, int iColumn, xui_table_track_t* pTrack)
{
	if ( !__xuiWidgetValid(pWidget) || (pTrack == NULL) || (iColumn < 0) || (iColumn >= pWidget->iTableColumnCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pTrack = pWidget->pTableColumns[iColumn];
	return XUI_OK;
}

XUI_API int xuiWidgetSetTableCell(xui_widget pWidget, int iRow, int iColumn, int iRowSpan, int iColumnSpan)
{
	if ( !__xuiWidgetValid(pWidget) || (iRow < 0) || (iColumn < 0) || (iRowSpan <= 0) || (iColumnSpan <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->tLayout.iTableRow == iRow && pWidget->tLayout.iTableColumn == iColumn &&
		pWidget->tLayout.iTableRowSpan == iRowSpan && pWidget->tLayout.iTableColumnSpan == iColumnSpan ) {
		return XUI_OK;
	}
	pWidget->tLayout.iTableRow = iRow;
	pWidget->tLayout.iTableColumn = iColumn;
	pWidget->tLayout.iTableRowSpan = iRowSpan;
	pWidget->tLayout.iTableColumnSpan = iColumnSpan;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetTableCell(xui_widget pWidget, int* pRow, int* pColumn, int* pRowSpan, int* pColumnSpan)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pRow != NULL ) {
		*pRow = pWidget->tLayout.iTableRow;
	}
	if ( pColumn != NULL ) {
		*pColumn = pWidget->tLayout.iTableColumn;
	}
	if ( pRowSpan != NULL ) {
		*pRowSpan = pWidget->tLayout.iTableRowSpan;
	}
	if ( pColumnSpan != NULL ) {
		*pColumnSpan = pWidget->tLayout.iTableColumnSpan;
	}
	return XUI_OK;
}

static int __xuiWidgetMeasureOperation(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasuredSize)
{
	if ( !__xuiWidgetValid(pWidget) || (pMeasuredSize == NULL) ||
	     !__xuiNonNegativeFloatValid(tConstraint.fX) ||
	     !__xuiNonNegativeFloatValid(tConstraint.fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiInternalLayoutMeasure(pWidget, tConstraint, pMeasuredSize);
}

XUI_API int xuiWidgetMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasuredSize)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetMeasureOperation(pWidget, tConstraint, pMeasuredSize);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

static int __xuiWidgetArrangeOperation(xui_widget pParent, xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tOldRect;
	xui_rect_t tNewRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValid(tRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tOldRect = xuiWidgetGetWorldRect(pWidget);
	iRet = pParent != NULL ? xuiInternalLayoutArrangeChild(pParent, pWidget, tRect)
		: xuiInternalLayoutArrange(pWidget, tRect);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tNewRect = xuiWidgetGetWorldRect(pWidget);
	iRet = __xuiWidgetInvalidateWorldRect(pWidget, tOldRect, XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, tNewRect, XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetArrange(xui_widget pWidget, xui_rect_t tRect)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetArrangeOperation(NULL, pWidget, tRect);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetArrangeChild(xui_widget pParent, xui_widget pChild, xui_rect_t tRect)
{
	xui_context pContext;
	int iRet;
	if ( !__xuiWidgetValid(pParent) || !__xuiWidgetValid(pChild) ||
		pChild->pParent != pParent || pParent->pContext != pChild->pContext ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = pParent->pContext;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetArrangeOperation(pParent, pChild, tRect);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

static int __xuiWidgetSetVisibleOperation(xui_widget pWidget, int bVisible)
{
	xui_widget pStructuralRoot;
	int iRet;
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bVisible = bVisible ? 1 : 0;
	if ( pWidget->bVisible == bVisible ) {
		return XUI_OK;
	}
	xuiInternalStateRecordVisible(pWidget);
	pWidget->bVisible = bVisible;
	__xuiWidgetBumpLayoutVersion(pWidget);
	if ( pWidget->pParent != NULL ) {
		__xuiWidgetBumpLayoutVersion(pWidget->pParent);
	}
	if ( !bVisible ) {
		xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
		if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
		__xuiWidgetSyncInactiveTree(pWidget);
		if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
	}
	iRet = __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget),
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pStructuralRoot = pWidget->pParent != NULL ? pWidget->pParent : pWidget;
	return xuiWidgetInvalidate(pStructuralRoot,
		XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetVisible(xui_widget pWidget, int bVisible)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetSetVisibleOperation(pWidget, bVisible);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetGetVisible(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bVisible : 0;
}

XUI_API int xuiWidgetGetEffectiveVisible(xui_widget pWidget)
{
	return __xuiWidgetEffectiveVisible(pWidget);
}

static int __xuiWidgetSetEnabledOperation(xui_widget pWidget, int bEnabled)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bEnabled = bEnabled ? 1 : 0;
	if ( pWidget->bEnabled == bEnabled ) {
		return XUI_OK;
	}
	xuiInternalStateRecordEnabled(pWidget);
	pWidget->bEnabled = bEnabled;
	__xuiWidgetMarkDescendantPaintDirty(pWidget,
		XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( !bEnabled ) {
		xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
		if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
	}
	iRet = xuiWidgetResolveStyle(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget), XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetEnabled(xui_widget pWidget, int bEnabled)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetSetEnabledOperation(pWidget, bEnabled);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetGetEnabled(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bEnabled : 0;
}

XUI_API int xuiWidgetGetEffectiveEnabled(xui_widget pWidget)
{
	return __xuiWidgetEffectiveEnabled(pWidget);
}

XUI_API uint32_t xuiWidgetGetInputState(xui_widget pWidget)
{
	uint32_t iState;

	if ( !__xuiWidgetValid(pWidget) ) {
		return 0;
	}
	iState = pWidget->iInputState;
	if ( !__xuiWidgetEffectiveEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	return iState;
}

static int __xuiWidgetSetHitTestVisibleOperation(xui_widget pWidget, int bVisible)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bVisible = bVisible ? 1 : 0;
	if ( pWidget->bHitTestVisible == bVisible ) {
		return XUI_OK;
	}
	pWidget->bHitTestVisible = bVisible;
	if ( !bVisible ) {
		xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
		if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetHitTestVisible(xui_widget pWidget, int bVisible)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetSetHitTestVisibleOperation(pWidget, bVisible);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetGetHitTestVisible(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bHitTestVisible : 0;
}

XUI_API int xuiWidgetGetEffectiveHitTestVisible(xui_widget pWidget)
{
	xui_widget pScan;

	if ( !__xuiWidgetEffectiveVisible(pWidget) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( !pScan->bHitTestVisible ) return 0;
	}
	return 1;
}

static int __xuiWidgetSetFocusableOperation(xui_widget pWidget, int bFocusable)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bFocusable = bFocusable ? 1 : 0;
	if ( pWidget->bFocusable == bFocusable ) {
		return XUI_OK;
	}
	pWidget->bFocusable = bFocusable;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_STATE_CHANGED);
	if ( !bFocusable && (pWidget->pContext != NULL) && (pWidget->pContext->pFocusWidget == pWidget) ) {
		pWidget->bInteractionCancelling = 1;
		iRet = xuiInternalInputCancelFocusSubtree(pWidget->pContext, pWidget);
		if ( pWidget->iMagic == XUI_WIDGET_MAGIC ) pWidget->bInteractionCancelling = 0;
		if ( iRet != XUI_OK ) return iRet;
		if ( !__xuiWidgetValid(pWidget) ) return XUI_OK;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetFocusable(xui_widget pWidget, int bFocusable)
{
	xui_context pContext;
	int iRet;

	pContext = __xuiWidgetValid(pWidget) ? pWidget->pContext : NULL;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiWidgetSetFocusableOperation(pWidget, bFocusable);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetGetFocusable(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bFocusable : 0;
}

XUI_API int xuiWidgetGetEffectiveFocusable(xui_widget pWidget)
{
	xui_widget pScan;

	if ( !__xuiWidgetValid(pWidget) || !pWidget->bFocusable ||
	     !__xuiWidgetEffectiveVisible(pWidget) ||
	     !__xuiWidgetEffectiveEnabled(pWidget) ||
	     !__xuiWidgetAttachedToContext(pWidget) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan->bInteractionCancelling ) return 0;
	}
	return 1;
}

XUI_API int xuiWidgetSetTabStop(xui_widget pWidget, int bTabStop)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->bTabStop = bTabStop ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiWidgetGetTabStop(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bTabStop : 0;
}

XUI_API int xuiWidgetSetTabIndex(xui_widget pWidget, int iTabIndex)
{
	if ( !__xuiWidgetValid(pWidget) || (iTabIndex < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->iTabIndex = iTabIndex;
	return XUI_OK;
}

XUI_API int xuiWidgetGetTabIndex(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iTabIndex : 0;
}

XUI_API int xuiWidgetSetFocusScope(xui_widget pWidget, int bFocusScope)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->bFocusScope = bFocusScope ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiWidgetGetFocusScope(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bFocusScope : 0;
}

XUI_API int xuiWidgetSetDefaultActionOwned(xui_widget pWidget,xui_widget_action_proc onAction,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onDefaultAction,onAction!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onDefaultAction=onAction;pWidget->pDefaultActionUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=XUI_OK;
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetDefaultAction(xui_widget pWidget, xui_widget_action_proc onAction, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}

    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onDefaultAction))return xuiWidgetSetDefaultActionOwned(pWidget,onAction,pUser,NULL);
	pWidget->onDefaultAction = onAction;
	pWidget->pDefaultActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiWidgetSetCancelActionOwned(xui_widget pWidget,xui_widget_action_proc onAction,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onCancelAction,onAction!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onCancelAction=onAction;pWidget->pCancelActionUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=XUI_OK;
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetCancelAction(xui_widget pWidget, xui_widget_action_proc onAction, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}

    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onCancelAction))return xuiWidgetSetCancelActionOwned(pWidget,onAction,pUser,NULL);
	pWidget->onCancelAction = onAction;
	pWidget->pCancelActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiWidgetSetDragEnabled(xui_widget pWidget, int bEnabled)
{
	uint64_t iOldOwnMask;
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
	pWidget->bDragEnabled = bEnabled ? 1 : 0;
	if ( pWidget->bDragEnabled ) {
		pWidget->iEventInterestMask |= XUI_EVENT_MASK_DRAG;
	} else if ( !pWidget->bDropEnabled ) {
		pWidget->iEventInterestMask &= ~XUI_EVENT_MASK_DRAG;
	}
	__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	return XUI_OK;
}

XUI_API int xuiWidgetGetDragEnabled(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bDragEnabled : 0;
}

XUI_API int xuiWidgetSetDropEnabled(xui_widget pWidget, int bEnabled)
{
	uint64_t iOldOwnMask;
	if ( !__xuiWidgetValid(pWidget) ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
	pWidget->bDropEnabled = bEnabled ? 1 : 0;
	if ( pWidget->bDropEnabled ) {
		pWidget->iEventInterestMask |= XUI_EVENT_MASK_DRAG;
	} else if ( !pWidget->bDragEnabled ) {
		pWidget->iEventInterestMask &= ~XUI_EVENT_MASK_DRAG;
	}
	__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	return XUI_OK;
}

XUI_API int xuiWidgetGetDropEnabled(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bDropEnabled : 0;
}

static int __xuiInputImeRectEqual(xui_rect_t tA, xui_rect_t tB)
{
	const float fEpsilon = 0.01f;
	float fDelta;

	fDelta = tA.fX - tB.fX;
	if ( fDelta < -fEpsilon || fDelta > fEpsilon ) return 0;
	fDelta = tA.fY - tB.fY;
	if ( fDelta < -fEpsilon || fDelta > fEpsilon ) return 0;
	fDelta = tA.fW - tB.fW;
	if ( fDelta < -fEpsilon || fDelta > fEpsilon ) return 0;
	fDelta = tA.fH - tB.fH;
	return (fDelta >= -fEpsilon && fDelta <= fEpsilon) ? 1 : 0;
}

void xuiInternalInputInvalidateImeOwner(xui_context pContext)
{
	static uint64_t nextOwner;
	xui_ime_composition_t tComposition;
	if ( !xuiInternalContextIsValid(pContext) ) return;
	if ( pContext->bImeCompositionActive ) {
		memset(&tComposition, 0, sizeof(tComposition));
		tComposition.iSize = sizeof(tComposition);
		tComposition.sText = "";
		/* Clear marked text before rotating the owner. A delayed native END then
		 * becomes harmless instead of leaving the focused editor input-locked. */
		(void)xuiInputImeCompositionEx(pContext, &tComposition);
	}
	if ( ++nextOwner == 0 ) ++nextOwner;
	pContext->iImeOwner = nextOwner;
	if ( pContext->onImeFocusChanged != NULL ) pContext->onImeFocusChanged(pContext);
}

static int __xuiInputSyncImeOperation(xui_context pContext, int bForceEnabled, int bForceRect)
{
	xui_widget pFocus;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tOldRect;
	int bEnabled;
	int bOldEnabled;
	int bOldHasRect;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pFocus = pContext->pFocusWidget;
	if ( bForceEnabled ) xuiInternalInputInvalidateImeOwner(pContext);
	bOldEnabled = pContext->bImeEnabled;
	bOldHasRect = pContext->bHasImeCandidateRect;
	tOldRect = pContext->tImeCandidateRect;
	bEnabled = (pFocus != NULL) &&
	           __xuiWidgetEffectiveVisible(pFocus) &&
	           __xuiWidgetEffectiveEnabled(pFocus) &&
	           ((pFocus->iImeMode == XUI_IME_ENABLED) ||
	            ((pFocus->iImeMode == XUI_IME_AUTO) && pFocus->bFocusable));
	memset(&tRect, 0, sizeof(tRect));
	pContext->bImeEnabled = bEnabled;
	pContext->bHasImeCandidateRect = 0;
	if ( bEnabled ) {
		if ( pFocus->onImeCandidateRect != NULL ) {
			tRect = xuiInternalInvokeImeCandidateRect(pFocus);
		} else {
			tRect = xuiWidgetGetWorldRect(pFocus);
		}
		if(!__xuiWidgetValid(pFocus) || xuiInternalContextDestroyPending(pContext))return XUI_OK;
		pContext->tImeCandidateRect = tRect;
		pContext->bHasImeCandidateRect = 1;
	}
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) {
		return XUI_OK;
	}
	if ( bForceEnabled || (bOldEnabled != bEnabled) ) {
		iRet = pProxy->imeSetEnabled(pProxy, bEnabled);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) {
			return XUI_OK;
		}
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( bEnabled && (bForceRect || !bOldEnabled || !bOldHasRect || !__xuiInputImeRectEqual(tOldRect, tRect)) ) {
		iRet = pProxy->imeSetCandidateRect(pProxy, tRect);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) {
			return XUI_OK;
		}
		return iRet;
	}
	return XUI_OK;
}

static int __xuiInputSyncIme(xui_context pContext,int bForceEnabled,int bForceRect)
{
    if(!xuiInternalContextIsValid(pContext))return XUI_ERROR_INVALID_ARGUMENT;
    xuiInternalOperationEnter(pContext);
    int result=__xuiInputSyncImeOperation(pContext,bForceEnabled,bForceRect);
    xuiInternalOperationLeave(pContext);return result;
}

int xuiInternalInputSyncIme(xui_context pContext)
{
	return __xuiInputSyncIme(pContext, 1, 1);
}

int xuiInternalInputRefreshIme(xui_context pContext)
{
	return __xuiInputSyncIme(pContext, 0, 0);
}

int xuiInternalInputRefreshImePosition(xui_context pContext)
{
	return __xuiInputSyncIme(pContext, 0, 1);
}

XUI_API int xuiWidgetSetImeMode(xui_widget pWidget, int iImeMode)
{
	uint64_t iOldOwnMask;
	if ( !__xuiWidgetValid(pWidget) ||
	     ((iImeMode != XUI_IME_DISABLED) && (iImeMode != XUI_IME_ENABLED) && (iImeMode != XUI_IME_AUTO)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
	pWidget->iImeMode = iImeMode;
	if ( iImeMode == XUI_IME_DISABLED ) {
		pWidget->iEventInterestMask &= ~XUI_EVENT_MASK_IME;
	} else {
		pWidget->iEventInterestMask |= XUI_EVENT_MASK_IME;
	}
	__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	if ( (pWidget->pContext != NULL) && (pWidget->pContext->pFocusWidget == pWidget) ) {
		return xuiInternalInputSyncIme(pWidget->pContext);
	}
	return XUI_OK;
}

XUI_API int xuiWidgetGetImeMode(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iImeMode : XUI_IME_DISABLED;
}

XUI_API int xuiWidgetSetImeCandidateRectOwned(xui_widget pWidget,xui_widget_ime_rect_proc onRect,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onImeCandidateRect,onRect!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onImeCandidateRect=onRect;pWidget->pImeCandidateRectUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=(pWidget->pContext->pFocusWidget==pWidget) ? xuiInternalInputSyncIme(pWidget->pContext) : XUI_OK;
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetImeCandidateRect(xui_widget pWidget, xui_widget_ime_rect_proc onRect, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}

    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onImeCandidateRect))return xuiWidgetSetImeCandidateRectOwned(pWidget,onRect,pUser,NULL);
	pWidget->onImeCandidateRect = onRect;
	pWidget->pImeCandidateRectUser = pUser;
	if ( (pWidget->pContext != NULL) && (pWidget->pContext->pFocusWidget == pWidget) ) {
		return xuiInternalInputSyncIme(pWidget->pContext);
	}
	return XUI_OK;
}

XUI_API int xuiWidgetGetImeCandidateRect(xui_widget pWidget, xui_widget_ime_rect_proc* pRect, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pRect != NULL ) {
		*pRect = pWidget->onImeCandidateRect;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pImeCandidateRectUser;
	}
	return XUI_OK;
}

XUI_API int xuiHasImeCandidateRect(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->bHasImeCandidateRect : 0;
}

XUI_API xui_rect_t xuiGetImeCandidateRect(xui_context pContext)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( xuiInternalContextIsValid(pContext) && pContext->bHasImeCandidateRect ) {
		tRect = pContext->tImeCandidateRect;
	}
	return tRect;
}

XUI_API int xuiWidgetSetEventCallback(xui_widget pWidget, xui_widget_event_proc onEvent, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->pEventOwners != NULL ) return xuiWidgetSetEventCallbackOwned(pWidget, onEvent, pUser, NULL);
	pWidget->onEvent = onEvent;
	pWidget->pEventUser = pUser;
	return XUI_OK;
}

	/* One sparse allocation per widget that actually registers an owned event.
 * Ordinary borrowed C registration keeps its existing two-store fast path. */
static int __xuiWidgetSetOwnedEvent(xui_widget pWidget, int iEventType,
	xui_widget_event_proc onEvent, void* pUser, const xui_callback_owner_t* pOwner)
{
	if ( !__xuiWidgetValid(pWidget) || iEventType < -1 || iEventType >= XUI_WIDGET_EVENT_SLOT_COUNT ||
		!xuiInternalCallbackOwnerValid(pOwner) || (onEvent == NULL && pOwner != NULL && pOwner->onHold != NULL) )
		return XUI_ERROR_INVALID_ARGUMENT;
	/* Hold is user code: snapshot the incoming descriptor before it can
	 * reenter, replace the same slot or invalidate the context/widget. */
	xui_callback_owner_t tCandidate = pOwner == NULL ? (xui_callback_owner_t){0} : *pOwner;
	pOwner = &tCandidate;
	xui_context pContext = pWidget->pContext;
	xui_call_scope_t tScope = {0};
	int iResult = xuiCallScopeBegin(pContext, &tScope);
	if ( iResult != XUI_OK ) return iResult;
	xui_event_owners_t* pNew = NULL;
	xui_callback_owner_t* pNewHandlers = NULL;
	if ( pWidget->pEventOwners == NULL && pOwner != NULL && pOwner->onHold != NULL ) {
		pNew = xrtCalloc(1, sizeof(*pNew));
		if ( pNew == NULL ) { xuiInternalCallScopeRecordOutcome(&tScope,XUI_ERROR_OUT_OF_MEMORY,xrtGetError()); return xuiCallScopeEnd(&tScope); }
	}
	if ( iEventType != -1 && pOwner != NULL && pOwner->onHold != NULL &&
		(pWidget->pEventOwners == NULL || pWidget->pEventOwners->pHandlers == NULL) ) {
		pNewHandlers = xrtCalloc(XUI_WIDGET_EVENT_SLOT_COUNT, sizeof(*pNewHandlers));
		if ( pNewHandlers == NULL ) { xuiInternalCallScopeRecordOutcome(&tScope,XUI_ERROR_OUT_OF_MEMORY,xrtGetError()); xrtFree(pNew); return xuiCallScopeEnd(&tScope); }
	}
    bool bHeld=false;iResult=xuiInternalCallbackHoldFresh(pOwner,&bHeld);
    if(iResult!=XUI_OK && !bHeld){xuiInternalCallScopeRecordOutcome(&tScope,iResult,xrtGetError());xrtFree(pNewHandlers);xrtFree(pNew);return xuiCallScopeEnd(&tScope);}
    if(iResult!=XUI_OK || !__xuiWidgetValid(pWidget) || pContext->bCloseRequested || pContext->bDestroyPending || pContext->bDestroying){
        if(iResult==XUI_OK)iResult=XUI_ERROR_INVALID_STATE;
        xuiInternalCallScopeRecordOutcome(&tScope,iResult,xrtGetError());
		xrtFree(pNewHandlers); xrtFree(pNew);
		(void)xuiInternalCallbackOwnerDrop(pContext, pOwner);
        return xuiCallScopeEnd(&tScope);
	}
	/* A nested registration may have installed either allocation. Never
	 * overwrite it with a stale pre-Hold candidate; replace its actual slot
	 * and release the nested owner after the commit, exactly once. */
	if ( pWidget->pEventOwners != NULL ) {
		xrtFree(pNew); pNew = NULL;
		if ( pWidget->pEventOwners->pHandlers != NULL ) {
			xrtFree(pNewHandlers); pNewHandlers = NULL;
		}
	}
	xrtownershipscope tMutation = {0};
	if ( !xrtOwnershipMutationBegin(&tMutation) ) {
        xuiInternalCallScopeRecordOutcome(&tScope,XUI_ERROR_INVALID_STATE,xrtGetError());
		xrtFree(pNewHandlers); xrtFree(pNew); (void)xuiInternalCallbackOwnerDrop(pContext, pOwner);
        return xuiCallScopeEnd(&tScope);
	}
	if ( pNew != NULL ) pWidget->pEventOwners = pNew;
	if ( pNewHandlers != NULL ) pWidget->pEventOwners->pHandlers = pNewHandlers;
	xui_callback_owner_t tOld = {0};
	xui_callback_owner_t* pSlot = pWidget->pEventOwners == NULL ? NULL :
		(iEventType == -1 ? &pWidget->pEventOwners->tPrimary :
		 pWidget->pEventOwners->pHandlers == NULL ? NULL : &pWidget->pEventOwners->pHandlers[iEventType]);
	if ( pSlot != NULL ) { tOld = *pSlot; *pSlot = pOwner == NULL ? (xui_callback_owner_t){0} : *pOwner; }
	if ( iEventType == -1 ) { pWidget->onEvent = onEvent; pWidget->pEventUser = pUser; }
	else {
		uint64_t iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
		pWidget->arrEventHandlers[iEventType] = onEvent;
		pWidget->arrEventUsers[iEventType] = pUser;
		uint64_t iMask = __xuiWidgetEventTypeMask(iEventType);
		if ( onEvent != NULL ) pWidget->iEventHandlerMask |= iMask;
		else pWidget->iEventHandlerMask &= ~iMask;
		__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	}
	if ( !xrtOwnershipScopeEnd(&tMutation) ) abort();
    (void)xuiInternalCallbackOwnerDrop(pContext, &tOld);
    return xuiCallScopeEnd(&tScope);
}

XUI_API int xuiWidgetSetEventCallbackOwned(xui_widget pWidget, xui_widget_event_proc onEvent, void* pUser,
	const xui_callback_owner_t* pOwner)
{ return __xuiWidgetSetOwnedEvent(pWidget, -1, onEvent, pUser, pOwner); }

XUI_API int xuiWidgetSetEventHandlerOwned(xui_widget pWidget, int iEventType, xui_widget_event_proc onEvent, void* pUser,
	const xui_callback_owner_t* pOwner)
{
	if ( iEventType < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiWidgetSetOwnedEvent(pWidget, iEventType, onEvent, pUser, pOwner);
}

static int __xuiWidgetDestroyReadySelf(xui_widget widget)
{
    if(!__xuiWidgetMemoryValid(widget))return XUI_ERROR_INVALID_ARGUMENT;
    if(widget->iDestroyVisitEpoch&XUI_DESTROY_PREPARING)return XUI_ERROR_BUSY;
    if(widget->bCacheMutationActive)return XUI_ERROR_BUSY;
    if(widget->pActiveUpdateDraw && widget->pContext->pOwnedProxy &&
       xuiProxyResourceOwnershipBusy(xuiDrawOwnership(widget->pActiveUpdateDraw)))return XUI_ERROR_BUSY;
    for(xui_widget_type type=widget->pInitializedType;type;type=type->pParent){
        if(type->onDestroyReady){int status=type->onDestroyReady(widget,widget->pTypeData);if(status!=XUI_OK)return status;}
    }
    return XUI_OK;
}
static int __xuiWidgetDestroyReadyVisit(xui_widget widget,uint32_t epoch)
{
    if(__xuiWidgetVisitSeen(widget,epoch))return XUI_OK;
    __xuiWidgetVisitMark(widget,epoch);
    int status=__xuiWidgetDestroyReadySelf(widget);if(status!=XUI_OK)return status;
    for(xui_widget child=widget->pFirstChild;child;child=child->pNextSibling){
        status=__xuiWidgetDestroyReadyVisit(child,epoch);if(status!=XUI_OK)return status;
    }
    for(xui_widget part=widget->pFirstOwnedPart;part;part=part->pOwnedPartNext){
        status=__xuiWidgetDestroyReadyVisit(part,epoch);if(status!=XUI_OK)return status;
    }
    return XUI_OK;
}
int xuiInternalWidgetDestroyReady(xui_widget widget)
{
    if(!__xuiWidgetMemoryValid(widget))return XUI_ERROR_INVALID_ARGUMENT;
    return __xuiWidgetDestroyReadyVisit(widget,__xuiWidgetVisitBegin(widget->pContext));
}
static int __xuiWidgetDestroyPrepareSelf(xui_widget widget,bool* complete)
{
    *complete=false;
    if(!__xuiWidgetMemoryValid(widget))return XUI_ERROR_INVALID_ARGUMENT;
    if(widget->bCacheMutationActive)return XUI_STATUS_DEFERRED;
    bool all=true;int result=XUI_OK;xerror* first=NULL;
    bool drawWaiting=widget->pActiveUpdateDraw && widget->pContext->pOwnedProxy &&
        xuiProxyResourceOwnershipBusy(xuiDrawOwnership(widget->pActiveUpdateDraw));
    if(!drawWaiting){result=__xuiWidgetEndActiveUpdate(widget);first=xrtTakeError();}
    if(widget->pActiveUpdateDraw)all=false;
    else {
        int caches=__xuiWidgetDestroyAllCaches(widget);xerror* error=xrtTakeError();
        if(caches<0 || error){
            if(result>=0){result=caches<0 ? caches:XUI_ERROR_CALLBACK_FAILED;first=error;}else xrtErrorFree(error);
        }
        if(widget->pCacheSlots)all=false;
    }
    if(result>=0 && first)result=XUI_ERROR_CALLBACK_FAILED;
    if(result>=0)result=XUI_OK;
    for(xui_widget_type type=widget->pInitializedType;type;type=type->pParent){
        /* Pure readiness refuses only this layer's cache. Another layer may
         * own the topmost session which makes the first one ready next pass. */
        if(!type->onDestroyReady && !type->onDestroyPrepare)continue;
        xui_internal_destroy_scope_t mode={0};
        xuiInternalDestroyScopeBegin(widget->pContext,xuiInternalDestroyModeInherited(widget->pContext),&mode);
        if(type->onDestroyReady && type->onDestroyReady(widget,widget->pTypeData)!=XUI_OK){
            xuiInternalDestroyScopeEnd(&mode);all=false;continue;
        }
        if(!type->onDestroyPrepare){xuiInternalDestroyScopeEnd(&mode);continue;}
        bool done=false;int next=type->onDestroyPrepare(widget,widget->pTypeData,&done);
        xuiInternalDestroyScopeEnd(&mode);
        xerror* error=xrtTakeError();
        if(next<0 || error){
            if(next>=0)next=XUI_ERROR_CALLBACK_FAILED;
            if(result==XUI_OK){result=next;first=error;}else xrtErrorFree(error);
            xuiInternalContextRecordOutcome(widget->pContext,next,first);
        }else if((next!=XUI_OK && next!=XUI_STATUS_DEFERRED) || (next==XUI_STATUS_DEFERRED && done)){
            if(result==XUI_OK)result=XUI_ERROR_INVALID_STATE;
            xuiInternalContextRecordOutcome(widget->pContext,XUI_ERROR_INVALID_STATE,first);
        }
        if(!done)all=false;
    }
    *complete=all;xrtSetErrorTake(first);return all || result<0 ? result:XUI_STATUS_DEFERRED;
}
static int __xuiWidgetDestroyPrepareVisit(xui_widget widget,uint32_t epoch,bool* complete)
{
    /* The native-owner/visual union may contain a Menu-like cycle. A recursive
     * edge observes this same in-progress frame; the frame itself contributes
     * its own completion. Preserve the marker across reentrant pure queries. */
    if(__xuiWidgetVisitSeen(widget,epoch) || (widget->iDestroyVisitEpoch&XUI_DESTROY_PREPARING)){
        *complete=true;return XUI_OK;
    }
    __xuiWidgetVisitMark(widget,epoch);widget->iDestroyVisitEpoch|=XUI_DESTROY_PREPARING;
    bool all=false;int result=__xuiWidgetDestroyPrepareSelf(widget,&all);xerror* first=xrtTakeError();
    for(int relation=0;relation<2;++relation){
        for(xui_widget value=relation ? widget->pFirstOwnedPart:widget->pFirstChild;value;
            value=relation ? value->pOwnedPartNext:value->pNextSibling){
            bool done=false;int status=__xuiWidgetDestroyPrepareVisit(value,epoch,&done);xerror* error=xrtTakeError();
            if(status<0 || error){
                if(status>=0)status=XUI_ERROR_CALLBACK_FAILED;
                if(result>=0){result=status;first=error;}else xrtErrorFree(error);
            }
            if(!done)all=false;
        }
    }
    widget->iDestroyVisitEpoch&=~XUI_DESTROY_PREPARING;__xuiWidgetVisitMark(widget,epoch);
    *complete=all;xrtSetErrorTake(first);return all || result<0 ? (result<0 ? result:XUI_OK):XUI_STATUS_DEFERRED;
}
static int __xuiWidgetDestroyPrepareClosure(xui_widget widget,bool* complete)
{
    *complete=false;
    return __xuiWidgetDestroyPrepareVisit(widget,__xuiWidgetVisitBegin(widget->pContext),complete);
}
int xuiInternalContextPrepareDestroy(xui_context context,bool* complete)
{
    if(!complete)return XUI_ERROR_INVALID_ARGUMENT;
    *complete=false;
    if(!xuiInternalContextIsValid(context))return XUI_ERROR_INVALID_ARGUMENT;
    int result=XUI_OK;xerror* first=xrtTakeError();
    if(first)result=XUI_ERROR_CALLBACK_FAILED;
    size_t previous=SIZE_MAX;
    for(;;){
        size_t remaining=0;
        bool done=false;int status=xuiInternalTextMeasureCachePrepare(context,&done);
        xerror* error=xrtTakeError();
        if(status<0 || error){
            if(result==XUI_OK){result=status<0 ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}else xrtErrorFree(error);
            xuiInternalContextRecordOutcome(context,status<0 ? status:XUI_ERROR_CALLBACK_FAILED,first);
        }
        if(!done)++remaining;
        done=false;status=xuiInternalContextPrepareManagers(context,&done);error=xrtTakeError();
        if(status<0 || error){
            if(result==XUI_OK){result=status<0 ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}else xrtErrorFree(error);
            xuiInternalContextRecordOutcome(context,status<0 ? status:XUI_ERROR_CALLBACK_FAILED,first);
        }
        if(!done)++remaining;
        /* A deferred earlier widget must not prevent a later topmost Draw or
         * independent cache from making actual progress in this pass. */
        for(xui_widget widget=context->pAllWidgets;widget;widget=widget->pContextNext){
            done=false;status=__xuiWidgetDestroyPrepareSelf(widget,&done);error=xrtTakeError();
            if(status<0 || error){
                if(result==XUI_OK){result=status<0 ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}else xrtErrorFree(error);
            }
            if(!done)++remaining;
        }
        if(!remaining){*complete=true;break;}
        if(remaining>=previous)break;
        previous=remaining;
    }
    xrtSetErrorTake(first);return *complete || result<0 ? result:XUI_STATUS_DEFERRED;
}
XUI_API bool xuiContextOwnershipBusy(xui_context pContext)
{
    if(!xuiInternalContextIsValid(pContext) || pContext->iOperationDepth!=0 ||
        pContext->iWidgetCallbackDepth!=0 || pContext->iDestroyFlushDepth!=0 ||
        pContext->bDestroyPending || pContext->bDestroying)return true;
    if(pContext->pOwnedProxy && !xuiInternalProxyCreator(pContext->pOwnedProxy))return true;
    if(xuiInternalTextMeasureCacheReady(pContext)!=XUI_OK)return true;
    if(!xuiInternalContextManagersReady(pContext))return true;
    /* Each actual widget is inspected once, including unattached/pending
     * instances still physically owned by this Context. No subtree repeats. */
    for(xui_widget widget=pContext->pAllWidgets;widget;widget=widget->pContextNext)
        if(__xuiWidgetDestroyReadySelf(widget)!=XUI_OK)return true;
    return false;
}

XUI_API bool xuiContextTraceEventOwners(xui_context pContext, xrtownershipvisitor onVisit, void* pUser)
{
	if ( onVisit == NULL || xuiContextOwnershipBusy(pContext) ) return false;
	for ( xui_widget pWidget = pContext->pAllWidgets; pWidget != NULL; pWidget = pWidget->pContextNext ) {
		const xui_event_owners_t* pOwners = pWidget->pEventOwners;
		if ( pOwners == NULL ) continue;
		if ( pOwners->tPrimary.tReference.Data != NULL && !onVisit(pOwners->tPrimary.tReference, pUser) ) return false;
		if ( pOwners->pHandlers != NULL )
			for ( int i = 0; i < XUI_WIDGET_EVENT_SLOT_COUNT; ++i )
				if ( pOwners->pHandlers[i].tReference.Data != NULL && !onVisit(pOwners->pHandlers[i].tReference, pUser) ) return false;
	}
	return true;
}

XUI_API bool xuiContextTraceOwnership(xui_context pContext, xrtownershipvisitor onVisit, void* pUser)
{
	if ( !xuiContextTraceEventOwners(pContext, onVisit, pUser) ) return false;
    if(!xuiInternalContextTraceResources(pContext,onVisit,pUser))return false;
    if(!xuiInternalContextTraceManagers(pContext,onVisit,pUser))return false;
	for ( xui_widget pWidget = pContext->pAllWidgets; pWidget != NULL; pWidget = pWidget->pContextNext ) {
        if(pWidget->pEventOwners) {
            for(const xui_callback_binding_t* binding=pWidget->pEventOwners->pBindings;binding;binding=binding->pNext) {
                if(!onVisit(binding->tOwner.tReference,pUser))return false;
            }
        }
        if(pContext->pOwnedProxy){
            for(int i=0;i<pWidget->iCacheCount;++i){
                xui_surface surface=pWidget->pCacheSlots[i].pSurface;
                if(surface && !onVisit(xuiSurfaceOwnership(surface),pUser))return false;
            }
            if(pWidget->pActiveUpdateDraw && !onVisit(xuiDrawOwnership(pWidget->pActiveUpdateDraw),pUser))return false;
        }
        if(pContext->pOwnedProxy && pWidget->bTypeInitialized){
            for(xui_widget_type type=pWidget->pType;type;type=type->pParent){
                xui_type_resource_view_t view;if(!__xuiTypeResourceView(pWidget,type,&view))return false;
                for(size_t i=0;i<view.count;++i){
                    bool surface=false;void* value=__xuiTypeResource(pWidget,&view,i,&surface);
                    if(value && !onVisit(surface ? xuiSurfaceOwnership(value):xuiFontOwnership(value),pUser))return false;
                }
            }
        }
		for ( xui_widget_type pType = pWidget->pInitializedType; pType != NULL; pType = pType->pParent )
			if ( pType->onTraceOwnership != NULL &&
			     !pType->onTraceOwnership(pWidget, pWidget->pTypeData, onVisit, pUser) ) return false;
    }
	return true;
}

XUI_API int xuiWidgetGetEventCallback(xui_widget pWidget, xui_widget_event_proc* pEvent, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pEvent != NULL ) {
		*pEvent = pWidget->onEvent;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pEventUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetCursorQueryCallbackOwned(xui_widget pWidget,xui_widget_cursor_proc onQueryCursor,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onQueryCursor,onQueryCursor!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onQueryCursor=onQueryCursor;pWidget->pQueryCursorUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=XUI_OK;
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetCursorQueryCallback(xui_widget pWidget, xui_widget_cursor_proc onQueryCursor, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}

    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onQueryCursor))return xuiWidgetSetCursorQueryCallbackOwned(pWidget,onQueryCursor,pUser,NULL);
	pWidget->onQueryCursor = onQueryCursor;
	pWidget->pQueryCursorUser = pUser;
	return XUI_OK;
}

XUI_API int xuiWidgetGetCursorQueryCallback(xui_widget pWidget, xui_widget_cursor_proc* pQueryCursor, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pQueryCursor != NULL ) {
		*pQueryCursor = pWidget->onQueryCursor;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pQueryCursorUser;
	}
	return XUI_OK;
}

static int __xuiQueryCursorOperation(xui_context pContext, int iX, int iY)
{
	xui_widget pWidget;
	int iCursor;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_CURSOR_ARROW;
	}
	pWidget = (pContext->pPointerCaptureWidget != NULL) ? pContext->pPointerCaptureWidget :
		((pContext->pActiveWidget != NULL) ? pContext->pActiveWidget :
		 xuiHitTest(pContext, iX, iY, XUI_WIDGET_HIT_DEFAULT));
	for ( ; pWidget != NULL; pWidget = pWidget->pParent ) {
		if ( pWidget->onQueryCursor == NULL ) {
			continue;
		}
		iCursor = xuiInternalInvokeCursorQueryCallback(pWidget,iX,iY);
		if(!__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pContext))return XUI_CURSOR_ARROW;
		if ( (iCursor >= XUI_CURSOR_ARROW) && (iCursor <= XUI_CURSOR_NOT_ALLOWED) ) {
			return iCursor;
		}
	}
	return XUI_CURSOR_ARROW;
}

XUI_API int xuiQueryCursor(xui_context pContext, int iX, int iY)
{
	int iCursor;
	xuiInternalOperationEnter(pContext);
	iCursor = __xuiQueryCursorOperation(pContext, iX, iY);
	xuiInternalOperationLeave(pContext);
	return iCursor;
}

XUI_API int xuiWidgetSetEventHandler(xui_widget pWidget, int iEventType, xui_widget_event_proc onEvent, void* pUser)
{
	uint64_t iMask;
	uint64_t iOldOwnMask;

	if ( !__xuiWidgetValid(pWidget) ||
	     (iEventType < 0) ||
	     (iEventType >= XUI_WIDGET_EVENT_SLOT_COUNT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->pEventOwners != NULL ) return xuiWidgetSetEventHandlerOwned(pWidget, iEventType, onEvent, pUser, NULL);
	iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
	pWidget->arrEventHandlers[iEventType] = onEvent;
	pWidget->arrEventUsers[iEventType] = pUser;
	iMask = __xuiWidgetEventTypeMask(iEventType);
	if ( iMask != 0ull ) {
		if ( onEvent != NULL ) {
			pWidget->iEventHandlerMask |= iMask;
		} else {
			pWidget->iEventHandlerMask &= ~iMask;
		}
		__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	}
	return XUI_OK;
}

XUI_API int xuiWidgetGetEventHandler(xui_widget pWidget, int iEventType, xui_widget_event_proc* pEvent, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ||
	     (iEventType < 0) ||
	     (iEventType >= XUI_WIDGET_EVENT_SLOT_COUNT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pEvent != NULL ) {
		*pEvent = pWidget->arrEventHandlers[iEventType];
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->arrEventUsers[iEventType];
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetEventInterest(xui_widget pWidget, uint64_t iEventMask, int bEnabled)
{
	uint64_t iOldOwnMask;
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldOwnMask = __xuiWidgetOwnEventMask(pWidget);
	if ( bEnabled ) {
		pWidget->iEventInterestMask |= iEventMask;
	} else {
		pWidget->iEventInterestMask &= ~iEventMask;
	}
	__xuiWidgetApplyOwnEventMaskChange(pWidget, iOldOwnMask);
	return XUI_OK;
}

XUI_API uint64_t xuiWidgetGetEventMask(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? __xuiWidgetOwnEventMask(pWidget) : 0ull;
}

XUI_API uint64_t xuiWidgetGetSubtreeEventMask(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iSubtreeEventMask : 0ull;
}

XUI_API xui_widget xuiOverlayRoot(xui_context pContext)
{
	xui_widget pRoot;
	xui_layout_t tLayout;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		return pContext->pOverlayRoot;
	}
	if ( __xuiWidgetCreateInternal(pContext, &g_xuiWidgetBaseType, &pRoot, NULL, NULL) != XUI_OK ) {
		return NULL;
	}
	tLayout = pRoot->tLayout;
	tLayout.iLayoutType = XUI_LAYOUT_OVERLAY;
	tLayout.iWidthMode = XUI_SIZE_FILL;
	tLayout.iHeightMode = XUI_SIZE_FILL;
	tLayout.iLayer = XUI_LAYER_FLOATING;
	pRoot->tLayout = tLayout;
	pRoot->bFocusable = 0;
	pRoot->bTabStop = 0;
	pContext->pOverlayRoot = pRoot;
	(void)xuiInternalContextInvalidateAll(pContext);
	return pRoot;
}

int xuiInternalWidgetSetTreeLayer(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pChild;
	int iRet;

	if ( pWidget == NULL ) return XUI_OK;
	if ( !__xuiWidgetValid(pWidget) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiWidgetSetLayer(pWidget, iLayer, iZIndex);
	if ( iRet != XUI_OK ) return iRet;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = xuiInternalWidgetSetTreeLayer(pChild, iLayer, iZIndex);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

int xuiInternalOverlayNextZ(xui_context pContext, int iLayer)
{
	xui_widget pRoot;
	xui_widget pChild;
	int iChildLayer;
	int iChildZ;
	int iNextZ = 0;

	if ( !xuiInternalContextIsValid(pContext) ) return 0;
	pRoot = pContext->pOverlayRoot;
	if ( pRoot == NULL ) return 0;
	for ( pChild = pRoot->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !xuiWidgetGetEffectiveVisible(pChild) ||
			xuiWidgetGetLayer(pChild, &iChildLayer, &iChildZ) != XUI_OK ||
			iChildLayer != iLayer ) continue;
		if ( iChildZ >= iNextZ ) {
			iNextZ = (iChildZ < INT_MAX - 1) ? (iChildZ + 1) : (INT_MAX - 1);
		}
	}
	return iNextZ;
}

static int __xuiOverlayAttachPart(xui_context pContext, xui_widget pOwner, xui_widget pOverlay, int iLayer, int iZIndex,const void* authority)
{
    if(__xuiWidgetValid(pOverlay) && pOverlay->pInternalOwner && pOverlay->pInternalOwner!=authority)return XUI_ERROR_INVALID_STATE;
	xui_widget pRoot;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     !__xuiWidgetValid(pOverlay) ||
	     (pOverlay->pContext != pContext) ||
	     (pOverlay == pContext->pRoot) ||
	     (pOverlay == pContext->pOverlayRoot) ||
	     ((pOwner != NULL) && (!__xuiWidgetValid(pOwner) || (pOwner->pContext != pContext))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pRoot = xuiOverlayRoot(pContext);
	if ( pRoot == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pOverlay->pParent != NULL ) {
		iRet = __xuiWidgetRemoveFromParentOperation(pOverlay,authority);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( iLayer <= XUI_LAYER_NORMAL ) {
		iLayer = XUI_LAYER_FLOATING;
	}
	pOverlay->pOverlayOwner = pOwner;
	iRet = xuiWidgetSetLayer(pOverlay, iLayer, iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInsertBeforeOperation(pRoot,pOverlay,NULL,authority);
}

XUI_API int xuiOverlayAttach(xui_context context,xui_widget owner,xui_widget overlay,int layer,int z)
{return __xuiOverlayAttachPart(context,owner,overlay,layer,z,NULL);}
int xuiInternalWidgetOverlayAttachPart(xui_widget owner,xui_widget overlayOwner,xui_widget part,int layer,int z)
{
    if(!__xuiWidgetValid(owner) || !__xuiWidgetValid(part) || part->pInternalOwner!=owner)return XUI_ERROR_INVALID_STATE;
    return __xuiOverlayAttachPart(owner->pContext,overlayOwner,part,layer,z,owner);
}

XUI_API int xuiOverlayDetach(xui_widget pOverlay)
{
    if(__xuiWidgetValid(pOverlay) && pOverlay->pInternalOwner)return XUI_ERROR_INVALID_STATE;
	if ( !__xuiWidgetValid(pOverlay) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pOverlay->pOverlayOwner = NULL;
	return xuiWidgetRemoveFromParent(pOverlay);
}

XUI_API int xuiOverlayBringToFront(xui_widget pOverlay)
{
	xui_widget pParent;
	int iRet;

	if ( !__xuiWidgetValid(pOverlay) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pParent = pOverlay->pParent;
	if ( pParent == NULL ) {
		return XUI_OK;
	}
	if ( pParent->pLastChild == pOverlay ) {
		return XUI_OK;
	}
	/* Tree order breaks ties within the existing layer/z group. */
	iRet = xuiInternalLayoutAttach(pParent, pOverlay, NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pOverlay->pPrevSibling != NULL ) {
		pOverlay->pPrevSibling->pNextSibling = pOverlay->pNextSibling;
	} else {
		pParent->pFirstChild = pOverlay->pNextSibling;
	}
	if ( pOverlay->pNextSibling != NULL ) {
		pOverlay->pNextSibling->pPrevSibling = pOverlay->pPrevSibling;
	}
	pOverlay->pPrevSibling = pParent->pLastChild;
	pOverlay->pNextSibling = NULL;
	pParent->pLastChild->pNextSibling = pOverlay;
	pParent->pLastChild = pOverlay;
	__xuiWidgetStackingDirty(pParent);
	__xuiWidgetBumpLayoutVersion(pParent);
	return xuiWidgetInvalidate(pParent,
		XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_widget xuiOverlayGetOwner(xui_widget pOverlay)
{
	return __xuiWidgetValid(pOverlay) ? pOverlay->pOverlayOwner : NULL;
}

XUI_API xui_widget xuiOverlayTop(xui_context pContext)
{
	const xui_widget* children;
	int count;
	int i;

	if ( !xuiInternalContextIsValid(pContext) || (pContext->pOverlayRoot == NULL) ) {
		return NULL;
	}
	if ( xuiInternalStackingChildren(pContext->pOverlayRoot, &children, &count) != XUI_OK ) return NULL;
	for ( i = count - 1; i >= 0; --i ) {
		if ( xuiWidgetGetEffectiveVisible(children[i]) ) return children[i];
	}
	return NULL;
}

static int __xuiTooltipDescEnabled(const xui_tooltip_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 0;
	}
	if ( pDesc->iType == XUI_TOOLTIP_TEXT ) {
		return (pDesc->sText != NULL) && (pDesc->sText[0] != '\0');
	}
	if ( pDesc->iType == XUI_TOOLTIP_CUSTOM ) {
		return (pDesc->onMeasure != NULL) && (pDesc->onPaint != NULL);
	}
	return 0;
}

static void __xuiTooltipNormalizeDesc(xui_tooltip_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return;
	}
	pDesc->iSize = sizeof(*pDesc);
	if ( !__xuiTooltipTypeValid(pDesc->iType) ) {
		pDesc->iType = XUI_TOOLTIP_NONE;
	}
	if ( !__xuiTooltipAnchorValid(pDesc->iAnchor) ) {
		pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_WIDGET_BOTTOM;
	}
	if ( !__xuiRectValid(pDesc->tAnchorRect) ) {
		memset(&pDesc->tAnchorRect, 0, sizeof(pDesc->tAnchorRect));
		pDesc->bCustomAnchorRect = 0;
	}
	if ( !__xuiFloatValid(pDesc->fOffsetX) ) {
		pDesc->fOffsetX = 0.0f;
	}
	if ( !__xuiFloatValid(pDesc->fOffsetY) ) {
		pDesc->fOffsetY = 6.0f;
	}
	if ( (pDesc->fDelay != pDesc->fDelay) || (pDesc->fDelay < -1.0f) ) {
		pDesc->fDelay = 0.35f;
	}
	if ( (pDesc->iType == XUI_TOOLTIP_TEXT) && ((pDesc->sText == NULL) || (pDesc->sText[0] == '\0')) ) {
		pDesc->iType = XUI_TOOLTIP_NONE;
	}
	if ( (pDesc->iType == XUI_TOOLTIP_CUSTOM) && ((pDesc->onMeasure == NULL) || (pDesc->onPaint == NULL)) ) {
		pDesc->iType = XUI_TOOLTIP_NONE;
	}
}

static int __xuiTooltipTextSame(const char* sLeft, const char* sRight)
{
	if ( sLeft == sRight ) {
		return 1;
	}
	if ( sLeft == NULL ) {
		sLeft = "";
	}
	if ( sRight == NULL ) {
		sRight = "";
	}
	return strcmp(sLeft, sRight) == 0;
}

static int __xuiTooltipDescSame(const xui_tooltip_desc_t* pLeft, const xui_tooltip_desc_t* pRight)
{
	if ( pLeft == pRight ) {
		return 1;
	}
	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return 0;
	}
	return (pLeft->iType == pRight->iType) &&
	       __xuiTooltipTextSame(pLeft->sText, pRight->sText) &&
	       (pLeft->iAnchor == pRight->iAnchor) &&
	       (pLeft->bCustomAnchorRect == pRight->bCustomAnchorRect) &&
	       (pLeft->tAnchorRect.fX == pRight->tAnchorRect.fX) &&
	       (pLeft->tAnchorRect.fY == pRight->tAnchorRect.fY) &&
	       (pLeft->tAnchorRect.fW == pRight->tAnchorRect.fW) &&
	       (pLeft->tAnchorRect.fH == pRight->tAnchorRect.fH) &&
	       (pLeft->fOffsetX == pRight->fOffsetX) &&
	       (pLeft->fOffsetY == pRight->fOffsetY) &&
	       (pLeft->fDelay == pRight->fDelay) &&
	       (pLeft->bFollowCursor == pRight->bFollowCursor) &&
	       (pLeft->onMeasure == pRight->onMeasure) &&
	       (pLeft->onPaint == pRight->onPaint) &&
	       (pLeft->pUser == pRight->pUser);
}

static void __xuiTooltipClearActiveDesc(xui_context pContext)
{
	if ( pContext == NULL ) {
		return;
	}
	if ( pContext->sActiveTooltipText != NULL ) {
		xrtFree(pContext->sActiveTooltipText);
		pContext->sActiveTooltipText = NULL;
	}
	__xuiTooltipDefaultDesc(&pContext->tActiveTooltip);
}

static int __xuiTooltipSetActiveDesc(xui_context pContext, const xui_tooltip_desc_t* pDesc)
{
	char* sText;

	if ( (pContext == NULL) || (pDesc == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sText = NULL;
	if ( (pDesc->iType == XUI_TOOLTIP_TEXT) && (pDesc->sText != NULL) ) {
		sText = __xuiStringDuplicate(pDesc->sText);
		if ( sText == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	__xuiTooltipClearActiveDesc(pContext);
	pContext->tActiveTooltip = *pDesc;
	pContext->tActiveTooltip.iSize = sizeof(pContext->tActiveTooltip);
	if ( pContext->tActiveTooltip.fDelay < 0.0f ) {
		pContext->tActiveTooltip.fDelay = pContext->tInteractionPolicy.fTooltipInitialDelay;
	}
	pContext->sActiveTooltipText = sText;
	if ( sText != NULL ) {
		pContext->tActiveTooltip.sText = sText;
	}
	return XUI_OK;
}

static void __xuiTooltipResolveDelay(xui_context pContext, xui_tooltip_desc_t* pDesc)
{
	if ( (pContext != NULL) && (pDesc != NULL) && (pDesc->fDelay < 0.0f) ) {
		pDesc->fDelay = pContext->tInteractionPolicy.fTooltipInitialDelay;
	}
}

static int __xuiTooltipResolveWidget(xui_context pContext, xui_widget pWidget, xui_tooltip_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 0;
	}
	__xuiTooltipDefaultDesc(pDesc);
	if ( !__xuiWidgetValid(pWidget) || (pWidget->pContext != pContext) ) {
		return 0;
	}
	if ( pWidget->onTooltipResolve != NULL ) {
		if ( pWidget->onTooltipResolve(pContext, pWidget, pDesc, pWidget->pTooltipUser) == 0 ) {
			__xuiTooltipDefaultDesc(pDesc);
			return 0;
		}
		__xuiTooltipNormalizeDesc(pDesc);
		__xuiTooltipResolveDelay(pContext, pDesc);
		if ( __xuiTooltipDescEnabled(pDesc) ) {
			return 1;
		}
		__xuiTooltipDefaultDesc(pDesc);
		return 0;
	}
	*pDesc = pWidget->tTooltip;
	__xuiTooltipNormalizeDesc(pDesc);
	__xuiTooltipResolveDelay(pContext, pDesc);
	if ( __xuiTooltipDescEnabled(pDesc) ) {
		return 1;
	}
	__xuiTooltipDefaultDesc(pDesc);
	return 0;
}

static int __xuiWidgetContains(xui_widget pRoot, xui_widget pWidget)
{
	xui_widget pScan;

	if ( (pRoot == NULL) || (pWidget == NULL) ) {
		return 0;
	}
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan == pRoot ) {
			return 1;
		}
	}
	return 0;
}

static xui_widget __xuiTooltipFindOwner(xui_context pContext, xui_widget pWidget, xui_tooltip_desc_t* pDesc)
{
	xui_tooltip_desc_t tDesc;

	for ( ; pWidget != NULL; pWidget = pWidget->pParent ) {
		if ( __xuiTooltipResolveWidget(pContext, pWidget, &tDesc) ) {
			if ( pDesc != NULL ) {
				*pDesc = tDesc;
			}
			return pWidget;
		}
	}
	if ( pDesc != NULL ) {
		__xuiTooltipDefaultDesc(pDesc);
	}
	return NULL;
}

static float __xuiTooltipClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) {
		return fMin;
	}
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static xui_vec2_t __xuiTooltipMeasure(xui_context pContext, xui_widget pOwner, const xui_tooltip_desc_t* pDesc)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;
	xui_font pFont;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( (pContext == NULL) || (pDesc == NULL) ) {
		return tSize;
	}
	if ( pDesc->iType == XUI_TOOLTIP_CUSTOM ) {
		tSize = pDesc->onMeasure(pContext, pOwner, pDesc->pUser);
		if ( !__xuiWidgetSizeValid(tSize) ) {
			tSize.fX = 0.0f;
			tSize.fY = 0.0f;
		}
	} else if ( pDesc->iType == XUI_TOOLTIP_TEXT ) {
		pProxy = xuiInternalContextGetProxy(pContext);
		pFont = xuiGetDefaultFont(pContext);
		if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) && (pDesc->sText != NULL) ) {
			if ( pProxy->textMeasure(pProxy, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pFont, .sText=pDesc->sText, .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT}, &tSize) != XUI_OK ) {
				tSize.fX = 0.0f;
				tSize.fY = 0.0f;
			}
		}
		if ( tSize.fX <= 0.0f ) {
			tSize.fX = (float)((pDesc->sText != NULL) ? strlen(pDesc->sText) : 0u) * 7.0f;
		}
		if ( tSize.fY <= 0.0f ) {
			tSize.fY = 16.0f;
		}
		tSize.fX += 12.0f;
		tSize.fY += 8.0f;
	}
	if ( tSize.fX < 12.0f ) {
		tSize.fX = 12.0f;
	}
	if ( tSize.fY < 18.0f ) {
		tSize.fY = 18.0f;
	}
	tSize.fX = xuiInternalSnapSize(tSize.fX);
	tSize.fY = xuiInternalSnapSize(tSize.fY);
	return tSize;
}

static xui_rect_t __xuiTooltipResolveRect(xui_context pContext, xui_widget pOwner, const xui_tooltip_desc_t* pDesc, xui_vec2_t tSize)
{
	xui_rect_t tRect;
	xui_rect_t tRoot;
	xui_rect_t tOwner;
	float fMargin;

	memset(&tRect, 0, sizeof(tRect));
	if ( (pContext == NULL) || !__xuiWidgetValid(pOwner) || (pDesc == NULL) ) {
		return tRect;
	}
	if ( pContext->pRoot != NULL ) {
		tRoot = xuiWidgetGetWorldRect(pContext->pRoot);
	} else {
		tRoot.fX = 0.0f;
		tRoot.fY = 0.0f;
		tRoot.fW = pContext->fViewportWidth;
		tRoot.fH = pContext->fViewportHeight;
	}
	tOwner = (pDesc->bCustomAnchorRect != 0) ? pDesc->tAnchorRect : xuiWidgetGetWorldRect(pOwner);
	tRect.fW = tSize.fX;
	tRect.fH = tSize.fY;
	fMargin = 2.0f;
	switch ( pDesc->iAnchor ) {
	case XUI_TOOLTIP_ANCHOR_CURSOR:
		tRect.fX = pContext->fTooltipMouseX + pDesc->fOffsetX;
		tRect.fY = pContext->fTooltipMouseY + pDesc->fOffsetY;
		break;
	case XUI_TOOLTIP_ANCHOR_WIDGET_TOP:
		tRect.fX = tOwner.fX + pDesc->fOffsetX;
		tRect.fY = tOwner.fY - tRect.fH - pDesc->fOffsetY;
		break;
	case XUI_TOOLTIP_ANCHOR_WIDGET_RIGHT:
		tRect.fX = tOwner.fX + tOwner.fW + pDesc->fOffsetX;
		tRect.fY = tOwner.fY + pDesc->fOffsetY;
		break;
	case XUI_TOOLTIP_ANCHOR_WIDGET_LEFT:
		tRect.fX = tOwner.fX - tRect.fW - pDesc->fOffsetX;
		tRect.fY = tOwner.fY + pDesc->fOffsetY;
		break;
	case XUI_TOOLTIP_ANCHOR_WIDGET_BOTTOM:
	default:
		tRect.fX = tOwner.fX + pDesc->fOffsetX;
		tRect.fY = tOwner.fY + tOwner.fH + pDesc->fOffsetY;
		break;
	}
	if ( (pDesc->iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_BOTTOM) && (tRect.fY + tRect.fH > tRoot.fY + tRoot.fH - fMargin) ) {
		tRect.fY = tOwner.fY - tRect.fH - pDesc->fOffsetY;
	} else if ( (pDesc->iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_TOP) && (tRect.fY < tRoot.fY + fMargin) ) {
		tRect.fY = tOwner.fY + tOwner.fH + pDesc->fOffsetY;
	}
	if ( (pDesc->iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_RIGHT) && (tRect.fX + tRect.fW > tRoot.fX + tRoot.fW - fMargin) ) {
		tRect.fX = tOwner.fX - tRect.fW - pDesc->fOffsetX;
	} else if ( (pDesc->iAnchor == XUI_TOOLTIP_ANCHOR_WIDGET_LEFT) && (tRect.fX < tRoot.fX + fMargin) ) {
		tRect.fX = tOwner.fX + tOwner.fW + pDesc->fOffsetX;
	}
	tRect.fX = __xuiTooltipClamp(tRect.fX, tRoot.fX + fMargin, tRoot.fX + tRoot.fW - tRect.fW - fMargin);
	tRect.fY = __xuiTooltipClamp(tRect.fY, tRoot.fY + fMargin, tRoot.fY + tRoot.fH - tRect.fH - fMargin);
	return xuiInternalSnapRect(tRect);
}

static void __xuiTooltipStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	if ( xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty) == XUI_OK &&
	     tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR ) *pColor = tProperty.tValue.iColor;
}

static int __xuiTooltipCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_chrome_style_t tChrome;
	xui_rect_t tRect;
	xui_rect_t tText;
	xui_font pFont;
	int iRet;

	(void)iStateId;
	pContext = (xui_context)pUser;
	if ( !xuiInternalContextIsValid(pContext) || (pDraw == NULL) || (pContext->bTooltipOpen == 0) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	memset(&tChrome, 0, sizeof(tChrome));
	tChrome.iSize = sizeof(tChrome);
	(void)xuiGetChromeStyle(pContext, &tChrome);
	__xuiTooltipStyleColor(pWidget, "tooltip.background.color", &tChrome.iTooltipColor);
	__xuiTooltipStyleColor(pWidget, "tooltip.border.color", &tChrome.iPopupBorderColor);
	__xuiTooltipStyleColor(pWidget, "tooltip.text.color", &tChrome.iTooltipTextColor);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect.fW = pContext->tTooltipRect.fW;
	tRect.fH = pContext->tTooltipRect.fH;
	iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, tChrome.iTooltipColor);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (tChrome.fBorderWidth > 0.0f) && ((tChrome.iPopupBorderColor & 0xffu) != 0u) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect, tChrome.fBorderWidth, tChrome.iPopupBorderColor);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( pContext->tActiveTooltip.iType == XUI_TOOLTIP_CUSTOM ) {
		return pContext->tActiveTooltip.onPaint(pContext, pContext->pTooltipOwner, pDraw, tRect, pContext->tActiveTooltip.pUser);
	}
	if ( pContext->tActiveTooltip.iType == XUI_TOOLTIP_TEXT ) {
		pFont = xuiGetDefaultFont(pContext);
		if ( pFont == NULL ) {
			return XUI_OK;
		}
		tText.fX = tRect.fX + 6.0f;
		tText.fY = tRect.fY + 3.0f;
		tText.fW = tRect.fW - 12.0f;
		tText.fH = tRect.fH - 6.0f;
		return pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pFont, .sText=pContext->tActiveTooltip.sText, .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tText, tChrome.iTooltipTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static int __xuiTooltipEnsurePopup(xui_context pContext)
{
	static const char* const arrColors[] = {
		"tooltip.background.color", "tooltip.border.color", "tooltip.text.color"
	};
	xui_widget pPopup;
	xui_widget_type pType;
	xui_widget_type_desc_t tType;
	xui_style_property_info_t tInfo;
	xui_cache_policy_t tPolicy;
	size_t i;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->pTooltipPopupWidget != NULL ) {
		return XUI_OK;
	}
	pType = xuiWidgetFindType(pContext, "tooltip");
	if ( pType == NULL ) {
		memset(&tType, 0, sizeof(tType));
		tType.iSize = sizeof(tType);
		tType.sName = "tooltip";
		iRet = xuiWidgetRegisterType(pContext, &pType, &tType);
		if ( iRet != XUI_OK ) return iRet;
		memset(&tInfo, 0, sizeof(tInfo));
		tInfo.iSize = sizeof(tInfo);
		tInfo.pWidgetType = pType;
		tInfo.iValueType = XUI_STYLE_VALUE_COLOR;
		tInfo.iDirtyFlags = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
		for ( i = 0; i < sizeof(arrColors) / sizeof(arrColors[0]); ++i ) {
			tInfo.sName = arrColors[i];
			iRet = xuiStyleRegisterProperty(pContext, &tInfo, NULL);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	iRet = xuiWidgetCreateTyped(pContext, pType, &pPopup, NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	(void)xuiWidgetSetCachePolicy(pPopup, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pPopup, __xuiTooltipCacheRender, pContext);
	(void)xuiWidgetSetVisible(pPopup, 0);
	(void)xuiWidgetSetEnabled(pPopup, 0);
	(void)xuiWidgetSetHitTestVisible(pPopup, 0);
	(void)xuiWidgetSetLayoutType(pPopup, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pPopup, XUI_FLOW_ABSOLUTE);
	pContext->pTooltipPopupWidget = pPopup;
	return XUI_OK;
}

static int __xuiTooltipOpen(xui_context pContext)
{
	xui_vec2_t tSize;
	xui_rect_t tRect;
	xui_widget pPopup;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (pContext->pTooltipOwner == NULL) ) {
		return XUI_OK;
	}
	if ( !__xuiTooltipDescEnabled(&pContext->tActiveTooltip) ||
	     !__xuiWidgetEffectiveVisible(pContext->pTooltipOwner) ||
	     !__xuiWidgetEffectiveEnabled(pContext->pTooltipOwner) ) {
		xuiInternalTooltipCancel(pContext);
		return XUI_OK;
	}
	iRet = __xuiTooltipEnsurePopup(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pPopup = pContext->pTooltipPopupWidget;
	tSize = __xuiTooltipMeasure(pContext, pContext->pTooltipOwner, &pContext->tActiveTooltip);
	tRect = __xuiTooltipResolveRect(pContext, pContext->pTooltipOwner, &pContext->tActiveTooltip, tSize);
	pContext->tTooltipRect = tRect;
	if ( pPopup->pParent == NULL ) {
		iRet = xuiOverlayAttach(pContext, pContext->pTooltipOwner, pPopup, XUI_LAYER_TOOLTIP, 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	} else {
		pPopup->pOverlayOwner = pContext->pTooltipOwner;
		iRet = xuiWidgetSetLayer(pPopup, XUI_LAYER_TOOLTIP, pPopup->tLayout.iZIndex);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iRet = xuiWidgetSetRect(pPopup, tRect);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiOverlayBringToFront(pPopup);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWidgetSetVisible(pPopup, 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pContext->bTooltipOpen = 1;
	return xuiWidgetInvalidate(pPopup, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTooltipClose(xui_context pContext)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->bTooltipOpen == 0) && (pContext->pTooltipPopupWidget == NULL) ) {
		return XUI_OK;
	}
	pContext->bTooltipOpen = 0;
	if ( pContext->pTooltipPopupWidget == NULL ) {
		return XUI_OK;
	}
	pContext->pTooltipPopupWidget->pOverlayOwner = NULL;
	iRet = xuiWidgetSetVisible(pContext->pTooltipPopupWidget, 0);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pContext->pTooltipPopupWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

void xuiInternalTooltipCancel(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return;
	}
	(void)__xuiTooltipClose(pContext);
	pContext->pTooltipOwner = NULL;
	pContext->fTooltipHoverTime = 0.0f;
	__xuiTooltipClearActiveDesc(pContext);
}

void xuiInternalTooltipDetachWidget(xui_context pContext, xui_widget pWidget)
{
	if ( !xuiInternalContextIsValid(pContext) || (pWidget == NULL) ) {
		return;
	}
	if ( __xuiWidgetContains(pWidget, pContext->pTooltipOwner) ) {
		xuiInternalTooltipCancel(pContext);
	}
}

int xuiInternalTooltipPointerMove(xui_context pContext, xui_widget pHitWidget)
{
	xui_tooltip_desc_t tDesc;
	xui_widget pOwner;
	int bWasOpen;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->fTooltipMouseX = pContext->fPointerX;
	pContext->fTooltipMouseY = pContext->fPointerY;
	if ( pContext->pPointerCaptureWidget != NULL ) {
		pOwner = NULL;
		__xuiTooltipDefaultDesc(&tDesc);
	} else {
		pOwner = __xuiTooltipFindOwner(pContext, pHitWidget, &tDesc);
	}
	if ( pOwner != pContext->pTooltipOwner ) {
		iRet = __xuiTooltipClose(pContext);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pContext->pTooltipOwner = pOwner;
		pContext->fTooltipHoverTime = 0.0f;
		if ( pOwner != NULL ) {
			iRet = __xuiTooltipSetActiveDesc(pContext, &tDesc);
			if ( iRet != XUI_OK ) {
				pContext->pTooltipOwner = NULL;
				__xuiTooltipClearActiveDesc(pContext);
				return iRet;
			}
		} else {
			__xuiTooltipClearActiveDesc(pContext);
		}
	} else if ( (pOwner != NULL) && !__xuiTooltipDescSame(&pContext->tActiveTooltip, &tDesc) ) {
		bWasOpen = pContext->bTooltipOpen;
		iRet = __xuiTooltipSetActiveDesc(pContext, &tDesc);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( bWasOpen ) {
			return __xuiTooltipOpen(pContext);
		} else {
			pContext->fTooltipHoverTime = 0.0f;
		}
	} else if ( (pContext->bTooltipOpen != 0) && (pContext->tActiveTooltip.bFollowCursor != 0) ) {
		return __xuiTooltipOpen(pContext);
	}
	return XUI_OK;
}

int xuiInternalTooltipUpdate(xui_context pContext, float fDelta)
{
	xui_tooltip_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !isfinite(fDelta) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta < 0.0f ) {
		fDelta = 0.0f;
	}
	if ( pContext->pTooltipOwner == NULL ) {
		return XUI_OK;
	}
	if ( pContext->pPointerCaptureWidget != NULL ) {
		xuiInternalTooltipCancel(pContext);
		return XUI_OK;
	}
	if ( !__xuiTooltipResolveWidget(pContext, pContext->pTooltipOwner, &tDesc) ||
	     !__xuiWidgetEffectiveVisible(pContext->pTooltipOwner) ||
	     !__xuiWidgetEffectiveEnabled(pContext->pTooltipOwner) ) {
		xuiInternalTooltipCancel(pContext);
		return XUI_OK;
	}
	if ( !__xuiTooltipDescSame(&pContext->tActiveTooltip, &tDesc) ) {
		iRet = __xuiTooltipSetActiveDesc(pContext, &tDesc);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pContext->bTooltipOpen != 0 ) {
			return __xuiTooltipOpen(pContext);
		}
		pContext->fTooltipHoverTime = 0.0f;
	}
	pContext->fTooltipHoverTime += fDelta;
	if ( (pContext->fTooltipHoverTime >= pContext->tActiveTooltip.fDelay) &&
	     (pContext->bTooltipOpen == 0) ) {
		return __xuiTooltipOpen(pContext);
	}
	return XUI_OK;
}

static void __xuiWidgetSyncInactiveTree(xui_widget pWidget)
{
	xui_widget pChild;
	xui_widget pNext;
	int iRet;
	if ( !__xuiWidgetValid(pWidget) || !pWidget->bSubtreeHasInactiveUpdate ) return;
	if ( pWidget->onUpdate != NULL &&
		(pWidget->pType->iFlags & XUI_WIDGET_TYPE_UPDATE_ON_INACTIVE) != 0 ) {
		pWidget->pContext->iWidgetCallbackDepth++;
		iRet = xuiInternalInvokeUpdateCallback(pWidget,0.0f);
		pWidget->pContext->iWidgetCallbackDepth--;
		if ( iRet != XUI_OK ) {
			xuiInternalReportError(pWidget->pContext, pWidget, iRet, XUI_ERROR_STAGE_UPDATE, 1,
				"widget.inactive", "The inactive widget sync failed and the remaining tree continued.");
		}
	}
	if ( !__xuiWidgetValid(pWidget) ) return;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pNext ) {
		pNext = pChild->pNextSibling;
		__xuiWidgetSyncInactiveTree(pChild);
	}
}

static void __xuiWidgetUpdateTree(xui_widget pWidget, float fDelta)
{
	xui_widget pChild;
	xui_widget pNext;
	int iRet;
	if ( !__xuiWidgetValid(pWidget) || !pWidget->bVisible || !pWidget->bSubtreeHasUpdate ) return;
	if ( pWidget->onUpdate != NULL ) {
		pWidget->pContext->iWidgetCallbackDepth++;
		iRet = xuiInternalInvokeUpdateCallback(pWidget,fDelta);
		pWidget->pContext->iWidgetCallbackDepth--;
		if ( iRet != XUI_OK ) {
			xuiInternalReportError(pWidget->pContext, pWidget, iRet, XUI_ERROR_STAGE_UPDATE, 1,
				"widget.update", "The widget update failed and the remaining update tree continued.");
		}
	}
	if ( !__xuiWidgetValid(pWidget) || !pWidget->bVisible ) return;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pNext ) {
		pNext = pChild->pNextSibling;
		__xuiWidgetUpdateTree(pChild, fDelta);
	}
}

static int __xuiUpdateOperation(xui_context pContext, float fDelta)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !isfinite(fDelta) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta < 0.0f ) {
		fDelta = 0.0f;
	}
	xuiInternalCaretBlinkUpdate(pContext, fDelta);
	iRet = xuiInternalContextPressUpdate(pContext, fDelta);
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, pContext->pContextPressWidget, iRet, XUI_ERROR_STAGE_INPUT, 1,
			"input.context_press", "Context-press dispatch failed and was cancelled.");
		xuiInternalContextPressCancel(pContext);
	}
	iRet = xuiInternalTooltipUpdate(pContext, fDelta);
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, pContext->pTooltipOwner, iRet, XUI_ERROR_STAGE_UPDATE, 1,
			"tooltip.update", "Tooltip update failed and the tooltip was closed.");
		xuiInternalTooltipCancel(pContext);
	}
	__xuiWidgetUpdateTree(pContext->pRoot, fDelta);
	__xuiWidgetUpdateTree(pContext->pOverlayRoot, fDelta);
	xuiInternalWidgetDestroyFlush(pContext);
	return XUI_OK;
}

XUI_API int xuiUpdate(xui_context pContext, float fDelta)
{
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiUpdateOperation(pContext, fDelta);
	if ( xuiInternalContextIsValid(pContext) && !xuiInternalContextDestroyPending(pContext) &&
		pContext->iOperationDepth == 1 ) {
		xuiInternalStateChangeFlush(pContext);
		xuiInternalAccessibilityFlush(pContext);
	}
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiWidgetSetTooltipText(xui_widget pWidget, const char* sText)
{
	xui_tooltip_desc_t tDesc;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTooltipDefaultDesc(&tDesc);
	if ( (sText != NULL) && (sText[0] != '\0') ) {
		tDesc.iType = XUI_TOOLTIP_TEXT;
		tDesc.sText = sText;
	}
	return xuiWidgetSetTooltip(pWidget, &tDesc);
}

XUI_API int xuiWidgetSetTooltip(xui_widget pWidget, const xui_tooltip_desc_t* pDesc)
{
	xui_tooltip_desc_t tDesc;
	char* sText;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDesc == NULL ) {
		return xuiWidgetClearTooltip(pWidget);
	}
	if ( !__xuiTooltipDescStructValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tDesc = *pDesc;
	__xuiTooltipNormalizeDesc(&tDesc);
	if ( !__xuiTooltipDescEnabled(&tDesc) ) {
		return xuiWidgetClearTooltip(pWidget);
	}
	sText = NULL;
	if ( (tDesc.iType == XUI_TOOLTIP_TEXT) && (tDesc.sText != NULL) ) {
		sText = __xuiStringDuplicate(tDesc.sText);
		if ( sText == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pWidget->sTooltipText != NULL ) {
		xrtFree(pWidget->sTooltipText);
	}
	pWidget->sTooltipText = sText;
	pWidget->tTooltip = tDesc;
	pWidget->tTooltip.iSize = sizeof(pWidget->tTooltip);
	if ( sText != NULL ) {
		pWidget->tTooltip.sText = sText;
	}
	pWidget->onTooltipResolve = NULL;
	pWidget->pTooltipUser = NULL;
	iRet = xuiWidgetSetEventInterest(pWidget, XUI_EVENT_MASK_TOOLTIP, 1);
	if ( (iRet == XUI_OK) && (pWidget->pContext != NULL) && (pWidget->pContext->pTooltipOwner == pWidget) ) {
		xuiInternalTooltipCancel(pWidget->pContext);
		iRet = xuiInternalTooltipPointerMove(pWidget->pContext, pWidget->pContext->pHoverWidget);
	}
	return iRet;
}

XUI_API int xuiWidgetSetTooltipResolver(xui_widget pWidget, xui_tooltip_resolve_proc onResolve, void* pUser)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->sTooltipText != NULL ) {
		xrtFree(pWidget->sTooltipText);
		pWidget->sTooltipText = NULL;
	}
	__xuiTooltipDefaultDesc(&pWidget->tTooltip);
	pWidget->onTooltipResolve = onResolve;
	pWidget->pTooltipUser = pUser;
	iRet = xuiWidgetSetEventInterest(pWidget, XUI_EVENT_MASK_TOOLTIP, onResolve != NULL);
	if ( (iRet == XUI_OK) && (pWidget->pContext != NULL) && (pWidget->pContext->pTooltipOwner == pWidget) ) {
		xuiInternalTooltipCancel(pWidget->pContext);
		iRet = xuiInternalTooltipPointerMove(pWidget->pContext, pWidget->pContext->pHoverWidget);
	}
	return iRet;
}

XUI_API int xuiWidgetClearTooltip(xui_widget pWidget)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->sTooltipText != NULL ) {
		xrtFree(pWidget->sTooltipText);
		pWidget->sTooltipText = NULL;
	}
	__xuiTooltipDefaultDesc(&pWidget->tTooltip);
	pWidget->onTooltipResolve = NULL;
	pWidget->pTooltipUser = NULL;
	iRet = xuiWidgetSetEventInterest(pWidget, XUI_EVENT_MASK_TOOLTIP, 0);
	if ( (pWidget->pContext != NULL) && (pWidget->pContext->pTooltipOwner == pWidget) ) {
		xuiInternalTooltipCancel(pWidget->pContext);
	}
	return iRet;
}

XUI_API const xui_tooltip_desc_t* xuiWidgetGetTooltip(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? &pWidget->tTooltip : NULL;
}

XUI_API int xuiWidgetTooltipIsOpen(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->bTooltipOpen : 0;
}

XUI_API xui_widget xuiWidgetTooltipGetOwner(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->pTooltipOwner : NULL;
}

XUI_API xui_rect_t xuiWidgetTooltipGetRect(xui_context pContext)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( xuiInternalContextIsValid(pContext) ) {
		tRect = pContext->tTooltipRect;
	}
	return tRect;
}

XUI_API int xuiWidgetInvalidate(xui_widget pWidget, uint32_t iFlags)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget), iFlags);
}

int xuiInternalWidgetInvalidatePaintIfNeeded(xui_widget pWidget)
{
	const uint32_t iFlags = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;

	if ( !__xuiWidgetValid(pWidget) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pWidget->iDirtyFlags & iFlags) == iFlags ) return XUI_OK;
	return __xuiWidgetInvalidateWorldRect(pWidget,
		xuiWidgetGetWorldRect(pWidget), iFlags);
}

int xuiInternalWidgetInvalidateArrange(xui_widget pWidget, uint32_t iPaintFlags)
{
	xui_widget pScan;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	xuiInternalLayoutInvalidate(pWidget, 0);
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		pScan->bArrangeValid = 0;
	}
	iPaintFlags &= ~(XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE);
	return __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget), iPaintFlags);
}

XUI_API int xuiWidgetInvalidateRect(xui_widget pWidget, xui_rect_t tRect, uint32_t iFlags)
{
	xui_rect_t tWorldRect;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValid(tRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorldRect = xuiWidgetGetWorldRect(pWidget);
	tWorldRect.fX += tRect.fX;
	tWorldRect.fY += tRect.fY;
	tWorldRect.fW = tRect.fW;
	tWorldRect.fH = tRect.fH;
	return __xuiWidgetInvalidateWorldRect(pWidget, tWorldRect, iFlags);
}

XUI_API uint32_t xuiWidgetGetDirtyFlags(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iDirtyFlags : 0;
}

XUI_API void xuiWidgetClearDirty(xui_widget pWidget, uint32_t iFlags)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return;
	}
	if ( iFlags == 0 ) {
		iFlags = XUI_WIDGET_DIRTY_ALL;
	}
	pWidget->iDirtyFlags &= ~iFlags;
	__xuiWidgetRepairDirtyToRoot(pWidget);
}

XUI_API int xuiWidgetSetStyleName(xui_widget pWidget, const char* sName)
{
	char* sCopy;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (sName != NULL) && (sName[0] == '\0') ) {
		sName = NULL;
	}
	if ( ((pWidget->sStyleName == NULL) && (sName == NULL)) ||
	     ((pWidget->sStyleName != NULL) && (sName != NULL) && __xuiStringEqual(pWidget->sStyleName, sName)) ) {
		return XUI_OK;
	}
	sCopy = NULL;
	if ( sName != NULL ) {
		sCopy = __xuiStringDuplicate(sName);
		if ( sCopy == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pWidget->sStyleName != NULL ) {
		xrtFree(pWidget->sStyleName);
	}
	pWidget->sStyleName = sCopy;
	iRet = xuiWidgetResolveStyle(pWidget);
	return iRet;
}

XUI_API const char* xuiWidgetGetStyleName(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->sStyleName : NULL;
}

XUI_API int xuiWidgetAddStyleClass(xui_widget pWidget, const char* sClass)
{
	char** pClasses;
	char* sCopy;
	int iCapacity;

	if ( !__xuiWidgetValid(pWidget) || (sClass == NULL) || (sClass[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiWidgetHasStyleClass(pWidget, sClass) ) {
		return XUI_OK;
	}
	if ( pWidget->iStyleClassCount >= pWidget->iStyleClassCapacity ) {
		iCapacity = (pWidget->iStyleClassCapacity > 0) ? (pWidget->iStyleClassCapacity * 2) : 4;
		pClasses = (char**)xrtRealloc(pWidget->pStyleClasses, sizeof(*pClasses) * (size_t)iCapacity);
		if ( pClasses == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pWidget->pStyleClasses = pClasses;
		pWidget->iStyleClassCapacity = iCapacity;
	}
	sCopy = __xuiStringDuplicate(sClass);
	if ( sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pWidget->pStyleClasses[pWidget->iStyleClassCount++] = sCopy;
	return xuiWidgetResolveStyle(pWidget);
}

XUI_API int xuiWidgetRemoveStyleClass(xui_widget pWidget, const char* sClass)
{
	int i;
	int iMoveCount;

	if ( !__xuiWidgetValid(pWidget) || (sClass == NULL) || (sClass[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pWidget->iStyleClassCount; i++ ) {
		if ( __xuiStringEqual(pWidget->pStyleClasses[i], sClass) ) {
			xrtFree(pWidget->pStyleClasses[i]);
			iMoveCount = pWidget->iStyleClassCount - i - 1;
			if ( iMoveCount > 0 ) {
				memmove(&pWidget->pStyleClasses[i], &pWidget->pStyleClasses[i + 1], sizeof(pWidget->pStyleClasses[0]) * (size_t)iMoveCount);
			}
			pWidget->iStyleClassCount--;
			return xuiWidgetResolveStyle(pWidget);
		}
	}
	return XUI_OK;
}

XUI_API void xuiWidgetClearStyleClasses(xui_widget pWidget)
{
	int i;

	if ( !__xuiWidgetValid(pWidget) ) {
		return;
	}
	for ( i = 0; i < pWidget->iStyleClassCount; i++ ) {
		xrtFree(pWidget->pStyleClasses[i]);
	}
	pWidget->iStyleClassCount = 0;
	(void)xuiWidgetResolveStyle(pWidget);
}

XUI_API int xuiWidgetHasStyleClass(xui_widget pWidget, const char* sClass)
{
	int i;

	if ( !__xuiWidgetValid(pWidget) || (sClass == NULL) || (sClass[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pWidget->iStyleClassCount; i++ ) {
		if ( __xuiStringEqual(pWidget->pStyleClasses[i], sClass) ) {
			return 1;
		}
	}
	return 0;
}

XUI_API int xuiWidgetGetStyleClassCount(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iStyleClassCount : 0;
}

XUI_API const char* xuiWidgetGetStyleClass(xui_widget pWidget, int iIndex)
{
	if ( !__xuiWidgetValid(pWidget) || (iIndex < 0) || (iIndex >= pWidget->iStyleClassCount) ) {
		return NULL;
	}
	return pWidget->pStyleClasses[iIndex];
}

XUI_API int xuiWidgetSetInlineStyle(xui_widget pWidget, const xui_style_property_t* pProperties, int iPropertyCount)
{
	xui_style_prop_t* pCopy;
	int iCopyCount;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (iPropertyCount < 0) ||
	     ((iPropertyCount > 0) && (pProperties == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pCopy = NULL;
	iCopyCount = 0;
	for ( iRet = 0; iRet < iPropertyCount; iRet++ ) {
		if ( !__xuiStylePropertyValid(&pProperties[iRet]) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = __xuiStylePropArrayCopy(pWidget->pContext, &pCopy, &iCopyCount, pProperties, iPropertyCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiStylePropArrayClear(pWidget->pInlineStyle, pWidget->iInlineStyleCount);
	pWidget->pInlineStyle = pCopy;
	pWidget->iInlineStyleCount = iCopyCount;
	return xuiWidgetResolveStyle(pWidget);
}

XUI_API int xuiWidgetGetInlineStyleProperty(xui_widget pWidget, const char* sName, xui_style_property_t* pProperty)
{
	const xui_style_prop_t* pProp;

	if ( !__xuiWidgetValid(pWidget) || (sName == NULL) || (sName[0] == '\0') || (pProperty == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProp = __xuiStyleFindPropConst(pWidget->pInlineStyle, pWidget->iInlineStyleCount, sName);
	if ( pProp == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	__xuiStylePropExport(pProp, pProperty);
	return XUI_OK;
}

XUI_API int xuiWidgetResolveStyle(xui_widget pWidget)
{
	xui_style_prop_t* pResolved;
	uint32_t iHash;
	uint32_t iDirtyFlags;
	int iResolvedCount;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pResolved = NULL;
	iResolvedCount = 0;
	iHash = 0;
	iRet = __xuiWidgetBuildResolvedStyle(pWidget, &pResolved, &iResolvedCount, &iHash);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iDirtyFlags = __xuiWidgetStyleDiffDirty(
		pWidget->pResolvedStyle,
		pWidget->iResolvedStyleCount,
		pResolved,
		iResolvedCount,
		pWidget->iResolvedStyleHash,
		iHash);
	__xuiStylePropArrayClear(pWidget->pResolvedStyle, pWidget->iResolvedStyleCount);
	pWidget->pResolvedStyle = pResolved;
	pWidget->iResolvedStyleCount = iResolvedCount;
	pWidget->iResolvedStyleHash = iHash;
	pWidget->iResolvedStyleGeneration = pWidget->pContext->iStyleGeneration;
	if ( iDirtyFlags != 0 ) {
		pWidget->iStyleVersion++;
		if ( pWidget->iStyleVersion == 0 ) {
			pWidget->iStyleVersion = 1;
		}
		return xuiWidgetInvalidate(pWidget, iDirtyFlags);
	}
	return XUI_OK;
}

XUI_API uint32_t xuiWidgetGetStyleGeneration(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iResolvedStyleGeneration : 0;
}

XUI_API uint32_t xuiWidgetGetStyleHash(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iResolvedStyleHash : 0;
}

XUI_API int xuiWidgetGetResolvedStyleProperty(xui_widget pWidget, const char* sName, xui_style_property_t* pProperty)
{
	const xui_style_prop_t* pProp;

	if ( !__xuiWidgetValid(pWidget) || (sName == NULL) || (sName[0] == '\0') || (pProperty == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProp = __xuiStyleFindPropConst(pWidget->pResolvedStyle, pWidget->iResolvedStyleCount, sName);
	if ( pProp == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	__xuiStylePropExport(pProp, pProperty);
	return XUI_OK;
}

XUI_API int xuiWidgetGetResolvedStylePropertyCount(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iResolvedStyleCount : 0;
}

XUI_API int xuiWidgetGetResolvedStylePropertyAt(xui_widget pWidget, int iIndex, xui_style_property_t* pProperty)
{
	if ( !__xuiWidgetValid(pWidget) || (iIndex < 0) || (iIndex >= pWidget->iResolvedStyleCount) || (pProperty == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiStylePropExport(&pWidget->pResolvedStyle[iIndex], pProperty);
	return XUI_OK;
}

XUI_API int xuiWidgetSetStateId(xui_widget pWidget, uint32_t iStateId)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->iStateId == iStateId ) {
		return XUI_OK;
	}
	pWidget->iStateId = iStateId;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiWidgetGetStateId(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iStateId : 0;
}

XUI_API int xuiWidgetSetCachePolicy(xui_widget pWidget, const xui_cache_policy_t* pPolicy)
{
	xui_cache_policy_t tPolicy;

	if ( !__xuiWidgetValid(pWidget) || !__xuiCachePolicyValid(pPolicy) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiCachePolicySupported(pPolicy->iPolicy) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	tPolicy = *pPolicy;
	if ( tPolicy.iSize == 0 ) {
		tPolicy.iSize = sizeof(tPolicy);
	}
	if ( (pWidget->tCachePolicy.iPolicy == tPolicy.iPolicy) &&
	     (pWidget->tCachePolicy.iFlags == tPolicy.iFlags) &&
	     (pWidget->tCachePolicy.iClearColor == tPolicy.iClearColor) ) {
		return XUI_OK;
	}
	pWidget->tCachePolicy = tPolicy;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_cache_policy_t xuiWidgetGetCachePolicy(xui_widget pWidget)
{
	xui_cache_policy_t tPolicy;

	memset(&tPolicy, 0, sizeof(tPolicy));
	if ( __xuiWidgetValid(pWidget) ) {
		tPolicy = pWidget->tCachePolicy;
	}
	return tPolicy;
}

XUI_API int xuiWidgetSetUpdateCallbackOwned(xui_widget pWidget,xui_widget_update_proc onUpdate,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onUpdate,onUpdate!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onUpdate=onUpdate;pWidget->pUpdateUser=pUser;
	__xuiWidgetRepairUpdateStateToRoot(pWidget);
    xuiInternalCallbackUpdatePublish(&update);
    result=XUI_OK;
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetUpdateCallback(xui_widget pWidget, xui_widget_update_proc onUpdate, void* pUser)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onUpdate))return xuiWidgetSetUpdateCallbackOwned(pWidget,onUpdate,pUser,NULL);
	pWidget->onUpdate=onUpdate;pWidget->pUpdateUser=pUser;
	__xuiWidgetRepairUpdateStateToRoot(pWidget);
    return XUI_OK;
}

XUI_API int xuiWidgetSetCacheRenderCallbackOwned(xui_widget pWidget,xui_widget_cache_render_proc onRender,void* pUser,const xui_callback_owner_t* pOwner)
{
    if(!__xuiWidgetValid(pWidget))return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_update_t update={0};
    int result=xuiInternalCallbackUpdateBegin(pWidget,&pWidget->onCacheRender,onRender!=NULL,pUser,pOwner,&update);
    if(result!=XUI_OK)return result;
    pWidget->onCacheRender=onRender;pWidget->pCacheRenderUser=pUser;
    xuiInternalCallbackUpdatePublish(&update);
    result=xuiWidgetInvalidate(pWidget,XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
    return xuiInternalCallbackUpdateEnd(&update,result);
}

XUI_API int xuiWidgetSetCacheRenderCallback(xui_widget pWidget, xui_widget_cache_render_proc onRender, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}

    if(xuiInternalCallbackSlotOwned(pWidget,&pWidget->onCacheRender))return xuiWidgetSetCacheRenderCallbackOwned(pWidget,onRender,pUser,NULL);
	if ( (pWidget->onCacheRender == onRender) && (pWidget->pCacheRenderUser == pUser) ) {
		return XUI_OK;
	}
	pWidget->onCacheRender = onRender;
	pWidget->pCacheRenderUser = pUser;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetCacheRenderCallback(xui_widget pWidget, xui_widget_cache_render_proc* pRender, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pRender != NULL ) {
		*pRender = pWidget->onCacheRender;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pCacheRenderUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetSetCacheStateCount(xui_widget widget,int count)
{
    if(!__xuiWidgetValid(widget) || count<0 || (size_t)count>SIZE_MAX/sizeof(xui_widget_cache_slot_t))return XUI_ERROR_INVALID_ARGUMENT;
    if(widget->bCacheMutationActive)return XUI_ERROR_BUSY;
    if(widget->pActiveUpdateDraw)return XUI_ERROR_ALREADY_INITIALIZED;
    if(count==widget->iCacheCount)return XUI_OK;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(widget->pContext,&scope);if(status!=XUI_OK)return status;
    widget->bCacheMutationActive=1;
    xui_widget_cache_slot_t* slots=count ? xrtCalloc((size_t)count,sizeof(*slots)):NULL;
    bool complete=true;
    if(count && !slots)status=XUI_ERROR_OUT_OF_MEMORY;
    if(status==XUI_OK){
        for(int i=count;i<widget->iCacheCount;++i){
            bool consumed=false;int next=__xuiWidgetReleaseCacheSlot(widget,&widget->pCacheSlots[i],&consumed);
            if(next!=XUI_OK){status=next;complete=false;break;}
            if(!consumed){status=XUI_ERROR_BUSY;complete=false;break;}
        }
    }
    if(status==XUI_OK && complete && __xuiWidgetValid(widget) && !widget->pContext->bCloseRequested){
        int copy=widget->iCacheCount<count ? widget->iCacheCount:count;
        for(int i=0;i<copy;++i){slots[i]=widget->pCacheSlots[i];memset(&widget->pCacheSlots[i],0,sizeof(widget->pCacheSlots[i]));}
        for(int i=copy;i<count;++i)slots[i].iStateId=__xuiWidgetNextFreeStateId(slots,i);
        xrtFree(widget->pCacheSlots);widget->pCacheSlots=slots;widget->iCacheCount=count;slots=NULL;
        status=xuiWidgetInvalidate(widget,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);
    }else if(status==XUI_OK)status=XUI_ERROR_INVALID_STATE;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    xrtFree(slots);widget->bCacheMutationActive=0;
    status=xuiInternalCallScopeEndOutcome(&scope,status,&first);xrtSetErrorTake(first);return status;
}

XUI_API int xuiWidgetGetCacheStateCount(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iCacheCount : 0;
}

XUI_API int xuiWidgetSetCacheStateId(xui_widget pWidget, int iIndex, uint32_t iStateId)
{
    if(__xuiWidgetMemoryValid(pWidget) && pWidget->bCacheMutationActive)return XUI_ERROR_BUSY;
	if ( !__xuiWidgetValid(pWidget) || (iIndex < 0) || (iIndex >= pWidget->iCacheCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->pActiveUpdateDraw != NULL ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	if ( (__xuiWidgetIndexOfState(pWidget, iStateId) >= 0) && (pWidget->pCacheSlots[iIndex].iStateId != iStateId) ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	if ( pWidget->pCacheSlots[iIndex].iStateId == iStateId ) {
		return XUI_OK;
	}
	pWidget->pCacheSlots[iIndex].iStateId = iStateId;
	pWidget->pCacheSlots[iIndex].iFlags |= XUI_WIDGET_DIRTY_CACHE;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiWidgetGetCacheStateId(xui_widget pWidget, int iIndex)
{
	if ( !__xuiWidgetValid(pWidget) || (iIndex < 0) || (iIndex >= pWidget->iCacheCount) ) {
		return 0;
	}
	return pWidget->pCacheSlots[iIndex].iStateId;
}

XUI_API xui_surface xuiWidgetGetCacheSurface(xui_widget pWidget, uint32_t iStateId)
{
	xui_widget_cache_slot_t* pSlot;

	if ( !__xuiWidgetValid(pWidget) || pWidget->bCacheMutationActive ) {
		return NULL;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, iStateId);
	return (pSlot != NULL) ? pSlot->pSurface : NULL;
}

static int __xuiWidgetEnsureCacheStateSlot(xui_widget pWidget, uint32_t iStateId);

static int __xuiWidgetUpdateBeginOperation(xui_widget pWidget, uint32_t iStateId, uint32_t iFlags, uint32_t iClearColor, xui_draw_context* ppDraw)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tClearRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (ppDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppDraw = NULL;
    if(pWidget->bCacheMutationActive || pWidget->pContext->bCloseRequested)return XUI_ERROR_BUSY;
	if ( pWidget->pActiveUpdateDraw != NULL ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = __xuiWidgetEnsureCacheStateSlot(pWidget, iStateId);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, iStateId);
	if ( pSlot == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pSlot->iContentPolicy != __xuiWidgetEffectiveCachePolicy(pWidget) ) {
		__xuiWidgetPurgeCacheSlotSurface(pWidget, pSlot);
	}
	pWidget->iActiveUpdateStateId = iStateId;
	iRet = __xuiWidgetEnsureCacheSurface(pWidget, pSlot);
	if ( iRet != XUI_OK ) {
		pWidget->iActiveUpdateStateId = 0;
		return iRet;
	}
    pWidget->bCacheMutationActive=1;
    iRet=pProxy->drawBegin(pProxy,&pWidget->pActiveUpdateDraw,pSlot->pSurface);
    pWidget->bCacheMutationActive=0;
    pWidget->pActiveUpdateSlot=pSlot;
    if(iRet==XUI_OK && (!__xuiWidgetValid(pWidget) || pWidget->pContext->bCloseRequested))iRet=XUI_ERROR_INVALID_STATE;
    if(iRet!=XUI_OK){
        xerror* first=xrtTakeError();xui_cleanup_prefix_t prefix={0};
        int begun=xuiInternalCleanupPrefixBegin(pWidget->pContext,&prefix,iRet,first);
        (void)__xuiWidgetEndActiveUpdate(pWidget);
        if(begun==XUI_OK)(void)xuiInternalCleanupPrefixEnd(&prefix);
        xrtClearError();xrtSetErrorTake(first);return iRet;
    }
    *ppDraw=pWidget->pActiveUpdateDraw;
	if ( (iFlags & XUI_WIDGET_UPDATE_CLEAR) != 0 ) {
		tClearRect.fX = 0.0f;
		tClearRect.fY = 0.0f;
		tClearRect.fW = (float)pSlot->iWidth;
		tClearRect.fH = (float)pSlot->iHeight;
        pWidget->bCacheMutationActive=1;
		iRet = pProxy->drawClearRect(pProxy, *ppDraw, tClearRect, iClearColor);
        pWidget->bCacheMutationActive=0;
        if(iRet==XUI_OK && (!__xuiWidgetValid(pWidget) || pWidget->pContext->bCloseRequested))iRet=XUI_ERROR_INVALID_STATE;
		if ( iRet != XUI_OK ) {
            xerror* first=xrtTakeError();xui_cleanup_prefix_t prefix={0};
            int begun=xuiInternalCleanupPrefixBegin(pWidget->pContext,&prefix,iRet,first);
            (void)__xuiWidgetEndActiveUpdate(pWidget);
            if(begun==XUI_OK)(void)xuiInternalCleanupPrefixEnd(&prefix);
            *ppDraw=NULL;xrtClearError();xrtSetErrorTake(first);return iRet;
		}
	}
	pWidget->pActiveUpdateDraw = *ppDraw;
	pWidget->pActiveUpdateSlot = pSlot;
	pSlot->iDpiGeneration = pWidget->pContext->iDpiGeneration;
	pSlot->iContentPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	return XUI_OK;
}

typedef struct xui_cache_output_watch_t {bool invalid;xui_draw_context* output;} xui_cache_output_watch_t;
static void __xuiWidgetCacheOutputInvalidated(void* raw)
{
    xui_cache_output_watch_t* watch=raw;watch->invalid=true;*watch->output=NULL;
}
XUI_API int xuiWidgetUpdateBegin(xui_widget widget,uint32_t state,uint32_t flags,uint32_t clear,xui_draw_context* output)
{
    if(output)*output=NULL;
    if(!__xuiWidgetValid(widget) || !output)return XUI_ERROR_INVALID_ARGUMENT;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(widget->pContext,&scope);if(status!=XUI_OK)return status;
    xui_lifetime_link_t link={0};xui_cache_output_watch_t watch={false,output};
    status=xuiWidgetLifetimeWatch(widget,&link,__xuiWidgetCacheOutputInvalidated,&watch);
    if(status==XUI_OK)status=__xuiWidgetUpdateBeginOperation(widget,state,flags,clear,output);
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    status=xuiInternalCallScopeEndOutcome(&scope,status,&first);
    xuiLifetimeUnwatch(&link);
    if(status==XUI_OK && watch.invalid){xrtErrorFree(first);first=NULL;status=XUI_ERROR_INVALID_STATE;}
    if(status!=XUI_OK)*output=NULL;
    xrtSetErrorTake(first);return status;
}

XUI_API int xuiWidgetUpdateEnd(xui_widget widget,uint32_t state,xui_draw_context draw)
{
    if(!__xuiWidgetMemoryValid(widget) || !draw || widget->pActiveUpdateDraw!=draw || widget->iActiveUpdateStateId!=state)return XUI_ERROR_INVALID_ARGUMENT;
    xui_context context=widget->pContext;
    if(context->pOwnedProxy && !xuiInternalProxyCreator(context->pOwnedProxy))return XUI_ERROR_BUSY;
    xui_call_scope_t scope={0};int status=xuiInternalCleanupCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;
    xui_widget_cache_slot_t* slot=widget->pActiveUpdateSlot;
    status=__xuiWidgetEndActiveUpdate(widget);
    if(status==XUI_OK && slot && __xuiWidgetValid(widget) && !context->bCloseRequested){
        slot->iFlags&=~XUI_WIDGET_DIRTY_CACHE;
        xui_proxy proxy=xuiInternalContextGetProxy(context);
        if(proxy && proxy->surfaceGetGeneration && slot->pSurface)(void)proxy->surfaceGetGeneration(proxy,slot->pSurface,&slot->iSurfaceGeneration);
        status=xuiWidgetInvalidate(widget,XUI_WIDGET_DIRTY_RENDER);
    }
    xerror* first=status<0 ? xrtTakeError():NULL;
    int reported=status<0 ? status:XUI_OK;
    reported=xuiInternalCallScopeEndOutcome(&scope,reported,&first);xrtSetErrorTake(first);
    return reported==XUI_OK && status==XUI_STATUS_DEFERRED ? XUI_STATUS_DEFERRED:reported;
}

static int __xuiWidgetHasCurrentCacheSurface(xui_widget pWidget)
{
	xui_widget_cache_slot_t* pSlot;

	pSlot = __xuiWidgetFindCacheSlot(pWidget, pWidget->iStateId);
	return (pSlot != NULL) && (pSlot->pSurface != NULL);
}

static int __xuiWidgetEnsureCacheStateSlot(xui_widget pWidget, uint32_t iStateId)
{
	int iOldCount;
	int iRet;

	if ( __xuiWidgetFindCacheSlot(pWidget, iStateId) != NULL ) {
		return XUI_OK;
	}
	if ( pWidget->iCacheCount == 0 ) {
		return __xuiWidgetEnsureSingleCacheSlot(pWidget, iStateId);
	}
	iOldCount = pWidget->iCacheCount;
	iRet = xuiWidgetSetCacheStateCount(pWidget, iOldCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetSetCacheStateId(pWidget, iOldCount, iStateId);
}

static int __xuiWidgetRenderTreeToDraw(xui_widget pWidget, xui_draw_context pDraw,
	xui_rect_t tTargetWorld, xui_rect_t tClip);

static int __xuiWidgetRenderCacheToDraw(xui_widget pWidget, xui_draw_context pDraw,
	xui_rect_t tTargetWorld, xui_rect_t tClip)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	xui_rect_t tWorld;
	float fScaleX;
	float fScaleY;
	int iPolicy;
	int iRet;

	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	if ( !pWidget->bVisible || (iPolicy == XUI_CACHE_POLICY_NONE) ) {
		return XUI_OK;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, pWidget->iStateId);
	if ( (pSlot == NULL) || (pSlot->pSurface == NULL) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tDst = __xuiWidgetIntersectRect(tWorld, tClip);
	if ( tDst.fW <= 0 || tDst.fH <= 0 ) return XUI_OK;
	fScaleX = (float)pSlot->iWidth / tWorld.fW;
	fScaleY = (float)pSlot->iHeight / tWorld.fH;
	tSrc = xuiInternalRectFromFloatNearest((tDst.fX - tWorld.fX) * fScaleX,
		(tDst.fY - tWorld.fY) * fScaleY, tDst.fW * fScaleX, tDst.fH * fScaleY);
	tDst.fX -= tTargetWorld.fX;
	tDst.fY -= tTargetWorld.fY;
	iRet = pProxy->drawSurface(pProxy, pDraw, pSlot->pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
	if ( iRet == XUI_OK ) {
		pWidget->pContext->tRenderStats.iDrawnCaches++;
	}
	return iRet;
}

static int __xuiWidgetRenderTreeToDraw(xui_widget pWidget, xui_draw_context pDraw,
	xui_rect_t tTargetWorld, xui_rect_t tClip)
{
	const xui_widget* ppChildren;
	xui_rect_t tWorld;
	xui_rect_t arrChildClips[4];
	int iClipCount = 1;
	int c;
	int iChildCount;
	int i;
	int iPolicy;
	int iRet;

	if ( !pWidget->bVisible || tClip.fW <= 0 || tClip.fH <= 0 ) {
		return XUI_OK;
	}
	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	iRet = __xuiWidgetRenderCacheToDraw(pWidget, pDraw, tTargetWorld, tClip);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( pWidget->tLayout.iOverflow == XUI_OVERFLOW_CLIP ||
	     pWidget->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN ) {
		tClip = __xuiWidgetIntersectRect(tClip, tWorld);
	}
	arrChildClips[0] = tClip;
	if ( iPolicy == XUI_CACHE_POLICY_SUBTREE && __xuiWidgetHasCurrentCacheSurface(pWidget) ) {
		/* The cache already contains the descendants inside its own box. */
		iClipCount = xuiInternalRectSubtract(tClip, tWorld, arrChildClips);
	}
	if ( iClipCount == 0 || tClip.fW <= 0 || tClip.fH <= 0 ) return XUI_OK;
	iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iChildCount; ++i ) {
		for ( c = 0; c < iClipCount; ++c ) {
			iRet = __xuiWidgetRenderTreeToDraw(ppChildren[i], pDraw, tTargetWorld, arrChildClips[c]);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiWidgetCacheStateNeedsUpdate(xui_widget pWidget, xui_widget_cache_slot_t* pSlot)
{
	int iPolicy;
	int iWidth;
	int iHeight;

	if ( pSlot == NULL ) {
		return 1;
	}
	iWidth = xuiInternalPixelCeil(pWidget->tRect.fW);
	iHeight = xuiInternalPixelCeil(pWidget->tRect.fH);
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return (pSlot->pSurface != NULL);
	}
	if ( (pSlot->pSurface == NULL) || ((pSlot->iFlags & XUI_WIDGET_DIRTY_CACHE) != 0) ) {
		return 1;
	}
	if ( pSlot->iDpiGeneration != pWidget->pContext->iDpiGeneration ) return 1;
	if ( (pSlot->iWidth != iWidth) || (pSlot->iHeight != iHeight) ) {
		return 1;
	}
	if ( (pWidget->iDirtyFlags & XUI_RENDER_CACHE_DIRTY_FLAGS) != 0 ) {
		return 1;
	}
	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	if ( (iPolicy == XUI_CACHE_POLICY_SUBTREE) &&
	     ((pWidget->iSubtreeDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) != 0) ) {
		return 1;
	}
	return 0;
}

static int __xuiWidgetUpdateCacheState(xui_widget pWidget, uint32_t iStateId)
{
	xui_draw_context pDraw;
	const xui_widget* ppChildren;
	int iChildCount;
	int i;
	xui_rect_t tWorldRect;
	uint32_t iUpdateFlags;
	int iPolicy;
	int iRet;
	int iEndRet;

	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	pDraw = NULL;
	iUpdateFlags = 0;
	if ( (pWidget->tCachePolicy.iFlags & XUI_CACHE_CLEAR_ON_UPDATE) != 0 ) {
		iUpdateFlags |= XUI_WIDGET_UPDATE_CLEAR;
	}
	iRet = xuiWidgetUpdateBegin(pWidget, iStateId, iUpdateFlags, pWidget->tCachePolicy.iClearColor, &pDraw);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pWidget->onCacheRender != NULL ) {
		iRet = xuiInternalInvokeCacheRenderCallback(pWidget,pDraw,iStateId);
	}
	/* Destruction is deferred by RenderPrepare's operation, but the cache must
	 * stop now and close its draw before any surface can be purged. */
	if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) ) {
		__xuiWidgetEndActiveUpdate(pWidget);
		return XUI_OK;
	}
	if ( (iRet == XUI_OK) && (iPolicy == XUI_CACHE_POLICY_SUBTREE) ) {
		tWorldRect = xuiWidgetGetWorldRect(pWidget);
		iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
		for ( i = 0; iRet == XUI_OK && i < iChildCount; ++i ) {
			iRet = __xuiWidgetRenderTreeToDraw(ppChildren[i], pDraw, tWorldRect, tWorldRect);
			if ( iRet != XUI_OK ) {
				break;
			}
		}
	}
	iEndRet = xuiWidgetUpdateEnd(pWidget, iStateId, pDraw);
	pDraw = NULL;
	if ( iRet == XUI_OK ) {
		iRet = iEndRet;
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_TREE);
	pWidget->pContext->tRenderStats.iUpdatedCaches++;
	return XUI_OK;
}

static int __xuiWidgetPrepareCacheState(xui_widget pWidget, uint32_t iStateId, int bForce)
{
	xui_widget_cache_slot_t* pSlot;
	int iWidth;
	int iHeight;
	int iRet;

	iRet = __xuiWidgetEnsureCacheStateSlot(pWidget, iStateId);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, iStateId);
	iWidth = xuiInternalPixelCeil(pWidget->tRect.fW);
	iHeight = xuiInternalPixelCeil(pWidget->tRect.fH);
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		if ( pSlot != NULL && pSlot->pSurface != NULL ) {
			__xuiWidgetPurgeCacheSlotSurface(pWidget, pSlot);
		}
		if ( pSlot != NULL ) {
			pSlot->iFlags &= ~XUI_WIDGET_DIRTY_CACHE;
		}
		pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_TREE);
		return XUI_OK;
	}
	if ( !bForce && !__xuiWidgetCacheStateNeedsUpdate(pWidget, pSlot) ) {
		return XUI_OK;
	}
	iRet = __xuiWidgetUpdateCacheState(pWidget, iStateId);
	if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) ) return XUI_OK;
	pSlot = __xuiWidgetFindCacheSlot(pWidget, iStateId);
	if ( iRet != XUI_OK ) {
		if ( pSlot != NULL ) {
			__xuiWidgetPurgeCacheSlotSurface(pWidget, pSlot);
		}
		return iRet;
	}
	pWidget->pContext->tRenderStats.iUpdatedWidgets++;
	return XUI_OK;
}

static int __xuiWidgetPrepareCache(xui_widget pWidget)
{
	int i;
	int iCount;
	int iRet;
	int iPolicy;
	int bForceAll;

	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	/* AUTO can change meaning when its last child is detached. Keep all state
	 * slots consistent, including manual SELF caches without a paint callback. */
	for ( i = 0; i < pWidget->iCacheCount; ++i ) {
		xui_widget_cache_slot_t* pSlot = &pWidget->pCacheSlots[i];
		if ( pSlot->iContentPolicy != iPolicy && pSlot != pWidget->pActiveUpdateSlot ) {
			__xuiWidgetPurgeCacheSlotSurface(pWidget, pSlot);
		}
	}
	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	if ( iPolicy == XUI_CACHE_POLICY_NONE ) {
		pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_TREE);
		return XUI_OK;
	}
	if ( (iPolicy == XUI_CACHE_POLICY_SELF) && (pWidget->onCacheRender == NULL) ) {
		pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_TREE);
		return XUI_OK;
	}
	if ( ((pWidget->tCachePolicy.iFlags & XUI_CACHE_UPDATE_ALL_STATES) != 0) && (pWidget->iCacheCount > 0) ) {
		bForceAll = ((pWidget->iDirtyFlags & XUI_RENDER_CACHE_DIRTY_FLAGS) != 0) ||
		            ((iPolicy == XUI_CACHE_POLICY_SUBTREE) &&
		             ((pWidget->iSubtreeDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) != 0));
		iCount = pWidget->iCacheCount;
		for ( i = 0; i < iCount; i++ ) {
			iRet = __xuiWidgetPrepareCacheState(pWidget, pWidget->pCacheSlots[i].iStateId, bForceAll);
			if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) ) return XUI_OK;
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
		return XUI_OK;
	}
	return __xuiWidgetPrepareCacheState(pWidget, pWidget->iStateId, 0);
}

static void __xuiWidgetRenderPrepareTree(xui_widget pWidget)
{
	xui_widget pChild;
	xui_widget pNext;
	xui_internal_prepare_paint_proc onPreparePaint;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) || !pWidget->bVisible ) {
		return;
	}
	if ( (pWidget->iSubtreeDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) == 0 &&
	     !pWidget->bSubtreePrepareFrameDependent &&
	     pWidget->iResolvedStyleGeneration == pWidget->pContext->iStyleGeneration ) {
		pWidget->pContext->tRenderStats.iPrepareSkippedSubtrees++;
		return;
	}
	pWidget->pContext->tRenderStats.iPrepareVisitedWidgets++;
	/* Detached and newly created widgets can miss the context's style refresh. */
	if ( pWidget->iResolvedStyleGeneration != pWidget->pContext->iStyleGeneration ) {
		iRet = xuiWidgetResolveStyle(pWidget);
		if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) || !pWidget->bVisible ) return;
		if ( iRet != XUI_OK ) {
			xuiInternalReportError(pWidget->pContext, pWidget, iRet, XUI_ERROR_STAGE_CACHE, 1,
				"cache.style", "The widget style could not be resolved before preparing its paint dependencies.");
			return;
		}
	}
	onPreparePaint = __xuiWidgetTypePreparePaint(pWidget->pType);
	if ( onPreparePaint != NULL ) {
		iRet = onPreparePaint(pWidget);
		if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) || !pWidget->bVisible ) return;
		if ( iRet != XUI_OK ) {
			xuiInternalReportError(pWidget->pContext, pWidget, iRet, XUI_ERROR_STAGE_CACHE, 1,
				"cache.dependencies", "The widget paint dependencies could not be prepared and were skipped.");
			return;
		}
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pNext ) {
		pNext = pChild->pNextSibling;
		__xuiWidgetRenderPrepareTree(pChild);
		if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) ) return;
	}
	iRet = __xuiWidgetPrepareCache(pWidget);
	if ( !__xuiWidgetValid(pWidget) || xuiInternalContextDestroyPending(pWidget->pContext) ) return;
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pWidget->pContext, pWidget, iRet, XUI_ERROR_STAGE_CACHE, 1,
			"cache.prepare", "The widget cache could not be prepared and was skipped.");
	}
	pWidget->iSubtreeDirtyFlags = __xuiWidgetAggregateDirtyFlags(pWidget);
}

static int __xuiRenderPrepareOperation(xui_context pContext)
{
	xui_widget pRoot;
	uint32_t iGeneration;
	uint64_t iDirtySnapshot;
	uint64_t iEventSnapshot;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !xuiInternalContextHasProxy(pContext) ) {
		xuiInternalReportError(pContext, NULL, XUI_ERROR_NOT_INITIALIZED, XUI_ERROR_STAGE_CACHE, 0,
			"cache.proxy", "No render proxy is attached to the XUI context.");
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iGeneration = pContext->tRenderStats.iGeneration + 1;
	if ( iGeneration == 0 ) {
		iGeneration = 1;
	}
	iDirtySnapshot = pContext->iDirtyPropagationSnapshot;
	iEventSnapshot = pContext->iEventMaskPropagationSnapshot;
	memset(&pContext->tRenderStats, 0, sizeof(pContext->tRenderStats));
	pContext->tRenderStats.iSize = sizeof(pContext->tRenderStats);
	pContext->tRenderStats.iGeneration = iGeneration;
	iRet = xuiInternalEnsureLayout(pContext);
	if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, pContext->pRoot, iRet, XUI_ERROR_STAGE_LAYOUT, 0,
			"layout", "Layout failed; rendering cannot safely continue with a new tree.");
		return iRet;
	}
	pRoot = pContext->pRoot;
	if ( pRoot != NULL ) {
		__xuiWidgetRenderPrepareTree(pRoot);
		if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
	}
	pRoot = pContext->pOverlayRoot;
	if ( pRoot != NULL ) {
		__xuiWidgetRenderPrepareTree(pRoot);
		if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
	}
	pContext->tRenderStats.iDirtyPropagationSteps =
		pContext->iDirtyPropagationTotal - iDirtySnapshot;
	pContext->tRenderStats.iEventMaskPropagationSteps =
		pContext->iEventMaskPropagationTotal - iEventSnapshot;
	pContext->iDirtyPropagationSnapshot = pContext->iDirtyPropagationTotal;
	pContext->iEventMaskPropagationSnapshot = pContext->iEventMaskPropagationTotal;
	pContext->tRenderStats.iRootStructureRevision = pContext->iRootStructureRevision;
	pContext->tRenderStats.iOverlayStructureRevision = pContext->iOverlayStructureRevision;
#if XUI_ENABLE_INCREMENTAL_VERIFY
	{
		int bValid = 1;
		if ( pContext->pRoot != NULL )
			(void)__xuiWidgetVerifyDirtyMaskRecursive(pContext->pRoot, &bValid);
		if ( pContext->pOverlayRoot != NULL )
			(void)__xuiWidgetVerifyDirtyMaskRecursive(pContext->pOverlayRoot, &bValid);
		if ( !bValid ) return XUI_ERROR_INVALID_STATE;
	}
#endif
	return XUI_OK;
}

XUI_API int xuiRenderPrepare(xui_context pContext)
{
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiRenderPrepareOperation(pContext);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiGetRenderStats(xui_context pContext, xui_render_stats_t* pStats)
{
	size_t iCopySize;
	xui_render_stats_t tStats;

	if ( !xuiInternalContextIsValid(pContext) || (pStats == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCopySize = pStats->iSize;
	if ( iCopySize == 0 || iCopySize > sizeof(*pStats) ) iCopySize = sizeof(*pStats);
	tStats = pContext->tRenderStats;
	tStats.iSize = sizeof(tStats);
	memcpy(pStats, &tStats, iCopySize);
	return XUI_OK;
}

XUI_API int xuiSetCacheBudget(xui_context pContext, size_t iBudgetBytes)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iCacheBudgetBytes = iBudgetBytes;
	pContext->iCacheGeneration++;
	if ( pContext->iCacheGeneration == 0 ) {
		pContext->iCacheGeneration = 1;
	}
	if ( iBudgetBytes > 0u ) {
		return xuiPurgeCaches(pContext, iBudgetBytes);
	}
	return XUI_OK;
}

XUI_API size_t xuiGetCacheBudget(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->iCacheBudgetBytes : 0u;
}

XUI_API int xuiGetCacheStats(xui_context pContext, xui_cache_stats_t* pStats)
{
	size_t iUsedBytes;
	int iSurfaceCount;

	if ( !xuiInternalContextIsValid(pContext) || (pStats == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iUsedBytes = 0u;
	iSurfaceCount = 0;
	__xuiWidgetCollectCacheStats(pContext->pRoot, &iUsedBytes, &iSurfaceCount);
	__xuiWidgetCollectCacheStats(pContext->pOverlayRoot, &iUsedBytes, &iSurfaceCount);
	memset(pStats, 0, sizeof(*pStats));
	pStats->iSize = sizeof(*pStats);
	pStats->iBudgetBytes = pContext->iCacheBudgetBytes;
	pStats->iUsedBytes = iUsedBytes;
	pStats->iSurfaceCount = iSurfaceCount;
	pStats->iGeneration = pContext->iCacheGeneration;
	return XUI_OK;
}

XUI_API int xuiPurgeCaches(xui_context pContext, size_t iTargetBytes)
{
	size_t iUsedBytes;
	int iSurfaceCount;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iUsedBytes = 0u;
	iSurfaceCount = 0;
	__xuiWidgetCollectCacheStats(pContext->pRoot, &iUsedBytes, &iSurfaceCount);
	__xuiWidgetCollectCacheStats(pContext->pOverlayRoot, &iUsedBytes, &iSurfaceCount);
	if ( iUsedBytes <= iTargetBytes ) {
		return XUI_OK;
	}
	__xuiWidgetPurgeCachesToTarget(pContext->pRoot, &iUsedBytes, iTargetBytes);
	__xuiWidgetPurgeCachesToTarget(pContext->pOverlayRoot, &iUsedBytes, iTargetBytes);
	pContext->iCacheGeneration++;
	if ( pContext->iCacheGeneration == 0 ) {
		pContext->iCacheGeneration = 1;
	}
	return xuiInternalContextInvalidateAll(pContext);
}

static xui_rect_t __xuiWidgetInheritedRenderClip(xui_context pContext, xui_widget pWidget)
{
	xui_rect_t tClip;
	xui_widget pAncestor;

	tClip.fX = 0.0f;
	tClip.fY = 0.0f;
	tClip.fW = pContext->fViewportWidth;
	tClip.fH = pContext->fViewportHeight;
	for ( pAncestor = pWidget->pParent; pAncestor != NULL; pAncestor = pAncestor->pParent ) {
		if ( (pAncestor->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN) ||
		     (pAncestor->tLayout.iOverflow == XUI_OVERFLOW_CLIP) ) {
			tClip = __xuiWidgetIntersectRect(tClip,
				xuiInternalSnapRect(xuiWidgetGetWorldRect(pAncestor)));
		}
	}
	return tClip;
}

static int __xuiWidgetRemoveRenderRange(xui_context pContext, xui_widget pWidget,
	xui_widget* ppBefore, xui_widget* ppAfter)
{
	xui_widget pEnd;
	xui_widget pScan;
	xui_widget pNext;
	int iRemoved = 0;

	*ppBefore = NULL;
	*ppAfter = NULL;
	if ( pWidget == NULL || !pWidget->bRenderNodeLinked ) return 0;
	pEnd = pWidget->pRenderSubtreeLast != NULL ? pWidget->pRenderSubtreeLast : pWidget;
	*ppBefore = pWidget->pRenderPrev;
	*ppAfter = pEnd->pRenderNext;
	if ( *ppBefore != NULL ) (*ppBefore)->pRenderNext = *ppAfter;
	else pContext->pRenderHead = *ppAfter;
	if ( *ppAfter != NULL ) (*ppAfter)->pRenderPrev = *ppBefore;
	else pContext->pRenderTail = *ppBefore;
	for ( pScan = pWidget; pScan != *ppAfter; pScan = pNext ) {
		pNext = pScan->pRenderNext;
		pScan->pRenderPrev = NULL;
		pScan->pRenderNext = NULL;
		pScan->pRenderSubtreeLast = NULL;
		pScan->bRenderNodeLinked = 0;
		iRemoved++;
	}
	pContext->iRenderLinkedNodeCount -= iRemoved;
	if ( pContext->iRenderLinkedNodeCount < 0 ) pContext->iRenderLinkedNodeCount = 0;
	pContext->tRenderStats.iRenderNodesRemoved += iRemoved;
	pContext->bRenderIndexDirty = 1;
	return iRemoved;
}

static void __xuiWidgetClearRenderStructureDirtyTree(xui_widget pWidget)
{
	xui_widget pChild;
	if ( pWidget == NULL ) return;
	pWidget->bRenderStructureDirty = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling )
		__xuiWidgetClearRenderStructureDirtyTree(pChild);
	pWidget->bSubtreeRenderStructureDirty = 0;
}

static int __xuiWidgetRebuildRenderRange(xui_context pContext, xui_widget pWidget)
{
	xui_widget pBefore;
	xui_widget pAfter;
	xui_widget pHead = NULL;
	xui_widget pTail = NULL;
	xui_widget pOldEnd;
	xui_widget pAncestor;
	xui_widget pReplacement;
	xui_rect_t tClip;
	int iCount = 0;
	int iRet;

	if ( pWidget == NULL ) return XUI_OK;
	pOldEnd = pWidget->bRenderNodeLinked ?
		(pWidget->pRenderSubtreeLast != NULL ? pWidget->pRenderSubtreeLast : pWidget) : NULL;
	(void)__xuiWidgetRemoveRenderRange(pContext, pWidget, &pBefore, &pAfter);
	tClip = __xuiWidgetInheritedRenderClip(pContext, pWidget);
	iRet = __xuiWidgetBuildRenderChainRecursive(pContext, pWidget, tClip, 1,
		&pHead, &pTail, &iCount);
	if ( iRet != XUI_OK ) return iRet;
	if ( pHead != NULL ) {
		pHead->pRenderPrev = pBefore;
		if ( pBefore != NULL ) pBefore->pRenderNext = pHead;
		else pContext->pRenderHead = pHead;
		pTail->pRenderNext = pAfter;
		if ( pAfter != NULL ) pAfter->pRenderPrev = pTail;
		else pContext->pRenderTail = pTail;
	} else {
		if ( pBefore != NULL ) pBefore->pRenderNext = pAfter;
		else pContext->pRenderHead = pAfter;
		if ( pAfter != NULL ) pAfter->pRenderPrev = pBefore;
		else pContext->pRenderTail = pBefore;
	}
	pContext->iRenderLinkedNodeCount += iCount;
	pContext->tRenderStats.iRenderNodesInserted += iCount;
	pContext->bRenderIndexDirty = 1;
	/* A partial range rebuild may change the final node of every ancestor for
	 * which this subtree is the final visible child. Keep those range ends exact. */
	pReplacement = pTail != NULL ? pTail : pBefore;
	for ( pAncestor = pWidget->pParent; pOldEnd != NULL && pAncestor != NULL;
		pAncestor = pAncestor->pParent ) {
		if ( pAncestor->pRenderSubtreeLast != pOldEnd ) break;
		pAncestor->pRenderSubtreeLast = pReplacement;
	}
	__xuiWidgetClearRenderStructureDirtyTree(pWidget);
	if ( pWidget->pParent != NULL ) __xuiWidgetRepairRenderStructureToRoot(pWidget->pParent);
	return XUI_OK;
}

static int __xuiWidgetRebuildDirtyRenderRanges(xui_context pContext, xui_widget pWidget)
{
	const xui_widget* ppChildren;
	int iChildCount;
	int i;
	int iRet;

	if ( pWidget == NULL || !pWidget->bSubtreeRenderStructureDirty )
		return XUI_OK;
	if ( pWidget->bRenderStructureDirty )
		return __xuiWidgetRebuildRenderRange(pContext, pWidget);
	iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iChildCount; ++i ) {
		xui_widget pChild = ppChildren[i];
		if ( !pChild->bSubtreeRenderStructureDirty ) continue;
		if ( !pChild->bRenderNodeLinked ) return __xuiWidgetRebuildRenderRange(pContext, pWidget);
		iRet = __xuiWidgetRebuildDirtyRenderRanges(pContext, pChild);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiWidgetUpdateRenderRecords(xui_context pContext, xui_widget pWidget,
	xui_rect_t tParentClip, int bHasClip, int bForce)
{
	const xui_widget* ppChildren;
	xui_rect_t tWorldRect;
	xui_rect_t tPaintRect;
	xui_rect_t tChildClip;
	int bChildHasClip;
	int bForceChildren;
	int iChildCount;
	int i;
	int iRet;

	if ( pWidget == NULL || !pWidget->bVisible ) return XUI_OK;
	if ( !bForce && (pWidget->iSubtreeDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) == 0 )
		return XUI_OK;
	bForceChildren = bForce || pWidget->bRenderGeometryDirty;
	tWorldRect = xuiInternalSnapRect(xuiWidgetGetWorldRect(pWidget));
	tPaintRect = bHasClip ? __xuiWidgetIntersectRect(tWorldRect, tParentClip) : tWorldRect;
	if ( pWidget->bRenderNodeLinked &&
	     (bForce || pWidget->bRenderGeometryDirty ||
	      (pWidget->iDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) != 0) ) {
		__xuiWidgetFillRenderNode(pWidget, tPaintRect);
		pContext->tRenderStats.iRenderNodesUpdated++;
		pContext->bRenderIndexDirty = 1;
	}
	tChildClip = tParentClip;
	bChildHasClip = bHasClip;
	if ( (pWidget->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN) ||
	     (pWidget->tLayout.iOverflow == XUI_OVERFLOW_CLIP) ) {
		tChildClip = bHasClip ? __xuiWidgetIntersectRect(tPaintRect, tParentClip) : tPaintRect;
		bChildHasClip = 1;
	}
	iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iChildCount; ++i ) {
		iRet = __xuiWidgetUpdateRenderRecords(pContext, ppChildren[i], tChildClip,
			bChildHasClip, bForceChildren);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static void __xuiWidgetClearLinkedRenderTree(xui_context pContext)
{
	xui_widget pScan;
	xui_widget pNext;

	for ( pScan = pContext->pRenderHead; pScan != NULL; pScan = pNext ) {
		pNext = pScan->pRenderNext;
		pScan->pRenderPrev = NULL;
		pScan->pRenderNext = NULL;
		pScan->pRenderSubtreeLast = NULL;
		pScan->bRenderNodeLinked = 0;
	}
	pContext->pRenderHead = NULL;
	pContext->pRenderTail = NULL;
	pContext->iRenderLinkedNodeCount = 0;
	pContext->bRenderIndexDirty = 1;
}

static int __xuiWidgetBuildFullRenderTree(xui_context pContext, xui_rect_t tRootClip)
{
	xui_widget pHead = NULL;
	xui_widget pTail = NULL;
	int iOldCount = pContext->iRenderLinkedNodeCount;
	int iCount = 0;
	int iRet;

	__xuiWidgetClearLinkedRenderTree(pContext);
	if ( pContext->pRoot != NULL ) {
		iRet = __xuiWidgetBuildRenderChainRecursive(pContext, pContext->pRoot, tRootClip, 1,
			&pHead, &pTail, &iCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		iRet = __xuiWidgetBuildRenderChainRecursive(pContext, pContext->pOverlayRoot, tRootClip, 1,
			&pHead, &pTail, &iCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	pContext->pRenderHead = pHead;
	pContext->pRenderTail = pTail;
	pContext->iRenderLinkedNodeCount = iCount;
	pContext->tRenderStats.iRenderNodesRemoved += iOldCount;
	pContext->tRenderStats.iRenderNodesInserted += iCount;
	pContext->bRenderIndexDirty = 1;
	__xuiWidgetClearRenderStructureDirtyTree(pContext->pRoot);
	__xuiWidgetClearRenderStructureDirtyTree(pContext->pOverlayRoot);
	return XUI_OK;
}

static int __xuiWidgetAppendTopRenderRange(xui_context pContext, xui_widget pWidget,
	xui_rect_t tRootClip)
{
	xui_widget pHead = NULL;
	xui_widget pTail = NULL;
	int iCount = 0;
	int iRet;

	iRet = __xuiWidgetBuildRenderChainRecursive(pContext, pWidget, tRootClip, 1,
		&pHead, &pTail, &iCount);
	if ( iRet != XUI_OK ) return iRet;
	if ( pHead != NULL ) {
		pHead->pRenderPrev = pContext->pRenderTail;
		if ( pContext->pRenderTail != NULL ) pContext->pRenderTail->pRenderNext = pHead;
		else pContext->pRenderHead = pHead;
		pContext->pRenderTail = pTail;
	}
	pContext->iRenderLinkedNodeCount += iCount;
	pContext->tRenderStats.iRenderNodesInserted += iCount;
	pContext->bRenderIndexDirty = 1;
	__xuiWidgetClearRenderStructureDirtyTree(pWidget);
	if ( pWidget->pParent != NULL ) __xuiWidgetRepairRenderStructureToRoot(pWidget->pParent);
	return XUI_OK;
}

#if XUI_ENABLE_INCREMENTAL_VERIFY
static int __xuiWidgetVerifyRenderChainRecursive(xui_widget pWidget, xui_rect_t tParentClip,
	int bHasClip, xui_widget* ppScan, int* pCount)
{
	const xui_widget* ppChildren;
	xui_rect_t tWorldRect;
	xui_rect_t tPaintRect;
	xui_rect_t tChildClip;
	xui_render_node_t* pNode;
	xui_widget pExpectedLast;
	int bChildHasClip;
	int iChildCount;
	int i;
	int iRet;

	if ( pWidget == NULL || !pWidget->bVisible ) return XUI_OK;
	if ( *ppScan != pWidget || !pWidget->bRenderNodeLinked ) {
		fprintf(stderr, "XUI incremental verification failed: render order/link\n");
		return XUI_ERROR_INVALID_STATE;
	}
	pNode = &pWidget->tRenderNode;
	tWorldRect = xuiInternalSnapRect(xuiWidgetGetWorldRect(pWidget));
	tPaintRect = bHasClip ? __xuiWidgetIntersectRect(tWorldRect, tParentClip) : tWorldRect;
	if ( pNode->pWidget != pWidget || memcmp(&pNode->tWorldRect, &tWorldRect, sizeof(tWorldRect)) != 0 ||
	     memcmp(&pNode->tPaintRect, &tPaintRect, sizeof(tPaintRect)) != 0 ||
	     pNode->iLayer != pWidget->tLayout.iLayer || pNode->iZIndex != pWidget->tLayout.iZIndex ||
	     pNode->iStateId != pWidget->iStateId ||
	     pNode->iCachePolicy != __xuiWidgetEffectiveCachePolicy(pWidget) )
	{
		fprintf(stderr, "XUI incremental verification failed: render node geometry/state\n");
		return XUI_ERROR_INVALID_STATE;
	}
	(*pCount)++;
	*ppScan = pWidget->pRenderNext;
	tChildClip = tParentClip;
	bChildHasClip = bHasClip;
	if ( (pWidget->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN) ||
	     (pWidget->tLayout.iOverflow == XUI_OVERFLOW_CLIP) ) {
		tChildClip = bHasClip ? __xuiWidgetIntersectRect(tPaintRect, tParentClip) : tPaintRect;
		bChildHasClip = 1;
	}
	iRet = xuiInternalStackingChildren(pWidget, &ppChildren, &iChildCount);
	if ( iRet != XUI_OK ) return iRet;
	pExpectedLast = pWidget;
	for ( i = 0; i < iChildCount; ++i ) {
		iRet = __xuiWidgetVerifyRenderChainRecursive(ppChildren[i], tChildClip,
			bChildHasClip, ppScan, pCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( ppChildren[i]->bVisible && ppChildren[i]->pRenderSubtreeLast != NULL )
			pExpectedLast = ppChildren[i]->pRenderSubtreeLast;
	}
	if ( pWidget->pRenderSubtreeLast != pExpectedLast ) {
		fprintf(stderr, "XUI render range mismatch: widget=%p type=%s stored=%p/%s expected=%p/%s children=%d\n",
			(void*)pWidget,
			pWidget->pType != NULL ? pWidget->pType->sName : "?",
			(void*)pWidget->pRenderSubtreeLast,
			pWidget->pRenderSubtreeLast != NULL && pWidget->pRenderSubtreeLast->pType != NULL ?
				pWidget->pRenderSubtreeLast->pType->sName : "null",
			(void*)pExpectedLast,
			pExpectedLast != NULL && pExpectedLast->pType != NULL ? pExpectedLast->pType->sName : "null",
			pWidget->iChildCount);
		return XUI_ERROR_INVALID_STATE;
	}
	return XUI_OK;
}

static int __xuiWidgetVerifyRenderChain(xui_context pContext, xui_rect_t tRootClip)
{
	xui_widget pScan = pContext->pRenderHead;
	int iCount = 0;
	int iRet;
	if ( pContext->pRoot != NULL ) {
		iRet = __xuiWidgetVerifyRenderChainRecursive(pContext->pRoot, tRootClip, 1, &pScan, &iCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		iRet = __xuiWidgetVerifyRenderChainRecursive(pContext->pOverlayRoot, tRootClip, 1, &pScan, &iCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pScan != NULL || iCount != pContext->iRenderLinkedNodeCount ) {
		fprintf(stderr, "XUI incremental verification failed: render chain count\n");
		return XUI_ERROR_INVALID_STATE;
	}
	return XUI_OK;
}
#endif

static int __xuiBuildRenderTreeOperation(xui_context pContext)
{
	xui_rect_t tRootClip;
	uint32_t iGeneration;
	int iOldCount;
	int bOverlayAdded = 0;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->pRoot == NULL) && (pContext->pOverlayRoot == NULL) ) {
		__xuiWidgetClearLinkedRenderTree(pContext);
		pContext->iRenderNodeCount = 0;
		return XUI_OK;
	}
	iRet = xuiInternalEnsureLayout(pContext);
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, pContext->pRoot, iRet, XUI_ERROR_STAGE_LAYOUT, 0,
			"render_tree.layout", "The render tree could not be rebuilt because layout failed.");
		return iRet;
	}
	tRootClip.fX = 0.0f;
	tRootClip.fY = 0.0f;
	tRootClip.fW = pContext->fViewportWidth;
	tRootClip.fH = pContext->fViewportHeight;
	iOldCount = pContext->iRenderLinkedNodeCount;
	if ( pContext->iRenderTreeGeneration == 0 ||
	     (pContext->pRoot != NULL && pContext->pRoot->bVisible && !pContext->pRoot->bRenderNodeLinked) ) {
		iRet = __xuiWidgetBuildFullRenderTree(pContext, tRootClip);
	} else {
		iRet = XUI_OK;
		if ( pContext->pOverlayRoot != NULL && pContext->pOverlayRoot->bVisible &&
		     !pContext->pOverlayRoot->bRenderNodeLinked ) {
			iRet = __xuiWidgetAppendTopRenderRange(pContext, pContext->pOverlayRoot, tRootClip);
			bOverlayAdded = (iRet == XUI_OK);
		}
		if ( iRet == XUI_OK ) iRet = __xuiWidgetRebuildDirtyRenderRanges(pContext, pContext->pRoot);
		if ( iRet == XUI_OK && !bOverlayAdded )
			iRet = __xuiWidgetRebuildDirtyRenderRanges(pContext, pContext->pOverlayRoot);
	}
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, pContext->pRoot, iRet, XUI_ERROR_STAGE_RENDER, 0,
			"render_tree.build", "The incremental render tree could not be updated.");
		return iRet;
	}
	if ( pContext->pRoot != NULL ) {
		iRet = __xuiWidgetUpdateRenderRecords(pContext, pContext->pRoot, tRootClip, 1, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		iRet = __xuiWidgetUpdateRenderRecords(pContext, pContext->pOverlayRoot, tRootClip, 1, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	pContext->iRenderNodeCount = pContext->iRenderLinkedNodeCount;
	pContext->tRenderStats.iRenderNodesReused = iOldCount - pContext->tRenderStats.iRenderNodesRemoved;
	if ( pContext->tRenderStats.iRenderNodesReused < 0 ) pContext->tRenderStats.iRenderNodesReused = 0;
	pContext->tRenderStats.iRootStructureRevision = pContext->iRootStructureRevision;
	pContext->tRenderStats.iOverlayStructureRevision = pContext->iOverlayStructureRevision;
#if XUI_ENABLE_INCREMENTAL_VERIFY
	iRet = __xuiWidgetVerifyRenderChain(pContext, tRootClip);
	if ( iRet != XUI_OK ) return iRet;
#endif
	iGeneration = pContext->iRenderTreeGeneration + 1;
	pContext->iRenderTreeGeneration = (iGeneration != 0) ? iGeneration : 1;
	return XUI_OK;
}

XUI_API int xuiBuildRenderTree(xui_context pContext)
{
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiBuildRenderTreeOperation(pContext);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiGetRenderNodeCount(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return 0;
	}
	return pContext->iRenderLinkedNodeCount;
}

static int __xuiWidgetRefreshRenderNodeIndex(xui_context pContext)
{
	xui_widget pScan;
	int iRet;
	int iCount = 0;

	if ( !pContext->bRenderIndexDirty &&
	     pContext->iRenderNodeCount == pContext->iRenderLinkedNodeCount ) return XUI_OK;
	iRet = __xuiWidgetReserveRenderNodes(pContext, pContext->iRenderLinkedNodeCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( pScan = pContext->pRenderHead; pScan != NULL; pScan = pScan->pRenderNext ) {
		pContext->pRenderNodes[iCount++] = pScan->tRenderNode;
	}
	pContext->iRenderNodeCount = iCount;
	pContext->bRenderIndexDirty = 0;
	return XUI_OK;
}

XUI_API int xuiGetRenderNode(xui_context pContext, int iIndex, xui_render_node_t* pNode)
{
	if ( !xuiInternalContextIsValid(pContext) ||
	     (pNode == NULL) ||
	     (iIndex < 0) ||
	     (iIndex >= pContext->iRenderLinkedNodeCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiWidgetRefreshRenderNodeIndex(pContext) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	*pNode = pContext->pRenderNodes[iIndex];
	pNode->iSize = sizeof(*pNode);
	return XUI_OK;
}

XUI_API int xuiWidgetRenderCache(xui_widget pWidget, xui_surface pTarget)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	int iPolicy;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (pTarget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	if ( !pWidget->bVisible || (iPolicy == XUI_CACHE_POLICY_NONE) ) {
		return XUI_OK;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, pWidget->iStateId);
	if ( (pSlot == NULL) || (pSlot->pSurface == NULL) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tSrc.fX = 0.0f;
	tSrc.fY = 0.0f;
	tSrc.fW = (float)pSlot->iWidth;
	tSrc.fH = (float)pSlot->iHeight;
	tDst = xuiWidgetGetWorldRect(pWidget);
	tDst = xuiInternalSnapRect(tDst);
	iRet = pProxy->surfaceDrawTo(pProxy, pTarget, pSlot->pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
	if ( iRet == XUI_OK ) {
		pWidget->pContext->tRenderStats.iDrawnCaches++;
	}
	return iRet;
}

static int __xuiWidgetRenderTreeToSurface(xui_widget pWidget, xui_surface pTarget)
{
	xui_widget pChild;
	int iPolicy;
	int iRet;

	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	iRet = xuiWidgetRenderCache(pWidget, pTarget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (iPolicy == XUI_CACHE_POLICY_SUBTREE) &&
	     __xuiWidgetHasCurrentCacheSurface(pWidget) ) {
		pWidget->pContext->tRenderStats.iSkippedWidgets += __xuiWidgetCountDescendants(pWidget);
		__xuiWidgetClearDirtyRecursive(pWidget, XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
		return XUI_OK;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWidgetRenderTreeToSurface(pChild, pTarget);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
	pWidget->iSubtreeDirtyFlags = __xuiWidgetAggregateDirtyFlags(pWidget);
	return XUI_OK;
}

XUI_API int xuiWidgetRenderTree(xui_widget pWidget, xui_surface pTarget)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (pTarget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiWidgetRenderTreeToSurface(pWidget, pTarget);
	__xuiWidgetRepairDirtyToRoot(pWidget);
	return iRet;
}

static int __xuiDebugDepth(xui_widget pWidget)
{
	int iDepth;

	iDepth = 0;
	for ( ; __xuiWidgetValid(pWidget) && (pWidget->pParent != NULL); pWidget = pWidget->pParent ) {
		iDepth++;
	}
	return iDepth;
}

XUI_API int xuiDebugWidgetInspect(xui_widget pWidget, xui_debug_widget_info_t* pInfo)
{
	size_t iCopySize;
	xui_debug_widget_info_t tInfo;

	if ( !__xuiWidgetValid(pWidget) || (pInfo == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCopySize = pInfo->iSize;
	if ( iCopySize == 0 || iCopySize > sizeof(*pInfo) ) iCopySize = sizeof(*pInfo);
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.pWidget = pWidget;
	tInfo.pParent = pWidget->pParent;
	tInfo.iDepth = __xuiDebugDepth(pWidget);
	tInfo.iChildCount = pWidget->iChildCount;
	tInfo.bVisible = pWidget->bVisible;
	tInfo.bEnabled = pWidget->bEnabled;
	tInfo.bFocusable = pWidget->bFocusable;
	tInfo.bTabStop = pWidget->bTabStop;
	tInfo.bFocusScope = pWidget->bFocusScope;
	tInfo.bHitTestVisible = pWidget->bHitTestVisible;
	tInfo.bDragEnabled = pWidget->bDragEnabled;
	tInfo.bDropEnabled = pWidget->bDropEnabled;
	tInfo.iImeMode = pWidget->iImeMode;
	tInfo.iLayer = pWidget->tLayout.iLayer;
	tInfo.iZIndex = pWidget->tLayout.iZIndex;
	tInfo.iDirtyFlags = pWidget->iDirtyFlags;
	tInfo.iEventMask = __xuiWidgetOwnEventMask(pWidget);
	tInfo.iSubtreeEventMask = pWidget->iSubtreeEventMask;
	tInfo.tRect = pWidget->tRect;
	tInfo.tWorldRect = xuiWidgetGetWorldRect(pWidget);
	tInfo.iSubtreeDirtyFlags = pWidget->iSubtreeDirtyFlags;
	memcpy(pInfo, &tInfo, iCopySize);
	return XUI_OK;
}

static int __xuiDebugAppend(char* sBuffer, int iCapacity, int* pOffset, const char* sFormat, ...)
{
	va_list tArgs;
	int iRemain;
	int iWritten;

	if ( (sBuffer == NULL) || (iCapacity <= 0) || (pOffset == NULL) || (*pOffset >= iCapacity) ) {
		return XUI_OK;
	}
	iRemain = iCapacity - *pOffset;
	va_start(tArgs, sFormat);
	iWritten = vsnprintf(sBuffer + *pOffset, (size_t)iRemain, sFormat, tArgs);
	va_end(tArgs);
	if ( iWritten < 0 ) {
		return XUI_ERROR;
	}
	if ( iWritten >= iRemain ) {
		*pOffset = iCapacity - 1;
		sBuffer[iCapacity - 1] = '\0';
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*pOffset += iWritten;
	return XUI_OK;
}

static int __xuiDebugDumpTree(xui_widget pWidget, char* sBuffer, int iCapacity, int* pOffset, int iDepth)
{
	xui_widget pChild;
	const char* sTypeName;
	int i;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	for ( i = 0; i < iDepth; i++ ) {
		iRet = __xuiDebugAppend(sBuffer, iCapacity, pOffset, "  ");
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	sTypeName = (pWidget->pType != NULL) ? pWidget->pType->sName : "widget";
	iRet = __xuiDebugAppend(sBuffer, iCapacity, pOffset,
		"%s rect=%d,%d,%d,%d visible=%d enabled=%d dirty=0x%08x event=0x%llx\n",
		sTypeName,
		pWidget->tRect.fX, pWidget->tRect.fY, pWidget->tRect.fW, pWidget->tRect.fH,
		pWidget->bVisible, pWidget->bEnabled, pWidget->iDirtyFlags,
		(unsigned long long)__xuiWidgetOwnEventMask(pWidget));
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiDebugDumpTree(pChild, sBuffer, iCapacity, pOffset, iDepth + 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiDebugWidgetTreeDump(xui_context pContext, xui_widget pRoot, char* sBuffer, int iCapacity)
{
	int iOffset;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sBuffer[0] = '\0';
	iOffset = 0;
	if ( pRoot != NULL ) {
		if ( !__xuiWidgetValid(pRoot) || (pRoot->pContext != pContext) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		return __xuiDebugDumpTree(pRoot, sBuffer, iCapacity, &iOffset, 0);
	}
	iRet = __xuiDebugAppend(sBuffer, iCapacity, &iOffset, "root\n");
	if ( iRet == XUI_OK ) {
		iRet = __xuiDebugDumpTree(pContext->pRoot, sBuffer, iCapacity, &iOffset, 1);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiDebugAppend(sBuffer, iCapacity, &iOffset, "overlay\n");
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiDebugDumpTree(pContext->pOverlayRoot, sBuffer, iCapacity, &iOffset, 1);
	}
	return iRet;
}

XUI_API int xuiDebugLayoutSnapshot(xui_context pContext, char* sBuffer, int iCapacity)
{
	int iOffset;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sBuffer[0] = '\0';
	iOffset = 0;
	iRet = __xuiDebugAppend(sBuffer, iCapacity, &iOffset,
		"viewport=%d,%d dpi=%.2f damage=%d render_nodes=%d\n",
		pContext->fViewportWidth, pContext->fViewportHeight, pContext->fDpiScale,
		pContext->iDamageCount, pContext->iRenderNodeCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiDebugWidgetTreeDump(pContext, NULL, sBuffer + iOffset, iCapacity - iOffset);
}

XUI_API int xuiDebugEventTrace(xui_context pContext, const xui_event_t* pEvent, char* sBuffer, int iCapacity)
{
	if ( !xuiInternalContextIsValid(pContext) || (pEvent == NULL) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	snprintf(sBuffer, (size_t)iCapacity,
		"event type=%d phase=%d target=%p current=%p related=%p key=%d button=%d xy=%d,%d focus=%p capture=%p hover=%p",
		pEvent->iType, pEvent->iPhase, (void*)pEvent->pTarget, (void*)pEvent->pCurrentTarget,
		(void*)pEvent->pRelated, pEvent->iKey, pEvent->iButton, pEvent->fX, pEvent->fY,
		(void*)pContext->pFocusWidget, (void*)pContext->pPointerCaptureWidget, (void*)pContext->pHoverWidget);
	sBuffer[iCapacity - 1] = '\0';
	return XUI_OK;
}

#endif
