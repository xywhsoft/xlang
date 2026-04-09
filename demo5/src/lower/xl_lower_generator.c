#include "xl_lower.h"

typedef struct XL_GeneratorTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_GeneratorTextBuffer;

void xlLowerGeneratorTouch(void)
{
}

static char* __xlGeneratorDupText(const char* sText)
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

static int __xlGeneratorBufGrow(XL_GeneratorTextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 256 : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) {
		iNewCap *= 2;
	}

	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return XL_RC_OK;
}

static void __xlGeneratorBufAppend(XL_GeneratorTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlGeneratorBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlGeneratorBufAppendFormat(XL_GeneratorTextBuffer* pBuf, const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return;
	}

	if ( __xlGeneratorBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlGeneratorAppendEscaped(XL_GeneratorTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlGeneratorBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlGeneratorBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlGeneratorBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlGeneratorBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlGeneratorBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlGeneratorBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlGeneratorBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static void __xlLowerGeneratorTrace(
	XL_DiagContext* pDiag,
	int bTraceLower,
	const XL_HirNode* pNode,
	const char* sFormat,
	...
)
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
		"generator node=%s@%d:%d ",
		(pNode != NULL) ? xlHirKindName(pNode->eKind) : "none",
		(pNode != NULL) ? pNode->iLine : 0,
		(pNode != NULL) ? pNode->iColumn : 0
	);
	if ( (iOffset < 0) || ((size_t)iOffset >= sizeof(sText)) ) {
		return;
	}

	va_start(args, sFormat);
	vsnprintf(sText + iOffset, sizeof(sText) - (size_t)iOffset, sFormat, args);
	va_end(args);

	xlDiagEmit(pDiag, XL_DIAG_LEVEL_INFO, "lower.generator", __FILE__, __LINE__, "%s", sText);
}

static int __xlGeneratorFuncGrow(XL_GeneratorSmFunc* pFunc)
{
	XL_GeneratorSmState* pNewStates;
	size_t iNewCap;

	if ( pFunc->iStateCount < pFunc->iStateCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pFunc->iStateCap == 0) ? 4 : (pFunc->iStateCap * 2);
	pNewStates = (XL_GeneratorSmState*)realloc(pFunc->pStates, sizeof(XL_GeneratorSmState) * iNewCap);
	if ( pNewStates == NULL ) {
		return XL_RC_IO_ERROR;
	}

	memset(pNewStates + pFunc->iStateCap, 0, sizeof(XL_GeneratorSmState) * (iNewCap - pFunc->iStateCap));
	pFunc->pStates = pNewStates;
	pFunc->iStateCap = iNewCap;
	return XL_RC_OK;
}

static int __xlGeneratorSmGrow(XL_GeneratorSm* pSm)
{
	XL_GeneratorSmFunc* pNewFuncs;
	size_t iNewCap;

	if ( pSm->iFuncCount < pSm->iFuncCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pSm->iFuncCap == 0) ? 4 : (pSm->iFuncCap * 2);
	pNewFuncs = (XL_GeneratorSmFunc*)realloc(pSm->pFuncs, sizeof(XL_GeneratorSmFunc) * iNewCap);
	if ( pNewFuncs == NULL ) {
		return XL_RC_IO_ERROR;
	}

	memset(pNewFuncs + pSm->iFuncCap, 0, sizeof(XL_GeneratorSmFunc) * (iNewCap - pSm->iFuncCap));
	pSm->pFuncs = pNewFuncs;
	pSm->iFuncCap = iNewCap;
	return XL_RC_OK;
}

static int __xlGeneratorStringifyNode(const XL_HirNode* pNode, XL_GeneratorTextBuffer* pBuf)
{
	size_t i;

	if ( pNode == NULL ) {
		__xlGeneratorBufAppend(pBuf, "<null>");
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_NAME) || (pNode->eKind == XL_HIR_LITERAL) ) {
		__xlGeneratorBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
		return XL_RC_OK;
	}

	if ( pNode->eKind == XL_HIR_CALL ) {
		__xlGeneratorStringifyNode((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, pBuf);
		__xlGeneratorBufAppend(pBuf, "(");
		if ( pNode->iChildCount > 1 ) {
			const XL_HirNode* pArgs = pNode->ppChildren[1];

			for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
				if ( i != 0 ) {
					__xlGeneratorBufAppend(pBuf, ", ");
				}
				__xlGeneratorStringifyNode(pArgs->ppChildren[i], pBuf);
			}
		}
		__xlGeneratorBufAppend(pBuf, ")");
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_UNARY) && (pNode->sText != NULL) ) {
		__xlGeneratorBufAppend(pBuf, pNode->sText);
		__xlGeneratorBufAppend(pBuf, " ");
		__xlGeneratorStringifyNode((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, pBuf);
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_BINARY) && (pNode->sText != NULL) && (pNode->iChildCount == 2) ) {
		__xlGeneratorStringifyNode(pNode->ppChildren[0], pBuf);
		__xlGeneratorBufAppend(pBuf, " ");
		__xlGeneratorBufAppend(pBuf, pNode->sText);
		__xlGeneratorBufAppend(pBuf, " ");
		__xlGeneratorStringifyNode(pNode->ppChildren[1], pBuf);
		return XL_RC_OK;
	}

	__xlGeneratorBufAppend(pBuf, xlHirKindName(pNode->eKind));
	return XL_RC_OK;
}

static char* __xlGeneratorNodeText(const XL_HirNode* pNode)
{
	XL_GeneratorTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlGeneratorStringifyNode(pNode, &buf);
	return buf.sData;
}

static const XL_HirNode* __xlFindYieldNode(const XL_HirNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return NULL;
	}

	if ( pNode->eKind == XL_HIR_YIELD ) {
		return pNode;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		const XL_HirNode* pYield;

		pYield = __xlFindYieldNode(pNode->ppChildren[i]);
		if ( pYield != NULL ) {
			return pYield;
		}
	}

	return NULL;
}

static int __xlCollectYieldCount(const XL_HirNode* pNode)
{
	size_t i;
	int iCount;

	if ( pNode == NULL ) {
		return 0;
	}

	iCount = (pNode->eKind == XL_HIR_YIELD) ? 1 : 0;
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iCount += __xlCollectYieldCount(pNode->ppChildren[i]);
	}

	return iCount;
}

static int __xlGeneratorAddState(
	XL_GeneratorSmFunc* pFunc,
	const XL_HirNode* pNode,
	const char* sLabel,
	const char* sDetail,
	const char* sYieldExpr,
	int bSuspend,
	int bFinal
)
{
	XL_GeneratorSmState* pState;

	if ( (pFunc == NULL) || (__xlGeneratorFuncGrow(pFunc) != XL_RC_OK) ) {
		return XL_RC_IO_ERROR;
	}

	pState = &pFunc->pStates[pFunc->iStateCount];
	memset(pState, 0, sizeof(*pState));
	pState->iStateId = (int)pFunc->iStateCount;
	pState->iLine = (pNode != NULL) ? pNode->iLine : 0;
	pState->iColumn = (pNode != NULL) ? pNode->iColumn : 0;
	pState->iNextStateId = -1;
	pState->bSuspend = bSuspend;
	pState->bFinal = bFinal;
	pState->sLabel = __xlGeneratorDupText(sLabel);
	pState->sDetail = __xlGeneratorDupText(sDetail);
	pState->sYieldExpr = __xlGeneratorDupText(sYieldExpr);
	if ( ((pState->sLabel == NULL) && (sLabel != NULL)) ||
		((pState->sDetail == NULL) && (sDetail != NULL)) ||
		((pState->sYieldExpr == NULL) && (sYieldExpr != NULL)) ) {
		free(pState->sLabel);
		free(pState->sDetail);
		free(pState->sYieldExpr);
		memset(pState, 0, sizeof(*pState));
		return XL_RC_IO_ERROR;
	}

	pFunc->iStateCount += 1;
	return XL_RC_OK;
}

static int __xlGeneratorAddStatementStates(
	XL_GeneratorSmFunc* pFunc,
	const XL_HirNode* pStmt,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_HirNode* pYield;
	char* sYieldText;
	char* sDetail;
	int iYieldCount;
	int iRc;

	if ( (pFunc == NULL) || (pStmt == NULL) ) {
		return XL_RC_OK;
	}

	pYield = __xlFindYieldNode(pStmt);
	iYieldCount = __xlCollectYieldCount(pStmt);
	sDetail = __xlGeneratorNodeText(pStmt);
	if ( sDetail == NULL ) {
		return XL_RC_IO_ERROR;
	}

	if ( pYield == NULL ) {
		iRc = __xlGeneratorAddState(pFunc, pStmt, "step", sDetail, NULL, 0, 0);
		free(sDetail);
		return iRc;
	}

	sYieldText = __xlGeneratorNodeText((pYield->iChildCount != 0) ? pYield->ppChildren[0] : NULL);
	if ( sYieldText == NULL ) {
		free(sDetail);
		return XL_RC_IO_ERROR;
	}

	__xlLowerGeneratorTrace(
		pDiag,
		bTraceLower,
		pStmt,
		"build generator state func='%s' yieldCount=%d yield='%s'",
		(pFunc->sFuncName != NULL) ? pFunc->sFuncName : "",
		iYieldCount,
		sYieldText
	);
	iRc = __xlGeneratorAddState(pFunc, pStmt, "yield", sDetail, sYieldText, 1, 0);
	free(sYieldText);
	free(sDetail);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlGeneratorAddState(pFunc, pStmt, "resume", "resume after yield", NULL, 0, 0);
	return iRc;
}

static int __xlBuildGeneratorStatesForFunc(
	XL_GeneratorSmFunc* pFunc,
	const XL_HirNode* pFuncNode,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_HirNode* pBody;
	size_t i;
	int iRc;

	pBody = (pFuncNode->iChildCount > 1) ? pFuncNode->ppChildren[1] : NULL;
	iRc = __xlGeneratorAddState(pFunc, pFuncNode, "entry", "generator entry", NULL, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	for ( i = 0; (pBody != NULL) && (i < pBody->iChildCount); ++i ) {
		iRc = __xlGeneratorAddStatementStates(pFunc, pBody->ppChildren[i], pDiag, bTraceLower);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return __xlGeneratorAddState(pFunc, pFuncNode, "done", "generator final", NULL, 0, 1);
}

static void __xlGeneratorFixTransitions(XL_GeneratorSmFunc* pFunc)
{
	size_t i;

	if ( pFunc == NULL ) {
		return;
	}

	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		if ( pFunc->pStates[i].bFinal ) {
			pFunc->pStates[i].iNextStateId = -1;
		} else {
			pFunc->pStates[i].iNextStateId = (int)i + 1;
		}
	}
}

static void __xlGeneratorFreeFunc(XL_GeneratorSmFunc* pFunc)
{
	size_t i;

	if ( pFunc == NULL ) {
		return;
	}

	free(pFunc->sFuncName);
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		free(pFunc->pStates[i].sLabel);
		free(pFunc->pStates[i].sDetail);
		free(pFunc->pStates[i].sYieldExpr);
	}
	free(pFunc->pStates);
	memset(pFunc, 0, sizeof(*pFunc));
}

void xlGeneratorSmInit(XL_GeneratorSm* pSm)
{
	if ( pSm != NULL ) {
		memset(pSm, 0, sizeof(*pSm));
	}
}

void xlGeneratorSmFree(XL_GeneratorSm* pSm)
{
	size_t i;

	if ( pSm == NULL ) {
		return;
	}

	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		__xlGeneratorFreeFunc(&pSm->pFuncs[i]);
	}
	free(pSm->pFuncs);
	memset(pSm, 0, sizeof(*pSm));
}

int xlLowerBuildGeneratorSm(const XL_HirNode* pHirRoot, XL_DiagContext* pDiag, int bTraceLower, XL_GeneratorSm* pSm)
{
	XL_GeneratorSmFunc* pFunc;
	const XL_HirNode* pNode;
	size_t i;
	int iRc;

	if ( (pHirRoot == NULL) || (pSm == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlGeneratorSmInit(pSm);
	for ( i = 0; i < pHirRoot->iChildCount; ++i ) {
		pNode = pHirRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_HIR_FUNC) ) {
			continue;
		}
		if ( __xlCollectYieldCount(pNode) == 0 ) {
			continue;
		}

		if ( __xlGeneratorSmGrow(pSm) != XL_RC_OK ) {
			xlGeneratorSmFree(pSm);
			return XL_RC_IO_ERROR;
		}

		pFunc = &pSm->pFuncs[pSm->iFuncCount];
		memset(pFunc, 0, sizeof(*pFunc));
		pFunc->sFuncName = __xlGeneratorDupText((pNode->sText != NULL) ? pNode->sText : "generator");
		if ( pFunc->sFuncName == NULL ) {
			xlGeneratorSmFree(pSm);
			return XL_RC_IO_ERROR;
		}
		pFunc->iLine = pNode->iLine;
		pFunc->iColumn = pNode->iColumn;
		iRc = __xlBuildGeneratorStatesForFunc(pFunc, pNode, pDiag, bTraceLower);
		if ( iRc != XL_RC_OK ) {
			xlGeneratorSmFree(pSm);
			return iRc;
		}
		__xlGeneratorFixTransitions(pFunc);
		pSm->iFuncCount += 1;
	}

	return XL_RC_OK;
}

char* xlGeneratorSmDumpText(const XL_GeneratorSm* pSm)
{
	XL_GeneratorTextBuffer buf;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlGeneratorBufAppend(&buf, "GeneratorStateMachine\n");
	if ( (pSm == NULL) || (pSm->iFuncCount == 0) ) {
		__xlGeneratorBufAppend(&buf, "\t<none>\n");
		return buf.sData;
	}

	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		__xlGeneratorBufAppendFormat(
			&buf,
			"Func %s @ %d:%d\n",
			(pSm->pFuncs[i].sFuncName != NULL) ? pSm->pFuncs[i].sFuncName : "",
			pSm->pFuncs[i].iLine,
			pSm->pFuncs[i].iColumn
		);
		for ( j = 0; j < pSm->pFuncs[i].iStateCount; ++j ) {
			const XL_GeneratorSmState* pState;

			pState = &pSm->pFuncs[i].pStates[j];
			__xlGeneratorBufAppendFormat(
				&buf,
				"\tS%d label=%s next=%d suspend=%d final=%d @ %d:%d\n",
				pState->iStateId,
				(pState->sLabel != NULL) ? pState->sLabel : "",
				pState->iNextStateId,
				pState->bSuspend,
				pState->bFinal,
				pState->iLine,
				pState->iColumn
			);
			if ( (pState->sDetail != NULL) && (pState->sDetail[0] != '\0') ) {
				__xlGeneratorBufAppendFormat(&buf, "\t\tdetail: %s\n", pState->sDetail);
			}
			if ( (pState->sYieldExpr != NULL) && (pState->sYieldExpr[0] != '\0') ) {
				__xlGeneratorBufAppendFormat(&buf, "\t\tyield: %s\n", pState->sYieldExpr);
			}
		}
	}

	return buf.sData;
}

char* xlGeneratorSmDumpJson(const XL_GeneratorSm* pSm)
{
	XL_GeneratorTextBuffer buf;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlGeneratorBufAppend(&buf, "{\n\t\"funcs\": [");
	if ( (pSm != NULL) && (pSm->iFuncCount != 0) ) {
		__xlGeneratorBufAppend(&buf, "\n");
	}

	for ( i = 0; (pSm != NULL) && (i < pSm->iFuncCount); ++i ) {
		__xlGeneratorBufAppend(&buf, "\t\t{\n\t\t\t\"name\": \"");
		__xlGeneratorAppendEscaped(&buf, pSm->pFuncs[i].sFuncName);
		__xlGeneratorBufAppend(&buf, "\",\n\t\t\t\"line\": ");
		__xlGeneratorBufAppendFormat(&buf, "%d,\n", pSm->pFuncs[i].iLine);
		__xlGeneratorBufAppend(&buf, "\t\t\t\"column\": ");
		__xlGeneratorBufAppendFormat(&buf, "%d,\n", pSm->pFuncs[i].iColumn);
		__xlGeneratorBufAppend(&buf, "\t\t\t\"states\": [");
		if ( pSm->pFuncs[i].iStateCount != 0 ) {
			__xlGeneratorBufAppend(&buf, "\n");
		}

		for ( j = 0; j < pSm->pFuncs[i].iStateCount; ++j ) {
			const XL_GeneratorSmState* pState;

			pState = &pSm->pFuncs[i].pStates[j];
			__xlGeneratorBufAppend(&buf, "\t\t\t\t{\n\t\t\t\t\t\"id\": ");
			__xlGeneratorBufAppendFormat(&buf, "%d,\n", pState->iStateId);
			__xlGeneratorBufAppend(&buf, "\t\t\t\t\t\"label\": \"");
			__xlGeneratorAppendEscaped(&buf, pState->sLabel);
			__xlGeneratorBufAppend(&buf, "\",\n\t\t\t\t\t\"next\": ");
			__xlGeneratorBufAppendFormat(&buf, "%d,\n", pState->iNextStateId);
			__xlGeneratorBufAppend(&buf, "\t\t\t\t\t\"suspend\": ");
			__xlGeneratorBufAppend(&buf, pState->bSuspend ? "1" : "0");
			__xlGeneratorBufAppend(&buf, ",\n\t\t\t\t\t\"final\": ");
			__xlGeneratorBufAppend(&buf, pState->bFinal ? "1" : "0");
			__xlGeneratorBufAppend(&buf, ",\n\t\t\t\t\t\"detail\": \"");
			__xlGeneratorAppendEscaped(&buf, pState->sDetail);
			__xlGeneratorBufAppend(&buf, "\",\n\t\t\t\t\t\"yield\": ");
			if ( (pState->sYieldExpr != NULL) && (pState->sYieldExpr[0] != '\0') ) {
				__xlGeneratorBufAppend(&buf, "\"");
				__xlGeneratorAppendEscaped(&buf, pState->sYieldExpr);
				__xlGeneratorBufAppend(&buf, "\"");
			} else {
				__xlGeneratorBufAppend(&buf, "null");
			}
			__xlGeneratorBufAppend(&buf, "\n\t\t\t\t}");
			if ( (j + 1) != pSm->pFuncs[i].iStateCount ) {
				__xlGeneratorBufAppend(&buf, ",");
			}
			__xlGeneratorBufAppend(&buf, "\n");
		}

		if ( pSm->pFuncs[i].iStateCount != 0 ) {
			__xlGeneratorBufAppend(&buf, "\t\t\t");
		}
		__xlGeneratorBufAppend(&buf, "]\n\t\t}");
		if ( (i + 1) != pSm->iFuncCount ) {
			__xlGeneratorBufAppend(&buf, ",");
		}
		__xlGeneratorBufAppend(&buf, "\n");
	}

	if ( (pSm != NULL) && (pSm->iFuncCount != 0) ) {
		__xlGeneratorBufAppend(&buf, "\t");
	}
	__xlGeneratorBufAppend(&buf, "]\n}\n");
	return buf.sData;
}
