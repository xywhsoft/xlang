#include "xl_resolve.h"

typedef struct XL_ResolveContext {
	XL_DiagContext* pDiag;
	XL_SemResult* pResult;
	int bTraceSem;
	int iCurrentScopeId;
	int iLoopDepth;
} XL_ResolveContext;

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
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
	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_OK;
	}
	if ( __xlFindConflictingSymbolInScope(&pCtx->pResult->symbols, iScopeId, sName, eKind) >= 0 ) {
		return __xlSemAddDiag(pCtx, XL_SEM_DIAG_ERROR, pNode, "duplicate symbol '%s' in current scope", sName);
	}

	return xlSymbolTableAdd(&pCtx->pResult->symbols, eKind, iScopeId, sName, sTypeName, iLine, iColumn, bAsync, bGenerator);
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
	const char** arrParamNames;
	const char** arrParamTypes;
	size_t i;
	int iRc;
	int bGenerator;

	pParams = __xlAstChild(pNode, 0);
	pRetType = __xlAstChild(pNode, 1);
	arrParamNames = NULL;
	arrParamTypes = NULL;

	if ( (pNode == NULL) || (pNode->sText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( (pParams != NULL) && (pParams->iChildCount != 0) ) {
		arrParamNames = (const char**)calloc(pParams->iChildCount, sizeof(const char*));
		arrParamTypes = (const char**)calloc(pParams->iChildCount, sizeof(const char*));
		if ( (arrParamNames == NULL) || (arrParamTypes == NULL) ) {
			free(arrParamNames);
			free(arrParamTypes);
			return XL_RC_IO_ERROR;
		}

		for ( i = 0; i < pParams->iChildCount; ++i ) {
			pParam = pParams->ppChildren[i];
			pParamType = __xlAstChild(pParam, 0);
			arrParamNames[i] = (pParam != NULL) ? pParam->sText : "";
			arrParamTypes[i] = ((pParamType != NULL) && (pParamType->sText != NULL)) ? pParamType->sText : "";
		}
	}

	bGenerator = __xlNodeHasYield(__xlAstChild(pNode, 2));

	iRc = xlFuncTableAdd(
		&pCtx->pResult->funcs,
		pNode->sText,
		((pRetType != NULL) && (pRetType->sText != NULL)) ? pRetType->sText : "",
		arrParamNames,
		arrParamTypes,
		(pParams != NULL) ? pParams->iChildCount : 0,
		iScopeId,
		pNode->iLine,
		pNode->iColumn,
		pNode->bFlag,
		bGenerator
	);

	free(arrParamNames);
	free(arrParamTypes);
	return iRc;
}

static const char* __xlInferExprType(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveExpr(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveStmt(XL_ResolveContext* pCtx, const XL_AstNode* pNode);
static int __xlResolveForIn(XL_ResolveContext* pCtx, const XL_AstNode* pNode);

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
		sTypeName = __xlInferExprType(pCtx, pArgs->ppChildren[i]);
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

static int __xlResolveCallByName(
	XL_ResolveContext* pCtx,
	const XL_AstNode* pCallNode,
	const char* sName,
	const XL_AstNode* pArgs
)
{
	const XL_FuncInfo* pFunc;
	const XL_SymbolInfo* pNonFunc;
	const char* sArgType;
	const char* sParamType;
	size_t i;
	size_t j;
	size_t iArgCount;
	size_t iNameMatchCount;
	size_t iCompatibleCount;
	size_t iBestMatchCount;
	size_t iBestFuncIndex;
	int iScore;
	int iBestScore;
	int bCompatible;
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
		if ( pFunc->iParamCount != iArgCount ) {
			continue;
		}

		bCompatible = 1;
		iScore = 0;
		for ( j = 0; j < iArgCount; ++j ) {
			sArgType = __xlInferExprType(pCtx, pArgs->ppChildren[j]);
			sParamType = pFunc->arrParamTypes[j];
			if ( (sArgType != NULL) && (sArgType[0] != '\0') && (sParamType != NULL) && (sParamType[0] != '\0') ) {
				if ( strcmp(sArgType, sParamType) == 0 ) {
					iScore += 2;
				} else {
					bCompatible = 0;
					break;
				}
			} else {
				iScore += 1;
			}
		}

		if ( !bCompatible ) {
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
	const XL_SymbolInfo* pSymbol;
	const XL_FuncInfo* pFunc;
	const XL_AstNode* pLeft;
	const XL_AstNode* pRight;
	const XL_AstNode* pCallee;
	const char* sLeftType;
	const char* sRightType;
	size_t i;
	size_t iArgCount;
	int iBestScore;
	size_t iBestFuncIndex;
	size_t iBestMatchCount;
	int iScore;
	int bCompatible;

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
			pCallee = __xlAstChild(pNode, 0);
			if ( (pCallee == NULL) || (pCallee->eKind != XL_AST_IDENT) || (pCallee->sText == NULL) ) {
				return "";
			}

			iArgCount = (__xlAstChild(pNode, 1) != NULL) ? __xlAstChild(pNode, 1)->iChildCount : 0;
			iBestScore = -1;
			iBestFuncIndex = 0;
			iBestMatchCount = 0;
			for ( i = 0; i < pCtx->pResult->funcs.iCount; ++i ) {
				pFunc = xlFuncTableGet(&pCtx->pResult->funcs, i);
				if ( (pFunc == NULL) || (strcmp(pFunc->sName, pCallee->sText) != 0) ) {
					continue;
				}
				if ( !__xlScopeContains(&pCtx->pResult->scopes, pCtx->iCurrentScopeId, pFunc->iScopeId) ) {
					continue;
				}
				if ( pFunc->iParamCount != iArgCount ) {
					continue;
				}

				bCompatible = 1;
				iScore = 0;
				for ( size_t j = 0; j < iArgCount; ++j ) {
					sLeftType = __xlInferExprType(pCtx, __xlAstChild(__xlAstChild(pNode, 1), j));
					sRightType = pFunc->arrParamTypes[j];
					if ( (sLeftType != NULL) && (sLeftType[0] != '\0') && (sRightType != NULL) && (sRightType[0] != '\0') ) {
						if ( strcmp(sLeftType, sRightType) == 0 ) {
							iScore += 2;
						} else {
							bCompatible = 0;
							break;
						}
					} else {
						iScore += 1;
					}
				}

				if ( !bCompatible ) {
					continue;
				}
				if ( iScore > iBestScore ) {
					iBestScore = iScore;
					iBestFuncIndex = i;
					iBestMatchCount = 1;
				} else if ( iScore == iBestScore ) {
					iBestMatchCount += 1;
				}
			}

			if ( iBestMatchCount == 1 ) {
				pFunc = xlFuncTableGet(&pCtx->pResult->funcs, iBestFuncIndex);
				if ( pFunc == NULL ) {
					return "";
				}
				if ( pFunc->bGenerator ) {
					return "generator";
				}
				return pFunc->sReturnTypeName;
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
			__xlSemTrace(pCtx, pNode, "resolved identifier '%s' -> %s", pNode->sText, xlSymbolKindName(pSymbol->eKind));
			return XL_RC_OK;
		case XL_AST_CALL:
			return __xlResolveCallExpr(pCtx, pNode);
		case XL_AST_MEMBER:
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
			return XL_RC_OK;
		default:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				iRc = __xlResolveExpr(pCtx, pNode->ppChildren[i]);
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
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
	return __xlResolveExpr(pCtx, pValueNode);
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

static int __xlResolveFuncDecl(XL_ResolveContext* pCtx, const XL_AstNode* pNode)
{
	const XL_AstNode* pParams;
	const XL_AstNode* pRetType;
	const XL_AstNode* pBody;
	const XL_AstNode* pParamNode;
	const XL_AstNode* pParamType;
	int iFuncScopeId;
	int iPrevScopeId;
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

	iPrevScopeId = pCtx->iCurrentScopeId;
	pCtx->iCurrentScopeId = iFuncScopeId;
	__xlSemTrace(pCtx, pNode, "enter func '%s'", pNode->sText);

	if ( pParams != NULL ) {
		for ( i = 0; i < pParams->iChildCount; ++i ) {
			pParamNode = pParams->ppChildren[i];
			pParamType = __xlAstChild(pParamNode, 0);
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
		}
	}

	iRc = __xlResolveBlock(pCtx, pBody, "func.body");
	pCtx->iCurrentScopeId = iPrevScopeId;
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
		case XL_AST_ASSERT:
		case XL_AST_YIELD:
			return __xlResolveExpr(pCtx, __xlAstChild(pNode, 0));
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
	int iRc;

	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange1Names, arrRange1Types, 1, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange2Names, arrRange2Types, 2, 0, 0, 0, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = xlFuncTableAdd(&pResult->funcs, "range", "range", arrRange3Names, arrRange3Types, 3, 0, 0, 0, 0, 0);
	return iRc;
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
	free(pResult->diags.pItems);
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
