#include "src/ir/xl6_hir_type.h"

#include "src/common/xl6_common.h"
#include "src/type/xl6_type_model.h"

/* ------------------------------------ 类型键 ------------------------------------ */

static uint64 xl6HirTypeHashBytes(uint64 hash, const void* data, size_t size)
{
    const unsigned char* bytes;
    size_t i;

    bytes = (const unsigned char*)data;
    for ( i = 0; i < size; ++i ) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64 xl6HirTypeHash(
    XL6_HirTypeKind kind,
    XL6_TypeId sourceTypeId,
    XL6_TypeAbiId abiId,
    const XL6_HirTypeId* params,
    size_t paramCount,
    const XL6_HirTypeId* returns,
    size_t returnCount
)
{
    uint64 hash;

    hash = UINT64_C(14695981039346656037);
    hash = xl6HirTypeHashBytes(hash, &kind, sizeof(kind));
    if ( kind == XL6_HIR_TYPE_TYPE_PARAM ) {
        hash = xl6HirTypeHashBytes(hash, &sourceTypeId, sizeof(sourceTypeId));
    }
    hash = xl6HirTypeHashBytes(hash, &abiId, sizeof(abiId));
    hash = xl6HirTypeHashBytes(hash, &paramCount, sizeof(paramCount));
    if ( paramCount > 0 ) {
        hash = xl6HirTypeHashBytes(hash, params, paramCount * sizeof(*params));
    }
    hash = xl6HirTypeHashBytes(hash, &returnCount, sizeof(returnCount));
    if ( returnCount > 0 ) {
        hash = xl6HirTypeHashBytes(hash, returns, returnCount * sizeof(*returns));
    }
    return hash;
}

static int xl6HirTypeEquals(
    const XL6_HirType* type,
    XL6_HirTypeKind kind,
    XL6_TypeId sourceTypeId,
    XL6_TypeAbiId abiId,
    const XL6_HirTypeId* params,
    size_t paramCount,
    const XL6_HirTypeId* returns,
    size_t returnCount,
    uint64 hash
)
{
    if ( type == NULL || type->hash != hash || type->kind != kind ||
         type->abiId != abiId ||
         type->paramCount != paramCount || type->returnCount != returnCount ) {
        return 0;
    }
    if ( kind == XL6_HIR_TYPE_TYPE_PARAM &&
         type->sourceTypeId != sourceTypeId ) {
        return 0;
    }
    if ( paramCount > 0 &&
         memcmp(type->params, params, paramCount * sizeof(*params)) != 0 ) {
        return 0;
    }
    return returnCount == 0 ||
           memcmp(type->returns, returns, returnCount * sizeof(*returns)) == 0;
}

/* ------------------------------------ 类型池 ------------------------------------ */

void xl6HirTypePoolInit(XL6_HirTypePool* pool)
{
    if ( pool != NULL ) {
        memset(pool, 0, sizeof(*pool));
    }
}

void xl6HirTypePoolFree(XL6_HirTypePool* pool)
{
    size_t i;

    if ( pool == NULL ) {
        return;
    }
    for ( i = 0; i < pool->count; ++i ) {
        xrtFree(pool->items[i].name);
        xrtFree(pool->items[i].params);
        xrtFree(pool->items[i].returns);
    }
    xrtFree(pool->items);
    xrtFree(pool->slots);
    memset(pool, 0, sizeof(*pool));
}

static int xl6HirTypePoolReindex(XL6_HirTypePool* pool, size_t requiredCount)
{
    size_t capacity;
    size_t* slots;
    size_t i;

    if ( pool->slotCapacity > 0 && requiredCount * 2 <= pool->slotCapacity ) {
        return 1;
    }
    capacity = pool->slotCapacity == 0 ? 128 : pool->slotCapacity * 2;
    while ( requiredCount * 2 > capacity ) {
        capacity *= 2;
    }
    slots = (size_t*)xrtCalloc(capacity, sizeof(*slots));
    if ( slots == NULL ) {
        return 0;
    }
    for ( i = 0; i < pool->count; ++i ) {
        size_t slot = (size_t)(pool->items[i].hash & (uint64)(capacity - 1));
        while ( slots[slot] != 0 ) {
            slot = (slot + 1) & (capacity - 1);
        }
        slots[slot] = i + 1;
    }
    xrtFree(pool->slots);
    pool->slots = slots;
    pool->slotCapacity = capacity;
    return 1;
}

static XL6_HirTypeId xl6HirTypePoolIntern(
    XL6_HirTypePool* pool,
    XL6_HirTypeKind kind,
    XL6_TypeId sourceTypeId,
    XL6_TypeAbiId abiId,
    const char* name,
    const XL6_HirTypeId* params,
    size_t paramCount,
    const XL6_HirTypeId* returns,
    size_t returnCount
)
{
    uint64 hash;
    size_t slot;
    XL6_HirType* item;

    if ( pool == NULL || name == NULL ||
         (kind != XL6_HIR_TYPE_FUNCTION &&
          kind != XL6_HIR_TYPE_TUPLE &&
          kind != XL6_HIR_TYPE_TYPE_PARAM &&
          abiId == XL6_TYPE_ABI_ID_INVALID) ) {
        return XL6_HIR_TYPE_INVALID;
    }
    hash = xl6HirTypeHash(
        kind,
        sourceTypeId,
        abiId,
        params,
        paramCount,
        returns,
        returnCount
    );
    if ( !xl6HirTypePoolReindex(pool, pool->count + 1) ) {
        return XL6_HIR_TYPE_INVALID;
    }

    slot = (size_t)(hash & (uint64)(pool->slotCapacity - 1));
    while ( pool->slots[slot] != 0 ) {
        size_t index = pool->slots[slot] - 1;
        if ( index < pool->count &&
             xl6HirTypeEquals(
                 &pool->items[index],
                 kind,
                 sourceTypeId,
                 abiId,
                 params,
                 paramCount,
                 returns,
                 returnCount,
                 hash
             ) ) {
            return (XL6_HirTypeId)(index + 1);
        }
        slot = (slot + 1) & (pool->slotCapacity - 1);
    }

    if ( pool->count == pool->capacity ) {
        size_t newCapacity = pool->capacity == 0 ? 32 : pool->capacity * 2;
        XL6_HirType* newItems = (XL6_HirType*)xrtRealloc(
            pool->items,
            newCapacity * sizeof(*newItems)
        );
        if ( newItems == NULL ) {
            return XL6_HIR_TYPE_INVALID;
        }
        pool->items = newItems;
        pool->capacity = newCapacity;
    }

    item = &pool->items[pool->count];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->sourceTypeId = sourceTypeId;
    item->abiId = abiId;
    item->name = (char*)xrtCopyStr((str)name, strlen(name));
    if ( item->name == NULL ) {
        return XL6_HIR_TYPE_INVALID;
    }
    if ( paramCount > 0 ) {
        item->params = (XL6_HirTypeId*)xrtMalloc(paramCount * sizeof(*params));
        if ( item->params == NULL ) {
            xrtFree(item->name);
            memset(item, 0, sizeof(*item));
            return XL6_HIR_TYPE_INVALID;
        }
        memcpy(item->params, params, paramCount * sizeof(*params));
    }
    if ( returnCount > 0 ) {
        item->returns = (XL6_HirTypeId*)xrtMalloc(returnCount * sizeof(*returns));
        if ( item->returns == NULL ) {
            xrtFree(item->params);
            xrtFree(item->name);
            memset(item, 0, sizeof(*item));
            return XL6_HIR_TYPE_INVALID;
        }
        memcpy(item->returns, returns, returnCount * sizeof(*returns));
    }
    item->paramCount = paramCount;
    item->returnCount = returnCount;
    item->hash = hash;

    pool->count += 1;
    pool->slots[slot] = pool->count;
    return (XL6_HirTypeId)pool->count;
}

const XL6_HirType* xl6HirTypeGet(const XL6_HirTypePool* pool, XL6_HirTypeId typeId)
{
    size_t index;

    if ( pool == NULL || typeId == XL6_HIR_TYPE_INVALID ) {
        return NULL;
    }
    index = (size_t)typeId - 1;
    return index < pool->count ? &pool->items[index] : NULL;
}

static int xl6HirAstAddChild(XL6_AstNode* parent, XL6_AstNode* child)
{
    size_t oldCount;

    if ( parent == NULL || child == NULL ) {
        return 0;
    }
    oldCount = parent->childCount;
    xl6AstAddChild(parent, child);
    return parent->childCount == oldCount + 1;
}

static XL6_AstNode* xl6HirTypeCloneAstDepth(
    const XL6_HirTypePool* pool,
    XL6_HirTypeId typeId,
    size_t depth
)
{
    const XL6_HirType* type;
    XL6_AstNode* result;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    size_t i;

    if ( depth > 128 ) {
        return NULL;
    }
    type = xl6HirTypeGet(pool, typeId);
    if ( type == NULL || type->name == NULL ) {
        return NULL;
    }
    result = xl6AstNewText(
        XL6_AST_TYPE,
        type->name,
        strlen(type->name),
        NULL
    );
    if ( result == NULL ) {
        return NULL;
    }
    result->semanticTypeId = type->sourceTypeId;
    result->semanticTypeAbiId = type->abiId;
    if ( type->kind == XL6_HIR_TYPE_NULLABLE ) {
        /*
            nullable 是编译器规范类型，不能退化成用户泛型 nullable<T>。
            后端只接受带内部标志的 T?，因此从 typed HIR 重建 AST 视图时
            必须恢复这项语义身份。
        */
        result->flags |= XL6_AST_FLAG_INTERNAL_NULLABLE;
    }

    if ( type->paramCount > 0 ) {
        params = xl6AstNew(XL6_AST_LIST, NULL);
        if ( params == NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        for ( i = 0; i < type->paramCount; ++i ) {
            XL6_AstNode* param = xl6HirTypeCloneAstDepth(
                pool,
                type->params[i],
                depth + 1
            );
            if ( param == NULL || !xl6HirAstAddChild(params, param) ) {
                xl6AstFree(param);
                xl6AstFree(params);
                xl6AstFree(result);
                return NULL;
            }
        }
        if ( !xl6HirAstAddChild(result, params) ) {
            xl6AstFree(params);
            xl6AstFree(result);
            return NULL;
        }
    }

    if ( type->kind == XL6_HIR_TYPE_FUNCTION ) {
        returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        if ( returns == NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        for ( i = 0; i < type->returnCount; ++i ) {
            XL6_AstNode* returnType = xl6HirTypeCloneAstDepth(
                pool,
                type->returns[i],
                depth + 1
            );
            if ( returnType == NULL || !xl6HirAstAddChild(returns, returnType) ) {
                xl6AstFree(returnType);
                xl6AstFree(returns);
                xl6AstFree(result);
                return NULL;
            }
        }
        /* function 即使没有参数，也保留稳定的 children[0]/[1] 形态。 */
        if ( result->childCount == 0 ) {
            params = xl6AstNew(XL6_AST_LIST, NULL);
            if ( params == NULL || !xl6HirAstAddChild(result, params) ) {
                xl6AstFree(params);
                xl6AstFree(returns);
                xl6AstFree(result);
                return NULL;
            }
        }
        if ( !xl6HirAstAddChild(result, returns) ) {
            xl6AstFree(returns);
            xl6AstFree(result);
            return NULL;
        }
    }

    if ( type->kind == XL6_HIR_TYPE_CLASS ) {
        XL6_AstNode* wrapper;
        XL6_AstNode* args;

        wrapper = xl6AstNewText(
            XL6_AST_TYPE,
            "__class_handle",
            sizeof("__class_handle") - 1,
            NULL
        );
        args = xl6AstNew(XL6_AST_LIST, NULL);
        if ( wrapper == NULL || args == NULL ) {
            xl6AstFree(wrapper);
            xl6AstFree(args);
            xl6AstFree(result);
            return NULL;
        }
        wrapper->flags |= XL6_AST_FLAG_CLASS_HANDLE;
        if ( !xl6HirAstAddChild(args, result) ) {
            xl6AstFree(wrapper);
            xl6AstFree(args);
            xl6AstFree(result);
            return NULL;
        }
        if ( !xl6HirAstAddChild(wrapper, args) ) {
            xl6AstFree(wrapper);
            xl6AstFree(args);
            return NULL;
        }
        return wrapper;
    }
    return result;
}

XL6_AstNode* xl6HirTypeCloneAst(
    const XL6_HirTypePool* pool,
    XL6_HirTypeId typeId
)
{
    return xl6HirTypeCloneAstDepth(pool, typeId, 0);
}

/* ------------------------------------ AST 类型驻留 ------------------------------------ */

static int xl6HirTypeInternList(
    XL6_HirTypePool* pool,
    const XL6_AstNode* moduleRoot,
    const XL6_AstNode* list,
    XL6_HirTypeId** outItems,
    size_t* outCount
)
{
    XL6_HirTypeId* items;
    size_t i;

    *outItems = NULL;
    *outCount = 0;
    if ( list == NULL || list->childCount == 0 ) {
        return 1;
    }
    items = (XL6_HirTypeId*)xrtCalloc(list->childCount, sizeof(*items));
    if ( items == NULL ) {
        return 0;
    }
    for ( i = 0; i < list->childCount; ++i ) {
        items[i] = xl6HirTypeInternAst(pool, moduleRoot, list->children[i]);
        if ( items[i] == XL6_HIR_TYPE_INVALID ) {
            xrtFree(items);
            return 0;
        }
    }
    *outItems = items;
    *outCount = list->childCount;
    return 1;
}

static const XL6_AstNode* xl6HirFindTypeDecl(
    const XL6_AstNode* moduleRoot,
    const XL6_AstNode* typeNode
)
{
    size_t i;

    if ( moduleRoot == NULL || typeNode == NULL ) {
        return NULL;
    }
    for ( i = 0; i < moduleRoot->childCount; ++i ) {
        const XL6_AstNode* decl = moduleRoot->children[i];

        if ( decl == NULL ||
             (decl->kind != XL6_AST_CLASS &&
              decl->kind != XL6_AST_PROTOCOL &&
              decl->kind != XL6_AST_ENUM &&
              decl->kind != XL6_AST_UNION) ) {
            continue;
        }
        if ( typeNode->semanticTypeId != XL6_TYPE_ID_INVALID &&
             decl->semanticTypeId == typeNode->semanticTypeId ) {
            return decl;
        }
        if ( typeNode->semanticTypeId == XL6_TYPE_ID_INVALID &&
             decl->text != NULL && typeNode->text != NULL &&
             strcmp(decl->text, typeNode->text) == 0 ) {
            return decl;
        }
    }
    return NULL;
}

static XL6_HirTypeKind xl6HirTypeKindFromAst(
    const XL6_AstNode* moduleRoot,
    const XL6_AstNode* typeNode,
    size_t paramCount
)
{
    const XL6_AstNode* decl;

    if ( strcmp(typeNode->text, "function") == 0 ) {
        return XL6_HIR_TYPE_FUNCTION;
    }
    if ( strcmp(typeNode->text, "__tuple") == 0 ) {
        return XL6_HIR_TYPE_TUPLE;
    }
    if ( strcmp(typeNode->text, "weak") == 0 ) {
        return XL6_HIR_TYPE_WEAK;
    }
    if ( strcmp(typeNode->text, "nullable") == 0 ) {
        return XL6_HIR_TYPE_NULLABLE;
    }
    if ( typeNode->semanticTypeAbiId == XL6_TYPE_ABI_ID_INVALID &&
         typeNode->semanticTypeId != XL6_TYPE_ID_INVALID ) {
        return XL6_HIR_TYPE_TYPE_PARAM;
    }
    if ( (typeNode->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 ) {
        return XL6_HIR_TYPE_CLASS;
    }
    decl = xl6HirFindTypeDecl(moduleRoot, typeNode);
    if ( decl != NULL && decl->kind == XL6_AST_CLASS ) {
        /*
         * 泛型 class 仍然是引用对象。必须先识别声明种类，再判断普通泛型实例；
         * 否则 Builder<int> 会在 HIR 往返后丢失 class handle 语义。
         */
        return XL6_HIR_TYPE_CLASS;
    }
    if ( decl != NULL && decl->kind == XL6_AST_PROTOCOL ) {
        return XL6_HIR_TYPE_PROTOCOL;
    }
    if ( decl != NULL && decl->kind == XL6_AST_UNION ) {
        return XL6_HIR_TYPE_UNION;
    }
    if ( decl != NULL && decl->kind == XL6_AST_ENUM ) {
        return (decl->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ?
            XL6_HIR_TYPE_NUMERIC_ENUM :
            XL6_HIR_TYPE_TAGGED_ENUM;
    }
    if ( paramCount > 0 ||
         strcmp(typeNode->text, "array<var>") == 0 ||
         strcmp(typeNode->text, "dict<var>") == 0 ) {
        return XL6_HIR_TYPE_GENERIC_INSTANCE;
    }
    return XL6_HIR_TYPE_BUILTIN;
}

XL6_HirTypeId xl6HirTypeInternAst(
    XL6_HirTypePool* pool,
    const XL6_AstNode* moduleRoot,
    const XL6_AstNode* typeNode
)
{
    XL6_HirTypeId* params;
    XL6_HirTypeId* returns;
    size_t paramCount;
    size_t returnCount;
    XL6_HirTypeKind kind;
    XL6_HirTypeId typeId;

    typeNode = xl6TypeResolveAlias(moduleRoot, typeNode);
    if ( typeNode != NULL &&
         (typeNode->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 &&
         typeNode->childCount == 1 &&
         typeNode->children[0] != NULL &&
         typeNode->children[0]->childCount == 1 ) {
        typeNode = typeNode->children[0]->children[0];
    }
    if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE ||
         typeNode->text == NULL || typeNode->text[0] == '\0' ) {
        return XL6_HIR_TYPE_INVALID;
    }

    params = NULL;
    returns = NULL;
    paramCount = 0;
    returnCount = 0;
    if ( typeNode->childCount > 0 &&
         !xl6HirTypeInternList(
             pool,
             moduleRoot,
             typeNode->children[0],
             &params,
             &paramCount
         ) ) {
        return XL6_HIR_TYPE_INVALID;
    }
    kind = xl6HirTypeKindFromAst(moduleRoot, typeNode, paramCount);
    if ( kind == XL6_HIR_TYPE_FUNCTION && typeNode->childCount > 1 &&
         !xl6HirTypeInternList(
             pool,
             moduleRoot,
             typeNode->children[1],
             &returns,
             &returnCount
         ) ) {
        xrtFree(params);
        return XL6_HIR_TYPE_INVALID;
    }

    typeId = xl6HirTypePoolIntern(
        pool,
        kind,
        typeNode->semanticTypeId,
        typeNode->semanticTypeAbiId,
        xl6TypeCanonicalBuiltinName(typeNode->text),
        params,
        paramCount,
        returns,
        returnCount
    );
    xrtFree(params);
    xrtFree(returns);
    return typeId;
}
