#ifndef XL_LOWER_H
#define XL_LOWER_H

#include <string.h>
#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"
#include "../sem/xl_convert.h"
#include "../sem/xl_resolve.h"
#include "xl_hir.h"
#include "xl_mir.h"

typedef struct XL_AsyncSmState {
	int iStateId;
	char* sLabel;
	char* sDetail;
	char* sAwaitExpr;
	int iNextStateId;
	int iLine;
	int iColumn;
	int bSuspend;
	int bFinal;
} XL_AsyncSmState;

typedef struct XL_AsyncSmFunc {
	char* sFuncName;
	char* sAbiName;
	XL_AsyncSmState* pStates;
	size_t iStateCount;
	size_t iStateCap;
	int iLine;
	int iColumn;
} XL_AsyncSmFunc;

typedef struct XL_AsyncSm {
	XL_AsyncSmFunc* pFuncs;
	size_t iFuncCount;
	size_t iFuncCap;
} XL_AsyncSm;

typedef struct XL_GeneratorSmState {
	int iStateId;
	char* sLabel;
	char* sDetail;
	char* sYieldExpr;
	int iNextStateId;
	int iLine;
	int iColumn;
	int bSuspend;
	int bFinal;
} XL_GeneratorSmState;

typedef struct XL_GeneratorSmFunc {
	char* sFuncName;
	char* sAbiName;
	XL_GeneratorSmState* pStates;
	size_t iStateCount;
	size_t iStateCap;
	int iLine;
	int iColumn;
} XL_GeneratorSmFunc;

typedef struct XL_GeneratorSm {
	XL_GeneratorSmFunc* pFuncs;
	size_t iFuncCount;
	size_t iFuncCap;
} XL_GeneratorSm;

typedef struct XL_HybridSmPoint {
	int iLine;
	int iColumn;
	int iAsyncStateId;
	int iGeneratorStateId;
	int iProjectionGroupId;
	int iProjectedGeneratorStateId;
	int iProjectedPointIndex;
	int bAwait;
	int bYield;
} XL_HybridSmPoint;

typedef struct XL_HybridSmFunc {
	char* sFuncName;
	char* sAbiName;
	XL_HybridSmPoint* pPoints;
	size_t iPointCount;
	size_t iPointCap;
	size_t iAsyncFinalCount;
	size_t iGeneratorFinalCount;
	int iLine;
	int iColumn;
} XL_HybridSmFunc;

typedef struct XL_HybridSm {
	XL_HybridSmFunc* pFuncs;
	size_t iFuncCount;
	size_t iFuncCap;
} XL_HybridSm;

typedef struct XL_LowerResult {
	XL_HirNode* pHirRoot;
	XL_MirNode* pMirRoot;
	const XL_SemResult* pSemResult;
	XL_AsyncSm asyncSm;
	XL_GeneratorSm generatorSm;
	XL_HybridSm hybridSm;
} XL_LowerResult;

static inline const XL_ClassInfo* xlLowerFindClassForConversion(const XL_SemResult* pSem, const char* sTypeName)
{
	size_t i;

	if ( (pSem == NULL) || (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return NULL;
	}
	for ( i = 0; i < pSem->classes.iCount; ++i ) {
		const XL_ClassInfo* pClass = &pSem->classes.pItems[i];

		if ( (pClass->sName != NULL) && (strcmp(pClass->sName, sTypeName) == 0) ) {
			return pClass;
		}
	}
	return NULL;
}

static inline int xlLowerTypeIsObjectClass(const XL_SemResult* pSem, const char* sTypeName)
{
	const XL_ClassInfo* pClass = xlLowerFindClassForConversion(pSem, sTypeName);
	return (pClass != NULL) && (pClass->bIsObject != 0);
}

static inline int xlLowerObjectClassExtends(const XL_SemResult* pSem, const char* sActualType, const char* sExpectedType)
{
	const XL_ClassInfo* pClass;
	const char* sCurrent;

	if ( (pSem == NULL) || (sActualType == NULL) || (sExpectedType == NULL) ) {
		return 0;
	}
	sCurrent = sActualType;
	while ( (sCurrent != NULL) && (sCurrent[0] != '\0') ) {
		if ( strcmp(sCurrent, sExpectedType) == 0 ) {
			return 1;
		}
		pClass = xlLowerFindClassForConversion(pSem, sCurrent);
		if ( pClass == NULL ) {
			break;
		}
		sCurrent = pClass->sBaseName;
	}
	return 0;
}

static inline int xlLowerExtractSingleGenericArg(const char* sTypeName, const char* sBaseName, char* sOut, size_t iOutSize)
{
	const char* sStart;
	const char* sEnd;
	size_t iBaseLen;
	size_t iLen;

	if ( (sTypeName == NULL) || (sBaseName == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	iBaseLen = strlen(sBaseName);
	if ( (strncmp(sTypeName, sBaseName, iBaseLen) != 0) || (sTypeName[iBaseLen] != '<') ) {
		return 0;
	}
	sStart = sTypeName + iBaseLen + 1;
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

static inline int xlLowerContainerDynamicElementConversion(const char* sExpectedType, const char* sActualType, const char* sBaseName)
{
	char sExpectedItem[128];
	char sActualItem[128];

	if ( !xlLowerExtractSingleGenericArg(sExpectedType, sBaseName, sExpectedItem, sizeof(sExpectedItem)) ||
		!xlLowerExtractSingleGenericArg(sActualType, sBaseName, sActualItem, sizeof(sActualItem)) ) {
		return 0;
	}
	if ( strcmp(sExpectedItem, sActualItem) == 0 ) {
		return 0;
	}
	return ((strcmp(sExpectedItem, "var") == 0) || (strcmp(sExpectedItem, "any") == 0));
}

static inline int xlLowerCanAnnotateImplicitConversion(const XL_SemResult* pSem, const char* sExpectedType, const char* sActualType)
{
	XL_Conversion conv;

	if ( (sExpectedType == NULL) || (sExpectedType[0] == '\0') ||
		(sActualType == NULL) || (sActualType[0] == '\0') ||
		(strcmp(sActualType, sExpectedType) == 0) ) {
		return 0;
	}
	if ( xlTypeCanConvertImplicit(sExpectedType, sActualType, &conv) ) {
		return (conv.eKind != XL_CONV_EXACT) && (conv.eKind != XL_CONV_ALIAS);
	}
	if ( xlLowerContainerDynamicElementConversion(sExpectedType, sActualType, "array") ||
		xlLowerContainerDynamicElementConversion(sExpectedType, sActualType, "list") ||
		xlLowerContainerDynamicElementConversion(sExpectedType, sActualType, "set") ||
		xlLowerContainerDynamicElementConversion(sExpectedType, sActualType, "dict") ) {
		return 1;
	}
	if ( ((strcmp(sExpectedType, "var") == 0) || (strcmp(sExpectedType, "any") == 0)) &&
		(xlLowerFindClassForConversion(pSem, sActualType) != NULL) ) {
		return 1;
	}
	if ( (strcmp(sActualType, "null") == 0) && (xlLowerFindClassForConversion(pSem, sExpectedType) != NULL) ) {
		return 1;
	}
	if ( (strcmp(sExpectedType, "object") == 0) && xlLowerTypeIsObjectClass(pSem, sActualType) ) {
		return 1;
	}
	if ( xlLowerTypeIsObjectClass(pSem, sExpectedType) &&
		xlLowerTypeIsObjectClass(pSem, sActualType) &&
		xlLowerObjectClassExtends(pSem, sActualType, sExpectedType) ) {
		return 1;
	}
	return 0;
}

void xlLowerResultInit(XL_LowerResult* pResult);
void xlLowerResultFree(XL_LowerResult* pResult);
int xlLowerAst(
	const XL_AstNode* pRoot,
	const XL_SemResult* pSemResult,
	XL_DiagContext* pDiag,
	int bTraceLower,
	XL_LowerResult* pResult
);
char* xlLowerDumpText(const XL_LowerResult* pResult);
char* xlLowerDumpJson(const XL_LowerResult* pResult);
char* xlHirDumpJson(const XL_HirNode* pNode);
char* xlMirDumpJson(const XL_MirNode* pNode);
void xlAsyncSmInit(XL_AsyncSm* pSm);
void xlAsyncSmFree(XL_AsyncSm* pSm);
int xlLowerBuildAsyncSm(const XL_HirNode* pHirRoot, XL_DiagContext* pDiag, int bTraceLower, XL_AsyncSm* pSm);
char* xlAsyncSmDumpText(const XL_AsyncSm* pSm);
char* xlAsyncSmDumpJson(const XL_AsyncSm* pSm);
void xlGeneratorSmInit(XL_GeneratorSm* pSm);
void xlGeneratorSmFree(XL_GeneratorSm* pSm);
int xlLowerBuildGeneratorSm(const XL_HirNode* pHirRoot, XL_DiagContext* pDiag, int bTraceLower, XL_GeneratorSm* pSm);
char* xlGeneratorSmDumpText(const XL_GeneratorSm* pSm);
char* xlGeneratorSmDumpJson(const XL_GeneratorSm* pSm);
void xlHybridSmInit(XL_HybridSm* pSm);
void xlHybridSmFree(XL_HybridSm* pSm);
int xlLowerBuildHybridSm(const XL_AsyncSm* pAsyncSm, const XL_GeneratorSm* pGeneratorSm, XL_DiagContext* pDiag, XL_HybridSm* pSm);
char* xlLowerDumpStateMachinesText(const XL_LowerResult* pResult);
char* xlLowerDumpStateMachinesJson(const XL_LowerResult* pResult);

#endif
