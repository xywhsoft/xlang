#include "xui_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XUI_RENDER_CACHE_DIRTY_FLAGS \
	(XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE)
#define XUI_RENDER_SUBTREE_DIRTY_FLAGS \
	(XUI_RENDER_CACHE_DIRTY_FLAGS | XUI_WIDGET_DIRTY_RENDER)

static int __xuiWidgetValid(xui_widget pWidget)
{
	return (pWidget != NULL) && (pWidget->iMagic == XUI_WIDGET_MAGIC);
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
	XUI_WIDGET_TYPE_MAGIC,
	NULL,
	NULL,
	NULL,
	"widget",
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	{0},
	{0},
	0,
	1
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

static int __xuiWidgetCreateInternal(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData);

static int __xuiFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_CONTEXT_MAX_VIEWPORT) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiRectValidFloat(xui_rect_t tRect)
{
	return __xuiFloatValid(tRect.fX) &&
	       __xuiFloatValid(tRect.fY) &&
	       (tRect.fW == tRect.fW) &&
	       (tRect.fH == tRect.fH) &&
	       (tRect.fW >= 0.0f) &&
	       (tRect.fH >= 0.0f) &&
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
	return __xuiSizeModeValid(pTrack->iSizeMode) &&
	       __xuiNonNegativeFloatValid(pTrack->fValue) &&
	       __xuiNonNegativeFloatValid(pTrack->fMin) &&
	       __xuiNonNegativeFloatValid(pTrack->fMax) &&
	       __xuiNonNegativeFloatValid(pTrack->fWeight) &&
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
	pDesc->fDelay = 0.35f;
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
	     !__xuiRectValidFloat(pDesc->tAnchorRect) ||
	     !__xuiFloatValid(pDesc->fOffsetX) ||
	     !__xuiFloatValid(pDesc->fOffsetY) ||
	     (pDesc->fDelay != pDesc->fDelay) ||
	     (pDesc->fDelay > XUI_LAYOUT_UNBOUNDED) ) {
		return 0;
	}
	return 1;
}

static char* __xuiStringDuplicate(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText) + 1u;
	sCopy = (char*)xrtMalloc(iSize);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iSize);
	return sCopy;
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

static xui_style_property_entry_t* __xuiStyleFindPropertyById(xui_context pContext, uint32_t iPropertyId)
{
	xui_style_property_entry_t* pEntry;

	if ( iPropertyId == 0 ) {
		return NULL;
	}
	for ( pEntry = pContext->pStyleProperties; pEntry != NULL; pEntry = pEntry->pNext ) {
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
	for ( pEntry = pContext->pStyleProperties; pEntry != NULL; pEntry = pEntry->pNext ) {
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
	pWidget->tLayout.iOverflow = XUI_OVERFLOW_VISIBLE;
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
	pWidget->tLayout.fShrink = 1.0f;
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

static uint64_t __xuiWidgetRecomputeSubtreeEventMask(xui_widget pWidget)
{
	xui_widget pChild;
	uint64_t iMask;

	if ( !__xuiWidgetValid(pWidget) ) {
		return 0ull;
	}
	iMask = __xuiWidgetOwnEventMask(pWidget);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iMask |= __xuiWidgetRecomputeSubtreeEventMask(pChild);
	}
	pWidget->iSubtreeEventMask = iMask;
	return iMask;
}

static void __xuiWidgetUpdateEventMasksToRoot(xui_widget pWidget)
{
	xui_widget pScan;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		(void)__xuiWidgetRecomputeSubtreeEventMask(pScan);
	}
}

static void __xuiWidgetInvalidateMeasureTree(xui_widget pWidget)
{
	xui_widget pParent;

	pWidget->bMeasureValid = 0;
	pWidget->bArrangeValid = 0;
	for ( pParent = pWidget->pParent; pParent != NULL; pParent = pParent->pParent ) {
		pParent->bMeasureValid = 0;
		pParent->bArrangeValid = 0;
	}
}

static void __xuiWidgetMarkDirtyOnly(xui_widget pWidget, uint32_t iFlags)
{
	xui_widget pParent;

	if ( iFlags == 0 ) {
		iFlags = XUI_WIDGET_DIRTY_RENDER;
	}
	if ( (iFlags & (XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_TREE)) != 0 ) {
		__xuiWidgetInvalidateMeasureTree(pWidget);
	}
	pWidget->iDirtyFlags |= iFlags;
	pWidget->iSubtreeDirtyFlags |= iFlags;
	__xuiWidgetBumpGeneration(pWidget);
	for ( pParent = pWidget->pParent; pParent != NULL; pParent = pParent->pParent ) {
		pParent->iSubtreeDirtyFlags |= iFlags;
		__xuiWidgetBumpGeneration(pParent);
	}
	xuiInternalContextBumpGeneration(pWidget->pContext);
}

static uint32_t __xuiWidgetRecomputeSubtreeDirtyFlags(xui_widget pWidget)
{
	xui_widget pChild;
	uint32_t iFlags;

	iFlags = pWidget->iDirtyFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iFlags |= __xuiWidgetRecomputeSubtreeDirtyFlags(pChild);
	}
	pWidget->iSubtreeDirtyFlags = iFlags;
	return iFlags;
}

static void __xuiWidgetClearDirtyRecursive(xui_widget pWidget, uint32_t iFlags)
{
	xui_widget pChild;

	pWidget->iDirtyFlags &= ~iFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetClearDirtyRecursive(pChild, iFlags);
	}
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

static void __xuiWidgetDestroyCacheSlot(xui_widget pWidget, xui_widget_cache_slot_t* pSlot)
{
	xui_proxy pProxy;

	if ( pSlot->pSurface != NULL ) {
		pProxy = xuiInternalContextGetProxy(pWidget->pContext);
		if ( (pProxy != NULL) && (pProxy->surfaceDestroy != NULL) ) {
			pProxy->surfaceDestroy(pProxy, pSlot->pSurface);
		}
	}
	memset(pSlot, 0, sizeof(*pSlot));
}

static void __xuiWidgetDestroyAllCaches(xui_widget pWidget)
{
	int i;

	if ( pWidget->pCacheSlots == NULL ) {
		return;
	}
	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		__xuiWidgetDestroyCacheSlot(pWidget, &pWidget->pCacheSlots[i]);
	}
	xrtFree(pWidget->pCacheSlots);
	pWidget->pCacheSlots = NULL;
	pWidget->iCacheCount = 0;
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

static void __xuiWidgetPurgeCacheSlotSurface(xui_widget pWidget, xui_widget_cache_slot_t* pSlot)
{
	xui_proxy pProxy;

	if ( (pWidget == NULL) || (pSlot == NULL) || (pSlot->pSurface == NULL) ) {
		return;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( (pProxy != NULL) && (pProxy->surfaceDestroy != NULL) ) {
		pProxy->surfaceDestroy(pProxy, pSlot->pSurface);
	}
	pSlot->pSurface = NULL;
	pSlot->iWidth = 0;
	pSlot->iHeight = 0;
	pSlot->iSurfaceGeneration = 0;
	pSlot->iFlags |= XUI_WIDGET_DIRTY_CACHE;
	pWidget->iDirtyFlags |= XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	pWidget->iSubtreeDirtyFlags |= XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
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

static void __xuiWidgetEndActiveUpdate(xui_widget pWidget)
{
	xui_proxy pProxy;

	if ( pWidget->pActiveUpdateDraw == NULL ) {
		return;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( (pProxy != NULL) && (pProxy->drawEnd != NULL) ) {
		(void)pProxy->drawEnd(pProxy, pWidget->pActiveUpdateDraw);
	}
	pWidget->pActiveUpdateDraw = NULL;
	pWidget->pActiveUpdateSlot = NULL;
	pWidget->iActiveUpdateStateId = 0;
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
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_SUBTREE_TILED ) {
		return XUI_CACHE_POLICY_SUBTREE;
	}
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_DISPLAY_LIST ) {
		return XUI_CACHE_POLICY_SELF;
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

static int __xuiWidgetAppendRenderNode(xui_context pContext, xui_widget pWidget, xui_rect_t tPaintRect)
{
	xui_render_node_t* pNode;
	int iRet;

	iRet = __xuiWidgetReserveRenderNodes(pContext, pContext->iRenderNodeCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pNode = &pContext->pRenderNodes[pContext->iRenderNodeCount++];
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
	return XUI_OK;
}

static int __xuiWidgetRenderNodeGreater(const xui_render_node_t* pA, const xui_render_node_t* pB)
{
	if ( pA->iLayer != pB->iLayer ) {
		return pA->iLayer > pB->iLayer;
	}
	return pA->iZIndex > pB->iZIndex;
}

static void __xuiWidgetSortRenderNodes(xui_context pContext)
{
	xui_render_node_t tNode;
	int i;
	int j;

	for ( i = 1; i < pContext->iRenderNodeCount; i++ ) {
		tNode = pContext->pRenderNodes[i];
		j = i - 1;
		while ( (j >= 0) && __xuiWidgetRenderNodeGreater(&pContext->pRenderNodes[j], &tNode) ) {
			pContext->pRenderNodes[j + 1] = pContext->pRenderNodes[j];
			j--;
		}
		pContext->pRenderNodes[j + 1] = tNode;
	}
}

static int __xuiWidgetBuildRenderTreeRecursive(xui_context pContext, xui_widget pWidget, xui_rect_t tParentClip, int bHasClip)
{
	xui_widget pChild;
	xui_rect_t tWorldRect;
	xui_rect_t tPaintRect;
	xui_rect_t tChildClip;
	int bChildHasClip;
	int iRet;

	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	tWorldRect = xuiInternalSnapRect(xuiWidgetGetWorldRect(pWidget));
	tPaintRect = bHasClip ? __xuiWidgetIntersectRect(tWorldRect, tParentClip) : tWorldRect;
	iRet = __xuiWidgetAppendRenderNode(pContext, pWidget, tPaintRect);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tChildClip = tParentClip;
	bChildHasClip = bHasClip;
	if ( (pWidget->tLayout.iOverflow == XUI_OVERFLOW_HIDDEN) ||
	     (pWidget->tLayout.iOverflow == XUI_OVERFLOW_CLIP) ) {
		tChildClip = bHasClip ? __xuiWidgetIntersectRect(tPaintRect, tParentClip) : tPaintRect;
		bChildHasClip = 1;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWidgetBuildRenderTreeRecursive(pContext, pChild, tChildClip, bChildHasClip);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
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
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	if ( pSlot->pSurface != NULL ) {
		pProxy->surfaceDestroy(pProxy, pSlot->pSurface);
		memset(pSlot, 0, sizeof(*pSlot));
		pSlot->iStateId = pWidget->iActiveUpdateStateId;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iKind = XUI_SURFACE_KIND_TEXTURE;
	tDesc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
	tDesc.iWidth = iWidth;
	tDesc.iHeight = iHeight;
	tDesc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
	iRet = pProxy->surfaceCreate(pProxy, &pSlot->pSurface, &tDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pSlot->iWidth = iWidth;
	pSlot->iHeight = iHeight;
	pSlot->iFlags = XUI_WIDGET_DIRTY_CACHE;
	return XUI_OK;
}

static int __xuiWidgetCanAttachChild(xui_widget pParent, xui_widget pChild)
{
	xui_widget pScan;

	if ( !__xuiWidgetValid(pParent) || !__xuiWidgetValid(pChild) || (pParent == pChild) ) {
		return 0;
	}
	if ( pParent->pContext != pChild->pContext ) {
		return 0;
	}
	if ( (pChild->pParent != NULL) || (pChild->pPrevSibling != NULL) || (pChild->pNextSibling != NULL) ) {
		return 0;
	}
	if ( (pChild->pContext->pRoot == pChild) || (pChild->pContext->pOverlayRoot == pChild) ) {
		return 0;
	}
	for ( pScan = pParent; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan == pChild ) {
			return 0;
		}
	}
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

static xui_vec2_t __xuiWidgetMeasureOwnContent(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_vec2_t tSize;
	int iRet;

	tSize = __xuiVec2(0.0f, 0.0f);
	if ( pWidget->onContentMeasure == NULL ) {
		return tSize;
	}
	tConstraint.fX = __xuiFiniteConstraint(tConstraint.fX);
	tConstraint.fY = __xuiFiniteConstraint(tConstraint.fY);
	iRet = pWidget->onContentMeasure(pWidget, tConstraint, &tSize, pWidget->pContentMeasureUser);
	if ( (iRet != XUI_OK) || !__xuiWidgetSizeValid(tSize) ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	return __xuiWidgetClampSize(pWidget, tSize);
}

static xui_vec2_t __xuiWidgetMeasureCustomLayout(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_vec2_t tSize;
	int iRet;

	tSize = __xuiVec2(0.0f, 0.0f);
	if ( pWidget->onLayoutMeasure == NULL ) {
		return tSize;
	}
	tConstraint.fX = __xuiFiniteConstraint(tConstraint.fX);
	tConstraint.fY = __xuiFiniteConstraint(tConstraint.fY);
	iRet = pWidget->onLayoutMeasure(pWidget, tConstraint, &tSize, pWidget->pLayoutUser);
	if ( (iRet != XUI_OK) || !__xuiWidgetSizeValid(tSize) ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	return tSize;
}

static xui_vec2_t __xuiWidgetMaxSize(xui_vec2_t tA, xui_vec2_t tB)
{
	return __xuiVec2(__xuiMaxFloat(tA.fX, tB.fX), __xuiMaxFloat(tA.fY, tB.fY));
}

static float __xuiWidgetAxisPreferred(xui_widget pWidget, int bHorizontal, xui_vec2_t tContentSize, xui_vec2_t tConstraint)
{
	int iMode;
	float fPreferred;
	float fRectSize;
	float fContentSize;
	float fConstraint;

	iMode = bHorizontal ? pWidget->tLayout.iWidthMode : pWidget->tLayout.iHeightMode;
	fPreferred = bHorizontal ? pWidget->tLayout.fPreferredWidth : pWidget->tLayout.fPreferredHeight;
	fRectSize = bHorizontal ? pWidget->tRect.fW : pWidget->tRect.fH;
	fContentSize = bHorizontal ? tContentSize.fX : tContentSize.fY;
	fConstraint = bHorizontal ? tConstraint.fX : tConstraint.fY;
	if ( iMode == XUI_SIZE_FIXED ) {
		return (fPreferred > 0.0f) ? fPreferred : fRectSize;
	}
	if ( iMode == XUI_SIZE_FILL ) {
		if ( __xuiConstraintIsFinite(fConstraint) ) {
			return fConstraint;
		}
		return (fPreferred > 0.0f) ? fPreferred : fRectSize;
	}
	if ( fContentSize > 0.0f ) {
		return fContentSize;
	}
	if ( fPreferred > 0.0f ) {
		return fPreferred;
	}
	return fRectSize;
}

static int __xuiWidgetVisibleChildCount(xui_widget pWidget)
{
	xui_widget pChild;
	int iCount;

	iCount = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( pChild->bVisible ) {
			iCount++;
		}
	}
	return iCount;
}

static xui_vec2_t __xuiWidgetMeasureInternal(xui_widget pWidget, xui_vec2_t tConstraint);

static xui_vec2_t __xuiWidgetMeasureManual(xui_widget pWidget)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_vec2_t tSize;
	float fRight;
	float fBottom;

	tSize = __xuiVec2(0.0f, 0.0f);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED));
		fRight = pChild->tRect.fX + tChildSize.fX + pChild->tLayout.tMargin.fLeft + pChild->tLayout.tMargin.fRight;
		fBottom = pChild->tRect.fY + tChildSize.fY + pChild->tLayout.tMargin.fTop + pChild->tLayout.tMargin.fBottom;
		tSize.fX = __xuiMaxFloat(tSize.fX, fRight);
		tSize.fY = __xuiMaxFloat(tSize.fY, fBottom);
	}
	return tSize;
}

static xui_vec2_t __xuiWidgetMeasureOverlay(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_vec2_t tSize;
	xui_vec2_t tChildConstraint;

	tSize = __xuiVec2(0.0f, 0.0f);
	tChildConstraint.fX = __xuiFiniteConstraint(tConstraint.fX);
	tChildConstraint.fY = __xuiFiniteConstraint(tConstraint.fY);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tChildSize = __xuiWidgetMeasureInternal(pChild, tChildConstraint);
		tSize.fX = __xuiMaxFloat(tSize.fX, tChildSize.fX + pChild->tLayout.tMargin.fLeft + pChild->tLayout.tMargin.fRight);
		tSize.fY = __xuiMaxFloat(tSize.fY, tChildSize.fY + pChild->tLayout.tMargin.fTop + pChild->tLayout.tMargin.fBottom);
	}
	return tSize;
}

static xui_vec2_t __xuiWidgetMeasureLinear(xui_widget pWidget, xui_vec2_t tConstraint, int bHorizontal)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_vec2_t tChildConstraint;
	xui_vec2_t tSize;
	xui_thickness_t tMargin;
	int iCount;

	tSize = __xuiVec2(0.0f, 0.0f);
	iCount = 0;
	tChildConstraint = tConstraint;
	if ( bHorizontal ) {
		tChildConstraint.fX = XUI_LAYOUT_UNBOUNDED;
	} else {
		tChildConstraint.fY = XUI_LAYOUT_UNBOUNDED;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildSize = __xuiWidgetMeasureInternal(pChild, tChildConstraint);
		if ( bHorizontal ) {
			tSize.fX += tChildSize.fX + tMargin.fLeft + tMargin.fRight;
			tSize.fY = __xuiMaxFloat(tSize.fY, tChildSize.fY + tMargin.fTop + tMargin.fBottom);
		} else {
			tSize.fX = __xuiMaxFloat(tSize.fX, tChildSize.fX + tMargin.fLeft + tMargin.fRight);
			tSize.fY += tChildSize.fY + tMargin.fTop + tMargin.fBottom;
		}
		iCount++;
	}
	if ( iCount > 1 ) {
		if ( bHorizontal ) {
			tSize.fX += pWidget->tLayout.fGap * (float)(iCount - 1);
		} else {
			tSize.fY += pWidget->tLayout.fGap * (float)(iCount - 1);
		}
	}
	return tSize;
}

static xui_vec2_t __xuiWidgetMeasureDock(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_vec2_t tSize;
	xui_thickness_t tMargin;
	float fSideWidth;
	float fSideHeight;

	(void)tConstraint;
	tSize = __xuiVec2(0.0f, 0.0f);
	fSideWidth = 0.0f;
	fSideHeight = 0.0f;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED));
		tChildSize.fX += tMargin.fLeft + tMargin.fRight;
		tChildSize.fY += tMargin.fTop + tMargin.fBottom;
		switch ( pChild->tLayout.iDock ) {
		case XUI_DOCK_LEFT:
		case XUI_DOCK_RIGHT:
			fSideWidth += tChildSize.fX;
			fSideHeight = __xuiMaxFloat(fSideHeight, tChildSize.fY);
			break;
		case XUI_DOCK_TOP:
		case XUI_DOCK_BOTTOM:
			tSize.fX = __xuiMaxFloat(tSize.fX, fSideWidth + tChildSize.fX);
			tSize.fY += __xuiMaxFloat(fSideHeight, tChildSize.fY);
			fSideWidth = 0.0f;
			fSideHeight = 0.0f;
			break;
		case XUI_DOCK_FILL:
		default:
			tSize.fX = __xuiMaxFloat(tSize.fX, fSideWidth + tChildSize.fX);
			fSideHeight = __xuiMaxFloat(fSideHeight, tChildSize.fY);
			break;
		}
	}
	tSize.fX = __xuiMaxFloat(tSize.fX, fSideWidth);
	tSize.fY += fSideHeight;
	return tSize;
}

static xui_vec2_t __xuiWidgetMeasureGrid(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_thickness_t tMargin;
	float fItemWidth;
	float fItemHeight;
	int iCount;
	int iColumns;
	int iRows;

	(void)tConstraint;
	fItemWidth = pWidget->tLayout.fGridItemWidth;
	fItemHeight = pWidget->tLayout.fGridItemHeight;
	iCount = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED));
		fItemWidth = __xuiMaxFloat(fItemWidth, tChildSize.fX + tMargin.fLeft + tMargin.fRight);
		fItemHeight = __xuiMaxFloat(fItemHeight, tChildSize.fY + tMargin.fTop + tMargin.fBottom);
		iCount++;
	}
	if ( iCount <= 0 ) {
		return __xuiVec2(0.0f, 0.0f);
	}
	iColumns = pWidget->tLayout.iGridColumnCount;
	if ( iColumns <= 0 ) {
		iColumns = 1;
	}
	if ( iColumns > iCount ) {
		iColumns = iCount;
	}
	iRows = (iCount + iColumns - 1) / iColumns;
	return __xuiVec2(
		(fItemWidth * (float)iColumns) + (pWidget->tLayout.fGap * (float)(iColumns - 1)),
		(fItemHeight * (float)iRows) + (pWidget->tLayout.fGap * (float)(iRows - 1)));
}

static void __xuiWidgetFlowCloseLine(xui_vec2_t* pSize, float* pLineWidth, float* pLineHeight, int* pLineCount, float fGap)
{
	if ( *pLineCount <= 0 ) {
		return;
	}
	pSize->fX = __xuiMaxFloat(pSize->fX, *pLineWidth);
	if ( pSize->fY > 0.0f ) {
		pSize->fY += fGap;
	}
	pSize->fY += *pLineHeight;
	*pLineWidth = 0.0f;
	*pLineHeight = 0.0f;
	*pLineCount = 0;
}

static xui_vec2_t __xuiWidgetMeasureFlow(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_vec2_t tSize;
	xui_vec2_t tChildConstraint;
	xui_thickness_t tMargin;
	float fAvailableWidth;
	float fItemWidth;
	float fItemHeight;
	float fLineWidth;
	float fLineHeight;
	int iLineCount;

	tSize = __xuiVec2(0.0f, 0.0f);
	fAvailableWidth = __xuiConstraintIsFinite(tConstraint.fX) ? tConstraint.fX : XUI_LAYOUT_UNBOUNDED;
	fLineWidth = 0.0f;
	fLineHeight = 0.0f;
	iLineCount = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible || (pChild->tLayout.iFlowMode == XUI_FLOW_NONE) || (pChild->tLayout.iFlowMode == XUI_FLOW_ABSOLUTE) ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildConstraint.fX = __xuiConstraintIsFinite(fAvailableWidth) ? __xuiMaxFloat(0.0f, fAvailableWidth - tMargin.fLeft - tMargin.fRight) : XUI_LAYOUT_UNBOUNDED;
		tChildConstraint.fY = XUI_LAYOUT_UNBOUNDED;
		tChildSize = __xuiWidgetMeasureInternal(pChild, tChildConstraint);
		fItemWidth = tChildSize.fX + tMargin.fLeft + tMargin.fRight;
		fItemHeight = tChildSize.fY + tMargin.fTop + tMargin.fBottom;
		if ( pChild->tLayout.iFlowMode == XUI_FLOW_BLOCK ) {
			__xuiWidgetFlowCloseLine(&tSize, &fLineWidth, &fLineHeight, &iLineCount, pWidget->tLayout.fGap);
			tSize.fX = __xuiMaxFloat(tSize.fX, fItemWidth);
			if ( tSize.fY > 0.0f ) {
				tSize.fY += pWidget->tLayout.fGap;
			}
			tSize.fY += fItemHeight;
			continue;
		}
		if ( (iLineCount > 0) &&
		     __xuiConstraintIsFinite(fAvailableWidth) &&
		     ((fLineWidth + pWidget->tLayout.fGap + fItemWidth) > fAvailableWidth) ) {
			__xuiWidgetFlowCloseLine(&tSize, &fLineWidth, &fLineHeight, &iLineCount, pWidget->tLayout.fGap);
		}
		if ( iLineCount > 0 ) {
			fLineWidth += pWidget->tLayout.fGap;
		}
		fLineWidth += fItemWidth;
		fLineHeight = __xuiMaxFloat(fLineHeight, fItemHeight);
		iLineCount++;
	}
	__xuiWidgetFlowCloseLine(&tSize, &fLineWidth, &fLineHeight, &iLineCount, pWidget->tLayout.fGap);
	return tSize;
}

static int __xuiWidgetTableGetCounts(xui_widget pWidget, int* pRows, int* pColumns)
{
	xui_widget pChild;
	int iRows;
	int iColumns;
	int iChildEnd;

	iRows = pWidget->iTableRowCount;
	iColumns = pWidget->iTableColumnCount;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		iChildEnd = pChild->tLayout.iTableRow + pChild->tLayout.iTableRowSpan;
		if ( iChildEnd > iRows ) {
			iRows = iChildEnd;
		}
		iChildEnd = pChild->tLayout.iTableColumn + pChild->tLayout.iTableColumnSpan;
		if ( iChildEnd > iColumns ) {
			iColumns = iChildEnd;
		}
	}
	if ( iRows < 0 ) {
		iRows = 0;
	}
	if ( iColumns < 0 ) {
		iColumns = 0;
	}
	*pRows = iRows;
	*pColumns = iColumns;
	return (iRows > 0) && (iColumns > 0);
}

static xui_table_track_t __xuiWidgetTableRowTrack(xui_widget pWidget, int iRow)
{
	if ( (iRow >= 0) && (iRow < pWidget->iTableRowCount) && (pWidget->pTableRows != NULL) ) {
		return pWidget->pTableRows[iRow];
	}
	return __xuiTableTrackDefault();
}

static xui_table_track_t __xuiWidgetTableColumnTrack(xui_widget pWidget, int iColumn)
{
	if ( (iColumn >= 0) && (iColumn < pWidget->iTableColumnCount) && (pWidget->pTableColumns != NULL) ) {
		return pWidget->pTableColumns[iColumn];
	}
	return __xuiTableTrackDefault();
}

static void __xuiWidgetTableInitTrackSize(float* pSizes, int iCount, xui_widget pWidget, int bColumn)
{
	xui_table_track_t tTrack;
	int i;

	for ( i = 0; i < iCount; i++ ) {
		tTrack = bColumn ? __xuiWidgetTableColumnTrack(pWidget, i) : __xuiWidgetTableRowTrack(pWidget, i);
		if ( tTrack.iSizeMode == XUI_SIZE_FIXED ) {
			pSizes[i] = __xuiClampFloat(tTrack.fValue, tTrack.fMin, tTrack.fMax);
		} else {
			pSizes[i] = tTrack.fMin;
		}
	}
}

static float __xuiWidgetTableSpanSum(const float* pSizes, int iStart, int iSpan, float fGap)
{
	float fSum;
	int i;

	fSum = 0.0f;
	for ( i = 0; i < iSpan; i++ ) {
		fSum += pSizes[iStart + i];
	}
	if ( iSpan > 1 ) {
		fSum += fGap * (float)(iSpan - 1);
	}
	return fSum;
}

static void __xuiWidgetTableDistributeExtra(float* pSizes, int iStart, int iSpan, float fExtra, xui_widget pWidget, int bColumn)
{
	xui_table_track_t tTrack;
	float fWeight;
	float fTotalWeight;
	int i;

	if ( (iSpan <= 0) || (fExtra <= 0.0f) ) {
		return;
	}
	fTotalWeight = 0.0f;
	for ( i = 0; i < iSpan; i++ ) {
		tTrack = bColumn ? __xuiWidgetTableColumnTrack(pWidget, iStart + i) : __xuiWidgetTableRowTrack(pWidget, iStart + i);
		if ( tTrack.iSizeMode != XUI_SIZE_FIXED ) {
			fTotalWeight += (tTrack.fWeight > 0.0f) ? tTrack.fWeight : 1.0f;
		}
	}
	if ( fTotalWeight <= 0.0f ) {
		fTotalWeight = (float)iSpan;
	}
	for ( i = 0; i < iSpan; i++ ) {
		tTrack = bColumn ? __xuiWidgetTableColumnTrack(pWidget, iStart + i) : __xuiWidgetTableRowTrack(pWidget, iStart + i);
		if ( (tTrack.iSizeMode == XUI_SIZE_FIXED) && (fTotalWeight != (float)iSpan) ) {
			continue;
		}
		fWeight = ((tTrack.fWeight > 0.0f) ? tTrack.fWeight : 1.0f) / fTotalWeight;
		pSizes[iStart + i] = __xuiClampFloat(pSizes[iStart + i] + (fExtra * fWeight), tTrack.fMin, tTrack.fMax);
	}
}

static void __xuiWidgetTableDistributeAvailable(float* pSizes, int iCount, xui_widget pWidget, float fAvailable, int bColumn)
{
	xui_table_track_t tTrack;
	float fUsed;
	float fExtra;
	float fWeight;
	float fTotalWeight;
	int i;
	int iFillCount;

	if ( !__xuiConstraintIsFinite(fAvailable) || (iCount <= 0) ) {
		return;
	}
	fUsed = 0.0f;
	fTotalWeight = 0.0f;
	iFillCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		fUsed += pSizes[i];
		tTrack = bColumn ? __xuiWidgetTableColumnTrack(pWidget, i) : __xuiWidgetTableRowTrack(pWidget, i);
		if ( tTrack.iSizeMode == XUI_SIZE_FILL ) {
			fTotalWeight += (tTrack.fWeight > 0.0f) ? tTrack.fWeight : 1.0f;
			iFillCount++;
		}
	}
	if ( iCount > 1 ) {
		fUsed += pWidget->tLayout.fGap * (float)(iCount - 1);
	}
	fExtra = fAvailable - fUsed;
	if ( (fExtra <= 0.0f) || (iFillCount <= 0) ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		tTrack = bColumn ? __xuiWidgetTableColumnTrack(pWidget, i) : __xuiWidgetTableRowTrack(pWidget, i);
		if ( tTrack.iSizeMode != XUI_SIZE_FILL ) {
			continue;
		}
		fWeight = ((tTrack.fWeight > 0.0f) ? tTrack.fWeight : 1.0f) / fTotalWeight;
		pSizes[i] = __xuiClampFloat(pSizes[i] + (fExtra * fWeight), tTrack.fMin, tTrack.fMax);
	}
}

static int __xuiWidgetTableCompute(xui_widget pWidget, xui_vec2_t tConstraint, float** ppColumns, int* pColumnCount, float** ppRows, int* pRowCount)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_thickness_t tMargin;
	float* pColumns;
	float* pRows;
	float fNeed;
	float fHave;
	int iRows;
	int iColumns;
	int iRow;
	int iColumn;
	int iRowSpan;
	int iColumnSpan;

	*ppColumns = NULL;
	*ppRows = NULL;
	*pColumnCount = 0;
	*pRowCount = 0;
	if ( !__xuiWidgetTableGetCounts(pWidget, &iRows, &iColumns) ) {
		return XUI_OK;
	}
	pColumns = (float*)xrtMalloc(sizeof(float) * (size_t)iColumns);
	pRows = (float*)xrtMalloc(sizeof(float) * (size_t)iRows);
	if ( (pColumns == NULL) || (pRows == NULL) ) {
		if ( pColumns != NULL ) {
			xrtFree(pColumns);
		}
		if ( pRows != NULL ) {
			xrtFree(pRows);
		}
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiWidgetTableInitTrackSize(pColumns, iColumns, pWidget, 1);
	__xuiWidgetTableInitTrackSize(pRows, iRows, pWidget, 0);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		iRow = pChild->tLayout.iTableRow;
		iColumn = pChild->tLayout.iTableColumn;
		iRowSpan = pChild->tLayout.iTableRowSpan;
		iColumnSpan = pChild->tLayout.iTableColumnSpan;
		if ( (iRow < 0) || (iColumn < 0) || (iRowSpan <= 0) || (iColumnSpan <= 0) ||
		     ((iRow + iRowSpan) > iRows) || ((iColumn + iColumnSpan) > iColumns) ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED));
		fNeed = tChildSize.fX + tMargin.fLeft + tMargin.fRight;
		fHave = __xuiWidgetTableSpanSum(pColumns, iColumn, iColumnSpan, pWidget->tLayout.fGap);
		if ( fNeed > fHave ) {
			__xuiWidgetTableDistributeExtra(pColumns, iColumn, iColumnSpan, fNeed - fHave, pWidget, 1);
		}
		fNeed = tChildSize.fY + tMargin.fTop + tMargin.fBottom;
		fHave = __xuiWidgetTableSpanSum(pRows, iRow, iRowSpan, pWidget->tLayout.fGap);
		if ( fNeed > fHave ) {
			__xuiWidgetTableDistributeExtra(pRows, iRow, iRowSpan, fNeed - fHave, pWidget, 0);
		}
	}
	__xuiWidgetTableDistributeAvailable(pColumns, iColumns, pWidget, tConstraint.fX, 1);
	__xuiWidgetTableDistributeAvailable(pRows, iRows, pWidget, tConstraint.fY, 0);
	*ppColumns = pColumns;
	*ppRows = pRows;
	*pColumnCount = iColumns;
	*pRowCount = iRows;
	return XUI_OK;
}

static xui_vec2_t __xuiWidgetMeasureTable(xui_widget pWidget, xui_vec2_t tConstraint)
{
	float* pColumns;
	float* pRows;
	xui_vec2_t tSize;
	int iColumnCount;
	int iRowCount;
	int i;
	int iRet;

	tSize = __xuiVec2(0.0f, 0.0f);
	iRet = __xuiWidgetTableCompute(pWidget, tConstraint, &pColumns, &iColumnCount, &pRows, &iRowCount);
	if ( iRet != XUI_OK ) {
		return tSize;
	}
	for ( i = 0; i < iColumnCount; i++ ) {
		tSize.fX += pColumns[i];
	}
	for ( i = 0; i < iRowCount; i++ ) {
		tSize.fY += pRows[i];
	}
	if ( iColumnCount > 1 ) {
		tSize.fX += pWidget->tLayout.fGap * (float)(iColumnCount - 1);
	}
	if ( iRowCount > 1 ) {
		tSize.fY += pWidget->tLayout.fGap * (float)(iRowCount - 1);
	}
	if ( pColumns != NULL ) {
		xrtFree(pColumns);
	}
	if ( pRows != NULL ) {
		xrtFree(pRows);
	}
	return tSize;
}

static xui_vec2_t __xuiWidgetMeasureInternal(xui_widget pWidget, xui_vec2_t tConstraint)
{
	xui_vec2_t tContentSize;
	xui_vec2_t tOwnContentSize;
	xui_vec2_t tSize;
	xui_thickness_t tPadding;

	tConstraint.fX = __xuiFiniteConstraint(tConstraint.fX);
	tConstraint.fY = __xuiFiniteConstraint(tConstraint.fY);
	if ( pWidget->bMeasureValid &&
	     (pWidget->tMeasureConstraint.fX == tConstraint.fX) &&
	     (pWidget->tMeasureConstraint.fY == tConstraint.fY) ) {
		return pWidget->tMeasuredSize;
	}
	if ( pWidget->onLayoutMeasure != NULL ) {
		tContentSize = __xuiWidgetMeasureCustomLayout(pWidget, tConstraint);
	} else {
		switch ( pWidget->tLayout.iLayoutType ) {
		case XUI_LAYOUT_OVERLAY:
			tContentSize = __xuiWidgetMeasureOverlay(pWidget, tConstraint);
			break;
		case XUI_LAYOUT_ROW:
			tContentSize = __xuiWidgetMeasureLinear(pWidget, tConstraint, 1);
			break;
		case XUI_LAYOUT_COLUMN:
			tContentSize = __xuiWidgetMeasureLinear(pWidget, tConstraint, 0);
			break;
		case XUI_LAYOUT_FLOW:
			tContentSize = __xuiWidgetMeasureFlow(pWidget, tConstraint);
			break;
		case XUI_LAYOUT_TABLE:
			tContentSize = __xuiWidgetMeasureTable(pWidget, tConstraint);
			break;
		case XUI_LAYOUT_DOCK:
			tContentSize = __xuiWidgetMeasureDock(pWidget, tConstraint);
			break;
		case XUI_LAYOUT_GRID:
			tContentSize = __xuiWidgetMeasureGrid(pWidget, tConstraint);
			break;
		case XUI_LAYOUT_MANUAL:
		default:
			tContentSize = __xuiWidgetMeasureManual(pWidget);
			break;
		}
	}
	tOwnContentSize = __xuiWidgetMeasureOwnContent(pWidget, tConstraint);
	tContentSize = __xuiWidgetMaxSize(tContentSize, tOwnContentSize);
	tPadding = pWidget->tLayout.tPadding;
	tContentSize.fX += tPadding.fLeft + tPadding.fRight;
	tContentSize.fY += tPadding.fTop + tPadding.fBottom;
	tSize.fX = __xuiWidgetAxisPreferred(pWidget, 1, tContentSize, tConstraint);
	tSize.fY = __xuiWidgetAxisPreferred(pWidget, 0, tContentSize, tConstraint);
	tSize = __xuiWidgetClampSize(pWidget, tSize);
	pWidget->tMeasuredSize = tSize;
	pWidget->tMeasureConstraint = tConstraint;
	pWidget->bMeasureValid = 1;
	return tSize;
}

static float __xuiWidgetChildMainBase(xui_widget pChild, xui_vec2_t tMeasuredSize, int bHorizontal)
{
	float fPreferred;
	float fRectSize;
	int iMode;

	iMode = bHorizontal ? pChild->tLayout.iWidthMode : pChild->tLayout.iHeightMode;
	fPreferred = bHorizontal ? pChild->tLayout.fPreferredWidth : pChild->tLayout.fPreferredHeight;
	fRectSize = bHorizontal ? pChild->tRect.fW : pChild->tRect.fH;
	if ( iMode == XUI_SIZE_CONTENT ) {
		return bHorizontal ? tMeasuredSize.fX : tMeasuredSize.fY;
	}
	if ( iMode == XUI_SIZE_FILL ) {
		if ( fPreferred > 0.0f ) {
			return fPreferred;
		}
		return bHorizontal ? pChild->tLayout.fMinWidth : pChild->tLayout.fMinHeight;
	}
	if ( fPreferred > 0.0f ) {
		return fPreferred;
	}
	if ( fRectSize > 0.0f ) {
		return fRectSize;
	}
	return bHorizontal ? tMeasuredSize.fX : tMeasuredSize.fY;
}

static float __xuiWidgetChildGrowWeight(xui_widget pChild, int bHorizontal)
{
	int iMode;

	iMode = bHorizontal ? pChild->tLayout.iWidthMode : pChild->tLayout.iHeightMode;
	if ( pChild->tLayout.fGrow > 0.0f ) {
		return pChild->tLayout.fGrow;
	}
	if ( iMode == XUI_SIZE_FILL ) {
		return 1.0f;
	}
	return 0.0f;
}

static float __xuiWidgetChildCrossSize(xui_widget pChild, xui_vec2_t tMeasuredSize, float fAvailable, int bHorizontal)
{
	int iMode;
	int iAlign;
	float fSize;
	float fPreferred;

	iMode = bHorizontal ? pChild->tLayout.iHeightMode : pChild->tLayout.iWidthMode;
	iAlign = bHorizontal ? pChild->tLayout.iAlignY : pChild->tLayout.iAlignX;
	fPreferred = bHorizontal ? pChild->tLayout.fPreferredHeight : pChild->tLayout.fPreferredWidth;
	if ( (iMode == XUI_SIZE_FILL) || (iAlign == XUI_ALIGN_STRETCH) ) {
		fSize = fAvailable;
	} else if ( fPreferred > 0.0f ) {
		fSize = fPreferred;
	} else {
		fSize = bHorizontal ? tMeasuredSize.fY : tMeasuredSize.fX;
	}
	if ( bHorizontal ) {
		fSize = __xuiClampFloat(fSize, pChild->tLayout.fMinHeight, pChild->tLayout.fMaxHeight);
	} else {
		fSize = __xuiClampFloat(fSize, pChild->tLayout.fMinWidth, pChild->tLayout.fMaxWidth);
	}
	return __xuiMaxFloat(0.0f, fSize);
}

static float __xuiWidgetAlignOffset(int iAlign, float fAvailable, float fSize)
{
	if ( iAlign == XUI_ALIGN_CENTER ) {
		return (fAvailable - fSize) * 0.5f;
	}
	if ( iAlign == XUI_ALIGN_END ) {
		return fAvailable - fSize;
	}
	return 0.0f;
}

static void __xuiWidgetArrangeInternal(xui_widget pWidget, xui_rect_t tRect);

static void __xuiWidgetArrangeManual(xui_widget pWidget)
{
	xui_widget pChild;

	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiWidgetArrangeInternal(pChild, pChild->tRect);
	}
}

static void __xuiWidgetArrangeOverlay(xui_widget pWidget, xui_rect_t tContent)
{
	xui_widget pChild;
	xui_vec2_t tMeasuredSize;
	xui_rect_t tChildRect;
	xui_thickness_t tMargin;
	float fAvailableWidth;
	float fAvailableHeight;

	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		if ( pChild->tLayout.iFlowMode == XUI_FLOW_ABSOLUTE ) {
			__xuiWidgetArrangeInternal(pChild, pChild->tRect);
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		fAvailableWidth = __xuiMaxFloat(0.0f, tContent.fW - tMargin.fLeft - tMargin.fRight);
		fAvailableHeight = __xuiMaxFloat(0.0f, tContent.fH - tMargin.fTop - tMargin.fBottom);
		tMeasuredSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(fAvailableWidth, fAvailableHeight));
		tChildRect.fW = ((pChild->tLayout.iWidthMode == XUI_SIZE_FILL) || (pChild->tLayout.iAlignX == XUI_ALIGN_STRETCH)) ?
			fAvailableWidth : __xuiWidgetChildCrossSize(pChild, tMeasuredSize, fAvailableWidth, 0);
		tChildRect.fH = ((pChild->tLayout.iHeightMode == XUI_SIZE_FILL) || (pChild->tLayout.iAlignY == XUI_ALIGN_STRETCH)) ?
			fAvailableHeight : __xuiWidgetChildCrossSize(pChild, tMeasuredSize, fAvailableHeight, 1);
		tChildRect.fX = tContent.fX + tMargin.fLeft + __xuiWidgetAlignOffset(pChild->tLayout.iAlignX, fAvailableWidth, tChildRect.fW);
		tChildRect.fY = tContent.fY + tMargin.fTop + __xuiWidgetAlignOffset(pChild->tLayout.iAlignY, fAvailableHeight, tChildRect.fH);
		__xuiWidgetArrangeInternal(pChild, tChildRect);
	}
}

static void __xuiWidgetArrangeLinear(xui_widget pWidget, xui_rect_t tContent, int bHorizontal)
{
	xui_widget pChild;
	xui_vec2_t tMeasuredSize;
	xui_thickness_t tMargin;
	xui_rect_t tChildRect;
	float fMainAvailable;
	float fCrossAvailable;
	float fTotalBase;
	float fTotalGrow;
	float fTotalShrink;
	float fRemaining;
	float fGrow;
	float fBase;
	float fMainSize;
	float fCrossSize;
	float fCursor;
	float fChildCrossAvailable;
	float fMinMain;
	int iCount;

	fMainAvailable = bHorizontal ? tContent.fW : tContent.fH;
	fCrossAvailable = bHorizontal ? tContent.fH : tContent.fW;
	iCount = __xuiWidgetVisibleChildCount(pWidget);
	fTotalBase = (iCount > 1) ? pWidget->tLayout.fGap * (float)(iCount - 1) : 0.0f;
	fTotalGrow = 0.0f;
	fTotalShrink = 0.0f;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tMeasuredSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, fCrossAvailable));
		fBase = __xuiWidgetChildMainBase(pChild, tMeasuredSize, bHorizontal);
		fTotalBase += fBase;
		fTotalBase += bHorizontal ? (tMargin.fLeft + tMargin.fRight) : (tMargin.fTop + tMargin.fBottom);
		fTotalGrow += __xuiWidgetChildGrowWeight(pChild, bHorizontal);
		fTotalShrink += pChild->tLayout.fShrink;
	}
	fRemaining = fMainAvailable - fTotalBase;
	fCursor = bHorizontal ? tContent.fX : tContent.fY;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tMeasuredSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, fCrossAvailable));
		fGrow = __xuiWidgetChildGrowWeight(pChild, bHorizontal);
		fMainSize = __xuiWidgetChildMainBase(pChild, tMeasuredSize, bHorizontal);
		if ( (fRemaining > 0.0f) && (fTotalGrow > 0.0f) && (fGrow > 0.0f) ) {
			fMainSize += fRemaining * (fGrow / fTotalGrow);
		} else if ( (fRemaining < 0.0f) && (fTotalShrink > 0.0f) && (pChild->tLayout.fShrink > 0.0f) ) {
			fMainSize += fRemaining * (pChild->tLayout.fShrink / fTotalShrink);
		}
		fMinMain = bHorizontal ? pChild->tLayout.fMinWidth : pChild->tLayout.fMinHeight;
		fMainSize = __xuiMaxFloat(fMinMain, fMainSize);
		fChildCrossAvailable = fCrossAvailable - (bHorizontal ? (tMargin.fTop + tMargin.fBottom) : (tMargin.fLeft + tMargin.fRight));
		fChildCrossAvailable = __xuiMaxFloat(0.0f, fChildCrossAvailable);
		fCrossSize = __xuiWidgetChildCrossSize(pChild, tMeasuredSize, fChildCrossAvailable, bHorizontal);
		if ( bHorizontal ) {
			tChildRect.fX = fCursor + tMargin.fLeft;
			tChildRect.fY = tContent.fY + tMargin.fTop + __xuiWidgetAlignOffset(pChild->tLayout.iAlignY, fChildCrossAvailable, fCrossSize);
			tChildRect.fW = fMainSize;
			tChildRect.fH = fCrossSize;
			fCursor += fMainSize + tMargin.fLeft + tMargin.fRight + pWidget->tLayout.fGap;
		} else {
			tChildRect.fX = tContent.fX + tMargin.fLeft + __xuiWidgetAlignOffset(pChild->tLayout.iAlignX, fChildCrossAvailable, fCrossSize);
			tChildRect.fY = fCursor + tMargin.fTop;
			tChildRect.fW = fCrossSize;
			tChildRect.fH = fMainSize;
			fCursor += fMainSize + tMargin.fTop + tMargin.fBottom + pWidget->tLayout.fGap;
		}
		__xuiWidgetArrangeInternal(pChild, tChildRect);
	}
}

static float __xuiWidgetArrangeAxisSize(xui_widget pChild, xui_vec2_t tMeasuredSize, float fAvailable, int bHorizontal)
{
	int iMode;
	int iAlign;
	float fSize;
	float fPreferred;

	iMode = bHorizontal ? pChild->tLayout.iWidthMode : pChild->tLayout.iHeightMode;
	iAlign = bHorizontal ? pChild->tLayout.iAlignX : pChild->tLayout.iAlignY;
	fPreferred = bHorizontal ? pChild->tLayout.fPreferredWidth : pChild->tLayout.fPreferredHeight;
	if ( (iMode == XUI_SIZE_FILL) || (iAlign == XUI_ALIGN_STRETCH) ) {
		fSize = fAvailable;
	} else if ( fPreferred > 0.0f ) {
		fSize = fPreferred;
	} else {
		fSize = bHorizontal ? tMeasuredSize.fX : tMeasuredSize.fY;
	}
	if ( bHorizontal ) {
		fSize = __xuiClampFloat(fSize, pChild->tLayout.fMinWidth, pChild->tLayout.fMaxWidth);
	} else {
		fSize = __xuiClampFloat(fSize, pChild->tLayout.fMinHeight, pChild->tLayout.fMaxHeight);
	}
	return __xuiMaxFloat(0.0f, fSize);
}

static void __xuiWidgetArrangeFlow(xui_widget pWidget, xui_rect_t tContent)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_rect_t tChildRect;
	xui_thickness_t tMargin;
	float fLineX;
	float fLineY;
	float fLineHeight;
	float fItemWidth;
	float fItemHeight;
	float fAvailableWidth;
	int iLineCount;

	fLineX = 0.0f;
	fLineY = 0.0f;
	fLineHeight = 0.0f;
	iLineCount = 0;
	fAvailableWidth = tContent.fW;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible || (pChild->tLayout.iFlowMode == XUI_FLOW_NONE) || (pChild->tLayout.iFlowMode == XUI_FLOW_ABSOLUTE) ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(__xuiMaxFloat(0.0f, fAvailableWidth - tMargin.fLeft - tMargin.fRight), XUI_LAYOUT_UNBOUNDED));
		fItemWidth = tChildSize.fX + tMargin.fLeft + tMargin.fRight;
		fItemHeight = tChildSize.fY + tMargin.fTop + tMargin.fBottom;
		if ( pChild->tLayout.iFlowMode == XUI_FLOW_BLOCK ) {
			if ( iLineCount > 0 ) {
				fLineY += fLineHeight + pWidget->tLayout.fGap;
				fLineX = 0.0f;
				fLineHeight = 0.0f;
				iLineCount = 0;
			}
			tChildRect.fW = ((pChild->tLayout.iWidthMode == XUI_SIZE_FILL) || (pChild->tLayout.iAlignX == XUI_ALIGN_STRETCH)) ?
				__xuiMaxFloat(0.0f, fAvailableWidth - tMargin.fLeft - tMargin.fRight) :
				__xuiWidgetArrangeAxisSize(pChild, tChildSize, __xuiMaxFloat(0.0f, fAvailableWidth - tMargin.fLeft - tMargin.fRight), 1);
			tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, tChildSize.fY, 0);
			tChildRect.fX = tContent.fX + tMargin.fLeft + __xuiWidgetAlignOffset(pChild->tLayout.iAlignX, __xuiMaxFloat(0.0f, fAvailableWidth - tMargin.fLeft - tMargin.fRight), tChildRect.fW);
			tChildRect.fY = tContent.fY + fLineY + tMargin.fTop;
			__xuiWidgetArrangeInternal(pChild, tChildRect);
			fLineY += fItemHeight + pWidget->tLayout.fGap;
			continue;
		}
		if ( (iLineCount > 0) && ((fLineX + pWidget->tLayout.fGap + fItemWidth) > fAvailableWidth) ) {
			fLineY += fLineHeight + pWidget->tLayout.fGap;
			fLineX = 0.0f;
			fLineHeight = 0.0f;
			iLineCount = 0;
		}
		if ( iLineCount > 0 ) {
			fLineX += pWidget->tLayout.fGap;
		}
		tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, tChildSize.fX, 1);
		tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, tChildSize.fY, 0);
		tChildRect.fX = tContent.fX + fLineX + tMargin.fLeft;
		tChildRect.fY = tContent.fY + fLineY + tMargin.fTop;
		__xuiWidgetArrangeInternal(pChild, tChildRect);
		fLineX += fItemWidth;
		fLineHeight = __xuiMaxFloat(fLineHeight, fItemHeight);
		iLineCount++;
	}
}

static void __xuiWidgetArrangeDock(xui_widget pWidget, xui_rect_t tContent)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_rect_t tRemaining;
	xui_rect_t tChildRect;
	xui_thickness_t tMargin;
	float fAvailableW;
	float fAvailableH;
	int iDock;

	tRemaining = tContent;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		iDock = (pChild->tLayout.iDock != 0) ? pChild->tLayout.iDock : XUI_DOCK_FILL;
		tMargin = pChild->tLayout.tMargin;
		fAvailableW = __xuiMaxFloat(0.0f, tRemaining.fW - tMargin.fLeft - tMargin.fRight);
		fAvailableH = __xuiMaxFloat(0.0f, tRemaining.fH - tMargin.fTop - tMargin.fBottom);
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(fAvailableW, fAvailableH));
		tChildRect = tRemaining;
		switch ( iDock ) {
		case XUI_DOCK_LEFT:
			tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableW, 1) + tMargin.fLeft + tMargin.fRight;
			tRemaining.fX += tChildRect.fW + pWidget->tLayout.fGap;
			tRemaining.fW = __xuiMaxFloat(0.0f, tRemaining.fW - tChildRect.fW - pWidget->tLayout.fGap);
			break;
		case XUI_DOCK_RIGHT:
			tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableW, 1) + tMargin.fLeft + tMargin.fRight;
			tChildRect.fX = tRemaining.fX + tRemaining.fW - tChildRect.fW;
			tRemaining.fW = __xuiMaxFloat(0.0f, tRemaining.fW - tChildRect.fW - pWidget->tLayout.fGap);
			break;
		case XUI_DOCK_TOP:
			tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableH, 0) + tMargin.fTop + tMargin.fBottom;
			tRemaining.fY += tChildRect.fH + pWidget->tLayout.fGap;
			tRemaining.fH = __xuiMaxFloat(0.0f, tRemaining.fH - tChildRect.fH - pWidget->tLayout.fGap);
			break;
		case XUI_DOCK_BOTTOM:
			tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableH, 0) + tMargin.fTop + tMargin.fBottom;
			tChildRect.fY = tRemaining.fY + tRemaining.fH - tChildRect.fH;
			tRemaining.fH = __xuiMaxFloat(0.0f, tRemaining.fH - tChildRect.fH - pWidget->tLayout.fGap);
			break;
		case XUI_DOCK_FILL:
		default:
			tChildRect = tRemaining;
			break;
		}
		fAvailableW = __xuiMaxFloat(0.0f, tChildRect.fW - tMargin.fLeft - tMargin.fRight);
		fAvailableH = __xuiMaxFloat(0.0f, tChildRect.fH - tMargin.fTop - tMargin.fBottom);
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(fAvailableW, fAvailableH));
		tChildRect.fX += tMargin.fLeft;
		tChildRect.fY += tMargin.fTop;
		tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableW, 1);
		tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableH, 0);
		__xuiWidgetArrangeInternal(pChild, tChildRect);
	}
}

static void __xuiWidgetArrangeGrid(xui_widget pWidget, xui_rect_t tContent)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_rect_t tChildRect;
	xui_thickness_t tMargin;
	float fItemWidth;
	float fItemHeight;
	float fCellX;
	float fCellY;
	float fAvailableW;
	float fAvailableH;
	int iColumns;
	int iIndex;

	fItemWidth = pWidget->tLayout.fGridItemWidth;
	fItemHeight = pWidget->tLayout.fGridItemHeight;
	if ( (fItemWidth <= 0.0f) || (fItemHeight <= 0.0f) ) {
		for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
			if ( !pChild->bVisible ) {
				continue;
			}
			tMargin = pChild->tLayout.tMargin;
			tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED));
			fItemWidth = __xuiMaxFloat(fItemWidth, tChildSize.fX + tMargin.fLeft + tMargin.fRight);
			fItemHeight = __xuiMaxFloat(fItemHeight, tChildSize.fY + tMargin.fTop + tMargin.fBottom);
		}
	}
	iColumns = pWidget->tLayout.iGridColumnCount;
	if ( iColumns <= 0 ) {
		iColumns = 1;
	}
	if ( fItemWidth <= 0.0f ) {
		fItemWidth = tContent.fW;
	}
	if ( fItemHeight <= 0.0f ) {
		fItemHeight = tContent.fH;
	}
	iIndex = 0;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		fCellX = tContent.fX + (float)(iIndex % iColumns) * (fItemWidth + pWidget->tLayout.fGap);
		fCellY = tContent.fY + (float)(iIndex / iColumns) * (fItemHeight + pWidget->tLayout.fGap);
		fAvailableW = __xuiMaxFloat(0.0f, fItemWidth - tMargin.fLeft - tMargin.fRight);
		fAvailableH = __xuiMaxFloat(0.0f, fItemHeight - tMargin.fTop - tMargin.fBottom);
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(fAvailableW, fAvailableH));
		tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableW, 1);
		tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableH, 0);
		tChildRect.fX = fCellX + tMargin.fLeft + __xuiWidgetAlignOffset(pChild->tLayout.iAlignX, fAvailableW, tChildRect.fW);
		tChildRect.fY = fCellY + tMargin.fTop + __xuiWidgetAlignOffset(pChild->tLayout.iAlignY, fAvailableH, tChildRect.fH);
		__xuiWidgetArrangeInternal(pChild, tChildRect);
		iIndex++;
	}
}

static float __xuiWidgetTableOffset(const float* pSizes, int iCount, int iIndex, float fGap)
{
	float fOffset;
	int i;

	(void)iCount;
	fOffset = 0.0f;
	for ( i = 0; i < iIndex; i++ ) {
		fOffset += pSizes[i];
		fOffset += fGap;
	}
	return fOffset;
}

static void __xuiWidgetArrangeTable(xui_widget pWidget, xui_rect_t tContent)
{
	xui_widget pChild;
	xui_vec2_t tChildSize;
	xui_rect_t tChildRect;
	xui_thickness_t tMargin;
	float* pColumns;
	float* pRows;
	float fCellX;
	float fCellY;
	float fCellW;
	float fCellH;
	float fAvailableW;
	float fAvailableH;
	int iColumnCount;
	int iRowCount;
	int iRow;
	int iColumn;
	int iRowSpan;
	int iColumnSpan;
	int iRet;

	iRet = __xuiWidgetTableCompute(pWidget, __xuiVec2(tContent.fW, tContent.fH), &pColumns, &iColumnCount, &pRows, &iRowCount);
	if ( iRet != XUI_OK ) {
		return;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		if ( !pChild->bVisible ) {
			continue;
		}
		iRow = pChild->tLayout.iTableRow;
		iColumn = pChild->tLayout.iTableColumn;
		iRowSpan = pChild->tLayout.iTableRowSpan;
		iColumnSpan = pChild->tLayout.iTableColumnSpan;
		if ( (iRow < 0) || (iColumn < 0) || (iRowSpan <= 0) || (iColumnSpan <= 0) ||
		     ((iRow + iRowSpan) > iRowCount) || ((iColumn + iColumnSpan) > iColumnCount) ) {
			continue;
		}
		tMargin = pChild->tLayout.tMargin;
		fCellX = tContent.fX + __xuiWidgetTableOffset(pColumns, iColumnCount, iColumn, pWidget->tLayout.fGap);
		fCellY = tContent.fY + __xuiWidgetTableOffset(pRows, iRowCount, iRow, pWidget->tLayout.fGap);
		fCellW = __xuiWidgetTableSpanSum(pColumns, iColumn, iColumnSpan, pWidget->tLayout.fGap);
		fCellH = __xuiWidgetTableSpanSum(pRows, iRow, iRowSpan, pWidget->tLayout.fGap);
		fAvailableW = __xuiMaxFloat(0.0f, fCellW - tMargin.fLeft - tMargin.fRight);
		fAvailableH = __xuiMaxFloat(0.0f, fCellH - tMargin.fTop - tMargin.fBottom);
		tChildSize = __xuiWidgetMeasureInternal(pChild, __xuiVec2(fAvailableW, fAvailableH));
		tChildRect.fW = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableW, 1);
		tChildRect.fH = __xuiWidgetArrangeAxisSize(pChild, tChildSize, fAvailableH, 0);
		tChildRect.fX = fCellX + tMargin.fLeft + __xuiWidgetAlignOffset(pChild->tLayout.iAlignX, fAvailableW, tChildRect.fW);
		tChildRect.fY = fCellY + tMargin.fTop + __xuiWidgetAlignOffset(pChild->tLayout.iAlignY, fAvailableH, tChildRect.fH);
		__xuiWidgetArrangeInternal(pChild, tChildRect);
	}
	if ( pColumns != NULL ) {
		xrtFree(pColumns);
	}
	if ( pRows != NULL ) {
		xrtFree(pRows);
	}
}

static void __xuiWidgetArrangeInternal(xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tContent;

	tRect = xuiInternalSnapRect(tRect);
	pWidget->tRect = tRect;
	pWidget->bArrangeValid = 1;
	tContent = __xuiWidgetContentRect(pWidget);
	if ( pWidget->onLayoutArrange != NULL ) {
		(void)pWidget->onLayoutArrange(pWidget, tContent, pWidget->pLayoutUser);
	} else {
		switch ( pWidget->tLayout.iLayoutType ) {
		case XUI_LAYOUT_OVERLAY:
			__xuiWidgetArrangeOverlay(pWidget, tContent);
			break;
		case XUI_LAYOUT_ROW:
			__xuiWidgetArrangeLinear(pWidget, tContent, 1);
			break;
		case XUI_LAYOUT_COLUMN:
			__xuiWidgetArrangeLinear(pWidget, tContent, 0);
			break;
		case XUI_LAYOUT_FLOW:
			__xuiWidgetArrangeFlow(pWidget, tContent);
			break;
		case XUI_LAYOUT_TABLE:
			__xuiWidgetArrangeTable(pWidget, tContent);
			break;
		case XUI_LAYOUT_DOCK:
			__xuiWidgetArrangeDock(pWidget, tContent);
			break;
		case XUI_LAYOUT_GRID:
			__xuiWidgetArrangeGrid(pWidget, tContent);
			break;
		case XUI_LAYOUT_MANUAL:
		default:
			__xuiWidgetArrangeManual(pWidget);
			break;
		}
	}
	pWidget->iDirtyFlags &= ~XUI_WIDGET_DIRTY_LAYOUT;
	pWidget->iSubtreeDirtyFlags &= ~XUI_WIDGET_DIRTY_LAYOUT;
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
	if ( (pDesc->iFlags & ~(XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY)) != 0 ) {
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

static xui_widget_layout_measure_proc __xuiWidgetTypeLayoutMeasure(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onLayoutMeasure != NULL ) {
			return pType->onLayoutMeasure;
		}
	}
	return NULL;
}

static xui_widget_layout_arrange_proc __xuiWidgetTypeLayoutArrange(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onLayoutArrange != NULL ) {
			return pType->onLayoutArrange;
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

static xui_widget_update_proc __xuiWidgetTypeUpdate(xui_widget_type pType)
{
	for ( ; __xuiWidgetTypeValid(pType); pType = pType->pParent ) {
		if ( pType->onUpdate != NULL ) {
			return pType->onUpdate;
		}
	}
	return NULL;
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

static void __xuiWidgetTypeDestroyChain(xui_widget pWidget, xui_widget_type pType);

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
		if ( iRet != XUI_OK ) {
			__xuiWidgetTypeDestroyChain(pWidget, pType->pParent);
			return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiWidgetTypeDestroyChain(xui_widget pWidget, xui_widget_type pType)
{
	if ( !__xuiWidgetTypeValid(pType) ) {
		return;
	}
	if ( pType->onDestroy != NULL ) {
		pType->onDestroy(pWidget, pWidget->pTypeData, pType->pUser);
	}
	__xuiWidgetTypeDestroyChain(pWidget, pType->pParent);
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

static int __xuiWidgetLayoutChanged(xui_widget pWidget)
{
	pWidget->iLayoutVersion++;
	if ( pWidget->iLayoutVersion == 0 ) {
		pWidget->iLayoutVersion = 1;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiWidgetResizeTrackArray(xui_table_track_t** ppTracks, int* pCount, int iNewCount)
{
	xui_table_track_t* pTracks;
	int i;
	int iCopyCount;

	if ( iNewCount < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iNewCount == *pCount ) {
		return XUI_OK;
	}
	if ( iNewCount == 0 ) {
		if ( *ppTracks != NULL ) {
			xrtFree(*ppTracks);
		}
		*ppTracks = NULL;
		*pCount = 0;
		return XUI_OK;
	}
	pTracks = (xui_table_track_t*)xrtMalloc(sizeof(*pTracks) * (size_t)iNewCount);
	if ( pTracks == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iCopyCount = *pCount;
	if ( iCopyCount > iNewCount ) {
		iCopyCount = iNewCount;
	}
	for ( i = 0; i < iCopyCount; i++ ) {
		pTracks[i] = (*ppTracks)[i];
	}
	for ( i = iCopyCount; i < iNewCount; i++ ) {
		pTracks[i] = __xuiTableTrackDefault();
	}
	if ( *ppTracks != NULL ) {
		xrtFree(*ppTracks);
	}
	*ppTracks = pTracks;
	*pCount = iNewCount;
	return XUI_OK;
}

XUI_API int xuiLayout(xui_context pContext)
{
	xui_widget pRoot;
	xui_widget pOverlayRoot;
	xui_vec2_t tViewport;
	xui_rect_t tRootRect;
	int bNeedRoot;
	int bNeedOverlay;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pRoot = pContext->pRoot;
	pOverlayRoot = pContext->pOverlayRoot;
	tViewport = xuiGetViewportSize(pContext);
	if ( (tViewport.fX <= 0.0f) || (tViewport.fY <= 0.0f) ) {
		return XUI_OK;
	}
	tRootRect.fX = 0.0f;
	tRootRect.fY = 0.0f;
	tRootRect.fW = tViewport.fX;
	tRootRect.fH = tViewport.fY;
	if ( pRoot != NULL ) {
		bNeedRoot = ((pRoot->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) != 0) ||
		            !pRoot->bArrangeValid ||
		            (pRoot->tRect.fX != tRootRect.fX) ||
		            (pRoot->tRect.fY != tRootRect.fY) ||
		            (pRoot->tRect.fW != tRootRect.fW) ||
		            (pRoot->tRect.fH != tRootRect.fH);
		if ( bNeedRoot ) {
			__xuiWidgetArrangeInternal(pRoot, tRootRect);
		}
	}
	if ( pOverlayRoot != NULL ) {
		bNeedOverlay = ((pOverlayRoot->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) != 0) ||
		               !pOverlayRoot->bArrangeValid ||
		               (pOverlayRoot->tRect.fX != tRootRect.fX) ||
		               (pOverlayRoot->tRect.fY != tRootRect.fY) ||
		               (pOverlayRoot->tRect.fW != tRootRect.fW) ||
		               (pOverlayRoot->tRect.fH != tRootRect.fH);
		if ( bNeedOverlay ) {
			__xuiWidgetArrangeInternal(pOverlayRoot, tRootRect);
		}
	}
	return xuiInternalInputRefreshIme(pContext);
}

XUI_API int xuiSetRootWidget(xui_context pContext, xui_widget pWidget)
{
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
	if ( pContext->pRoot != NULL ) {
		xuiInternalContextDetachWidget(pContext, pContext->pRoot);
		__xuiWidgetMarkDirtyOnly(pContext->pRoot, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
	}
	pContext->pRoot = pWidget;
	if ( pWidget != NULL ) {
		__xuiWidgetMarkDirtyOnly(pWidget, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiInternalContextInvalidateAll(pContext);
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
	pType->onLayoutMeasure = pDesc->onLayoutMeasure;
	pType->onLayoutArrange = pDesc->onLayoutArrange;
	pType->onCacheRender = pDesc->onCacheRender;
	pType->onUpdate = pDesc->onUpdate;
	pType->tLayout = pDesc->tLayout;
	pType->tCachePolicy = pDesc->tCachePolicy;
	pType->pNext = pContext->pWidgetTypes;
	pContext->pWidgetTypes = pType;
	*ppType = pType;
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
	memset(&tDefaultValue, 0, sizeof(tDefaultValue));
	if ( (pInfo->tDefaultValue.iSize >= sizeof(pInfo->tDefaultValue)) &&
	     (pInfo->tDefaultValue.iType != XUI_STYLE_VALUE_NONE) ) {
		if ( !__xuiStyleValueTypeValid(pInfo->tDefaultValue.iType) ||
		     (pInfo->tDefaultValue.iType == XUI_STYLE_VALUE_TOKEN) ||
		     ((pInfo->iValueType != XUI_STYLE_VALUE_NONE) && (pInfo->tDefaultValue.iType != pInfo->iValueType)) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iRet = __xuiStyleValueCopy(&tDefaultValue, &pInfo->tDefaultValue);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	} else {
		tDefaultValue.iSize = sizeof(tDefaultValue);
		tDefaultValue.iType = XUI_STYLE_VALUE_NONE;
	}
	sName = __xuiStringDuplicate(pInfo->sName);
	if ( sName == NULL ) {
		__xuiStyleValueReset(&tDefaultValue);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pEntry = __xuiStyleFindPropertyByName(pContext, pInfo->sName);
	if ( pEntry == NULL ) {
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
		pEntry->pNext = pContext->pStyleProperties;
		pContext->pStyleProperties = pEntry;
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

static int __xuiWidgetCreateInternal(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData)
{
	xui_widget pWidget;
	xui_widget_type pOwner;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
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
	__xuiWidgetInitLayout(pWidget);
	pWidget->bVisible = 1;
	pWidget->bEnabled = 1;
	pWidget->bHitTestVisible = 1;
	pWidget->bTabStop = 1;
	pWidget->iImeMode = XUI_IME_DISABLED;
	pWidget->iDirtyFlags = XUI_WIDGET_DIRTY_ALL;
	pWidget->iSubtreeDirtyFlags = XUI_WIDGET_DIRTY_ALL;
	pWidget->iGeneration = 1;
	pWidget->iResolvedStyleHash = __xuiStyleHashProps(NULL, 0);
	pWidget->iResolvedStyleGeneration = pContext->iStyleGeneration;
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
	pWidget->onLayoutMeasure = __xuiWidgetTypeLayoutMeasure(pType);
	pWidget->onLayoutArrange = __xuiWidgetTypeLayoutArrange(pType);
	pWidget->pLayoutUser = pWidget->pTypeData;
	pWidget->onCacheRender = __xuiWidgetTypeCacheRender(pType);
	pWidget->pCacheRenderUser = pWidget->pTypeData;
	pWidget->onUpdate = __xuiWidgetTypeUpdate(pType);
	pWidget->pUpdateUser = pWidget->pTypeData;
	iRet = __xuiWidgetTypeInitChain(pWidget, pType, pCreateData);
	if ( iRet != XUI_OK ) {
		if ( pWidget->pTypeData != NULL ) {
			xrtFree(pWidget->pTypeData);
		}
		pWidget->iMagic = 0;
		xrtFree(pWidget);
		return iRet;
	}
	pType->iWidgetCount++;
	*ppWidget = pWidget;
	return XUI_OK;
}

XUI_API int xuiWidgetCreate(xui_context pContext, xui_widget* ppWidget)
{
	return __xuiWidgetCreateInternal(pContext, &g_xuiWidgetBaseType, ppWidget, NULL);
}

XUI_API int xuiWidgetCreateTyped(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData)
{
	return __xuiWidgetCreateInternal(pContext, pType, ppWidget, pCreateData);
}

XUI_API void xuiWidgetDestroy(xui_widget pWidget)
{
	xui_widget pChild;
	xui_widget pNext;
	xui_context pContext;

	if ( !__xuiWidgetValid(pWidget) ) {
		return;
	}
	pContext = pWidget->pContext;
	pChild = pWidget->pFirstChild;
	while ( pChild != NULL ) {
		pNext = pChild->pNextSibling;
		xuiWidgetDestroy(pChild);
		pChild = pNext;
	}
	if ( pWidget->pParent != NULL ) {
		(void)xuiWidgetRemoveFromParent(pWidget);
	} else if ( (pContext != NULL) && (pContext->pRoot == pWidget) ) {
		xuiInternalContextDetachWidget(pContext, pWidget);
		pContext->pRoot = NULL;
		(void)xuiInternalContextInvalidateAll(pContext);
	} else if ( (pContext != NULL) && (pContext->pOverlayRoot == pWidget) ) {
		xuiInternalContextDetachWidget(pContext, pWidget);
		pContext->pOverlayRoot = NULL;
		(void)xuiInternalContextInvalidateAll(pContext);
	}
	if ( pContext != NULL ) {
		xuiInternalContextDetachWidget(pContext, pWidget);
	}
	__xuiWidgetEndActiveUpdate(pWidget);
	__xuiWidgetTypeDestroyChain(pWidget, pWidget->pType);
	__xuiWidgetDestroyAllCaches(pWidget);
	__xuiWidgetDestroyTableTracks(pWidget);
	__xuiWidgetDestroyStyleData(pWidget);
	__xuiWidgetDestroyTooltipData(pWidget);
	if ( pWidget->pTypeData != NULL ) {
		xrtFree(pWidget->pTypeData);
	}
	if ( __xuiWidgetTypeValid(pWidget->pType) && (pWidget->pType->iWidgetCount > 0) ) {
		pWidget->pType->iWidgetCount--;
	}
	pWidget->iMagic = 0;
	xrtFree(pWidget);
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

XUI_API int xuiWidgetInsertBefore(xui_widget pParent, xui_widget pChild, xui_widget pBefore)
{
	if ( !__xuiWidgetCanAttachChild(pParent, pChild) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pBefore != NULL ) {
		if ( !__xuiWidgetValid(pBefore) || (pBefore->pParent != pParent) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
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
	__xuiWidgetUpdateEventMasksToRoot(pParent);
	(void)xuiWidgetInvalidate(pParent, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiWidgetRemoveFromParent(xui_widget pWidget)
{
	xui_widget pParent;
	xui_rect_t tWorldRect;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pParent = pWidget->pParent;
	if ( pParent == NULL ) {
		return XUI_OK;
	}
	xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
	tWorldRect = xuiWidgetGetWorldRect(pWidget);
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
	(void)__xuiWidgetRecomputeSubtreeEventMask(pWidget);
	__xuiWidgetUpdateEventMasksToRoot(pParent);
	(void)__xuiWidgetInvalidateWorldRect(pParent, tWorldRect, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
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

XUI_API int xuiWidgetSetRect(xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tOldRect;
	xui_rect_t tNewRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValidFloat(tRect) ) {
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
	pWidget->tRect = tRect;
	tNewRect = xuiWidgetGetWorldRect(pWidget);
	iRet = __xuiWidgetInvalidateWorldRect(pWidget, tOldRect, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, tNewRect, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
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
	if ( !__xuiWidgetValid(pWidget) || !__xuiLayoutStructValid(pLayout) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->tLayout = *pLayout;
	return __xuiWidgetLayoutChanged(pWidget);
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

XUI_API int xuiWidgetSetContentMeasureCallback(xui_widget pWidget, xui_widget_content_measure_proc onMeasure, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
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

XUI_API int xuiWidgetSetLayoutCallbacks(xui_widget pWidget, xui_widget_layout_measure_proc onMeasure, xui_widget_layout_arrange_proc onArrange, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->onLayoutMeasure == onMeasure) &&
	     (pWidget->onLayoutArrange == onArrange) &&
	     (pWidget->pLayoutUser == pUser) ) {
		return XUI_OK;
	}
	pWidget->onLayoutMeasure = onMeasure;
	pWidget->onLayoutArrange = onArrange;
	pWidget->pLayoutUser = pUser;
	return __xuiWidgetLayoutChanged(pWidget);
}

XUI_API int xuiWidgetGetLayoutCallbacks(xui_widget pWidget, xui_widget_layout_measure_proc* pMeasure, xui_widget_layout_arrange_proc* pArrange, void** ppUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pMeasure != NULL ) {
		*pMeasure = pWidget->onLayoutMeasure;
	}
	if ( pArrange != NULL ) {
		*pArrange = pWidget->onLayoutArrange;
	}
	if ( ppUser != NULL ) {
		*ppUser = pWidget->pLayoutUser;
	}
	return XUI_OK;
}

XUI_API int xuiWidgetMeasureContent(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pContentSize)
{
	if ( !__xuiWidgetValid(pWidget) || (pContentSize == NULL) ||
	     (tConstraint.fX != tConstraint.fX) || (tConstraint.fY != tConstraint.fY) ||
	     (tConstraint.fX < 0.0f) || (tConstraint.fY < 0.0f) ||
	     (tConstraint.fX > XUI_LAYOUT_UNBOUNDED) || (tConstraint.fY > XUI_LAYOUT_UNBOUNDED) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pContentSize = __xuiWidgetMeasureOwnContent(pWidget, tConstraint);
	return XUI_OK;
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
	pWidget->tLayout.iOverflow = iOverflow;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetOverflow(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->tLayout.iOverflow : XUI_OVERFLOW_VISIBLE;
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
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->tLayout.iLayer == iLayer) && (pWidget->tLayout.iZIndex == iZIndex) ) {
		return XUI_OK;
	}
	pWidget->tLayout.iLayer = iLayer;
	pWidget->tLayout.iZIndex = iZIndex;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_TREE | XUI_WIDGET_DIRTY_RENDER);
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
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (iRows < 0) || (iColumns < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiWidgetResizeTrackArray(&pWidget->pTableRows, &pWidget->iTableRowCount, iRows);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiWidgetResizeTrackArray(&pWidget->pTableColumns, &pWidget->iTableColumnCount, iColumns);
	if ( iRet != XUI_OK ) {
		return iRet;
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

XUI_API int xuiWidgetMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasuredSize)
{
	if ( !__xuiWidgetValid(pWidget) || (pMeasuredSize == NULL) ||
	     (tConstraint.fX != tConstraint.fX) || (tConstraint.fY != tConstraint.fY) ||
	     (tConstraint.fX < 0.0f) || (tConstraint.fY < 0.0f) ||
	     (tConstraint.fX > XUI_LAYOUT_UNBOUNDED) || (tConstraint.fY > XUI_LAYOUT_UNBOUNDED) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pMeasuredSize = __xuiWidgetMeasureInternal(pWidget, tConstraint);
	return XUI_OK;
}

XUI_API int xuiWidgetArrange(xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tOldRect;
	xui_rect_t tNewRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValidFloat(tRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tOldRect = xuiWidgetGetWorldRect(pWidget);
	__xuiWidgetArrangeInternal(pWidget, tRect);
	tNewRect = xuiWidgetGetWorldRect(pWidget);
	iRet = __xuiWidgetInvalidateWorldRect(pWidget, tOldRect, XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, tNewRect, XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetSetVisible(xui_widget pWidget, int bVisible)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bVisible = bVisible ? 1 : 0;
	if ( pWidget->bVisible == bVisible ) {
		return XUI_OK;
	}
	pWidget->bVisible = bVisible;
	if ( !bVisible ) {
		xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget), XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetVisible(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bVisible : 0;
}

XUI_API int xuiWidgetSetEnabled(xui_widget pWidget, int bEnabled)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bEnabled = bEnabled ? 1 : 0;
	if ( pWidget->bEnabled == bEnabled ) {
		return XUI_OK;
	}
	pWidget->bEnabled = bEnabled;
	if ( !bEnabled ) {
		xuiInternalContextDetachWidget(pWidget->pContext, pWidget);
	}
	iRet = xuiWidgetResolveStyle(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetInvalidateWorldRect(pWidget, xuiWidgetGetWorldRect(pWidget), XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetEnabled(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bEnabled : 0;
}

XUI_API uint32_t xuiWidgetGetInputState(xui_widget pWidget)
{
	uint32_t iState;

	if ( !__xuiWidgetValid(pWidget) ) {
		return 0;
	}
	iState = pWidget->iInputState;
	if ( !pWidget->bEnabled ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	return iState;
}

XUI_API int xuiWidgetSetHitTestVisible(xui_widget pWidget, int bVisible)
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
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetHitTestVisible(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bHitTestVisible : 0;
}

XUI_API int xuiWidgetSetFocusable(xui_widget pWidget, int bFocusable)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bFocusable = bFocusable ? 1 : 0;
	if ( pWidget->bFocusable == bFocusable ) {
		return XUI_OK;
	}
	pWidget->bFocusable = bFocusable;
	if ( !bFocusable && (pWidget->pContext != NULL) && (pWidget->pContext->pFocusWidget == pWidget) ) {
		pWidget->iInputState &= ~XUI_WIDGET_STATE_FOCUS;
		pWidget->pContext->pFocusWidget = NULL;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetFocusable(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bFocusable : 0;
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

XUI_API int xuiWidgetSetDefaultAction(xui_widget pWidget, xui_widget_action_proc onAction, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->onDefaultAction = onAction;
	pWidget->pDefaultActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiWidgetSetCancelAction(xui_widget pWidget, xui_widget_action_proc onAction, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->onCancelAction = onAction;
	pWidget->pCancelActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiWidgetSetDragEnabled(xui_widget pWidget, int bEnabled)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->bDragEnabled = bEnabled ? 1 : 0;
	if ( pWidget->bDragEnabled ) {
		pWidget->iEventInterestMask |= XUI_EVENT_MASK_DRAG;
	} else {
		pWidget->iEventInterestMask &= ~XUI_EVENT_MASK_DRAG;
	}
	__xuiWidgetUpdateEventMasksToRoot(pWidget);
	return XUI_OK;
}

XUI_API int xuiWidgetGetDragEnabled(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->bDragEnabled : 0;
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

static int __xuiInputSyncIme(xui_context pContext, int bForceEnabled, int bForceRect)
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
	bOldEnabled = pContext->bImeEnabled;
	bOldHasRect = pContext->bHasImeCandidateRect;
	tOldRect = pContext->tImeCandidateRect;
	bEnabled = (pFocus != NULL) &&
	           pFocus->bVisible &&
	           pFocus->bEnabled &&
	           ((pFocus->iImeMode == XUI_IME_ENABLED) ||
	            ((pFocus->iImeMode == XUI_IME_AUTO) && pFocus->bFocusable));
	memset(&tRect, 0, sizeof(tRect));
	pContext->bImeEnabled = bEnabled;
	pContext->bHasImeCandidateRect = 0;
	if ( bEnabled ) {
		if ( pFocus->onImeCandidateRect != NULL ) {
			tRect = pFocus->onImeCandidateRect(pFocus, pFocus->pImeCandidateRectUser);
		} else {
			tRect = xuiWidgetGetWorldRect(pFocus);
		}
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
	if ( !__xuiWidgetValid(pWidget) ||
	     ((iImeMode != XUI_IME_DISABLED) && (iImeMode != XUI_IME_ENABLED) && (iImeMode != XUI_IME_AUTO)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->iImeMode = iImeMode;
	if ( iImeMode == XUI_IME_DISABLED ) {
		pWidget->iEventInterestMask &= ~XUI_EVENT_MASK_IME;
	} else {
		pWidget->iEventInterestMask |= XUI_EVENT_MASK_IME;
	}
	__xuiWidgetUpdateEventMasksToRoot(pWidget);
	if ( (pWidget->pContext != NULL) && (pWidget->pContext->pFocusWidget == pWidget) ) {
		return xuiInternalInputSyncIme(pWidget->pContext);
	}
	return XUI_OK;
}

XUI_API int xuiWidgetGetImeMode(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iImeMode : XUI_IME_DISABLED;
}

XUI_API int xuiWidgetSetImeCandidateRect(xui_widget pWidget, xui_widget_ime_rect_proc onRect, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
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
	pWidget->onEvent = onEvent;
	pWidget->pEventUser = pUser;
	return XUI_OK;
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

XUI_API int xuiWidgetSetEventHandler(xui_widget pWidget, int iEventType, xui_widget_event_proc onEvent, void* pUser)
{
	uint64_t iMask;

	if ( !__xuiWidgetValid(pWidget) ||
	     (iEventType < 0) ||
	     (iEventType >= XUI_WIDGET_EVENT_SLOT_COUNT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pWidget->arrEventHandlers[iEventType] = onEvent;
	pWidget->arrEventUsers[iEventType] = pUser;
	iMask = __xuiWidgetEventTypeMask(iEventType);
	if ( iMask != 0ull ) {
		if ( onEvent != NULL ) {
			pWidget->iEventHandlerMask |= iMask;
		} else {
			pWidget->iEventHandlerMask &= ~iMask;
		}
		__xuiWidgetUpdateEventMasksToRoot(pWidget);
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
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( bEnabled ) {
		pWidget->iEventInterestMask |= iEventMask;
	} else {
		pWidget->iEventInterestMask &= ~iEventMask;
	}
	__xuiWidgetUpdateEventMasksToRoot(pWidget);
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
	if ( __xuiWidgetCreateInternal(pContext, &g_xuiWidgetBaseType, &pRoot, NULL) != XUI_OK ) {
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

static int __xuiWidgetSetOverlayTreeLayer(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pChild;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetSetLayer(pWidget, iLayer, iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWidgetSetOverlayTreeLayer(pChild, iLayer, iZIndex);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiOverlayAttach(xui_context pContext, xui_widget pOwner, xui_widget pOverlay, int iLayer, int iZIndex)
{
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
		iRet = xuiWidgetRemoveFromParent(pOverlay);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( iLayer <= XUI_LAYER_NORMAL ) {
		iLayer = XUI_LAYER_FLOATING;
	}
	pOverlay->pOverlayOwner = pOwner;
	iRet = __xuiWidgetSetOverlayTreeLayer(pOverlay, iLayer, iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetAddChild(pRoot, pOverlay);
}

XUI_API int xuiOverlayDetach(xui_widget pOverlay)
{
	if ( !__xuiWidgetValid(pOverlay) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pOverlay->pOverlayOwner = NULL;
	return xuiWidgetRemoveFromParent(pOverlay);
}

XUI_API int xuiOverlayBringToFront(xui_widget pOverlay)
{
	xui_widget pParent;
	int iLayer;
	int iZIndex;
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
	iLayer = pOverlay->tLayout.iLayer;
	iZIndex = pOverlay->tLayout.iZIndex;
	iRet = xuiWidgetRemoveFromParent(pOverlay);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWidgetAddChild(pParent, pOverlay);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetSetOverlayTreeLayer(pOverlay, iLayer, iZIndex + 1);
}

XUI_API xui_widget xuiOverlayGetOwner(xui_widget pOverlay)
{
	return __xuiWidgetValid(pOverlay) ? pOverlay->pOverlayOwner : NULL;
}

XUI_API xui_widget xuiOverlayTop(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) || (pContext->pOverlayRoot == NULL) ) {
		return NULL;
	}
	return pContext->pOverlayRoot->pLastChild;
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
	if ( pDesc->fDelay < 0.0f ) {
		pDesc->fDelay = 0.0f;
	}
	if ( !__xuiRectValidFloat(pDesc->tAnchorRect) ) {
		memset(&pDesc->tAnchorRect, 0, sizeof(pDesc->tAnchorRect));
		pDesc->bCustomAnchorRect = 0;
	}
	if ( !__xuiFloatValid(pDesc->fOffsetX) ) {
		pDesc->fOffsetX = 0.0f;
	}
	if ( !__xuiFloatValid(pDesc->fOffsetY) ) {
		pDesc->fOffsetY = 6.0f;
	}
	if ( (pDesc->fDelay != pDesc->fDelay) || (pDesc->fDelay < 0.0f) ) {
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
	pContext->sActiveTooltipText = sText;
	if ( sText != NULL ) {
		pContext->tActiveTooltip.sText = sText;
	}
	return XUI_OK;
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
		if ( __xuiTooltipDescEnabled(pDesc) ) {
			return 1;
		}
		__xuiTooltipDefaultDesc(pDesc);
		return 0;
	}
	*pDesc = pWidget->tTooltip;
	__xuiTooltipNormalizeDesc(pDesc);
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
	} else if ( pDesc->iType == XUI_TOOLTIP_TEXT ) {
		pProxy = xuiInternalContextGetProxy(pContext);
		pFont = xuiGetDefaultFont(pContext);
		if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) && (pDesc->sText != NULL) ) {
			if ( pProxy->textMeasure(pProxy, pFont, pDesc->sText, &tSize) != XUI_OK ) {
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
	if ( (tSize.fX != tSize.fX) || (tSize.fX < 12.0f) ) {
		tSize.fX = 12.0f;
	}
	if ( (tSize.fY != tSize.fY) || (tSize.fY < 18.0f) ) {
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

static int __xuiTooltipCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_context pContext;
	xui_proxy pProxy;
	xui_chrome_style_t tChrome;
	xui_rect_t tRect;
	xui_rect_t tText;
	xui_font pFont;
	int iRet;

	(void)pWidget;
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
		return pProxy->drawText(pProxy, pDraw, pFont, pContext->tActiveTooltip.sText, tText,
			tChrome.iTooltipTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static int __xuiTooltipEnsurePopup(xui_context pContext)
{
	xui_widget pPopup;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->pTooltipPopupWidget != NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetCreate(pContext, &pPopup);
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
	     !pContext->pTooltipOwner->bVisible ||
	     !pContext->pTooltipOwner->bEnabled ) {
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
	if ( fDelta != fDelta ) {
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
	     !pContext->pTooltipOwner->bVisible ||
	     !pContext->pTooltipOwner->bEnabled ) {
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

static int __xuiWidgetUpdateTree(xui_widget pWidget, float fDelta)
{
	xui_widget pChild;
	xui_widget pNext;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	if ( pWidget->onUpdate != NULL ) {
		iRet = pWidget->onUpdate(pWidget, fDelta, pWidget->pUpdateUser);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pNext ) {
		pNext = pChild->pNextSibling;
		iRet = __xuiWidgetUpdateTree(pChild, fDelta);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiUpdate(xui_context pContext, float fDelta)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta != fDelta ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta < 0.0f ) {
		fDelta = 0.0f;
	}
	iRet = xuiInternalContextPressUpdate(pContext, fDelta);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiInternalTooltipUpdate(pContext, fDelta);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiWidgetUpdateTree(pContext->pRoot, fDelta);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWidgetUpdateTree(pContext->pOverlayRoot, fDelta);
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

XUI_API int xuiWidgetInvalidateRect(xui_widget pWidget, xui_rect_t tRect, uint32_t iFlags)
{
	xui_rect_t tWorldRect;

	if ( !__xuiWidgetValid(pWidget) || !__xuiRectValidFloat(tRect) ) {
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
	pWidget->iSubtreeDirtyFlags &= ~iFlags;
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

XUI_API int xuiWidgetSetCacheRenderCallback(xui_widget pWidget, xui_widget_cache_render_proc onRender, void* pUser)
{
	if ( !__xuiWidgetValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
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

XUI_API int xuiWidgetSetCacheStateCount(xui_widget pWidget, int iCount)
{
	xui_widget_cache_slot_t* pSlots;
	int i;
	int iCopyCount;

	if ( !__xuiWidgetValid(pWidget) || (iCount < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget->pActiveUpdateDraw != NULL ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	if ( iCount == pWidget->iCacheCount ) {
		return XUI_OK;
	}
	if ( iCount == 0 ) {
		__xuiWidgetDestroyAllCaches(pWidget);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	pSlots = (xui_widget_cache_slot_t*)xrtMalloc(sizeof(*pSlots) * (size_t)iCount);
	if ( pSlots == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pSlots, 0, sizeof(*pSlots) * (size_t)iCount);
	iCopyCount = pWidget->iCacheCount;
	if ( iCopyCount > iCount ) {
		iCopyCount = iCount;
	}
	for ( i = 0; i < iCopyCount; i++ ) {
		pSlots[i] = pWidget->pCacheSlots[i];
		memset(&pWidget->pCacheSlots[i], 0, sizeof(pWidget->pCacheSlots[i]));
	}
	for ( i = iCopyCount; i < iCount; i++ ) {
		pSlots[i].iStateId = __xuiWidgetNextFreeStateId(pSlots, i);
	}
	for ( i = iCopyCount; i < pWidget->iCacheCount; i++ ) {
		__xuiWidgetDestroyCacheSlot(pWidget, &pWidget->pCacheSlots[i]);
	}
	if ( pWidget->pCacheSlots != NULL ) {
		xrtFree(pWidget->pCacheSlots);
	}
	pWidget->pCacheSlots = pSlots;
	pWidget->iCacheCount = iCount;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWidgetGetCacheStateCount(xui_widget pWidget)
{
	return __xuiWidgetValid(pWidget) ? pWidget->iCacheCount : 0;
}

XUI_API int xuiWidgetSetCacheStateId(xui_widget pWidget, int iIndex, uint32_t iStateId)
{
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

	if ( !__xuiWidgetValid(pWidget) ) {
		return NULL;
	}
	pSlot = __xuiWidgetFindCacheSlot(pWidget, iStateId);
	return (pSlot != NULL) ? pSlot->pSurface : NULL;
}

static int __xuiWidgetEnsureCacheStateSlot(xui_widget pWidget, uint32_t iStateId);

XUI_API int xuiWidgetUpdateBegin(xui_widget pWidget, uint32_t iStateId, uint32_t iFlags, uint32_t iClearColor, xui_draw_context* ppDraw)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tClearRect;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (ppDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppDraw = NULL;
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
	pWidget->iActiveUpdateStateId = iStateId;
	iRet = __xuiWidgetEnsureCacheSurface(pWidget, pSlot);
	if ( iRet != XUI_OK ) {
		pWidget->iActiveUpdateStateId = 0;
		return iRet;
	}
	iRet = pProxy->drawBegin(pProxy, ppDraw, pSlot->pSurface);
	if ( iRet != XUI_OK ) {
		pWidget->iActiveUpdateStateId = 0;
		return iRet;
	}
	if ( (iFlags & XUI_WIDGET_UPDATE_CLEAR) != 0 ) {
		tClearRect.fX = 0.0f;
		tClearRect.fY = 0.0f;
		tClearRect.fW = (float)pSlot->iWidth;
		tClearRect.fH = (float)pSlot->iHeight;
		iRet = pProxy->drawClearRect(pProxy, *ppDraw, tClearRect, iClearColor);
		if ( iRet != XUI_OK ) {
			(void)pProxy->drawEnd(pProxy, *ppDraw);
			*ppDraw = NULL;
			pWidget->iActiveUpdateStateId = 0;
			return iRet;
		}
	}
	pWidget->pActiveUpdateDraw = *ppDraw;
	pWidget->pActiveUpdateSlot = pSlot;
	return XUI_OK;
}

XUI_API int xuiWidgetUpdateEnd(xui_widget pWidget, uint32_t iStateId, xui_draw_context pDraw)
{
	xui_proxy pProxy;
	xui_widget_cache_slot_t* pSlot;
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget->pActiveUpdateDraw != pDraw) || (pWidget->iActiveUpdateStateId != iStateId) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pWidget->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pSlot = pWidget->pActiveUpdateSlot;
	iRet = pProxy->drawEnd(pProxy, pDraw);
	pWidget->pActiveUpdateDraw = NULL;
	pWidget->pActiveUpdateSlot = NULL;
	pWidget->iActiveUpdateStateId = 0;
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pSlot != NULL ) {
		pSlot->iFlags &= ~XUI_WIDGET_DIRTY_CACHE;
		if ( (pProxy->surfaceGetGeneration != NULL) && (pSlot->pSurface != NULL) ) {
			(void)pProxy->surfaceGetGeneration(pProxy, pSlot->pSurface, &pSlot->iSurfaceGeneration);
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_RENDER);
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

static int __xuiWidgetRenderTreeToDraw(xui_widget pWidget, xui_draw_context pDraw, float fOffsetX, float fOffsetY);

static int __xuiWidgetRenderCacheToDraw(xui_widget pWidget, xui_draw_context pDraw, float fOffsetX, float fOffsetY)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tSrc;
	xui_rect_t tDst;
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
	tSrc.fX = 0.0f;
	tSrc.fY = 0.0f;
	tSrc.fW = (float)pSlot->iWidth;
	tSrc.fH = (float)pSlot->iHeight;
	tDst = xuiWidgetGetWorldRect(pWidget);
	tDst.fX -= fOffsetX;
	tDst.fY -= fOffsetY;
	tDst = xuiInternalSnapRect(tDst);
	iRet = pProxy->drawSurface(pProxy, pDraw, pSlot->pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
	if ( iRet == XUI_OK ) {
		pWidget->pContext->tRenderStats.iDrawnCaches++;
	}
	return iRet;
}

static int __xuiWidgetRenderTreeToDraw(xui_widget pWidget, xui_draw_context pDraw, float fOffsetX, float fOffsetY)
{
	xui_widget pChild;
	int iPolicy;
	int iRet;

	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	iPolicy = __xuiWidgetEffectiveCachePolicy(pWidget);
	iRet = __xuiWidgetRenderCacheToDraw(pWidget, pDraw, fOffsetX, fOffsetY);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (iPolicy == XUI_CACHE_POLICY_SUBTREE) &&
	     __xuiWidgetHasCurrentCacheSurface(pWidget) ) {
		return XUI_OK;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWidgetRenderTreeToDraw(pChild, pDraw, fOffsetX, fOffsetY);
		if ( iRet != XUI_OK ) {
			return iRet;
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
	xui_widget pChild;
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
		iRet = pWidget->onCacheRender(pWidget, pDraw, iStateId, pWidget->pCacheRenderUser);
	}
	if ( (iRet == XUI_OK) && (iPolicy == XUI_CACHE_POLICY_SUBTREE) ) {
		tWorldRect = xuiWidgetGetWorldRect(pWidget);
		for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
			iRet = __xuiWidgetRenderTreeToDraw(pChild, pDraw, tWorldRect.fX, tWorldRect.fY);
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
	if ( iRet != XUI_OK ) {
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
	if ( !pWidget->bVisible || (iPolicy == XUI_CACHE_POLICY_NONE) ) {
		return XUI_OK;
	}
	if ( (iPolicy == XUI_CACHE_POLICY_SELF) && (pWidget->onCacheRender == NULL) ) {
		return XUI_OK;
	}
	if ( ((pWidget->tCachePolicy.iFlags & XUI_CACHE_UPDATE_ALL_STATES) != 0) && (pWidget->iCacheCount > 0) ) {
		bForceAll = ((pWidget->iDirtyFlags & XUI_RENDER_CACHE_DIRTY_FLAGS) != 0) ||
		            ((iPolicy == XUI_CACHE_POLICY_SUBTREE) &&
		             ((pWidget->iSubtreeDirtyFlags & XUI_RENDER_SUBTREE_DIRTY_FLAGS) != 0));
		iCount = pWidget->iCacheCount;
		for ( i = 0; i < iCount; i++ ) {
			iRet = __xuiWidgetPrepareCacheState(pWidget, pWidget->pCacheSlots[i].iStateId, bForceAll);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
		return XUI_OK;
	}
	return __xuiWidgetPrepareCacheState(pWidget, pWidget->iStateId, 0);
}

static int __xuiWidgetRenderPrepareTree(xui_widget pWidget)
{
	xui_widget pChild;
	int iRet;

	if ( !pWidget->bVisible ) {
		return XUI_OK;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWidgetRenderPrepareTree(pChild);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return __xuiWidgetPrepareCache(pWidget);
}

XUI_API int xuiRenderPrepare(xui_context pContext)
{
	uint32_t iGeneration;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !xuiInternalContextHasProxy(pContext) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iGeneration = pContext->tRenderStats.iGeneration + 1;
	if ( iGeneration == 0 ) {
		iGeneration = 1;
	}
	memset(&pContext->tRenderStats, 0, sizeof(pContext->tRenderStats));
	pContext->tRenderStats.iSize = sizeof(pContext->tRenderStats);
	pContext->tRenderStats.iGeneration = iGeneration;
	iRet = xuiLayout(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pContext->pRoot != NULL ) {
		iRet = __xuiWidgetRenderPrepareTree(pContext->pRoot);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		(void)__xuiWidgetRecomputeSubtreeDirtyFlags(pContext->pRoot);
	}
	if ( pContext->pOverlayRoot != NULL ) {
		iRet = __xuiWidgetRenderPrepareTree(pContext->pOverlayRoot);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		(void)__xuiWidgetRecomputeSubtreeDirtyFlags(pContext->pOverlayRoot);
	}
	return XUI_OK;
}

XUI_API int xuiGetRenderStats(xui_context pContext, xui_render_stats_t* pStats)
{
	if ( !xuiInternalContextIsValid(pContext) || (pStats == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pStats = pContext->tRenderStats;
	pStats->iSize = sizeof(*pStats);
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

XUI_API int xuiBuildRenderTree(xui_context pContext)
{
	xui_rect_t tRootClip;
	uint32_t iGeneration;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iRenderNodeCount = 0;
	if ( (pContext->pRoot == NULL) && (pContext->pOverlayRoot == NULL) ) {
		return XUI_OK;
	}
	iRet = xuiLayout(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tRootClip.fX = 0.0f;
	tRootClip.fY = 0.0f;
	tRootClip.fW = pContext->fViewportWidth;
	tRootClip.fH = pContext->fViewportHeight;
	if ( pContext->pRoot != NULL ) {
		iRet = __xuiWidgetBuildRenderTreeRecursive(pContext, pContext->pRoot, tRootClip, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( pContext->pOverlayRoot != NULL ) {
		iRet = __xuiWidgetBuildRenderTreeRecursive(pContext, pContext->pOverlayRoot, tRootClip, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	__xuiWidgetSortRenderNodes(pContext);
	iGeneration = pContext->iRenderTreeGeneration + 1;
	pContext->iRenderTreeGeneration = (iGeneration != 0) ? iGeneration : 1;
	return XUI_OK;
}

XUI_API int xuiGetRenderNodeCount(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return 0;
	}
	return pContext->iRenderNodeCount;
}

XUI_API int xuiGetRenderNode(xui_context pContext, int iIndex, xui_render_node_t* pNode)
{
	if ( !xuiInternalContextIsValid(pContext) ||
	     (pNode == NULL) ||
	     (iIndex < 0) ||
	     (iIndex >= pContext->iRenderNodeCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
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
	return XUI_OK;
}

XUI_API int xuiWidgetRenderTree(xui_widget pWidget, xui_surface pTarget)
{
	int iRet;

	if ( !__xuiWidgetValid(pWidget) || (pTarget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiWidgetRenderTreeToSurface(pWidget, pTarget);
	(void)__xuiWidgetRecomputeSubtreeDirtyFlags(pWidget);
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
	if ( !__xuiWidgetValid(pWidget) || (pInfo == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->pWidget = pWidget;
	pInfo->pParent = pWidget->pParent;
	pInfo->iDepth = __xuiDebugDepth(pWidget);
	pInfo->iChildCount = pWidget->iChildCount;
	pInfo->bVisible = pWidget->bVisible;
	pInfo->bEnabled = pWidget->bEnabled;
	pInfo->bFocusable = pWidget->bFocusable;
	pInfo->bTabStop = pWidget->bTabStop;
	pInfo->bFocusScope = pWidget->bFocusScope;
	pInfo->bHitTestVisible = pWidget->bHitTestVisible;
	pInfo->bDragEnabled = pWidget->bDragEnabled;
	pInfo->iImeMode = pWidget->iImeMode;
	pInfo->iLayer = pWidget->tLayout.iLayer;
	pInfo->iZIndex = pWidget->tLayout.iZIndex;
	pInfo->iDirtyFlags = pWidget->iDirtyFlags;
	pInfo->iEventMask = __xuiWidgetOwnEventMask(pWidget);
	pInfo->iSubtreeEventMask = pWidget->iSubtreeEventMask;
	pInfo->tRect = pWidget->tRect;
	pInfo->tWorldRect = xuiWidgetGetWorldRect(pWidget);
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
		"%s rect=%.1f,%.1f,%.1f,%.1f visible=%d enabled=%d dirty=0x%08x event=0x%llx\n",
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
		"viewport=%.1f,%.1f dpi=%.2f damage=%d render_nodes=%d\n",
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
		"event type=%d phase=%d target=%p current=%p related=%p key=%d button=%d xy=%.1f,%.1f focus=%p capture=%p hover=%p",
		pEvent->iType, pEvent->iPhase, (void*)pEvent->pTarget, (void*)pEvent->pCurrentTarget,
		(void*)pEvent->pRelated, pEvent->iKey, pEvent->iButton, pEvent->fX, pEvent->fY,
		(void*)pContext->pFocusWidget, (void*)pContext->pPointerCaptureWidget, (void*)pContext->pHoverWidget);
	sBuffer[iCapacity - 1] = '\0';
	return XUI_OK;
}
