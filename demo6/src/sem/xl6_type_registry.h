#ifndef XL6_TYPE_REGISTRY_H
#define XL6_TYPE_REGISTRY_H

#include "src/ast/xl6_ast.h"
#include "src/type/xl6_type_id.h"

typedef enum XL6_TypeKind {
    XL6_TYPE_BUILTIN,
    XL6_TYPE_ALIAS,
    XL6_TYPE_CLASS,
    XL6_TYPE_UNION,
    XL6_TYPE_PROTOCOL,
    XL6_TYPE_NUMERIC_ENUM,
    XL6_TYPE_TAGGED_ENUM
} XL6_TypeKind;

typedef struct XL6_TypeInfo {
    XL6_TypeId id;
    XL6_TypeAbiId abiId;
    char* name;
    char* abiName;
    XL6_TypeKind kind;
    int allowGeneric;
    int genericArity;
    XL6_AstNode* decl;
} XL6_TypeInfo;

typedef struct XL6_TypeRegistry {
    XL6_TypeInfo* items;
    size_t count;
    size_t capacity;
    size_t* slots;
    size_t slotCapacity;
    XL6_TypeId nextId;
} XL6_TypeRegistry;

void xl6TypeRegistryInit(XL6_TypeRegistry* registry);
void xl6TypeRegistryFree(XL6_TypeRegistry* registry);
int xl6TypeRegistryAdd(
    XL6_TypeRegistry* registry,
    const char* name,
    const char* abiName,
    XL6_TypeKind kind,
    int allowGeneric,
    int genericArity,
    XL6_AstNode* decl
);
int xl6TypeRegistryAddBuiltins(XL6_TypeRegistry* registry);
int xl6TypeRegistryIsBuiltinName(const char* name);
const XL6_TypeInfo* xl6TypeRegistryFind(const XL6_TypeRegistry* registry, const char* name);
const XL6_TypeInfo* xl6TypeRegistryFindId(
    const XL6_TypeRegistry* registry,
    XL6_TypeId id
);
XL6_TypeId xl6TypeRegistryReserveId(XL6_TypeRegistry* registry);

#endif
