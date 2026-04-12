#include "xl_lower.h"

typedef struct XL_AsyncTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_AsyncTextBuffer;

void xlLowerAsyncTouch(void)
{
}

static char* __xlAsyncDupText(const char* sText)
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

static int __xlAsyncBufGrow(XL_AsyncTextBuffer* pBuf, size_t iNeed)
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

static void __xlAsyncBufAppend(XL_AsyncTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlAsyncBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlAsyncBufAppendFormat(XL_AsyncTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlAsyncBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlAsyncAppendEscaped(XL_AsyncTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlAsyncBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlAsyncBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlAsyncBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlAsyncBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlAsyncBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlAsyncBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlAsyncBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static void __xlLowerAsyncTrace(
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
		"async node=%s@%d:%d ",
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

	xlDiagEmit(pDiag, XL_DIAG_LEVEL_INFO, "lower.async", __FILE__, __LINE__, "%s", sText);
}

static int __xlAsyncFuncGrow(XL_AsyncSmFunc* pFunc)
{
	XL_AsyncSmState* pNewStates;
	size_t iNewCap;

	if ( pFunc->iStateCount < pFunc->iStateCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pFunc->iStateCap == 0) ? 4 : (pFunc->iStateCap * 2);
	pNewStates = (XL_AsyncSmState*)realloc(pFunc->pStates, sizeof(XL_AsyncSmState) * iNewCap);
	if ( pNewStates == NULL ) {
		return XL_RC_IO_ERROR;
	}

	memset(pNewStates + pFunc->iStateCap, 0, sizeof(XL_AsyncSmState) * (iNewCap - pFunc->iStateCap));
	pFunc->pStates = pNewStates;
	pFunc->iStateCap = iNewCap;
	return XL_RC_OK;
}

static int __xlAsyncSmGrow(XL_AsyncSm* pSm)
{
	XL_AsyncSmFunc* pNewFuncs;
	size_t iNewCap;

	if ( pSm->iFuncCount < pSm->iFuncCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pSm->iFuncCap == 0) ? 4 : (pSm->iFuncCap * 2);
	pNewFuncs = (XL_AsyncSmFunc*)realloc(pSm->pFuncs, sizeof(XL_AsyncSmFunc) * iNewCap);
	if ( pNewFuncs == NULL ) {
		return XL_RC_IO_ERROR;
	}

	memset(pNewFuncs + pSm->iFuncCap, 0, sizeof(XL_AsyncSmFunc) * (iNewCap - pSm->iFuncCap));
	pSm->pFuncs = pNewFuncs;
	pSm->iFuncCap = iNewCap;
	return XL_RC_OK;
}

static int __xlAsyncStringifyNode(const XL_HirNode* pNode, XL_AsyncTextBuffer* pBuf)
{
	size_t i;

	if ( pNode == NULL ) {
		__xlAsyncBufAppend(pBuf, "<null>");
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_NAME) || (pNode->eKind == XL_HIR_LITERAL) ) {
		__xlAsyncBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
		return XL_RC_OK;
	}

	if ( pNode->eKind == XL_HIR_CALL ) {
		__xlAsyncStringifyNode((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, pBuf);
		__xlAsyncBufAppend(pBuf, "(");
		if ( pNode->iChildCount > 1 ) {
			const XL_HirNode* pArgs = pNode->ppChildren[1];

			for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
				if ( i != 0 ) {
					__xlAsyncBufAppend(pBuf, ", ");
				}
				__xlAsyncStringifyNode(pArgs->ppChildren[i], pBuf);
			}
		}
		__xlAsyncBufAppend(pBuf, ")");
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_UNARY) && (pNode->sText != NULL) ) {
		__xlAsyncBufAppend(pBuf, pNode->sText);
		__xlAsyncBufAppend(pBuf, " ");
		__xlAsyncStringifyNode((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, pBuf);
		return XL_RC_OK;
	}

	if ( (pNode->eKind == XL_HIR_BINARY) && (pNode->sText != NULL) && (pNode->iChildCount == 2) ) {
		__xlAsyncStringifyNode(pNode->ppChildren[0], pBuf);
		__xlAsyncBufAppend(pBuf, " ");
		__xlAsyncBufAppend(pBuf, pNode->sText);
		__xlAsyncBufAppend(pBuf, " ");
		__xlAsyncStringifyNode(pNode->ppChildren[1], pBuf);
		return XL_RC_OK;
	}

	__xlAsyncBufAppend(pBuf, xlHirKindName(pNode->eKind));
	return XL_RC_OK;
}

static char* __xlAsyncNodeText(const XL_HirNode* pNode)
{
	XL_AsyncTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlAsyncStringifyNode(pNode, &buf);
	return buf.sData;
}

static const XL_HirNode* __xlFindAwaitNode(const XL_HirNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return NULL;
	}

	if ( (pNode->eKind == XL_HIR_UNARY) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) ) {
		return pNode;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		const XL_HirNode* pAwait;

		pAwait = __xlFindAwaitNode(pNode->ppChildren[i]);
		if ( pAwait != NULL ) {
			return pAwait;
		}
	}

	return NULL;
}

static int __xlCollectAwaitCount(const XL_HirNode* pNode)
{
	size_t i;
	int iCount;

	if ( pNode == NULL ) {
		return 0;
	}

	iCount = ((pNode->eKind == XL_HIR_UNARY) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0)) ? 1 : 0;
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iCount += __xlCollectAwaitCount(pNode->ppChildren[i]);
	}

	return iCount;
}

static int __xlAsyncAddState(
	XL_AsyncSmFunc* pFunc,
	const XL_HirNode* pNode,
	const char* sLabel,
	const char* sDetail,
	const char* sAwaitExpr,
	int bSuspend,
	int bFinal
)
{
	XL_AsyncSmState* pState;

	if ( (pFunc == NULL) || (__xlAsyncFuncGrow(pFunc) != XL_RC_OK) ) {
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
	pState->sLabel = __xlAsyncDupText(sLabel);
	pState->sDetail = __xlAsyncDupText(sDetail);
	pState->sAwaitExpr = __xlAsyncDupText(sAwaitExpr);
	if ( ((pState->sLabel == NULL) && (sLabel != NULL)) ||
		((pState->sDetail == NULL) && (sDetail != NULL)) ||
		((pState->sAwaitExpr == NULL) && (sAwaitExpr != NULL)) ) {
		free(pState->sLabel);
		free(pState->sDetail);
		free(pState->sAwaitExpr);
		memset(pState, 0, sizeof(*pState));
		return XL_RC_IO_ERROR;
	}

	pFunc->iStateCount += 1;
	return XL_RC_OK;
}

static int __xlAsyncAddStatementStates(
	XL_AsyncSmFunc* pFunc,
	const XL_HirNode* pStmt,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_HirNode* pAwait;
	char* sAwaitText;
	char* sDetail;
	int iAwaitCount;
	int iRc;

	if ( (pFunc == NULL) || (pStmt == NULL) ) {
		return XL_RC_OK;
	}

	pAwait = __xlFindAwaitNode(pStmt);
	iAwaitCount = __xlCollectAwaitCount(pStmt);
	sDetail = __xlAsyncNodeText(pStmt);
	if ( sDetail == NULL ) {
		return XL_RC_IO_ERROR;
	}

	if ( pAwait == NULL ) {
		iRc = __xlAsyncAddState(pFunc, pStmt, "step", sDetail, NULL, 0, 0);
		free(sDetail);
		return iRc;
	}

	sAwaitText = __xlAsyncNodeText((pAwait->iChildCount != 0) ? pAwait->ppChildren[0] : NULL);
	if ( sAwaitText == NULL ) {
		free(sDetail);
		return XL_RC_IO_ERROR;
	}

	__xlLowerAsyncTrace(
		pDiag,
		bTraceLower,
		pStmt,
		"build async state func='%s' awaitCount=%d await='%s'",
		(pFunc->sFuncName != NULL) ? pFunc->sFuncName : "",
		iAwaitCount,
		sAwaitText
	);
	iRc = __xlAsyncAddState(pFunc, pStmt, "await", sDetail, sAwaitText, 1, 0);
	free(sAwaitText);
	free(sDetail);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlAsyncAddState(pFunc, pStmt, "resume", "resume after await", NULL, 0, 0);
	return iRc;
}

static int __xlAsyncAddNodeStatesRecursive(
	XL_AsyncSmFunc* pFunc,
	const XL_HirNode* pNode,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	size_t i;
	int iRc;

	if ( (pFunc == NULL) || (pNode == NULL) ) {
		return XL_RC_OK;
	}

	switch ( pNode->eKind ) {
		case XL_HIR_BLOCK:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				iRc = __xlAsyncAddNodeStatesRecursive(pFunc, pNode->ppChildren[i], pDiag, bTraceLower);
				if ( iRc != XL_RC_OK ) {
					return iRc;
				}
			}
			return XL_RC_OK;
		case XL_HIR_TRY:
			iRc = __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, pDiag, bTraceLower);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			iRc = __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL, pDiag, bTraceLower);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			return __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 3) ? pNode->ppChildren[3] : NULL, pDiag, bTraceLower);
		case XL_HIR_IF:
			iRc = __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, pDiag, bTraceLower);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			return __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL, pDiag, bTraceLower);
		case XL_HIR_WHILE:
			return __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, pDiag, bTraceLower);
		case XL_HIR_FORIN:
			return __xlAsyncAddNodeStatesRecursive(pFunc, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, pDiag, bTraceLower);
		default:
			return __xlAsyncAddStatementStates(pFunc, pNode, pDiag, bTraceLower);
	}
}

static int __xlBuildAsyncStatesForFunc(
	XL_AsyncSmFunc* pFunc,
	const XL_HirNode* pFuncNode,
	XL_DiagContext* pDiag,
	int bTraceLower
)
{
	const XL_HirNode* pBody;
	size_t i;
	int iRc;

	pBody = (pFuncNode->iChildCount > 1) ? pFuncNode->ppChildren[1] : NULL;
	iRc = __xlAsyncAddState(pFunc, pFuncNode, "entry", "async entry", NULL, 0, 0);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	for ( i = 0; (pBody != NULL) && (i < pBody->iChildCount); ++i ) {
		iRc = __xlAsyncAddNodeStatesRecursive(pFunc, pBody->ppChildren[i], pDiag, bTraceLower);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return __xlAsyncAddState(pFunc, pFuncNode, "done", "async final", NULL, 0, 1);
}

static void __xlAsyncFixTransitions(XL_AsyncSmFunc* pFunc)
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

static void __xlAsyncFreeFunc(XL_AsyncSmFunc* pFunc)
{
	size_t i;

	if ( pFunc == NULL ) {
		return;
	}

	free(pFunc->sFuncName);
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		free(pFunc->pStates[i].sLabel);
		free(pFunc->pStates[i].sDetail);
		free(pFunc->pStates[i].sAwaitExpr);
	}
	free(pFunc->pStates);
	memset(pFunc, 0, sizeof(*pFunc));
}

void xlAsyncSmInit(XL_AsyncSm* pSm)
{
	if ( pSm != NULL ) {
		memset(pSm, 0, sizeof(*pSm));
	}
}

void xlAsyncSmFree(XL_AsyncSm* pSm)
{
	size_t i;

	if ( pSm == NULL ) {
		return;
	}

	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		__xlAsyncFreeFunc(&pSm->pFuncs[i]);
	}
	free(pSm->pFuncs);
	memset(pSm, 0, sizeof(*pSm));
}

int xlLowerBuildAsyncSm(const XL_HirNode* pHirRoot, XL_DiagContext* pDiag, int bTraceLower, XL_AsyncSm* pSm)
{
	XL_AsyncSmFunc* pFunc;
	const XL_HirNode* pNode;
	size_t i;
	int iRc;

	if ( (pHirRoot == NULL) || (pSm == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlAsyncSmInit(pSm);
	for ( i = 0; i < pHirRoot->iChildCount; ++i ) {
		pNode = pHirRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_HIR_FUNC) || (pNode->bFlag == 0) ) {
			continue;
		}

		if ( __xlAsyncSmGrow(pSm) != XL_RC_OK ) {
			xlAsyncSmFree(pSm);
			return XL_RC_IO_ERROR;
		}

		pFunc = &pSm->pFuncs[pSm->iFuncCount];
		memset(pFunc, 0, sizeof(*pFunc));
		pFunc->sFuncName = __xlAsyncDupText((pNode->sText != NULL) ? pNode->sText : "async");
		if ( pFunc->sFuncName == NULL ) {
			xlAsyncSmFree(pSm);
			return XL_RC_IO_ERROR;
		}
		pFunc->iLine = pNode->iLine;
		pFunc->iColumn = pNode->iColumn;
		iRc = __xlBuildAsyncStatesForFunc(pFunc, pNode, pDiag, bTraceLower);
		if ( iRc != XL_RC_OK ) {
			xlAsyncSmFree(pSm);
			return iRc;
		}
		__xlAsyncFixTransitions(pFunc);
		pSm->iFuncCount += 1;
	}

	return XL_RC_OK;
}

char* xlAsyncSmDumpText(const XL_AsyncSm* pSm)
{
	XL_AsyncTextBuffer buf;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlAsyncBufAppend(&buf, "AsyncStateMachine\n");
	if ( (pSm == NULL) || (pSm->iFuncCount == 0) ) {
		__xlAsyncBufAppend(&buf, "\t<none>\n");
		return buf.sData;
	}

	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		__xlAsyncBufAppendFormat(
			&buf,
			"Func %s @ %d:%d\n",
			(pSm->pFuncs[i].sFuncName != NULL) ? pSm->pFuncs[i].sFuncName : "",
			pSm->pFuncs[i].iLine,
			pSm->pFuncs[i].iColumn
		);
		for ( j = 0; j < pSm->pFuncs[i].iStateCount; ++j ) {
			const XL_AsyncSmState* pState;

			pState = &pSm->pFuncs[i].pStates[j];
			__xlAsyncBufAppendFormat(
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
				__xlAsyncBufAppendFormat(&buf, "\t\tdetail: %s\n", pState->sDetail);
			}
			if ( (pState->sAwaitExpr != NULL) && (pState->sAwaitExpr[0] != '\0') ) {
				__xlAsyncBufAppendFormat(&buf, "\t\tawait: %s\n", pState->sAwaitExpr);
			}
		}
	}

	return buf.sData;
}

char* xlAsyncSmDumpJson(const XL_AsyncSm* pSm)
{
	XL_AsyncTextBuffer buf;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlAsyncBufAppend(&buf, "{\n\t\"funcs\": [");
	if ( (pSm != NULL) && (pSm->iFuncCount != 0) ) {
		__xlAsyncBufAppend(&buf, "\n");
	}

	for ( i = 0; (pSm != NULL) && (i < pSm->iFuncCount); ++i ) {
		__xlAsyncBufAppend(&buf, "\t\t{\n\t\t\t\"name\": \"");
		__xlAsyncAppendEscaped(&buf, pSm->pFuncs[i].sFuncName);
		__xlAsyncBufAppend(&buf, "\",\n\t\t\t\"line\": ");
		__xlAsyncBufAppendFormat(&buf, "%d,\n", pSm->pFuncs[i].iLine);
		__xlAsyncBufAppend(&buf, "\t\t\t\"column\": ");
		__xlAsyncBufAppendFormat(&buf, "%d,\n", pSm->pFuncs[i].iColumn);
		__xlAsyncBufAppend(&buf, "\t\t\t\"states\": [");
		if ( pSm->pFuncs[i].iStateCount != 0 ) {
			__xlAsyncBufAppend(&buf, "\n");
		}

		for ( j = 0; j < pSm->pFuncs[i].iStateCount; ++j ) {
			const XL_AsyncSmState* pState;

			pState = &pSm->pFuncs[i].pStates[j];
			__xlAsyncBufAppend(&buf, "\t\t\t\t{\n\t\t\t\t\t\"id\": ");
			__xlAsyncBufAppendFormat(&buf, "%d,\n", pState->iStateId);
			__xlAsyncBufAppend(&buf, "\t\t\t\t\t\"label\": \"");
			__xlAsyncAppendEscaped(&buf, pState->sLabel);
			__xlAsyncBufAppend(&buf, "\",\n\t\t\t\t\t\"next\": ");
			__xlAsyncBufAppendFormat(&buf, "%d,\n", pState->iNextStateId);
			__xlAsyncBufAppend(&buf, "\t\t\t\t\t\"suspend\": ");
			__xlAsyncBufAppend(&buf, pState->bSuspend ? "1" : "0");
			__xlAsyncBufAppend(&buf, ",\n\t\t\t\t\t\"final\": ");
			__xlAsyncBufAppend(&buf, pState->bFinal ? "1" : "0");
			__xlAsyncBufAppend(&buf, ",\n\t\t\t\t\t\"detail\": \"");
			__xlAsyncAppendEscaped(&buf, pState->sDetail);
			__xlAsyncBufAppend(&buf, "\",\n\t\t\t\t\t\"await\": ");
			if ( (pState->sAwaitExpr != NULL) && (pState->sAwaitExpr[0] != '\0') ) {
				__xlAsyncBufAppend(&buf, "\"");
				__xlAsyncAppendEscaped(&buf, pState->sAwaitExpr);
				__xlAsyncBufAppend(&buf, "\"");
			} else {
				__xlAsyncBufAppend(&buf, "null");
			}
			__xlAsyncBufAppend(&buf, "\n\t\t\t\t}");
			if ( (j + 1) != pSm->pFuncs[i].iStateCount ) {
				__xlAsyncBufAppend(&buf, ",");
			}
			__xlAsyncBufAppend(&buf, "\n");
		}

		if ( pSm->pFuncs[i].iStateCount != 0 ) {
			__xlAsyncBufAppend(&buf, "\t\t\t");
		}
		__xlAsyncBufAppend(&buf, "]\n\t\t}");
		if ( (i + 1) != pSm->iFuncCount ) {
			__xlAsyncBufAppend(&buf, ",");
		}
		__xlAsyncBufAppend(&buf, "\n");
	}

	if ( (pSm != NULL) && (pSm->iFuncCount != 0 ) ) {
		__xlAsyncBufAppend(&buf, "\t");
	}
	__xlAsyncBufAppend(&buf, "]\n}\n");
	return buf.sData;
}
