#include "xl_lower.h"
XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);

static XL_HirKind __xlExprKind(const XL_AstNode* pNode)
{
	switch ( pNode->eKind ) {
		case XL_AST_CALL:
			return XL_HIR_CALL;
		case XL_AST_FUNC_EXPR:
			return XL_HIR_NAME;
		case XL_AST_MEMBER:
			return XL_HIR_MEMBER;
		case XL_AST_INDEX:
			return XL_HIR_INDEX;
		case XL_AST_ASSIGN:
		case XL_AST_TERNARY:
		case XL_AST_BINARY:
			return XL_HIR_BINARY;
		case XL_AST_UNARY:
			return XL_HIR_UNARY;
		case XL_AST_IDENT:
			return XL_HIR_NAME;
		case XL_AST_YIELD:
			return XL_HIR_YIELD;
		default:
			return XL_HIR_LITERAL;
	}
}

static void __xlApplySemNodeInfo(XL_HirNode* pHir, const XL_SemResult* pSem, const XL_AstNode* pNode)
{
	const XL_SemNodeInfo* pInfo;
	const XL_FuncInfo* pFunc;
	const XL_SymbolInfo* pSymbol;

	if ( (pHir == NULL) || (pSem == NULL) || (pNode == NULL) ) {
		return;
	}

	pInfo = xlSemNodeInfoFind(pSem, pNode);
	if ( pInfo == NULL ) {
		return;
	}

	pHir->iScopeId = pInfo->iScopeId;
	pHir->iSymbolIndex = pInfo->iSymbolIndex;
	pHir->iFuncIndex = pInfo->iFuncIndex;
	pSymbol = (pInfo->iSymbolIndex >= 0) ? xlSymbolTableGet(&pSem->symbols, (size_t)pInfo->iSymbolIndex) : NULL;
	pFunc = (pInfo->iFuncIndex >= 0) ? xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex) : NULL;
	if ( (pFunc != NULL) && (pFunc->sAbiSymbol != NULL) && (pFunc->sAbiSymbol[0] != '\0') ) {
		xlHirSetAbiName(pHir, pFunc->sAbiSymbol);
	}
	if ( (pInfo->sTypeName != NULL) && (pInfo->sTypeName[0] != '\0') ) {
		xlHirSetTypeName(pHir, pInfo->sTypeName);
	}
	if ( (pSymbol != NULL) && (pSymbol->sTypeName != NULL) && (strcmp(pSymbol->sTypeName, "var") == 0) ) {
		pHir->bFlag |= XL_HIR_FLAG_STORAGE_VAR;
	} else if ( (pSem != NULL) && (pSymbol != NULL) &&
		((pSymbol->eKind == XL_SYMBOL_CONST) || (pSymbol->eKind == XL_SYMBOL_VAR) || (pSymbol->eKind == XL_SYMBOL_IMPORT)) &&
		(pSymbol->sTypeName != NULL) && (pSymbol->sTypeName[0] != '\0') ) {
		size_t i;

		for ( i = 0; i < pSem->classes.iCount; ++i ) {
			const XL_ClassInfo* pClass = &pSem->classes.pItems[i];

			if ( (pClass->sName != NULL) && (strcmp(pClass->sName, pSymbol->sTypeName) == 0) && (pClass->bIsObject != 0) ) {
				pHir->bFlag |= XL_HIR_FLAG_STORAGE_VAR;
				break;
			}
		}
	}
}

static void __xlAnnotateImplicitConversion(XL_HirNode* pHir, const char* sExpectedType, const XL_SemResult* pSem)
{
	const char* sActualType;
	char sText[256];

	if ( (pHir == NULL) || (sExpectedType == NULL) || (sExpectedType[0] == '\0') ) {
		return;
	}
	sActualType = pHir->sTypeName;
	if ( !xlLowerCanAnnotateImplicitConversion(pSem, sExpectedType, sActualType) ) {
		return;
	}
	snprintf(sText, sizeof(sText), "%s->%s", sActualType, sExpectedType);
	xlHirSetConversion(pHir, sText);
}

static int __xlLowerTypeHasBaseName(const char* sTypeName, const char* sBaseName)
{
	size_t iBaseLen;

	if ( (sTypeName == NULL) || (sBaseName == NULL) ) {
		return 0;
	}
	iBaseLen = strlen(sBaseName);
	if ( strncmp(sTypeName, sBaseName, iBaseLen) != 0 ) {
		return 0;
	}
	return (sTypeName[iBaseLen] == '\0') || (sTypeName[iBaseLen] == '<');
}

static int __xlLowerExtractContainerValueType(const char* sTypeName, const char* sBaseName, char* sOut, size_t iOutSize)
{
	const char* sStart;
	const char* sEnd;
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	if ( !__xlLowerTypeHasBaseName(sTypeName, sBaseName) ) {
		return 0;
	}
	iLen = strlen(sBaseName);
	if ( sTypeName[iLen] == '\0' ) {
		snprintf(sOut, iOutSize, "%s", "any");
		return 1;
	}
	sStart = sTypeName + iLen + 1;
	sEnd = strrchr(sStart, '>');
	if ( (sEnd == NULL) || (sEnd <= sStart) ) {
		return 0;
	}
	iLen = (size_t)(sEnd - sStart);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}
	memcpy(sOut, sStart, iLen);
	sOut[iLen] = '\0';
	return sOut[0] != '\0';
}

static int __xlLowerExtractLiteralItemType(const char* sTypeName, char* sOut, size_t iOutSize)
{
	if ( __xlLowerExtractContainerValueType(sTypeName, "array", sOut, iOutSize) ) {
		return 1;
	}
	if ( __xlLowerExtractContainerValueType(sTypeName, "list", sOut, iOutSize) ) {
		return 1;
	}
	if ( __xlLowerExtractContainerValueType(sTypeName, "set", sOut, iOutSize) ) {
		return 1;
	}
	return __xlLowerExtractContainerValueType(sTypeName, "dict", sOut, iOutSize);
}

static int __xlLowerIsNumericCommonBinaryOp(const char* sOp)
{
	if ( sOp == NULL ) {
		return 0;
	}
	return (strcmp(sOp, "+") == 0) ||
		(strcmp(sOp, "-") == 0) ||
		(strcmp(sOp, "*") == 0) ||
		(strcmp(sOp, "/") == 0) ||
		(strcmp(sOp, "%") == 0) ||
		(strcmp(sOp, "**") == 0);
}

static int __xlLowerIsEqualityOp(const char* sOp)
{
	return (sOp != NULL) && ((strcmp(sOp, "==") == 0) || (strcmp(sOp, "!=") == 0));
}

static const char* __xlLowerCallCalleeText(const XL_AstNode* pNode)
{
	const XL_AstNode* pCallee;

	pCallee = ((pNode != NULL) && (pNode->iChildCount > 0)) ? pNode->ppChildren[0] : NULL;
	return (pCallee != NULL) ? pCallee->sText : NULL;
}

static void __xlAnnotateLiteralCallConversions(XL_HirNode* pArgs, const char* sCallType, const char* sCalleeName, const XL_SemResult* pSem)
{
	char sItemType[128];
	size_t i;

	if ( (pArgs == NULL) || (sCalleeName == NULL) || !__xlLowerExtractLiteralItemType(sCallType, sItemType, sizeof(sItemType)) ) {
		return;
	}
	if ( (strcmp(sCalleeName, "__array_literal") == 0) ||
		(strcmp(sCalleeName, "__list_literal") == 0) ||
		(strcmp(sCalleeName, "__set_literal") == 0) ) {
		for ( i = 0; i < pArgs->iChildCount; ++i ) {
			__xlAnnotateImplicitConversion(pArgs->ppChildren[i], sItemType, pSem);
		}
		return;
	}
	if ( strcmp(sCalleeName, "__dict_literal") == 0 ) {
		for ( i = 1; i < pArgs->iChildCount; i += 2 ) {
			__xlAnnotateImplicitConversion(pArgs->ppChildren[i], sItemType, pSem);
		}
	}
}

static void __xlAnnotateCommonExprConversions(const XL_AstNode* pNode, XL_HirNode* pHir, const XL_SemResult* pSem)
{
	if ( (pNode == NULL) || (pHir == NULL) || (pHir->sTypeName == NULL) || (pHir->sTypeName[0] == '\0') ) {
		return;
	}
	if ( (pNode->eKind == XL_AST_BINARY) && __xlLowerIsNumericCommonBinaryOp(pNode->sText) ) {
		if ( pHir->iChildCount >= 1 ) {
			__xlAnnotateImplicitConversion(pHir->ppChildren[0], pHir->sTypeName, pSem);
		}
		if ( pHir->iChildCount >= 2 ) {
			__xlAnnotateImplicitConversion(pHir->ppChildren[1], pHir->sTypeName, pSem);
		}
		return;
	}
	if ( (pNode->eKind == XL_AST_BINARY) && __xlLowerIsEqualityOp(pNode->sText) && (pHir->iChildCount >= 2) ) {
		XL_HirNode* pLeft = pHir->ppChildren[0];
		XL_HirNode* pRight = pHir->ppChildren[1];

		if ( (pLeft != NULL) && (pRight != NULL) &&
			xlLowerCanAnnotateImplicitConversion(pSem, pRight->sTypeName, pLeft->sTypeName) ) {
			__xlAnnotateImplicitConversion(pLeft, pRight->sTypeName, pSem);
		} else if ( (pLeft != NULL) && (pRight != NULL) &&
			xlLowerCanAnnotateImplicitConversion(pSem, pLeft->sTypeName, pRight->sTypeName) ) {
			__xlAnnotateImplicitConversion(pRight, pLeft->sTypeName, pSem);
		}
		return;
	}
	if ( pNode->eKind == XL_AST_TERNARY ) {
		if ( pHir->iChildCount >= 2 ) {
			__xlAnnotateImplicitConversion(pHir->ppChildren[1], pHir->sTypeName, pSem);
		}
		if ( pHir->iChildCount >= 3 ) {
			__xlAnnotateImplicitConversion(pHir->ppChildren[2], pHir->sTypeName, pSem);
		}
	}
}

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
	if ( (pNode != NULL) && (pNode->eKind == XL_AST_NAMED_ARG) ) {
		return __xlAstChild(pNode, 0);
	}

	return pNode;
}

static int __xlIsFunctionTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strncmp(sTypeName, "function<", 9) == 0);
}

static size_t __xlFunctionTypeReturnCount(const char* sTypeName)
{
	const char* sScan;
	const char* sReturnStart;
	size_t iAngleDepth;
	size_t iCount;

	if ( !__xlIsFunctionTypeName(sTypeName) ) {
		return 0;
	}

	sReturnStart = sTypeName + 9;
	sScan = sReturnStart;
	iAngleDepth = 0;
	iCount = 1;
	while ( *sScan != '\0' ) {
		if ( *sScan == '<' ) {
			++iAngleDepth;
		} else if ( (*sScan == '>') && (iAngleDepth != 0) ) {
			--iAngleDepth;
		} else if ( (*sScan == '(') && (iAngleDepth == 0) ) {
			break;
		} else if ( (*sScan == ',') && (iAngleDepth == 0) ) {
			++iCount;
		}
		++sScan;
	}

	return ((*sScan == '(') && (sScan > sReturnStart)) ? iCount : 0;
}

static const XL_FuncInfo* __xlResolvedFuncInfo(const XL_SemResult* pSem, const XL_AstNode* pNode)
{
	const XL_SemNodeInfo* pInfo;

	if ( (pSem == NULL) || (pNode == NULL) ) {
		return NULL;
	}

	pInfo = xlSemNodeInfoFind(pSem, pNode);
	if ( (pInfo == NULL) || (pInfo->iFuncIndex < 0) ) {
		return NULL;
	}
	return xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex);
}

static const XL_FuncInfo* __xlExprMultiReturnFunc(const XL_SemResult* pSem, const XL_AstNode* pNode)
{
	const XL_FuncInfo* pFunc;
	const XL_AstNode* pTargetNode;

	if ( pNode == NULL ) {
		return NULL;
	}

	pTargetNode = pNode;
	if ( (pNode->eKind == XL_AST_UNARY) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) ) {
		pTargetNode = __xlAstChild(pNode, 0);
	}

	if ( (pTargetNode == NULL) || (pTargetNode->eKind != XL_AST_CALL) ) {
		return NULL;
	}

	pFunc = __xlResolvedFuncInfo(pSem, pTargetNode);
	return ((pFunc != NULL) && (pFunc->iReturnCount > 1)) ? pFunc : NULL;
}

static size_t __xlExprMultiReturnCount(const XL_SemResult* pSem, const XL_AstNode* pNode)
{
	const XL_FuncInfo* pFunc;
	const XL_AstNode* pTargetNode;
	const XL_AstNode* pCallee;
	const XL_SemNodeInfo* pInfo;
	const XL_SemNodeInfo* pCallInfo;

	pFunc = __xlExprMultiReturnFunc(pSem, pNode);
	if ( pFunc != NULL ) {
		return pFunc->iReturnCount;
	}

	pTargetNode = pNode;
	if ( (pNode != NULL) && (pNode->eKind == XL_AST_UNARY) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) ) {
		pTargetNode = __xlAstChild(pNode, 0);
	}
	if ( (pTargetNode == NULL) || (pTargetNode->eKind != XL_AST_CALL) ) {
		return 0;
	}

	pCallInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pTargetNode) : NULL;
	if ( (pCallInfo != NULL) && (pCallInfo->iFuncIndex >= 0) ) {
		return 0;
	}

	pCallee = __xlAstChild(pTargetNode, 0);
	pInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pCallee) : NULL;
	if ( (pInfo == NULL) || (pInfo->sTypeName == NULL) ) {
		return 0;
	}
	return __xlFunctionTypeReturnCount(pInfo->sTypeName);
}

static int __xlFindParamIndexByNameFrom(const XL_FuncInfo* pFunc, const char* sName, size_t iStartIndex)
{
	size_t i;

	if ( (pFunc == NULL) || (sName == NULL) ) {
		return -1;
	}

	for ( i = iStartIndex; i < pFunc->iParamCount; ++i ) {
		if ( (pFunc->arrParamNames[i] != NULL) && (strcmp(pFunc->arrParamNames[i], sName) == 0) ) {
			return (int)i;
		}
	}

	return -1;
}

static void __xlLowerCallArgsOriginal(
	XL_HirNode* pArgs,
	const XL_AstNode* pAstArgs,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	size_t i;

	for ( i = 0; (pAstArgs != NULL) && (i < pAstArgs->iChildCount); ++i ) {
		xlHirAppendChild(pArgs, xlLowerExprToHir(__xlArgValue(pAstArgs->ppChildren[i]), pSem, pDiag, bTraceLower));
	}
}

static XL_HirNode* __xlCreateHiddenCallHir(const XL_AstNode* pSource, const char* sName, const char* sTypeName)
{
	XL_HirNode* pCall;
	XL_HirNode* pCallee;
	XL_HirNode* pArgs;

	pCall = xlHirCreateNode(XL_HIR_CALL, pSource);
	pCallee = xlHirCreateNode(XL_HIR_NAME, pSource);
	pArgs = xlHirCreateNode(XL_HIR_BLOCK, pSource);
	if ( (pCall == NULL) || (pCallee == NULL) || (pArgs == NULL) ) {
		xlHirFree(pCall);
		xlHirFree(pCallee);
		xlHirFree(pArgs);
		return NULL;
	}

	xlHirSetText(pCallee, sName);
	xlHirSetText(pArgs, "args");
	if ( (sTypeName != NULL) && (sTypeName[0] != '\0') ) {
		xlHirSetTypeName(pCall, sTypeName);
	}
	xlHirAppendChild(pCall, pCallee);
	xlHirAppendChild(pCall, pArgs);
	return pCall;
}

static XL_HirNode* __xlCreateStringLiteralHir(const XL_AstNode* pSource, const char* sText)
{
	XL_HirNode* pNode;
	char* sQuoted;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sQuoted = (char*)malloc(iLen + 3);
	if ( sQuoted == NULL ) {
		return NULL;
	}

	sQuoted[0] = '"';
	memcpy(sQuoted + 1, sText, iLen);
	sQuoted[iLen + 1] = '"';
	sQuoted[iLen + 2] = '\0';

	pNode = xlHirCreateNode(XL_HIR_LITERAL, pSource);
	if ( pNode != NULL ) {
		xlHirSetText(pNode, sQuoted);
		xlHirSetTypeName(pNode, "string");
	}

	free(sQuoted);
	return pNode;
}

static XL_HirNode* __xlCreateIntLiteralHir(const XL_AstNode* pSource, int64_t iValue)
{
	XL_HirNode* pNode;
	char sText[32];

	pNode = xlHirCreateNode(XL_HIR_LITERAL, pSource);
	if ( pNode != NULL ) {
		snprintf(sText, sizeof(sText), "%lld", (long long)iValue);
		xlHirSetText(pNode, sText);
		xlHirSetTypeName(pNode, "int");
		pNode->iIntValue = iValue;
	}
	return pNode;
}

static XL_HirNode* __xlCreateExpandedMultiReturnItemHir(
	const XL_AstNode* pSource,
	const XL_AstNode* pCallNode,
	size_t iItemIndex,
	const char* sTypeName,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	XL_HirNode* pIndex;

	pIndex = xlHirCreateNode(XL_HIR_INDEX, pSource);
	if ( pIndex == NULL ) {
		return NULL;
	}

	xlHirSetText(pIndex, "__mret_item__");
	if ( (sTypeName != NULL) && (sTypeName[0] != '\0') ) {
		xlHirSetTypeName(pIndex, sTypeName);
	}
	xlHirAppendChild(pIndex, xlLowerExprToHir(pCallNode, pSem, pDiag, bTraceLower));
	xlHirAppendChild(pIndex, __xlCreateIntLiteralHir(pSource, (int64_t)iItemIndex));
	return pIndex;
}

static void __xlLowerCallArgsResolved(
	XL_HirNode* pArgs,
	const XL_AstNode* pCallNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_SemNodeInfo* pInfo;
	const XL_FuncInfo* pFunc;
	const XL_AstNode* pAstArgs;
	const XL_AstNode* pArg;
	const XL_AstNode* pValue;
	const XL_AstNode** arrBound;
	int* arrBoundExpandIndex;
	const XL_AstNode* pSelfValue;
	const XL_AstNode** arrVarArgValues;
	const char** arrVarArgNames;
	int* arrVarArgExpandIndex;
	const XL_AstNode* pPackedArgValue;
	size_t iFixedParamCount;
	size_t iVarArgIndex;
	size_t iVarArgCount;
	size_t i;
	size_t iNextPos;
	size_t iStartIndex;
	int iParamIndex;

	pInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pCallNode) : NULL;
	pFunc = ((pSem != NULL) && (pInfo != NULL) && (pInfo->iFuncIndex >= 0)) ? xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex) : NULL;
	pAstArgs = __xlAstChild(pCallNode, 1);
	if ( (pFunc == NULL) || (pFunc->iParamCount == 0) ) {
		__xlLowerCallArgsOriginal(pArgs, pAstArgs, pSem, pDiag, bTraceLower);
		return;
	}
	iVarArgIndex = (pFunc->eVarArgKind != XL_FUNC_VARARG_NONE) ? (pFunc->iParamCount - 1) : pFunc->iParamCount;
	iFixedParamCount = iVarArgIndex;

	arrBound = (const XL_AstNode**)calloc(pFunc->iParamCount, sizeof(XL_AstNode*));
	arrBoundExpandIndex = (int*)calloc(pFunc->iParamCount, sizeof(int));
	arrVarArgValues = (const XL_AstNode**)calloc(((pAstArgs != NULL) && (pAstArgs->iChildCount != 0)) ? pAstArgs->iChildCount : 1, sizeof(XL_AstNode*));
	arrVarArgNames = (const char**)calloc(((pAstArgs != NULL) && (pAstArgs->iChildCount != 0)) ? pAstArgs->iChildCount : 1, sizeof(const char*));
	arrVarArgExpandIndex = (int*)calloc(((pAstArgs != NULL) && (pAstArgs->iChildCount != 0)) ? pAstArgs->iChildCount : 1, sizeof(int));
	pPackedArgValue = NULL;
	iVarArgCount = 0;
	if ( (arrBound == NULL) || (arrBoundExpandIndex == NULL) || (arrVarArgValues == NULL) || (arrVarArgNames == NULL) || (arrVarArgExpandIndex == NULL) ) {
		free(arrBound);
		free(arrBoundExpandIndex);
		free(arrVarArgValues);
		free(arrVarArgNames);
		free(arrVarArgExpandIndex);
		__xlLowerCallArgsOriginal(pArgs, pAstArgs, pSem, pDiag, bTraceLower);
		return;
	}
	memset(arrBoundExpandIndex, 0xFF, sizeof(int) * pFunc->iParamCount);
	memset(arrVarArgExpandIndex, 0xFF, sizeof(int) * ((((pAstArgs != NULL) && (pAstArgs->iChildCount != 0)) ? pAstArgs->iChildCount : 1)));

	pSelfValue = NULL;
	iNextPos = 0;
	iStartIndex = 0;
	if ( (pFunc->sReceiverType != NULL) && (pFunc->sReceiverType[0] != '\0') ) {
		const XL_AstNode* pCallee;

		pCallee = __xlAstChild(pCallNode, 0);
		pSelfValue = ((pCallee != NULL) && (pCallee->eKind == XL_AST_MEMBER)) ? __xlAstChild(pCallee, 0) : NULL;
		if ( pSelfValue == NULL ) {
			pSelfValue = pCallee;
		}
		if ( pSelfValue == NULL ) {
			free(arrBound);
			free(arrVarArgValues);
			free(arrVarArgNames);
			__xlLowerCallArgsOriginal(pArgs, pAstArgs, pSem, pDiag, bTraceLower);
			return;
		}
		arrBound[0] = pSelfValue;
		iNextPos = 1;
		iStartIndex = 1;
	}

	for ( i = 0; (pAstArgs != NULL) && (i < pAstArgs->iChildCount); ++i ) {
		pArg = pAstArgs->ppChildren[i];
		pValue = __xlArgValue(pArg);
		if ( (__xlArgName(pArg) != NULL) && (pFunc->eVarArgKind != XL_FUNC_VARARG_NONE) && (strcmp(__xlArgName(pArg), "arg") == 0) ) {
			pPackedArgValue = pValue;
			continue;
		}
		if ( (__xlArgName(pArg) == NULL) && (__xlExprMultiReturnCount(pSem, pValue) > 1) ) {
			const XL_FuncInfo* pExpandedFunc;
			size_t iExpandedCount;
			size_t j;

			pExpandedFunc = __xlExprMultiReturnFunc(pSem, pValue);
			iExpandedCount = (pExpandedFunc != NULL) ? pExpandedFunc->iReturnCount : __xlExprMultiReturnCount(pSem, pValue);
			for ( j = 0; j < iExpandedCount; ++j ) {
				while ( (iNextPos < iFixedParamCount) && (arrBound[iNextPos] != NULL) ) {
					++iNextPos;
				}
				iParamIndex = (iNextPos < iFixedParamCount) ? (int)iNextPos : -1;
				if ( iParamIndex >= 0 ) {
					arrBound[iParamIndex] = pValue;
					arrBoundExpandIndex[iParamIndex] = (int)j;
					++iNextPos;
				} else if ( pFunc->eVarArgKind == XL_FUNC_VARARG_ARRAY ) {
					arrVarArgNames[iVarArgCount] = NULL;
					arrVarArgValues[iVarArgCount] = pValue;
					arrVarArgExpandIndex[iVarArgCount] = (int)j;
					iVarArgCount += 1;
				}
			}
			continue;
		}
		if ( __xlArgName(pArg) != NULL ) {
			iParamIndex = __xlFindParamIndexByNameFrom(pFunc, __xlArgName(pArg), iStartIndex);
		} else {
			while ( (iNextPos < iFixedParamCount) && (arrBound[iNextPos] != NULL) ) {
				++iNextPos;
			}
			iParamIndex = (iNextPos < iFixedParamCount) ? (int)iNextPos : -1;
			++iNextPos;
		}

		if ( (iParamIndex >= 0) && ((size_t)iParamIndex < iFixedParamCount) ) {
			arrBound[iParamIndex] = pValue;
		} else if ( pFunc->eVarArgKind != XL_FUNC_VARARG_NONE ) {
			arrVarArgNames[iVarArgCount] = __xlArgName(pArg);
			arrVarArgValues[iVarArgCount] = pValue;
			iVarArgCount += 1;
		}
	}

	for ( i = 0; i < iFixedParamCount; ++i ) {
		pValue = arrBound[i];
		if ( (pValue == NULL) && (pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[i] ) {
			pValue = pFunc->arrParamDefaultValues[i];
		}
		if ( pValue != NULL ) {
			if ( arrBoundExpandIndex[i] >= 0 ) {
				xlHirAppendChild(
					pArgs,
					__xlCreateExpandedMultiReturnItemHir(
						pValue,
						pValue,
						(size_t)arrBoundExpandIndex[i],
						(pFunc->arrParamTypes != NULL) ? pFunc->arrParamTypes[i] : NULL,
						pSem,
						pDiag,
						bTraceLower
					)
				);
			} else {
				XL_HirNode* pArgHir;

				pArgHir = xlLowerExprToHir(pValue, pSem, pDiag, bTraceLower);
				__xlAnnotateImplicitConversion(pArgHir, (pFunc->arrParamTypes != NULL) ? pFunc->arrParamTypes[i] : NULL, pSem);
				xlHirAppendChild(pArgs, pArgHir);
			}
		}
	}
	if ( pFunc->eVarArgKind == XL_FUNC_VARARG_ARRAY ) {
		XL_HirNode* pPackedCall;
		XL_HirNode* pPackedArgs;

		if ( pPackedArgValue != NULL ) {
			XL_HirNode* pPackedHir;

			pPackedHir = xlLowerExprToHir(pPackedArgValue, pSem, pDiag, bTraceLower);
			__xlAnnotateImplicitConversion(pPackedHir, (pFunc->arrParamTypes != NULL) ? pFunc->arrParamTypes[iVarArgIndex] : NULL, pSem);
			xlHirAppendChild(pArgs, pPackedHir);
		} else {
			pPackedCall = __xlCreateHiddenCallHir(pCallNode, "__array_literal", "array<var>");
			pPackedArgs = (pPackedCall != NULL) ? pPackedCall->ppChildren[1] : NULL;
			for ( i = 0; i < iVarArgCount; ++i ) {
				if ( arrVarArgExpandIndex[i] >= 0 ) {
					xlHirAppendChild(
						pPackedArgs,
						__xlCreateExpandedMultiReturnItemHir(
							arrVarArgValues[i],
							arrVarArgValues[i],
							(size_t)arrVarArgExpandIndex[i],
							"var",
							pSem,
							pDiag,
							bTraceLower
						)
					);
				} else {
					xlHirAppendChild(pPackedArgs, xlLowerExprToHir(arrVarArgValues[i], pSem, pDiag, bTraceLower));
				}
			}
			__xlAnnotateLiteralCallConversions(pPackedArgs, "array<var>", "__array_literal", pSem);
			xlHirAppendChild(pArgs, pPackedCall);
		}
	} else if ( pFunc->eVarArgKind == XL_FUNC_VARARG_DICT ) {
		XL_HirNode* pPackedCall;
		XL_HirNode* pPackedArgs;

		if ( pPackedArgValue != NULL ) {
			XL_HirNode* pPackedHir;

			pPackedHir = xlLowerExprToHir(pPackedArgValue, pSem, pDiag, bTraceLower);
			__xlAnnotateImplicitConversion(pPackedHir, (pFunc->arrParamTypes != NULL) ? pFunc->arrParamTypes[iVarArgIndex] : NULL, pSem);
			xlHirAppendChild(pArgs, pPackedHir);
		} else {
			pPackedCall = __xlCreateHiddenCallHir(pCallNode, "__dict_literal", "dict<var>");
			pPackedArgs = (pPackedCall != NULL) ? pPackedCall->ppChildren[1] : NULL;
			for ( i = 0; i < iVarArgCount; ++i ) {
				xlHirAppendChild(pPackedArgs, __xlCreateStringLiteralHir(pCallNode, (arrVarArgNames[i] != NULL) ? arrVarArgNames[i] : ""));
				xlHirAppendChild(pPackedArgs, xlLowerExprToHir(arrVarArgValues[i], pSem, pDiag, bTraceLower));
			}
			__xlAnnotateLiteralCallConversions(pPackedArgs, "dict<var>", "__dict_literal", pSem);
			xlHirAppendChild(pArgs, pPackedCall);
		}
	}

	free(arrBound);
	free(arrBoundExpandIndex);
	free(arrVarArgValues);
	free(arrVarArgNames);
	free(arrVarArgExpandIndex);
}

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pHir;
	XL_HirNode* pArgs;
	XL_HirNode* pCalleeHir;
	const XL_SemNodeInfo* pInfo;
	const XL_FuncInfo* pFunc;
	size_t i;

	XL_UNUSED(bTraceLower);
	XL_UNUSED(pDiag);

	if ( pNode == NULL ) {
		return NULL;
	}

	pHir = xlHirCreateNode(__xlExprKind(pNode), pNode);
	if ( pHir == NULL ) {
		return NULL;
	}

	xlHirSetText(pHir, pNode->sText);
	__xlApplySemNodeInfo(pHir, pSem, pNode);
	if ( (pHir->sTypeName == NULL) || (pHir->sTypeName[0] == '\0') ) {
		switch ( pNode->eKind ) {
			case XL_AST_INT:
				xlHirSetTypeName(pHir, "int");
				break;
			case XL_AST_FLOAT:
				xlHirSetTypeName(pHir, "float");
				break;
			case XL_AST_STRING:
			case XL_AST_TEMPLATE:
				xlHirSetTypeName(pHir, "string");
				break;
			case XL_AST_TIME:
				xlHirSetTypeName(pHir, "time");
				break;
			case XL_AST_BOOL:
				xlHirSetTypeName(pHir, "bool");
				break;
			case XL_AST_NULL:
				xlHirSetTypeName(pHir, "null");
				break;
			default:
				break;
		}
	}
	if ( pNode->eKind == XL_AST_INT ) {
		pHir->iIntValue = pNode->iIntValue;
	} else if ( pNode->eKind == XL_AST_FLOAT ) {
		pHir->fFloatValue = pNode->fFloatValue;
	} else {
		pHir->bFlag = pNode->bFlag;
	}

	if ( pNode->eKind == XL_AST_CALL ) {
		pInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pNode) : NULL;
		pFunc = ((pSem != NULL) && (pInfo != NULL) && (pInfo->iFuncIndex >= 0)) ? xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex) : NULL;
		if ( (pFunc != NULL) && (pFunc->sReceiverType != NULL) && (pFunc->sReceiverType[0] != '\0') ) {
			pCalleeHir = xlHirCreateNode(XL_HIR_NAME, pNode->ppChildren[0]);
			if ( pCalleeHir != NULL ) {
				xlHirSetText(pCalleeHir, pFunc->sName);
				pCalleeHir->iFuncIndex = (pInfo != NULL) ? pInfo->iFuncIndex : -1;
				if ( (pFunc->sAbiSymbol != NULL) && (pFunc->sAbiSymbol[0] != '\0') ) {
					xlHirSetAbiName(pCalleeHir, pFunc->sAbiSymbol);
				}
			}
			xlHirAppendChild(pHir, pCalleeHir);
		} else {
			xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[0], pSem, pDiag, bTraceLower));
		}

		pArgs = xlHirCreateNode(XL_HIR_BLOCK, pNode);
		if ( pArgs != NULL ) {
			xlHirSetText(pArgs, "args");
			__xlApplySemNodeInfo(pArgs, pSem, pNode->ppChildren[1]);
			__xlLowerCallArgsResolved(pArgs, pNode, pSem, pDiag, bTraceLower);
			__xlAnnotateLiteralCallConversions(pArgs, pHir->sTypeName, __xlLowerCallCalleeText(pNode), pSem);
			xlHirAppendChild(pHir, pArgs);
		}
		return pHir;
	}

	if ( pNode->eKind == XL_AST_FUNC_EXPR ) {
		const XL_FuncInfo* pFuncInfo;

		pFuncInfo = __xlResolvedFuncInfo(pSem, pNode);
		if ( (pFuncInfo != NULL) && (pFuncInfo->sName != NULL) && (pFuncInfo->sName[0] != '\0') ) {
			xlHirSetText(pHir, pFuncInfo->sName);
		}
		return pHir;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[i], pSem, pDiag, bTraceLower));
	}
	if ( (pNode->eKind == XL_AST_ASSIGN) && (pHir->iChildCount >= 2) && (pHir->ppChildren[0] != NULL) ) {
		__xlAnnotateImplicitConversion(pHir->ppChildren[1], pHir->ppChildren[0]->sTypeName, pSem);
	}
	__xlAnnotateCommonExprConversions(pNode, pHir, pSem);

	return pHir;
}
