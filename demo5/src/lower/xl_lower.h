#ifndef XL_LOWER_H
#define XL_LOWER_H

#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"
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
