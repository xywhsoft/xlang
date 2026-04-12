#ifndef XL_TYPE_H
#define XL_TYPE_H

#include "../common/xl_common.h"

// 语义层中的类型注册信息。
typedef struct XL_TypeInfo {
	char* sName;
	char* sAliasTarget;
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

typedef enum XL_ProtocolKind {
	XL_PROTOCOL_NONE = 0,
	XL_PROTOCOL_CALLABLE = 1 << 0,
	XL_PROTOCOL_INDEXABLE = 1 << 1,
	XL_PROTOCOL_ITERABLE = 1 << 2,
	XL_PROTOCOL_ITERATOR = 1 << 3,
	XL_PROTOCOL_AWAITABLE = 1 << 4,
	XL_PROTOCOL_GENERATOR = 1 << 5
} XL_ProtocolKind;

typedef struct XL_ProtocolInfo {
	char* sTypeName;
	unsigned int uMask;
} XL_ProtocolInfo;

typedef struct XL_ProtocolTable {
	XL_ProtocolInfo* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_ProtocolTable;

void xlTypeTableInit(XL_TypeTable* pTable);
void xlTypeTableFree(XL_TypeTable* pTable);
int xlTypeTableAdd(XL_TypeTable* pTable, const char* sName, int bBuiltin, int iLine, int iColumn);
int xlTypeTableAddAlias(XL_TypeTable* pTable, const char* sName, const char* sAliasTarget, int iLine, int iColumn);
const XL_TypeInfo* xlTypeTableFind(const XL_TypeTable* pTable, const char* sName);
const XL_TypeInfo* xlTypeTableGet(const XL_TypeTable* pTable, size_t iIndex);

void xlProtocolTableInit(XL_ProtocolTable* pTable);
void xlProtocolTableFree(XL_ProtocolTable* pTable);
int xlProtocolTableAdd(XL_ProtocolTable* pTable, const char* sTypeName, XL_ProtocolKind eProtocol);
const XL_ProtocolInfo* xlProtocolTableFind(const XL_ProtocolTable* pTable, const char* sTypeName);
int xlProtocolTableHas(const XL_ProtocolTable* pTable, const char* sTypeName, XL_ProtocolKind eProtocol);

#endif
