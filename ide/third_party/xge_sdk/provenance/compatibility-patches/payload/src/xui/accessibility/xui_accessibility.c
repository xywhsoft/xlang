#include "xui_config.h"
#if XGE_ENABLE_XUI
#include "xui_internal.h"
#include "xui_accessibility_text_internal.h"
#include "xui_native_accessibility_internal.h"

#include <stddef.h>
#include <string.h>

#define XUI_A11Y_BIT(type) (1u << ((type) - 1))

static int __xuiAccessibleValid(xui_widget pWidget)
{
	return pWidget != NULL && pWidget->pContext != NULL && xuiInternalWidgetIsValid(pWidget) &&
		!xuiInternalContextDestroyPending(pWidget->pContext) && !pWidget->pContext->bDestroying;
}

static int __xuiAccessibleCanAct(xui_widget pWidget)
{
	xui_widget pScan;
	xui_widget pSlow = pWidget, pFast = pWidget, pModal;
	if ( !__xuiAccessibleValid(pWidget) || !xuiWidgetGetEffectiveVisible(pWidget) ||
		!xuiWidgetGetEffectiveEnabled(pWidget) ) return 0;
	for ( pScan = pWidget; pScan != NULL; pScan = xuiInternalInputFocusParent(pScan) ) {
		if ( !__xuiAccessibleValid(pScan) || pScan->bInteractionCancelling ||
			!xuiWidgetGetEffectiveVisible(pScan) || !xuiWidgetGetEffectiveEnabled(pScan) ) return 0;
		pSlow = xuiInternalInputFocusParent(pSlow);
		pFast = xuiInternalInputFocusParent(xuiInternalInputFocusParent(pFast));
		if ( pSlow != NULL && pSlow == pFast ) return 0;
	}
	pModal = xuiInternalInputModalRoot(pWidget->pContext);
	return pModal == NULL || xuiInternalInputFocusContains(pModal, pWidget);
}

static const xui_internal_accessibility_adapter_t* __xuiAccessibleAdapter(xui_widget pWidget)
{
	xui_widget_type pType;
	for ( pType = pWidget->pType; pType != NULL; pType = pType->pParent ) {
		if ( pType->pAccessibleAdapter != NULL ) return pType->pAccessibleAdapter;
	}
	return NULL;
}

static xui_widget __xuiAccessibleEditTarget(xui_widget pWidget)
{
	int iDepth;
	for ( iDepth = 0; iDepth < 8 && __xuiAccessibleValid(pWidget); iDepth++ ) {
		if ( pWidget->pEditDelegate == NULL ) return pWidget;
		pWidget = pWidget->pEditDelegate;
	}
	return NULL;
}

static int __xuiAccessibleRole(xui_widget pWidget)
{
	static const struct { const char* sType; int iRole; } roles[] = {
		{"label", XUI_ACCESSIBLE_ROLE_TEXT}, {"hyperlink", XUI_ACCESSIBLE_ROLE_LINK},
		{"image", XUI_ACCESSIBLE_ROLE_IMAGE}, {"separator", XUI_ACCESSIBLE_ROLE_SEPARATOR},
		{"webview", XUI_ACCESSIBLE_ROLE_EMBEDDED_OBJECT},
		{"panel", XUI_ACCESSIBLE_ROLE_GROUP}, {"radiogroup", XUI_ACCESSIBLE_ROLE_RADIO_GROUP},
		{"listview", XUI_ACCESSIBLE_ROLE_LIST}, {"messagelist", XUI_ACCESSIBLE_ROLE_LIST},
		{"inventorygrid", XUI_ACCESSIBLE_ROLE_LIST},
		{"treeview", XUI_ACCESSIBLE_ROLE_TREE}, {"menu", XUI_ACCESSIBLE_ROLE_MENU},
		{"menubar", XUI_ACCESSIBLE_ROLE_MENU}, {"toolbar", XUI_ACCESSIBLE_ROLE_TOOLBAR},
		{"tableview", XUI_ACCESSIBLE_ROLE_TABLE}, {"tablegrid", XUI_ACCESSIBLE_ROLE_TABLE},
		{"propertygrid", XUI_ACCESSIBLE_ROLE_TABLE}, {"tabs", XUI_ACCESSIBLE_ROLE_TAB_LIST},
		{"scrollbar", XUI_ACCESSIBLE_ROLE_SCROLLBAR}, {"progress", XUI_ACCESSIBLE_ROLE_PROGRESS}
	};
	xui_widget_type pType;
	size_t i;
	if ( pWidget == pWidget->pContext->pRoot ) return XUI_ACCESSIBLE_ROLE_ROOT;
	for ( pType = pWidget->pType; pType != NULL; pType = pType->pParent ) {
		for ( i = 0; i < sizeof(roles) / sizeof(roles[0]); i++ ) {
			if ( strcmp(pType->sName, roles[i].sType) == 0 ) return roles[i].iRole;
		}
	}
	return XUI_ACCESSIBLE_ROLE_GENERIC;
}

/* Drain before publishing borrowed strings, while destruction is still deferred.
 * A provider that mutates during a query cannot return a pre-mutation snapshot. */
static void __xuiAccessibleDrain(xui_context pContext)
{
	if ( !xuiInternalContextDestroyPending(pContext) && pContext->iOperationDepth == 1 &&
		!pContext->bAccessibilityFlushing ) xuiInternalStateChangeFlush(pContext);
}

XUI_API int xuiWidgetSetAccessibilityProvider(xui_widget pWidget,
	xui_accessible_count_proc onCount, xui_accessible_get_proc onGet,
	xui_accessible_action_proc onAction, void* pUser)
{
	if ( !__xuiAccessibleValid(pWidget) || ((onCount == NULL) != (onGet == NULL)) )
		return XUI_ERROR_INVALID_ARGUMENT;
	pWidget->onAccessibleCount = onCount;
	pWidget->onAccessibleGet = onGet;
	pWidget->onAccessibleAction = onAction;
	pWidget->onAccessibleFocus = NULL;
	pWidget->pAccessibleUser = (onCount != NULL || onAction != NULL) ? pUser : NULL;
	pWidget->iAccessibleProviderGeneration++;
	return xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED, 0);
}

XUI_API int xuiInternalAccessibilitySetFocusResolver(xui_widget pWidget,
	xui_internal_accessible_focus_proc onFocus)
{
	if ( !__xuiAccessibleValid(pWidget) ||
	     (onFocus != NULL && pWidget->onAccessibleGet == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->onAccessibleFocus = onFocus;
	pWidget->iAccessibleProviderGeneration++;
	return XUI_OK;
}

XUI_API int xuiInternalAccessibilityGetFocusedNode(xui_widget pWidget,
	uint64_t* pNodeId)
{
	xui_context pContext;
	uint32_t iGeneration;
	uint32_t iMutation;
	int iRet;

	if ( pNodeId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pNodeId = 0;
	if ( !__xuiAccessibleValid(pWidget) || pWidget->bAccessibleQueryActive ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->onAccessibleFocus == NULL ) return XUI_ERROR_UNSUPPORTED;
	pContext = pWidget->pContext;
	xuiInternalOperationEnter(pContext);
	pWidget->bAccessibleQueryActive = 1;
	__xuiAccessibleDrain(pContext);
	iGeneration = pWidget->iAccessibleProviderGeneration;
	iMutation = pWidget->iAccessibleMutationGeneration;
	iRet = __xuiAccessibleValid(pWidget) && pWidget->onAccessibleFocus != NULL ?
		pWidget->onAccessibleFocus(pWidget, pNodeId, pWidget->pAccessibleUser) :
		XUI_ERROR_INVALID_ARGUMENT;
	__xuiAccessibleDrain(pContext);
	if ( !__xuiAccessibleValid(pWidget) ||
	     iGeneration != pWidget->iAccessibleProviderGeneration ||
	     iMutation != pWidget->iAccessibleMutationGeneration ) {
		iRet = XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet != XUI_OK ) *pNodeId = 0;
	pWidget->bAccessibleQueryActive = 0;
	xuiInternalOperationLeave(pContext);
	return iRet;
}

static int __xuiAccessibleSetText(xui_widget pWidget, char** psTarget, const char* sText)
{
	char* sCopy = NULL;
	if ( !__xuiAccessibleValid(pWidget) || psTarget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText != NULL && sText[0] != 0 ) {
		size_t iLength = strlen(sText) + 1;
		sCopy = (char*)xrtMalloc(iLength);
		if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		memcpy(sCopy, sText, iLength);
	}
	xrtFree(*psTarget);
	*psTarget = sCopy;
	return xuiWidgetNotifyAccessibility(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED, 1);
}

XUI_API int xuiWidgetSetAccessibleName(xui_widget pWidget, const char* sName)
{
	return __xuiAccessibleSetText(pWidget, pWidget != NULL ? &pWidget->sAccessibleName : NULL, sName);
}

XUI_API const char* xuiWidgetGetAccessibleName(xui_widget pWidget)
{
	return __xuiAccessibleValid(pWidget) ? pWidget->sAccessibleName : NULL;
}

XUI_API int xuiWidgetSetAccessibleDescription(xui_widget pWidget, const char* sDescription)
{
	return __xuiAccessibleSetText(pWidget,
		pWidget != NULL ? &pWidget->sAccessibleDescription : NULL, sDescription);
}

XUI_API const char* xuiWidgetGetAccessibleDescription(xui_widget pWidget)
{
	return __xuiAccessibleValid(pWidget) ? pWidget->sAccessibleDescription : NULL;
}

XUI_API int xuiWidgetGetAccessibleNodeCount(xui_widget pWidget)
{
	xui_context pContext;
	uint32_t iGeneration, iMutation;
	int iCount = 0;
	if ( !__xuiAccessibleValid(pWidget) || pWidget->bAccessibleQueryActive ) return 0;
	pContext = pWidget->pContext;
	xuiInternalOperationEnter(pContext);
	pWidget->bAccessibleQueryActive = 1;
	__xuiAccessibleDrain(pContext);
	iGeneration = pWidget->iAccessibleProviderGeneration;
	iMutation = pWidget->iAccessibleMutationGeneration;
	if ( __xuiAccessibleValid(pWidget) ) {
		iCount = pWidget->onAccessibleCount != NULL ?
			pWidget->onAccessibleCount(pWidget, pWidget->pAccessibleUser) : 1;
	}
	__xuiAccessibleDrain(pContext);
	if ( !__xuiAccessibleValid(pWidget) || iGeneration != pWidget->iAccessibleProviderGeneration ||
		iMutation != pWidget->iAccessibleMutationGeneration ) iCount = 0;
	pWidget->bAccessibleQueryActive = 0;
	xuiInternalOperationLeave(pContext);
	return iCount > 0 ? iCount : 0;
}

static int __xuiAccessibleDefaultNode(xui_widget pWidget, xui_accessible_node_t* pNode)
{
	const xui_internal_accessibility_adapter_t* pAdapter = __xuiAccessibleAdapter(pWidget);
	int iRet = XUI_OK;
	pNode->iId = 1;
	pNode->iRole = __xuiAccessibleRole(pWidget);
	pNode->tBounds = xuiWidgetGetWorldRect(pWidget);
	if ( pAdapter != NULL && pAdapter->getNode != NULL ) iRet = pAdapter->getNode(pWidget, pNode);
	if ( pWidget->onDefaultAction == NULL && (pAdapter == NULL || pAdapter->performAction == NULL) )
		pNode->iActions &= ~(XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_ACTIVATE) |
			XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_TOGGLE));
	if ( pWidget->sAccessibleName != NULL ) pNode->sName = pWidget->sAccessibleName;
	return iRet;
}

XUI_API int xuiWidgetGetAccessibleNode(xui_widget pWidget, int iIndex, xui_accessible_node_t* pNode)
{
	xui_context pContext;
	xui_accessible_node_t tNode;
	uint32_t iGeneration, iRevision, iMutation;
	size_t iSize;
	int iRet = XUI_ERROR_INVALID_ARGUMENT;
	if ( pNode == NULL ) return iRet;
	iSize = pNode->iSize != 0 ? pNode->iSize : sizeof(*pNode);
	if ( iSize < offsetof(xui_accessible_node_t, iActions) ) return iRet;
	if ( iSize > sizeof(*pNode) ) iSize = sizeof(*pNode);
	memset(pNode, 0, iSize);
	pNode->iSize = (uint32_t)iSize;
	if ( !__xuiAccessibleValid(pWidget) || iIndex < 0 || pWidget->bAccessibleQueryActive ) return iRet;
	pContext = pWidget->pContext;
	xuiInternalOperationEnter(pContext);
	pWidget->bAccessibleQueryActive = 1;
	__xuiAccessibleDrain(pContext);
	iGeneration = pWidget->iAccessibleProviderGeneration;
	iRevision = pWidget->iAccessibilityRevision;
	iMutation = pWidget->iAccessibleMutationGeneration;
	memset(&tNode, 0, sizeof(tNode));
	tNode.iSize = sizeof(tNode);
	if ( __xuiAccessibleValid(pWidget) ) {
		if ( pWidget->onAccessibleGet != NULL ) {
			iRet = pWidget->onAccessibleGet(pWidget, iIndex, &tNode, pWidget->pAccessibleUser);
		} else if ( iIndex == 0 ) iRet = __xuiAccessibleDefaultNode(pWidget, &tNode);
	}
	__xuiAccessibleDrain(pContext);
	if ( !__xuiAccessibleValid(pWidget) || iGeneration != pWidget->iAccessibleProviderGeneration ||
		iRevision != pWidget->iAccessibilityRevision ||
		iMutation != pWidget->iAccessibleMutationGeneration ) iRet = XUI_ERROR_INVALID_ARGUMENT;
	if ( iRet == XUI_OK ) {
		xui_widget pFocusTarget = __xuiAccessibleEditTarget(pWidget);
		int bRootNode = pWidget->onAccessibleGet == NULL || iIndex == 0;
		if ( tNode.sName == NULL ) tNode.sName = pWidget->sAccessibleName;
		if ( tNode.sDescription == NULL ) tNode.sDescription = pWidget->sAccessibleDescription;
		if ( (tNode.iState & XUI_ACCESSIBLE_STATE_DISABLED) == 0 &&
		     xuiInternalInputFocusAllowed(pContext, pFocusTarget) ) {
			if ( bRootNode ||
			     (tNode.iActions & XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS)) != 0 )
				tNode.iState |= XUI_ACCESSIBLE_STATE_FOCUSABLE;
			if ( bRootNode &&
			     (pWidget->onAccessibleGet == NULL || pWidget->onAccessibleAction != NULL || tNode.iId <= 1) )
				tNode.iActions |= XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS);
		}
		if ( (tNode.iState & XUI_ACCESSIBLE_STATE_DISABLED) == 0 &&
		     bRootNode &&
		     (pContext->pFocusWidget == pWidget ||
		      (pFocusTarget != NULL && pContext->pFocusWidget == pFocusTarget)) )
			tNode.iState |= XUI_ACCESSIBLE_STATE_FOCUSED;
		if ( (tNode.iState & XUI_ACCESSIBLE_STATE_DISABLED) != 0 ) {
			tNode.iActions = 0;
			tNode.iState &= ~(XUI_ACCESSIBLE_STATE_FOCUSABLE | XUI_ACCESSIBLE_STATE_FOCUSED);
		}
		if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) tNode.iState |= XUI_ACCESSIBLE_STATE_DISABLED;
		if ( !xuiWidgetGetEffectiveVisible(pWidget) ) tNode.iState |= XUI_ACCESSIBLE_STATE_OFFSCREEN;
		if ( !__xuiAccessibleCanAct(pWidget) ) {
			tNode.iActions = 0;
			tNode.iState &= ~(XUI_ACCESSIBLE_STATE_FOCUSABLE | XUI_ACCESSIBLE_STATE_FOCUSED);
		}
		tNode.iSize = (uint32_t)iSize;
		memcpy(pNode, &tNode, iSize);
	}
	pWidget->bAccessibleQueryActive = 0;
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiInternalAccessibilityCreateTextRange(xui_widget widget, uint64_t id,
	const xui_internal_accessible_text_request* request,
	xui_internal_accessible_text_range** out)
{
	xui_context context;
	const xui_internal_accessibility_adapter_t* adapter;
	xui_internal_accessible_text_range* range = NULL;
	uint32_t generation, mutation;
	int result = XUI_ERROR_UNSUPPORTED;
	if (!out) return XUI_ERROR_INVALID_ARGUMENT;
	*out = NULL;
	if (!request || !__xuiAccessibleValid(widget) || widget->bAccessibleQueryActive ||
		widget->bAccessibleActionActive) return XUI_ERROR_INVALID_ARGUMENT;
	context = widget->pContext;
	xuiInternalOperationEnter(context);
	widget->bAccessibleQueryActive = 1;
	__xuiAccessibleDrain(context);
	generation = widget->iAccessibleProviderGeneration;
	mutation = widget->iAccessibleMutationGeneration;
	if (__xuiAccessibleValid(widget)) {
		adapter = __xuiAccessibleAdapter(widget);
		if (adapter && adapter->createTextRange)
			result = adapter->createTextRange(widget, id, request, &range);
	}
	__xuiAccessibleDrain(context);
	if (!__xuiAccessibleValid(widget) || generation != widget->iAccessibleProviderGeneration ||
		mutation != widget->iAccessibleMutationGeneration) result = XUI_ERROR_INVALID_STATE;
	if (result == XUI_OK && !range) result = XUI_ERROR_INVALID_STATE;
	if (result == XUI_OK && range) *out = range;
	else if (range) range->ops->release(range);
	widget->bAccessibleQueryActive = 0;
	xuiInternalOperationLeave(context);
	return result;
}

XUI_API int xuiWidgetPerformAccessibleAction(xui_widget pWidget, uint64_t iNodeId,
	int iAction, const void* pData)
{
	xui_context pContext;
	const xui_internal_accessibility_adapter_t* pAdapter;
	xui_accessible_node_t tNode;
	int iRet = XUI_ERROR_UNSUPPORTED;
	if ( !__xuiAccessibleCanAct(pWidget) || pWidget->bAccessibleActionActive ||
		pWidget->bAccessibleQueryActive )
		return XUI_ERROR_INVALID_ARGUMENT;
	pContext = pWidget->pContext;
	xuiInternalOperationEnter(pContext);
	pWidget->bAccessibleActionActive = 1;
	if ( pWidget->onAccessibleAction != NULL ) {
		iRet = pWidget->onAccessibleAction(pWidget, iNodeId, iAction, pData, pWidget->pAccessibleUser);
	} else if ( iAction == XUI_ACCESSIBLE_ACTION_FOCUS && (iNodeId == 0 || iNodeId == 1) ) {
		xui_widget pTarget = __xuiAccessibleEditTarget(pWidget);
		iRet = pTarget != NULL ? xuiSetFocusWidget(pContext, pTarget) : XUI_ERROR_INVALID_ARGUMENT;
	} else if ( pWidget->onAccessibleGet == NULL && (iNodeId == 0 || iNodeId == 1) &&
		iAction >= 1 && iAction <= XUI_ACCESSIBLE_ACTION_CLOSE ) {
		memset(&tNode, 0, sizeof(tNode));
		tNode.iSize = sizeof(tNode);
		pAdapter = __xuiAccessibleAdapter(pWidget);
		if ( __xuiAccessibleDefaultNode(pWidget, &tNode) == XUI_OK &&
			(tNode.iActions & XUI_ACCESSIBLE_ACTION_MASK(iAction)) != 0 ) {
			if ( (iAction == XUI_ACCESSIBLE_ACTION_ACTIVATE || iAction == XUI_ACCESSIBLE_ACTION_TOGGLE) &&
				pWidget->onDefaultAction != NULL ) {
				iRet=xuiInternalInvokeDefaultAction(pWidget);
			} else if ( pAdapter != NULL && pAdapter->performAction != NULL ) {
				iRet = pAdapter->performAction(pWidget, iAction, pData);
			}
		}
	}
	/* Successful commands may intentionally destroy their widget or context. */
	pWidget->bAccessibleActionActive = 0;
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API uint32_t xuiWidgetGetAccessibilityRevision(xui_widget pWidget)
{
	return __xuiAccessibleValid(pWidget) ? pWidget->iAccessibilityRevision : 0;
}

XUI_API int xuiWidgetNotifyAccessibility(xui_widget pWidget, int iEventType, uint64_t iNodeId)
{
	xui_context pContext;
	xui_accessibility_event_t tEvent;
	uint32_t iBit;
	if ( !__xuiAccessibleValid(pWidget) || iEventType < XUI_ACCESSIBLE_EVENT_TREE_CHANGED ||
		iEventType > XUI_ACCESSIBLE_EVENT_STATE_CHANGED ) return XUI_ERROR_INVALID_ARGUMENT;
	iBit = XUI_A11Y_BIT(iEventType);
	pWidget->iAccessibleMutationGeneration++;
	if ( (pWidget->iActiveAccessibilityEvents & iBit) != 0 ) return XUI_OK;
	pContext = pWidget->pContext;
	xuiInternalOperationEnter(pContext);
	pWidget->iActiveAccessibilityEvents |= iBit;
	pWidget->iAccessibilityRevision++;
	if ( pWidget->iAccessibilityRevision == 0 ) pWidget->iAccessibilityRevision = 1;
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iSize = sizeof(tEvent);
	tEvent.iType = iEventType;
	tEvent.iNodeId = iNodeId;
	tEvent.iRevision = pWidget->iAccessibilityRevision;
	xuiInternalNativeAccessibilityNotify(pContext, pWidget, &tEvent);
	if ( pContext->onAccessibilityEvent != NULL ) {
		pContext->onAccessibilityEvent(pContext, pWidget, &tEvent, pContext->pAccessibilityEventUser);
	}
	pWidget->iActiveAccessibilityEvents &= ~iBit;
	xuiInternalOperationLeave(pContext);
	return XUI_OK;
}

void xuiInternalAccessibilityQueue(xui_widget pWidget, int iEventType)
{
	xui_context pContext;
	uint32_t iBit;
	if ( !__xuiAccessibleValid(pWidget) || iEventType < XUI_ACCESSIBLE_EVENT_TREE_CHANGED ||
		iEventType > XUI_ACCESSIBLE_EVENT_STATE_CHANGED ) return;
	pContext = pWidget->pContext;
	pWidget->iAccessibleMutationGeneration++;
	iBit = XUI_A11Y_BIT(iEventType);
	if ( (pWidget->iActiveAccessibilityEvents & iBit) != 0 ||
		(pContext->bAccessibilityFlushing && pWidget->iAccessibilitySeenEpoch == pContext->iAccessibilityFlushEpoch &&
		(pWidget->iAccessibilitySeenEvents & iBit) != 0) ) return;
	if ( pWidget->iPendingAccessibilityEvents == 0 ) {
		pWidget->pAccessibilityPrev = pContext->pAccessibilityTail;
		pWidget->pAccessibilityNext = NULL;
		if ( pContext->pAccessibilityTail != NULL ) pContext->pAccessibilityTail->pAccessibilityNext = pWidget;
		else pContext->pAccessibilityHead = pWidget;
		pContext->pAccessibilityTail = pWidget;
	}
	pWidget->iPendingAccessibilityEvents |= iBit;
	if ( iEventType == XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED ) {
		xui_widget pHost = xuiInternalEditHost(pWidget);
		if ( pHost != pWidget ) xuiInternalAccessibilityQueue(pHost, iEventType);
	}
}

void xuiInternalAccessibilityDetach(xui_widget pWidget)
{
	xui_context pContext = pWidget->pContext;
	if ( pWidget->iPendingAccessibilityEvents == 0 ) return;
	if ( pWidget->pAccessibilityPrev != NULL ) pWidget->pAccessibilityPrev->pAccessibilityNext = pWidget->pAccessibilityNext;
	else pContext->pAccessibilityHead = pWidget->pAccessibilityNext;
	if ( pWidget->pAccessibilityNext != NULL ) pWidget->pAccessibilityNext->pAccessibilityPrev = pWidget->pAccessibilityPrev;
	else pContext->pAccessibilityTail = pWidget->pAccessibilityPrev;
	pWidget->pAccessibilityPrev = pWidget->pAccessibilityNext = NULL;
	pWidget->iPendingAccessibilityEvents = 0;
}

/* Only publish at the outer update barrier: a control setter can call nested
 * Widget operations while it still owns pointers needed after those calls. */
void xuiInternalAccessibilityFlush(xui_context pContext)
{
	xui_widget pWidget;
	if ( !xuiInternalContextIsValid(pContext) || xuiInternalContextDestroyPending(pContext) ||
		pContext->bAccessibilityFlushing ) return;
	pContext->bAccessibilityFlushing = 1;
	pContext->iAccessibilityFlushEpoch++;
	if ( pContext->iAccessibilityFlushEpoch == 0 ) pContext->iAccessibilityFlushEpoch = 1;
	while ( !xuiInternalContextDestroyPending(pContext) && (pWidget = pContext->pAccessibilityHead) != NULL ) {
		uint32_t iEvents = pWidget->iPendingAccessibilityEvents;
		int iType;
		xuiInternalAccessibilityDetach(pWidget);
		if ( !__xuiAccessibleValid(pWidget) ) continue;
		if ( pWidget->iAccessibilitySeenEpoch != pContext->iAccessibilityFlushEpoch ) {
			pWidget->iAccessibilitySeenEpoch = pContext->iAccessibilityFlushEpoch;
			pWidget->iAccessibilitySeenEvents = 0;
		}
		/* Bound cross-widget callback cycles to one event of each kind per drain. */
		iEvents &= ~pWidget->iAccessibilitySeenEvents;
		pWidget->iAccessibilitySeenEvents |= iEvents;
		for ( iType = XUI_ACCESSIBLE_EVENT_TREE_CHANGED; iType <= XUI_ACCESSIBLE_EVENT_STATE_CHANGED; iType++ ) {
			if ( !__xuiAccessibleValid(pWidget) ) break;
			if ( (iEvents & XUI_A11Y_BIT(iType)) != 0 )
				(void)xuiWidgetNotifyAccessibility(pWidget, iType, 1);
		}
	}
	pContext->bAccessibilityFlushing = 0;
}

int xuiInternalAccessibilityEditNode(xui_widget pWidget, xui_accessible_node_t* pNode)
{
	xui_widget pTarget = __xuiAccessibleEditTarget(pWidget);
	if ( pTarget != NULL && pTarget != pWidget ) {
		const xui_internal_accessibility_adapter_t* pAdapter = __xuiAccessibleAdapter(pTarget);
		if ( pAdapter != NULL && pAdapter->getNode != NULL ) return pAdapter->getNode(pTarget, pNode);
	}
	pNode->iRole = XUI_ACCESSIBLE_ROLE_TEXTBOX;
	pNode->sValue = xuiEditGetText(pWidget);
	pNode->iActions |= XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SET_SELECTION);
	if ( xuiEditIsReadonly(pWidget) ) pNode->iState |= XUI_ACCESSIBLE_STATE_READONLY;
	else {
		pNode->iState |= XUI_ACCESSIBLE_STATE_EDITABLE;
		pNode->iActions |= XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SET_VALUE);
	}
	(void)xuiEditGetSelection(pWidget, &pNode->iTextStart, &pNode->iTextEnd);
	return XUI_OK;
}

int xuiInternalAccessibilityEditAction(xui_widget pWidget, int iAction, const void* pData)
{
	if ( !__xuiAccessibleCanAct(__xuiAccessibleEditTarget(pWidget)) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iAction == XUI_ACCESSIBLE_ACTION_SET_SELECTION ) {
		const xui_accessible_selection_t* pSelection = (const xui_accessible_selection_t*)pData;
		if ( pSelection == NULL || pSelection->iSize < sizeof(*pSelection) ||
			pSelection->iAnchor < 0 || pSelection->iCaret < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
		return xuiEditSetSelection(pWidget, pSelection->iAnchor, pSelection->iCaret);
	}
	if ( iAction == XUI_ACCESSIBLE_ACTION_SET_VALUE && !xuiEditIsReadonly(pWidget) ) {
		const xui_accessible_value_t* pValue = (const xui_accessible_value_t*)pData;
		if ( pValue == NULL || pValue->iSize < sizeof(*pValue) ||
			pValue->iType != XUI_ACCESSIBLE_VALUE_TEXT || pValue->sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
		return xuiEditSetText(pWidget, pValue->sText);
	}
	return XUI_ERROR_UNSUPPORTED;
}

#endif
