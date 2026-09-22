#include "xui_internal.h"

#include <string.h>

typedef struct xui_workflow_widget_data_t {
	xui_workflow pWorkflow;
	int bOwnWorkflow;
	xui_widget pCanvas;
} xui_workflow_widget_data_t;

static int __xuiWorkflowWidgetDescValid(const xui_workflow_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( pDesc->iSize != 0u && pDesc->iSize < sizeof(*pDesc) ) {
		return 0;
	}
	return (pDesc->tGraph.iSize == 0u || pDesc->tGraph.iSize >= sizeof(pDesc->tGraph));
}

static xui_workflow_widget_data_t* __xuiWorkflowWidgetGetData(xui_widget pWidget)
{
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pType = xuiWorkflowGetType(xuiWidgetGetContext(pWidget));
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_workflow_widget_data_t*)xuiWidgetGetTypeData(pWidget);
}

static void __xuiWorkflowWidgetDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_DOCK;
	pLayout->iWidthMode = XUI_SIZE_FILL;
	pLayout->iHeightMode = XUI_SIZE_FILL;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_STRETCH;
	pLayout->iAlignY = XUI_ALIGN_STRETCH;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static int __xuiWorkflowWidgetInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_workflow_widget_data_t* pData;
	const xui_workflow_desc_t* pDesc;
	xui_flow_graph_desc_t tGraphDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || !__xuiWorkflowWidgetDescValid((const xui_workflow_desc_t*)pCreateData) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_workflow_widget_data_t*)pTypeData;
	pDesc = (const xui_workflow_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	if ( pDesc != NULL ) {
		pData->pWorkflow = pDesc->pWorkflow;
		pData->bOwnWorkflow = pDesc->bOwnWorkflow ? 1 : 0;
	}
	if ( pData->pWorkflow == NULL ) {
		iRet = xuiWorkflowCreate(&pData->pWorkflow);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pData->bOwnWorkflow = 1;
	}
	memset(&tGraphDesc, 0, sizeof(tGraphDesc));
	tGraphDesc.iSize = sizeof(tGraphDesc);
	if ( pDesc != NULL && pDesc->tGraph.iSize >= sizeof(pDesc->tGraph) ) {
		tGraphDesc = pDesc->tGraph;
		tGraphDesc.iSize = sizeof(tGraphDesc);
	}
	tGraphDesc.pGraph = xuiWorkflowGetGraph(pData->pWorkflow);
	tGraphDesc.bOwnGraph = 0;
	iRet = xuiFlowGraphWidgetCreate(xuiWidgetGetContext(pWidget), &pData->pCanvas, &tGraphDesc);
	if ( iRet != XUI_OK ) {
		if ( pData->bOwnWorkflow && pData->pWorkflow != NULL ) {
			xuiWorkflowDestroy(pData->pWorkflow);
		}
		memset(pData, 0, sizeof(*pData));
		return iRet;
	}
	(void)xuiWidgetSetSizeMode(pData->pCanvas, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetDock(pData->pCanvas, XUI_DOCK_FILL);
	(void)xuiWidgetSetAlign(pData->pCanvas, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);
	iRet = xuiWidgetAddChild(pWidget, pData->pCanvas);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pCanvas);
		pData->pCanvas = NULL;
		if ( pData->bOwnWorkflow && pData->pWorkflow != NULL ) {
			xuiWorkflowDestroy(pData->pWorkflow);
		}
		memset(pData, 0, sizeof(*pData));
		return iRet;
	}
	return XUI_OK;
}

static void __xuiWorkflowWidgetDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_workflow_widget_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_workflow_widget_data_t*)pTypeData;
	if ( pData != NULL ) {
		if ( pData->bOwnWorkflow && pData->pWorkflow != NULL ) {
			xuiWorkflowDestroy(pData->pWorkflow);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

XUI_API xui_widget_type xuiWorkflowGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "workflow");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "workflow";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT;
	tDesc.iTypeDataSize = sizeof(xui_workflow_widget_data_t);
	tDesc.onInit = __xuiWorkflowWidgetInit;
	tDesc.onDestroy = __xuiWorkflowWidgetDestroy;
	__xuiWorkflowWidgetDefaultLayout(&tDesc.tLayout);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	return (iRet == XUI_OK) ? pType : NULL;
}

XUI_API int xuiWorkflowWidgetCreate(xui_context pContext, xui_widget* ppWidget, const xui_workflow_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiWorkflowWidgetDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiWorkflowGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_workflow xuiWorkflowWidgetGetWorkflow(xui_widget pWidget)
{
	xui_workflow_widget_data_t* pData;

	pData = __xuiWorkflowWidgetGetData(pWidget);
	return (pData != NULL) ? pData->pWorkflow : NULL;
}

XUI_API int xuiWorkflowWidgetSetWorkflow(xui_widget pWidget, xui_workflow pWorkflow, int bOwnWorkflow)
{
	xui_workflow_widget_data_t* pData;
	int iRet;

	pData = __xuiWorkflowWidgetGetData(pWidget);
	if ( (pData == NULL) || (pWorkflow == NULL) || (xuiWorkflowGetGraph(pWorkflow) == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphWidgetSetGraph(pData->pCanvas, xuiWorkflowGetGraph(pWorkflow), 0);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pData->bOwnWorkflow && pData->pWorkflow != NULL && pData->pWorkflow != pWorkflow ) {
		xuiWorkflowDestroy(pData->pWorkflow);
	}
	pData->pWorkflow = pWorkflow;
	pData->bOwnWorkflow = bOwnWorkflow ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_widget xuiWorkflowWidgetGetCanvas(xui_widget pWidget)
{
	xui_workflow_widget_data_t* pData;

	pData = __xuiWorkflowWidgetGetData(pWidget);
	return (pData != NULL) ? pData->pCanvas : NULL;
}
