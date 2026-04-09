#ifndef XL_SCOPE_H
#define XL_SCOPE_H

#include "../common/xl_common.h"

typedef enum XL_ScopeKind {
	XL_SCOPE_MODULE = 0,
	XL_SCOPE_FUNC,
	XL_SCOPE_BLOCK
} XL_ScopeKind;

// 作用域树节点。
typedef struct XL_ScopeInfo {
	int iId;
	int iParentId;
	XL_ScopeKind eKind;
	char* sName;
	int iDepth;
	int iLine;
	int iColumn;
} XL_ScopeInfo;

typedef struct XL_ScopeTable {
	XL_ScopeInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_ScopeTable;

void xlScopeTableInit(XL_ScopeTable* pTable);
void xlScopeTableFree(XL_ScopeTable* pTable);
int xlScopeTableAdd(
	XL_ScopeTable* pTable,
	XL_ScopeKind eKind,
	int iParentId,
	const char* sName,
	int iLine,
	int iColumn,
	int* piScopeId
);
const XL_ScopeInfo* xlScopeTableGet(const XL_ScopeTable* pTable, int iScopeId);
const char* xlScopeKindName(XL_ScopeKind eKind);

#endif
