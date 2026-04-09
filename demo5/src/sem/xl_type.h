#ifndef XL_TYPE_H
#define XL_TYPE_H

#include "../common/xl_common.h"

// 语义层中的类型注册信息。
typedef struct XL_TypeInfo {
	char* sName;
	int bBuiltin;
	int iLine;
	int iColumn;
} XL_TypeInfo;

// 类型注册表，当前阶段只保留稳定的查表接口。
typedef struct XL_TypeTable {
	XL_TypeInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_TypeTable;

void xlTypeTableInit(XL_TypeTable* pTable);
void xlTypeTableFree(XL_TypeTable* pTable);
int xlTypeTableAdd(XL_TypeTable* pTable, const char* sName, int bBuiltin, int iLine, int iColumn);
const XL_TypeInfo* xlTypeTableFind(const XL_TypeTable* pTable, const char* sName);
const XL_TypeInfo* xlTypeTableGet(const XL_TypeTable* pTable, size_t iIndex);

#endif
