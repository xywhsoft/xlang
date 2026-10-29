#include "xui_internal.h"
#include "xui_value_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct xui_workflow_type_model_t {
	char* sId;
	int iVersion;
	char* sTitle;
	char* sCategory;
	char* sDescription;
	char* sIcon;
	xarray arrInputs;
	xarray arrOutputs;
	xvalue* pConfigSchema;
	xui_workflow_dynamic_ports_proc onDynamicPorts;
	void* pDynamicPortsUser;
	xui_workflow_validate_proc onValidate;
	void* pValidateUser;
} xui_workflow_type_model_t;

typedef struct xui_workflow_variable_model_t {
	char* sId;
	char* sTitle;
	char* sType;
	char* sScope;
	xvalue* pDefaultValue;
} xui_workflow_variable_model_t;

typedef struct xui_workflow_node_run_state_model_t {
	char* sNodeId;
	int iState;
	char* sPreview;
} xui_workflow_node_run_state_model_t;

typedef struct xui_workflow_edge_run_state_model_t {
	char* sEdgeId;
	int iState;
	char* sPreview;
} xui_workflow_edge_run_state_model_t;

typedef struct xui_workflow_runtime_state_model_t {
	int iWorkflowState;
	xarray arrNodeStates;
	xarray arrEdgeStates;
} xui_workflow_runtime_state_model_t;

typedef struct xui_workflow_diagnostic_model_t {
	int iSeverity;
	char* sCode;
	char* sMessage;
	char* sPath;
} xui_workflow_diagnostic_model_t;

struct xui_workflow_t {
	xui_flow_graph pGraph;
	xarray arrTypes;
	xarray arrVariables;
	xarray arrDiagnostics;
	xui_workflow_runtime_state_model_t tRuntime;
	xmap mapTypes;
	xmap mapVariables;
	uint32_t iNextNodeId;
	uint32_t iNextVariableId;
};

static char* __xuiWorkflowCopyString(const char* sText)
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

static void __xuiWorkflowFreePortDesc(xui_flow_port_desc_t* pPort)
{
	if ( pPort == NULL ) {
		return;
	}
	xrtFree((void*)pPort->sId);
	xrtFree((void*)pPort->sTitle);
	xrtFree((void*)pPort->sDataType);
	memset(pPort, 0, sizeof(*pPort));
}

static int __xuiWorkflowCopyPortArray(xarray* pDst, const xui_flow_port_desc_t* pSrc, int iCount)
{
	xui_flow_port_desc_t* pPort;
	uint32_t iPos;
	int i;

	if ( iCount < 0 || (iCount > 0 && pSrc == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( pSrc[i].iSize < sizeof(pSrc[i]) || pSrc[i].sId == NULL || pSrc[i].sId[0] == 0 ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iPos = __xuiArraySlotAdd(pDst, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pPort = (xui_flow_port_desc_t*)__xuiArraySlotGet(pDst, iPos);
		memset(pPort, 0, sizeof(*pPort));
		*pPort = pSrc[i];
		pPort->sId = __xuiWorkflowCopyString(pSrc[i].sId);
		pPort->sTitle = __xuiWorkflowCopyString(pSrc[i].sTitle);
		pPort->sDataType = __xuiWorkflowCopyString(pSrc[i].sDataType);
		if ( pPort->sId == NULL || (pSrc[i].sTitle != NULL && pPort->sTitle == NULL) || (pSrc[i].sDataType != NULL && pPort->sDataType == NULL) ) {
			__xuiWorkflowFreePortDesc(pPort);
			__xuiArraySlotRemove(pDst, iPos, 1u);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	return XUI_OK;
}

static void __xuiWorkflowFreeType(xui_workflow_type_model_t* pType)
{
	uint32_t i;

	if ( pType == NULL ) {
		return;
	}
	for ( i = 1u; i <= pType->arrInputs.Count; ++i ) {
		__xuiWorkflowFreePortDesc((xui_flow_port_desc_t*)__xuiArraySlotGet(&pType->arrInputs, i));
	}
	for ( i = 1u; i <= pType->arrOutputs.Count; ++i ) {
		__xuiWorkflowFreePortDesc((xui_flow_port_desc_t*)__xuiArraySlotGet(&pType->arrOutputs, i));
	}
	xrtArrayUnit(&pType->arrInputs);
	xrtArrayUnit(&pType->arrOutputs);
	xrtFree(pType->sId);
	xrtFree(pType->sTitle);
	xrtFree(pType->sCategory);
	xrtFree(pType->sDescription);
	xrtFree(pType->sIcon);
	if ( pType->pConfigSchema != NULL ) {
		xrtValueRelease(pType->pConfigSchema);
	}
	memset(pType, 0, sizeof(*pType));
}

static void __xuiWorkflowFreeVariable(xui_workflow_variable_model_t* pVariable)
{
	if ( pVariable == NULL ) {
		return;
	}
	xrtFree(pVariable->sId);
	xrtFree(pVariable->sTitle);
	xrtFree(pVariable->sType);
	xrtFree(pVariable->sScope);
	if ( pVariable->pDefaultValue != NULL ) {
		xrtValueRelease(pVariable->pDefaultValue);
	}
	memset(pVariable, 0, sizeof(*pVariable));
}

static void __xuiWorkflowFreeNodeRunState(xui_workflow_node_run_state_model_t* pState)
{
	if ( pState == NULL ) {
		return;
	}
	xrtFree(pState->sNodeId);
	xrtFree(pState->sPreview);
	memset(pState, 0, sizeof(*pState));
}

static void __xuiWorkflowFreeEdgeRunState(xui_workflow_edge_run_state_model_t* pState)
{
	if ( pState == NULL ) {
		return;
	}
	xrtFree(pState->sEdgeId);
	xrtFree(pState->sPreview);
	memset(pState, 0, sizeof(*pState));
}

static void __xuiWorkflowFreeDiagnostic(xui_workflow_diagnostic_model_t* pDiagnostic)
{
	if ( pDiagnostic == NULL ) {
		return;
	}
	xrtFree(pDiagnostic->sCode);
	xrtFree(pDiagnostic->sMessage);
	xrtFree(pDiagnostic->sPath);
	memset(pDiagnostic, 0, sizeof(*pDiagnostic));
}

static void __xuiWorkflowClearVariables(xui_workflow pWorkflow)
{
	uint32_t i;

	if ( pWorkflow == NULL ) {
		return;
	}
	for ( i = 1u; i <= pWorkflow->arrVariables.Count; ++i ) {
		__xuiWorkflowFreeVariable((xui_workflow_variable_model_t*)__xuiArraySlotGet(&pWorkflow->arrVariables, i));
	}
	xrtArrayUnit(&pWorkflow->arrVariables);
	xrtArrayInit(&pWorkflow->arrVariables, sizeof(xui_workflow_variable_model_t));
	xrtMapUnit(&pWorkflow->mapVariables);
	xrtMapInit(&pWorkflow->mapVariables, sizeof(int));
	pWorkflow->iNextVariableId = 0u;
}

static int __xuiWorkflowDictFindIndex(xmap* pDict, const char* sId)
{
	int* pValue;

	if ( (pDict == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return -1;
	}
	pValue = (int*)__xuiMapGet(pDict, (ptr)(void*)sId, (uint32)strlen(sId));
	return (pValue != NULL) ? *pValue : -1;
}

static int __xuiWorkflowDictSetIndex(xmap* pDict, const char* sId, int iIndex)
{
	int* pValue;
	bool bNew;

	if ( (pDict == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pValue = (int*)__xuiMapGetOrAdd(pDict, (ptr)(void*)sId, (uint32)strlen(sId), &bNew);
	if ( pValue == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*pValue = iIndex;
	return bNew ? XUI_OK : XUI_ERROR_ALREADY_INITIALIZED;
}

static int __xuiWorkflowConfigKindToXvoType(int iKind)
{
	switch ( iKind ) {
		case XUI_WORKFLOW_CONFIG_FIELD_STRING:
		case XUI_WORKFLOW_CONFIG_FIELD_TEXTAREA:
		case XUI_WORKFLOW_CONFIG_FIELD_EXPRESSION:
		case XUI_WORKFLOW_CONFIG_FIELD_SELECT:
		case XUI_WORKFLOW_CONFIG_FIELD_VARIABLE_REF:
		case XUI_WORKFLOW_CONFIG_FIELD_NODE_OUTPUT_REF:
			return XVALUE_STRING;
		case XUI_WORKFLOW_CONFIG_FIELD_MULTI_SELECT:
		case XUI_WORKFLOW_CONFIG_FIELD_ARRAY:
			return XVALUE_ARRAY;
		case XUI_WORKFLOW_CONFIG_FIELD_OBJECT:
		case XUI_WORKFLOW_CONFIG_FIELD_GROUP:
		case XUI_WORKFLOW_CONFIG_FIELD_TABS:
		case XUI_WORKFLOW_CONFIG_FIELD_CONDITION_BUILDER:
		case XUI_WORKFLOW_CONFIG_FIELD_MAPPING_BUILDER:
			return XVALUE_OBJECT;
		case XUI_WORKFLOW_CONFIG_FIELD_INT:
			return XVALUE_INT;
		case XUI_WORKFLOW_CONFIG_FIELD_FLOAT:
			return XVALUE_FLOAT;
		case XUI_WORKFLOW_CONFIG_FIELD_BOOL:
			return XVALUE_BOOL;
		default:
			return XVALUE_NULL;
	}
}

static int __xuiWorkflowConfigDefaultForKind(int iKind, xvalue** ppValue)
{
	if ( ppValue == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppValue = NULL;
	switch ( iKind ) {
		case XUI_WORKFLOW_CONFIG_FIELD_STRING:
		case XUI_WORKFLOW_CONFIG_FIELD_TEXTAREA:
		case XUI_WORKFLOW_CONFIG_FIELD_EXPRESSION:
		case XUI_WORKFLOW_CONFIG_FIELD_SELECT:
		case XUI_WORKFLOW_CONFIG_FIELD_VARIABLE_REF:
		case XUI_WORKFLOW_CONFIG_FIELD_NODE_OUTPUT_REF:
			*ppValue = __xuiValueCreateText("", 0, false);
			break;
		case XUI_WORKFLOW_CONFIG_FIELD_MULTI_SELECT:
		case XUI_WORKFLOW_CONFIG_FIELD_ARRAY:
			*ppValue = xrtValueArray();
			break;
		case XUI_WORKFLOW_CONFIG_FIELD_OBJECT:
		case XUI_WORKFLOW_CONFIG_FIELD_GROUP:
		case XUI_WORKFLOW_CONFIG_FIELD_TABS:
		case XUI_WORKFLOW_CONFIG_FIELD_CONDITION_BUILDER:
		case XUI_WORKFLOW_CONFIG_FIELD_MAPPING_BUILDER:
			*ppValue = xrtValueObject();
			break;
		case XUI_WORKFLOW_CONFIG_FIELD_INT:
			*ppValue = xrtValueInt(0);
			break;
		case XUI_WORKFLOW_CONFIG_FIELD_FLOAT:
			*ppValue = xrtValueFloat(0.0);
			break;
		case XUI_WORKFLOW_CONFIG_FIELD_BOOL:
			*ppValue = xrtValueBool(false);
			break;
		default:
			return XUI_ERROR_INVALID_ARGUMENT;
	}
	return (*ppValue != NULL) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static xvalue* __xuiWorkflowConfigFields(xvalue* pSchema)
{
	xvalue* pFields;

	if ( (pSchema == NULL) || (xrtValueType(pSchema) != XVALUE_OBJECT) ) {
		return NULL;
	}
	pFields = __xuiValueObjectGet(pSchema, "fields", 6);
	return (xrtValueType(pFields) == XVALUE_ARRAY) ? pFields : NULL;
}

static xui_workflow_type_model_t* __xuiWorkflowFindTypeModel(xui_workflow pWorkflow, const char* sType)
{
	int iType;

	if ( (pWorkflow == NULL) || (sType == NULL) ) {
		return NULL;
	}
	iType = xuiWorkflowFindNodeType(pWorkflow, sType);
	return (iType >= 0) ? (xui_workflow_type_model_t*)__xuiArraySlotGet(&pWorkflow->arrTypes, (uint32_t)iType + 1u) : NULL;
}

static int __xuiWorkflowGetNodeType(xui_workflow pWorkflow, const char* sNodeId, xui_flow_node_info_t* pInfo)
{
	int iNode;
	int iRet;

	if ( (pWorkflow == NULL) || (sNodeId == NULL) || (sNodeId[0] == 0) || (pInfo == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pWorkflow->pGraph, sNodeId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, iNode, pInfo);
	if ( iRet != XUI_OK || pInfo->sType == NULL || pInfo->sType[0] == 0 ) {
		return (iRet == XUI_OK) ? XUI_ERROR_INVALID_ARGUMENT : iRet;
	}
	return XUI_OK;
}

static void __xuiWorkflowSummaryAppend(char* sBuffer, int iCapacity, int* pOffset, const char* sText)
{
	int iWritten;

	if ( (sBuffer == NULL) || (iCapacity <= 0) || (pOffset == NULL) || (*pOffset >= iCapacity - 1) || (sText == NULL) ) {
		return;
	}
	iWritten = snprintf(sBuffer + *pOffset, (size_t)(iCapacity - *pOffset), "%s", sText);
	if ( iWritten < 0 ) {
		return;
	}
	*pOffset += iWritten;
	if ( *pOffset >= iCapacity ) {
		*pOffset = iCapacity - 1;
		sBuffer[iCapacity - 1] = '\0';
	}
}

static int __xuiWorkflowSummaryValue(char* sBuffer, int iCapacity, const char* sId, xvalue* pValue)
{
	const char* sText;

	if ( (sBuffer == NULL) || (iCapacity <= 0) || (sId == NULL) || (pValue == NULL) ) {
		return 0;
	}
	switch ( xrtValueType(pValue) ) {
		case XVALUE_STRING:
			sText = (const char*)__xuiValueGetText(pValue);
			if ( sText == NULL || sText[0] == 0 ) return 0;
			snprintf(sBuffer, (size_t)iCapacity, "%s=%s", sId, sText);
			return 1;
		case XVALUE_INT:
			snprintf(sBuffer, (size_t)iCapacity, "%s=%lld", sId, (long long)__xuiValueGetInt(pValue));
			return 1;
		case XVALUE_FLOAT:
			snprintf(sBuffer, (size_t)iCapacity, "%s=%.2f", sId, __xuiValueGetFloat(pValue));
			return 1;
		case XVALUE_BOOL:
			snprintf(sBuffer, (size_t)iCapacity, "%s=%s", sId, __xuiValueGetBool(pValue) ? "true" : "false");
			return 1;
		default:
			break;
	}
	return 0;
}

static int __xuiWorkflowConfigFieldKind(xui_workflow pWorkflow, const char* sType, const char* sField)
{
	xui_workflow_type_model_t* pType;
	xvalue* pFields;
	xvalue* pField;
	const char* sId;
	uint32_t i;
	uint32_t iCount;

	if ( (pWorkflow == NULL) || (sType == NULL) || (sField == NULL) ) {
		return 0;
	}
	pType = __xuiWorkflowFindTypeModel(pWorkflow, sType);
	if ( pType == NULL ) {
		return 0;
	}
	pFields = __xuiWorkflowConfigFields(pType->pConfigSchema);
	if ( pFields == NULL ) {
		return 0;
	}
	iCount = xrtValueCount(pFields);
	for ( i = 0u; i < iCount; ++i ) {
		pField = xrtValueArrayGet(pFields, i);
		if ( xrtValueType(pField) != XVALUE_OBJECT ) {
			continue;
		}
		sId = (const char*)__xuiValueObjectGetText(pField, "id", 2);
		if ( sId != NULL && strcmp(sId, sField) == 0 ) {
			return (int)__xuiValueObjectGetInt(pField, "kind", 4);
		}
	}
	return 0;
}

static int __xuiWorkflowRefreshNodeSummary(xui_workflow pWorkflow, const char* sNodeId, const char* sType, xvalue* pConfig)
{
	xui_workflow_type_model_t* pType;
	xvalue* pFields;
	xvalue* pField;
	xvalue* pValue;
	const char* sId;
	char sSummary[256];
	char sPart[96];
	int iOffset;
	int iParts;
	uint32_t i;
	uint32_t iCount;

	if ( (pWorkflow == NULL) || (sNodeId == NULL) || (sType == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = __xuiWorkflowFindTypeModel(pWorkflow, sType);
	if ( pType == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sSummary[0] = '\0';
	iOffset = 0;
	iParts = 0;
	pFields = __xuiWorkflowConfigFields(pType->pConfigSchema);
	if ( pFields != NULL && pConfig != NULL && xrtValueType(pConfig) == XVALUE_OBJECT ) {
		iCount = xrtValueCount(pFields);
		for ( i = 0u; i < iCount && iParts < 3; ++i ) {
			pField = xrtValueArrayGet(pFields, i);
			if ( xrtValueType(pField) != XVALUE_OBJECT ) {
				continue;
			}
			sId = (const char*)__xuiValueObjectGetText(pField, "id", 2);
			if ( sId == NULL || sId[0] == 0 ) {
				continue;
			}
			pValue = __xuiValueObjectGet(pConfig, sId, (uint32_t)strlen(sId));
			if ( !__xuiWorkflowSummaryValue(sPart, (int)sizeof(sPart), sId, pValue) ) {
				continue;
			}
			if ( iParts > 0 ) {
				__xuiWorkflowSummaryAppend(sSummary, (int)sizeof(sSummary), &iOffset, ", ");
			}
			__xuiWorkflowSummaryAppend(sSummary, (int)sizeof(sSummary), &iOffset, sPart);
			iParts++;
		}
	}
	return xuiFlowGraphSetNodeSummary(pWorkflow->pGraph, sNodeId, (sSummary[0] != 0) ? sSummary : NULL);
}

static void __xuiWorkflowConfigAddDiagnostic(xui_workflow_config_diagnostic_t* pDiagnostics, int iDiagnosticCapacity, int* pDiagnosticCount, const char* sPath, const char* sCode, const char* sMessage)
{
	xui_workflow_config_diagnostic_t* pDiagnostic;
	int iIndex;

	if ( pDiagnosticCount == NULL ) {
		return;
	}
	iIndex = *pDiagnosticCount;
	(*pDiagnosticCount)++;
	if ( (pDiagnostics == NULL) || (iIndex < 0) || (iIndex >= iDiagnosticCapacity) ) {
		return;
	}
	pDiagnostic = &pDiagnostics[iIndex];
	memset(pDiagnostic, 0, sizeof(*pDiagnostic));
	pDiagnostic->iSize = sizeof(*pDiagnostic);
	pDiagnostic->sPath = sPath;
	pDiagnostic->sCode = sCode;
	pDiagnostic->sMessage = sMessage;
}

XUI_API int xuiWorkflowCreate(xui_workflow* ppWorkflow)
{
	xui_workflow pWorkflow;
	int iRet;

	if ( ppWorkflow == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWorkflow = NULL;
	pWorkflow = (xui_workflow)xrtCalloc(1u, sizeof(*pWorkflow));
	if ( pWorkflow == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = xuiFlowGraphCreate(&pWorkflow->pGraph);
	if ( iRet != XUI_OK ) {
		xrtFree(pWorkflow);
		return iRet;
	}
	xrtArrayInit(&pWorkflow->arrTypes, sizeof(xui_workflow_type_model_t));
	xrtArrayInit(&pWorkflow->arrVariables, sizeof(xui_workflow_variable_model_t));
	xrtArrayInit(&pWorkflow->arrDiagnostics, sizeof(xui_workflow_diagnostic_model_t));
	xrtArrayInit(&pWorkflow->tRuntime.arrNodeStates, sizeof(xui_workflow_node_run_state_model_t));
	xrtArrayInit(&pWorkflow->tRuntime.arrEdgeStates, sizeof(xui_workflow_edge_run_state_model_t));
	xrtMapInit(&pWorkflow->mapTypes, sizeof(int));
	xrtMapInit(&pWorkflow->mapVariables, sizeof(int));
	*ppWorkflow = pWorkflow;
	return XUI_OK;
}

XUI_API void xuiWorkflowDestroy(xui_workflow pWorkflow)
{
	uint32_t i;

	if ( pWorkflow == NULL ) {
		return;
	}
	for ( i = 1u; i <= pWorkflow->arrTypes.Count; ++i ) {
		__xuiWorkflowFreeType((xui_workflow_type_model_t*)__xuiArraySlotGet(&pWorkflow->arrTypes, i));
	}
	for ( i = 1u; i <= pWorkflow->arrVariables.Count; ++i ) {
		__xuiWorkflowFreeVariable((xui_workflow_variable_model_t*)__xuiArraySlotGet(&pWorkflow->arrVariables, i));
	}
	for ( i = 1u; i <= pWorkflow->arrDiagnostics.Count; ++i ) {
		__xuiWorkflowFreeDiagnostic((xui_workflow_diagnostic_model_t*)__xuiArraySlotGet(&pWorkflow->arrDiagnostics, i));
	}
	for ( i = 1u; i <= pWorkflow->tRuntime.arrNodeStates.Count; ++i ) {
		__xuiWorkflowFreeNodeRunState((xui_workflow_node_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrNodeStates, i));
	}
	for ( i = 1u; i <= pWorkflow->tRuntime.arrEdgeStates.Count; ++i ) {
		__xuiWorkflowFreeEdgeRunState((xui_workflow_edge_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrEdgeStates, i));
	}
	xrtArrayUnit(&pWorkflow->arrTypes);
	xrtArrayUnit(&pWorkflow->arrVariables);
	xrtArrayUnit(&pWorkflow->arrDiagnostics);
	xrtArrayUnit(&pWorkflow->tRuntime.arrNodeStates);
	xrtArrayUnit(&pWorkflow->tRuntime.arrEdgeStates);
	xrtMapUnit(&pWorkflow->mapTypes);
	xrtMapUnit(&pWorkflow->mapVariables);
	xuiFlowGraphDestroy(pWorkflow->pGraph);
	xrtFree(pWorkflow);
}

XUI_API xui_flow_graph xuiWorkflowGetGraph(xui_workflow pWorkflow)
{
	return (pWorkflow != NULL) ? pWorkflow->pGraph : NULL;
}

XUI_API int xuiWorkflowConfigSchemaCreate(xvalue** ppSchema)
{
	xvalue* pSchema;
	xvalue* pFields;

	if ( ppSchema == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppSchema = NULL;
	pSchema = xrtValueObject();
	if ( pSchema == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pFields = xrtValueArray();
	if ( pFields == NULL ) {
		xrtValueRelease(pSchema);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xuiValueObjectSetValue(pSchema, "fields", 6, pFields, true) ) {
		xrtValueRelease(pFields);
		xrtValueRelease(pSchema);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	*ppSchema = pSchema;
	return XUI_OK;
}

XUI_API int xuiWorkflowConfigSchemaAddField(xvalue* pSchema, const xui_workflow_config_field_desc_t* pDesc)
{
	xvalue* pFields;
	xvalue* pField;
	xvalue* pDefaultCopy;
	xvalue* pMetaCopy;

	if ( (pSchema == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ||
	     (__xuiWorkflowConfigKindToXvoType(pDesc->iKind) == XVALUE_NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pFields = __xuiWorkflowConfigFields(pSchema);
	if ( pFields == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pField = xrtValueObject();
	if ( pField == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xuiValueObjectSetText(pField, "id", 2, (ptr)(void*)pDesc->sId, (uint32_t)strlen(pDesc->sId), false) ||
	     !__xuiValueObjectSetInt(pField, "kind", 4, pDesc->iKind) ||
	     !__xuiValueObjectSetBool(pField, "required", 8, pDesc->bRequired ? true : false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sTitle != NULL && !__xuiValueObjectSetText(pField, "title", 5, (ptr)(void*)pDesc->sTitle, (uint32_t)strlen(pDesc->sTitle), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sExpressionLanguage != NULL && !__xuiValueObjectSetText(pField, "expressionLanguage", 18, (ptr)(void*)pDesc->sExpressionLanguage, (uint32_t)strlen(pDesc->sExpressionLanguage), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sRefScope != NULL && !__xuiValueObjectSetText(pField, "refScope", 8, (ptr)(void*)pDesc->sRefScope, (uint32_t)strlen(pDesc->sRefScope), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sRefType != NULL && !__xuiValueObjectSetText(pField, "refType", 7, (ptr)(void*)pDesc->sRefType, (uint32_t)strlen(pDesc->sRefType), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sGroup != NULL && !__xuiValueObjectSetText(pField, "group", 5, (ptr)(void*)pDesc->sGroup, (uint32_t)strlen(pDesc->sGroup), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->sTab != NULL && !__xuiValueObjectSetText(pField, "tab", 3, (ptr)(void*)pDesc->sTab, (uint32_t)strlen(pDesc->sTab), false) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->pOptions != NULL ) {
		pMetaCopy = xrtValueDeepClone(pDesc->pOptions);
		if ( pMetaCopy == NULL ) {
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( !__xuiValueObjectSetValue(pField, "options", 7, pMetaCopy, true) ) {
			xrtValueRelease(pMetaCopy);
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pDesc->pChildren != NULL ) {
		pMetaCopy = xrtValueDeepClone(pDesc->pChildren);
		if ( pMetaCopy == NULL ) {
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( !__xuiValueObjectSetValue(pField, "children", 8, pMetaCopy, true) ) {
			xrtValueRelease(pMetaCopy);
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pDesc->bHasMin && !__xuiValueObjectSetFloat(pField, "min", 3, pDesc->fMin) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->bHasMax && !__xuiValueObjectSetFloat(pField, "max", 3, pDesc->fMax) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pDesc->bHasDefault ) {
		pDefaultCopy = xrtValueDeepClone(pDesc->pDefaultValue);
		if ( pDefaultCopy == NULL ) {
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( !__xuiValueObjectSetValue(pField, "default", 7, pDefaultCopy, true) ) {
			xrtValueRelease(pDefaultCopy);
			xrtValueRelease(pField);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( !__xuiValueArrayAppendValue(pFields, pField, true) ) {
		xrtValueRelease(pField);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

XUI_API int xuiWorkflowRegisterNodeType(xui_workflow pWorkflow, const xui_workflow_node_type_desc_t* pDesc, int* pIndex)
{
	xui_workflow_type_model_t* pType;
	uint32_t iPos;
	int iIndex;
	int iRet;

	if ( (pWorkflow == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiWorkflowFindNodeType(pWorkflow, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iPos = __xuiArraySlotAdd(&pWorkflow->arrTypes, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pType = (xui_workflow_type_model_t*)__xuiArraySlotGet(&pWorkflow->arrTypes, iPos);
	memset(pType, 0, sizeof(*pType));
	xrtArrayInit(&pType->arrInputs, sizeof(xui_flow_port_desc_t));
	xrtArrayInit(&pType->arrOutputs, sizeof(xui_flow_port_desc_t));
	pType->sId = __xuiWorkflowCopyString(pDesc->sId);
	pType->iVersion = pDesc->iVersion;
	pType->sTitle = __xuiWorkflowCopyString(pDesc->sTitle);
	pType->sCategory = __xuiWorkflowCopyString(pDesc->sCategory);
	pType->sDescription = __xuiWorkflowCopyString(pDesc->sDescription);
	pType->sIcon = __xuiWorkflowCopyString(pDesc->sIcon);
	pType->pConfigSchema = pDesc->pConfigSchema;
	pType->onDynamicPorts = pDesc->onDynamicPorts;
	pType->pDynamicPortsUser = pDesc->pDynamicPortsUser;
	pType->onValidate = pDesc->onValidate;
	pType->pValidateUser = pDesc->pValidateUser;
	if ( pType->pConfigSchema != NULL ) {
		xrtValueRetain(pType->pConfigSchema);
	}
	if ( pType->sId == NULL ||
	     (pDesc->sTitle != NULL && pType->sTitle == NULL) ||
	     (pDesc->sCategory != NULL && pType->sCategory == NULL) ||
	     (pDesc->sDescription != NULL && pType->sDescription == NULL) ||
	     (pDesc->sIcon != NULL && pType->sIcon == NULL) ) {
		__xuiWorkflowFreeType(pType);
		__xuiArraySlotRemove(&pWorkflow->arrTypes, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowCopyPortArray(&pType->arrInputs, pDesc->pInputs, pDesc->iInputCount);
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowCopyPortArray(&pType->arrOutputs, pDesc->pOutputs, pDesc->iOutputCount);
	}
	if ( iRet != XUI_OK ) {
		__xuiWorkflowFreeType(pType);
		__xuiArraySlotRemove(&pWorkflow->arrTypes, iPos, 1u);
		return iRet;
	}
	iRet = __xuiWorkflowDictSetIndex(&pWorkflow->mapTypes, pType->sId, iIndex);
	if ( iRet != XUI_OK ) {
		__xuiWorkflowFreeType(pType);
		__xuiArraySlotRemove(&pWorkflow->arrTypes, iPos, 1u);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	return XUI_OK;
}

XUI_API int xuiWorkflowFindNodeType(xui_workflow pWorkflow, const char* sId)
{
	if ( pWorkflow == NULL ) {
		return -1;
	}
	return __xuiWorkflowDictFindIndex(&pWorkflow->mapTypes, sId);
}

XUI_API int xuiWorkflowGetNodeTypeCount(xui_workflow pWorkflow)
{
	return (pWorkflow != NULL) ? (int)pWorkflow->arrTypes.Count : 0;
}

static int __xuiWorkflowAddDynamicPorts(xui_workflow pWorkflow, const xui_workflow_type_model_t* pType, const char* sNodeId, int iNode, xvalue* pConfig)
{
	xui_flow_port_desc_t arrPorts[32];
	int iCount;
	int i;
	int iRet;

	if ( (pWorkflow == NULL) || (pType == NULL) || (pType->onDynamicPorts == NULL) ) {
		return XUI_OK;
	}
	memset(arrPorts, 0, sizeof(arrPorts));
	iCount = 0;
	iRet = pType->onDynamicPorts(pWorkflow, sNodeId, pType->sId, pConfig, arrPorts, (int)(sizeof(arrPorts) / sizeof(arrPorts[0])), &iCount, pType->pDynamicPortsUser);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iCount < 0 || iCount > (int)(sizeof(arrPorts) / sizeof(arrPorts[0])) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( arrPorts[i].iSize < sizeof(arrPorts[i]) ) {
			arrPorts[i].iSize = sizeof(arrPorts[i]);
		}
		arrPorts[i].bDynamic = 1;
		iRet = xuiFlowGraphAddPort(pWorkflow->pGraph, iNode, &arrPorts[i], NULL);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiWorkflowRebuildDynamicPorts(xui_workflow pWorkflow, const xui_workflow_type_model_t* pType, const char* sNodeId, xvalue* pConfig)
{
	xui_flow_port_desc_t arrPorts[32];
	int iCount;
	int iRet;

	if ( (pWorkflow == NULL) || (pType == NULL) || (pType->onDynamicPorts == NULL) ) {
		return XUI_OK;
	}
	memset(arrPorts, 0, sizeof(arrPorts));
	iCount = 0;
	iRet = pType->onDynamicPorts(pWorkflow, sNodeId, pType->sId, pConfig, arrPorts, (int)(sizeof(arrPorts) / sizeof(arrPorts[0])), &iCount, pType->pDynamicPortsUser);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iCount < 0 || iCount > (int)(sizeof(arrPorts) / sizeof(arrPorts[0])) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiFlowGraphRebuildNodeDynamicPorts(pWorkflow->pGraph, sNodeId, arrPorts, iCount);
}

XUI_API int xuiWorkflowAddNode(xui_workflow pWorkflow, const char* sType, const char* sId, const char* sTitle, float fX, float fY, int* pIndex)
{
	xui_workflow_type_model_t* pType;
	xui_flow_node_desc_t tNode;
	xui_flow_port_desc_t* pPort;
	xvalue* pDefaultConfig;
	uint32_t i;
	int iType;
	int iNode;
	int iRet;

	if ( (pWorkflow == NULL) || (sType == NULL) || (sId == NULL) || (sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iType = xuiWorkflowFindNodeType(pWorkflow, sType);
	if ( iType < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = (xui_workflow_type_model_t*)__xuiArraySlotGet(&pWorkflow->arrTypes, (uint32_t)iType + 1u);
	memset(&tNode, 0, sizeof(tNode));
	tNode.iSize = sizeof(tNode);
	tNode.sId = sId;
	tNode.sType = sType;
	tNode.sTitle = (sTitle != NULL) ? sTitle : pType->sTitle;
	tNode.fX = fX;
	tNode.fY = fY;
	tNode.fW = 220.0f;
	tNode.fH = 120.0f;
	iRet = xuiFlowGraphAddNode(pWorkflow->pGraph, &tNode, &iNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pDefaultConfig = NULL;
	for ( i = 1u; i <= pType->arrInputs.Count; ++i ) {
		pPort = (xui_flow_port_desc_t*)__xuiArraySlotGet(&pType->arrInputs, i);
		iRet = xuiFlowGraphAddPort(pWorkflow->pGraph, iNode, pPort, NULL);
		if ( iRet != XUI_OK ) {
			(void)xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
			return iRet;
		}
	}
	for ( i = 1u; i <= pType->arrOutputs.Count; ++i ) {
		pPort = (xui_flow_port_desc_t*)__xuiArraySlotGet(&pType->arrOutputs, i);
		iRet = xuiFlowGraphAddPort(pWorkflow->pGraph, iNode, pPort, NULL);
		if ( iRet != XUI_OK ) {
			(void)xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
			return iRet;
		}
	}
	iRet = xuiWorkflowCreateDefaultConfig(pWorkflow, sType, &pDefaultConfig);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
		return iRet;
	}
	iRet = __xuiWorkflowAddDynamicPorts(pWorkflow, pType, sId, iNode, pDefaultConfig);
	if ( iRet != XUI_OK ) {
		xrtValueRelease(pDefaultConfig);
		(void)xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
		return iRet;
	}
	iRet = xuiFlowGraphSetNodeConfig(pWorkflow->pGraph, sId, pDefaultConfig);
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowRefreshNodeSummary(pWorkflow, sId, sType, pDefaultConfig);
	}
	xrtValueRelease(pDefaultConfig);
	if ( iRet != XUI_OK ) {
		(void)xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iNode;
	}
	return XUI_OK;
}

XUI_API int xuiWorkflowConnect(xui_workflow pWorkflow, const char* sEdgeId, const char* sFromNode, const char* sFromPort, const char* sToNode, const char* sToPort, int* pIndex)
{
	xui_flow_edge_desc_t tEdge;

	if ( (pWorkflow == NULL) || (sEdgeId == NULL) || (sEdgeId[0] == 0) ||
	     (sFromNode == NULL) || (sFromPort == NULL) || (sToNode == NULL) || (sToPort == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEdge, 0, sizeof(tEdge));
	tEdge.iSize = sizeof(tEdge);
	tEdge.sId = sEdgeId;
	tEdge.iKind = XUI_FLOW_PORT_CONTROL;
	tEdge.sFromNode = sFromNode;
	tEdge.sFromPort = sFromPort;
	tEdge.sToNode = sToNode;
	tEdge.sToPort = sToPort;
	return xuiFlowGraphAddEdge(pWorkflow->pGraph, &tEdge, pIndex);
}

XUI_API int xuiWorkflowRemoveNode(xui_workflow pWorkflow, const char* sId)
{
	if ( pWorkflow == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiFlowGraphRemoveNode(pWorkflow->pGraph, sId);
}

XUI_API int xuiWorkflowSelectNode(xui_workflow pWorkflow, const char* sId, int bSelected)
{
	if ( pWorkflow == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiFlowGraphSelectNode(pWorkflow->pGraph, sId, bSelected);
}

XUI_API int xuiWorkflowGetSelectedNode(xui_workflow pWorkflow, xui_flow_node_info_t* pInfo)
{
	xui_flow_node_info_t tInfo;
	int i;
	int iCount;
	int iRet;

	if ( (pWorkflow == NULL) || (pInfo == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = xuiFlowGraphGetNodeCount(pWorkflow->pGraph);
	for ( i = 0; i < iCount; ++i ) {
		iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, i, &tInfo);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( xuiFlowGraphIsNodeSelected(pWorkflow->pGraph, tInfo.sId) ) {
			*pInfo = tInfo;
			return XUI_OK;
		}
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiWorkflowGetNodeCount(xui_workflow pWorkflow)
{
	return (pWorkflow != NULL) ? xuiFlowGraphGetNodeCount(pWorkflow->pGraph) : 0;
}

XUI_API int xuiWorkflowGetNodeLibraryCount(xui_workflow pWorkflow)
{
	return (pWorkflow != NULL) ? (int)pWorkflow->arrTypes.Count : 0;
}

XUI_API int xuiWorkflowGetNodeLibraryItem(xui_workflow pWorkflow, int iIndex, xui_workflow_node_library_item_t* pItem)
{
	xui_workflow_type_model_t* pType;

	if ( (pWorkflow == NULL) || (pItem == NULL) || (iIndex < 0) || ((uint32_t)iIndex >= pWorkflow->arrTypes.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = (xui_workflow_type_model_t*)__xuiArraySlotGet(&pWorkflow->arrTypes, (uint32_t)iIndex + 1u);
	memset(pItem, 0, sizeof(*pItem));
	pItem->iSize = sizeof(*pItem);
	pItem->sId = pType->sId;
	pItem->iVersion = pType->iVersion;
	pItem->sTitle = pType->sTitle;
	pItem->sCategory = pType->sCategory;
	pItem->sDescription = pType->sDescription;
	pItem->sIcon = pType->sIcon;
	pItem->iInputCount = (int)pType->arrInputs.Count;
	pItem->iOutputCount = (int)pType->arrOutputs.Count;
	return XUI_OK;
}

XUI_API int xuiWorkflowCreateDefaultConfig(xui_workflow pWorkflow, const char* sType, xvalue** ppConfig)
{
	xui_workflow_type_model_t* pType;
	xvalue* pFields;
	xvalue* pConfig;
	xvalue* pField;
	xvalue* pDefault;
	xvalue* pValue;
	const char* sId;
	uint32_t i;
	uint32_t iCount;
	int iKind;
	int iRet;

	if ( ppConfig == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppConfig = NULL;
	pType = __xuiWorkflowFindTypeModel(pWorkflow, sType);
	if ( pType == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pConfig = xrtValueObject();
	if ( pConfig == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pFields = __xuiWorkflowConfigFields(pType->pConfigSchema);
	if ( pFields == NULL ) {
		*ppConfig = pConfig;
		return XUI_OK;
	}
	iCount = xrtValueCount(pFields);
	for ( i = 0u; i < iCount; ++i ) {
		pField = xrtValueArrayGet(pFields, i);
		if ( xrtValueType(pField) != XVALUE_OBJECT ) {
			continue;
		}
		sId = (const char*)__xuiValueObjectGetText(pField, "id", 2);
		iKind = (int)__xuiValueObjectGetInt(pField, "kind", 4);
		if ( (sId == NULL) || (sId[0] == 0) ) {
			continue;
		}
		pDefault = __xuiValueObjectGet(pField, "default", 7);
		if ( xrtValueType(pDefault) != XVALUE_NULL ) {
			pValue = xrtValueDeepClone(pDefault);
			if ( pValue == NULL ) {
				xrtValueRelease(pConfig);
				return XUI_ERROR_OUT_OF_MEMORY;
			}
		} else {
			iRet = __xuiWorkflowConfigDefaultForKind(iKind, &pValue);
			if ( iRet != XUI_OK ) {
				xrtValueRelease(pConfig);
				return iRet;
			}
		}
		if ( !__xuiValueObjectSetValue(pConfig, sId, (uint32_t)strlen(sId), pValue, true) ) {
			xrtValueRelease(pValue);
			xrtValueRelease(pConfig);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	*ppConfig = pConfig;
	return XUI_OK;
}

XUI_API int xuiWorkflowValidateConfig(xui_workflow pWorkflow, const char* sType, xvalue* pConfig, int* pDiagnosticCount)
{
	return xuiWorkflowValidateConfigEx(pWorkflow, sType, pConfig, NULL, 0, pDiagnosticCount);
}

XUI_API int xuiWorkflowValidateConfigEx(xui_workflow pWorkflow, const char* sType, xvalue* pConfig, xui_workflow_config_diagnostic_t* pDiagnostics, int iDiagnosticCapacity, int* pDiagnosticCount)
{
	xui_workflow_type_model_t* pType;
	xvalue* pFields;
	xvalue* pField;
	xvalue* pValue;
	const char* sId;
	int iDiagnostics;
	int iKind;
	int iExpectedType;
	int iValueType;
	int bRequired;
	double fValue;
	uint32_t i;
	uint32_t iCount;

	if ( (pDiagnosticCount == NULL) || (iDiagnosticCapacity < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pDiagnosticCount = 0;
	pType = __xuiWorkflowFindTypeModel(pWorkflow, sType);
	if ( pType == NULL || (pConfig != NULL && xrtValueType(pConfig) != XVALUE_OBJECT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pFields = __xuiWorkflowConfigFields(pType->pConfigSchema);
	if ( pFields == NULL ) {
		return XUI_OK;
	}
	iDiagnostics = 0;
	iCount = xrtValueCount(pFields);
	for ( i = 0u; i < iCount; ++i ) {
		pField = xrtValueArrayGet(pFields, i);
		if ( xrtValueType(pField) != XVALUE_OBJECT ) {
			continue;
		}
		sId = (const char*)__xuiValueObjectGetText(pField, "id", 2);
		iKind = (int)__xuiValueObjectGetInt(pField, "kind", 4);
		bRequired = __xuiValueObjectGetBool(pField, "required", 8) ? 1 : 0;
		if ( (sId == NULL) || (sId[0] == 0) ) {
			continue;
		}
		pValue = (pConfig != NULL) ? __xuiValueObjectGet(pConfig, sId, (uint32_t)strlen(sId)) : NULL;
		iValueType = xrtValueType(pValue);
		if ( bRequired && (iValueType == XVALUE_NULL || ((iValueType == XVALUE_STRING) && __xuiValueGetSize(pValue) == 0u)) ) {
			__xuiWorkflowConfigAddDiagnostic(pDiagnostics, iDiagnosticCapacity, &iDiagnostics, sId, "required", "Required config field is missing.");
			continue;
		}
		if ( iValueType == XVALUE_NULL ) {
			continue;
		}
		iExpectedType = __xuiWorkflowConfigKindToXvoType(iKind);
		if ( iExpectedType == XVALUE_FLOAT ) {
			if ( (iValueType != XVALUE_FLOAT) && (iValueType != XVALUE_INT) ) {
				__xuiWorkflowConfigAddDiagnostic(pDiagnostics, iDiagnosticCapacity, &iDiagnostics, sId, "type", "Config field has the wrong value type.");
				continue;
			}
		} else if ( iValueType != iExpectedType ) {
			__xuiWorkflowConfigAddDiagnostic(pDiagnostics, iDiagnosticCapacity, &iDiagnostics, sId, "type", "Config field has the wrong value type.");
			continue;
		}
		if ( (iKind == XUI_WORKFLOW_CONFIG_FIELD_INT) || (iKind == XUI_WORKFLOW_CONFIG_FIELD_FLOAT) ) {
			fValue = (iValueType == XVALUE_INT) ? (double)__xuiValueGetInt(pValue) : __xuiValueGetFloat(pValue);
			if ( xrtValueType(__xuiValueObjectGet(pField, "min", 3)) != XVALUE_NULL && fValue < __xuiValueObjectGetFloat(pField, "min", 3) ) {
				__xuiWorkflowConfigAddDiagnostic(pDiagnostics, iDiagnosticCapacity, &iDiagnostics, sId, "range.min", "Config field is below minimum.");
			}
			if ( xrtValueType(__xuiValueObjectGet(pField, "max", 3)) != XVALUE_NULL && fValue > __xuiValueObjectGetFloat(pField, "max", 3) ) {
				__xuiWorkflowConfigAddDiagnostic(pDiagnostics, iDiagnosticCapacity, &iDiagnostics, sId, "range.max", "Config field is above maximum.");
			}
		}
	}
	*pDiagnosticCount = iDiagnostics;
	return XUI_OK;
}

XUI_API int xuiWorkflowValidateGraph(xui_workflow pWorkflow, int* pDiagnosticCount)
{
	xui_flow_edge_info_t tEdge;
	xui_flow_node_info_t tNode;
	xui_flow_port_info_t tFromPort;
	xui_flow_port_info_t tToPort;
	xui_flow_diagnostic_desc_t tDiagnostic;
	xui_workflow_type_model_t* pType;
	xui_workflow_config_diagnostic_t arrConfigDiagnostics[16];
	xui_flow_diagnostic_desc_t arrHostDiagnostics[16];
	xvalue* pConfig;
	xvalue* pFields;
	xvalue* pField;
	xvalue* pValue;
	const char* sField;
	const char* sRef;
	int* pReachable;
	int i;
	int j;
	int iCount;
	int iNodeCount;
	int iDiagnostics;
	int iConfigDiagnostics;
	int iHostDiagnostics;
	int iStartCount;
	int iEndCount;
	int iCycleFound;
	int bChanged;
	int iKind;
	int iRet;

	if ( (pWorkflow == NULL) || (pDiagnosticCount == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pDiagnosticCount = 0;
	iRet = xuiFlowGraphClearDiagnostics(pWorkflow->pGraph);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iDiagnostics = 0;
	iStartCount = 0;
	iEndCount = 0;
	iCycleFound = 0;
	pReachable = NULL;
	iNodeCount = xuiFlowGraphGetNodeCount(pWorkflow->pGraph);
	for ( i = 0; i < iNodeCount; ++i ) {
		iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, i, &tNode);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( tNode.sType == NULL || xuiWorkflowFindNodeType(pWorkflow, tNode.sType) < 0 ) {
			memset(&tDiagnostic, 0, sizeof(tDiagnostic));
			tDiagnostic.iSize = sizeof(tDiagnostic);
			tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
			tDiagnostic.sCode = "workflow.node.missing_type";
			tDiagnostic.sMessage = "Workflow node references an unregistered node type.";
			tDiagnostic.sNode = tNode.sId;
			tDiagnostic.sPath = "nodes";
			iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
			if ( iRet != XUI_OK ) return iRet;
			iDiagnostics++;
			continue;
		}
		pType = __xuiWorkflowFindTypeModel(pWorkflow, tNode.sType);
		pConfig = NULL;
		iRet = xuiFlowGraphGetNodeConfig(pWorkflow->pGraph, tNode.sId, &pConfig);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iRet = xuiWorkflowValidateConfigEx(pWorkflow, tNode.sType, pConfig, arrConfigDiagnostics, (int)(sizeof(arrConfigDiagnostics) / sizeof(arrConfigDiagnostics[0])), &iConfigDiagnostics);
		if ( iRet != XUI_OK ) {
			if ( pConfig != NULL ) xrtValueRelease(pConfig);
			return iRet;
		}
		for ( j = 0; j < iConfigDiagnostics && j < (int)(sizeof(arrConfigDiagnostics) / sizeof(arrConfigDiagnostics[0])); ++j ) {
			memset(&tDiagnostic, 0, sizeof(tDiagnostic));
			tDiagnostic.iSize = sizeof(tDiagnostic);
			tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
			tDiagnostic.sCode = arrConfigDiagnostics[j].sCode;
			tDiagnostic.sMessage = arrConfigDiagnostics[j].sMessage;
			tDiagnostic.sNode = tNode.sId;
			tDiagnostic.sPath = arrConfigDiagnostics[j].sPath;
			iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
			if ( iRet != XUI_OK ) return iRet;
			iDiagnostics++;
		}
		pFields = (pType != NULL) ? __xuiWorkflowConfigFields(pType->pConfigSchema) : NULL;
		if ( pFields != NULL && pConfig != NULL && xrtValueType(pConfig) == XVALUE_OBJECT ) {
			iCount = xrtValueCount(pFields);
			for ( j = 0; j < iCount; ++j ) {
				pField = xrtValueArrayGet(pFields, (uint32_t)j);
				if ( xrtValueType(pField) != XVALUE_OBJECT ) {
					continue;
				}
				sField = (const char*)__xuiValueObjectGetText(pField, "id", 2);
				iKind = (int)__xuiValueObjectGetInt(pField, "kind", 4);
				if ( sField == NULL || (iKind != XUI_WORKFLOW_CONFIG_FIELD_VARIABLE_REF && iKind != XUI_WORKFLOW_CONFIG_FIELD_EXPRESSION) ) {
					continue;
				}
				pValue = __xuiValueObjectGet(pConfig, sField, (uint32_t)strlen(sField));
				if ( xrtValueType(pValue) != XVALUE_STRING || __xuiValueGetSize(pValue) == 0u ) {
					continue;
				}
				sRef = (const char*)__xuiValueGetText(pValue);
				if ( iKind == XUI_WORKFLOW_CONFIG_FIELD_EXPRESSION ) {
					if ( strstr(sRef, "${") == NULL || strchr(sRef, '}') != NULL ) {
						continue;
					}
					memset(&tDiagnostic, 0, sizeof(tDiagnostic));
					tDiagnostic.iSize = sizeof(tDiagnostic);
					tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
					tDiagnostic.sCode = "workflow.config.expression";
					tDiagnostic.sMessage = "Workflow config expression reference is not closed.";
					tDiagnostic.sNode = tNode.sId;
					tDiagnostic.sPath = sField;
					iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
					if ( iRet != XUI_OK ) return iRet;
					iDiagnostics++;
					continue;
				}
				if ( xuiWorkflowFindVariable(pWorkflow, sRef) >= 0 ) {
					continue;
				}
				memset(&tDiagnostic, 0, sizeof(tDiagnostic));
				tDiagnostic.iSize = sizeof(tDiagnostic);
				tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
				tDiagnostic.sCode = "workflow.config.variable_ref";
				tDiagnostic.sMessage = "Workflow config references a missing variable.";
				tDiagnostic.sNode = tNode.sId;
				tDiagnostic.sPath = sField;
				iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
				if ( iRet != XUI_OK ) return iRet;
				iDiagnostics++;
			}
		}
		if ( pType != NULL && pType->onValidate != NULL ) {
			memset(arrHostDiagnostics, 0, sizeof(arrHostDiagnostics));
			iHostDiagnostics = 0;
			iRet = pType->onValidate(pWorkflow, tNode.sId, tNode.sType, pConfig, arrHostDiagnostics, (int)(sizeof(arrHostDiagnostics) / sizeof(arrHostDiagnostics[0])), &iHostDiagnostics, pType->pValidateUser);
			if ( iRet != XUI_OK ) {
				if ( pConfig != NULL ) xrtValueRelease(pConfig);
				return iRet;
			}
			for ( j = 0; j < iHostDiagnostics && j < (int)(sizeof(arrHostDiagnostics) / sizeof(arrHostDiagnostics[0])); ++j ) {
				if ( arrHostDiagnostics[j].iSize < sizeof(arrHostDiagnostics[j]) ) {
					arrHostDiagnostics[j].iSize = sizeof(arrHostDiagnostics[j]);
				}
				if ( arrHostDiagnostics[j].sNode == NULL ) {
					arrHostDiagnostics[j].sNode = tNode.sId;
				}
				iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &arrHostDiagnostics[j], NULL);
				if ( iRet != XUI_OK ) return iRet;
				iDiagnostics++;
			}
		}
		if ( pConfig != NULL ) {
			xrtValueRelease(pConfig);
		}
		if ( tNode.sType != NULL && strcmp(tNode.sType, "start") == 0 ) {
			iStartCount++;
		}
		if ( tNode.sType != NULL && strcmp(tNode.sType, "end") == 0 ) {
			iEndCount++;
		}
	}
	if ( iStartCount == 0 ) {
		memset(&tDiagnostic, 0, sizeof(tDiagnostic));
		tDiagnostic.iSize = sizeof(tDiagnostic);
		tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
		tDiagnostic.sCode = "workflow.start.missing";
		tDiagnostic.sMessage = "Workflow requires at least one Start node.";
		tDiagnostic.sPath = "nodes";
		iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
		if ( iRet != XUI_OK ) return iRet;
		iDiagnostics++;
	}
	if ( iEndCount == 0 ) {
		memset(&tDiagnostic, 0, sizeof(tDiagnostic));
		tDiagnostic.iSize = sizeof(tDiagnostic);
		tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
		tDiagnostic.sCode = "workflow.end.missing";
		tDiagnostic.sMessage = "Workflow requires at least one End node.";
		tDiagnostic.sPath = "nodes";
		iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
		if ( iRet != XUI_OK ) return iRet;
		iDiagnostics++;
	}
	iCount = xuiFlowGraphGetEdgeCount(pWorkflow->pGraph);
	for ( i = 0; i < iCount; ++i ) {
		iRet = xuiFlowGraphGetEdge(pWorkflow->pGraph, i, &tEdge);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( !tEdge.bInvalid ) {
			iRet = xuiFlowGraphGetPort(pWorkflow->pGraph, tEdge.iFromNode, tEdge.iFromPort, &tFromPort);
			if ( iRet != XUI_OK ) return iRet;
			iRet = xuiFlowGraphGetPort(pWorkflow->pGraph, tEdge.iToNode, tEdge.iToPort, &tToPort);
			if ( iRet != XUI_OK ) return iRet;
			if ( tFromPort.iDirection != XUI_FLOW_PORT_OUTPUT || tToPort.iDirection != XUI_FLOW_PORT_INPUT ) {
				memset(&tDiagnostic, 0, sizeof(tDiagnostic));
				tDiagnostic.iSize = sizeof(tDiagnostic);
				tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
				tDiagnostic.sCode = "workflow.edge.direction";
				tDiagnostic.sMessage = "Workflow edge must connect an output port to an input port.";
				tDiagnostic.sEdge = tEdge.sId;
				tDiagnostic.sPath = "edges";
				iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
				if ( iRet != XUI_OK ) return iRet;
				iDiagnostics++;
			}
			continue;
		}
		memset(&tDiagnostic, 0, sizeof(tDiagnostic));
		tDiagnostic.iSize = sizeof(tDiagnostic);
		tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
		tDiagnostic.sCode = "workflow.edge.invalid_ref";
		tDiagnostic.sMessage = "Workflow edge references a missing node or port.";
		tDiagnostic.sEdge = tEdge.sId;
		tDiagnostic.sNode = (tEdge.iFromNode < 0) ? tEdge.sFromNode : ((tEdge.iToNode < 0) ? tEdge.sToNode : NULL);
		tDiagnostic.sPath = "edges";
		iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iDiagnostics++;
	}
	if ( iNodeCount > 0 && iCount > 0 ) {
		for ( i = 0; i < iCount && !iCycleFound; ++i ) {
			iRet = xuiFlowGraphGetEdge(pWorkflow->pGraph, i, &tEdge);
			if ( iRet != XUI_OK ) return iRet;
			if ( tEdge.bInvalid || tEdge.iKind != XUI_FLOW_PORT_CONTROL || tEdge.iFromNode < 0 || tEdge.iToNode < 0 ) {
				continue;
			}
			pReachable = (int*)xrtCalloc((uint32_t)iNodeCount, sizeof(int));
			if ( pReachable == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
			pReachable[tEdge.iToNode] = 1;
			do {
				bChanged = 0;
				for ( j = 0; j < iCount; ++j ) {
					xui_flow_edge_info_t tWalkEdge;
					iRet = xuiFlowGraphGetEdge(pWorkflow->pGraph, j, &tWalkEdge);
					if ( iRet != XUI_OK ) {
						xrtFree(pReachable);
						return iRet;
					}
					if ( tWalkEdge.bInvalid || tWalkEdge.iKind != XUI_FLOW_PORT_CONTROL || tWalkEdge.iFromNode < 0 || tWalkEdge.iToNode < 0 ) {
						continue;
					}
					if ( pReachable[tWalkEdge.iFromNode] && !pReachable[tWalkEdge.iToNode] ) {
						pReachable[tWalkEdge.iToNode] = 1;
						bChanged = 1;
					}
				}
			} while ( bChanged );
			if ( pReachable[tEdge.iFromNode] ) {
				memset(&tDiagnostic, 0, sizeof(tDiagnostic));
				tDiagnostic.iSize = sizeof(tDiagnostic);
				tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
				tDiagnostic.sCode = "workflow.control.cycle";
				tDiagnostic.sMessage = "Workflow control flow contains a cycle.";
				tDiagnostic.sEdge = tEdge.sId;
				tDiagnostic.sPath = "edges";
				iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
				xrtFree(pReachable);
				pReachable = NULL;
				if ( iRet != XUI_OK ) return iRet;
				iDiagnostics++;
				iCycleFound = 1;
				break;
			}
			xrtFree(pReachable);
			pReachable = NULL;
		}
	}
	if ( iStartCount > 0 && iNodeCount > 0 ) {
		pReachable = (int*)xrtCalloc((uint32_t)iNodeCount, sizeof(int));
		if ( pReachable == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		for ( i = 0; i < iNodeCount; ++i ) {
			iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, i, &tNode);
			if ( iRet != XUI_OK ) {
				xrtFree(pReachable);
				return iRet;
			}
			if ( tNode.sType != NULL && strcmp(tNode.sType, "start") == 0 ) {
				pReachable[i] = 1;
			}
		}
		do {
			bChanged = 0;
			for ( j = 0; j < iCount; ++j ) {
				iRet = xuiFlowGraphGetEdge(pWorkflow->pGraph, j, &tEdge);
				if ( iRet != XUI_OK ) {
					xrtFree(pReachable);
					return iRet;
				}
				if ( tEdge.bInvalid || tEdge.iKind != XUI_FLOW_PORT_CONTROL || tEdge.iFromNode < 0 || tEdge.iToNode < 0 ) {
					continue;
				}
				if ( pReachable[tEdge.iFromNode] && !pReachable[tEdge.iToNode] ) {
					pReachable[tEdge.iToNode] = 1;
					bChanged = 1;
				}
			}
		} while ( bChanged );
		for ( i = 0; i < iNodeCount; ++i ) {
			if ( pReachable[i] ) {
				continue;
			}
			iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, i, &tNode);
			if ( iRet != XUI_OK ) {
				xrtFree(pReachable);
				return iRet;
			}
			memset(&tDiagnostic, 0, sizeof(tDiagnostic));
			tDiagnostic.iSize = sizeof(tDiagnostic);
			tDiagnostic.iSeverity = XUI_CODE_DIAGNOSTIC_ERROR;
			tDiagnostic.sCode = "workflow.node.unreachable";
			tDiagnostic.sMessage = "Workflow node is not reachable from a Start node.";
			tDiagnostic.sNode = tNode.sId;
			tDiagnostic.sPath = "nodes";
			iRet = xuiFlowGraphAddDiagnostic(pWorkflow->pGraph, &tDiagnostic, NULL);
			if ( iRet != XUI_OK ) {
				xrtFree(pReachable);
				return iRet;
			}
			iDiagnostics++;
		}
		xrtFree(pReachable);
	}
	*pDiagnosticCount = iDiagnostics;
	return XUI_OK;
}

XUI_API int xuiWorkflowSetNodeConfig(xui_workflow pWorkflow, const char* sId, xvalue* pConfig)
{
	xui_flow_node_info_t tInfo;
	xui_workflow_type_model_t* pType;
	int iDiagnostics;
	int iRet;

	iRet = __xuiWorkflowGetNodeType(pWorkflow, sId, &tInfo);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWorkflowValidateConfig(pWorkflow, tInfo.sType, pConfig, &iDiagnostics);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iDiagnostics > 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphSetNodeConfig(pWorkflow->pGraph, sId, pConfig);
	if ( iRet == XUI_OK ) {
		pType = __xuiWorkflowFindTypeModel(pWorkflow, tInfo.sType);
		if ( pType == NULL ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iRet = __xuiWorkflowRebuildDynamicPorts(pWorkflow, pType, sId, pConfig);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowRefreshNodeSummary(pWorkflow, sId, tInfo.sType, pConfig);
	}
	return iRet;
}

XUI_API int xuiWorkflowGetNodeConfig(xui_workflow pWorkflow, const char* sId, xvalue** ppConfig)
{
	if ( pWorkflow == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiFlowGraphGetNodeConfig(pWorkflow->pGraph, sId, ppConfig);
}

XUI_API int xuiWorkflowMakeVariableId(xui_workflow pWorkflow, char* sBuffer, int iCapacity)
{
	if ( (pWorkflow == NULL) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	do {
		snprintf(sBuffer, (size_t)iCapacity, "var_%u", ++pWorkflow->iNextVariableId);
	} while ( __xuiWorkflowDictFindIndex(&pWorkflow->mapVariables, sBuffer) >= 0 );
	return XUI_OK;
}

XUI_API int xuiWorkflowAddVariable(xui_workflow pWorkflow, const xui_workflow_variable_desc_t* pDesc, int* pIndex)
{
	xui_workflow_variable_model_t* pVariable;
	uint32_t iPos;
	int iIndex;
	int iRet;

	if ( (pWorkflow == NULL) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) || (pDesc->sId == NULL) || (pDesc->sId[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiWorkflowFindVariable(pWorkflow, pDesc->sId) >= 0 ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iPos = __xuiArraySlotAdd(&pWorkflow->arrVariables, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = (int)iPos - 1;
	pVariable = (xui_workflow_variable_model_t*)__xuiArraySlotGet(&pWorkflow->arrVariables, iPos);
	memset(pVariable, 0, sizeof(*pVariable));
	pVariable->sId = __xuiWorkflowCopyString(pDesc->sId);
	pVariable->sTitle = __xuiWorkflowCopyString(pDesc->sTitle);
	pVariable->sType = __xuiWorkflowCopyString(pDesc->sType);
	pVariable->sScope = __xuiWorkflowCopyString(pDesc->sScope);
	pVariable->pDefaultValue = pDesc->pDefaultValue;
	if ( pVariable->pDefaultValue != NULL ) {
		xrtValueRetain(pVariable->pDefaultValue);
	}
	if ( pVariable->sId == NULL ||
	     (pDesc->sTitle != NULL && pVariable->sTitle == NULL) ||
	     (pDesc->sType != NULL && pVariable->sType == NULL) ||
	     (pDesc->sScope != NULL && pVariable->sScope == NULL) ) {
		__xuiWorkflowFreeVariable(pVariable);
		__xuiArraySlotRemove(&pWorkflow->arrVariables, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowDictSetIndex(&pWorkflow->mapVariables, pVariable->sId, iIndex);
	if ( iRet != XUI_OK ) {
		__xuiWorkflowFreeVariable(pVariable);
		__xuiArraySlotRemove(&pWorkflow->arrVariables, iPos, 1u);
		return iRet;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	return XUI_OK;
}

XUI_API int xuiWorkflowFindVariable(xui_workflow pWorkflow, const char* sId)
{
	if ( pWorkflow == NULL ) {
		return -1;
	}
	return __xuiWorkflowDictFindIndex(&pWorkflow->mapVariables, sId);
}

XUI_API int xuiWorkflowGetVariableCount(xui_workflow pWorkflow)
{
	return (pWorkflow != NULL) ? (int)pWorkflow->arrVariables.Count : 0;
}

XUI_API int xuiWorkflowGetVariable(xui_workflow pWorkflow, int iIndex, xui_workflow_variable_desc_t* pDesc)
{
	xui_workflow_variable_model_t* pVariable;

	if ( (pWorkflow == NULL) || (pDesc == NULL) || (iIndex < 0) || ((uint32_t)iIndex >= pWorkflow->arrVariables.Count) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pVariable = (xui_workflow_variable_model_t*)__xuiArraySlotGet(&pWorkflow->arrVariables, (uint32_t)iIndex + 1u);
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->sId = pVariable->sId;
	pDesc->sTitle = pVariable->sTitle;
	pDesc->sType = pVariable->sType;
	pDesc->sScope = pVariable->sScope;
	pDesc->pDefaultValue = pVariable->pDefaultValue;
	return XUI_OK;
}

static const char* __xuiWorkflowXsonText(xvalue* pValue)
{
	return (pValue != NULL && xrtValueType(pValue) == XVALUE_STRING) ?
		(const char*)__xuiValueGetText(pValue) : NULL;
}

static int __xuiWorkflowXsonSetText(xvalue* pTable, const char* sKey, const char* sValue)
{
	if ( sValue == NULL ) {
		return XUI_OK;
	}
	return __xuiValueObjectSetText(pTable, sKey, 0, (ptr)(void*)sValue, 0, false) ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiWorkflowXsonSetValueCopy(xvalue* pTable, const char* sKey, xvalue* pValue)
{
	xvalue* pCopy;

	if ( pValue == NULL ) {
		return XUI_OK;
	}
	pCopy = xrtValueDeepClone(pValue);
	if ( pCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xuiValueObjectSetValue(pTable, sKey, 0, pCopy, true) ) {
		xrtValueRelease(pCopy);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiWorkflowXsonSetObject(xvalue* pTable, const char* sKey, xvalue* pChild)
{
	if ( pChild == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xuiValueObjectSetValue(pTable, sKey, 0, pChild, true) ) {
		xrtValueRelease(pChild);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static int __xuiWorkflowAppendPortXValue(xvalue* pPorts, const xui_flow_port_info_t* pPort)
{
	xvalue* pObj;
	int iRet;

	pObj = xrtValueObject();
	if ( pObj == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowXsonSetText(pObj, "id", pPort->sId);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "title", pPort->sTitle);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "dataType", pPort->sDataType);
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pObj, "direction", 0, pPort->iDirection) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pObj, "kind", 0, pPort->iKind) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetBool(pObj, "required", 0, pPort->bRequired ? true : false) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetBool(pObj, "multi", 0, pPort->bMulti ? true : false) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetBool(pObj, "dynamic", 0, pPort->bDynamic ? true : false) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueArrayAppendValue(pPorts, pObj, true) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet != XUI_OK ) {
		xrtValueRelease(pObj);
	}
	return iRet;
}

static int __xuiWorkflowAppendNodeXValue(xui_workflow pWorkflow, xvalue* pNodes, xvalue* pTypeRefs, int iNode)
{
	xui_flow_node_info_t tNode;
	xui_flow_port_info_t tPort;
	xvalue* pObj;
	xvalue* pPorts;
	xvalue* pConfig;
	int i;
	int iPortCount;
	int iRet;

	memset(&tNode, 0, sizeof(tNode));
	tNode.iSize = sizeof(tNode);
	iRet = xuiFlowGraphGetNode(pWorkflow->pGraph, iNode, &tNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pObj = xrtValueObject();
	pPorts = xrtValueArray();
	if ( pObj == NULL || pPorts == NULL ) {
		if ( pObj != NULL ) xrtValueRelease(pObj);
		if ( pPorts != NULL ) xrtValueRelease(pPorts);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowXsonSetText(pObj, "id", tNode.sId);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "type", tNode.sType);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "title", tNode.sTitle);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "summary", tNode.sSummary);
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pObj, "x", 0, tNode.fX) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pObj, "y", 0, tNode.fY) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pObj, "w", 0, tNode.fW) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pObj, "h", 0, tNode.fH) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pObj, "runState", 0, tNode.iRunState) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "runPreview", tNode.sRunPreview);
	if ( iRet == XUI_OK && tNode.sType != NULL && !__xuiValueArrayAppendText(pTypeRefs, (ptr)(void*)tNode.sType, 0, false) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	iPortCount = xuiFlowGraphGetNodePortCount(pWorkflow->pGraph, iNode);
	for ( i = 0; iRet == XUI_OK && i < iPortCount; ++i ) {
		memset(&tPort, 0, sizeof(tPort));
		tPort.iSize = sizeof(tPort);
		iRet = xuiFlowGraphGetPort(pWorkflow->pGraph, iNode, i, &tPort);
		if ( iRet == XUI_OK ) {
			iRet = __xuiWorkflowAppendPortXValue(pPorts, &tPort);
		}
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pObj, "ports", pPorts);
		pPorts = NULL;
	}
	pConfig = NULL;
	if ( iRet == XUI_OK && xuiFlowGraphGetNodeConfig(pWorkflow->pGraph, tNode.sId, &pConfig) == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetValueCopy(pObj, "config", pConfig);
		xrtValueRelease(pConfig);
	}
	if ( iRet == XUI_OK && !__xuiValueArrayAppendValue(pNodes, pObj, true) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( pPorts != NULL ) xrtValueRelease(pPorts);
	if ( iRet != XUI_OK ) xrtValueRelease(pObj);
	return iRet;
}

static int __xuiWorkflowAppendEdgeXValue(xui_workflow pWorkflow, xvalue* pEdges, int iEdge)
{
	xui_flow_edge_info_t tEdge;
	xvalue* pObj;
	xvalue* pRoute;
	int iRet;

	memset(&tEdge, 0, sizeof(tEdge));
	tEdge.iSize = sizeof(tEdge);
	iRet = xuiFlowGraphGetEdge(pWorkflow->pGraph, iEdge, &tEdge);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pObj = xrtValueObject();
	if ( pObj == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pRoute = NULL;
	iRet = __xuiWorkflowXsonSetText(pObj, "id", tEdge.sId);
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pObj, "kind", 0, tEdge.iKind) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "fromNode", tEdge.sFromNode);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "fromPort", tEdge.sFromPort);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "toNode", tEdge.sToNode);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "toPort", tEdge.sToPort);
	if ( iRet == XUI_OK && (tEdge.iRouteStyle != XUI_FLOW_ROUTE_AUTO || tEdge.fRouteBias != 0.0f || tEdge.fRouteSourceOffset != 0.0f || tEdge.fRouteTargetOffset != 0.0f) ) {
		pRoute = xrtValueObject();
		if ( pRoute == NULL ) {
			iRet = XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pRoute, "style", 0, tEdge.iRouteStyle) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
		if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pRoute, "bias", 0, tEdge.fRouteBias) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
		if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pRoute, "sourceOffset", 0, tEdge.fRouteSourceOffset) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
		if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pRoute, "targetOffset", 0, tEdge.fRouteTargetOffset) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
		if ( iRet == XUI_OK ) {
			iRet = __xuiWorkflowXsonSetObject(pObj, "route", pRoute);
			pRoute = NULL;
		}
	}
	if ( iRet == XUI_OK && !__xuiValueObjectSetBool(pObj, "invalid", 0, tEdge.bInvalid ? true : false) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pObj, "runState", 0, tEdge.iRunState) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "runPreview", tEdge.sRunPreview);
	if ( iRet == XUI_OK && !__xuiValueArrayAppendValue(pEdges, pObj, true) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( pRoute != NULL ) xrtValueRelease(pRoute);
	if ( iRet != XUI_OK ) xrtValueRelease(pObj);
	return iRet;
}

static int __xuiWorkflowAppendVariableXValue(xvalue* pVariables, const xui_workflow_variable_model_t* pVariable)
{
	xvalue* pObj;
	int iRet;

	pObj = xrtValueObject();
	if ( pObj == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowXsonSetText(pObj, "id", pVariable->sId);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "title", pVariable->sTitle);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "type", pVariable->sType);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pObj, "scope", pVariable->sScope);
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetValueCopy(pObj, "default", pVariable->pDefaultValue);
	if ( iRet == XUI_OK && !__xuiValueArrayAppendValue(pVariables, pObj, true) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet != XUI_OK ) xrtValueRelease(pObj);
	return iRet;
}

XUI_API int xuiWorkflowToXValue(xui_workflow pWorkflow, xvalue** ppValue)
{
	xui_flow_viewport_t tViewport;
	xui_workflow_variable_model_t* pVariable;
	xvalue* pRoot;
	xvalue* pMeta;
	xvalue* pTypes;
	xvalue* pVariables;
	xvalue* pNodes;
	xvalue* pEdges;
	xvalue* pViewport;
	uint32_t i;
	int iRet;

	if ( (pWorkflow == NULL) || (ppValue == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppValue = NULL;
	pRoot = xrtValueObject();
	pMeta = xrtValueObject();
	pTypes = xrtValueArray();
	pVariables = xrtValueArray();
	pNodes = xrtValueArray();
	pEdges = xrtValueArray();
	pViewport = xrtValueObject();
	if ( pRoot == NULL || pMeta == NULL || pTypes == NULL || pVariables == NULL || pNodes == NULL || pEdges == NULL || pViewport == NULL ) {
		if ( pRoot != NULL ) xrtValueRelease(pRoot);
		if ( pMeta != NULL ) xrtValueRelease(pMeta);
		if ( pTypes != NULL ) xrtValueRelease(pTypes);
		if ( pVariables != NULL ) xrtValueRelease(pVariables);
		if ( pNodes != NULL ) xrtValueRelease(pNodes);
		if ( pEdges != NULL ) xrtValueRelease(pEdges);
		if ( pViewport != NULL ) xrtValueRelease(pViewport);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xuiWorkflowXsonSetText(pRoot, "kind", "xui.workflow");
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pRoot, "version", 0, 1) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) iRet = __xuiWorkflowXsonSetText(pMeta, "format", "xui.workflow.v1");
	if ( iRet == XUI_OK && !__xuiValueObjectSetInt(pMeta, "schemaVersion", 0, 1) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	for ( i = 1u; iRet == XUI_OK && i <= pWorkflow->arrVariables.Count; ++i ) {
		pVariable = (xui_workflow_variable_model_t*)__xuiArraySlotGet(&pWorkflow->arrVariables, i);
		iRet = __xuiWorkflowAppendVariableXValue(pVariables, pVariable);
	}
	for ( i = 0u; iRet == XUI_OK && i < (uint32_t)xuiFlowGraphGetNodeCount(pWorkflow->pGraph); ++i ) {
		iRet = __xuiWorkflowAppendNodeXValue(pWorkflow, pNodes, pTypes, (int)i);
	}
	for ( i = 0u; iRet == XUI_OK && i < (uint32_t)xuiFlowGraphGetEdgeCount(pWorkflow->pGraph); ++i ) {
		iRet = __xuiWorkflowAppendEdgeXValue(pWorkflow, pEdges, (int)i);
	}
	memset(&tViewport, 0, sizeof(tViewport));
	tViewport.iSize = sizeof(tViewport);
	if ( iRet == XUI_OK ) iRet = xuiFlowGraphGetViewport(pWorkflow->pGraph, &tViewport);
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pViewport, "panX", 0, tViewport.fPanX) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pViewport, "panY", 0, tViewport.fPanY) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pViewport, "zoom", 0, tViewport.fZoom) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pViewport, "width", 0, tViewport.fWidth) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK && !__xuiValueObjectSetFloat(pViewport, "height", 0, tViewport.fHeight) ) iRet = XUI_ERROR_OUT_OF_MEMORY;
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "metadata", pMeta);
		pMeta = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "nodeTypes", pTypes);
		pTypes = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "variables", pVariables);
		pVariables = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "nodes", pNodes);
		pNodes = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "edges", pEdges);
		pEdges = NULL;
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiWorkflowXsonSetObject(pRoot, "viewport", pViewport);
		pViewport = NULL;
	}
	if ( pMeta != NULL ) xrtValueRelease(pMeta);
	if ( pTypes != NULL ) xrtValueRelease(pTypes);
	if ( pVariables != NULL ) xrtValueRelease(pVariables);
	if ( pNodes != NULL ) xrtValueRelease(pNodes);
	if ( pEdges != NULL ) xrtValueRelease(pEdges);
	if ( pViewport != NULL ) xrtValueRelease(pViewport);
	if ( iRet != XUI_OK ) {
		xrtValueRelease(pRoot);
		return iRet;
	}
	*ppValue = pRoot;
	return XUI_OK;
}

static int __xuiWorkflowLoadPortXValue(xui_flow_graph pGraph, int iNode, xvalue* pObj)
{
	xui_flow_port_desc_t tPort;

	if ( pObj == NULL || xrtValueType(pObj) != XVALUE_OBJECT ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPort, 0, sizeof(tPort));
	tPort.iSize = sizeof(tPort);
	tPort.sId = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "id", 0));
	tPort.sTitle = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "title", 0));
	tPort.sDataType = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "dataType", 0));
	tPort.iDirection = (int)__xuiValueGetInt(__xuiValueObjectGet(pObj, "direction", 0));
	tPort.iKind = (int)__xuiValueGetInt(__xuiValueObjectGet(pObj, "kind", 0));
	tPort.bRequired = __xuiValueGetBool(__xuiValueObjectGet(pObj, "required", 0)) ? 1 : 0;
	tPort.bMulti = __xuiValueGetBool(__xuiValueObjectGet(pObj, "multi", 0)) ? 1 : 0;
	tPort.bDynamic = __xuiValueGetBool(__xuiValueObjectGet(pObj, "dynamic", 0)) ? 1 : 0;
	return xuiFlowGraphAddPort(pGraph, iNode, &tPort, NULL);
}

static int __xuiWorkflowLoadNodeXValue(xui_flow_graph pGraph, xvalue* pObj)
{
	xui_flow_node_desc_t tNode;
	xvalue* pPorts;
	xvalue* pConfig;
	uint32_t i;
	int iNode;
	int iRet;

	if ( pObj == NULL || xrtValueType(pObj) != XVALUE_OBJECT ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tNode, 0, sizeof(tNode));
	tNode.iSize = sizeof(tNode);
	tNode.sId = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "id", 0));
	tNode.sType = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "type", 0));
	tNode.sTitle = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "title", 0));
	tNode.sSummary = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "summary", 0));
	tNode.fX = (float)__xuiValueGetFloat(__xuiValueObjectGet(pObj, "x", 0));
	tNode.fY = (float)__xuiValueGetFloat(__xuiValueObjectGet(pObj, "y", 0));
	tNode.fW = (float)__xuiValueGetFloat(__xuiValueObjectGet(pObj, "w", 0));
	tNode.fH = (float)__xuiValueGetFloat(__xuiValueObjectGet(pObj, "h", 0));
	iRet = xuiFlowGraphAddNode(pGraph, &tNode, &iNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pPorts = __xuiValueObjectGet(pObj, "ports", 0);
	if ( pPorts != NULL && xrtValueType(pPorts) == XVALUE_ARRAY ) {
		for ( i = 0u; i < xrtValueCount(pPorts); ++i ) {
			iRet = __xuiWorkflowLoadPortXValue(pGraph, iNode, xrtValueArrayGet(pPorts, i));
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	pConfig = __xuiValueObjectGet(pObj, "config", 0);
	if ( pConfig != NULL && xrtValueType(pConfig) != XVALUE_NULL ) {
		iRet = xuiFlowGraphSetNodeConfig(pGraph, tNode.sId, pConfig);
	}
	if ( iRet == XUI_OK ) {
		xvalue* pRunState = __xuiValueObjectGet(pObj, "runState", 0);
		if ( pRunState != NULL && xrtValueType(pRunState) != XVALUE_NULL ) {
			iRet = xuiFlowGraphSetNodeRunState(pGraph, tNode.sId, (int)__xuiValueGetInt(pRunState), __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "runPreview", 0)));
		}
	}
	return iRet;
}

static int __xuiWorkflowLoadEdgeXValue(xui_flow_graph pGraph, xvalue* pObj)
{
	xui_flow_edge_desc_t tEdge;
	xvalue* pRoute;

	if ( pObj == NULL || xrtValueType(pObj) != XVALUE_OBJECT ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEdge, 0, sizeof(tEdge));
	tEdge.iSize = sizeof(tEdge);
	tEdge.sId = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "id", 0));
	tEdge.iKind = (int)__xuiValueGetInt(__xuiValueObjectGet(pObj, "kind", 0));
	tEdge.sFromNode = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "fromNode", 0));
	tEdge.sFromPort = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "fromPort", 0));
	tEdge.sToNode = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "toNode", 0));
	tEdge.sToPort = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "toPort", 0));
	pRoute = __xuiValueObjectGet(pObj, "route", 0);
	if ( pRoute != NULL && xrtValueType(pRoute) == XVALUE_OBJECT ) {
		tEdge.iRouteStyle = (int)__xuiValueGetInt(__xuiValueObjectGet(pRoute, "style", 0));
		tEdge.fRouteBias = (float)__xuiValueGetFloat(__xuiValueObjectGet(pRoute, "bias", 0));
		tEdge.fRouteSourceOffset = (float)__xuiValueGetFloat(__xuiValueObjectGet(pRoute, "sourceOffset", 0));
		tEdge.fRouteTargetOffset = (float)__xuiValueGetFloat(__xuiValueObjectGet(pRoute, "targetOffset", 0));
	}
	{
		int iRet = xuiFlowGraphAddEdgePreserveInvalid(pGraph, &tEdge, NULL);
		xvalue* pRunState = __xuiValueObjectGet(pObj, "runState", 0);
		if ( iRet == XUI_OK && pRunState != NULL &&
			 xrtValueType(pRunState) != XVALUE_NULL ) {
			iRet = xuiFlowGraphSetEdgeRunState(pGraph, tEdge.sId, (int)__xuiValueGetInt(pRunState), __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "runPreview", 0)));
		}
		return iRet;
	}
}

static int __xuiWorkflowLoadVariableXValue(xui_workflow pWorkflow, xvalue* pObj)
{
	xui_workflow_variable_desc_t tVariable;
	xvalue* pDefault;

	if ( pObj == NULL || xrtValueType(pObj) != XVALUE_OBJECT ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tVariable, 0, sizeof(tVariable));
	tVariable.iSize = sizeof(tVariable);
	tVariable.sId = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "id", 0));
	tVariable.sTitle = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "title", 0));
	tVariable.sType = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "type", 0));
	tVariable.sScope = __xuiWorkflowXsonText(__xuiValueObjectGet(pObj, "scope", 0));
	pDefault = __xuiValueObjectGet(pObj, "default", 0);
	if ( pDefault != NULL && xrtValueType(pDefault) != XVALUE_NULL ) {
		tVariable.pDefaultValue = pDefault;
	}
	return xuiWorkflowAddVariable(pWorkflow, &tVariable, NULL);
}

XUI_API int xuiWorkflowLoadXValue(xui_workflow pWorkflow, xvalue* pValue)
{
	xui_flow_graph pGraph;
	xvalue* pNodes;
	xvalue* pEdges;
	xvalue* pVariables;
	xvalue* pViewportValue;
	xui_flow_viewport_t tViewport;
	uint32_t i;
	int iRet;

	if ( (pWorkflow == NULL) || (pValue == NULL) ||
		 (xrtValueType(pValue) != XVALUE_OBJECT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pNodes = __xuiValueObjectGet(pValue, "nodes", 0);
	pEdges = __xuiValueObjectGet(pValue, "edges", 0);
	pVariables = __xuiValueObjectGet(pValue, "variables", 0);
	if ( (pNodes == NULL) || (xrtValueType(pNodes) != XVALUE_ARRAY) ||
		 (pEdges == NULL) || (xrtValueType(pEdges) != XVALUE_ARRAY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphCreate(&pGraph);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 0u; i < xrtValueCount(pNodes); ++i ) {
		iRet = __xuiWorkflowLoadNodeXValue(pGraph, xrtValueArrayGet(pNodes, i));
		if ( iRet != XUI_OK ) {
			xuiFlowGraphDestroy(pGraph);
			return iRet;
		}
	}
	for ( i = 0u; i < xrtValueCount(pEdges); ++i ) {
		iRet = __xuiWorkflowLoadEdgeXValue(pGraph, xrtValueArrayGet(pEdges, i));
		if ( iRet != XUI_OK ) {
			xuiFlowGraphDestroy(pGraph);
			return iRet;
		}
	}
	pViewportValue = __xuiValueObjectGet(pValue, "viewport", 0);
	if ( pViewportValue != NULL && xrtValueType(pViewportValue) == XVALUE_OBJECT ) {
		memset(&tViewport, 0, sizeof(tViewport));
		tViewport.iSize = sizeof(tViewport);
		tViewport.fPanX = (float)__xuiValueGetFloat(__xuiValueObjectGet(pViewportValue, "panX", 0));
		tViewport.fPanY = (float)__xuiValueGetFloat(__xuiValueObjectGet(pViewportValue, "panY", 0));
		tViewport.fZoom = (float)__xuiValueGetFloat(__xuiValueObjectGet(pViewportValue, "zoom", 0));
		tViewport.fWidth = (float)__xuiValueGetFloat(__xuiValueObjectGet(pViewportValue, "width", 0));
		tViewport.fHeight = (float)__xuiValueGetFloat(__xuiValueObjectGet(pViewportValue, "height", 0));
		iRet = xuiFlowGraphSetViewport(pGraph, &tViewport);
		if ( iRet != XUI_OK ) {
			xuiFlowGraphDestroy(pGraph);
			return iRet;
		}
	}
	xuiFlowGraphDestroy(pWorkflow->pGraph);
	pWorkflow->pGraph = pGraph;
	__xuiWorkflowClearVariables(pWorkflow);
	if ( pVariables != NULL && xrtValueType(pVariables) == XVALUE_ARRAY ) {
		for ( i = 0u; i < xrtValueCount(pVariables); ++i ) {
			iRet = __xuiWorkflowLoadVariableXValue(pWorkflow, xrtValueArrayGet(pVariables, i));
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

XUI_API int xuiWorkflowSaveXSONFile(xui_workflow pWorkflow, const char* sPath)
{
	xvalue* pValue;
	int iRet;

	if ( (pWorkflow == NULL) || (sPath == NULL) || (sPath[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWorkflowToXValue(pWorkflow, &pValue);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xrtXsonStringifyFile(sPath, pValue, true) ?
		XUI_OK : XUI_ERROR_RESOURCE_FAILED;
	xrtValueRelease(pValue);
	return iRet;
}

XUI_API int xuiWorkflowLoadXSONFile(xui_workflow pWorkflow, const char* sPath)
{
	xvalue* pValue;
	int iRet;

	if ( (pWorkflow == NULL) || (sPath == NULL) || (sPath[0] == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pValue = xrtXsonParseFile(sPath);
	if ( pValue == NULL || xrtValueType(pValue) == XVALUE_NULL ) {
		if ( pValue != NULL ) xrtValueRelease(pValue);
		return XUI_ERROR_RESOURCE_FAILED;
	}
	iRet = xuiWorkflowLoadXValue(pWorkflow, pValue);
	xrtValueRelease(pValue);
	return iRet;
}

static int __xuiWorkflowNodeRunStateValid(int iState)
{
	return (iState >= XUI_WORKFLOW_NODE_RUN_IDLE && iState <= XUI_WORKFLOW_NODE_RUN_WARNING);
}

static int __xuiWorkflowEdgeRunStateValid(int iState)
{
	return (iState >= XUI_WORKFLOW_EDGE_RUN_IDLE && iState <= XUI_WORKFLOW_EDGE_RUN_INVALID);
}

static int __xuiWorkflowFindNodeRunState(xui_workflow pWorkflow, const char* sNodeId)
{
	xui_workflow_node_run_state_model_t* pState;
	uint32_t i;

	if ( (pWorkflow == NULL) || (sNodeId == NULL) || (sNodeId[0] == 0) ) {
		return -1;
	}
	for ( i = 1u; i <= pWorkflow->tRuntime.arrNodeStates.Count; ++i ) {
		pState = (xui_workflow_node_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrNodeStates, i);
		if ( pState->sNodeId != NULL && strcmp(pState->sNodeId, sNodeId) == 0 ) {
			return (int)i - 1;
		}
	}
	return -1;
}

static int __xuiWorkflowFindEdgeRunState(xui_workflow pWorkflow, const char* sEdgeId)
{
	xui_workflow_edge_run_state_model_t* pState;
	uint32_t i;

	if ( (pWorkflow == NULL) || (sEdgeId == NULL) || (sEdgeId[0] == 0) ) {
		return -1;
	}
	for ( i = 1u; i <= pWorkflow->tRuntime.arrEdgeStates.Count; ++i ) {
		pState = (xui_workflow_edge_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrEdgeStates, i);
		if ( pState->sEdgeId != NULL && strcmp(pState->sEdgeId, sEdgeId) == 0 ) {
			return (int)i - 1;
		}
	}
	return -1;
}

XUI_API int xuiWorkflowSetNodeRunState(xui_workflow pWorkflow, const xui_workflow_node_run_state_t* pState)
{
	xui_workflow_node_run_state_model_t* pModel;
	char* sPreview;
	uint32_t iPos;
	int iIndex;

	if ( (pWorkflow == NULL) || (pState == NULL) || (pState->iSize < sizeof(*pState)) ||
	     (pState->sNodeId == NULL) || (pState->sNodeId[0] == 0) || !__xuiWorkflowNodeRunStateValid(pState->iState) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiFlowGraphFindNode(pWorkflow->pGraph, pState->sNodeId) < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sPreview = __xuiWorkflowCopyString(pState->sPreview);
	if ( pState->sPreview != NULL && sPreview == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = __xuiWorkflowFindNodeRunState(pWorkflow, pState->sNodeId);
	if ( iIndex >= 0 ) {
		pModel = (xui_workflow_node_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrNodeStates, (uint32_t)iIndex + 1u);
		xrtFree(pModel->sPreview);
		pModel->sPreview = sPreview;
		pModel->iState = pState->iState;
		return xuiFlowGraphSetNodeRunState(pWorkflow->pGraph, pState->sNodeId, pState->iState, pState->sPreview);
	}
	iPos = __xuiArraySlotAdd(&pWorkflow->tRuntime.arrNodeStates, 1u);
	if ( iPos == 0u ) {
		xrtFree(sPreview);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pModel = (xui_workflow_node_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrNodeStates, iPos);
	memset(pModel, 0, sizeof(*pModel));
	pModel->sNodeId = __xuiWorkflowCopyString(pState->sNodeId);
	pModel->sPreview = sPreview;
	pModel->iState = pState->iState;
	if ( pModel->sNodeId == NULL ) {
		__xuiWorkflowFreeNodeRunState(pModel);
		__xuiArraySlotRemove(&pWorkflow->tRuntime.arrNodeStates, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return xuiFlowGraphSetNodeRunState(pWorkflow->pGraph, pState->sNodeId, pState->iState, pState->sPreview);
}

XUI_API int xuiWorkflowGetNodeRunState(xui_workflow pWorkflow, const char* sNodeId, xui_workflow_node_run_state_t* pState)
{
	xui_workflow_node_run_state_model_t* pModel;
	int iIndex;

	if ( (pWorkflow == NULL) || (sNodeId == NULL) || (sNodeId[0] == 0) || (pState == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiWorkflowFindNodeRunState(pWorkflow, sNodeId);
	memset(pState, 0, sizeof(*pState));
	pState->iSize = sizeof(*pState);
	pState->sNodeId = sNodeId;
	if ( iIndex < 0 ) {
		pState->iState = XUI_WORKFLOW_NODE_RUN_IDLE;
		return XUI_OK;
	}
	pModel = (xui_workflow_node_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrNodeStates, (uint32_t)iIndex + 1u);
	pState->sNodeId = pModel->sNodeId;
	pState->iState = pModel->iState;
	pState->sPreview = pModel->sPreview;
	return XUI_OK;
}

XUI_API int xuiWorkflowSetEdgeRunState(xui_workflow pWorkflow, const xui_workflow_edge_run_state_t* pState)
{
	xui_workflow_edge_run_state_model_t* pModel;
	char* sPreview;
	uint32_t iPos;
	int iIndex;

	if ( (pWorkflow == NULL) || (pState == NULL) || (pState->iSize < sizeof(*pState)) ||
	     (pState->sEdgeId == NULL) || (pState->sEdgeId[0] == 0) || !__xuiWorkflowEdgeRunStateValid(pState->iState) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( xuiFlowGraphFindEdge(pWorkflow->pGraph, pState->sEdgeId) < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sPreview = __xuiWorkflowCopyString(pState->sPreview);
	if ( pState->sPreview != NULL && sPreview == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iIndex = __xuiWorkflowFindEdgeRunState(pWorkflow, pState->sEdgeId);
	if ( iIndex >= 0 ) {
		pModel = (xui_workflow_edge_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrEdgeStates, (uint32_t)iIndex + 1u);
		xrtFree(pModel->sPreview);
		pModel->sPreview = sPreview;
		pModel->iState = pState->iState;
		return xuiFlowGraphSetEdgeRunState(pWorkflow->pGraph, pState->sEdgeId, pState->iState, pState->sPreview);
	}
	iPos = __xuiArraySlotAdd(&pWorkflow->tRuntime.arrEdgeStates, 1u);
	if ( iPos == 0u ) {
		xrtFree(sPreview);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pModel = (xui_workflow_edge_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrEdgeStates, iPos);
	memset(pModel, 0, sizeof(*pModel));
	pModel->sEdgeId = __xuiWorkflowCopyString(pState->sEdgeId);
	pModel->sPreview = sPreview;
	pModel->iState = pState->iState;
	if ( pModel->sEdgeId == NULL ) {
		__xuiWorkflowFreeEdgeRunState(pModel);
		__xuiArraySlotRemove(&pWorkflow->tRuntime.arrEdgeStates, iPos, 1u);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return xuiFlowGraphSetEdgeRunState(pWorkflow->pGraph, pState->sEdgeId, pState->iState, pState->sPreview);
}

XUI_API int xuiWorkflowGetEdgeRunState(xui_workflow pWorkflow, const char* sEdgeId, xui_workflow_edge_run_state_t* pState)
{
	xui_workflow_edge_run_state_model_t* pModel;
	int iIndex;

	if ( (pWorkflow == NULL) || (sEdgeId == NULL) || (sEdgeId[0] == 0) || (pState == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiWorkflowFindEdgeRunState(pWorkflow, sEdgeId);
	memset(pState, 0, sizeof(*pState));
	pState->iSize = sizeof(*pState);
	pState->sEdgeId = sEdgeId;
	if ( iIndex < 0 ) {
		pState->iState = XUI_WORKFLOW_EDGE_RUN_IDLE;
		return XUI_OK;
	}
	pModel = (xui_workflow_edge_run_state_model_t*)__xuiArraySlotGet(&pWorkflow->tRuntime.arrEdgeStates, (uint32_t)iIndex + 1u);
	pState->sEdgeId = pModel->sEdgeId;
	pState->iState = pModel->iState;
	pState->sPreview = pModel->sPreview;
	return XUI_OK;
}
