#ifndef XL_FUNC_H
#define XL_FUNC_H

#include "../common/xl_common.h"

typedef struct XL_AstNode XL_AstNode;

typedef struct XL_FuncInfo {
	char* sName;
	char* sReceiverType;
	char* sMethodName;
	char* sReturnTypeName;
	char* sOverloadGroup;
	char* sSignatureId;
	char* sDispatchKey;
	char* sAbiSymbol;
	char** arrParamNames;
	char** arrParamTypes;
	unsigned char* arrParamHasDefault;
	const XL_AstNode** arrParamDefaultValues;
	size_t iParamCount;
	int iScopeId;
	int iLine;
	int iColumn;
	int bAsync;
	int bGenerator;
} XL_FuncInfo;

typedef struct XL_FuncTable {
	XL_FuncInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_FuncTable;

void xlFuncTableInit(XL_FuncTable* pTable);
void xlFuncTableFree(XL_FuncTable* pTable);
int xlFuncTableAdd(
	XL_FuncTable* pTable,
	const char* sName,
	const char* sReceiverType,
	const char* sMethodName,
	const char* sReturnTypeName,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount,
	const unsigned char* arrParamHasDefault,
	const XL_AstNode* const* arrParamDefaultValues,
	int iScopeId,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator
);
const XL_FuncInfo* xlFuncTableGet(const XL_FuncTable* pTable, size_t iIndex);

#endif
