#ifndef XL_RESOLVE_H
#define XL_RESOLVE_H

#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"
#include "xl_check.h"
#include "xl_func.h"
#include "xl_scope.h"
#include "xl_symbol.h"
#include "xl_type.h"

typedef enum XL_SemDiagLevel {
	XL_SEM_DIAG_INFO = 0,
	XL_SEM_DIAG_WARN,
	XL_SEM_DIAG_ERROR
} XL_SemDiagLevel;

// 语义阶段的诊断对象，便于后续统一 dump。
typedef struct XL_SemDiagItem {
	XL_SemDiagLevel eLevel;
	char* sMessage;
	int iLine;
	int iColumn;
} XL_SemDiagItem;

typedef struct XL_SemDiagList {
	XL_SemDiagItem* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_SemDiagList;

typedef struct XL_SemNodeInfo {
	const XL_AstNode* pNode;
	int iScopeId;
	int iSymbolIndex;
	int iFuncIndex;
	char* sTypeName;
} XL_SemNodeInfo;

typedef struct XL_SemNodeTable {
	XL_SemNodeInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_SemNodeTable;

typedef struct XL_SemResult {
	XL_ScopeTable scopes;
	XL_SymbolTable symbols;
	XL_TypeTable types;
	XL_ProtocolTable protocols;
	XL_FuncTable funcs;
	XL_SemDiagList diags;
	XL_SemNodeTable nodes;
	int bHadError;
} XL_SemResult;

void xlSemResultInit(XL_SemResult* pResult);
void xlSemResultFree(XL_SemResult* pResult);
int xlSemAnalyzeAst(const XL_AstNode* pRoot, XL_DiagContext* pDiag, int bTraceSem, XL_SemResult* pResult);
const char* xlSemDiagLevelName(XL_SemDiagLevel eLevel);
const XL_SemNodeInfo* xlSemNodeInfoFind(const XL_SemResult* pResult, const XL_AstNode* pNode);

#endif
