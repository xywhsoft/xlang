#ifndef XL_FUNC_H
#define XL_FUNC_H

#include "../common/xl_common.h"
#include "../ast/xl_ast.h"

#define XL_FUNC_VARARG_NONE 0
#define XL_FUNC_VARARG_ARRAY 1
#define XL_FUNC_VARARG_DICT 2

#define XL_FUNC_LANGUAGE_ABI_V1 "xlang@v1"
#define XL_FUNC_NATIVE_ABI_V1 "c@v1"

typedef struct XL_FuncInfo {
	char* sName;
	char* sReceiverType;
	char* sMethodName;
	char* sReturnTypeName;
	char* sStorageReturnTypeName;
	char* sGeneratorValueTypeName;
	char* sOverloadGroup;
	char* sSignatureId;
	char* sDispatchKey;
	char* sLanguageAbi;
	char* sNativeAbi;
	char* sAbiSymbol;
	char** arrParamNames;
	char** arrParamTypes;
	char** arrReturnTypeNames;
	char** arrTypeParamNames;
	char** arrGenericParamTypes;
	char** arrGenericReturnTypeNames;
	char** arrGenericInstanceTypeArgs;
	char** arrCaptureNames;
	char** arrCaptureTypeNames;
	int* arrCaptureSymbolIndexes;
	unsigned char* arrParamHasDefault;
	const XL_AstNode** arrParamDefaultValues;
	const XL_AstNode* pDeclNode;
	size_t iParamCount;
	size_t iReturnCount;
	size_t iTypeParamCount;
	size_t iGenericInstanceTypeArgCount;
	size_t iCaptureCount;
	int iGenericBaseFuncIndex;
	int iScopeId;
	int iLine;
	int iColumn;
	int bAsync;
	int bGenerator;
	int eVarArgKind;
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
	int bGenerator,
	int eVarArgKind
);
int xlFuncTableAddEx(
	XL_FuncTable* pTable,
	const char* sName,
	const char* sReceiverType,
	const char* sMethodName,
	const char* sReturnTypeName,
	const char* sStorageReturnTypeName,
	const char* const* arrReturnTypeNames,
	size_t iReturnCount,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount,
	const unsigned char* arrParamHasDefault,
	const XL_AstNode* const* arrParamDefaultValues,
	int iScopeId,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator,
	int eVarArgKind
);
const XL_FuncInfo* xlFuncTableGet(const XL_FuncTable* pTable, size_t iIndex);
XL_FuncInfo* xlFuncTableGetMutable(XL_FuncTable* pTable, size_t iIndex);
int xlFuncInfoRefreshDerivedNames(XL_FuncInfo* pInfo);
int xlFuncInfoAddCapture(XL_FuncInfo* pInfo, const char* sName, const char* sTypeName, int iSymbolIndex);
int xlFuncInfoSetGenerics(
	XL_FuncInfo* pInfo,
	const char* const* arrTypeParamNames,
	size_t iTypeParamCount,
	const char* const* arrGenericParamTypes,
	size_t iGenericParamCount,
	const char* const* arrGenericReturnTypeNames,
	size_t iGenericReturnCount
);
int xlFuncInfoSetGenericInstance(
	XL_FuncInfo* pInfo,
	int iGenericBaseFuncIndex,
	const char* const* arrTypeArgs,
	size_t iTypeArgCount
);

#endif
