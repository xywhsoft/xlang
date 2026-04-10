#include "xl_resolve.h"

typedef struct XL_ResolveContext {
	XL_DiagContext* pDiag;
	XL_SemResult* pResult;
	int bTraceSem;
	int iCurrentScopeId;
	int iLoopDepth;
	int iAsyncDepth;
} XL_ResolveContext;

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
}

static const char* __xlArgName(const XL_AstNode* pNode)
{
	if ( (pNode == NULL) || (pNode->eKind != XL_AST_NAMED_ARG) ) {
		return NULL;
	}

	return pNode->sText;
}

static const XL_AstNode* __xlArgValue(const XL_AstNode* pNode)
{
	if ( pNode == NULL ) {
		return NULL;
	}
	if ( pNode->eKind == XL_AST_NAMED_ARG ) {
		return __xlAstChild(pNode, 0);
	}

	return pNode;
}

static const XL_AstNode* __xlParamTypeNode(const XL_AstNode* pNode)
{
	return __xlAstChild(pNode, 0);
}

static const XL_AstNode* __xlParamDefaultNode(const XL_AstNode* pNode)
{
	return __xlAstChild(pNode, 1);
}

static int __xlIsStaticDefaultExpr(const XL_AstNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return 1;
	}

	switch ( pNode->eKind ) {
		case XL_AST_INT:
		case XL_AST_FLOAT:
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
		case XL_AST_BOOL:
		case XL_AST_NULL:
		case XL_AST_TIME:
			return 1;
		case XL_AST_UNARY:
		case XL_AST_BINARY:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				if ( !__xlIsStaticDefaultExpr(pNode->ppChildren[i]) ) {
					return 0;
				}
			}
			return 1;
		default:
			return 0;
	}
}

static const XL_AstNode* __xlBuiltinDefaultIntZero(void)
{
	static XL_AstNode* pNode = NULL;

	if ( pNode == NULL ) {
		pNode = xlAstCreateNode(XL_AST_INT, NULL);
		if ( pNode != NULL ) {
			pNode->iIntValue = 0;
			xlAstSetText(pNode, "0", 1);
		}
	}
	return pNode;
}

static const XL_AstNode* __xlBuiltinDefaultEmptyString(void)
{
	static XL_AstNode* pNode = NULL;

	if ( pNode == NULL ) {
		pNode = xlAstCreateNode(XL_AST_STRING, NULL);
		if ( pNode != NULL ) {
			xlAstSetText(pNode, "", 0);
		}
	}
	return pNode;
}

static const XL_AstNode* __xlBuiltinDefaultNull(void)
{
	static XL_AstNode* pNode = NULL;

	if ( pNode == NULL ) {
		pNode = xlAstCreateNode(XL_AST_NULL, NULL);
		if ( pNode != NULL ) {
			xlAstSetText(pNode, "null", 4);
		}
	}
	return pNode;
}

static const char* __xlErrorFieldTypeName(const char* sFieldName)
{
	if ( sFieldName == NULL ) {
		return NULL;
	}
	if ( (strcmp(sFieldName, "type") == 0) || (strcmp(sFieldName, "message") == 0) ||
		(strcmp(sFieldName, "data") == 0) || (strcmp(sFieldName, "stack") == 0) ) {
		return "string";
	}
	if ( strcmp(sFieldName, "code") == 0 ) {
		return "int";
	}
	if ( strcmp(sFieldName, "cause") == 0 ) {
		return "error";
	}
	return NULL;
}

static int __xlTypeMatches(const char* sExpectedType, const char* sActualType)
{
	if ( (sExpectedType == NULL) || (sExpectedType[0] == '\0') ) {
		return 1;
	}
	if ( (sActualType == NULL) || (sActualType[0] == '\0') ) {
		return 1;
	}
	if ( strcmp(sExpectedType, sActualType) == 0 ) {
		return 1;
	}
	if ( strcmp(sExpectedType, "any") == 0 ) {
		return 1;
	}
	if ( (strcmp(sActualType, "null") == 0) && (strcmp(sExpectedType, "error") == 0) ) {
		return 1;
	}
	if ( ((strcmp(sExpectedType, "string") == 0) && (strcmp(sActualType, "error") == 0)) ||
		((strcmp(sExpectedType, "error") == 0) && (strcmp(sActualType, "string") == 0)) ) {
		return 1;
	}

	return 0;
}

static int __xlNodeHasYield(const XL_AstNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return 0;
	}
	if ( pNode->eKind == XL_AST_YIELD ) {
		return 1;
	}
	if ( pNode->eKind == XL_AST_FUNC_DECL ) {
		return 0;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlNodeHasYield(pNode->ppChildren[i]) ) {
			return 1;
		}
	}

	return 0;
}

static int __xlNodeHasFinallyEscape(const XL_AstNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return 0;
	}
	if ( pNode->eKind == XL_AST_FUNC_DECL ) {
		return 0;
	}
	if ( (pNode->eKind == XL_AST_RETURN) || (pNode->eKind == XL_AST_BREAK) || (pNode->eKind == XL_AST_CONTINUE) ) {
		return 1;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlNodeHasFinallyEscape(pNode->ppChildren[i]) ) {
			return 1;
		}
	}

	return 0;
}

static char* __xlSemDupText(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

static int __xlSemNodeGrow(XL_SemNodeTable* pTable)
{
	XL_SemNodeInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 32 : (pTable->iCapacity * 2);
	pNewItems = (XL_SemNodeInfo*)realloc(pTable->pItems, sizeof(XL_SemNodeInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

static XL_SemNodeInfo* __xlSemNodeEnsure(XL_SemResult* pResult, const XL_AstNode* pNode)
{
	size_t i;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pResult->nodes.iCount; ++i ) {
		if ( pResult->nodes.pItems[i].pNode == pNode ) {
			return &pResult->nodes.pItems[i];
		}
	}

	if ( __xlSemNodeGrow(&pResult->nodes) != XL_RC_OK ) {
		return NULL;
	}

	memset(&pResult->nodes.pItems[pResult->nodes.iCount], 0, sizeof(pResult->nodes.pItems[pResult->nodes.iCount]));
	pResult->nodes.pItems[pResult->nodes.iCount].pNode = pNode;
	pResult->nodes.pItems[pResult->nodes.iCount].iScopeId = -1;
	pResult->nodes.pItems[pResult->nodes.iCount].iSymbolIndex = -1;
	pResult->nodes.pItems[pResult->nodes.iCount].iFuncIndex = -1;
	return &pResult->nodes.pItems[pResult->nodes.iCount++];
}

static void __xlSemNodeSetType(XL_SemResult* pResult, const XL_AstNode* pNode, const char* sTypeName)
{
	XL_SemNodeInfo* pInfo;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return;
	}

	pInfo = __xlSemNodeEnsure(pResult, pNode);
	if ( pInfo == NULL ) {
		return;
	}

	free(pInfo->sTypeName);
	pInfo->sTypeName = __xlSemDupText((sTypeName != NULL) ? sTypeName : "");
}

static void __xlSemNodeSetScope(XL_SemResult* pResult, const XL_AstNode* pNode, int iScopeId)
{
	XL_SemNodeInfo* pInfo;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return;
	}

	pInfo = __xlSemNodeEnsure(pResult, pNode);
	if ( pInfo != NULL ) {
		pInfo->iScopeId = iScopeId;
	}
}

static void __xlSemNodeSetSymbol(XL_SemResult* pResult, const XL_AstNode* pNode, int iSymbolIndex)
{
	XL_SemNodeInfo* pInfo;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return;
	}

	pInfo = __xlSemNodeEnsure(pResult, pNode);
	if ( pInfo != NULL ) {
		pInfo->iSymbolIndex = iSymbolIndex;
	}
}

static void __xlSemNodeSetFunc(XL_SemResult* pResult, const XL_AstNode* pNode, int iFuncIndex)
{
	XL_SemNodeInfo* pInfo;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return;
	}

	pInfo = __xlSemNodeEnsure(pResult, pNode);
	if ( pInfo != NULL ) {
		pInfo->iFuncIndex = iFuncIndex;
	}
}

static int __xlSemDiagGrow(XL_SemDiagList* pList)
{
	XL_SemDiagItem* pNewItems;
	size_t iNewCapacity;

	if ( pList == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pList->iCount < pList->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pList->iCapacity == 0) ? 8 : (pList->iCapacity * 2);
	pNewItems = (XL_SemDiagItem*)realloc(pList->pItems, sizeof(XL_SemDiagItem) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pList->pItems = pNewItems;
	pList->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

const char* xlSemDiagLevelName(XL_SemDiagLevel eLevel)
{
	switch ( eLevel ) {
		case XL_SEM_DIAG_INFO:
			return "info";
		case XL_SEM_DIAG_WARN:
			return "warn";
		case XL_SEM_DIAG_ERROR:
			return "error";
		default:
			return "unknown";
	}
}

const XL_SemNodeInfo* xlSemNodeInfoFind(const XL_SemResult* pResult, const XL_AstNode* pNode)
{
	size_t i;

	if ( (pResult == NULL) || (pNode == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pResult->nodes.iCount; ++i ) {
		if ( pResult->nodes.pItems[i].pNode == pNode ) {
			return &pResult->nodes.pItems[i];
		}
	}

	return NULL;
}

static int __xlSemAddDiag(
	XL_ResolveContext* pCtx,
	XL_SemDiagLevel eLevel,
	const XL_AstNode* pNode,
	const char* sFormat,
	...
)
{
	XL_SemDiagItem* pItem;
	va_list args;
	va_list args2;
	int iNeed;
	char* sMessage;
	int iLine;
	int iColumn;

	if ( (pCtx == NULL) || (pCtx->pResult == NULL) || (sFormat == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlSemDiagGrow(&pCtx->pResult->diags) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return XL_RC_FAIL;
	}

	sMessage = (char*)malloc((size_t)iNeed + 1);
	if ( sMessage == NULL ) {
		va_end(args2);
		return XL_RC_IO_ERROR;
	}

	vsnprintf(sMessage, (size_t)iNeed + 1, sFormat, args2);
	va_end(args2);

	pItem = &pCtx->pResult->diags.pItems[pCtx->pResult->diags.iCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->eLevel = eLevel;
	pItem->sMessage = sMessage;
	pItem->iLine = (pNode != NULL) ? pNode->iLine : 0;
	pItem->iColumn = (pNode != NULL) ? pNode->iColumn : 0;

	if ( eLevel == XL_SEM_DIAG_ERROR ) {
		pCtx->pResult->bHadError = 1;
	}

	iLine = pItem->iLine;
	iColumn = pItem->iColumn;
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmit(
			pCtx->pDiag,
			(eLevel == XL_SEM_DIAG_ERROR) ? XL_DIAG_LEVEL_ERROR : ((eLevel == XL_SEM_DIAG_WARN) ? XL_DIAG_LEVEL_WARN : XL_DIAG_LEVEL_INFO),
			"sem",
			__FILE__,
			__LINE__,
			"%d:%d: %s",
			iLine,
			iColumn,
			sMessage
		);
	}

	return XL_RC_OK;
}

static void __xlSemTrace(XL_ResolveContext* pCtx, const XL_AstNode* pNode, const char* sFormat, ...)
{
	va_list args;
	char sText[XL_DIAG_TEXT_MAX];
	int iOffset;

	if ( (pCtx == NULL) || !pCtx->bTraceSem || (pCtx->pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}

	iOffset = snprintf(
		sText,
		sizeof(sText),
		"scope=%d node=%s@%d:%d ",
		pCtx->iCurrentScopeId,
		(pNode != NULL) ? xlAstKindName(pNode->eKind) : "none",
		(pNode != NULL) ? pNode->iLine : 0,
		(pNode != NULL) ? pNode->iColumn : 0
	);
	if ( (iOffset < 0) || ((size_t)iOffset >= sizeof(sText)) ) {
		return;
	}

	va_start(args, sFormat);
	vsnprintf(sText + iOffset, sizeof(sText) - (size_t)iOffset, sFormat, args);
	va_end(args);

	xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_INFO, "sem", __FILE__, __LINE__, "%s", sText);
}

static int __xlScopeContains(const XL_ScopeTable* pScopes, int iCurrentScopeId, int iTargetScopeId)
{
	const XL_ScopeInfo* pScope;

	while ( iCurrentScopeId >= 0 ) {
		if ( iCurrentScopeId == iTargetScopeId ) {
			return 1;
		}

		pScope = xlScopeTableGet(pScopes, iCurrentScopeId);
		if ( pScope == NULL ) {
			break;
		}
		iCurrentScopeId = pScope->iParentId;
	}

	return 0;
}

static int __xlSymbolCanShareName(XL_SymbolKind eExisting, XL_SymbolKind eNew)
{
	return (eExisting == XL_SYMBOL_FUNC) && (eNew == XL_SYMBOL_FUNC);
}

static int __xlFindConflictingSymbolInScope(
	const XL_SymbolTable* pTable,
	int iScopeId,
	const char* sName,
	XL_SymbolKind eKind
)
{
	size_t i;

	if ( (pTable == NULL) || (sName == NULL) ) {
		return -1;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		if ( (pTable->pItems[i].iScopeId != iScopeId) || (strcmp(pTable->pItems[i].sName, sName) != 0) ) {
			continue;
		}
		if ( !__xlSymbolCanShareName(pTable->pItems[i].eKind, eKind) ) {
			return (int)i;
		}
	}

	return -1;
}

static int __xlDeclareSymbol(
	XL_ResolveContext* pCtx,
	XL_SymbolKind eKind,
	int iScopeId,
	const char* sName,
	const char* sTypeName,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator,
	const XL_AstNode* pNode
)
{
	size_t iPrevCount;
	int iRc;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_OK;
	}
	if ( __xlFindConflictingSymbolInScope(&pCtx->pResult->symbols, iScopeId, sName, eKind) >= 0 ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "duplicate symbol '%s' in current scope", sName);
	}

	iPrevCount = pCtx->pResult->symbols.iCount;
	iRc = xlSymbolTableAdd(&pCtx->pResult->symbols, eKind, iScopeId, sName, sTypeName, iLine, iColumn, bAsync, bGenerator);
	if ( iRc == XL_RC_OK ) {
		__xlSemNodeSetScope(pCtx->pResult, pNode, iScopeId);
		__xlSemNodeSetSymbol(pCtx->pResult, pNode, (int)iPrevCount);
		__xlSemNodeSetType(pCtx->pResult, pNode, sTypeName);
	}
	return iRc;
}

static int __xlResolveTypeRef(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	if ( pNode == NULL ) {
		return XL_RC_OK;
	}
	if ( (pNode->sText == NULL) || (pNode->sText[0] == '\0') ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "empty type reference");
	}
	if ( xlTypeTableFind(&pCtx->pResult->types, pNode->sText) == NULL ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "unknown type '%s'", pNode->sText);
	}

	return XL_RC_OK;
}

static char* __xlBuildImportPath(const XL_AstNode* pNode)
{
	char* sLeft;
	char* sOut;
	size_t iLeftLen;
	size_t iRightLen;

	if ( pNode == NULL ) {
		return __xlSemDupText("");
	}
	if ( pNode->eKind == XL_AST_IDENT ) {
		return __xlSemDupText(pNode->sText);
	}
	if ( (pNode->eKind != XL_AST_MEMBER) || (pNode->sText == NULL) ) {
		return __xlSemDupText("");
	}

	sLeft = __xlBuildImportPath(__xlAstChild(pNode, 0));
	if ( sLeft == NULL ) {
		return NULL;
	}

	iLeftLen = strlen(sLeft);
	iRightLen = strlen(pNode->sText);
	sOut = (char*)malloc(iLeftLen + iRightLen + 2);
	if ( sOut == NULL ) {
		free(sLeft);
		return NULL;
	}

	if ( iLeftLen == 0 ) {
		snprintf(sOut, iLeftLen + iRightLen + 2, "%s", pNode->sText);
	} else {
		snprintf(sOut, iLeftLen + iRightLen + 2, "%s.%s", sLeft, pNode->sText);
	}

	free(sLeft);
	return sOut;
}

static int __xlCollectFuncInfo(XL_ResolveContext* pCtx, const XL_AstNode* pNode, int iScopeId)
{
	const XL_AstNode* pParams;
	const XL_AstNode* pRetType;
	const XL_AstNode* pParam;
	const XL_AstNode* pParamType;
	const XL_AstNode* pParamDefault;
	const char** arrParamNames;
	const char** arrParamTypes;
	const XL_AstNode** arrParamDefaults;
	unsigned char* arrParamHasDefault;
	size_t i;
	int iRc;
	int bGenerator;
	int bSeenDefault;
	size_t iPrevCount;

	pParams = __xlAstChild(pNode, 0);
	pRetType = __xlAstChild(pNode, 1);
	arrParamNames = NULL;
	arrParamTypes = NULL;
	arrParamDefaults = NULL;
	arrParamHasDefault = NULL;
	bSeenDefault = 0;

	if ( (pNode == NULL) || (pNode->sText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( (pParams != NULL) && (pParams->iChildCount != 0) ) {
		arrParamNames = (const char**)calloc(pParams->iChildCount, sizeof(const char*));
		arrParamTypes = (const char**)calloc(pParams->iChildCount, sizeof(const char*));
		arrParamDefaults = (const XL_AstNode**)calloc(pParams->iChildCount, sizeof(XL_AstNode*));
		arrParamHasDefault = (unsigned char*)calloc(pParams->iChildCount, sizeof(unsigned char));
		if ( (arrParamNames == NULL) || (arrParamTypes == NULL) ||
			(arrParamDefaults == NULL) || (arrParamHasDefault == NULL) ) {
			free(arrParamNames);
			free(arrParamTypes);
			free(arrParamDefaults);
			free(arrParamHasDefault);
			return XL_RC_IO_ERROR;
		}

		for ( i = 0; i < pParams->iChildCount; ++i ) {
			pParam = pParams->ppChildren[i];
			pParamType = __xlParamTypeNode(pParam);
			pParamDefault = __xlParamDefaultNode(pParam);
			arrParamNames[i] = (pParam != NULL) ? pParam->sText : "";
			arrParamTypes[i] = ((pParamType != NULL) && (pParamType->sText != NULL)) ? pParamType->sText : "";
			arrParamDefaults[i] = pParamDefault;
			arrParamHasDefault[i] = (pParamDefault != NULL) ? 1u : 0u;
			if ( pParamDefault != NULL ) {
				bSeenDefault = 1;
			} else if ( bSeenDefault ) {
				free(arrParamNames);
				free(arrParamTypes);
				free(arrParamDefaults);
				free(arrParamHasDefault);
				return __xlSemAddDiag(
					pCtx,
					XL_SEM_DIAG_ERROR,
					pParam,
					"required parameter '%s' cannot follow a default parameter",
					(pParam != NULL) ? pParam->sText : ""
				);
			}
		}
	}

	bGenerator = __xlNodeHasYield(__xlAstChild(pNode, 2));

	iPrevCount = pCtx->pResult->funcs.iCount;
	iRc = xlFuncTableAdd(
		&pCtx->pResult->funcs,
		pNode->sText,
		((pRetType != NULL) && (pRetType->sText != NULL)) ? pRetType->sText : "",
		arrParamNames,
		arrParamTypes,
		(pParams != NULL) ? pParams->iChildCount : 0,
		arrParamHasDefault,
		arrParamDefaults,
		iScopeId,
		pNode->iLine,
		pNode->iColumn,
		pNode->bFlag,
		bGenerator
	);

	free(arrParamNames);
	free(arrParamTypes);
	free(arrParamDefaults);
	free(arrParamHasDefault);
	if ( iRc == XL_RC_OK ) {
		__xlSemNodeSetFunc(pCtx->pResult, pNode, (int)iPrevCount);
		__xlSemNodeSetType(
			pCtx->pResult,
			pNode,
			((pRetType != NULL) && (pRetType->sText != NULL)) ? pRetType->sText : ""
		);
	}
	return iRc;
}

static const char* __xlInferExprType(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveExpr(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveStmt(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveForIn(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveTry(XL_ResolveContext* pCtx, const XL_AstNode* pNode);

static int __xlResolveBlock(XL_ResolveContext* pCtx, const XL_AstNode* pNode, const char* sName)
{
	size_t i;
	int iPrevScopeId;
	int iBlockScopeId;
	int iRc;

	if ( pNode == NULL ) {
		return XL_RC_OK;
	}

	iRc = xlScopeTableAdd(
		&pCtx->pResult->scopes,
		XL_SCOPE_BLOCK,
		pCtx->iCurrentScopeId,
		sName,
		pNode->iLine,
		pNode->iColumn,
		&iBlockScopeId
	);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlSemNodeSetScope(pCtx->pResult, pNode, iBlockScopeId);
	__xlSemTrace(pCtx, pNode, "enter block '%s'", (sName != NULL) ? sName : "block");
	iPrevScopeId = pCtx->iCurrentScopeId;
	pCtx->iCurrentScopeId = iBlockScopeId;

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iRc = __xlResolveStmt(pCtx, pNode->ppChildren[i]);
		if ( iRc != XL_RC_OK ) {
			pCtx->iCurrentScopeId = iPrevScopeId;
			return iRc;
		}
	}

	pCtx->iCurrentScopeId = iPrevScopeId;
	return XL_RC_OK;
}

static int __xlResolveExprList(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	size_t i;
	int iRc;

	if ( pNode == NULL ) {
		return XL_RC_OK;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iRc = __xlResolveExpr(pCtx, pNode->ppChildren[i]);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return XL_RC_OK;
}

static const XL_SymbolInfo* __xlFindVisibleNonFuncSymbol(
	const XL_SymbolTable* pSymbols,
	const XL_ScopeTable* pScopes,
	int iScopeId,
	const char* sName
)
{
	const XL_ScopeInfo* pScope;
	size_t i;

	while ( iScopeId >= 0 ) {
		for ( i = 0; i < pSymbols->iCount; ++i ) {
			if ( (pSymbols->pItems[i].iScopeId == iScopeId) &&
				(strcmp(pSymbols->pItems[i].sName, sName) == 0) &&
				(pSymbols->pItems[i].eKind != XL_SYMBOL_FUNC) ) {
				return &pSymbols->pItems[i];
			}
		}

		pScope = xlScopeTableGet(pScopes, iScopeId);
		if ( pScope == NULL ) {
			break;
		}
		iScopeId = pScope->iParentId;
	}

	return NULL;
}

static int __xlFormatArgTypes(
	XL_ResolveContext* pCtx,
	const XL_AstNode* pArgs,
	char* sOut,
	size_t iOutSize
)
{
	size_t i;
	size_t iUsed;
	const char* sTypeName;

	XL_UNUSED(pCtx);

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	sOut[0] = '\0';
	iUsed = 0;
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		sTypeName = __xlInferExprType(pCtx, __xlArgValue(pArgs->ppChildren[i]));
		iUsed += (size_t)snprintf(
			sOut + iUsed,
			(iUsed < iOutSize) ? (iOutSize - iUsed) : 0,
			"%s%s",
			(i == 0) ? "" : ", ",
			((sTypeName != NULL) && (sTypeName[0] != '\0')) ? sTypeName : "?"
		);
	}

	return XL_RC_OK;
}

static int __xlFindParamIndexByName(const XL_FuncInfo* pFunc, const char* sName)
{
	size_t i;

	if ( (pFunc == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return -1;
	}

	for ( i = 0; i < pFunc->iParamCount; ++i ) {
		if ( (pFunc->arrParamNames[i] != NULL) && (strcmp(pFunc->arrParamNames[i], sName) == 0) ) {
			return (int)i;
		}
	}

	return -1;
}

static int __xlValidateCallArgShape(XL_ResolveContext* pCtx, const XL_AstNode* pCallNode, const XL_AstNode* pArgs)
{
	const XL_AstNode* pArg;
	const char* sName;
	size_t i;
	size_t j;
	int bSeenNamed;

	bSeenNamed = 0;
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		pArg = pArgs->ppChildren[i];
		sName = __xlArgName(pArg);
		if ( sName != NULL ) {
			bSeenNamed = 1;
			for ( j = i + 1; j < pArgs->iChildCount; ++j ) {
				if ( (__xlArgName(pArgs->ppChildren[j]) != NULL) &&
					(strcmp(sName, __xlArgName(pArgs->ppChildren[j])) == 0) ) {
					return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pCallNode, "duplicate named argument '%s'", sName);
				}
			}
			continue;
		}

		if ( bSeenNamed ) {
			return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pCallNode, "positional arguments cannot follow named arguments");
		}
	}

	return XL_RC_OK;
}

static int __xlMatchCallToFunc(
	XL_ResolveContext* pCtx,
	const XL_FuncInfo* pFunc,
	const XL_AstNode* pArgs,
	int* piScore
)
{
	const XL_AstNode* pArg;
	const XL_AstNode* pValue;
	const char* sName;
	const char* sArgType;
	const char* sParamType;
	unsigned char* arrBound;
	size_t i;
	size_t iNextPos;
	int iParamIndex;
	int iScore;

	if ( piScore != NULL ) {
		*piScore = -1;
	}
	if ( (pCtx == NULL) || (pFunc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	arrBound = (unsigned char*)calloc((pFunc->iParamCount != 0) ? pFunc->iParamCount : 1, sizeof(unsigned char));
	if ( arrBound == NULL ) {
		return XL_RC_IO_ERROR;
	}

	iScore = 0;
	iNextPos = 0;
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		pArg = pArgs->ppChildren[i];
		pValue = __xlArgValue(pArg);
		sName = __xlArgName(pArg);
		if ( sName != NULL ) {
			iParamIndex = __xlFindParamIndexByName(pFunc, sName);
			if ( iParamIndex < 0 ) {
				free(arrBound);
				return XL_RC_FAIL;
			}
		} else {
			while ( (iNextPos < pFunc->iParamCount) && arrBound[iNextPos] ) {
				++iNextPos;
			}
			if ( iNextPos >= pFunc->iParamCount ) {
				free(arrBound);
				return XL_RC_FAIL;
			}
			iParamIndex = (int)iNextPos;
			++iNextPos;
		}

		if ( arrBound[iParamIndex] ) {
			free(arrBound);
			return XL_RC_FAIL;
		}
		arrBound[iParamIndex] = 1u;

		sArgType = __xlInferExprType(pCtx, pValue);
		sParamType = pFunc->arrParamTypes[iParamIndex];
		if ( (sArgType != NULL) && (sArgType[0] != '\0') && (sParamType != NULL) && (sParamType[0] != '\0') ) {
			if ( !__xlTypeMatches(sParamType, sArgType) ) {
				free(arrBound);
				return XL_RC_FAIL;
			}
			iScore += 2;
		} else {
			iScore += 1;
		}
	}

	for ( i = 0; i < pFunc->iParamCount; ++i ) {
		if ( arrBound[i] ) {
			continue;
		}
		if ( (pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[i] ) {
			sArgType = __xlInferExprType(pCtx, pFunc->arrParamDefaultValues[i]);
			sParamType = pFunc->arrParamTypes[i];
			if ( (sArgType != NULL) && (sArgType[0] != '\0') && (sParamType != NULL) && (sParamType[0] != '\0') ) {
				if ( !__xlTypeMatches(sParamType, sArgType) ) {
					free(arrBound);
					return XL_RC_FAIL;
				}
				iScore += 2;
			} else {
				iScore += 1;
			}
			continue;
		}
		free(arrBound);
		return XL_RC_FAIL;
	}

	free(arrBound);
	if ( piScore != NULL ) {
		*piScore = iScore;
	}
	return XL_RC_OK;
}

static int __xlResolveCallByName(
	XL_ResolveContext* pCtx,
	const XL_AstNode* pCallNode,
	const char* sName,
	const XL_AstNode* pArgs
)
{
	const XL_FuncInfo* pFunc;
	const XL_SymbolInfo* pNonFunc;
	size_t i;
	size_t iArgCount;
	size_t iNameMatchCount;
	size_t iCompatibleCount;
	size_t iBestMatchCount;
	size_t iBestFuncIndex;
	int iScore;
	int iBestScore;
	char sArgText[XL_DIAG_TEXT_MAX];

	iArgCount = (pArgs != NULL) ? pArgs->iChildCount : 0;
	iNameMatchCount = 0;
	iCompatibleCount = 0;
	iBestMatchCount = 0;
	iBestFuncIndex = 0;
	iBestScore = -1;

	for ( i = 0; i < pCtx->pResult->funcs.iCount; ++i ) {
		pFunc = xlFuncTableGet(&pCtx->pResult->funcs, i);
		if ( (pFunc == NULL) || (strcmp(pFunc->sName, sName) != 0) ) {
			continue;
		}
		if ( !__xlScopeContains(&pCtx->pResult->scopes, pCtx->iCurrentScopeId, pFunc->iScopeId) ) {
			continue;
		}

		iNameMatchCount += 1;
		if ( __xlMatchCallToFunc(pCtx, pFunc, pArgs, &iScore) != XL_RC_OK ) {
			continue;
		}

		iCompatibleCount += 1;
		if ( iScore > iBestScore ) {
			iBestScore = iScore;
			iBestFuncIndex = i;
			iBestMatchCount = 1;
		} else if ( iScore == iBestScore ) {
			iBestMatchCount += 1;
		}
	}

	if ( iNameMatchCount == 0 ) {
		pNonFunc = __xlFindVisibleNonFuncSymbol(&pCtx->pResult->symbols, &pCtx->pResult->scopes, pCtx->iCurrentScopeId, sName);
		if ( pNonFunc != NULL ) {
			return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pCallNode, "'%s' is not callable", sName);
		}
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pCallNode, "unresolved function '%s'", sName);
	}

	if ( iCompatibleCount == 0 ) {
		__xlFormatArgTypes(pCtx, pArgs, sArgText, sizeof(sArgText));
		return __xlSemAddDiag(
			pCtx,
			XL_SEM_DIAG_ERROR,
			pCallNode,
			"no overload matches '%s(%s)'",
			sName,
			sArgText
		);
	}

	if ( iBestMatchCount > 1 ) {
		__xlFormatArgTypes(pCtx, pArgs, sArgText, sizeof(sArgText));
		return __xlSemAddDiag(
			pCtx,
			XL_SEM_DIAG_ERROR,
			pCallNode,
			"ambiguous overload for '%s(%s)'",
			sName,
			sArgText
		);
	}

	pFunc = xlFuncTableGet(&pCtx->pResult->funcs, iBestFuncIndex);
	if ( pFunc != NULL ) {
		__xlSemNodeSetFunc(pCtx->pResult, pCallNode, (int)iBestFuncIndex);
		__xlSemNodeSetType(
			pCtx->pResult,
			pCallNode,
			pFunc->bGenerator ? "generator" : ((pFunc->sReturnTypeName != NULL) ? pFunc->sReturnTypeName : "")
		);
		__xlSemTrace(
			pCtx,
			pCallNode,
			"resolved call '%s' -> func@%d:%d with %llu args",
			sName,
			pFunc->iLine,
			pFunc->iColumn,
			(unsigned long long)iArgCount
		);
	}

	return XL_RC_OK;
}

static int __xlResolveCallExpr(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pCallee;
	const XL_AstNode* pArgs;
	int iRc;

	pCallee = __xlAstChild(pNode, 0);
	pArgs = __xlAstChild(pNode, 1);

	iRc = __xlValidateCallArgShape(pCtx, pNode, pArgs);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlResolveExprList(pCtx, pArgs);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( (pCallee != NULL) && (pCallee->eKind == XL_AST_IDENT) && (pCallee->sText != NULL) ) {
		return __xlResolveCallByName(pCtx, pNode, pCallee->sText, pArgs);
	}

	iRc = __xlResolveExpr(pCtx, pCallee);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	return XL_RC_OK;
}

static const char* __xlInferExprType(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_SemNodeInfo* pInfo;
	const XL_SymbolInfo* pSymbol;
	const XL_FuncInfo* pFunc;
	const XL_AstNode* pLeft;
	const XL_AstNode* pRight;
	const XL_AstNode* pCallee;
	const char* sLeftType;
	const char* sRightType;
	size_t i;

	if ( pNode == NULL ) {
		return "";
	}

	switch ( pNode->eKind ) {
		case XL_AST_INT:
			return "int";
		case XL_AST_FLOAT:
			return "float";
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
			return "string";
		case XL_AST_BOOL:
			return "bool";
		case XL_AST_NULL:
			return "null";
		case XL_AST_TIME:
			return "time";
		case XL_AST_IDENT:
			pSymbol = xlSymbolTableFindVisible(&pCtx->pResult->symbols, &pCtx->pResult->scopes, pCtx->iCurrentScopeId, pNode->sText);
			if ( (pSymbol != NULL) && (pSymbol->sTypeName != NULL) && (pSymbol->sTypeName[0] != '\0') ) {
				return pSymbol->sTypeName;
			}
			return "";
		case XL_AST_UNARY:
			if ( (pNode->sText != NULL) && ((strcmp(pNode->sText, "not") == 0) || (strcmp(pNode->sText, "!") == 0)) ) {
				return "bool";
			}
			return __xlInferExprType(pCtx, __xlAstChild(pNode, 0));
		case XL_AST_NAMED_ARG:
			return __xlInferExprType(pCtx, __xlAstChild(pNode, 0));
		case XL_AST_BINARY:
			pLeft = __xlAstChild(pNode, 0);
			pRight = __xlAstChild(pNode, 1);
			sLeftType = __xlInferExprType(pCtx, pLeft);
			sRightType = __xlInferExprType(pCtx, pRight);
			if ( (pNode->sText != NULL) &&
				((strcmp(pNode->sText, ">") == 0) || (strcmp(pNode->sText, ">=") == 0) ||
				(strcmp(pNode->sText, "<") == 0) || (strcmp(pNode->sText, "<=") == 0) ||
				(strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0) ||
				(strcmp(pNode->sText, "~=") == 0) || (strcmp(pNode->sText, "and") == 0) ||
				(strcmp(pNode->sText, "or") == 0) || (strcmp(pNode->sText, "in") == 0))) {
				return "bool";
			}
			if ( (strcmp(sLeftType, "float") == 0) || (strcmp(sRightType, "float") == 0) ) {
				return "float";
			}
			if ( (strcmp(sLeftType, "int") == 0) && (strcmp(sRightType, "int") == 0) ) {
				return "int";
			}
			if ( (strcmp(sLeftType, "string") == 0) && (strcmp(sRightType, "string") == 0) ) {
				return "string";
			}
			return sLeftType;
		case XL_AST_CALL:
			pInfo = xlSemNodeInfoFind(pCtx->pResult, pNode);
			if ( (pInfo != NULL) && (pInfo->iFuncIndex >= 0) ) {
				pFunc = xlFuncTableGet(&pCtx->pResult->funcs, (size_t)pInfo->iFuncIndex);
				if ( pFunc == NULL ) {
					return "";
				}
				if ( pFunc->bGenerator ) {
					return "generator";
				}
				return pFunc->sReturnTypeName;
			}

			pCallee = __xlAstChild(pNode, 0);
			if ( (pCallee == NULL) || (pCallee->eKind != XL_AST_IDENT) || (pCallee->sText == NULL) ) {
				return "";
			}
			return "";
		case XL_AST_MEMBER:
			pLeft = __xlAstChild(pNode, 0);
			sLeftType = __xlInferExprType(pCtx, pLeft);
			if ( (sLeftType != NULL) && (strcmp(sLeftType, "error") == 0) ) {
				sRightType = __xlErrorFieldTypeName(pNode->sText);
				return (sRightType != NULL) ? sRightType : "";
			}
			return "";
		default:
			return "";
	}
}

static int __xlResolveForIn(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pTargets;
	const XL_AstNode* pIterable;
	const XL_AstNode* pBody;
	const XL_AstNode* pTarget;
	const char* sIterableType;
	const char* sValueType;
	const char* sTargetType;
	int iPrevScopeId;
	int iLoopScopeId;
	int iPrevLoopDepth;
	int iRc;
	size_t i;

	pTargets = __xlAstChild(pNode, 0);
	pIterable = __xlAstChild(pNode, 1);
	pBody = __xlAstChild(pNode, 2);
	if ( (pTargets == NULL) || (pTargets->iChildCount == 0) ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "for-in requires at least one binding");
	}
	if ( pTargets->iChildCount > 2 ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "for-in supports at most two bindings in stage 1");
	}

	iRc = __xlResolveExpr(pCtx, pIterable);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	sIterableType = __xlInferExprType(pCtx, pIterable);
	__xlSemNodeSetType(pCtx->pResult, pIterable, sIterableType);
	if ( (sIterableType != NULL) && (sIterableType[0] != '\0') && !xlCheckIterableTypeName(sIterableType) ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pIterable, "type '%s' is not iterable", sIterableType);
	}

	if ( (sIterableType != NULL) && (strcmp(sIterableType, "range") == 0) ) {
		sValueType = "int";
	} else {
		sValueType = "any";
	}

	iRc = xlScopeTableAdd(
		&pCtx->pResult->scopes,
		XL_SCOPE_BLOCK,
		pCtx->iCurrentScopeId,
		"for.in",
		pNode->iLine,
		pNode->iColumn,
		&iLoopScopeId
	);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlSemNodeSetScope(pCtx->pResult, pNode, iLoopScopeId);
	iPrevScopeId = pCtx->iCurrentScopeId;
	iPrevLoopDepth = pCtx->iLoopDepth;
	pCtx->iCurrentScopeId = iLoopScopeId;
	pCtx->iLoopDepth += 1;
	__xlSemTrace(pCtx, pNode, "enter for-in");

	for ( i = 0; i < pTargets->iChildCount; ++i ) {
		pTarget = pTargets->ppChildren[i];
		if ( (pTarget == NULL) || (pTarget->sText == NULL) ) {
			continue;
		}
		if ( (pTargets->iChildCount == 2) && (i == 0) ) {
			sTargetType = "int";
		} else {
			sTargetType = sValueType;
		}
		iRc = __xlDeclareSymbol(
			pCtx,
			XL_SYMBOL_VAR,
			pCtx->iCurrentScopeId,
			pTarget->sText,
			sTargetType,
			pTarget->iLine,
			pTarget->iColumn,
			0,
			0,
			pTarget
		);
		if ( iRc != XL_RC_OK ) {
			pCtx->iCurrentScopeId = iPrevScopeId;
			pCtx->iLoopDepth = iPrevLoopDepth;
			return iRc;
		}
		__xlSemTrace(pCtx, pTarget, "declared for-in binding '%s' as %s", pTarget->sText, sTargetType);
	}

	iRc = __xlResolveBlock(pCtx, pBody, "for.in.body");
	pCtx->iCurrentScopeId = iPrevScopeId;
	pCtx->iLoopDepth = iPrevLoopDepth;
	return iRc;
}

static int __xlResolveExpr(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_SymbolInfo* pSymbol;
	const char* sTypeName;
	size_t i;
	int iRc;

	if ( pNode == NULL ) {
		return XL_RC_OK;
	}

	switch ( pNode->eKind ) {
		case XL_AST_IDENT:
			pSymbol = xlSymbolTableFindVisible(&pCtx->pResult->symbols, &pCtx->pResult->scopes, pCtx->iCurrentScopeId, pNode->sText);
			if ( pSymbol == NULL ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "unresolved identifier '%s'", pNode->sText);
			}
			__xlSemNodeSetSymbol(pCtx->pResult, pNode, (int)(pSymbol - pCtx->pResult->symbols.pItems));
			__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
			__xlSemNodeSetType(pCtx->pResult, pNode, pSymbol->sTypeName);
			__xlSemTrace(pCtx, pNode, "resolved identifier '%s' -> %s", pNode->sText, xlSymbolKindName(pSymbol->eKind));
			return XL_RC_OK;
		case XL_AST_CALL:
			iRc = __xlResolveCallExpr(pCtx, pNode);
			if ( iRc == XL_RC_OK ) {
				__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
				__xlSemNodeSetType(pCtx->pResult, pNode, __xlInferExprType(pCtx, pNode));
			}
			return iRc;
		case XL_AST_MEMBER:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				iRc = __xlResolveExpr(pCtx, pNode->ppChildren[i]);
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
			}
			__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
			sTypeName = __xlInferExprType(pCtx, pNode);
			if ( (__xlInferExprType(pCtx, __xlAstChild(pNode, 0)) != NULL) &&
				(strcmp(__xlInferExprType(pCtx, __xlAstChild(pNode, 0)), "error") == 0) &&
				((sTypeName == NULL) || (sTypeName[0] == '\0')) ) {
				return __xlSemAddDiag(
					pCtx,
					XL_SEM_DIAG_ERROR,
					pNode,
					"error has no field '%s'",
					(pNode->sText != NULL) ? pNode->sText : ""
				);
			}
			if ( (sTypeName != NULL) && (sTypeName[0] != '\0') ) {
				__xlSemNodeSetType(pCtx->pResult, pNode, sTypeName);
			}
			return XL_RC_OK;
		case XL_AST_INDEX:
		case XL_AST_BINARY:
		case XL_AST_UNARY:
		case XL_AST_EXPR_STMT:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				iRc = __xlResolveExpr(pCtx, pNode->ppChildren[i]);
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
			}
			if ( (pNode->eKind == XL_AST_UNARY) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) &&
				(pCtx->iAsyncDepth <= 0) ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "await is only allowed in async function");
			}
			__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
			sTypeName = __xlInferExprType(pCtx, pNode);
			if ( (sTypeName != NULL) && (sTypeName[0] != '\0') ) {
				__xlSemNodeSetType(pCtx->pResult, pNode, sTypeName);
			}
			return XL_RC_OK;
		case XL_AST_LIST:
			return __xlResolveExprList(pCtx, pNode);
		case XL_AST_INT:
		case XL_AST_FLOAT:
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
		case XL_AST_BOOL:
		case XL_AST_NULL:
		case XL_AST_TIME:
			__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
			__xlSemNodeSetType(pCtx->pResult, pNode, __xlInferExprType(pCtx, pNode));
			return XL_RC_OK;
		default:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				iRc = __xlResolveExpr(pCtx, pNode->ppChildren[i]);
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
			}
			__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
			sTypeName = __xlInferExprType(pCtx, pNode);
			if ( (sTypeName != NULL) && (sTypeName[0] != '\0') ) {
				__xlSemNodeSetType(pCtx->pResult, pNode, sTypeName);
			}
			return XL_RC_OK;
	}
}

static int __xlResolveConstDecl(XL_ResolveContext* pCtx, const XL_AstNode* pNode, XL_SymbolKind eKind)
{
	const XL_AstNode* pTypeNode;
	const XL_AstNode* pValueNode;
	int iRc;

	pTypeNode = __xlAstChild(pNode, 0);
	pValueNode = __xlAstChild(pNode, 1);

	if ( pTypeNode != NULL ) {
		iRc = __xlResolveTypeRef(pCtx, pTypeNode);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	iRc = __xlDeclareSymbol(
		pCtx,
		eKind,
		pCtx->iCurrentScopeId,
		pNode->sText,
		(pTypeNode != NULL) ? pTypeNode->sText : "",
		pNode->iLine,
		pNode->iColumn,
		0,
		0,
		pNode
	);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlSemTrace(pCtx, pNode, "declared %s '%s'", xlSymbolKindName(eKind), pNode->sText);
	iRc = __xlResolveExpr(pCtx, pValueNode);
	if ( iRc == XL_RC_OK ) {
		if ( (pTypeNode != NULL) && (pTypeNode->sText != NULL) ) {
			__xlSemNodeSetType(pCtx->pResult, pNode, pTypeNode->sText);
		} else {
			__xlSemNodeSetType(pCtx->pResult, pNode, __xlInferExprType(pCtx, pValueNode));
		}
	}
	return iRc;
}

static int __xlResolveIf(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pCondNode;
	const XL_AstNode* pThenBlock;
	const XL_AstNode* pElseIfList;
	const XL_AstNode* pElseBlock;
	const XL_AstNode* pElseIfNode;
	int iRc;
	size_t i;

	pCondNode = __xlAstChild(pNode, 0);
	pThenBlock = __xlAstChild(pNode, 1);
	pElseIfList = __xlAstChild(pNode, 2);
	pElseBlock = __xlAstChild(pNode, 3);

	iRc = __xlResolveExpr(pCtx, pCondNode);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlResolveBlock(pCtx, pThenBlock, "if.then");
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( pElseIfList != NULL ) {
		for ( i = 0; i < pElseIfList->iChildCount; ++i ) {
			pElseIfNode = pElseIfList->ppChildren[i];
			iRc = __xlResolveExpr(pCtx, __xlAstChild(pElseIfNode, 0));
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			iRc = __xlResolveBlock(pCtx, __xlAstChild(pElseIfNode, 1), "if.elseif");
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
		}
	}

	return __xlResolveBlock(pCtx, pElseBlock, "if.else");
}

static int __xlResolveTry(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pTryBlock;
	const XL_AstNode* pCatchName;
	const XL_AstNode* pCatchBlock;
	const XL_AstNode* pFinallyBlock;
	int bHasCatch;
	int bHasFinally;
	int iRc;
	int iPrevScopeId;
	int iCatchScopeId;

	pTryBlock = __xlAstChild(pNode, 0);
	pCatchName = __xlAstChild(pNode, 1);
	pCatchBlock = __xlAstChild(pNode, 2);
	pFinallyBlock = __xlAstChild(pNode, 3);
	bHasCatch = (pNode->bFlag & 1) != 0;
	bHasFinally = (pNode->bFlag & 2) != 0;

	if ( !bHasCatch && !bHasFinally ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "try requires catch or finally");
	}
	if ( bHasFinally &&
		(__xlNodeHasFinallyEscape(pTryBlock) || __xlNodeHasFinallyEscape(pCatchBlock) || __xlNodeHasFinallyEscape(pFinallyBlock)) ) {
		return __xlSemAddDiag(
			pCtx,
			XL_SEM_DIAG_ERROR,
			pNode,
			"return/break/continue inside try-finally is not supported in demo5"
		);
	}

	iRc = __xlResolveBlock(pCtx, pTryBlock, "try.body");
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( bHasCatch ) {
		iRc = xlScopeTableAdd(
			&pCtx->pResult->scopes,
			XL_SCOPE_BLOCK,
			pCtx->iCurrentScopeId,
			"try.catch",
			pNode->iLine,
			pNode->iColumn,
			&iCatchScopeId
		);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}

		iPrevScopeId = pCtx->iCurrentScopeId;
		pCtx->iCurrentScopeId = iCatchScopeId;
		if ( pCatchName != NULL ) {
			iRc = __xlDeclareSymbol(
				pCtx,
				XL_SYMBOL_VAR,
				pCtx->iCurrentScopeId,
				pCatchName->sText,
				"error",
				pCatchName->iLine,
				pCatchName->iColumn,
				0,
				0,
				pCatchName
			);
			if ( iRc != XL_RC_OK ) {
				pCtx->iCurrentScopeId = iPrevScopeId;
				return iRc;
			}
			__xlSemTrace(pCtx, pCatchName, "declared catch binding '%s'", pCatchName->sText);
		}

		iRc = __xlResolveBlock(pCtx, pCatchBlock, "try.catch.body");
		pCtx->iCurrentScopeId = iPrevScopeId;
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	if ( bHasFinally ) {
		iRc = __xlResolveBlock(pCtx, pFinallyBlock, "try.finally");
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	__xlSemNodeSetScope(pCtx->pResult, pNode, pCtx->iCurrentScopeId);
	return XL_RC_OK;
}

static int __xlResolveFuncDecl(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pParams;
	const XL_AstNode* pRetType;
	const XL_AstNode* pBody;
	const XL_AstNode* pParamNode;
	const XL_AstNode* pParamType;
	const XL_AstNode* pParamDefault;
	const char* sDefaultType;
	int iFuncScopeId;
	int iPrevScopeId;
	int iPrevAsyncDepth;
	int iRc;
	size_t i;

	pParams = __xlAstChild(pNode, 0);
	pRetType = __xlAstChild(pNode, 1);
	pBody = __xlAstChild(pNode, 2);

	if ( pCtx->iCurrentScopeId != 0 ) {
		iRc = __xlDeclareSymbol(
			pCtx,
			XL_SYMBOL_FUNC,
			pCtx->iCurrentScopeId,
			pNode->sText,
			((pRetType != NULL) && (pRetType->sText != NULL)) ? pRetType->sText : "",
			pNode->iLine,
			pNode->iColumn,
			pNode->bFlag,
			__xlNodeHasYield(__xlAstChild(pNode, 2)),
			pNode
		);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}

		iRc = __xlCollectFuncInfo(pCtx, pNode, pCtx->iCurrentScopeId);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	if ( pRetType != NULL ) {
		iRc = __xlResolveTypeRef(pCtx, pRetType);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	iRc = xlScopeTableAdd(
		&pCtx->pResult->scopes,
		XL_SCOPE_FUNC,
		pCtx->iCurrentScopeId,
		pNode->sText,
		pNode->iLine,
		pNode->iColumn,
		&iFuncScopeId
	);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlSemNodeSetScope(pCtx->pResult, pNode, iFuncScopeId);
	iPrevScopeId = pCtx->iCurrentScopeId;
	iPrevAsyncDepth = pCtx->iAsyncDepth;
	pCtx->iCurrentScopeId = iFuncScopeId;
	if ( pNode->bFlag ) {
		pCtx->iAsyncDepth += 1;
	}
	__xlSemTrace(pCtx, pNode, "enter func '%s'", pNode->sText);

	if ( pParams != NULL ) {
		for ( i = 0; i < pParams->iChildCount; ++i ) {
			pParamNode = pParams->ppChildren[i];
			pParamType = __xlParamTypeNode(pParamNode);
			pParamDefault = __xlParamDefaultNode(pParamNode);
			if ( pParamType != NULL ) {
				iRc = __xlResolveTypeRef(pCtx, pParamType);
				if ( iRc != XL_RC_OK ) {
					pCtx->iCurrentScopeId = iPrevScopeId;
					return iRc;
				}
			}
			iRc = __xlDeclareSymbol(
				pCtx,
				XL_SYMBOL_PARAM,
				pCtx->iCurrentScopeId,
				pParamNode->sText,
				(pParamType != NULL) ? pParamType->sText : "",
				pParamNode->iLine,
				pParamNode->iColumn,
				0,
				0,
				pParamNode
			);
			if ( iRc != XL_RC_OK ) {
				pCtx->iCurrentScopeId = iPrevScopeId;
				return iRc;
			}
			__xlSemTrace(pCtx, pParamNode, "declared param '%s'", pParamNode->sText);

			if ( pParamDefault != NULL ) {
				if ( !__xlIsStaticDefaultExpr(pParamDefault) ) {
					pCtx->iCurrentScopeId = iPrevScopeId;
					pCtx->iAsyncDepth = iPrevAsyncDepth;
					return __xlSemAddDiag(
						pCtx,
						XL_SEM_DIAG_ERROR,
						pParamDefault,
						"default value for parameter '%s' must be a static expression in demo5",
						pParamNode->sText
					);
				}
				iRc = __xlResolveExpr(pCtx, pParamDefault);
				if ( iRc != XL_RC_OK ) {
					pCtx->iCurrentScopeId = iPrevScopeId;
					pCtx->iAsyncDepth = iPrevAsyncDepth;
					return iRc;
				}
				if ( (pParamType != NULL) && (pParamType->sText != NULL) && (pParamType->sText[0] != '\0') ) {
					sDefaultType = __xlInferExprType(pCtx, pParamDefault);
					if ( (sDefaultType != NULL) && (sDefaultType[0] != '\0') && !__xlTypeMatches(pParamType->sText, sDefaultType) ) {
						pCtx->iCurrentScopeId = iPrevScopeId;
						pCtx->iAsyncDepth = iPrevAsyncDepth;
						return __xlSemAddDiag(
							pCtx,
							XL_SEM_DIAG_ERROR,
							pParamDefault,
							"default value for parameter '%s' has type '%s', expected '%s'",
							pParamNode->sText,
							sDefaultType,
							pParamType->sText
						);
					}
				}
			}
		}
	}

	iRc = __xlResolveBlock(pCtx, pBody, "func.body");
	pCtx->iCurrentScopeId = iPrevScopeId;
	pCtx->iAsyncDepth = iPrevAsyncDepth;
	return iRc;
}

static int __xlResolveStmt(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	char* sImportPath;
	int iRc;

	if ( pNode == NULL ) {
		return XL_RC_OK;
	}

	switch ( pNode->eKind ) {
		case XL_AST_FUNC_DECL:
			return __xlResolveFuncDecl(pCtx, pNode);
		case XL_AST_CONST_DECL:
			return __xlResolveConstDecl(pCtx, pNode, XL_SYMBOL_CONST);
		case XL_AST_VAR_DECL:
			return __xlResolveConstDecl(pCtx, pNode, XL_SYMBOL_VAR);
		case XL_AST_IF:
			return __xlResolveIf(pCtx, pNode);
		case XL_AST_TRY:
			return __xlResolveTry(pCtx, pNode);
		case XL_AST_FORIN:
			return __xlResolveForIn(pCtx, pNode);
		case XL_AST_IMPORT:
			sImportPath = __xlBuildImportPath(__xlAstChild(pNode, 0));
			if ( sImportPath == NULL ) {
				return XL_RC_IO_ERROR;
			}
			iRc = __xlDeclareSymbol(
				pCtx,
				XL_SYMBOL_IMPORT,
				pCtx->iCurrentScopeId,
				sImportPath,
				"",
				pNode->iLine,
				pNode->iColumn,
				0,
				0,
				pNode
			);
			free(sImportPath);
			return iRc;
		case XL_AST_EXPORT:
			return __xlResolveExpr(pCtx, __xlAstChild(pNode, 0));
		case XL_AST_RETURN:
		case XL_AST_THROW:
		case XL_AST_YIELD:
			return __xlResolveExpr(pCtx, __xlAstChild(pNode, 0));
		case XL_AST_ASSERT:
			iRc = __xlResolveExpr(pCtx, __xlAstChild(pNode, 0));
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			if ( __xlAstChild(pNode, 1) != NULL ) {
				const char* sAssertMsgType;

				iRc = __xlResolveExpr(pCtx, __xlAstChild(pNode, 1));
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
				sAssertMsgType = __xlInferExprType(pCtx, __xlAstChild(pNode, 1));
				if ( (sAssertMsgType != NULL) && (sAssertMsgType[0] != '\0') &&
					!__xlTypeMatches("string", sAssertMsgType) ) {
					return __xlSemAddDiag(
						pCtx,
						XL_SEM_DIAG_ERROR,
						__xlAstChild(pNode, 1),
						"assert message must be string-compatible, got '%s'",
						sAssertMsgType
					);
				}
			}
			return XL_RC_OK;
		case XL_AST_BREAK:
			if ( !xlCheckJumpLevel((int)pNode->iIntValue) ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "break level must be a positive integer");
			}
			if ( !xlCheckLoopJumpAllowed(pCtx->iLoopDepth, (int)pNode->iIntValue) ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "break is not inside a matching loop");
			}
			return XL_RC_OK;
		case XL_AST_CONTINUE:
			if ( !xlCheckJumpLevel((int)pNode->iIntValue) ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "continue level must be a positive integer");
			}
			if ( !xlCheckLoopJumpAllowed(pCtx->iLoopDepth, (int)pNode->iIntValue) ) {
				return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "continue is not inside a matching loop");
			}
			return XL_RC_OK;
		case XL_AST_BLOCK:
			return __xlResolveBlock(pCtx, pNode, "block");
		default:
			return __xlResolveExpr(pCtx, pNode);
	}
}

static int __xlRegisterBuiltinTypes(XL_SemResult* pResult)
{
	static const char* arrBuiltinTypes[] = {
		"any",
		"bool",
		"error",
		"float",
		"generator",
		"int",
		"iterable",
		"iterator",
		"null",
		"range",
		"string",
		"task",
		"time",
		"void"
	};
	size_t i;
	int iRc;

	for ( i = 0; i < XL_COUNTOF(arrBuiltinTypes); ++i ) {
		iRc = xlTypeTableAdd(&pResult->types, arrBuiltinTypes[i], 1, 0, 0);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return XL_RC_OK;
}

static int __xlRegisterBuiltinFuncs(XL_SemResult* pResult)
{
	static const char* arrRange1Names[] = { "stop" };
	static const char* arrRange1Types[] = { "int" };
	static const char* arrRange2Names[] = { "start", "stop" };
	static const char* arrRange2Types[] = { "int", "int" };
	static const char* arrRange3Names[] = { "start", "stop", "step" };
	static const char* arrRange3Types[] = { "int", "int", "int" };
	static const char* arrErrorNames[] = { "message", "code", "data", "stack", "cause" };
	static const char* arrErrorTypes[] = { "string", "int", "string", "string", "error" };
	static const unsigned char arrErrorHasDefault[] = { 0, 1, 1, 1, 1 };
	const XL_AstNode* arrErrorDefaults[5];
	int iRc;

	arrErrorDefaults[0] = NULL;
	arrErrorDefaults[1] = __xlBuiltinDefaultIntZero();
	arrErrorDefaults[2] = __xlBuiltinDefaultEmptyString();
	arrErrorDefaults[3] = __xlBuiltinDefaultEmptyString();
	arrErrorDefaults[4] = __xlBuiltinDefaultNull();

	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange1Names, arrRange1Types, 1, NULL, NULL, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange2Names, arrRange2Types, 2, NULL, NULL, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange3Names, arrRange3Types, 3, NULL, NULL, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "Error", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "TypeError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "ValueError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "IOError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "TimeoutError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "CancelledError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "NotImplementedError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	return xlFuncTableAdd(&pResult->funcs, "NativeError", "error", arrErrorNames, arrErrorTypes, 5, arrErrorHasDefault, arrErrorDefaults, 0, 0, 0, 0, 0);
}

static int __xlCollectTopLevelFuncs(XL_ResolveContext* pCtx, const XL_AstNode* pRoot)
{
	const XL_AstNode* pNode;
	const XL_AstNode* pRetType;
	size_t i;
	int iRc;

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		pNode = pRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_AST_FUNC_DECL) ) {
			continue;
		}

		pRetType = __xlAstChild(pNode, 1);
		iRc = __xlDeclareSymbol(
			pCtx,
			XL_SYMBOL_FUNC,
			pCtx->iCurrentScopeId,
			pNode->sText,
			((pRetType != NULL) && (pRetType->sText != NULL)) ? pRetType->sText : "",
			pNode->iLine,
			pNode->iColumn,
			pNode->bFlag,
			__xlNodeHasYield(__xlAstChild(pNode, 2)),
			pNode
		);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}

		iRc = __xlCollectFuncInfo(pCtx, pNode, pCtx->iCurrentScopeId);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return XL_RC_OK;
}

void xlSemResultInit(XL_SemResult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}

	memset(pResult, 0, sizeof(*pResult));
	xlScopeTableInit(&pResult->scopes);
	xlSymbolTableInit(&pResult->symbols);
	xlTypeTableInit(&pResult->types);
	xlFuncTableInit(&pResult->funcs);
}

void xlSemResultFree(XL_SemResult* pResult)
{
	size_t i;

	if ( pResult == NULL ) {
		return;
	}

	for ( i = 0; i < pResult->diags.iCount; ++i ) {
		free(pResult->diags.pItems[i].sMessage);
	}
	for ( i = 0; i < pResult->nodes.iCount; ++i ) {
		free(pResult->nodes.pItems[i].sTypeName);
	}
	free(pResult->diags.pItems);
	free(pResult->nodes.pItems);
	xlScopeTableFree(&pResult->scopes);
	xlSymbolTableFree(&pResult->symbols);
	xlTypeTableFree(&pResult->types);
	xlFuncTableFree(&pResult->funcs);
	memset(pResult, 0, sizeof(*pResult));
}

int xlSemAnalyzeAst(const XL_AstNode* pRoot, XL_DiagContext* pDiag, int bTraceSem, XL_SemResult* pResult)
{
	XL_ResolveContext ctx;
	size_t i;
	int iRc;

	if ( (pRoot == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlSemResultInit(pResult);
	memset(&ctx, 0, sizeof(ctx));
	ctx.pDiag = pDiag;
	ctx.pResult = pResult;
	ctx.bTraceSem = bTraceSem;
	ctx.iCurrentScopeId = -1;

	iRc = xlScopeTableAdd(&pResult->scopes, XL_SCOPE_MODULE, -1, "<module>", pRoot->iLine, pRoot->iColumn, &ctx.iCurrentScopeId);
	if ( iRc != XL_RC_OK ) {
		xlSemResultFree(pResult);
		return iRc;
	}
	__xlSemNodeSetScope(pResult, pRoot, ctx.iCurrentScopeId);

	iRc = __xlRegisterBuiltinTypes(pResult);
	if ( iRc != XL_RC_OK ) {
		xlSemResultFree(pResult);
		return iRc;
	}

	iRc = __xlRegisterBuiltinFuncs(pResult);
	if ( iRc != XL_RC_OK ) {
		xlSemResultFree(pResult);
		return iRc;
	}

	iRc = __xlCollectTopLevelFuncs(&ctx, pRoot);
	if ( iRc != XL_RC_OK ) {
		xlSemResultFree(pResult);
		return iRc;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		iRc = __xlResolveStmt(&ctx, pRoot->ppChildren[i]);
		if ( iRc == XL_RC_IO_ERROR ) {
			xlSemResultFree(pResult);
			return iRc;
		}
	}

	return pResult->bHadError ? XL_RC_FAIL : XL_RC_OK;
}
