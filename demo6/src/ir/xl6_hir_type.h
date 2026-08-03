#ifndef XL6_HIR_TYPE_H
#define XL6_HIR_TYPE_H

#include "src/ast/xl6_ast.h"
#include "src/type/xl6_type_id.h"

typedef XL6_TypeId XL6_HirTypeId;

#define XL6_HIR_TYPE_INVALID 0u

typedef enum XL6_HirTypeKind {
    XL6_HIR_TYPE_BUILTIN = 1,
    XL6_HIR_TYPE_CLASS,
    XL6_HIR_TYPE_UNION,
    XL6_HIR_TYPE_FUNCTION,
    XL6_HIR_TYPE_TUPLE,
    XL6_HIR_TYPE_GENERIC_INSTANCE,
    XL6_HIR_TYPE_TYPE_PARAM,
    XL6_HIR_TYPE_NULLABLE,
    XL6_HIR_TYPE_WEAK,
    XL6_HIR_TYPE_PROTOCOL,
    XL6_HIR_TYPE_NUMERIC_ENUM,
    XL6_HIR_TYPE_TAGGED_ENUM
} XL6_HirTypeKind;

typedef struct XL6_HirType {
    XL6_HirTypeKind kind;
    XL6_TypeId sourceTypeId;
    XL6_TypeAbiId abiId;
    /* name 只用于诊断和 dump，不参与类型身份计算。 */
    char* name;
    XL6_HirTypeId* params;
    size_t paramCount;
    XL6_HirTypeId* returns;
    size_t returnCount;
    uint64 hash;
} XL6_HirType;

typedef struct XL6_HirTypePool {
    XL6_HirType* items;
    size_t count;
    size_t capacity;
    size_t* slots;
    size_t slotCapacity;
} XL6_HirTypePool;

void xl6HirTypePoolInit(XL6_HirTypePool* pool);
void xl6HirTypePoolFree(XL6_HirTypePool* pool);
XL6_HirTypeId xl6HirTypeInternAst(
    XL6_HirTypePool* pool,
    const XL6_AstNode* moduleRoot,
    const XL6_AstNode* typeNode
);
const XL6_HirType* xl6HirTypeGet(const XL6_HirTypePool* pool, XL6_HirTypeId typeId);
XL6_AstNode* xl6HirTypeCloneAst(
    const XL6_HirTypePool* pool,
    XL6_HirTypeId typeId
);

#endif
