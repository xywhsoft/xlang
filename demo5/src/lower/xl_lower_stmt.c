#include "xl_lower.h"

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower);
void xlLowerAsyncTouch(void);
void xlLowerGeneratorTouch(void);

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
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

static XL_HirNode* __xlLowerStmtToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower);

static XL_HirNode* __xlLowerBlockToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pBlock;
	size_t i;

	pBlock = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	if ( pBlock == NULL ) {
		return NULL;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirAppendChild(pBlock, __xlLowerStmtToHir(pNode->ppChildren[i], pDiag, bTraceLower));
	}

	return pBlock;
}

static XL_HirNode* __xlLowerFuncToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
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
	if ( __xlAstChild(pNode, 1) != NULL ) {
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
			xlHirSetTypeName(pParamHir, (pParamType != NULL) ? pParamType->sText : "");
			xlHirAppendChild(pParams, pParamHir);
		}
	}

	xlHirAppendChild(pFunc, pParams);
	xlHirAppendChild(pFunc, __xlLowerBlockToHir(__xlAstChild(pNode, 2), pDiag, bTraceLower));
	return pFunc;
}

static XL_HirNode* __xlLowerIfToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pIf;
	const XL_AstNode* pElseIfList;
	size_t i;

	pIf = xlHirCreateNode(XL_HIR_IF, pNode);
	if ( pIf == NULL ) {
		return NULL;
	}

	xlHirAppendChild(pIf, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
	xlHirAppendChild(pIf, __xlLowerBlockToHir(__xlAstChild(pNode, 1), pDiag, bTraceLower));

	pElseIfList = __xlAstChild(pNode, 2);
	for ( i = 0; (pElseIfList != NULL) && (i < pElseIfList->iChildCount); ++i ) {
		XL_HirNode* pElseIf;

		pElseIf = xlHirCreateNode(XL_HIR_IF, pElseIfList->ppChildren[i]);
		if ( pElseIf != NULL ) {
			xlHirSetText(pElseIf, "elseif");
			xlHirAppendChild(pElseIf, xlLowerExprToHir(__xlAstChild(pElseIfList->ppChildren[i], 0), pDiag, bTraceLower));
			xlHirAppendChild(pElseIf, __xlLowerBlockToHir(__xlAstChild(pElseIfList->ppChildren[i], 1), pDiag, bTraceLower));
			xlHirAppendChild(pIf, pElseIf);
		}
	}

	if ( __xlAstChild(pNode, 3) != NULL ) {
		XL_HirNode* pElse;

		pElse = __xlLowerBlockToHir(__xlAstChild(pNode, 3), pDiag, bTraceLower);
		if ( pElse != NULL ) {
			xlHirSetText(pElse, "else");
			xlHirAppendChild(pIf, pElse);
		}
	}

	return pIf;
}

static XL_HirNode* __xlLowerForInToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pLoop;
	XL_HirNode* pTargets;
	const XL_AstNode* pAstTargets;
	size_t i;

	pLoop = xlHirCreateNode(XL_HIR_FORIN, pNode);
	if ( pLoop == NULL ) {
		return NULL;
	}

	pTargets = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	if ( pTargets != NULL ) {
		xlHirSetText(pTargets, "targets");
		pAstTargets = __xlAstChild(pNode, 0);
		for ( i = 0; (pAstTargets != NULL) && (i < pAstTargets->iChildCount); ++i ) {
			xlHirAppendChild(pTargets, xlLowerExprToHir(pAstTargets->ppChildren[i], pDiag, bTraceLower));
		}
		xlHirAppendChild(pLoop, pTargets);
	}

	xlHirAppendChild(pLoop, xlLowerExprToHir(__xlAstChild(pNode, 1), pDiag, bTraceLower));
	xlHirAppendChild(pLoop, __xlLowerBlockToHir(__xlAstChild(pNode, 2), pDiag, bTraceLower));
	return pLoop;
}

static XL_HirNode* __xlLowerStmtToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pHir;

	if ( pNode == NULL ) {
		return NULL;
	}

	__xlLowerTrace(pDiag, bTraceLower, pNode, "lower stmt");
	switch ( pNode->eKind ) {
		case XL_AST_FUNC_DECL:
			return __xlLowerFuncToHir(pNode, pDiag, bTraceLower);
		case XL_AST_CONST_DECL:
		case XL_AST_VAR_DECL:
			pHir = xlHirCreateNode((pNode->eKind == XL_AST_CONST_DECL) ? XL_HIR_CONST : XL_HIR_VAR, pNode);
			xlHirSetText(pHir, pNode->sText);
			if ( __xlAstChild(pNode, 0) != NULL ) {
				xlHirSetTypeName(pHir, __xlAstChild(pNode, 0)->sText);
			}
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 1), pDiag, bTraceLower));
			return pHir;
		case XL_AST_IF:
			return __xlLowerIfToHir(pNode, pDiag, bTraceLower);
		case XL_AST_FORIN:
			return __xlLowerForInToHir(pNode, pDiag, bTraceLower);
		case XL_AST_RETURN:
			pHir = xlHirCreateNode(XL_HIR_RETURN, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
			return pHir;
		case XL_AST_BREAK:
			pHir = xlHirCreateNode(XL_HIR_BREAK, pNode);
			pHir->iIntValue = pNode->iIntValue;
			return pHir;
		case XL_AST_CONTINUE:
			pHir = xlHirCreateNode(XL_HIR_CONTINUE, pNode);
			pHir->iIntValue = pNode->iIntValue;
			return pHir;
		case XL_AST_THROW:
			pHir = xlHirCreateNode(XL_HIR_THROW, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
			return pHir;
		case XL_AST_YIELD:
			pHir = xlHirCreateNode(XL_HIR_YIELD, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
			return pHir;
		case XL_AST_IMPORT:
			pHir = xlHirCreateNode(XL_HIR_IMPORT, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
			return pHir;
		case XL_AST_EXPORT:
			pHir = xlHirCreateNode(XL_HIR_EXPORT, pNode);
			xlHirAppendChild(pHir, xlLowerExprToHir(__xlAstChild(pNode, 0), pDiag, bTraceLower));
			return pHir;
		case XL_AST_BLOCK:
			return __xlLowerBlockToHir(pNode, pDiag, bTraceLower);
		default:
			return xlLowerExprToHir(pNode, pDiag, bTraceLower);
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
		case XL_HIR_YIELD:
			return XL_MIR_YIELD;
		case XL_HIR_IMPORT:
			return XL_MIR_IMPORT;
		case XL_HIR_EXPORT:
			return XL_MIR_EXPORT;
		case XL_HIR_CALL:
			return XL_MIR_CALL;
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
	xlMirSetTypeName(pMir, pHir->sTypeName);
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

int xlLowerAst(const XL_AstNode* pRoot, XL_DiagContext* pDiag, int bTraceLower, XL_LowerResult* pResult)
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

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		xlHirAppendChild(pResult->pHirRoot, __xlLowerStmtToHir(pRoot->ppChildren[i], pDiag, bTraceLower));
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
