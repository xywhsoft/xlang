#include "xl_lower.h"

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);
void xlLowerAsyncTouch(void);
void xlLowerGeneratorTouch(void);

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

static const XL_AstNode* __xlFuncReceiverNode(const XL_AstNode* pNode)
{
	if ( (pNode == NULL) || (pNode->eKind != XL_AST_FUNC_DECL) || (pNode->iChildCount <= 3) ) {
		return NULL;
	}

	return pNode->ppChildren[3];
}

static void __xlLowerTrace(XL_DiagContext* pDiag, int bTraceLower, const XL_AstNode* pNode, const char* sFormat, ...)
{
	va_list args;
	char sText[XL_DIAG_TEXT_MAX];
	int iOffset;

	if ( !bTraceLower || (pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}

	iOffset = snprintf(
		sText,
		sizeof(sText),
		"node=%s@%d:%d ",
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

	xlDiagEmit(pDiag, XL_DIAG_LEVEL_INFO, "lower", __FILE__, __LINE__, "%s", sText);
}

static XL_HirNode* __xlLowerStmtToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);

static XL_HirNode* __xlLowerBlockToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pBlock;
	size_t i;

	pBlock = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	if ( pBlock == NULL ) {
		return NULL;
	}
	__xlApplySemNodeInfo(pBlock, pSem, pNode);

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirAppendChild(pBlock, __xlLowerStmtToHir(pNode->ppChildren[i], pSem, pDiag, bTraceLower));
	}

	return pBlock;
}

static XL_HirNode* __xlLowerFuncToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pFunc;
	XL_HirNode* pParams;
	const XL_AstNode* pAstParams;
	const XL_AstNode* pParam;
	const XL_AstNode* pParamType;
	size_t i;

	pFunc = xlHirCreateNode(XL_HIR_FUNC, pNode);
	if ( pFunc == NULL ) {
		return NULL;
	}

	xlHirSetText(pFunc, pNode->sText);
	if ( (__xlFuncReceiverNode(pNode) == NULL) && (pNode->sText2 != NULL) && (pNode->sText2[0] != '\0') ) {
		xlHirSetExternName(pFunc, pNode->sText2);
	}
	__xlApplySemNodeInfo(pFunc, pSem, pNode);
	if ( ((pFunc->sTypeName == NULL) || (pFunc->sTypeName[0] == '\0')) && (__xlAstChild(pNode, 1) != NULL) ) {
		xlHirSetTypeName(pFunc, __xlAstChild(pNode, 1)->sText);
	}
	pFunc->bFlag = pNode->bFlag;

	pParams = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	xlHirSetText(pParams, "params");
	pAstParams = __xlAstChild(pNode, 0);
	for ( i = 0; (pAstParams != NULL) && (i < pAstParams->iChildCount); ++i ) {
		XL_HirNode* pParamHir;

		pParam = pAstParams->ppChildren[i];
		pParamType = __xlAstChild(pParam, 0);
		pParamHir = xlHirCreateNode(XL_HIR_PARAM, pParam);
		if ( pParamHir != NULL ) {
			xlHirSetText(pParamHir, pParam->sText);
			__xlApplySemNodeInfo(pParamHir, pSem, pParam);
			if ( ((pParamHir->sTypeName == NULL) || (pParamHir->sTypeName[0] == '\0')) && (pParamType != NULL) ) {
				xlHirSetTypeName(pParamHir, pParamType->sText);
			}
			xlHirAppendChild(pParams, pParamHir);
		}
	}

	xlHirAppendChild(pFunc, pParams);
	xlHirAppendChild(pFunc, __xlLowerBlockToHir(__xlAstChild(pNode, 2), pSem, pDiag, bTraceLower));
	return pFunc;
}

static XL_HirNode* __xlLowerIfToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pIf;
	const XL_AstNode* pElseIfList;
	size_t i;

	pIf = xlHirCreateNode(XL_HIR_IF, pNode);
	if ( pIf == NULL ) {
		return NULL;
	}
	__xlApplySemNodeInfo(pIf, pSem, pNode);

	xlHirAppendChild(pIf, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
	xlHirAppendChild(pIf, __xlLowerBlockToHir(__xlAstChild(pNode, 1), pSem, pDiag, bTraceLower));

	pElseIfList = __xlAstChild(pNode, 2);
	for ( i = 0; (pElseIfList != NULL) && (i < pElseIfList->iChildCount); ++i ) {
		XL_HirNode* pElseIf;

			pElseIf = xlHirCreateNode(XL_HIR_IF, pElseIfList->ppChildren[i]);
			if ( pElseIf != NULL ) {
				xlHirSetText(pElseIf, "elseif");
				__xlApplySemNodeInfo(pElseIf, pSem, pElseIfList->ppChildren[i]);
				xlHirAppendChild(pElseIf, xlLowerExprToHir(__xlAstChild(pElseIfList->ppChildren[i], 0), pSem, pDiag, bTraceLower));
				xlHirAppendChild(pElseIf, __xlLowerBlockToHir(__xlAstChild(pElseIfList->ppChildren[i], 1), pSem, pDiag, bTraceLower));
				xlHirAppendChild(pIf, pElseIf);
			}
		}

	if ( __xlAstChild(pNode, 3) != NULL ) {
		XL_HirNode* pElse;

		pElse = __xlLowerBlockToHir(__xlAstChild(pNode, 3), pSem, pDiag, bTraceLower);
		if ( pElse != NULL ) {
			xlHirSetText(pElse, "else");
			xlHirAppendChild(pIf, pElse);
		}
	}

	return pIf;
}

static XL_HirNode* __xlLowerForInToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pLoop;
	XL_HirNode* pTargets;
	const XL_AstNode* pAstTargets;
	size_t i;

	pLoop = xlHirCreateNode(XL_HIR_FORIN, pNode);
	if ( pLoop == NULL ) {
		return NULL;
	}
	__xlApplySemNodeInfo(pLoop, pSem, pNode);

	pTargets = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	if ( pTargets != NULL ) {
		xlHirSetText(pTargets, "targets");
		__xlApplySemNodeInfo(pTargets, pSem, pNode);
		pAstTargets = __xlAstChild(pNode, 0);
		for ( i = 0; (pAstTargets != NULL) && (i < pAstTargets->iChildCount); ++i ) {
			xlHirAppendChild(pTargets, xlLowerExprToHir(pAstTargets->ppChildren[i], pSem, pDiag, bTraceLower));
		}
		xlHirAppendChild(pLoop, pTargets);
	}

	xlHirAppendChild(pLoop, xlLowerExprToHir(__xlAstChild(pNode, 1), pSem, pDiag, bTraceLower));
	xlHirAppendChild(pLoop, __xlLowerBlockToHir(__xlAstChild(pNode, 2), pSem, pDiag, bTraceLower));
	return pLoop;
}

static XL_HirNode* __xlLowerWhileToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pLoop;

	pLoop = xlHirCreateNode(XL_HIR_WHILE, pNode);
	if ( pLoop == NULL ) {
		return NULL;
	}
	__xlApplySemNodeInfo(pLoop, pSem, pNode);
	pLoop->bFlag = pNode->bFlag;

	xlHirAppendChild(pLoop, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
	xlHirAppendChild(pLoop, __xlLowerBlockToHir(__xlAstChild(pNode, 1), pSem, pDiag, bTraceLower));
	return pLoop;
}

static XL_HirNode* __xlLowerTryToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pTry;
	const XL_AstNode* pCatchName;
	XL_HirNode* pCatchNameHir;

	pTry = xlHirCreateNode(XL_HIR_TRY, pNode);
	if ( pTry == NULL ) {
		return NULL;
	}
	__xlApplySemNodeInfo(pTry, pSem, pNode);
	pTry->bFlag = pNode->bFlag;

	xlHirAppendChild(pTry, __xlLowerBlockToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
	pCatchName = __xlAstChild(pNode, 1);
	pCatchNameHir = xlLowerExprToHir(pCatchName, pSem, pDiag, bTraceLower);
	if ( pCatchNameHir == NULL ) {
		pCatchNameHir = xlHirCreateNode(XL_HIR_NAME, NULL);
		if ( pCatchNameHir != NULL ) {
			xlHirSetText(pCatchNameHir, "");
		}
	}
	xlHirAppendChild(pTry, pCatchNameHir);
	xlHirAppendChild(pTry, __xlLowerBlockToHir(__xlAstChild(pNode, 2), pSem, pDiag, bTraceLower));
	xlHirAppendChild(pTry, __xlLowerBlockToHir(__xlAstChild(pNode, 3), pSem, pDiag, bTraceLower));
	return pTry;
}

static XL_HirNode* __xlLowerStmtToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pHir;

	if ( pNode == NULL ) {
		return NULL;
	}

	__xlLowerTrace(pDiag, bTraceLower, pNode, "lower stmt");
	switch ( pNode->eKind ) {
		case XL_AST_TYPEDEF:
			return NULL;
		case XL_AST_FUNC_DECL:
			return __xlLowerFuncToHir(pNode, pSem, pDiag, bTraceLower);
		case XL_AST_CONST_DECL:
		case XL_AST_VAR_DECL:
		{
			const XL_AstNode* pTypeNode;
			const XL_AstNode* pValueNode;

			pHir = xlHirCreateNode((pNode->eKind == XL_AST_CONST_DECL) ? XL_HIR_CONST : XL_HIR_VAR, pNode);
			xlHirSetText(pHir, pNode->sText);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			pTypeNode = __xlAstChild(pNode, 0);
			pValueNode = __xlAstChild(pNode, 1);
			if ( (pValueNode == NULL) && (pTypeNode != NULL) && (pTypeNode->eKind != XL_AST_TYPE_REF) ) {
				pValueNode = pTypeNode;
				pTypeNode = NULL;
			}
			if ( ((pHir->sTypeName == NULL) || (pHir->sTypeName[0] == '\0')) && (pTypeNode != NULL) ) {
				xlHirSetTypeName(pHir, pTypeNode->sText);
			}
			xlHirAppendChild(pHir, xlLowerExprToHir(pValueNode, pSem, pDiag, bTraceLower));
			return pHir;
		}
		case XL_AST_IF:
			return __xlLowerIfToHir(pNode, pSem, pDiag, bTraceLower);
		case XL_AST_TRY:
			return __xlLowerTryToHir(pNode, pSem, pDiag, bTraceLower);
		case XL_AST_FORIN:
			return __xlLowerForInToHir(pNode, pSem, pDiag, bTraceLower);
		case XL_AST_WHILE:
			return __xlLowerWhileToHir(pNode, pSem, pDiag, bTraceLower);
		case XL_AST_RETURN:
			pHir = xlHirCreateNode(XL_HIR_RETURN, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_BREAK:
			pHir = xlHirCreateNode(XL_HIR_BREAK, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			pHir->iIntValue = pNode->iIntValue;
			return pHir;
		case XL_AST_CONTINUE:
			pHir = xlHirCreateNode(XL_HIR_CONTINUE, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			pHir->iIntValue = pNode->iIntValue;
			return pHir;
		case XL_AST_THROW:
			pHir = xlHirCreateNode(XL_HIR_THROW, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_ASSERT:
			pHir = xlHirCreateNode(XL_HIR_ASSERT, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 1), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_YIELD:
			pHir = xlHirCreateNode(XL_HIR_YIELD, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_IMPORT:
			pHir = xlHirCreateNode(XL_HIR_IMPORT, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_EXPORT:
			pHir = xlHirCreateNode(XL_HIR_EXPORT, pNode);
			__xlApplySemNodeInfo(pHir, pSem, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower));
			return pHir;
		case XL_AST_BLOCK:
			return __xlLowerBlockToHir(pNode, pSem, pDiag, bTraceLower);
		default:
			return xlLowerExprToHir(pNode, pSem, pDiag, bTraceLower);
	}
}

static XL_MirKind __xlMirKindFromHir(XL_HirKind eKind)
{
	switch ( eKind ) {
		case XL_HIR_PROGRAM:
			return XL_MIR_PROGRAM;
		case XL_HIR_FUNC:
			return XL_MIR_FUNC;
		case XL_HIR_BLOCK:
			return XL_MIR_BLOCK;
		case XL_HIR_CONST:
		case XL_HIR_VAR:
			return XL_MIR_BIND;
		case XL_HIR_IF:
			return XL_MIR_BRANCH;
		case XL_HIR_TRY:
			return XL_MIR_TRY;
		case XL_HIR_WHILE:
			return XL_MIR_WHILE;
		case XL_HIR_FORIN:
			return XL_MIR_FORIN;
		case XL_HIR_RETURN:
			return XL_MIR_RETURN;
		case XL_HIR_BREAK:
			return XL_MIR_BREAK;
		case XL_HIR_CONTINUE:
			return XL_MIR_CONTINUE;
		case XL_HIR_THROW:
			return XL_MIR_THROW;
		case XL_HIR_ASSERT:
			return XL_MIR_ASSERT;
		case XL_HIR_YIELD:
			return XL_MIR_YIELD;
		case XL_HIR_IMPORT:
			return XL_MIR_IMPORT;
		case XL_HIR_EXPORT:
			return XL_MIR_EXPORT;
		case XL_HIR_CALL:
			return XL_MIR_CALL;
		case XL_HIR_MEMBER:
			return XL_MIR_MEMBER;
		default:
			return XL_MIR_VALUE;
	}
}

static XL_MirNode* __xlLowerHirToMir(const XL_HirNode* pHir)
{
	XL_MirNode* pMir;
	size_t i;

	if ( pHir == NULL ) {
		return NULL;
	}

	pMir = xlMirCreateNode(__xlMirKindFromHir(pHir->eKind), pHir);
	if ( pMir == NULL ) {
		return NULL;
	}

	xlMirSetText(pMir, pHir->sText);
	xlMirSetAbiName(pMir, pHir->sAbiName);
	xlMirSetExternName(pMir, pHir->sExternName);
	xlMirSetTypeName(pMir, pHir->sTypeName);
	pMir->iScopeId = pHir->iScopeId;
	pMir->iSymbolIndex = pHir->iSymbolIndex;
	pMir->iFuncIndex = pHir->iFuncIndex;
	pMir->iIntValue = pHir->iIntValue;
	pMir->fFloatValue = pHir->fFloatValue;
	pMir->bFlag = pHir->bFlag;

	for ( i = 0; i < pHir->iChildCount; ++i ) {
		xlMirAppendChild(pMir, __xlLowerHirToMir(pHir->ppChildren[i]));
	}

	return pMir;
}

void xlLowerResultInit(XL_LowerResult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}

void xlLowerResultFree(XL_LowerResult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}

	xlHirFree(pResult->pHirRoot);
	xlMirFree(pResult->pMirRoot);
	xlAsyncSmFree(&pResult->asyncSm);
	xlGeneratorSmFree(&pResult->generatorSm);
	memset(pResult, 0, sizeof(*pResult));
}

int xlLowerAst(
	const XL_AstNode* pRoot,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower,
	XL_LowerResult* pResult
)
{
	size_t i;

	if ( (pRoot == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlLowerResultInit(pResult);
	xlLowerAsyncTouch();
	xlLowerGeneratorTouch();

	pResult->pHirRoot = xlHirCreateNode(XL_HIR_PROGRAM, pRoot);
	if ( pResult->pHirRoot == NULL ) {
		return XL_RC_IO_ERROR;
	}
	__xlApplySemNodeInfo(pResult->pHirRoot, pSem, pRoot);

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		xlHirAppendChild(pResult->pHirRoot, __xlLowerStmtToHir(pRoot->ppChildren[i], pSem, pDiag, bTraceLower));
	}

	pResult->pMirRoot = __xlLowerHirToMir(pResult->pHirRoot);
	if ( pResult->pMirRoot == NULL ) {
		xlLowerResultFree(pResult);
		return XL_RC_IO_ERROR;
	}

	if ( xlLowerBuildAsyncSm(pResult->pHirRoot, pDiag, bTraceLower, &pResult->asyncSm) != XL_RC_OK ) {
		xlLowerResultFree(pResult);
		return XL_RC_IO_ERROR;
	}

	if ( xlLowerBuildGeneratorSm(pResult->pHirRoot, pDiag, bTraceLower, &pResult->generatorSm) != XL_RC_OK ) {
		xlLowerResultFree(pResult);
		return XL_RC_IO_ERROR;
	}

	return XL_RC_OK;
}
