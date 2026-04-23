#include "xl_lower.h"

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);
void xlLowerAsyncTouch(void);
void xlLowerGeneratorTouch(void);

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
	}
}

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
}

static const XL_AstNode* __xlClassBaseTypeNode(const XL_AstNode* pNode)
{
	const XL_AstNode* pChild;

	pChild = __xlAstChild(pNode, 0);
	return ((pChild != NULL) && (pChild->eKind == XL_AST_TYPE_REF)) ? pChild : NULL;
}

static const XL_AstNode* __xlClassMemberListNode(const XL_AstNode* pNode)
{
	return (__xlClassBaseTypeNode(pNode) != NULL) ? __xlAstChild(pNode, 1) : __xlAstChild(pNode, 0);
}

static XL_HirNode* __xlLowerBlockToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);
static XL_HirNode* __xlLowerFuncToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);

static const XL_AstNode* __xlFuncReceiverNode(const XL_AstNode* pNode)
{
	if ( (pNode == NULL) || (pNode->eKind != XL_AST_FUNC_DECL) || (pNode->iChildCount <= 3) ) {
		return NULL;
	}

	return (pNode->ppChildren[3]->eKind == XL_AST_TYPE_REF) ? pNode->ppChildren[3] : NULL;
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

static char* __xlLowerDupText(const char* sText)
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

static const XL_GeneratorSmFunc* __xlFindGeneratorSmFunc(
	const XL_GeneratorSm* pSm,
	const XL_AsyncSmFunc* pAsyncFunc
)
{
	size_t i;
	const XL_GeneratorSmFunc* pNameMatch;
	size_t iNameMatchCount;

	if ( (pSm == NULL) || (pAsyncFunc == NULL) ) {
		return NULL;
	}

	pNameMatch = NULL;
	iNameMatchCount = 0;
	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		const XL_GeneratorSmFunc* pFunc = &pSm->pFuncs[i];

		if ( (pAsyncFunc->sAbiName != NULL) && (pAsyncFunc->sAbiName[0] != '\0') ) {
			if ( strcmp((pFunc->sAbiName != NULL) ? pFunc->sAbiName : "", pAsyncFunc->sAbiName) == 0 ) {
				return pFunc;
			}
		}
		if ( pFunc->iLine != pAsyncFunc->iLine || pFunc->iColumn != pAsyncFunc->iColumn ) {
			if ( strcmp((pFunc->sFuncName != NULL) ? pFunc->sFuncName : "", (pAsyncFunc->sFuncName != NULL) ? pAsyncFunc->sFuncName : "") == 0 ) {
				pNameMatch = pFunc;
				++iNameMatchCount;
			}
			continue;
		}
		if ( strcmp((pFunc->sFuncName != NULL) ? pFunc->sFuncName : "", (pAsyncFunc->sFuncName != NULL) ? pAsyncFunc->sFuncName : "") != 0 ) {
			continue;
		}
		return pFunc;
	}
	if ( iNameMatchCount == 1 ) {
		return pNameMatch;
	}
	return NULL;
}

static int __xlHybridSmGrow(XL_HybridSm* pSm)
{
	XL_HybridSmFunc* pNewFuncs;
	size_t iNewCap;

	if ( pSm->iFuncCount < pSm->iFuncCap ) {
		return XL_RC_OK;
	}
	iNewCap = (pSm->iFuncCap == 0) ? 4 : (pSm->iFuncCap * 2);
	pNewFuncs = (XL_HybridSmFunc*)realloc(pSm->pFuncs, sizeof(XL_HybridSmFunc) * iNewCap);
	if ( pNewFuncs == NULL ) {
		return XL_RC_IO_ERROR;
	}
	memset(pNewFuncs + pSm->iFuncCap, 0, sizeof(XL_HybridSmFunc) * (iNewCap - pSm->iFuncCap));
	pSm->pFuncs = pNewFuncs;
	pSm->iFuncCap = iNewCap;
	return XL_RC_OK;
}

static int __xlHybridPointGrow(XL_HybridSmFunc* pFunc)
{
	XL_HybridSmPoint* pNewPoints;
	size_t iNewCap;

	if ( pFunc->iPointCount < pFunc->iPointCap ) {
		return XL_RC_OK;
	}
	iNewCap = (pFunc->iPointCap == 0) ? 4 : (pFunc->iPointCap * 2);
	pNewPoints = (XL_HybridSmPoint*)realloc(pFunc->pPoints, sizeof(XL_HybridSmPoint) * iNewCap);
	if ( pNewPoints == NULL ) {
		return XL_RC_IO_ERROR;
	}
	memset(pNewPoints + pFunc->iPointCap, 0, sizeof(XL_HybridSmPoint) * (iNewCap - pFunc->iPointCap));
	pFunc->pPoints = pNewPoints;
	pFunc->iPointCap = iNewCap;
	return XL_RC_OK;
}

static int __xlHybridFindPoint(const XL_HybridSmFunc* pFunc, int iLine, int iColumn)
{
	size_t i;

	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		if ( pFunc->pPoints[i].iLine == iLine && pFunc->pPoints[i].iColumn == iColumn ) {
			return (int)i;
		}
	}
	return -1;
}

static int __xlHybridEnsurePoint(XL_HybridSmFunc* pFunc, int iLine, int iColumn)
{
	int iIndex;

	iIndex = __xlHybridFindPoint(pFunc, iLine, iColumn);
	if ( iIndex >= 0 ) {
		return iIndex;
	}
	if ( __xlHybridPointGrow(pFunc) != XL_RC_OK ) {
		return -1;
	}
	iIndex = (int)pFunc->iPointCount;
	pFunc->pPoints[iIndex].iLine = iLine;
	pFunc->pPoints[iIndex].iColumn = iColumn;
	pFunc->pPoints[iIndex].iAsyncStateId = -1;
	pFunc->pPoints[iIndex].iGeneratorStateId = -1;
	pFunc->pPoints[iIndex].iProjectionGroupId = -1;
	pFunc->pPoints[iIndex].iProjectedGeneratorStateId = -1;
	pFunc->pPoints[iIndex].iProjectedPointIndex = -1;
	pFunc->iPointCount += 1;
	return iIndex;
}

static int __xlHybridPointCompare(const void* pLeft, const void* pRight)
{
	const XL_HybridSmPoint* pA;
	const XL_HybridSmPoint* pB;

	pA = (const XL_HybridSmPoint*)pLeft;
	pB = (const XL_HybridSmPoint*)pRight;
	if ( pA->iLine != pB->iLine ) {
		return (pA->iLine < pB->iLine) ? -1 : 1;
	}
	if ( pA->iColumn != pB->iColumn ) {
		return (pA->iColumn < pB->iColumn) ? -1 : 1;
	}
	return 0;
}

void xlHybridSmInit(XL_HybridSm* pSm)
{
	if ( pSm != NULL ) {
		memset(pSm, 0, sizeof(*pSm));
	}
}

void xlHybridSmFree(XL_HybridSm* pSm)
{
	size_t i;

	if ( pSm == NULL ) {
		return;
	}
	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		free(pSm->pFuncs[i].sFuncName);
		free(pSm->pFuncs[i].sAbiName);
		free(pSm->pFuncs[i].pPoints);
	}
	free(pSm->pFuncs);
	memset(pSm, 0, sizeof(*pSm));
}

static int __xlHybridAssignProjectionGroups(XL_HybridSmFunc* pFunc, XL_DiagContext* pDiag)
{
	size_t i;
	size_t iNextYieldIndex;

	if ( pFunc == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		pFunc->pPoints[i].iProjectionGroupId = -1;
		pFunc->pPoints[i].iProjectedGeneratorStateId = -1;
		pFunc->pPoints[i].iProjectedPointIndex = -1;
	}

	iNextYieldIndex = 0;
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( !pPoint->bYield ) {
			continue;
		}
		pPoint->iProjectionGroupId = (int)iNextYieldIndex;
		pPoint->iProjectedGeneratorStateId = pPoint->iGeneratorStateId;
		pPoint->iProjectedPointIndex = (int)i;
		iNextYieldIndex += 1;
	}

	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];
		size_t j;

		if ( !pPoint->bAwait ) {
			continue;
		}
		for ( j = i; j < pFunc->iPointCount; ++j ) {
			XL_HybridSmPoint* pTarget = &pFunc->pPoints[j];

			if ( !pTarget->bYield ) {
				continue;
			}
			if ( j < i ) {
				continue;
			}
			pPoint->iProjectionGroupId = pTarget->iProjectionGroupId;
			pPoint->iProjectedGeneratorStateId = pTarget->iGeneratorStateId;
			pPoint->iProjectedPointIndex = (int)j;
			break;
		}
		if ( pPoint->iProjectionGroupId >= 0 ) {
			continue;
		}
		xlDiagEmitSource(
			pDiag,
			XL_DIAG_LEVEL_ERROR,
			"lower",
			__FILE__,
			__LINE__,
			NULL,
			pPoint->iLine,
			pPoint->iColumn,
			"async suspend point in %s at %d:%d could not be projected onto a later generator suspend point",
			(pFunc->sFuncName != NULL) ? pFunc->sFuncName : "<anonymous>",
			pPoint->iLine,
			pPoint->iColumn
		);
		return XL_RC_FAIL;
	}

	return XL_RC_OK;
}

int xlLowerBuildHybridSm(const XL_AsyncSm* pAsyncSm, const XL_GeneratorSm* pGeneratorSm, XL_DiagContext* pDiag, XL_HybridSm* pSm)
{
	size_t i;

	if ( (pAsyncSm == NULL) || (pGeneratorSm == NULL) || (pSm == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	xlHybridSmInit(pSm);
	for ( i = 0; i < pAsyncSm->iFuncCount; ++i ) {
		const XL_AsyncSmFunc* pAsyncFunc;
		const XL_GeneratorSmFunc* pGeneratorFunc;
		XL_HybridSmFunc* pHybridFunc;
		size_t j;

		pAsyncFunc = &pAsyncSm->pFuncs[i];
		pGeneratorFunc = __xlFindGeneratorSmFunc(pGeneratorSm, pAsyncFunc);
		if ( pGeneratorFunc == NULL ) {
			continue;
		}
		if ( __xlHybridSmGrow(pSm) != XL_RC_OK ) {
			xlHybridSmFree(pSm);
			return XL_RC_IO_ERROR;
		}
		pHybridFunc = &pSm->pFuncs[pSm->iFuncCount++];
		memset(pHybridFunc, 0, sizeof(*pHybridFunc));
		pHybridFunc->sFuncName = __xlLowerDupText((pAsyncFunc->sFuncName != NULL) ? pAsyncFunc->sFuncName : "");
		pHybridFunc->sAbiName = __xlLowerDupText((pAsyncFunc->sAbiName != NULL) ? pAsyncFunc->sAbiName : "");
		if ( (pHybridFunc->sFuncName == NULL) || (pHybridFunc->sAbiName == NULL) ) {
			xlHybridSmFree(pSm);
			return XL_RC_IO_ERROR;
		}
		pHybridFunc->iLine = pAsyncFunc->iLine;
		pHybridFunc->iColumn = pAsyncFunc->iColumn;

		for ( j = 0; j < pAsyncFunc->iStateCount; ++j ) {
			int iIndex;

			if ( pAsyncFunc->pStates[j].bFinal ) {
				pHybridFunc->iAsyncFinalCount += 1;
			}
			if ( !pAsyncFunc->pStates[j].bSuspend ) {
				continue;
			}
			iIndex = __xlHybridEnsurePoint(pHybridFunc, pAsyncFunc->pStates[j].iLine, pAsyncFunc->pStates[j].iColumn);
			if ( iIndex < 0 ) {
				xlHybridSmFree(pSm);
				return XL_RC_IO_ERROR;
			}
			pHybridFunc->pPoints[iIndex].bAwait = 1;
			pHybridFunc->pPoints[iIndex].iAsyncStateId = pAsyncFunc->pStates[j].iStateId;
		}
		for ( j = 0; j < pGeneratorFunc->iStateCount; ++j ) {
			int iIndex;

			if ( pGeneratorFunc->pStates[j].bFinal ) {
				pHybridFunc->iGeneratorFinalCount += 1;
			}
			if ( !pGeneratorFunc->pStates[j].bSuspend ) {
				continue;
			}
			iIndex = __xlHybridEnsurePoint(pHybridFunc, pGeneratorFunc->pStates[j].iLine, pGeneratorFunc->pStates[j].iColumn);
			if ( iIndex < 0 ) {
				xlHybridSmFree(pSm);
				return XL_RC_IO_ERROR;
			}
			pHybridFunc->pPoints[iIndex].bYield = 1;
			pHybridFunc->pPoints[iIndex].iGeneratorStateId = pGeneratorFunc->pStates[j].iStateId;
		}
		if ( pHybridFunc->iPointCount > 1 ) {
			qsort(pHybridFunc->pPoints, pHybridFunc->iPointCount, sizeof(XL_HybridSmPoint), __xlHybridPointCompare);
		}
		if ( __xlHybridAssignProjectionGroups(pHybridFunc, pDiag) != XL_RC_OK ) {
			xlHybridSmFree(pSm);
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int __xlValidateHybridStateMachines(const XL_LowerResult* pResult, XL_DiagContext* pDiag)
{
	size_t i;

	if ( pResult == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pResult->hybridSm.iFuncCount; ++i ) {
		const XL_HybridSmFunc* pHybridFunc;
		size_t iAwaitCount;
		size_t iYieldCount;
		size_t j;

		pHybridFunc = &pResult->hybridSm.pFuncs[i];
		iAwaitCount = 0;
		iYieldCount = 0;
		for ( j = 0; j < pHybridFunc->iPointCount; ++j ) {
			if ( pHybridFunc->pPoints[j].bAwait ) {
				iAwaitCount += 1;
				if ( pHybridFunc->pPoints[j].iProjectedGeneratorStateId <= 0 ) {
					xlDiagEmitSource(
						pDiag,
						XL_DIAG_LEVEL_ERROR,
						"lower",
						__FILE__,
						__LINE__,
						NULL,
						pHybridFunc->pPoints[j].iLine,
						pHybridFunc->pPoints[j].iColumn,
						"async suspend point in %s at %d:%d is missing projected generator state",
						(pHybridFunc->sFuncName != NULL) ? pHybridFunc->sFuncName : "<anonymous>",
						pHybridFunc->pPoints[j].iLine,
						pHybridFunc->pPoints[j].iColumn
					);
					return XL_RC_FAIL;
				}
			}
			if ( pHybridFunc->pPoints[j].bYield ) {
				iYieldCount += 1;
			}
		}
		if ( iAwaitCount > iYieldCount ) {
			xlDiagEmitSource(
				pDiag,
				XL_DIAG_LEVEL_ERROR,
				"lower",
				__FILE__,
				__LINE__,
				NULL,
				pHybridFunc->iLine,
				pHybridFunc->iColumn,
				"async/generator suspend count mismatch in %s at %d:%d (async=%zu generator=%zu)",
				(pHybridFunc->sFuncName != NULL) ? pHybridFunc->sFuncName : "<anonymous>",
				pHybridFunc->iLine,
				pHybridFunc->iColumn,
				iAwaitCount,
				iYieldCount
			);
			return XL_RC_FAIL;
		}
		if ( pHybridFunc->iAsyncFinalCount != pHybridFunc->iGeneratorFinalCount ) {
			xlDiagEmitSource(
				pDiag,
				XL_DIAG_LEVEL_ERROR,
				"lower",
				__FILE__,
				__LINE__,
				NULL,
				pHybridFunc->iLine,
				pHybridFunc->iColumn,
				"async/generator final-state count mismatch in %s at %d:%d (async=%zu generator=%zu)",
				(pHybridFunc->sFuncName != NULL) ? pHybridFunc->sFuncName : "<anonymous>",
				pHybridFunc->iLine,
				pHybridFunc->iColumn,
				pHybridFunc->iAsyncFinalCount,
				pHybridFunc->iGeneratorFinalCount
			);
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static XL_HirNode* __xlCreateLiteralHir(const XL_AstNode* pSource, const char* sTypeName, const char* sText, int64_t iIntValue)
{
	XL_HirNode* pNode;
	char sIntText[32];

	pNode = xlHirCreateNode(XL_HIR_LITERAL, pSource);
	if ( pNode != NULL ) {
		if ( sText != NULL ) {
			xlHirSetText(pNode, sText);
		} else if ( (sTypeName != NULL) && (strcmp(sTypeName, "int") == 0) ) {
			snprintf(sIntText, sizeof(sIntText), "%lld", (long long)iIntValue);
			xlHirSetText(pNode, sIntText);
		}
		if ( sTypeName != NULL ) {
			xlHirSetTypeName(pNode, sTypeName);
		}
		pNode->iIntValue = iIntValue;
	}
	return pNode;
}

static XL_HirNode* __xlCreateNameHir(const XL_AstNode* pSource, const char* sName, const char* sTypeName)
{
	XL_HirNode* pNode;

	pNode = xlHirCreateNode(XL_HIR_NAME, pSource);
	if ( pNode != NULL ) {
		xlHirSetText(pNode, sName);
		if ( sTypeName != NULL ) {
			xlHirSetTypeName(pNode, sTypeName);
		}
	}
	return pNode;
}

static void __xlLowerSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
{
	size_t i;
	size_t j;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	j = 0;
	if ( (sText == NULL) || (sText[0] == '\0') ) {
		sOut[j++] = '_';
	} else {
		for ( i = 0; (sText[i] != '\0') && ((j + 1) < iOutSize); ++i ) {
			unsigned char ch = (unsigned char)sText[i];
			sOut[j++] = ((ch >= 'a') && (ch <= 'z')) ||
				((ch >= 'A') && (ch <= 'Z')) ||
				((ch >= '0') && (ch <= '9')) ||
				(ch == '_') ? (char)ch : '_';
		}
	}
	sOut[j] = '\0';
}

static void __xlClassCreateSymbolName(char* sOut, size_t iOutSize, const char* sClassName, const XL_AstNode* pCreateNode)
{
	char sSafeClass[128];

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	__xlLowerSanitizeIdentPart(sSafeClass, sizeof(sSafeClass), sClassName);
	snprintf(
		sOut,
		iOutSize,
		"xl_class_%s__create__%d_%d",
		sSafeClass,
		(pCreateNode != NULL) ? pCreateNode->iLine : 0,
		(pCreateNode != NULL) ? pCreateNode->iColumn : 0
	);
}

static void __xlClassDestroySymbolName(char* sOut, size_t iOutSize, const char* sClassName)
{
	char sSafeClass[128];

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	__xlLowerSanitizeIdentPart(sSafeClass, sizeof(sSafeClass), sClassName);
	snprintf(sOut, iOutSize, "xl_class_%s__destroy", sSafeClass);
}

static XL_HirNode* __xlCreateParamHir(const XL_AstNode* pSource, const char* sName, const char* sTypeName)
{
	XL_HirNode* pParam;

	pParam = xlHirCreateNode(XL_HIR_PARAM, pSource);
	if ( pParam != NULL ) {
		xlHirSetText(pParam, sName);
		xlHirSetTypeName(pParam, sTypeName);
	}
	return pParam;
}

static void __xlHirPrependChild(XL_HirNode* pNode, XL_HirNode* pChild)
{
	XL_HirNode** ppNewItems;
	size_t iNewCap;

	if ( (pNode == NULL) || (pChild == NULL) ) {
		return;
	}
	if ( pNode->iChildCount >= pNode->iChildCapacity ) {
		iNewCap = (pNode->iChildCapacity == 0) ? 4 : (pNode->iChildCapacity * 2);
		ppNewItems = (XL_HirNode**)realloc(pNode->ppChildren, sizeof(XL_HirNode*) * iNewCap);
		if ( ppNewItems == NULL ) {
			return;
		}
		pNode->ppChildren = ppNewItems;
		pNode->iChildCapacity = iNewCap;
	}
	memmove(&pNode->ppChildren[1], &pNode->ppChildren[0], sizeof(XL_HirNode*) * pNode->iChildCount);
	pNode->ppChildren[0] = pChild;
	pNode->iChildCount += 1;
}

static void __xlHirInsertChildAt(XL_HirNode* pNode, XL_HirNode* pChild, size_t iIndex)
{
	XL_HirNode** ppNewItems;
	size_t iNewCap;

	if ( (pNode == NULL) || (pChild == NULL) ) {
		return;
	}
	if ( iIndex > pNode->iChildCount ) {
		iIndex = pNode->iChildCount;
	}
	if ( pNode->iChildCount >= pNode->iChildCapacity ) {
		iNewCap = (pNode->iChildCapacity == 0) ? 4 : (pNode->iChildCapacity * 2);
		ppNewItems = (XL_HirNode**)realloc(pNode->ppChildren, sizeof(XL_HirNode*) * iNewCap);
		if ( ppNewItems == NULL ) {
			return;
		}
		pNode->ppChildren = ppNewItems;
		pNode->iChildCapacity = iNewCap;
	}
	memmove(&pNode->ppChildren[iIndex + 1], &pNode->ppChildren[iIndex], sizeof(XL_HirNode*) * (pNode->iChildCount - iIndex));
	pNode->ppChildren[iIndex] = pChild;
	pNode->iChildCount += 1;
}

static int __xlHirIsBaseCtorCall(const XL_HirNode* pNode)
{
	const XL_HirNode* pCallee;

	if ( (pNode == NULL) || (pNode->eKind != XL_HIR_CALL) || (pNode->iChildCount == 0) ) {
		return 0;
	}
	pCallee = pNode->ppChildren[0];
	return (pCallee != NULL) &&
		(pCallee->eKind == XL_HIR_NAME) &&
		(pCallee->sText != NULL) &&
		(strcmp(pCallee->sText, "base") == 0);
}

static XL_HirNode* __xlCreateClassFieldInitAssignHir(
	const XL_AstNode* pClassNode,
	const XL_AstNode* pFieldNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_AstNode* pTypeNode;
	const XL_AstNode* pValueNode;
	XL_HirNode* pAssign;
	XL_HirNode* pMember;

	if ( (pClassNode == NULL) || (pFieldNode == NULL) || (pFieldNode->eKind != XL_AST_VAR_DECL) ) {
		return NULL;
	}
	pTypeNode = __xlAstChild(pFieldNode, 0);
	pValueNode = __xlAstChild(pFieldNode, 1);
	if ( (pValueNode == NULL) && (pTypeNode != NULL) && (pTypeNode->eKind != XL_AST_TYPE_REF) ) {
		pValueNode = pTypeNode;
		pTypeNode = NULL;
	}
	if ( pValueNode == NULL ) {
		return NULL;
	}

	pAssign = xlHirCreateNode(XL_HIR_BINARY, pFieldNode);
	pMember = xlHirCreateNode(XL_HIR_MEMBER, pFieldNode);
	if ( (pAssign == NULL) || (pMember == NULL) ) {
		xlHirFree(pAssign);
		xlHirFree(pMember);
		return NULL;
	}
	xlHirSetText(pAssign, "=");
	xlHirSetTypeName(pAssign, (pTypeNode != NULL) ? pTypeNode->sText : "");
	xlHirSetText(pMember, pFieldNode->sText);
	xlHirSetTypeName(pMember, (pTypeNode != NULL) ? pTypeNode->sText : "");
	xlHirAppendChild(pMember, __xlCreateNameHir(pFieldNode, "self", pClassNode->sText));
	xlHirAppendChild(pAssign, pMember);
	xlHirAppendChild(pAssign, xlLowerExprToHir(pValueNode, pSem, pDiag, bTraceLower));
	return pAssign;
}

static XL_HirNode* __xlCreateClassFieldInitBlockHir(
	const XL_AstNode* pClassNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_AstNode* pMembers;
	XL_HirNode* pBlock;
	size_t i;

	pMembers = __xlClassMemberListNode(pClassNode);
	if ( pMembers == NULL ) {
		return NULL;
	}
	pBlock = xlHirCreateNode(XL_HIR_BLOCK, pClassNode);
	if ( pBlock == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pMembers->iChildCount; ++i ) {
		XL_HirNode* pAssign = __xlCreateClassFieldInitAssignHir(pClassNode, pMembers->ppChildren[i], pSem, pDiag, bTraceLower);
		if ( pAssign != NULL ) {
			xlHirAppendChild(pBlock, pAssign);
		}
	}
	if ( pBlock->iChildCount == 0 ) {
		xlHirFree(pBlock);
		return NULL;
	}
	return pBlock;
}

static XL_HirNode* __xlCreateArrayLiteralCallHir(const XL_AstNode* pSource, const XL_AstNode* pValues, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pCall;
	XL_HirNode* pCallee;
	XL_HirNode* pArgs;
	size_t i;

	pCall = xlHirCreateNode(XL_HIR_CALL, pSource);
	pCallee = xlHirCreateNode(XL_HIR_NAME, pSource);
	pArgs = xlHirCreateNode(XL_HIR_BLOCK, pSource);
	if ( (pCall == NULL) || (pCallee == NULL) || (pArgs == NULL) ) {
		xlHirFree(pCall);
		xlHirFree(pCallee);
		xlHirFree(pArgs);
		return NULL;
	}

	xlHirSetText(pCallee, "__array_literal");
	xlHirSetText(pArgs, "args");
	xlHirSetTypeName(pCall, "array<var>");
	xlHirAppendChild(pCall, pCallee);
	xlHirAppendChild(pCall, pArgs);
	for ( i = 0; (pValues != NULL) && (i < pValues->iChildCount); ++i ) {
		xlHirAppendChild(pArgs, xlLowerExprToHir(pValues->ppChildren[i], pSem, pDiag, bTraceLower));
	}
	return pCall;
}

static XL_HirNode* __xlLowerCreateToHir(
	const XL_AstNode* pClassNode,
	const XL_AstNode* pCreateNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	XL_HirNode* pFunc;
	XL_HirNode* pParams;
	XL_HirNode* pBody;
	XL_HirNode* pReturn;
	char sSymbolName[256];
	const XL_AstNode* pAstParams;
	const XL_AstNode* pParam;
	const XL_AstNode* pParamType;
	size_t i;

	if ( (pClassNode == NULL) || (pCreateNode == NULL) ) {
		return NULL;
	}

	pFunc = xlHirCreateNode(XL_HIR_FUNC, pCreateNode);
	pParams = xlHirCreateNode(XL_HIR_BLOCK, pCreateNode);
	if ( (pFunc == NULL) || (pParams == NULL) ) {
		xlHirFree(pFunc);
		xlHirFree(pParams);
		return NULL;
	}

	__xlClassCreateSymbolName(sSymbolName, sizeof(sSymbolName), pClassNode->sText, pCreateNode);
	xlHirSetText(pFunc, "__class_create");
	xlHirSetAbiName(pFunc, sSymbolName);
	xlHirSetTypeName(pFunc, pClassNode->sText);
	__xlApplySemNodeInfo(pFunc, pSem, pCreateNode);
	xlHirSetTypeName(pFunc, pClassNode->sText);

	xlHirSetText(pParams, "params");
	xlHirAppendChild(pParams, __xlCreateParamHir(pCreateNode, "self", pClassNode->sText));
	pAstParams = __xlAstChild(pCreateNode, 0);
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

	pBody = __xlLowerBlockToHir(__xlAstChild(pCreateNode, 2), pSem, pDiag, bTraceLower);
	if ( pBody == NULL ) {
		pBody = xlHirCreateNode(XL_HIR_BLOCK, pCreateNode);
	}
	if ( pBody != NULL ) {
		XL_HirNode* pFieldInitBlock = __xlCreateClassFieldInitBlockHir(pClassNode, pSem, pDiag, bTraceLower);
		if ( pFieldInitBlock != NULL ) {
			if ( (pBody->iChildCount > 0) && __xlHirIsBaseCtorCall(pBody->ppChildren[0]) ) {
				__xlHirInsertChildAt(pBody, pFieldInitBlock, 1);
			} else {
				__xlHirPrependChild(pBody, pFieldInitBlock);
			}
		}
	}
	pReturn = xlHirCreateNode(XL_HIR_RETURN, pCreateNode);
	if ( pReturn != NULL ) {
		xlHirAppendChild(pReturn, __xlCreateNameHir(pCreateNode, "self", pClassNode->sText));
		xlHirAppendChild(pBody, pReturn);
	}

	xlHirAppendChild(pFunc, pParams);
	xlHirAppendChild(pFunc, pBody);
	return pFunc;
}

static XL_HirNode* __xlLowerDestroyToHir(
	const XL_AstNode* pClassNode,
	const XL_AstNode* pDestroyNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	XL_HirNode* pFunc;
	XL_HirNode* pParams;
	XL_HirNode* pBody;
	char sSymbolName[256];

	if ( (pClassNode == NULL) || (pDestroyNode == NULL) ) {
		return NULL;
	}

	pFunc = xlHirCreateNode(XL_HIR_FUNC, pDestroyNode);
	pParams = xlHirCreateNode(XL_HIR_BLOCK, pDestroyNode);
	if ( (pFunc == NULL) || (pParams == NULL) ) {
		xlHirFree(pFunc);
		xlHirFree(pParams);
		return NULL;
	}

	__xlClassDestroySymbolName(sSymbolName, sizeof(sSymbolName), pClassNode->sText);
	xlHirSetText(pFunc, "__class_destroy");
	xlHirSetAbiName(pFunc, sSymbolName);
	xlHirSetTypeName(pFunc, "void");
	__xlApplySemNodeInfo(pFunc, pSem, pDestroyNode);
	xlHirSetTypeName(pFunc, "void");

	xlHirSetText(pParams, "params");
	xlHirAppendChild(pParams, __xlCreateParamHir(pDestroyNode, "self", pClassNode->sText));
	pBody = __xlLowerBlockToHir(__xlAstChild(pDestroyNode, 2), pSem, pDiag, bTraceLower);
	if ( pBody == NULL ) {
		pBody = xlHirCreateNode(XL_HIR_BLOCK, pDestroyNode);
	}

	xlHirAppendChild(pFunc, pParams);
	xlHirAppendChild(pFunc, pBody);
	return pFunc;
}

static void __xlCollectClassCreatesToHir(
	XL_HirNode* pRoot,
	const XL_AstNode* pNode,
	const XL_SemResult* pSem,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_AstNode* pMembers;
	size_t i;

	if ( (pRoot == NULL) || (pNode == NULL) || (pNode->eKind != XL_AST_CLASS_DECL) ) {
		return;
	}

	pMembers = __xlClassMemberListNode(pNode);
	for ( i = 0; (pMembers != NULL) && (i < pMembers->iChildCount); ++i ) {
		if ( (pMembers->ppChildren[i] != NULL) && (pMembers->ppChildren[i]->eKind == XL_AST_CREATE_DECL) ) {
			xlHirAppendChild(pRoot, __xlLowerCreateToHir(pNode, pMembers->ppChildren[i], pSem, pDiag, bTraceLower));
		} else if ( (pMembers->ppChildren[i] != NULL) && (pMembers->ppChildren[i]->eKind == XL_AST_DESTROY_DECL) ) {
			xlHirAppendChild(pRoot, __xlLowerDestroyToHir(pNode, pMembers->ppChildren[i], pSem, pDiag, bTraceLower));
		} else if ( (pMembers->ppChildren[i] != NULL) && (pMembers->ppChildren[i]->eKind == XL_AST_FUNC_DECL) ) {
			xlHirAppendChild(pRoot, __xlLowerFuncToHir(pMembers->ppChildren[i], pSem, pDiag, bTraceLower));
		}
	}
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
static void __xlCollectFuncExprsToHir(XL_HirNode* pRoot, const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower);

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
		XL_HirNode* pStmtHir;

		pStmtHir = __xlLowerStmtToHir(pNode->ppChildren[i], pSem, pDiag, bTraceLower);
		if ( (pStmtHir != NULL) && (pStmtHir->eKind == XL_HIR_BLOCK) &&
			(pNode->ppChildren[i] != NULL) && (pNode->ppChildren[i]->eKind == XL_AST_UNPACK_DECL) ) {
			size_t j;

			for ( j = 0; j < pStmtHir->iChildCount; ++j ) {
				xlHirAppendChild(pBlock, pStmtHir->ppChildren[j]);
				pStmtHir->ppChildren[j] = NULL;
			}
			xlHirFree(pStmtHir);
			continue;
		}
		xlHirAppendChild(pBlock, pStmtHir);
	}

	return pBlock;
}

static XL_HirNode* __xlLowerFuncToHir(const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pFunc;
	XL_HirNode* pParams;
	const XL_FuncInfo* pFuncInfo;
	const XL_AstNode* pAstParams;
	const XL_AstNode* pParam;
	const XL_AstNode* pParamType;
	size_t i;

	pFunc = xlHirCreateNode(XL_HIR_FUNC, pNode);
	if ( pFunc == NULL ) {
		return NULL;
	}

	xlHirSetText(pFunc, pNode->sText);
	if ( (pNode->iIntValue != 0) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		const char* sExternName = pNode->sText2;
		if ( (__xlFuncReceiverNode(pNode) != NULL) && ((sExternName == NULL) || (sExternName[0] == '\0')) ) {
			sExternName = pNode->sText;
		}
		xlHirSetExternName(pFunc, sExternName);
	}
	__xlApplySemNodeInfo(pFunc, pSem, pNode);
	pFuncInfo = __xlResolvedFuncInfo(pSem, pNode);
	if ( (pFuncInfo != NULL) && (pFuncInfo->sStorageReturnTypeName != NULL) && (pFuncInfo->sStorageReturnTypeName[0] != '\0') ) {
		xlHirSetTypeName(pFunc, pFuncInfo->sStorageReturnTypeName);
	}
	pFunc->bFlag = pNode->bFlag;

	pParams = xlHirCreateNode(XL_HIR_BLOCK, pNode);
	xlHirSetText(pParams, "params");
	pAstParams = __xlAstChild(pNode, 0);
	if ( (pFuncInfo != NULL) &&
		(pFuncInfo->sReceiverType != NULL) &&
		(pFuncInfo->sReceiverType[0] != '\0') &&
		(__xlFuncReceiverNode(pNode) == NULL) &&
		(pFuncInfo->iParamCount > ((pAstParams != NULL) ? pAstParams->iChildCount : 0u)) ) {
		XL_HirNode* pSelfParam;

		pSelfParam = __xlCreateParamHir(
			pNode,
			((pFuncInfo->arrParamNames != NULL) && (pFuncInfo->arrParamNames[0] != NULL)) ? pFuncInfo->arrParamNames[0] : "self",
			((pFuncInfo->arrParamTypes != NULL) && (pFuncInfo->arrParamTypes[0] != NULL)) ? pFuncInfo->arrParamTypes[0] : pFuncInfo->sReceiverType
		);
		xlHirAppendChild(pParams, pSelfParam);
	}
	for ( i = 0; (pAstParams != NULL) && (i < pAstParams->iChildCount); ++i ) {
		XL_HirNode* pParamHir;

		pParam = pAstParams->ppChildren[i];
		pParamType = __xlAstChild(pParam, 0);
		pParamHir = xlHirCreateNode(XL_HIR_PARAM, pParam);
		if ( pParamHir != NULL ) {
			xlHirSetText(pParamHir, pParam->sText);
			__xlApplySemNodeInfo(pParamHir, pSem, pParam);
			pParamHir->bFlag = pParam->bFlag;
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

static void __xlCollectFuncExprsToHir(XL_HirNode* pRoot, const XL_AstNode* pNode, const XL_SemResult* pSem, XL_DiagContext* pDiag, int bTraceLower)
{
	size_t i;

	if ( (pRoot == NULL) || (pNode == NULL) ) {
		return;
	}

	if ( pNode->eKind == XL_AST_FUNC_EXPR ) {
		xlHirAppendChild(pRoot, __xlLowerFuncToHir(pNode, pSem, pDiag, bTraceLower));
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlCollectFuncExprsToHir(pRoot, pNode->ppChildren[i], pSem, pDiag, bTraceLower);
	}
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
		case XL_AST_CLASS_DECL:
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
			if ( (pNode->sText2 != NULL) && (pNode->sText2[0] != '\0') ) {
				xlHirSetExternName(pHir, pNode->sText2);
			}
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
		case XL_AST_UNPACK_DECL:
		{
			const XL_AstNode* pTargets;
			const XL_AstNode* pValueNode;
			XL_HirNode* pBlock;
			XL_HirNode* pTempBind;
			char sTempName[64];
			size_t i;

			pTargets = __xlAstChild(pNode, 0);
			pValueNode = __xlAstChild(pNode, 1);
			pBlock = xlHirCreateNode(XL_HIR_BLOCK, pNode);
			if ( pBlock == NULL ) {
				return NULL;
			}
			__xlApplySemNodeInfo(pBlock, pSem, pNode);
			snprintf(sTempName, sizeof(sTempName), "xl_unpack_%d_%d", pNode->iLine, pNode->iColumn);

			pTempBind = xlHirCreateNode(XL_HIR_VAR, pNode);
			if ( pTempBind != NULL ) {
				xlHirSetText(pTempBind, sTempName);
				xlHirSetTypeName(pTempBind, "array<var>");
				xlHirAppendChild(pTempBind, xlLowerExprToHir(pValueNode, pSem, pDiag, bTraceLower));
				xlHirAppendChild(pBlock, pTempBind);
			}

			for ( i = 0; (pTargets != NULL) && (i < pTargets->iChildCount); ++i ) {
				const XL_AstNode* pTarget;
				const XL_SemNodeInfo* pTargetInfo;
				XL_HirNode* pBind;
				XL_HirNode* pIndex;

				pTarget = pTargets->ppChildren[i];
				if ( (pTarget == NULL) || (pTarget->sText == NULL) || (strcmp(pTarget->sText, "_") == 0) ) {
					continue;
				}
				pBind = xlHirCreateNode(XL_HIR_VAR, pTarget);
				pIndex = xlHirCreateNode(XL_HIR_INDEX, pTarget);
				if ( (pBind == NULL) || (pIndex == NULL) ) {
					xlHirFree(pBind);
					xlHirFree(pIndex);
					continue;
				}
				xlHirSetText(pBind, pTarget->sText);
				pTargetInfo = (pSem != NULL) ? xlSemNodeInfoFind(pSem, pTarget) : NULL;
				if ( (pTargetInfo != NULL) && (pTargetInfo->sTypeName != NULL) ) {
					xlHirSetTypeName(pBind, pTargetInfo->sTypeName);
					xlHirSetTypeName(pIndex, pTargetInfo->sTypeName);
				}
				xlHirAppendChild(pIndex, __xlCreateNameHir(pTarget, sTempName, "array<var>"));
				xlHirAppendChild(pIndex, __xlCreateLiteralHir(pTarget, "int", NULL, (int64_t)i));
				xlHirAppendChild(pBind, pIndex);
				xlHirAppendChild(pBlock, pBind);
			}
			return pBlock;
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
			{
				const XL_AstNode* pValues;

				pValues = __xlAstChild(pNode, 0);
				if ( (pValues != NULL) && (pValues->iChildCount > 1) ) {
					xlHirAppendChild(pHir, __xlCreateArrayLiteralCallHir(pNode, pValues, pSem, pDiag, bTraceLower));
				} else if ( (pValues != NULL) && (pValues->iChildCount != 0) ) {
					xlHirAppendChild(pHir, xlLowerExprToHir(pValues->ppChildren[0], pSem, pDiag, bTraceLower));
				}
			}
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
		case XL_AST_EXPR_STMT:
			if ( (__xlAstChild(pNode, 0) != NULL) && (__xlAstChild(pNode, 0)->eKind == XL_AST_YIELD) ) {
				return __xlLowerStmtToHir(__xlAstChild(pNode, 0), pSem, pDiag, bTraceLower);
			}
			return xlLowerExprToHir(pNode, pSem, pDiag, bTraceLower);
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
	xlHybridSmFree(&pResult->hybridSm);
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
	pResult->pSemResult = pSem;
	__xlApplySemNodeInfo(pResult->pHirRoot, pSem, pRoot);

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		__xlCollectClassCreatesToHir(pResult->pHirRoot, pRoot->ppChildren[i], pSem, pDiag, bTraceLower);
	}
	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		xlHirAppendChild(pResult->pHirRoot, __xlLowerStmtToHir(pRoot->ppChildren[i], pSem, pDiag, bTraceLower));
	}
	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		__xlCollectFuncExprsToHir(pResult->pHirRoot, pRoot->ppChildren[i], pSem, pDiag, bTraceLower);
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
	if ( xlLowerBuildHybridSm(&pResult->asyncSm, &pResult->generatorSm, pDiag, &pResult->hybridSm) != XL_RC_OK ) {
		xlLowerResultFree(pResult);
		return XL_RC_IO_ERROR;
	}
	if ( __xlValidateHybridStateMachines(pResult, pDiag) != XL_RC_OK ) {
		xlLowerResultFree(pResult);
		return XL_RC_FAIL;
	}

	return XL_RC_OK;
}
