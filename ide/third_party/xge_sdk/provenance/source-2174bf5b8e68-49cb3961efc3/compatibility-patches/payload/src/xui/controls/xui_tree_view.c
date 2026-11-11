#include "xui_config.h"
#if XUI_ENABLE_TREE_VIEW
#include "xui_internal.h"

#include <limits.h>
#include <string.h>

#define XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT 24.0f
#define XUI_TREE_VIEW_DEFAULT_WIDTH 240.0f
#define XUI_TREE_VIEW_DEFAULT_ROWS 7
#define XUI_TREE_VIEW_ACTIVE_ROW 0
#define XUI_TREE_VIEW_ACTIVE_EXPANDER 1
#define XUI_TREE_VIEW_ACTIVE_CHECK 2

#ifndef XUI_TREE_VIEW_TEST_COUNT
#define XUI_TREE_VIEW_TEST_COUNT(field, count) ((void)0)
#endif

typedef struct xui_tree_view_index_t {
	int iParent;
	int iFirstChild;
	int iLastChild;
	int iNextSibling;
	int iVisible;
} xui_tree_view_index_t;

typedef struct xui_tree_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_tree_view_node_t* arrNodes;
	xui_tree_view_index_t* arrIndex;
	xmap mapNodeIds;
	int* arrVisible;
	int iFirstRoot;
	int iLastRoot;
	int iNodeCount;
	int iNodeCapacity;
	int iVisibleCount;
	int iVisibleCapacity;
	xui_font pFont;
	xui_tree_view_select_proc onSelect;
	void* pSelectUser;
	xui_tree_view_context_proc onContext;
	void* pContextUser;
	xui_tree_view_count_proc onCount;
	xui_tree_view_node_proc onNode;
	xui_tree_view_node_owned_proc onNodeOwned;
	void* pCountUser;
	void* pNodeUser;
	int bRefreshingAdapter;
	xui_tree_view_item_proc onRenderItem;
	void* pRenderItemUser;
	xui_tree_view_icon_proc onRenderIcon;
	void* pRenderIconUser;
	int iSelectedId;
	int iHoverVisible;
	int iFocusVisible;
	int iActiveVisible;
	int iActivePart;
	uint64_t iActivePointerId;
	int iActivePointerType;
	int iChangeCount;
	int iSelectCount;
	float fBaseItemHeight;
	float fBaseIndent;
	float fBasePadding;
	float fBaseBorderWidth;
	float fItemHeight;
	float fIndent;
	float fPadding;
	float fBorderWidth;
	uint32_t iBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iFocusColor;
	uint32_t iRowColor;
	uint32_t iHoverColor;
	uint32_t iSelectedColor;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iExpanderColor;
	uint32_t iIconColor;
	uint32_t iCheckColor;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iScrollbarHoverColor;
	uint32_t iScrollbarActiveColor;
	uint32_t iScrollbarFocusColor;
	uint32_t iScrollbarDisabledColor;
	uint32_t iSelectedTextColor;
	uint32_t iSelectedExpanderColor;
	uint32_t iExpanderBackgroundColor;
	uint32_t iExpanderDisabledBackgroundColor;
	uint32_t iExpanderSelectedBackgroundColor;
	uint32_t iExpanderSelectedActiveBackgroundColor;
	uint32_t iExpanderActiveBackgroundColor;
	uint32_t iExpanderHoverBackgroundColor;
	uint32_t iCheckBackgroundColor;
	uint32_t iCheckMarkColor;
	uint32_t iFolderTabColor;
	uint32_t iFolderBodyColor;
	uint32_t iFileBackgroundColor;
	uint32_t iFileLineColor;
	uint32_t iPreparedStyleHash;
	int bPaintStylePrepared;
	char sAccessibleName[256];
} xui_tree_view_data_t;

/* Typed native snapshot, retained through the callback's physical return.
 * Scalar arguments and caller-owned hit snapshots precede reentrant Hold. */
static int __xuiOwnedInvoke_xuiTreeViewSetContextMenu(xui_widget widget,xui_tree_view_data_t* data,xui_tree_view_context_proc selected,void* user,int iNodeId,int iVisibleIndex,float fX,float fY)
{
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return selected(widget,iNodeId,iVisibleIndex,fX,fY,user);
    xui_callback_snapshot_t snapshot={0};
    int status=xuiInternalCallbackSnapshotBeginReported(widget,&data->onContext,&user,&snapshot);
    if(status!=XUI_OK)return status;
    int value=selected(widget,iNodeId,iVisibleIndex,fX,fY,user);
    status=value<0 ? value:XUI_OK;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    int ended=xuiInternalCallbackSnapshotEndOutcome(&snapshot,status,first);
    return ended!=XUI_OK ? ended:value;
}


static xui_tree_view_data_t* __xuiTreeViewGetData(xui_widget pWidget);
static void __xuiTreeViewCancelPointer(xui_widget pWidget,
	xui_tree_view_data_t* pData, int bReleaseCapture);

static int __xuiTreeViewWidgetAlive(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_context pContext;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return 0;
	pContext = xuiWidgetGetContext(pWidget);
	return xuiInternalContextIsValid(pContext) && !xuiInternalContextDestroyPending(pContext) &&
		__xuiTreeViewGetData(pWidget) == pData;
}

static int __xuiTreeViewFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTreeViewDescValid(const xui_tree_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( pDesc->iNodeCount < 0 ) {
		return 0;
	}
	if ( pDesc->iNodeCount > 0 && pDesc->arrNodes == NULL ) {
		return 0;
	}
	if ( ((pDesc->fItemHeight != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fItemHeight)) ||
	     ((pDesc->fIndent != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fIndent)) ||
	     ((pDesc->fPadding != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fPadding)) ||
	     ((pDesc->fBorderWidth != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fBorderWidth)) ) {
		return 0;
	}
	if ( (double)pDesc->iNodeCount *
	     (double)((pDesc->fItemHeight > 0.0f) ? pDesc->fItemHeight : XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT) >
	     (double)XUI_LAYOUT_UNBOUNDED ) return 0;
	return 1;
}

static int __xuiTreeViewScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiTreeViewAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiTreeViewMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiTreeViewMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static const char* __xuiTreeViewText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

XUI_API void xuiTreeViewNodeOutputInit(xui_tree_view_node_output_t* output)
{
    if(!output)return;
    memset(output,0,sizeof(*output));output->iId=-1;output->iParent=-1;
    output->bEnabled=1;output->bIconReserved=1;
}
XUI_API int xuiTreeViewNodeOutputSetText(xui_tree_view_node_output_t* output,xstrview text)
{
    if(!output || text.Size>INT_MAX || (text.Size && !text.Data) ||
       (text.Size && memchr(text.Data,0,text.Size)))return XUI_ERROR_INVALID_ARGUMENT;
    if(text.Data==output->_tText.Data && text.Size==output->_tText.Size)return XUI_OK;
    char* copy=NULL;
    if(text.Size){
        copy=xrtMalloc(text.Size+1);
        if(!copy)return XUI_ERROR_OUT_OF_MEMORY;
        memcpy(copy,text.Data,text.Size);copy[text.Size]=0;
    }
    const char* old=output->_tText.Data;
    output->_tText=(xstrview){copy,text.Size};
    xrtFree((void*)old);
    return XUI_OK;
}
XUI_API xstrview xuiTreeViewNodeOutputGetText(const xui_tree_view_node_output_t* output)
{return output ? output->_tText:(xstrview){0};}
XUI_API void xuiTreeViewNodeOutputDispose(xui_tree_view_node_output_t* output)
{
    if(!output)return;
    const char* text=output->_tText.Data;
    xuiTreeViewNodeOutputInit(output);
    xrtFree((void*)text);
}
static xui_tree_view_node_t __xuiTreeViewNodeOutputBorrow(const xui_tree_view_node_output_t* output)
{
    return (xui_tree_view_node_t){.iId=output->iId,.iParent=output->iParent,.iDepth=output->iDepth,
        .sText=output->_tText.Data ? output->_tText.Data:"",.bExpanded=output->bExpanded,
        .bEnabled=output->bEnabled,.bHasChildren=output->bHasChildren,.bIconReserved=output->bIconReserved,
        .bCheckReserved=output->bCheckReserved,.bChecked=output->bChecked,.pUser=output->pUser};
}

static char* __xuiTreeViewDupText(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void __xuiTreeViewFreeNodeText(xui_tree_view_node_t* pNode)
{
	if ( (pNode != NULL) && (pNode->sText != NULL) ) {
		xrtFree((void*)pNode->sText);
		pNode->sText = NULL;
	}
}

static void __xuiTreeViewInitNodeStorage(xui_tree_view_data_t* pData)
{
	pData->arrNodes = NULL;
	pData->arrIndex = NULL;
	memset(&pData->mapNodeIds, 0, sizeof(pData->mapNodeIds));
	pData->arrVisible = NULL;
	pData->iFirstRoot = -1;
	pData->iLastRoot = -1;
	pData->iNodeCount = 0;
	pData->iNodeCapacity = 0;
	pData->iVisibleCount = 0;
	pData->iVisibleCapacity = 0;
}

static void __xuiTreeViewMoveNodeStorage(xui_tree_view_data_t* target,xui_tree_view_data_t* source)
{
    target->arrNodes=source->arrNodes;target->arrIndex=source->arrIndex;
    target->mapNodeIds=source->mapNodeIds;target->arrVisible=source->arrVisible;
    target->iFirstRoot=source->iFirstRoot;target->iLastRoot=source->iLastRoot;
    target->iNodeCount=source->iNodeCount;target->iNodeCapacity=source->iNodeCapacity;
    target->iVisibleCount=source->iVisibleCount;target->iVisibleCapacity=source->iVisibleCapacity;
    __xuiTreeViewInitNodeStorage(source);
}

static void __xuiTreeViewResetNodes(xui_tree_view_data_t* pData)
{
	int i;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		__xuiTreeViewFreeNodeText(&pData->arrNodes[i]);
	}
	if ( pData->mapNodeIds.ValueSize != 0 ) xrtMapClear(&pData->mapNodeIds);
	pData->iNodeCount = 0;
	pData->iVisibleCount = 0;
	pData->iFirstRoot = -1;
	pData->iLastRoot = -1;
}

static void __xuiTreeViewClearNodeStorage(xui_tree_view_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	__xuiTreeViewResetNodes(pData);
	if ( pData->mapNodeIds.ValueSize != 0 ) xrtMapUnit(&pData->mapNodeIds);
	if ( pData->arrNodes != NULL ) {
		xrtFree(pData->arrNodes);
	}
	if ( pData->arrIndex != NULL ) {
		xrtFree(pData->arrIndex);
	}
	if ( pData->arrVisible != NULL ) {
		xrtFree(pData->arrVisible);
	}
	__xuiTreeViewInitNodeStorage(pData);
}

static void __xuiTreeViewDefaults(xui_tree_view_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	__xuiTreeViewInitNodeStorage(pData);
	pData->iSelectedId = -1;
	pData->iHoverVisible = -1;
	pData->iFocusVisible = -1;
	pData->iActiveVisible = -1;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	pData->iActivePointerId = XUI_POINTER_ID_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	pData->fBaseItemHeight = XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT;
	pData->fBaseIndent = 18.0f;
	pData->fBasePadding = 8.0f;
	pData->fBaseBorderWidth = 1.0f;
	pData->fItemHeight = XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT;
	pData->fIndent = 18.0f;
	pData->fPadding = 8.0f;
	pData->fBorderWidth = 1.0f;
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(132, 174, 214, 255);
	pData->iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iRowColor = XUI_COLOR_RGBA(255, 255, 255, 0);
	pData->iHoverColor = XUI_COLOR_RGBA(231, 243, 253, 255);
	pData->iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 210);
	pData->iExpanderColor = XUI_COLOR_RGBA(61, 91, 125, 240);
	pData->iIconColor = XUI_COLOR_RGBA(98, 139, 181, 240);
	pData->iCheckColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iTrackColor = XUI_COLOR_RGBA(224, 234, 244, 255);
	pData->iThumbColor = XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iScrollbarHoverColor = XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iScrollbarActiveColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iScrollbarFocusColor = XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iScrollbarDisabledColor = XUI_COLOR_RGBA(181, 190, 204, 135);
	pData->iSelectedTextColor = XUI_COLOR_WHITE;
	pData->iSelectedExpanderColor = XUI_COLOR_WHITE;
	pData->iExpanderBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iExpanderDisabledBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iExpanderSelectedBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iExpanderSelectedActiveBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iExpanderActiveBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iExpanderHoverBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iCheckBackgroundColor = XUI_COLOR_WHITE;
	pData->iCheckMarkColor = XUI_COLOR_WHITE;
	pData->iFolderTabColor = XUI_COLOR_RGBA(171, 201, 231, 230);
	pData->iFolderBodyColor = XUI_COLOR_RGBA(206, 226, 246, 235);
	pData->iFileBackgroundColor = XUI_COLOR_RGBA(255, 255, 255, 230);
	pData->iFileLineColor = XUI_COLOR_RGBA(164, 190, 217, 210);
}

static void __xuiTreeViewApplyDesc(xui_tree_view_data_t* pData, const xui_tree_view_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pFont = pDesc->pFont;
	if ( pDesc->fItemHeight > 0.0f ) pData->fBaseItemHeight = pData->fItemHeight = pDesc->fItemHeight;
	if ( pDesc->fIndent > 0.0f ) pData->fBaseIndent = pData->fIndent = pDesc->fIndent;
	if ( pDesc->fPadding > 0.0f ) pData->fBasePadding = pData->fPadding = pDesc->fPadding;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBaseBorderWidth = pData->fBorderWidth = pDesc->fBorderWidth;
	if ( pDesc->iSelectedId >= 0 ) pData->iSelectedId = pDesc->iSelectedId;
	if ( __xuiTreeViewAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiTreeViewAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiTreeViewAlpha(pDesc->iFocusColor) != 0 ) pData->iFocusColor = pDesc->iFocusColor;
	if ( pDesc->iRowColor != 0 ) pData->iRowColor = pDesc->iRowColor;
	if ( __xuiTreeViewAlpha(pDesc->iHoverColor) != 0 ) pData->iHoverColor = pDesc->iHoverColor;
	if ( __xuiTreeViewAlpha(pDesc->iSelectedColor) != 0 ) pData->iSelectedColor = pDesc->iSelectedColor;
	if ( __xuiTreeViewAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiTreeViewAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiTreeViewAlpha(pDesc->iExpanderColor) != 0 ) pData->iExpanderColor = pDesc->iExpanderColor;
	if ( __xuiTreeViewAlpha(pDesc->iIconColor) != 0 ) pData->iIconColor = pDesc->iIconColor;
	if ( __xuiTreeViewAlpha(pDesc->iCheckColor) != 0 ) pData->iCheckColor = pDesc->iCheckColor;
	if ( __xuiTreeViewAlpha(pDesc->iTrackColor) != 0 ) pData->iTrackColor = pDesc->iTrackColor;
	if ( __xuiTreeViewAlpha(pDesc->iThumbColor) != 0 ) pData->iThumbColor = pDesc->iThumbColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarHoverColor) != 0 ) pData->iScrollbarHoverColor = pDesc->iScrollbarHoverColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarActiveColor) != 0 ) pData->iScrollbarActiveColor = pDesc->iScrollbarActiveColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarFocusColor) != 0 ) pData->iScrollbarFocusColor = pDesc->iScrollbarFocusColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarDisabledColor) != 0 ) pData->iScrollbarDisabledColor = pDesc->iScrollbarDisabledColor;
}

static int __xuiTreeViewStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiTreeViewStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiTreeViewFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiTreeViewStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiTreeViewResolve(xui_widget pWidget, xui_tree_view_data_t* pData, xui_tree_view_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->fItemHeight = pData->fBaseItemHeight;
	pResolved->fIndent = pData->fBaseIndent;
	pResolved->fPadding = pData->fBasePadding;
	pResolved->fBorderWidth = pData->fBaseBorderWidth;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->pFont = __xuiTreeViewStyleFont(pWidget, pResolved->pFont);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.background.color", &pResolved->iBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.border.color", &pResolved->iBorderColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.focus.color", &pResolved->iFocusColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.color", &pResolved->iRowColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.hover_color", &pResolved->iHoverColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.selected_color", &pResolved->iSelectedColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.text.color", &pResolved->iTextColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.color", &pResolved->iExpanderColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.color", &pResolved->iIconColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.check.color", &pResolved->iCheckColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.text.selected_color", &pResolved->iSelectedTextColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.selected_color", &pResolved->iSelectedExpanderColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.background_color", &pResolved->iExpanderBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.disabled_background_color", &pResolved->iExpanderDisabledBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.selected_background_color", &pResolved->iExpanderSelectedBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.selected_active_background_color", &pResolved->iExpanderSelectedActiveBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.active_background_color", &pResolved->iExpanderActiveBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.hover_background_color", &pResolved->iExpanderHoverBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.check.background_color", &pResolved->iCheckBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.check.mark_color", &pResolved->iCheckMarkColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.folder_tab_color", &pResolved->iFolderTabColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.folder_body_color", &pResolved->iFolderBodyColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.file_background_color", &pResolved->iFileBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.file_line_color", &pResolved->iFileLineColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.track_color", &pResolved->iTrackColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.thumb_color", &pResolved->iThumbColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.hover_color", &pResolved->iScrollbarHoverColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.active_color", &pResolved->iScrollbarActiveColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.focus_color", &pResolved->iScrollbarFocusColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.scrollbar.disabled_color", &pResolved->iScrollbarDisabledColor);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.item.height", &pResolved->fItemHeight);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.indent", &pResolved->fIndent);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.padding", &pResolved->fPadding);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.border.width", &pResolved->fBorderWidth);
	if ( pResolved->fItemHeight < 1.0f ) {
		pResolved->fItemHeight = XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT;
	}
}

static int __xuiTreeViewEnsureNodeCapacity(xui_tree_view_data_t* pData, int iCount)
{
	xui_tree_view_node_t* pNewNodes;
	xui_tree_view_index_t* pNewIndex;
	int iNewCapacity;

	if ( pData == NULL || iCount < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCount <= pData->iNodeCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = (pData->iNodeCapacity > 0) ? pData->iNodeCapacity : 16;
	while ( iNewCapacity < iCount ) {
		iNewCapacity = (iNewCapacity > INT_MAX / 2) ? iCount : iNewCapacity * 2;
	}
	if ( (size_t)iNewCapacity > SIZE_MAX / sizeof(*pNewNodes) ||
	     (size_t)iNewCapacity > SIZE_MAX / sizeof(*pNewIndex) ) return XUI_ERROR_OUT_OF_MEMORY;
	pNewNodes = (xui_tree_view_node_t*)xrtRealloc(pData->arrNodes, (size_t)iNewCapacity * sizeof(*pNewNodes));
	if ( pNewNodes == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->arrNodes = pNewNodes;
	pNewIndex = (xui_tree_view_index_t*)xrtRealloc(pData->arrIndex, (size_t)iNewCapacity * sizeof(*pNewIndex));
	if ( pNewIndex == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pData->arrIndex = pNewIndex;
	pData->iNodeCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiTreeViewEnsureVisibleCapacity(xui_tree_view_data_t* pData, int iCount)
{
	int* pNewVisible;
	int iNewCapacity;

	if ( pData == NULL || iCount < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCount <= pData->iVisibleCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = (pData->iVisibleCapacity > 0) ? pData->iVisibleCapacity : 16;
	while ( iNewCapacity < iCount ) {
		iNewCapacity = (iNewCapacity > INT_MAX / 2) ? iCount : iNewCapacity * 2;
	}
	if ( (size_t)iNewCapacity > SIZE_MAX / sizeof(*pNewVisible) ) return XUI_ERROR_OUT_OF_MEMORY;
	pNewVisible = (int*)xrtRealloc(pData->arrVisible, (size_t)iNewCapacity * sizeof(*pNewVisible));
	if ( pNewVisible == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->arrVisible = pNewVisible;
	pData->iVisibleCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiTreeViewFindNodeData(const xui_tree_view_data_t* pData, int iNodeId)
{
	const int* pIndex;

	if ( (pData == NULL) || (iNodeId < 0) || (pData->iNodeCount == 0) ) {
		return -1;
	}
	XUI_TREE_VIEW_TEST_COUNT(iIdLookups, 1);
	pIndex = (const int*)xrtMapConstGet(&pData->mapNodeIds, xuiXrtBytes(&iNodeId, sizeof(iNodeId)));
	return (pIndex != NULL) ? *pIndex : -1;
}

static int __xuiTreeViewVisibleIndexData(const xui_tree_view_data_t* pData, int iNodeId)
{
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
}

static int __xuiTreeViewNodeEnabledData(const xui_tree_view_data_t* pData, int iNode)
{
	if ( (pData == NULL) || (iNode < 0) || (iNode >= pData->iNodeCount) ) {
		return 0;
	}
	return pData->arrNodes[iNode].bEnabled != 0;
}

static int __xuiTreeViewAppendNodeDataTakeText(xui_tree_view_data_t* pData, const xui_tree_view_node_t* pSource, xstrview* pOwnedText)
{
	xui_tree_view_node_t* pNode;
	xui_tree_view_node_t tNode;
	xui_tree_view_index_t* pIndex;
	int* pIdIndex;
	int iParent;
	int iPrevious;
	int iRet;
	int bTakeText;

	if ( (pData == NULL) || (pSource == NULL) || (pSource->iId < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiTreeViewFindNodeData(pData, pSource->iId) >= 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iParent = __xuiTreeViewFindNodeData(pData, pSource->iParent);
	if ( (pSource->iParent >= 0) && (iParent < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iNodeCount == INT_MAX ) return XUI_ERROR_OUT_OF_MEMORY;
	tNode = *pSource;
	iRet = __xuiTreeViewEnsureNodeCapacity(pData, pData->iNodeCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	/* Reserve all rows before publishing the node, so expanding cannot fail
	 * halfway through a splice and leave the inverse index inconsistent. */
	iRet = __xuiTreeViewEnsureVisibleCapacity(pData, pData->iNodeCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pData->mapNodeIds.ValueSize == 0) && !xrtMapInit(&pData->mapNodeIds, sizeof(int)) ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	bTakeText = pOwnedText != NULL && pOwnedText->Data != NULL;
	tNode.sText = bTakeText ? pOwnedText->Data : __xuiTreeViewDupText(tNode.sText);
	if ( tNode.sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	XUI_TREE_VIEW_TEST_COUNT(iIdInserts, 1);
	pIdIndex = (int*)xrtMapGetOrAdd(&pData->mapNodeIds, xuiXrtBytes(&tNode.iId, sizeof(tNode.iId)), NULL);
	if ( pIdIndex == NULL ) {
		if ( !bTakeText ) __xuiTreeViewFreeNodeText(&tNode);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*pIdIndex = pData->iNodeCount;
	pNode = &pData->arrNodes[pData->iNodeCount];
	*pNode = tNode;
	if ( bTakeText ) *pOwnedText = (xstrview){0};
	pNode->bEnabled = tNode.bEnabled ? 1 : 0;
	pNode->bExpanded = tNode.bExpanded ? 1 : 0;
	pNode->bHasChildren = 0;
	pNode->bIconReserved = tNode.bIconReserved ? 1 : 0;
	pNode->bCheckReserved = tNode.bCheckReserved ? 1 : 0;
	pNode->bChecked = tNode.bChecked ? 1 : 0;
	pIndex = &pData->arrIndex[pData->iNodeCount];
	pIndex->iParent = iParent;
	pIndex->iFirstChild = -1;
	pIndex->iLastChild = -1;
	pIndex->iNextSibling = -1;
	pIndex->iVisible = -1;
	iPrevious = (iParent >= 0) ? pData->arrIndex[iParent].iLastChild : pData->iLastRoot;
	if ( iPrevious >= 0 ) pData->arrIndex[iPrevious].iNextSibling = pData->iNodeCount;
	if ( iParent >= 0 ) {
		if ( iPrevious < 0 ) pData->arrIndex[iParent].iFirstChild = pData->iNodeCount;
		pData->arrIndex[iParent].iLastChild = pData->iNodeCount;
		pData->arrNodes[iParent].bHasChildren = 1;
	} else {
		if ( iPrevious < 0 ) pData->iFirstRoot = pData->iNodeCount;
		pData->iLastRoot = pData->iNodeCount;
	}
	XUI_TREE_VIEW_TEST_COUNT(iHierarchySteps, 1);
	pData->iNodeCount++;
	return XUI_OK;
}

static int __xuiTreeViewAppendNodeData(xui_tree_view_data_t* data,const xui_tree_view_node_t* source)
{return __xuiTreeViewAppendNodeDataTakeText(data,source,NULL);}

static void __xuiTreeViewRollbackLastNodeData(xui_tree_view_data_t* pData,
	int iParent, int iPrevious)
{
	int iNode;

	if ( pData == NULL || pData->iNodeCount <= 0 ) return;
	iNode = pData->iNodeCount - 1;
	if ( iPrevious >= 0 ) {
		pData->arrIndex[iPrevious].iNextSibling = -1;
	} else if ( iParent >= 0 ) {
		pData->arrIndex[iParent].iFirstChild = -1;
	} else {
		pData->iFirstRoot = -1;
	}
	if ( iParent >= 0 ) {
		pData->arrIndex[iParent].iLastChild = iPrevious;
		pData->arrNodes[iParent].bHasChildren = (iPrevious >= 0) ? 1 : 0;
	} else {
		pData->iLastRoot = iPrevious;
	}
	(void)xrtMapRemove(&pData->mapNodeIds,
		xuiXrtBytes(&pData->arrNodes[iNode].iId,
			sizeof(pData->arrNodes[iNode].iId)));
	__xuiTreeViewFreeNodeText(&pData->arrNodes[iNode]);
	pData->iNodeCount--;
}

static int __xuiTreeViewUpdateContentSizeFor(xui_widget pWidget,
	xui_tree_view_data_t* pData, int iVisibleCount, float fItemHeight)
{
	xui_rect_t tFrame;
	float fHeight;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ||
	     (iVisibleCount < 0) || (fItemHeight <= 0.0f) ||
	     ((double)iVisibleCount * (double)fItemHeight > (double)XUI_LAYOUT_UNBOUNDED) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tFrame = xuiWidgetGetContentRect(pWidget);
	if ( pData->fBorderWidth > 0.0f ) {
		tFrame = xuiInternalInsetRect(tFrame, pData->fBorderWidth);
	}
	if ( tFrame.fW < 0.0f ) tFrame.fW = 0.0f;
	if ( tFrame.fH < 0.0f ) tFrame.fH = 0.0f;
	fHeight = fItemHeight * (float)iVisibleCount;
	iRet = xuiScrollFrameSetWheelStep(pData->pFrame, fItemHeight * 3.0f);
	if ( iRet != XUI_OK ) return iRet;
	return xuiScrollFrameSetContentSize(pData->pFrame,
		__xuiTreeViewMaxFloat(0.0f, tFrame.fW),
		__xuiTreeViewMaxFloat(0.0f, fHeight));
}

static int __xuiTreeViewUpdateContentSize(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTreeViewUpdateContentSizeFor(pWidget, pData,
		pData->iVisibleCount, pData->fItemHeight);
}

static int __xuiTreeViewInvalidateRows(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport != NULL ) {
		iRet = xuiWidgetInvalidate(pData->pViewport,
			XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( iRet != XUI_OK ) return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTreeViewInvalidateLayoutRows(xui_widget pWidget,
	xui_tree_view_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport != NULL ) {
		iRet = xuiWidgetInvalidate(pData->pViewport,
			XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( iRet != XUI_OK ) return iRet;
	}
	return xuiWidgetInvalidate(pWidget,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTreeViewInvalidateViewportRow(xui_tree_view_data_t* pData, int iVisible)
{
	xui_rect_t tViewport;
	xui_rect_t tRow;
	float fOffsetY;
	float fTop;
	float fBottom;

	if ( (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetRect(pData->pViewport);
	if ( (tViewport.fW <= 0.0f) || (tViewport.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fTop = (float)iVisible * pData->fItemHeight - fOffsetY;
	fBottom = fTop + pData->fItemHeight;
	if ( (fBottom <= 0.0f) || (fTop >= tViewport.fH) ) {
		return XUI_OK;
	}
	tRow.fX = 0.0f;
	tRow.fY = __xuiTreeViewMaxFloat(0.0f, fTop);
	tRow.fW = tViewport.fW;
	tRow.fH = __xuiTreeViewMinFloat(tViewport.fH, fBottom) - tRow.fY;
	if ( tRow.fH <= 0.0f ) {
		return XUI_OK;
	}
	return xuiWidgetInvalidateRect(pData->pViewport, tRow, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTreeViewInvalidateHoverRows(xui_widget pWidget, xui_tree_view_data_t* pData, int iOldHover, int iNewHover)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport == NULL ) {
		return __xuiTreeViewInvalidateRows(pWidget, pData);
	}
	iRet = __xuiTreeViewInvalidateViewportRow(pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiTreeViewInvalidateViewportRow(pData, iNewHover);
	}
	return iRet;
}

/* Thread through parent/sibling indices instead of recursing on tree depth. */
static int __xuiTreeViewNextVisibleNode(const xui_tree_view_data_t* pData, int iNode, int iStopParent)
{
	XUI_TREE_VIEW_TEST_COUNT(iHierarchySteps, 1);
	if ( pData->arrNodes[iNode].bExpanded && (pData->arrIndex[iNode].iFirstChild >= 0) ) {
		return pData->arrIndex[iNode].iFirstChild;
	}
	while ( iNode != iStopParent ) {
		XUI_TREE_VIEW_TEST_COUNT(iHierarchySteps, 1);
		if ( pData->arrIndex[iNode].iNextSibling >= 0 ) return pData->arrIndex[iNode].iNextSibling;
		iNode = pData->arrIndex[iNode].iParent;
	}
	return -1;
}

static int __xuiTreeViewWriteVisibleNodes(xui_tree_view_data_t* pData, int iNode, int iStopParent, int iAt)
{
	int iParent;
	while ( iNode >= 0 ) {
		iParent = pData->arrIndex[iNode].iParent;
		pData->arrNodes[iNode].iDepth = (iParent >= 0) ? pData->arrNodes[iParent].iDepth + 1 : 0;
		pData->arrIndex[iNode].iVisible = iAt;
		pData->arrVisible[iAt++] = iNode;
		XUI_TREE_VIEW_TEST_COUNT(iVisibleWrites, 1);
		iNode = __xuiTreeViewNextVisibleNode(pData, iNode, iStopParent);
	}
	return iAt;
}

static void __xuiTreeViewMoveVisibleTail(xui_tree_view_data_t* pData, int iFrom, int iTo)
{
	int i;
	int iTail = pData->iVisibleCount - iFrom;
	if ( iFrom == iTo ) return;
	if ( pData->iActiveVisible >= iFrom ) pData->iActiveVisible += iTo - iFrom;
	else if ( pData->iActiveVisible >= iTo ) pData->iActiveVisible = -1;
	if ( iTail > 0 ) memmove(pData->arrVisible + iTo, pData->arrVisible + iFrom, (size_t)iTail * sizeof(int));
	XUI_TREE_VIEW_TEST_COUNT(iVisibleMoves, iTail);
	pData->iVisibleCount = iTo + iTail;
	for ( i = iTo; i < pData->iVisibleCount; i++ ) {
		pData->arrIndex[pData->arrVisible[i]].iVisible = i;
		XUI_TREE_VIEW_TEST_COUNT(iVisibleWrites, 1);
	}
}

static void __xuiTreeViewNormalizeVisibleState(xui_tree_view_data_t* pData,
	int iFallbackNodeId)
{
	int iVisible;
	int iNode;

	pData->iHoverVisible = -1;
	iVisible = __xuiTreeViewVisibleIndexData(pData, pData->iSelectedId);
	iNode = (iVisible >= 0) ? pData->arrVisible[iVisible] : -1;
	if ( (iVisible < 0) || !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
		iVisible = __xuiTreeViewVisibleIndexData(pData, iFallbackNodeId);
		iNode = (iVisible >= 0) ? pData->arrVisible[iVisible] : -1;
		if ( (iVisible >= 0) && __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			pData->iSelectedId = iFallbackNodeId;
		} else {
			pData->iSelectedId = -1;
			iVisible = -1;
		}
	}
	pData->iFocusVisible = iVisible;
}

static int __xuiTreeViewRebuildVisible(xui_widget pWidget,
	xui_tree_view_data_t* pData, int iFallbackNodeId)
{
	int i;
	int iRet = __xuiTreeViewEnsureVisibleCapacity(pData, pData->iNodeCount);
	(void)pWidget;
	if ( iRet != XUI_OK ) return iRet;
	XUI_TREE_VIEW_TEST_COUNT(iRebuilds, 1);
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pData->arrIndex[i].iVisible = -1;
		XUI_TREE_VIEW_TEST_COUNT(iVisibleWrites, 1);
	}
	pData->iVisibleCount = __xuiTreeViewWriteVisibleNodes(pData, pData->iFirstRoot, -1, 0);
	__xuiTreeViewNormalizeVisibleState(pData, iFallbackNodeId);
	return XUI_OK;
}

static int __xuiTreeViewSetExpandedData(xui_widget pWidget, xui_tree_view_data_t* pData, int iNode, int bExpanded)
{
	int iVisible = pData->arrIndex[iNode].iVisible;
	int iAt = iVisible + 1;
	int iChild;
	int iCount;
	int iEnd;
	int iNewVisibleCount;
	int iOldSelectedId;
	int iOldFocusVisible;
	int iRet;
	bExpanded = bExpanded ? 1 : 0;
	if ( pData->arrNodes[iNode].bExpanded == bExpanded ) return XUI_OK;
	iNewVisibleCount = pData->iVisibleCount;
	if ( iVisible >= 0 ) {
		if ( bExpanded ) {
			iCount = 0;
			iChild = pData->arrIndex[iNode].iFirstChild;
			while ( iChild >= 0 ) {
				iCount++;
				iChild = __xuiTreeViewNextVisibleNode(pData, iChild, iNode);
			}
			iNewVisibleCount += iCount;
		} else {
			iEnd = iAt;
			while ( iEnd < pData->iVisibleCount ) {
				iChild = pData->arrVisible[iEnd];
				XUI_TREE_VIEW_TEST_COUNT(iHierarchySteps, 1);
				if ( pData->arrNodes[iChild].iDepth <= pData->arrNodes[iNode].iDepth ) break;
				iEnd++;
			}
			iNewVisibleCount -= iEnd - iAt;
		}
	}
	if ( pWidget != NULL ) {
		iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
		if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
		if ( iNewVisibleCount != pData->iVisibleCount ) {
			iRet = __xuiTreeViewUpdateContentSizeFor(pWidget, pData,
				iNewVisibleCount, pData->fItemHeight);
			if ( iRet != XUI_OK ) return iRet;
		}
		__xuiTreeViewCancelPointer(pWidget, pData, 1);
	}
	iOldSelectedId = pData->iSelectedId;
	iOldFocusVisible = pData->iFocusVisible;
	if ( iVisible >= 0 ) {
		if ( bExpanded ) {
			iCount = iNewVisibleCount - pData->iVisibleCount;
			__xuiTreeViewMoveVisibleTail(pData, iAt, iAt + iCount);
			(void)__xuiTreeViewWriteVisibleNodes(pData,
				pData->arrIndex[iNode].iFirstChild, iNode, iAt);
		} else {
			iEnd = iAt + (pData->iVisibleCount - iNewVisibleCount);
			for ( iChild = iAt; iChild < iEnd; iChild++ ) {
				pData->arrIndex[pData->arrVisible[iChild]].iVisible = -1;
				XUI_TREE_VIEW_TEST_COUNT(iVisibleWrites, 1);
			}
			__xuiTreeViewMoveVisibleTail(pData, iEnd, iAt);
		}
	}
	pData->arrNodes[iNode].bExpanded = bExpanded;
	__xuiTreeViewNormalizeVisibleState(pData,
		bExpanded ? -1 : pData->arrNodes[iNode].iId);
	pData->iChangeCount++;
	if ( pWidget != NULL ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
		if ( pData->iSelectedId != iOldSelectedId ) {
			xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
		}
		if ( pData->iFocusVisible != iOldFocusVisible ) {
			xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
		}
	}
	return XUI_OK;
}

static int __xuiTreeViewAddNodeData(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_tree_view_node_t* pSource)
{
	int iParent = __xuiTreeViewFindNodeData(pData, pSource->iParent);
	int iPrevious = (iParent >= 0) ? pData->arrIndex[iParent].iLastChild : pData->iLastRoot;
	int iAt;
	int iNode;
	int bVisible;
	int iRet;

	iAt = pData->iVisibleCount;
	bVisible = (iParent < 0) ||
		((pData->arrIndex[iParent].iVisible >= 0) && pData->arrNodes[iParent].bExpanded);
	if ( bVisible && iParent >= 0 ) {
		if ( iPrevious < 0 ) {
			iAt = pData->arrIndex[iParent].iVisible + 1;
		} else {
			int iTail = iPrevious;
			while ( pData->arrNodes[iTail].bExpanded &&
			        (pData->arrIndex[iTail].iLastChild >= 0) ) {
				XUI_TREE_VIEW_TEST_COUNT(iHierarchySteps, 1);
				iTail = pData->arrIndex[iTail].iLastChild;
			}
			iAt = pData->arrIndex[iTail].iVisible + 1;
		}
	}
	if ( pWidget != NULL ) {
		iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
		if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	}
	iRet = __xuiTreeViewAppendNodeData(pData, pSource);
	if ( iRet != XUI_OK ) return iRet;
	iNode = pData->iNodeCount - 1;
	if ( pWidget != NULL && bVisible ) {
		iRet = __xuiTreeViewUpdateContentSizeFor(pWidget, pData,
			pData->iVisibleCount + 1, pData->fItemHeight);
		if ( iRet != XUI_OK ) {
			__xuiTreeViewRollbackLastNodeData(pData, iParent, iPrevious);
			return iRet;
		}
	}
	if ( pWidget != NULL ) __xuiTreeViewCancelPointer(pWidget, pData, 1);
	if ( bVisible ) {
		__xuiTreeViewMoveVisibleTail(pData, iAt, iAt + 1);
		pData->arrVisible[iAt] = iNode;
		pData->arrIndex[iNode].iVisible = iAt;
		pData->arrNodes[iNode].iDepth = (iParent >= 0) ? pData->arrNodes[iParent].iDepth + 1 : 0;
		XUI_TREE_VIEW_TEST_COUNT(iVisibleWrites, 1);
	}
	pData->iChangeCount++;
	__xuiTreeViewNormalizeVisibleState(pData, -1);
	if ( pWidget != NULL ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
		if ( bVisible ) {
			xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
		}
	}
	return XUI_OK;
}

static uint32_t __xuiTreeViewState(xui_widget pWidget)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
		iState |= XUI_WIDGET_STATE_FOCUS;
	}
	return iState;
}

static int __xuiTreeViewSyncState(xui_widget pWidget)
{
	return xuiWidgetSetStateId(pWidget, __xuiTreeViewState(pWidget));
}

static void __xuiTreeViewNotifySelect(xui_widget pWidget, xui_tree_view_data_t* pData, int iNodeId)
{
	xui_tree_view_select_proc onSelect;

	if ( (pWidget == NULL) || (pData == NULL) || (iNodeId < 0) ) {
		return;
	}
	pData->iSelectCount++;
	onSelect = pData->onSelect;
	if ( onSelect != NULL ) {
		XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onSelect, xui_tree_view_select_proc, pData->pSelectUser, pWidget, iNodeId);
	}
}

static int __xuiTreeViewEnsureVisibleIndex(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible)
{
	xui_rect_t tRect;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return XUI_OK;
	}
	tRect.fX = 0.0f;
	tRect.fY = (float)iVisible * pData->fItemHeight;
	tRect.fW = 1.0f;
	tRect.fH = pData->fItemHeight;
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

static int __xuiTreeViewSetSelectedVisible(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, int bNotify)
{
	int iNode;
	int iNodeId;
	int iOld;
	int iOldFocus;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		iNodeId = -1;
	} else {
		iNode = pData->arrVisible[iVisible];
		if ( !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
			iNodeId = -1;
			iVisible = -1;
		} else {
			iNodeId = pData->arrNodes[iNode].iId;
		}
	}
	iOld = pData->iSelectedId;
	iOldFocus = pData->iFocusVisible;
	if ( iOld == iNodeId && iOldFocus == iVisible ) {
		return (iVisible >= 0) ?
			__xuiTreeViewEnsureVisibleIndex(pWidget, pData, iVisible) : XUI_OK;
	}
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	if ( iVisible >= 0 ) {
		iRet = __xuiTreeViewEnsureVisibleIndex(pWidget, pData, iVisible);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iSelectedId = iNodeId;
	pData->iFocusVisible = iVisible;
	if ( iOld != iNodeId ) {
		pData->iChangeCount++;
	}
	if ( iOld != iNodeId ) xuiInternalAccessibilityQueue(pWidget,
		XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	if ( iOldFocus != iVisible ) xuiInternalAccessibilityQueue(pWidget,
		XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	if ( bNotify && iNodeId >= 0 && iOld != iNodeId ) {
		__xuiTreeViewNotifySelect(pWidget, pData, iNodeId);
	}
	return XUI_OK;
}

static int __xuiTreeViewFindEnabledVisible(const xui_tree_view_data_t* pData, int iStart, int iStep)
{
	int i;
	int iNode;

	if ( (pData == NULL) || (iStep == 0) ) {
		return -1;
	}
	for ( i = iStart; (i >= 0) && (i < pData->iVisibleCount); i += iStep ) {
		iNode = pData->arrVisible[i];
		if ( __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiTreeViewFindEnabledDescendantVisible(const xui_tree_view_data_t* pData, int iParentVisible)
{
	int i;
	int iNode;
	int iParentDepth;

	if ( pData == NULL || iParentVisible < 0 || iParentVisible >= pData->iVisibleCount ) return -1;
	iParentDepth = pData->arrNodes[pData->arrVisible[iParentVisible]].iDepth;
	for ( i = iParentVisible + 1; i < pData->iVisibleCount; i++ ) {
		iNode = pData->arrVisible[i];
		if ( pData->arrNodes[iNode].iDepth <= iParentDepth ) break;
		if ( __xuiTreeViewNodeEnabledData(pData, iNode) ) return i;
	}
	return -1;
}

static int __xuiTreeViewFindNearestEnabledVisible(const xui_tree_view_data_t* pData, int iIndex, int iStep)
{
	int iResult;

	if ( pData == NULL ) {
		return -1;
	}
	if ( iIndex < 0 ) iIndex = 0;
	if ( iIndex >= pData->iVisibleCount ) iIndex = pData->iVisibleCount - 1;
	iResult = __xuiTreeViewFindEnabledVisible(pData, iIndex, iStep);
	if ( iResult >= 0 ) {
		return iResult;
	}
	return __xuiTreeViewFindEnabledVisible(pData, iIndex, -iStep);
}

static xui_rect_t __xuiTreeViewFrameRect(xui_widget pWidget, const xui_tree_view_data_t* pData)
{
	xui_rect_t tRect;
	float fInset;

	tRect = xuiWidgetGetContentRect(pWidget);
	fInset = (pData != NULL && pData->fBorderWidth > 0.0f) ? pData->fBorderWidth : 0.0f;
	if ( fInset > 0.0f ) {
		tRect = xuiInternalInsetRect(tRect, fInset);
	}
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiTreeViewViewportRectLocal(xui_widget pWidget, const xui_tree_view_data_t* pData)
{
	xui_rect_t tFrame;
	xui_rect_t tViewport;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	(void)pWidget;
	tFrame = xuiWidgetGetRect(pData->pFrame);
	tViewport = xuiScrollFrameGetViewportRect(pData->pFrame);
	tViewport.fX += tFrame.fX;
	tViewport.fY += tFrame.fY;
	return tViewport;
}

static int __xuiTreeViewPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) && (fY < (tRect.fY + tRect.fH));
}

static int __xuiTreeViewHitWorld(xui_widget pWidget, xui_tree_view_data_t* pData, float fX, float fY)
{
	xui_rect_t tViewportWorld;
	float fOffsetY;
	float fLocalY;
	int iVisible;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return -1;
	}
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	if ( !__xuiTreeViewPointInRect(tViewportWorld, fX, fY) ) {
		return -1;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewportWorld.fY + fOffsetY;
	if ( fLocalY < 0.0f ) {
		return -1;
	}
	iVisible = (int)(fLocalY / pData->fItemHeight);
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return -1;
	}
	return iVisible;
}

static int __xuiTreeViewSetHover(xui_widget pWidget, xui_tree_view_data_t* pData, int iHover)
{
	int iOldHover;
	int iRet;

	if ( (pData == NULL) || (pData->iHoverVisible == iHover) ) {
		return XUI_OK;
	}
	iOldHover = pData->iHoverVisible;
	iRet = __xuiTreeViewInvalidateHoverRows(pWidget, pData, iOldHover, iHover);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	pData->iHoverVisible = iHover;
	return XUI_OK;
}

static xui_rect_t __xuiTreeViewExpanderRect(const xui_tree_view_data_t* pData, const xui_tree_view_node_t* pNode, xui_rect_t tRow)
{
	float fSafePadding;
	float fSize;
	float fX;
	float fY;

	fSize = 14.0f;
	fSafePadding = __xuiTreeViewMaxFloat(pData->fPadding, pData->fBorderWidth + 2.0f);
	fX = tRow.fX + fSafePadding + (float)pNode->iDepth * pData->fIndent;
	fY = tRow.fY + (tRow.fH - fSize) * 0.5f;
	return xuiInternalSnapRect((xui_rect_t){fX, fY, fSize, fSize});
}

static xui_rect_t __xuiTreeViewCheckRect(const xui_tree_view_data_t* pData, const xui_tree_view_node_t* pNode, xui_rect_t tRow)
{
	xui_rect_t tExpander;
	float fSize;
	float fX;
	float fY;

	tExpander = __xuiTreeViewExpanderRect(pData, pNode, tRow);
	fSize = 12.0f;
	fX = tExpander.fX + tExpander.fW + 5.0f;
	fY = tRow.fY + (tRow.fH - fSize) * 0.5f;
	return xuiInternalSnapRect((xui_rect_t){fX, fY, fSize, fSize});
}

static int __xuiTreeViewHitExpander(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, float fX, float fY)
{
	xui_rect_t tRow;
	xui_rect_t tExpander;
	xui_rect_t tWorld;
	xui_tree_view_node_t* pNode;
	float fX0;
	float fW;

	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return 0;
	}
	pNode = &pData->arrNodes[pData->arrVisible[iVisible]];
	if ( pNode->bHasChildren == 0 ) {
		return 0;
	}
	tRow = xuiTreeViewGetItemRect(pWidget, iVisible);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRow.fX += tWorld.fX;
	tRow.fY += tWorld.fY;
	tExpander = __xuiTreeViewExpanderRect(pData, pNode, tRow);
	fX0 = tExpander.fX - 2.0f;
	fW = __xuiTreeViewMaxFloat(22.0f, pData->fIndent + 4.0f);
	return (fX >= fX0) && (fX < fX0 + fW) && (fY >= tRow.fY) && (fY < tRow.fY + tRow.fH);
}

static int __xuiTreeViewHitCheck(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, float fX, float fY)
{
	xui_rect_t tRow;
	xui_rect_t tCheck;
	xui_rect_t tWorld;
	xui_tree_view_node_t* pNode;

	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return 0;
	}
	pNode = &pData->arrNodes[pData->arrVisible[iVisible]];
	if ( pNode->bCheckReserved == 0 ) {
		return 0;
	}
	tRow = xuiTreeViewGetItemRect(pWidget, iVisible);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRow.fX += tWorld.fX;
	tRow.fY += tWorld.fY;
	tCheck = __xuiTreeViewCheckRect(pData, pNode, tRow);
	tCheck.fX -= 4.0f;
	tCheck.fW += 8.0f;
	tCheck.fY = tRow.fY;
	tCheck.fH = tRow.fH;
	return __xuiTreeViewPointInRect(tCheck, fX, fY);
}

static int __xuiTreeViewApplyFrameStyle(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_tree_view_data_t tResolved;
	int iRet;

	(void)pWidget;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiScrollFrameSetScrollbarMode(pData->pFrame, xuiScrollFrameGetScrollbarMode(pData->pFrame));
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetScrollbarPolicy(pData->pFrame, XUI_SCROLLBAR_POLICY_HIDDEN, XUI_SCROLLBAR_POLICY_AUTO);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelAxis(pData->pFrame, XUI_WHEEL_AXIS_VERTICAL);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelStep(pData->pFrame, pData->fItemHeight * 3.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetContentDragEnabled(pData->pFrame, 0);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetMetrics(pData->pFrame, 8.0f, 18.0f, 0.0f);
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame, tResolved.iTrackColor, tResolved.iThumbColor, tResolved.iScrollbarHoverColor, tResolved.iScrollbarActiveColor, tResolved.iScrollbarFocusColor, tResolved.iScrollbarDisabledColor);
	return iRet;
}

static int __xuiTreeViewPointerMatches(const xui_tree_view_data_t* pData,
	const xui_event_t* pEvent)
{
	return pData != NULL && pEvent != NULL && pData->iActiveVisible >= 0 &&
		pData->iActivePointerId == pEvent->iPointerId &&
		pData->iActivePointerType == pEvent->iPointerType;
}

static void __xuiTreeViewCancelPointer(xui_widget pWidget,
	xui_tree_view_data_t* pData, int bReleaseCapture)
{
	xui_context pContext;
	uint64_t iPointerId;
	int iPointerType;

	if ( pWidget == NULL || pData == NULL || pData->iActiveVisible < 0 ) return;
	pContext = xuiWidgetGetContext(pWidget);
	iPointerId = pData->iActivePointerId;
	iPointerType = pData->iActivePointerType;
	pData->iActiveVisible = -1;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	pData->iActivePointerId = XUI_POINTER_ID_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	if ( bReleaseCapture && pContext != NULL &&
	     xuiGetPointerCaptureEx(pContext, iPointerId, iPointerType) == pWidget ) {
		(void)xuiReleasePointerCaptureEx(pContext, iPointerId, iPointerType, pWidget);
	}
}

static int __xuiTreeViewPointerMove(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iHover;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	iHover = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	return __xuiTreeViewSetHover(pWidget, pData, iHover);
}

static int __xuiTreeViewPointerDown(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iVisible;
	int iNode;
	int iNodeId;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
		return XUI_OK;
	}
	if ( pData->iActiveVisible >= 0 ) {
		return __xuiTreeViewPointerMatches(pData, pEvent) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	}
	iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iVisible < 0 ) {
		return XUI_OK;
	}
	iNode = pData->arrVisible[iVisible];
	iNodeId = pData->arrNodes[iNode].iId;
	if ( !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
		return XUI_EVENT_DISPATCH_STOP;
	}
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	iVisible = (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
	if ( iVisible < 0 || !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
		return XUI_EVENT_DISPATCH_STOP;
	}
	iRet = xuiSetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId,
		pEvent->iPointerType, pWidget);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	pData->iActiveVisible = iVisible;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	pData->iActivePointerId = pEvent->iPointerId;
	pData->iActivePointerType = pEvent->iPointerType;
	if ( __xuiTreeViewHitExpander(pWidget, pData, iVisible, pEvent->fX, pEvent->fY) ) {
		pData->iActivePart = XUI_TREE_VIEW_ACTIVE_EXPANDER;
	} else if ( __xuiTreeViewHitCheck(pWidget, pData, iVisible, pEvent->fX, pEvent->fY) ) {
		pData->iActivePart = XUI_TREE_VIEW_ACTIVE_CHECK;
	}
	iRet = __xuiTreeViewSetHover(pWidget, pData, iVisible);
	if ( iRet != XUI_OK ) {
		if ( __xuiTreeViewWidgetAlive(pWidget, pData) ) __xuiTreeViewCancelPointer(pWidget, pData, 1);
		return iRet;
	}
	pData->iFocusVisible = iVisible;
	if ( pData->iActivePart == XUI_TREE_VIEW_ACTIVE_ROW ) {
		iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 1);
		if ( iRet != XUI_OK ) {
			if ( __xuiTreeViewWidgetAlive(pWidget, pData) ) __xuiTreeViewCancelPointer(pWidget, pData, 1);
			return iRet;
		}
		if ( !__xuiTreeViewWidgetAlive(pWidget, pData) ) return XUI_EVENT_DISPATCH_STOP;
	}
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
}

static int __xuiTreeViewPointerUp(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iVisible;
	int iNode;
	int iActiveVisible;
	int iActivePart;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !__xuiTreeViewPointerMatches(pData, pEvent) ||
	     xuiGetPointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType) != pWidget ) {
		return XUI_OK;
	}
	iRet = XUI_OK;
	iActiveVisible = pData->iActiveVisible;
	iActivePart = pData->iActivePart;
	iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	__xuiTreeViewCancelPointer(pWidget, pData, 1);
	if ( iVisible == iActiveVisible ) {
		iNode = pData->arrVisible[iVisible];
		if ( __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			if ( (iActivePart == XUI_TREE_VIEW_ACTIVE_EXPANDER) && (pData->arrNodes[iNode].bHasChildren != 0) ) {
				iRet = __xuiTreeViewSetExpandedData(pWidget, pData, iNode, !pData->arrNodes[iNode].bExpanded);
			} else if ( (iActivePart == XUI_TREE_VIEW_ACTIVE_CHECK) && (pData->arrNodes[iNode].bCheckReserved != 0) ) {
				iRet = xuiTreeViewSetNodeChecked(pWidget,
					pData->arrNodes[iNode].iId,
					!pData->arrNodes[iNode].bChecked);
			}
		}
	}
	if ( iRet == XUI_OK && __xuiTreeViewWidgetAlive(pWidget, pData) ) {
		iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	}
	return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
}

static int __xuiTreeViewDoubleClick(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iVisible;
	int iNode;
	int iNodeId;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) || !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_OK;
	iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iVisible < 0 ) return XUI_OK;
	iNode = pData->arrVisible[iVisible];
	if ( !__xuiTreeViewNodeEnabledData(pData, iNode) ) return XUI_OK;
	iNodeId = pData->arrNodes[iNode].iId;
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	iVisible = (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
	if ( iVisible < 0 || !__xuiTreeViewNodeEnabledData(pData, iNode) ) return XUI_EVENT_DISPATCH_STOP;
	iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 1);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_EVENT_DISPATCH_STOP;
	if ( pData->arrNodes[iNode].bHasChildren != 0 ) {
		return __xuiTreeViewSetExpandedData(pWidget, pData, iNode, !pData->arrNodes[iNode].bExpanded);
	}
	return XUI_OK;
}

static int __xuiTreeViewContextMenu(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_tree_view_context_proc onContext;
	void* pContextUser;
	int iVisible;
	int iNode;
	int iNodeId;
	int iRet;
	float fX;
	float fY;
	xui_rect_t tAnchor;
	xui_rect_t tWorld;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) || !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_OK;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
		iVisible = pData->iFocusVisible;
		if ( iVisible < 0 ) iVisible = __xuiTreeViewVisibleIndexData(pData, pData->iSelectedId);
		tAnchor = (iVisible >= 0) ? xuiTreeViewGetItemRect(pWidget, iVisible) : tWorld;
		if ( iVisible >= 0 ) {
			tAnchor.fX += tWorld.fX;
			tAnchor.fY += tWorld.fY;
		}
	} else {
		iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
		tAnchor = tWorld;
	}
	xuiInternalContextMenuPoint(pEvent, tAnchor, &fX, &fY);
	iNode = (iVisible >= 0 && iVisible < pData->iVisibleCount) ? pData->arrVisible[iVisible] : -1;
	iNodeId = (iNode >= 0) ? pData->arrNodes[iNode].iId : -1;
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	iVisible = (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
	if ( iNode >= 0 && __xuiTreeViewNodeEnabledData(pData, iNode) ) {
		iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 1);
		if ( iRet != XUI_OK ) return iRet;
		if ( !__xuiTreeViewWidgetAlive(pWidget, pData) ) return XUI_EVENT_DISPATCH_STOP;
		iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
		iVisible = (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
	} else {
		iVisible = -1;
		iNode = -1;
	}
	onContext = pData->onContext;
	pContextUser = pData->pContextUser;
	if ( onContext != NULL ) {
		return __xuiOwnedInvoke_xuiTreeViewSetContextMenu(pWidget,pData,onContext,pContextUser ,iNode >= 0 ? pData->arrNodes[iNode].iId : -1,iVisible,fX,fY);
	}
	return XUI_OK;
}

static int __xuiTreeViewVisibleRows(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_rect_t tViewport;
	int iRows;

	tViewport = xuiTreeViewGetViewportRect(pWidget);
	iRows = (pData->fItemHeight > 0.0f) ? (int)(tViewport.fH / pData->fItemHeight) : 1;
	return (iRows > 0) ? iRows : 1;
}

static int __xuiTreeViewKeyDown(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iCurrent;
	int iTarget;
	int iNode;
	int iParentVisible;
	int iRows;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEffectiveEnabled(pWidget) || (pData->iVisibleCount <= 0) ) {
		return XUI_OK;
	}
	iCurrent = __xuiTreeViewVisibleIndexData(pData, pData->iSelectedId);
	if ( iCurrent < 0 ) {
		iCurrent = pData->iFocusVisible;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_UP:
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindEnabledVisible(pData, iCurrent - 1, -1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_DOWN:
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindEnabledVisible(pData, iCurrent + 1, 1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_PAGE_UP:
		iRows = __xuiTreeViewVisibleRows(pWidget, pData);
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindNearestEnabledVisible(pData, iCurrent - iRows, -1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_PAGE_DOWN:
		iRows = __xuiTreeViewVisibleRows(pWidget, pData);
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindNearestEnabledVisible(pData, iCurrent + iRows, 1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_HOME:
		iTarget = __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_END:
		iTarget = __xuiTreeViewFindEnabledVisible(pData, pData->iVisibleCount - 1, -1);
		if ( iTarget >= 0 ) {
			iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_KEY_RIGHT:
		if ( iCurrent >= 0 ) {
			iNode = pData->arrVisible[iCurrent];
			if ( (pData->arrNodes[iNode].bHasChildren != 0) && (pData->arrNodes[iNode].bExpanded == 0) ) {
				iRet = __xuiTreeViewSetExpandedData(pWidget, pData, iNode, 1);
				if ( iRet != XUI_OK ) return iRet;
			} else if ( pData->arrNodes[iNode].bHasChildren != 0 ) {
				iTarget = __xuiTreeViewFindEnabledDescendantVisible(pData, iCurrent);
				if ( iTarget >= 0 ) {
					iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_LEFT:
		if ( iCurrent >= 0 ) {
			iNode = pData->arrVisible[iCurrent];
			if ( (pData->arrNodes[iNode].bHasChildren != 0) && (pData->arrNodes[iNode].bExpanded != 0) ) {
				iRet = __xuiTreeViewSetExpandedData(pWidget, pData, iNode, 0);
				if ( iRet != XUI_OK ) return iRet;
			} else if ( pData->arrNodes[iNode].iParent >= 0 ) {
				iParentVisible = __xuiTreeViewVisibleIndexData(pData, pData->arrNodes[iNode].iParent);
				if ( iParentVisible >= 0 ) {
					iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iParentVisible, 1);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_SPACE:
		if ( iCurrent >= 0 ) {
			iNode = pData->arrVisible[iCurrent];
			if ( pData->arrNodes[iNode].bCheckReserved ) {
				iRet = xuiTreeViewSetNodeChecked(pWidget,
					pData->arrNodes[iNode].iId,
					!pData->arrNodes[iNode].bChecked);
				return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
			}
		}
		/* A non-checkable row keeps the existing activation behavior. */
		/* fall through */
	case XUI_KEY_ENTER:
		if ( pData->iSelectedId >= 0 ) {
			__xuiTreeViewNotifySelect(pWidget, pData, pData->iSelectedId);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiTreeViewEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_tree_view_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiTreeViewPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		return __xuiTreeViewSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( __xuiTreeViewPointerMatches(pData, pEvent) ) {
			__xuiTreeViewCancelPointer(pWidget, pData, 0);
		}
		return __xuiTreeViewSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTreeViewPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTreeViewPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		return __xuiTreeViewDoubleClick(pWidget, pData, pEvent);
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiTreeViewContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiTreeViewKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			return __xuiTreeViewSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->pTarget != pWidget ) return XUI_OK;
		if ( pEvent->iType == XUI_EVENT_ENABLED_CHANGED && pData->pFrame != NULL ) {
			int iRet = xuiWidgetSetEnabled(pData->pFrame,
				xuiWidgetGetEffectiveEnabled(pWidget));
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( !xuiWidgetGetEffectiveEnabled(pWidget) || !xuiWidgetGetEffectiveVisible(pWidget) ) {
			__xuiTreeViewCancelPointer(pWidget, pData, 1);
		}
		{
			int iRet = __xuiTreeViewSyncState(pWidget);
			if ( iRet != XUI_OK ) return iRet;
		}
		return __xuiTreeViewInvalidateRows(pWidget, pData);
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( pEvent->pTarget != pWidget ) return XUI_OK;
		return __xuiTreeViewUpdateContentSize(pWidget, pData);
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiTreeViewFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_tree_view_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	pData->iHoverVisible = -1;
	pData->iChangeCount++;
	(void)__xuiTreeViewInvalidateRows(pWidget, pData);
}

static int __xuiTreeViewContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	float fRows;

	(void)tConstraint;
	pData = (xui_tree_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	fRows = (float)((pData->iVisibleCount > 0 && pData->iVisibleCount < XUI_TREE_VIEW_DEFAULT_ROWS) ? pData->iVisibleCount : XUI_TREE_VIEW_DEFAULT_ROWS);
	if ( fRows < 3.0f ) {
		fRows = 3.0f;
	}
	pSize->fX = xuiInternalSnapSize(XUI_TREE_VIEW_DEFAULT_WIDTH);
	pSize->fY = xuiInternalSnapSize((tResolved.fItemHeight * fRows) + tResolved.fBorderWidth * 2.0f);
	return XUI_OK;
}

static int __xuiTreeViewPrepare(xui_widget pWidget, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_thickness_t tOldMargin;
	xui_thickness_t tMargin;
	int iRet;

	pData = (xui_tree_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	if ( (double)pData->iVisibleCount * (double)tResolved.fItemHeight >
	     (double)XUI_LAYOUT_UNBOUNDED ) return XUI_ERROR_INVALID_ARGUMENT;
	tOldMargin = xuiWidgetGetMargin(pData->pFrame);
	tMargin = (xui_thickness_t){tResolved.fBorderWidth, tResolved.fBorderWidth, tResolved.fBorderWidth, tResolved.fBorderWidth};
	iRet = xuiWidgetSetMargin(pData->pFrame, tMargin);
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewApplyFrameStyle(pWidget, &tResolved);
	if ( iRet != XUI_OK ) {
		(void)xuiWidgetSetMargin(pData->pFrame, tOldMargin);
		(void)__xuiTreeViewApplyFrameStyle(pWidget, pData);
		return iRet;
	}
	pData->fItemHeight = tResolved.fItemHeight;
	pData->fIndent = tResolved.fIndent;
	pData->fPadding = tResolved.fPadding;
	pData->fBorderWidth = tResolved.fBorderWidth;
	return XUI_OK;
}

static int __xuiTreeViewLayoutComplete(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_tree_view_data_t* pData;
	int iRet;
	(void)tContentRect;
	pData = (xui_tree_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTreeViewUpdateContentSize(pWidget, pData);
	return iRet;
}

static int __xuiTreeViewDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTreeViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiTreeViewDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTreeViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiTreeViewPreparePaint(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	xui_tree_view_data_t tResolved;
	int iRet;
	if ( pData == NULL || pData->pFrame == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bPaintStylePrepared && pData->iPreparedStyleHash == xuiWidgetGetStyleHash(pWidget) ) return XUI_OK;
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	iRet = xuiScrollFrameSetColors(pData->pFrame, tResolved.iTrackColor, tResolved.iThumbColor, tResolved.iScrollbarHoverColor, tResolved.iScrollbarActiveColor, tResolved.iScrollbarFocusColor, tResolved.iScrollbarDisabledColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInvalidatePaintIfNeeded(pData->pViewport);
	if ( iRet != XUI_OK ) return iRet;
	pData->iPreparedStyleHash = xuiWidgetGetStyleHash(pWidget);
	pData->bPaintStylePrepared = 1;
	return XUI_OK;
}

static int __xuiTreeViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	int bFocused;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	bFocused = ((xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && xuiWidgetGetEnabled(pWidget)) ? 1 : 0;
	if ( bFocused ) {
		iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iFocusColor);
	}
	return iRet;
}

static uint32_t __xuiTreeViewExpanderHotspotColor(const xui_tree_view_data_t* pResolved, int iState, int bActiveExpander)
{
	if ( (iState & XUI_TREE_ITEM_DISABLED) != 0 ) {
		return pResolved->iExpanderDisabledBackgroundColor;
	}
	if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
		return bActiveExpander ? pResolved->iExpanderSelectedActiveBackgroundColor : pResolved->iExpanderSelectedBackgroundColor;
	}
	if ( bActiveExpander ) {
		return pResolved->iExpanderActiveBackgroundColor;
	}
	if ( (iState & XUI_TREE_ITEM_HOVER) != 0 ) {
		return pResolved->iExpanderHoverBackgroundColor;
	}
	return pResolved->iExpanderBackgroundColor;
}

static uint32_t __xuiTreeViewExpanderGlyphColor(const xui_tree_view_data_t* pResolved, int iState)
{
	if ( pResolved == NULL ) {
		return 0;
	}
	if ( (iState & XUI_TREE_ITEM_DISABLED) != 0 ) {
		return pResolved->iDisabledTextColor;
	}
	if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
		return pResolved->iSelectedExpanderColor;
	}
	return pResolved->iExpanderColor;
}

static int __xuiTreeViewDrawCheck(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bChecked, int bDisabled, uint32_t iColor, uint32_t iDisabledColor, const xui_tree_view_data_t* pResolved)
{
	uint32_t iBorder;
	uint32_t iFill;
	int iRet;

	iBorder = bDisabled ? iDisabledColor : iColor;
	iFill = bChecked ? iColor : pResolved->iCheckBackgroundColor;
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( bChecked && (pProxy->drawLine != NULL) ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 3.0f, tRect.fY + 6.0f, tRect.fX + 5.2f, tRect.fY + 8.2f, 1.7f, pResolved->iCheckMarkColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 5.0f, tRect.fY + 8.1f, tRect.fX + 9.3f, tRect.fY + 3.8f, 1.7f, pResolved->iCheckMarkColor);
	}
	return iRet;
}

static int __xuiTreeViewDrawIcon(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bFolder, uint32_t iColor, const xui_tree_view_data_t* pResolved)
{
	xui_rect_t tBody;
	xui_rect_t tTab;
	int iRet;

	if ( __xuiTreeViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	if ( bFolder ) {
		tTab = xuiInternalSnapRect((xui_rect_t){tRect.fX + 1.0f, tRect.fY + 3.0f, tRect.fW * 0.42f, 3.0f});
		tBody = xuiInternalSnapRect((xui_rect_t){tRect.fX + 1.0f, tRect.fY + 5.0f, tRect.fW - 2.0f, tRect.fH - 6.0f});
		iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tTab, pResolved->iFolderTabColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tBody, pResolved->iFolderBodyColor);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTreeViewDrawRectStroke(pProxy, pDraw, tBody, 1.0f, iColor);
	}
	tBody = xuiInternalSnapRect((xui_rect_t){tRect.fX + 3.0f, tRect.fY + 2.0f, tRect.fW - 6.0f, tRect.fH - 4.0f});
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tBody, pResolved->iFileBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tBody, 1.0f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( pProxy->drawLine != NULL ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tBody.fX + 3.0f, tBody.fY + 5.0f, tBody.fX + tBody.fW - 3.0f, tBody.fY + 5.0f, 1.0f, pResolved->iFileLineColor);
	}
	return iRet;
}

static int __xuiTreeViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_tree_view_node_t tNodeSnapshot;
	xui_tree_view_item_proc onRenderItem;
	void* pRenderItemUser;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tRow;
	xui_rect_t tFill;
	xui_rect_t tText;
	xui_rect_t tExpander;
	xui_rect_t tCheck;
	xui_rect_t tIcon;
	xui_tree_view_node_t* pNode;
	float fOffsetY;
	float fViewportW;
	float fViewportH;
	float fY;
	float fTextX;
	int iStart;
	int iEnd;
	int i;
	int iNode;
	int iState;
	int iHandled;
	int iRet;
	int iGeneration;
	int bActiveExpander;
	uint32_t iTextColor;
	uint32_t iExpanderColor;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiTreeViewGetData(pWidget);
	if ( (pViewport == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pViewport);
	fViewportW = __xuiTreeViewMaxFloat(0.0f, tRect.fW);
	fViewportH = __xuiTreeViewMaxFloat(0.0f, tRect.fH);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pData->iVisibleCount <= 0) || (tResolved.fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	iStart = (int)(fOffsetY / tResolved.fItemHeight);
	if ( iStart < 0 ) iStart = 0;
	iEnd = (int)((fOffsetY + fViewportH) / tResolved.fItemHeight) + 2;
	if ( iEnd > pData->iVisibleCount ) iEnd = pData->iVisibleCount;
	for ( i = iStart; i < iEnd; i++ ) {
		iNode = pData->arrVisible[i];
		pNode = &pData->arrNodes[iNode];
		fY = ((float)i * tResolved.fItemHeight) - fOffsetY;
		tRow = xuiInternalSnapRect((xui_rect_t){0.0f, fY, fViewportW, tResolved.fItemHeight});
		iState = 0;
		if ( pData->iSelectedId == pNode->iId ) iState |= XUI_TREE_ITEM_SELECTED;
		if ( pData->iHoverVisible == i ) iState |= XUI_TREE_ITEM_HOVER;
		if ( !__xuiTreeViewNodeEnabledData(pData, iNode) || !xuiWidgetGetEffectiveEnabled(pWidget) ) iState |= XUI_TREE_ITEM_DISABLED;
		if ( pData->iFocusVisible == i ) iState |= XUI_TREE_ITEM_FOCUS;
		if ( pNode->bExpanded ) iState |= XUI_TREE_ITEM_EXPANDED;
		if ( pNode->bHasChildren ) iState |= XUI_TREE_ITEM_HAS_CHILDREN;
		if ( pNode->bChecked ) iState |= XUI_TREE_ITEM_CHECKED;
		if ( pData->onRenderItem != NULL ) {
			onRenderItem = pData->onRenderItem;
			pRenderItemUser = pData->pRenderItemUser;
			iGeneration = pData->iChangeCount;
			tNodeSnapshot = *pNode;
			iHandled = onRenderItem(pWidget, tNodeSnapshot.iId, i, &tNodeSnapshot,
				pDraw, tRow, iState, pRenderItemUser);
			if ( iHandled < 0 ) return iHandled;
			if ( !__xuiTreeViewWidgetAlive(pWidget, pData) ) return XUI_OK;
			if ( pData->iChangeCount != iGeneration || pData->onRenderItem != onRenderItem ||
			     pData->pRenderItemUser != pRenderItemUser ) return XUI_OK;
			iNode = pData->arrVisible[i];
			pNode = &pData->arrNodes[iNode];
			if ( iHandled ) continue;
		}
		if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iSelectedColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( (iState & XUI_TREE_ITEM_HOVER) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iHoverColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( __xuiTreeViewAlpha(tResolved.iRowColor) != 0 ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tRow, tResolved.iRowColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		tExpander = __xuiTreeViewExpanderRect(&tResolved, pNode, tRow);
		if ( pNode->bHasChildren ) {
			bActiveExpander = (pData->iActiveVisible == i) && (pData->iActivePart == XUI_TREE_VIEW_ACTIVE_EXPANDER);
			tFill = xuiInternalSnapRect((xui_rect_t){tExpander.fX - 1.0f, tExpander.fY - 1.0f, tExpander.fW + 2.0f, tExpander.fH + 2.0f});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, __xuiTreeViewExpanderHotspotColor(&tResolved, iState, bActiveExpander));
			if ( iRet != XUI_OK ) return iRet;
			iExpanderColor = __xuiTreeViewExpanderGlyphColor(&tResolved, iState);
			iRet = xuiInternalDrawDisclosureChevron(pProxy, pDraw, tExpander, pNode->bExpanded, iExpanderColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		fTextX = tExpander.fX + tExpander.fW + 5.0f;
		if ( pNode->bCheckReserved ) {
			tCheck = __xuiTreeViewCheckRect(&tResolved, pNode, tRow);
			iRet = __xuiTreeViewDrawCheck(pProxy, pDraw, tCheck, pNode->bChecked, (iState & XUI_TREE_ITEM_DISABLED) != 0, tResolved.iCheckColor, tResolved.iDisabledTextColor, &tResolved);
			if ( iRet != XUI_OK ) return iRet;
			fTextX = tCheck.fX + tCheck.fW + 6.0f;
		}
		if ( pNode->bIconReserved ) {
			tIcon = xuiInternalSnapRect((xui_rect_t){fTextX, tRow.fY + (tRow.fH - 14.0f) * 0.5f, 15.0f, 14.0f});
            int handled = 0;
            if ( pData->onRenderIcon != NULL ) {
                xui_tree_view_icon_proc callback = pData->onRenderIcon;
                void* user = pData->pRenderIconUser;
                int generation = pData->iChangeCount;
                handled = callback(pWidget, pNode->iId, pDraw, tIcon, iState, user);
                if ( handled < 0 ) return handled;
                if ( !__xuiTreeViewWidgetAlive(pWidget, pData) ) return XUI_OK;
                if ( pData->iChangeCount != generation || pData->onRenderIcon != callback || pData->pRenderIconUser != user ) return XUI_OK;
            }
            if ( !handled ) {
                iRet = __xuiTreeViewDrawIcon(pProxy, pDraw, tIcon, pNode->bHasChildren, ((iState & XUI_TREE_ITEM_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iIconColor, &tResolved);
                if ( iRet != XUI_OK ) return iRet;
            }
			fTextX = tIcon.fX + tIcon.fW + 6.0f;
		}
		iTextColor = ((iState & XUI_TREE_ITEM_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
		if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
			iTextColor = tResolved.iSelectedTextColor;
		}
		if ( (tResolved.pFont != NULL) && (__xuiTreeViewAlpha(iTextColor) != 0) ) {
			tText = xuiInternalSnapRect((xui_rect_t){fTextX, tRow.fY, __xuiTreeViewMaxFloat(1.0f, tRow.fW - fTextX - tResolved.fPadding), tRow.fH});
			iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=tResolved.pFont, .sText=__xuiTreeViewText(pNode->sText), .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tText, iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		/* Keep the row outline above expander/check decoration backgrounds. */
		if ( ((iState & XUI_TREE_ITEM_FOCUS) != 0) &&
		     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
		     ((iState & XUI_TREE_ITEM_DISABLED) == 0) ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tFill, 1.0f, tResolved.iFocusColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiTreeViewDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iMeasureContainment = XUI_MEASURE_CONTAIN_BOTH;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiTreeViewDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTreeViewInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTreeViewEvent, NULL);
	return iRet;
}

static int __xuiTreeViewAccessibleCount(xui_widget pWidget, void* pUser)
{
	xui_tree_view_data_t* pData = (xui_tree_view_data_t*)pUser;

	(void)pWidget;
	if ( pData == NULL || pData->iNodeCount >= 0x7ffffffe ) return 0;
	return pData->iNodeCount + 1;
}

static int __xuiTreeViewAccessibleFocus(xui_widget pWidget,
	uint64_t* pNodeId, void* pUser)
{
	xui_tree_view_data_t* pData = (xui_tree_view_data_t*)pUser;
	int iVisible;
	int iNode;

	if ( pData == NULL || pNodeId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pNodeId = 1;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) != pWidget ) return XUI_OK;
	iVisible = pData->iFocusVisible;
	if ( iVisible < 0 || iVisible >= pData->iVisibleCount ) return XUI_OK;
	iNode = pData->arrVisible[iVisible];
	if ( iNode >= 0 && iNode < pData->iNodeCount ) {
		*pNodeId = (uint64_t)iNode + 2u;
	}
	return XUI_OK;
}

static int __xuiTreeViewAccessibleGet(xui_widget pWidget, int iIndex,
	xui_accessible_node_t* pNode, void* pUser)
{
	xui_tree_view_data_t* pData = (xui_tree_view_data_t*)pUser;
	xui_tree_view_node_t* pTreeNode;
	xui_rect_t tWorld;
	xui_rect_t tViewport;
	xui_rect_t tItem;
	const char* sText;
	size_t iTextSize;
	int iNode;
	int iParent;
	int iVisible;

	if ( pData == NULL || pNode == NULL || iIndex < 0 ||
	     iIndex > pData->iNodeCount ) return XUI_ERROR_INVALID_ARGUMENT;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( iIndex == 0 ) {
		pNode->iId = 1;
		pNode->iRole = XUI_ACCESSIBLE_ROLE_TREE;
		pNode->sName = xuiWidgetGetAccessibleName(pWidget) != NULL ?
			xuiWidgetGetAccessibleName(pWidget) : "Tree";
		pNode->sDescription = xuiWidgetGetAccessibleDescription(pWidget);
		pNode->tBounds = tWorld;
		pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSABLE |
			XUI_ACCESSIBLE_STATE_VERTICAL;
		if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
			pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSED;
		}
		pNode->iActions = XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS);
		pNode->iRowCount = pData->iNodeCount;
		pNode->iColumnCount = 1;
		if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
			pNode->iState |= XUI_ACCESSIBLE_STATE_DISABLED;
			pNode->iActions = 0;
		}
		return XUI_OK;
	}
	iNode = iIndex - 1;
	pTreeNode = &pData->arrNodes[iNode];
	iParent = pData->arrIndex[iNode].iParent;
	iVisible = pData->arrIndex[iNode].iVisible;
	pNode->iId = (uint64_t)iNode + 2u;
	pNode->iParentId = (iParent >= 0) ? (uint64_t)iParent + 2u : 1u;
	pNode->iRole = XUI_ACCESSIBLE_ROLE_TREE_ITEM;
	sText = __xuiTreeViewText(pTreeNode->sText);
	if ( sText[0] != '\0' ) {
		iTextSize = strlen(sText);
		if ( iTextSize >= sizeof(pData->sAccessibleName) ) {
			iTextSize = sizeof(pData->sAccessibleName) - 1u;
		}
		memcpy(pData->sAccessibleName, sText, iTextSize);
		pData->sAccessibleName[iTextSize] = '\0';
		xuiInternalUtf8TrimIncomplete(pData->sAccessibleName);
	} else {
		memcpy(pData->sAccessibleName, "Tree item", sizeof("Tree item"));
	}
	pNode->sName = pData->sAccessibleName;
	pNode->iRow = (iVisible >= 0) ? iVisible : iNode;
	pNode->iColumn = 0;
	pNode->iRowCount = 1;
	pNode->iColumnCount = 1;
	pNode->iState |= XUI_ACCESSIBLE_STATE_SELECTABLE;
	if ( pData->iSelectedId == pTreeNode->iId ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_SELECTED;
	}
	if ( iVisible >= 0 && pData->iFocusVisible == iVisible &&
	     xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSED;
	}
	if ( !pTreeNode->bEnabled || !xuiWidgetGetEffectiveEnabled(pWidget) ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_DISABLED;
	}
	if ( pTreeNode->bHasChildren ) {
		pNode->iState |= pTreeNode->bExpanded ?
			XUI_ACCESSIBLE_STATE_EXPANDED : XUI_ACCESSIBLE_STATE_COLLAPSED;
	}
	if ( pTreeNode->bCheckReserved && pTreeNode->bChecked ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_CHECKED;
	}
	pNode->iActions = XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SCROLL_INTO_VIEW) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SET_SELECTION) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_ACTIVATE);
	if ( pTreeNode->bHasChildren ) {
		pNode->iActions |= XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_EXPAND) |
			XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_COLLAPSE);
	}
	if ( pTreeNode->bCheckReserved ) {
		pNode->iActions |= XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_TOGGLE);
	}
	if ( (pNode->iState & XUI_ACCESSIBLE_STATE_DISABLED) != 0 ) {
		pNode->iActions = 0;
	}
	if ( iVisible >= 0 ) {
		tItem = xuiTreeViewGetItemRect(pWidget, iVisible);
		pNode->tBounds = (xui_rect_t){tWorld.fX + tItem.fX,
			tWorld.fY + tItem.fY, tItem.fW, tItem.fH};
		tViewport = xuiTreeViewGetViewportRect(pWidget);
		if ( tItem.fX + tItem.fW <= tViewport.fX ||
		     tItem.fY + tItem.fH <= tViewport.fY ||
		     tItem.fX >= tViewport.fX + tViewport.fW ||
		     tItem.fY >= tViewport.fY + tViewport.fH ) {
			pNode->iState |= XUI_ACCESSIBLE_STATE_OFFSCREEN;
		}
	} else {
		pNode->tBounds = tWorld;
		pNode->tBounds.fW = 0.0f;
		pNode->tBounds.fH = 0.0f;
		pNode->iState |= XUI_ACCESSIBLE_STATE_OFFSCREEN;
	}
	return XUI_OK;
}

static int __xuiTreeViewAccessibleAction(xui_widget pWidget, uint64_t iNodeId,
	int iAction, const void* pPayload, void* pUser)
{
	xui_tree_view_data_t* pData = (xui_tree_view_data_t*)pUser;
	int iNode;
	int iNodeValue;
	int iVisible;
	int iRet;

	(void)pPayload;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_ERROR_INVALID_STATE;
	if ( iNodeId == 0 || iNodeId == 1 ) {
		return iAction == XUI_ACCESSIBLE_ACTION_FOCUS ?
			xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget) :
			XUI_ERROR_UNSUPPORTED;
	}
	if ( iNodeId < 2 || iNodeId > (uint64_t)pData->iNodeCount + 1u ) {
		return XUI_ERROR_NOT_FOUND;
	}
	iNode = (int)iNodeId - 2;
	if ( !pData->arrNodes[iNode].bEnabled ) return XUI_ERROR_INVALID_STATE;
	iNodeValue = pData->arrNodes[iNode].iId;
	if ( iAction == XUI_ACCESSIBLE_ACTION_EXPAND ||
	     iAction == XUI_ACCESSIBLE_ACTION_COLLAPSE ) {
		if ( !pData->arrNodes[iNode].bHasChildren ) return XUI_ERROR_UNSUPPORTED;
		return xuiTreeViewSetNodeExpanded(pWidget, iNodeValue,
			iAction == XUI_ACCESSIBLE_ACTION_EXPAND);
	}
	if ( iAction == XUI_ACCESSIBLE_ACTION_TOGGLE ) {
		if ( !pData->arrNodes[iNode].bCheckReserved ) return XUI_ERROR_UNSUPPORTED;
		return xuiTreeViewSetNodeChecked(pWidget, iNodeValue,
			!pData->arrNodes[iNode].bChecked);
	}
	if ( iAction == XUI_ACCESSIBLE_ACTION_SCROLL_INTO_VIEW ) {
		return xuiTreeViewEnsureVisible(pWidget, iNodeValue);
	}
	if ( iAction != XUI_ACCESSIBLE_ACTION_FOCUS &&
	     iAction != XUI_ACCESSIBLE_ACTION_SET_SELECTION &&
	     iAction != XUI_ACCESSIBLE_ACTION_ACTIVATE ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iRet = xuiTreeViewEnsureVisible(pWidget, iNodeValue);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeValue);
	iVisible = (iNode >= 0) ? pData->arrIndex[iNode].iVisible : -1;
	if ( iVisible < 0 ) return XUI_ERROR_INVALID_STATE;
	iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible,
		iAction == XUI_ACCESSIBLE_ACTION_SET_SELECTION);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	if ( iAction == XUI_ACCESSIBLE_ACTION_ACTIVATE ) {
		__xuiTreeViewNotifySelect(pWidget, pData, iNodeValue);
	}
	return XUI_OK;
}

static int __xuiTreeViewInitViewport(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pData->pViewport, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pData->pViewport, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCachePolicy(pData->pViewport, &tPolicy);
	if ( iRet == XUI_OK ) {
		iRet = xuiWidgetSetCacheRenderCallback(pData->pViewport,
			__xuiTreeViewViewportRender, pWidget);
	}
	return iRet;
}

static int __xuiTreeViewCreateFrame(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_tree_view_desc_t* pDesc)
{
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;

	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_HIDDEN;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = (pDesc != NULL && pDesc->iScrollbarMode != XUI_SCROLLBAR_MODE_FULL && __xuiTreeViewScrollbarModeValid(pDesc->iScrollbarMode)) ? pDesc->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_NONE;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fItemHeight * 3.0f;
	tFrameDesc.iTrackColor = pData->iTrackColor;
	tFrameDesc.iThumbColor = pData->iThumbColor;
	tFrameDesc.iHoverColor = pData->iScrollbarHoverColor;
	tFrameDesc.iActiveColor = pData->iScrollbarActiveColor;
	tFrameDesc.iFocusColor = pData->iScrollbarFocusColor;
	tFrameDesc.iDisabledColor = pData->iScrollbarDisabledColor;
	iRet = xuiInternalScrollFrameCreatePart(pWidget,&pData->pFrame,&tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInsertPart(pWidget,pWidget,pData->pFrame,NULL);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	iRet = xuiWidgetSetFlowMode(pData->pFrame, XUI_FLOW_BLOCK);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pData->pFrame, XUI_SIZE_FILL, XUI_SIZE_FILL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetAlign(pData->pFrame, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiTreeViewFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	return XUI_OK;
}

static int __xuiTreeViewInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_tree_view_data_t* pData;
	const xui_tree_view_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_tree_view_data_t*)pTypeData;
	pDesc = (const xui_tree_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTreeViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewDefaults(pData);
	__xuiTreeViewApplyDesc(pData, pDesc);
	iRet = xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_OVERLAY);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pWidget, 1);
	if ( iRet == XUI_OK ) {
		iRet = xuiWidgetSetPadding(pWidget,
			(xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	}
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewCreateFrame(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetSetAccessibilityProvider(pWidget,
		__xuiTreeViewAccessibleCount, __xuiTreeViewAccessibleGet,
		__xuiTreeViewAccessibleAction, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalAccessibilitySetFocusResolver(pWidget,
		__xuiTreeViewAccessibleFocus);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->arrNodes != NULL) && (pDesc->iNodeCount > 0) ) {
		iRet = xuiTreeViewSetNodes(pWidget, pDesc->arrNodes, pDesc->iNodeCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->iSelectedId >= 0 ) {
			iRet = xuiTreeViewSetSelected(pWidget, pDesc->iSelectedId);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return __xuiTreeViewSyncState(pWidget);
}

static void __xuiTreeViewDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_tree_view_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_tree_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiTreeViewClearNodeStorage(pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiTreeViewRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiTreeViewRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.check.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.text.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.disabled_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.selected_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.selected_active_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.active_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.hover_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.check.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.check.mark_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.folder_tab_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.folder_body_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.file_background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.file_line_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.track_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.thumb_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.scrollbar.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.item.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.indent", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_tree_view_data_t* __xuiTreeViewGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "treeview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_tree_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiTreeViewGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "treeview");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "treeview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_tree_view_data_t);
	tDesc.onInit = __xuiTreeViewInit;
	tDesc.onDestroy = __xuiTreeViewDestroy;
	tDesc.onContentMeasure = __xuiTreeViewContentMeasure;
	tDesc.onLayoutPrepare = __xuiTreeViewPrepare;
	tDesc.onLayoutComplete = __xuiTreeViewLayoutComplete;
	tDesc.onCacheRender = __xuiTreeViewCacheRender;
	__xuiTreeViewDefaultLayout(&tLayout);
	__xuiTreeViewDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiTreeViewRegisterStyleProperties(pContext, pType);
	pType->onPreparePaint = __xuiTreeViewPreparePaint;
	    static const size_t fontOffsets[]={offsetof(xui_tree_view_data_t,pFont)};
    pType->pFontOffsets=fontOffsets;pType->iFontSlotCount=sizeof(fontOffsets)/sizeof(fontOffsets[0]);
    return pType;
}

static int __xuiTreeViewCreateWithOwner(xui_context pContext, xui_widget* ppWidget, const xui_tree_view_desc_t* pDesc, xui_widget pOwner)
{
	xui_widget_type pType;

	if ( ppWidget == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	if ( !xuiInternalContextIsValid(pContext) || !__xuiTreeViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = xuiTreeViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return pOwner ? xuiInternalWidgetCreatePart(pOwner,pType,ppWidget,pDesc) : xuiWidgetCreateTyped(pContext,pType,ppWidget,pDesc);
}
XUI_API int xuiTreeViewCreate(xui_context pContext,xui_widget* ppWidget,const xui_tree_view_desc_t* pDesc)
{return __xuiTreeViewCreateWithOwner(pContext,ppWidget,pDesc,NULL);}
int xuiInternalTreeViewCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_tree_view_desc_t* pDesc)
{
    if(ppWidget)*ppWidget=NULL;
    xui_context context=xuiWidgetGetContext(pOwner);
    return context ? __xuiTreeViewCreateWithOwner(context,ppWidget,pDesc,pOwner):XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiTreeViewSetSelectOwned(xui_widget pWidget, xui_tree_view_select_proc onSelect, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onSelect, onSelect != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTreeViewSetSelect(xui_widget pWidget, xui_tree_view_select_proc onSelect, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onSelect) )
		return xuiTreeViewSetSelectOwned(pWidget, onSelect, pUser, NULL);
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetContextMenuOwned(xui_widget pWidget, xui_tree_view_context_proc onContext, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t ownedUpdate={0};
    int ownedStatus=xuiInternalCallbackUpdateBegin(pWidget,&pData->onContext,onContext!=NULL,pUser,pOwner,&ownedUpdate);
    if(ownedStatus!=XUI_OK)return ownedStatus;
    pData->onContext = onContext;
	pData->pContextUser = pUser;
    xuiInternalCallbackUpdatePublish(&ownedUpdate);
	return xuiInternalCallbackUpdateEnd(&ownedUpdate,XUI_OK);
}

XUI_API int xuiTreeViewSetContextMenu(xui_widget pWidget, xui_tree_view_context_proc onContext, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if(xuiInternalCallbackSlotOwned(pWidget,&pData->onContext))return xuiTreeViewSetContextMenuOwned(pWidget,onContext,pUser,NULL);
    pData->onContext = onContext;
	pData->pContextUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetItemRenderer(xui_widget pWidget, xui_tree_view_item_proc onRender, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->onRenderItem == onRender && pData->pRenderItemUser == pUser ) return XUI_OK;
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	pData->onRenderItem = onRender;
	pData->pRenderItemUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetIconRenderer(xui_widget widget, xui_tree_view_icon_proc callback, void* user)
{
    xui_tree_view_data_t* data = __xuiTreeViewGetData(widget);
    if ( !data ) return XUI_ERROR_INVALID_ARGUMENT;
    if ( data->onRenderIcon == callback && data->pRenderIconUser == user ) return XUI_OK;
    int result = __xuiTreeViewInvalidateRows(widget, data);
    if ( result != XUI_OK || !__xuiTreeViewWidgetAlive(widget, data) ) return result;
    data->onRenderIcon = callback;
    data->pRenderIconUser = user;
    return XUI_OK;
}

static int __xuiTreeViewSetAdapter(xui_widget widget,xui_tree_view_count_proc count,void* countUser,
    const xui_callback_owner_t* countOwner,xui_tree_view_node_proc rawNode,
    xui_tree_view_node_owned_proc ownedNode,void* nodeUser,const xui_callback_owner_t* nodeOwner)
{
    xui_tree_view_data_t* data=__xuiTreeViewGetData(widget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    if(data->bRefreshingAdapter)return XUI_ERROR_INVALID_STATE;
    xui_callback_group_entry_t entries[]={
        {.pSlot=&data->onCount,.bHasCallback=count!=NULL,.pUser=countUser,.pOwner=countOwner},
        {.pSlot=&data->onNodeOwned,.bHasCallback=ownedNode!=NULL,.pUser=nodeUser,.pOwner=nodeOwner},
    };
    xui_callback_group_update_t update={0};
    int result=xuiInternalCallbackGroupUpdateBegin(widget,entries,2,&update);
    if(result!=XUI_OK)return result;
    data->onCount=count;data->pCountUser=countUser;
    data->onNode=rawNode;data->onNodeOwned=ownedNode;data->pNodeUser=nodeUser;
    xuiInternalCallbackGroupUpdatePublish(&update);
    result=xuiTreeViewRefreshAdapter(widget);
    return xuiInternalCallbackGroupUpdateEnd(&update,result);
}
XUI_API int xuiTreeViewSetAdapterOwned(xui_widget widget,
    xui_tree_view_count_proc count,void* countUser,const xui_callback_owner_t* countOwner,
    xui_tree_view_node_owned_proc node,void* nodeUser,const xui_callback_owner_t* nodeOwner)
{return __xuiTreeViewSetAdapter(widget,count,countUser,countOwner,NULL,node,nodeUser,nodeOwner);}
XUI_API int xuiTreeViewSetAdapter(xui_widget widget,xui_tree_view_count_proc count,xui_tree_view_node_proc node,void* user)
{return __xuiTreeViewSetAdapter(widget,count,user,NULL,node,NULL,user,NULL);}

static int __xuiTreeViewAdapterCurrent(xui_widget widget, xui_tree_view_data_t* data, int change)
{
	return xuiInternalWidgetIsValid(widget) &&
		!xuiInternalContextDestroyPending(xuiWidgetGetContext(widget)) && data->iChangeCount == change;
}

static int __xuiTreeViewRefreshAdapterOperation(xui_widget pWidget)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tStaged;
	xui_tree_view_node_t tNode;
	int iCount;
	int i;
	int iSelectedId;
	int iChange;
	int iReadRet;
	float fScroll;
	int iRet = XUI_OK;

	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bRefreshingAdapter ) return XUI_ERROR_INVALID_STATE;
	if ( (pData->onCount == NULL) || (pData->onNode == NULL && pData->onNodeOwned == NULL) ) {
		return XUI_OK;
	}
	pData->bRefreshingAdapter = 1;
	iChange = pData->iChangeCount;
	tStaged = *pData;
	__xuiTreeViewInitNodeStorage(&tStaged);
    xui_tree_view_count_proc countCallback=pData->onCount;
    void* countUser=pData->pCountUser;
    xui_callback_snapshot_t countSnapshot={0};
    iRet=xuiInternalCallbackSnapshotBeginReturned(pWidget,&pData->onCount,&countUser,&countSnapshot);
    if(iRet!=XUI_OK)goto cleanup;
    iCount=countCallback(pWidget,countUser);
    iRet=xuiInternalCallbackSnapshotEnd(&countSnapshot,pData->onNodeOwned && iCount<0 ? iCount:XUI_OK);
    if(iRet!=XUI_OK)goto cleanup;
	if ( !__xuiTreeViewAdapterCurrent(pWidget, pData, iChange) ) {
		iRet = XUI_ERROR_INVALID_STATE;
		goto cleanup;
	}
	if ( iCount < 0 ) {
		iCount = 0;
	}
	for ( i = 0; i < iCount; i++ ) {
		memset(&tNode, 0, sizeof(tNode));
		tNode.iId = -1;
		tNode.iParent = -1;
		tNode.sText = "";
		tNode.bEnabled = 1;
		tNode.bIconReserved = 1;
        if(pData->onNodeOwned){
            xui_tree_view_node_output_t output;
            xuiTreeViewNodeOutputInit(&output);
            xui_tree_view_node_owned_proc nodeCallback=pData->onNodeOwned;
            void* nodeUser=pData->pNodeUser;
            xui_callback_snapshot_t nodeSnapshot={0};
            iReadRet=xuiInternalCallbackSnapshotBeginReturned(pWidget,&pData->onNodeOwned,&nodeUser,&nodeSnapshot);
            if(iReadRet==XUI_OK)iReadRet=nodeCallback(pWidget,i,&output,nodeUser);
            if(!__xuiTreeViewAdapterCurrent(pWidget,pData,iChange))iReadRet=XUI_ERROR_INVALID_STATE;
            if(iReadRet==XUI_OK){
                tNode=__xuiTreeViewNodeOutputBorrow(&output);
                iReadRet=__xuiTreeViewAppendNodeDataTakeText(&tStaged,&tNode,&output._tText);
            }
            /* Output data is either moved into staged storage or disposed;
             * neither its text nor callback code can outlive this snapshot. */
            xuiTreeViewNodeOutputDispose(&output);
            iRet=xuiInternalCallbackSnapshotEnd(&nodeSnapshot,iReadRet);
            if(iRet!=XUI_OK)goto cleanup;
            continue;
        }
        iReadRet = pData->onNode(pWidget, i, &tNode, pData->pNodeUser);
		if ( !__xuiTreeViewAdapterCurrent(pWidget, pData, iChange) ) {
			iRet = XUI_ERROR_INVALID_STATE;
			goto cleanup;
		}
		if ( iReadRet != XUI_OK ) {
			continue;
		}
		iRet = __xuiTreeViewAppendNodeData(&tStaged, &tNode);
		if ( iRet == XUI_ERROR_OUT_OF_MEMORY ) goto cleanup;
	}
	/* SetNodes publishes owned node storage atomically and keeps current widget
	 * configuration, including changes made by adapter callbacks. */
	iSelectedId = pData->iSelectedId;
	fScroll = xuiTreeViewGetScroll(pWidget);
	iRet = xuiTreeViewSetNodes(pWidget, tStaged.arrNodes, tStaged.iNodeCount);
	if ( iRet == XUI_OK ) {
		iRet = xuiTreeViewSetSelected(pWidget, iSelectedId);
		if ( iRet == XUI_OK ) iRet = xuiTreeViewSetScroll(pWidget, fScroll);
	}
cleanup:
	__xuiTreeViewClearNodeStorage(&tStaged);
	pData->bRefreshingAdapter = 0;
	return iRet;
}

XUI_API int xuiTreeViewRefreshAdapter(xui_widget pWidget)
{
	xui_context context = xuiWidgetGetContext(pWidget);
	int result;
	xuiInternalOperationEnter(context);
	result = __xuiTreeViewRefreshAdapterOperation(pWidget);
	xuiInternalOperationLeave(context);
	return result;
}

XUI_API int xuiTreeViewClear(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	float fOldScroll;
	int iOldVisibleCount;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iNodeCount == 0 && pData->iSelectedId < 0 &&
	     pData->iHoverVisible < 0 && pData->iFocusVisible < 0 &&
	     pData->iActiveVisible < 0 ) return XUI_OK;
	iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	fOldScroll = xuiTreeViewGetScroll(pWidget);
	iOldVisibleCount = pData->iVisibleCount;
	iRet = xuiTreeViewSetScroll(pWidget, 0.0f);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewUpdateContentSizeFor(pWidget, pData, 0,
		pData->fItemHeight);
	if ( iRet != XUI_OK ) {
		(void)__xuiTreeViewUpdateContentSizeFor(pWidget, pData,
			iOldVisibleCount, pData->fItemHeight);
		(void)xuiTreeViewSetScroll(pWidget, fOldScroll);
		return iRet;
	}
	__xuiTreeViewCancelPointer(pWidget, pData, 1);
	__xuiTreeViewResetNodes(pData);
	pData->iSelectedId = -1;
	pData->iHoverVisible = -1;
	pData->iFocusVisible = -1;
	pData->iActiveVisible = -1;
	pData->iActivePointerId = XUI_POINTER_ID_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTreeViewSetNodes(xui_widget pWidget, const xui_tree_view_node_t* pNodes, int iCount)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tStaged;
	xui_tree_view_data_t tOld;
	float fOldScroll;
	int i;
	int iRet;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) || ((iCount > 0) && (pNodes == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	/* Build a separate owned node/visible set. A malformed descriptor or an
	 * allocation failure must leave the currently displayed tree untouched. */
	tStaged = *pData;
	__xuiTreeViewInitNodeStorage(&tStaged);
	tStaged.iSelectedId = -1;
	tStaged.iHoverVisible = -1;
	tStaged.iFocusVisible = -1;
	tStaged.iActiveVisible = -1;
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xuiTreeViewAppendNodeData(&tStaged, &pNodes[i]);
		if ( iRet != XUI_OK ) {
			__xuiTreeViewClearNodeStorage(&tStaged);
			return iRet;
		}
	}
	iRet = __xuiTreeViewRebuildVisible(NULL, &tStaged, -1);
	if ( iRet != XUI_OK ) {
		__xuiTreeViewClearNodeStorage(&tStaged);
		return iRet;
	}
	if ( (double)tStaged.iVisibleCount * (double)tStaged.fItemHeight >
	     (double)XUI_LAYOUT_UNBOUNDED ) {
		__xuiTreeViewClearNodeStorage(&tStaged);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) {
		__xuiTreeViewClearNodeStorage(&tStaged);
		return iRet;
	}
	fOldScroll = xuiTreeViewGetScroll(pWidget);
	iRet = xuiTreeViewSetScroll(pWidget, 0.0f);
	if ( iRet != XUI_OK ) {
		__xuiTreeViewClearNodeStorage(&tStaged);
		return iRet;
	}
	iRet = __xuiTreeViewUpdateContentSizeFor(pWidget, pData,
		tStaged.iVisibleCount, tStaged.fItemHeight);
	if ( iRet != XUI_OK ) {
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		(void)xuiTreeViewSetScroll(pWidget, fOldScroll);
		__xuiTreeViewClearNodeStorage(&tStaged);
		return iRet;
	}
	__xuiTreeViewCancelPointer(pWidget, pData, 1);
	tOld = *pData;
    __xuiTreeViewMoveNodeStorage(pData,&tStaged);
    pData->iSelectedId=-1;pData->iHoverVisible=-1;pData->iFocusVisible=-1;pData->iActiveVisible=-1;
    pData->iChangeCount++;
	__xuiTreeViewClearNodeStorage(&tOld);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTreeViewAddNode(xui_widget pWidget, int iId, int iParentId, const char* sText)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_node_t tNode;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iId < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tNode, 0, sizeof(tNode));
	tNode.iId = iId;
	tNode.iParent = iParentId;
	tNode.sText = sText;
	tNode.bEnabled = 1;
	tNode.bIconReserved = 1;
	return __xuiTreeViewAddNodeData(pWidget, pData, &tNode);
}

XUI_API int xuiTreeViewGetNodeCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iNodeCount : 0;
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetNode(xui_widget pWidget, int iIndex)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iNodeCount) ) return NULL;
	return &pData->arrNodes[iIndex];
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetNodeById(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? &pData->arrNodes[iNode] : NULL;
}

XUI_API int xuiTreeViewFindNode(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewFindNodeData(pData, iNodeId);
}

XUI_API int xuiTreeViewSetNodeExpanded(xui_widget pWidget, int iNodeId, int bExpanded)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTreeViewSetExpandedData(pWidget, pData, iNode, bExpanded);
}

XUI_API int xuiTreeViewGetNodeExpanded(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bExpanded : 0;
}

XUI_API int xuiTreeViewToggleNode(xui_widget pWidget, int iNodeId)
{
	return xuiTreeViewSetNodeExpanded(pWidget, iNodeId, !xuiTreeViewGetNodeExpanded(pWidget, iNodeId));
}

XUI_API int xuiTreeViewSetNodeEnabled(xui_widget pWidget, int iNodeId, int bEnabled)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	int iOldSelectedId;
	int iOldFocusVisible;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	bEnabled = bEnabled ? 1 : 0;
	if ( pData->arrNodes[iNode].bEnabled == bEnabled ) return XUI_OK;
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iOldSelectedId = pData->iSelectedId;
	iOldFocusVisible = pData->iFocusVisible;
	pData->arrNodes[iNode].bEnabled = bEnabled;
	__xuiTreeViewNormalizeVisibleState(pData, -1);
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	if ( pData->iSelectedId != iOldSelectedId ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	}
	if ( pData->iFocusVisible != iOldFocusVisible ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	}
	return XUI_OK;
}

XUI_API int xuiTreeViewGetNodeEnabled(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bEnabled : 0;
}

XUI_API int xuiTreeViewSetNodeChecked(xui_widget pWidget, int iNodeId, int bChecked)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	int bHadCheck;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	bChecked = bChecked ? 1 : 0;
	if ( pData->arrNodes[iNode].bCheckReserved && pData->arrNodes[iNode].bChecked == bChecked ) return XUI_OK;
	bHadCheck = pData->arrNodes[iNode].bCheckReserved;
	{
		int iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
		if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	}
	pData->arrNodes[iNode].bCheckReserved = 1;
	pData->arrNodes[iNode].bChecked = bChecked;
	pData->iChangeCount++;
	if ( !bHadCheck ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	}
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_VALUE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_STATE_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTreeViewGetNodeChecked(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bChecked : 0;
}

XUI_API int xuiTreeViewSetNodeDecorations(xui_widget pWidget, int iNodeId, int bIcon, int bCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	bIcon = bIcon ? 1 : 0;
	bCheck = bCheck ? 1 : 0;
	if ( pData->arrNodes[iNode].bIconReserved == bIcon &&
	     pData->arrNodes[iNode].bCheckReserved == bCheck ) return XUI_OK;
	{
		int iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
		if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	}
	pData->arrNodes[iNode].bIconReserved = bIcon;
	pData->arrNodes[iNode].bCheckReserved = bCheck;
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTreeViewSetSelected(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iVisible;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iVisible = __xuiTreeViewVisibleIndexData(pData, iNodeId);
	return __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 0);
}

XUI_API int xuiTreeViewGetSelected(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedId : -1;
}

XUI_API int xuiTreeViewGetVisibleCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iVisibleCount : 0;
}

XUI_API int xuiTreeViewGetVisibleNodeId(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) return -1;
	return pData->arrNodes[pData->arrVisible[iVisible]].iId;
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetVisibleNode(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) return NULL;
	return &pData->arrNodes[pData->arrVisible[iVisible]];
}

XUI_API int xuiTreeViewGetVisibleIndexOfId(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewVisibleIndexData(pData, iNodeId);
}

XUI_API int xuiTreeViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFont == pFont ) return XUI_OK;
    xui_font_slot_update_t fontUpdate={0};
    int fontStatus=xuiInternalFontSlotPrepare(pWidget->pContext,&pData->pFont,pFont,pWidget->bTypeInitialized!=0,false,&fontUpdate);
    if(fontStatus!=XUI_OK)return fontStatus;
    fontUpdate.widget=pWidget;
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
	fontStatus=xuiInternalFontSlotPublish(&fontUpdate);
    if(fontStatus!=XUI_OK)return xuiInternalFontSlotEnd(&fontUpdate,true,fontStatus);
	return xuiInternalFontSlotEnd(&fontUpdate,true,XUI_OK);
}

XUI_API xui_font xuiTreeViewGetFont(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiTreeViewSetMetrics(xui_widget pWidget, float fItemHeight, float fIndent, float fPadding, float fBorderWidth)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	xui_tree_view_data_t tCandidate;
	xui_tree_view_data_t tOldData;
	float fNewBaseItemHeight;
	float fNewBaseIndent;
	float fNewBasePadding;
	float fNewBaseBorderWidth;
	int iRet;
	if ( (pData == NULL) ||
	     ((fItemHeight != 0.0f) && !__xuiTreeViewFloatValid(fItemHeight)) ||
	     ((fIndent != 0.0f) && !__xuiTreeViewFloatValid(fIndent)) ||
	     ((fPadding != 0.0f) && !__xuiTreeViewFloatValid(fPadding)) ||
	     ((fBorderWidth != 0.0f) && !__xuiTreeViewFloatValid(fBorderWidth)) ) return XUI_ERROR_INVALID_ARGUMENT;
	fNewBaseItemHeight = (fItemHeight > 0.0f) ? fItemHeight : pData->fBaseItemHeight;
	fNewBaseIndent = (fIndent > 0.0f) ? fIndent : pData->fBaseIndent;
	fNewBasePadding = (fPadding > 0.0f) ? fPadding : pData->fBasePadding;
	fNewBaseBorderWidth = fBorderWidth;
	if ( (double)pData->iVisibleCount * (double)fNewBaseItemHeight >
	     (double)XUI_LAYOUT_UNBOUNDED ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->fBaseItemHeight == fNewBaseItemHeight && pData->fBaseIndent == fNewBaseIndent &&
	     pData->fBasePadding == fNewBasePadding && pData->fBaseBorderWidth == fNewBaseBorderWidth ) return XUI_OK;
	tCandidate = *pData;
	tCandidate.fBaseItemHeight = fNewBaseItemHeight;
	tCandidate.fBaseIndent = fNewBaseIndent;
	tCandidate.fBasePadding = fNewBasePadding;
	tCandidate.fBaseBorderWidth = fNewBaseBorderWidth;
	__xuiTreeViewResolve(pWidget, &tCandidate, &tCandidate);
	if ( (double)pData->iVisibleCount * (double)tCandidate.fItemHeight >
	     (double)XUI_LAYOUT_UNBOUNDED ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	__xuiTreeViewCancelPointer(pWidget, pData, 1);
	tOldData = *pData;
	pData->fBaseItemHeight = fNewBaseItemHeight;
	pData->fBaseIndent = fNewBaseIndent;
	pData->fBasePadding = fNewBasePadding;
	pData->fBaseBorderWidth = fNewBaseBorderWidth;
	pData->fItemHeight = tCandidate.fItemHeight;
	pData->fIndent = tCandidate.fIndent;
	pData->fPadding = tCandidate.fPadding;
	pData->fBorderWidth = tCandidate.fBorderWidth;
	iRet = __xuiTreeViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) {
		*pData = tOldData;
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		return iRet;
	}
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTreeViewGetMetrics(xui_widget pWidget, float* pItemHeight, float* pIndent, float* pPadding, float* pBorderWidth)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pIndent != NULL ) *pIndent = pData->fIndent;
	if ( pPadding != NULL ) *pPadding = pData->fPadding;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetScroll(xui_widget pWidget, float fOffsetY)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetOffset(pData->pFrame, 0.0f, fOffsetY);
}

XUI_API float xuiTreeViewGetScroll(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	float fOffsetY;
	fOffsetY = 0.0f;
	if ( (pData != NULL) && (pData->pFrame != NULL) ) {
		(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	}
	return fOffsetY;
}

XUI_API int xuiTreeViewEnsureVisible(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	xui_rect_t tTarget;
	int* arrParents;
	int iNode;
	int iParent;
	int iVisible;
	int iParentCount;
	int iCursor;
	int iNewVisibleCount;
	int iOldSelectedId;
	int iOldFocusVisible;
	int i;
	float fOldScroll;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iParentCount = 0;
	for ( iParent = pData->arrIndex[iNode].iParent; iParent >= 0;
	      iParent = pData->arrIndex[iParent].iParent ) {
		if ( !pData->arrNodes[iParent].bExpanded ) iParentCount++;
	}
	if ( iParentCount == 0 ) {
		iVisible = pData->arrIndex[iNode].iVisible;
		return __xuiTreeViewEnsureVisibleIndex(pWidget, pData, iVisible);
	}
	arrParents = (int*)xrtMalloc((size_t)iParentCount * sizeof(int));
	if ( arrParents == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	i = 0;
	for ( iParent = pData->arrIndex[iNode].iParent; iParent >= 0;
	      iParent = pData->arrIndex[iParent].iParent ) {
		if ( !pData->arrNodes[iParent].bExpanded ) arrParents[i++] = iParent;
	}
	iRet = __xuiTreeViewEnsureVisibleCapacity(pData, pData->iNodeCount);
	if ( iRet != XUI_OK ) {
		xrtFree(arrParents);
		return iRet;
	}
	for ( i = 0; i < iParentCount; i++ ) pData->arrNodes[arrParents[i]].bExpanded = 1;
	iNewVisibleCount = 0;
	iVisible = -1;
	iCursor = pData->iFirstRoot;
	while ( iCursor >= 0 ) {
		if ( iCursor == iNode ) iVisible = iNewVisibleCount;
		iNewVisibleCount++;
		iCursor = __xuiTreeViewNextVisibleNode(pData, iCursor, -1);
	}
	for ( i = 0; i < iParentCount; i++ ) pData->arrNodes[arrParents[i]].bExpanded = 0;
	if ( iVisible < 0 ||
	     (double)iNewVisibleCount * (double)pData->fItemHeight >
	     (double)XUI_LAYOUT_UNBOUNDED ) {
		xrtFree(arrParents);
		return XUI_ERROR_INVALID_STATE;
	}
	iRet = __xuiTreeViewInvalidateLayoutRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) {
		xrtFree(arrParents);
		return iRet;
	}
	fOldScroll = xuiTreeViewGetScroll(pWidget);
	iRet = __xuiTreeViewUpdateContentSizeFor(pWidget, pData,
		iNewVisibleCount, pData->fItemHeight);
	if ( iRet == XUI_OK ) {
		tTarget = (xui_rect_t){0.0f, (float)iVisible * pData->fItemHeight,
			1.0f, pData->fItemHeight};
		iRet = xuiScrollFrameEnsureRectVisible(pData->pFrame, tTarget);
	}
	if ( iRet != XUI_OK ) {
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		(void)xuiTreeViewSetScroll(pWidget, fOldScroll);
		xrtFree(arrParents);
		return iRet;
	}
	__xuiTreeViewCancelPointer(pWidget, pData, 1);
	iOldSelectedId = pData->iSelectedId;
	iOldFocusVisible = pData->iFocusVisible;
	for ( i = 0; i < iParentCount; i++ ) pData->arrNodes[arrParents[i]].bExpanded = 1;
	iRet = __xuiTreeViewRebuildVisible(pWidget, pData, -1);
	if ( iRet != XUI_OK ) {
		for ( i = 0; i < iParentCount; i++ ) {
			pData->arrNodes[arrParents[i]].bExpanded = 0;
		}
		(void)__xuiTreeViewRebuildVisible(NULL, pData, -1);
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		(void)xuiTreeViewSetScroll(pWidget, fOldScroll);
		xrtFree(arrParents);
		return iRet;
	}
	xrtFree(arrParents);
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	if ( pData->iSelectedId != iOldSelectedId ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	}
	if ( pData->iFocusVisible != iOldFocusVisible ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	}
	return XUI_OK;
}

XUI_API int xuiTreeViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) || !__xuiTreeViewScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
}

XUI_API int xuiTreeViewGetScrollbarMode(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetScrollbarMode(pData->pFrame) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiTreeViewSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder, uint32_t iFocus, uint32_t iRow, uint32_t iHover, uint32_t iSelected, uint32_t iText, uint32_t iDisabledText)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iBackgroundColor == iBackground && pData->iBorderColor == iBorder &&
	     pData->iFocusColor == iFocus && pData->iRowColor == iRow &&
	     pData->iHoverColor == iHover && pData->iSelectedColor == iSelected &&
	     pData->iTextColor == iText && pData->iDisabledTextColor == iDisabledText ) return XUI_OK;
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	pData->iBackgroundColor = iBackground;
	pData->iBorderColor = iBorder;
	pData->iFocusColor = iFocus;
	pData->iRowColor = iRow;
	pData->iHoverColor = iHover;
	pData->iSelectedColor = iSelected;
	pData->iTextColor = iText;
	pData->iDisabledTextColor = iDisabledText;
	return XUI_OK;
}

XUI_API int xuiTreeViewGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder, uint32_t* pFocus, uint32_t* pRow, uint32_t* pHover, uint32_t* pSelected, uint32_t* pText, uint32_t* pDisabledText)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pRow != NULL ) *pRow = pData->iRowColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pSelected != NULL ) *pSelected = pData->iSelectedColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetDecorationColors(xui_widget pWidget, uint32_t iExpander, uint32_t iIcon, uint32_t iCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iExpanderColor == iExpander && pData->iIconColor == iIcon &&
	     pData->iCheckColor == iCheck ) return XUI_OK;
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	pData->iExpanderColor = iExpander;
	pData->iIconColor = iIcon;
	pData->iCheckColor = iCheck;
	return XUI_OK;
}

XUI_API int xuiTreeViewGetDecorationColors(xui_widget pWidget, uint32_t* pExpander, uint32_t* pIcon, uint32_t* pCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pExpander != NULL ) *pExpander = pData->iExpanderColor;
	if ( pIcon != NULL ) *pIcon = pData->iIconColor;
	if ( pCheck != NULL ) *pCheck = pData->iCheckColor;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetScrollbarColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_tree_view_data_t tCandidate;
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iTrackColor == iTrack && pData->iThumbColor == iThumb &&
	     pData->iScrollbarHoverColor == iHover && pData->iScrollbarActiveColor == iActive &&
	     pData->iScrollbarFocusColor == iFocus && pData->iScrollbarDisabledColor == iDisabled ) return XUI_OK;
	tCandidate = *pData;
	tCandidate.iTrackColor = iTrack;
	tCandidate.iThumbColor = iThumb;
	tCandidate.iScrollbarHoverColor = iHover;
	tCandidate.iScrollbarActiveColor = iActive;
	tCandidate.iScrollbarFocusColor = iFocus;
	tCandidate.iScrollbarDisabledColor = iDisabled;
	__xuiTreeViewResolve(pWidget, &tCandidate, &tCandidate);
	iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
	if ( iRet != XUI_OK || !__xuiTreeViewWidgetAlive(pWidget, pData) ) return iRet;
	iRet = xuiScrollFrameSetColors(pData->pFrame, tCandidate.iTrackColor,
		tCandidate.iThumbColor, tCandidate.iScrollbarHoverColor,
		tCandidate.iScrollbarActiveColor, tCandidate.iScrollbarFocusColor,
		tCandidate.iScrollbarDisabledColor);
	if ( iRet != XUI_OK ) return iRet;
	pData->iTrackColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iScrollbarHoverColor = iHover;
	pData->iScrollbarActiveColor = iActive;
	pData->iScrollbarFocusColor = iFocus;
	pData->iScrollbarDisabledColor = iDisabled;
	return XUI_OK;
}

XUI_API int xuiTreeViewGetScrollbarColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iScrollbarHoverColor;
	if ( pActive != NULL ) *pActive = pData->iScrollbarActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iScrollbarFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iScrollbarDisabledColor;
	return XUI_OK;
}

XUI_API xui_widget xuiTreeViewGetFrameWidget(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiTreeViewGetViewportWidget(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_scroll_model_t* xuiTreeViewGetModel(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API xui_rect_t xuiTreeViewGetViewportRect(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewViewportRectLocal(pWidget, pData);
}

XUI_API xui_rect_t xuiTreeViewGetItemRect(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tViewport = xuiTreeViewGetViewportRect(pWidget);
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	return (xui_rect_t){tViewport.fX, tViewport.fY + (float)iVisible * pData->fItemHeight - fOffsetY, tViewport.fW, pData->fItemHeight};
}

XUI_API int xuiTreeViewGetItemAt(xui_widget pWidget, float fX, float fY)
{
	xui_tree_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;
	float fLocalY;
	int iVisible;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->fItemHeight <= 0.0f) ) return -1;
	tViewport = xuiTreeViewGetViewportRect(pWidget);
	if ( !__xuiTreeViewPointInRect(tViewport, fX, fY) ) return -1;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewport.fY + fOffsetY;
	iVisible = (int)(fLocalY / pData->fItemHeight);
	return (iVisible >= 0 && iVisible < pData->iVisibleCount) ? iVisible : -1;
}

XUI_API int xuiTreeViewGetHoverIndex(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iHoverVisible : -1;
}

XUI_API int xuiTreeViewGetFocusIndex(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iFocusVisible : -1;
}

XUI_API int xuiTreeViewGetSelectCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiTreeViewGetChangeCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

#endif
