#include "../xui.h"

#include <stdio.h>
#include <string.h>

#define XUI_FLOW_COMMAND_HISTORY_DEFAULT_LIMIT 256

typedef struct xui_flow_port_model_t {
	char* sId;
	char* sTitle;
	char* sDataType;
	int iDirection;
	int iKind;
	int bRequired;
	int bMulti;
	int bDynamic;
} xui_flow_port_model_t;

typedef struct xui_flow_node_model_t {
	char* sId;
	char* sType;
	char* sTitle;
	char* sSummary;
	float fX;
	float fY;
	float fW;
	float fH;
	xvalue pConfig;
	int iRunState;
	char* sRunPreview;
	xarray_struct arrPorts;
	xdict_struct mapPorts;
} xui_flow_node_model_t;

typedef struct xui_flow_edge_model_t {
	char* sId;
	int iKind;
	char* sFromNode;
	char* sFromPort;
	char* sToNode;
	char* sToPort;
	int iRouteStyle;
	float fRouteBias;
	float fRouteSourceOffset;
	float fRouteTargetOffset;
	int iFromNode;
	int iFromPort;
	int iToNode;
	int iToPort;
	int bInvalid;
	int iRunState;
	char* sRunPreview;
} xui_flow_edge_model_t;

typedef struct xui_flow_diagnostic_model_t {
	int iSeverity;
	char* sCode;
	char* sMessage;
	char* sNode;
	char* sEdge;
	char* sPath;
} xui_flow_diagnostic_model_t;

typedef struct xui_flow_command_model_t {
	int iType;
	xui_flow_node_desc_t tNode;
	xui_flow_edge_desc_t tEdge;
	xarray_struct arrPorts;
	xarray_struct arrEdges;
	xarray_struct arrMoves;
	char* sNodeId;
	float fOldX;
	float fOldY;
	float fNewX;
	float fNewY;
	xvalue pOldConfig;
	xvalue pNewConfig;
} xui_flow_command_model_t;

struct xui_flow_graph_t {
	xarray_struct arrNodes;
	xarray_struct arrEdges;
	xdict_struct mapNodes;
	xdict_struct mapEdges;
	xarray_struct arrSelectedNodes;
	xarray_struct arrSelectedEdges;
	xarray_struct arrDiagnostics;
	xarray_struct arrUndo;
	xarray_struct arrRedo;
	xui_flow_viewport_t tViewport;
	uint32_t iNextNodeId;
	uint32_t iNextEdgeId;
	uint32_t iRevision;
	uint32_t iCleanRevision;
	int iCommandHistoryLimit;
};

static int __xuiFlowPortDescValid(const xui_flow_port_desc_t* pDesc);
static int __xuiFlowPortFind(const xui_flow_node_model_t* pNode, const char* sId);

static void __xuiFlowBumpRevision(xui_flow_graph pGraph)
{
	if ( pGraph != NULL ) {
		pGraph->iRevision++;
		if ( pGraph->iRevision == 0u ) {
			pGraph->iRevision = 1u;
		}
	}
}

static char* __xuiFlowCopyString(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText);
	sCopy = (char*)xrtMalloc(iSize + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iSize + 1u);
	return sCopy;
}

static void __xuiFlowFreePort(xui_flow_port_model_t* pPort)
{
	if ( pPort == NULL ) {
		return;
	}
	xrtFree(pPort->sId);
	xrtFree(pPort->sTitle);
	xrtFree(pPort->sDataType);
	memset(pPort, 0, sizeof(*pPort));
}

static void __xuiFlowFreeNode(xui_flow_node_model_t* pNode)
{
	uint32_t i;

	if ( pNode == NULL ) {
		return;
	}
	for ( i = 1u; i <= pNode->arrPorts.Count; ++i ) {
		__xuiFlowFreePort((xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, i));
	}
	xrtArrayUnit(&pNode->arrPorts);
	xrtDictUnit(&pNode->mapPorts);
	if ( pNode->pConfig != NULL ) {
		xvoUnref(pNode->pConfig);
	}
	xrtFree(pNode->sId);
	xrtFree(pNode->sType);
	xrtFree(pNode->sTitle);
	xrtFree(pNode->sSummary);
	xrtFree(pNode->sRunPreview);
	memset(pNode, 0, sizeof(*pNode));
}

static void __xuiFlowFreeEdge(xui_flow_edge_model_t* pEdge)
{
	if ( pEdge == NULL ) {
		return;
	}
	xrtFree(pEdge->sId);
	xrtFree(pEdge->sFromNode);
	xrtFree(pEdge->sFromPort);
	xrtFree(pEdge->sToNode);
	xrtFree(pEdge->sToPort);
	xrtFree(pEdge->sRunPreview);
	memset(pEdge, 0, sizeof(*pEdge));
}

static void __xuiFlowFreeDiagnostic(xui_flow_diagnostic_model_t* pDiagnostic)
{
	if ( pDiagnostic == NULL ) {
		return;
	}
	xrtFree(pDiagnostic->sCode);
	xrtFree(pDiagnostic->sMessage);
	xrtFree(pDiagnostic->sNode);
	xrtFree(pDiagnostic->sEdge);
	xrtFree(pDiagnostic->sPath);
	memset(pDiagnostic, 0, sizeof(*pDiagnostic));
}

static void __xuiFlowFreePortDesc(xui_flow_port_desc_t* pPort)
{
	if ( pPort == NULL ) {
		return;
	}
	xrtFree((void*)pPort->sId);
	xrtFree((void*)pPort->sTitle);
	xrtFree((void*)pPort->sDataType);
	memset(pPort, 0, sizeof(*pPort));
}

static void __xuiFlowFreeEdgeDesc(xui_flow_edge_desc_t* pEdge)
{
	if ( pEdge == NULL ) {
		return;
	}
	xrtFree((void*)pEdge->sId);
	xrtFree((void*)pEdge->sFromNode);
	xrtFree((void*)pEdge->sFromPort);
	xrtFree((void*)pEdge->sToNode);
	xrtFree((void*)pEdge->sToPort);
	memset(pEdge, 0, sizeof(*pEdge));
}

static void __xuiFlowFreeCommand(xui_flow_command_model_t* pCommand)
{
	uint32_t i;
	xui_flow_move_node_record_t* pMove;

	if ( pCommand == NULL ) {
		return;
	}
	xrtFree((void*)pCommand->tNode.sId);
	xrtFree((void*)pCommand->tNode.sType);
	xrtFree((void*)pCommand->tNode.sTitle);
	xrtFree((void*)pCommand->tNode.sSummary);
	__xuiFlowFreeEdgeDesc(&pCommand->tEdge);
	for ( i = 1u; i <= pCommand->arrPorts.Count; ++i ) {
		__xuiFlowFreePortDesc((xui_flow_port_desc_t*)xrtArrayGet_Unsafe(&pCommand->arrPorts, i));
	}
	xrtArrayUnit(&pCommand->arrPorts);
	for ( i = 1u; i <= pCommand->arrEdges.Count; ++i ) {
		__xuiFlowFreeEdgeDesc((xui_flow_edge_desc_t*)xrtArrayGet_Unsafe(&pCommand->arrEdges, i));
	}
	xrtArrayUnit(&pCommand->arrEdges);
	for ( i = 1u; i <= pCommand->arrMoves.Count; ++i ) {
		pMove = (xui_flow_move_node_record_t*)xrtArrayGet_Unsafe(&pCommand->arrMoves, i);
		xrtFree((void*)pMove->sId);
		memset(pMove, 0, sizeof(*pMove));
	}
	xrtArrayUnit(&pCommand->arrMoves);
	if ( pCommand->pOldConfig != NULL ) {
		xvoUnref(pCommand->pOldConfig);
	}
	if ( pCommand->pNewConfig != NULL ) {
		xvoUnref(pCommand->pNewConfig);
	}
	xrtFree(pCommand->sNodeId);
	memset(pCommand, 0, sizeof(*pCommand));
}

static int __xuiFlowCopyNodeDesc(xui_flow_node_desc_t* pDst, const xui_flow_node_desc_t* pSrc)
{
	if ( (pDst == NULL) || (pSrc == NULL) || (pSrc->iSize < sizeof(*pSrc)) ||
	     (pSrc->sId == NULL) || (pSrc->sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pDst, 0, sizeof(*pDst));
	pDst->iSize = sizeof(*pDst);
	pDst->sId = __xuiFlowCopyString(pSrc->sId);
	pDst->sType = __xuiFlowCopyString(pSrc->sType);
	pDst->sTitle = __xuiFlowCopyString(pSrc->sTitle);
	pDst->sSummary = __xuiFlowCopyString(pSrc->sSummary);
	pDst->fX = pSrc->fX;
	pDst->fY = pSrc->fY;
	pDst->fW = pSrc->fW;
	pDst->fH = pSrc->fH;
	if ( (pDst->sId == NULL) || ((pSrc->sType != NULL) && (pDst->sType == NULL)) ||
	     ((pSrc->sTitle != NULL) && (pDst->sTitle == NULL)) ||
	     ((pSrc->sSummary != NULL) && (pDst->sSummary == NULL)) ) {
		xrtFree((void*)pDst->sId);
		xrtFree((void*)pDst->sType);
		xrtFree((void*)pDst->sTitle);
		xrtFree((void*)pDst->sSummary);
		memset(pDst, 0, sizeof(*pDst));
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiFlowCopyPortDesc(xui_flow_port_desc_t* pDst, const xui_flow_port_desc_t* pSrc)
{
	if ( (pDst == NULL) || !__xuiFlowPortDescValid(pSrc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pDst, 0, sizeof(*pDst));
	pDst->iSize = sizeof(*pDst);
	pDst->sId = __xuiFlowCopyString(pSrc->sId);
	pDst->sTitle = __xuiFlowCopyString(pSrc->sTitle);
	pDst->sDataType = __xuiFlowCopyString(pSrc->sDataType);
	pDst->iDirection = pSrc->iDirection;
	pDst->iKind = pSrc->iKind;
	pDst->bRequired = pSrc->bRequired;
	pDst->bMulti = pSrc->bMulti;
	pDst->bDynamic = pSrc->bDynamic;
	if ( (pDst->sId == NULL) || ((pSrc->sTitle != NULL) && (pDst->sTitle == NULL)) ||
	     ((pSrc->sDataType != NULL) && (pDst->sDataType == NULL)) ) {
		__xuiFlowFreePortDesc(pDst);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiFlowCopyEdgeDesc(xui_flow_edge_desc_t* pDst, const xui_flow_edge_desc_t* pSrc)
{
	if ( (pDst == NULL) || (pSrc == NULL) || (pSrc->iSize < sizeof(*pSrc)) ||
	     (pSrc->sId == NULL) || (pSrc->sId[0] == 0) || (pSrc->sFromNode == NULL) ||
	     (pSrc->sFromPort == NULL) || (pSrc->sToNode == NULL) || (pSrc->sToPort == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pDst, 0, sizeof(*pDst));
	pDst->iSize = sizeof(*pDst);
	pDst->sId = __xuiFlowCopyString(pSrc->sId);
	pDst->sFromNode = __xuiFlowCopyString(pSrc->sFromNode);
	pDst->sFromPort = __xuiFlowCopyString(pSrc->sFromPort);
	pDst->sToNode = __xuiFlowCopyString(pSrc->sToNode);
	pDst->sToPort = __xuiFlowCopyString(pSrc->sToPort);
	pDst->iKind = pSrc->iKind;
	pDst->iRouteStyle = pSrc->iRouteStyle;
	pDst->fRouteBias = pSrc->fRouteBias;
	pDst->fRouteSourceOffset = pSrc->fRouteSourceOffset;
	pDst->fRouteTargetOffset = pSrc->fRouteTargetOffset;
	if ( (pDst->sId == NULL) || (pDst->sFromNode == NULL) || (pDst->sFromPort == NULL) ||
	     (pDst->sToNode == NULL) || (pDst->sToPort == NULL) ) {
		__xuiFlowFreeEdgeDesc(pDst);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiFlowRouteStyleValid(int iRouteStyle)
{
	return iRouteStyle >= XUI_FLOW_ROUTE_AUTO && iRouteStyle <= XUI_FLOW_ROUTE_BEZIER;
}

static int __xuiFlowCopyValue(xvalue pSrc, xvalue* ppDst)
{
	if ( ppDst == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppDst = NULL;
	if ( pSrc == NULL ) {
		return XUI_OK;
	}
	*ppDst = xvoDeepCopy(pSrc);
	return (*ppDst != NULL) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static void __xuiFlowFreeDiagnostics(xui_flow_graph pGraph)
{
	uint32_t i;

	if ( pGraph == NULL ) {
		return;
	}
	for ( i = 1u; i <= pGraph->arrDiagnostics.Count; ++i ) {
		__xuiFlowFreeDiagnostic((xui_flow_diagnostic_model_t*)xrtArrayGet_Unsafe(&pGraph->arrDiagnostics, i));
	}
	xrtArrayUnit(&pGraph->arrDiagnostics);
}

static void __xuiFlowFreeCommandArray(xarray pArray)
{
	uint32_t i;

	if ( pArray == NULL ) {
		return;
	}
	for ( i = 1u; i <= pArray->Count; ++i ) {
		__xuiFlowFreeCommand((xui_flow_command_model_t*)xrtArrayGet_Unsafe(pArray, i));
	}
	xrtArrayUnit(pArray);
}

static int __xuiFlowPushCommand(xarray pArray, const xui_flow_command_model_t* pCommand)
{
	xui_flow_command_model_t* pDst;
	uint32_t iPos;

	if ( (pArray == NULL) || (pCommand == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPos = xrtArrayAppend(pArray, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst = (xui_flow_command_model_t*)xrtArrayGet_Unsafe(pArray, iPos);
	*pDst = *pCommand;
	return XUI_OK;
}

static void __xuiFlowTrimCommandArray(xarray pArray, int iLimit)
{
	xui_flow_command_model_t* pCommand;

	if ( (pArray == NULL) || (iLimit < 0) ) {
		return;
	}
	while ( (int)pArray->Count > iLimit ) {
		pCommand = (xui_flow_command_model_t*)xrtArrayGet_Unsafe(pArray, 1u);
		__xuiFlowFreeCommand(pCommand);
		xrtArrayRemove(pArray, 1u, 1u);
	}
}

static void __xuiFlowTrimHistory(xui_flow_graph pGraph)
{
	if ( pGraph == NULL ) {
		return;
	}
	__xuiFlowTrimCommandArray(&pGraph->arrUndo, pGraph->iCommandHistoryLimit);
	__xuiFlowTrimCommandArray(&pGraph->arrRedo, pGraph->iCommandHistoryLimit);
}

static int __xuiFlowPopCommand(xarray pArray, xui_flow_command_model_t* pCommand)
{
	xui_flow_command_model_t* pSrc;

	if ( (pArray == NULL) || (pCommand == NULL) || (pArray->Count == 0u) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSrc = (xui_flow_command_model_t*)xrtArrayGet_Unsafe(pArray, pArray->Count);
	*pCommand = *pSrc;
	memset(pSrc, 0, sizeof(*pSrc));
	xrtArrayRemove(pArray, pArray->Count, 1u);
	return XUI_OK;
}

static void __xuiFlowClearRedo(xui_flow_graph pGraph)
{
	if ( pGraph == NULL ) {
		return;
	}
	__xuiFlowFreeCommandArray(&pGraph->arrRedo);
	xrtArrayInit(&pGraph->arrRedo, sizeof(xui_flow_command_model_t), XRT_OBJMODE_LOCAL);
}

static void __xuiFlowInitCommand(xui_flow_command_model_t* pCommand)
{
	if ( pCommand == NULL ) {
		return;
	}
	memset(pCommand, 0, sizeof(*pCommand));
	xrtArrayInit(&pCommand->arrPorts, sizeof(xui_flow_port_desc_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pCommand->arrEdges, sizeof(xui_flow_edge_desc_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pCommand->arrMoves, sizeof(xui_flow_move_node_record_t), XRT_OBJMODE_LOCAL);
}

static int __xuiFlowDictFindIndex(xdict pDict, const char* sId)
{
	int* pValue;

	if ( (pDict == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return -1;
	}
	pValue = (int*)xrtDictGet(pDict, (ptr)(void*)sId, (uint32)strlen(sId));
	return (pValue != NULL) ? *pValue : -1;
}

static int __xuiFlowDictSetIndex(xdict pDict, const char* sId, int iIndex)
{
	int* pValue;
	bool bNew;

	if ( (pDict == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pValue = (int*)xrtDictSet(pDict, (ptr)(void*)sId, (uint32)strlen(sId), &bNew);
	if ( pValue == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*pValue = iIndex;
	return bNew ? XUI_OK : XUI_ERROR_ALREADY_INITIALIZED;
}

static void __xuiFlowFreeSelectionArray(xarray pArray)
{
	uint32_t i;
	char** ppId;

	if ( pArray == NULL ) {
		return;
	}
	for ( i = 1u; i <= pArray->Count; ++i ) {
		ppId = (char**)xrtArrayGet_Unsafe(pArray, i);
		xrtFree(*ppId);
		*ppId = NULL;
	}
	xrtArrayUnit(pArray);
}

static int __xuiFlowSelectionFind(xarray pArray, const char* sId)
{
	uint32_t i;
	char** ppId;

	if ( (pArray == NULL) || (sId == NULL) ) {
		return -1;
	}
	for ( i = 1u; i <= pArray->Count; ++i ) {
		ppId = (char**)xrtArrayGet_Unsafe(pArray, i);
		if ( (*ppId != NULL) && strcmp(*ppId, sId) == 0 ) {
			return (int)i - 1;
		}
	}
	return -1;
}

static int __xuiFlowSelectionSet(xarray pArray, const char* sId, int bSelected)
{
	char** ppId;
	uint32_t iPos;
	int iIndex;

	if ( (pArray == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiFlowSelectionFind(pArray, sId);
	if ( bSelected ) {
		if ( iIndex >= 0 ) {
			return XUI_OK;
		}
		iPos = xrtArrayAppend(pArray, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		ppId = (char**)xrtArrayGet_Unsafe(pArray, iPos);
		*ppId = __xuiFlowCopyString(sId);
		if ( *ppId == NULL ) {
			xrtArrayRemove(pArray, iPos, 1u);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		return XUI_OK;
	}
	if ( iIndex < 0 ) {
		return XUI_OK;
	}
	ppId = (char**)xrtArrayGet_Unsafe(pArray, (uint32_t)iIndex + 1u);
	xrtFree(*ppId);
	*ppId = NULL;
	xrtArrayRemove(pArray, (uint32_t)iIndex + 1u, 1u);
	return XUI_OK;
}

static int __xuiFlowRebuildNodeMap(xui_flow_graph pGraph)
{
	uint32_t i;
	int iRet;
	xui_flow_node_model_t* pNode;

	xrtDictUnit(&pGraph->mapNodes);
	xrtDictInit(&pGraph->mapNodes, sizeof(int), XRT_OBJMODE_LOCAL);
	for ( i = 1u; i <= pGraph->arrNodes.Count; ++i ) {
		pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, i);
		iRet = __xuiFlowDictSetIndex(&pGraph->mapNodes, pNode->sId, (int)i - 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiFlowRebuildEdgeMapAndRefs(xui_flow_graph pGraph)
{
	uint32_t i;
	int iRet;
	xui_flow_edge_model_t* pEdge;
	xui_flow_node_model_t* pFromNode;
	xui_flow_node_model_t* pToNode;

	xrtDictUnit(&pGraph->mapEdges);
	xrtDictInit(&pGraph->mapEdges, sizeof(int), XRT_OBJMODE_LOCAL);
	for ( i = 1u; i <= pGraph->arrEdges.Count; ++i ) {
		pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, i);
		pEdge->iFromNode = xuiFlowGraphFindNode(pGraph, pEdge->sFromNode);
		pEdge->iToNode = xuiFlowGraphFindNode(pGraph, pEdge->sToNode);
		pEdge->iFromPort = -1;
		pEdge->iToPort = -1;
		if ( pEdge->iFromNode >= 0 ) {
			pFromNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)pEdge->iFromNode + 1u);
			pEdge->iFromPort = __xuiFlowPortFind(pFromNode, pEdge->sFromPort);
		}
		if ( pEdge->iToNode >= 0 ) {
			pToNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)pEdge->iToNode + 1u);
			pEdge->iToPort = __xuiFlowPortFind(pToNode, pEdge->sToPort);
		}
		pEdge->bInvalid = (pEdge->iFromNode < 0 || pEdge->iToNode < 0 || pEdge->iFromPort < 0 || pEdge->iToPort < 0) ? 1 : 0;
		iRet = __xuiFlowDictSetIndex(&pGraph->mapEdges, pEdge->sId, (int)i - 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiFlowRemoveEdgeAt(xui_flow_graph pGraph, int iEdge)
{
	xui_flow_edge_model_t* pEdge;

	if ( (pGraph == NULL) || (iEdge < 0) || ((uint32_t)iEdge >= pGraph->arrEdges.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)iEdge + 1u);
	__xuiFlowSelectionSet(&pGraph->arrSelectedEdges, pEdge->sId, 0);
	__xuiFlowFreeEdge(pEdge);
	xrtArrayRemove(&pGraph->arrEdges, (uint32_t)iEdge + 1u, 1u);
	return __xuiFlowRebuildEdgeMapAndRefs(pGraph);
}

static int __xuiFlowCommandAppendPort(xui_flow_command_model_t* pCommand, const xui_flow_port_model_t* pPort)
{
	xui_flow_port_desc_t tPort;
	xui_flow_port_desc_t* pDst;
	uint32_t iPos;
	int iRet;

	if ( (pCommand == NULL) || (pPort == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPort, 0, sizeof(tPort));
	tPort.iSize = sizeof(tPort);
	tPort.sId = pPort->sId;
	tPort.sTitle = pPort->sTitle;
	tPort.sDataType = pPort->sDataType;
	tPort.iDirection = pPort->iDirection;
	tPort.iKind = pPort->iKind;
	tPort.bRequired = pPort->bRequired;
	tPort.bMulti = pPort->bMulti;
	tPort.bDynamic = pPort->bDynamic;
	iPos = xrtArrayAppend(&pCommand->arrPorts, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst = (xui_flow_port_desc_t*)xrtArrayGet_Unsafe(&pCommand->arrPorts, iPos);
	iRet = __xuiFlowCopyPortDesc(pDst, &tPort);
	if ( iRet != XUI_OK ) {
		xrtArrayRemove(&pCommand->arrPorts, iPos, 1u);
	}
	return iRet;
}

static int __xuiFlowCommandAppendEdge(xui_flow_command_model_t* pCommand, const xui_flow_edge_model_t* pEdge)
{
	xui_flow_edge_desc_t tEdge;
	xui_flow_edge_desc_t* pDst;
	uint32_t iPos;
	int iRet;

	if ( (pCommand == NULL) || (pEdge == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEdge, 0, sizeof(tEdge));
	tEdge.iSize = sizeof(tEdge);
	tEdge.sId = pEdge->sId;
	tEdge.iKind = pEdge->iKind;
	tEdge.sFromNode = pEdge->sFromNode;
	tEdge.sFromPort = pEdge->sFromPort;
	tEdge.sToNode = pEdge->sToNode;
	tEdge.sToPort = pEdge->sToPort;
	tEdge.iRouteStyle = pEdge->iRouteStyle;
	tEdge.fRouteBias = pEdge->fRouteBias;
	tEdge.fRouteSourceOffset = pEdge->fRouteSourceOffset;
	tEdge.fRouteTargetOffset = pEdge->fRouteTargetOffset;
	iPos = xrtArrayAppend(&pCommand->arrEdges, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst = (xui_flow_edge_desc_t*)xrtArrayGet_Unsafe(&pCommand->arrEdges, iPos);
	iRet = __xuiFlowCopyEdgeDesc(pDst, &tEdge);
	if ( iRet != XUI_OK ) {
		xrtArrayRemove(&pCommand->arrEdges, iPos, 1u);
	}
	return iRet;
}

static int __xuiFlowCommandAppendMove(xui_flow_command_model_t* pCommand, const xui_flow_move_node_record_t* pRecord)
{
	xui_flow_move_node_record_t* pDst;
	uint32_t iPos;

	if ( (pCommand == NULL) || (pRecord == NULL) || (pRecord->iSize < sizeof(*pRecord)) ||
	     (pRecord->sId == NULL) || (pRecord->sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pRecord->fOldX == pRecord->fNewX) && (pRecord->fOldY == pRecord->fNewY) ) {
		return XUI_OK;
	}
	iPos = xrtArrayAppend(&pCommand->arrMoves, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst = (xui_flow_move_node_record_t*)xrtArrayGet_Unsafe(&pCommand->arrMoves, iPos);
	memset(pDst, 0, sizeof(*pDst));
	pDst->iSize = sizeof(*pDst);
	pDst->sId = __xuiFlowCopyString(pRecord->sId);
	if ( pDst->sId == NULL ) {
		xrtArrayRemove(&pCommand->arrMoves, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDst->fOldX = pRecord->fOldX;
	pDst->fOldY = pRecord->fOldY;
	pDst->fNewX = pRecord->fNewX;
	pDst->fNewY = pRecord->fNewY;
	return XUI_OK;
}

static int __xuiFlowCommandSnapshotRemoveEdge(xui_flow_graph pGraph, const char* sId, xui_flow_command_model_t* pCommand)
{
	xui_flow_edge_model_t* pEdge;
	int iEdge;

	if ( (pGraph == NULL) || (sId == NULL) || (pCommand == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iEdge = xuiFlowGraphFindEdge(pGraph, sId);
	if ( iEdge < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)iEdge + 1u);
	pCommand->iType = XUI_FLOW_COMMAND_REMOVE_EDGE;
	return __xuiFlowCopyEdgeDesc(&pCommand->tEdge, &(xui_flow_edge_desc_t){
		sizeof(xui_flow_edge_desc_t), pEdge->sId, pEdge->iKind, pEdge->sFromNode, pEdge->sFromPort, pEdge->sToNode, pEdge->sToPort, pEdge->iRouteStyle, pEdge->fRouteBias, pEdge->fRouteSourceOffset, pEdge->fRouteTargetOffset
	});
}

static int __xuiFlowCommandSnapshotRemoveNode(xui_flow_graph pGraph, const char* sId, xui_flow_command_model_t* pCommand)
{
	xui_flow_node_model_t* pNode;
	xui_flow_edge_model_t* pEdge;
	uint32_t i;
	int iNode;
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (pCommand == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	pCommand->iType = XUI_FLOW_COMMAND_REMOVE_NODE;
	iRet = __xuiFlowCopyNodeDesc(&pCommand->tNode, &(xui_flow_node_desc_t){
		sizeof(xui_flow_node_desc_t), pNode->sId, pNode->sType, pNode->sTitle, pNode->fX, pNode->fY, pNode->fW, pNode->fH, pNode->sSummary
	});
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 1u; i <= pNode->arrPorts.Count; ++i ) {
		iRet = __xuiFlowCommandAppendPort(pCommand, (xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, i));
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	for ( i = 1u; i <= pGraph->arrEdges.Count; ++i ) {
		pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, i);
		if ( strcmp(pEdge->sFromNode, sId) == 0 || strcmp(pEdge->sToNode, sId) == 0 ) {
			iRet = __xuiFlowCommandAppendEdge(pCommand, pEdge);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiFlowCommandRestoreRemovedNode(xui_flow_graph pGraph, const xui_flow_command_model_t* pCommand)
{
	uint32_t i;
	int iNode;
	int iRet;

	if ( (pGraph == NULL) || (pCommand == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphAddNode(pGraph, &pCommand->tNode, &iNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 1u; i <= pCommand->arrPorts.Count; ++i ) {
		iRet = xuiFlowGraphAddPort(pGraph, iNode, (xui_flow_port_desc_t*)xrtArrayGet_Unsafe((xarray)&pCommand->arrPorts, i), NULL);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	for ( i = 1u; i <= pCommand->arrEdges.Count; ++i ) {
		iRet = xuiFlowGraphAddEdge(pGraph, (xui_flow_edge_desc_t*)xrtArrayGet_Unsafe((xarray)&pCommand->arrEdges, i), NULL);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiFlowPortFind(const xui_flow_node_model_t* pNode, const char* sId)
{
	if ( pNode == NULL ) {
		return -1;
	}
	return __xuiFlowDictFindIndex((xdict)(void*)&pNode->mapPorts, sId);
}

static int __xuiFlowPortDescValid(const xui_flow_port_desc_t* pDesc)
{
	if ( (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) || (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ) {
		return 0;
	}
	if ( (pDesc->iDirection != XUI_FLOW_PORT_INPUT) && (pDesc->iDirection != XUI_FLOW_PORT_OUTPUT) ) {
		return 0;
	}
	if ( (pDesc->iKind != XUI_FLOW_PORT_CONTROL) && (pDesc->iKind != XUI_FLOW_PORT_DATA) ) {
		return 0;
	}
	return 1;
}

XUI_API int xuiFlowGraphCreate(xui_flow_graph* ppGraph)
{
	xui_flow_graph pGraph;

	if ( ppGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppGraph = NULL;
	pGraph = (xui_flow_graph)xrtCalloc(1u, sizeof(*pGraph));
	if ( pGraph == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	xrtArrayInit(&pGraph->arrNodes, sizeof(xui_flow_node_model_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrEdges, sizeof(xui_flow_edge_model_t), XRT_OBJMODE_LOCAL);
	xrtDictInit(&pGraph->mapNodes, sizeof(int), XRT_OBJMODE_LOCAL);
	xrtDictInit(&pGraph->mapEdges, sizeof(int), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrSelectedNodes, sizeof(char*), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrSelectedEdges, sizeof(char*), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrDiagnostics, sizeof(xui_flow_diagnostic_model_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrUndo, sizeof(xui_flow_command_model_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrRedo, sizeof(xui_flow_command_model_t), XRT_OBJMODE_LOCAL);
	pGraph->tViewport.iSize = sizeof(pGraph->tViewport);
	pGraph->tViewport.fZoom = 1.0f;
	pGraph->iCommandHistoryLimit = XUI_FLOW_COMMAND_HISTORY_DEFAULT_LIMIT;
	*ppGraph = pGraph;
	return XUI_OK;
}

XUI_API void xuiFlowGraphDestroy(xui_flow_graph pGraph)
{
	uint32_t i;

	if ( pGraph == NULL ) {
		return;
	}
	for ( i = 1u; i <= pGraph->arrNodes.Count; ++i ) {
		__xuiFlowFreeNode((xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, i));
	}
	for ( i = 1u; i <= pGraph->arrEdges.Count; ++i ) {
		__xuiFlowFreeEdge((xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, i));
	}
	xrtArrayUnit(&pGraph->arrNodes);
	xrtArrayUnit(&pGraph->arrEdges);
	xrtDictUnit(&pGraph->mapNodes);
	xrtDictUnit(&pGraph->mapEdges);
	__xuiFlowFreeSelectionArray(&pGraph->arrSelectedNodes);
	__xuiFlowFreeSelectionArray(&pGraph->arrSelectedEdges);
	__xuiFlowFreeDiagnostics(pGraph);
	__xuiFlowFreeCommandArray(&pGraph->arrUndo);
	__xuiFlowFreeCommandArray(&pGraph->arrRedo);
	xrtFree(pGraph);
}

XUI_API int xuiFlowGraphAddNode(xui_flow_graph pGraph, const xui_flow_node_desc_t* pDesc, int* pIndex)
{
	xui_flow_node_model_t* pNode;
	uint32_t iPos;
	int iIndex;
	int iRet;

	if ( (pGraph == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) || (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiFlowGraphFindNode(pGraph, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iPos = xrtArrayAppend(&pGraph->arrNodes, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, iPos);
	memset(pNode, 0, sizeof(*pNode));
	xrtArrayInit(&pNode->arrPorts, sizeof(xui_flow_port_model_t), XRT_OBJMODE_LOCAL);
	xrtDictInit(&pNode->mapPorts, sizeof(int), XRT_OBJMODE_LOCAL);
	pNode->sId = __xuiFlowCopyString(pDesc->sId);
	pNode->sType = __xuiFlowCopyString(pDesc->sType);
	pNode->sTitle = __xuiFlowCopyString(pDesc->sTitle);
	pNode->sSummary = __xuiFlowCopyString(pDesc->sSummary);
	pNode->fX = pDesc->fX;
	pNode->fY = pDesc->fY;
	pNode->fW = pDesc->fW;
	pNode->fH = pDesc->fH;
	if ( pNode->sId == NULL ||
	     (pDesc->sType != NULL && pNode->sType == NULL) ||
	     (pDesc->sTitle != NULL && pNode->sTitle == NULL) ||
	     (pDesc->sSummary != NULL && pNode->sSummary == NULL) ) {
		__xuiFlowFreeNode(pNode);
		xrtArrayRemove(&pGraph->arrNodes, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFlowDictSetIndex(&pGraph->mapNodes, pNode->sId, iIndex);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeNode(pNode);
		xrtArrayRemove(&pGraph->arrNodes, iPos, 1u);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphRemoveNode(xui_flow_graph pGraph, const char* sId)
{
	xui_flow_node_model_t* pNode;
	xui_flow_edge_model_t* pEdge;
	int iNode;
	int i;
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = (int)pGraph->arrEdges.Count - 1; i >= 0; --i ) {
		pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)i + 1u);
		if ( strcmp(pEdge->sFromNode, sId) == 0 || strcmp(pEdge->sToNode, sId) == 0 ) {
			iRet = __xuiFlowRemoveEdgeAt(pGraph, i);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	__xuiFlowSelectionSet(&pGraph->arrSelectedNodes, pNode->sId, 0);
	__xuiFlowFreeNode(pNode);
	xrtArrayRemove(&pGraph->arrNodes, (uint32_t)iNode + 1u, 1u);
	iRet = __xuiFlowRebuildNodeMap(pGraph);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiFlowRebuildEdgeMapAndRefs(pGraph);
	if ( iRet == XUI_OK ) {
		__xuiFlowBumpRevision(pGraph);
	}
	return iRet;
}

XUI_API int xuiFlowGraphRemoveEdge(xui_flow_graph pGraph, const char* sId)
{
	int iEdge;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iEdge = xuiFlowGraphFindEdge(pGraph, sId);
	if ( iEdge < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iEdge = __xuiFlowRemoveEdgeAt(pGraph, iEdge);
	if ( iEdge == XUI_OK ) {
		__xuiFlowBumpRevision(pGraph);
	}
	return iEdge;
}

XUI_API int xuiFlowGraphAddPort(xui_flow_graph pGraph, int iNode, const xui_flow_port_desc_t* pDesc, int* pIndex)
{
	xui_flow_node_model_t* pNode;
	xui_flow_port_model_t* pPort;
	uint32_t iPos;
	int iIndex;
	int iRet;

	if ( (pGraph == NULL) || !__xuiFlowPortDescValid(pDesc) || (iNode < 0) || ((uint32_t)iNode >= pGraph->arrNodes.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	if ( __xuiFlowPortFind(pNode, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iPos = xrtArrayAppend(&pNode->arrPorts, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pPort = (xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, iPos);
	memset(pPort, 0, sizeof(*pPort));
	pPort->sId = __xuiFlowCopyString(pDesc->sId);
	pPort->sTitle = __xuiFlowCopyString(pDesc->sTitle);
	pPort->sDataType = __xuiFlowCopyString(pDesc->sDataType);
	pPort->iDirection = pDesc->iDirection;
	pPort->iKind = pDesc->iKind;
	pPort->bRequired = pDesc->bRequired;
	pPort->bMulti = pDesc->bMulti;
	pPort->bDynamic = pDesc->bDynamic;
	if ( pPort->sId == NULL || (pDesc->sTitle != NULL && pPort->sTitle == NULL) || (pDesc->sDataType != NULL && pPort->sDataType == NULL) ) {
		__xuiFlowFreePort(pPort);
		xrtArrayRemove(&pNode->arrPorts, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFlowDictSetIndex(&pNode->mapPorts, pPort->sId, iIndex);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreePort(pPort);
		xrtArrayRemove(&pNode->arrPorts, iPos, 1u);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

static int __xuiFlowRebuildPortMap(xui_flow_node_model_t* pNode)
{
	xui_flow_port_model_t* pPort;
	uint32_t i;
	int iRet;

	if ( pNode == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	xrtDictUnit(&pNode->mapPorts);
	xrtDictInit(&pNode->mapPorts, sizeof(int), XRT_OBJMODE_LOCAL);
	for ( i = 1u; i <= pNode->arrPorts.Count; ++i ) {
		pPort = (xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, i);
		iRet = __xuiFlowDictSetIndex(&pNode->mapPorts, pPort->sId, (int)i - 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiFlowGraphRebuildNodeDynamicPorts(xui_flow_graph pGraph, const char* sNodeId, const xui_flow_port_desc_t* pPorts, int iPortCount)
{
	xui_flow_node_model_t* pNode;
	xui_flow_port_model_t* pPort;
	xui_flow_port_desc_t tPort;
	int iNode;
	int i;
	int iRet;

	if ( (pGraph == NULL) || (sNodeId == NULL) || (sNodeId[0] == 0) || (iPortCount < 0) || (iPortCount > 0 && pPorts == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sNodeId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	for ( i = (int)pNode->arrPorts.Count - 1; i >= 0; --i ) {
		pPort = (xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, (uint32_t)i + 1u);
		if ( pPort->bDynamic ) {
			__xuiFlowFreePort(pPort);
			xrtArrayRemove(&pNode->arrPorts, (uint32_t)i + 1u, 1u);
		}
	}
	iRet = __xuiFlowRebuildPortMap(pNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 0; i < iPortCount; ++i ) {
		tPort = pPorts[i];
		tPort.iSize = sizeof(tPort);
		tPort.bDynamic = 1;
		iRet = xuiFlowGraphAddPort(pGraph, iNode, &tPort, NULL);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iRet = __xuiFlowRebuildEdgeMapAndRefs(pGraph);
	if ( iRet == XUI_OK ) {
		__xuiFlowBumpRevision(pGraph);
	}
	return iRet;
}

XUI_API int xuiFlowGraphAddEdge(xui_flow_graph pGraph, const xui_flow_edge_desc_t* pDesc, int* pIndex)
{
	xui_flow_edge_model_t* pEdge;
	xui_flow_node_model_t* pFromNode;
	xui_flow_node_model_t* pToNode;
	uint32_t iPos;
	int iIndex;
	int iFromNode;
	int iToNode;
	int iFromPort;
	int iToPort;
	int iRet;

	if ( (pGraph == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ||
	     (pDesc->sFromNode == NULL) || (pDesc->sFromPort == NULL) ||
	     (pDesc->sToNode == NULL) || (pDesc->sToPort == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pDesc->iKind != XUI_FLOW_PORT_CONTROL) && (pDesc->iKind != XUI_FLOW_PORT_DATA) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiFlowRouteStyleValid(pDesc->iRouteStyle) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiFlowGraphFindEdge(pGraph, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iFromNode = xuiFlowGraphFindNode(pGraph, pDesc->sFromNode);
	iToNode = xuiFlowGraphFindNode(pGraph, pDesc->sToNode);
	if ( (iFromNode < 0) || (iToNode < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pFromNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iFromNode + 1u);
	pToNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iToNode + 1u);
	iFromPort = __xuiFlowPortFind(pFromNode, pDesc->sFromPort);
	iToPort = __xuiFlowPortFind(pToNode, pDesc->sToPort);
	if ( (iFromPort < 0) || (iToPort < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPos = xrtArrayAppend(&pGraph->arrEdges, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, iPos);
	memset(pEdge, 0, sizeof(*pEdge));
	pEdge->sId = __xuiFlowCopyString(pDesc->sId);
	pEdge->sFromNode = __xuiFlowCopyString(pDesc->sFromNode);
	pEdge->sFromPort = __xuiFlowCopyString(pDesc->sFromPort);
	pEdge->sToNode = __xuiFlowCopyString(pDesc->sToNode);
	pEdge->sToPort = __xuiFlowCopyString(pDesc->sToPort);
	pEdge->iKind = pDesc->iKind;
	pEdge->iRouteStyle = pDesc->iRouteStyle;
	pEdge->fRouteBias = pDesc->fRouteBias;
	pEdge->fRouteSourceOffset = pDesc->fRouteSourceOffset;
	pEdge->fRouteTargetOffset = pDesc->fRouteTargetOffset;
	pEdge->iFromNode = iFromNode;
	pEdge->iFromPort = iFromPort;
	pEdge->iToNode = iToNode;
	pEdge->iToPort = iToPort;
	if ( pEdge->sId == NULL || pEdge->sFromNode == NULL || pEdge->sFromPort == NULL || pEdge->sToNode == NULL || pEdge->sToPort == NULL ) {
		__xuiFlowFreeEdge(pEdge);
		xrtArrayRemove(&pGraph->arrEdges, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFlowDictSetIndex(&pGraph->mapEdges, pEdge->sId, iIndex);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeEdge(pEdge);
		xrtArrayRemove(&pGraph->arrEdges, iPos, 1u);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphAddEdgePreserveInvalid(xui_flow_graph pGraph, const xui_flow_edge_desc_t* pDesc, int* pIndex)
{
	xui_flow_edge_model_t* pEdge;
	uint32_t iPos;
	int iIndex;
	int iRet;

	if ( (pGraph == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ||
	     (pDesc->sFromNode == NULL) || (pDesc->sFromPort == NULL) ||
	     (pDesc->sToNode == NULL) || (pDesc->sToPort == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pDesc->iKind != XUI_FLOW_PORT_CONTROL) && (pDesc->iKind != XUI_FLOW_PORT_DATA) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiFlowRouteStyleValid(pDesc->iRouteStyle) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiFlowGraphFindEdge(pGraph, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iPos = xrtArrayAppend(&pGraph->arrEdges, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, iPos);
	memset(pEdge, 0, sizeof(*pEdge));
	pEdge->sId = __xuiFlowCopyString(pDesc->sId);
	pEdge->sFromNode = __xuiFlowCopyString(pDesc->sFromNode);
	pEdge->sFromPort = __xuiFlowCopyString(pDesc->sFromPort);
	pEdge->sToNode = __xuiFlowCopyString(pDesc->sToNode);
	pEdge->sToPort = __xuiFlowCopyString(pDesc->sToPort);
	pEdge->iKind = pDesc->iKind;
	pEdge->iRouteStyle = pDesc->iRouteStyle;
	pEdge->fRouteBias = pDesc->fRouteBias;
	pEdge->fRouteSourceOffset = pDesc->fRouteSourceOffset;
	pEdge->fRouteTargetOffset = pDesc->fRouteTargetOffset;
	if ( pEdge->sId == NULL || pEdge->sFromNode == NULL || pEdge->sFromPort == NULL || pEdge->sToNode == NULL || pEdge->sToPort == NULL ) {
		__xuiFlowFreeEdge(pEdge);
		xrtArrayRemove(&pGraph->arrEdges, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFlowRebuildEdgeMapAndRefs(pGraph);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeEdge(pEdge);
		xrtArrayRemove(&pGraph->arrEdges, iPos, 1u);
		(void)__xuiFlowRebuildEdgeMapAndRefs(pGraph);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphFindNode(xui_flow_graph pGraph, const char* sId)
{
	if ( pGraph == NULL ) {
		return -1;
	}
	return __xuiFlowDictFindIndex(&pGraph->mapNodes, sId);
}

XUI_API int xuiFlowGraphFindEdge(xui_flow_graph pGraph, const char* sId)
{
	if ( pGraph == NULL ) {
		return -1;
	}
	return __xuiFlowDictFindIndex(&pGraph->mapEdges, sId);
}

XUI_API int xuiFlowGraphGetNodeCount(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? (int)pGraph->arrNodes.Count : 0;
}

XUI_API int xuiFlowGraphGetEdgeCount(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? (int)pGraph->arrEdges.Count : 0;
}

XUI_API int xuiFlowGraphGetNodePortCount(xui_flow_graph pGraph, int iNode)
{
	xui_flow_node_model_t* pNode;

	if ( (pGraph == NULL) || (iNode < 0) || ((uint32_t)iNode >= pGraph->arrNodes.Count) ) {
		return 0;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	return (int)pNode->arrPorts.Count;
}

XUI_API uint32_t xuiFlowGraphGetRevision(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? pGraph->iRevision : 0u;
}

XUI_API int xuiFlowGraphGetNode(xui_flow_graph pGraph, int iNode, xui_flow_node_info_t* pInfo)
{
	xui_flow_node_model_t* pNode;

	if ( (pGraph == NULL) || (pInfo == NULL) || (iNode < 0) || ((uint32_t)iNode >= pGraph->arrNodes.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->sId = pNode->sId;
	pInfo->sType = pNode->sType;
	pInfo->sTitle = pNode->sTitle;
	pInfo->sSummary = pNode->sSummary;
	pInfo->fX = pNode->fX;
	pInfo->fY = pNode->fY;
	pInfo->fW = pNode->fW;
	pInfo->fH = pNode->fH;
	pInfo->iRunState = pNode->iRunState;
	pInfo->sRunPreview = pNode->sRunPreview;
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetPort(xui_flow_graph pGraph, int iNode, int iPort, xui_flow_port_info_t* pInfo)
{
	xui_flow_node_model_t* pNode;
	xui_flow_port_model_t* pPort;

	if ( (pGraph == NULL) || (pInfo == NULL) || (iNode < 0) || ((uint32_t)iNode >= pGraph->arrNodes.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	if ( (iPort < 0) || ((uint32_t)iPort >= pNode->arrPorts.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPort = (xui_flow_port_model_t*)xrtArrayGet_Unsafe(&pNode->arrPorts, (uint32_t)iPort + 1u);
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->sId = pPort->sId;
	pInfo->sTitle = pPort->sTitle;
	pInfo->sDataType = pPort->sDataType;
	pInfo->iDirection = pPort->iDirection;
	pInfo->iKind = pPort->iKind;
	pInfo->bRequired = pPort->bRequired;
	pInfo->bMulti = pPort->bMulti;
	pInfo->bDynamic = pPort->bDynamic;
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetEdge(xui_flow_graph pGraph, int iEdge, xui_flow_edge_info_t* pInfo)
{
	xui_flow_edge_model_t* pEdge;

	if ( (pGraph == NULL) || (pInfo == NULL) || (iEdge < 0) || ((uint32_t)iEdge >= pGraph->arrEdges.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)iEdge + 1u);
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->sId = pEdge->sId;
	pInfo->iKind = pEdge->iKind;
	pInfo->sFromNode = pEdge->sFromNode;
	pInfo->sFromPort = pEdge->sFromPort;
	pInfo->sToNode = pEdge->sToNode;
	pInfo->sToPort = pEdge->sToPort;
	pInfo->iFromNode = pEdge->iFromNode;
	pInfo->iFromPort = pEdge->iFromPort;
	pInfo->iToNode = pEdge->iToNode;
	pInfo->iToPort = pEdge->iToPort;
	pInfo->bInvalid = pEdge->bInvalid;
	pInfo->iRunState = pEdge->iRunState;
	pInfo->sRunPreview = pEdge->sRunPreview;
	pInfo->iRouteStyle = pEdge->iRouteStyle;
	pInfo->fRouteBias = pEdge->fRouteBias;
	pInfo->fRouteSourceOffset = pEdge->fRouteSourceOffset;
	pInfo->fRouteTargetOffset = pEdge->fRouteTargetOffset;
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetNodePosition(xui_flow_graph pGraph, const char* sId, float fX, float fY)
{
	xui_flow_node_model_t* pNode;
	int iNode;

	if ( (pGraph == NULL) || (sId == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	if ( (pNode->fX == fX) && (pNode->fY == fY) ) {
		return XUI_OK;
	}
	pNode->fX = fX;
	pNode->fY = fY;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetNodeBounds(xui_flow_graph pGraph, const char* sId, float fX, float fY, float fW, float fH)
{
	xui_flow_node_model_t* pNode;
	int iNode;

	if ( (pGraph == NULL) || (sId == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fW < 24.0f ) fW = 24.0f;
	if ( fH < 24.0f ) fH = 24.0f;
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	pNode->fX = fX;
	pNode->fY = fY;
	pNode->fW = fW;
	pNode->fH = fH;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetNodeSummary(xui_flow_graph pGraph, const char* sId, const char* sSummary)
{
	xui_flow_node_model_t* pNode;
	char* sCopy;
	int iNode;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xuiFlowCopyString(sSummary);
	if ( sSummary != NULL && sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	xrtFree(pNode->sSummary);
	pNode->sSummary = sCopy;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

static int __xuiFlowRunStateValid(int iState, int bEdge)
{
	if ( bEdge ) {
		return iState >= XUI_WORKFLOW_EDGE_RUN_IDLE && iState <= XUI_WORKFLOW_EDGE_RUN_INVALID;
	}
	return iState >= XUI_WORKFLOW_NODE_RUN_IDLE && iState <= XUI_WORKFLOW_NODE_RUN_WARNING;
}

XUI_API int xuiFlowGraphSetNodeRunState(xui_flow_graph pGraph, const char* sId, int iState, const char* sPreview)
{
	xui_flow_node_model_t* pNode;
	char* sCopy;
	int iNode;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) || !__xuiFlowRunStateValid(iState, 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xuiFlowCopyString(sPreview);
	if ( sPreview != NULL && sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	xrtFree(pNode->sRunPreview);
	pNode->sRunPreview = sCopy;
	pNode->iRunState = iState;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetEdgeRunState(xui_flow_graph pGraph, const char* sId, int iState, const char* sPreview)
{
	xui_flow_edge_model_t* pEdge;
	char* sCopy;
	int iEdge;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) || !__xuiFlowRunStateValid(iState, 1) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iEdge = xuiFlowGraphFindEdge(pGraph, sId);
	if ( iEdge < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xuiFlowCopyString(sPreview);
	if ( sPreview != NULL && sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)iEdge + 1u);
	xrtFree(pEdge->sRunPreview);
	pEdge->sRunPreview = sCopy;
	pEdge->iRunState = iState;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetEdgeRoute(xui_flow_graph pGraph, const char* sId, int iRouteStyle, float fRouteBias, float fSourceOffset, float fTargetOffset)
{
	xui_flow_edge_model_t* pEdge;
	int iEdge;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) || !__xuiFlowRouteStyleValid(iRouteStyle) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iEdge = xuiFlowGraphFindEdge(pGraph, sId);
	if ( iEdge < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pEdge = (xui_flow_edge_model_t*)xrtArrayGet_Unsafe(&pGraph->arrEdges, (uint32_t)iEdge + 1u);
	if ( pEdge->iRouteStyle == iRouteStyle && pEdge->fRouteBias == fRouteBias &&
	     pEdge->fRouteSourceOffset == fSourceOffset && pEdge->fRouteTargetOffset == fTargetOffset ) {
		return XUI_OK;
	}
	pEdge->iRouteStyle = iRouteStyle;
	pEdge->fRouteBias = fRouteBias;
	pEdge->fRouteSourceOffset = fSourceOffset;
	pEdge->fRouteTargetOffset = fTargetOffset;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetNodeConfig(xui_flow_graph pGraph, const char* sId, xvalue pConfig)
{
	xui_flow_node_model_t* pNode;
	xvalue pCopy;
	int iNode;
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFlowCopyValue(pConfig, &pCopy);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	if ( pNode->pConfig != NULL ) {
		xvoUnref(pNode->pConfig);
	}
	pNode->pConfig = pCopy;
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetNodeConfig(xui_flow_graph pGraph, const char* sId, xvalue* ppConfig)
{
	xui_flow_node_model_t* pNode;
	int iNode;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) || (ppConfig == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppConfig = NULL;
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	if ( pNode->pConfig != NULL ) {
		xvoAddRef(pNode->pConfig);
	}
	*ppConfig = pNode->pConfig;
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandAddNode(xui_flow_graph pGraph, const xui_flow_node_desc_t* pDesc, int* pIndex)
{
	xui_flow_command_model_t tCommand;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowInitCommand(&tCommand);
	tCommand.iType = XUI_FLOW_COMMAND_ADD_NODE;
	iRet = __xuiFlowCopyNodeDesc(&tCommand.tNode, pDesc);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = xuiFlowGraphAddNode(pGraph, pDesc, pIndex);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphRemoveNode(pGraph, pDesc->sId);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandRemoveNode(xui_flow_graph pGraph, const char* sId)
{
	xui_flow_command_model_t tCommand;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowInitCommand(&tCommand);
	iRet = __xuiFlowCommandSnapshotRemoveNode(pGraph, sId, &tCommand);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = xuiFlowGraphRemoveNode(pGraph, sId);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)__xuiFlowCommandRestoreRemovedNode(pGraph, &tCommand);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandMoveNode(xui_flow_graph pGraph, const xui_flow_move_node_desc_t* pDesc)
{
	xui_flow_command_model_t tCommand;
	xui_flow_node_info_t tNode;
	int iNode;
	int iRet;

	if ( (pGraph == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) || (pDesc->sId == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, pDesc->sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tNode, 0, sizeof(tNode));
	tNode.iSize = sizeof(tNode);
	iRet = xuiFlowGraphGetNode(pGraph, iNode, &tNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (tNode.fX == pDesc->fX) && (tNode.fY == pDesc->fY) ) {
		return XUI_OK;
	}
	__xuiFlowInitCommand(&tCommand);
	tCommand.iType = XUI_FLOW_COMMAND_MOVE_NODE;
	tCommand.sNodeId = __xuiFlowCopyString(pDesc->sId);
	if ( tCommand.sNodeId == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	tCommand.fOldX = tNode.fX;
	tCommand.fOldY = tNode.fY;
	tCommand.fNewX = pDesc->fX;
	tCommand.fNewY = pDesc->fY;
	iRet = xuiFlowGraphSetNodePosition(pGraph, pDesc->sId, pDesc->fX, pDesc->fY);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphSetNodePosition(pGraph, pDesc->sId, tCommand.fOldX, tCommand.fOldY);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandRecordMoveNode(xui_flow_graph pGraph, const char* sId, float fOldX, float fOldY, float fNewX, float fNewY)
{
	xui_flow_move_node_record_t tRecord;

	memset(&tRecord, 0, sizeof(tRecord));
	tRecord.iSize = sizeof(tRecord);
	tRecord.sId = sId;
	tRecord.fOldX = fOldX;
	tRecord.fOldY = fOldY;
	tRecord.fNewX = fNewX;
	tRecord.fNewY = fNewY;
	return xuiFlowGraphCommandRecordMoveNodes(pGraph, &tRecord, 1);
}

XUI_API int xuiFlowGraphCommandRecordMoveNodes(xui_flow_graph pGraph, const xui_flow_move_node_record_t* pRecords, int iRecordCount)
{
	xui_flow_command_model_t tCommand;
	int i;
	int iRet;

	if ( (pGraph == NULL) || (pRecords == NULL) || (iRecordCount <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowInitCommand(&tCommand);
	tCommand.iType = XUI_FLOW_COMMAND_MOVE_NODE;
	for ( i = 0; i < iRecordCount; ++i ) {
		if ( (pRecords[i].iSize < sizeof(pRecords[i])) || (pRecords[i].sId == NULL) || (xuiFlowGraphFindNode(pGraph, pRecords[i].sId) < 0) ) {
			__xuiFlowFreeCommand(&tCommand);
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iRet = __xuiFlowCommandAppendMove(&tCommand, &pRecords[i]);
		if ( iRet != XUI_OK ) {
			__xuiFlowFreeCommand(&tCommand);
			return iRet;
		}
	}
	if ( tCommand.arrMoves.Count == 0u ) {
		__xuiFlowFreeCommand(&tCommand);
		return XUI_OK;
	}
	if ( __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand) != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandSetNodeConfig(xui_flow_graph pGraph, const char* sId, xvalue pConfig)
{
	xui_flow_command_model_t tCommand;
	xui_flow_node_model_t* pNode;
	int iNode;
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	__xuiFlowInitCommand(&tCommand);
	tCommand.iType = XUI_FLOW_COMMAND_SET_NODE_CONFIG;
	tCommand.sNodeId = __xuiFlowCopyString(sId);
	if ( tCommand.sNodeId == NULL ) {
		__xuiFlowFreeCommand(&tCommand);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiFlowCopyValue(pNode->pConfig, &tCommand.pOldConfig);
	if ( iRet == XUI_OK ) {
		iRet = __xuiFlowCopyValue(pConfig, &tCommand.pNewConfig);
	}
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = xuiFlowGraphSetNodeConfig(pGraph, sId, pConfig);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphSetNodeConfig(pGraph, sId, tCommand.pOldConfig);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandAddEdge(xui_flow_graph pGraph, const xui_flow_edge_desc_t* pDesc, int* pIndex)
{
	xui_flow_command_model_t tCommand;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowInitCommand(&tCommand);
	tCommand.iType = XUI_FLOW_COMMAND_ADD_EDGE;
	iRet = __xuiFlowCopyEdgeDesc(&tCommand.tEdge, pDesc);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = xuiFlowGraphAddEdge(pGraph, pDesc, pIndex);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphRemoveEdge(pGraph, pDesc->sId);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCommandRemoveEdge(xui_flow_graph pGraph, const char* sId)
{
	xui_flow_command_model_t tCommand;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowInitCommand(&tCommand);
	iRet = __xuiFlowCommandSnapshotRemoveEdge(pGraph, sId, &tCommand);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = xuiFlowGraphRemoveEdge(pGraph, sId);
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphAddEdge(pGraph, &tCommand.tEdge, NULL);
		__xuiFlowFreeCommand(&tCommand);
		return iRet;
	}
	__xuiFlowTrimHistory(pGraph);
	memset(&tCommand, 0, sizeof(tCommand));
	__xuiFlowClearRedo(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphCanUndo(xui_flow_graph pGraph)
{
	return (pGraph != NULL && pGraph->arrUndo.Count > 0u) ? 1 : 0;
}

XUI_API int xuiFlowGraphCanRedo(xui_flow_graph pGraph)
{
	return (pGraph != NULL && pGraph->arrRedo.Count > 0u) ? 1 : 0;
}

XUI_API int xuiFlowGraphUndo(xui_flow_graph pGraph)
{
	xui_flow_command_model_t tCommand;
	xui_flow_move_node_record_t* pMove;
	uint32_t i;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tCommand, 0, sizeof(tCommand));
	iRet = __xuiFlowPopCommand(&pGraph->arrUndo, &tCommand);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( tCommand.iType == XUI_FLOW_COMMAND_ADD_NODE ) {
		iRet = xuiFlowGraphRemoveNode(pGraph, tCommand.tNode.sId);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_REMOVE_NODE ) {
		iRet = __xuiFlowCommandRestoreRemovedNode(pGraph, &tCommand);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_MOVE_NODE ) {
		iRet = XUI_OK;
		if ( tCommand.arrMoves.Count > 0u ) {
			for ( i = 1u; i <= tCommand.arrMoves.Count; ++i ) {
				pMove = (xui_flow_move_node_record_t*)xrtArrayGet_Unsafe(&tCommand.arrMoves, i);
				iRet = xuiFlowGraphSetNodePosition(pGraph, pMove->sId, pMove->fOldX, pMove->fOldY);
				if ( iRet != XUI_OK ) break;
			}
		} else {
			iRet = xuiFlowGraphSetNodePosition(pGraph, tCommand.sNodeId, tCommand.fOldX, tCommand.fOldY);
		}
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_ADD_EDGE ) {
		iRet = xuiFlowGraphRemoveEdge(pGraph, tCommand.tEdge.sId);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_REMOVE_EDGE ) {
		iRet = xuiFlowGraphAddEdge(pGraph, &tCommand.tEdge, NULL);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_SET_NODE_CONFIG ) {
		iRet = xuiFlowGraphSetNodeConfig(pGraph, tCommand.sNodeId, tCommand.pOldConfig);
	} else {
		iRet = XUI_ERROR_UNSUPPORTED;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiFlowPushCommand(&pGraph->arrRedo, &tCommand);
	}
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
	} else {
		__xuiFlowTrimHistory(pGraph);
		memset(&tCommand, 0, sizeof(tCommand));
	}
	return iRet;
}

XUI_API int xuiFlowGraphRedo(xui_flow_graph pGraph)
{
	xui_flow_command_model_t tCommand;
	xui_flow_move_node_record_t* pMove;
	uint32_t i;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tCommand, 0, sizeof(tCommand));
	iRet = __xuiFlowPopCommand(&pGraph->arrRedo, &tCommand);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( tCommand.iType == XUI_FLOW_COMMAND_ADD_NODE ) {
		iRet = xuiFlowGraphAddNode(pGraph, &tCommand.tNode, NULL);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_REMOVE_NODE ) {
		iRet = xuiFlowGraphRemoveNode(pGraph, tCommand.tNode.sId);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_MOVE_NODE ) {
		iRet = XUI_OK;
		if ( tCommand.arrMoves.Count > 0u ) {
			for ( i = 1u; i <= tCommand.arrMoves.Count; ++i ) {
				pMove = (xui_flow_move_node_record_t*)xrtArrayGet_Unsafe(&tCommand.arrMoves, i);
				iRet = xuiFlowGraphSetNodePosition(pGraph, pMove->sId, pMove->fNewX, pMove->fNewY);
				if ( iRet != XUI_OK ) break;
			}
		} else {
			iRet = xuiFlowGraphSetNodePosition(pGraph, tCommand.sNodeId, tCommand.fNewX, tCommand.fNewY);
		}
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_ADD_EDGE ) {
		iRet = xuiFlowGraphAddEdge(pGraph, &tCommand.tEdge, NULL);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_REMOVE_EDGE ) {
		iRet = xuiFlowGraphRemoveEdge(pGraph, tCommand.tEdge.sId);
	} else if ( tCommand.iType == XUI_FLOW_COMMAND_SET_NODE_CONFIG ) {
		iRet = xuiFlowGraphSetNodeConfig(pGraph, tCommand.sNodeId, tCommand.pNewConfig);
	} else {
		iRet = XUI_ERROR_UNSUPPORTED;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiFlowPushCommand(&pGraph->arrUndo, &tCommand);
	}
	if ( iRet != XUI_OK ) {
		__xuiFlowFreeCommand(&tCommand);
	} else {
		__xuiFlowTrimHistory(pGraph);
		memset(&tCommand, 0, sizeof(tCommand));
	}
	return iRet;
}

XUI_API int xuiFlowGraphSetCommandHistoryLimit(xui_flow_graph pGraph, int iLimit)
{
	if ( (pGraph == NULL) || (iLimit < 1) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pGraph->iCommandHistoryLimit = iLimit;
	__xuiFlowTrimHistory(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetCommandHistoryLimit(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? pGraph->iCommandHistoryLimit : 0;
}

XUI_API int xuiFlowGraphMarkClean(xui_flow_graph pGraph)
{
	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pGraph->iCleanRevision = pGraph->iRevision;
	return XUI_OK;
}

XUI_API int xuiFlowGraphIsDirty(xui_flow_graph pGraph)
{
	return (pGraph != NULL && pGraph->iRevision != pGraph->iCleanRevision) ? 1 : 0;
}

XUI_API uint32_t xuiFlowGraphGetCleanRevision(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? pGraph->iCleanRevision : 0u;
}

XUI_API int xuiFlowGraphMakeNodeId(xui_flow_graph pGraph, char* sBuffer, int iCapacity)
{
	if ( (pGraph == NULL) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	do {
		snprintf(sBuffer, (size_t)iCapacity, "node_%u", ++pGraph->iNextNodeId);
	} while ( xuiFlowGraphFindNode(pGraph, sBuffer) >= 0 );
	return XUI_OK;
}

XUI_API int xuiFlowGraphMakeEdgeId(xui_flow_graph pGraph, char* sBuffer, int iCapacity)
{
	if ( (pGraph == NULL) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	do {
		snprintf(sBuffer, (size_t)iCapacity, "edge_%u", ++pGraph->iNextEdgeId);
	} while ( xuiFlowGraphFindEdge(pGraph, sBuffer) >= 0 );
	return XUI_OK;
}

XUI_API int xuiFlowGraphMakePortId(xui_flow_graph pGraph, int iNode, char* sBuffer, int iCapacity)
{
	xui_flow_node_model_t* pNode;
	uint32_t iNext;

	if ( (pGraph == NULL) || (sBuffer == NULL) || (iCapacity <= 0) || (iNode < 0) || ((uint32_t)iNode >= pGraph->arrNodes.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNode = (xui_flow_node_model_t*)xrtArrayGet_Unsafe(&pGraph->arrNodes, (uint32_t)iNode + 1u);
	iNext = pNode->arrPorts.Count + 1u;
	do {
		snprintf(sBuffer, (size_t)iCapacity, "port_%u", iNext++);
	} while ( __xuiFlowPortFind(pNode, sBuffer) >= 0 );
	return XUI_OK;
}

XUI_API int xuiFlowGraphClearSelection(xui_flow_graph pGraph)
{
	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowFreeSelectionArray(&pGraph->arrSelectedNodes);
	__xuiFlowFreeSelectionArray(&pGraph->arrSelectedEdges);
	xrtArrayInit(&pGraph->arrSelectedNodes, sizeof(char*), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pGraph->arrSelectedEdges, sizeof(char*), XRT_OBJMODE_LOCAL);
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphSelectNode(xui_flow_graph pGraph, const char* sId, int bSelected)
{
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (xuiFlowGraphFindNode(pGraph, sId) < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFlowSelectionSet(&pGraph->arrSelectedNodes, sId, bSelected);
	if ( iRet == XUI_OK ) {
		__xuiFlowBumpRevision(pGraph);
	}
	return iRet;
}

XUI_API int xuiFlowGraphSelectEdge(xui_flow_graph pGraph, const char* sId, int bSelected)
{
	int iRet;

	if ( (pGraph == NULL) || (sId == NULL) || (xuiFlowGraphFindEdge(pGraph, sId) < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFlowSelectionSet(&pGraph->arrSelectedEdges, sId, bSelected);
	if ( iRet == XUI_OK ) {
		__xuiFlowBumpRevision(pGraph);
	}
	return iRet;
}

XUI_API int xuiFlowGraphIsNodeSelected(xui_flow_graph pGraph, const char* sId)
{
	return (pGraph != NULL && __xuiFlowSelectionFind(&pGraph->arrSelectedNodes, sId) >= 0) ? 1 : 0;
}

XUI_API int xuiFlowGraphIsEdgeSelected(xui_flow_graph pGraph, const char* sId)
{
	return (pGraph != NULL && __xuiFlowSelectionFind(&pGraph->arrSelectedEdges, sId) >= 0) ? 1 : 0;
}

XUI_API int xuiFlowGraphGetSelectedNodeCount(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? (int)pGraph->arrSelectedNodes.Count : 0;
}

XUI_API int xuiFlowGraphGetSelectedEdgeCount(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? (int)pGraph->arrSelectedEdges.Count : 0;
}

XUI_API int xuiFlowGraphDeleteSelection(xui_flow_graph pGraph)
{
	char** ppId;
	char* sId;
	int iRet;

	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	while ( pGraph->arrSelectedEdges.Count > 0u ) {
		ppId = (char**)xrtArrayGet_Unsafe(&pGraph->arrSelectedEdges, 1u);
		sId = __xuiFlowCopyString(*ppId);
		if ( sId == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		iRet = xuiFlowGraphRemoveEdge(pGraph, sId);
		xrtFree(sId);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	while ( pGraph->arrSelectedNodes.Count > 0u ) {
		ppId = (char**)xrtArrayGet_Unsafe(&pGraph->arrSelectedNodes, 1u);
		sId = __xuiFlowCopyString(*ppId);
		if ( sId == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		iRet = xuiFlowGraphRemoveNode(pGraph, sId);
		xrtFree(sId);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiFlowGraphSetViewport(xui_flow_graph pGraph, const xui_flow_viewport_t* pViewport)
{
	if ( (pGraph == NULL) || (pViewport == NULL) || (pViewport->iSize < sizeof(*pViewport)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pGraph->tViewport = *pViewport;
	pGraph->tViewport.iSize = sizeof(pGraph->tViewport);
	if ( pGraph->tViewport.fZoom <= 0.001f ) {
		pGraph->tViewport.fZoom = 0.001f;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetViewport(xui_flow_graph pGraph, xui_flow_viewport_t* pViewport)
{
	if ( (pGraph == NULL) || (pViewport == NULL) || (pViewport->iSize < sizeof(*pViewport)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pViewport = pGraph->tViewport;
	pViewport->iSize = sizeof(*pViewport);
	return XUI_OK;
}

XUI_API int xuiFlowGraphWorldToScreen(xui_flow_graph pGraph, float fWorldX, float fWorldY, float* pScreenX, float* pScreenY)
{
	if ( (pGraph == NULL) || (pScreenX == NULL) || (pScreenY == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pScreenX = (fWorldX + pGraph->tViewport.fPanX) * pGraph->tViewport.fZoom;
	*pScreenY = (fWorldY + pGraph->tViewport.fPanY) * pGraph->tViewport.fZoom;
	return XUI_OK;
}

XUI_API int xuiFlowGraphScreenToWorld(xui_flow_graph pGraph, float fScreenX, float fScreenY, float* pWorldX, float* pWorldY)
{
	float fZoom;

	if ( (pGraph == NULL) || (pWorldX == NULL) || (pWorldY == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fZoom = pGraph->tViewport.fZoom;
	if ( fZoom <= 0.001f ) {
		fZoom = 0.001f;
	}
	*pWorldX = (fScreenX / fZoom) - pGraph->tViewport.fPanX;
	*pWorldY = (fScreenY / fZoom) - pGraph->tViewport.fPanY;
	return XUI_OK;
}

XUI_API int xuiFlowGraphAddDiagnostic(xui_flow_graph pGraph, const xui_flow_diagnostic_desc_t* pDesc, int* pIndex)
{
	xui_flow_diagnostic_model_t* pDiagnostic;
	uint32_t iPos;

	if ( (pGraph == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPos = xrtArrayAppend(&pGraph->arrDiagnostics, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDiagnostic = (xui_flow_diagnostic_model_t*)xrtArrayGet_Unsafe(&pGraph->arrDiagnostics, iPos);
	memset(pDiagnostic, 0, sizeof(*pDiagnostic));
	pDiagnostic->iSeverity = pDesc->iSeverity;
	pDiagnostic->sCode = __xuiFlowCopyString(pDesc->sCode);
	pDiagnostic->sMessage = __xuiFlowCopyString(pDesc->sMessage);
	pDiagnostic->sNode = __xuiFlowCopyString(pDesc->sNode);
	pDiagnostic->sEdge = __xuiFlowCopyString(pDesc->sEdge);
	pDiagnostic->sPath = __xuiFlowCopyString(pDesc->sPath);
	if ( (pDesc->sCode != NULL && pDiagnostic->sCode == NULL) ||
	     (pDesc->sMessage != NULL && pDiagnostic->sMessage == NULL) ||
	     (pDesc->sNode != NULL && pDiagnostic->sNode == NULL) ||
	     (pDesc->sEdge != NULL && pDiagnostic->sEdge == NULL) ||
	     (pDesc->sPath != NULL && pDiagnostic->sPath == NULL) ) {
		__xuiFlowFreeDiagnostic(pDiagnostic);
		xrtArrayRemove(&pGraph->arrDiagnostics, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pIndex != NULL ) {
		*pIndex = (int)iPos - 1;
	}
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphClearDiagnostics(xui_flow_graph pGraph)
{
	if ( pGraph == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowFreeDiagnostics(pGraph);
	xrtArrayInit(&pGraph->arrDiagnostics, sizeof(xui_flow_diagnostic_model_t), XRT_OBJMODE_LOCAL);
	__xuiFlowBumpRevision(pGraph);
	return XUI_OK;
}

XUI_API int xuiFlowGraphGetDiagnosticCount(xui_flow_graph pGraph)
{
	return (pGraph != NULL) ? (int)pGraph->arrDiagnostics.Count : 0;
}

XUI_API int xuiFlowGraphGetNodeDiagnosticCount(xui_flow_graph pGraph, const char* sNodeId)
{
	xui_flow_diagnostic_model_t* pDiagnostic;
	uint32_t i;
	int iCount;

	if ( (pGraph == NULL) || (sNodeId == NULL) ) {
		return 0;
	}
	iCount = 0;
	for ( i = 1u; i <= pGraph->arrDiagnostics.Count; ++i ) {
		pDiagnostic = (xui_flow_diagnostic_model_t*)xrtArrayGet_Unsafe(&pGraph->arrDiagnostics, i);
		if ( pDiagnostic->sNode != NULL && strcmp(pDiagnostic->sNode, sNodeId) == 0 ) {
			iCount++;
		}
	}
	return iCount;
}

XUI_API int xuiFlowGraphGetEdgeDiagnosticCount(xui_flow_graph pGraph, const char* sEdgeId)
{
	xui_flow_diagnostic_model_t* pDiagnostic;
	uint32_t i;
	int iCount;

	if ( (pGraph == NULL) || (sEdgeId == NULL) ) {
		return 0;
	}
	iCount = 0;
	for ( i = 1u; i <= pGraph->arrDiagnostics.Count; ++i ) {
		pDiagnostic = (xui_flow_diagnostic_model_t*)xrtArrayGet_Unsafe(&pGraph->arrDiagnostics, i);
		if ( pDiagnostic->sEdge != NULL && strcmp(pDiagnostic->sEdge, sEdgeId) == 0 ) {
			iCount++;
		}
	}
	return iCount;
}
