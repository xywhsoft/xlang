#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_sem_docs.h"
#include "src/sem/xl6_sem_field_attrs.h"
#include "src/sem/xl6_protocol.h"
#include "src/sem/xl6_type_registry.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

typedef enum XL6_SymbolKind {
    XL6_SYM_VALUE,
    XL6_SYM_CONST,
    XL6_SYM_FUNC,
    XL6_SYM_GLOBAL_VALUE,
    XL6_SYM_GLOBAL_CONST
} XL6_SymbolKind;

typedef struct XL6_Symbol {
    char* name;
    XL6_SymbolKind kind;
    XL6_AstNode* type;
    int line;
    int column;
} XL6_Symbol;

typedef struct XL6_Scope {
    XL6_Symbol* items;
    size_t count;
    size_t capacity;
    size_t* slots;
    size_t slotCapacity;
    struct XL6_Scope* parent;
} XL6_Scope;

typedef struct XL6_TypeParamScope {
    XL6_AstNode* params;
    struct XL6_TypeParamScope* parent;
} XL6_TypeParamScope;

typedef struct XL6_SemContext {
    XL6_DiagList* diags;
    XL6_Scope* scope;
    XL6_TypeParamScope* typeParams;
    XL6_AstNode* currentReturns;
    int currentReturnTypesValid;
    XL6_AstNode* root;
    XL6_TypeRegistry types;
    XL6_AstNode** ownedTypeNodes;
    size_t ownedTypeNodeCount;
    size_t ownedTypeNodeCapacity;
    XL6_Scope* captureBoundary;
    int captureBoundaryAllowsCapture;
    XL6_AstNode* currentClass;
    XL6_AstNode* currentFunction;
    const char* implicitSelfName;
    int inClassCreate;
    int loopDepth;
    int catchDepth;
    int generatorDepth;
} XL6_SemContext;

static void xl6SemErrorNode(XL6_SemContext* ctx, const XL6_AstNode* node, const char* message);
static int xl6SemCheckType(XL6_SemContext* ctx, XL6_AstNode* typeNode);
static int xl6SemCheckBoundarySymbolUse(XL6_SemContext* ctx, const XL6_AstNode* node, XL6_Symbol* sym, int writeAccess);
static XL6_AstNode* xl6SemFindTypeDecl(XL6_SemContext* ctx, const char* name);
static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemInferAutoTypeNodeRaw(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemTypeNodeFromConstructorCallee(XL6_SemContext* ctx, const XL6_AstNode* callee);
static XL6_AstNode* xl6SemNewGenericContainerType(const char* name, XL6_AstNode* itemType, const XL6_AstNode* at);
static XL6_AstNode* xl6SemCloneAwaitResultType(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneAsyncCallResultType(XL6_SemContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs, const XL6_AstNode* at);
static const char* xl6SemStaticTypeNameFromTypeNode(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static const char* xl6SemDisplayTypeName(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static void xl6SemFormatTypeName(XL6_SemContext* ctx, const XL6_AstNode* typeNode, char* buffer, size_t capacity);
static void xl6SemTypeMismatchError(XL6_SemContext* ctx, XL6_AstNode* node, const char* reason, const XL6_AstNode* targetType, const char* valueType);
static void xl6SemConversionMismatchError(XL6_SemContext* ctx, XL6_AstNode* node, const char* targetType, const char* sourceType);
static void xl6SemVarBoxMismatchError(XL6_SemContext* ctx, XL6_AstNode* node, const char* reason);
static void xl6SemCountMismatchError(XL6_SemContext* ctx, XL6_AstNode* node, const char* reason, size_t expected, size_t actual);
static int xl6SemCheckKnownCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* func);
static int xl6SemExprFunctionValueMatchAlreadyDiagnosed(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode);
static int xl6SemCheckChannelTypeArg(XL6_SemContext* ctx, const XL6_AstNode* callee);
static int xl6SemChannelElementTypeIsFirstStageSafe(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemChannelElementTypeIsMessageSafe(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemCheckChannelCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemChannelCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemIsSpecialCollectionTypeName(const char* name);
static int xl6SemCheckSpecialCollectionCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemSpecialCollectionCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static XL6_AstNode* xl6SemCloneSpecialCollectionCallType(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckFutureCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemFutureCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static XL6_AstNode* xl6SemCloneFutureValueTypeFromType(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemCheckTaskCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemTaskCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static XL6_AstNode* xl6SemCloneTaskCallType(XL6_SemContext* ctx, XL6_AstNode* call);
static int xl6SemCheckThreadCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemThreadCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckSyncCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemSyncCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemTypeCanCopy(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemTypeCanBeBorrowedParam(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemFuncReturnsFreshOnlyValue(XL6_SemContext* ctx, const XL6_AstNode* func);
static int xl6SemCheckCopyableExpr(XL6_SemContext* ctx, XL6_AstNode* expr, int allowFreshTypeValue, const char* message);
static int xl6SemIsFutureLikeTypeName(const char* name);
static int xl6SemFuncReturnsFutureLikeResource(XL6_SemContext* ctx, const XL6_AstNode* func);
static int xl6SemIsSyncHandleTypeName(const char* name);
static int xl6SemIsThreadTypeName(const char* name);
static int xl6SemIsGeneratorTypeName(const char* name);
static int xl6SemFuncReturnsGenerator(XL6_SemContext* ctx, const XL6_AstNode* func);
static int xl6SemFuncIsAsyncGenerator(XL6_SemContext* ctx, const XL6_AstNode* func);
static int xl6SemIsResourceHandleTypeName(const char* name);
static const XL6_AstNode* xl6SemCallableParams(const XL6_AstNode* node);
static const XL6_AstNode* xl6SemCallableReturns(const XL6_AstNode* node);
static int xl6SemFixedParamCount(const XL6_AstNode* params);
static int xl6SemParamRequiredCount(const XL6_AstNode* params);
static int xl6SemFuncHasVararg(const XL6_AstNode* params);
static int xl6SemCallArgCount(const XL6_AstNode* call);
static XL6_AstNode* xl6SemCallArgExprAt(XL6_AstNode* node, size_t index);
typedef enum XL6_SemContainerKind {
    XL6_SEM_CONTAINER_NONE = 0,
    XL6_SEM_CONTAINER_VAR,
    XL6_SEM_CONTAINER_ARRAY,
    XL6_SEM_CONTAINER_LIST,
    XL6_SEM_CONTAINER_SET,
    XL6_SEM_CONTAINER_DICT
} XL6_SemContainerKind;
static XL6_SemContainerKind xl6SemContainerKindFromType(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int* pTyped);
static const XL6_AstNode* xl6SemContainerItemType(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemContainerMethodSupported(XL6_SemContainerKind kind, int typed, const char* memberName);
static int xl6SemMemberIsContainerMethod(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* memberName);
static int xl6SemBinaryIsContainerOp(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemClassHasTypeParams(const XL6_AstNode* classNode);
static int xl6SemClassHasCreate(const XL6_AstNode* classNode);
static const XL6_AstNode* xl6SemResolveAliasType(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemClassDeclHasNativeHandle(XL6_SemContext* ctx, const XL6_AstNode* classNode);
static const XL6_AstNode* xl6SemTypeNodeGenericArgs(const XL6_AstNode* typeNode);
static const XL6_AstNode* xl6SemClassHandleTargetType(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static const XL6_AstNode* xl6SemClassValueType(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemClassHandleCanWiden(XL6_SemContext* ctx, const XL6_AstNode* sourceType, const XL6_AstNode* targetType);
static int xl6SemTypeUsesClassReferenceSemantics(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemIsIntegerTypeName(const char* name);
static int xl6SemIsFloatTypeName(const char* name);
static int xl6SemIsBoolTypeName(const char* name);
static int xl6SemCheckGenericConstraintDecl(XL6_SemContext* ctx, const XL6_AstNode* param);
static int xl6SemCheckGenericConstraints(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* atNode,
    int report);
static int xl6SemCheckGenericInferredConstraints(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    const XL6_AstNode* atNode,
    int report);
static int xl6SemExprCanMatchParamType(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode);

static int xl6TextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static int xl6SemSymbolIsConst(const XL6_Symbol* sym)
{
    return sym != NULL &&
           (sym->kind == XL6_SYM_CONST || sym->kind == XL6_SYM_GLOBAL_CONST);
}

static int xl6SemSymbolIsGlobalValue(const XL6_Symbol* sym)
{
    return sym != NULL &&
           (sym->kind == XL6_SYM_GLOBAL_VALUE || sym->kind == XL6_SYM_GLOBAL_CONST);
}

static int xl6SemAstHasFlag(const XL6_AstNode* node, unsigned int flag)
{
    return node != NULL && (node->flags & flag) != 0;
}

static int xl6SemIsExternClassProperty(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_GET || node->kind == XL6_AST_LET) &&
           xl6SemAstHasFlag(node, XL6_AST_FLAG_EXTERN);
}

static int xl6SemIsNativeBackedExternMember(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ||
            node->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
            xl6SemIsExternClassProperty(node));
}


static int xl6SemCanAccessClassMember(XL6_SemContext* ctx, const XL6_AstNode* ownerClass, const XL6_AstNode* member)
{
    const XL6_AstNode* currentClass;
    int guard;

    if ( member == NULL ) {
        return 1;
    }
    if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_PRIVATE) ) {
        return ctx != NULL && ownerClass != NULL && ctx->currentClass == ownerClass;
    }
    if ( !xl6SemAstHasFlag(member, XL6_AST_FLAG_PROTECTED) ) {
        return 1;
    }
    if ( ctx == NULL || ownerClass == NULL || ctx->currentClass == NULL ) {
        return 0;
    }

    /* protected 允许当前 class 和任意子类访问，不改变 C 布局。 */
    currentClass = ctx->currentClass;
    for ( guard = 0; guard < 64; ++guard ) {
        const XL6_AstNode* base;
        XL6_AstNode* baseDecl;

        if ( currentClass == NULL || currentClass->kind != XL6_AST_CLASS ) {
            return 0;
        }
        if ( currentClass == ownerClass ||
             (currentClass->text != NULL && ownerClass->text != NULL && xl6TextEquals(currentClass->text, ownerClass->text)) ) {
            return 1;
        }
        base = currentClass->childCount > 0 ? currentClass->children[0] : NULL;
        if ( base == NULL || base->kind != XL6_AST_NAME || base->text == NULL ) {
            return 0;
        }
        baseDecl = xl6SemFindTypeDecl(ctx, base->text);
        if ( baseDecl == currentClass ) {
            return 0;
        }
        currentClass = baseDecl;
    }
    return 0;
}

static int xl6SemCheckClassMemberAccess(
    XL6_SemContext* ctx,
    XL6_AstNode* at,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* member)
{
    if ( xl6SemCanAccessClassMember(ctx, ownerClass, member) ) {
        return 1;
    }
    xl6SemErrorNode(
        ctx,
        at != NULL ? at : (XL6_AstNode*)member,
        xl6SemAstHasFlag(member, XL6_AST_FLAG_PROTECTED) ?
            "protected class member is not accessible" :
            "private class member is not accessible");
    return 0;
}

static int xl6SemIsImplicitSelfTarget(XL6_SemContext* ctx, const XL6_AstNode* node)
{
    return ctx != NULL &&
           node != NULL &&
           node->kind == XL6_AST_NAME &&
           xl6TextEquals(node->text, ctx->implicitSelfName);
}

static int xl6SemTypeIsVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           (xl6TextEquals(typeNode->text, "array<var>") || xl6TextEquals(typeNode->text, "dict<var>"));
}

static int xl6SemTypeIsArrayVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6TextEquals(typeNode->text, "array<var>");
}

static int xl6SemTypeIsDictVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6TextEquals(typeNode->text, "dict<var>");
}

static int xl6SemTypeIsCVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6TextEquals(typeNode->text, "c_vararg");
}

static void xl6SemErrorNode(XL6_SemContext* ctx, const XL6_AstNode* node, const char* message)
{
    if ( (ctx == NULL) || (ctx->diags == NULL) ) {
        return;
    }
    xl6DiagAdd(ctx->diags, node != NULL ? node->line : 1, node != NULL ? node->column : 1, message);
}

static void xl6ScopeInit(XL6_Scope* scope, XL6_Scope* parent)
{
    memset(scope, 0, sizeof(*scope));
    scope->parent = parent;
}

static void xl6ScopeFree(XL6_Scope* scope)
{
    size_t i;

    if ( scope == NULL ) {
        return;
    }
    for ( i = 0; i < scope->count; ++i ) {
        xrtFree(scope->items[i].name);
    }
    xrtFree(scope->items);
    xrtFree(scope->slots);
    memset(scope, 0, sizeof(*scope));
}

static unsigned long long xl6ScopeNameHash(const char* name)
{
    unsigned long long hash = 1469598103934665603ULL;

    while ( name != NULL && *name != '\0' ) {
        hash ^= (unsigned char)*name++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int xl6ScopeEnsureIndex(XL6_Scope* scope, size_t requiredCount)
{
    size_t capacity;
    size_t* slots;
    size_t i;

    if ( scope == NULL ) {
        return 0;
    }
    if ( scope->slotCapacity != 0 && requiredCount * 2 <= scope->slotCapacity ) {
        return 1;
    }
    capacity = scope->slotCapacity == 0 ? 16 : scope->slotCapacity * 2;
    while ( requiredCount * 2 > capacity ) {
        capacity *= 2;
    }
    slots = (size_t*)xrtCalloc(capacity, sizeof(size_t));
    if ( slots == NULL ) {
        return 0;
    }
    for ( i = 0; i < scope->count; ++i ) {
        size_t slot = (size_t)(xl6ScopeNameHash(scope->items[i].name) & (capacity - 1));
        while ( slots[slot] != 0 ) {
            slot = (slot + 1) & (capacity - 1);
        }
        slots[slot] = i + 1;
    }
    xrtFree(scope->slots);
    scope->slots = slots;
    scope->slotCapacity = capacity;
    return 1;
}

static void xl6ScopeIndexLastSymbol(XL6_Scope* scope)
{
    size_t index;
    size_t slot;

    if ( scope == NULL || scope->count == 0 || scope->slotCapacity == 0 ) {
        return;
    }
    index = scope->count - 1;
    slot = (size_t)(xl6ScopeNameHash(scope->items[index].name) & (scope->slotCapacity - 1));
    while ( scope->slots[slot] != 0 ) {
        slot = (slot + 1) & (scope->slotCapacity - 1);
    }
    scope->slots[slot] = index + 1;
}

static XL6_Symbol* xl6ScopeFindLocal(XL6_Scope* scope, const char* name)
{
    size_t slot;
    size_t start;

    if ( scope == NULL || name == NULL || scope->slotCapacity == 0 ) {
        return NULL;
    }
    slot = (size_t)(xl6ScopeNameHash(name) & (scope->slotCapacity - 1));
    start = slot;
    while ( scope->slots[slot] != 0 ) {
        size_t index = scope->slots[slot] - 1;
        if ( index < scope->count && xl6TextEquals(scope->items[index].name, name) ) {
            return &scope->items[index];
        }
        slot = (slot + 1) & (scope->slotCapacity - 1);
        if ( slot == start ) {
            break;
        }
    }
    return NULL;
}

static XL6_Symbol* xl6ScopeFind(XL6_Scope* scope, const char* name)
{
    XL6_Scope* current;

    for ( current = scope; current != NULL; current = current->parent ) {
        XL6_Symbol* sym = xl6ScopeFindLocal(current, name);
        if ( sym != NULL ) {
            return sym;
        }
    }
    return NULL;
}

static int xl6ScopeNameIsOutsideBoundary(XL6_Scope* scope, XL6_Scope* boundary, const char* name)
{
    XL6_Scope* current;
    int crossedBoundary;

    if ( scope == NULL || boundary == NULL || name == NULL ) {
        return 0;
    }
    crossedBoundary = 0;
    for ( current = scope; current != NULL; current = current->parent ) {
        if ( xl6ScopeFindLocal(current, name) != NULL ) {
            return crossedBoundary;
        }
        if ( current == boundary ) {
            crossedBoundary = 1;
        }
    }
    return 0;
}

static int xl6ScopeAddTyped(XL6_SemContext* ctx, const XL6_AstNode* node, XL6_SymbolKind kind, XL6_AstNode* typeNode)
{
    XL6_Symbol* oldSym;
    XL6_Symbol* newItems;
    XL6_Scope* scope;
    size_t newCapacity;

    if ( (ctx == NULL) || (ctx->scope == NULL) || (node == NULL) || (node->text == NULL) ) {
        return 0;
    }

    scope = ctx->scope;
    oldSym = xl6ScopeFindLocal(scope, node->text);
    if ( oldSym != NULL ) {
        xl6SemErrorNode(ctx, node, "同一作用域内重复声明");
        return 0;
    }

    if ( !xl6ScopeEnsureIndex(scope, scope->count + 1) ) {
        xl6SemErrorNode(ctx, node, "符号索引内存不足");
        return 0;
    }

    if ( scope->count == scope->capacity ) {
        newCapacity = (scope->capacity == 0) ? 8 : scope->capacity * 2;
        newItems = (XL6_Symbol*)xrtRealloc(scope->items, newCapacity * sizeof(XL6_Symbol));
        if ( newItems == NULL ) {
            xl6SemErrorNode(ctx, node, "符号表内存不足");
            return 0;
        }
        scope->items = newItems;
        scope->capacity = newCapacity;
    }

    scope->items[scope->count].name = xrtCopyStr(node->text, strlen(node->text));
    scope->items[scope->count].kind = kind;
    scope->items[scope->count].type = typeNode;
    scope->items[scope->count].line = node->line;
    scope->items[scope->count].column = node->column;
    scope->count += 1;
    xl6ScopeIndexLastSymbol(scope);
    return 1;
}

static int xl6ScopeAdd(XL6_SemContext* ctx, const XL6_AstNode* node, XL6_SymbolKind kind)
{
    return xl6ScopeAddTyped(ctx, node, kind, NULL);
}

static void xl6SemPushScope(XL6_SemContext* ctx, XL6_Scope* scope)
{
    xl6ScopeInit(scope, ctx->scope);
    ctx->scope = scope;
}

static void xl6SemPopScope(XL6_SemContext* ctx, XL6_Scope* scope)
{
    ctx->scope = scope->parent;
    xl6ScopeFree(scope);
}

static void xl6SemPushTypeParams(XL6_SemContext* ctx, XL6_TypeParamScope* scope, XL6_AstNode* params)
{
    scope->params = params;
    scope->parent = ctx->typeParams;
    ctx->typeParams = scope;
}

static void xl6SemPopTypeParams(XL6_SemContext* ctx, XL6_TypeParamScope* scope)
{
    ctx->typeParams = scope->parent;
}

static int xl6SemOwnTypeNode(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    XL6_AstNode** newItems;
    size_t newCapacity;

    if ( ctx == NULL || typeNode == NULL ) {
        return 1;
    }
    if ( ctx->ownedTypeNodeCount == ctx->ownedTypeNodeCapacity ) {
        newCapacity = ctx->ownedTypeNodeCapacity == 0 ? 8 : ctx->ownedTypeNodeCapacity * 2;
        newItems = (XL6_AstNode**)xrtRealloc(ctx->ownedTypeNodes, newCapacity * sizeof(XL6_AstNode*));
        if ( newItems == NULL ) {
            return 0;
        }
        ctx->ownedTypeNodes = newItems;
        ctx->ownedTypeNodeCapacity = newCapacity;
    }
    ctx->ownedTypeNodes[ctx->ownedTypeNodeCount++] = typeNode;
    return 1;
}

static void xl6SemFreeOwnedTypeNodes(XL6_SemContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->ownedTypeNodeCount; ++i ) {
        xl6AstFree(ctx->ownedTypeNodes[i]);
    }
    xrtFree(ctx->ownedTypeNodes);
    ctx->ownedTypeNodes = NULL;
    ctx->ownedTypeNodeCount = 0;
    ctx->ownedTypeNodeCapacity = 0;
}

static const char* xl6SemFindTypeParamName(XL6_SemContext* ctx, const char* name)
{
    XL6_TypeParamScope* scope;
    size_t i;

    if ( name == NULL ) {
        return NULL;
    }
    for ( scope = ctx->typeParams; scope != NULL; scope = scope->parent ) {
        if ( scope->params == NULL ) {
            continue;
        }
        for ( i = 0; i < scope->params->childCount; ++i ) {
            XL6_AstNode* param = scope->params->children[i];
            if ( param != NULL && xl6TextEquals(param->text, name) ) {
                return param->text;
            }
        }
    }
    return NULL;
}

static int xl6SemFindTypeParam(XL6_SemContext* ctx, const char* name)
{
    return xl6SemFindTypeParamName(ctx, name) != NULL;
}

static int xl6SemCheckTypeParams(XL6_SemContext* ctx, XL6_AstNode* params)
{
    size_t i;
    size_t j;
    int ok;

    if ( params == NULL ) {
        return 1;
    }

    ok = 1;
    for ( i = 0; i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( param == NULL || param->text == NULL ) {
            xl6SemErrorNode(ctx, param, "泛型参数缺少名称");
            ok = 0;
            continue;
        }
        if ( xl6TextEquals(param->text, "auto") || xl6TypeRegistryFind(&ctx->types, param->text) != NULL ) {
            xl6SemErrorNode(ctx, param, "泛型参数名不能和已有类型名重复");
            ok = 0;
        }
        for ( j = 0; j < i; ++j ) {
            XL6_AstNode* oldParam = params->children[j];
            if ( oldParam != NULL && xl6TextEquals(oldParam->text, param->text) ) {
                xl6SemErrorNode(ctx, param, "重复声明泛型参数");
                ok = 0;
                break;
            }
        }
        if ( !xl6SemCheckGenericConstraintDecl(ctx, param) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemTypeIsAuto(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           typeNode->kind == XL6_AST_TYPE &&
           xl6TextEquals(typeNode->text, "auto");
}

static int xl6SemTypeContainsAliasCycle(XL6_SemContext* ctx, const XL6_AstNode* typeNode);

static int xl6SemCheckInternalClassHandleType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    XL6_AstNode* target;
    const XL6_TypeInfo* targetInfo;

    args = xl6SemTypeNodeGenericArgs(typeNode);
    target = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    targetInfo = target != NULL && target->text != NULL ?
                 xl6TypeRegistryFind(&ctx->types, target->text) : NULL;
    if ( target == NULL || targetInfo == NULL || targetInfo->kind != XL6_TYPE_CLASS ) {
        xl6SemErrorNode(ctx, typeNode, "编译器内部 class 引用类型无效");
        return 0;
    }
    return xl6SemCheckType(ctx, target);
}

static int xl6SemCheckType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;
    size_t i;
    int ok;
    int genericCount;

    if ( typeNode == NULL ) {
        return 1;
    }
    if ( typeNode->kind != XL6_AST_TYPE ) {
        return 1;
    }
    if ( (typeNode->flags & XL6_AST_FLAG_ASYNC) != 0 &&
         !xl6TextEquals(typeNode->text, "generator") ) {
        xl6SemErrorNode(ctx, typeNode, "async type marker is only supported on generator<T>");
        return 0;
    }

    if ( xl6TextEquals(typeNode->text, "auto") ) {
        return 1;
    }
    if ( xl6TextEquals(typeNode->text, "array<var>") || xl6TextEquals(typeNode->text, "dict<var>") ) {
        return 1;
    }
    if ( xl6TextEquals(typeNode->text, "__class_handle") &&
         (typeNode->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 ) {
        return xl6SemCheckInternalClassHandleType(ctx, typeNode);
    }
    if ( xl6TextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        XL6_AstNode* params = typeNode->children[0];
        XL6_AstNode* returns = typeNode->children[1];
        int seenVararg = 0;

        ok = 1;
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                XL6_AstNode* paramType = params->children[i];
                if ( xl6SemTypeIsCVararg(paramType) ) {
                    xl6SemErrorNode(ctx, paramType, "c_vararg 只能用于 extern C 函数声明");
                    ok = 0;
                } else if ( xl6SemTypeIsVararg(paramType) ) {
                    if ( seenVararg ) {
                        xl6SemErrorNode(ctx, paramType, "func 类型最多只能声明一个可变参数");
                        ok = 0;
                    }
                    if ( i + 1 < params->childCount ) {
                        xl6SemErrorNode(ctx, paramType, "func 类型可变参数必须位于最后");
                        ok = 0;
                    }
                    seenVararg = 1;
                } else if ( xl6SemTypeIsAuto(paramType) ) {
                    xl6SemErrorNode(ctx, paramType, "func 类型参数不能使用 auto");
                    ok = 0;
                } else if ( !xl6SemCheckType(ctx, paramType) ) {
                    ok = 0;
                }
            }
        }
        if ( returns != NULL ) {
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( xl6SemTypeIsAuto(returns->children[i]) ) {
                    xl6SemErrorNode(ctx, returns->children[i], "func 类型返回值不能使用 auto");
                    ok = 0;
                } else if ( !xl6SemCheckType(ctx, returns->children[i]) ) {
                    ok = 0;
                }
            }
        }
        return ok;
    }

    if ( xl6SemFindTypeParam(ctx, typeNode->text) ) {
        if ( typeNode->childCount > 0 ) {
            xl6SemErrorNode(ctx, typeNode, "泛型参数不能再接收类型实参");
            return 0;
        }
        return 1;
    }

    genericCount = 0;
    if ( typeNode->childCount > 0 && typeNode->children[0] != NULL && typeNode->children[0]->kind == XL6_AST_LIST ) {
        genericCount = (int)typeNode->children[0]->childCount;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        str message = xrtFormat("未知类型: %s", typeNode->text != NULL ? typeNode->text : "");
        xl6SemErrorNode(ctx, typeNode, message != NULL ? (const char*)message : "未知类型");
        xrtFree(message);
        return 0;
    }

    if ( typeInfo->kind != XL6_TYPE_BUILTIN &&
         xl6SemTypeContainsAliasCycle(ctx, typeNode) ) {
        return 0;
    }

    ok = 1;
    if ( genericCount > 0 ) {
        if ( !typeInfo->allowGeneric ) {
            xl6SemErrorNode(ctx, typeNode, "该类型不接受泛型参数");
            ok = 0;
        } else if ( genericCount != typeInfo->genericArity ) {
            xl6SemCountMismatchError(
                ctx,
                typeNode,
                typeInfo->kind == XL6_TYPE_CLASS ? "泛型 class 类型参数数量不匹配" : "泛型参数数量不匹配",
                (size_t)typeInfo->genericArity,
                (size_t)genericCount);
            ok = 0;
        } else if ( typeInfo->kind == XL6_TYPE_CLASS &&
                    typeInfo->decl != NULL &&
                    !xl6SemCheckGenericConstraints(
                        ctx,
                        typeInfo->decl->childCount > 2 ? typeInfo->decl->children[2] : NULL,
                        typeNode->childCount > 0 ? typeNode->children[0] : NULL,
                        typeNode,
                        1) ) {
            ok = 0;
        }
    }

    if ( genericCount == 1 &&
         (xl6TextEquals(typeNode->text, "array") ||
          xl6TextEquals(typeNode->text, "list") ||
          xl6TextEquals(typeNode->text, "set") ||
          xl6TextEquals(typeNode->text, "dict")) ) {
        const XL6_AstNode* args = typeNode->childCount > 0 ? typeNode->children[0] : NULL;
        const XL6_AstNode* itemType = args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1 ?
                                      args->children[0] : NULL;
        if ( itemType != NULL &&
             !xl6SemIsFutureLikeTypeName(itemType->text) &&
             !xl6SemTypeUsesClassReferenceSemantics(ctx, itemType) &&
             !xl6SemTypeCanCopy(ctx, itemType) ) {
            xl6SemErrorNode(ctx, itemType, "typed container item type must be copyable");
            ok = 0;
        }
    }
    if ( xl6TextEquals(typeNode->text, "generator") ) {
        const XL6_AstNode* typeArgs = (typeNode->childCount > 0 && typeNode->children[0] != NULL &&
                                      typeNode->children[0]->kind == XL6_AST_LIST) ? typeNode->children[0] : NULL;
        const XL6_AstNode* itemType = typeArgs != NULL && typeArgs->childCount == 1 ? typeArgs->children[0] : NULL;
        if ( genericCount != 1 || itemType == NULL ) {
            xl6SemErrorNode(ctx, typeNode, "generator 类型必须写成 generator<T>");
            ok = 0;
        } else if ( !xl6SemChannelElementTypeIsFirstStageSafe(ctx, itemType) ) {
            xl6SemErrorNode(ctx, itemType, "generator<T> 当前只支持 bool、整数、浮点、string、time、point 等具备稳定复制语义的元素类型");
            ok = 0;
        }
    }
    {
        const XL6_AstNode* typeArgs = xl6SemTypeNodeGenericArgs(typeNode);
        if ( typeArgs != NULL ) {
            for ( i = 0; i < typeArgs->childCount; ++i ) {
                if ( !xl6SemCheckType(ctx, typeArgs->children[i]) ) {
                    ok = 0;
                }
            }
        }
    }
    return ok;
}

static int xl6SemCheckExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckStmt(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckChildrenExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckChildrenExprFrom(XL6_SemContext* ctx, XL6_AstNode* node, size_t firstChild);
static int xl6SemCheckAnonFunc(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckAwaitExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckYieldStmt(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemTypeEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b);
static int xl6SemExprCanMatchFunctionType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode);
static int xl6SemExprCanMatchFunctionTypeEx(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode, int reportError);
static int xl6SemFuncHasVararg(const XL6_AstNode* params);
static int xl6SemIsContainerBaseTypeName(const char* name);
static int xl6SemFixedParamCount(const XL6_AstNode* params);
static int xl6SemParamRequiredCount(const XL6_AstNode* params);
static XL6_AstNode* xl6SemFindFunctionValueFunc(XL6_SemContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode, int reportError);
static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneTypeNode(const XL6_AstNode* node);
static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneIndexedItemType(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneRecordFieldType(XL6_SemContext* ctx, const XL6_AstNode* receiverType, const char* fieldName);
static XL6_AstNode* xl6SemCloneMemberFieldType(XL6_SemContext* ctx, XL6_AstNode* member);
static const XL6_AstNode* xl6SemFindClassCallableMemberNode(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName);
static const XL6_AstNode* xl6SemFindProtocolMethodForExpr(XL6_SemContext* ctx, XL6_AstNode* expr, const char* protocolName);
static const XL6_AstNode* xl6SemFindProtocolMethodForExprArgs(XL6_SemContext* ctx, XL6_AstNode* expr, const char* protocolName, XL6_AstNode** args, size_t argCount, int report);
static const XL6_AstNode* xl6SemFindProtocolMethodForExprCall(XL6_SemContext* ctx, XL6_AstNode* expr, const char* protocolName, XL6_AstNode* call, int report);
static size_t xl6SemIndexArgCount(const XL6_AstNode* node);
static XL6_AstNode* xl6SemIndexArgAt(XL6_AstNode* node, size_t index);
static const XL6_AstNode* xl6SemProtocolParamType(const XL6_AstNode* method, size_t index);
static int xl6SemCheckProtocolArg(XL6_SemContext* ctx, XL6_AstNode* atNode, const XL6_AstNode* method, size_t paramIndex, XL6_AstNode* argExpr, const char* message);
static int xl6SemCheckProtocolCallArgs(XL6_SemContext* ctx, XL6_AstNode* call, const XL6_AstNode* method);
static XL6_AstNode* xl6SemProtocolTempCallNew(const XL6_AstNode* atNode);
static int xl6SemProtocolTempCallAddArg(XL6_AstNode* call, XL6_AstNode* expr);
static void xl6SemProtocolTempCallFree(XL6_AstNode* call);
static const char* xl6SemUnaryProtocolName(const char* op);
static const char* xl6SemBinaryProtocolName(const char* op);
static int xl6SemClassHasCallableMember(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName);
static const XL6_AstNode* xl6SemFindClassFieldNode(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const XL6_AstNode** ownerClass);
static int xl6SemCanAccessClassMember(XL6_SemContext* ctx, const XL6_AstNode* ownerClass, const XL6_AstNode* member);
static XL6_AstNode* xl6SemClassDeclFromTypeMemberReceiver(XL6_SemContext* ctx, XL6_AstNode* member);
static const XL6_AstNode* xl6SemFindStaticClassFieldNode(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const XL6_AstNode** ownerClass);
static XL6_AstNode* xl6SemCloneStaticClassFieldType(XL6_SemContext* ctx, XL6_AstNode* member);
static XL6_AstNode* xl6SemResolveStaticClassFuncCall(XL6_SemContext* ctx, XL6_AstNode* call, int report);
static XL6_AstNode* xl6SemCloneProtocolReturnType(XL6_SemContext* ctx, const XL6_AstNode* receiverType, const char* protocolName);
static XL6_AstNode* xl6SemClonePropertyGetterType(XL6_SemContext* ctx, XL6_AstNode* member, XL6_AstNode** args, size_t argCount, int report);
static XL6_AstNode* xl6SemClonePropertyIndexGetterType(XL6_SemContext* ctx, XL6_AstNode* indexExpr, int report);
static XL6_AstNode* xl6SemClonePropertySetterValueType(XL6_SemContext* ctx, XL6_AstNode* left, XL6_AstNode* valueExpr, int report);
static int xl6SemClassHasPropertyName(XL6_SemContext* ctx, XL6_AstNode* classNode, const char* name, int kind);
static int xl6SemLeftNamesClassProperty(XL6_SemContext* ctx, XL6_AstNode* left);
static int xl6SemCheckPropertyAssignTarget(XL6_SemContext* ctx, XL6_AstNode* left, XL6_AstNode* right, const char* op);
static const char* xl6SemMemberName(const XL6_AstNode* node);
static int xl6SemCheckProtocolMember(XL6_SemContext* ctx, XL6_AstNode* member);
static int xl6SemCheckPropertyMember(XL6_SemContext* ctx, XL6_AstNode* member);
static int xl6SemTypeNameIsClassValue(XL6_SemContext* ctx, const char* typeName);
static XL6_AstNode* xl6SemOwnedSimpleType(XL6_SemContext* ctx, const char* typeName, const XL6_AstNode* at);

static int xl6SemParamIsArrayVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6TextEquals(param->children[0]->text, "array<var>");
}

static int xl6SemParamIsDictVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6TextEquals(param->children[0]->text, "dict<var>");
}

static int xl6SemParamIsCVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6SemTypeIsCVararg(param->children[0]);
}

static int xl6SemParamIsVararg(const XL6_AstNode* param)
{
    return xl6SemParamIsArrayVararg(param) || xl6SemParamIsDictVararg(param) ||
           xl6SemParamIsCVararg(param);
}

static unsigned int xl6SemParamMode(const XL6_AstNode* param)
{
    return param != NULL ?
           (param->flags & (XL6_AST_FLAG_BYVAL | XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) : 0;
}

static unsigned int xl6SemCurrentParamModeByName(XL6_SemContext* ctx, const char* name)
{
    XL6_AstNode* params;
    size_t i;

    if ( ctx == NULL || ctx->currentFunction == NULL || name == NULL ) {
        return 0;
    }
    params = ctx->currentFunction->childCount > 1 ? ctx->currentFunction->children[1] : NULL;
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6TextEquals(param->text, name) ) {
            return xl6SemParamMode(param);
        }
    }
    return 0;
}

static int xl6SemExprIsWritableLValue(XL6_SemContext* ctx, const XL6_AstNode* expr)
{
    XL6_Symbol* sym;

    if ( expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_MEMBER || expr->kind == XL6_AST_INDEX ) {
        return 1;
    }
    if ( expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    sym = xl6ScopeFind(ctx != NULL ? ctx->scope : NULL, expr->text);
    return sym != NULL && sym->kind != XL6_SYM_FUNC && !xl6SemSymbolIsConst(sym) &&
           !xl6SemIsImplicitSelfTarget(ctx, expr);
}

static int xl6SemCallArgModeMatchesParam(
    XL6_SemContext* ctx,
    const XL6_AstNode* arg,
    const XL6_AstNode* param,
    const XL6_AstNode* argExpr)
{
    unsigned int paramMode;
    unsigned int argMode;

    paramMode = xl6SemParamMode(param);
    argMode = arg != NULL ?
              (arg->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) : 0;
    if ( paramMode == XL6_AST_FLAG_BYREF ) {
        return argMode == XL6_AST_FLAG_BYREF && xl6SemExprIsWritableLValue(ctx, argExpr);
    }
    if ( paramMode == XL6_AST_FLAG_OUT ) {
        return argMode == XL6_AST_FLAG_OUT && xl6SemExprIsWritableLValue(ctx, argExpr);
    }
    return argMode == 0;
}

static int xl6SemParamVarargKind(const XL6_AstNode* param)
{
    if ( xl6SemParamIsArrayVararg(param) ) {
        return 1;
    }
    if ( xl6SemParamIsDictVararg(param) ) {
        return 2;
    }
    if ( xl6SemParamIsCVararg(param) ) {
        return 3;
    }
    return 0;
}

static int xl6SemParamListHasCVararg(const XL6_AstNode* params)
{
    size_t i;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        if ( xl6SemParamIsCVararg(params->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static XL6_AstNode* xl6SemFindTypeDecl(XL6_SemContext* ctx, const char* name)
{
    const XL6_TypeInfo* typeInfo;

    if ( ctx == NULL || name == NULL ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, name);
    return typeInfo != NULL ? typeInfo->decl : NULL;
}

static XL6_AstNode* xl6SemNewTypeFromName(const XL6_AstNode* nameNode)
{
    XL6_AstNode* typeNode;

    if ( nameNode == NULL || nameNode->text == NULL ) {
        return NULL;
    }
    typeNode = xl6AstNewText(XL6_AST_TYPE, nameNode->text, strlen(nameNode->text), NULL);
    if ( typeNode != NULL ) {
        typeNode->line = nameNode->line;
        typeNode->column = nameNode->column;
    }
    return typeNode;
}

static int xl6SemNameIsRegisteredType(XL6_SemContext* ctx, const char* name)
{
    return ctx != NULL && xl6TypeRegistryFind(&ctx->types, name) != NULL;
}

static const XL6_AstNode* xl6SemAliasTargetType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    if ( ctx == NULL || typeNode == NULL || typeNode->kind != XL6_AST_TYPE ||
         typeNode->text == NULL || typeNode->childCount > 0 ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_ALIAS ||
         typeInfo->decl == NULL || typeInfo->decl->childCount == 0 ) {
        return NULL;
    }
    return typeInfo->decl->children[0];
}

static int xl6SemSimpleTypeAliasHasCycle(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* current;
    const char* seen[32];
    size_t seenCount;
    int guard;

    if ( ctx == NULL || typeNode == NULL || typeNode->kind != XL6_AST_TYPE ||
         typeNode->text == NULL || typeNode->childCount > 0 ) {
        return 0;
    }

    current = typeNode;
    seenCount = 0;
    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* target;
        size_t i;

        if ( current == NULL || current->kind != XL6_AST_TYPE ||
             current->text == NULL || current->childCount > 0 ) {
            return 0;
        }
        for ( i = 0; i < seenCount; ++i ) {
            if ( xl6TextEquals(seen[i], current->text) ) {
                return 1;
            }
        }
        seen[seenCount++] = current->text;

        target = xl6SemAliasTargetType(ctx, current);
        if ( target == NULL ) {
            return 0;
        }
        current = target;
    }
    return 1;
}

static int xl6SemTypeContainsAliasCycle(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( xl6SemSimpleTypeAliasHasCycle(ctx, typeNode) ) {
        return 1;
    }
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( xl6SemTypeContainsAliasCycle(ctx, typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static const XL6_AstNode* xl6SemResolveAliasType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* target = xl6SemAliasTargetType(ctx, typeNode);
        if ( target == NULL ) {
            return typeNode;
        }
        typeNode = target;
    }
    return typeNode;
}

static const char* xl6SemResolveAliasTypeName(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    return typeNode != NULL ? typeNode->text : NULL;
}

static int xl6SemClassDeclHasDestroy(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || classNode->childCount < 2 ) {
        return 0;
    }
    body = classNode->children[1];
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CREATE &&
             xl6TextEquals(xl6SemMemberName(member), "destroy") ) {
            return 1;
        }
        if ( member != NULL &&
             member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL &&
             xl6TextEquals(xl6SemMemberName(member), "destroy") ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemClassDeclHasNativeHandle(XL6_SemContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || classNode->childCount < 2 ) {
        return 0;
    }
    body = classNode->children[1];
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* typeNode;

        if ( member == NULL ||
             member->kind != XL6_AST_FIELD ||
             !xl6SemAstHasFlag(member, XL6_AST_FLAG_NATIVE) ||
             xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) ||
             !xl6TextEquals(member->text, "handle") ||
             member->childCount == 0 ) {
            continue;
        }
        typeNode = member->children[0];
        typeNode = xl6SemResolveAliasType(ctx, typeNode);
        return typeNode != NULL && typeNode->text != NULL && xl6TextEquals(typeNode->text, "point");
    }
    return 0;
}

static int xl6SemBodyHasDerive(const XL6_AstNode* body, const char* ability)
{
    size_t i;

    if ( body == NULL || ability == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_DERIVE &&
             xl6TextEquals(member->text, ability) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemClassDeclHasDerive(const XL6_AstNode* classNode, const char* ability)
{
    const XL6_AstNode* body;

    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    return xl6SemBodyHasDerive(body, ability);
}

static int xl6SemClassDeclHasFunc(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || name == NULL || classNode->childCount < 2 ) {
        return 0;
    }
    body = classNode->children[1];
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_FUNC &&
             xl6TextEquals(xl6SemMemberName(member), name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemTypeCanCaptureDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;

    if ( typeNode == NULL || depth > 32 ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS ) {
        return 1;
    }
    /* class 捕获只保留受管理句柄，不复制对象主体或 native 资源。 */
    return 1;
}

static int xl6SemTypeCanCapture(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanCaptureDepth(ctx, typeNode, 0);
}

static int xl6SemTypeCanCopyDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* classTarget;
    const XL6_AstNode* decl;

    if ( typeNode == NULL || depth > 32 ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }
    classTarget = xl6SemClassHandleTargetType(ctx, typeNode);
    if ( classTarget != NULL ) {
        typeNode = classTarget;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }
    if ( typeInfo->kind == XL6_TYPE_BUILTIN &&
         (xl6TextEquals(typeNode->text, "array") ||
          xl6TextEquals(typeNode->text, "list") ||
          xl6TextEquals(typeNode->text, "set") ||
          xl6TextEquals(typeNode->text, "dict")) ) {
        const XL6_AstNode* args = typeNode->childCount > 0 ? typeNode->children[0] : NULL;
        const XL6_AstNode* itemType = args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1 ?
                                      args->children[0] : NULL;
        /* future/task 在容器中使用共享句柄语义，容器复制会保留句柄引用。 */
        return itemType == NULL ||
               xl6SemIsFutureLikeTypeName(itemType->text) ||
               xl6SemTypeUsesClassReferenceSemantics(ctx, itemType) ||
               xl6SemTypeCanCopyDepth(ctx, itemType, depth + 1);
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS ) {
        return 1;
    }

    decl = typeInfo->decl;
    if ( decl == NULL ) {
        return 1;
    }
    if ( xl6SemClassDeclHasFunc(decl, "__copy__") ||
         xl6SemClassDeclHasFunc(decl, "__clone__") ||
         xl6SemClassDeclHasDerive(decl, "copy") ||
         xl6SemClassDeclHasDerive(decl, "clone") ) {
        return 1;
    }
    /* 复制对象必须显式声明协议；字段看起来可复制不代表 class 自动可复制。 */
    return 0;
}

static int xl6SemTypeCanCopy(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanCopyDepth(ctx, typeNode, 0);
}

static int xl6SemTypeCanBeBorrowedParam(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_TypeInfo* typeInfo;

    if ( xl6SemTypeUsesClassReferenceSemantics(ctx, typeNode) ||
         xl6SemTypeCanCopy(ctx, typeNode) ) {
        return 1;
    }
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 1;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, concreteType->text);
    return typeInfo != NULL &&
           typeInfo->kind == XL6_TYPE_CLASS &&
           xl6SemClassDeclHasNativeHandle(ctx, typeInfo->decl);
}

static int xl6SemTypeRequiresFreshValue(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_TypeInfo* typeInfo;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, concreteType->text);
    return typeInfo != NULL &&
           typeInfo->kind == XL6_TYPE_CLASS &&
           !xl6SemTypeCanCopy(ctx, concreteType);
}

static int xl6SemTypeCanBoxToVarDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;

    if ( typeNode == NULL || depth > 32 ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }
    if ( xl6SemTypeUsesClassReferenceSemantics(ctx, typeNode) ) {
        return 1;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }

    /* class 装箱保留受管理句柄，不会复制 native 资源。 */
    if ( typeInfo->kind == XL6_TYPE_CLASS ) {
        return 1;
    }
    return 1;
}

static int xl6SemTypeCanBoxToVar(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanBoxToVarDepth(ctx, typeNode, 0);
}

static int xl6SemExprCanBoxToVar(XL6_SemContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int ok;

    if ( expr == NULL ) {
        return 1;
    }

    typeNode = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
    if ( typeNode == NULL ) {
        return 1;
    }

    ok = xl6SemTypeCanBoxToVar(ctx, typeNode);
    xl6AstFree(typeNode);
    return ok;
}

static int xl6SemIsIntegerTypeName(const char* name)
{
    return xl6TextEquals(name, "int") || xl6TextEquals(name, "int8") ||
           xl6TextEquals(name, "uint8") || xl6TextEquals(name, "int16") ||
           xl6TextEquals(name, "uint16") || xl6TextEquals(name, "int32") ||
           xl6TextEquals(name, "uint32") || xl6TextEquals(name, "int64") ||
           xl6TextEquals(name, "uint64") ||
           xl6TextEquals(name, "c_char") || xl6TextEquals(name, "c_schar") ||
           xl6TextEquals(name, "c_uchar") || xl6TextEquals(name, "c_short") ||
           xl6TextEquals(name, "c_ushort") || xl6TextEquals(name, "c_int") ||
           xl6TextEquals(name, "c_uint") || xl6TextEquals(name, "c_long") ||
           xl6TextEquals(name, "c_ulong") || xl6TextEquals(name, "c_longlong") ||
           xl6TextEquals(name, "c_ulonglong") || xl6TextEquals(name, "c_size") ||
           xl6TextEquals(name, "c_ssize") || xl6TextEquals(name, "c_ptrdiff") ||
           xl6TextEquals(name, "c_intptr") || xl6TextEquals(name, "c_uintptr") ||
           xl6TextEquals(name, "c_wchar");
}

static int xl6SemIsFloatTypeName(const char* name)
{
    return xl6TextEquals(name, "float") || xl6TextEquals(name, "float32") ||
           xl6TextEquals(name, "float64") ||
           xl6TextEquals(name, "c_float") || xl6TextEquals(name, "c_double");
}

static int xl6SemTypeSupportsTaskSpawnXValueBridge(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( typeName == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(typeName, "var") ) {
        return 1;
    }
    if ( xl6SemTypeNodeGenericArgs(concreteType) != NULL &&
         (xl6TextEquals(typeName, "Stack") ||
          xl6TextEquals(typeName, "AvlTree") ||
          xl6TextEquals(typeName, "SpscQueue") ||
          xl6TextEquals(typeName, "MpscQueue") ||
          xl6TextEquals(typeName, "MpmcQueue") ||
          xl6TextEquals(typeName, "WaitQueue")) ) {
        return 1;
    }
    if ( xl6SemTypeNodeGenericArgs(concreteType) != NULL &&
         (xl6TextEquals(typeName, "array") ||
          xl6TextEquals(typeName, "list") ||
          xl6TextEquals(typeName, "set") ||
          xl6TextEquals(typeName, "dict")) ) {
        return 0;
    }
    if ( xl6TextEquals(typeName, "bool") ||
         xl6TextEquals(typeName, "int") ||
         xl6TextEquals(typeName, "int64") ||
         xl6TextEquals(typeName, "float") ||
         xl6TextEquals(typeName, "float64") ||
         xl6TextEquals(typeName, "string") ||
         xl6TextEquals(typeName, "time") ||
         xl6TextEquals(typeName, "point") ||
         xl6TextEquals(typeName, "array") ||
         xl6TextEquals(typeName, "list") ||
         xl6TextEquals(typeName, "set") ||
         xl6TextEquals(typeName, "dict") ) {
        return 1;
    }
    return 0;
}

static int xl6SemExprSupportsTaskSpawnXValueBridge(XL6_SemContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int ok;

    if ( expr == NULL ) {
        return 0;
    }
    typeNode = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
    ok = xl6SemTypeSupportsTaskSpawnXValueBridge(ctx, typeNode);
    xl6AstFree(typeNode);
    return ok;
}

static int xl6SemIsBoolTypeName(const char* name)
{
    return xl6TextEquals(name, "bool") || xl6TextEquals(name, "c_bool");
}

typedef struct XL6_SemIntTypeInfo {
    int bits;
    int isSigned;
} XL6_SemIntTypeInfo;

static int xl6SemIntTypeInfo(const char* name, XL6_SemIntTypeInfo* outInfo)
{
    XL6_SemIntTypeInfo info;

    if ( name == NULL ) {
        return 0;
    }
    info.bits = 0;
    info.isSigned = 1;
    if ( xl6TextEquals(name, "int") || xl6TextEquals(name, "int64") ) {
        info.bits = 64;
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "uint64") ) {
        info.bits = 64;
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "int8") ) {
        info.bits = 8;
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "uint8") ) {
        info.bits = 8;
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "int16") ) {
        info.bits = 16;
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "uint16") ) {
        info.bits = 16;
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "int32") ) {
        info.bits = 32;
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "uint32") ) {
        info.bits = 32;
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_char") ) {
        info.bits = (int)(sizeof(char) * CHAR_BIT);
        info.isSigned = (CHAR_MIN < 0);
    } else if ( xl6TextEquals(name, "c_schar") ) {
        info.bits = (int)(sizeof(signed char) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_uchar") ) {
        info.bits = (int)(sizeof(unsigned char) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_short") ) {
        info.bits = (int)(sizeof(short) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_ushort") ) {
        info.bits = (int)(sizeof(unsigned short) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_int") ) {
        info.bits = (int)(sizeof(int) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_uint") ) {
        info.bits = (int)(sizeof(unsigned int) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_long") ) {
        info.bits = (int)(sizeof(long) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_ulong") ) {
        info.bits = (int)(sizeof(unsigned long) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_longlong") ) {
        info.bits = (int)(sizeof(long long) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_ulonglong") ) {
        info.bits = (int)(sizeof(unsigned long long) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_size") || xl6TextEquals(name, "c_uintptr") ) {
        info.bits = (int)(sizeof(size_t) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6TextEquals(name, "c_ssize") || xl6TextEquals(name, "c_ptrdiff") || xl6TextEquals(name, "c_intptr") ) {
        info.bits = (int)(sizeof(size_t) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6TextEquals(name, "c_wchar") ) {
        info.bits = (int)(sizeof(wchar_t) * CHAR_BIT);
        info.isSigned = ((wchar_t)-1) < (wchar_t)0;
    } else {
        return 0;
    }
    if ( outInfo != NULL ) {
        *outInfo = info;
    }
    return info.bits > 0;
}

static int xl6SemFloatTypeBits(const char* name)
{
    if ( xl6TextEquals(name, "float32") || xl6TextEquals(name, "c_float") ) {
        return 32;
    }
    if ( xl6TextEquals(name, "float") || xl6TextEquals(name, "float64") || xl6TextEquals(name, "c_double") ) {
        return 64;
    }
    return 0;
}

static int xl6SemLiteralDigitValue(char ch)
{
    if ( ch >= '0' && ch <= '9' ) {
        return ch - '0';
    }
    if ( ch >= 'a' && ch <= 'f' ) {
        return ch - 'a' + 10;
    }
    if ( ch >= 'A' && ch <= 'F' ) {
        return ch - 'A' + 10;
    }
    return -1;
}

static int xl6SemParseUIntLiteralText(const char* text, uint64* outValue)
{
    const char* p;
    int base;
    uint64 value;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }
    p = text;
    base = 10;
    if ( p[0] == '0' && p[1] == 'x' ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'o' ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'b' ) {
        base = 2;
        p += 2;
    }

    value = 0;
    while ( *p != '\0' ) {
        int digit;

        if ( *p == '_' ) {
            ++p;
            continue;
        }
        digit = xl6SemLiteralDigitValue(*p);
        if ( digit < 0 || digit >= base || value > (~(uint64)0 - (uint64)digit) / (uint64)base ) {
            return 0;
        }
        value = value * (uint64)base + (uint64)digit;
        ++p;
    }
    if ( outValue != NULL ) {
        *outValue = value;
    }
    return 1;
}

static int xl6SemExprIntegerLiteralValue(const XL6_AstNode* expr, uint64* outMagnitude, int* outNegative)
{
    const XL6_AstNode* valueNode;
    int negative;

    if ( expr == NULL ) {
        return 0;
    }
    valueNode = expr;
    negative = 0;
    if ( expr->kind == XL6_AST_UNARY && expr->childCount == 1 &&
         (xl6TextEquals(expr->text, "-") || xl6TextEquals(expr->text, "+")) ) {
        negative = xl6TextEquals(expr->text, "-");
        valueNode = expr->children[0];
    }
    if ( valueNode == NULL || valueNode->kind != XL6_AST_INT ) {
        return 0;
    }
    if ( !xl6SemParseUIntLiteralText(valueNode->text, outMagnitude) ) {
        return 0;
    }
    if ( outNegative != NULL ) {
        *outNegative = negative;
    }
    return 1;
}

static uint64 xl6SemUnsignedMaxForBits(int bits)
{
    if ( bits >= 64 ) {
        return ~(uint64)0;
    }
    return (((uint64)1) << bits) - 1;
}

static int xl6SemExprIntegerLiteralFitsType(const XL6_AstNode* expr, const char* targetType)
{
    XL6_SemIntTypeInfo info;
    uint64 magnitude;
    int negative;
    int floatBits;

    if ( !xl6SemExprIntegerLiteralValue(expr, &magnitude, &negative) ) {
        return 0;
    }
    floatBits = xl6SemFloatTypeBits(targetType);
    if ( floatBits > 0 ) {
        /* 整数字面量只在能够被目标浮点格式精确表示时参与安全拓宽。 */
        uint64 exactMax = floatBits <= 32 ? (((uint64)1) << 24) : (((uint64)1) << 53);
        return magnitude <= exactMax;
    }
    if ( !xl6SemIntTypeInfo(targetType, &info) ) {
        return 0;
    }
    if ( info.isSigned ) {
        uint64 positiveMax = info.bits >= 64 ? 0x7fffffffffffffffULL : ((((uint64)1) << (info.bits - 1)) - 1);
        uint64 negativeMax = info.bits >= 64 ? 0x8000000000000000ULL : (((uint64)1) << (info.bits - 1));
        return negative ? magnitude <= negativeMax : magnitude <= positiveMax;
    }
    return !negative && magnitude <= xl6SemUnsignedMaxForBits(info.bits);
}

static int xl6SemCanSafeWidenTypeName(const char* source, const char* target)
{
    XL6_SemIntTypeInfo srcInt;
    XL6_SemIntTypeInfo dstInt;
    int srcFloatBits;
    int dstFloatBits;

    if ( source == NULL || target == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(source, target) ) {
        return 1;
    }
    if ( xl6SemIsBoolTypeName(source) && xl6SemIsBoolTypeName(target) ) {
        return 1;
    }
    if ( xl6SemIsBoolTypeName(source) &&
         (xl6SemIntTypeInfo(target, NULL) || xl6SemFloatTypeBits(target) > 0) ) {
        return 1;
    }
    if ( xl6SemIntTypeInfo(source, &srcInt) && xl6SemIntTypeInfo(target, &dstInt) ) {
        if ( srcInt.isSigned && dstInt.isSigned ) {
            return dstInt.bits >= srcInt.bits;
        }
        if ( !srcInt.isSigned && !dstInt.isSigned ) {
            return dstInt.bits >= srcInt.bits;
        }
        if ( !srcInt.isSigned && dstInt.isSigned ) {
            return dstInt.bits > srcInt.bits;
        }
        return 0;
    }
    srcFloatBits = xl6SemFloatTypeBits(source);
    dstFloatBits = xl6SemFloatTypeBits(target);
    if ( srcFloatBits > 0 && dstFloatBits > 0 ) {
        return dstFloatBits >= srcFloatBits;
    }
    return 0;
}

static int xl6SemIsNumericTypeName(const char* name)
{
    return xl6SemIsIntegerTypeName(name) || xl6SemIsFloatTypeName(name);
}

static int xl6SemIntLiteralIsZeroText(const char* text)
{
    size_t i;
    int hasDigit;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }
    i = 0;
    if ( text[0] == '&' && (text[1] == 'h' || text[1] == 'H' || text[1] == 'o' || text[1] == 'O' || text[1] == 'b' || text[1] == 'B') ) {
        i = 2;
    } else if ( text[0] == '0' && (text[1] == 'x' || text[1] == 'X' || text[1] == 'o' || text[1] == 'O' || text[1] == 'b' || text[1] == 'B') ) {
        i = 2;
    }

    hasDigit = 0;
    for ( ; text[i] != '\0'; ++i ) {
        if ( text[i] == '_' ) {
            continue;
        }
        if ( text[i] == '0' ) {
            hasDigit = 1;
            continue;
        }
        return 0;
    }
    return hasDigit;
}

static int xl6SemFloatLiteralIsZeroText(const char* text)
{
    size_t i;
    int hasDigit;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }
    hasDigit = 0;
    for ( i = 0; text[i] != '\0'; ++i ) {
        char ch = text[i];
        if ( ch == 'e' || ch == 'E' ) {
            break;
        }
        if ( ch == '_' || ch == '.' || ch == '+' || ch == '-' ) {
            continue;
        }
        if ( ch == '0' ) {
            hasDigit = 1;
            continue;
        }
        if ( ch >= '1' && ch <= '9' ) {
            return 0;
        }
        return 0;
    }
    return hasDigit;
}

static int xl6SemExprIsZeroLiteral(const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_INT ) {
        return xl6SemIntLiteralIsZeroText(expr->text);
    }
    if ( expr->kind == XL6_AST_FLOAT ) {
        return xl6SemFloatLiteralIsZeroText(expr->text);
    }
    return 0;
}
static int xl6SemIsArrayTypeName(const char* name)
{
    return xl6TextEquals(name, "array") || xl6TextEquals(name, "array<var>");
}

static int xl6SemIsListTypeName(const char* name)
{
    return xl6TextEquals(name, "list") || xl6TextEquals(name, "list<var>");
}

static int xl6SemIsDictTypeName(const char* name)
{
    return xl6TextEquals(name, "dict") || xl6TextEquals(name, "dict<var>");
}

static int xl6SemIsSetTypeName(const char* name)
{
    return xl6TextEquals(name, "set") || xl6TextEquals(name, "set<var>");
}

static int xl6SemIsBuiltinConvertTypeName(const char* name)
{
    return xl6SemIsIntegerTypeName(name) || xl6SemIsFloatTypeName(name) ||
           xl6SemIsBoolTypeName(name) || xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "time") || xl6TextEquals(name, "point") ||
           xl6TextEquals(name, "type") || xl6TextEquals(name, "var");
}

static int xl6SemClassDeclHasValueDerive(const XL6_AstNode* classNode)
{
    return xl6SemClassDeclHasDerive(classNode, "value") ||
           xl6SemClassDeclHasDerive(classNode, "json") ||
           xl6SemClassDeclHasDerive(classNode, "xson");
}

static int xl6SemClassDeclHasFormatDerive(const XL6_AstNode* classNode, const char* format)
{
    if ( !xl6SemClassDeclHasValueDerive(classNode) ) {
        return 0;
    }
    if ( xl6TextEquals(format, "json") ) {
        return xl6SemClassDeclHasDerive(classNode, "value") ||
               xl6SemClassDeclHasDerive(classNode, "json");
    }
    if ( xl6TextEquals(format, "xson") ) {
        return xl6SemClassDeclHasDerive(classNode, "value") ||
               xl6SemClassDeclHasDerive(classNode, "xson");
    }
    return 1;
}

static int xl6SemTypeNodeGenericArgCount(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;

    if ( typeNode == NULL || typeNode->childCount == 0 ) {
        return 0;
    }
    args = typeNode->children[0];
    return args != NULL && args->kind == XL6_AST_LIST ? (int)args->childCount : 0;
}

static const XL6_AstNode* xl6SemTypeNodeFirstGenericArg(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;

    if ( typeNode == NULL || typeNode->childCount == 0 ) {
        return NULL;
    }
    args = typeNode->children[0];
    return args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1 ? args->children[0] : NULL;
}

static int xl6SemTypeCanDeriveValueDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* itemType;
    const char* name;

    if ( typeNode == NULL || depth > 32 ) {
        return 1;
    }

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    typeNode = xl6SemClassValueType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }

    name = typeNode->text;
    if ( xl6SemIsBoolTypeName(name) ||
         xl6SemIsIntegerTypeName(name) ||
         xl6SemIsFloatTypeName(name) ||
         xl6TextEquals(name, "string") ||
         xl6TextEquals(name, "time") ||
         xl6TextEquals(name, "point") ||
         xl6TextEquals(name, "null") ||
         xl6TextEquals(name, "var") ) {
        return xl6SemTypeNodeGenericArgCount(typeNode) == 0;
    }

    if ( xl6TextEquals(name, "array") ||
         xl6TextEquals(name, "list") ||
         xl6TextEquals(name, "set") ||
         xl6TextEquals(name, "dict") ) {
        itemType = xl6SemTypeNodeFirstGenericArg(typeNode);
        return itemType == NULL ||
               xl6SemTypeCanDeriveValueDepth(ctx, itemType, depth + 1);
    }
    if ( xl6TextEquals(name, "object") ) {
        return 1;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, name);
    if ( typeInfo == NULL ) {
        return 0;
    }
    if ( typeInfo->kind == XL6_TYPE_CLASS ) {
        if ( xl6TextEquals(name, "object") ) {
            return 1;
        }
        return typeInfo->decl != NULL &&
               !xl6SemClassHasTypeParams(typeInfo->decl) &&
               xl6SemClassDeclHasValueDerive(typeInfo->decl);
    }
    return 0;
}

static int xl6SemTypeCanDeriveValue(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanDeriveValueDepth(ctx, typeNode, 0);
}

static const XL6_AstNode* xl6SemGenericConstraintList(const XL6_AstNode* param)
{
    size_t i;

    if ( param == NULL ) {
        return NULL;
    }
    for ( i = 0; i < param->childCount; ++i ) {
        const XL6_AstNode* child = param->children[i];
        if ( child != NULL && child->kind == XL6_AST_GENERIC_CONSTRAINT ) {
            return child;
        }
    }
    return NULL;
}

static int xl6SemIsGenericAbilityConstraint(const char* name)
{
    return xl6TextEquals(name, "comparable") ||
           xl6TextEquals(name, "json") ||
           xl6TextEquals(name, "xson") ||
           xl6TextEquals(name, "cloneable");
}

static int xl6SemDeclHasCallableName(const XL6_AstNode* decl, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( decl == NULL || name == NULL ) {
        return 0;
    }
    body = decl->childCount > 1 ? decl->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( (member->kind == XL6_AST_FUNC ||
              member->kind == XL6_AST_ASYNC_FUNC ||
              member->kind == XL6_AST_EXPORT_FUNC ||
              member->kind == XL6_AST_CREATE ||
              member->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
              member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL) &&
             xl6TextEquals(xl6SemMemberName(member), name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemTypeHasComparableProtocol(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;
    const char* name;

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    name = typeNode != NULL ? typeNode->text : NULL;
    if ( name == NULL ) {
        return 0;
    }
    if ( xl6SemIsBoolTypeName(name) ||
         xl6SemIsIntegerTypeName(name) ||
         xl6SemIsFloatTypeName(name) ||
         xl6TextEquals(name, "string") ||
         xl6TextEquals(name, "time") ||
         xl6TextEquals(name, "type") ) {
        return typeNode->childCount == 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, name);
    if ( typeInfo == NULL || typeInfo->decl == NULL ||
         typeInfo->kind != XL6_TYPE_CLASS ) {
        return 0;
    }
    return xl6SemDeclHasCallableName(typeInfo->decl, "__eq__") ||
           xl6SemDeclHasCallableName(typeInfo->decl, "__lt__") ||
           xl6SemDeclHasCallableName(typeInfo->decl, "__le__") ||
           xl6SemDeclHasCallableName(typeInfo->decl, "__gt__") ||
           xl6SemDeclHasCallableName(typeInfo->decl, "__ge__");
}

static int xl6SemTypeHasPersistenceFormat(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* format)
{
    const XL6_TypeInfo* typeInfo;
    const char* name;

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    typeNode = xl6SemClassValueType(ctx, typeNode);
    name = typeNode != NULL ? typeNode->text : NULL;
    if ( name == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, name);
    if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS ) {
        return typeInfo->decl != NULL && xl6SemClassDeclHasFormatDerive(typeInfo->decl, format);
    }
    return xl6SemTypeCanDeriveValue(ctx, typeNode);
}

static int xl6SemTypeIsOrExtendsClass(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* baseName)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* decl;
    int guard;

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || baseName == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(typeNode->text, baseName) ) {
        return 1;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    decl = typeInfo != NULL ? typeInfo->decl : NULL;
    for ( guard = 0; decl != NULL && guard < 64; ++guard ) {
        const XL6_AstNode* base = decl->childCount > 0 ? decl->children[0] : NULL;
        if ( base == NULL || base->kind != XL6_AST_NAME || base->text == NULL ) {
            return 0;
        }
        if ( xl6TextEquals(base->text, baseName) ) {
            return 1;
        }
        decl = xl6SemFindTypeDecl(ctx, base->text);
    }
    return 0;
}

static int xl6SemTypeSatisfiesGenericConstraint(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeArg,
    const XL6_AstNode* constraint)
{
    const XL6_AstNode* actualType;
    const XL6_AstNode* classTarget;
    const char* name;

    name = constraint != NULL ? constraint->text : NULL;
    if ( name == NULL ) {
        return 0;
    }
    actualType = xl6SemResolveAliasType(ctx, typeArg);
    classTarget = xl6SemClassHandleTargetType(ctx, actualType);
    if ( classTarget != NULL ) {
        actualType = classTarget;
    }
    if ( xl6TextEquals(name, "comparable") ) {
        return xl6SemTypeHasComparableProtocol(ctx, actualType);
    }
    if ( xl6TextEquals(name, "json") || xl6TextEquals(name, "xson") ) {
        return xl6SemTypeHasPersistenceFormat(ctx, actualType, name);
    }
    if ( xl6TextEquals(name, "cloneable") ) {
        return xl6SemTypeCanCopy(ctx, actualType);
    }
    return xl6SemTypeIsOrExtendsClass(ctx, actualType, name);
}

static int xl6SemCheckGenericConstraintDecl(XL6_SemContext* ctx, const XL6_AstNode* param)
{
    const XL6_AstNode* constraints;
    size_t i;
    int ok;

    constraints = xl6SemGenericConstraintList(param);
    ok = 1;
    for ( i = 0; constraints != NULL && i < constraints->childCount; ++i ) {
        const XL6_AstNode* constraint = constraints->children[i];
        const XL6_TypeInfo* typeInfo;
        const char* name = constraint != NULL ? constraint->text : NULL;

        if ( name == NULL ) {
            xl6SemErrorNode(ctx, param, "generic constraint is missing a name");
            ok = 0;
            continue;
        }
        if ( xl6SemIsGenericAbilityConstraint(name) ) {
            continue;
        }
        typeInfo = xl6TypeRegistryFind(&ctx->types, name);
        if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS ) {
            xl6SemErrorNode(ctx, constraint, "unknown generic constraint");
            ok = 0;
            continue;
        }
        if ( constraint->childCount > 0 ) {
            xl6SemErrorNode(ctx, constraint, "generic class constraint cannot use type arguments in this stage");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckOneGenericArgConstraints(
    XL6_SemContext* ctx,
    const XL6_AstNode* param,
    const XL6_AstNode* typeArg,
    const XL6_AstNode* atNode,
    int report)
{
    const XL6_AstNode* constraints;
    size_t i;
    int ok;

    constraints = xl6SemGenericConstraintList(param);
    if ( constraints == NULL || constraints->childCount == 0 ) {
        return 1;
    }
    if ( typeArg == NULL ) {
        if ( report ) {
            xl6SemErrorNode(ctx, atNode != NULL ? atNode : param, "generic type argument cannot be inferred for constrained parameter");
        }
        return 0;
    }

    ok = 1;
    for ( i = 0; i < constraints->childCount; ++i ) {
        const XL6_AstNode* constraint = constraints->children[i];
        const XL6_TypeInfo* typeInfo;
        const char* name = constraint != NULL ? constraint->text : NULL;

        if ( name == NULL ) {
            ok = 0;
            continue;
        }
        if ( !xl6SemIsGenericAbilityConstraint(name) ) {
            typeInfo = xl6TypeRegistryFind(&ctx->types, name);
            if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS || constraint->childCount > 0 ) {
                ok = 0;
                continue;
            }
        }
        if ( !xl6SemTypeSatisfiesGenericConstraint(ctx, typeArg, constraint) ) {
            if ( report ) {
                xl6SemErrorNode(ctx, atNode != NULL ? atNode : typeArg, "generic type argument does not satisfy constraint");
            }
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckGenericConstraints(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* atNode,
    int report)
{
    size_t i;
    int ok;

    if ( typeParams == NULL || typeParams->childCount == 0 ) {
        return 1;
    }
    if ( typeArgs == NULL || typeArgs->childCount < typeParams->childCount ) {
        return 0;
    }
    ok = 1;
    for ( i = 0; i < typeParams->childCount; ++i ) {
        if ( !xl6SemCheckOneGenericArgConstraints(
                ctx,
                typeParams->children[i],
                i < typeArgs->childCount ? typeArgs->children[i] : NULL,
                atNode,
                report) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckGenericInferredConstraints(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    const XL6_AstNode* atNode,
    int report)
{
    size_t i;
    int ok;

    if ( typeParams == NULL || typeParams->childCount == 0 ) {
        return 1;
    }
    ok = 1;
    for ( i = 0; i < typeParams->childCount; ++i ) {
        if ( !xl6SemCheckOneGenericArgConstraints(
                ctx,
                typeParams->children[i],
                inferredTypes != NULL ? inferredTypes[i] : NULL,
                atNode,
                report) ) {
            ok = 0;
        }
    }
    return ok;
}

static const char* xl6SemExprDeclaredTypeName(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;

    if ( ctx == NULL || expr == NULL ) {
        return NULL;
    }
    if ( expr->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, expr->text);
        if ( sym != NULL && sym->type != NULL && sym->type->text != NULL &&
             !xl6TextEquals(sym->type->text, "auto") ) {
            return sym->type->text;
        }
    }
    return xl6SemExprStaticTypeName(ctx, expr);
}

static int xl6SemCanConvertType(const char* source, const char* target)
{
    if ( source == NULL || target == NULL ) {
        return 1;
    }
    if ( xl6TextEquals(source, target) ) {
        return 1;
    }
    if ( xl6TextEquals(source, "var") ) {
        return xl6SemIsBuiltinConvertTypeName(target);
    }
    if ( xl6TextEquals(target, "var") ) {
        return xl6SemIsBuiltinConvertTypeName(source) ||
               xl6TextEquals(source, "null") || xl6TextEquals(source, "array") ||
               xl6TextEquals(source, "list") || xl6TextEquals(source, "set") ||
               xl6TextEquals(source, "dict") || xl6TextEquals(source, "function");
    }
    if ( xl6TextEquals(source, "null") ) {
        return xl6SemIsBoolTypeName(target) || xl6SemIsIntegerTypeName(target) ||
               xl6SemIsFloatTypeName(target) || xl6TextEquals(target, "string") ||
               xl6TextEquals(target, "time") || xl6TextEquals(target, "point");
    }
    if ( xl6SemIsIntegerTypeName(target) ) {
        return xl6SemIsIntegerTypeName(source) || xl6SemIsFloatTypeName(source) ||
               xl6SemIsBoolTypeName(source) || xl6TextEquals(source, "string") ||
               xl6TextEquals(source, "time");
    }
    if ( xl6SemIsFloatTypeName(target) ) {
        return xl6SemIsIntegerTypeName(source) || xl6SemIsFloatTypeName(source) ||
               xl6SemIsBoolTypeName(source) || xl6TextEquals(source, "string") ||
               xl6TextEquals(source, "time");
    }
    if ( xl6SemIsBoolTypeName(target) ) {
        return xl6SemIsIntegerTypeName(source) || xl6SemIsFloatTypeName(source) ||
               xl6SemIsBoolTypeName(source) || xl6TextEquals(source, "string") ||
               xl6TextEquals(source, "time") || xl6TextEquals(source, "point");
    }
    if ( xl6TextEquals(target, "string") ) {
        return xl6SemIsIntegerTypeName(source) || xl6SemIsFloatTypeName(source) ||
               xl6SemIsBoolTypeName(source) || xl6TextEquals(source, "string") ||
               xl6TextEquals(source, "time") || xl6TextEquals(source, "point") ||
               xl6TextEquals(source, "type");
    }
    if ( xl6TextEquals(target, "time") ) {
        return xl6TextEquals(source, "time") || xl6TextEquals(source, "string") ||
               xl6SemIsIntegerTypeName(source);
    }
    if ( xl6TextEquals(target, "point") ) {
        return xl6TextEquals(source, "point");
    }
    return 0;
}

static int xl6SemTryNormalizeGenericVarDecl(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* outer;
    XL6_AstNode* inner;
    XL6_AstNode* typeName;
    XL6_AstNode* argName;
    XL6_AstNode* varName;
    XL6_AstNode* typeNode;
    XL6_AstNode* argType;
    XL6_AstNode* args;
    const XL6_TypeInfo* typeInfo;
    char* varText;
    int varLine;
    int varColumn;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_EXPR_STMT || node->childCount != 1 ) {
        return 0;
    }
    outer = node->children[0];
    if ( outer == NULL || outer->kind != XL6_AST_BINARY || !xl6TextEquals(outer->text, ">") || outer->childCount != 2 ) {
        return 0;
    }
    inner = outer->children[0];
    varName = outer->children[1];
    if ( inner == NULL || inner->kind != XL6_AST_BINARY || !xl6TextEquals(inner->text, "<") || inner->childCount != 2 ||
         varName == NULL || varName->kind != XL6_AST_NAME ) {
        return 0;
    }
    typeName = inner->children[0];
    argName = inner->children[1];
    if ( typeName == NULL || typeName->kind != XL6_AST_NAME || argName == NULL || argName->kind != XL6_AST_NAME ) {
        return 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeName->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS || !typeInfo->allowGeneric || typeInfo->genericArity != 1 ) {
        return 0;
    }

    typeNode = xl6SemNewTypeFromName(typeName);
    argType = xl6SemNewTypeFromName(argName);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( typeNode == NULL || argType == NULL || args == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argType);
        xl6AstFree(args);
        xl6SemErrorNode(ctx, node, "泛型 class 声明归一化内存不足");
        return 0;
    }
    xl6AstAddChild(args, argType);
    xl6AstAddChild(typeNode, args);

    varText = xrtCopyStr(varName->text, strlen(varName->text));
    varLine = varName->line;
    varColumn = varName->column;
    xl6AstFree(outer);
    xrtFree(node->children);
    xrtFree(node->text);
    node->kind = XL6_AST_VAR_DECL;
    node->text = varText;
    node->line = varLine;
    node->column = varColumn;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    xl6AstAddChild(node, typeNode);
    return 1;
}

static void xl6SemNormalizeGenericVarDecls(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( xl6SemTryNormalizeGenericVarDecl(ctx, node) ) {
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6SemNormalizeGenericVarDecls(ctx, node->children[i]);
    }
}

static int xl6SemDigitValue(char ch)
{
    if ( ch >= '0' && ch <= '9' ) {
        return ch - '0';
    }
    if ( ch >= 'a' && ch <= 'f' ) {
        return ch - 'a' + 10;
    }
    if ( ch >= 'A' && ch <= 'F' ) {
        return ch - 'A' + 10;
    }
    return -1;
}

static int xl6SemIntLiteralToInt(const XL6_AstNode* node, int* outValue)
{
    const char* p;
    int base;
    int value;

    if ( node == NULL || node->kind != XL6_AST_INT || node->text == NULL ) {
        return 0;
    }

    p = node->text;
    base = 10;
    if ( p[0] == '0' && p[1] == 'x' ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'o' ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'b' ) {
        base = 2;
        p += 2;
    }

    value = 0;
    while ( *p != '\0' ) {
        int digit;

        if ( *p == '_' ) {
            ++p;
            continue;
        }
        digit = xl6SemDigitValue(*p);
        if ( digit < 0 || digit >= base || value > (INT_MAX - digit) / base ) {
            return 0;
        }
        value = value * base + digit;
        ++p;
    }
    if ( outValue != NULL ) {
        *outValue = value;
    }
    return 1;
}

static int xl6SemIntLiteralIsZero(const XL6_AstNode* node)
{
    int value;

    if ( !xl6SemIntLiteralToInt(node, &value) ) {
        return 0;
    }
    return value == 0;
}

static int xl6SemCheckRangeCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* args;
    int ok;

    if ( node->childCount < 2 ) {
        return 1;
    }

    args = node->children[1];
    ok = 1;
    if ( args->childCount < 1 || args->childCount > 3 ) {
        xl6SemErrorNode(ctx, node, "range 参数数量必须是 1、2 或 3");
        ok = 0;
    }
    if ( args->childCount >= 3 ) {
        XL6_AstNode* stepArg = args->children[2];
        if ( stepArg != NULL && stepArg->childCount > 0 && xl6SemIntLiteralIsZero(stepArg->children[0]) ) {
            xl6SemErrorNode(ctx, stepArg->children[0], "range step 不能是 0");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemFixedParamCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6SemParamIsVararg(params->children[count]) ) {
        ++count;
    }
    return count;
}

static int xl6SemFindParamIndex(const XL6_AstNode* params, int fixedCount, const char* name)
{
    int i;

    if ( params == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < fixedCount; ++i ) {
        if ( xl6TextEquals(params->children[i]->text, name) ) {
            return i;
        }
    }
    return -1;
}

static const char* xl6SemFuncName(const XL6_AstNode* node)
{
    if ( node != NULL && node->childCount > 0 && node->children[0] != NULL ) {
        return node->children[0]->text;
    }
    return NULL;
}

static int xl6SemIsFuncDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC ||
            node->kind == XL6_AST_EXTERN_FUNC || node->kind == XL6_AST_EXPORT_FUNC);
}

static int xl6SemIsExternalVarDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXTERN_VAR || node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST);
}

static XL6_AstNode* xl6SemExternalVarNameNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static XL6_AstNode* xl6SemExternalVarTypeNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static XL6_AstNode* xl6SemExternalVarInitExpr(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6SemExternalVarHasCSymbol(const XL6_AstNode* node)
{
    return node != NULL && node->text != NULL && node->text[0] != '\0';
}

static int xl6SemIsPackageExportVarDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST) &&
           !xl6SemExternalVarHasCSymbol(node);
}

static XL6_AstNode* xl6SemVarDeclNameNode(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return xl6SemExternalVarNameNode(node);
    }
    return (XL6_AstNode*)node;
}

static XL6_AstNode* xl6SemVarDeclTypeNode(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return xl6SemExternalVarTypeNode(node);
    }
    return node->childCount > 0 ? node->children[0] : NULL;
}

static XL6_AstNode* xl6SemVarDeclInitExpr(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return xl6SemExternalVarInitExpr(node);
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}

static int xl6SemVarDeclIsConst(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_CONST_DECL || node->kind == XL6_AST_EXPORT_CONST);
}

static const char* xl6SemLiteralExprTypeName(const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return NULL;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT: return "int";
    case XL6_AST_FLOAT: return "float";
    case XL6_AST_STRING: return "string";
    case XL6_AST_INTERP_STRING: return "string";
    case XL6_AST_TIME: return "time";
    case XL6_AST_BOOL: return "bool";
    case XL6_AST_NULL: return "null";
    default: return NULL;
    }
}

static XL6_AstNode* xl6SemFuncTypeParams(const XL6_AstNode* func)
{
    return func != NULL && func->childCount > 4 ? func->children[4] : NULL;
}

static int xl6SemFuncIsGeneric(const XL6_AstNode* func)
{
    XL6_AstNode* typeParams;

    typeParams = xl6SemFuncTypeParams(func);
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6SemTypeParamIndexIn(const XL6_AstNode* typeParams, const char* name)
{
    size_t i;

    if ( typeParams == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        XL6_AstNode* param = typeParams->children[i];
        if ( param != NULL && xl6TextEquals(param->text, name) ) {
            return (int)i;
        }
    }
    return -1;
}

static int xl6SemTypeContainsTypeParam(const XL6_AstNode* typeParams, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( xl6SemTypeParamIndexIn(typeParams, typeNode->text) >= 0 && typeNode->childCount == 0 ) {
        return 1;
    }
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( xl6SemTypeContainsTypeParam(typeParams, typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemRememberInferredType(
    XL6_SemContext* ctx,
    XL6_AstNode** inferredTypes,
    int index,
    const XL6_AstNode* argType)
{
    XL6_AstNode* typeCopy;

    if ( inferredTypes == NULL || index < 0 || argType == NULL ) {
        return 0;
    }

    typeCopy = xl6SemCloneTypeNode(xl6SemResolveAliasType(ctx, argType));
    if ( typeCopy == NULL ) {
        return 0;
    }

    if ( inferredTypes[index] == NULL ) {
        inferredTypes[index] = typeCopy;
        return 1;
    }

    if ( xl6SemTypeEquals(ctx, inferredTypes[index], typeCopy) ) {
        xl6AstFree(typeCopy);
        return 1;
    }

    xl6AstFree(typeCopy);
    return 0;
}

static int xl6SemInferGenericTypeFromTypes(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    const XL6_AstNode* paramType,
    const XL6_AstNode* argType)
{
    const XL6_AstNode* paramResolved;
    const XL6_AstNode* argResolved;
    int typeParamIndex;
    size_t i;

    if ( paramType == NULL || !xl6SemTypeContainsTypeParam(typeParams, paramType) ) {
        return 1;
    }
    if ( argType == NULL ) {
        return 0;
    }

    paramResolved = xl6SemResolveAliasType(ctx, paramType);
    argResolved = xl6SemResolveAliasType(ctx, argType);
    if ( paramResolved == NULL || argResolved == NULL ) {
        return 0;
    }

    typeParamIndex = xl6SemTypeParamIndexIn(typeParams, paramResolved->text);
    if ( typeParamIndex >= 0 && paramResolved->childCount == 0 ) {
        return xl6SemRememberInferredType(ctx, inferredTypes, typeParamIndex, argResolved);
    }

    if ( paramResolved->kind != argResolved->kind ||
         paramResolved->childCount != argResolved->childCount ||
         ((paramResolved->text == NULL) != (argResolved->text == NULL)) ||
         (paramResolved->text != NULL && !xl6TextEquals(paramResolved->text, argResolved->text)) ) {
        return 0;
    }

    for ( i = 0; i < paramResolved->childCount; ++i ) {
        const XL6_AstNode* paramChild = paramResolved->children[i];
        const XL6_AstNode* argChild = argResolved->children[i];

        if ( xl6SemTypeContainsTypeParam(typeParams, paramChild) ) {
            if ( !xl6SemInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, paramChild, argChild) ) {
                return 0;
            }
        } else if ( !xl6SemTypeEquals(ctx, paramChild, argChild) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemContainerLiteralMatchesType(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* resolvedType;
    const XL6_AstNode* itemType;
    size_t i;

    if ( expr == NULL || typeNode == NULL ) {
        return 0;
    }
    resolvedType = xl6SemResolveAliasType(ctx, typeNode);
    if ( resolvedType == NULL ) {
        return 0;
    }
    if ( (expr->kind == XL6_AST_ARRAY_LITERAL && !xl6TextEquals(resolvedType->text, "array")) ||
         (expr->kind == XL6_AST_LIST_LITERAL && !xl6TextEquals(resolvedType->text, "list")) ||
         (expr->kind == XL6_AST_SET_LITERAL && !xl6TextEquals(resolvedType->text, "set")) ||
         (expr->kind == XL6_AST_DICT_LITERAL && !xl6TextEquals(resolvedType->text, "dict")) ) {
        return 0;
    }
    itemType = xl6SemContainerItemType(ctx, resolvedType);
    if ( itemType == NULL ) {
        return 1;
    }

    for ( i = 0; i < expr->childCount; ++i ) {
        const XL6_AstNode* item = expr->children[i];

        /* list/dict 的 child 是 key:value；泛型参数约束 value。 */
        if ( item != NULL && item->kind == XL6_AST_KEY_VALUE ) {
            item = item->childCount > 1 ? item->children[1] : NULL;
        }
        if ( item == NULL || !xl6SemExprCanMatchParamType(ctx, item, itemType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemExprParamMatchCost(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_AstNode* exprTypeNode;
    const char* exprType;
    const char* targetType;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( xl6SemTypeContainsAliasCycle(ctx, typeNode) ) {
        return 0;
    }
    if ( xl6SemFindTypeParam(ctx, typeNode->text) ) {
        return 40;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    targetType = typeNode != NULL ? typeNode->text : NULL;
    if ( targetType == NULL ) {
        return 0;
    }

    exprType = xl6SemExprStaticTypeName(ctx, (XL6_AstNode*)expr);
    if ( exprType == NULL ) {
        exprType = xl6SemLiteralExprTypeName(expr);
    }

    if ( xl6TextEquals(targetType, "var") ) {
        if ( exprType != NULL && xl6TextEquals(exprType, "var") ) {
            return 0;
        }
        return xl6SemExprCanBoxToVar(ctx, expr) ? 80 : -1;
    }
    if ( xl6TextEquals(targetType, "function") ) {
        return xl6SemExprCanMatchFunctionType(ctx, expr, typeNode) ? 0 : -1;
    }
    if ( xl6TextEquals(targetType, "type") ) {
        return exprType != NULL && xl6TextEquals(exprType, "type") ? 0 : -1;
    }
    if ( exprType != NULL && xl6TextEquals(exprType, "null") &&
         xl6SemClassHandleTargetType(ctx, typeNode) != NULL ) {
        return 10;
    }
    /* point<T> 只是静态标签；和裸 point 一样，必须能够表示空指针。 */
    if ( exprType != NULL && xl6TextEquals(exprType, "null") &&
         xl6TextEquals(targetType, "point") ) {
        return 5;
    }
    if ( expr != NULL &&
         (expr->kind == XL6_AST_ARRAY_LITERAL || expr->kind == XL6_AST_LIST_LITERAL ||
          expr->kind == XL6_AST_SET_LITERAL || expr->kind == XL6_AST_DICT_LITERAL) &&
         typeNode->childCount > 0 ) {
        return xl6SemContainerLiteralMatchesType(ctx, expr, typeNode) ? 5 : -1;
    }

    exprTypeNode = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
    if ( exprTypeNode != NULL ) {
        if ( xl6SemTypeEquals(ctx, exprTypeNode, typeNode) ) {
            xl6AstFree(exprTypeNode);
            return 0;
        }
        if ( xl6SemClassHandleCanWiden(ctx, exprTypeNode, typeNode) ) {
            xl6AstFree(exprTypeNode);
            return 15;
        }
        /* 静态泛型容器可以装箱为同类动态容器，反方向仍由逐项校验规则负责。 */
        if ( xl6SemTypeNodeGenericArgCount(exprTypeNode) > 0 &&
             xl6SemTypeNodeGenericArgCount(typeNode) == 0 &&
             xl6SemIsContainerBaseTypeName(exprTypeNode->text) &&
             xl6TextEquals(exprTypeNode->text, typeNode->text) ) {
            xl6AstFree(exprTypeNode);
            return 30;
        }
        /*
            无类型实参的动态容器可以导入同类静态泛型容器。
            cgen/runtime 会逐项验证并事务式提交；静态泛型之间仍保持不变性。
        */
        if ( xl6SemTypeNodeGenericArgCount(exprTypeNode) == 0 &&
             xl6SemTypeNodeGenericArgCount(typeNode) > 0 &&
             xl6SemIsContainerBaseTypeName(exprTypeNode->text) &&
             xl6TextEquals(exprTypeNode->text, typeNode->text) ) {
            xl6AstFree(exprTypeNode);
            return 30;
        }
        /*
            泛型类型不能退化为只比较外层名字，否则 Base 会误匹配
            Derived，array<int> 也会误匹配 array<string>。
            允许的泛型转换必须在上方作为明确规则处理。
        */
        if ( (xl6SemTypeNodeGenericArgCount(exprTypeNode) > 0 ||
              xl6SemTypeNodeGenericArgCount(typeNode) > 0) &&
             !(expr != NULL && expr->kind == XL6_AST_TYPED_LITERAL &&
               xl6SemTypeNodeGenericArgCount(exprTypeNode) == 0 &&
               xl6TextEquals(exprTypeNode->text, typeNode->text)) ) {
            xl6AstFree(exprTypeNode);
            return -1;
        }
        xl6AstFree(exprTypeNode);
    }

    if ( exprType == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(exprType, "var") ) {
        /*
            var 的运行时值不能参与静态参数匹配。
            需要具体类型时必须显式使用 Type(expr) 或 expr as Type。
        */
        return -1;
    }
    if ( !xl6SemNameIsRegisteredType(ctx, exprType) && !xl6SemFindTypeParam(ctx, exprType) ) {
        return 0;
    }

    if ( xl6TextEquals(exprType, targetType) ) {
        return 0;
    }
    if ( xl6TextEquals(exprType, "null") && xl6TextEquals(targetType, "point") ) {
        return 5;
    }
    if ( xl6TextEquals(exprType, "null") && xl6TextEquals(targetType, "__class_handle") ) {
        return 5;
    }

    if ( expr != NULL && xl6SemExprIntegerLiteralFitsType(expr, targetType) ) {
        return 1;
    }
    if ( expr != NULL && expr->kind == XL6_AST_FLOAT && xl6SemIsFloatTypeName(targetType) ) {
        return 2;
    }

    if ( xl6SemCanSafeWidenTypeName(exprType, targetType) ) {
        if ( xl6SemIsFloatTypeName(targetType) &&
             (xl6SemIsIntegerTypeName(exprType) || xl6SemIsBoolTypeName(exprType)) ) {
            return 20;
        }
        return 10;
    }
    return -1;
}

static int xl6SemExprCanMatchParamType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    return xl6SemExprParamMatchCost(ctx, expr, typeNode) >= 0;
}

static int xl6SemInferGenericFromExprShape(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* paramType,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    int depth)
{
    const XL6_AstNode* literal;
    const XL6_AstNode* itemType;
    XL6_AstNode* exprType;
    size_t i;
    int containerShape;

    if ( expr == NULL || paramType == NULL || depth > 16 ) {
        return 0;
    }
    literal = expr;
    if ( expr->kind == XL6_AST_TYPED_LITERAL && expr->childCount > 0 ) {
        literal = expr->children[0];
    }
    containerShape = literal != NULL &&
        ((literal->kind == XL6_AST_ARRAY_LITERAL && xl6TextEquals(paramType->text, "array")) ||
         (literal->kind == XL6_AST_LIST_LITERAL && xl6TextEquals(paramType->text, "list")) ||
         (literal->kind == XL6_AST_SET_LITERAL && xl6TextEquals(paramType->text, "set")) ||
         (literal->kind == XL6_AST_DICT_LITERAL && xl6TextEquals(paramType->text, "dict")));
    itemType = containerShape ? xl6SemTypeNodeFirstGenericArg(paramType) : NULL;
    if ( itemType != NULL ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            const XL6_AstNode* item = literal->children[i];

            if ( item != NULL && item->kind == XL6_AST_KEY_VALUE ) {
                item = item->childCount > 1 ? item->children[1] : NULL;
            }
            if ( item == NULL ||
                 !xl6SemInferGenericFromExprShape(
                     ctx, item, itemType, typeParams, inferredTypes, depth + 1) ) {
                return 0;
            }
        }
        /* 空字面量不提供新信息，但可以由其他参数完成 T 的推断。 */
        return 1;
    }

    exprType = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
    if ( exprType == NULL ) {
        return 0;
    }
    containerShape = xl6SemInferGenericTypeFromTypes(
        ctx, typeParams, inferredTypes, paramType, exprType);
    xl6AstFree(exprType);
    return containerShape;
}

static int xl6SemExprCanMatchGenericParamType(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes)
{
    int index;
    int ok;

    if ( typeNode == NULL ) {
        return 1;
    }

    if ( xl6SemTypeContainsTypeParam(typeParams, typeNode) ) {
        ok = xl6SemInferGenericFromExprShape(
            ctx, expr, typeNode, typeParams, inferredTypes, 0);
        if ( ok ) {
            return 1;
        }

        index = xl6SemTypeParamIndexIn(typeParams, typeNode->text);
        if ( index < 0 && !xl6TextEquals(typeNode->text, "function") ) {
            return xl6SemExprCanMatchParamType(ctx, expr, typeNode);
        }
        return 0;
    }

    (void)inferredTypes;
    return xl6SemExprCanMatchParamType(ctx, expr, typeNode);
}

static int xl6SemExprGenericParamMatchCost(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes)
{
    int index;
    int ok;

    if ( typeNode == NULL ) {
        return 0;
    }

    if ( xl6SemTypeContainsTypeParam(typeParams, typeNode) ) {
        ok = xl6SemInferGenericFromExprShape(
            ctx, expr, typeNode, typeParams, inferredTypes, 0);
        if ( ok ) {
            return 40;
        }

        index = xl6SemTypeParamIndexIn(typeParams, typeNode->text);
        if ( index < 0 && !xl6TextEquals(typeNode->text, "function") ) {
            return xl6SemExprParamMatchCost(ctx, expr, typeNode);
        }
        return -1;
    }

    (void)inferredTypes;
    return xl6SemExprParamMatchCost(ctx, expr, typeNode);
}

static const XL6_AstNode* xl6SemGenericTypeParamArg(
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs,
    const char* name)
{
    size_t i;

    if ( typeParams == NULL || typeArgs == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount && i < typeArgs->childCount; ++i ) {
        XL6_AstNode* param = typeParams->children[i];
        if ( param != NULL && xl6TextEquals(param->text, name) ) {
            return typeArgs->children[i];
        }
    }
    return NULL;
}

static XL6_AstNode* xl6SemSubstituteGenericTypeArgs(
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* replacement;
    XL6_AstNode* copy;
    size_t i;

    if ( typeNode == NULL ) {
        return NULL;
    }
    replacement = xl6SemGenericTypeParamArg(typeParams, typeArgs, typeNode->text);
    if ( replacement != NULL && typeNode->childCount == 0 ) {
        return xl6SemCloneTypeNode(replacement);
    }

    copy = xl6SemCloneTypeNode(typeNode);
    if ( copy == NULL ) {
        return NULL;
    }
    for ( i = 0; i < copy->childCount; ++i ) {
        XL6_AstNode* child = xl6SemSubstituteGenericTypeArgs(typeParams, typeArgs, typeNode->children[i]);
        if ( child == NULL && typeNode->children[i] != NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstFree(copy->children[i]);
        copy->children[i] = child;
    }
    return copy;
}

static int xl6SemExprExplicitGenericParamMatchCost(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs)
{
    XL6_AstNode* concreteType;
    int cost;

    concreteType = xl6SemSubstituteGenericTypeArgs(typeParams, typeArgs, typeNode);
    cost = xl6SemExprParamMatchCost(ctx, expr, concreteType != NULL ? concreteType : typeNode);
    xl6AstFree(concreteType);
    return cost;
}

static int xl6SemCallCandidateMatches(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* func, int* outCost)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    XL6_AstNode* typeParams;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int hasCVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    XL6_AstNode** inferredTypes;
    size_t i;
    int ok;
    int cost;

    params = (XL6_AstNode*)xl6SemCallableParams(func);
    typeParams = xl6SemFuncTypeParams(func);
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
    hasCVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsCVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    inferredTypes = typeParams != NULL && typeParams->childCount > 0 ?
                    (XL6_AstNode**)xrtCalloc(typeParams->childCount, sizeof(XL6_AstNode*)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 0;

    if ( fixedCount > 0 && provided == NULL ) {
        xrtFree(inferredTypes);
        return 0;
    }
    if ( typeParams != NULL && typeParams->childCount > 0 && inferredTypes == NULL ) {
        xrtFree(provided);
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    XL6_AstNode* paramType = params->children[index]->childCount > 0 ? params->children[index]->children[0] : NULL;
                    XL6_AstNode* argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    if ( !xl6SemCallArgModeMatchesParam(ctx, arg, params->children[index], argExpr) ) {
                        ok = 0;
                    }
                    argCost = ok ? xl6SemExprGenericParamMatchCost(ctx, argExpr, paramType, typeParams, inferredTypes) : -1;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[index] = 1;
                } else if ( !hasDictVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            } else {
                if ( seenNamed ) {
                    ok = 0;
                } else if ( positionalIndex < fixedCount ) {
                    XL6_AstNode* paramType = params->children[positionalIndex]->childCount > 0 ? params->children[positionalIndex]->children[0] : NULL;
                    XL6_AstNode* argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    if ( !xl6SemCallArgModeMatchesParam(ctx, arg, params->children[positionalIndex], argExpr) ) {
                        ok = 0;
                    }
                    argCost = ok ? xl6SemExprGenericParamMatchCost(ctx, argExpr, paramType, typeParams, inferredTypes) : -1;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg && !hasCVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            }
        }
    }

    for ( i = 0; ok && i < (size_t)fixedCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    if ( ok && inferredTypes != NULL &&
         !xl6SemCheckGenericInferredConstraints(ctx, typeParams, inferredTypes, call, 0) ) {
        ok = 0;
    }

    if ( inferredTypes != NULL ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferredTypes[i]);
        }
    }
    xrtFree(provided);
    xrtFree(inferredTypes);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static int xl6SemCallCandidateMatchesExplicitGeneric(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* func,
    const XL6_AstNode* typeArgs,
    int* outCost)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    XL6_AstNode* typeParams;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int hasCVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;
    int cost;

    typeParams = xl6SemFuncTypeParams(func);
    if ( typeParams == NULL || typeParams->childCount == 0 ||
         typeArgs == NULL || typeParams->childCount != typeArgs->childCount ) {
        return 0;
    }
    if ( !xl6SemCheckGenericConstraints(ctx, typeParams, typeArgs, call, 0) ) {
        return 0;
    }

    params = (XL6_AstNode*)xl6SemCallableParams(func);
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
    hasCVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsCVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 40;

    if ( fixedCount > 0 && provided == NULL ) {
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg == NULL ) {
                continue;
            }
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    XL6_AstNode* paramType = params->children[index]->childCount > 0 ? params->children[index]->children[0] : NULL;
                    XL6_AstNode* argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    if ( !xl6SemCallArgModeMatchesParam(ctx, arg, params->children[index], argExpr) ) {
                        ok = 0;
                    }
                    argCost = ok ? xl6SemExprExplicitGenericParamMatchCost(ctx, argExpr, paramType, typeParams, typeArgs) : -1;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[index] = 1;
                } else if ( !hasDictVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            } else {
                if ( seenNamed ) {
                    ok = 0;
                } else if ( positionalIndex < fixedCount ) {
                    XL6_AstNode* paramType = params->children[positionalIndex]->childCount > 0 ? params->children[positionalIndex]->children[0] : NULL;
                    XL6_AstNode* argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    if ( !xl6SemCallArgModeMatchesParam(ctx, arg, params->children[positionalIndex], argExpr) ) {
                        ok = 0;
                    }
                    argCost = ok ? xl6SemExprExplicitGenericParamMatchCost(ctx, argExpr, paramType, typeParams, typeArgs) : -1;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg && !hasCVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            }
        }
    }

    for ( i = 0; ok && i < (size_t)fixedCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    xrtFree(provided);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static int xl6SemParamsRejectCallArgCount(const XL6_AstNode* params, int actualCount, int* expectedCount)
{
    int requiredCount;
    int fixedCount;

    requiredCount = xl6SemParamRequiredCount(params);
    fixedCount = xl6SemFixedParamCount(params);
    if ( actualCount < requiredCount ) {
        if ( expectedCount != NULL ) {
            *expectedCount = requiredCount;
        }
        return 1;
    }
    if ( !xl6SemFuncHasVararg(params) && actualCount > fixedCount ) {
        if ( expectedCount != NULL ) {
            *expectedCount = fixedCount;
        }
        return 1;
    }
    return 0;
}

static int xl6SemDiagnoseCallArgTypeMismatch(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* func,
    const char* reason)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    XL6_AstNode* typeParams;
    int fixedCount;
    int positionalIndex;
    int seenNamed;
    size_t i;

    params = (XL6_AstNode*)xl6SemCallableParams(func);
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    typeParams = xl6SemFuncTypeParams(func);
    if ( typeParams != NULL && typeParams->childCount > 0 ) {
        return 0;
    }
    if ( params == NULL || args == NULL ) {
        return 0;
    }

    fixedCount = xl6SemFixedParamCount(params);
    positionalIndex = 0;
    seenNamed = 0;
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* paramType;
        XL6_AstNode* argExpr;
        int paramIndex;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            seenNamed = 1;
            paramIndex = xl6SemFindParamIndex(params, fixedCount, arg->text);
            if ( paramIndex < 0 ) {
                return 0;
            }
            argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
        } else {
            if ( seenNamed || positionalIndex >= fixedCount ) {
                return 0;
            }
            paramIndex = positionalIndex;
            argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
            positionalIndex += 1;
        }

        paramType = params->children[paramIndex] != NULL && params->children[paramIndex]->childCount > 0 ?
                    params->children[paramIndex]->children[0] : NULL;
        if ( paramType != NULL && xl6SemExprParamMatchCost(ctx, argExpr, paramType) < 0 ) {
            const char* valueType = xl6SemExprStaticTypeName(ctx, argExpr);
            if ( valueType == NULL ) {
                valueType = xl6SemLiteralExprTypeName(argExpr);
            }
            xl6SemTypeMismatchError(ctx, argExpr != NULL ? argExpr : call, reason, paramType, valueType);
            return 1;
        }
    }
    return 0;
}

static int xl6SemDiagnoseGenericCallArgMismatch(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* func)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    XL6_AstNode* typeParams;
    XL6_AstNode** inferredTypes;
    int* provided;
    int fixedCount;
    int positionalIndex;
    int seenNamed;
    size_t i;

    params = (XL6_AstNode*)xl6SemCallableParams(func);
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    typeParams = xl6SemFuncTypeParams(func);
    if ( typeParams == NULL || typeParams->childCount == 0 || params == NULL || args == NULL ) {
        return 0;
    }

    fixedCount = xl6SemFixedParamCount(params);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    inferredTypes = (XL6_AstNode**)xrtCalloc(typeParams->childCount, sizeof(XL6_AstNode*));
    if ( (fixedCount > 0 && provided == NULL) || inferredTypes == NULL ) {
        xrtFree(provided);
        xrtFree(inferredTypes);
        return 0;
    }

    positionalIndex = 0;
    seenNamed = 0;
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* argExpr;
        XL6_AstNode* paramType;
        XL6_AstNode* exprType;
        const XL6_AstNode* paramResolved;
        const XL6_AstNode* targetType;
        const char* valueType;
        int paramIndex;
        int typeParamIndex;
        size_t j;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            seenNamed = 1;
            paramIndex = xl6SemFindParamIndex(params, fixedCount, arg->text);
            if ( paramIndex < 0 || provided[paramIndex] ) {
                goto cleanup_no_diagnostic;
            }
            argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
        } else {
            if ( seenNamed || positionalIndex >= fixedCount ) {
                goto cleanup_no_diagnostic;
            }
            paramIndex = positionalIndex;
            argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
            positionalIndex += 1;
        }
        provided[paramIndex] = 1;

        paramType = params->children[paramIndex] != NULL && params->children[paramIndex]->childCount > 0 ?
                    params->children[paramIndex]->children[0] : NULL;
        if ( paramType == NULL || !xl6SemTypeContainsTypeParam(typeParams, paramType) ) {
            continue;
        }

        exprType = xl6SemInferAutoTypeNode(ctx, argExpr);
        if ( exprType == NULL ) {
            continue;
        }
        if ( !xl6SemInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, paramType, exprType) ) {
            paramResolved = xl6SemResolveAliasType(ctx, paramType);
            typeParamIndex = paramResolved != NULL && paramResolved->childCount == 0 ?
                             xl6SemTypeParamIndexIn(typeParams, paramResolved->text) : -1;
            targetType = typeParamIndex >= 0 ? inferredTypes[typeParamIndex] : NULL;
            valueType = xl6SemStaticTypeNameFromTypeNode(ctx, exprType);
            if ( valueType == NULL ) {
                valueType = xl6SemDisplayTypeName(ctx, exprType);
            }
            xl6SemTypeMismatchError(
                ctx,
                argExpr != NULL ? argExpr : call,
                "泛型函数类型推断冲突",
                targetType != NULL ? targetType : paramType,
                valueType);
            xl6AstFree(exprType);
            for ( j = 0; j < typeParams->childCount; ++j ) {
                xl6AstFree(inferredTypes[j]);
            }
            xrtFree(inferredTypes);
            xrtFree(provided);
            return 1;
        }
        xl6AstFree(exprType);
    }

    if ( !xl6SemCheckGenericInferredConstraints(ctx, typeParams, inferredTypes, call, 1) ) {
        if ( inferredTypes != NULL ) {
            for ( i = 0; i < typeParams->childCount; ++i ) {
                xl6AstFree(inferredTypes[i]);
            }
        }
        xrtFree(inferredTypes);
        xrtFree(provided);
        return 1;
    }

cleanup_no_diagnostic:
    if ( inferredTypes != NULL ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferredTypes[i]);
        }
    }
    xrtFree(inferredTypes);
    xrtFree(provided);
    return 0;
}

static int xl6SemCallHasDynamicVarArgument(XL6_SemContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* args;
    size_t i;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        const XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        const char* typeName = xl6SemExprStaticTypeName(ctx, (XL6_AstNode*)expr);

        if ( xl6TextEquals(typeName, "var") ) {
            return 1;
        }
    }
    return 0;
}

static XL6_AstNode* xl6SemResolveCallFuncEx(XL6_SemContext* ctx, XL6_AstNode* call, const char* name, int report)
{
    size_t i;
    XL6_AstNode* fixedSelected;
    XL6_AstNode* varargSelected;
    int candidateCount;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;
    int actualArgCount;
    int countRejectCount;
    int countRejectExpected;
    int countRejectSameExpected;
    XL6_AstNode* onlyCandidate;

    fixedSelected = NULL;
    varargSelected = NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    actualArgCount = xl6SemCallArgCount(call);
    countRejectCount = 0;
    countRejectExpected = -1;
    countRejectSameExpected = 1;
    onlyCandidate = NULL;
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( xl6SemIsFuncDecl(node) && xl6TextEquals(xl6SemFuncName(node), name) ) {
            XL6_AstNode* params;
            int cost;
            int expected;

            params = (XL6_AstNode*)xl6SemCallableParams(node);
            candidateCount += 1;
            if ( candidateCount == 1 ) {
                onlyCandidate = node;
            } else {
                onlyCandidate = NULL;
            }
            if ( xl6SemParamsRejectCallArgCount(params, actualArgCount, &expected) ) {
                countRejectCount += 1;
                if ( countRejectExpected < 0 ) {
                    countRejectExpected = expected;
                } else if ( countRejectExpected != expected ) {
                    countRejectSameExpected = 0;
                }
            }
            if ( xl6SemCallCandidateMatches(ctx, call, node, &cost) ) {
                if ( xl6SemFuncHasVararg(params) ) {
                    if ( varargBestCount == 0 || cost < varargBestCost ) {
                        varargSelected = node;
                        varargBestCost = cost;
                        varargBestCount = 1;
                    } else if ( cost == varargBestCost ) {
                        varargBestCount += 1;
                    }
                } else {
                    if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                        fixedSelected = node;
                        fixedBestCost = cost;
                        fixedBestCount = 1;
                    } else if ( cost == fixedBestCost ) {
                        fixedBestCount += 1;
                    }
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedSelected;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6SemErrorNode(ctx, call, "函数调用存在重载歧义");
        }
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( report ) {
        if ( varargBestCount > 1 ) {
            xl6SemErrorNode(ctx, call, "函数调用存在重载歧义");
        } else if ( candidateCount > 0 ) {
            if ( countRejectCount == candidateCount && countRejectSameExpected && countRejectExpected >= 0 ) {
                xl6SemCountMismatchError(ctx, call, "函数调用实参数量和重载签名不匹配", (size_t)countRejectExpected, (size_t)actualArgCount);
            } else if ( xl6SemCallHasDynamicVarArgument(ctx, call) ) {
                xl6SemErrorNode(ctx, call, "dynamic var argument requires explicit 'as Type' conversion before overload resolution");
            } else if ( candidateCount == 1 &&
                        xl6SemDiagnoseCallArgTypeMismatch(ctx, call, onlyCandidate, "函数调用实参类型和参数类型不匹配") ) {
                return NULL;
            } else if ( candidateCount == 1 && xl6SemDiagnoseGenericCallArgMismatch(ctx, call, onlyCandidate) ) {
                return NULL;
            } else if ( candidateCount == 1 && !xl6SemCheckKnownCall(ctx, call, onlyCandidate) ) {
                return NULL;
            } else {
                xl6SemErrorNode(ctx, call, "没有匹配的函数重载");
            }
        } else {
            /* 没有候选时保持静默，由未知标识符或后续检查给出更具体的诊断。 */
        }
    }
    return NULL;
}

static XL6_AstNode* xl6SemResolveCallFunc(XL6_SemContext* ctx, XL6_AstNode* call, const char* name)
{
    return xl6SemResolveCallFuncEx(ctx, call, name, 1);
}

static XL6_AstNode* xl6SemResolveExplicitGenericCallFunc(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    const char* name,
    const XL6_AstNode* typeArgs,
    int report)
{
    size_t i;
    XL6_AstNode* fixedSelected;
    XL6_AstNode* varargSelected;
    int candidateCount;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    fixedSelected = NULL;
    varargSelected = NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL || typeArgs == NULL ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( xl6SemIsFuncDecl(node) && xl6SemFuncIsGeneric(node) &&
             xl6TextEquals(xl6SemFuncName(node), name) ) {
            int cost;
            candidateCount += 1;
            if ( xl6SemCallCandidateMatchesExplicitGeneric(ctx, call, node, typeArgs, &cost) ) {
                XL6_AstNode* params = (XL6_AstNode*)xl6SemCallableParams(node);
                if ( xl6SemFuncHasVararg(params) ) {
                    if ( varargBestCount == 0 || cost < varargBestCost ) {
                        varargSelected = node;
                        varargBestCost = cost;
                        varargBestCount = 1;
                    } else if ( cost == varargBestCost ) {
                        varargBestCount += 1;
                    }
                } else {
                    if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                        fixedSelected = node;
                        fixedBestCost = cost;
                        fixedBestCount = 1;
                    } else if ( cost == fixedBestCost ) {
                        fixedBestCount += 1;
                    }
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedSelected;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6SemErrorNode(ctx, call, "显式泛型函数调用存在重载歧义");
        }
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( report ) {
        if ( varargBestCount > 1 ) {
            xl6SemErrorNode(ctx, call, "显式泛型函数调用存在重载歧义");
        } else if ( candidateCount > 0 ) {
            xl6SemErrorNode(ctx, call, "没有匹配的显式泛型函数重载");
        }
    }
    return NULL;
}

static int xl6SemCallArgCount(const XL6_AstNode* call)
{
    const XL6_AstNode* args;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    return args != NULL ? (int)args->childCount : 0;
}

static XL6_AstNode* xl6SemResolveCreateMember(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* typeDecl,
    XL6_AstKind memberKind)
{
    XL6_AstNode* body;
    XL6_AstNode* fixedSelected;
    XL6_AstNode* varargSelected;
    int candidateCount;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;
    int actualArgCount;
    int countRejectCount;
    int countRejectExpected;
    int countRejectSameExpected;
    size_t i;

    body = NULL;
    if ( typeDecl != NULL && typeDecl->kind == XL6_AST_CLASS ) {
        body = typeDecl->childCount > 1 ? typeDecl->children[1] : NULL;
    }

    fixedSelected = NULL;
    varargSelected = NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    actualArgCount = xl6SemCallArgCount(call);
    countRejectCount = 0;
    countRejectExpected = -1;
    countRejectSameExpected = 1;
    if ( body == NULL ) {
        return NULL;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        const XL6_AstNode* params;
        const char* name;
        int acceptsNativeClassExternCreate;

        acceptsNativeClassExternCreate =
            typeDecl != NULL &&
            typeDecl->kind == XL6_AST_CLASS &&
            memberKind == XL6_AST_CREATE &&
            member != NULL &&
            member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL;

        if ( member == NULL || (member->kind != memberKind && !acceptsNativeClassExternCreate) ) {
            continue;
        }
        name = xl6SemMemberName(member);
        if ( !xl6TextEquals(name, "create") ) {
            continue;
        }
        {
            int cost;
            int expected;

            params = xl6SemCallableParams(member);
            candidateCount += 1;
            if ( xl6SemParamsRejectCallArgCount(params, actualArgCount, &expected) ) {
                countRejectCount += 1;
                if ( countRejectExpected < 0 ) {
                    countRejectExpected = expected;
                } else if ( countRejectExpected != expected ) {
                    countRejectSameExpected = 0;
                }
            }
            if ( !xl6SemCallCandidateMatches(ctx, call, member, &cost) ) {
                continue;
            }
            if ( xl6SemFuncHasVararg(params) ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargSelected = member;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedSelected = member;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedSelected;
    }
    if ( fixedBestCount > 1 ) {
            xl6SemErrorNode(ctx, call, "构造调用存在重载歧义");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( varargBestCount > 1 ) {
            xl6SemErrorNode(ctx, call, "构造调用存在重载歧义");
    } else if ( candidateCount > 0 ) {
        if ( countRejectCount == candidateCount && countRejectSameExpected && countRejectExpected >= 0 ) {
            xl6SemCountMismatchError(ctx, call, "create 调用实参数量和重载签名不匹配", (size_t)countRejectExpected, (size_t)actualArgCount);
        } else {
            xl6SemErrorNode(ctx, call, "没有匹配的 create 重载");
        }
    }
    return NULL;
}

static int xl6SemClassHasTypeParams(const XL6_AstNode* classNode)
{
    const XL6_AstNode* typeParams;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static const XL6_AstNode* xl6SemCtorCallTypeArgs(XL6_AstNode* call)
{
    XL6_AstNode* callee;

    callee = call != NULL && call->childCount > 0 ? call->children[0] : NULL;
    if ( callee == NULL || callee->kind != XL6_AST_GENERIC_INSTANT || callee->childCount < 2 ) {
        return NULL;
    }
    return callee->children[1];
}

static const XL6_AstNode* xl6SemClassTypeParamArg(const XL6_AstNode* classNode, const XL6_AstNode* typeArgs, const char* name)
{
    const XL6_AstNode* typeParams;
    size_t i;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    if ( typeParams == NULL || typeArgs == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount && i < typeArgs->childCount; ++i ) {
        const XL6_AstNode* param = typeParams->children[i];
        if ( param != NULL && xl6TextEquals(param->text, name) ) {
            return typeArgs->children[i];
        }
    }
    return NULL;
}

static XL6_AstNode* xl6SemSubstituteClassType(
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* replacement;
    XL6_AstNode* copy;
    size_t i;

    if ( typeNode == NULL ) {
        return NULL;
    }
    replacement = xl6SemClassTypeParamArg(classNode, typeArgs, typeNode->text);
    if ( replacement != NULL && typeNode->childCount == 0 ) {
        return xl6SemCloneTypeNode(replacement);
    }

    copy = xl6SemCloneTypeNode(typeNode);
    if ( copy == NULL ) {
        return NULL;
    }
    for ( i = 0; i < copy->childCount; ++i ) {
        XL6_AstNode* child = xl6SemSubstituteClassType(classNode, typeArgs, typeNode->children[i]);
        if ( child == NULL && typeNode->children[i] != NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstFree(copy->children[i]);
        copy->children[i] = child;
    }
    return copy;
}

/*
    泛型 class 的成员签名必须先代入实例类型，再复用统一的调用匹配器。
    这样可选参数、命名参数和可变参数仍只维护一套规则。
*/
static int xl6SemGenericClassCallableCandidateMatches(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* member,
    const XL6_AstNode* classNode,
    const XL6_AstNode* concreteType,
    int* outCost)
{
    const XL6_AstNode* typeArgs;
    XL6_AstNode* concreteMember;
    int result;

    typeArgs = concreteType != NULL ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
    if ( typeArgs == NULL || !xl6SemClassHasTypeParams(classNode) ) {
        return xl6SemCallCandidateMatches(ctx, call, member, outCost);
    }

    concreteMember = xl6SemSubstituteClassType(classNode, typeArgs, member);
    if ( concreteMember == NULL ) {
        return 0;
    }
    result = xl6SemCallCandidateMatches(ctx, call, concreteMember, outCost);
    xl6AstFree(concreteMember);
    return result;
}

static int xl6SemExprCanMatchClassCreateParam(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* paramType,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeArgs)
{
    XL6_AstNode* concreteType;
    int ok;

    concreteType = xl6SemSubstituteClassType(classNode, typeArgs, paramType);
    if ( concreteType == NULL ) {
        return xl6SemExprCanMatchParamType(ctx, expr, paramType);
    }
    ok = xl6SemExprCanMatchParamType(ctx, expr, concreteType);
    xl6AstFree(concreteType);
    return ok;
}

static int xl6SemGenericClassCreateCandidateMatches(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* createNode,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeArgs)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;

    params = createNode != NULL && createNode->childCount > 0 ? createNode->children[0] : NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;

    if ( fixedCount > 0 && provided == NULL ) {
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                XL6_AstNode* paramType;
                XL6_AstNode* argExpr;

                seenNamed = 1;
                index = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( index < 0 ) {
                    if ( !hasDictVararg ) {
                        ok = 0;
                    }
                    continue;
                }
                paramType = params->children[index]->childCount > 0 ? params->children[index]->children[0] : NULL;
                argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                if ( provided[index] ||
                     !xl6SemExprCanMatchClassCreateParam(ctx, argExpr, paramType, classNode, typeArgs) ) {
                    ok = 0;
                }
                provided[index] = 1;
            } else {
                XL6_AstNode* paramType;
                XL6_AstNode* argExpr;

                if ( seenNamed ) {
                    ok = 0;
                    continue;
                }
                if ( positionalIndex >= fixedCount ) {
                    if ( !hasArrayVararg ) {
                        ok = 0;
                    }
                    continue;
                }
                paramType = params->children[positionalIndex]->childCount > 0 ? params->children[positionalIndex]->children[0] : NULL;
                argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                if ( provided[positionalIndex] ||
                     !xl6SemExprCanMatchClassCreateParam(ctx, argExpr, paramType, classNode, typeArgs) ) {
                    ok = 0;
                }
                provided[positionalIndex] = 1;
                positionalIndex += 1;
            }
        }
    }

    for ( i = 0; ok && i < (size_t)fixedCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        }
    }

    xrtFree(provided);
    return ok;
}

static XL6_AstNode* xl6SemResolveGenericClassCreate(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const XL6_AstNode* typeArgs)
{
    XL6_AstNode* body;
    XL6_AstNode* selected;
    int candidateCount;
    int matchCount;
    size_t i;

    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    selected = NULL;
    candidateCount = 0;
    matchCount = 0;
    if ( body == NULL ) {
        return NULL;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        if ( member == NULL || member->kind != XL6_AST_CREATE ||
             !xl6TextEquals(xl6SemMemberName(member), "create") ) {
            continue;
        }
        candidateCount += 1;
        if ( xl6SemGenericClassCreateCandidateMatches(ctx, call, member, classNode, typeArgs) ) {
            selected = member;
            matchCount += 1;
        }
    }

    if ( matchCount == 1 ) {
        return selected;
    }
    if ( matchCount > 1 ) {
        xl6SemErrorNode(ctx, call, "构造调用存在重载歧义");
    } else if ( candidateCount > 0 ) {
        xl6SemErrorNode(ctx, call, "没有匹配的 create 重载");
    }
    return NULL;
}

static int xl6SemCheckNativeClassPointBindingCtor(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* classNode)
{
    XL6_AstNode* args;
    XL6_AstNode* arg;
    XL6_AstNode* argExpr;
    XL6_AstNode pointType;
    const char* argTypeName;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( args == NULL || args->childCount != 1 ) {
        const char* className = classNode != NULL && classNode->text != NULL ? classNode->text : "<class>";
        char message[192];
        snprintf(message, sizeof(message), "native-backed class without create can only bind one point handle: %s", className);
        xl6SemErrorNode(ctx, call, message);
        return 0;
    }
    arg = args->children[0];
    if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG || arg->childCount == 0 ) {
        xl6SemErrorNode(ctx, arg != NULL ? arg : call, "native-backed class handle binding requires one positional point argument");
        return 0;
    }
    argExpr = arg->children[0];
    argTypeName = xl6SemExprStaticTypeName(ctx, argExpr);
    if ( argTypeName != NULL && !xl6TextEquals(argTypeName, "point") ) {
        xl6SemErrorNode(ctx, argExpr, "native-backed class handle binding argument must be point");
        return 0;
    }

    memset(&pointType, 0, sizeof(pointType));
    pointType.kind = XL6_AST_TYPE;
    pointType.text = "point";
    if ( !xl6SemExprCanMatchParamType(ctx, argExpr, &pointType) ) {
        xl6SemErrorNode(ctx, argExpr, "native-backed class handle binding argument must be point");
        return 0;
    }
    if ( xl6SemClassDeclHasDestroy(classNode) && argExpr->kind == XL6_AST_NAME ) {
        const XL6_Symbol* source = xl6ScopeFind(ctx->scope, argExpr->text);
        if ( xl6SemSymbolIsConst(source) ) {
            xl6SemErrorNode(ctx, argExpr, "owning native-backed class cannot consume a const point handle");
            return 0;
        }
    }
    return 1;
}

static int xl6SemCheckClassCtorCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* classNode)
{
    const XL6_AstNode* typeArgs;
    int hasCreate;

    if ( classNode == NULL ) {
        return 1;
    }

    if ( xl6SemClassHasTypeParams(classNode) ) {
        typeArgs = xl6SemCtorCallTypeArgs(call);
        if ( typeArgs == NULL ) {
            xl6SemErrorNode(ctx, call, "开放泛型 class 必须显式实例化后构造");
            return 0;
        }
        if ( xl6SemResolveGenericClassCreate(ctx, call, classNode, typeArgs) != NULL ) {
            return 1;
        }
        if ( xl6SemCallArgCount(call) == 0 ) {
            return 1;
        }
        return 0;
    }

    hasCreate = xl6SemClassHasCreate(classNode);
    if ( xl6SemResolveCreateMember(ctx, call, classNode, XL6_AST_CREATE) != NULL ) {
        return 1;
    }
    if ( hasCreate ) {
        return 0;
    }
    if ( !hasCreate && xl6SemClassDeclHasNativeHandle(ctx, classNode) && xl6SemCallArgCount(call) > 0 ) {
        return xl6SemCheckNativeClassPointBindingCtor(ctx, call, classNode);
    }
    if ( xl6SemCallArgCount(call) == 0 ) {
        return 1;
    }
    xl6SemCountMismatchError(ctx, call, "class 默认构造不接收实参", 0, (size_t)xl6SemCallArgCount(call));
    return 0;
}

static int xl6SemCheckCtorCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* callee;
    XL6_AstNode* nameNode;
    const XL6_TypeInfo* typeInfo;

    if ( call == NULL || call->childCount == 0 ) {
        return 1;
    }
    callee = call->children[0];
    nameNode = callee;
    if ( callee != NULL && callee->kind == XL6_AST_GENERIC_INSTANT ) {
        nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
    }
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || nameNode->text == NULL ) {
        return 1;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, nameNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_CLASS ) {
        return xl6SemCheckClassCtorCall(ctx, call, typeInfo->decl);
    }
    return 1;
}

static XL6_AstNode* xl6SemBaseClassDecl(XL6_SemContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;

    if ( classNode == NULL || classNode->childCount == 0 ) {
        return NULL;
    }
    base = classNode->children[0];
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    return xl6SemFindTypeDecl(ctx, base->text);
}

static int xl6SemClassHasCreate(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             (member->kind == XL6_AST_CREATE || member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL) &&
             xl6TextEquals(xl6SemMemberName(member), "create") ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckBaseCtorCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* baseDecl;

    if ( !ctx->inClassCreate || ctx->currentClass == NULL ) {
        xl6SemErrorNode(ctx, call, "base(...) 只能在 class 的 create 体内使用");
        return 0;
    }

    baseDecl = xl6SemBaseClassDecl(ctx, ctx->currentClass);
    if ( baseDecl == NULL || baseDecl->kind != XL6_AST_CLASS ) {
        xl6SemErrorNode(ctx, call, "当前 class 没有父类，不能调用 base(...)");
        return 0;
    }

    if ( xl6SemResolveCreateMember(ctx, call, baseDecl, XL6_AST_CREATE) != NULL ) {
        return 1;
    }
    if ( xl6SemClassHasCreate(baseDecl) ) {
        return 0;
    }
    if ( xl6SemCallArgCount(call) == 0 ) {
        return 1;
    }

    xl6SemErrorNode(ctx, call, "父类没有 create，base(...) 不能传参");
    return 0;
}

static XL6_AstNode* xl6SemResolveClassFuncExMode(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const XL6_AstNode* concreteType,
    const char* name,
    int requireStatic,
    int report)
{
    XL6_AstNode* body;
    XL6_AstNode* fixedSelected;
    XL6_AstNode* varargSelected;
    XL6_AstNode* baseDecl;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;
    size_t i;

    if ( ctx == NULL || classNode == NULL || classNode->kind != XL6_AST_CLASS || name == NULL ) {
        return NULL;
    }
    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    fixedSelected = NULL;
    varargSelected = NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;

    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            XL6_AstNode* params;
            int cost;

            if ( member == NULL ||
                 (member->kind != XL6_AST_FUNC &&
                  member->kind != XL6_AST_EXPORT_FUNC &&
                  member->kind != XL6_AST_NATIVE_EXTERN_FUNC) ||
                 !xl6TextEquals(xl6SemMemberName(member), name) ) {
                continue;
            }
            if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) != requireStatic ) {
                continue;
            }
            if ( !xl6SemCanAccessClassMember(ctx, classNode, member) ) {
                continue;
            }
            if ( !xl6SemGenericClassCallableCandidateMatches(
                    ctx,
                    call,
                    member,
                    classNode,
                    concreteType,
                    &cost) ) {
                continue;
            }
            params = (XL6_AstNode*)xl6SemCallableParams(member);
            if ( xl6SemFuncHasVararg(params) ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargSelected = member;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedSelected = member;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedSelected;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6SemErrorNode(ctx, call, "class 方法调用存在重载歧义");
        }
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( varargBestCount > 1 ) {
        if ( report ) {
            xl6SemErrorNode(ctx, call, "class 方法调用存在重载歧义");
        }
        return NULL;
    }

    baseDecl = xl6SemBaseClassDecl(ctx, classNode);
    if ( baseDecl != NULL && baseDecl->kind == XL6_AST_CLASS ) {
        return xl6SemResolveClassFuncExMode(ctx, call, baseDecl, NULL, name, requireStatic, report);
    }
    return NULL;
}

static XL6_AstNode* xl6SemResolveClassFuncEx(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const char* name,
    int report)
{
    return xl6SemResolveClassFuncExMode(ctx, call, classNode, NULL, name, 0, report);
}

static XL6_AstNode* xl6SemResolveClassFuncForTypeEx(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const XL6_AstNode* concreteType,
    const char* name,
    int report)
{
    return xl6SemResolveClassFuncExMode(ctx, call, classNode, concreteType, name, 0, report);
}

static XL6_AstNode* xl6SemResolveStaticClassFuncEx(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const char* name,
    int report)
{
    return xl6SemResolveClassFuncExMode(ctx, call, classNode, NULL, name, 1, report);
}

static int xl6SemGenericClassPropertyCandidateMatches(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* member,
    const XL6_AstNode* classNode,
    const XL6_AstNode* concreteType,
    int* outCost)
{
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    int fixedCount;
    int actualCount;
    int requiredCount;
    int cost;
    int i;

    typeArgs = concreteType != NULL ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
    if ( typeArgs == NULL || !xl6SemClassHasTypeParams(classNode) ) {
        return xl6SemCallCandidateMatches(ctx, call, member, outCost);
    }

    params = xl6SemCallableParams(member);
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    actualCount = args != NULL ? (int)args->childCount : 0;
    requiredCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param == NULL || param->childCount < 2 ) {
            requiredCount += 1;
        }
    }
    if ( actualCount < requiredCount || actualCount > fixedCount ) {
        return 0;
    }

    cost = 0;
    for ( i = 0; i < actualCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* argExpr;
        const XL6_AstNode* param;
        const XL6_AstNode* paramType;
        XL6_AstNode* concreteParamType;
        int argCost;

        /* 属性维度参数不支持命名参数，合成调用也始终是位置参数。 */
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            return 0;
        }
        argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
        param = params->children[i];
        paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        concreteParamType = xl6SemSubstituteClassType(classNode, typeArgs, paramType);
        argCost = xl6SemExprParamMatchCost(
            ctx,
            argExpr,
            concreteParamType != NULL ? concreteParamType : paramType);
        xl6AstFree(concreteParamType);
        if ( argCost < 0 ) {
            return 0;
        }
        cost += argCost;
    }
    cost += (fixedCount - actualCount) * 100;
    if ( outCost != NULL ) {
        *outCost = cost;
    }
    return 1;
}

static XL6_AstNode* xl6SemResolveClassPropertyEx(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* classNode,
    const XL6_AstNode* concreteType,
    const char* name,
    XL6_AstKind kind,
    int report)
{
    XL6_AstNode* body;
    XL6_AstNode* fixedSelected;
    XL6_AstNode* baseDecl;
    int fixedBestCount;
    int fixedBestCost;
    size_t i;

    if ( ctx == NULL || classNode == NULL || classNode->kind != XL6_AST_CLASS ||
         name == NULL || (kind != XL6_AST_GET && kind != XL6_AST_LET) ) {
        return NULL;
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    fixedSelected = NULL;
    fixedBestCount = 0;
    fixedBestCost = 0;

    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            XL6_AstNode* params;
            int cost;

            if ( member == NULL || member->kind != kind ||
                 !xl6TextEquals(xl6SemMemberName(member), name) ) {
                continue;
            }
            if ( !xl6SemCanAccessClassMember(ctx, classNode, member) ) {
                continue;
            }
            params = (XL6_AstNode*)xl6SemCallableParams(member);
            if ( xl6SemFuncHasVararg(params) ||
                 !xl6SemGenericClassPropertyCandidateMatches(
                     ctx,
                     call,
                     member,
                     classNode,
                     concreteType,
                     &cost) ) {
                continue;
            }
            if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                fixedSelected = member;
                fixedBestCost = cost;
                fixedBestCount = 1;
            } else if ( cost == fixedBestCost ) {
                fixedBestCount += 1;
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedSelected;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6SemErrorNode(ctx, call, "class property overload is ambiguous");
        }
        return NULL;
    }

    baseDecl = xl6SemBaseClassDecl(ctx, classNode);
    if ( baseDecl != NULL && baseDecl->kind == XL6_AST_CLASS ) {
        return xl6SemResolveClassPropertyEx(ctx, call, baseDecl, NULL, name, kind, report);
    }
    return NULL;
}

static XL6_AstNode* xl6SemCloneMemberReceiverType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;

    if ( ctx == NULL || member == NULL || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL ) {
        return NULL;
    }
    if ( receiver->kind == XL6_AST_NAME ) {
        if ( xl6TextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
            return xl6AstNewText(XL6_AST_TYPE, ctx->currentClass->text, strlen(ctx->currentClass->text), NULL);
        }
        sym = xl6ScopeFind(ctx->scope, receiver->text);
        return sym != NULL && sym->type != NULL ? xl6SemCloneTypeNode(sym->type) : NULL;
    }
    if ( receiver->kind == XL6_AST_MEMBER ) {
        return xl6SemCloneMemberFieldType(ctx, receiver);
    }
    return xl6SemInferAutoTypeNode(ctx, receiver);
}

static XL6_AstNode* xl6SemCloneClassCallableReturnType(
    XL6_SemContext* ctx,
    const XL6_AstNode* ownerType,
    XL6_AstNode* classNode,
    XL6_AstNode* callable)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* typeArgs;

    (void)ctx;
    if ( classNode == NULL || callable == NULL ) {
        return NULL;
    }
    returns = xl6SemCallableReturns(callable);
    if ( returns == NULL || returns->childCount != 1 ) {
        return NULL;
    }
    typeArgs = ownerType != NULL ? xl6SemTypeNodeGenericArgs(ownerType) : NULL;
    if ( typeArgs != NULL && xl6SemClassHasTypeParams(classNode) ) {
        return xl6SemSubstituteClassType(classNode, typeArgs, returns->children[0]);
    }
    return xl6SemCloneTypeNode(returns->children[0]);
}

static XL6_AstNode* xl6SemClonePropertyGetterType(
    XL6_SemContext* ctx,
    XL6_AstNode* member,
    XL6_AstNode** args,
    size_t argCount,
    int report)
{
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    XL6_AstNode* call;
    XL6_AstNode* getter;
    XL6_AstNode* result;
    size_t i;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }

    receiverType = xl6SemCloneMemberReceiverType(ctx, member);
    concreteType = xl6SemClassValueType(ctx, receiverType);
    classNode = concreteType != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS ||
         xl6SemFindClassFieldNode(ctx, classNode, member->text, NULL) != NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    call = xl6SemProtocolTempCallNew(member);
    if ( call == NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    for ( i = 0; i < argCount; ++i ) {
        if ( !xl6SemProtocolTempCallAddArg(call, args != NULL ? args[i] : NULL) ) {
            xl6SemProtocolTempCallFree(call);
            xl6AstFree(receiverType);
            return NULL;
        }
    }

    getter = xl6SemResolveClassPropertyEx(ctx, call, classNode, concreteType, member->text, XL6_AST_GET, report);
    result = xl6SemCloneClassCallableReturnType(ctx, concreteType, classNode, getter);
    xl6SemProtocolTempCallFree(call);
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6SemCloneClassMemberCallReturnType(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    XL6_AstNode* func;
    XL6_AstNode* result;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }

    receiverType = xl6SemCloneMemberReceiverType(ctx, member);
    concreteType = xl6SemClassValueType(ctx, receiverType);
    classNode = concreteType != NULL && concreteType->text != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    func = xl6SemResolveClassFuncForTypeEx(ctx, call, classNode, concreteType, member->text, 0);
    result = xl6SemCloneClassCallableReturnType(ctx, concreteType, classNode, func);
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6SemClonePropertyIndexGetterType(XL6_SemContext* ctx, XL6_AstNode* indexExpr, int report)
{
    XL6_AstNode* receiver;
    XL6_AstNode** args;
    XL6_AstNode* result;
    size_t argCount;
    size_t i;

    if ( indexExpr == NULL ||
         (indexExpr->kind != XL6_AST_INDEX && indexExpr->kind != XL6_AST_SAFE_INDEX) ||
         indexExpr->childCount == 0 ) {
        return NULL;
    }
    receiver = indexExpr->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    argCount = xl6SemIndexArgCount(indexExpr);
    args = argCount > 0 ? (XL6_AstNode**)xrtCalloc(argCount, sizeof(XL6_AstNode*)) : NULL;
    if ( argCount > 0 && args == NULL ) {
        return NULL;
    }
    for ( i = 0; i < argCount; ++i ) {
        args[i] = xl6SemIndexArgAt(indexExpr, i);
    }
    result = xl6SemClonePropertyGetterType(ctx, receiver, args, argCount, report);
    xrtFree(args);
    return result;
}

static XL6_AstNode* xl6SemClonePropertySetterValueType(
    XL6_SemContext* ctx,
    XL6_AstNode* left,
    XL6_AstNode* valueExpr,
    int report)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    XL6_AstNode* call;
    XL6_AstNode* setter;
    const XL6_AstNode* params;
    const XL6_AstNode* valueParam;
    XL6_AstNode* result;
    size_t argCount;
    size_t i;

    if ( left == NULL || valueExpr == NULL ) {
        return NULL;
    }
    if ( left->kind == XL6_AST_MEMBER ) {
        member = left;
        argCount = 0;
    } else if ( left->kind == XL6_AST_INDEX && left->childCount > 0 &&
                left->children[0] != NULL && left->children[0]->kind == XL6_AST_MEMBER ) {
        member = left->children[0];
        argCount = xl6SemIndexArgCount(left);
    } else {
        return NULL;
    }

    receiverType = xl6SemCloneMemberReceiverType(ctx, member);
    concreteType = xl6SemClassValueType(ctx, receiverType);
    classNode = concreteType != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS ||
         xl6SemFindClassFieldNode(ctx, classNode, member->text, NULL) != NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    call = xl6SemProtocolTempCallNew(left);
    if ( call == NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    for ( i = 0; i < argCount; ++i ) {
        if ( !xl6SemProtocolTempCallAddArg(call, xl6SemIndexArgAt(left, i)) ) {
            xl6SemProtocolTempCallFree(call);
            xl6AstFree(receiverType);
            return NULL;
        }
    }
    if ( !xl6SemProtocolTempCallAddArg(call, valueExpr) ) {
        xl6SemProtocolTempCallFree(call);
        xl6AstFree(receiverType);
        return NULL;
    }

    setter = xl6SemResolveClassPropertyEx(ctx, call, classNode, concreteType, member->text, XL6_AST_LET, report);
    params = setter != NULL ? xl6SemCallableParams(setter) : NULL;
    valueParam = params != NULL && params->childCount > 0 ? params->children[params->childCount - 1] : NULL;
    result = NULL;
    if ( valueParam != NULL && valueParam->childCount > 0 ) {
        const XL6_AstNode* typeArgs = concreteType != NULL ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
        if ( typeArgs != NULL && xl6SemClassHasTypeParams(classNode) ) {
            result = xl6SemSubstituteClassType(classNode, typeArgs, valueParam->children[0]);
        } else {
            result = xl6SemCloneTypeNode(valueParam->children[0]);
        }
    }

    xl6SemProtocolTempCallFree(call);
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemClassHasPropertyName(XL6_SemContext* ctx, XL6_AstNode* classNode, const char* name, int kind)
{
    XL6_AstNode* body;
    XL6_AstNode* baseDecl;
    size_t i;

    if ( ctx == NULL || classNode == NULL || classNode->kind != XL6_AST_CLASS || name == NULL ) {
        return 0;
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( (member->kind == XL6_AST_GET || member->kind == XL6_AST_LET) &&
             (kind == 0 || member->kind == (XL6_AstKind)kind) &&
             xl6TextEquals(xl6SemMemberName(member), name) &&
             xl6SemCanAccessClassMember(ctx, classNode, member) ) {
            return 1;
        }
    }

    baseDecl = xl6SemBaseClassDecl(ctx, classNode);
    return baseDecl != NULL && baseDecl->kind == XL6_AST_CLASS ?
        xl6SemClassHasPropertyName(ctx, baseDecl, name, kind) :
        0;
}

static int xl6SemLeftNamesClassProperty(XL6_SemContext* ctx, XL6_AstNode* left)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    int result;

    if ( left == NULL ) {
        return 0;
    }
    if ( left->kind == XL6_AST_MEMBER ) {
        member = left;
    } else if ( left->kind == XL6_AST_INDEX && left->childCount > 0 &&
                left->children[0] != NULL && left->children[0]->kind == XL6_AST_MEMBER ) {
        member = left->children[0];
    } else {
        return 0;
    }
    if ( member == NULL || member->text == NULL ) {
        return 0;
    }

    receiverType = xl6SemCloneMemberReceiverType(ctx, member);
    concreteType = xl6SemClassValueType(ctx, receiverType);
    classNode = concreteType != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    result = classNode != NULL && classNode->kind == XL6_AST_CLASS &&
             xl6SemFindClassFieldNode(ctx, classNode, member->text, NULL) == NULL &&
             xl6SemClassHasPropertyName(ctx, classNode, member->text, 0);
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemCheckPropertyAssignTarget(XL6_SemContext* ctx, XL6_AstNode* left, XL6_AstNode* right, const char* op)
{
    XL6_AstNode* member;
    XL6_AstNode* getterType;
    XL6_AstNode* targetType;
    size_t i;
    int ok;

    if ( !xl6SemLeftNamesClassProperty(ctx, left) ) {
        return -1;
    }

    if ( xl6TextEquals(op, "?=") ) {
        xl6SemErrorNode(ctx, left, "class property does not support ?=");
        return 0;
    }

    getterType = NULL;
    if ( !xl6TextEquals(op, "=") ) {
        getterType = left != NULL && left->kind == XL6_AST_INDEX ?
            xl6SemClonePropertyIndexGetterType(ctx, left, 1) :
            xl6SemClonePropertyGetterType(ctx, left, NULL, 0, 1);
        if ( getterType == NULL ) {
            xl6SemErrorNode(ctx, left, "class property compound assignment requires matching get");
            return 0;
        }
    }

    /*
        普通赋值按右值选择 let 重载；复合赋值先读取属性，运算结果保持 getter 类型，
        因而应使用属性本身选择 let，不能错误地按右操作数类型选择重载。
    */
    targetType = xl6SemClonePropertySetterValueType(
        ctx,
        left,
        getterType != NULL ? left : right,
        1);
    if ( targetType == NULL ) {
        xl6SemErrorNode(
            ctx,
            left,
            getterType != NULL ?
                "class property compound assignment requires matching let" :
                "class property assignment requires matching let");
        xl6AstFree(getterType);
        return 0;
    }
    ok = 1;

    /*
        属性赋值不会把属性名本身当作 getter 表达式检查，否则只写属性将无法成立。
        这里单独检查接收者和全部维度参数，并为索引左值记录 setter 的值类型。
    */
    member = left->kind == XL6_AST_INDEX && left->childCount > 0 ? left->children[0] : left;
    if ( member != NULL && member->kind == XL6_AST_MEMBER && member->childCount > 0 &&
         !xl6SemCheckExpr(ctx, member->children[0]) ) {
        ok = 0;
    }
    if ( left->kind == XL6_AST_INDEX ) {
        for ( i = 0; i < xl6SemIndexArgCount(left); ++i ) {
            XL6_AstNode* arg = xl6SemIndexArgAt(left, i);
            if ( arg != NULL && !xl6SemCheckExpr(ctx, arg) ) {
                ok = 0;
            }
        }
        if ( left->inferredType == NULL ) {
            left->inferredType = xl6SemCloneTypeNode(targetType);
        }
    }
    if ( getterType != NULL && !xl6SemExprCanMatchParamType(ctx, left, targetType) ) {
        xl6SemTypeMismatchError(
            ctx,
            left,
            "property compound result type does not match setter value type",
            targetType,
            getterType->text);
        ok = 0;
    } else if ( getterType == NULL && right != NULL && !xl6SemExprCanMatchParamType(ctx, right, targetType) ) {
        xl6SemTypeMismatchError(
            ctx,
            right,
            "property assignment expression type does not match setter value type",
            targetType,
            xl6SemExprStaticTypeName(ctx, right));
        ok = 0;
    }
    xl6AstFree(getterType);
    xl6AstFree(targetType);
    return ok;
}

static size_t xl6SemIndexArgCount(const XL6_AstNode* node)
{
    const XL6_AstNode* payload;

    if ( node == NULL ||
         (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) ||
         node->childCount < 2 ) {
        return 0;
    }
    payload = node->children[1];
    if ( payload == NULL ) {
        return 0;
    }
    return payload->kind == XL6_AST_LIST ? payload->childCount : 1;
}

static XL6_AstNode* xl6SemIndexArgAt(XL6_AstNode* node, size_t index)
{
    XL6_AstNode* payload;

    if ( node == NULL ||
         (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) ||
         node->childCount < 2 ) {
        return NULL;
    }
    payload = node->children[1];
    if ( payload == NULL ) {
        return NULL;
    }
    if ( payload->kind == XL6_AST_LIST ) {
        return index < payload->childCount ? payload->children[index] : NULL;
    }
    return index == 0 ? payload : NULL;
}

static XL6_AstNode* xl6SemProtocolTempCallNew(const XL6_AstNode* atNode)
{
    XL6_AstNode* call;
    XL6_AstNode* callee;
    XL6_AstNode* args;

    call = xl6AstNew(XL6_AST_CALL, NULL);
    callee = xl6AstNewText(XL6_AST_NAME, "__protocol__", 12, NULL);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( call == NULL || callee == NULL || args == NULL ) {
        xl6AstFree(call);
        xl6AstFree(callee);
        xl6AstFree(args);
        return NULL;
    }
    if ( atNode != NULL ) {
        call->line = atNode->line;
        call->column = atNode->column;
    }
    xl6AstAddChild(call, callee);
    xl6AstAddChild(call, args);
    return call;
}

static int xl6SemProtocolTempCallAddArg(XL6_AstNode* call, XL6_AstNode* expr)
{
    XL6_AstNode* args;
    XL6_AstNode* arg;

    if ( call == NULL || call->childCount < 2 || expr == NULL ) {
        return 0;
    }
    args = call->children[1];
    arg = xl6AstNew(XL6_AST_ARG, NULL);
    if ( arg == NULL ) {
        return 0;
    }
    arg->line = expr->line;
    arg->column = expr->column;
    xl6AstAddChild(arg, expr);
    xl6AstAddChild(args, arg);
    return 1;
}

static void xl6SemProtocolTempCallFree(XL6_AstNode* call)
{
    XL6_AstNode* args;
    size_t i;

    if ( call == NULL ) {
        return;
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->childCount > 0 ) {
                arg->children[0] = NULL;
            }
        }
    }
    xl6AstFree(call);
}

static const XL6_AstNode* xl6SemFindProtocolMethodForExprCall(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* protocolName,
    XL6_AstNode* call,
    int report)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* classType;
    XL6_AstNode* decl;
    XL6_AstNode* method;

    if ( ctx == NULL || expr == NULL || protocolName == NULL || call == NULL ) {
        return NULL;
    }

    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    classType = xl6SemClassValueType(ctx, concreteType);
    decl = classType != NULL && classType->text != NULL ? xl6SemFindTypeDecl(ctx, classType->text) : NULL;
    method = decl != NULL && decl->kind == XL6_AST_CLASS ?
             xl6SemResolveClassFuncForTypeEx(ctx, call, decl, classType, protocolName, report) :
             NULL;
    if ( method != NULL && classType != NULL && xl6SemClassHasTypeParams(decl) ) {
        const XL6_AstNode* typeArgs = xl6SemTypeNodeGenericArgs(classType);
        XL6_AstNode* concreteMethod = typeArgs != NULL ?
            xl6SemSubstituteClassType(decl, typeArgs, method) : NULL;

        if ( concreteMethod != NULL ) {
            if ( xl6SemOwnTypeNode(ctx, concreteMethod) ) {
                method = concreteMethod;
            } else {
                xl6AstFree(concreteMethod);
                method = NULL;
            }
        }
    }
    xl6AstFree(typeNode);
    return method;
}

static const XL6_AstNode* xl6SemFindProtocolMethodForExprArgs(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* protocolName,
    XL6_AstNode** args,
    size_t argCount,
    int report)
{
    XL6_AstNode* call;
    const XL6_AstNode* method;
    size_t i;

    call = xl6SemProtocolTempCallNew(expr);
    if ( call == NULL ) {
        return NULL;
    }
    for ( i = 0; i < argCount; ++i ) {
        if ( args[i] != NULL && !xl6SemProtocolTempCallAddArg(call, args[i]) ) {
            xl6SemProtocolTempCallFree(call);
            return NULL;
        }
    }

    method = xl6SemFindProtocolMethodForExprCall(ctx, expr, protocolName, call, report);
    xl6SemProtocolTempCallFree(call);
    return method;
}

static const XL6_AstNode* xl6SemFindProtocolMethodForIndex(
    XL6_SemContext* ctx,
    XL6_AstNode* indexNode,
    const char* protocolName,
    XL6_AstNode* valueExpr,
    int report)
{
    XL6_AstNode* receiver;
    XL6_AstNode* call;
    const XL6_AstNode* method;
    size_t i;

    if ( indexNode == NULL ||
         (indexNode->kind != XL6_AST_INDEX && indexNode->kind != XL6_AST_SAFE_INDEX) ||
         indexNode->childCount < 2 ) {
        return NULL;
    }
    receiver = indexNode->children[0];
    call = xl6SemProtocolTempCallNew(indexNode);
    if ( call == NULL ) {
        return NULL;
    }
    for ( i = 0; i < xl6SemIndexArgCount(indexNode); ++i ) {
        if ( !xl6SemProtocolTempCallAddArg(call, xl6SemIndexArgAt(indexNode, i)) ) {
            xl6SemProtocolTempCallFree(call);
            return NULL;
        }
    }
    if ( valueExpr != NULL && !xl6SemProtocolTempCallAddArg(call, valueExpr) ) {
        xl6SemProtocolTempCallFree(call);
        return NULL;
    }

    method = xl6SemFindProtocolMethodForExprCall(ctx, receiver, protocolName, call, report);
    xl6SemProtocolTempCallFree(call);
    return method;
}

/*
    为已经选中的泛型函数重新构造本次调用的类型实参。
    重载解析只负责选中候选，不应让后续返回类型继续携带未替换的 T。
*/
static XL6_AstNode* xl6SemInferCallTypeArgs(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* func)
{
    const XL6_AstNode* typeParams;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    XL6_AstNode** inferred;
    XL6_AstNode* result;
    int fixedCount;
    int positionalIndex;
    size_t i;

    typeParams = xl6SemFuncTypeParams(func);
    params = xl6SemCallableParams(func);
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( typeParams == NULL || typeParams->childCount == 0 || params == NULL ) {
        return NULL;
    }

    inferred = (XL6_AstNode**)xrtCalloc(typeParams->childCount, sizeof(XL6_AstNode*));
    if ( inferred == NULL ) {
        return NULL;
    }
    fixedCount = xl6SemFixedParamCount((XL6_AstNode*)params);
    positionalIndex = 0;
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* argExpr;
        const XL6_AstNode* param;
        const XL6_AstNode* paramType;
        XL6_AstNode* exprType;
        int paramIndex;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            paramIndex = xl6SemFindParamIndex((XL6_AstNode*)params, fixedCount, arg->text);
        } else {
            paramIndex = positionalIndex++;
        }
        if ( paramIndex < 0 || paramIndex >= fixedCount ) {
            continue;
        }
        argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
        param = params->children[paramIndex];
        paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        if ( !xl6SemTypeContainsTypeParam(typeParams, paramType) ) {
            continue;
        }
        exprType = xl6SemInferAutoTypeNode(ctx, argExpr);
        if ( exprType == NULL ||
             !xl6SemInferGenericTypeFromTypes(ctx, typeParams, inferred, paramType, exprType) ) {
            xl6AstFree(exprType);
            goto fail;
        }
        xl6AstFree(exprType);
    }

    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        goto fail;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        size_t oldCount;

        if ( inferred[i] == NULL ) {
            xl6AstFree(result);
            goto fail;
        }
        oldCount = result->childCount;
        xl6AstAddChild(result, inferred[i]);
        if ( result->childCount == oldCount ) {
            xl6AstFree(result);
            goto fail;
        }
        inferred[i] = NULL;
    }
    xrtFree(inferred);
    return result;

fail:
    for ( i = 0; i < typeParams->childCount; ++i ) {
        xl6AstFree(inferred[i]);
    }
    xrtFree(inferred);
    return NULL;
}

static XL6_AstNode* xl6SemCloneConcreteReturnTypes(
    const XL6_AstNode* returns,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classTypeArgs,
    const XL6_AstNode* func,
    const XL6_AstNode* funcTypeArgs)
{
    const XL6_AstNode* funcTypeParams;
    XL6_AstNode* result;
    size_t i;

    if ( returns == NULL ) {
        return NULL;
    }
    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        return NULL;
    }
    funcTypeParams = xl6SemFuncTypeParams(func);
    for ( i = 0; i < returns->childCount; ++i ) {
        XL6_AstNode* concrete;
        XL6_AstNode* finalType;

        if ( classNode != NULL && classTypeArgs != NULL && xl6SemClassHasTypeParams(classNode) ) {
            concrete = xl6SemSubstituteClassType(classNode, classTypeArgs, returns->children[i]);
        } else {
            concrete = xl6SemCloneTypeNode(returns->children[i]);
        }
        if ( concrete == NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        if ( funcTypeParams != NULL && funcTypeArgs != NULL ) {
            finalType = xl6SemSubstituteGenericTypeArgs(funcTypeParams, funcTypeArgs, concrete);
            xl6AstFree(concrete);
            concrete = finalType;
        }
        if ( concrete == NULL ) {
            xl6AstFree(concrete);
            xl6AstFree(result);
            return NULL;
        }
        {
            size_t oldCount = result->childCount;
            xl6AstAddChild(result, concrete);
            if ( result->childCount == oldCount ) {
                xl6AstFree(concrete);
                xl6AstFree(result);
                return NULL;
            }
        }
    }
    return result;
}

static XL6_AstNode* xl6SemCallReturnTypesEx(XL6_SemContext* ctx, XL6_AstNode* expr, XL6_AstNode** ownedType)
{
    XL6_AstNode* callee;
    XL6_AstNode* func;
    XL6_Symbol* sym;

    if ( ownedType != NULL ) {
        *ownedType = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL ) {
        return NULL;
    }
    if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
        XL6_AstNode* nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        XL6_AstNode* typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;

        if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || ownedType == NULL ) {
            return NULL;
        }
        func = xl6SemResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
        if ( func != NULL && func->childCount > 2 ) {
            *ownedType = xl6SemCloneConcreteReturnTypes(func->children[2], NULL, NULL, func, typeArgs);
            return *ownedType;
        }
        return NULL;
    }
    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* staticMethod = xl6SemResolveStaticClassFuncCall(ctx, expr, 0);
        if ( staticMethod != NULL && staticMethod->childCount > 2 ) {
            return staticMethod->children[2];
        }
        XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, callee);
        if ( memberType != NULL && xl6TextEquals(memberType->text, "function") && memberType->childCount > 1 ) {
            if ( ownedType != NULL ) {
                *ownedType = memberType;
                return memberType->children[1];
            }
        }
        xl6AstFree(memberType);
        if ( callee->childCount > 0 ) {
            XL6_AstNode* receiver = callee->children[0];
            XL6_AstNode* receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
            const XL6_AstNode* concreteType = xl6SemClassValueType(ctx, receiverType);
            XL6_AstNode* classDecl = concreteType != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
            XL6_AstNode* method = NULL;

            if ( classDecl != NULL && classDecl->kind == XL6_AST_CLASS ) {
                method = xl6SemResolveClassFuncForTypeEx(ctx, expr, classDecl, concreteType, callee->text, 0);
            }
            if ( method != NULL && method->childCount > 2 ) {
                const XL6_AstNode* classTypeArgs = xl6SemTypeNodeGenericArgs(concreteType);
                XL6_AstNode* methodTypeArgs = xl6SemInferCallTypeArgs(ctx, expr, method);

                if ( ownedType != NULL &&
                     ((classTypeArgs != NULL && xl6SemClassHasTypeParams(classDecl)) || methodTypeArgs != NULL) ) {
                    *ownedType = xl6SemCloneConcreteReturnTypes(
                        method->children[2],
                        classDecl,
                        classTypeArgs,
                        method,
                        methodTypeArgs);
                    xl6AstFree(methodTypeArgs);
                    xl6AstFree(receiverType);
                    return *ownedType;
                }
                xl6AstFree(methodTypeArgs);
                xl6AstFree(receiverType);
                return method->children[2];
            }
            xl6AstFree(receiverType);
        }
        return NULL;
    }
    if ( callee->kind != XL6_AST_NAME ) {
        return NULL;
    }

    sym = xl6ScopeFind(ctx->scope, callee->text);
    if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") &&
         sym->type->childCount > 1 ) {
        return sym->type->children[1];
    }

    func = xl6SemResolveCallFuncEx(ctx, expr, callee->text, 0);
    if ( func != NULL && xl6SemFuncIsGeneric(func) && ownedType != NULL ) {
        XL6_AstNode* typeArgs = xl6SemInferCallTypeArgs(ctx, expr, func);
        if ( typeArgs != NULL ) {
            *ownedType = xl6SemCloneConcreteReturnTypes(func->children[2], NULL, NULL, func, typeArgs);
            xl6AstFree(typeArgs);
            return *ownedType;
        }
    }
    return func != NULL && func->childCount > 2 ? func->children[2] : NULL;
}

static XL6_AstNode* xl6SemCallReturnTypes(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    return xl6SemCallReturnTypesEx(ctx, expr, NULL);
}

static int xl6SemExprIsMultiReturnCall(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* ownedType;
    XL6_AstNode* returns;
    int result;

    ownedType = NULL;
    returns = xl6SemCallReturnTypesEx(ctx, expr, &ownedType);
    result = returns != NULL && returns->childCount > 1;
    xl6AstFree(ownedType);
    return result;
}
static int xl6SemTypeEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( xl6SemTypeContainsAliasCycle(ctx, a) || xl6SemTypeContainsAliasCycle(ctx, b) ) {
        return 1;
    }
    a = xl6SemResolveAliasType(ctx, a);
    b = xl6SemResolveAliasType(ctx, b);
    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->kind != b->kind || a->childCount != b->childCount ) {
        return 0;
    }
    if ( (a->text == NULL) != (b->text == NULL) ) {
        return 0;
    }
    if ( a->text != NULL && !xl6TextEquals(a->text, b->text) ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        if ( !xl6SemTypeEquals(ctx, a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemRuntimeTypeTargetIsInvalid(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int isCast)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6TextEquals(name, "auto") || xl6TextEquals(name, "var") ) {
        return 1;
    }
    if ( isCast && xl6TextEquals(name, "null") ) {
        return 1;
    }
    return 0;
}

static int xl6SemRuntimeCastTargetSupported(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_TypeInfo* typeInfo;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( name == NULL ) {
        return 0;
    }
    if ( xl6SemClassHandleTargetType(ctx, concreteType) != NULL ) {
        return 1;
    }

    if ( xl6SemIsBoolTypeName(name) ||
         xl6SemIsIntegerTypeName(name) ||
         xl6SemIsFloatTypeName(name) ||
         xl6TextEquals(name, "string") ||
         xl6TextEquals(name, "time") ||
         xl6TextEquals(name, "point") ||
         xl6TextEquals(name, "type") ) {
        return concreteType->childCount == 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, name);
    return typeInfo != NULL &&
            (typeInfo->kind == XL6_TYPE_CLASS ||
             typeInfo->kind == XL6_TYPE_UNION);
}

static int xl6SemCheckRuntimeTypeExpr(XL6_SemContext* ctx, XL6_AstNode* node, int isCast)
{
    XL6_AstNode* value;
    XL6_AstNode* target;
    XL6_AstNode* sourceType;
    const XL6_AstNode* concreteSourceType;
    int ok;

    if ( node == NULL || node->childCount < 2 ) {
        return 1;
    }

    value = node->children[0];
    target = node->children[1];
    ok = 1;

    if ( !xl6SemCheckExpr(ctx, value) ) {
        ok = 0;
    }
    if ( !xl6SemCheckType(ctx, target) ) {
        ok = 0;
    }
    if ( xl6SemRuntimeTypeTargetIsInvalid(ctx, target, isCast) ) {
        xl6SemErrorNode(ctx, target, isCast ? "as 目标类型不能是 auto/var/null" : "is 目标类型不能是 auto/var");
        ok = 0;
    }

    if ( !isCast ) {
        return ok;
    }

    if ( !xl6SemRuntimeCastTargetSupported(ctx, target) ) {
        xl6SemErrorNode(ctx, target, "as 当前只支持基础值、class、native-backed class 和 union 目标类型");
        ok = 0;
    }

    sourceType = xl6SemInferAutoTypeNode(ctx, value);
    concreteSourceType = xl6SemResolveAliasType(ctx, sourceType);
    if ( xl6SemClassHandleTargetType(ctx, target) != NULL ) {
        const XL6_AstNode* sourceRefTarget = xl6SemClassHandleTargetType(ctx, concreteSourceType);
        const XL6_AstNode* targetRefTarget = xl6SemClassHandleTargetType(ctx, target);

        if ( concreteSourceType != NULL && xl6TextEquals(concreteSourceType->text, "var") ) {
            /* var 的实际 class 类型只能在运行时校验。 */
        } else if ( sourceRefTarget == NULL || targetRefTarget == NULL ||
             (!xl6SemTypeIsOrExtendsClass(ctx, sourceRefTarget, targetRefTarget->text) &&
              !xl6SemTypeIsOrExtendsClass(ctx, targetRefTarget, sourceRefTarget->text)) ) {
            xl6SemErrorNode(ctx, value, "as class 的源和目标必须属于同一 class 继承链");
            ok = 0;
        }
    } else if ( concreteSourceType != NULL &&
         !xl6TextEquals(concreteSourceType->text, "var") &&
         !xl6SemTypeEquals(ctx, concreteSourceType, target) ) {
        xl6SemErrorNode(ctx, value, "as 的源表达式必须是 var，或者已经是目标类型");
        ok = 0;
    }
    xl6AstFree(sourceType);
    return ok;
}

static const XL6_AstNode* xl6SemCallableItemType(const XL6_AstNode* item)
{
    if ( item != NULL && item->kind == XL6_AST_PARAM && item->childCount > 0 ) {
        return item->children[0];
    }
    return item;
}

static int xl6SemCallableTypeListEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->childCount != b->childCount ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        if ( !xl6SemTypeEquals(ctx, xl6SemCallableItemType(a->children[i]), xl6SemCallableItemType(b->children[i])) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemCallableListLooksLikeParams(const XL6_AstNode* list)
{
    return list != NULL && list->childCount > 0 &&
           list->children[0] != NULL && list->children[0]->kind == XL6_AST_PARAM;
}

static int xl6SemCallableParamPrefixMatches(XL6_SemContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params, size_t count)
{
    size_t i;

    if ( (count > 0) && (expected == NULL || params == NULL) ) {
        return 0;
    }
    if ( expected != NULL && expected->childCount < count ) {
        return 0;
    }
    if ( params != NULL && params->childCount < count ) {
        return 0;
    }
    for ( i = 0; i < count; ++i ) {
        if ( !xl6SemTypeEquals(ctx,
                               xl6SemCallableItemType(expected->children[i]),
                               xl6SemCallableItemType(params->children[i])) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemCallableParamListMatchesFunctionParams(XL6_SemContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params)
{
    size_t expectedCount;
    size_t expectedFixed;
    int expectedHasArrayVararg;
    int expectedHasDictVararg;
    int actualHasVararg;
    int actualFixed;
    int actualRequired;

    /*
        func 值本身只保存签名，不保存默认值。
        只有真实函数参数表（XL6_AST_PARAM）才能按可选参数展开匹配。
    */
    if ( !xl6SemCallableListLooksLikeParams(params) ) {
        return xl6SemCallableTypeListEquals(ctx, expected, params);
    }

    expectedCount = expected != NULL ? expected->childCount : 0;
    expectedHasArrayVararg = expectedCount > 0 && xl6SemTypeIsArrayVararg(expected->children[expectedCount - 1]);
    expectedHasDictVararg = expectedCount > 0 && xl6SemTypeIsDictVararg(expected->children[expectedCount - 1]);
    expectedFixed = (expectedHasArrayVararg || expectedHasDictVararg) ? expectedCount - 1 : expectedCount;
    actualFixed = xl6SemFixedParamCount(params);
    actualRequired = xl6SemParamRequiredCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)actualFixed;

    if ( expectedHasArrayVararg || expectedHasDictVararg ) {
        const XL6_AstNode* actualVararg;

        if ( !actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
            return 0;
        }
        actualVararg = params->children[actualFixed];
        if ( expectedHasArrayVararg && !xl6SemParamIsArrayVararg(actualVararg) ) {
            return 0;
        }
        if ( expectedHasDictVararg && !xl6SemParamIsDictVararg(actualVararg) ) {
            return 0;
        }
        return xl6SemCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
    }

    if ( actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
        return 0;
    }
    return xl6SemCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
}

static int xl6SemFunctionTypeMatchesParts(
    XL6_SemContext* ctx,
    const XL6_AstNode* functionType,
    const XL6_AstNode* params,
    const XL6_AstNode* returns)
{
    const XL6_AstNode* expectedParams;
    const XL6_AstNode* expectedReturns;

    if ( functionType == NULL || !xl6TextEquals(functionType->text, "function") ) {
        return 0;
    }
    if ( functionType->childCount == 0 ) {
        return 1;
    }
    if ( functionType->childCount < 2 ) {
        return 0;
    }
    expectedParams = functionType->children[0];
    expectedReturns = functionType->children[1];
    return xl6SemCallableParamListMatchesFunctionParams(ctx, expectedParams, params) &&
           xl6SemCallableTypeListEquals(ctx, expectedReturns, returns);
}

static XL6_AstNode* xl6SemFindFunctionValueFunc(XL6_SemContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode, int reportError)
{
    XL6_AstNode* selected;
    int candidateCount;
    int matchCount;
    size_t i;

    selected = NULL;
    candidateCount = 0;
    matchCount = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL || functionType == NULL ||
         !xl6TextEquals(functionType->text, "function") ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* func = ctx->root->children[i];
        if ( func == NULL ||
             (func->kind != XL6_AST_FUNC && func->kind != XL6_AST_ASYNC_FUNC &&
              func->kind != XL6_AST_EXTERN_FUNC && func->kind != XL6_AST_EXPORT_FUNC) ||
             xl6SemFuncIsGeneric(func) ||
             !xl6TextEquals(xl6SemFuncName(func), name) ) {
            continue;
        }
        candidateCount += 1;
        if ( functionType->childCount == 0 ||
             xl6SemFunctionTypeMatchesParts(ctx,
                                             functionType,
                                             func->childCount > 1 ? func->children[1] : NULL,
                                             func->childCount > 2 ? func->children[2] : NULL) ) {
            selected = func;
            matchCount += 1;
        }
    }

    if ( matchCount == 1 ) {
        return selected;
    }
    if ( reportError ) {
        if ( candidateCount == 0 ) {
            xl6SemErrorNode(ctx, atNode, "找不到可作为 func 值的具名函数");
        } else if ( functionType->childCount == 0 ) {
            xl6SemErrorNode(ctx, atNode, "func 目标无法从重载函数名选择唯一候选");
        } else if ( matchCount == 0 ) {
            xl6SemErrorNode(ctx, atNode, "找不到和 func 目标签名匹配的具名函数");
        } else {
            xl6SemErrorNode(ctx, atNode, "func 目标签名匹配到多个具名函数候选");
        }
    }
    return NULL;
}

static int xl6SemExprCanMatchFunctionTypeEx(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode, int reportError)
{
    XL6_Symbol* sym;
    XL6_AstNode* returns;

    if ( typeNode == NULL || !xl6TextEquals(typeNode->text, "function") ) {
        return 1;
    }
    if ( expr == NULL ) {
        return 1;
    }
    if ( expr->kind == XL6_AST_ANON_FUNC ) {
        return xl6SemFunctionTypeMatchesParts(ctx,
                                              typeNode,
                                              expr->childCount > 0 ? expr->children[0] : NULL,
                                              expr->childCount > 1 ? expr->children[1] : NULL);
    }
    if ( expr->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, expr->text);
        if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") ) {
            if ( typeNode->childCount == 0 ) {
                return 1;
            }
            return xl6SemFunctionTypeMatchesParts(ctx,
                                                   typeNode,
                                                   sym->type->childCount > 0 ? sym->type->children[0] : NULL,
                                                   sym->type->childCount > 1 ? sym->type->children[1] : NULL);
        }
        if ( sym != NULL && sym->kind == XL6_SYM_FUNC ) {
            return xl6SemFindFunctionValueFunc(ctx, expr->text, typeNode, expr, reportError) != NULL;
        }
    }
    if ( expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
         expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
        XL6_AstNode* func = xl6SemResolveCallFuncEx(ctx, (XL6_AstNode*)expr, expr->children[0]->text, 0);
        returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
        if ( returns != NULL && returns->childCount == 1 &&
             returns->children[0] != NULL && xl6TextEquals(returns->children[0]->text, "function") ) {
            if ( typeNode->childCount == 0 ) {
                return 1;
            }
            return xl6SemFunctionTypeMatchesParts(ctx,
                                                   typeNode,
                                                   returns->children[0]->childCount > 0 ? returns->children[0]->children[0] : NULL,
                                                   returns->children[0]->childCount > 1 ? returns->children[0]->children[1] : NULL);
        }
    }
    return 1;
}

static int xl6SemExprCanMatchFunctionType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    return xl6SemExprCanMatchFunctionTypeEx(ctx, expr, typeNode, 1);
}

static int xl6SemExprFunctionValueMatchAlreadyDiagnosed(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_Symbol* sym;

    if ( ctx == NULL || expr == NULL || typeNode == NULL ||
         !xl6TextEquals(typeNode->text, "function") ||
         expr->kind != XL6_AST_NAME ) {
        return 0;
    }

    sym = xl6ScopeFind(ctx->scope, expr->text);
    return sym != NULL && sym->kind == XL6_SYM_FUNC;
}

static int xl6SemParamRequiredCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6SemParamIsVararg(params->children[count]) ) {
        if ( params->children[count]->childCount > 1 ) {
            break;
        }
        ++count;
    }
    return count;
}

static int xl6SemFuncHasVararg(const XL6_AstNode* params)
{
    int fixedCount;

    fixedCount = xl6SemFixedParamCount(params);
    return params != NULL && params->childCount > (size_t)fixedCount;
}

static const XL6_AstNode* xl6SemCallableParams(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_CREATE ) {
        return node->childCount > 0 ? node->children[0] : NULL;
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6SemCallableReturns(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_CREATE || node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
        return NULL;
    }
    return node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6SemCallableReturnCount(const XL6_AstNode* node)
{
    const XL6_AstNode* returns;

    returns = xl6SemCallableReturns(node);
    return returns != NULL ? (int)returns->childCount : 0;
}

static const XL6_AstNode* xl6SemCallableParamTypeAt(const XL6_AstNode* params, int index)
{
    const XL6_AstNode* param;

    if ( params == NULL || index < 0 || (size_t)index >= params->childCount ) {
        return NULL;
    }
    param = params->children[index];
    return param != NULL && param->childCount > 0 ? param->children[0] : NULL;
}

static const XL6_AstNode* xl6SemCallableReturnTypeAt(const XL6_AstNode* node, int index)
{
    const XL6_AstNode* returns;

    returns = xl6SemCallableReturns(node);
    if ( returns == NULL || index < 0 || (size_t)index >= returns->childCount ) {
        return NULL;
    }
    return returns->children[index];
}

static int xl6SemTypeNameMatches(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* expected)
{
    if ( expected == NULL ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    return typeNode != NULL && typeNode->text != NULL && xl6TextEquals(typeNode->text, expected);
}

static int xl6SemCheckProtocolMember(XL6_SemContext* ctx, XL6_AstNode* member)
{
    const char* name;
    const XL6_ProtocolRule* rule;
    const XL6_AstNode* params;
    int fixedCount;
    int requiredCount;
    int returnCount;
    int hasVararg;
    int returnCountOk;
    char message[256];
    int ok;

    name = xl6SemMemberName(member);
    rule = xl6ProtocolFindRule(name);
    if ( rule == NULL ) {
        if ( xl6ProtocolLooksReservedName(name) ) {
            snprintf(message, sizeof(message), "未知协议方法名: %s", name);
            xl6SemErrorNode(ctx, member, message);
            return 0;
        }
        return 1;
    }

    params = xl6SemCallableParams(member);
    fixedCount = xl6SemFixedParamCount(params);
    requiredCount = xl6SemParamRequiredCount(params);
    returnCount = xl6SemCallableReturnCount(member);
    hasVararg = xl6SemFuncHasVararg(params);
    returnCountOk = 1;
    ok = 1;

    /*
        协议方法是语言语义入口，签名必须稳定。
        当前不允许协议参数使用默认值，避免动态分发和 C ABI 产生两套解释。
    */
    if ( requiredCount != fixedCount ) {
        snprintf(message, sizeof(message), "协议方法 %s 参数不能声明默认值", name);
        xl6SemErrorNode(ctx, member, message);
        ok = 0;
    }
    if ( hasVararg && !rule->allowVararg ) {
        snprintf(message, sizeof(message), "协议方法 %s 不接受可变参数", name);
        xl6SemErrorNode(ctx, member, message);
        ok = 0;
    }
    if ( fixedCount < rule->minParams || (rule->maxParams >= 0 && fixedCount > rule->maxParams) ) {
        snprintf(message, sizeof(message), "协议方法 %s 参数数量不匹配", name);
        xl6SemErrorNode(ctx, member, message);
        ok = 0;
    }
    if ( returnCount < rule->minReturns || (rule->maxReturns >= 0 && returnCount > rule->maxReturns) ) {
        snprintf(message, sizeof(message), "协议方法 %s 返回值数量不匹配", name);
        xl6SemErrorNode(ctx, member, message);
        returnCountOk = 0;
        ok = 0;
    }
    if ( rule->firstParamType != NULL &&
         !xl6SemTypeNameMatches(ctx, xl6SemCallableParamTypeAt(params, 0), rule->firstParamType) ) {
        snprintf(message, sizeof(message), "协议方法 %s 第一个参数必须是 %s", name, rule->firstParamType);
        xl6SemErrorNode(ctx, member, message);
        ok = 0;
    }
    if ( returnCountOk &&
         rule->firstReturnType != NULL &&
         !xl6SemTypeNameMatches(ctx, xl6SemCallableReturnTypeAt(member, 0), rule->firstReturnType) ) {
        snprintf(message, sizeof(message), "协议方法 %s 第一个返回值必须是 %s", name, rule->firstReturnType);
        xl6SemErrorNode(ctx, member, message);
        ok = 0;
    }

    return ok;
}

static int xl6SemParamPrefixEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b, int count)
{
    int i;

    for ( i = 0; i < count; ++i ) {
        XL6_AstNode* aType = (a->children[i]->childCount > 0) ? a->children[i]->children[0] : NULL;
        XL6_AstNode* bType = (b->children[i]->childCount > 0) ? b->children[i]->children[0] : NULL;
        unsigned int aMode = xl6SemParamMode(a->children[i]);
        unsigned int bMode = xl6SemParamMode(b->children[i]);
        if ( aMode == XL6_AST_FLAG_BYVAL ) {
            aMode = 0;
        }
        if ( bMode == XL6_AST_FLAG_BYVAL ) {
            bMode = 0;
        }
        if ( aMode != bMode ) {
            return 0;
        }
        if ( !xl6SemTypeEquals(ctx, aType, bType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemOverloadShapeConflicts(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    const XL6_AstNode* aParams;
    const XL6_AstNode* bParams;
    int aRequired;
    int bRequired;
    int aFixed;
    int bFixed;
    int aVararg;
    int bVararg;
    int minFixed;
    int start;
    int count;

    aParams = xl6SemCallableParams(a);
    bParams = xl6SemCallableParams(b);
    aRequired = xl6SemParamRequiredCount(aParams);
    bRequired = xl6SemParamRequiredCount(bParams);
    aFixed = xl6SemFixedParamCount(aParams);
    bFixed = xl6SemFixedParamCount(bParams);
    aVararg = xl6SemFuncHasVararg(aParams);
    bVararg = xl6SemFuncHasVararg(bParams);

    if ( aVararg || bVararg ) {
        if ( aVararg && bVararg && aFixed == bFixed ) {
            int aKind = xl6SemParamVarargKind(aParams->children[aFixed]);
            int bKind = xl6SemParamVarargKind(bParams->children[bFixed]);
            return aKind == bKind && xl6SemParamPrefixEquals(ctx, aParams, bParams, aFixed);
        }
        return 0;
    }

    minFixed = aFixed < bFixed ? aFixed : bFixed;
    start = aRequired > bRequired ? aRequired : bRequired;
    for ( count = start; count <= minFixed; ++count ) {
        if ( xl6SemParamPrefixEquals(ctx, aParams, bParams, count) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckOverloadConflicts(XL6_SemContext* ctx)
{
    size_t i;
    size_t j;
    int ok;

    ok = 1;
    if ( ctx == NULL || ctx->root == NULL ) {
        return 1;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* a = ctx->root->children[i];
        if ( !xl6SemIsFuncDecl(a) ) {
            continue;
        }
        for ( j = i + 1; j < ctx->root->childCount; ++j ) {
            XL6_AstNode* b = ctx->root->children[j];
            if ( xl6SemIsFuncDecl(b) &&
                 xl6TextEquals(xl6SemFuncName(a), xl6SemFuncName(b)) &&
                 xl6SemOverloadShapeConflicts(ctx, a, b) ) {
                xl6SemErrorNode(ctx, b, "重复或歧义的函数重载签名");
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemIsExternCDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_EXTERN_FUNC || node->kind == XL6_AST_EXTERN_VAR);
}

static const XL6_AstNode* xl6SemExternFuncParams(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6SemExternFuncReturns(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6SemTypeListEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b, int params)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->childCount != b->childCount ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        const XL6_AstNode* aType = NULL;
        const XL6_AstNode* bType = NULL;
        if ( params ) {
            aType = a->children[i] != NULL && a->children[i]->childCount > 0 ? a->children[i]->children[0] : NULL;
            bType = b->children[i] != NULL && b->children[i]->childCount > 0 ? b->children[i]->children[0] : NULL;
            if ( xl6SemParamMode(a->children[i]) != xl6SemParamMode(b->children[i]) ) {
                return 0;
            }
        } else {
            aType = a->children[i];
            bType = b->children[i];
        }
        if ( !xl6SemTypeEquals(ctx, aType, bType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemExternCDeclsCompatible(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    if ( a == NULL || b == NULL ) {
        return 0;
    }
    if ( a->kind == XL6_AST_EXTERN_FUNC ) {
        return xl6SemTypeListEquals(ctx, xl6SemExternFuncParams(a), xl6SemExternFuncParams(b), 1) &&
               xl6SemTypeListEquals(ctx, xl6SemExternFuncReturns(a), xl6SemExternFuncReturns(b), 0);
    }
    if ( a->kind == XL6_AST_EXTERN_VAR ) {
        return xl6SemTypeEquals(ctx, xl6SemExternalVarTypeNode(a), xl6SemExternalVarTypeNode(b));
    }
    return 0;
}

static int xl6SemCheckExternCAbiConflicts(XL6_SemContext* ctx)
{
    size_t i;
    size_t j;
    int ok;

    ok = 1;
    if ( ctx == NULL || ctx->root == NULL ) {
        return 1;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* a = ctx->root->children[i];
        if ( !xl6SemIsExternCDecl(a) || a->text == NULL ) {
            continue;
        }
        for ( j = i + 1; j < ctx->root->childCount; ++j ) {
            XL6_AstNode* b = ctx->root->children[j];
            if ( !xl6SemIsExternCDecl(b) || !xl6TextEquals(a->text, b->text) ) {
                continue;
            }
            if ( !xl6SemExternCDeclsCompatible(ctx, a, b) ) {
                xl6SemErrorNode(ctx, b, "extern C symbol declared with incompatible ABI");
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemDeclareGlobalFuncName(XL6_SemContext* ctx, XL6_AstNode* nameNode)
{
    XL6_Symbol* oldSym;

    oldSym = xl6ScopeFindLocal(ctx->scope, nameNode != NULL ? nameNode->text : NULL);
    if ( oldSym != NULL && oldSym->kind == XL6_SYM_FUNC ) {
        return 1;
    }
    return xl6ScopeAdd(ctx, nameNode, XL6_SYM_FUNC);
}

static int xl6SemCheckKnownCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* func)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int hasCVararg;
    int* provided;
    int positionalIndex;
    int seenNamed;
    size_t i;
    int ok;

    params = func->childCount > 1 ? func->children[1] : NULL;
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
    hasCVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsCVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;

    if ( fixedCount > 0 && provided == NULL ) {
        xl6SemErrorNode(ctx, call, "函数调用检查内存不足");
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                XL6_AstNode* argExpr;

                seenNamed = 1;
                argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
                    ok = 0;
                }
                index = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    if ( provided[index] ) {
                        xl6SemErrorNode(ctx, arg, "重复传入同一个命名参数");
                        ok = 0;
                    }
                    provided[index] = 1;
                } else if ( !hasDictVararg ) {
                    xl6SemErrorNode(ctx, arg, "未知命名参数");
                    ok = 0;
                }
            } else {
                XL6_AstNode* argExpr;

                argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
                if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
                    ok = 0;
                }
                if ( seenNamed ) {
                    xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "位置参数不能出现在命名参数之后");
                    ok = 0;
                }
                if ( positionalIndex < fixedCount ) {
                    if ( provided[positionalIndex] ) {
                        xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "参数已由命名参数传入");
                        ok = 0;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg && !hasCVararg ) {
                    xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "实参数量超过函数声明");
                    ok = 0;
                }
            }
        }
    }

    for ( i = 0; i < (size_t)fixedCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
                    xl6SemErrorNode(ctx, call, "缺少必选参数");
            ok = 0;
            break;
        }
    }

    xrtFree(provided);
    return ok;
}

static int xl6SemCheckFunctionValueCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* functionType)
{
    XL6_AstNode* params;
    XL6_AstNode* args;
    size_t paramCount;
    size_t fixedCount;
    size_t positionalIndex;
    size_t paramIndex;
    size_t positionalArgCount;
    size_t i;
    int hasArrayVararg;
    int hasDictVararg;
    int seenNamed;
    int hasUnsupportedNamed;
    int countMismatch;
    int ok;

    if ( functionType == NULL || !xl6TextEquals(functionType->text, "function") ) {
        return 1;
    }
    if ( functionType->childCount == 0 ) {
        int namedReported = 0;
        ok = 1;
        args = call->childCount > 1 ? call->children[1] : NULL;
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                XL6_AstNode* arg = args->children[i];
                XL6_AstNode* argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
                if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                    if ( !namedReported ) {
                        xl6SemErrorNode(ctx, call, "func 调用不支持命名参数");
                        namedReported = 1;
                    }
                    ok = 0;
                    continue;
                }
                if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
                    ok = 0;
                }
            }
        }
        return ok;
    }
    if ( functionType->childCount < 2 ) {
        xl6SemErrorNode(ctx, functionType, "func 类型签名不完整");
        return 0;
    }

    params = functionType->children[0];
    args = call->childCount > 1 ? call->children[1] : NULL;
    paramCount = params != NULL ? params->childCount : 0;
    hasArrayVararg = paramCount > 0 && xl6SemTypeIsArrayVararg(params->children[paramCount - 1]);
    hasDictVararg = paramCount > 0 && xl6SemTypeIsDictVararg(params->children[paramCount - 1]);
    fixedCount = (hasArrayVararg || hasDictVararg) ? paramCount - 1 : paramCount;
    positionalIndex = 0;
    positionalArgCount = 0;
    seenNamed = 0;
    hasUnsupportedNamed = 0;
    countMismatch = 0;
    ok = 1;

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind != XL6_AST_NAMED_ARG ) {
                positionalArgCount += 1;
            } else if ( arg != NULL && !hasDictVararg ) {
                hasUnsupportedNamed = 1;
            }
        }
    }
    if ( hasUnsupportedNamed ) {
        xl6SemErrorNode(ctx, call, "func 值调用不支持命名参数");
        ok = 0;
    }
    if ( !hasUnsupportedNamed && positionalArgCount < fixedCount ) {
        xl6SemCountMismatchError(ctx, call, "func 调用实参数量和签名不匹配", fixedCount, positionalArgCount);
        countMismatch = 1;
        ok = 0;
    } else if ( !hasUnsupportedNamed && positionalArgCount > fixedCount && !hasArrayVararg ) {
        xl6SemCountMismatchError(ctx, call, "func 调用实参数量和签名不匹配", fixedCount, positionalArgCount);
        countMismatch = 1;
        ok = 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            XL6_AstNode* argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                seenNamed = 1;
                if ( hasDictVararg ) {
                    if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                        xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
                        ok = 0;
                    }
                    continue;
                }
                continue;
            }
            if ( seenNamed ) {
                xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "位置参数不能出现在命名参数之后");
                ok = 0;
                continue;
            }
            if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
                ok = 0;
                continue;
            }
            paramIndex = positionalIndex;
            positionalIndex += 1;
            if ( paramIndex >= fixedCount && !hasArrayVararg ) {
                if ( !countMismatch ) {
                    xl6SemCountMismatchError(ctx, argExpr, "func 调用实参数量和签名不匹配", fixedCount, positionalArgCount);
                }
                ok = 0;
                continue;
            }
            if ( paramIndex < fixedCount &&
                 !xl6SemExprCanMatchParamType(ctx, argExpr, params->children[paramIndex]) ) {
                xl6SemTypeMismatchError(
                    ctx,
                    argExpr,
                    "func 调用实参类型和签名不匹配",
                    params->children[paramIndex],
                    xl6SemExprStaticTypeName(ctx, argExpr));
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemCheckVarFunctionCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* args;
    size_t i;
    int seenNamed;
    int ok;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    seenNamed = 0;
    ok = 1;
    if ( args == NULL ) {
        return 1;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;

        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
            seenNamed = 1;
        } else if ( seenNamed ) {
            xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "位置参数不能出现在命名参数之后");
            ok = 0;
        }
        if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
            xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实参");
            ok = 0;
            continue;
        }
        if ( !xl6SemExprCanBoxToVar(ctx, argExpr) ) {
            xl6SemVarBoxMismatchError(ctx, argExpr, "var 函数调用实参必须能装箱为 var");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckMemberFunctionValueCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* callee;
    XL6_AstNode* memberType;
    int ok;

    if ( call == NULL || call->childCount == 0 ) {
        return -1;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return -1;
    }

    memberType = xl6SemCloneMemberFieldType(ctx, callee);
    if ( memberType == NULL ) {
        return -1;
    }
    if ( xl6TextEquals(memberType->text, "function") ) {
        ok = xl6SemCheckFunctionValueCall(ctx, call, memberType);
        xl6AstFree(memberType);
        return ok;
    }
    if ( xl6TextEquals(memberType->text, "var") ) {
        ok = xl6SemCheckVarFunctionCall(ctx, call);
        xl6AstFree(memberType);
        return ok;
    }

    xl6AstFree(memberType);
    return -1;
}
static int xl6SemCheckTypeCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* args;

    if ( call == NULL || call->childCount < 2 ) {
        xl6SemErrorNode(ctx, call, "type(...) 必须接收一个表达式参数");
        return 0;
    }
    args = call->children[1];
    if ( args == NULL || args->childCount != 1 ) {
        xl6SemErrorNode(ctx, call, "type(...) 必须接收一个表达式参数");
        return 0;
    }
    if ( args->children[0] != NULL && args->children[0]->kind == XL6_AST_NAMED_ARG ) {
        xl6SemErrorNode(ctx, args->children[0], "type(...) 不支持命名参数");
        return 0;
    }
    return 1;
}

static int xl6SemExprHasClassToString(XL6_SemContext* ctx, XL6_AstNode* expr);

static int xl6SemCheckTypeConvertCall(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* callee;
    XL6_AstNode* args;
    XL6_AstNode* arg;
    XL6_AstNode* argExpr;
    const char* sourceType;

    callee = call != NULL && call->childCount > 0 ? call->children[0] : NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( callee == NULL || callee->kind != XL6_AST_NAME || callee->text == NULL ) {
        return 1;
    }
    if ( args == NULL || args->childCount != 1 ) {
        xl6SemErrorNode(ctx, call, "类型转换必须接收一个表达式参数");
        return 0;
    }
    arg = args->children[0];
    if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
        xl6SemErrorNode(ctx, arg, "类型转换不支持命名参数");
        return 0;
    }
    argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
    if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
        xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为类型转换参数");
        return 0;
    }
    sourceType = xl6SemExprDeclaredTypeName(ctx, argExpr);
    if ( xl6TextEquals(callee->text, "var") && !xl6SemExprCanBoxToVar(ctx, argExpr) ) {
        xl6SemVarBoxMismatchError(ctx, argExpr, "该表达式不能装入 var");
        return 0;
    }
    if ( xl6TextEquals(callee->text, "string") && xl6SemExprHasClassToString(ctx, argExpr) ) {
        return 1;
    }
    if ( !xl6SemCanConvertType(sourceType, callee->text) ) {
        xl6SemConversionMismatchError(ctx, argExpr, callee->text, sourceType);
        return 0;
    }
    return 1;
}

static int xl6SemCallIsTypeBuiltin(XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 &&
           node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6TextEquals(node->children[0]->text, "type");
}

static int xl6SemTypeNameHasImplicitToString(const char* name)
{
    return xl6TextEquals(name, "null") ||
           xl6TextEquals(name, "var") ||
           xl6TextEquals(name, "bool") ||
           xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) ||
           xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "time") ||
           xl6TextEquals(name, "point");
}

static int xl6SemExprHasClassToString(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* symbol;
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    int result;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    symbol = expr->kind == XL6_AST_NAME ? xl6ScopeFind(ctx->scope, expr->text) : NULL;
    typeNode = symbol != NULL && symbol->type != NULL ?
               xl6SemCloneTypeNode(symbol->type) :
               xl6SemInferAutoTypeNode(ctx, expr);
    concreteType = xl6SemClassValueType(ctx, typeNode);
    classNode = concreteType != NULL && concreteType->text != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    result = classNode != NULL &&
             classNode->kind == XL6_AST_CLASS &&
             ((!xl6SemClassHasTypeParams(classNode) && xl6SemClassDeclHasDerive(classNode, "string")) ||
              xl6SemFindProtocolMethodForExprArgs(ctx, expr, "__toString__", NULL, 0, 0) != NULL);
    xl6AstFree(typeNode);
    return result;
}

static int xl6SemExprIsVarValue(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    sym = xl6ScopeFind(ctx->scope, expr->text);
    return sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "var");
}

static int xl6SemExprHasImplicitToString(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    const char* typeName;

    if ( xl6SemExprIsVarValue(ctx, expr) ) {
        return 1;
    }
    typeName = xl6SemExprStaticTypeName(ctx, expr);
    return xl6SemTypeNameHasImplicitToString(typeName) ||
           xl6SemExprHasClassToString(ctx, expr);
}

static int xl6SemIsImplicitToStringCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* args;
    XL6_AstNode* receiver;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || !xl6TextEquals(callee->text, "toString") ) {
        return 0;
    }
    args = node->childCount > 1 ? node->children[1] : NULL;
    if ( args != NULL && args->childCount != 0 ) {
        return 0;
    }
    receiver = callee->childCount > 0 ? callee->children[0] : NULL;
    return xl6SemExprHasImplicitToString(ctx, receiver);
}


static XL6_AstNode* xl6SemCallArgExprAt(XL6_AstNode* node, size_t index)
{
    XL6_AstNode* args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    XL6_AstNode* arg = args != NULL && index < args->childCount ? args->children[index] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6SemCallArgsArePositional(XL6_SemContext* ctx, XL6_AstNode* node, const char* message)
{
    XL6_AstNode* args;
    size_t i;
    int ok;

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    if ( args == NULL ) {
        return 1;
    }
    ok = 1;
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, arg != NULL ? arg : args, message);
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemIsCopyBuiltinName(const char* name)
{
    return xl6TextEquals(name, "copy") || xl6TextEquals(name, "clone");
}

static int xl6SemHasFunctionNamed(XL6_SemContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;

        if ( node == NULL ||
             (node->kind != XL6_AST_FUNC &&
              node->kind != XL6_AST_ASYNC_FUNC &&
              node->kind != XL6_AST_EXPORT_FUNC) ||
             node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6TextEquals(nameNode->text, name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCopyBuiltinIsShadowed(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;

    if ( node == NULL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || callee->text == NULL ) {
        return 0;
    }
    if ( xl6ScopeFind(ctx != NULL ? ctx->scope : NULL, callee->text) != NULL ) {
        return 1;
    }
    return xl6SemHasFunctionNamed(ctx, callee->text);
}

static int xl6SemCheckCopyBuiltinCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* arg;
    int ok;

    if ( node == NULL ||
         node->childCount == 0 ||
         node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_NAME ||
         !xl6SemIsCopyBuiltinName(node->children[0]->text) ) {
        return -1;
    }
    if ( xl6SemCopyBuiltinIsShadowed(ctx, node) ) {
        return -1;
    }

    ok = 1;
    if ( xl6SemCallArgCount(node) != 1 ) {
        xl6SemErrorNode(ctx, node, "copy/clone 必须接收一个参数");
        ok = 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "copy/clone 只接受位置参数") ) {
        ok = 0;
    }
    arg = xl6SemCallArgExprAt(node, 0);
    if ( arg != NULL &&
         !xl6SemCheckCopyableExpr(ctx, arg, 1, "不可复制类型不能调用 copy/clone") ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemExprHasStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr, const char* typeName)
{
    XL6_AstNode* inferred;
    const XL6_AstNode* concrete;
    int result;

    inferred = xl6SemInferAutoTypeNode(ctx, expr);
    concrete = xl6SemResolveAliasType(ctx, inferred);
    result = concrete != NULL && xl6TextEquals(concrete->text, typeName);
    xl6AstFree(inferred);
    return result;
}

static int xl6SemPersistenceGenericName(const char* name, const char** outFormat, const char** outParam)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( outParam != NULL ) {
        *outParam = NULL;
    }
    if ( xl6TextEquals(name, "__xl_std_json_parse") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        if ( outParam != NULL ) {
            *outParam = "text";
        }
        return 1;
    }
    if ( xl6TextEquals(name, "__xl_std_json_read") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        if ( outParam != NULL ) {
            *outParam = "path";
        }
        return 1;
    }
    if ( xl6TextEquals(name, "__xl_std_xson_parse") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        if ( outParam != NULL ) {
            *outParam = "text";
        }
        return 1;
    }
    if ( xl6TextEquals(name, "__xl_std_xson_read") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        if ( outParam != NULL ) {
            *outParam = "path";
        }
        return 1;
    }
    return 0;
}

static int xl6SemPersistenceStringifyName(const char* name, const char** outFormat)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( xl6TextEquals(name, "__xl_std_json_stringify") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        return 1;
    }
    if ( xl6TextEquals(name, "__xl_std_xson_stringify") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        return 1;
    }
    return 0;
}

static int xl6SemPersistenceWriteName(const char* name, const char** outFormat)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( xl6TextEquals(name, "__xl_std_json_write") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        return 1;
    }
    if ( xl6TextEquals(name, "__xl_std_xson_write") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        return 1;
    }
    return 0;
}

static XL6_AstNode* xl6SemPersistenceCallArg(XL6_AstNode* node, const char* name, size_t index)
{
    XL6_AstNode* args;
    size_t i;
    size_t positionalIndex;

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    positionalIndex = 0;
    if ( args == NULL ) {
        return NULL;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->childCount == 0 ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            if ( xl6TextEquals(arg->text, name) ) {
                return arg->children[0];
            }
            continue;
        }
        if ( positionalIndex == index ) {
            return arg->children[0];
        }
        positionalIndex += 1;
    }
    return NULL;
}

static const XL6_AstNode* xl6SemPersistenceGenericTypeArg(const XL6_AstNode* instant)
{
    const XL6_AstNode* typeArgs;

    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    typeArgs = instant->children[1];
    if ( typeArgs == NULL || typeArgs->kind != XL6_AST_LIST || typeArgs->childCount != 1 ) {
        return NULL;
    }
    return typeArgs->children[0];
}

static XL6_AstNode* xl6SemPersistenceTargetClass(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concrete;
    const XL6_TypeInfo* typeInfo;

    concrete = xl6SemResolveAliasType(ctx, typeNode);
    concrete = xl6SemClassValueType(ctx, concrete);
    if ( concrete == NULL || concrete->text == NULL || concrete->childCount > 0 ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, concrete->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS ) {
        return NULL;
    }
    return typeInfo->decl;
}

static int xl6SemCheckPersistenceTarget(
    XL6_SemContext* ctx,
    XL6_AstNode* atNode,
    const XL6_AstNode* typeNode,
    const char* format)
{
    XL6_AstNode* classNode;

    classNode = xl6SemPersistenceTargetClass(ctx, typeNode);
    if ( classNode == NULL ) {
        xl6SemErrorNode(ctx, atNode, "json/xson 泛型持久化目标必须是非泛型 class");
        return 0;
    }
    if ( xl6SemClassHasTypeParams(classNode) ) {
        xl6SemErrorNode(ctx, atNode, "json/xson 泛型持久化当前不支持泛型 class");
        return 0;
    }
    if ( !xl6SemClassDeclHasFormatDerive(classNode, format) ) {
        xl6SemErrorNode(ctx, atNode, "json/xson 持久化目标 class 必须声明对应的 derive value/json/xson");
        return 0;
    }
    return 1;
}

static int xl6SemCheckStringLikeArg(XL6_SemContext* ctx, XL6_AstNode* expr, const char* message)
{
    const char* typeName;

    typeName = xl6SemExprStaticTypeName(ctx, expr);
    if ( typeName != NULL && !xl6TextEquals(typeName, "string") && !xl6TextEquals(typeName, "var") ) {
        xl6SemErrorNode(ctx, expr, message);
        return 0;
    }
    return 1;
}

static int xl6SemCheckBoolLikeArg(XL6_SemContext* ctx, XL6_AstNode* expr, const char* message)
{
    const char* typeName;

    typeName = xl6SemExprStaticTypeName(ctx, expr);
    if ( typeName != NULL && !xl6SemIsBoolTypeName(typeName) && !xl6TextEquals(typeName, "var") ) {
        xl6SemErrorNode(ctx, expr, message);
        return 0;
    }
    return 1;
}

static int xl6SemCheckPersistenceGenericInstant(XL6_SemContext* ctx, XL6_AstNode* instant)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* typeArgs;
    const XL6_AstNode* typeArg;
    const char* format;

    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return -1;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6SemPersistenceGenericName(nameNode->text, &format, NULL) ) {
        return -1;
    }
    if ( typeArgs == NULL || typeArgs->kind != XL6_AST_LIST || typeArgs->childCount != 1 ) {
        xl6SemCountMismatchError(ctx, instant, "json/xson 泛型持久化目标类型数量不匹配", 1, typeArgs != NULL ? typeArgs->childCount : 0);
        return 0;
    }
    typeArg = typeArgs->children[0];
    return xl6SemCheckPersistenceTarget(ctx, instant, typeArg, format);
}

static int xl6SemCheckPersistenceGenericCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* instant;
    XL6_AstNode* nameNode;
    XL6_AstNode* argExpr;
    const XL6_AstNode* typeArg;
    const char* format;
    const char* paramName;
    int ok;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL || node->children[0]->kind != XL6_AST_GENERIC_INSTANT ) {
        return -1;
    }
    instant = node->children[0];
    nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6SemPersistenceGenericName(nameNode->text, &format, &paramName) ) {
        return -1;
    }

    ok = xl6SemCheckPersistenceGenericInstant(ctx, instant);
    if ( xl6SemCallArgCount(node) != 1 ) {
        xl6SemCountMismatchError(ctx, node, "json/xson 泛型持久化调用需要一个文本或路径参数", 1, (size_t)xl6SemCallArgCount(node));
        ok = 0;
    }
    typeArg = xl6SemPersistenceGenericTypeArg(instant);
    if ( typeArg == NULL ) {
        ok = 0;
    }
    argExpr = xl6SemPersistenceCallArg(node, paramName, 0);
    if ( argExpr == NULL ) {
        xl6SemErrorNode(ctx, node, "json/xson 泛型持久化参数名称不匹配");
        ok = 0;
    } else if ( !xl6SemCheckStringLikeArg(ctx, argExpr, "json/xson 泛型持久化参数必须是 string 或 var") ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemIsPersistenceGenericCallNode(XL6_AstNode* node)
{
    XL6_AstNode* instant;
    XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL || node->children[0]->kind != XL6_AST_GENERIC_INSTANT ) {
        return 0;
    }
    instant = node->children[0];
    nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6SemPersistenceGenericName(nameNode->text, NULL, NULL);
}

static XL6_AstNode* xl6SemClassArgForPersistence(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    XL6_AstNode** outTypeNode)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* classNode;

    if ( outTypeNode != NULL ) {
        *outTypeNode = NULL;
    }
    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    classNode = xl6SemPersistenceTargetClass(ctx, typeNode);
    if ( classNode == NULL ) {
        xl6AstFree(typeNode);
        return NULL;
    }
    if ( outTypeNode != NULL ) {
        *outTypeNode = typeNode;
    } else {
        xl6AstFree(typeNode);
    }
    return classNode;
}

static int xl6SemCheckPersistenceStdlibCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* valueExpr;
    XL6_AstNode* pathExpr;
    XL6_AstNode* prettyExpr;
    XL6_AstNode* valueType;
    XL6_AstNode* classNode;
    const char* format;
    int isWrite;
    int ok;
    int argCount;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL || node->children[0]->kind != XL6_AST_NAME ) {
        return -1;
    }
    callee = node->children[0];
    isWrite = 0;
    if ( !xl6SemPersistenceStringifyName(callee->text, &format) ) {
        if ( !xl6SemPersistenceWriteName(callee->text, &format) ) {
            return -1;
        }
        isWrite = 1;
    }

    valueExpr = isWrite ? xl6SemPersistenceCallArg(node, "value", 1) :
                          xl6SemPersistenceCallArg(node, "value", 0);
    classNode = xl6SemClassArgForPersistence(ctx, valueExpr, &valueType);
    if ( classNode == NULL ) {
        return -1;
    }

    ok = 1;
    argCount = xl6SemCallArgCount(node);
    if ( isWrite ) {
        if ( argCount < 2 || argCount > 3 ) {
            xl6SemErrorNode(ctx, node, "json/xson.write(class) 需要 path、value 和可选 pretty 参数");
            ok = 0;
        }
        pathExpr = xl6SemPersistenceCallArg(node, "path", 0);
        if ( pathExpr == NULL ) {
            xl6SemErrorNode(ctx, node, "json/xson.write(class) 缺少 path 参数");
            ok = 0;
        } else if ( !xl6SemCheckStringLikeArg(ctx, pathExpr, "json/xson.write path 必须是 string 或 var") ) {
            ok = 0;
        }
    } else if ( argCount < 1 || argCount > 2 ) {
        xl6SemErrorNode(ctx, node, "json/xson.stringify(class) 需要 value 和可选 pretty 参数");
        ok = 0;
    }
    prettyExpr = xl6SemPersistenceCallArg(node, "pretty", isWrite ? 2 : 1);
    if ( prettyExpr != NULL &&
         !xl6SemCheckBoolLikeArg(ctx, prettyExpr, "json/xson pretty 参数必须是 bool 或 var") ) {
        ok = 0;
    }
    if ( !xl6SemClassDeclHasFormatDerive(classNode, format) ) {
        xl6SemErrorNode(ctx, valueExpr, "json/xson 持久化 class 实参必须声明对应的 derive value/json/xson");
        ok = 0;
    }
    xl6AstFree(valueType);
    return ok;
}

static const XL6_AstNode* xl6SemPersistenceGenericCallTypeArg(const XL6_AstNode* expr)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    instant = expr->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    nameNode = instant->children[0];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6SemPersistenceGenericName(nameNode->text, NULL, NULL) ) {
        return NULL;
    }
    return xl6SemPersistenceGenericTypeArg(instant);
}

static int xl6SemCheckIntArg(XL6_SemContext* ctx, XL6_AstNode* expr, const char* message)
{
    XL6_AstNode intType;

    memset(&intType, 0, sizeof(intType));
    intType.kind = XL6_AST_TYPE;
    intType.text = "int";
    if ( expr != NULL && xl6SemExprCanMatchParamType(ctx, expr, &intType) ) {
        return 1;
    }
    xl6SemErrorNode(ctx, expr, message);
    return 0;
}

static XL6_AstNode* xl6SemCloneSyncReceiverType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return xl6SemInferAutoTypeNode(ctx, receiver);
    }
    sym = xl6ScopeFind(ctx->scope, receiver->text);
    if ( sym == NULL || sym->type == NULL ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(sym->type);
}

static int xl6SemCheckSyncCtorCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    const char* name;
    size_t argCount;
    int ok;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL || node->children[0]->kind != XL6_AST_NAME ) {
        return -1;
    }
    callee = node->children[0];
    name = callee->text;
    if ( !xl6SemIsSyncHandleTypeName(name) ) {
        return -1;
    }

    ok = xl6SemCallArgsArePositional(ctx, node, "sync constructor accepts positional arguments only");
    argCount = xl6SemCallArgCount(node);
    if ( xl6TextEquals(name, "mutex") || xl6TextEquals(name, "cond") || xl6TextEquals(name, "rwlock") ) {
        if ( argCount != 0 ) {
            xl6SemErrorNode(ctx, node, "sync constructor does not accept arguments");
            ok = 0;
        }
        return ok;
    }
    if ( xl6TextEquals(name, "sem") ) {
        if ( argCount != 2 ) {
            xl6SemErrorNode(ctx, node, "sem constructor requires init and max arguments");
            return 0;
        }
        if ( !xl6SemCheckIntArg(ctx, xl6SemCallArgExprAt(node, 0), "sem init argument must be int") ) {
            ok = 0;
        }
        if ( !xl6SemCheckIntArg(ctx, xl6SemCallArgExprAt(node, 1), "sem max argument must be int") ) {
            ok = 0;
        }
        return ok;
    }
    return -1;
}

static int xl6SemCheckSyncMethodNoArgs(XL6_SemContext* ctx, XL6_AstNode* node)
{
    if ( xl6SemCallArgCount(node) != 0 ) {
        xl6SemErrorNode(ctx, node, "sync method does not accept arguments");
        return 0;
    }
    return 1;
}

static int xl6SemCheckSyncMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const char* typeName;
    size_t argCount;
    int ok;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return -1;
    }
    member = node->children[0];
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6SemCloneSyncReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsSyncHandleTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return -1;
    }

    ok = 1;
    typeName = concreteReceiverType->text;
    argCount = xl6SemCallArgCount(node);
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, receiver != NULL ? receiver : member, "sync method receiver must be a named variable");
        ok = 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "sync method accepts positional arguments only") ) {
        ok = 0;
    }

    if ( xl6TextEquals(typeName, "mutex") ) {
        if ( xl6TextEquals(member->text, "lock") || xl6TextEquals(member->text, "tryLock") || xl6TextEquals(member->text, "unlock") ) {
            ok = xl6SemCheckSyncMethodNoArgs(ctx, node) && ok;
            xl6AstFree(receiverType);
            return ok;
        }
    }

    if ( xl6TextEquals(typeName, "sem") ) {
        if ( xl6TextEquals(member->text, "wait") || xl6TextEquals(member->text, "tryWait") || xl6TextEquals(member->text, "post") ) {
            ok = xl6SemCheckSyncMethodNoArgs(ctx, node) && ok;
            xl6AstFree(receiverType);
            return ok;
        }
        if ( xl6TextEquals(member->text, "waitTimeout") || xl6TextEquals(member->text, "postMultiple") ) {
            if ( argCount != 1 ) {
                xl6SemErrorNode(ctx, node, "sync method requires one int argument");
                ok = 0;
            } else if ( !xl6SemCheckIntArg(ctx, xl6SemCallArgExprAt(node, 0), "sync method argument must be int") ) {
                ok = 0;
            }
            xl6AstFree(receiverType);
            return ok;
        }
    }

    if ( xl6TextEquals(typeName, "cond") ) {
        if ( xl6TextEquals(member->text, "signal") || xl6TextEquals(member->text, "broadcast") ) {
            ok = xl6SemCheckSyncMethodNoArgs(ctx, node) && ok;
            xl6AstFree(receiverType);
            return ok;
        }
        if ( xl6TextEquals(member->text, "wait") || xl6TextEquals(member->text, "waitTimeout") ) {
            if ( (xl6TextEquals(member->text, "wait") && argCount != 1) ||
                 (xl6TextEquals(member->text, "waitTimeout") && argCount != 2) ) {
                xl6SemErrorNode(ctx, node, "cond wait requires mutex and optional timeout");
                ok = 0;
            } else {
                XL6_AstNode* mutexArg = xl6SemCallArgExprAt(node, 0);
                if ( !xl6SemExprHasStaticTypeName(ctx, mutexArg, "mutex") ) {
                    xl6SemErrorNode(ctx, mutexArg, "cond wait first argument must be mutex");
                    ok = 0;
                }
                if ( xl6TextEquals(member->text, "waitTimeout") &&
                     !xl6SemCheckIntArg(ctx, xl6SemCallArgExprAt(node, 1), "cond wait timeout must be int") ) {
                    ok = 0;
                }
            }
            xl6AstFree(receiverType);
            return ok;
        }
    }

    if ( xl6TextEquals(typeName, "rwlock") ) {
        if ( xl6TextEquals(member->text, "readLock") || xl6TextEquals(member->text, "tryReadLock") ||
             xl6TextEquals(member->text, "readUnlock") || xl6TextEquals(member->text, "writeLock") ||
             xl6TextEquals(member->text, "tryWriteLock") || xl6TextEquals(member->text, "writeUnlock") ||
             xl6TextEquals(member->text, "downgrade") || xl6TextEquals(member->text, "upgrade") ) {
            ok = xl6SemCheckSyncMethodNoArgs(ctx, node) && ok;
            xl6AstFree(receiverType);
            return ok;
        }
    }

    xl6SemErrorNode(ctx, member, "unknown sync method");
    xl6AstFree(receiverType);
    return 0;
}

static XL6_AstNode* xl6SemNewThreadEntryType(const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* intType;

    typeNode = xl6AstNewText(XL6_AST_TYPE, "function", 8, NULL);
    params = xl6AstNew(XL6_AST_LIST, NULL);
    returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    intType = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
    if ( typeNode == NULL || params == NULL || returns == NULL || intType == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(params);
        xl6AstFree(returns);
        xl6AstFree(intType);
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
        intType->line = at->line;
        intType->column = at->column;
    }
    xl6AstAddChild(returns, intType);
    xl6AstAddChild(typeNode, params);
    xl6AstAddChild(typeNode, returns);
    return typeNode;
}

static XL6_AstNode* xl6SemCloneThreadReceiverType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return xl6SemInferAutoTypeNode(ctx, receiver);
    }
    sym = xl6ScopeFind(ctx->scope, receiver->text);
    if ( sym == NULL || sym->type == NULL ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(sym->type);
}

static int xl6SemCheckThreadCtorCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* entryType;
    XL6_AstNode* entryExpr;
    size_t argCount;
    int ok;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL || node->children[0]->kind != XL6_AST_NAME ) {
        return -1;
    }
    callee = node->children[0];
    if ( !xl6TextEquals(callee->text, "thread") ) {
        return -1;
    }

    ok = xl6SemCallArgsArePositional(ctx, node, "thread constructor accepts positional arguments only");
    argCount = xl6SemCallArgCount(node);
    if ( argCount != 1 ) {
        xl6SemErrorNode(ctx, node, "thread constructor requires one func() as int entry argument");
        return 0;
    }

    entryExpr = xl6SemCallArgExprAt(node, 0);
    entryType = xl6SemNewThreadEntryType(callee);
    if ( entryType == NULL ) {
        xl6SemErrorNode(ctx, node, "thread constructor entry type allocation failed");
        return 0;
    }
    if ( !xl6SemExprCanMatchFunctionTypeEx(ctx, entryExpr, entryType, 0) ) {
        xl6SemErrorNode(ctx, entryExpr != NULL ? entryExpr : node, "thread entry must match func() as int");
        ok = 0;
    }
    xl6AstFree(entryType);
    return ok;
}

static int xl6SemCheckThreadMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    size_t argCount;
    int ok;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return -1;
    }
    member = node->children[0];
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6SemCloneThreadReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsThreadTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return -1;
    }

    ok = 1;
    argCount = xl6SemCallArgCount(node);
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, receiver != NULL ? receiver : member, "thread method receiver must be a named variable");
        ok = 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "thread method accepts positional arguments only") ) {
        ok = 0;
    }

    if ( xl6TextEquals(member->text, "wait") ||
         xl6TextEquals(member->text, "stop") ||
         xl6TextEquals(member->text, "state") ||
         xl6TextEquals(member->text, "exitCode") ) {
        if ( argCount != 0 ) {
            xl6SemErrorNode(ctx, node, "thread method does not accept arguments");
            ok = 0;
        }
        xl6AstFree(receiverType);
        return ok;
    }

    if ( xl6TextEquals(member->text, "waitTimeout") ) {
        if ( argCount != 1 ) {
            xl6SemErrorNode(ctx, node, "thread.waitTimeout requires one int timeout argument");
            ok = 0;
        } else if ( !xl6SemCheckIntArg(ctx, xl6SemCallArgExprAt(node, 0), "thread.waitTimeout argument must be int") ) {
            ok = 0;
        }
        xl6AstFree(receiverType);
        return ok;
    }

    xl6SemErrorNode(ctx, member, "unknown thread method");
    xl6AstFree(receiverType);
    return 0;
}

static int xl6SemCheckThreadCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ctorOk;

    ctorOk = xl6SemCheckThreadCtorCall(ctx, node);
    if ( ctorOk >= 0 ) {
        return ctorOk;
    }
    return xl6SemCheckThreadMethodCall(ctx, node);
}

static const char* xl6SemThreadCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const char* result;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    callee = node->children[0];
    if ( callee != NULL && callee->kind == XL6_AST_NAME && xl6TextEquals(callee->text, "thread") ) {
        return "thread";
    }
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    member = callee;
    receiverType = xl6SemCloneThreadReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsThreadTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6TextEquals(member->text, "wait") ||
         xl6TextEquals(member->text, "waitTimeout") ||
         xl6TextEquals(member->text, "stop") ) {
        result = "bool";
    } else if ( xl6TextEquals(member->text, "state") || xl6TextEquals(member->text, "exitCode") ) {
        result = "int";
    }
    xl6AstFree(receiverType);
    return result;
}
static int xl6SemCheckSyncCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ctorOk;

    ctorOk = xl6SemCheckSyncCtorCall(ctx, node);
    if ( ctorOk >= 0 ) {
        return ctorOk;
    }
    return xl6SemCheckSyncMethodCall(ctx, node);
}

static const char* xl6SemSyncCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const char* typeName;
    const char* result;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    callee = node->children[0];
    if ( callee != NULL && callee->kind == XL6_AST_NAME && xl6SemIsSyncHandleTypeName(callee->text) ) {
        return callee->text;
    }
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    member = callee;
    receiverType = xl6SemCloneSyncReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsSyncHandleTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    typeName = concreteReceiverType->text;
    result = NULL;
    if ( xl6TextEquals(typeName, "mutex") ) {
        if ( xl6TextEquals(member->text, "lock") || xl6TextEquals(member->text, "tryLock") || xl6TextEquals(member->text, "unlock") ) {
            result = "bool";
        }
    } else if ( xl6TextEquals(typeName, "sem") ) {
        if ( xl6TextEquals(member->text, "wait") || xl6TextEquals(member->text, "tryWait") ||
             xl6TextEquals(member->text, "waitTimeout") || xl6TextEquals(member->text, "post") ||
             xl6TextEquals(member->text, "postMultiple") ) {
            result = "bool";
        }
    } else if ( xl6TextEquals(typeName, "cond") ) {
        if ( xl6TextEquals(member->text, "wait") || xl6TextEquals(member->text, "waitTimeout") ||
             xl6TextEquals(member->text, "signal") || xl6TextEquals(member->text, "broadcast") ) {
            result = "bool";
        }
    } else if ( xl6TextEquals(typeName, "rwlock") ) {
        if ( xl6TextEquals(member->text, "readLock") || xl6TextEquals(member->text, "tryReadLock") ||
             xl6TextEquals(member->text, "readUnlock") || xl6TextEquals(member->text, "writeLock") ||
             xl6TextEquals(member->text, "tryWriteLock") || xl6TextEquals(member->text, "writeUnlock") ||
             xl6TextEquals(member->text, "downgrade") || xl6TextEquals(member->text, "upgrade") ) {
            result = "bool";
        }
    }
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemCheckContainerMethodArgCount(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    const char* reason,
    int minCount,
    int maxCount)
{
    int actualCount;

    actualCount = xl6SemCallArgCount(node);
    if ( actualCount >= minCount && actualCount <= maxCount ) {
        return 1;
    }
    xl6SemCountMismatchError(ctx, node, reason, (size_t)minCount, (size_t)actualCount);
    return 0;
}

static int xl6SemCheckContainerMethodItemArg(
    XL6_SemContext* ctx,
    XL6_AstNode* receiverType,
    XL6_AstNode* argExpr,
    const char* message)
{
    const XL6_AstNode* itemType;

    itemType = xl6SemContainerItemType(ctx, receiverType);
    if ( itemType == NULL || xl6TextEquals(itemType->text, "var") ) {
        return 1;
    }
    if ( xl6SemExprCanMatchParamType(ctx, argExpr, itemType) ) {
        return 1;
    }
    xl6SemTypeMismatchError(ctx, argExpr, message, itemType, xl6SemExprStaticTypeName(ctx, argExpr));
    return 0;
}

static int xl6SemCheckContainerMethodStringArg(
    XL6_SemContext* ctx,
    XL6_AstNode* argExpr,
    const char* message)
{
    const char* keyTypeName;

    keyTypeName = xl6SemExprStaticTypeName(ctx, argExpr);
    if ( keyTypeName == NULL || xl6TextEquals(keyTypeName, "string") || xl6TextEquals(keyTypeName, "var") ) {
        return 1;
    }
    xl6SemErrorNode(ctx, argExpr, message);
    return 0;
}

static int xl6SemCheckContainerMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* callee;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* arg0;
    XL6_AstNode* arg1;
    XL6_SemContainerKind kind;
    int typed;
    int argCount;
    int ok;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return -1;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ||
         callee->text == NULL || callee->childCount == 0 ) {
        return -1;
    }

    receiver = callee->children[0];
    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    kind = xl6SemContainerKindFromType(ctx, receiverType, &typed);
    if ( !xl6SemContainerMethodSupported(kind, typed, callee->text) ) {
        xl6AstFree(receiverType);
        return -1;
    }

    ok = xl6SemCallArgsArePositional(ctx, node, "container method arguments must be positional");
    argCount = xl6SemCallArgCount(node);
    arg0 = argCount > 0 ? xl6SemCallArgExprAt(node, 0) : NULL;
    arg1 = argCount > 1 ? xl6SemCallArgExprAt(node, 1) : NULL;

    if ( xl6TextEquals(callee->text, "push") || xl6TextEquals(callee->text, "append") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array push/append expects one value argument", 1, 1) && ok;
        if ( ok && kind == XL6_SEM_CONTAINER_ARRAY ) {
            ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "array push/append value type mismatch") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "insert") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.insert expects index and value arguments", 2, 2) && ok;
        if ( ok ) {
            ok = xl6SemCheckIntArg(ctx, arg0, "array.insert index must be int") && ok;
            if ( kind == XL6_SEM_CONTAINER_ARRAY ) {
                ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg1, "array.insert value type mismatch") && ok;
            }
        }
    } else if ( xl6TextEquals(callee->text, "remove") ) {
        if ( kind == XL6_SEM_CONTAINER_ARRAY || kind == XL6_SEM_CONTAINER_VAR ) {
            ok = xl6SemCheckContainerMethodArgCount(ctx, node, "container.remove expects one or two arguments", 1, 2) && ok;
            if ( ok && arg0 != NULL && kind == XL6_SEM_CONTAINER_ARRAY ) {
                ok = xl6SemCheckIntArg(ctx, arg0, "array.remove index must be int") && ok;
                if ( arg1 != NULL ) {
                    ok = xl6SemCheckIntArg(ctx, arg1, "array.remove count must be int") && ok;
                }
            }
        } else if ( kind == XL6_SEM_CONTAINER_LIST ) {
            ok = xl6SemCheckContainerMethodArgCount(ctx, node, "list.remove expects one key argument", 1, 1) && ok;
            if ( ok ) {
                ok = xl6SemCheckIntArg(ctx, arg0, "list.remove key must be int") && ok;
            }
        } else if ( kind == XL6_SEM_CONTAINER_SET ) {
            ok = xl6SemCheckContainerMethodArgCount(ctx, node, "set.remove expects one value argument", 1, 1) && ok;
            if ( ok ) {
                ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "set.remove value type mismatch") && ok;
            }
        } else if ( kind == XL6_SEM_CONTAINER_DICT ) {
            ok = xl6SemCheckContainerMethodArgCount(ctx, node, "dict.remove expects one key argument", 1, 1) && ok;
            if ( ok ) {
                ok = xl6SemCheckContainerMethodStringArg(ctx, arg0, "dict.remove key must be string") && ok;
            }
        }
    } else if ( xl6TextEquals(callee->text, "removeValue") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.removeValue expects one value argument", 1, 1) && ok;
        if ( ok && kind == XL6_SEM_CONTAINER_ARRAY ) {
            ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "array.removeValue value type mismatch") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "contains") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "container.contains expects one argument", 1, 1) && ok;
        if ( ok && kind == XL6_SEM_CONTAINER_DICT ) {
            ok = xl6SemCheckContainerMethodStringArg(ctx, arg0, "dict.contains key must be string") && ok;
        } else if ( ok && (kind == XL6_SEM_CONTAINER_ARRAY ||
                           kind == XL6_SEM_CONTAINER_LIST ||
                           kind == XL6_SEM_CONTAINER_SET) ) {
            ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "container.contains value type mismatch") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "add") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "set.add expects one value argument", 1, 1) && ok;
        if ( ok && kind == XL6_SEM_CONTAINER_SET ) {
            ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "set.add value type mismatch") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "pop") || xl6TextEquals(callee->text, "clear") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "container method expects no arguments", 0, 0) && ok;
    } else if ( xl6TextEquals(callee->text, "take") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "container.take expects one key/index argument", 1, 1) && ok;
        if ( ok && (kind == XL6_SEM_CONTAINER_ARRAY || kind == XL6_SEM_CONTAINER_LIST) ) {
            ok = xl6SemCheckIntArg(ctx, arg0, "container.take index/key must be int") && ok;
        } else if ( ok && kind == XL6_SEM_CONTAINER_DICT ) {
            ok = xl6SemCheckContainerMethodStringArg(ctx, arg0, "dict.take key must be string") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "reserve") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.reserve expects one count argument", 1, 1) && ok;
        if ( ok ) {
            ok = xl6SemCheckIntArg(ctx, arg0, "array.reserve count must be int") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "swap") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.swap expects two index arguments", 2, 2) && ok;
        if ( ok ) {
            ok = xl6SemCheckIntArg(ctx, arg0, "array.swap first index must be int") && ok;
            ok = xl6SemCheckIntArg(ctx, arg1, "array.swap second index must be int") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "indexOf") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.indexOf expects one value argument", 1, 1) && ok;
        if ( ok && kind == XL6_SEM_CONTAINER_ARRAY ) {
            ok = xl6SemCheckContainerMethodItemArg(ctx, receiverType, arg0, "array.indexOf value type mismatch") && ok;
        }
    } else if ( xl6TextEquals(callee->text, "reverse") ) {
        ok = xl6SemCheckContainerMethodArgCount(ctx, node, "array.reverse expects no arguments", 0, 0) && ok;
    }

    xl6AstFree(receiverType);
    return ok;
}

static int xl6SemCheckCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;
    int channelOk;
    int specialCollectionOk;
    int syncOk;
    int taskOk;
    int threadOk;
    int futureOk;
    int memberCallOk;
    XL6_AstNode* func;
    XL6_Symbol* sym;

    if ( xl6SemIsImplicitToStringCall(ctx, node) ) {
        XL6_AstNode* callee = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* receiver = callee != NULL && callee->childCount > 0 ? callee->children[0] : NULL;
        return receiver != NULL ? xl6SemCheckExpr(ctx, receiver) : 1;
    }

    ok = xl6SemIsPersistenceGenericCallNode(node) ?
         xl6SemCheckChildrenExprFrom(ctx, node, 1) :
         xl6SemCheckChildrenExpr(ctx, node);
    if ( xl6SemIsImplicitToStringCall(ctx, node) ) {
        return ok;
    }
    {
        int copyBuiltinOk = xl6SemCheckCopyBuiltinCall(ctx, node);
        if ( copyBuiltinOk >= 0 ) {
            return copyBuiltinOk && ok;
        }
    }
    {
        int persistenceGenericOk = xl6SemCheckPersistenceGenericCall(ctx, node);
        if ( persistenceGenericOk >= 0 ) {
            return persistenceGenericOk && ok;
        }
    }
    {
        int persistenceStdlibOk = xl6SemCheckPersistenceStdlibCall(ctx, node);
        if ( persistenceStdlibOk >= 0 ) {
            return persistenceStdlibOk && ok;
        }
    }
    {
        int containerMethodOk = xl6SemCheckContainerMethodCall(ctx, node);
        if ( containerMethodOk >= 0 ) {
            return containerMethodOk && ok;
        }
    }
      if ( node->childCount > 0 &&
           node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6TextEquals(node->children[0]->text, "range") ) {
          if ( !xl6SemCheckRangeCall(ctx, node) ) {
              ok = 0;
          }
      }
      if ( node->childCount > 0 &&
           node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6TextEquals(node->children[0]->text, "base") ) {
          return xl6SemCheckBaseCtorCall(ctx, node) && ok;
      }
      if ( node->childCount > 0 && node->children[0] != NULL ) {
          XL6_AstNode* callee = node->children[0];
          XL6_AstNode* nameNode = callee;

        if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
            nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        }
        if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
            const XL6_TypeInfo* typeInfo = xl6TypeRegistryFind(&ctx->types, nameNode->text);
            if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS ) {
                return xl6SemCheckCtorCall(ctx, node) && ok;
            }
        }
    }
    threadOk = xl6SemCheckThreadCall(ctx, node);
    if ( threadOk >= 0 ) {
        return threadOk && ok;
    }
    syncOk = xl6SemCheckSyncCall(ctx, node);
    if ( syncOk >= 0 ) {
        return syncOk && ok;
    }
    specialCollectionOk = xl6SemCheckSpecialCollectionCall(ctx, node);
    if ( specialCollectionOk >= 0 ) {
        return specialCollectionOk && ok;
    }
    channelOk = xl6SemCheckChannelCall(ctx, node);
    if ( channelOk >= 0 ) {
        return channelOk && ok;
    }
    futureOk = xl6SemCheckFutureCall(ctx, node);
    if ( futureOk >= 0 ) {
        return futureOk && ok;
    }
    taskOk = xl6SemCheckTaskCall(ctx, node);
    if ( taskOk >= 0 ) {
        return taskOk && ok;
    }
    if ( xl6SemCallIsTypeBuiltin(node) ) {
        return xl6SemCheckTypeCall(ctx, node) && ok;
    }
    if ( node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6SemIsBuiltinConvertTypeName(node->children[0]->text) &&
         xl6SemNameIsRegisteredType(ctx, node->children[0]->text) ) {
        return xl6SemCheckTypeConvertCall(ctx, node) && ok;
    }

    if ( node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_MEMBER &&
         xl6SemClassDeclFromTypeMemberReceiver(ctx, node->children[0]) != NULL ) {
        func = xl6SemResolveStaticClassFuncCall(ctx, node, 1);
        if ( func != NULL && !xl6SemCheckKnownCall(ctx, node, func) ) {
            ok = 0;
        } else if ( func == NULL ) {
            XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, node->children[0]);
            if ( memberType == NULL ) {
                xl6SemErrorNode(ctx, node->children[0], "unknown static class member");
                ok = 0;
            }
            xl6AstFree(memberType);
        }
        return ok;
    }

    memberCallOk = xl6SemCheckMemberFunctionValueCall(ctx, node);
    if ( memberCallOk >= 0 ) {
        return memberCallOk && ok;
    }

    if ( node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
        XL6_AstNode* instant = node->children[0];
        XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
        XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
        if ( !ok ) {
            return 0;
        }
        if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
            func = xl6SemResolveExplicitGenericCallFunc(ctx, node, nameNode->text, typeArgs, 1);
            return func != NULL && ok;
        }
    }

    if ( node->childCount > 0 && node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, node->children[0]->text);
        if ( sym != NULL && !xl6SemCheckBoundarySymbolUse(ctx, node->children[0], sym, 0) ) {
            return 0;
        }
        if ( sym != NULL && sym->kind != XL6_SYM_FUNC && sym->type != NULL && xl6TextEquals(sym->type->text, "function") ) {
            if ( !xl6SemCheckFunctionValueCall(ctx, node, sym->type) ) {
                ok = 0;
            }
            return ok;
        }
        if ( sym != NULL && sym->kind != XL6_SYM_FUNC && sym->type != NULL && xl6TextEquals(sym->type->text, "var") ) {
            if ( !xl6SemCheckVarFunctionCall(ctx, node) ) {
                ok = 0;
            }
            return ok;
        }
        {
            const XL6_AstNode* protocolMethod = xl6SemFindProtocolMethodForExprCall(ctx, node->children[0], "__call__", node, 1);
            if ( protocolMethod != NULL ) {
                return xl6SemCheckProtocolCallArgs(ctx, node, protocolMethod) && ok;
            }
            protocolMethod = xl6SemFindProtocolMethodForExpr(ctx, node->children[0], "__call__");
            if ( protocolMethod != NULL ) {
                xl6SemCheckProtocolCallArgs(ctx, node, protocolMethod);
                return 0;
            }
        }
        func = xl6SemResolveCallFunc(ctx, node, node->children[0]->text);
    } else {
        const XL6_AstNode* protocolMethod = node->childCount > 0 && node->children[0] != NULL ?
                                            xl6SemFindProtocolMethodForExprCall(ctx, node->children[0], "__call__", node, 1) :
                                            NULL;
        if ( protocolMethod != NULL ) {
            return xl6SemCheckProtocolCallArgs(ctx, node, protocolMethod) && ok;
        }
        if ( node->childCount > 0 && node->children[0] != NULL ) {
            protocolMethod = xl6SemFindProtocolMethodForExpr(ctx, node->children[0], "__call__");
            if ( protocolMethod != NULL ) {
                xl6SemCheckProtocolCallArgs(ctx, node, protocolMethod);
                return 0;
            }
        }
        func = NULL;
    }
    if ( func != NULL && !xl6SemCheckKnownCall(ctx, node, func) ) {
        ok = 0;
    }
    return ok;
}

static XL6_AstNode* xl6SemFindExplicitGenericFunc(
    XL6_SemContext* ctx,
    const char* name,
    XL6_AstNode* typeArgs,
    int* outExpectedArity)
{
    XL6_AstNode* selected;
    size_t i;
    int matchCount;
    int candidateCount;
    int expectedArity;
    int sameExpectedArity;

    selected = NULL;
    matchCount = 0;
    candidateCount = 0;
    expectedArity = -1;
    sameExpectedArity = 1;
    if ( outExpectedArity != NULL ) {
        *outExpectedArity = -1;
    }
    if ( ctx == NULL || ctx->root == NULL || name == NULL || typeArgs == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* params;
        if ( node == NULL ||
             (node->kind != XL6_AST_FUNC && node->kind != XL6_AST_ASYNC_FUNC) ||
             !xl6SemFuncIsGeneric(node) ||
             node->childCount == 0 || node->children[0] == NULL ||
             !xl6TextEquals(node->children[0]->text, name) ) {
            continue;
        }
        params = xl6SemFuncTypeParams(node);
        candidateCount += 1;
        if ( params != NULL ) {
            int arity = (int)params->childCount;
            if ( expectedArity < 0 ) {
                expectedArity = arity;
            } else if ( expectedArity != arity ) {
                sameExpectedArity = 0;
            }
        }
        if ( params != NULL && params->childCount == typeArgs->childCount &&
             xl6SemCheckGenericConstraints(ctx, params, typeArgs, NULL, 0) ) {
            selected = node;
            matchCount += 1;
        }
    }
    if ( matchCount >= 1 ) {
        return selected;
    }
    if ( candidateCount > 0 && sameExpectedArity && expectedArity >= 0 && outExpectedArity != NULL ) {
        *outExpectedArity = expectedArity;
    }
    return NULL;
}

static int xl6SemCheckGenericInstant(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* typeArgs;
    const XL6_TypeInfo* typeInfo;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( node == NULL || node->childCount < 2 ) {
        return ok;
    }
    nameNode = node->children[0];
    typeArgs = node->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || typeArgs == NULL ) {
        xl6SemErrorNode(ctx, node, "显式泛型实例化目标必须是名称");
        return 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, nameNode->text);
    if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS ) {
        if ( !typeInfo->allowGeneric || typeInfo->genericArity != (int)typeArgs->childCount ) {
            xl6SemCountMismatchError(ctx, node, "泛型 class 实参数量不匹配", (size_t)typeInfo->genericArity, typeArgs->childCount);
            ok = 0;
        }
        if ( ok && typeInfo->decl != NULL &&
             !xl6SemCheckGenericConstraints(
                 ctx,
                 typeInfo->decl->childCount > 2 ? typeInfo->decl->children[2] : NULL,
                 typeArgs,
                 node,
                 1) ) {
            ok = 0;
        }
        return ok;
    }

    if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_BUILTIN && xl6TextEquals(nameNode->text, "channel") ) {
        if ( !typeInfo->allowGeneric || typeInfo->genericArity != (int)typeArgs->childCount ) {
            xl6SemCountMismatchError(ctx, node, "channel generic argument count mismatch", (size_t)typeInfo->genericArity, typeArgs->childCount);
            ok = 0;
        } else if ( !xl6SemCheckChannelTypeArg(ctx, node) ) {
            ok = 0;
        }
        return ok;
    }
    if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_BUILTIN &&
         xl6SemIsSpecialCollectionTypeName(nameNode->text) ) {
        if ( !typeInfo->allowGeneric || typeInfo->genericArity != (int)typeArgs->childCount ) {
            xl6SemCountMismatchError(
                ctx,
                node,
                "专用数据结构泛型实参数量不匹配",
                (size_t)typeInfo->genericArity,
                typeArgs->childCount);
            ok = 0;
        }
        return ok;
    }
    {
        int persistenceOk = xl6SemCheckPersistenceGenericInstant(ctx, node);
        if ( persistenceOk >= 0 ) {
            return persistenceOk && ok;
        }
    }
    {
        int expectedArity;
        expectedArity = -1;
        if ( xl6SemFindExplicitGenericFunc(ctx, nameNode->text, typeArgs, &expectedArity) == NULL ) {
            if ( expectedArity >= 0 ) {
                xl6SemCountMismatchError(ctx, node, "显式泛型函数类型实参数量不匹配", (size_t)expectedArity, typeArgs->childCount);
            } else {
                xl6SemErrorNode(ctx, node, "找不到匹配的显式泛型函数实例");
            }
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckTypedLiteral(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* literal;
    XL6_AstNode* typeNode;
    int ok;

    if ( node->childCount == 0 ) {
        return 1;
    }
    literal = node->children[0];
    typeNode = node->childCount > 1 ? node->children[1] : NULL;
    ok = 1;

    if ( xl6TextEquals(node->text, "array") ) {
        if ( literal->kind != XL6_AST_ARRAY_LITERAL ) {
            xl6SemErrorNode(ctx, node, "array 字面量必须使用普通 [] 值列表");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "list") ) {
        if ( literal->kind != XL6_AST_LIST_LITERAL ) {
            xl6SemErrorNode(ctx, node, "list 字面量必须使用 [key: value]");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "set") ) {
        /*
            普通 {} 默认解析为 dict；只有带 set 前缀的空 {} 才改为空集合。
            非空 set 仍必须使用 {value, value}，不能混用 {key: value}。
        */
        if ( literal->kind != XL6_AST_SET_LITERAL &&
             !(literal->kind == XL6_AST_DICT_LITERAL && literal->childCount == 0) ) {
            xl6SemErrorNode(ctx, node, "set 字面量必须使用普通 {} 值列表");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "dict") ) {
        if ( literal->kind != XL6_AST_DICT_LITERAL ) {
            xl6SemErrorNode(ctx, node, "dict 字面量必须使用 {key: value}");
            return 0;
        }
    } else {
        xl6SemErrorNode(ctx, node, "未知的带类型容器字面量");
        return 0;
    }

    if ( typeNode != NULL ) {
        if ( typeNode->kind != XL6_AST_TYPE || !xl6TextEquals(typeNode->text, node->text) ) {
            xl6SemErrorNode(ctx, typeNode, "container generic literal prefix must match its container type");
            ok = 0;
        } else if ( !xl6SemCheckType(ctx, typeNode) ) {
            ok = 0;
        }
    }
    return xl6SemCheckExpr(ctx, literal) && ok;
}

static const char* xl6SemBinaryNumericResultType(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* leftType,
    const char* rightType)
{
    XL6_AstNode* left;
    XL6_AstNode* right;

    if ( ctx == NULL || expr == NULL || expr->childCount < 2 ||
         leftType == NULL || rightType == NULL ||
         !xl6SemIsNumericTypeName(leftType) || !xl6SemIsNumericTypeName(rightType) ) {
        return NULL;
    }

    left = expr->children[0];
    right = expr->children[1];
    if ( xl6TextEquals(expr->text, "<<") || xl6TextEquals(expr->text, ">>") ||
         xl6TextEquals(expr->text, "%") ||
         xl6TextEquals(expr->text, "&") ||
         xl6TextEquals(expr->text, "|") ||
         xl6TextEquals(expr->text, "^") ) {
        return xl6SemIsIntegerTypeName(leftType) && xl6SemIsIntegerTypeName(rightType) ? leftType : NULL;
    }
    if ( right != NULL && xl6SemExprIntegerLiteralFitsType(right, leftType) ) {
        return leftType;
    }
    if ( left != NULL && xl6SemExprIntegerLiteralFitsType(left, rightType) ) {
        return rightType;
    }
    if ( xl6SemCanSafeWidenTypeName(rightType, leftType) ) {
        return leftType;
    }
    if ( xl6SemCanSafeWidenTypeName(leftType, rightType) ) {
        return rightType;
    }
    /*
     * 算术表达式的类型提升不同于赋值隐式转换：
     * int * float 这类表达式应得到 float，但 float target = int value 仍由赋值规则单独禁止。
     */
    if ( xl6TextEquals(expr->text, "+") ||
         xl6TextEquals(expr->text, "-") ||
         xl6TextEquals(expr->text, "*") ||
         xl6TextEquals(expr->text, "/") ) {
        if ( xl6SemIsFloatTypeName(leftType) || xl6SemIsFloatTypeName(rightType) ) {
            return "float";
        }
        if ( xl6SemIsIntegerTypeName(leftType) && xl6SemIsIntegerTypeName(rightType) ) {
            return "int";
        }
    }
    return NULL;
}

static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;
    XL6_AstNode* func;
    XL6_AstNode* returns;
    XL6_AstNode* constructedType;
    const char* leftType;
    const char* rightType;
    const char* receiverType;
    const char* constructedTypeName;

    if ( expr == NULL ) {
        return NULL;
    }

    switch ( expr->kind ) {
    case XL6_AST_INT:
        return "int";
    case XL6_AST_FLOAT:
        return "float";
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
        return "string";
    case XL6_AST_TIME:
        return "time";
    case XL6_AST_BOOL:
        return "bool";
    case XL6_AST_NULL:
        return "null";
    case XL6_AST_ARRAY_LITERAL:
        return "array";
    case XL6_AST_LIST_LITERAL:
        return "list";
    case XL6_AST_SET_LITERAL:
        return "set";
    case XL6_AST_DICT_LITERAL:
        return "dict";
    case XL6_AST_TYPED_LITERAL:
        return expr->text;
    case XL6_AST_ANON_FUNC:
        return "function";
    case XL6_AST_NAME:
        sym = xl6ScopeFind(ctx->scope, expr->text);
        if ( sym == NULL && ctx->implicitSelfName != NULL && ctx->currentClass != NULL ) {
            const XL6_AstNode* field = xl6SemFindClassFieldNode(ctx, ctx->currentClass, expr->text, NULL);
            if ( field != NULL && field->childCount > 0 && field->children[0] != NULL ) {
                return xl6SemResolveAliasTypeName(ctx, field->children[0]);
            }
        }
        if ( sym == NULL && xl6SemNameIsRegisteredType(ctx, expr->text) ) {
            return "type";
        }
        if ( sym == NULL || sym->type == NULL || sym->type->text == NULL ||
             xl6TextEquals(sym->type->text, "auto") ) {
            return NULL;
        }
        return xl6SemResolveAliasTypeName(ctx, sym->type);
    case XL6_AST_UNARY:
        if ( xl6TextEquals(expr->text, "not") || xl6TextEquals(expr->text, "!") ) {
            return "bool";
        }
        if ( expr->childCount > 0 ) {
            return xl6SemExprStaticTypeName(ctx, expr->children[0]);
        }
        return NULL;
    case XL6_AST_AWAIT: {
        XL6_AstNode* awaitType = xl6SemCloneAwaitResultType(ctx, expr);
        const char* awaitTypeName = xl6SemStaticTypeNameFromTypeNode(ctx, awaitType);
        xl6AstFree(awaitType);
        return awaitTypeName;
    }
    case XL6_AST_TYPE_IS:
        return "bool";
    case XL6_AST_TYPE_AS:
        if ( expr->childCount > 1 ) {
            return xl6SemStaticTypeNameFromTypeNode(ctx, expr->children[1]);
        }
        return NULL;
    case XL6_AST_BINARY:
        if ( xl6TextEquals(expr->text, "and") || xl6TextEquals(expr->text, "or") ||
             xl6TextEquals(expr->text, "==") || xl6TextEquals(expr->text, "!=") ||
             xl6TextEquals(expr->text, "~=") || xl6TextEquals(expr->text, "<") ||
             xl6TextEquals(expr->text, "<=") || xl6TextEquals(expr->text, ">") ||
             xl6TextEquals(expr->text, ">=") || xl6TextEquals(expr->text, "in") ) {
            return "bool";
        }
        leftType = expr->childCount > 0 ? xl6SemExprStaticTypeName(ctx, expr->children[0]) : NULL;
        rightType = expr->childCount > 1 ? xl6SemExprStaticTypeName(ctx, expr->children[1]) : NULL;
        if ( xl6TextEquals(expr->text, "??") ) {
            if ( leftType != NULL && xl6TextEquals(leftType, "null") ) {
                return rightType;
            }
            if ( rightType != NULL && xl6TextEquals(rightType, "null") ) {
                return leftType;
            }
            if ( leftType != NULL && rightType != NULL && xl6TextEquals(leftType, rightType) ) {
                return leftType;
            }
            if ( (leftType != NULL && xl6TextEquals(leftType, "var")) ||
                 (rightType != NULL && xl6TextEquals(rightType, "var")) ) {
                return "var";
            }
            return NULL;
        }
        /* 加法任一侧静态为 string 时，动态另一侧也只能走字符串拼接路径。 */
        if ( xl6TextEquals(expr->text, "+") &&
             ((leftType != NULL && xl6TextEquals(leftType, "string")) ||
              (rightType != NULL && xl6TextEquals(rightType, "string"))) ) {
            return "string";
        }
        if ( (leftType != NULL && xl6TextEquals(leftType, "var")) ||
             (rightType != NULL && xl6TextEquals(rightType, "var")) ) {
            return "var";
        }
        if ( leftType != NULL && rightType != NULL && xl6TextEquals(leftType, rightType) ) {
            return leftType;
        }
        if ( leftType != NULL && rightType != NULL ) {
            const char* numericType = xl6SemBinaryNumericResultType(ctx, expr, leftType, rightType);
            if ( numericType != NULL ) {
                return numericType;
            }
        }
        return NULL;
    case XL6_AST_CONDITIONAL:
        leftType = expr->childCount > 1 ? xl6SemExprStaticTypeName(ctx, expr->children[1]) : NULL;
        rightType = expr->childCount > 2 ? xl6SemExprStaticTypeName(ctx, expr->children[2]) : NULL;
        if ( leftType != NULL && rightType != NULL && xl6TextEquals(leftType, rightType) ) {
            return leftType;
        }
        if ( leftType != NULL && xl6TextEquals(leftType, "null") ) {
            return rightType;
        }
        if ( rightType != NULL && xl6TextEquals(rightType, "null") ) {
            return leftType;
        }
        if ( leftType != NULL && rightType != NULL ) {
            return "var";
        }
        return NULL;
    case XL6_AST_MEMBER: {
        XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, expr);
        const char* memberTypeName;

        if ( memberType != NULL ) {
            memberTypeName = xl6SemStaticTypeNameFromTypeNode(ctx, memberType);
            xl6AstFree(memberType);
            return memberTypeName;
        }
        receiverType = expr->childCount > 0 ? xl6SemExprStaticTypeName(ctx, expr->children[0]) : NULL;
        if ( xl6TextEquals(expr->text, "length") &&
             (xl6TextEquals(receiverType, "string") ||
              xl6TextEquals(receiverType, "var") ||
              xl6SemIsArrayTypeName(receiverType) ||
              xl6SemIsListTypeName(receiverType) ||
              xl6TextEquals(receiverType, "set") ||
              xl6TextEquals(receiverType, "set<var>") ||
              xl6SemIsDictTypeName(receiverType)) ) {
            return "int";
        }
        if ( xl6TextEquals(receiverType, "error") ) {
            if ( xl6TextEquals(expr->text, "kind") ||
                 xl6TextEquals(expr->text, "message") ||
                 xl6TextEquals(expr->text, "file") ) {
                return "string";
            }
            if ( xl6TextEquals(expr->text, "line") ||
                 xl6TextEquals(expr->text, "column") ) {
                return "int";
            }
        }
        return NULL;
    }
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX: {
        XL6_AstNode* itemType = xl6SemCloneIndexedItemType(ctx, expr);
        const char* itemTypeName;

        if ( itemType != NULL ) {
            if ( expr->kind == XL6_AST_SAFE_INDEX ) {
                xl6AstFree(itemType);
                return "var";
            }
            itemTypeName = xl6SemStaticTypeNameFromTypeNode(ctx, itemType);
            xl6AstFree(itemType);
            return itemTypeName;
        }
        receiverType = expr->childCount > 0 ? xl6SemExprStaticTypeName(ctx, expr->children[0]) : NULL;
        if ( receiverType != NULL &&
             (xl6SemIsArrayTypeName(receiverType) || xl6SemIsListTypeName(receiverType) ||
              xl6SemIsDictTypeName(receiverType) ||
              xl6TextEquals(receiverType, "var")) ) {
            return "var";
        }
        return NULL;
    }
    case XL6_AST_CALL:
        if ( xl6SemIsImplicitToStringCall(ctx, expr) ) {
            return "string";
        }
        constructedType = expr->childCount > 0 ? xl6SemTypeNodeFromConstructorCallee(ctx, expr->children[0]) : NULL;
        if ( constructedType != NULL ) {
            /*
             * 构造类型节点是临时 AST，不能把节点自身的 text 指针返回给调用者。
             * 这里只返回类型注册表或别名解析得到的稳定名称，用于诊断上下文。
             */
            constructedTypeName = xl6SemStaticTypeNameFromTypeNode(ctx, constructedType);
            xl6AstFree(constructedType);
            if ( constructedTypeName != NULL ) {
                return constructedTypeName;
            }
        }
        {
            const char* threadResultType = xl6SemThreadCallResultTypeName(ctx, expr);
            if ( threadResultType != NULL ) {
                return threadResultType;
            }
        }
        {
            const char* syncResultType = xl6SemSyncCallResultTypeName(ctx, expr);
            if ( syncResultType != NULL ) {
                return syncResultType;
            }
        }
        {
            const char* specialCollectionResultType = xl6SemSpecialCollectionCallResultTypeName(ctx, expr);
            if ( specialCollectionResultType != NULL ) {
                return specialCollectionResultType;
            }
        }
        {
            const char* channelResultType = xl6SemChannelCallResultTypeName(ctx, expr);
            if ( channelResultType != NULL ) {
                return channelResultType;
            }
        }
        {
            const char* futureResultType = xl6SemFutureCallResultTypeName(ctx, expr);
            if ( futureResultType != NULL ) {
                return futureResultType;
            }
        }
        {
            const char* taskResultType = xl6SemTaskCallResultTypeName(ctx, expr);
            if ( taskResultType != NULL ) {
                return taskResultType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_MEMBER ) {
            XL6_AstNode* containerCallee = expr->children[0];
            if ( containerCallee->childCount > 0 ) {
                XL6_AstNode* receiverType = xl6SemInferAutoTypeNode(ctx, containerCallee->children[0]);
                int isContainerMethod = xl6SemMemberIsContainerMethod(ctx, receiverType, containerCallee->text);
                if ( isContainerMethod ) {
                    if ( xl6TextEquals(containerCallee->text, "pop") ||
                         xl6TextEquals(containerCallee->text, "take") ) {
                        int typed = 0;
                        XL6_SemContainerKind kind = xl6SemContainerKindFromType(ctx, receiverType, &typed);
                        const XL6_AstNode* itemType = (typed && kind == XL6_SEM_CONTAINER_ARRAY)
                            ? xl6SemContainerItemType(ctx, receiverType)
                            : NULL;
                        const char* resultType = itemType != NULL
                            ? xl6SemStaticTypeNameFromTypeNode(ctx, itemType)
                            : NULL;
                        if ( resultType != NULL &&
                             !(xl6SemIsIntegerTypeName(resultType) ||
                               xl6SemIsFloatTypeName(resultType) ||
                               xl6SemIsBoolTypeName(resultType) ||
                               xl6TextEquals(resultType, "string") ||
                               xl6TextEquals(resultType, "time") ||
                               xl6TextEquals(resultType, "point")) ) {
                            resultType = NULL;
                        }
                        xl6AstFree(receiverType);
                        return resultType != NULL ? resultType : "var";
                    }
                    if ( xl6TextEquals(containerCallee->text, "indexOf") ) {
                        xl6AstFree(receiverType);
                        return "int";
                    }
                    xl6AstFree(receiverType);
                    return "bool";
                }
                xl6AstFree(receiverType);
            }
            XL6_AstNode* staticMethod = xl6SemResolveStaticClassFuncCall(ctx, expr, 0);
            if ( staticMethod != NULL && staticMethod->childCount > 2 ) {
                const XL6_AstNode* staticReturns = staticMethod->children[2];
                if ( staticReturns != NULL && staticReturns->childCount == 1 ) {
                    return xl6SemResolveAliasTypeName(ctx, staticReturns->children[0]);
                }
            }
            {
            XL6_AstNode* classReturnType = xl6SemCloneClassMemberCallReturnType(ctx, expr);
            if ( classReturnType != NULL ) {
                const char* classReturnName = xl6SemStaticTypeNameFromTypeNode(ctx, classReturnType);
                xl6AstFree(classReturnType);
                return classReturnName;
            }
            }
            {
            XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, expr->children[0]);
            if ( memberType != NULL && xl6TextEquals(memberType->text, "function") ) {
                const XL6_AstNode* memberReturns = memberType->childCount > 1 ? memberType->children[1] : NULL;
                const char* resultType = NULL;

                if ( memberType->childCount == 0 ) {
                    resultType = "var";
                } else if ( memberReturns != NULL && memberReturns->childCount == 1 ) {
                    resultType = xl6SemStaticTypeNameFromTypeNode(ctx, memberReturns->children[0]);
                }
                xl6AstFree(memberType);
                return resultType;
            }
            if ( memberType != NULL && xl6TextEquals(memberType->text, "var") ) {
                xl6AstFree(memberType);
                return "var";
            }
            xl6AstFree(memberType);
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* instant = expr->children[0];
            XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
            XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
            const XL6_AstNode* persistenceTypeArg = xl6SemPersistenceGenericCallTypeArg(expr);
            if ( persistenceTypeArg != NULL ) {
                return xl6SemResolveAliasTypeName(ctx, persistenceTypeArg);
            }
            if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                func = xl6SemResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
                returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
                if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC && !xl6SemFuncReturnsGenerator(ctx, func) ) {
                    return "future";
                }
                if ( returns != NULL && returns->childCount == 1 ) {
                    const XL6_AstNode* returnType = returns->children[0];
                    const XL6_AstNode* replacement = xl6SemGenericTypeParamArg(
                        xl6SemFuncTypeParams(func),
                        typeArgs,
                        returnType != NULL && returnType->childCount == 0 ? returnType->text : NULL);
                    return xl6SemResolveAliasTypeName(ctx, replacement != NULL ? replacement : returnType);
                }
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
            if ( xl6TextEquals(expr->children[0]->text, "type") ) {
                return "type";
            }
            if ( xl6SemIsBuiltinConvertTypeName(expr->children[0]->text) &&
                 xl6SemNameIsRegisteredType(ctx, expr->children[0]->text) ) {
                return expr->children[0]->text;
            }
            sym = xl6ScopeFind(ctx->scope, expr->children[0]->text);
            if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") &&
                 sym->type->childCount == 0 ) {
                return "var";
            }
            if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") &&
                 sym->type->childCount > 1 && sym->type->children[1] != NULL &&
                 sym->type->children[1]->childCount == 1 ) {
                return xl6SemResolveAliasTypeName(ctx, sym->type->children[1]->children[0]);
            }
            if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "var") ) {
                return "var";
            }
            func = xl6SemResolveCallFuncEx(ctx, expr, expr->children[0]->text, 0);
            returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC && !xl6SemFuncReturnsGenerator(ctx, func) ) {
                return "future";
            }
            if ( returns != NULL && returns->childCount == 1 ) {
                return xl6SemResolveAliasTypeName(ctx, returns->children[0]);
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

static XL6_AstNode* xl6SemCloneTypeNode(const XL6_AstNode* node)
{
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }
    if ( node->text != NULL ) {
        copy = xl6AstNewText(node->kind, node->text, strlen(node->text), NULL);
    } else {
        copy = xl6AstNew(node->kind, NULL);
    }
    if ( copy == NULL ) {
        return NULL;
    }
    copy->line = node->line;
    copy->column = node->column;
    copy->flags = node->flags;
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* childCopy = xl6SemCloneTypeNode(node->children[i]);
        if ( childCopy == NULL && node->children[i] != NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, childCopy);
    }
    return copy;
}

static int xl6SemIsContainerBaseTypeName(const char* name)
{
    return xl6TextEquals(name, "array") ||
           xl6TextEquals(name, "list") ||
           xl6TextEquals(name, "set") ||
           xl6TextEquals(name, "dict");
}

static const XL6_AstNode* xl6SemTypeNodeGenericArgs(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ? typeNode->children[0] : NULL;
}

static int xl6SemIsFutureLikeTypeName(const char* name)
{
    return xl6TextEquals(name, "future") || xl6TextEquals(name, "task");
}

static int xl6SemTypeIsFutureLikeResource(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    return concreteType != NULL && xl6SemIsFutureLikeTypeName(concreteType->text);
}

static int xl6SemIsGeneratorTypeName(const char* name)
{
    return xl6TextEquals(name, "generator");
}

static int xl6SemIsChannelTypeName(const char* name)
{
    return xl6TextEquals(name, "channel");
}

static int xl6SemIsSyncHandleTypeName(const char* name)
{
    return xl6TextEquals(name, "mutex") ||
           xl6TextEquals(name, "sem") ||
           xl6TextEquals(name, "cond") ||
           xl6TextEquals(name, "rwlock");
}

static int xl6SemIsThreadTypeName(const char* name)
{
    return xl6TextEquals(name, "thread");
}

static int xl6SemIsResourceHandleTypeName(const char* name)
{
    return xl6SemIsFutureLikeTypeName(name) ||
           xl6SemIsChannelTypeName(name) ||
           xl6SemIsSyncHandleTypeName(name) ||
           xl6SemIsGeneratorTypeName(name) ||
           xl6SemIsThreadTypeName(name);
}

static XL6_AstNode* xl6SemNewGenericUnaryType(const char* name, XL6_AstNode* arg, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* args;

    if ( name == NULL || arg == NULL ) {
        xl6AstFree(arg);
        return NULL;
    }

    typeNode = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( typeNode == NULL || args == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(args);
        xl6AstFree(arg);
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    xl6AstAddChild(args, arg);
    xl6AstAddChild(typeNode, args);
    return typeNode;
}

static XL6_AstNode* xl6SemCloneSingleGenericArgType(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* baseName)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL || !xl6TextEquals(concreteType->text, baseName) ) {
        return NULL;
    }
    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(args->children[0]);
}

static int xl6SemIsChannelGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT ) {
        return 0;
    }
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeArgs = node->childCount > 1 ? node->children[1] : NULL;
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6TextEquals(nameNode->text, "channel") &&
           typeArgs != NULL &&
           typeArgs->childCount == 1;
}

static int xl6SemChannelElementTypeIsFirstStageSafe(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( name == NULL || (concreteType != NULL && concreteType->childCount > 0) ) {
        return 0;
    }
    return xl6TextEquals(name, "bool") ||
           xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) ||
           xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "time") ||
           xl6TextEquals(name, "point");
}

static int xl6SemChannelElementTypeIsMessageSafe(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( name == NULL || (concreteType != NULL && concreteType->childCount > 0) ) {
        return 0;
    }
    return xl6TextEquals(name, "var") ||
           xl6SemChannelElementTypeIsFirstStageSafe(ctx, typeNode);
}

static int xl6SemCheckChannelTypeArg(XL6_SemContext* ctx, const XL6_AstNode* callee)
{
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* itemType;

    if ( !xl6SemIsChannelGenericInstant(callee) ) {
        return 0;
    }
    typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
    itemType = typeArgs != NULL && typeArgs->childCount == 1 ? typeArgs->children[0] : NULL;
    if ( itemType == NULL || !xl6SemChannelElementTypeIsMessageSafe(ctx, itemType) ) {
        xl6SemErrorNode(ctx, itemType != NULL ? itemType : callee, "channel<T> 当前只支持 bool、整数、浮点、string、time、point 等具备稳定消息复制语义的元素类型");
        return 0;
    }
    return 1;
}

static int xl6SemCheckChannelCtorCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* args;
    XL6_AstNode intType;
    size_t i;
    int ok;

    ok = 1;
    if ( node == NULL || node->childCount == 0 ) {
        ok = 0;
    }
    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    if ( args == NULL ) {
        return ok;
    }
    if ( args->childCount > 1 ) {
        XL6_AstNode* errNode = args->children[1] != NULL ? args->children[1] : (args->children[0] != NULL ? args->children[0] : node);
        if ( errNode != NULL && errNode->line <= 0 && errNode->childCount > 0 && errNode->children[0] != NULL ) {
            errNode = errNode->children[0];
        }
        xl6SemErrorNode(ctx, errNode != NULL ? errNode : args, "channel 构造函数最多接收一个容量参数");
        ok = 0;
    }
    memset(&intType, 0, sizeof(intType));
    intType.kind = XL6_AST_TYPE;
    intType.text = "int";
    for ( i = 0; i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        if ( arg != NULL && arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, arg, "channel 构造函数不接受命名参数");
            ok = 0;
            continue;
        }
        if ( expr != NULL && !xl6SemExprCanMatchParamType(ctx, expr, &intType) ) {
            xl6SemErrorNode(ctx, expr, "channel 容量参数必须是整数");
            ok = 0;
        }
    }
    return ok;
}

static XL6_AstNode* xl6SemCloneChannelReceiverType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return xl6SemInferAutoTypeNode(ctx, receiver);
    }
    sym = xl6ScopeFind(ctx->scope, receiver->text);
    if ( sym == NULL || sym->type == NULL ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(sym->type);
}

static int xl6SemCheckChannelMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* itemType;
    XL6_AstNode* args;
    const XL6_AstNode* concreteReceiverType;
    size_t argCount;
    int ok;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return -1;
    }
    member = node->children[0];
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6SemCloneChannelReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsChannelTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return -1;
    }

    ok = 1;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, receiver != NULL ? receiver : member, "channel receiver 当前只支持具名变量");
        ok = 0;
    }
    args = node->childCount > 1 ? node->children[1] : NULL;
    argCount = args != NULL ? args->childCount : 0;

    if ( xl6TextEquals(member->text, "close") ||
         xl6TextEquals(member->text, "count") ||
         xl6TextEquals(member->text, "isClosed") ) {
        if ( argCount != 0 ) {
        xl6SemErrorNode(ctx, args, "该 channel 方法不接收参数");
            ok = 0;
        }
        xl6AstFree(receiverType);
        return ok;
    }

    if ( xl6TextEquals(member->text, "send") ||
         xl6TextEquals(member->text, "trySend") ) {
        XL6_AstNode* arg = argCount == 1 ? args->children[0] : NULL;
        XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        itemType = xl6SemCloneSingleGenericArgType(ctx, receiverType, "channel");
        if ( argCount != 1 || arg == NULL || arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, node, "channel.send 必须传入一个位置参数");
            ok = 0;
        } else if ( itemType == NULL || !xl6SemExprCanMatchParamType(ctx, expr, itemType) ) {
            xl6SemTypeMismatchError(
                ctx,
                expr != NULL ? expr : arg,
                "channel.send 参数类型必须匹配 channel<T> 的元素类型",
                itemType,
                xl6SemExprStaticTypeName(ctx, expr));
            ok = 0;
        }
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return ok;
    }

    if ( xl6TextEquals(member->text, "recvInto") ||
         xl6TextEquals(member->text, "tryRecvInto") ||
         xl6TextEquals(member->text, "recvTimeoutInto") ) {
        int isTimeout = xl6TextEquals(member->text, "recvTimeoutInto");
        const char* methodName = isTimeout ? "channel.recvTimeoutInto" :
            (xl6TextEquals(member->text, "tryRecvInto") ? "channel.tryRecvInto" : "channel.recvInto");
        size_t expectedCount = isTimeout ? 2u : 1u;
        char message[160];
        XL6_AstNode* arg = argCount > 0 ? args->children[0] : NULL;
        XL6_AstNode* timeoutArg = isTimeout && argCount > 1 ? args->children[1] : NULL;
        XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        XL6_AstNode* timeoutExpr = timeoutArg != NULL && timeoutArg->childCount > 0 ? timeoutArg->children[0] : NULL;
        XL6_Symbol* targetSym = expr != NULL && expr->kind == XL6_AST_NAME ? xl6ScopeFind(ctx->scope, expr->text) : NULL;
        itemType = xl6SemCloneSingleGenericArgType(ctx, receiverType, "channel");
        if ( argCount != expectedCount ||
             arg == NULL || arg->kind != XL6_AST_ARG ||
             expr == NULL || expr->kind != XL6_AST_NAME ||
             (isTimeout && (timeoutArg == NULL || timeoutArg->kind != XL6_AST_ARG || timeoutExpr == NULL)) ) {
            snprintf(message, sizeof(message), "%s requires positional out variable%s",
                     methodName, isTimeout ? " and timeoutMs" : "");
            xl6SemErrorNode(ctx, node, message);
            ok = 0;
        } else if ( targetSym == NULL || targetSym->type == NULL ) {
            snprintf(message, sizeof(message), "%s target variable is not declared", methodName);
            xl6SemErrorNode(ctx, expr, message);
            ok = 0;
        } else if ( xl6SemSymbolIsConst(targetSym) ) {
            xl6SemErrorNode(ctx, expr, "const cannot be reassigned");
            ok = 0;
        } else if ( !xl6SemCheckBoundarySymbolUse(ctx, expr, targetSym, 1) ) {
            ok = 0;
        } else if ( itemType == NULL || !xl6SemTypeEquals(ctx, targetSym->type, itemType) ) {
            snprintf(message, sizeof(message), "%s target variable type must equal channel<T> item type", methodName);
            xl6SemTypeMismatchError(
                ctx,
                expr,
                message,
                itemType,
                xl6SemDisplayTypeName(ctx, targetSym->type));
            ok = 0;
        } else if ( isTimeout && !xl6SemIsIntegerTypeName(xl6SemExprStaticTypeName(ctx, timeoutExpr)) ) {
            xl6SemErrorNode(ctx, timeoutExpr, "channel.recvTimeoutInto timeoutMs must be int");
            ok = 0;
        }
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return ok;
    }

    xl6SemErrorNode(ctx, member, "unknown channel method");
    xl6AstFree(receiverType);
    return 0;
}

static int xl6SemCheckChannelCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    if ( node != NULL && node->childCount > 0 && xl6SemIsChannelGenericInstant(node->children[0]) ) {
        return xl6SemCheckChannelCtorCall(ctx, node);
    }
    return xl6SemCheckChannelMethodCall(ctx, node);
}

static const char* xl6SemChannelCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const char* result;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    if ( xl6SemIsChannelGenericInstant(node->children[0]) ) {
        return "channel";
    }
    member = node->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiverType = xl6SemCloneChannelReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsChannelTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6TextEquals(member->text, "send") ||
         xl6TextEquals(member->text, "trySend") ||
         xl6TextEquals(member->text, "recvInto") ||
         xl6TextEquals(member->text, "tryRecvInto") ||
         xl6TextEquals(member->text, "recvTimeoutInto") ||
         xl6TextEquals(member->text, "isClosed") ) {
        result = "bool";
    } else if ( xl6TextEquals(member->text, "count") ) {
        result = "int";
    }
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6SemCloneFutureReceiverType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    return xl6SemInferAutoTypeNode(ctx, receiver);
}

static int xl6SemFutureMethodArgCount(const char* name)
{
    if ( xl6TextEquals(name, "waitTimeout") ||
         xl6TextEquals(name, "waitUntil") ||
         xl6TextEquals(name, "setDebugName") ||
         xl6TextEquals(name, "then") ||
         xl6TextEquals(name, "catch") ||
         xl6TextEquals(name, "finally") ) {
        return 1;
    }
    if ( xl6TextEquals(name, "wait") ||
         xl6TextEquals(name, "cancel") ||
         xl6TextEquals(name, "state") ||
         xl6TextEquals(name, "status") ||
         xl6TextEquals(name, "isDone") ||
         xl6TextEquals(name, "isPending") ||
         xl6TextEquals(name, "isCancelled") ||
         xl6TextEquals(name, "isFailed") ||
         xl6TextEquals(name, "error") ||
         xl6TextEquals(name, "debugName") ||
         xl6TextEquals(name, "createTimeMs") ||
         xl6TextEquals(name, "completeTimeMs") ||
         xl6TextEquals(name, "pendingContinuations") ) {
        return 0;
    }
    return -1;
}

static XL6_AstNode* xl6SemFunctionTypeFromCallableParts(
    const XL6_AstNode* params,
    const XL6_AstNode* returns,
    const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* paramTypes;
    XL6_AstNode* returnTypes;
    size_t i;

    typeNode = xl6AstNewText(XL6_AST_TYPE, "function", 8u, NULL);
    paramTypes = xl6AstNew(XL6_AST_LIST, NULL);
    returnTypes = returns != NULL
        ? xl6SemCloneTypeNode(returns)
        : xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( typeNode == NULL || paramTypes == NULL || returnTypes == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(paramTypes);
        xl6AstFree(returnTypes);
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        XL6_AstNode* paramType = param != NULL && param->childCount > 0
            ? xl6SemCloneTypeNode(param->children[0])
            : NULL;
        if ( paramType == NULL ) {
            xl6AstFree(typeNode);
            xl6AstFree(paramTypes);
            xl6AstFree(returnTypes);
            return NULL;
        }
        xl6AstAddChild(paramTypes, paramType);
    }
    xl6AstAddChild(typeNode, paramTypes);
    xl6AstAddChild(typeNode, returnTypes);
    return typeNode;
}

static XL6_AstNode* xl6SemNewFutureCallbackType(
    const XL6_AstNode* paramType,
    const XL6_AstNode* returnType,
    const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* params;
    XL6_AstNode* returns;

    typeNode = xl6AstNewText(XL6_AST_TYPE, "function", 8u, NULL);
    params = xl6AstNew(XL6_AST_LIST, NULL);
    returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( typeNode == NULL || params == NULL || returns == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(params);
        xl6AstFree(returns);
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    if ( paramType != NULL ) {
        xl6AstAddChild(params, xl6SemCloneTypeNode(paramType));
    }
    if ( returnType != NULL ) {
        xl6AstAddChild(returns, xl6SemCloneTypeNode(returnType));
    }
    xl6AstAddChild(typeNode, params);
    xl6AstAddChild(typeNode, returns);
    return typeNode;
}

static XL6_AstNode* xl6SemResolveFutureNamedCallbackType(
    XL6_SemContext* ctx,
    XL6_AstNode* callbackExpr,
    int kind,
    const XL6_AstNode* inputType,
    int* diagnosed)
{
    XL6_AstNode errorType;
    XL6_AstNode* selectedType;
    int candidateCount;
    int matchCount;
    size_t i;

    if ( diagnosed != NULL ) {
        *diagnosed = 0;
    }
    if ( ctx == NULL || ctx->root == NULL || callbackExpr == NULL ||
         callbackExpr->kind != XL6_AST_NAME || callbackExpr->text == NULL ||
         inputType == NULL ) {
        return NULL;
    }

    memset(&errorType, 0, sizeof(errorType));
    errorType.kind = XL6_AST_TYPE;
    errorType.text = "error";
    selectedType = NULL;
    candidateCount = 0;
    matchCount = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* func = ctx->root->children[i];
        const XL6_AstNode* params;
        const XL6_AstNode* returns;
        const XL6_AstNode* outputType;
        XL6_AstNode* expectedType;

        if ( func == NULL ||
             (func->kind != XL6_AST_FUNC && func->kind != XL6_AST_EXTERN_FUNC &&
              func->kind != XL6_AST_EXPORT_FUNC) ||
             xl6SemFuncIsGeneric(func) ||
             !xl6TextEquals(xl6SemFuncName(func), callbackExpr->text) ) {
            continue;
        }
        candidateCount += 1;
        params = func->childCount > 1 ? func->children[1] : NULL;
        returns = func->childCount > 2 ? func->children[2] : NULL;
        outputType = kind == 1 && returns != NULL && returns->childCount == 1
            ? returns->children[0]
            : (kind == 2 ? inputType : NULL);
        if ( kind == 1 && outputType == NULL ) {
            continue;
        }
        expectedType = xl6SemNewFutureCallbackType(
            kind == 1 ? inputType : (kind == 2 ? &errorType : NULL),
            outputType,
            callbackExpr);
        if ( expectedType == NULL ) {
            continue;
        }
        if ( !xl6SemFunctionTypeMatchesParts(ctx, expectedType, params, returns) ) {
            xl6AstFree(expectedType);
            continue;
        }
        matchCount += 1;
        if ( matchCount == 1 ) {
            selectedType = expectedType;
        } else {
            xl6AstFree(expectedType);
            xl6AstFree(selectedType);
            selectedType = NULL;
        }
    }

    if ( matchCount == 1 ) {
        return selectedType;
    }
    if ( diagnosed != NULL ) {
        *diagnosed = 1;
    }
    xl6SemErrorNode(
        ctx,
        callbackExpr,
        candidateCount == 0
            ? "future continuation callback function was not found"
            : (matchCount == 0
                ? (kind == 1
                    ? "future.then named callback must have one matching parameter and one return value"
                    : (kind == 2
                        ? "future.catch named callback must match func(error) as T"
                        : "future.finally named callback must match func()"))
                : "future continuation callback is ambiguous; use an explicitly typed func value"));
    return NULL;
}

static int xl6SemCheckFutureContinuation(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    XL6_AstNode* member,
    const XL6_AstNode* receiverType)
{
    XL6_AstNode errorType;
    XL6_AstNode* callbackExpr;
    XL6_AstNode* callbackType;
    XL6_AstNode* inputType;
    XL6_AstNode* outputType;
    XL6_AstNode* expectedType;
    XL6_AstNode* inferredType;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int kind;
    int ok;
    int diagnosed;

    kind = xl6TextEquals(member->text, "then") ? 1 :
           (xl6TextEquals(member->text, "catch") ? 2 :
            (xl6TextEquals(member->text, "finally") ? 3 : 0));
    if ( kind == 0 ) {
        return 1;
    }

    callbackExpr = xl6SemCallArgExprAt(call, 0);
    callbackType = xl6SemInferAutoTypeNode(ctx, callbackExpr);
    inputType = xl6SemCloneFutureValueTypeFromType(ctx, receiverType);
    diagnosed = 0;
    if ( (callbackType == NULL || !xl6TextEquals(callbackType->text, "function") ||
          callbackType->childCount < 2) &&
         callbackExpr != NULL && callbackExpr->kind == XL6_AST_NAME ) {
        xl6AstFree(callbackType);
        callbackType = xl6SemResolveFutureNamedCallbackType(
            ctx,
            callbackExpr,
            kind,
            inputType,
            &diagnosed);
        if ( callbackType != NULL && callbackExpr->inferredType == NULL ) {
            callbackExpr->inferredType = xl6SemCloneTypeNode(callbackType);
        }
    }
    params = callbackType != NULL && callbackType->childCount > 0 ? callbackType->children[0] : NULL;
    returns = callbackType != NULL && callbackType->childCount > 1 ? callbackType->children[1] : NULL;
    if ( callbackType == NULL || !xl6TextEquals(callbackType->text, "function") || inputType == NULL ) {
        if ( !diagnosed ) {
            xl6SemErrorNode(ctx, callbackExpr != NULL ? callbackExpr : call, "future continuation requires a complete func value");
        }
        xl6AstFree(callbackType);
        xl6AstFree(inputType);
        return 0;
    }

    outputType = NULL;
    expectedType = NULL;
    if ( kind == 1 ) {
        if ( params == NULL || params->childCount != 1 || returns == NULL || returns->childCount != 1 ) {
            xl6SemErrorNode(ctx, callbackExpr, "future.then requires func(T) as U");
        } else {
            outputType = xl6SemCloneTypeNode(returns->children[0]);
            expectedType = xl6SemNewFutureCallbackType(inputType, outputType, callbackExpr);
        }
    } else if ( kind == 2 ) {
        memset(&errorType, 0, sizeof(errorType));
        errorType.kind = XL6_AST_TYPE;
        errorType.text = "error";
        outputType = xl6SemCloneTypeNode(inputType);
        expectedType = xl6SemNewFutureCallbackType(&errorType, inputType, callbackExpr);
    } else {
        outputType = xl6SemCloneTypeNode(inputType);
        expectedType = xl6SemNewFutureCallbackType(NULL, NULL, callbackExpr);
    }

    ok = expectedType != NULL && outputType != NULL &&
         xl6SemExprCanMatchFunctionTypeEx(ctx, callbackExpr, expectedType, 0);
    if ( !ok && expectedType != NULL ) {
        xl6SemErrorNode(ctx, callbackExpr,
                        kind == 1 ? "future.then callback must match func(T) as U" :
                        (kind == 2 ? "future.catch callback must match func(error) as T" :
                         "future.finally callback must match func()"));
    }

    if ( ok && call->inferredType == NULL ) {
        inferredType = kind == 1
            ? xl6SemNewGenericContainerType("future", xl6SemCloneTypeNode(outputType), call)
            : xl6SemCloneTypeNode(receiverType);
        call->inferredType = inferredType;
    }

    xl6AstFree(expectedType);
    xl6AstFree(outputType);
    xl6AstFree(inputType);
    xl6AstFree(callbackType);
    return ok;
}

static int xl6SemCheckFutureCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode intType;
    const XL6_AstNode* concreteReceiverType;
    int expectedCount;
    int isContinuation;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return -1;
    }
    member = node->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return -1;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6SemCloneFutureReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsFutureLikeTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return -1;
    }
    isContinuation = xl6TextEquals(member->text, "then") ||
                     xl6TextEquals(member->text, "catch") ||
                     xl6TextEquals(member->text, "finally");
    if ( (receiver == NULL || receiver->kind != XL6_AST_NAME) && !isContinuation ) {
        xl6SemErrorNode(ctx, receiver != NULL ? receiver : member, "future receiver 当前只支持具名变量");
        xl6AstFree(receiverType);
        return 0;
    }

    expectedCount = xl6SemFutureMethodArgCount(member->text);
    if ( expectedCount < 0 ) {
        xl6SemErrorNode(ctx, member, "unknown future method");
        xl6AstFree(receiverType);
        return 0;
    }
    if ( xl6SemCallArgCount(node) != expectedCount ) {
        xl6SemCountMismatchError(ctx, node, "future method argument count mismatch", (size_t)expectedCount, (size_t)xl6SemCallArgCount(node));
        xl6AstFree(receiverType);
        return 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "future method does not accept named arguments") ) {
        xl6AstFree(receiverType);
        return 0;
    }
    if ( isContinuation && !xl6SemCheckFutureContinuation(ctx, node, member, receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }
    memset(&intType, 0, sizeof(intType));
    intType.kind = XL6_AST_TYPE;
    intType.text = "int";
    if ( (xl6TextEquals(member->text, "waitTimeout") ||
          xl6TextEquals(member->text, "waitUntil")) &&
         !xl6SemExprCanMatchParamType(ctx, xl6SemCallArgExprAt(node, 0), &intType) ) {
        xl6SemTypeMismatchError(ctx,
                                xl6SemCallArgExprAt(node, 0),
                    "future.waitTimeout 参数必须是整数毫秒",
                                &intType,
                                xl6SemExprStaticTypeName(ctx, xl6SemCallArgExprAt(node, 0)));
        xl6AstFree(receiverType);
        return 0;
    }
    if ( xl6TextEquals(member->text, "setDebugName") ) {
        XL6_AstNode stringType;
        memset(&stringType, 0, sizeof(stringType));
        stringType.kind = XL6_AST_TYPE;
        stringType.text = "string";
        if ( !xl6SemExprCanMatchParamType(ctx, xl6SemCallArgExprAt(node, 0), &stringType) ) {
            xl6SemTypeMismatchError(ctx,
                                    xl6SemCallArgExprAt(node, 0),
                                    "future.setDebugName parameter must be string",
                                    &stringType,
                                    xl6SemExprStaticTypeName(ctx, xl6SemCallArgExprAt(node, 0)));
            xl6AstFree(receiverType);
            return 0;
        }
    }
    xl6AstFree(receiverType);
    return 1;
}

static const char* xl6SemFutureCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const char* result;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return NULL;
    }
    member = node->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }
    receiverType = xl6SemCloneFutureReceiverType(ctx, member);
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteReceiverType == NULL || !xl6SemIsFutureLikeTypeName(concreteReceiverType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6TextEquals(member->text, "wait") ||
         xl6TextEquals(member->text, "waitTimeout") ||
         xl6TextEquals(member->text, "waitUntil") ||
         xl6TextEquals(member->text, "setDebugName") ||
         xl6TextEquals(member->text, "cancel") ||
         xl6TextEquals(member->text, "isDone") ||
         xl6TextEquals(member->text, "isPending") ||
         xl6TextEquals(member->text, "isCancelled") ||
         xl6TextEquals(member->text, "isFailed") ) {
        result = "bool";
    } else if ( xl6TextEquals(member->text, "state") ||
                xl6TextEquals(member->text, "status") ||
                xl6TextEquals(member->text, "createTimeMs") ||
                xl6TextEquals(member->text, "completeTimeMs") ||
                xl6TextEquals(member->text, "pendingContinuations") ) {
        result = "int";
    } else if ( xl6TextEquals(member->text, "error") ||
                xl6TextEquals(member->text, "debugName") ) {
        result = "string";
    } else if ( xl6TextEquals(member->text, "then") ||
                xl6TextEquals(member->text, "catch") ||
                xl6TextEquals(member->text, "finally") ) {
        result = "future";
    }
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemIsTaskNamespaceMemberCall(const XL6_AstNode* call, const char** outName)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;

    if ( outName != NULL ) {
        *outName = NULL;
    }
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME || !xl6TextEquals(receiver->text, "task") ) {
        return 0;
    }
    if ( outName != NULL ) {
        *outName = member->text;
    }
    return 1;
}

static XL6_AstNode* xl6SemCloneFutureValueTypeFromType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* result;

    result = xl6SemCloneSingleGenericArgType(ctx, typeNode, "future");
    if ( result == NULL ) {
        result = xl6SemCloneSingleGenericArgType(ctx, typeNode, "task");
    }
    return result;
}

static XL6_AstNode* xl6SemCloneTaskArrayItemFutureValueType(XL6_SemContext* ctx, XL6_AstNode* argExpr)
{
    XL6_AstNode* argType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    XL6_AstNode* result;
    size_t i;

    if ( argExpr == NULL ) {
        return NULL;
    }

    if ( argExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        if ( argExpr->childCount == 0 ) {
            return NULL;
        }
        result = NULL;
        for ( i = 0; i < argExpr->childCount; ++i ) {
            XL6_AstNode* itemType = xl6SemInferAutoTypeNode(ctx, argExpr->children[i]);
            XL6_AstNode* valueType = xl6SemCloneFutureValueTypeFromType(ctx, itemType);
            xl6AstFree(itemType);
            if ( valueType == NULL ) {
                xl6AstFree(result);
                return NULL;
            }
            if ( result == NULL ) {
                result = valueType;
            } else if ( xl6SemTypeEquals(ctx, result, valueType) ) {
                xl6AstFree(valueType);
            } else {
                xl6AstFree(result);
                xl6AstFree(valueType);
                return NULL;
            }
        }
        return result;
    }

    argType = xl6SemInferAutoTypeNode(ctx, argExpr);
    concreteType = xl6SemResolveAliasType(ctx, argType);
    if ( concreteType == NULL || !xl6TextEquals(concreteType->text, "array") ) {
        xl6AstFree(argType);
        return NULL;
    }
    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        xl6AstFree(argType);
        return NULL;
    }
    result = xl6SemCloneFutureValueTypeFromType(ctx, args->children[0]);
    xl6AstFree(argType);
    return result;
}

static int xl6SemIsTaskAggregateApiName(const char* name)
{
    return xl6TextEquals(name, "all") || xl6TextEquals(name, "race");
}

static int xl6SemIsTaskWaitApiName(const char* name)
{
    return xl6TextEquals(name, "whenAny");
}

static int xl6SemIsKnownTaskApiName(const char* name)
{
    return xl6TextEquals(name, "spawn") ||
           xl6TextEquals(name, "cancel") ||
           xl6SemIsTaskWaitApiName(name) ||
           xl6SemIsTaskAggregateApiName(name);
}

static int xl6SemTaskExprIsFutureLike(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    int result;

    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    result = concreteType != NULL && xl6SemIsFutureLikeTypeName(concreteType->text);
    xl6AstFree(typeNode);
    return result;
}

static int xl6SemTaskArgIsFutureCollection(XL6_SemContext* ctx, XL6_AstNode* argExpr)
{
    XL6_AstNode* argType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    size_t i;
    int result;

    if ( argExpr == NULL ) {
        return 0;
    }
    if ( argExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        if ( argExpr->childCount == 0 ) {
            return 0;
        }
        for ( i = 0; i < argExpr->childCount; ++i ) {
            if ( !xl6SemTaskExprIsFutureLike(ctx, argExpr->children[i]) ) {
                return 0;
            }
        }
        return 1;
    }

    argType = xl6SemInferAutoTypeNode(ctx, argExpr);
    concreteType = xl6SemResolveAliasType(ctx, argType);
    result = 0;
    if ( concreteType != NULL && xl6TextEquals(concreteType->text, "array") ) {
        args = xl6SemTypeNodeGenericArgs(concreteType);
        if ( args != NULL && args->childCount == 1 ) {
            XL6_AstNode* futureValueType = xl6SemCloneFutureValueTypeFromType(ctx, args->children[0]);
            result = futureValueType != NULL;
            xl6AstFree(futureValueType);
        }
    }
    xl6AstFree(argType);
    return result;
}

static XL6_AstNode* xl6SemCloneTaskCallType(XL6_SemContext* ctx, XL6_AstNode* call)
{
    const char* name;
    XL6_AstNode* argExpr;
    XL6_AstNode* funcType;
    XL6_AstNode* valueType;
    XL6_AstNode* arrayType;
    const XL6_AstNode* concreteValueType;

    if ( !xl6SemIsTaskNamespaceMemberCall(call, &name) ) {
        return NULL;
    }
    if ( xl6TextEquals(name, "cancel") ) {
        return xl6AstNewText(XL6_AST_TYPE, "bool", 4, NULL);
    }
    if ( xl6SemIsTaskWaitApiName(name) ) {
        argExpr = xl6SemCallArgExprAt(call, 0);
        if ( !xl6SemTaskArgIsFutureCollection(ctx, argExpr) ) {
            return NULL;
        }
        valueType = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
        return xl6SemNewGenericUnaryType("future", valueType, call);
    }
    if ( xl6TextEquals(name, "spawn") ) {
        const XL6_AstNode* returns;

        argExpr = xl6SemCallArgExprAt(call, 0);
        funcType = xl6SemInferAutoTypeNode(ctx, argExpr);
        if ( funcType == NULL || !xl6TextEquals(funcType->text, "function") || funcType->childCount < 2 ) {
            xl6AstFree(funcType);
            return NULL;
        }
        returns = funcType->children[1];
        if ( returns == NULL || returns->childCount > 1 ) {
            xl6AstFree(funcType);
            return NULL;
        }
        if ( returns->childCount == 0 ) {
            valueType = xl6AstNewText(XL6_AST_TYPE, "null", 4, NULL);
        } else {
            valueType = xl6SemCloneTypeNode(returns->children[0]);
        }
        xl6AstFree(funcType);
        return xl6SemNewGenericUnaryType("future", valueType, call);
    }
    if ( !xl6SemIsTaskAggregateApiName(name) ) {
        return NULL;
    }
    argExpr = xl6SemCallArgExprAt(call, 0);
    valueType = xl6SemCloneTaskArrayItemFutureValueType(ctx, argExpr);
    if ( valueType == NULL ) {
        return NULL;
    }
    if ( xl6TextEquals(name, "race") ) {
        return xl6SemNewGenericUnaryType("future", valueType, call);
    }
    concreteValueType = xl6SemResolveAliasType(ctx, valueType);
    if ( concreteValueType != NULL && xl6TextEquals(concreteValueType->text, "null") ) {
        return xl6SemNewGenericUnaryType("future", valueType, call);
    }
    arrayType = xl6SemNewGenericUnaryType("array", valueType, call);
    return xl6SemNewGenericUnaryType("future", arrayType, call);
}

static int xl6SemCheckTaskSpawnCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* args;
    XL6_AstNode* fnExpr;
    XL6_AstNode* fnType;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    size_t paramCount;
    size_t requiredCount;
    size_t fixedCount;
    size_t actualCount;
    size_t i;
    int hasArrayVararg;
    int hasDictVararg;
    int ok;

    if ( xl6SemCallArgCount(node) < 1 ) {
        xl6SemCountMismatchError(ctx, node, "task.spawn requires at least a func argument", 1, (size_t)xl6SemCallArgCount(node));
        return 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "task.spawn does not accept named arguments") ) {
        return 0;
    }

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    fnExpr = xl6SemCallArgExprAt(node, 0);
    fnType = xl6SemInferAutoTypeNode(ctx, fnExpr);
    if ( fnType == NULL || !xl6TextEquals(fnType->text, "function") || fnType->childCount < 2 ) {
        xl6SemErrorNode(ctx, fnExpr != NULL ? fnExpr : node, "task.spawn first argument must be a func value with a complete signature");
        xl6AstFree(fnType);
        return 0;
    }

    params = fnType->children[0];
    returns = fnType->children[1];
    if ( returns == NULL || returns->childCount > 1 ) {
        xl6SemErrorNode(ctx, fnExpr != NULL ? fnExpr : node, "task.spawn currently supports only no-return or single-return func values");
        xl6AstFree(fnType);
        return 0;
    }
    if ( returns->childCount == 1 ) {
        returnType = xl6SemResolveAliasType(ctx, returns->children[0]);
        if ( xl6TextEquals(returnType != NULL ? returnType->text : NULL, "var") ) {
            xl6SemErrorNode(ctx, returns->children[0], "task.spawn return type cannot be var in the first XRT future bridge");
            xl6AstFree(fnType);
            return 0;
        }
        if ( xl6SemIsContainerBaseTypeName(returnType != NULL ? returnType->text : NULL) ) {
            xl6SemErrorNode(ctx, returns->children[0], "task.spawn return type must be supported by the current XRT xvalue call bridge");
            xl6AstFree(fnType);
            return 0;
        }
        if ( !xl6SemTypeSupportsTaskSpawnXValueBridge(ctx, returns->children[0]) ) {
            xl6SemErrorNode(ctx, returns->children[0], "task.spawn return type must be supported by the current XRT xvalue call bridge");
            xl6AstFree(fnType);
            return 0;
        }
    }

    paramCount = params != NULL ? params->childCount : 0;
    hasArrayVararg = paramCount > 0 && xl6SemTypeIsArrayVararg(params->children[paramCount - 1]);
    hasDictVararg = paramCount > 0 && xl6SemTypeIsDictVararg(params->children[paramCount - 1]);
    if ( hasDictVararg ) {
        xl6SemErrorNode(ctx, fnExpr, "task.spawn does not support *** func values yet");
        xl6AstFree(fnType);
        return 0;
    }
    fixedCount = (hasArrayVararg || hasDictVararg) ? paramCount - 1 : paramCount;
    /* task.spawn 读取的是 func 类型签名。签名中的 array<var> 只表示 ... 尾部，不是必填参数。 */
    requiredCount = fixedCount;
    actualCount = xl6SemCallArgCount(node) > 0 ? (size_t)xl6SemCallArgCount(node) - 1 : 0;
    ok = 1;
    if ( actualCount < requiredCount || (actualCount > fixedCount && !hasArrayVararg) ) {
        size_t expectedCount = actualCount < requiredCount ? requiredCount : fixedCount;
        xl6SemCountMismatchError(ctx, node, "task.spawn argument count does not match func signature", expectedCount + 1, actualCount + 1);
        ok = 0;
    }

    for ( i = 0; args != NULL && i < actualCount; ++i ) {
        XL6_AstNode* arg = args->children[i + 1];
        XL6_AstNode* argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        const XL6_AstNode* paramType = i < fixedCount ? params->children[i] : NULL;
        const XL6_AstNode* concreteParamType = paramType != NULL ? xl6SemResolveAliasType(ctx, paramType) : NULL;

        if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
            xl6SemErrorNode(ctx, argExpr, "multi-return call cannot be passed as one task.spawn argument");
            ok = 0;
            continue;
        }
        if ( concreteParamType != NULL && xl6SemIsContainerBaseTypeName(concreteParamType->text) ) {
            xl6SemErrorNode(ctx, paramType, "task.spawn parameter type must be supported by the current XRT xvalue call bridge");
            ok = 0;
            continue;
        }
        if ( paramType != NULL && !xl6SemTypeSupportsTaskSpawnXValueBridge(ctx, paramType) ) {
            xl6SemErrorNode(ctx, paramType, "task.spawn parameter type must be supported by the current XRT xvalue call bridge");
            ok = 0;
            continue;
        }
        if ( paramType != NULL && !xl6SemExprCanMatchParamType(ctx, argExpr, paramType) ) {
            xl6SemTypeMismatchError(ctx, argExpr, "task.spawn argument type does not match func signature", paramType, xl6SemExprStaticTypeName(ctx, argExpr));
            ok = 0;
        }
        if ( !xl6SemExprSupportsTaskSpawnXValueBridge(ctx, argExpr) ) {
            xl6SemVarBoxMismatchError(ctx, argExpr, "task.spawn argument must be supported by the current XRT xvalue call bridge");
            ok = 0;
        }
    }

    xl6AstFree(fnType);
    return ok;
}

static int xl6SemCheckTaskCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* name;
    XL6_AstNode* argExpr;
    XL6_AstNode* argType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* valueType;
    XL6_AstNode* callee;
    int ok;

    if ( !xl6SemIsTaskNamespaceMemberCall(node, &name) ) {
        return -1;
    }

    if ( xl6TextEquals(name, "spawn") ) {
        return xl6SemCheckTaskSpawnCall(ctx, node);
    }
    if ( !xl6SemIsKnownTaskApiName(name) ) {
        callee = node != NULL && node->childCount > 0 ? node->children[0] : node;
        xl6SemErrorNode(ctx, callee, "standard library namespace has no such symbol");
        return 0;
    }

    ok = 1;
    if ( xl6SemCallArgCount(node) != 1 ) {
        xl6SemCountMismatchError(ctx, node, "task API 需要一个参数", 1, (size_t)xl6SemCallArgCount(node));
        ok = 0;
    }
    if ( !xl6SemCallArgsArePositional(ctx, node, "task API 不接受命名参数") ) {
        ok = 0;
    }
    if ( !ok ) {
        return 0;
    }

    argExpr = xl6SemCallArgExprAt(node, 0);
    if ( xl6TextEquals(name, "cancel") ) {
        argType = xl6SemInferAutoTypeNode(ctx, argExpr);
        concreteType = xl6SemResolveAliasType(ctx, argType);
        if ( concreteType == NULL || !xl6SemIsFutureLikeTypeName(concreteType->text) ) {
            xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : node, "task.cancel 参数必须是 future<T> 或 task<T>");
            ok = 0;
        }
        xl6AstFree(argType);
        return ok;
    }
    if ( xl6SemIsTaskWaitApiName(name) ) {
        if ( !xl6SemTaskArgIsFutureCollection(ctx, argExpr) ) {
            xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : node, "task.whenAny 参数必须是非空 array<future<T>> 或 future<T> 数组字面量");
            return 0;
        }
        return 1;
    }

    valueType = xl6SemCloneTaskArrayItemFutureValueType(ctx, argExpr);
    if ( valueType == NULL ) {
        xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : node, "task.all/task.race 参数必须是非空且完成值类型一致的 array<future<T>> 或 future<T> 数组字面量");
        return 0;
    }
    /*
     * future<T> 的结果由 T 的 XRT 类型描述符负责复制、移动和析构。
     * 聚合 API 不再维护独立类型白名单，否则新增一等公民类型时会形成
     * “async 可返回但 task.all/race 不可组合”的能力裂缝。
     */
    concreteType = xl6SemResolveAliasType(ctx, valueType);
    /*
     * task.all/race 不消耗源 future，聚合结果和源 future 都允许重复 await。
     * 因此完成值必须可以安全复制；future/task 自身由 XRT 引用计数，属于
     * 唯一可以放行的资源句柄。channel、generator、thread 和同步句柄均为
     * 独占资源，不能用浅拷贝伪造共享语义。
     */
    if ( concreteType != NULL &&
         !xl6SemIsFutureLikeTypeName(concreteType->text) &&
         !xl6SemTypeUsesClassReferenceSemantics(ctx, concreteType) &&
         !xl6SemTypeCanCopy(ctx, concreteType) ) {
        xl6SemErrorNode(
            ctx,
            valueType,
            "task.all/task.race result type must be safely copyable; move-only resource handles cannot be aggregated");
        xl6AstFree(valueType);
        return 0;
    }
    xl6AstFree(valueType);
    return 1;
}

#include "src/sem/xl6_sem_special_collection.c"

static const char* xl6SemTaskCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* name;

    (void)ctx;
    if ( !xl6SemIsTaskNamespaceMemberCall(node, &name) ) {
        return NULL;
    }
    if ( xl6TextEquals(name, "cancel") ) {
        return "bool";
    }
    if ( xl6TextEquals(name, "spawn") ||
         xl6SemIsTaskAggregateApiName(name) ||
         xl6SemIsTaskWaitApiName(name) ) {
        return "future";
    }
    return NULL;
}

static XL6_AstNode* xl6SemCloneAwaitResultType(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* operand;
    XL6_AstNode* operandType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    XL6_AstNode* resultType;

    if ( expr == NULL ) {
        return NULL;
    }
    operand = expr->kind == XL6_AST_AWAIT && expr->childCount > 0 ? expr->children[0] : expr;
    operandType = xl6SemInferAutoTypeNode(ctx, operand);
    concreteType = xl6SemResolveAliasType(ctx, operandType);
    if ( concreteType == NULL || concreteType->text == NULL ||
         !xl6SemIsFutureLikeTypeName(concreteType->text) ) {
        xl6AstFree(operandType);
        return NULL;
    }

    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        xl6AstFree(operandType);
        return NULL;
    }
    resultType = xl6SemCloneTypeNode(args->children[0]);
    xl6AstFree(operandType);
    return resultType;
}

static const char* xl6SemStaticTypeNameFromTypeNode(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo != NULL ) {
        return typeInfo->name;
    }
    return xl6SemFindTypeParamName(ctx, typeNode->text);
}

static XL6_AstNode* xl6SemCloneAsyncCallResultType(XL6_SemContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs, const XL6_AstNode* at)
{
    const XL6_AstNode* returns;
    XL6_AstNode* valueType;

    if ( func == NULL || func->kind != XL6_AST_ASYNC_FUNC ) {
        return NULL;
    }
    returns = func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        valueType = xl6SemSubstituteGenericTypeArgs(
            xl6SemFuncTypeParams(func),
            typeArgs,
            returns->children[0]);
    } else {
        valueType = xl6AstNewText(XL6_AST_TYPE, "null", 4, NULL);
    }
    if ( xl6SemFuncReturnsGenerator(ctx, func) ) {
        valueType->flags &= ~XL6_AST_FLAG_ASYNC;
        return valueType;
    }
    return xl6SemNewGenericUnaryType("future", valueType, at);
}

static XL6_AstNode* xl6SemCloneContainerItemType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL || !xl6SemIsContainerBaseTypeName(concreteType->text) ) {
        return NULL;
    }

    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        return NULL;
    }

    itemType = xl6SemResolveAliasType(ctx, args->children[0]);
    if ( itemType == NULL || xl6TextEquals(itemType->text, "var") ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(itemType);
}

static XL6_AstNode* xl6SemCloneIndexedItemType(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* itemType;
    const XL6_AstNode* concreteType;

    if ( expr == NULL || (expr->kind != XL6_AST_INDEX && expr->kind != XL6_AST_SAFE_INDEX) ) {
        return NULL;
    }

    receiver = expr->childCount > 0 ? expr->children[0] : NULL;
    if ( receiver != NULL && receiver->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* propertyType = xl6SemClonePropertyIndexGetterType(ctx, expr, 0);
        if ( propertyType != NULL ) {
            return propertyType;
        }
    }
    receiverType = xl6SemInferAutoTypeNode(ctx, receiver);
    concreteType = xl6SemClassValueType(ctx, receiverType);
    if ( concreteType != NULL &&
         xl6SemIsContainerBaseTypeName(concreteType->text) &&
         xl6SemIndexArgCount(expr) != 1 ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    if ( concreteType != NULL && xl6TextEquals(concreteType->text, "set") ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    itemType = xl6SemCloneContainerItemType(ctx, receiverType);
    if ( itemType == NULL ) {
        const XL6_AstNode* method = xl6SemFindProtocolMethodForIndex(ctx, expr, "__item__", NULL, 0);
        const XL6_AstNode* returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
        if ( returns != NULL && returns->childCount == 1 ) {
            itemType = xl6SemCloneTypeNode(returns->children[0]);
        }
    }
    xl6AstFree(receiverType);
    return itemType;
}

static XL6_AstNode* xl6SemFunctionTypeFromAnon(const XL6_AstNode* expr)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( expr == NULL || expr->kind != XL6_AST_ANON_FUNC ) {
        return NULL;
    }
    params = expr->childCount > 0 ? expr->children[0] : NULL;
    returns = expr->childCount > 1 ? expr->children[1] : NULL;
    return xl6SemFunctionTypeFromCallableParts(params, returns, expr);
}

static XL6_AstNode* xl6SemTypeNodeFromConstructorCallee(XL6_SemContext* ctx, const XL6_AstNode* callee)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    const XL6_TypeInfo* typeInfo;
    XL6_AstNode* typeNode;
    XL6_AstNode* typeArgsCopy;

    if ( ctx == NULL || callee == NULL ) {
        return NULL;
    }
    nameNode = callee;
    typeArgs = NULL;
    if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
        nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
    }
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || nameNode->text == NULL ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, nameNode->text);
    if ( typeInfo == NULL ) {
        return NULL;
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS &&
         typeInfo->kind != XL6_TYPE_CLASS &&
         !(typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsSpecialCollectionTypeName(nameNode->text) && callee->kind == XL6_AST_GENERIC_INSTANT) &&
         !(typeInfo->kind == XL6_TYPE_BUILTIN && xl6TextEquals(nameNode->text, "channel") && xl6SemIsChannelGenericInstant(callee)) &&
         !(typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsSyncHandleTypeName(nameNode->text) && callee->kind == XL6_AST_NAME) &&
         !(typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsThreadTypeName(nameNode->text) && callee->kind == XL6_AST_NAME) ) {
        return NULL;
    }

    typeNode = xl6AstNewText(XL6_AST_TYPE, nameNode->text, strlen(nameNode->text), NULL);
    if ( typeNode == NULL ) {
        return NULL;
    }
    typeNode->line = nameNode->line;
    typeNode->column = nameNode->column;
    if ( typeArgs != NULL ) {
        typeArgsCopy = xl6SemCloneTypeNode(typeArgs);
        if ( typeArgsCopy == NULL ) {
            xl6AstFree(typeNode);
            return NULL;
        }
        xl6AstAddChild(typeNode, typeArgsCopy);
    }
    return typeNode;
}

static XL6_AstNode* xl6SemNewGenericContainerType(
    const char* name,
    XL6_AstNode* itemType,
    const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* args;

    typeNode = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( typeNode == NULL || args == NULL || itemType == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(args);
        xl6AstFree(itemType);
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    xl6AstAddChild(args, itemType);
    xl6AstAddChild(typeNode, args);
    return typeNode;
}

/* 同质字面量在语义阶段直接形成静态泛型类型，后端不再重复猜测。 */
static XL6_AstNode* xl6SemInferContainerLiteralType(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* name)
{
    XL6_AstNode* common;
    size_t i;
    int usePairValue;

    if ( expr == NULL || name == NULL ) {
        return NULL;
    }
    if ( expr->childCount == 0 ) {
        return xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    }

    common = NULL;
    usePairValue = xl6TextEquals(name, "list") || xl6TextEquals(name, "dict");
    for ( i = 0; i < expr->childCount; ++i ) {
        XL6_AstNode* item = expr->children[i];
        XL6_AstNode* value = usePairValue && item != NULL && item->childCount > 1 ?
                             item->children[1] : item;
        XL6_AstNode* valueType = xl6SemInferAutoTypeNode(ctx, value);
        const XL6_AstNode* concreteType = xl6SemResolveAliasType(ctx, valueType);
        XL6_AstNode* canonicalType = concreteType != NULL ? xl6SemCloneTypeNode(concreteType) : NULL;

        xl6AstFree(valueType);
        if ( canonicalType == NULL ) {
            xl6AstFree(common);
            return xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
        }
        if ( common == NULL ) {
            common = canonicalType;
        } else if ( xl6SemTypeEquals(ctx, common, canonicalType) ) {
            xl6AstFree(canonicalType);
        } else {
            xl6AstFree(common);
            xl6AstFree(canonicalType);
            return xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
        }
    }

    if ( common != NULL && xl6TextEquals(common->text, "null") ) {
        xl6AstFree(common);
        common = xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
    }
    return xl6SemNewGenericContainerType(name, common, expr);
}

static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* result;
    const XL6_TypeInfo* typeInfo;

    if ( expr == NULL ) {
        return NULL;
    }
    if ( expr->inferredType != NULL ) {
        return xl6SemCloneTypeNode(expr->inferredType);
    }
    result = xl6SemInferAutoTypeNodeRaw(ctx, expr);
    typeInfo = result != NULL && result->text != NULL ?
               xl6TypeRegistryFind(&ctx->types, result->text) : NULL;
    if ( result != NULL && typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS ) {
        XL6_AstNode* wrapper = xl6AstNewText(XL6_AST_TYPE, "__class_handle", 14, NULL);
        XL6_AstNode* args = xl6AstNew(XL6_AST_LIST, NULL);
        if ( wrapper != NULL && args != NULL ) {
            wrapper->line = result->line;
            wrapper->column = result->column;
            wrapper->flags |= XL6_AST_FLAG_CLASS_HANDLE;
            xl6AstAddChild(args, result);
            xl6AstAddChild(wrapper, args);
            result = wrapper;
        } else {
            xl6AstFree(wrapper);
            xl6AstFree(args);
        }
    }
    if ( result != NULL && expr->inferredType == NULL ) {
        expr->inferredType = xl6SemCloneTypeNode(result);
    }
    return result;
}

static XL6_AstNode* xl6SemInferAutoTypeNodeRaw(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;
    XL6_AstNode* func;
    XL6_AstNode* returns;
    XL6_AstNode* constructedType;
    const char* typeName;

    if ( expr == NULL ) {
        return NULL;
    }
    if ( expr->kind == XL6_AST_ANON_FUNC ) {
        return xl6SemFunctionTypeFromAnon(expr);
    }
    if ( expr->kind == XL6_AST_AWAIT ) {
        return xl6SemCloneAwaitResultType(ctx, expr);
    }
    if ( expr->kind == XL6_AST_TYPE_IS ) {
        return xl6AstNewText(XL6_AST_TYPE, "bool", 4, NULL);
    }
    if ( expr->kind == XL6_AST_TYPE_AS ) {
        return expr->childCount > 1 ? xl6SemCloneTypeNode(expr->children[1]) : NULL;
    }
    if ( expr->kind == XL6_AST_TYPED_LITERAL &&
         expr->childCount > 1 &&
         expr->children[1] != NULL ) {
        return xl6SemCloneTypeNode(expr->children[1]);
    }
    if ( expr->kind == XL6_AST_TYPED_LITERAL && expr->childCount > 0 ) {
        if ( xl6TextEquals(expr->text, "array") || xl6TextEquals(expr->text, "list") ||
             xl6TextEquals(expr->text, "set") || xl6TextEquals(expr->text, "dict") ) {
            return xl6SemInferContainerLiteralType(ctx, expr->children[0], expr->text);
        }
    }
    if ( expr->kind == XL6_AST_ARRAY_LITERAL ) {
        return xl6SemInferContainerLiteralType(ctx, expr, "array");
    }
    if ( expr->kind == XL6_AST_LIST_LITERAL ) {
        return xl6SemInferContainerLiteralType(ctx, expr, "list");
    }
    if ( expr->kind == XL6_AST_SET_LITERAL ) {
        return xl6SemInferContainerLiteralType(ctx, expr, "set");
    }
    if ( expr->kind == XL6_AST_DICT_LITERAL ) {
        return xl6SemInferContainerLiteralType(ctx, expr, "dict");
    }
    if ( expr->kind == XL6_AST_INDEX || expr->kind == XL6_AST_SAFE_INDEX ) {
        XL6_AstNode* itemType = xl6SemCloneIndexedItemType(ctx, expr);
        if ( itemType != NULL ) {
            if ( expr->kind == XL6_AST_SAFE_INDEX ) {
                xl6AstFree(itemType);
                return xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
            }
            return itemType;
        }
    }
    if ( expr->kind == XL6_AST_SAFE_MEMBER ) {
        return xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
    }
    if ( expr->kind == XL6_AST_UNARY && expr->childCount > 0 ) {
        const char* protocolName = xl6SemUnaryProtocolName(expr->text);
        const XL6_AstNode* method = protocolName != NULL ?
                                    xl6SemFindProtocolMethodForExprArgs(ctx, expr->children[0], protocolName, NULL, 0, 0) :
                                    NULL;
        returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
        if ( returns != NULL && returns->childCount == 1 ) {
            return xl6SemCloneTypeNode(returns->children[0]);
        }
        {
            const char* staticType = xl6SemExprStaticTypeName(ctx, expr);
            if ( staticType != NULL ) {
                return xl6AstNewText(XL6_AST_TYPE, staticType, strlen(staticType), NULL);
            }
        }
    }
    if ( expr->kind == XL6_AST_BINARY && expr->childCount > 1 ) {
        const char* protocolName = xl6SemBinaryProtocolName(expr->text);
        const XL6_AstNode* method = NULL;
        if ( xl6TextEquals(expr->text, "??") ) {
            XL6_AstNode* leftType = xl6SemInferAutoTypeNode(ctx, expr->children[0]);
            XL6_AstNode* rightType = xl6SemInferAutoTypeNode(ctx, expr->children[1]);
            const XL6_AstNode* leftConcrete = xl6SemResolveAliasType(ctx, leftType);
            const XL6_AstNode* rightConcrete = xl6SemResolveAliasType(ctx, rightType);
            XL6_AstNode* result = NULL;

            if ( leftConcrete != NULL && xl6TextEquals(leftConcrete->text, "null") ) {
                result = xl6SemCloneTypeNode(rightType);
            } else if ( leftType != NULL && rightType != NULL && xl6SemTypeEquals(ctx, leftType, rightType) ) {
                result = xl6SemCloneTypeNode(leftType);
            } else if ( (leftConcrete != NULL && xl6TextEquals(leftConcrete->text, "var")) ||
                        (rightConcrete != NULL && xl6TextEquals(rightConcrete->text, "var")) ) {
                result = xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
            }
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            if ( result != NULL ) {
                return result;
            }
        }
        if ( xl6SemBinaryIsContainerOp(ctx, expr) ) {
            if ( xl6TextEquals(expr->text, "==") || xl6TextEquals(expr->text, "!=") ||
                 xl6TextEquals(expr->text, "<") || xl6TextEquals(expr->text, "<=") ||
                 xl6TextEquals(expr->text, ">") || xl6TextEquals(expr->text, ">=") ) {
                return xl6AstNewText(XL6_AST_TYPE, "bool", 4, NULL);
            }
            return xl6SemInferAutoTypeNode(ctx, expr->children[0]);
        }
        if ( xl6TextEquals(expr->text, "in") ) {
            XL6_AstNode* arg = expr->children[0];
            method = xl6SemFindProtocolMethodForExprArgs(ctx, expr->children[1], "__contains__", &arg, 1, 0);
        } else if ( protocolName != NULL ) {
            XL6_AstNode* arg = expr->children[1];
            method = xl6SemFindProtocolMethodForExprArgs(ctx, expr->children[0], protocolName, &arg, 1, 0);
        }
        returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
        if ( returns != NULL && returns->childCount == 1 ) {
            return xl6SemCloneTypeNode(returns->children[0]);
        }
    }
    if ( expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        XL6_AstNode* thenType = xl6SemInferAutoTypeNode(ctx, expr->children[1]);
        XL6_AstNode* elseType = xl6SemInferAutoTypeNode(ctx, expr->children[2]);
        const XL6_AstNode* thenConcrete = xl6SemResolveAliasType(ctx, thenType);
        const XL6_AstNode* elseConcrete = xl6SemResolveAliasType(ctx, elseType);
        XL6_AstNode* result = NULL;

        if ( thenConcrete != NULL && xl6TextEquals(thenConcrete->text, "null") && elseType != NULL ) {
            result = xl6SemCloneTypeNode(elseType);
        } else if ( elseConcrete != NULL && xl6TextEquals(elseConcrete->text, "null") && thenType != NULL ) {
            result = xl6SemCloneTypeNode(thenType);
        } else if ( thenType != NULL && elseType != NULL && xl6SemTypeEquals(ctx, thenType, elseType) ) {
            result = xl6SemCloneTypeNode(thenType);
        } else if ( (thenConcrete != NULL && xl6TextEquals(thenConcrete->text, "var")) ||
                    (elseConcrete != NULL && xl6TextEquals(elseConcrete->text, "var")) ) {
            result = xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
        }

        xl6AstFree(thenType);
        xl6AstFree(elseType);
        if ( result != NULL ) {
            return result;
        }
    }
    if ( expr->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, expr->text);
        if ( sym != NULL && sym->type != NULL &&
             !xl6TextEquals(sym->type->text, "auto") && !xl6TextEquals(sym->type->text, "var") ) {
            return xl6SemCloneTypeNode(sym->type);
        }
        if ( sym == NULL && ctx->implicitSelfName != NULL && ctx->currentClass != NULL ) {
            const XL6_AstNode* field = xl6SemFindClassFieldNode(ctx, ctx->currentClass, expr->text, NULL);
            if ( field != NULL && field->childCount > 0 && field->children[0] != NULL ) {
                return xl6SemCloneTypeNode(field->children[0]);
            }
        }
    }
    if ( expr->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, expr);
        if ( memberType != NULL ) {
            return memberType;
        }
    }
    if ( expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
         expr->children[0] != NULL ) {
        if ( xl6SemIsImplicitToStringCall(ctx, expr) ) {
            return xl6AstNewText(XL6_AST_TYPE, "string", 6, NULL);
        }
        constructedType = xl6SemTypeNodeFromConstructorCallee(ctx, expr->children[0]);
        if ( constructedType != NULL ) {
            return constructedType;
        }
        {
            XL6_AstNode* taskType = xl6SemCloneTaskCallType(ctx, expr);
            if ( taskType != NULL ) {
                return taskType;
            }
        }
        if ( expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* instant = expr->children[0];
            XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
            XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
            const XL6_AstNode* persistenceTypeArg = xl6SemPersistenceGenericCallTypeArg(expr);
            if ( persistenceTypeArg != NULL ) {
                return xl6SemCloneTypeNode(persistenceTypeArg);
            }
            if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                func = xl6SemResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
                returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
                if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC ) {
                    return xl6SemCloneAsyncCallResultType(ctx, func, typeArgs, expr);
                }
                if ( returns != NULL && returns->childCount == 1 ) {
                    return xl6SemSubstituteGenericTypeArgs(
                        xl6SemFuncTypeParams(func),
                        typeArgs,
                        returns->children[0]);
                }
            }
            return NULL;
        }
        if ( expr->children[0]->kind == XL6_AST_MEMBER ) {
            XL6_AstNode* ownedReturnType = NULL;
            XL6_AstNode* memberReturns = xl6SemCallReturnTypesEx(ctx, expr, &ownedReturnType);
            XL6_AstNode* result = NULL;

            result = xl6SemCloneSpecialCollectionCallType(ctx, expr);
            if ( result != NULL ) {
                return result;
            }

            /*
             * 成员函数返回的泛型容器必须保留完整类型节点。
             * 只返回 array/list/set/dict 名称会让 for-in 无法推断 item 类型。
             */
            if ( memberReturns != NULL && memberReturns->childCount == 1 ) {
                result = xl6SemCloneTypeNode(memberReturns->children[0]);
            }
            xl6AstFree(ownedReturnType);
            return result;
        }
        if ( expr->children[0]->kind != XL6_AST_NAME ) {
            return NULL;
        }
        if ( xl6SemIsCopyBuiltinName(expr->children[0]->text) &&
             !xl6SemCopyBuiltinIsShadowed(ctx, expr) &&
             xl6SemCallArgCount(expr) == 1 ) {
            return xl6SemInferAutoTypeNode(ctx, xl6SemCallArgExprAt(expr, 0));
        }
        if ( xl6SemIsBuiltinConvertTypeName(expr->children[0]->text) &&
             xl6SemNameIsRegisteredType(ctx, expr->children[0]->text) ) {
            return xl6AstNewText(XL6_AST_TYPE, expr->children[0]->text, strlen(expr->children[0]->text), NULL);
        }
        sym = xl6ScopeFind(ctx->scope, expr->children[0]->text);
        if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") &&
             sym->type->childCount == 0 ) {
            return xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
        }
        if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "function") &&
             sym->type->childCount > 1 && sym->type->children[1] != NULL &&
             sym->type->children[1]->childCount == 1 ) {
            return xl6SemCloneTypeNode(sym->type->children[1]->children[0]);
        }
        if ( sym != NULL && sym->type != NULL && xl6TextEquals(sym->type->text, "var") ) {
            return xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
        }
        {
            const XL6_AstNode* protocolMethod = xl6SemFindProtocolMethodForExprCall(ctx, expr->children[0], "__call__", expr, 0);
            returns = protocolMethod != NULL && protocolMethod->childCount > 2 ? protocolMethod->children[2] : NULL;
            if ( returns != NULL && returns->childCount == 1 ) {
                return xl6SemCloneTypeNode(returns->children[0]);
            }
        }
        func = xl6SemResolveCallFuncEx(ctx, expr, expr->children[0]->text, 0);
        returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
        {
            XL6_AstNode* inferredArgs = xl6SemInferCallTypeArgs(ctx, expr, func);
            XL6_AstNode* result = NULL;

            if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC ) {
                result = xl6SemCloneAsyncCallResultType(ctx, func, inferredArgs, expr);
                xl6AstFree(inferredArgs);
                return result;
            }
            if ( returns != NULL && returns->childCount == 1 && inferredArgs != NULL ) {
                result = xl6SemSubstituteGenericTypeArgs(
                    xl6SemFuncTypeParams(func),
                    inferredArgs,
                    returns->children[0]);
                xl6AstFree(inferredArgs);
                return result;
            }
            xl6AstFree(inferredArgs);
        }
        if ( returns != NULL && returns->childCount == 1 ) {
            return xl6SemCloneTypeNode(returns->children[0]);
        }
    }

    typeName = xl6SemExprStaticTypeName(ctx, expr);
    return typeName != NULL ? xl6AstNewText(XL6_AST_TYPE, typeName, strlen(typeName), NULL) : NULL;
}

static int xl6SemFuncReturnsGenerator(XL6_SemContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;

    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return 0;
    }
    returnType = xl6SemResolveAliasType(ctx, returns->children[0]);
    return returnType != NULL && xl6SemIsGeneratorTypeName(returnType->text);
}

static int xl6SemFuncIsAsyncGenerator(XL6_SemContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;

    if ( !xl6SemFuncReturnsGenerator(ctx, func) ) {
        return 0;
    }
    if ( func->kind == XL6_AST_ASYNC_FUNC ) {
        return 1;
    }
    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    return returnType != NULL && (returnType->flags & XL6_AST_FLAG_ASYNC) != 0;
}

static int xl6SemExprIsFreshTypeValue(XL6_SemContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const char* taskName;
    XL6_AstNode* typeNode;
    XL6_AstNode* func;
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    if ( expr->kind == XL6_AST_TYPED_LITERAL ) {
        return expr->childCount > 0 &&
               xl6SemExprIsFreshTypeValue(ctx, expr->children[0]);
    }

    if ( expr->kind == XL6_AST_ARRAY_LITERAL ||
         expr->kind == XL6_AST_LIST_LITERAL ||
         expr->kind == XL6_AST_SET_LITERAL ||
         expr->kind == XL6_AST_DICT_LITERAL ) {
        /*
            容器字面量本身是新值，但不可复制元素必须同样来自新值。
            这样 future 等线性句柄可以直接装入字面量，同时禁止从已有变量静默复制。
        */
        for ( i = 0; i < expr->childCount; ++i ) {
            const XL6_AstNode* item = expr->children[i];
            XL6_AstNode* itemType;

            if ( item != NULL && item->kind == XL6_AST_KEY_VALUE ) {
                item = item->childCount > 1 ? item->children[1] : NULL;
            }
            if ( item == NULL ) {
                continue;
            }
            itemType = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)item);
            if ( itemType != NULL &&
                 !xl6SemTypeUsesClassReferenceSemantics(ctx, itemType) &&
                 !xl6SemTypeCanCopy(ctx, itemType) &&
                 !xl6SemExprIsFreshTypeValue(ctx, item) ) {
                xl6AstFree(itemType);
                return 0;
            }
            xl6AstFree(itemType);
        }
        return 1;
    }

    if ( expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    taskName = NULL;
    if ( xl6SemIsTaskNamespaceMemberCall((XL6_AstNode*)expr, &taskName) &&
         (xl6TextEquals(taskName, "spawn") ||
          xl6TextEquals(taskName, "all") ||
          xl6TextEquals(taskName, "whenAny") ||
          xl6TextEquals(taskName, "race")) ) {
        return 1;
    }
    callee = expr->children[0];
    typeNode = xl6SemTypeNodeFromConstructorCallee(ctx, callee);
    if ( typeNode != NULL ) {
        xl6AstFree(typeNode);
        return 1;
    }
    if ( callee != NULL && callee->kind == XL6_AST_GENERIC_INSTANT ) {
        XL6_AstNode* nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        XL6_AstNode* typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
        if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
            func = xl6SemResolveExplicitGenericCallFunc(ctx, (XL6_AstNode*)expr, nameNode->text, typeArgs, 0);
            return func != NULL &&
                   (func->kind == XL6_AST_ASYNC_FUNC ||
                    xl6SemFuncReturnsGenerator(ctx, func) ||
                    xl6SemFuncReturnsFutureLikeResource(ctx, func) ||
                    xl6SemFuncReturnsFreshOnlyValue(ctx, func));
        }
        return 0;
    }
    if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
        func = xl6SemResolveCallFuncEx(ctx, (XL6_AstNode*)expr, callee->text, 0);
        return func != NULL &&
               (func->kind == XL6_AST_ASYNC_FUNC ||
                xl6SemFuncReturnsGenerator(ctx, func) ||
                xl6SemFuncReturnsFutureLikeResource(ctx, func) ||
                xl6SemFuncReturnsFreshOnlyValue(ctx, func));
    }
    if ( callee != NULL && callee->kind == XL6_AST_MEMBER ) {
        if ( xl6TextEquals(callee->text, "then") ||
             xl6TextEquals(callee->text, "catch") ||
             xl6TextEquals(callee->text, "finally") ) {
            XL6_AstNode* receiverType = xl6SemCloneFutureReceiverType(
                ctx,
                (XL6_AstNode*)callee);
            const XL6_AstNode* concreteType = xl6SemResolveAliasType(ctx, receiverType);
            int freshFuture = concreteType != NULL &&
                              xl6SemIsFutureLikeTypeName(concreteType->text);
            xl6AstFree(receiverType);
            if ( freshFuture ) {
                return 1;
            }
        }
        XL6_AstNode* ownedReturnType = NULL;
        XL6_AstNode* returns = xl6SemCallReturnTypesEx(ctx, (XL6_AstNode*)expr, &ownedReturnType);
        const XL6_AstNode* returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
        int fresh = returnType != NULL &&
                    (xl6SemTypeIsFutureLikeResource(ctx, returnType) ||
                     xl6SemTypeRequiresFreshValue(ctx, returnType));
        xl6AstFree(ownedReturnType);
        return fresh;
    }
    return 0;
}

static int xl6SemFuncReturnsFutureLikeResource(XL6_SemContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* returns;

    if ( func == NULL || func->childCount <= 2 ) {
        return 0;
    }
    returns = func->children[2];
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6SemTypeIsFutureLikeResource(ctx, returns->children[0]);
}

static int xl6SemFuncReturnsFreshOnlyValue(XL6_SemContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* returns;

    if ( func == NULL || func->childCount <= 2 ) {
        return 0;
    }
    returns = func->children[2];
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6SemTypeRequiresFreshValue(ctx, returns->children[0]);
}

static int xl6SemCheckCopyableExpr(XL6_SemContext* ctx, XL6_AstNode* expr, int allowFreshTypeValue, const char* message)
{
    XL6_AstNode* typeNode;
    int ok;

    if ( expr == NULL ) {
        return 1;
    }
    if ( allowFreshTypeValue && xl6SemExprIsFreshTypeValue(ctx, expr) ) {
        return 1;
    }
    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    if ( typeNode == NULL ) {
        return 1;
    }
    ok = 1;
    if ( !xl6SemTypeUsesClassReferenceSemantics(ctx, typeNode) &&
         !xl6SemTypeCanCopy(ctx, typeNode) ) {
        xl6SemErrorNode(ctx, expr, message);
        ok = 0;
    }
    xl6AstFree(typeNode);
    return ok;
}

static const XL6_AstNode* xl6SemClassHandleTargetType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;

    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL ||
         !xl6TextEquals(typeNode->text, "__class_handle") ||
         (typeNode->flags & XL6_AST_FLAG_CLASS_HANDLE) == 0 ) {
        return NULL;
    }
    args = xl6SemTypeNodeGenericArgs(typeNode);
    return args != NULL && args->childCount == 1 ?
           xl6SemResolveAliasType(ctx, args->children[0]) : NULL;
}

static const XL6_AstNode* xl6SemClassValueType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* target = xl6SemClassHandleTargetType(ctx, typeNode);
    return target != NULL ? target : xl6SemResolveAliasType(ctx, typeNode);
}

static int xl6SemTypeUsesClassReferenceSemantics(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_TypeInfo* typeInfo;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( xl6SemClassHandleTargetType(ctx, concreteType) != NULL ) {
        return 1;
    }
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, concreteType->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS;
}

static int xl6SemClassHandleCanWiden(
    XL6_SemContext* ctx,
    const XL6_AstNode* sourceType,
    const XL6_AstNode* targetType)
{
    const XL6_AstNode* sourceTarget = xl6SemClassHandleTargetType(ctx, sourceType);
    const XL6_AstNode* targetTarget = xl6SemClassHandleTargetType(ctx, targetType);

    if ( sourceTarget == NULL || targetTarget == NULL || targetTarget->text == NULL ) {
        return 0;
    }
    return xl6SemTypeIsOrExtendsClass(ctx, sourceTarget, targetTarget->text);
}

static int xl6SemCurrentFunctionParamName(XL6_SemContext* ctx, const char* name)
{
    XL6_AstNode* params;
    size_t i;

    if ( ctx == NULL || ctx->currentFunction == NULL || name == NULL ) {
        return 0;
    }
    params = ctx->currentFunction->childCount > 1 ? ctx->currentFunction->children[1] : NULL;
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( param != NULL && param->text != NULL && xl6TextEquals(param->text, name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemExprCanMoveValueOnReturn(XL6_SemContext* ctx, XL6_AstNode* expr, const XL6_AstNode* returnType)
{
    XL6_Symbol* sym;
    XL6_AstNode* exprType;
    int result;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_NAME || expr->text == NULL || returnType == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(expr->text, "self") || xl6SemCurrentFunctionParamName(ctx, expr->text) ) {
        return 0;
    }
    sym = xl6ScopeFind(ctx->scope, expr->text);
    if ( sym == NULL || sym->kind != XL6_SYM_VALUE ) {
        return 0;
    }
    exprType = xl6SemInferAutoTypeNode(ctx, expr);
    result = exprType != NULL &&
             xl6SemTypeRequiresFreshValue(ctx, returnType) &&
             xl6SemTypeEquals(ctx, exprType, returnType);
    xl6AstFree(exprType);
    return result;
}

static int xl6SemCheckBoolExpr(XL6_SemContext* ctx, XL6_AstNode* expr, const char* message)
{
    const char* typeName;

    typeName = xl6SemExprStaticTypeName(ctx, expr);
    if ( typeName != NULL && !xl6SemIsBoolTypeName(typeName) && !xl6TextEquals(typeName, "var") ) {
        xl6SemErrorNode(ctx, expr, message);
        return 0;
    }
    return 1;
}

static const char* xl6SemUnaryProtocolName(const char* op)
{
    if ( xl6TextEquals(op, "-") ) {
        return "__neg__";
    }
    if ( xl6TextEquals(op, "+") ) {
        return "__pos__";
    }
    if ( xl6TextEquals(op, "not") || xl6TextEquals(op, "!") ) {
        return "__not__";
    }
    if ( xl6TextEquals(op, "~") ) {
        return "__bitNot__";
    }
    return NULL;
}

static const char* xl6SemBinaryProtocolName(const char* op)
{
    if ( xl6TextEquals(op, "+") ) {
        return "__add__";
    }
    if ( xl6TextEquals(op, "-") ) {
        return "__sub__";
    }
    if ( xl6TextEquals(op, "*") ) {
        return "__mul__";
    }
    if ( xl6TextEquals(op, "/") ) {
        return "__div__";
    }
    if ( xl6TextEquals(op, "%") ) {
        return "__mod__";
    }
    if ( xl6TextEquals(op, "**") ) {
        return "__pow__";
    }
    if ( xl6TextEquals(op, "==") ) {
        return "__eq__";
    }
    if ( xl6TextEquals(op, "!=") ) {
        return "__ne__";
    }
    if ( xl6TextEquals(op, "<") ) {
        return "__lt__";
    }
    if ( xl6TextEquals(op, "<=") ) {
        return "__le__";
    }
    if ( xl6TextEquals(op, ">") ) {
        return "__gt__";
    }
    if ( xl6TextEquals(op, ">=") ) {
        return "__ge__";
    }
    if ( xl6TextEquals(op, "~=") ) {
        return "__approx__";
    }
    return NULL;
}

static const char* xl6SemCompoundAssignBinaryOp(const char* op)
{
    if ( xl6TextEquals(op, "+=") ) { return "+"; }
    if ( xl6TextEquals(op, "-=") ) { return "-"; }
    if ( xl6TextEquals(op, "*=") ) { return "*"; }
    if ( xl6TextEquals(op, "/=") ) { return "/"; }
    if ( xl6TextEquals(op, "%=") ) { return "%"; }
    return NULL;
}

static int xl6SemCompoundAssignUsesClassProtocol(
    XL6_SemContext* ctx,
    XL6_AstNode* left,
    XL6_AstNode* right,
    const char* op)
{
    const char* binaryOp;
    const char* protocolName;
    XL6_AstNode* arg;
    const XL6_AstNode* method;

    if ( ctx == NULL || left == NULL || right == NULL || op == NULL ) {
        return 0;
    }
    binaryOp = xl6SemCompoundAssignBinaryOp(op);
    protocolName = binaryOp != NULL ? xl6SemBinaryProtocolName(binaryOp) : NULL;
    if ( protocolName == NULL ) {
        return 0;
    }
    arg = right;
    method = xl6SemFindProtocolMethodForExprArgs(ctx, left, protocolName, &arg, 1, 1);
    return method != NULL;
}

static int xl6SemExprClassHasDeriveEquals(XL6_SemContext* ctx, XL6_AstNode* left, XL6_AstNode* right)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    const XL6_AstNode* concreteLeft;
    XL6_AstNode* classNode;
    int result;

    if ( ctx == NULL || left == NULL || right == NULL ) {
        return 0;
    }
    leftType = xl6SemInferAutoTypeNode(ctx, left);
    rightType = xl6SemInferAutoTypeNode(ctx, right);
    concreteLeft = xl6SemResolveAliasType(ctx, leftType);
    classNode = concreteLeft != NULL && concreteLeft->text != NULL ? xl6SemFindTypeDecl(ctx, concreteLeft->text) : NULL;
    result = classNode != NULL &&
             classNode->kind == XL6_AST_CLASS &&
             xl6SemClassDeclHasDerive(classNode, "equals") &&
             xl6SemTypeEquals(ctx, leftType, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static int xl6SemCheckUnaryExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* operandType;
    const char* protocolName;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    protocolName = node != NULL ? xl6SemUnaryProtocolName(node->text) : NULL;
    if ( node != NULL && node->childCount > 0 && protocolName != NULL &&
         xl6SemFindProtocolMethodForExprArgs(ctx, node->children[0], protocolName, NULL, 0, 1) != NULL ) {
        return ok;
    }
    if ( node != NULL && (xl6TextEquals(node->text, "not") || xl6TextEquals(node->text, "!")) ) {
        if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "logical not operand must be bool") ) {
            ok = 0;
        }
        return ok;
    }
    if ( node != NULL && node->childCount > 0 &&
         (xl6TextEquals(node->text, "+") || xl6TextEquals(node->text, "-") || xl6TextEquals(node->text, "~")) ) {
        operandType = xl6SemExprStaticTypeName(ctx, node->children[0]);
        if ( operandType != NULL ) {
            if ( xl6TextEquals(node->text, "~") ) {
                if ( !xl6TextEquals(operandType, "var") && !xl6SemIsIntegerTypeName(operandType) ) {
                    xl6SemErrorNode(ctx, node, "bitwise not operand must be integer");
                    ok = 0;
                }
            } else if ( !xl6TextEquals(operandType, "var") && !xl6SemIsNumericTypeName(operandType) ) {
                xl6SemErrorNode(ctx, node, "unary numeric operand must be numeric");
                ok = 0;
            }
        }
    }
    return ok;
}
static int xl6SemExprIsBareFunctionSymbol(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_AstNode* nameNode;
    XL6_Symbol* sym;

    nameNode = expr;
    if ( expr != NULL && expr->kind == XL6_AST_GENERIC_INSTANT ) {
        nameNode = expr->childCount > 0 ? expr->children[0] : NULL;
    }
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || nameNode->text == NULL ) {
        return 0;
    }

    sym = xl6ScopeFind(ctx->scope, nameNode->text);
    return sym != NULL && sym->kind == XL6_SYM_FUNC;
}

static int xl6SemExprIsTypeLiteralSymbol(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;

    if ( expr == NULL || expr->kind != XL6_AST_NAME || expr->text == NULL ) {
        return 0;
    }

    sym = xl6ScopeFind(ctx->scope, expr->text);
    return sym == NULL && xl6SemNameIsRegisteredType(ctx, expr->text);
}

static int xl6SemCompareAllowsTypeLiteral(const char* op)
{
    return xl6TextEquals(op, "==") || xl6TextEquals(op, "!=");
}

static int xl6SemBinaryIsContainerOp(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    const XL6_AstNode* leftConcrete;
    const XL6_AstNode* rightConcrete;
    XL6_SemContainerKind leftKind;
    XL6_SemContainerKind rightKind;
    int leftTyped;
    int rightTyped;
    int sameType;
    const char* op;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_BINARY ||
         node->childCount < 2 || node->text == NULL ) {
        return 0;
    }

    op = node->text;
    leftType = xl6SemInferAutoTypeNode(ctx, node->children[0]);
    rightType = xl6SemInferAutoTypeNode(ctx, node->children[1]);
    leftConcrete = xl6SemResolveAliasType(ctx, leftType);
    rightConcrete = xl6SemResolveAliasType(ctx, rightType);
    leftKind = xl6SemContainerKindFromType(ctx, leftConcrete, &leftTyped);
    rightKind = xl6SemContainerKindFromType(ctx, rightConcrete, &rightTyped);
    sameType = leftConcrete != NULL && rightConcrete != NULL &&
               xl6SemTypeEquals(ctx, leftConcrete, rightConcrete);

    if ( leftKind == XL6_SEM_CONTAINER_ARRAY && rightKind == XL6_SEM_CONTAINER_ARRAY &&
         sameType && (xl6TextEquals(op, "+") || xl6TextEquals(op, "==") || xl6TextEquals(op, "!=")) ) {
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return 1;
    }
    if ( leftKind == XL6_SEM_CONTAINER_LIST ) {
        if ( rightKind == XL6_SEM_CONTAINER_LIST && sameType &&
             (xl6TextEquals(op, "+") || xl6TextEquals(op, "==") || xl6TextEquals(op, "!=")) ) {
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return 1;
        }
        if ( xl6TextEquals(op, "-") && rightConcrete != NULL &&
             xl6SemIsIntegerTypeName(rightConcrete->text) ) {
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return 1;
        }
    }
    if ( leftKind == XL6_SEM_CONTAINER_DICT ) {
        if ( rightKind == XL6_SEM_CONTAINER_DICT && sameType &&
             (xl6TextEquals(op, "+") || xl6TextEquals(op, "==") || xl6TextEquals(op, "!=")) ) {
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return 1;
        }
        if ( xl6TextEquals(op, "-") && rightConcrete != NULL &&
             xl6TextEquals(rightConcrete->text, "string") ) {
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return 1;
        }
    }
    if ( leftKind == XL6_SEM_CONTAINER_SET && rightKind == XL6_SEM_CONTAINER_SET && sameType &&
         (xl6TextEquals(op, "|") || xl6TextEquals(op, "&") ||
          xl6TextEquals(op, "-") || xl6TextEquals(op, "^") ||
          xl6TextEquals(op, "==") || xl6TextEquals(op, "!=") ||
          xl6TextEquals(op, "<") || xl6TextEquals(op, "<=") ||
          xl6TextEquals(op, ">") || xl6TextEquals(op, ">=")) ) {
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return 1;
    }

    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return 0;
}

static int xl6SemCheckBinaryExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* leftType;
    const char* rightType;
    const char* protocolName;
    const XL6_AstNode* protocolMethod;
    int ok;
    int containerOp;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    containerOp = xl6SemBinaryIsContainerOp(ctx, node);
    protocolName = node != NULL ? xl6SemBinaryProtocolName(node->text) : NULL;
    if ( node != NULL && node->childCount > 1 && xl6TextEquals(node->text, "in") ) {
        XL6_AstNode* arg = node->children[0];
        protocolMethod = xl6SemFindProtocolMethodForExprArgs(ctx, node->children[1], "__contains__", &arg, 1, 1);
        if ( protocolMethod != NULL ) {
            if ( !xl6SemCheckProtocolArg(ctx, node, protocolMethod, 0, node->children[0], "in 协议参数类型不匹配") ) {
                ok = 0;
            }
            return ok;
        }
        {
            const XL6_AstNode* fallbackMethod = xl6SemFindProtocolMethodForExpr(ctx, node->children[1], "__contains__");
            if ( fallbackMethod != NULL ) {
                xl6SemCheckProtocolArg(ctx, node, fallbackMethod, 0, node->children[0], "in protocol argument type mismatch");
                return 0;
            }
        }
    } else if ( node != NULL && node->childCount > 1 && protocolName != NULL ) {
        XL6_AstNode* arg = node->children[1];
        protocolMethod = xl6SemFindProtocolMethodForExprArgs(ctx, node->children[0], protocolName, &arg, 1, 1);
        if ( protocolMethod != NULL ) {
            if ( !xl6SemCheckProtocolArg(ctx, node, protocolMethod, 0, node->children[1], "运算符协议参数类型不匹配") ) {
                ok = 0;
            }
            return ok;
        }
        {
            const XL6_AstNode* fallbackMethod = xl6SemFindProtocolMethodForExpr(ctx, node->children[0], protocolName);
            if ( fallbackMethod != NULL ) {
                xl6SemCheckProtocolArg(ctx, node, fallbackMethod, 0, node->children[1], "operator protocol argument type mismatch");
                return 0;
            }
        }
    }
    if ( node != NULL && node->childCount > 1 &&
         (xl6TextEquals(node->text, "==") || xl6TextEquals(node->text, "!=")) ) {
        leftType = xl6SemExprStaticTypeName(ctx, node->children[0]);
        rightType = xl6SemExprStaticTypeName(ctx, node->children[1]);
        if ( leftType != NULL && rightType != NULL &&
             xl6SemTypeNameIsClassValue(ctx, leftType) &&
             xl6SemTypeNameIsClassValue(ctx, rightType) &&
             xl6SemFindProtocolMethodForExpr(ctx, node->children[0], protocolName) == NULL &&
             !xl6SemExprClassHasDeriveEquals(ctx, node->children[0], node->children[1]) ) {
            xl6SemErrorNode(ctx, node, "class equality requires __eq__/__ne__ protocol");
            ok = 0;
        }
    }
    if ( node != NULL && (xl6TextEquals(node->text, "and") || xl6TextEquals(node->text, "or")) ) {
        if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "逻辑运算左操作数必须是 bool") ) {
            ok = 0;
        }
        if ( node->childCount > 1 && !xl6SemCheckBoolExpr(ctx, node->children[1], "逻辑运算右操作数必须是 bool") ) {
            ok = 0;
        }
    }
    if ( node != NULL && xl6TextEquals(node->text, "+") ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( (leftType != NULL && xl6TextEquals(leftType, "string")) ||
             (rightType != NULL && xl6TextEquals(rightType, "string")) ) {
            if ( !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
                 (leftType == NULL || rightType == NULL ||
                  !xl6TextEquals(leftType, "string") || !xl6TextEquals(rightType, "string")) ) {
                xl6SemErrorNode(ctx, node, "字符串拼接两侧都必须是 string");
                ok = 0;
            }
        }
    }
    if ( node != NULL &&
         (xl6TextEquals(node->text, "+") || xl6TextEquals(node->text, "-") ||
          xl6TextEquals(node->text, "*") || xl6TextEquals(node->text, "/")) ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( !containerOp && leftType != NULL && rightType != NULL ) {
            int stringConcatCandidate = xl6TextEquals(node->text, "+") &&
                                        (xl6TextEquals(leftType, "string") ||
                                         xl6TextEquals(rightType, "string"));
            int sameGenericType = xl6TextEquals(leftType, rightType) &&
                                  xl6SemFindTypeParam(ctx, leftType);
            if ( !stringConcatCandidate && !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
                 !sameGenericType &&
                 (!xl6SemIsNumericTypeName(leftType) || !xl6SemIsNumericTypeName(rightType)) ) {
                xl6SemErrorNode(ctx, node, "numeric operator operands must be numeric");
                ok = 0;
            }
            if ( xl6TextEquals(node->text, "/") && xl6SemIsNumericTypeName(leftType) &&
                 xl6SemIsNumericTypeName(rightType) && node->childCount > 1 &&
                 xl6SemExprIsZeroLiteral(node->children[1]) ) {
                xl6SemErrorNode(ctx, node, "division by zero literal");
                ok = 0;
            }
        }
    }
    if ( node != NULL && xl6TextEquals(node->text, "**") ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( leftType != NULL && rightType != NULL &&
             (!xl6SemIsIntegerTypeName(leftType) || !xl6SemIsIntegerTypeName(rightType)) ) {
            xl6SemErrorNode(ctx, node, "power operator operands must be integer");
            ok = 0;
        }
    }
    if ( node != NULL &&
         (xl6TextEquals(node->text, "%") || xl6TextEquals(node->text, "&") ||
          xl6TextEquals(node->text, "|") || xl6TextEquals(node->text, "^") ||
          xl6TextEquals(node->text, "<<") || xl6TextEquals(node->text, ">>")) ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( !containerOp && leftType != NULL && rightType != NULL ) {
            if ( !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
                 (!xl6SemIsIntegerTypeName(leftType) || !xl6SemIsIntegerTypeName(rightType)) ) {
                xl6SemErrorNode(ctx, node, "integer operator operands must be integer");
                ok = 0;
            }
            if ( xl6TextEquals(node->text, "%") && xl6SemIsIntegerTypeName(leftType) &&
                 xl6SemIsIntegerTypeName(rightType) && node->childCount > 1 &&
                 xl6SemExprIsZeroLiteral(node->children[1]) ) {
                xl6SemErrorNode(ctx, node, "modulo by zero literal");
                ok = 0;
            }
        }
    }
    if ( node != NULL && xl6TextEquals(node->text, "??") ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( leftType != NULL && rightType != NULL &&
             !xl6TextEquals(leftType, "null") && !xl6TextEquals(rightType, "null") &&
             !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
             !xl6TextEquals(leftType, rightType) ) {
            xl6SemErrorNode(ctx, node, "空合并表达式两侧必须是同一静态类型，或一侧为 null");
            ok = 0;
        }
    }
    if ( node != NULL &&
         (xl6TextEquals(node->text, "==") || xl6TextEquals(node->text, "!=") ||
          xl6TextEquals(node->text, "<") || xl6TextEquals(node->text, "<=") ||
          xl6TextEquals(node->text, ">") || xl6TextEquals(node->text, ">=") ||
          xl6TextEquals(node->text, "~=")) ) {
        XL6_AstNode* left = node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* right = node->childCount > 1 ? node->children[1] : NULL;
        int typeLiteralOrderError;

        /*
            比较表达式必须比较值。
            函数名和类型字面量不能进入普通大小比较。
        */
        if ( xl6SemExprIsBareFunctionSymbol(ctx, left) ) {
            xl6SemErrorNode(ctx, left, "函数名不能作为普通比较操作数");
            ok = 0;
        }
        if ( xl6SemExprIsBareFunctionSymbol(ctx, right) ) {
            xl6SemErrorNode(ctx, right, "函数名不能作为普通比较操作数");
            ok = 0;
        }
        typeLiteralOrderError = 0;
        if ( !xl6SemCompareAllowsTypeLiteral(node->text) ) {
            if ( xl6SemExprIsTypeLiteralSymbol(ctx, left) ) {
                xl6SemErrorNode(ctx, left, "类型字面量只能用于 type 相等性比较");
                typeLiteralOrderError = 1;
                ok = 0;
            }
            if ( xl6SemExprIsTypeLiteralSymbol(ctx, right) ) {
                xl6SemErrorNode(ctx, right, "类型字面量只能用于 type 相等性比较");
                typeLiteralOrderError = 1;
                ok = 0;
            }
        }
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( (leftType != NULL && xl6TextEquals(leftType, "type")) ||
             (rightType != NULL && xl6TextEquals(rightType, "type")) ) {
            int leftIsTypeOperand = (leftType != NULL && xl6TextEquals(leftType, "type")) ||
                                    (left != NULL && left->kind == XL6_AST_NULL);
            int rightIsTypeOperand = (rightType != NULL && xl6TextEquals(rightType, "type")) ||
                                     (right != NULL && right->kind == XL6_AST_NULL);

            if ( !xl6SemCompareAllowsTypeLiteral(node->text) ) {
                if ( !typeLiteralOrderError ) {
                    xl6SemErrorNode(ctx, node, "type 值只能用于相等性比较");
                    ok = 0;
                }
            } else if ( !leftIsTypeOperand || !rightIsTypeOperand ) {
                xl6SemErrorNode(ctx, node, "type 值只能和 type 值或类型字面量比较");
                ok = 0;
            }
        }
        if ( leftType != NULL && rightType != NULL &&
             (xl6TextEquals(leftType, "string") || xl6TextEquals(rightType, "string")) ) {
            if ( !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
                 (!xl6TextEquals(leftType, "string") || !xl6TextEquals(rightType, "string")) ) {
                xl6SemErrorNode(ctx, node, "string comparison operands must both be string");
                ok = 0;
            }
        }
        if ( leftType != NULL && rightType != NULL &&
             (xl6TextEquals(leftType, "time") || xl6TextEquals(rightType, "time")) ) {
            if ( !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
                 (!xl6TextEquals(leftType, "time") || !xl6TextEquals(rightType, "time")) ) {
                xl6SemErrorNode(ctx, node, "time comparison operands must both be time");
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemTimeLiteralDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static int xl6SemTimeLiteralShape(const char* body, size_t len)
{
    size_t i;

    if ( body == NULL ) {
        return 0;
    }
    if ( len == 10 ) {
        if ( body[4] != '-' || body[7] != '-' ) {
            return 0;
        }
        for ( i = 0; i < len; ++i ) {
            if ( i != 4 && i != 7 && !xl6SemTimeLiteralDigit(body[i]) ) {
                return 0;
            }
        }
        return 1;
    }
    if ( len == 8 ) {
        if ( body[2] != ':' || body[5] != ':' ) {
            return 0;
        }
        for ( i = 0; i < len; ++i ) {
            if ( i != 2 && i != 5 && !xl6SemTimeLiteralDigit(body[i]) ) {
                return 0;
            }
        }
        return 1;
    }
    if ( len == 19 && body[10] == ' ' ) {
        return xl6SemTimeLiteralShape(body, 10) && xl6SemTimeLiteralShape(body + 11, 8);
    }
    return 0;
}

static int xl6SemCheckTimeLiteralExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* text;
    const char* body;
    size_t len;
    size_t bodyLen;
    const char* error;

    text = node != NULL ? node->text : NULL;
    len = text != NULL ? strlen(text) : 0;
    if ( len < 3 || text[0] != '#' || text[len - 1] != '#' ) {
        xl6SemErrorNode(ctx, node, "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#");
        return 0;
    }

    body = text + 1;
    bodyLen = len - 2;
    if ( !xl6SemTimeLiteralShape(body, bodyLen) ) {
        xl6SemErrorNode(ctx, node, "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#");
        return 0;
    }

    xrtClearError();
    (void)xrtStrToTime((char*)body, bodyLen);
    error = (const char*)xrtGetError();
    if ( error != NULL && error[0] != '\0' ) {
        xl6SemErrorNode(ctx, node, error);
        xrtClearError();
        return 0;
    }
    return 1;
}

static int xl6SemCheckConditionalExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( node != NULL && node->childCount > 0 &&
         !xl6SemCheckBoolExpr(ctx, node->children[0], "条件表达式条件必须是 bool") ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckChildrenExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    return xl6SemCheckChildrenExprFrom(ctx, node, 0);
}

static int xl6SemCheckChildrenExprFrom(XL6_SemContext* ctx, XL6_AstNode* node, size_t firstChild)
{
    size_t i;
    int ok;

    ok = 1;
    if ( node == NULL ) {
        return ok;
    }
    for ( i = firstChild; i < node->childCount; ++i ) {
        if ( !xl6SemCheckExpr(ctx, node->children[i]) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckIndexExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* receiver;
    XL6_AstNode* index;
    XL6_AstNode* receiverTypeNode = NULL;
    const XL6_AstNode* concreteType;
    XL6_AstNode* receiverDecl;
    const char* receiverType;
    const char* indexType;
    size_t argCount;
    int ok;

    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    if ( receiver != NULL && receiver->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* propertyType = xl6SemClonePropertyIndexGetterType(ctx, node, 0);
        if ( propertyType != NULL ) {
            size_t i;
            ok = 1;
            if ( receiver->childCount > 0 && !xl6SemCheckExpr(ctx, receiver->children[0]) ) {
                ok = 0;
            }
            for ( i = 0; i < xl6SemIndexArgCount(node); ++i ) {
                XL6_AstNode* arg = xl6SemIndexArgAt(node, i);
                if ( arg != NULL && !xl6SemCheckExpr(ctx, arg) ) {
                    ok = 0;
                }
            }
            xl6AstFree(propertyType);
            return ok;
        }
    }
    ok = xl6SemCheckChildrenExpr(ctx, node);
    index = xl6SemIndexArgAt(node, 0);
    argCount = xl6SemIndexArgCount(node);
    receiverType = receiver != NULL ? xl6SemExprStaticTypeName(ctx, receiver) : NULL;
    indexType = index != NULL ? xl6SemExprStaticTypeName(ctx, index) : NULL;

    if ( receiverType == NULL ) {
        return ok;
    }

    if ( xl6SemIsDictTypeName(receiverType) ) {
        if ( argCount != 1 ) {
            xl6SemErrorNode(ctx, node, "dict index supports exactly one key");
            return 0;
        }
        if ( indexType == NULL ) {
            return ok;
        }
        if ( !xl6TextEquals(indexType, "string") && !xl6TextEquals(indexType, "var") ) {
            xl6SemErrorNode(ctx, index, "dict 索引键必须是 string");
            ok = 0;
        }
    } else if ( xl6SemIsArrayTypeName(receiverType) || xl6SemIsListTypeName(receiverType) ) {
        if ( argCount != 1 ) {
            xl6SemErrorNode(ctx, node, "array/list index supports exactly one key");
            return 0;
        }
        if ( indexType == NULL ) {
            return ok;
        }
        if ( !xl6SemIsIntegerTypeName(indexType) ) {
            xl6SemErrorNode(ctx, index, "array/list 索引必须是 int");
            ok = 0;
        }
    } else if ( xl6TextEquals(receiverType, "var") ) {
        if ( argCount != 1 ) {
            xl6SemErrorNode(ctx, node, "var container index supports exactly one key");
            return 0;
        }
        if ( indexType == NULL ) {
            return ok;
        }
        if ( !xl6TextEquals(indexType, "var") && !xl6TextEquals(indexType, "string") && !xl6SemIsIntegerTypeName(indexType) ) {
            xl6SemErrorNode(ctx, index, "var container index key must be int, string or var");
            ok = 0;
        }
    } else {
        const XL6_AstNode* protocolMethod;
        receiverTypeNode = xl6SemInferAutoTypeNode(ctx, receiver);
        concreteType = xl6SemResolveAliasType(ctx, receiverTypeNode);
        receiverDecl = concreteType != NULL && concreteType->text != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
        protocolMethod = receiverDecl != NULL && receiverDecl->kind == XL6_AST_CLASS ?
                         xl6SemFindProtocolMethodForIndex(ctx, node, "__item__", NULL, 1) :
                         NULL;
        if ( protocolMethod != NULL &&
         !xl6SemCheckProtocolArg(ctx, node, protocolMethod, 0, index, "索引协议参数类型不匹配") ) {
            ok = 0;
        }
    }
    xl6AstFree(receiverTypeNode);
    return ok;
}

static int xl6SemSafeMemberGenericIsVar(XL6_SemContext* ctx, const XL6_AstNode* concreteType)
{
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;

    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount == 0 ) {
        return 1;
    }
    if ( args->childCount != 1 ) {
        return 0;
    }
    itemType = xl6SemResolveAliasType(ctx, args->children[0]);
    return itemType != NULL && xl6TextEquals(itemType->text, "var");
}

static int xl6SemSafeMemberLengthReceiverIsContainer(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6TextEquals(name, "var") ) {
        return 1;
    }
    if ( !xl6SemIsArrayTypeName(name) &&
         !xl6SemIsListTypeName(name) &&
         !xl6TextEquals(name, "set") &&
         !xl6TextEquals(name, "set<var>") &&
         !xl6SemIsDictTypeName(name) ) {
        return 0;
    }
    return 1;
}

static int xl6SemSafeMemberKeyReceiverIsDynamic(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6TextEquals(name, "var") ) {
        return 1;
    }
    if ( !xl6SemIsDictTypeName(name) ) {
        return 0;
    }
    return xl6SemSafeMemberGenericIsVar(ctx, concreteType);
}

static int xl6SemMemberReceiverIsXValueContainer(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    return xl6TextEquals(name, "var") ||
           xl6SemIsArrayTypeName(name) ||
           xl6SemIsListTypeName(name) ||
           xl6TextEquals(name, "set") ||
           xl6TextEquals(name, "set<var>") ||
           xl6SemIsDictTypeName(name);
}

static XL6_SemContainerKind xl6SemContainerKindFromType(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    int* pTyped)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;
    const char* name;

    if ( pTyped != NULL ) {
        *pTyped = 0;
    }
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6TextEquals(name, "var") ) {
        return XL6_SEM_CONTAINER_VAR;
    }
    if ( name == NULL ) {
        return XL6_SEM_CONTAINER_NONE;
    }

    args = xl6SemTypeNodeGenericArgs(concreteType);
    itemType = args != NULL && args->childCount == 1 ?
               xl6SemResolveAliasType(ctx, args->children[0]) : NULL;
    if ( pTyped != NULL && itemType != NULL && !xl6TextEquals(itemType->text, "var") ) {
        *pTyped = 1;
    }

    if ( xl6TextEquals(name, "array") || xl6TextEquals(name, "array<var>") ) {
        return XL6_SEM_CONTAINER_ARRAY;
    }
    if ( xl6TextEquals(name, "list") || xl6TextEquals(name, "list<var>") ) {
        return XL6_SEM_CONTAINER_LIST;
    }
    if ( xl6TextEquals(name, "set") || xl6TextEquals(name, "set<var>") ) {
        return XL6_SEM_CONTAINER_SET;
    }
    if ( xl6TextEquals(name, "dict") || xl6TextEquals(name, "dict<var>") ) {
        return XL6_SEM_CONTAINER_DICT;
    }
    return XL6_SEM_CONTAINER_NONE;
}

static const XL6_AstNode* xl6SemContainerItemType(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    args = xl6SemTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        return NULL;
    }
    return xl6SemResolveAliasType(ctx, args->children[0]);
}

static int xl6SemContainerMethodSupported(XL6_SemContainerKind kind, int typed, const char* memberName)
{
    if ( memberName == NULL || kind == XL6_SEM_CONTAINER_NONE ) {
        return 0;
    }
    if ( kind == XL6_SEM_CONTAINER_VAR ) {
        return xl6TextEquals(memberName, "push") ||
               xl6TextEquals(memberName, "append") ||
               xl6TextEquals(memberName, "insert") ||
               xl6TextEquals(memberName, "remove") ||
               xl6TextEquals(memberName, "removeValue") ||
               xl6TextEquals(memberName, "pop") ||
               xl6TextEquals(memberName, "take") ||
               xl6TextEquals(memberName, "clear") ||
               xl6TextEquals(memberName, "reserve") ||
               xl6TextEquals(memberName, "swap") ||
               xl6TextEquals(memberName, "add") ||
               xl6TextEquals(memberName, "contains");
    }
    if ( kind == XL6_SEM_CONTAINER_ARRAY ) {
        if ( typed ) {
            return xl6TextEquals(memberName, "push") ||
                   xl6TextEquals(memberName, "append") ||
                   xl6TextEquals(memberName, "insert") ||
                   xl6TextEquals(memberName, "remove") ||
                   xl6TextEquals(memberName, "removeValue") ||
                   xl6TextEquals(memberName, "pop") ||
                   xl6TextEquals(memberName, "take") ||
                   xl6TextEquals(memberName, "clear") ||
                   xl6TextEquals(memberName, "contains") ||
                   xl6TextEquals(memberName, "indexOf") ||
                   xl6TextEquals(memberName, "reverse");
        }
        return xl6TextEquals(memberName, "push") ||
               xl6TextEquals(memberName, "append") ||
               xl6TextEquals(memberName, "insert") ||
               xl6TextEquals(memberName, "remove") ||
               xl6TextEquals(memberName, "removeValue") ||
               xl6TextEquals(memberName, "pop") ||
               xl6TextEquals(memberName, "take") ||
               xl6TextEquals(memberName, "clear") ||
               xl6TextEquals(memberName, "reserve") ||
               xl6TextEquals(memberName, "swap") ||
               xl6TextEquals(memberName, "contains");
    }
    if ( kind == XL6_SEM_CONTAINER_LIST ) {
        if ( typed ) {
            return xl6TextEquals(memberName, "remove") ||
                   xl6TextEquals(memberName, "take") ||
                   xl6TextEquals(memberName, "clear") ||
                   xl6TextEquals(memberName, "contains");
        }
        return xl6TextEquals(memberName, "remove") ||
               xl6TextEquals(memberName, "take") ||
               xl6TextEquals(memberName, "clear") ||
               xl6TextEquals(memberName, "contains");
    }
    if ( kind == XL6_SEM_CONTAINER_SET ) {
        return xl6TextEquals(memberName, "add") ||
               xl6TextEquals(memberName, "remove") ||
               xl6TextEquals(memberName, "contains") ||
               xl6TextEquals(memberName, "clear");
    }
    if ( kind == XL6_SEM_CONTAINER_DICT ) {
        return xl6TextEquals(memberName, "remove") ||
               xl6TextEquals(memberName, "contains") ||
               xl6TextEquals(memberName, "take") ||
               xl6TextEquals(memberName, "clear");
    }
    return 0;
}

static int xl6SemMemberIsContainerMethod(XL6_SemContext* ctx, const XL6_AstNode* typeNode, const char* memberName)
{
    int typed;
    XL6_SemContainerKind kind;

    kind = xl6SemContainerKindFromType(ctx, typeNode, &typed);
    return xl6SemContainerMethodSupported(kind, typed, memberName);
}

static int xl6SemCheckSafeMemberExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( node == NULL ) {
        return ok;
    }

    receiver = node->childCount > 0 ? node->children[0] : NULL;
    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    if ( receiverType == NULL ) {
        return ok;
    }

    if ( xl6TextEquals(node->text, "length") ) {
        if ( !xl6SemSafeMemberLengthReceiverIsContainer(ctx, receiverType) ) {
            xl6SemErrorNode(ctx, receiver, "safe member length receiver must be a container");
            ok = 0;
        }
    } else if ( !xl6SemSafeMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        xl6SemErrorNode(ctx, node, "safe member key receiver must be var or dynamic dict");
        ok = 0;
    }

    xl6AstFree(receiverType);
    return ok;
}
static int xl6SemCheckMemberExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* fieldType;
    const XL6_AstNode* concreteReceiverType;
    const XL6_AstNode* receiverDecl;
    const char* receiverTypeName;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    receiverTypeName = receiver != NULL ? xl6SemExprStaticTypeName(ctx, receiver) : NULL;
    if ( xl6TextEquals(receiverTypeName, "error") &&
         !xl6TextEquals(node->text, "kind") &&
         !xl6TextEquals(node->text, "message") &&
         !xl6TextEquals(node->text, "file") &&
         !xl6TextEquals(node->text, "line") &&
         !xl6TextEquals(node->text, "column") ) {
        xl6SemErrorNode(ctx, node, "error only supports kind/message/file/line/column fields");
        ok = 0;
    }

    if ( node->text != NULL ) {
        XL6_AstNode* staticClass = xl6SemClassDeclFromTypeMemberReceiver(ctx, node);
        const XL6_AstNode* ownerClass = NULL;
        const XL6_AstNode* staticField = staticClass != NULL ?
            xl6SemFindStaticClassFieldNode(ctx, staticClass, node->text, &ownerClass) :
            NULL;

        if ( staticField != NULL && !xl6SemCheckClassMemberAccess(ctx, node, ownerClass, staticField) ) {
            return 0;
        }
    }

    if ( receiver != NULL && node->text != NULL ) {
        XL6_AstNode* accessType = xl6SemInferAutoTypeNode(ctx, receiver);
        const XL6_AstNode* accessConcreteType = xl6SemClassValueType(ctx, accessType);
        XL6_AstNode* accessDecl = accessConcreteType != NULL ? xl6SemFindTypeDecl(ctx, accessConcreteType->text) : NULL;
        const XL6_AstNode* ownerClass = NULL;
        const XL6_AstNode* field = accessDecl != NULL && accessDecl->kind == XL6_AST_CLASS ?
            xl6SemFindClassFieldNode(ctx, accessDecl, node->text, &ownerClass) :
            NULL;

        if ( field != NULL && !xl6SemCheckClassMemberAccess(ctx, node, ownerClass, field) ) {
            xl6AstFree(accessType);
            return 0;
        }
        xl6AstFree(accessType);
    }

    fieldType = xl6SemCloneMemberFieldType(ctx, node);
    if ( fieldType != NULL ) {
        xl6AstFree(fieldType);
        return ok;
    }

    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    concreteReceiverType = xl6SemClassValueType(ctx, receiverType);
    receiverDecl = concreteReceiverType != NULL ? xl6SemFindTypeDecl(ctx, concreteReceiverType->text) : NULL;
    if ( receiverDecl != NULL && receiverDecl->kind == XL6_AST_CLASS && xl6SemClassHasCallableMember(ctx, receiverDecl, node->text) ) {
        xl6AstFree(receiverType);
        return ok;
    }
    if ( receiverDecl != NULL && (receiverDecl->kind == XL6_AST_CLASS || receiverDecl->kind == XL6_AST_UNION) ) {
        xl6SemErrorNode(ctx, node, "unknown class/union field");
        ok = 0;
    } else if ( xl6SemMemberIsContainerMethod(ctx, receiverType, node->text) ) {
        /*
            容器成员 API 是数组、列表、集合、字典的隐性类方法。
            这里放行这些方法名，避免把 table 的 remove 方法误判为动态字段访问。
        */
    } else if ( !xl6TextEquals(node->text, "length") &&
                xl6SemMemberReceiverIsXValueContainer(ctx, receiverType) &&
                !xl6SemSafeMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        xl6SemErrorNode(ctx, node, "member key receiver must be var or dynamic dict");
        ok = 0;
    }
    xl6AstFree(receiverType);
    return ok;
}

static int xl6SemCheckFStringExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;
    int ok;

    ok = 1;
    if ( node == NULL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* child = node->children[i];
        if ( child == NULL || child->kind == XL6_AST_INTERP_TEXT ) {
            continue;
        }
        if ( !xl6SemCheckExpr(ctx, child) ) {
            ok = 0;
            continue;
        }
        if ( !xl6SemExprHasImplicitToString(ctx, child) ) {
            xl6SemErrorNode(ctx, child, "f-string 插值表达式不能转换为 string");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckBoundarySymbolUse(XL6_SemContext* ctx, const XL6_AstNode* node, XL6_Symbol* sym, int writeAccess)
{
    if ( ctx == NULL || node == NULL || sym == NULL || node->text == NULL ) {
        return 1;
    }
    if ( !xl6ScopeNameIsOutsideBoundary(ctx->scope, ctx->captureBoundary, node->text) ) {
        return 1;
    }
    if ( sym->kind == XL6_SYM_FUNC || xl6SemSymbolIsGlobalValue(sym) ) {
        return 1;
    }
    if ( !ctx->captureBoundaryAllowsCapture ) {
        xl6SemErrorNode(ctx, node, "函数体不能隐式捕获外层局部变量");
        return 0;
    }
    if ( xl6SemCurrentParamModeByName(ctx, node->text) == XL6_AST_FLAG_BYREF ||
         xl6SemCurrentParamModeByName(ctx, node->text) == XL6_AST_FLAG_OUT ) {
        xl6SemErrorNode(ctx, node, "匿名函数不能捕获 byref 或 out 参数");
        return 0;
    }
    if ( writeAccess ) {
        xl6SemErrorNode(ctx, node, "捕获变量在匿名函数体内只读");
        return 0;
    }
    if ( !xl6SemTypeCanCapture(ctx, sym->type) ) {
        xl6SemErrorNode(ctx, node, "该变量类型不能被匿名函数按值捕获");
        return 0;
    }
    return 1;
}

static int xl6SemCheckNameExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Symbol* sym;
    char message[256];

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_NAME ) {
        return 1;
    }
    sym = xl6ScopeFind(ctx->scope, node->text);
    if ( sym != NULL ) {
        return xl6SemCheckBoundarySymbolUse(ctx, node, sym, 0);
    }

    /*
        类型标识符本身是 type 值；class 成员函数内还允许省略 self 访问字段。
        task、range、type、copy、clone 是语义层直接处理的编译器内建，不会由
        标准库展开阶段生成普通函数符号。
    */
    if ( xl6SemNameIsRegisteredType(ctx, node->text) ||
         (ctx->implicitSelfName != NULL && ctx->currentClass != NULL &&
          xl6SemFindClassFieldNode(ctx, ctx->currentClass, node->text, NULL) != NULL) ||
         xl6TextEquals(node->text, "task") ||
         xl6TextEquals(node->text, "range") ||
         xl6TextEquals(node->text, "type") ||
         xl6TextEquals(node->text, "copy") ||
         xl6TextEquals(node->text, "clone") ||
         (ctx->currentClass != NULL && xl6TextEquals(node->text, "base")) ) {
        return 1;
    }

    snprintf(message, sizeof(message), "undeclared identifier '%s'", node->text != NULL ? node->text : "");
    xl6SemErrorNode(ctx, node, message);
    return 0;
}

static int xl6SemAstKindCarriesValueType(XL6_AstKind kind)
{
    return kind == XL6_AST_NAME ||
           kind == XL6_AST_INT ||
           kind == XL6_AST_FLOAT ||
           kind == XL6_AST_STRING ||
           kind == XL6_AST_INTERP_STRING ||
           kind == XL6_AST_TIME ||
           kind == XL6_AST_BOOL ||
           kind == XL6_AST_NULL ||
           kind == XL6_AST_CALL ||
           kind == XL6_AST_MEMBER ||
           kind == XL6_AST_SAFE_MEMBER ||
           kind == XL6_AST_INDEX ||
           kind == XL6_AST_SAFE_INDEX ||
           kind == XL6_AST_GENERIC_INSTANT ||
           kind == XL6_AST_CONDITIONAL ||
           kind == XL6_AST_BINARY ||
           kind == XL6_AST_TYPE_IS ||
           kind == XL6_AST_TYPE_AS ||
           kind == XL6_AST_UNARY ||
           kind == XL6_AST_AWAIT ||
           kind == XL6_AST_ANON_FUNC ||
           kind == XL6_AST_ARRAY_LITERAL ||
           kind == XL6_AST_LIST_LITERAL ||
           kind == XL6_AST_SET_LITERAL ||
           kind == XL6_AST_DICT_LITERAL ||
           kind == XL6_AST_TYPED_LITERAL;
}

static int xl6SemCheckExprRaw(XL6_SemContext* ctx, XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 1;
    }

    switch ( node->kind ) {
    case XL6_AST_NAME:
        return xl6SemCheckNameExpr(ctx, node);
    case XL6_AST_TIME:
        return xl6SemCheckTimeLiteralExpr(ctx, node);
    case XL6_AST_INTERP_STRING:
        return xl6SemCheckFStringExpr(ctx, node);
    case XL6_AST_ANON_FUNC:
        return xl6SemCheckAnonFunc(ctx, node);
    case XL6_AST_CALL:
        return xl6SemCheckCall(ctx, node);
    case XL6_AST_GENERIC_INSTANT:
        return xl6SemCheckGenericInstant(ctx, node);
    case XL6_AST_TYPED_LITERAL:
        return xl6SemCheckTypedLiteral(ctx, node);
    case XL6_AST_TYPE:
        return xl6SemCheckType(ctx, node);
    case XL6_AST_CONDITIONAL:
        return xl6SemCheckConditionalExpr(ctx, node);
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        return xl6SemCheckIndexExpr(ctx, node);
    case XL6_AST_MEMBER:
        return xl6SemCheckMemberExpr(ctx, node);
    case XL6_AST_SAFE_MEMBER:
        return xl6SemCheckSafeMemberExpr(ctx, node);
    case XL6_AST_BINARY:
        return xl6SemCheckBinaryExpr(ctx, node);
    case XL6_AST_TYPE_IS:
        return xl6SemCheckRuntimeTypeExpr(ctx, node, 0);
    case XL6_AST_TYPE_AS:
        return xl6SemCheckRuntimeTypeExpr(ctx, node, 1);
    case XL6_AST_UNARY:
        return xl6SemCheckUnaryExpr(ctx, node);
    case XL6_AST_AWAIT:
        return xl6SemCheckAwaitExpr(ctx, node);
    default:
        return xl6SemCheckChildrenExpr(ctx, node);
    }
}

static int xl6SemDeclareVarWithKind(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    XL6_SymbolKind valueKind,
    XL6_SymbolKind constKind)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* declaredType;
    XL6_AstNode* symbolType;
    XL6_AstNode* initExpr;
    XL6_AstNode* inferredType;
    int ok;
    int declaredTypeOk;

    ok = 1;
    declaredTypeOk = 1;
    nameNode = xl6SemVarDeclNameNode(node);
    declaredType = xl6SemVarDeclTypeNode(node);
    symbolType = declaredType;
    initExpr = xl6SemVarDeclInitExpr(node);

    if ( declaredType != NULL ) {
        if ( !xl6SemCheckType(ctx, declaredType) ) {
            ok = 0;
            declaredTypeOk = 0;
        }
        if ( xl6TextEquals(declaredType->text, "auto") && initExpr == NULL ) {
        xl6SemErrorNode(ctx, node, "auto 声明必须包含初始化表达式");
            ok = 0;
        }
        if ( xl6TextEquals(declaredType->text, "auto") && initExpr != NULL ) {
            inferredType = xl6SemInferAutoTypeNode(ctx, initExpr);
            if ( inferredType != NULL ) {
                if ( xl6SemOwnTypeNode(ctx, inferredType) ) {
                    symbolType = inferredType;
                } else {
                    xl6AstFree(inferredType);
                    symbolType = declaredType;
                    ok = 0;
                }
            }
        }
    }
    if ( !xl6ScopeAddTyped(ctx, nameNode, xl6SemVarDeclIsConst(node) ? constKind : valueKind, symbolType) ) {
        ok = 0;
    }
    if ( initExpr != NULL && !xl6SemCheckExpr(ctx, initExpr) ) {
        ok = 0;
    }
    if ( initExpr != NULL &&
         !xl6SemCheckCopyableExpr(ctx, initExpr, 1, "不可复制类型不能从已有值初始化") ) {
        ok = 0;
    }
    if ( initExpr != NULL && xl6SemExprIsMultiReturnCall(ctx, initExpr) ) {
        xl6SemErrorNode(ctx, initExpr, "多返回值函数调用必须使用多值接收");
        ok = 0;
    }
    if ( initExpr != NULL && declaredType != NULL && declaredTypeOk &&
         !xl6TextEquals(declaredType->text, "auto") &&
         !xl6SemExprCanMatchParamType(ctx, initExpr, declaredType) ) {
        if ( !xl6SemExprFunctionValueMatchAlreadyDiagnosed(ctx, initExpr, declaredType) ) {
            XL6_AstNode* actualType = xl6SemInferAutoTypeNode(ctx, initExpr);
            char actualTypeText[256];

            actualTypeText[0] = '\0';
            xl6SemFormatTypeName(ctx, actualType, actualTypeText, sizeof(actualTypeText));
            xl6SemTypeMismatchError(
                ctx,
                initExpr,
                "初始化表达式类型和声明类型不匹配",
                declaredType,
                actualTypeText[0] != '\0' ? actualTypeText : xl6SemExprStaticTypeName(ctx, initExpr));
            xl6AstFree(actualType);
        }
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok = xl6SemCheckExprRaw(ctx, node);

    if ( ok && node != NULL &&
         node->inferredType == NULL &&
         xl6SemAstKindCarriesValueType(node->kind) ) {
        XL6_AstNode* inferredType = xl6SemInferAutoTypeNode(ctx, node);
        xl6AstFree(inferredType);
    }
    return ok;
}

static int xl6SemDeclareVar(XL6_SemContext* ctx, XL6_AstNode* node)
{
    return xl6SemDeclareVarWithKind(ctx, node, XL6_SYM_VALUE, XL6_SYM_CONST);
}

static int xl6SemDeclareModuleGlobalVar(XL6_SemContext* ctx, XL6_AstNode* node)
{
    return xl6SemDeclareVarWithKind(ctx, node, XL6_SYM_GLOBAL_VALUE, XL6_SYM_GLOBAL_CONST);
}

static int xl6SemIsModuleGlobalDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_VAR_DECL ||
            node->kind == XL6_AST_CONST_DECL ||
            xl6SemIsPackageExportVarDecl(node));
}

static int xl6SemTypeCanAssignFromType(XL6_SemContext* ctx, const XL6_AstNode* valueType, const XL6_AstNode* targetType)
{
    if ( targetType == NULL || valueType == NULL ) {
        return 1;
    }
    if ( xl6TextEquals(targetType->text, "auto") ) {
        return 1;
    }
    if ( xl6TextEquals(targetType->text, "var") ) {
        return xl6SemTypeCanBoxToVar(ctx, valueType);
    }
    if ( xl6TextEquals(valueType->text, "null") && xl6TextEquals(targetType->text, "point") ) {
        return 1;
    }
    if ( xl6TextEquals(valueType->text, "null") && xl6SemClassHandleTargetType(ctx, targetType) != NULL ) {
        return 1;
    }
    if ( xl6SemClassHandleCanWiden(ctx, valueType, targetType) ) {
        return 1;
    }
    return xl6SemTypeEquals(ctx, valueType, targetType);
}

static int xl6SemCheckMultiVarDecl(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* names;
    XL6_AstNode* expr;
    XL6_AstNode* returns;
    XL6_AstNode* ownedReturnType;
    size_t i;
    int ok;

    ok = 1;
    names = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;

    if ( expr == NULL || !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    ownedReturnType = NULL;
    returns = xl6SemCallReturnTypesEx(ctx, expr, &ownedReturnType);
    if ( returns == NULL || returns->childCount <= 1 ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : node, "多返回值声明右侧必须是多返回值函数调用");
        ok = 0;
    } else if ( names == NULL || names->childCount != returns->childCount ) {
        xl6SemCountMismatchError(
            ctx,
            node,
            "多返回值声明数量和函数返回值数量不匹配",
            returns->childCount,
            names != NULL ? names->childCount : 0);
        ok = 0;
    }

    if ( names != NULL ) {
        for ( i = 0; i < names->childCount; ++i ) {
            XL6_AstNode* name = names->children[i];
            XL6_AstNode* typeNode = (returns != NULL && i < returns->childCount) ? returns->children[i] : NULL;

            if ( name == NULL || name->kind != XL6_AST_NAME ) {
            xl6SemErrorNode(ctx, name != NULL ? name : node, "多返回值声明目标必须是变量名");
                ok = 0;
                continue;
            }
            if ( xl6TextEquals(name->text, "_") ) {
                continue;
            }
            if ( typeNode != NULL ) {
                XL6_AstNode* symbolType = xl6SemCloneTypeNode(typeNode);
                if ( symbolType == NULL || !xl6SemOwnTypeNode(ctx, symbolType) ) {
                    xl6AstFree(symbolType);
                    ok = 0;
                    continue;
                }
                typeNode = symbolType;
            }
            if ( !xl6ScopeAddTyped(ctx, name, XL6_SYM_VALUE, typeNode) ) {
                ok = 0;
            }        }
    }
    xl6AstFree(ownedReturnType);
    return ok;
}

static int xl6SemCompoundAssignIsSimpleNumericOp(const char* op)
{
    return xl6TextEquals(op, "+=") ||
           xl6TextEquals(op, "-=") ||
           xl6TextEquals(op, "*=") ||
           xl6TextEquals(op, "/=");
}

static int xl6SemCompoundAssignIsIntegerOp(const char* op)
{
    return xl6TextEquals(op, "%=") ||
           xl6TextEquals(op, "&=") ||
           xl6TextEquals(op, "|=") ||
           xl6TextEquals(op, "^=") ||
           xl6TextEquals(op, "<<=") ||
           xl6TextEquals(op, ">>=");
}

static int xl6SemCompoundAssignIsIndexOnlyUnsupportedOp(const char* op)
{
    return xl6TextEquals(op, "&=") ||
           xl6TextEquals(op, "|=") ||
           xl6TextEquals(op, "^=") ||
           xl6TextEquals(op, "<<=") ||
           xl6TextEquals(op, ">>=");
}

static void xl6SemCompoundAssignError(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    const char* reason,
    const char* op,
    const char* leftType,
    const char* rightType)
{
    char message[256];

    snprintf(message, sizeof(message), "%s: op %s, left %s, right %s",
             reason != NULL ? reason : "compound assignment error",
             op != NULL ? op : "<missing>",
             leftType != NULL ? leftType : "<unknown>",
             rightType != NULL ? rightType : "<unknown>");
    xl6SemErrorNode(ctx, node, message);
}

static const char* xl6SemDisplayTypeName(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    name = xl6SemStaticTypeNameFromTypeNode(ctx, typeNode);
    if ( name != NULL ) {
        return name;
    }

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType != NULL && concreteType->text != NULL ) {
        return concreteType->text;
    }
    return typeNode != NULL ? typeNode->text : NULL;
}

static void xl6SemFormatTypeNameDepth(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    char* buffer,
    size_t capacity,
    int depth)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    size_t i;
    size_t used;

    if ( buffer == NULL || capacity == 0 || typeNode == NULL || depth > 32 ) {
        return;
    }
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL ) {
        concreteType = typeNode;
    }
    if ( concreteType->text == NULL ) {
        return;
    }
    args = concreteType->childCount > 0 ? concreteType->children[0] : NULL;
    if ( xl6TextEquals(concreteType->text, "__class_handle") &&
         args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1 ) {
        xl6SemFormatTypeNameDepth(ctx, args->children[0], buffer, capacity, depth + 1);
        return;
    }
    used = strlen(buffer);
    if ( used < capacity ) {
        snprintf(buffer + used, capacity - used, "%s", concreteType->text);
    }
    if ( xl6TextEquals(concreteType->text, "function") || concreteType->childCount == 0 ) {
        return;
    }
    args = concreteType->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST || args->childCount == 0 ) {
        return;
    }
    used = strlen(buffer);
    if ( used < capacity ) {
        snprintf(buffer + used, capacity - used, "<");
    }
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            used = strlen(buffer);
            if ( used < capacity ) {
                snprintf(buffer + used, capacity - used, ", ");
            }
        }
        xl6SemFormatTypeNameDepth(ctx, args->children[i], buffer, capacity, depth + 1);
    }
    used = strlen(buffer);
    if ( used < capacity ) {
        snprintf(buffer + used, capacity - used, ">");
    }
}

static void xl6SemFormatTypeName(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    char* buffer,
    size_t capacity)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6SemFormatTypeNameDepth(ctx, typeNode, buffer, capacity, 0);
}

    /* 类型不匹配需要直接说明目标类型和值类型，避免只显示泛化错误。 */
static void xl6SemTypeMismatchError(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    const char* reason,
    const XL6_AstNode* targetType,
    const char* valueType)
{
    const char* targetTypeName;
    char targetTypeText[256];
    char message[256];

    targetTypeText[0] = '\0';
    xl6SemFormatTypeName(ctx, targetType, targetTypeText, sizeof(targetTypeText));
    targetTypeName = targetTypeText[0] != '\0' ? targetTypeText : xl6SemDisplayTypeName(ctx, targetType);
    snprintf(message, sizeof(message), "%s: target %s, value %s",
             reason != NULL ? reason : "assignment type mismatch",
             targetTypeName != NULL ? targetTypeName : "<unknown>",
             valueType != NULL ? valueType : "<unknown>");
    xl6SemErrorNode(ctx, node, message);
}

    /* 类型转换错误使用 source 描述输入类型，比赋值场景中的 value 更直观。 */
static void xl6SemConversionMismatchError(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    const char* targetType,
    const char* sourceType)
{
    char message[256];

    snprintf(message, sizeof(message), "该表达式不能转换为目标类型: target %s, source %s",
             targetType != NULL ? targetType : "<unknown>",
             sourceType != NULL ? sourceType : "<unknown>");
    xl6SemErrorNode(ctx, node, message);
}

    /* var(...) 装箱失败需要暴露源类型，否则 class 生命周期边界很难定位。 */
static void xl6SemVarBoxMismatchError(XL6_SemContext* ctx, XL6_AstNode* node, const char* reason)
{
    XL6_AstNode* sourceType;
    const char* sourceTypeName;
    char message[256];

    sourceType = xl6SemInferAutoTypeNode(ctx, node);
    sourceTypeName = xl6SemDisplayTypeName(ctx, sourceType);
    snprintf(message, sizeof(message), "%s: target var, source %s",
             reason != NULL ? reason : "该表达式不能装入 var",
             sourceTypeName != NULL ? sourceTypeName : "<unknown>");
    xl6SemErrorNode(ctx, node, message);
    xl6AstFree(sourceType);
}

static void xl6SemCountMismatchError(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    const char* reason,
    size_t expected,
    size_t actual)
{
    char message[256];

    snprintf(message, sizeof(message), "%s: expected %lu, actual %lu",
             reason != NULL ? reason : "count mismatch",
             (unsigned long)expected,
             (unsigned long)actual);
    xl6SemErrorNode(ctx, node, message);
}

static int xl6SemCheckContainerCompoundAssignTypes(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    XL6_AstNode* left,
    XL6_AstNode* right,
    const char* op)
{
    XL6_AstNode* leftTypeNode;
    XL6_AstNode* rightTypeNode;
    const XL6_AstNode* leftConcrete;
    const XL6_AstNode* rightConcrete;
    XL6_SemContainerKind leftKind;
    XL6_SemContainerKind rightKind;
    const char* leftName;
    const char* rightName;
    int leftTyped;
    int rightTyped;
    int sameType;
    int result;

    if ( op == NULL || left == NULL || right == NULL ) {
        return -1;
    }

    leftTypeNode = xl6SemInferAutoTypeNode(ctx, left);
    rightTypeNode = xl6SemInferAutoTypeNode(ctx, right);
    leftConcrete = xl6SemResolveAliasType(ctx, leftTypeNode);
    rightConcrete = xl6SemResolveAliasType(ctx, rightTypeNode);
    leftKind = xl6SemContainerKindFromType(ctx, leftConcrete, &leftTyped);
    rightKind = xl6SemContainerKindFromType(ctx, rightConcrete, &rightTyped);
    leftName = xl6SemDisplayTypeName(ctx, leftConcrete);
    rightName = xl6SemDisplayTypeName(ctx, rightConcrete);
    sameType = leftConcrete != NULL && rightConcrete != NULL &&
               xl6SemTypeEquals(ctx, leftConcrete, rightConcrete);
    result = -1;

    if ( leftKind == XL6_SEM_CONTAINER_ARRAY ||
         leftKind == XL6_SEM_CONTAINER_LIST ||
         leftKind == XL6_SEM_CONTAINER_SET ||
         leftKind == XL6_SEM_CONTAINER_DICT ) {
        result = 0;
        if ( !leftTyped ) {
            xl6SemCompoundAssignError(ctx, node, "container compound assignment requires a static typed container", op, leftName, rightName);
        } else if ( leftKind == XL6_SEM_CONTAINER_ARRAY ) {
            if ( xl6TextEquals(op, "+=") && rightKind == XL6_SEM_CONTAINER_ARRAY && rightTyped && sameType ) {
                result = 1;
            } else {
                xl6SemCompoundAssignError(ctx, node, "array compound assignment supports only += with the same array<T>", op, leftName, rightName);
            }
        } else if ( leftKind == XL6_SEM_CONTAINER_LIST ) {
            if ( xl6TextEquals(op, "+=") && rightKind == XL6_SEM_CONTAINER_LIST && rightTyped && sameType ) {
                result = 1;
            } else if ( xl6TextEquals(op, "-=") && rightConcrete != NULL && xl6SemIsIntegerTypeName(rightConcrete->text) ) {
                result = 1;
            } else {
                xl6SemCompoundAssignError(ctx, node, "list compound assignment supports += list<T> or -= int key", op, leftName, rightName);
            }
        } else if ( leftKind == XL6_SEM_CONTAINER_DICT ) {
            if ( xl6TextEquals(op, "+=") && rightKind == XL6_SEM_CONTAINER_DICT && rightTyped && sameType ) {
                result = 1;
            } else if ( xl6TextEquals(op, "-=") && rightConcrete != NULL && xl6TextEquals(rightConcrete->text, "string") ) {
                result = 1;
            } else {
                xl6SemCompoundAssignError(ctx, node, "dict compound assignment supports += dict<T> or -= string key", op, leftName, rightName);
            }
        } else if ( leftKind == XL6_SEM_CONTAINER_SET ) {
            if ( (xl6TextEquals(op, "|=") || xl6TextEquals(op, "&=") ||
                  xl6TextEquals(op, "-=") || xl6TextEquals(op, "^=")) &&
                 rightKind == XL6_SEM_CONTAINER_SET && rightTyped && sameType ) {
                result = 1;
            } else {
                xl6SemCompoundAssignError(ctx, node, "set compound assignment supports |= &= -= ^= with the same set<T>", op, leftName, rightName);
            }
        }
    }

    xl6AstFree(leftTypeNode);
    xl6AstFree(rightTypeNode);
    return result;
}

static int xl6SemCheckCompoundAssignTypes(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    XL6_AstNode* left,
    XL6_AstNode* right,
    const char* op)
{
    const char* leftType;
    const char* rightType;
    int leftIsVar;
    int rightIsVar;
    int dynamicIndexTarget;
    int dynamicRightForTypedIndex;
    int resultIsFloat;
    int containerResult;

    if ( op == NULL || xl6TextEquals(op, "=") || xl6TextEquals(op, "?=") || right == NULL ) {
        return 1;
    }

    leftType = left != NULL ? xl6SemExprStaticTypeName(ctx, left) : NULL;
    rightType = right != NULL ? xl6SemExprStaticTypeName(ctx, right) : NULL;
    if ( leftType == NULL || rightType == NULL ) {
        return 1;
    }

    containerResult = xl6SemCheckContainerCompoundAssignTypes(ctx, node, left, right, op);
    if ( containerResult >= 0 ) {
        return containerResult;
    }

    leftIsVar = xl6TextEquals(leftType, "var");
    rightIsVar = xl6TextEquals(rightType, "var");
    dynamicIndexTarget = left != NULL && left->kind == XL6_AST_INDEX && leftIsVar;
    dynamicRightForTypedIndex = left != NULL && left->kind == XL6_AST_INDEX && !leftIsVar && rightIsVar;

    if ( left != NULL && left->kind == XL6_AST_INDEX &&
         xl6SemCompoundAssignIsIndexOnlyUnsupportedOp(op) ) {
        xl6SemCompoundAssignError(ctx, node, "index compound assignment operator is not supported", op, leftType, rightType);
        return 0;
    }

    if ( leftIsVar && !dynamicIndexTarget ) {
        xl6SemCompoundAssignError(ctx, node, "var compound assignment target is not supported", op, leftType, rightType);
        return 0;
    }
    if ( rightIsVar && !dynamicIndexTarget && !dynamicRightForTypedIndex ) {
        xl6SemCompoundAssignError(ctx, node, "compound assignment var operand is not supported", op, leftType, rightType);
        return 0;
    }

    if ( dynamicIndexTarget && !xl6TextEquals(op, "**=") ) {
        return 1;
    }

    if ( xl6SemCompoundAssignUsesClassProtocol(ctx, left, right, op) ) {
        return 1;
    }

    if ( xl6TextEquals(op, "+=") &&
         (xl6TextEquals(leftType, "string") || xl6TextEquals(rightType, "string")) ) {
        if ( !xl6TextEquals(leftType, "string") ||
             (!xl6TextEquals(rightType, "string") && !dynamicRightForTypedIndex) ) {
            xl6SemCompoundAssignError(ctx, node, "string compound assignment operands must both be string", op, leftType, rightType);
            return 0;
        }
        return 1;
    }

    if ( xl6SemCompoundAssignIsSimpleNumericOp(op) ) {
        if ( !xl6SemIsNumericTypeName(leftType) ||
             (!xl6SemIsNumericTypeName(rightType) && !dynamicRightForTypedIndex) ) {
            xl6SemCompoundAssignError(ctx, node, "numeric compound assignment operands must be numeric", op, leftType, rightType);
            return 0;
        }
        resultIsFloat = xl6SemIsFloatTypeName(leftType) || xl6SemIsFloatTypeName(rightType);
        if ( resultIsFloat && !xl6SemIsFloatTypeName(leftType) ) {
            xl6SemCompoundAssignError(ctx, node, "compound assignment result type does not match target type", op, leftType, rightType);
            return 0;
        }
        if ( xl6TextEquals(op, "/=") && xl6SemExprIsZeroLiteral(right) ) {
            xl6SemErrorNode(ctx, node, "division by zero literal");
            return 0;
        }
        return 1;
    }

    if ( xl6TextEquals(op, "**=") ) {
        if ( !xl6SemIsIntegerTypeName(leftType) ||
             (!xl6SemIsIntegerTypeName(rightType) && !dynamicRightForTypedIndex) ) {
            xl6SemCompoundAssignError(ctx, node, "power compound assignment operands must be integer", op, leftType, rightType);
            return 0;
        }
        return 1;
    }

    if ( xl6SemCompoundAssignIsIntegerOp(op) ) {
        if ( !xl6SemIsIntegerTypeName(leftType) ||
             (!xl6SemIsIntegerTypeName(rightType) && !dynamicRightForTypedIndex) ) {
            xl6SemCompoundAssignError(ctx, node, "integer compound assignment operands must be integer", op, leftType, rightType);
            return 0;
        }
        if ( xl6TextEquals(op, "%=") && xl6SemExprIsZeroLiteral(right) ) {
            xl6SemErrorNode(ctx, node, "modulo by zero literal");
            return 0;
        }
        return 1;
    }

    xl6SemCompoundAssignError(ctx, node, "unsupported compound assignment operator", op, leftType, rightType);
    return 0;
}

static int xl6SemMemberUsesLenProtocol(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* fieldType;
    XL6_AstNode* lenType;
    int result;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ||
         member->text == NULL || !xl6TextEquals(member->text, "length") ||
         member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    if ( receiverType == NULL ) {
        return 0;
    }

    fieldType = xl6SemCloneRecordFieldType(ctx, receiverType, "length");
    if ( fieldType != NULL ) {
        xl6AstFree(fieldType);
        xl6AstFree(receiverType);
        return 0;
    }

    lenType = xl6SemCloneProtocolReturnType(ctx, receiverType, "__len__");
    result = lenType != NULL;
    xl6AstFree(lenType);
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemCheckDynamicMemberAssignTarget(XL6_SemContext* ctx, XL6_AstNode* node, XL6_AstNode* left, const char* op)
{
    XL6_AstNode* fieldType;
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* receiverDecl;
    const char* typeName;
    int ok;

    if ( left == NULL || left->kind != XL6_AST_MEMBER ) {
        return 1;
    }

    if ( xl6SemMemberUsesLenProtocol(ctx, left) ) {
        xl6SemErrorNode(ctx, left, "member length is read-only");
        return 0;
    }

    fieldType = xl6SemCloneMemberFieldType(ctx, left);
    if ( fieldType != NULL ) {
        xl6AstFree(fieldType);
        return 1;
    }

    receiver = left->childCount > 0 ? left->children[0] : NULL;
    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    if ( receiverType == NULL ) {
        return 1;
    }

    ok = 1;
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( xl6SemClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6SemClassHandleTargetType(ctx, concreteType);
    }
    typeName = concreteType != NULL ? concreteType->text : NULL;
    receiverDecl = concreteType != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    if ( xl6TextEquals(left->text, "length") && xl6SemSafeMemberLengthReceiverIsContainer(ctx, receiverType) ) {
        xl6SemErrorNode(ctx, left, "member length is read-only");
        ok = 0;
    } else if ( xl6SemSafeMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        if ( receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER && receiver->kind != XL6_AST_INDEX ) {
            xl6SemErrorNode(ctx, receiver, "dynamic member assignment receiver must be mutable");
            ok = 0;
        } else if ( !xl6TextEquals(op, "=") ) {
            xl6SemErrorNode(ctx, node, "dynamic member assignment only supports =");
            ok = 0;
        }
    } else if ( receiverDecl != NULL &&
                (receiverDecl->kind == XL6_AST_CLASS || receiverDecl->kind == XL6_AST_UNION) &&
                !xl6SemClassHasCallableMember(ctx, receiverDecl, left->text) ) {
        /*
            成员表达式检查已经报告 unknown class/union field。
            这里避免再追加一条泛化的赋值目标错误。
        */
    } else if ( !xl6TextEquals(left->text, "length") &&
                xl6SemMemberReceiverIsXValueContainer(ctx, receiverType) &&
                !xl6SemSafeMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        /*
            成员表达式检查已经报告 member key receiver 的具体限制。
        */
    } else if ( typeName != NULL ) {
        xl6SemErrorNode(ctx, left, "member assignment receiver must be class/union field, var or dynamic dict");
        ok = 0;
    }

    xl6AstFree(receiverType);
    return ok;
}
static int xl6SemCheckAssign(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* left;
    XL6_AstNode* right;
    XL6_Symbol* sym;
    const char* op;
    int ok;
    int propertyAssignChecked;

    ok = 1;
    propertyAssignChecked = -1;
    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "=";

    if ( left == NULL ||
         (left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER && left->kind != XL6_AST_INDEX) ) {
        xl6SemErrorNode(ctx, left != NULL ? left : node, "当前赋值左侧必须是变量名");
        ok = 0;
    } else if ( left->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, left->text);
        if ( sym == NULL ) {
        xl6SemErrorNode(ctx, left, "赋值目标未声明");
            ok = 0;
        } else if ( xl6SemIsImplicitSelfTarget(ctx, left) ) {
            xl6SemErrorNode(ctx, left, "self 不能被重新赋值");
            ok = 0;
        } else if ( xl6SemSymbolIsConst(sym) ) {
            xl6SemErrorNode(ctx, left, "const 不能被重新赋值");
            ok = 0;
        } else if ( sym->kind == XL6_SYM_FUNC ) {
        xl6SemErrorNode(ctx, left, "函数名不能作为赋值目标");
            ok = 0;
        } else if ( !xl6SemCheckBoundarySymbolUse(ctx, left, sym, 1) ) {
            ok = 0;
        } else if ( right != NULL &&
                    (xl6TextEquals(op, "=") || xl6TextEquals(op, "?=")) &&
                    !xl6SemExprCanMatchParamType(ctx, right, sym->type) ) {
            xl6SemTypeMismatchError(
                ctx,
                right,
                "赋值表达式类型和目标变量类型不匹配",
                sym->type,
                xl6SemExprStaticTypeName(ctx, right));
            ok = 0;
        }
    } else if ( (left->kind == XL6_AST_MEMBER || left->kind == XL6_AST_INDEX) &&
                (propertyAssignChecked = xl6SemCheckPropertyAssignTarget(ctx, left, right, op)) >= 0 ) {
        if ( !propertyAssignChecked ) {
            ok = 0;
        }
    } else if ( !xl6SemCheckExpr(ctx, left) ) {
        ok = 0;
    }
    if ( left != NULL && left->kind == XL6_AST_MEMBER && propertyAssignChecked < 0 &&
         !xl6SemCheckDynamicMemberAssignTarget(ctx, node, left, op) ) {
        ok = 0;
    }

    if ( right != NULL && !xl6SemCheckExpr(ctx, right) ) {
        ok = 0;
    }
    if ( right != NULL && left != NULL && (left->kind == XL6_AST_MEMBER || left->kind == XL6_AST_INDEX) &&
         propertyAssignChecked < 0 &&
         (xl6TextEquals(op, "=") || xl6TextEquals(op, "?=")) ) {
        XL6_AstNode* targetType = left->kind == XL6_AST_MEMBER ?
            xl6SemCloneMemberFieldType(ctx, left) :
            xl6SemCloneIndexedItemType(ctx, left);
        if ( targetType != NULL && !xl6SemExprCanMatchParamType(ctx, right, targetType) ) {
            xl6SemTypeMismatchError(
                ctx,
                right,
                "assignment expression type does not match member/index target type",
                targetType,
                xl6SemExprStaticTypeName(ctx, right));
            ok = 0;
        }
        xl6AstFree(targetType);
    }
    if ( right != NULL &&
         !xl6SemCheckCopyableExpr(ctx, right, 1, "不可复制类型不能通过赋值复制") ) {
        ok = 0;
    }
    if ( right != NULL && xl6SemExprIsMultiReturnCall(ctx, right) ) {
        xl6SemErrorNode(ctx, right, "多返回值函数调用必须使用多值接收");
        ok = 0;
    }
    if ( !xl6SemCheckCompoundAssignTypes(ctx, node, left, right, op) ) {
        ok = 0;
    }
    return ok;
}

static void xl6SemTypeListAppend(char* buffer, size_t capacity, const char* text)
{
    size_t length;

    if ( buffer == NULL || capacity == 0 || text == NULL ) {
        return;
    }
    length = strlen(buffer);
    if ( length + 1 < capacity ) {
        snprintf(buffer + length, capacity - length, "%s", text);
    }
}

static void xl6SemMultiAssignTypeLists(
    XL6_SemContext* ctx,
    const XL6_AstNode* targets,
    const XL6_AstNode* returns,
    char* targetText,
    size_t targetCapacity,
    char* returnText,
    size_t returnCapacity)
{
    size_t i;

    if ( targetText != NULL && targetCapacity > 0 ) {
        targetText[0] = '\0';
    }
    if ( returnText != NULL && returnCapacity > 0 ) {
        returnText[0] = '\0';
    }
    for ( i = 0; targets != NULL && i < targets->childCount; ++i ) {
        const XL6_AstNode* target = targets->children[i];
        const XL6_AstNode* targetType = NULL;
        XL6_AstNode* ownedTargetType = NULL;
        const char* displayName = NULL;
        char name[256];

        name[0] = '\0';

        if ( i > 0 ) {
            xl6SemTypeListAppend(targetText, targetCapacity, ", ");
        }
        if ( target != NULL && target->kind == XL6_AST_NAME && xl6TextEquals(target->text, "_") ) {
            displayName = "_";
        } else if ( target != NULL && target->kind == XL6_AST_NAME ) {
            XL6_Symbol* symbol = xl6ScopeFind(ctx->scope, target->text);
            targetType = symbol != NULL ? symbol->type : NULL;
        } else if ( target != NULL && target->kind == XL6_AST_MEMBER ) {
            ownedTargetType = xl6SemCloneMemberFieldType(ctx, (XL6_AstNode*)target);
            targetType = ownedTargetType;
        } else if ( target != NULL && target->kind == XL6_AST_INDEX ) {
            ownedTargetType = xl6SemCloneIndexedItemType(ctx, (XL6_AstNode*)target);
            targetType = ownedTargetType;
        }
        if ( displayName == NULL ) {
            xl6SemFormatTypeName(ctx, targetType, name, sizeof(name));
            displayName = name[0] != '\0' ? name : xl6SemDisplayTypeName(ctx, targetType);
        }
        xl6SemTypeListAppend(targetText, targetCapacity, displayName != NULL ? displayName : "<unknown>");
        xl6AstFree(ownedTargetType);
    }
    for ( i = 0; returns != NULL && i < returns->childCount; ++i ) {
        const char* displayName;
        char name[256];

        xl6SemFormatTypeName(ctx, returns->children[i], name, sizeof(name));
        displayName = name[0] != '\0' ? name : xl6SemDisplayTypeName(ctx, returns->children[i]);
        if ( i > 0 ) {
            xl6SemTypeListAppend(returnText, returnCapacity, ", ");
        }
        xl6SemTypeListAppend(returnText, returnCapacity, displayName != NULL ? displayName : "<unknown>");
    }
}

static void xl6SemMultiAssignTypeMismatchError(
    XL6_SemContext* ctx,
    XL6_AstNode* target,
    size_t index,
    const XL6_AstNode* targetType,
    const XL6_AstNode* valueType,
    const XL6_AstNode* targets,
    const XL6_AstNode* returns)
{
    char targetList[512];
    char returnList[512];
    char message[1280];
    const char* targetDisplayName;
    const char* valueDisplayName;
    char targetName[256];
    char valueName[256];

    xl6SemMultiAssignTypeLists(
        ctx,
        targets,
        returns,
        targetList,
        sizeof(targetList),
        returnList,
        sizeof(returnList));
    xl6SemFormatTypeName(ctx, targetType, targetName, sizeof(targetName));
    xl6SemFormatTypeName(ctx, valueType, valueName, sizeof(valueName));
    targetDisplayName = targetName[0] != '\0' ? targetName : xl6SemDisplayTypeName(ctx, targetType);
    valueDisplayName = valueName[0] != '\0' ? valueName : xl6SemDisplayTypeName(ctx, valueType);
    snprintf(
        message,
        sizeof(message),
        "multi-return assignment item %lu type mismatch: target %s, return %s; targets (%s), returns (%s)",
        (unsigned long)(index + 1),
        targetDisplayName != NULL ? targetDisplayName : "<unknown>",
        valueDisplayName != NULL ? valueDisplayName : "<unknown>",
        targetList,
        returnList);
    xl6SemErrorNode(ctx, target, message);
}

static int xl6SemCheckMultiAssign(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* targets;
    XL6_AstNode* expr;
    XL6_AstNode* returns;
    XL6_AstNode* ownedReturnType;
    size_t i;
    int ok;

    ok = 1;
    targets = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;

    if ( expr == NULL || !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    ownedReturnType = NULL;
    returns = xl6SemCallReturnTypesEx(ctx, expr, &ownedReturnType);
    if ( returns == NULL || returns->childCount <= 1 ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : node, "多返回值赋值右侧必须是多返回值函数调用");
        ok = 0;
    } else if ( targets == NULL || targets->childCount != returns->childCount ) {
        xl6SemCountMismatchError(
            ctx,
            node,
            "多返回值赋值数量和函数返回值数量不匹配",
            returns->childCount,
            targets != NULL ? targets->childCount : 0);
        ok = 0;
    }

    if ( targets != NULL ) {
        for ( i = 0; i < targets->childCount; ++i ) {
            XL6_AstNode* target = targets->children[i];
            XL6_AstNode* valueType = (returns != NULL && i < returns->childCount) ? returns->children[i] : NULL;

            if ( target == NULL ) {
                ok = 0;
                continue;
            }
            if ( target->kind == XL6_AST_NAME && xl6TextEquals(target->text, "_") ) {
                continue;
            }
            if ( target->kind == XL6_AST_NAME ) {
                XL6_Symbol* sym = xl6ScopeFind(ctx->scope, target->text);
                if ( sym == NULL ) {
            xl6SemErrorNode(ctx, target, "多返回值赋值目标未声明");
                    ok = 0;
                } else if ( xl6SemIsImplicitSelfTarget(ctx, target) ) {
                    xl6SemErrorNode(ctx, target, "self 不能被重新赋值");
                    ok = 0;
                } else if ( xl6SemSymbolIsConst(sym) ) {
                    xl6SemErrorNode(ctx, target, "const 不能被重新赋值");
                    ok = 0;
                } else if ( sym->kind == XL6_SYM_FUNC ) {
            xl6SemErrorNode(ctx, target, "函数名不能作为赋值目标");
                    ok = 0;
                } else if ( !xl6SemCheckBoundarySymbolUse(ctx, target, sym, 1) ) {
                    ok = 0;
                } else if ( !xl6SemTypeCanAssignFromType(ctx, valueType, sym->type) ) {
                    xl6SemMultiAssignTypeMismatchError(
                        ctx,
                        target,
                        i,
                        sym->type,
                        valueType,
                        targets,
                        returns);
                    ok = 0;
                }
            } else if ( target->kind == XL6_AST_MEMBER || target->kind == XL6_AST_INDEX ) {
                XL6_AstNode* targetType = target->kind == XL6_AST_MEMBER ?
                    xl6SemCloneMemberFieldType(ctx, target) :
                    xl6SemCloneIndexedItemType(ctx, target);
                if ( !xl6SemCheckExpr(ctx, target) ) {
                    ok = 0;
                }
                if ( targetType != NULL && !xl6SemTypeCanAssignFromType(ctx, valueType, targetType) ) {
                    xl6SemMultiAssignTypeMismatchError(
                        ctx,
                        target,
                        i,
                        targetType,
                        valueType,
                        targets,
                        returns);
                    ok = 0;
                }
                xl6AstFree(targetType);
            } else {
                xl6SemErrorNode(ctx, target, "多返回值赋值目标必须是变量、字段或索引");
                ok = 0;
            }
        }
    }
    xl6AstFree(ownedReturnType);
    return ok;
}

static int xl6SemCheckBlock(XL6_SemContext* ctx, XL6_AstNode* block, int pushScope)
{
    XL6_Scope scope;
    size_t i;
    int ok;

    if ( block == NULL ) {
        return 1;
    }

    if ( pushScope ) {
        xl6SemPushScope(ctx, &scope);
    }

    ok = 1;
    for ( i = 0; i < block->childCount; ++i ) {
        if ( !xl6SemCheckStmt(ctx, block->children[i]) ) {
            ok = 0;
        }
    }

    if ( pushScope ) {
        xl6SemPopScope(ctx, &scope);
    }
    return ok;
}

static int xl6SemCheckIf(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;
    int ok;
    XL6_AstNode* elseifList;

    ok = 1;
    if ( node->childCount > 0 && !xl6SemCheckExpr(ctx, node->children[0]) ) {
        ok = 0;
    }
    if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "if 条件必须是 bool") ) {
        ok = 0;
    }
    if ( node->childCount > 1 && !xl6SemCheckBlock(ctx, node->children[1], 1) ) {
        ok = 0;
    }
    if ( node->childCount > 2 ) {
        elseifList = node->children[2];
        for ( i = 0; i < elseifList->childCount; ++i ) {
            XL6_AstNode* item = elseifList->children[i];
            if ( item->childCount > 0 && !xl6SemCheckExpr(ctx, item->children[0]) ) {
                ok = 0;
            }
            if ( item->childCount > 0 && !xl6SemCheckBoolExpr(ctx, item->children[0], "elseif 条件必须是 bool") ) {
                ok = 0;
            }
            if ( item->childCount > 1 && !xl6SemCheckBlock(ctx, item->children[1], 1) ) {
                ok = 0;
            }
        }
    }
    if ( node->childCount > 3 && !xl6SemCheckBlock(ctx, node->children[3], 1) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckLoop(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    ok = 1;
    if ( node->childCount > 0 && !xl6SemCheckExpr(ctx, node->children[0]) ) {
        ok = 0;
    }
    if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "while 条件必须是 bool") ) {
        ok = 0;
    }
    if ( node->childCount > 1 ) {
        ctx->loopDepth += 1;
        if ( !xl6SemCheckBlock(ctx, node->children[1], 1) ) {
            ok = 0;
        }
        ctx->loopDepth -= 1;
    }
    return ok;
}

static int xl6SemCheckDoWhile(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    ok = 1;
    if ( node->childCount > 0 ) {
        ctx->loopDepth += 1;
        if ( !xl6SemCheckBlock(ctx, node->children[0], 1) ) {
            ok = 0;
        }
        ctx->loopDepth -= 1;
    }
    if ( node->childCount > 1 && !xl6SemCheckExpr(ctx, node->children[1]) ) {
        ok = 0;
    }
    if ( node->childCount > 1 && !xl6SemCheckBoolExpr(ctx, node->children[1], "do while 条件必须是 bool") ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemIsErrorKindName(const char* name)
{
    static const char* const kinds[] = {
        "Error",
        "RuntimeError", "AssertionError", "NullReferenceError", "NotImplementedError", "NativeError",
        "TypeError",
        "ValueError", "RangeError",
        "LookupError", "IndexError", "KeyError", "AttributeError", "NameError",
        "IOError", "FileError", "EncodingError", "NetworkError", "TimeoutError",
        "ImportError", "ModuleNotFoundError",
        "AsyncError", "FutureError", "TaskError", "CancelledError", "GeneratorError",
        "ParseError", "SyntaxError",
        "BufferError",
        "CompressionError", "ArchiveError",
        "CsvError", "ConfigError", "TomlError", "PlistError", "XmlError"
    };
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(kinds); ++i ) {
        if ( xl6TextEquals(name, kinds[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckThrowStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* kindNode;
    XL6_AstNode* expr;
    const char* typeName;
    int ok;

    ok = 1;
    kindNode = node != NULL && node->childCount == 2 ? node->children[0] : NULL;
    expr = node != NULL && node->childCount > 0 ? node->children[node->childCount == 2 ? 1 : 0] : NULL;
    if ( expr == NULL ) {
        if ( ctx->catchDepth <= 0 ) {
            xl6SemErrorNode(ctx, node, "throw 只能在 catch 块内重新抛出当前异常");
            ok = 0;
        }
        return ok;
    }

    if ( kindNode != NULL &&
         (kindNode->kind != XL6_AST_NAME || !xl6SemIsErrorKindName(kindNode->text)) ) {
        xl6SemErrorNode(ctx, kindNode, "未知异常类型");
        ok = 0;
    }

    if ( !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    if ( xl6SemExprIsMultiReturnCall(ctx, expr) ) {
        xl6SemErrorNode(ctx, expr, "throw 表达式不能是多返回值函数调用");
        ok = 0;
    }
    typeName = xl6SemExprDeclaredTypeName(ctx, expr);
    if ( typeName != NULL && !xl6TextEquals(typeName, "string") ) {
        xl6SemErrorNode(ctx, expr, "throw 表达式必须是 string");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckAssertStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* cond;
    XL6_AstNode* message;
    const char* messageType;
    int ok;

    ok = 1;
    cond = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    message = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( cond == NULL ) {
        xl6SemErrorNode(ctx, node, "assert 缺少条件表达式");
        ok = 0;
    } else {
        if ( !xl6SemCheckExpr(ctx, cond) ) {
            ok = 0;
        }
        if ( !xl6SemCheckBoolExpr(ctx, cond, "assert 条件必须是 bool") ) {
            ok = 0;
        }
    }

    if ( message != NULL ) {
        if ( !xl6SemCheckExpr(ctx, message) ) {
            ok = 0;
        }
        if ( xl6SemExprIsMultiReturnCall(ctx, message) ) {
            xl6SemErrorNode(ctx, message, "assert 消息不能是多返回值函数调用");
            ok = 0;
        }
        messageType = xl6SemExprDeclaredTypeName(ctx, message);
        if ( messageType != NULL && !xl6TextEquals(messageType, "string") ) {
            xl6SemErrorNode(ctx, message, "assert 消息必须是 string");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemNodeContainsAbruptControl(XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_RETURN ||
         node->kind == XL6_AST_BREAK ||
         node->kind == XL6_AST_CONTINUE ) {
        return 1;
    }
    if ( node->kind == XL6_AST_FUNC ||
         node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_ANON_FUNC ||
         node->kind == XL6_AST_CLASS ) {
        return 0;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6SemNodeContainsAbruptControl(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckTryStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* tryBlock;
    XL6_AstNode* catchList;
    XL6_AstNode* finallyBlock;
    size_t i;
    int ok;
    int seenCatchAll;

    if ( node == NULL ) {
        return 1;
    }

    ok = 1;
    tryBlock = node->childCount > 0 ? node->children[0] : NULL;
    catchList = node->childCount > 1 ? node->children[1] : NULL;
    finallyBlock = node->childCount > 2 ? node->children[2] : NULL;

    if ( catchList == NULL || catchList->kind != XL6_AST_LIST ) {
        catchList = NULL;
    }
    if ( (catchList == NULL || catchList->childCount == 0) &&
         (finallyBlock == NULL || finallyBlock->childCount == 0) ) {
        xl6SemErrorNode(ctx, node, "try 必须包含 catch 或 finally");
        ok = 0;
    }
    if ( tryBlock != NULL && !xl6SemCheckBlock(ctx, tryBlock, 1) ) {
        ok = 0;
    }

    seenCatchAll = 0;
    if ( catchList != NULL ) {
        for ( i = 0; i < catchList->childCount; ++i ) {
        XL6_AstNode* catchNode = catchList->children[i];
        XL6_AstNode* catchKind = NULL;
        XL6_AstNode* catchName = NULL;
        XL6_AstNode* catchBlock = NULL;
        XL6_AstNode* errorType;
        XL6_Scope catchScope;

        if ( catchNode == NULL || catchNode->kind != XL6_AST_CATCH ) {
            continue;
        }
        if ( catchNode->childCount == 3 ) {
            catchKind = catchNode->children[0];
            catchName = catchNode->children[1];
            catchBlock = catchNode->children[2];
        } else {
            catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
            catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
        }
        if ( seenCatchAll ) {
            xl6SemErrorNode(ctx, catchNode, "catch e 必须位于所有分类 catch 之后");
            ok = 0;
        }
        if ( catchKind == NULL ) {
            seenCatchAll = 1;
        } else if ( catchKind->kind != XL6_AST_NAME || !xl6SemIsErrorKindName(catchKind->text) ) {
            xl6SemErrorNode(ctx, catchKind, "未知异常类型");
            ok = 0;
        }
        xl6SemPushScope(ctx, &catchScope);
        errorType = xl6SemOwnedSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
        if ( catchName == NULL || catchName->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, catchNode, "catch 缺少异常变量名");
            ok = 0;
        } else if ( errorType == NULL || !xl6ScopeAddTyped(ctx, catchName, XL6_SYM_VALUE, errorType) ) {
            ok = 0;
        }
        ctx->catchDepth += 1;
        if ( catchBlock != NULL && !xl6SemCheckBlock(ctx, catchBlock, 0) ) {
            ok = 0;
        }
        ctx->catchDepth -= 1;
        xl6SemPopScope(ctx, &catchScope);
        }
    }

    if ( finallyBlock != NULL && finallyBlock->childCount > 0 &&
         !xl6SemCheckBlock(ctx, finallyBlock, 1) ) {
        ok = 0;
    }
    return ok;
}

static XL6_AstNode* xl6SemOwnedSimpleType(XL6_SemContext* ctx, const char* typeName, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;

    if ( typeName == NULL ) {
        return NULL;
    }
    typeNode = xl6AstNewText(XL6_AST_TYPE, typeName, strlen(typeName), NULL);
    if ( typeNode == NULL ) {
        xl6SemErrorNode(ctx, at, "类型节点内存不足");
        return NULL;
    }
    if ( at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    if ( !xl6SemOwnTypeNode(ctx, typeNode) ) {
        xl6AstFree(typeNode);
            xl6SemErrorNode(ctx, at, "类型节点表内存不足");
        return NULL;
    }
    return typeNode;
}

static int xl6SemIsRangeCallNode(XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CALL &&
           node->childCount > 0 &&
           node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6TextEquals(node->children[0]->text, "range");
}

static XL6_AstNode* xl6SemCloneClassIterableItemType(XL6_SemContext* ctx, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* iterableClass;
    const XL6_AstNode* iterMethod;
    const XL6_AstNode* iterReturns;
    const XL6_AstNode* iteratorType;
    const XL6_AstNode* iteratorClass;
    const XL6_AstNode* nextMethod;
    const XL6_AstNode* nextReturns;
    const XL6_AstNode* iterableTypeArgs;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* ownedIteratorType;
    XL6_AstNode* result;

    concreteType = xl6SemResolveAliasType(ctx, iterableType);
    concreteType = xl6SemClassValueType(ctx, concreteType);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return NULL;
    }

    iterableClass = xl6SemFindTypeDecl(ctx, concreteType->text);
    if ( iterableClass == NULL || iterableClass->kind != XL6_AST_CLASS ) {
        return NULL;
    }
    iterableTypeArgs = xl6SemTypeNodeGenericArgs(concreteType);
    if ( xl6SemClassHasTypeParams(iterableClass) && iterableTypeArgs == NULL ) {
        return NULL;
    }

    iterMethod = xl6SemFindClassCallableMemberNode(ctx, iterableClass, "__iter__");
    iterReturns = iterMethod != NULL && iterMethod->childCount > 2 ? iterMethod->children[2] : NULL;
    if ( iterReturns == NULL || iterReturns->childCount != 1 ) {
        return NULL;
    }

    ownedIteratorType = NULL;
    iteratorType = iterReturns->children[0];
    if ( iterableTypeArgs != NULL && xl6SemClassHasTypeParams(iterableClass) ) {
        ownedIteratorType = xl6SemSubstituteClassType(iterableClass, iterableTypeArgs, iteratorType);
        iteratorType = ownedIteratorType;
    }
    iteratorType = xl6SemResolveAliasType(ctx, iteratorType);
    iteratorType = xl6SemClassValueType(ctx, iteratorType);
    if ( iteratorType == NULL || iteratorType->text == NULL ) {
        xl6AstFree(ownedIteratorType);
        return NULL;
    }
    iteratorClass = xl6SemFindTypeDecl(ctx, iteratorType->text);
    if ( iteratorClass == NULL || iteratorClass->kind != XL6_AST_CLASS ) {
        xl6AstFree(ownedIteratorType);
        return NULL;
    }
    typeArgs = xl6SemTypeNodeGenericArgs(iteratorType);
    if ( xl6SemClassHasTypeParams(iteratorClass) && typeArgs == NULL ) {
        xl6AstFree(ownedIteratorType);
        return NULL;
    }

    nextMethod = xl6SemFindClassCallableMemberNode(ctx, iteratorClass, "__next__");
    nextReturns = nextMethod != NULL && nextMethod->childCount > 2 ? nextMethod->children[2] : NULL;
    if ( nextReturns == NULL || nextReturns->childCount != 2 ||
         !xl6SemTypeNameMatches(ctx, nextReturns->children[0], "bool") ) {
        xl6AstFree(ownedIteratorType);
        return NULL;
    }

    if ( typeArgs != NULL && xl6SemClassHasTypeParams(iteratorClass) ) {
        result = xl6SemSubstituteClassType(iteratorClass, typeArgs, nextReturns->children[1]);
        xl6AstFree(ownedIteratorType);
        return result;
    }
    result = xl6SemCloneTypeNode(nextReturns->children[1]);
    xl6AstFree(ownedIteratorType);
    return result;
}

static const char* xl6SemForIterableTypeName(XL6_SemContext* ctx, XL6_AstNode* iterable)
{
    XL6_Symbol* sym;

    if ( iterable == NULL ) {
        return NULL;
    }
    if ( xl6SemIsRangeCallNode(iterable) ) {
        return "range";
    }
    if ( iterable->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, iterable->text);
        if ( sym != NULL && sym->type != NULL && sym->type->text != NULL && !xl6TextEquals(sym->type->text, "auto") ) {
            return sym->type->text;
        }
    }
    return xl6SemExprStaticTypeName(ctx, iterable);
}

static XL6_AstNode* xl6SemNewForTargetSimpleType(const char* typeName, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;

    if ( typeName == NULL ) {
        return NULL;
    }
    typeNode = xl6AstNewText(XL6_AST_TYPE, typeName, strlen(typeName), NULL);
    if ( typeNode != NULL && at != NULL ) {
        typeNode->line = at->line;
        typeNode->column = at->column;
    }
    return typeNode;
}

static XL6_AstNode* xl6SemCloneForTargetType(XL6_SemContext* ctx, XL6_AstNode* iterable, size_t targetIndex, size_t targetCount)
{
    XL6_AstNode* iterableType;
    XL6_AstNode* itemType;
    const XL6_AstNode* concreteType;
    const char* typeName;

    if ( xl6SemIsRangeCallNode(iterable) ) {
        return xl6SemNewForTargetSimpleType("int", iterable);
    }

    iterableType = xl6SemInferAutoTypeNode(ctx, iterable);
    concreteType = xl6SemResolveAliasType(ctx, iterableType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( typeName == NULL ) {
        xl6AstFree(iterableType);
        return NULL;
    }

    if ( xl6TextEquals(typeName, "generator") ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            xl6AstFree(iterableType);
            return xl6SemNewForTargetSimpleType("int", iterable);
        }
        itemType = xl6SemCloneSingleGenericArgType(ctx, concreteType, "generator");
        xl6AstFree(iterableType);
        return itemType;
    }

    if ( xl6TextEquals(typeName, "var") ) {
        xl6AstFree(iterableType);
        return xl6SemNewForTargetSimpleType("var", iterable);
    }

    if ( xl6TextEquals(typeName, "string") ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            xl6AstFree(iterableType);
            return xl6SemNewForTargetSimpleType("int", iterable);
        }
        xl6AstFree(iterableType);
        return xl6SemNewForTargetSimpleType("string", iterable);
    }

    if ( xl6SemIsDictTypeName(typeName) ) {
        if ( targetCount == 1 || targetIndex == 0 ) {
            xl6AstFree(iterableType);
            return xl6SemNewForTargetSimpleType("string", iterable);
        }
        itemType = xl6SemCloneContainerItemType(ctx, concreteType);
        xl6AstFree(iterableType);
        return itemType != NULL ? itemType : xl6SemNewForTargetSimpleType("var", iterable);
    }

    if ( xl6SemIsArrayTypeName(typeName) || xl6SemIsListTypeName(typeName) ||
         xl6TextEquals(typeName, "set") || xl6TextEquals(typeName, "set<var>") ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            xl6AstFree(iterableType);
            return xl6SemNewForTargetSimpleType("int", iterable);
        }
        itemType = xl6SemCloneContainerItemType(ctx, concreteType);
        xl6AstFree(iterableType);
        return itemType != NULL ? itemType : xl6SemNewForTargetSimpleType("var", iterable);
    }

    itemType = xl6SemCloneClassIterableItemType(ctx, concreteType);
    if ( itemType != NULL ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            xl6AstFree(itemType);
            xl6AstFree(iterableType);
            return xl6SemNewForTargetSimpleType("int", iterable);
        }
        xl6AstFree(iterableType);
        return itemType;
    }

    xl6AstFree(iterableType);
    return NULL;
}

static int xl6SemDeclareForTargetType(XL6_SemContext* ctx, XL6_AstNode* target, XL6_AstNode* typeNode)
{
    if ( target == NULL || typeNode == NULL ) {
        xl6AstFree(typeNode);
        return 0;
    }
    typeNode->line = target->line;
    typeNode->column = target->column;
    if ( !xl6SemOwnTypeNode(ctx, typeNode) ) {
        xl6AstFree(typeNode);
        return 0;
    }
    /* `_` 是丢弃绑定：保留推断出的类型供后续检查，但不进入作用域。 */
    if ( xl6TextEquals(target->text, "_") ) {
        return 1;
    }
    return xl6ScopeAddTyped(ctx, target, XL6_SYM_VALUE, typeNode);
}
static int xl6SemCheckFor(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Scope scope;
    XL6_AstNode* targets;
    XL6_AstNode* iterable;
    size_t i;
    int ok;

    ok = 1;
    xl6SemPushScope(ctx, &scope);

    iterable = node->childCount > 1 ? node->children[1] : NULL;
    if ( node->childCount > 1 && !xl6SemCheckExpr(ctx, iterable) ) {
        ok = 0;
    }

    if ( (node->flags & XL6_AST_FLAG_ASYNC) != 0 ) {
        XL6_AstNode* iterableType = xl6SemInferAutoTypeNode(ctx, iterable);
        const XL6_AstNode* concreteType = xl6SemResolveAliasType(ctx, iterableType);

        if ( concreteType == NULL || !xl6SemIsGeneratorTypeName(concreteType->text) ) {
            xl6SemErrorNode(ctx, iterable, "for await requires generator<T>");
            ok = 0;
        }
        xl6AstFree(iterableType);
    }

    if ( node->childCount > 0 && node->children[0] != NULL ) {
        targets = node->children[0];
        if ( targets->childCount == 0 || targets->childCount > 2 ) {
            xl6SemErrorNode(ctx, targets, "for in 只支持一个或两个迭代变量");
            ok = 0;
        } else {
            for ( i = 0; i < targets->childCount; ++i ) {
                XL6_AstNode* targetType = xl6SemCloneForTargetType(ctx, iterable, i, targets->childCount);
                if ( targetType == NULL ) {
                    xl6SemErrorNode(ctx, iterable, "for in iterable type is not supported");
                    ok = 0;
                    break;
                }
                if ( !xl6SemDeclareForTargetType(ctx, targets->children[i], targetType) ) {
                    ok = 0;
                }
            }
        }
    }
    if ( node->childCount > 2 ) {
        ctx->loopDepth += 1;
        if ( !xl6SemCheckBlock(ctx, node->children[2], 0) ) {
            ok = 0;
        }
        ctx->loopDepth -= 1;
    }

    xl6SemPopScope(ctx, &scope);
    return ok;
}

/*
    out 参数是只写别名：读取前必须已经整体赋值，所有正常出口必须赋值。
    分析按单个参数进行，状态只有一个布尔值，避免引入庞大的通用 CFG。
*/
static int xl6SemOutFlowExpr(
    XL6_SemContext* ctx,
    const XL6_AstNode* node,
    const char* name,
    int* assigned)
{
    size_t i;
    int ok = 1;

    if ( node == NULL || name == NULL || assigned == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 1;
    }
    if ( node->kind == XL6_AST_NAME && xl6TextEquals(node->text, name) ) {
        if ( !*assigned ) {
            xl6SemErrorNode(ctx, (XL6_AstNode*)node, "out 参数在整体赋值前不能读取");
            return 0;
        }
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 1 ) {
        if ( !xl6SemOutFlowExpr(ctx, node->children[0], name, assigned) ) {
            ok = 0;
        }
        for ( i = 0; node->children[1] != NULL && i < node->children[1]->childCount; ++i ) {
            const XL6_AstNode* arg = node->children[1]->children[i];
            const XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
            if ( arg != NULL && (arg->flags & XL6_AST_FLAG_OUT) != 0 &&
                 expr != NULL && expr->kind == XL6_AST_NAME && xl6TextEquals(expr->text, name) ) {
                *assigned = 1;
            } else if ( !xl6SemOutFlowExpr(ctx, expr, name, assigned) ) {
                ok = 0;
            }
        }
        return ok;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6SemOutFlowExpr(ctx, node->children[i], name, assigned) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemOutFlowBlock(
    XL6_SemContext* ctx,
    const XL6_AstNode* block,
    const char* name,
    int* assigned,
    int* fallsThrough);

static int xl6SemOutFlowStmt(
    XL6_SemContext* ctx,
    const XL6_AstNode* node,
    const char* name,
    int* assigned,
    int* fallsThrough)
{
    size_t i;
    int ok = 1;

    if ( node == NULL || assigned == NULL || fallsThrough == NULL || !*fallsThrough ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && node->childCount >= 2 ) {
        const XL6_AstNode* left = node->children[0];
        int wholeTarget = left != NULL && left->kind == XL6_AST_NAME && xl6TextEquals(left->text, name);
        if ( !xl6SemOutFlowExpr(ctx, node->children[1], name, assigned) ) {
            ok = 0;
        }
        if ( wholeTarget ) {
            if ( !xl6TextEquals(node->text, "=") && !*assigned ) {
                xl6SemErrorNode(ctx, (XL6_AstNode*)left, "out 参数在复合赋值前必须已经整体赋值");
                ok = 0;
            }
            *assigned = 1;
        } else if ( !xl6SemOutFlowExpr(ctx, left, name, assigned) ) {
            ok = 0;
        }
        return ok;
    }
    if ( node->kind == XL6_AST_MULTI_ASSIGN && node->childCount >= 2 ) {
        if ( !xl6SemOutFlowExpr(ctx, node->children[1], name, assigned) ) {
            ok = 0;
        }
        for ( i = 0; node->children[0] != NULL && i < node->children[0]->childCount; ++i ) {
            const XL6_AstNode* target = node->children[0]->children[i];
            if ( target != NULL && target->kind == XL6_AST_NAME && xl6TextEquals(target->text, name) ) {
                *assigned = 1;
            } else if ( !xl6SemOutFlowExpr(ctx, target, name, assigned) ) {
                ok = 0;
            }
        }
        return ok;
    }
    if ( node->kind == XL6_AST_RETURN ) {
        for ( i = 0; i < node->childCount; ++i ) {
            if ( !xl6SemOutFlowExpr(ctx, node->children[i], name, assigned) ) {
                ok = 0;
            }
        }
        if ( !*assigned ) {
            xl6SemErrorNode(ctx, (XL6_AstNode*)node, "out 参数必须在 return 前整体赋值");
            ok = 0;
        }
        *fallsThrough = 0;
        return ok;
    }
    if ( node->kind == XL6_AST_THROW ) {
        for ( i = 0; i < node->childCount; ++i ) {
            if ( !xl6SemOutFlowExpr(ctx, node->children[i], name, assigned) ) {
                ok = 0;
            }
        }
        *fallsThrough = 0;
        return ok;
    }
    if ( node->kind == XL6_AST_IF && node->childCount >= 4 ) {
        int entry;
        int merged = 1;
        int hasFallthrough = 0;
        int branchAssigned;
        int branchFalls;
        const XL6_AstNode* elseifs = node->children[2];

        if ( !xl6SemOutFlowExpr(ctx, node->children[0], name, assigned) ) {
            ok = 0;
        }
        entry = *assigned;

        branchAssigned = entry;
        branchFalls = 1;
        if ( !xl6SemOutFlowBlock(ctx, node->children[1], name, &branchAssigned, &branchFalls) ) {
            ok = 0;
        }
        if ( branchFalls ) {
            merged = branchAssigned;
            hasFallthrough = 1;
        }
        for ( i = 0; elseifs != NULL && i < elseifs->childCount; ++i ) {
            const XL6_AstNode* branch = elseifs->children[i];
            branchAssigned = entry;
            branchFalls = 1;
            if ( branch != NULL && branch->childCount > 0 &&
                 !xl6SemOutFlowExpr(ctx, branch->children[0], name, &branchAssigned) ) {
                ok = 0;
            }
            if ( branch != NULL && branch->childCount > 1 &&
                 !xl6SemOutFlowBlock(ctx, branch->children[1], name, &branchAssigned, &branchFalls) ) {
                ok = 0;
            }
            if ( branchFalls ) {
                merged = hasFallthrough ? (merged && branchAssigned) : branchAssigned;
                hasFallthrough = 1;
            }
        }
        branchAssigned = entry;
        branchFalls = 1;
        if ( !xl6SemOutFlowBlock(ctx, node->children[3], name, &branchAssigned, &branchFalls) ) {
            ok = 0;
        }
        if ( branchFalls ) {
            merged = hasFallthrough ? (merged && branchAssigned) : branchAssigned;
            hasFallthrough = 1;
        }
        *assigned = merged;
        *fallsThrough = hasFallthrough;
        return ok;
    }
    if ( node->kind == XL6_AST_WHILE || node->kind == XL6_AST_FOR || node->kind == XL6_AST_DO_WHILE ) {
        int loopAssigned = *assigned;
        int loopFalls = 1;
        for ( i = 0; i < node->childCount; ++i ) {
            const XL6_AstNode* child = node->children[i];
            if ( child != NULL && child->kind == XL6_AST_BLOCK ) {
                if ( !xl6SemOutFlowBlock(ctx, child, name, &loopAssigned, &loopFalls) ) {
                    ok = 0;
                }
            } else if ( !xl6SemOutFlowExpr(ctx, child, name, assigned) ) {
                ok = 0;
            }
        }
        return ok;
    }
    if ( node->kind == XL6_AST_BLOCK ) {
        return xl6SemOutFlowBlock(ctx, node, name, assigned, fallsThrough);
    }
    return xl6SemOutFlowExpr(ctx, node, name, assigned);
}

static int xl6SemOutFlowBlock(
    XL6_SemContext* ctx,
    const XL6_AstNode* block,
    const char* name,
    int* assigned,
    int* fallsThrough)
{
    size_t i;
    int ok = 1;

    for ( i = 0; block != NULL && *fallsThrough && i < block->childCount; ++i ) {
        if ( !xl6SemOutFlowStmt(ctx, block->children[i], name, assigned, fallsThrough) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckOutParams(XL6_SemContext* ctx, XL6_AstNode* func)
{
    XL6_AstNode* params = func != NULL && func->childCount > 1 ? func->children[1] : NULL;
    XL6_AstNode* body = func != NULL && func->childCount > 3 ? func->children[3] : NULL;
    size_t i;
    int ok = 1;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        int assigned = 0;
        int fallsThrough = 1;
        if ( xl6SemParamMode(param) != XL6_AST_FLAG_OUT ) {
            continue;
        }
        if ( !xl6SemOutFlowBlock(ctx, body, param->text, &assigned, &fallsThrough) ) {
            ok = 0;
        }
        if ( fallsThrough && !assigned ) {
            xl6SemErrorNode(ctx, param, "out 参数必须在函数正常结束前整体赋值");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Scope scope;
    XL6_TypeParamScope typeParamScope;
    XL6_AstNode* typeParams;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* oldReturns;
    XL6_AstNode* oldFunction;
    int oldReturnTypesValid;
    int returnTypesValid;
    size_t i;
    int ok;
    int pushedTypeParams;
    int oldGeneratorDepth;
    int isGeneratorFunc;
    const char* oldImplicitSelfName;
    XL6_Scope* oldCaptureBoundary;
    int oldCaptureBoundaryAllowsCapture;

    ok = 1;
    pushedTypeParams = 0;
    xl6SemPushScope(ctx, &scope);
    oldCaptureBoundary = ctx->captureBoundary;
    oldCaptureBoundaryAllowsCapture = ctx->captureBoundaryAllowsCapture;
    ctx->captureBoundary = &scope;
    ctx->captureBoundaryAllowsCapture = 0;
    if ( ctx->implicitSelfName != NULL ) {
        XL6_AstNode selfNode;
        XL6_AstNode* selfType;
        memset(&selfNode, 0, sizeof(selfNode));
        selfNode.kind = XL6_AST_NAME;
        selfNode.text = (char*)ctx->implicitSelfName;
        selfNode.line = node->line;
        selfNode.column = node->column;
        selfType = ctx->currentClass;
        if ( selfType != NULL ) {
            if ( !xl6ScopeAddTyped(ctx, &selfNode, XL6_SYM_VALUE, selfType) ) {
                ok = 0;
            }
        } else if ( !xl6ScopeAdd(ctx, &selfNode, XL6_SYM_VALUE) ) {
            ok = 0;
        }
    }

    typeParams = node->childCount > 4 ? node->children[4] : NULL;
    if ( typeParams != NULL && typeParams->childCount > 0 ) {
        if ( !xl6SemCheckTypeParams(ctx, typeParams) ) {
            ok = 0;
        }
        xl6SemPushTypeParams(ctx, &typeParamScope, typeParams);
        pushedTypeParams = 1;
    }

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        int seenOptional;
        int seenVararg;
        int seenBadVarargPosition;

        seenOptional = 0;
        seenVararg = 0;
        seenBadVarargPosition = 0;
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            int isVararg = xl6SemParamIsVararg(param);
            unsigned int paramMode = xl6SemParamMode(param);
            if ( xl6SemParamIsCVararg(param) ) {
                xl6SemErrorNode(ctx, param, "c_vararg 只能用于 extern C 函数声明");
                ok = 0;
            }
            if ( param->childCount > 0 ) {
                if ( xl6SemTypeIsAuto(param->children[0]) ) {
                    xl6SemErrorNode(ctx, param->children[0], "auto 不能作为函数参数类型");
                    ok = 0;
                } else {
                    if ( !xl6SemCheckType(ctx, param->children[0]) ) {
                        ok = 0;
                    }
                }
            }
            if ( isVararg && paramMode != 0 ) {
                xl6SemErrorNode(ctx, param, "可变参数不能声明 byval、byref 或 out");
                ok = 0;
            }
            if ( (paramMode == XL6_AST_FLAG_BYREF || paramMode == XL6_AST_FLAG_OUT) &&
                 param->childCount > 1 ) {
                xl6SemErrorNode(ctx, param, "byref 和 out 参数不能声明默认值");
                ok = 0;
            }
            if ( node->kind == XL6_AST_ASYNC_FUNC &&
                 (paramMode == XL6_AST_FLAG_BYREF || paramMode == XL6_AST_FLAG_OUT) ) {
                xl6SemErrorNode(ctx, param, "async 函数不能声明 byref 或 out 参数");
                ok = 0;
            }
            if ( node->kind == XL6_AST_ASYNC_FUNC && !isVararg && param->childCount > 0 ) {
                const XL6_AstNode* asyncParamType = xl6SemResolveAliasType(ctx, param->children[0]);
                if ( asyncParamType != NULL && xl6SemIsResourceHandleTypeName(asyncParamType->text) ) {
                    xl6SemErrorNode(ctx, param->children[0], "不可复制类型不能作为按值参数");
                    ok = 0;
                }
            }
            if ( isVararg ) {
                if ( seenVararg ) {
                    xl6SemErrorNode(ctx, param, "一个函数最多只能声明一个可变参数");
                    ok = 0;
                }
                if ( i + 1 < params->childCount ) {
                    xl6SemErrorNode(ctx, param, "可变参数必须位于参数表最后");
                    seenBadVarargPosition = 1;
                    ok = 0;
                }
                seenVararg = 1;
            } else if ( seenVararg && !seenBadVarargPosition ) {
                xl6SemErrorNode(ctx, param, "可变参数之后不能声明普通参数");
                ok = 0;
            } else if ( param->childCount > 1 ) {
                seenOptional = 1;
                if ( !xl6SemCheckExpr(ctx, param->children[1]) ) {
                    ok = 0;
                }
                if ( !xl6SemExprCanMatchParamType(ctx, param->children[1], param->children[0]) ) {
                    xl6SemTypeMismatchError(
                        ctx,
                        param->children[1],
                        "默认值表达式类型和参数类型不匹配",
                        param->children[0],
                        xl6SemExprStaticTypeName(ctx, param->children[1]));
                    ok = 0;
                }
            } else if ( seenOptional ) {
                xl6SemErrorNode(ctx, param, "必选参数不能出现在可选参数之后");
                ok = 0;
            }
            if ( !xl6ScopeAddTyped(ctx, param, XL6_SYM_VALUE, param->childCount > 0 ? param->children[0] : NULL) ) {
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    isGeneratorFunc = 0;
    returnTypesValid = 1;
    if ( node->kind == XL6_AST_ASYNC_FUNC && returns != NULL && returns->childCount > 1 ) {
        xl6SemErrorNode(ctx, returns->children[1] != NULL ? returns->children[1] : returns, "async 函数当前不支持 multi-return；只能省略返回值或返回一个值");
        ok = 0;
        returnTypesValid = 0;
    }
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            XL6_AstNode* returnType = (XL6_AstNode*)xl6SemResolveAliasType(ctx, returns->children[i]);
            if ( xl6SemTypeIsAuto(returns->children[i]) ) {
                xl6SemErrorNode(ctx, returns->children[i], "auto 不能作为函数返回类型");
                ok = 0;
            } else if ( !xl6SemCheckType(ctx, returns->children[i]) ) {
                ok = 0;
                returnTypesValid = 0;
            }
            if ( node->kind == XL6_AST_ASYNC_FUNC && returnType != NULL &&
                 xl6SemIsFutureLikeTypeName(returnType->text) ) {
                xl6SemErrorNode(ctx, returns->children[i], "async 函数返回类型应声明为完成后的值类型，不应直接声明 future/task");
                ok = 0;
                returnTypesValid = 0;
            }
            if ( returnType != NULL && xl6SemIsGeneratorTypeName(returnType->text) ) {
                isGeneratorFunc = 1;
            }
            if ( !xl6SemTypeIsAuto(returns->children[i]) &&
                 !(returnType != NULL && xl6SemIsGeneratorTypeName(returnType->text)) &&
                 !xl6SemTypeIsFutureLikeResource(ctx, returns->children[i]) &&
                 returnTypesValid &&
                 !xl6SemTypeUsesClassReferenceSemantics(ctx, returns->children[i]) &&
                 !xl6SemTypeCanCopy(ctx, returns->children[i]) &&
                 !xl6SemTypeRequiresFreshValue(ctx, returns->children[i]) ) {
                xl6SemErrorNode(ctx, returns->children[i], "不可复制类型不能作为返回值类型");
                ok = 0;
            }
        }
    }

    if ( isGeneratorFunc && params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            unsigned int paramMode = xl6SemParamMode(param);
            if ( paramMode == XL6_AST_FLAG_BYREF || paramMode == XL6_AST_FLAG_OUT ) {
                xl6SemErrorNode(ctx, param, "generator 函数不能声明 byref 或 out 参数");
                ok = 0;
            }
            if ( param != NULL && param->childCount > 0 &&
                 !xl6SemParamIsVararg(param) &&
                 !xl6SemChannelElementTypeIsFirstStageSafe(ctx, param->children[0]) ) {
                xl6SemErrorNode(ctx, param->children[0], "generator 参数当前只支持 bool、整数、浮点、string、time、point 等具备稳定复制语义的类型");
                ok = 0;
            }
        }
    }

    oldReturns = ctx->currentReturns;
    oldFunction = ctx->currentFunction;
    oldReturnTypesValid = ctx->currentReturnTypesValid;
    oldGeneratorDepth = ctx->generatorDepth;
    ctx->currentReturns = returns;
    ctx->currentFunction = node;
    ctx->currentReturnTypesValid = returnTypesValid;
    if ( isGeneratorFunc ) {
        ctx->generatorDepth += 1;
    }
    if ( node->childCount > 3 && !xl6SemCheckBlock(ctx, node->children[3], 0) ) {
        ok = 0;
    }
    if ( node->childCount > 3 && !xl6SemCheckOutParams(ctx, node) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    ctx->currentFunction = oldFunction;
    ctx->currentReturnTypesValid = oldReturnTypesValid;
    ctx->generatorDepth = oldGeneratorDepth;

    if ( pushedTypeParams ) {
        xl6SemPopTypeParams(ctx, &typeParamScope);
    }
    ctx->captureBoundary = oldCaptureBoundary;
    ctx->captureBoundaryAllowsCapture = oldCaptureBoundaryAllowsCapture;
    xl6SemPopScope(ctx, &scope);
    return ok;
}

static int xl6SemNodeUsesExportSymbol(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXPORT_FUNC ||
            node->kind == XL6_AST_EXPORT_VAR ||
            node->kind == XL6_AST_EXPORT_CONST);
}

static int xl6SemFindPreviousExportSymbolRec(const XL6_AstNode* scan, const XL6_AstNode* node, int* reachedNode)
{
    size_t i;

    if ( scan == NULL || node == NULL || node->text == NULL || reachedNode == NULL || *reachedNode ) {
        return 0;
    }
    if ( scan == node ) {
        *reachedNode = 1;
        return 0;
    }
    if ( xl6SemNodeUsesExportSymbol(scan) && xl6TextEquals(scan->text, node->text) ) {
        return 1;
    }
    for ( i = 0; i < scan->childCount; ++i ) {
        if ( xl6SemFindPreviousExportSymbolRec(scan->children[i], node, reachedNode) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckExportSymbolUnique(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int reachedNode;

    if ( ctx->root == NULL || node->text == NULL ) {
        return 1;
    }
    reachedNode = 0;
    if ( xl6SemFindPreviousExportSymbolRec(ctx->root, node, &reachedNode) ) {
        xl6SemErrorNode(ctx, node, "重复的 C export symbol");
        return 0;
    }
    return 1;
}

static int xl6SemIsCAbiParamType(const char* name)
{
    return xl6SemIsBoolTypeName(name) || xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) ||
           xl6TextEquals(name, "point") || xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "var") ||
           xl6TextEquals(name, "array") || xl6TextEquals(name, "array<var>") ||
           xl6TextEquals(name, "list") || xl6TextEquals(name, "list<var>") ||
           xl6TextEquals(name, "set") || xl6TextEquals(name, "set<var>") ||
           xl6TextEquals(name, "dict") || xl6TextEquals(name, "dict<var>");
}

static int xl6SemExternParamNeedsXValueWrapper(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return 0;
    }
    return xl6TextEquals(typeNode->text, "var") ||
           xl6TextEquals(typeNode->text, "array") || xl6TextEquals(typeNode->text, "array<var>") ||
           xl6TextEquals(typeNode->text, "list") || xl6TextEquals(typeNode->text, "list<var>") ||
           xl6TextEquals(typeNode->text, "set") || xl6TextEquals(typeNode->text, "set<var>") ||
           xl6TextEquals(typeNode->text, "dict") || xl6TextEquals(typeNode->text, "dict<var>") ||
           xl6TextEquals(typeNode->text, "__class_handle");
}

static int xl6SemIsCAbiReturnType(const char* name)
{
    return xl6SemIsBoolTypeName(name) || xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) || xl6TextEquals(name, "point");
}

static int xl6SemIsCAbiVarType(const char* name)
{
    return xl6SemIsBoolTypeName(name) || xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) ||
           xl6TextEquals(name, "point") || xl6TextEquals(name, "string");
}

static int xl6SemIsCAbiRecordType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_UNION;
}

static int xl6SemIsCAbiOpaqueHandleParamType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    return typeNode != NULL &&
           typeNode->text != NULL &&
           xl6SemIsChannelTypeName(typeNode->text);
}

static int xl6SemIsCAbiClassType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS ) {
        return 0;
    }
    if ( typeInfo->allowGeneric ) {
        return typeInfo->genericArity == (int)typeNode->childCount;
    }
    return typeNode->childCount == 0;
}

static int xl6SemIsCAbiClassHandleType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_AstNode* target = xl6SemClassHandleTargetType(ctx, typeNode);
    const XL6_TypeInfo* typeInfo;

    if ( target == NULL || target->text == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, target->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS;
}

static int xl6SemCheckExportCAbi(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* params;
    XL6_AstNode* returns;
    size_t i;
    int ok;

    ok = 1;

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            XL6_AstNode* typeNode = (param != NULL && param->childCount > 0) ? param->children[0] : NULL;

            if ( param != NULL && param->childCount > 1 ) {
                xl6SemErrorNode(ctx, param, "C ABI export 不支持可选参数");
                ok = 0;
            }
            if ( typeNode == NULL || xl6SemTypeIsCVararg(typeNode) ) {
                xl6SemErrorNode(ctx, param, "C ABI export 不支持可变参数");
                ok = 0;
                continue;
            }
            if ( xl6SemTypeIsArrayVararg(typeNode) || xl6SemTypeIsDictVararg(typeNode) ) {
                continue;
            }
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( !xl6SemIsCAbiClassHandleType(ctx, typeNode) &&
                 !xl6SemIsCAbiClassType(ctx, typeNode) &&
                 !xl6SemIsCAbiOpaqueHandleParamType(ctx, typeNode) &&
                 (typeNode->childCount > 0 ||
                  (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiRecordType(ctx, typeNode))) ) {
                xl6SemErrorNode(ctx, typeNode, "该参数类型不能直接进入当前 C ABI export");
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            XL6_AstNode* typeNode = returns->children[i];
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( typeNode == NULL ||
                 (!xl6SemIsCAbiClassHandleType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode) &&
                  (typeNode->childCount > 0 ||
                   (!xl6SemIsCAbiReturnType(typeNode->text) && !xl6SemIsCAbiRecordType(ctx, typeNode)))) ) {
                xl6SemErrorNode(ctx, typeNode, "该返回类型不能直接进入第二阶段 C ABI export");
                ok = 0;
            }
        }
    }

    return ok;
}

static int xl6SemIsInternalStdRuntimeExtern(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const char* prefix = "__xl_std_runtime_";

    if ( node == NULL || node->childCount == 0 ) {
        return 0;
    }
    nameNode = node->children[0];
    return nameNode != NULL && nameNode->text != NULL &&
           strncmp(nameNode->text, prefix, strlen(prefix)) == 0;
}

static int xl6SemCheckExternCAbi(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* params;
    XL6_AstNode* returns;
    size_t i;
    int ok;

    ok = 1;

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            XL6_AstNode* typeNode = (param != NULL && param->childCount > 0) ? param->children[0] : NULL;

            if ( param != NULL && param->childCount > 1 ) {
                xl6SemErrorNode(ctx, param, "C ABI extern does not support optional parameters");
                ok = 0;
            }
            if ( typeNode == NULL ) {
                xl6SemErrorNode(ctx, param, "C ABI extern does not support varargs");
                ok = 0;
                continue;
            }
            if ( xl6SemTypeIsArrayVararg(typeNode) || xl6SemTypeIsDictVararg(typeNode) ) {
                continue;
            }
            if ( xl6SemTypeIsCVararg(typeNode) ) {
                size_t j;
                if ( i + 1 < params->childCount ) {
                    xl6SemErrorNode(ctx, param, "c_vararg must be the last extern C parameter");
                    ok = 0;
                }
                for ( j = 0; j < i; ++j ) {
                    XL6_AstNode* prevParam = params->children[j];
                    XL6_AstNode* prevType = (prevParam != NULL && prevParam->childCount > 0) ? prevParam->children[0] : NULL;
                    if ( xl6SemExternParamNeedsXValueWrapper(ctx, prevType) ) {
                        xl6SemErrorNode(ctx, prevParam, "c_vararg extern cannot mix with xvalue managed parameters");
                        ok = 0;
                    }
                }
                continue;
            }
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( typeNode != NULL && xl6TextEquals(typeNode->text, "function") && xl6SemIsInternalStdRuntimeExtern(node) ) {
                continue;
            }
            if ( !xl6SemIsCAbiClassHandleType(ctx, typeNode) &&
                 !xl6SemIsCAbiClassType(ctx, typeNode) &&
                 !xl6SemIsCAbiOpaqueHandleParamType(ctx, typeNode) &&
                 (typeNode->childCount > 0 ||
                  (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiRecordType(ctx, typeNode))) ) {
                xl6SemErrorNode(ctx, typeNode, "parameter type cannot enter first-stage C ABI extern directly");
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6SemErrorNode(ctx, returns->children[1] != NULL ? returns->children[1] : returns, "C ABI extern does not support multiple return values");
        ok = 0;
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        XL6_AstNode* typeNode = returns->children[0];
        typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
        if ( typeNode != NULL && xl6SemIsFutureLikeTypeName(typeNode->text) ) {
            return ok;
        }
        if ( typeNode == NULL ||
             (!xl6SemIsCAbiClassHandleType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode) &&
              (typeNode->childCount > 0 ||
               (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiRecordType(ctx, typeNode)))) ) {
            xl6SemErrorNode(ctx, typeNode, "return type cannot enter first-stage C ABI extern directly");
            ok = 0;
        }
    }

    return ok;
}

static int xl6SemDeclareExternalVar(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* typeNode;
    XL6_SymbolKind kind;
    int ok;

    ok = 1;
    nameNode = xl6SemExternalVarNameNode(node);
    typeNode = xl6SemExternalVarTypeNode(node);

    if ( nameNode == NULL || nameNode->text == NULL ) {
        xl6SemErrorNode(ctx, node, "C ABI variable declaration missing xlang name");
        ok = 0;
    }
    if ( typeNode == NULL ) {
        xl6SemErrorNode(ctx, node, "C ABI variable declaration missing type");
        ok = 0;
    } else if ( xl6SemTypeIsAuto(typeNode) ) {
        xl6SemErrorNode(ctx, typeNode, "auto 不能作为 C ABI 变量类型");
        ok = 0;
    } else {
        if ( !xl6SemCheckType(ctx, typeNode) ) {
            ok = 0;
        }
        if ( !xl6SemIsCAbiClassHandleType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode) &&
             (xl6SemResolveAliasType(ctx, typeNode)->childCount > 0 ||
              (!xl6SemIsCAbiVarType(xl6SemResolveAliasTypeName(ctx, typeNode)) && !xl6SemIsCAbiRecordType(ctx, typeNode))) ) {
            xl6SemErrorNode(ctx, typeNode, "variable type cannot enter first-stage C ABI directly");
            ok = 0;
        }
    }

    if ( nameNode != NULL ) {
        kind = node->kind == XL6_AST_EXPORT_CONST ? XL6_SYM_GLOBAL_CONST : XL6_SYM_GLOBAL_VALUE;
        if ( !xl6ScopeAddTyped(ctx, nameNode, kind, typeNode) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckExternalVar(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* initExpr;
    int ok;

    ok = 1;
    typeNode = xl6SemExternalVarTypeNode(node);
    initExpr = xl6SemExternalVarInitExpr(node);

    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        if ( !xl6SemCheckExportSymbolUnique(ctx, node) ) {
            ok = 0;
        }
        if ( initExpr == NULL ) {
            xl6SemErrorNode(ctx, node, "C ABI export variable requires initializer");
            ok = 0;
        } else {
            if ( !xl6SemCheckExpr(ctx, initExpr) ) {
                ok = 0;
            }
            if ( xl6SemExprIsMultiReturnCall(ctx, initExpr) ) {
                xl6SemErrorNode(ctx, initExpr, "C ABI export variable initializer cannot use multi return");
                ok = 0;
            }
            if ( typeNode != NULL && !xl6SemExprCanMatchParamType(ctx, initExpr, typeNode) ) {
                xl6SemTypeMismatchError(
                    ctx,
                    initExpr,
                    "C ABI export variable initializer type mismatch",
                    typeNode,
                    xl6SemExprStaticTypeName(ctx, initExpr));
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemCheckExportFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    ok = xl6SemCheckExportSymbolUnique(ctx, node);
    if ( node != NULL && node->text != NULL && node->text[0] != '\0' &&
         !xl6SemCheckExportCAbi(ctx, node) ) {
        ok = 0;
    }
    if ( !xl6SemCheckFunc(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckExternFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* params;
    XL6_AstNode* returns;
    size_t i;
    int ok;

    ok = 1;
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            if ( param->childCount > 0 ) {
                if ( xl6SemTypeIsAuto(param->children[0]) ) {
                    xl6SemErrorNode(ctx, param->children[0], "auto 不能作为 extern 函数参数类型");
                    ok = 0;
                } else if ( !xl6SemCheckType(ctx, param->children[0]) ) {
                    ok = 0;
                }
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( xl6SemTypeIsAuto(returns->children[i]) ) {
                xl6SemErrorNode(ctx, returns->children[i], "auto 不能作为 extern 函数返回类型");
                ok = 0;
            } else if ( !xl6SemCheckType(ctx, returns->children[i]) ) {
                ok = 0;
            }
        }
    }
    if ( !xl6SemCheckExternCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemIsTypeDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_CLASS ||
            node->kind == XL6_AST_UNION ||
            node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static int xl6SemIsSpecialName(const char* name)
{
    return xl6TextEquals(name, "create") || xl6TextEquals(name, "destroy");
}

static const char* xl6SemMemberName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_FIELD:
    case XL6_AST_CREATE:
    case XL6_AST_UNION:
        return node->text;
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_GET:
    case XL6_AST_LET:
    case XL6_AST_NATIVE_EXTERN_SPECIAL:
    case XL6_AST_NATIVE_EXTERN_FUNC:
        if ( node->childCount > 0 && node->children[0] != NULL ) {
            return node->children[0]->text;
        }
        return NULL;
    default:
        return NULL;
    }
}

static int xl6SemIsClassPropertyMember(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_GET || node->kind == XL6_AST_LET);
}

static int xl6SemMembersShareOverloadGroup(const XL6_AstNode* a, const XL6_AstNode* b)
{
    const char* aName;
    const char* bName;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    if ( xl6SemIsClassPropertyMember(a) || xl6SemIsClassPropertyMember(b) ) {
        aName = xl6SemMemberName(a);
        bName = xl6SemMemberName(b);
        return a->kind == b->kind && aName != NULL && bName != NULL && xl6TextEquals(aName, bName);
    }
    if ( a->kind != XL6_AST_FUNC &&
         a->kind != XL6_AST_ASYNC_FUNC &&
         a->kind != XL6_AST_EXPORT_FUNC &&
         a->kind != XL6_AST_CREATE &&
         a->kind != XL6_AST_NATIVE_EXTERN_FUNC &&
         a->kind != XL6_AST_NATIVE_EXTERN_SPECIAL ) {
        return 0;
    }
    if ( b->kind != XL6_AST_FUNC &&
         b->kind != XL6_AST_ASYNC_FUNC &&
         b->kind != XL6_AST_EXPORT_FUNC &&
         b->kind != XL6_AST_CREATE &&
         b->kind != XL6_AST_NATIVE_EXTERN_FUNC &&
         b->kind != XL6_AST_NATIVE_EXTERN_SPECIAL ) {
        return 0;
    }

    aName = xl6SemMemberName(a);
    bName = xl6SemMemberName(b);
    return aName != NULL && bName != NULL && xl6TextEquals(aName, bName);
}

static int xl6SemCheckTypeDeclName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    ok = 1;
    if ( node->text == NULL ) {
        xl6SemErrorNode(ctx, node, "类型声明缺少名称");
        return 0;
    }
    if ( xl6TextEquals(node->text, "auto") ) {
        xl6SemErrorNode(ctx, node, "自定义类型名不能和内置类型名重复");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckParamListOnly(XL6_SemContext* ctx, XL6_AstNode* params, int addSymbols)
{
    size_t i;
    int ok;
    int seenOptional;
    int seenVararg;
    int seenBadVarargPosition;

    if ( params == NULL ) {
        return 1;
    }

    ok = 1;
    seenOptional = 0;
    seenVararg = 0;
    seenBadVarargPosition = 0;
    for ( i = 0; i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        int isVararg;
        unsigned int paramMode;

        if ( param == NULL ) {
            continue;
        }
        isVararg = xl6SemParamIsVararg(param);
        paramMode = xl6SemParamMode(param);

        if ( param->childCount > 0 ) {
            if ( xl6SemTypeIsAuto(param->children[0]) ) {
                xl6SemErrorNode(ctx, param->children[0], "auto 不能作为函数参数类型");
                ok = 0;
            } else {
                if ( !xl6SemCheckType(ctx, param->children[0]) ) {
                    ok = 0;
                }
            }
        }
        if ( isVararg && paramMode != 0 ) {
            xl6SemErrorNode(ctx, param, "可变参数不能声明 byval、byref 或 out");
            ok = 0;
        }
        if ( (paramMode == XL6_AST_FLAG_BYREF || paramMode == XL6_AST_FLAG_OUT) &&
             param->childCount > 1 ) {
            xl6SemErrorNode(ctx, param, "byref 和 out 参数不能声明默认值");
            ok = 0;
        }
        if ( isVararg ) {
            if ( seenVararg ) {
                xl6SemErrorNode(ctx, param, "一个函数最多只能声明一个可变参数");
                ok = 0;
            }
            if ( i + 1 < params->childCount ) {
                xl6SemErrorNode(ctx, param, "可变参数必须位于参数表最后");
                seenBadVarargPosition = 1;
                ok = 0;
            }
            seenVararg = 1;
        } else if ( seenVararg && !seenBadVarargPosition ) {
            xl6SemErrorNode(ctx, param, "可变参数之后不能声明普通参数");
            ok = 0;
        } else if ( param->childCount > 1 ) {
            seenOptional = 1;
            if ( !xl6SemCheckExpr(ctx, param->children[1]) ) {
                ok = 0;
            }
            if ( !xl6SemExprCanMatchParamType(ctx, param->children[1], param->children[0]) ) {
                xl6SemTypeMismatchError(
                    ctx,
                    param->children[1],
                    "默认值表达式类型和参数类型不匹配",
                    param->children[0],
                    xl6SemExprStaticTypeName(ctx, param->children[1]));
                ok = 0;
            }
        } else if ( seenOptional ) {
            xl6SemErrorNode(ctx, param, "必选参数不能出现在可选参数之后");
            ok = 0;
        }

        if ( addSymbols && !xl6ScopeAddTyped(ctx, param, XL6_SYM_VALUE, param->childCount > 0 ? param->children[0] : NULL) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckReturnTypes(XL6_SemContext* ctx, XL6_AstNode* returns)
{
    size_t i;
    int ok;

    if ( returns == NULL ) {
        return 1;
    }

    ok = 1;
    for ( i = 0; i < returns->childCount; ++i ) {
        if ( xl6SemTypeIsAuto(returns->children[i]) ) {
            xl6SemErrorNode(ctx, returns->children[i], "auto 不能作为函数返回类型");
            ok = 0;
        } else if ( !xl6SemCheckType(ctx, returns->children[i]) ) {
            ok = 0;
        }
        if ( !xl6SemTypeIsAuto(returns->children[i]) &&
             !xl6SemTypeIsFutureLikeResource(ctx, returns->children[i]) &&
             !xl6SemTypeUsesClassReferenceSemantics(ctx, returns->children[i]) &&
             !xl6SemTypeCanCopy(ctx, returns->children[i]) ) {
            xl6SemErrorNode(ctx, returns->children[i], "不可复制类型不能作为按值返回类型");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckAnonFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Scope scope;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* body;
    XL6_AstNode* oldReturns;
    int oldReturnTypesValid;
    int returnTypesValid;
    XL6_Scope* oldCaptureBoundary;
    int oldCaptureBoundaryAllowsCapture;
    XL6_AstNode* oldCurrentClass;
    int oldInClassCreate;
    int ok;

    params = node->childCount > 0 ? node->children[0] : NULL;
    returns = node->childCount > 1 ? node->children[1] : NULL;
    body = node->childCount > 2 ? node->children[2] : NULL;

    ok = 1;
    xl6SemPushScope(ctx, &scope);
    oldCaptureBoundary = ctx->captureBoundary;
    oldCaptureBoundaryAllowsCapture = ctx->captureBoundaryAllowsCapture;
    ctx->captureBoundary = &scope;
    ctx->captureBoundaryAllowsCapture = 1;
    if ( !xl6SemCheckParamListOnly(ctx, params, 1) ) {
        ok = 0;
    }
    if ( xl6SemParamListHasCVararg(params) ) {
        xl6SemErrorNode(ctx, params, "c_vararg 只能用于 extern C 函数声明");
        ok = 0;
    }
    returnTypesValid = xl6SemCheckReturnTypes(ctx, returns);
    if ( !returnTypesValid ) {
        ok = 0;
    }
    oldReturns = ctx->currentReturns;
    oldReturnTypesValid = ctx->currentReturnTypesValid;
    oldCurrentClass = ctx->currentClass;
    oldInClassCreate = ctx->inClassCreate;
    ctx->currentReturns = returns;
    ctx->currentReturnTypesValid = returnTypesValid;
    ctx->currentClass = NULL;
    ctx->inClassCreate = 0;
    if ( !xl6SemCheckBlock(ctx, body, 0) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    ctx->currentReturnTypesValid = oldReturnTypesValid;
    ctx->currentClass = oldCurrentClass;
    ctx->inClassCreate = oldInClassCreate;
    ctx->captureBoundary = oldCaptureBoundary;
    ctx->captureBoundaryAllowsCapture = oldCaptureBoundaryAllowsCapture;
    xl6SemPopScope(ctx, &scope);
    return ok;
}

static int xl6SemCheckFieldAttrValueExprs(XL6_SemContext* ctx, XL6_AstNode* field)
{
    size_t i;
    int ok;

    ok = 1;
    for ( i = 1; field != NULL && i < field->childCount; ++i ) {
        XL6_AstNode* attr = field->children[i];
        size_t j;

        if ( attr == NULL || attr->kind != XL6_AST_FIELD_ATTR ) {
            continue;
        }
        for ( j = 0; j < attr->childCount; ++j ) {
            XL6_AstNode* arg = attr->children[j];
            XL6_AstNode* value = arg;

            /* 注解选项名属于元数据，不进入变量名解析；只检查实际的值表达式。 */
            if ( arg != NULL && arg->kind == XL6_AST_KEY_VALUE ) {
                value = arg->childCount > 1 ? arg->children[1] : NULL;
            }
            if ( value != NULL && value->kind != XL6_AST_NAME &&
                 !xl6SemCheckExpr(ctx, value) ) {
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemCheckField(XL6_SemContext* ctx, XL6_AstNode* field)
{
    XL6_AstNode* defaultExpr;
    int ok;

    ok = 1;
    defaultExpr = xl6SemFieldDefaultExpr(field);
    if ( xl6SemIsSpecialName(field->text) ) {
        xl6SemErrorNode(ctx, field, "字段名不能使用 create 或 destroy");
        ok = 0;
    }
    if ( field->childCount > 0 ) {
        if ( xl6SemTypeIsAuto(field->children[0]) ) {
            xl6SemErrorNode(ctx, field->children[0], "auto 不能作为 class 字段类型");
            ok = 0;
        } else if ( !xl6SemCheckType(ctx, field->children[0]) ) {
            ok = 0;
        }
    }
    if ( defaultExpr != NULL && !xl6SemCheckExpr(ctx, defaultExpr) ) {
        ok = 0;
    }
    if ( defaultExpr != NULL && field->childCount > 0 &&
         !xl6SemExprCanMatchParamType(ctx, defaultExpr, field->children[0]) ) {
        xl6SemTypeMismatchError(
            ctx,
            defaultExpr,
            "字段默认值表达式类型和字段类型不匹配",
            field->children[0],
            xl6SemExprStaticTypeName(ctx, defaultExpr));
        ok = 0;
    }
    if ( !xl6SemCheckFieldAttrs(ctx->diags, field) ) {
        ok = 0;
    }
    if ( !xl6SemCheckFieldAttrValueExprs(ctx, field) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckPropertyMember(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;
    int ok;

    if ( member == NULL || (member->kind != XL6_AST_GET && member->kind != XL6_AST_LET) ) {
        return 1;
    }

    ok = xl6SemCheckFunc(ctx, member);
    nameNode = member->childCount > 0 ? member->children[0] : NULL;
    params = (XL6_AstNode*)xl6SemCallableParams(member);
    returns = xl6SemCallableReturns(member);

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( xl6SemParamIsVararg(param) ) {
            xl6SemErrorNode(ctx, param, "property parameters cannot be vararg");
            ok = 0;
        }
        if ( param != NULL && param->childCount > 1 ) {
            xl6SemErrorNode(ctx, param, "property parameters cannot be optional");
            ok = 0;
        }
    }

    if ( member->kind == XL6_AST_GET ) {
        if ( returns == NULL || returns->childCount != 1 ) {
            xl6SemErrorNode(ctx, member, "get property must declare exactly one return type");
            ok = 0;
        }
    } else {
        if ( params == NULL || params->childCount == 0 ) {
            xl6SemErrorNode(ctx, member, "let property must declare a value parameter");
            ok = 0;
        }
        if ( returns != NULL && returns->childCount != 0 ) {
            xl6SemErrorNode(ctx, member, "let property cannot declare return types");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckNoSelfInStaticClassMemberRec(XL6_SemContext* ctx, XL6_AstNode* node, int isRoot)
{
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }

    if ( node->kind == XL6_AST_NAME && xl6TextEquals(node->text, "self") ) {
        xl6SemErrorNode(ctx, node, "static class member cannot use self");
        return 0;
    }

    /* 嵌套声明拥有自己的 self 语义，不继承外层 static 成员的检查上下文。 */
    if ( !isRoot &&
         (node->kind == XL6_AST_FUNC ||
          node->kind == XL6_AST_ASYNC_FUNC ||
          node->kind == XL6_AST_ANON_FUNC ||
          node->kind == XL6_AST_CLASS) ) {
        return 1;
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6SemCheckNoSelfInStaticClassMemberRec(ctx, node->children[i], 0) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckNoSelfInStaticClassMember(XL6_SemContext* ctx, XL6_AstNode* member)
{
    return xl6SemCheckNoSelfInStaticClassMemberRec(ctx, member, 1);
}

static int xl6SemCheckClassMemberFlags(XL6_SemContext* ctx, XL6_AstNode* member)
{
    int ok;
    int hasAccess;
    int hasDispatchFlag;

    if ( member == NULL ) {
        return 1;
    }

    ok = 1;
    hasAccess = xl6SemAstHasFlag(member, XL6_AST_FLAG_PUBLIC) ||
                xl6SemAstHasFlag(member, XL6_AST_FLAG_PRIVATE) ||
                xl6SemAstHasFlag(member, XL6_AST_FLAG_PROTECTED);
    hasDispatchFlag = xl6SemAstHasFlag(member, XL6_AST_FLAG_OVERRIDE) ||
                      xl6SemAstHasFlag(member, XL6_AST_FLAG_FINAL) ||
                      xl6SemAstHasFlag(member, XL6_AST_FLAG_VIRTUAL);

    if ( (xl6SemAstHasFlag(member, XL6_AST_FLAG_PUBLIC) ? 1 : 0) +
         (xl6SemAstHasFlag(member, XL6_AST_FLAG_PRIVATE) ? 1 : 0) +
         (xl6SemAstHasFlag(member, XL6_AST_FLAG_PROTECTED) ? 1 : 0) > 1 ) {
        xl6SemErrorNode(ctx, member, "class member access must be exactly one of public, private or protected");
        ok = 0;
    }

    if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) &&
         member->kind != XL6_AST_FIELD &&
         member->kind != XL6_AST_FUNC &&
         member->kind != XL6_AST_ASYNC_FUNC &&
         member->kind != XL6_AST_EXPORT_FUNC ) {
        xl6SemErrorNode(ctx, member, "static is only supported on class fields and funcs");
        ok = 0;
    }

    if ( hasAccess &&
         member->kind != XL6_AST_FIELD &&
         member->kind != XL6_AST_FUNC &&
         member->kind != XL6_AST_ASYNC_FUNC &&
         member->kind != XL6_AST_GET &&
         member->kind != XL6_AST_LET &&
         member->kind != XL6_AST_CREATE ) {
        xl6SemErrorNode(ctx, member, "public/private/protected is only supported on class fields, funcs, properties and create/destroy");
        ok = 0;
    }

    if ( hasDispatchFlag &&
         member->kind != XL6_AST_FUNC &&
         member->kind != XL6_AST_GET &&
         member->kind != XL6_AST_LET ) {
        xl6SemErrorNode(ctx, member, "override/final/virtual is only supported on class funcs and properties");
        ok = 0;
    }

    return ok;
}

static int xl6SemMemberHasDispatchFlag(const XL6_AstNode* member)
{
    return xl6SemAstHasFlag(member, XL6_AST_FLAG_OVERRIDE) ||
           xl6SemAstHasFlag(member, XL6_AST_FLAG_FINAL) ||
           xl6SemAstHasFlag(member, XL6_AST_FLAG_VIRTUAL);
}

static int xl6SemMemberCanUseDispatchFlags(const XL6_AstNode* member)
{
    return member != NULL &&
           (member->kind == XL6_AST_FUNC ||
            member->kind == XL6_AST_GET ||
            member->kind == XL6_AST_LET);
}

static int xl6SemParamListsExactEqual(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t aCount;
    size_t bCount;
    size_t i;

    aCount = a != NULL ? a->childCount : 0;
    bCount = b != NULL ? b->childCount : 0;
    if ( aCount != bCount ) {
        return 0;
    }
    if ( xl6SemParamRequiredCount(a) != xl6SemParamRequiredCount(b) ) {
        return 0;
    }
    if ( xl6SemFixedParamCount(a) != xl6SemFixedParamCount(b) ) {
        return 0;
    }
    for ( i = 0; i < aCount; ++i ) {
        const XL6_AstNode* aParam = a->children[i];
        const XL6_AstNode* bParam = b->children[i];
        const XL6_AstNode* aType;
        const XL6_AstNode* bType;

        if ( xl6SemParamIsVararg(aParam) != xl6SemParamIsVararg(bParam) ) {
            return 0;
        }
        if ( xl6SemParamMode(aParam) != xl6SemParamMode(bParam) ) {
            return 0;
        }
        if ( xl6SemParamIsVararg(aParam) &&
             xl6SemParamVarargKind(aParam) != xl6SemParamVarargKind(bParam) ) {
            return 0;
        }
        aType = aParam != NULL && aParam->childCount > 0 ? aParam->children[0] : NULL;
        bType = bParam != NULL && bParam->childCount > 0 ? bParam->children[0] : NULL;
        if ( !xl6SemTypeEquals(ctx, aType, bType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemReturnListsExactEqual(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t aCount;
    size_t bCount;
    size_t i;

    aCount = a != NULL ? a->childCount : 0;
    bCount = b != NULL ? b->childCount : 0;
    if ( aCount != bCount ) {
        return 0;
    }
    for ( i = 0; i < aCount; ++i ) {
        if ( !xl6SemTypeEquals(ctx, a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemCallableSignatureExactEqual(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    if ( !xl6SemMembersShareOverloadGroup(a, b) ) {
        return 0;
    }
    if ( a == NULL || b == NULL || a->kind != b->kind ) {
        return 0;
    }
    return xl6SemParamListsExactEqual(ctx, xl6SemCallableParams(a), xl6SemCallableParams(b)) &&
           xl6SemReturnListsExactEqual(ctx, xl6SemCallableReturns(a), xl6SemCallableReturns(b));
}

static const XL6_AstNode* xl6SemFindBaseOverrideTarget(
    XL6_SemContext* ctx,
    const XL6_AstNode* baseClass,
    const XL6_AstNode* member,
    const XL6_AstNode** ownerClass)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* nextBase;
    size_t i;

    if ( ownerClass != NULL ) {
        *ownerClass = NULL;
    }
    if ( baseClass == NULL || member == NULL ) {
        return NULL;
    }

    body = baseClass->childCount > 1 ? baseClass->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* candidate = body->children[i];
        if ( xl6SemAstHasFlag(candidate, XL6_AST_FLAG_STATIC) ) {
            continue;
        }
        if ( xl6SemCallableSignatureExactEqual(ctx, member, candidate) ) {
            if ( ownerClass != NULL ) {
                *ownerClass = baseClass;
            }
            return candidate;
        }
    }

    base = baseClass->childCount > 0 ? baseClass->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    nextBase = xl6SemFindTypeDecl(ctx, base->text);
    return xl6SemFindBaseOverrideTarget(ctx, nextBase, member, ownerClass);
}

static int xl6SemBaseMemberIsVirtualChain(const XL6_AstNode* member)
{
    return xl6SemAstHasFlag(member, XL6_AST_FLAG_VIRTUAL) ||
           xl6SemAstHasFlag(member, XL6_AST_FLAG_OVERRIDE);
}

static int xl6SemCheckClassOverrideRules(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* baseDecl)
{
    XL6_AstNode* body;
    size_t i;
    int ok;

    if ( classNode == NULL || classNode->childCount < 2 ) {
        return 1;
    }

    body = classNode->children[1];
    ok = 1;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        const XL6_AstNode* targetOwner;
        const XL6_AstNode* target;
        int hasOverride;
        int hasVirtual;
        int hasFinal;
        int hasStatic;

        if ( member == NULL || !xl6SemMemberCanUseDispatchFlags(member) ) {
            continue;
        }

        hasOverride = xl6SemAstHasFlag(member, XL6_AST_FLAG_OVERRIDE);
        hasVirtual = xl6SemAstHasFlag(member, XL6_AST_FLAG_VIRTUAL);
        hasFinal = xl6SemAstHasFlag(member, XL6_AST_FLAG_FINAL);
        hasStatic = xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC);

        if ( hasStatic && (hasOverride || hasVirtual || hasFinal) ) {
            xl6SemErrorNode(ctx, member, "static class member cannot use override/final/virtual");
            ok = 0;
            continue;
        }
        if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_PRIVATE) && (hasOverride || hasVirtual || hasFinal) ) {
            xl6SemErrorNode(ctx, member, "private class member cannot use override/final/virtual");
            ok = 0;
            continue;
        }

        targetOwner = NULL;
        target = xl6SemFindBaseOverrideTarget(ctx, baseDecl, member, &targetOwner);
        (void)targetOwner;

        if ( hasOverride && target == NULL ) {
            xl6SemErrorNode(ctx, member, "override requires an exact base member signature");
            ok = 0;
            continue;
        }
        if ( target == NULL ) {
            continue;
        }
        if ( xl6SemAstHasFlag(target, XL6_AST_FLAG_FINAL) ) {
            xl6SemErrorNode(ctx, member, "cannot override a final class member");
            ok = 0;
            continue;
        }
        if ( !hasOverride ) {
            xl6SemErrorNode(ctx, member, "overriding a base member requires explicit override");
            ok = 0;
            continue;
        }
        if ( !xl6SemBaseMemberIsVirtualChain(target) ) {
            xl6SemErrorNode(ctx, member, "override target must be virtual");
            ok = 0;
        }
    }
    return ok;
}

static XL6_AstNode* xl6SemUnionBody(XL6_AstNode* node);

static int xl6SemCheckUnionMemberConflicts(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* body;
    size_t i;
    size_t j;
    int ok;

    body = xl6SemUnionBody(node);
    if ( body == NULL ) {
        return 1;
    }
    ok = 1;
    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* field = body->children[i];
        if ( field == NULL || field->kind != XL6_AST_FIELD || field->text == NULL ) {
            continue;
        }
        for ( j = i + 1; j < body->childCount; ++j ) {
            XL6_AstNode* other = body->children[j];
            if ( other != NULL && other->kind == XL6_AST_FIELD && xl6TextEquals(field->text, other->text) ) {
                xl6SemErrorNode(ctx, other, "duplicate union field name");
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemUnionFieldTypeIsPlainDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* decl;
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    const XL6_AstNode* typeArgs;
    size_t i;

    if ( typeNode == NULL || depth > 32 ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }
    if ( xl6TextEquals(typeNode->text, "__class_handle") ||
         xl6TextEquals(typeNode->text, "function") ||
         xl6TextEquals(typeNode->text, "var") ||
         xl6SemIsContainerBaseTypeName(typeNode->text) ||
         xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_BUILTIN ) {
        return 1;
    }
    decl = typeInfo->decl;
    if ( decl == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_UNION ) {
        body = decl->childCount > 0 ? decl->children[0] : NULL;
        for ( i = 0; body != NULL && i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field != NULL && field->kind == XL6_AST_FIELD && field->childCount > 0 &&
                 !xl6SemUnionFieldTypeIsPlainDepth(ctx, field->children[0], depth + 1) ) {
                return 0;
            }
        }
        return 1;
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS ) {
        return 1;
    }

    if ( xl6SemClassDeclHasDestroy(decl) ) {
        return 0;
    }
    base = decl->childCount > 0 ? decl->children[0] : NULL;
    if ( base != NULL && !xl6SemUnionFieldTypeIsPlainDepth(ctx, base, depth + 1) ) {
        return 0;
    }

    body = decl->childCount > 1 ? decl->children[1] : NULL;
    typeArgs = xl6SemTypeNodeGenericArgs(typeNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && member->childCount > 0 ) {
            XL6_AstNode* concreteType = NULL;
            const XL6_AstNode* checkType = member->children[0];
            if ( typeArgs != NULL && xl6SemClassHasTypeParams(decl) ) {
                concreteType = xl6SemSubstituteClassType(decl, typeArgs, checkType);
                if ( concreteType != NULL ) {
                    checkType = concreteType;
                }
            }
            if ( !xl6SemUnionFieldTypeIsPlainDepth(ctx, checkType, depth + 1) ) {
                xl6AstFree(concreteType);
                return 0;
            }
            xl6AstFree(concreteType);
        } else if ( member != NULL && member->kind == XL6_AST_UNION ) {
            XL6_AstNode* unionBody = xl6SemUnionBody((XL6_AstNode*)member);
            size_t j;
            for ( j = 0; unionBody != NULL && j < unionBody->childCount; ++j ) {
                const XL6_AstNode* field = unionBody->children[j];
                if ( field != NULL && field->kind == XL6_AST_FIELD && field->childCount > 0 ) {
                    XL6_AstNode* concreteType = NULL;
                    const XL6_AstNode* checkType = field->children[0];
                    if ( typeArgs != NULL && xl6SemClassHasTypeParams(decl) ) {
                        concreteType = xl6SemSubstituteClassType(decl, typeArgs, checkType);
                        if ( concreteType != NULL ) {
                            checkType = concreteType;
                        }
                    }
                    if ( !xl6SemUnionFieldTypeIsPlainDepth(ctx, checkType, depth + 1) ) {
                        xl6AstFree(concreteType);
                        return 0;
                    }
                    xl6AstFree(concreteType);
                }
            }
        }
    }
    return 1;
}

/*
    class 和 union 最终都会展开为 C 的按值记录布局。
    只要沿按值字段重新到达正在检查的声明，记录尺寸就不可能有限。
*/
static int xl6SemInlineLayoutTypeReachesDecl(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* targetDecl,
    int depth);

static int xl6SemInlineLayoutBodyReachesDecl(
    XL6_SemContext* ctx,
    const XL6_AstNode* body,
    const XL6_AstNode* ownerDecl,
    const XL6_AstNode* ownerType,
    const XL6_AstNode* targetDecl,
    int depth)
{
    const XL6_AstNode* typeArgs;
    size_t i;

    if ( body == NULL ) {
        return 0;
    }
    typeArgs = ownerType != NULL ? xl6SemTypeNodeGenericArgs(ownerType) : NULL;
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];

        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FIELD &&
             member->childCount > 0 &&
             !xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) ) {
            const XL6_AstNode* fieldType = member->children[0];
            XL6_AstNode* concreteType = NULL;

            if ( ownerDecl != NULL && typeArgs != NULL && xl6SemClassHasTypeParams(ownerDecl) ) {
                concreteType = xl6SemSubstituteClassType(ownerDecl, typeArgs, fieldType);
                if ( concreteType != NULL ) {
                    fieldType = concreteType;
                }
            }
            if ( xl6SemInlineLayoutTypeReachesDecl(ctx, fieldType, targetDecl, depth + 1) ) {
                xl6AstFree(concreteType);
                return 1;
            }
            xl6AstFree(concreteType);
            continue;
        }
        if ( member->kind == XL6_AST_UNION &&
             xl6SemInlineLayoutBodyReachesDecl(
                 ctx,
                 xl6SemUnionBody((XL6_AstNode*)member),
                 NULL,
                 NULL,
                 targetDecl,
                 depth + 1) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemInlineLayoutTypeReachesDecl(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* targetDecl,
    int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* decl;
    const XL6_AstNode* body;
    const XL6_AstNode* base;

    /* 深度耗尽本身说明存在未能收敛的递归实例，必须拒绝而不是继续递归。 */
    if ( depth > 128 ) {
        return 1;
    }
    typeNode = xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    if ( xl6TextEquals(typeNode->text, "__class_handle") ||
         xl6TextEquals(typeNode->text, "function") ||
         xl6TextEquals(typeNode->text, "var") ||
         xl6SemIsContainerBaseTypeName(typeNode->text) ||
         xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ||
         (typeInfo->kind != XL6_TYPE_CLASS &&
          typeInfo->kind != XL6_TYPE_UNION) ||
         typeInfo->decl == NULL ) {
        return 0;
    }
    decl = typeInfo->decl;
    if ( decl == targetDecl ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_UNION ) {
        return xl6SemInlineLayoutBodyReachesDecl(
            ctx,
            decl->childCount > 0 ? decl->children[0] : NULL,
            decl,
            typeNode,
            targetDecl,
            depth + 1);
    }

    base = decl->childCount > 0 ? decl->children[0] : NULL;
    if ( base != NULL && xl6SemInlineLayoutTypeReachesDecl(ctx, base, targetDecl, depth + 1) ) {
        return 1;
    }
    body = decl->childCount > 1 ? decl->children[1] : NULL;
    return xl6SemInlineLayoutBodyReachesDecl(ctx, body, decl, typeNode, targetDecl, depth + 1);
}

static int xl6SemCheckInlineLayoutCycles(
    XL6_SemContext* ctx,
    XL6_AstNode* decl,
    XL6_AstNode* body)
{
    XL6_AstNode* base;
    size_t i;
    int ok;

    if ( decl == NULL || body == NULL ) {
        return 1;
    }
    ok = 1;
    base = decl->kind == XL6_AST_CLASS && decl->childCount > 0 ? decl->children[0] : NULL;
    if ( base != NULL && xl6SemInlineLayoutTypeReachesDecl(ctx, base, decl, 0) ) {
        xl6SemErrorNode(ctx, base, "继承产生无限递归的按值布局");
        ok = 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];

        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FIELD &&
             member->childCount > 0 &&
             !xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) &&
             xl6SemInlineLayoutTypeReachesDecl(ctx, member->children[0], decl, 0) ) {
            xl6SemErrorNode(ctx, member, "字段产生无限递归的按值布局，请改用 class 或容器保存递归对象");
            ok = 0;
        } else if ( member->kind == XL6_AST_UNION &&
                    xl6SemInlineLayoutBodyReachesDecl(
                        ctx,
                        xl6SemUnionBody(member),
                        NULL,
                        NULL,
                        decl,
                        0) ) {
            xl6SemErrorNode(ctx, member, "union 产生无限递归的按值布局，请改用 class 或容器保存递归对象");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckUnion(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* body;
    size_t i;
    int ok;

    ok = 1;
    if ( ctx->currentClass == NULL ) {
        if ( node->text == NULL ) {
            xl6SemErrorNode(ctx, node, "top-level union must have a name");
            ok = 0;
        } else if ( !xl6SemCheckTypeDeclName(ctx, node) ) {
            ok = 0;
        }
    } else if ( node->text != NULL ) {
        xl6SemErrorNode(ctx, node, "class inline union must be anonymous in this phase");
        ok = 0;
    }

    body = xl6SemUnionBody(node);
    if ( body == NULL || body->childCount == 0 ) {
        xl6SemErrorNode(ctx, node, "union must contain at least one field");
        return 0;
    }
    if ( !xl6SemCheckUnionMemberConflicts(ctx, node) ) {
        ok = 0;
    }
    if ( node->text != NULL && !xl6SemCheckInlineLayoutCycles(ctx, node, body) ) {
        ok = 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* field = body->children[i];
        if ( field == NULL || field->kind != XL6_AST_FIELD ) {
            xl6SemErrorNode(ctx, field, "union only supports field declarations");
            ok = 0;
            continue;
        }
        if ( field->childCount > 1 ) {
            xl6SemErrorNode(ctx, field, "union fields do not support default values");
            ok = 0;
        }
        if ( !xl6SemCheckField(ctx, field) ) {
            ok = 0;
        }
        if ( field->childCount > 0 && !xl6SemUnionFieldTypeIsPlainDepth(ctx, field->children[0], 0) ) {
            xl6SemErrorNode(ctx, field->children[0], "union field type must be a plain C-compatible value type");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckClassSpecial(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Scope scope;
    XL6_AstNode emptyReturns;
    XL6_AstNode* oldReturns;
    int oldReturnTypesValid;
    XL6_AstNode* params;
    int ok;

    ok = 1;
    if ( !xl6SemIsSpecialName(node->text) ) {
        xl6SemErrorNode(ctx, node, "class 成员中只有 create/destroy 可以省略 func");
        ok = 0;
    }

    params = node->childCount > 0 ? node->children[0] : NULL;
    if ( xl6TextEquals(node->text, "destroy") && params != NULL && params->childCount > 0 ) {
        xl6SemErrorNode(ctx, node, "destroy 不能声明参数");
        ok = 0;
    }

    xl6SemPushScope(ctx, &scope);
    if ( ctx->implicitSelfName != NULL ) {
        XL6_AstNode selfNode;
        XL6_AstNode* selfType;
        memset(&selfNode, 0, sizeof(selfNode));
        selfNode.kind = XL6_AST_NAME;
        selfNode.text = (char*)ctx->implicitSelfName;
        selfNode.line = node->line;
        selfNode.column = node->column;
        selfType = ctx->currentClass;
        if ( selfType != NULL ) {
            if ( !xl6ScopeAddTyped(ctx, &selfNode, XL6_SYM_VALUE, selfType) ) {
                ok = 0;
            }
        } else if ( !xl6ScopeAdd(ctx, &selfNode, XL6_SYM_VALUE) ) {
            ok = 0;
        }
    }
    if ( params != NULL && !xl6SemCheckParamListOnly(ctx, params, 1) ) {
        ok = 0;
    }
    memset(&emptyReturns, 0, sizeof(emptyReturns));
    emptyReturns.kind = XL6_AST_RETURN_TYPES;
    oldReturns = ctx->currentReturns;
    oldReturnTypesValid = ctx->currentReturnTypesValid;
    ctx->currentReturns = &emptyReturns;
    ctx->currentReturnTypesValid = 1;
    if ( node->childCount > 1 && !xl6SemCheckBlock(ctx, node->children[1], 0) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    ctx->currentReturnTypesValid = oldReturnTypesValid;
    xl6SemPopScope(ctx, &scope);
    return ok;
}

static int xl6SemCheckClassNativeExternBase(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* node)
{
    int ok;

    ok = 1;
    if ( xl6SemClassHasTypeParams(classNode) ) {
        xl6SemErrorNode(ctx, node, "native-backed class extern member does not support generic class in this stage");
        ok = 0;
    }
    if ( xl6SemAstHasFlag(node, XL6_AST_FLAG_STATIC) ) {
        xl6SemErrorNode(ctx, node, "native-backed class extern member cannot be static");
        ok = 0;
    }
    if ( !xl6SemClassDeclHasNativeHandle(ctx, classNode) ) {
        xl6SemErrorNode(ctx, node, "native-backed class extern member requires native point handle field");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckClassExternParamsNoVararg(XL6_SemContext* ctx, XL6_AstNode* params)
{
    size_t i;
    int ok;

    ok = 1;
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        if ( xl6SemParamIsVararg(params->children[i]) ) {
            xl6SemErrorNode(ctx, params->children[i], "native-backed class extern member does not support vararg parameters");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckClassExternSpecial(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* params;
    int ok;

    ok = xl6SemCheckClassNativeExternBase(ctx, classNode, node);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;

    if ( nameNode == NULL || !xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode != NULL ? nameNode : node, "class extern without func can only declare create/destroy");
        ok = 0;
    }
    if ( nameNode != NULL && xl6TextEquals(nameNode->text, "destroy") && params != NULL && params->childCount > 0 ) {
        xl6SemErrorNode(ctx, nameNode, "native-backed class destroy cannot declare parameters");
        ok = 0;
    }
    if ( !xl6SemCheckParamListOnly(ctx, params, 0) ) {
        ok = 0;
    }
    if ( !xl6SemCheckClassExternParamsNoVararg(ctx, params) ) {
        ok = 0;
    }
    if ( !xl6SemCheckExternCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckClassExternFunc(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    int ok;

    ok = xl6SemCheckClassNativeExternBase(ctx, classNode, node);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( nameNode != NULL && xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode, "class native create/destroy must use the special extern declaration form");
        ok = 0;
    }
    if ( node->childCount > 1 && !xl6SemCheckParamListOnly(ctx, node->children[1], 0) ) {
        ok = 0;
    }
    if ( node->childCount > 1 && !xl6SemCheckClassExternParamsNoVararg(ctx, node->children[1]) ) {
        ok = 0;
    }
    if ( node->childCount > 2 && !xl6SemCheckReturnTypes(ctx, node->children[2]) ) {
        ok = 0;
    }
    if ( !xl6SemCheckExternCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckClassExternProperty(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    size_t i;
    int ok;
    int propertyParamsOk;

    ok = xl6SemCheckClassNativeExternBase(ctx, classNode, node);
    propertyParamsOk = 1;
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    returns = node->childCount > 2 ? node->children[2] : NULL;

    if ( nameNode != NULL && xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode, "class native create/destroy must use the special extern declaration form");
        ok = 0;
    }
    if ( !xl6SemCheckParamListOnly(ctx, params, 0) ) {
        ok = 0;
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( xl6SemParamIsVararg(param) ) {
            xl6SemErrorNode(ctx, param, "extern property parameters cannot be vararg");
            propertyParamsOk = 0;
            ok = 0;
        }
        if ( param != NULL && param->childCount > 1 ) {
            xl6SemErrorNode(ctx, param, "extern property parameters cannot be optional");
            propertyParamsOk = 0;
            ok = 0;
        }
    }
    if ( node->kind == XL6_AST_GET ) {
        if ( returns == NULL || returns->childCount != 1 ) {
            xl6SemErrorNode(ctx, node, "extern get property must declare exactly one return type");
            ok = 0;
        }
        if ( returns != NULL && !xl6SemCheckReturnTypes(ctx, returns) ) {
            ok = 0;
        }
    } else {
        if ( params == NULL || params->childCount == 0 ) {
            xl6SemErrorNode(ctx, node, "extern let property must declare a value parameter");
            ok = 0;
        }
        if ( returns != NULL && returns->childCount != 0 ) {
            xl6SemErrorNode(ctx, node, "extern let property cannot declare return types");
            ok = 0;
        }
    }
    if ( propertyParamsOk && !xl6SemCheckExternCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckClassExportFuncBase(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    int ok;

    if ( node == NULL || node->text == NULL || node->text[0] == '\0' ) {
        return 1;
    }

    ok = 1;
    nameNode = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    if ( xl6SemClassHasTypeParams(classNode) ) {
        xl6SemErrorNode(ctx, node, "class C ABI export member does not support generic class in this stage");
        ok = 0;
    }
    if ( xl6SemAstHasFlag(node, XL6_AST_FLAG_STATIC) ) {
        xl6SemErrorNode(ctx, node, "class C ABI export member cannot be static");
        ok = 0;
    }
    if ( nameNode != NULL && xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode, "create/destroy cannot be exported as a class C ABI member");
        ok = 0;
    }
    if ( !xl6SemCheckExportSymbolUnique(ctx, node) ) {
        ok = 0;
    }
    if ( !xl6SemCheckExportCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static XL6_AstNode* xl6SemUnionBody(XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6SemFindUnionFieldNode(const XL6_AstNode* unionNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( unionNode == NULL || fieldName == NULL ) {
        return NULL;
    }
    body = unionNode->childCount > 0 ? unionNode->children[0] : NULL;
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD && xl6TextEquals(field->text, fieldName) ) {
            return field;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6SemFindDirectClassFieldNode(const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || fieldName == NULL ) {
        return NULL;
    }
    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && xl6TextEquals(member->text, fieldName) ) {
            return member;
        }
        if ( member != NULL && member->kind == XL6_AST_UNION && member->text == NULL ) {
            const XL6_AstNode* field = xl6SemFindUnionFieldNode(member, fieldName);
            if ( field != NULL ) {
                return field;
            }
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6SemFindClassFieldNode(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const XL6_AstNode** ownerClass)
{
    const XL6_AstNode* field;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( ownerClass != NULL ) {
        *ownerClass = NULL;
    }
    if ( classNode == NULL || fieldName == NULL ) {
        return NULL;
    }

    field = xl6SemFindDirectClassFieldNode(classNode, fieldName);
    if ( field != NULL ) {
        if ( ownerClass != NULL ) {
            *ownerClass = classNode;
        }
        return field;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6SemFindTypeDecl(ctx, base->text);
    return xl6SemFindClassFieldNode(ctx, baseClass, fieldName, ownerClass);
}

static XL6_AstNode* xl6SemClassDeclFromTypeMemberReceiver(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;
    const XL6_TypeInfo* typeInfo;

    if ( ctx == NULL || member == NULL || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME || receiver->text == NULL ) {
        return NULL;
    }
    sym = xl6ScopeFind(ctx->scope, receiver->text);
    if ( sym != NULL ) {
        return NULL;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, receiver->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS ? typeInfo->decl : NULL;
}

static const XL6_AstNode* xl6SemFindStaticClassFieldNode(
    XL6_SemContext* ctx,
    const XL6_AstNode* classNode,
    const char* fieldName,
    const XL6_AstNode** ownerClass)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( ownerClass != NULL ) {
        *ownerClass = NULL;
    }
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || fieldName == NULL ) {
        return NULL;
    }
    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_FIELD &&
             xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) &&
             xl6TextEquals(member->text, fieldName) ) {
            if ( ownerClass != NULL ) {
                *ownerClass = classNode;
            }
            return member;
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6SemFindTypeDecl(ctx, base->text);
    return xl6SemFindStaticClassFieldNode(ctx, baseClass, fieldName, ownerClass);
}

static XL6_AstNode* xl6SemCloneStaticClassFieldType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* field;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }
    classNode = xl6SemClassDeclFromTypeMemberReceiver(ctx, member);
    if ( classNode == NULL ) {
        return NULL;
    }
    ownerClass = NULL;
    field = xl6SemFindStaticClassFieldNode(ctx, classNode, member->text, &ownerClass);
    if ( field == NULL || field->childCount == 0 || !xl6SemCanAccessClassMember(ctx, ownerClass, field) ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(field->children[0]);
}

static XL6_AstNode* xl6SemResolveStaticClassFuncCall(
    XL6_SemContext* ctx,
    XL6_AstNode* call,
    int report)
{
    XL6_AstNode* callee;
    XL6_AstNode* classNode;

    if ( call == NULL || call->childCount == 0 ) {
        return NULL;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || callee->text == NULL ) {
        return NULL;
    }
    classNode = xl6SemClassDeclFromTypeMemberReceiver(ctx, callee);
    if ( classNode == NULL ) {
        return NULL;
    }
    return xl6SemResolveStaticClassFuncEx(ctx, call, classNode, callee->text, report);
}

static XL6_AstNode* xl6SemCloneRecordFieldType(
    XL6_SemContext* ctx,
    const XL6_AstNode* receiverType,
    const char* fieldName)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* decl;
    const XL6_AstNode* field;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* typeArgs;

    concreteType = xl6SemClassValueType(ctx, receiverType);
    if ( concreteType == NULL || concreteType->text == NULL || fieldName == NULL ) {
        return NULL;
    }

    decl = xl6SemFindTypeDecl(ctx, concreteType->text);
    if ( decl == NULL ) {
        return NULL;
    }

    if ( decl->kind == XL6_AST_UNION ) {
        field = xl6SemFindUnionFieldNode(decl, fieldName);
        return field != NULL && field->childCount > 0 ? xl6SemCloneTypeNode(field->children[0]) : NULL;
    }

    if ( decl->kind != XL6_AST_CLASS ) {
        return NULL;
    }

    ownerClass = NULL;
    field = xl6SemFindClassFieldNode(ctx, decl, fieldName, &ownerClass);
    if ( field == NULL || field->childCount == 0 ) {
        return NULL;
    }
    if ( !xl6SemCanAccessClassMember(ctx, ownerClass, field) ) {
        return NULL;
    }

    typeArgs = (ownerClass == decl) ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
    if ( typeArgs != NULL && xl6SemClassHasTypeParams(ownerClass) ) {
        return xl6SemSubstituteClassType(ownerClass, typeArgs, field->children[0]);
    }
    return xl6SemCloneTypeNode(field->children[0]);
}

static const XL6_AstNode* xl6SemFindClassCallableMemberNode(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL || memberName == NULL ) {
        return NULL;
    }
    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member != NULL &&
                 (member->kind == XL6_AST_FUNC ||
                  member->kind == XL6_AST_ASYNC_FUNC ||
                  member->kind == XL6_AST_EXPORT_FUNC ||
                  member->kind == XL6_AST_CREATE ||
                  member->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
                  member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL) &&
                 xl6TextEquals(xl6SemMemberName(member), memberName) &&
                 xl6SemCanAccessClassMember(ctx, classNode, member) ) {
                return member;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6SemFindTypeDecl(ctx, base->text);
    return xl6SemFindClassCallableMemberNode(ctx, baseClass, memberName);
}

static int xl6SemClassHasCallableMember(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName)
{
    return xl6SemFindClassCallableMemberNode(ctx, classNode, memberName) != NULL;
}

static XL6_AstNode* xl6SemCloneProtocolReturnType(XL6_SemContext* ctx, const XL6_AstNode* receiverType, const char* protocolName)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* decl;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;

    concreteType = xl6SemClassValueType(ctx, receiverType);
    if ( concreteType == NULL || concreteType->text == NULL || protocolName == NULL ) {
        return NULL;
    }

    decl = xl6SemFindTypeDecl(ctx, concreteType->text);
    if ( decl == NULL || decl->kind != XL6_AST_CLASS ) {
        return NULL;
    }

    method = xl6SemFindClassCallableMemberNode(ctx, decl, protocolName);
    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return NULL;
    }
    return xl6SemCloneTypeNode(returns->children[0]);
}

static const XL6_AstNode* xl6SemProtocolParamType(const XL6_AstNode* method, size_t index)
{
    const XL6_AstNode* params;
    const XL6_AstNode* param;

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    if ( params == NULL || index >= params->childCount ) {
        return NULL;
    }
    param = params->children[index];
    return param != NULL && param->childCount > 0 ? param->children[0] : NULL;
}

static const XL6_AstNode* xl6SemFindProtocolMethodForExpr(XL6_SemContext* ctx, XL6_AstNode* expr, const char* protocolName)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* decl;
    const XL6_AstNode* method;

    if ( ctx == NULL || expr == NULL || protocolName == NULL ) {
        return NULL;
    }

    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    concreteType = xl6SemClassValueType(ctx, typeNode);
    decl = concreteType != NULL && concreteType->text != NULL ? xl6SemFindTypeDecl(ctx, concreteType->text) : NULL;
    method = decl != NULL && decl->kind == XL6_AST_CLASS ?
             xl6SemFindClassCallableMemberNode(ctx, decl, protocolName) :
             NULL;
    xl6AstFree(typeNode);
    return method;
}

static int xl6SemCheckProtocolArg(
    XL6_SemContext* ctx,
    XL6_AstNode* atNode,
    const XL6_AstNode* method,
    size_t paramIndex,
    XL6_AstNode* argExpr,
    const char* message)
{
    const XL6_AstNode* paramType;

    paramType = xl6SemProtocolParamType(method, paramIndex);
    if ( paramType == NULL || argExpr == NULL ) {
        return 1;
    }
    if ( xl6SemExprCanMatchParamType(ctx, argExpr, paramType) ) {
        return 1;
    }
    xl6SemTypeMismatchError(
        ctx,
        atNode != NULL ? atNode : argExpr,
        message,
        paramType,
        xl6SemExprStaticTypeName(ctx, argExpr));
    return 0;
}

static int xl6SemCheckProtocolCallArgs(XL6_SemContext* ctx, XL6_AstNode* call, const XL6_AstNode* method)
{
    const XL6_AstNode* params;
    XL6_AstNode* args;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int* provided;
    int positionalIndex;
    int seenNamed;
    size_t i;
    int ok;

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;

    if ( fixedCount > 0 && provided == NULL ) {
        xl6SemErrorNode(ctx, call, "协议调用检查内存不足");
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            XL6_AstNode* argExpr;
            int paramIndex;

            if ( arg == NULL ) {
                continue;
            }
            argExpr = arg->childCount > 0 ? arg->children[0] : NULL;
            if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个协议实参");
                ok = 0;
                continue;
            }

            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                seenNamed = 1;
                paramIndex = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( paramIndex >= 0 ) {
                    if ( provided[paramIndex] ) {
                        xl6SemErrorNode(ctx, arg, "重复传入同一个协议命名参数");
                        ok = 0;
                    } else {
                        provided[paramIndex] = 1;
                    }
                    if ( !xl6SemCheckProtocolArg(ctx, argExpr, method, (size_t)paramIndex, argExpr, "协议调用实参类型不匹配") ) {
                        ok = 0;
                    }
                } else if ( !hasDictVararg ) {
                    xl6SemErrorNode(ctx, arg, "未知协议命名参数");
                    ok = 0;
                }
                continue;
            }

            if ( seenNamed ) {
                xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "位置参数不能出现在命名参数之后");
                ok = 0;
                continue;
            }
            paramIndex = positionalIndex;
            positionalIndex += 1;
            if ( paramIndex < fixedCount ) {
                if ( provided[paramIndex] ) {
                    xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "参数已由命名参数传入");
                    ok = 0;
                } else {
                    provided[paramIndex] = 1;
                }
                if ( !xl6SemCheckProtocolArg(ctx, argExpr, method, (size_t)paramIndex, argExpr, "协议调用实参类型不匹配") ) {
                    ok = 0;
                }
            } else if ( !hasArrayVararg ) {
                xl6SemErrorNode(ctx, argExpr != NULL ? argExpr : arg, "协议调用实参数量超过声明");
                ok = 0;
            }
        }
    }

    for ( i = 0; i < (size_t)fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param != NULL && param->childCount < 2 ) {
            xl6SemErrorNode(ctx, call, "协议调用缺少必选参数");
            ok = 0;
            break;
        }
    }

    xrtFree(provided);
    return ok;
}
static XL6_AstNode* xl6SemCloneMemberFieldType(XL6_SemContext* ctx, XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* result;
    XL6_Symbol* sym;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }

    result = xl6SemCloneStaticClassFieldType(ctx, member);
    if ( result != NULL ) {
        return result;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL ) {
        return NULL;
    }

    receiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER ) {
        receiverType = xl6SemCloneMemberFieldType(ctx, receiver);
    } else if ( receiver->kind == XL6_AST_NAME ) {
        if ( xl6TextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
            receiverType = xl6AstNewText(XL6_AST_TYPE, ctx->currentClass->text, strlen(ctx->currentClass->text), NULL);
        } else {
            sym = xl6ScopeFind(ctx->scope, receiver->text);
            if ( sym != NULL && sym->type != NULL ) {
                receiverType = xl6SemCloneTypeNode(sym->type);
            }
        }
    } else {
        receiverType = xl6SemInferAutoTypeNode(ctx, receiver);
    }

    result = xl6SemCloneRecordFieldType(ctx, receiverType, member->text);
    if ( result == NULL && xl6TextEquals(member->text, "length") ) {
        result = xl6SemCloneProtocolReturnType(ctx, receiverType, "__len__");
    }
    if ( result == NULL ) {
        result = xl6SemClonePropertyGetterType(ctx, member, NULL, 0, 0);
    }
    if ( result == NULL ) {
        result = xl6SemCloneProtocolReturnType(ctx, receiverType, "__field__");
    }
    xl6AstFree(receiverType);
    return result;
}

static int xl6SemUnionContainsFieldName(const XL6_AstNode* unionNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( unionNode == NULL || name == NULL ) {
        return 0;
    }
    body = unionNode->childCount > 0 ? unionNode->children[0] : NULL;
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD && xl6TextEquals(field->text, name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemClassMemberExposesLayoutName(const XL6_AstNode* member, const char* name)
{
    if ( member == NULL || name == NULL ) {
        return 0;
    }
    if ( member->kind == XL6_AST_FIELD ) {
        return xl6TextEquals(member->text, name);
    }
    if ( member->kind == XL6_AST_UNION ) {
        if ( member->text != NULL ) {
            return xl6TextEquals(member->text, name);
        }
        return xl6SemUnionContainsFieldName(member, name);
    }
    return 0;
}

static int xl6SemCheckClassMemberConflicts(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    XL6_AstNode* body;
    size_t i;
    size_t j;
    int ok;
    int destroyCount;

    if ( typeNode->childCount < 2 ) {
        return 1;
    }

    body = typeNode->children[1];
    ok = 1;
    destroyCount = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        const char* name = xl6SemMemberName(member);

        if ( member != NULL &&
             (member->kind == XL6_AST_CREATE || member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL) &&
             xl6TextEquals(name, "destroy") ) {
            destroyCount += 1;
            if ( destroyCount > 1 ) {
                xl6SemErrorNode(ctx, member, "destroy can only be declared once");
                ok = 0;
            }
        }
        if ( member != NULL &&
             (member->kind == XL6_AST_FUNC ||
              member->kind == XL6_AST_ASYNC_FUNC ||
              member->kind == XL6_AST_EXPORT_FUNC ||
              member->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
             xl6SemIsSpecialName(name) ) {
            xl6SemErrorNode(ctx, member->children[0], "create/destroy must use the special declaration form");
            ok = 0;
        }
        for ( j = i + 1; j < body->childCount; ++j ) {
            XL6_AstNode* other = body->children[j];
            if ( xl6SemMembersShareOverloadGroup(member, other) &&
                 xl6SemOverloadShapeConflicts(ctx, member, other) ) {
                xl6SemErrorNode(ctx, other, "duplicate or ambiguous class member overload signature");
                ok = 0;
            }
            if ( member != NULL &&
                 other != NULL &&
                 xl6SemIsNativeBackedExternMember(member) &&
                 xl6SemIsNativeBackedExternMember(other) &&
                 xl6TextEquals(member->text, other->text) ) {
                xl6SemErrorNode(ctx, other, "same native-backed class cannot bind the same C symbol twice");
                ok = 0;
            }
        }

        if ( member != NULL &&
             (member->kind == XL6_AST_FIELD || (member->kind == XL6_AST_UNION && name != NULL)) &&
             name != NULL ) {
            for ( j = i + 1; j < body->childCount; ++j ) {
                XL6_AstNode* other = body->children[j];
                const char* otherName = xl6SemMemberName(other);
                if ( (otherName == NULL || !xl6TextEquals(name, otherName)) &&
                     !xl6SemClassMemberExposesLayoutName(other, name) ) {
                    continue;
                }
                xl6SemErrorNode(ctx, other, "class member layout name conflicts with an existing field or union");
                ok = 0;
            }
        }

        if ( xl6SemIsClassPropertyMember(member) && name != NULL ) {
            for ( j = i + 1; j < body->childCount; ++j ) {
                XL6_AstNode* other = body->children[j];
                if ( !xl6SemClassMemberExposesLayoutName(other, name) ) {
                    continue;
                }
                xl6SemErrorNode(ctx, other, "class property name conflicts with an existing field or union");
                ok = 0;
            }
        }

        if ( member != NULL && member->kind == XL6_AST_UNION && name == NULL && member->childCount > 0 ) {
            XL6_AstNode* unionBody = member->children[0];
            size_t k;
            for ( k = 0; unionBody != NULL && k < unionBody->childCount; ++k ) {
                XL6_AstNode* field = unionBody->children[k];
                const char* fieldName = field != NULL ? field->text : NULL;
                if ( fieldName == NULL ) {
                    continue;
                }
                for ( j = i + 1; j < body->childCount; ++j ) {
                    XL6_AstNode* other = body->children[j];
                    if ( !xl6SemClassMemberExposesLayoutName(other, fieldName) ) {
                        continue;
                    }
                    xl6SemErrorNode(ctx, other, "anonymous union field conflicts with an existing class member");
                    ok = 0;
                }
            }
        }
    }
    return ok;
}

static int xl6SemDeriveAbilityIndex(const char* name)
{
    static const char* abilities[] = {
        "copy",
        "clone",
        "moveOnly",
        "value",
        "json",
        "xson",
        "string",
        "hash",
        "equals"
    };
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < sizeof(abilities) / sizeof(abilities[0]); ++i ) {
        if ( xl6TextEquals(name, abilities[i]) ) {
            return (int)i + 1;
        }
    }
    return 0;
}

static int xl6SemCheckDeriveList(XL6_SemContext* ctx, XL6_AstNode* body)
{
    int seen[10];
    XL6_AstNode* copyNode;
    XL6_AstNode* cloneNode;
    XL6_AstNode* moveOnlyNode;
    size_t i;
    int ok;

    memset(seen, 0, sizeof(seen));
    copyNode = NULL;
    cloneNode = NULL;
    moveOnlyNode = NULL;
    ok = 1;

    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        int index;

        if ( member == NULL || member->kind != XL6_AST_DERIVE ) {
            continue;
        }
        index = xl6SemDeriveAbilityIndex(member->text);
        if ( index == 0 ) {
            xl6SemErrorNode(ctx, member, "未知 derive 能力");
            ok = 0;
            continue;
        }
        if ( seen[index] ) {
            xl6SemErrorNode(ctx, member, "閲嶅 derive 鑳藉姏澹版槑");
            ok = 0;
            continue;
        }
        seen[index] = 1;

        if ( xl6TextEquals(member->text, "copy") ) {
            copyNode = member;
        } else if ( xl6TextEquals(member->text, "clone") ) {
            cloneNode = member;
        } else if ( xl6TextEquals(member->text, "moveOnly") ) {
            moveOnlyNode = member;
        }
    }

    /*
        moveOnly 明确表达资源所有权不能复制。
        它和 copy/clone 同时出现会让生命周期语义互相矛盾。
    */
    if ( moveOnlyNode != NULL && (copyNode != NULL || cloneNode != NULL) ) {
        xl6SemErrorNode(ctx, moveOnlyNode, "derive moveOnly 不能与 derive copy 或 derive clone 同时使用");
        ok = 0;
    }

    return ok;
}

static int xl6SemCheckDeriveCopyFields(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* body)
{
    XL6_AstNode* base;
    size_t i;
    int ok;

    if ( classNode == NULL || body == NULL ||
         (!xl6SemClassDeclHasDerive(classNode, "copy") &&
          !xl6SemClassDeclHasDerive(classNode, "clone")) ) {
        return 1;
    }

    ok = 1;
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && !xl6SemTypeCanCopy(ctx, base) ) {
        xl6SemErrorNode(ctx, base, "derive copy/clone 的父类必须可复制");
        ok = 0;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        XL6_AstNode* fieldType;

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        if ( !xl6SemTypeCanCopy(ctx, fieldType) ) {
            xl6SemErrorNode(ctx, member, "derive copy/clone 的字段类型必须可复制");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckDeriveValueFields(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* body)
{
    XL6_AstNode* base;
    size_t i;
    int ok;

    if ( classNode == NULL || body == NULL || !xl6SemClassDeclHasValueDerive(classNode) ) {
        return 1;
    }

    ok = 1;
    if ( xl6SemClassHasTypeParams(classNode) ) {
        xl6SemErrorNode(ctx, classNode, "derive value/json/xson 当前不支持泛型 class");
        ok = 0;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME && !xl6SemTypeCanDeriveValue(ctx, base) ) {
        xl6SemErrorNode(ctx, base, "derive value/json/xson 的父类必须同样具备可持久化能力，object 除外");
        ok = 0;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        XL6_AstNode* fieldType;

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        if ( !xl6SemTypeCanDeriveValue(ctx, fieldType) ) {
            xl6SemErrorNode(ctx, member, "derive value/json/xson 字段必须是基础类型、动态 var/容器，或同样可持久化的 class");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemClassCreateCallableWithoutArgs(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasCreate;

    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return 1;
    }

    hasCreate = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* params;

        if ( member == NULL ||
             (member->kind != XL6_AST_CREATE && member->kind != XL6_AST_NATIVE_EXTERN_SPECIAL) ||
             !xl6TextEquals(xl6SemMemberName(member), "create") ) {
            continue;
        }

        hasCreate = 1;
        params = xl6SemCallableParams(member);
        if ( xl6SemParamRequiredCount(params) == 0 ) {
            return 1;
        }
    }
    return !hasCreate;
}

static int xl6SemNodeContainsBaseCtorCall(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6TextEquals(node->children[0]->text, "base") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6SemNodeContainsBaseCtorCall(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckClassBaseCreateRule(XL6_SemContext* ctx, XL6_AstNode* classNode, XL6_AstNode* baseDecl)
{
    XL6_AstNode* body;
    size_t i;
    int ok;
    int createCount;

    if ( classNode == NULL || baseDecl == NULL || xl6SemClassCreateCallableWithoutArgs(baseDecl) ) {
        return 1;
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    ok = 1;
    createCount = 0;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];

            if ( member == NULL ||
                 member->kind != XL6_AST_CREATE ||
                 !xl6TextEquals(xl6SemMemberName(member), "create") ) {
                continue;
            }

            createCount += 1;
            if ( !xl6SemNodeContainsBaseCtorCall(member->childCount > 1 ? member->children[1] : NULL) ) {
                xl6SemErrorNode(ctx, member, "父类 create 需要参数，子类 create 必须显式调用 base(...)");
                ok = 0;
            }
        }
    }

    if ( createCount == 0 ) {
        xl6SemErrorNode(ctx, classNode, "父类 create 需要参数，子类必须声明 create 并显式调用 base(...)");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckClass(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_TypeParamScope typeParamScope;
    XL6_AstNode* base;
    XL6_AstNode* body;
    XL6_AstNode* typeParams;
    XL6_AstNode* baseDecl;
    size_t i;
    int ok;
    int pushedTypeParams;
    const char* oldImplicitSelfName;
    XL6_AstNode* oldCurrentClass;
    int oldInClassCreate;

    ok = xl6SemCheckTypeDeclName(ctx, node);
    pushedTypeParams = 0;
    baseDecl = NULL;
    base = node->childCount > 0 ? node->children[0] : NULL;
    body = node->childCount > 1 ? node->children[1] : NULL;
    typeParams = node->childCount > 2 ? node->children[2] : NULL;

    if ( typeParams != NULL && typeParams->childCount > 0 ) {
        if ( !xl6SemCheckTypeParams(ctx, typeParams) ) {
            ok = 0;
        }
        if ( base != NULL && base->kind == XL6_AST_NAME ) {
            xl6SemErrorNode(ctx, base, "泛型 class 当前不支持继承");
            ok = 0;
        }
        xl6SemPushTypeParams(ctx, &typeParamScope, typeParams);
        pushedTypeParams = 1;
    }

    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        if ( xl6TextEquals(base->text, node->text) ) {
            xl6SemErrorNode(ctx, base, "class 不能继承自身");
            ok = 0;
        } else {
            baseDecl = xl6SemFindTypeDecl(ctx, base->text);
            if ( baseDecl == NULL ) {
                xl6SemErrorNode(ctx, base, "未知父类");
                ok = 0;
            } else if ( baseDecl->kind != XL6_AST_CLASS ) {
                xl6SemErrorNode(ctx, base, "class 只能继承 class");
                ok = 0;
            }
        }
    }

    if ( body != NULL ) {
        if ( !xl6SemCheckClassMemberConflicts(ctx, node) ) {
            ok = 0;
        }
        if ( !xl6SemCheckDeriveList(ctx, body) ) {
            ok = 0;
        }
        if ( !xl6SemCheckDeriveCopyFields(ctx, node, body) ) {
            ok = 0;
        }
        if ( !xl6SemCheckDeriveValueFields(ctx, node, body) ) {
            ok = 0;
        }
        if ( !xl6SemCheckInlineLayoutCycles(ctx, node, body) ) {
            ok = 0;
        }
        if ( !xl6SemCheckClassBaseCreateRule(ctx, node, baseDecl) ) {
            ok = 0;
        }
        if ( !xl6SemCheckClassOverrideRules(ctx, node, baseDecl) ) {
            ok = 0;
        }
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( !xl6SemCheckClassMemberFlags(ctx, member) ) {
                ok = 0;
            }
            if ( member->kind == XL6_AST_FIELD ) {
                if ( !xl6SemCheckField(ctx, member) ) {
                    ok = 0;
                }
            } else if ( member->kind == XL6_AST_DERIVE ) {
                continue;
            } else if ( member->kind == XL6_AST_UNION ) {
                oldCurrentClass = ctx->currentClass;
                ctx->currentClass = node;
                if ( !xl6SemCheckUnion(ctx, member) ) {
                    ok = 0;
                }
                ctx->currentClass = oldCurrentClass;
            } else if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
                if ( !xl6SemCheckClassExternSpecial(ctx, node, member) ) {
                    ok = 0;
                }
            } else if ( member->kind == XL6_AST_NATIVE_EXTERN_FUNC ) {
                if ( !xl6SemCheckClassExternFunc(ctx, node, member) ) {
                    ok = 0;
                }
                if ( !xl6SemCheckProtocolMember(ctx, member) ) {
                    ok = 0;
                }
            } else if ( xl6SemIsExternClassProperty(member) ) {
                if ( !xl6SemCheckClassExternProperty(ctx, node, member) ) {
                    ok = 0;
                }
            } else if ( member->kind == XL6_AST_FUNC ||
                        member->kind == XL6_AST_ASYNC_FUNC ||
                        member->kind == XL6_AST_EXPORT_FUNC ) {
                int memberOk;
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) ? NULL : "self";
                ctx->currentClass = node;
                ctx->inClassCreate = 0;
                memberOk = 1;
                if ( member->kind == XL6_AST_EXPORT_FUNC &&
                     !xl6SemCheckClassExportFuncBase(ctx, node, member) ) {
                    ok = 0;
                    memberOk = 0;
                }
                if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) &&
                     !xl6SemCheckNoSelfInStaticClassMember(ctx, member) ) {
                    ok = 0;
                    memberOk = 0;
                } else if ( memberOk && !xl6SemCheckFunc(ctx, member) ) {
                    ok = 0;
                }
                if ( !xl6SemCheckProtocolMember(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->inClassCreate = oldInClassCreate;
            } else if ( member->kind == XL6_AST_GET || member->kind == XL6_AST_LET ) {
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) ? NULL : "self";
                ctx->currentClass = node;
                ctx->inClassCreate = 0;
                if ( xl6SemAstHasFlag(member, XL6_AST_FLAG_STATIC) &&
                     !xl6SemCheckNoSelfInStaticClassMember(ctx, member) ) {
                    ok = 0;
                } else if ( !xl6SemCheckPropertyMember(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->inClassCreate = oldInClassCreate;
            } else if ( member->kind == XL6_AST_CREATE ) {
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = "self";
                ctx->currentClass = node;
                ctx->inClassCreate = xl6TextEquals(xl6SemMemberName(member), "create");
                if ( !xl6SemCheckClassSpecial(ctx, member) ) {
                    ok = 0;
                }
                if ( !xl6SemCheckProtocolMember(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->inClassCreate = oldInClassCreate;
            } else {
                xl6SemErrorNode(ctx, member, "class 中不支持该成员");
                ok = 0;
            }
        }
    }
    if ( pushedTypeParams ) {
        xl6SemPopTypeParams(ctx, &typeParamScope);
    }
    return ok;
}

static int xl6SemTypeAliasHasCycle(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const XL6_AstNode* current;
    int guard;

    if ( ctx == NULL || node == NULL || node->text == NULL || node->childCount == 0 ) {
        return 0;
    }
    current = node->children[0];
    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* next;
        if ( current == NULL || current->kind != XL6_AST_TYPE || current->text == NULL || current->childCount > 0 ) {
            return 0;
        }
        if ( xl6TextEquals(current->text, node->text) ) {
            return 1;
        }
        next = xl6SemAliasTargetType(ctx, current);
        if ( next == NULL ) {
            return 0;
        }
        current = next;
    }
    return 1;
}

static int xl6SemCheckTypeAlias(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* target;
    int ok;

    ok = 1;
    if ( node == NULL || node->text == NULL ) {
        xl6SemErrorNode(ctx, node, "type alias missing name");
        return 0;
    }
    target = node->childCount > 0 ? node->children[0] : NULL;
    if ( target == NULL ) {
        xl6SemErrorNode(ctx, node, "type alias missing target type");
        return 0;
    }
    if ( xl6SemTypeIsAuto(target) ) {
        xl6SemErrorNode(ctx, target, "typedef 不能指向 auto");
        ok = 0;
    } else if ( !xl6SemCheckType(ctx, target) ) {
        ok = 0;
    }
    if ( xl6SemTypeAliasHasCycle(ctx, node) ) {
        xl6SemErrorNode(ctx, node, "type alias cycle");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckBreakContinueStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;
    int level;

    ok = 1;
    level = 1;
    if ( ctx->loopDepth <= 0 ) {
        xl6SemErrorNode(ctx, node, "break/continue 必须位于循环体内");
        ok = 0;
    }
    if ( node->childCount > 0 ) {
        if ( !xl6SemCheckExpr(ctx, node->children[0]) ) {
            ok = 0;
        }
        if ( !xl6SemIntLiteralToInt(node->children[0], &level) ) {
            xl6SemErrorNode(ctx, node->children[0], "break/continue 层数必须是有效整数");
            ok = 0;
        } else if ( level <= 0 ) {
            xl6SemErrorNode(ctx, node->children[0], "break/continue 层数必须大于 0");
            ok = 0;
        } else if ( ctx->loopDepth > 0 && level > ctx->loopDepth ) {
            xl6SemErrorNode(ctx, node->children[0], "break/continue 层数超过当前循环深度");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemTypeIsClassValue(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_TypeInfo* typeInfo;

    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, concreteType->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS;
}

static int xl6SemTypeNameIsClassValue(XL6_SemContext* ctx, const char* typeName)
{
    const XL6_TypeInfo* typeInfo;

    if ( ctx == NULL || typeName == NULL ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeName);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_CLASS;
}

static int xl6SemAwaitOperandIsTaskAggregateTemporary(XL6_AstNode* node)
{
    XL6_AstNode* operand;
    const char* memberName;

    if ( node == NULL || node->kind != XL6_AST_AWAIT || node->childCount == 0 ) {
        return 0;
    }
    operand = node->children[0];
    if ( operand == NULL || operand->kind != XL6_AST_CALL ) {
        return 0;
    }
    if ( !xl6SemIsTaskNamespaceMemberCall(operand, &memberName) ) {
        return 0;
    }
    return xl6TextEquals(memberName, "all") || xl6TextEquals(memberName, "race");
}

static int xl6SemCheckAwaitExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* awaitType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( ctx->currentFunction != NULL &&
         ctx->currentFunction->kind != XL6_AST_ASYNC_FUNC &&
         !xl6SemFuncIsAsyncGenerator(ctx, ctx->currentFunction) ) {
        xl6SemErrorNode(ctx, node, "await can only be used at top level, inside async func, or inside async generator");
        ok = 0;
    }
    awaitType = xl6SemCloneAwaitResultType(ctx, node);
    if ( awaitType == NULL ) {
        xl6SemErrorNode(ctx, node, "await 只能用于 future<T> 或 task<T> 表达式");
        return 0;
    }
    xl6AstFree(awaitType);
    return ok;
}

static int xl6SemCheckYieldStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* yieldType;
    int ok;

    ok = 1;
    if ( ctx->generatorDepth <= 0 || ctx->currentReturns == NULL ||
         ctx->currentReturns->childCount != 1 ) {
        xl6SemErrorNode(ctx, node, "yield 必须位于返回 generator<T> 的函数体内");
        ok = 0;
    } else {
        yieldType = xl6SemCloneSingleGenericArgType(ctx, ctx->currentReturns->children[0], "generator");
        if ( yieldType == NULL ) {
            xl6SemErrorNode(ctx, ctx->currentReturns->children[0], "generator 必须写成 generator<T>");
            ok = 0;
        } else if ( node->childCount > 0 && !xl6SemExprCanMatchParamType(ctx, node->children[0], yieldType) ) {
            xl6SemTypeMismatchError(
                ctx,
                node->children[0],
                "yield 表达式类型和 generator<T> 元素类型不匹配",
                yieldType,
                xl6SemExprStaticTypeName(ctx, node->children[0]));
            ok = 0;
        }
        xl6AstFree(yieldType);
    }
    if ( !xl6SemCheckChildrenExpr(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckReturnMultiCallExpr(XL6_SemContext* ctx, XL6_AstNode* node, XL6_AstNode* expr)
{
    XL6_AstNode* ownedReturnType;
    XL6_AstNode* returns;
    size_t i;
    int ok;

    ok = 1;
    ownedReturnType = NULL;
    returns = xl6SemCallReturnTypesEx(ctx, expr, &ownedReturnType);
    if ( ctx->currentReturns == NULL || returns == NULL ) {
        xl6AstFree(ownedReturnType);
        return 0;
    }
    if ( returns->childCount != ctx->currentReturns->childCount ) {
        xl6SemCountMismatchError(
            ctx,
            node,
            "return 多返回值数量和函数声明不匹配",
            ctx->currentReturns->childCount,
            returns->childCount);
        ok = 0;
    }
    for ( i = 0; i < returns->childCount && i < ctx->currentReturns->childCount; ++i ) {
        if ( !xl6SemTypeCanAssignFromType(ctx, returns->children[i], ctx->currentReturns->children[i]) ) {
            xl6SemTypeMismatchError(
                ctx,
                expr,
                "return 多返回值类型和函数声明不匹配",
                ctx->currentReturns->children[i],
                returns->children[i] != NULL ? returns->children[i]->text : NULL);
            ok = 0;
        }
    }
    xl6AstFree(ownedReturnType);
    return ok;
}

static int xl6SemCheckReturnStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int expected;
    int actual;
    int ok;
    int i;

    ok = 1;
    if ( ctx->generatorDepth > 0 && node->childCount > 0 ) {
        xl6SemErrorNode(ctx, node, "generator 函数中的 return 不能携带返回值");
        ok = 0;
    }
    if ( ctx->generatorDepth > 0 ) {
        if ( !xl6SemCheckChildrenExpr(ctx, node) ) {
            ok = 0;
        }
        return ok;
    }
    if ( ctx->currentReturns == NULL ) {
        xl6SemErrorNode(ctx, node, "return 必须位于函数或构造/析构体内");
        ok = 0;
    } else if ( ctx->currentReturnTypesValid ) {
        expected = (int)ctx->currentReturns->childCount;
        actual = (int)node->childCount;
        if ( expected > 1 && actual == 1 && xl6SemExprIsMultiReturnCall(ctx, node->children[0]) ) {
            if ( !xl6SemCheckReturnMultiCallExpr(ctx, node, node->children[0]) ) {
                ok = 0;
            }
            expected = 0;
            actual = 0;
        }
        if ( expected != actual ) {
            xl6SemCountMismatchError(
                ctx,
                node,
                "return 返回值数量和函数声明不匹配",
                (size_t)expected,
                (size_t)actual);
            ok = 0;
        }
        for ( i = 0; i < expected && i < actual; ++i ) {
            if ( xl6SemExprIsMultiReturnCall(ctx, node->children[i]) ) {
                xl6SemErrorNode(ctx, node->children[i], "多返回值函数调用不能作为单个 return 表达式");
                ok = 0;
            } else if ( !xl6SemExprCanMatchParamType(ctx, node->children[i], ctx->currentReturns->children[i]) ) {
                xl6SemTypeMismatchError(
                    ctx,
                    node->children[i],
                    "return 表达式类型和函数返回类型不匹配",
                    ctx->currentReturns->children[i],
                    xl6SemExprStaticTypeName(ctx, node->children[i]));
                ok = 0;
            } else if ( xl6SemTypeRequiresFreshValue(ctx, ctx->currentReturns->children[i]) &&
                        !xl6SemExprCanMoveValueOnReturn(ctx, node->children[i], ctx->currentReturns->children[i]) &&
                        !xl6SemCheckCopyableExpr(
                            ctx,
                            node->children[i],
                            1,
                            "不可复制类型不能从已有值返回") ) {
                ok = 0;
            }
        }
    }
    if ( !xl6SemCheckChildrenExpr(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;

    if ( node == NULL ) {
        return 1;
    }

    ok = 1;
    switch ( node->kind ) {
    case XL6_AST_CLASS:
        return xl6SemCheckClass(ctx, node) && ok;
    case XL6_AST_UNION:
        return xl6SemCheckUnion(ctx, node) && ok;
    case XL6_AST_EXTERN_FUNC:
        return xl6SemCheckExternFunc(ctx, node) && ok;
    case XL6_AST_EXPORT_FUNC:
        return xl6SemCheckExportFunc(ctx, node) && ok;
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
        return xl6SemCheckFunc(ctx, node) && ok;
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        return xl6SemDeclareVar(ctx, node) && ok;
    case XL6_AST_MULTI_VAR_DECL:
        return xl6SemCheckMultiVarDecl(ctx, node) && ok;
    case XL6_AST_ASSIGN:
        return xl6SemCheckAssign(ctx, node) && ok;
    case XL6_AST_MULTI_ASSIGN:
        return xl6SemCheckMultiAssign(ctx, node) && ok;
    case XL6_AST_EXPR_STMT:
        return xl6SemCheckChildrenExpr(ctx, node) && ok;
    case XL6_AST_RETURN:
        return xl6SemCheckReturnStmt(ctx, node) && ok;
    case XL6_AST_YIELD:
        return xl6SemCheckYieldStmt(ctx, node) && ok;
    case XL6_AST_THROW:
        return xl6SemCheckThrowStmt(ctx, node) && ok;
    case XL6_AST_ASSERT:
        return xl6SemCheckAssertStmt(ctx, node) && ok;
    case XL6_AST_BREAK:
    case XL6_AST_CONTINUE:
        return xl6SemCheckBreakContinueStmt(ctx, node) && ok;
    case XL6_AST_IF:
        return xl6SemCheckIf(ctx, node) && ok;
    case XL6_AST_WHILE:
        return xl6SemCheckLoop(ctx, node) && ok;
    case XL6_AST_DO_WHILE:
        return xl6SemCheckDoWhile(ctx, node) && ok;
    case XL6_AST_FOR:
        return xl6SemCheckFor(ctx, node) && ok;
    case XL6_AST_TRY:
        return xl6SemCheckTryStmt(ctx, node) && ok;
    default:
        return xl6SemCheckExpr(ctx, node) && ok;
    }
}

static int xl6SemRegisterTypeDecls(XL6_SemContext* ctx)
{
    size_t i;
    int ok;

    ok = 1;
    if ( !xl6TypeRegistryAddBuiltins(&ctx->types) ) {
        xl6SemErrorNode(ctx, ctx->root, "初始化内置类型注册表失败");
        return 0;
    }

    for ( i = 0; ctx->root != NULL && i < ctx->root->childCount; ++i ) {
        XL6_AstNode* child = ctx->root->children[i];
        const XL6_TypeInfo* oldType;
        XL6_TypeKind typeKind;
        XL6_AstNode* typeParams;
        int allowGeneric;
        int genericArity;

        if ( !xl6SemIsTypeDecl(child) ) {
            continue;
        }
        if ( child->text == NULL ) {
            if ( child->kind == XL6_AST_UNION ) {
                /*
                    顶层 union 有专用诊断；这里不再追加泛化的类型声明缺名错误。
            */
                ok = 0;
                continue;
            }
            xl6SemErrorNode(ctx, child, "类型声明缺少名称");
            ok = 0;
            continue;
        }

        oldType = xl6TypeRegistryFind(&ctx->types, child->text);
        if ( oldType != NULL ) {
            if ( oldType->kind == XL6_TYPE_BUILTIN ) {
                xl6SemErrorNode(ctx, child, "自定义类型名不能和内置类型名重复");
            } else {
                xl6SemErrorNode(ctx, child, "重复声明自定义类型");
            }
            ok = 0;
            continue;
        }

        if ( child->kind == XL6_AST_TYPE_ALIAS || child->kind == XL6_AST_EXTERN_TYPE_ALIAS ) {
            typeKind = XL6_TYPE_ALIAS;
            allowGeneric = 0;
            genericArity = 0;
        } else if ( child->kind == XL6_AST_UNION ) {
            typeKind = XL6_TYPE_UNION;
            allowGeneric = 0;
            genericArity = 0;
        } else {
            typeKind = XL6_TYPE_CLASS;
            typeParams = child->childCount > 2 ? child->children[2] : NULL;
            allowGeneric = typeParams != NULL && typeParams->childCount > 0;
            genericArity = allowGeneric ? (int)typeParams->childCount : 0;
        }
        if ( !xl6TypeRegistryAdd(&ctx->types, child->text, typeKind, allowGeneric, genericArity, child) ) {
            xl6SemErrorNode(ctx, child, "注册自定义类型失败");
            ok = 0;
        }
    }
    return ok;
}

/*
 * class 在源码中直接写类型名；这里统一降为编译器私有句柄类型。
 * 私有表示继续复用已经稳定的 xvalue record 生命周期，但不会进入语言表面。
 */
static void xl6SemNormalizeClassReferenceTypes(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const XL6_TypeInfo* typeInfo;
    XL6_AstNode* target;
    XL6_AstNode* args;
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    /* 泛型约束保存的是类型身份，不是运行时值表示，必须保留源码类名。 */
    if ( node->kind == XL6_AST_GENERIC_CONSTRAINT ) {
        return;
    }
    /*
     * cgen/ABI 会在同一棵 AST 上再次执行语义分析。
     * 私有句柄一旦形成就不能继续进入目标类型，否则会被重复包装。
     */
    if ( node->kind == XL6_AST_TYPE &&
         (node->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 ) {
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6SemNormalizeClassReferenceTypes(ctx, node->children[i]);
    }
    if ( node->kind != XL6_AST_TYPE || node->text == NULL ||
         (node->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 ) {
        return;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, node->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS ) {
        return;
    }

    target = xl6AstNewText(XL6_AST_TYPE, node->text, strlen(node->text), NULL);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( target == NULL || args == NULL ) {
        xl6AstFree(target);
        xl6AstFree(args);
        xl6SemErrorNode(ctx, node, "class 引用类型规范化内存不足");
        return;
    }
    target->line = node->line;
    target->column = node->column;
    target->children = node->children;
    target->childCount = node->childCount;
    target->childCapacity = node->childCapacity;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    xrtFree(node->text);
    node->text = xrtCopyStr("__class_handle", 14);
    node->flags |= XL6_AST_FLAG_CLASS_HANDLE;
    xl6AstAddChild(args, target);
    xl6AstAddChild(node, args);
}

static int xl6SemIsTopLevelRuntimeStmt(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind != XL6_AST_IMPORT &&
           node->kind != XL6_AST_FUNC &&
           node->kind != XL6_AST_ASYNC_FUNC &&
           node->kind != XL6_AST_EXTERN_FUNC &&
           node->kind != XL6_AST_EXPORT_FUNC &&
           node->kind != XL6_AST_EXTERN_VAR &&
           node->kind != XL6_AST_EXPORT_VAR &&
           node->kind != XL6_AST_EXPORT_CONST &&
           node->kind != XL6_AST_VAR_DECL &&
           node->kind != XL6_AST_CONST_DECL &&
           node->kind != XL6_AST_TYPE_ALIAS &&
           node->kind != XL6_AST_EXTERN_TYPE_ALIAS &&
           node->kind != XL6_AST_UNION &&
           node->kind != XL6_AST_CLASS;
}

static int xl6SemCheckMainEntry(XL6_SemContext* ctx)
{
    size_t i;
    int ok;
    int mainCount;
    XL6_AstNode* mainNode;

    ok = 1;
    mainCount = 0;
    mainNode = NULL;

    if ( ctx == NULL || ctx->root == NULL ) {
        return 1;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* child = ctx->root->children[i];
        XL6_AstNode* nameNode;

        if ( xl6SemIsTopLevelRuntimeStmt(child) ) {
            continue;
        }
        if ( child == NULL ||
             (child->kind != XL6_AST_FUNC && child->kind != XL6_AST_ASYNC_FUNC &&
              child->kind != XL6_AST_EXTERN_FUNC && child->kind != XL6_AST_EXPORT_FUNC) ||
             child->childCount == 0 ) {
            continue;
        }
        nameNode = child->children[0];
        if ( nameNode == NULL || !xl6TextEquals(nameNode->text, "main") ) {
            continue;
        }

        mainCount += 1;
        if ( mainNode == NULL ) {
            mainNode = child;
        }
        if ( child->kind != XL6_AST_FUNC ) {
            xl6SemErrorNode(ctx, nameNode, "main 入口必须是普通 xlang 函数，不能是 extern 或 export");
            ok = 0;
        }
    }

    if ( mainCount > 1 ) {
        xl6SemErrorNode(ctx, mainNode, "一个编译目标只能声明一个 xlang main 函数");
        ok = 0;
    }
    if ( mainNode != NULL ) {
        XL6_AstNode* params = mainNode->childCount > 1 ? mainNode->children[1] : NULL;
        XL6_AstNode* returns = mainNode->childCount > 2 ? mainNode->children[2] : NULL;
        XL6_AstNode* typeParams = mainNode->childCount > 4 ? mainNode->children[4] : NULL;

        if ( typeParams != NULL && typeParams->childCount > 0 ) {
            xl6SemErrorNode(ctx, typeParams, "main 入口不能是泛型函数");
            ok = 0;
        }
        if ( params == NULL || params->childCount != 1 || !xl6SemParamIsArrayVararg(params->children[0]) ) {
            xl6SemErrorNode(ctx, mainNode, "main 入口参数必须是 ...，由 arg 接收 argc/argv");
            ok = 0;
        }
        if ( returns != NULL && returns->childCount > 1 ) {
            xl6SemErrorNode(ctx, returns, "main 入口不能返回多返回值");
            ok = 0;
        } else if ( returns != NULL && returns->childCount == 1 &&
                    (returns->children[0] == NULL || !xl6TextEquals(returns->children[0]->text, "int")) ) {
            xl6SemErrorNode(ctx, returns->children[0], "main 入口返回类型只能省略或声明为 int");
            ok = 0;
        }
    }

    return ok;
}

int xl6SemCheckModule(XL6_AstNode* root, XL6_DiagList* diags)
{
    XL6_SemContext ctx;
    XL6_Scope globalScope;
    size_t i;
    int ok;

    if ( root == NULL ) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.diags = diags;
    ctx.root = root;
    xl6TypeRegistryInit(&ctx.types);
    xl6SemPushScope(&ctx, &globalScope);

    ok = 1;
    if ( !xl6SemRegisterTypeDecls(&ctx) ) {
        ok = 0;
    }
    xl6SemNormalizeClassReferenceTypes(&ctx, root);
    xl6SemNormalizeGenericVarDecls(&ctx, root);
    if ( !xl6SemCheckOverloadConflicts(&ctx) ) {
        ok = 0;
    }
    if ( !xl6SemCheckExternCAbiConflicts(&ctx) ) {
        ok = 0;
    }
    if ( !xl6SemCheckMainEntry(&ctx) ) {
        ok = 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( (child->kind == XL6_AST_FUNC || child->kind == XL6_AST_ASYNC_FUNC ||
              child->kind == XL6_AST_EXTERN_FUNC || child->kind == XL6_AST_EXPORT_FUNC) && child->childCount > 0 ) {
            if ( !xl6SemDeclareGlobalFuncName(&ctx, child->children[0]) ) {
                ok = 0;
            }
        } else if ( xl6SemIsExternalVarDecl(child) && !xl6SemIsPackageExportVarDecl(child) ) {
            if ( !xl6SemDeclareExternalVar(&ctx, child) ) {
                ok = 0;
            }
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( xl6SemIsModuleGlobalDecl(child) && !xl6SemDeclareModuleGlobalVar(&ctx, child) ) {
            ok = 0;
        }
    }

    if ( !xl6SemCheckDocs(root, diags) ) {
        ok = 0;
    }

    for ( i = 0; i < root->childCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child->kind == XL6_AST_IMPORT || xl6SemIsFuncDecl(child) || xl6SemIsTypeDecl(child) ||
             xl6SemIsModuleGlobalDecl(child) ) {
            continue;
        } else if ( xl6SemIsExternalVarDecl(child) ) {
            if ( !xl6SemCheckExternalVar(&ctx, child) ) {
                ok = 0;
            }
        } else if ( !xl6SemCheckStmt(&ctx, child) ) {
            ok = 0;
        }
    }

    for ( i = 0; i < root->childCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child->kind == XL6_AST_FUNC || child->kind == XL6_AST_ASYNC_FUNC ) {
            if ( !xl6SemCheckFunc(&ctx, child) ) {
                ok = 0;
            }
        } else if ( child->kind == XL6_AST_EXPORT_FUNC ) {
            if ( !xl6SemCheckExportFunc(&ctx, child) ) {
                ok = 0;
            }
        } else if ( child->kind == XL6_AST_EXTERN_FUNC ) {
            if ( !xl6SemCheckExternFunc(&ctx, child) ) {
                ok = 0;
            }
        } else if ( child->kind == XL6_AST_CLASS ) {
            if ( !xl6SemCheckClass(&ctx, child) ) {
                ok = 0;
            }
        } else if ( child->kind == XL6_AST_UNION ) {
            if ( !xl6SemCheckUnion(&ctx, child) ) {
                ok = 0;
            }
        } else if ( child->kind == XL6_AST_TYPE_ALIAS || child->kind == XL6_AST_EXTERN_TYPE_ALIAS ) {
            if ( !xl6SemCheckTypeAlias(&ctx, child) ) {
                ok = 0;
            }
        }
    }

    xl6SemPopScope(&ctx, &globalScope);
    xl6TypeRegistryFree(&ctx.types);
    xl6SemFreeOwnedTypeNodes(&ctx);
    return ok && (diags == NULL || diags->count == 0);
}
