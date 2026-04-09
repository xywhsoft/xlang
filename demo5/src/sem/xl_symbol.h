#ifndef XL_SYMBOL_H
#define XL_SYMBOL_H

#include "../common/xl_common.h"
#include "xl_scope.h"

typedef enum XL_SymbolKind {
	XL_SYMBOL_FUNC = 0,
	XL_SYMBOL_PARAM,
	XL_SYMBOL_CONST,
	XL_SYMBOL_VAR,
	XL_SYMBOL_IMPORT,
	XL_SYMBOL_EXPORT
} XL_SymbolKind;

// 符号表项，后续可继续附加绑定结果。
typedef struct XL_SymbolInfo {
	XL_SymbolKind eKind;
	int iScopeId;
	char* sName;
	char* sTypeName;
	int iLine;
	int iColumn;
	int bAsync;
	int bGenerator;
} XL_SymbolInfo;

typedef struct XL_SymbolTable {
	XL_SymbolInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_SymbolTable;

void xlSymbolTableInit(XL_SymbolTable* pTable);
void xlSymbolTableFree(XL_SymbolTable* pTable);
int xlSymbolTableAdd(
	XL_SymbolTable* pTable,
	XL_SymbolKind eKind,
	int iScopeId,
	const char* sName,
	const char* sTypeName,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator
);
int xlSymbolTableFindInScope(const XL_SymbolTable* pTable, int iScopeId, const char* sName);
const XL_SymbolInfo* xlSymbolTableFindVisible(
	const XL_SymbolTable* pTable,
	const XL_ScopeTable* pScopes,
	int iScopeId,
	const char* sName
);
const XL_SymbolInfo* xlSymbolTableGet(const XL_SymbolTable* pTable, size_t iIndex);
const char* xlSymbolKindName(XL_SymbolKind eKind);

#endif
