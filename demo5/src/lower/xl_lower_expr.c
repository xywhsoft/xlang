#include "xl_lower.h"

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);

static XL_HirKind __xlExprKind(const XL_AstNode* pNode)
{
	switch ( pNode->eKind ) {
		case XL_AST_CALL:
			return XL_HIR_CALL;
		case XL_AST_MEMBER:
			return XL_HIR_MEMBER;
		case XL_AST_INDEX:
			return XL_HIR_INDEX;
		case XL_AST_BINARY:
			return XL_HIR_BINARY;
		case XL_AST_UNARY:
			return XL_HIR_UNARY;
		case XL_AST_IDENT:
			return XL_HIR_NAME;
		default:
			return XL_HIR_LITERAL;
	}
}

static void __xlApplySemNodeInfo(XL_HirNode* pHir, const XL_SemResult* pSem, const XL_AstNode* pNode)
{
	const XL_SemNodeInfo* pInfo;
	const XL_FuncInfo* pFunc;

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
	pFunc = (pInfo->iFuncIndex >= 0) ? xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex) : NULL;
	if ( (pFunc != NULL) && (pFunc->sAbiSymbol != NULL) && (pFunc->sAbiSymbol[0] != '\0') ) {
		xlHirSetAbiName(pHir, pFunc->sAbiSymbol);
	}
	if ( (pInfo->sTypeName != NULL) && (pInfo->sTypeName[0] != '\0') ) {
		xlHirSetTypeName(pHir, pInfo->sTypeName);
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

static int __xlFindParamIndexByName(const XL_FuncInfo* pFunc, const char* sName)
{
	size_t i;

	if ( (pFunc == NULL) || (sName == NULL) ) {
		return -1;
	}

	for ( i = 0; i < pFunc->iParamCount; ++i ) {
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
	size_t i;
	size_t iNextPos;
	int iParamIndex;

	pInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pCallNode) : NULL;
	pFunc = ((pSem != NULL) && (pInfo != NULL) && (pInfo->iFuncIndex >= 0)) ? xlFuncTableGet(&pSem->funcs, (size_t)pInfo->iFuncIndex) : NULL;
	pAstArgs = __xlAstChild(pCallNode, 1);
	if ( (pFunc == NULL) || (pFunc->iParamCount == 0) ) {
		__xlLowerCallArgsOriginal(pArgs, pAstArgs, pSem, pDiag, bTraceLower);
		return;
	}

	arrBound = (const XL_AstNode**)calloc(pFunc->iParamCount, sizeof(XL_AstNode*));
	if ( arrBound == NULL ) {
		__xlLowerCallArgsOriginal(pArgs, pAstArgs, pSem, pDiag, bTraceLower);
		return;
	}

	iNextPos = 0;
	for ( i = 0; (pAstArgs != NULL) && (i < pAstArgs->iChildCount); ++i ) {
		pArg = pAstArgs->ppChildren[i];
		pValue = __xlArgValue(pArg);
		if ( __xlArgName(pArg) != NULL ) {
			iParamIndex = __xlFindParamIndexByName(pFunc, __xlArgName(pArg));
		} else {
			while ( (iNextPos < pFunc->iParamCount) && (arrBound[iNextPos] != NULL) ) {
				++iNextPos;
			}
			iParamIndex = (iNextPos < pFunc->iParamCount) ? (int)iNextPos : -1;
			++iNextPos;
		}

		if ( (iParamIndex >= 0) && ((size_t)iParamIndex < pFunc->iParamCount) ) {
			arrBound[iParamIndex] = pValue;
		}
	}

	for ( i = 0; i < pFunc->iParamCount; ++i ) {
		pValue = arrBound[i];
		if ( (pValue == NULL) && (pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[i] ) {
			pValue = pFunc->arrParamDefaultValues[i];
		}
		if ( pValue != NULL ) {
			xlHirAppendChild(pArgs, xlLowerExprToHir(pValue, pSem, pDiag, bTraceLower));
		}
	}

	free(arrBound);
}

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pHir;
	XL_HirNode* pArgs;
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
		xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[0], pSem, pDiag, bTraceLower));

		pArgs = xlHirCreateNode(XL_HIR_BLOCK, pNode);
		if ( pArgs != NULL ) {
			xlHirSetText(pArgs, "args");
			__xlApplySemNodeInfo(pArgs, pSem, pNode->ppChildren[1]);
			__xlLowerCallArgsResolved(pArgs, pNode, pSem, pDiag, bTraceLower);
			xlHirAppendChild(pHir, pArgs);
		}
		return pHir;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[i], pSem, pDiag, bTraceLower));
	}

	return pHir;
}
