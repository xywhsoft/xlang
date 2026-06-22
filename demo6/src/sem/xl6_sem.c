#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_type_registry.h"

#include <limits.h>

typedef enum XL6_SymbolKind {
    XL6_SYM_VALUE,
    XL6_SYM_CONST,
    XL6_SYM_FUNC
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
    XL6_AstNode* root;
    XL6_TypeRegistry types;
    XL6_AstNode** ownedTypeNodes;
    size_t ownedTypeNodeCount;
    size_t ownedTypeNodeCapacity;
    XL6_Scope* captureBoundary;
    XL6_AstNode* currentClass;
    XL6_AstNode* currentCustom;
    XL6_AstNode* currentFunction;
    const char* implicitSelfName;
    int inClassCreate;
    int loopDepth;
    int catchDepth;
    int generatorDepth;
} XL6_SemContext;

static XL6_AstNode* xl6SemFindTypeDecl(XL6_SemContext* ctx, const char* name);
static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneAwaitResultType(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneAsyncCallResultType(XL6_SemContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs, const XL6_AstNode* at);
static const char* xl6SemStaticTypeNameFromTypeNode(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemCheckChannelTypeArg(XL6_SemContext* ctx, const XL6_AstNode* callee);
static int xl6SemChannelElementTypeIsFirstStageSafe(XL6_SemContext* ctx, const XL6_AstNode* typeNode);
static int xl6SemCheckChannelCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemChannelCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckThreadCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemThreadCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckSyncCall(XL6_SemContext* ctx, XL6_AstNode* node);
static const char* xl6SemSyncCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemIsSyncHandleTypeName(const char* name);
static int xl6SemIsThreadTypeName(const char* name);
static int xl6SemIsGeneratorTypeName(const char* name);
static int xl6SemIsResourceHandleTypeName(const char* name);
static const XL6_AstNode* xl6SemCallableParams(const XL6_AstNode* node);

static int xl6TextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static int xl6SemIsImplicitSelfTarget(XL6_SemContext* ctx, XL6_AstNode* node)
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
    memset(scope, 0, sizeof(*scope));
}

static XL6_Symbol* xl6ScopeFindLocal(XL6_Scope* scope, const char* name)
{
    size_t i;

    if ( (scope == NULL) || (name == NULL) ) {
        return NULL;
    }
    for ( i = 0; i < scope->count; ++i ) {
        if ( xl6TextEquals(scope->items[i].name, name) ) {
            return &scope->items[i];
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

    if ( scope->count == scope->capacity ) {
        newCapacity = (scope->capacity == 0) ? 8 : scope->capacity * 2;
        newItems = (XL6_Symbol*)xrtRealloc(scope->items, newCapacity * sizeof(XL6_Symbol));
        if ( newItems == NULL ) {
            xl6SemErrorNode(ctx, node, "符号表内存不�?");
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

static int xl6SemFindTypeParam(XL6_SemContext* ctx, const char* name)
{
    XL6_TypeParamScope* scope;
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( scope = ctx->typeParams; scope != NULL; scope = scope->parent ) {
        if ( scope->params == NULL ) {
            continue;
        }
        for ( i = 0; i < scope->params->childCount; ++i ) {
            XL6_AstNode* param = scope->params->children[i];
            if ( param != NULL && xl6TextEquals(param->text, name) ) {
                return 1;
            }
        }
    }
    return 0;
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
            xl6SemErrorNode(ctx, param, "泛型参数名不能和已有类型名重�?");
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
    }
    return ok;
}

static int xl6SemTypeIsAuto(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           typeNode->kind == XL6_AST_TYPE &&
           xl6TextEquals(typeNode->text, "auto");
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

    if ( xl6TextEquals(typeNode->text, "auto") ) {
        return 1;
    }
    if ( xl6TextEquals(typeNode->text, "array<var>") || xl6TextEquals(typeNode->text, "dict<var>") ) {
        return 1;
    }
    if ( xl6TextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        XL6_AstNode* params = typeNode->children[0];
        XL6_AstNode* returns = typeNode->children[1];
        int seenVararg = 0;

        ok = 1;
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                XL6_AstNode* paramType = params->children[i];
                if ( xl6SemTypeIsVararg(paramType) ) {
                    if ( seenVararg ) {
                        xl6SemErrorNode(ctx, paramType, "function 类型最多只能声明一个可变参�?");
                        ok = 0;
                    }
                    if ( i + 1 < params->childCount ) {
                        xl6SemErrorNode(ctx, paramType, "function 类型可变参数必须位于最�?");
                        ok = 0;
                    }
                    seenVararg = 1;
                } else if ( xl6SemTypeIsAuto(paramType) ) {
                    xl6SemErrorNode(ctx, paramType, "function 类型参数不能使用 auto");
                    ok = 0;
                } else if ( !xl6SemCheckType(ctx, paramType) ) {
                    ok = 0;
                }
            }
        }
        if ( returns != NULL ) {
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( xl6SemTypeIsAuto(returns->children[i]) ) {
                    xl6SemErrorNode(ctx, returns->children[i], "function 类型返回值不能使�?auto");
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
            xl6SemErrorNode(ctx, typeNode, "泛型参数不能再接收类型实�?");
            return 0;
        }
        return 1;
    }

    genericCount = 0;
    if ( typeNode->childCount > 0 && typeNode->children[0] != NULL && typeNode->children[0]->kind == XL6_AST_LIST ) {
        genericCount = (int)typeNode->children[0]->childCount;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo != NULL && typeInfo->kind != XL6_TYPE_BUILTIN ) {
        ok = 1;
        if ( typeInfo->kind == XL6_TYPE_CUSTOM && genericCount > 0 ) {
            xl6SemErrorNode(ctx, typeNode, "第一阶段 custom 类型不支持泛型参�?");
            ok = 0;
        } else if ( typeInfo->kind == XL6_TYPE_CLASS && typeInfo->allowGeneric ) {
            if ( genericCount != typeInfo->genericArity ) {
                xl6SemErrorNode(ctx, typeNode, "泛型 class 类型参数数量不匹�?");
                ok = 0;
            }
        } else if ( genericCount > 0 ) {
            xl6SemErrorNode(ctx, typeNode, "该类型不接受泛型参数");
            ok = 0;
        }
    for ( i = 0; i < typeNode->childCount; ++i ) {
            if ( !xl6SemCheckType(ctx, typeNode->children[i]) ) {
                ok = 0;
            }
        }
        return ok;
    }
    if ( typeInfo == NULL ) {
        xl6SemErrorNode(ctx, typeNode, "未知类型�?");
        return 0;
    }

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo != NULL && typeInfo->kind != XL6_TYPE_BUILTIN ) {
        if ( typeNode->childCount > 0 ) {
            xl6SemErrorNode(ctx, typeNode, "当前阶段 class/custom 类型暂不支持泛型参数");
            return 0;
        }
        return 1;
    }
    if ( typeInfo == NULL ) {
        xl6SemErrorNode(ctx, typeNode, "未知类型�?");
        return 0;
    }

    genericCount = 0;
    if ( typeNode->childCount > 0 && typeNode->children[0] != NULL && typeNode->children[0]->kind == XL6_AST_LIST ) {
        genericCount = (int)typeNode->children[0]->childCount;
    }

    ok = 1;
    if ( genericCount > 0 ) {
        if ( !typeInfo->allowGeneric ) {
            xl6SemErrorNode(ctx, typeNode, "该类型不接受泛型参数");
            ok = 0;
        } else if ( genericCount != typeInfo->genericArity ) {
            xl6SemErrorNode(ctx, typeNode, "泛型参数数量不匹�?");
            ok = 0;
        }
    }

    if ( xl6TextEquals(typeNode->text, "generator") ) {
        const XL6_AstNode* typeArgs = (typeNode->childCount > 0 && typeNode->children[0] != NULL &&
                                      typeNode->children[0]->kind == XL6_AST_LIST) ? typeNode->children[0] : NULL;
        const XL6_AstNode* itemType = typeArgs != NULL && typeArgs->childCount == 1 ? typeArgs->children[0] : NULL;
        if ( genericCount != 1 || itemType == NULL ) {
            xl6SemErrorNode(ctx, typeNode, "generator ����д�� generator<T>");
            ok = 0;
        } else if ( !xl6SemChannelElementTypeIsFirstStageSafe(ctx, itemType) ) {
            xl6SemErrorNode(ctx, itemType, "generator<T> ��һ��ֻ֧�� bool�����������㡢time��point �ȿɰ�ֵ���Ƶ�Ԫ������");
            ok = 0;
        }
    }
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( !xl6SemCheckType(ctx, typeNode->children[i]) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckStmt(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckChildrenExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckAnonFunc(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckAwaitExpr(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemCheckYieldStmt(XL6_SemContext* ctx, XL6_AstNode* node);
static int xl6SemTypeEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b);
static int xl6SemExprCanMatchFunctionType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode);
static int xl6SemFuncHasVararg(const XL6_AstNode* params);
static int xl6SemFixedParamCount(const XL6_AstNode* params);
static int xl6SemParamRequiredCount(const XL6_AstNode* params);
static XL6_AstNode* xl6SemFindFunctionValueFunc(XL6_SemContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode, int reportError);
static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneTypeNode(const XL6_AstNode* node);
static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneIndexedItemType(XL6_SemContext* ctx, XL6_AstNode* expr);
static XL6_AstNode* xl6SemCloneMemberFieldType(XL6_SemContext* ctx, XL6_AstNode* member);
static int xl6SemClassHasCallableMember(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName);
static const char* xl6SemMemberName(const XL6_AstNode* node);
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

static int xl6SemParamIsVararg(const XL6_AstNode* param)
{
    return xl6SemParamIsArrayVararg(param) || xl6SemParamIsDictVararg(param);
}

static int xl6SemParamVarargKind(const XL6_AstNode* param)
{
    if ( xl6SemParamIsArrayVararg(param) ) {
        return 1;
    }
    if ( xl6SemParamIsDictVararg(param) ) {
        return 2;
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
    }
    return 0;
}

static int xl6SemCustomDeclHasDestroy(const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( customNode == NULL || customNode->kind != XL6_AST_CUSTOM || customNode->childCount < 1 ) {
        return 0;
    }
    body = customNode->children[0];
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL &&
             member->childCount > 0 &&
             member->children[0] != NULL &&
             xl6TextEquals(member->children[0]->text, "destroy") ) {
            return 1;
        }
    }
    return 0;
}

static const XL6_AstNode* xl6SemClassTypeArgForParam(const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const char* name)
{
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    size_t i;

    if ( classNode == NULL || typeNode == NULL || name == NULL || classNode->childCount < 3 ||
         typeNode->childCount == 0 ) {
        return NULL;
    }
    params = classNode->children[2];
    args = typeNode->children[0];
    if ( params == NULL || args == NULL || params->kind != XL6_AST_LIST || args->kind != XL6_AST_LIST ) {
        return NULL;
    }
    for ( i = 0; i < params->childCount && i < args->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6TextEquals(param->text, name) ) {
            return args->children[i];
        }
    }
    return NULL;
}

static int xl6SemTypeCanCaptureDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* decl;
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    size_t i;

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
    if ( typeInfo->kind == XL6_TYPE_CUSTOM ) {
        return 0;
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS ) {
        return 1;
    }

    decl = typeInfo->decl;
    if ( decl == NULL ) {
        return 1;
    }
    if ( xl6SemClassDeclHasDestroy(decl) ) {
        return 0;
    }
    base = decl->childCount > 0 ? decl->children[0] : NULL;
    if ( base != NULL && !xl6SemTypeCanCaptureDepth(ctx, base, depth + 1) ) {
        return 0;
    }

    body = decl->childCount > 1 ? decl->children[1] : NULL;
    if ( body == NULL ) {
        return 1;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* concreteType;

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        concreteType = fieldType;
        if ( fieldType != NULL ) {
            const XL6_AstNode* typeArg = xl6SemClassTypeArgForParam(decl, typeNode, fieldType->text);
            if ( typeArg != NULL ) {
                concreteType = typeArg;
            }
        }
        if ( !xl6SemTypeCanCaptureDepth(ctx, concreteType, depth + 1) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemTypeCanCapture(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanCaptureDepth(ctx, typeNode, 0);
}

static int xl6SemTypeCanCopyDepth(XL6_SemContext* ctx, const XL6_AstNode* typeNode, int depth)
{
    const XL6_TypeInfo* typeInfo;
    const XL6_AstNode* decl;
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    size_t i;

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
    if ( typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }
    if ( typeInfo->kind == XL6_TYPE_CUSTOM ) {
        return !xl6SemCustomDeclHasDestroy(typeInfo->decl);
    }
    if ( typeInfo->kind != XL6_TYPE_CLASS ) {
        return 1;
    }

    decl = typeInfo->decl;
    if ( decl == NULL ) {
        return 1;
    }
    if ( xl6SemClassDeclHasDestroy(decl) ) {
        return 0;
    }
    base = decl->childCount > 0 ? decl->children[0] : NULL;
    if ( base != NULL && !xl6SemTypeCanCopyDepth(ctx, base, depth + 1) ) {
        return 0;
    }

    body = decl->childCount > 1 ? decl->children[1] : NULL;
    if ( body == NULL ) {
        return 1;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* concreteType;

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        concreteType = fieldType;
        if ( fieldType != NULL ) {
            const XL6_AstNode* typeArg = xl6SemClassTypeArgForParam(decl, typeNode, fieldType->text);
            if ( typeArg != NULL ) {
                concreteType = typeArg;
            }
        }
        if ( !xl6SemTypeCanCopyDepth(ctx, concreteType, depth + 1) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemTypeCanCopy(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6SemTypeCanCopyDepth(ctx, typeNode, 0);
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

    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_BUILTIN && xl6SemIsResourceHandleTypeName(typeNode->text) ) {
        return 0;
    }

    /*
        第二阶段开始，function/class/custom 都由 XRT �?callable/record/handle 盒子承载�?        class/custom 仍必须满足现有可复制性规则，避免装箱导致双析构或悬空资源�?    */
    if ( typeInfo->kind == XL6_TYPE_CLASS || typeInfo->kind == XL6_TYPE_CUSTOM ) {
        return xl6SemTypeCanCopy(ctx, typeNode);
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

static int xl6SemIsBoolTypeName(const char* name)
{
    return xl6TextEquals(name, "bool") || xl6TextEquals(name, "c_bool");
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

static int xl6SemIsBuiltinConvertTypeName(const char* name)
{
    return xl6SemIsIntegerTypeName(name) || xl6SemIsFloatTypeName(name) ||
           xl6SemIsBoolTypeName(name) || xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "time") || xl6TextEquals(name, "point") ||
           xl6TextEquals(name, "var");
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
               xl6TextEquals(source, "time") || xl6TextEquals(source, "point");
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
        xl6SemErrorNode(ctx, node, "泛型 class 声明归一化内存不�?");
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
    if ( p[0] == '0' && (p[1] == 'x' || p[1] == 'X') ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '&' && (p[1] == 'h' || p[1] == 'H') ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && (p[1] == 'o' || p[1] == 'O') ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '&' && (p[1] == 'o' || p[1] == 'O') ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && (p[1] == 'b' || p[1] == 'B') ) {
        base = 2;
        p += 2;
    } else if ( p[0] == '&' && (p[1] == 'b' || p[1] == 'B') ) {
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
        xl6SemErrorNode(ctx, node, "range 参数数量必须�?1�? �?3");
        ok = 0;
    }
    if ( args->childCount >= 3 ) {
        XL6_AstNode* stepArg = args->children[2];
        if ( stepArg != NULL && stepArg->childCount > 0 && xl6SemIntLiteralIsZero(stepArg->children[0]) ) {
            xl6SemErrorNode(ctx, stepArg->children[0], "range step 不能�?0");
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

static int xl6SemIsDocumentableNode(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_FUNC ||
            node->kind == XL6_AST_ASYNC_FUNC ||
            node->kind == XL6_AST_EXTERN_FUNC ||
            node->kind == XL6_AST_EXPORT_FUNC ||
            node->kind == XL6_AST_EXTERN_VAR ||
            node->kind == XL6_AST_EXPORT_VAR ||
            node->kind == XL6_AST_EXPORT_CONST ||
            node->kind == XL6_AST_TYPE_ALIAS ||
            node->kind == XL6_AST_EXTERN_TYPE_ALIAS ||
            node->kind == XL6_AST_CLASS ||
            node->kind == XL6_AST_CUSTOM ||
            node->kind == XL6_AST_UNION ||
            node->kind == XL6_AST_FIELD ||
            node->kind == XL6_AST_CREATE ||
            node->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ||
            node->kind == XL6_AST_CUSTOM_EXTERN_FUNC ||
            node->kind == XL6_AST_VAR_DECL ||
            node->kind == XL6_AST_CONST_DECL);
}

typedef struct XL6_SemDocSeenField {
    const char* kind;
    const char* name;
    size_t nameLen;
} XL6_SemDocSeenField;

static size_t xl6SemDocTrimEndLen(const char* text, size_t len)
{
    while ( len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n' || text[len - 1] == ' ' || text[len - 1] == '\t') ) {
        len -= 1;
    }
    return len;
}

static int xl6SemDocLineIsBlank(const char* text, size_t len)
{
    size_t i;

    len = xl6SemDocTrimEndLen(text, len);
    for ( i = 0; i < len; ++i ) {
        if ( text[i] != ' ' && text[i] != '\t' ) {
            return 0;
        }
    }
    return 1;
}

static int xl6SemDocLineIsMarker(const char* text, size_t len, const char* marker)
{
    size_t markerLen;

    len = xl6SemDocTrimEndLen(text, len);
    while ( len > 0 && (*text == ' ' || *text == '\t') ) {
        text += 1;
        len -= 1;
    }
    markerLen = strlen(marker);
    return len == markerLen && strncmp(text, marker, markerLen) == 0;
}

static size_t xl6SemDocLeadingIndent(const char* text, size_t len)
{
    size_t indent;

    indent = 0;
    while ( indent < len && (text[indent] == ' ' || text[indent] == '\t') ) {
        indent += 1;
    }
    return indent;
}

static void xl6SemDocStripBaseIndent(const char** text, size_t* len, size_t baseIndent)
{
    size_t removed;

    if ( text == NULL || *text == NULL || len == NULL || baseIndent == 0 ) {
        return;
    }
    removed = 0;
    while ( removed < baseIndent && *len > 0 && ((*text)[0] == ' ' || (*text)[0] == '\t') ) {
        *text += 1;
        *len -= 1;
        removed += 1;
    }
}

static size_t xl6SemDocBaseIndent(const char* doc)
{
    const char* p;

    if ( doc == NULL ) {
        return 0;
    }
    p = doc;
    while ( *p != '\0' ) {
        const char* line = p;
        size_t lineLen;

        while ( *p != '\0' && *p != '\r' && *p != '\n' ) {
            p += 1;
        }
        lineLen = (size_t)(p - line);
        if ( *p == '\r' ) {
            p += 1;
            if ( *p == '\n' ) {
                p += 1;
            }
        } else if ( *p == '\n' ) {
            p += 1;
        }
        if ( xl6SemDocLineIsBlank(line, lineLen) ) {
            continue;
        }
        if ( xl6SemDocLineIsMarker(line, lineLen, "#doc") ) {
            return xl6SemDocLeadingIndent(line, lineLen);
        }
        return 0;
    }
    return 0;
}
static int xl6SemDocParseHeader(
    const char* text,
    size_t len,
    const char** outKind,
    const char** outName,
    size_t* outNameLen)
{
    const char* colon;
    size_t headLen;

    if ( text == NULL || outKind == NULL || outName == NULL || outNameLen == NULL ) {
        return 0;
    }
    len = xl6SemDocTrimEndLen(text, len);
    if ( len == 0 || text[0] == ' ' || text[0] == '\t' ) {
        return 0;
    }

    colon = (const char*)memchr(text, ':', len);
    if ( colon == NULL ) {
        return 0;
    }
    headLen = (size_t)(colon - text);
    *outKind = NULL;
    *outName = "";
    *outNameLen = 0;

    if ( headLen == 4 && strncmp(text, "desc", 4) == 0 ) {
        *outKind = "desc";
    } else if ( headLen == 4 && strncmp(text, "note", 4) == 0 ) {
        *outKind = "note";
    } else if ( headLen == 7 && strncmp(text, "example", 7) == 0 ) {
        *outKind = "example";
    } else if ( headLen > 6 && strncmp(text, "param ", 6) == 0 ) {
        *outKind = "param";
        *outName = text + 6;
        *outNameLen = headLen - 6;
    } else if ( headLen > 7 && strncmp(text, "return ", 7) == 0 ) {
        *outKind = "return";
        *outName = text + 7;
        *outNameLen = headLen - 7;
    }
    return *outKind != NULL;
}

static int xl6SemDocRangeEquals(const char* text, size_t len, const char* value)
{
    size_t valueLen;

    if ( text == NULL || value == NULL ) {
        return 0;
    }
    valueLen = strlen(value);
    return len == valueLen && strncmp(text, value, len) == 0;
}

static int xl6SemDocSeenContains(
    XL6_SemDocSeenField* fields,
    size_t fieldCount,
    const char* kind,
    const char* name,
    size_t nameLen)
{
    size_t i;

    for ( i = 0; i < fieldCount; ++i ) {
        if ( fields[i].kind != NULL &&
             strcmp(fields[i].kind, kind) == 0 &&
             fields[i].nameLen == nameLen &&
             strncmp(fields[i].name, name, nameLen) == 0 ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemDocAddSeen(
    XL6_SemContext* ctx,
    const XL6_AstNode* node,
    XL6_SemDocSeenField** fields,
    size_t* fieldCount,
    size_t* fieldCapacity,
    const char* kind,
    const char* name,
    size_t nameLen)
{
    XL6_SemDocSeenField* newFields;
    size_t newCapacity;

    if ( fields == NULL || fieldCount == NULL || fieldCapacity == NULL ) {
        return 0;
    }
    if ( *fieldCount == *fieldCapacity ) {
        newCapacity = *fieldCapacity == 0 ? 8 : (*fieldCapacity * 2);
        newFields = (XL6_SemDocSeenField*)xrtRealloc(*fields, newCapacity * sizeof(XL6_SemDocSeenField));
        if ( newFields == NULL ) {
            xl6SemErrorNode(ctx, node, "#doc 字段检查内存不�?");
            return 0;
        }
        *fields = newFields;
        *fieldCapacity = newCapacity;
    }
    (*fields)[*fieldCount].kind = kind;
    (*fields)[*fieldCount].name = name;
    (*fields)[*fieldCount].nameLen = nameLen;
    *fieldCount += 1;
    return 1;
}

static const XL6_AstNode* xl6SemDocParamsForNode(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_CUSTOM_EXTERN_FUNC:
        return node->childCount > 1 ? node->children[1] : NULL;
    case XL6_AST_CREATE:
        return node->childCount > 0 ? node->children[0] : NULL;
    case XL6_AST_CUSTOM_EXTERN_SPECIAL:
        return node->childCount > 1 ? node->children[1] : NULL;
    default:
        return NULL;
    }
}

static const XL6_AstNode* xl6SemDocReturnsForNode(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_CUSTOM_EXTERN_FUNC:
        return node->childCount > 2 ? node->children[2] : NULL;
    default:
        return NULL;
    }
}

static int xl6SemDocParamExists(const XL6_AstNode* params, const char* name, size_t nameLen)
{
    size_t i;

    if ( params == NULL ) {
        return 0;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        if ( param == NULL ) {
            continue;
        }
        if ( xl6SemDocRangeEquals(name, nameLen, "...") &&
             typeNode != NULL && xl6TextEquals(typeNode->text, "array<var>") ) {
            return 1;
        }
        if ( xl6SemDocRangeEquals(name, nameLen, "***") &&
             typeNode != NULL && xl6TextEquals(typeNode->text, "dict<var>") ) {
            return 1;
        }
        if ( param->text != NULL && xl6SemDocRangeEquals(name, nameLen, param->text) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckDocField(
    XL6_SemContext* ctx,
    XL6_AstNode* node,
    XL6_SemDocSeenField** fields,
    size_t* fieldCount,
    size_t* fieldCapacity,
    const char* kind,
    const char* name,
    size_t nameLen)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int ok;

    ok = 1;
    if ( xl6TextEquals(kind, "desc") && xl6SemDocSeenContains(*fields, *fieldCount, kind, "", 0) ) {
        xl6SemErrorNode(ctx, node, "#doc desc 字段不能重复");
        ok = 0;
    } else if ( (xl6TextEquals(kind, "param") || xl6TextEquals(kind, "return")) &&
                xl6SemDocSeenContains(*fields, *fieldCount, kind, name, nameLen) ) {
        xl6SemErrorNode(ctx, node, "#doc param/return 字段不能重复描述同一个名�?");
        ok = 0;
    }

    if ( xl6TextEquals(kind, "param") ) {
        params = xl6SemDocParamsForNode(node);
        if ( nameLen == 0 ) {
            xl6SemErrorNode(ctx, node, "#doc param 字段缺少参数�?");
            ok = 0;
        } else if ( !xl6SemDocParamExists(params, name, nameLen) ) {
            xl6SemErrorNode(ctx, node, "#doc param 字段必须对应目标声明中的参数");
            ok = 0;
        }
    } else if ( xl6TextEquals(kind, "return") ) {
        returns = xl6SemDocReturnsForNode(node);
        if ( nameLen == 0 ) {
            xl6SemErrorNode(ctx, node, "#doc return 字段缺少返回值名");
            ok = 0;
        } else if ( returns == NULL || returns->childCount == 0 ) {
            xl6SemErrorNode(ctx, node, "#doc return 字段只能用于有返回值的函数声明");
            ok = 0;
        }
    }

    if ( !xl6SemDocAddSeen(ctx, node, fields, fieldCount, fieldCapacity, kind, name, nameLen) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckDocTarget(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* p;
    const char* line;
    const char* kind;
    const char* name;
    const char* parseLine;
    size_t lineLen;
    size_t parseLineLen;
    size_t nameLen;
    XL6_SemDocSeenField* fields;
    size_t fieldCount;
    size_t fieldCapacity;
    int ok;
    int seenField;
    size_t baseIndent;

    if ( node == NULL || node->doc == NULL || node->doc[0] == '\0' ) {
        return 1;
    }
    ok = 1;
    if ( !xl6SemIsDocumentableNode(node) ) {
        xl6SemErrorNode(ctx, node, "#doc 只能绑定到函数、类型、变量、常量、字段、extern/export 声明");
        ok = 0;
    }

    fields = NULL;
    fieldCount = 0;
    fieldCapacity = 0;
    seenField = 0;
    baseIndent = xl6SemDocBaseIndent(node->doc);
    p = node->doc;

    while ( *p != '\0' ) {
        line = p;
        while ( *p != '\0' && *p != '\r' && *p != '\n' ) {
            p += 1;
        }
        lineLen = (size_t)(p - line);
        if ( *p == '\r' ) {
            p += 1;
            if ( *p == '\n' ) {
                p += 1;
            }
        } else if ( *p == '\n' ) {
            p += 1;
        }

        parseLine = line;
        parseLineLen = lineLen;
        xl6SemDocStripBaseIndent(&parseLine, &parseLineLen, baseIndent);

        if ( xl6SemDocLineIsMarker(parseLine, parseLineLen, "#doc") ||
             xl6SemDocLineIsMarker(parseLine, parseLineLen, "#end") ||
             xl6SemDocLineIsBlank(parseLine, parseLineLen) ) {
            continue;
        }

        if ( xl6SemDocParseHeader(parseLine, parseLineLen, &kind, &name, &nameLen) ) {
            seenField = 1;
            if ( !xl6SemCheckDocField(ctx, node, &fields, &fieldCount, &fieldCapacity, kind, name, nameLen) ) {
                ok = 0;
            }
            continue;
        }

        if ( parseLineLen > 0 && (parseLine[0] == ' ' || parseLine[0] == '\t') ) {
            if ( !seenField ) {
                xl6SemErrorNode(ctx, node, "#doc 正文必须位于字段头之�?");
                ok = 0;
            }
            continue;
        }

        xl6SemErrorNode(ctx, node, "#doc 存在未知字段或未归属字段的裸文本");
        ok = 0;
    }

    if ( fieldCount == 0 ) {
        xl6SemErrorNode(ctx, node, "#doc 至少需要一个字�?");
        ok = 0;
    }
    xrtFree(fields);
    return ok;
}

static int xl6SemCheckDocsRecursive(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }
    ok = xl6SemCheckDocTarget(ctx, node);
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6SemCheckDocsRecursive(ctx, node->children[i]) ) {
            ok = 0;
        }
    }
    return ok;
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

static const char* xl6SemLiteralExprTypeName(const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return NULL;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT: return "int";
    case XL6_AST_FLOAT: return "float";
    case XL6_AST_STRING: return "string";
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

static int xl6SemExprParamMatchCost(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_AstNode* exprTypeNode;
    const char* exprType;
    const char* targetType;

    if ( typeNode == NULL ) {
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

    exprTypeNode = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
    if ( exprTypeNode != NULL ) {
        if ( xl6SemTypeEquals(ctx, exprTypeNode, typeNode) ) {
            xl6AstFree(exprTypeNode);
            return 0;
        }
        xl6AstFree(exprTypeNode);
    }

    if ( exprType == NULL ) {
        return 0;
    }
    if ( !xl6SemNameIsRegisteredType(ctx, exprType) && xl6SemFindTypeParam(ctx, exprType) == NULL ) {
        return 0;
    }

    if ( xl6TextEquals(exprType, targetType) ) {
        return 0;
    }

    if ( expr != NULL && expr->kind == XL6_AST_INT && xl6SemIsIntegerTypeName(targetType) ) {
        return 1;
    }
    if ( expr != NULL && expr->kind == XL6_AST_FLOAT && xl6SemIsFloatTypeName(targetType) ) {
        return 2;
    }

    if ( xl6SemIsBoolTypeName(targetType) && xl6SemIsBoolTypeName(exprType) ) {
        return 10;
    }
    if ( xl6SemIsIntegerTypeName(targetType) &&
         (xl6SemIsIntegerTypeName(exprType) || xl6SemIsBoolTypeName(exprType)) ) {
        return 10;
    }
    if ( xl6SemIsFloatTypeName(targetType) &&
         (xl6SemIsIntegerTypeName(exprType) || xl6SemIsFloatTypeName(exprType) || xl6SemIsBoolTypeName(exprType)) ) {
        return xl6SemIsIntegerTypeName(exprType) || xl6SemIsBoolTypeName(exprType) ? 20 : 10;
    }
    return -1;
}

static int xl6SemExprCanMatchParamType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    return xl6SemExprParamMatchCost(ctx, expr, typeNode) >= 0;
}

static int xl6SemExprCanMatchGenericParamType(
    XL6_SemContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes)
{
    int index;
    XL6_AstNode* exprType;
    int ok;

    if ( typeNode == NULL ) {
        return 1;
    }

    if ( xl6SemTypeContainsTypeParam(typeParams, typeNode) ) {
        exprType = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
        if ( exprType == NULL ) {
            return xl6SemExprCanMatchParamType(ctx, expr, typeNode);
        }
        ok = xl6SemInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, typeNode, exprType);
        xl6AstFree(exprType);
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
    XL6_AstNode* exprType;
    int ok;

    if ( typeNode == NULL ) {
        return 0;
    }

    if ( xl6SemTypeContainsTypeParam(typeParams, typeNode) ) {
        exprType = xl6SemInferAutoTypeNode(ctx, (XL6_AstNode*)expr);
        if ( exprType == NULL ) {
            return xl6SemExprParamMatchCost(ctx, expr, typeNode);
        }
        ok = xl6SemInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, typeNode, exprType);
        xl6AstFree(exprType);
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
                    argCost = xl6SemExprGenericParamMatchCost(ctx, argExpr, paramType, typeParams, inferredTypes);
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
                    argCost = xl6SemExprGenericParamMatchCost(ctx, argExpr, paramType, typeParams, inferredTypes);
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg ) {
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

    params = (XL6_AstNode*)xl6SemCallableParams(func);
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6SemFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6SemParamIsDictVararg(params->children[fixedCount]);
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
                    argCost = xl6SemExprExplicitGenericParamMatchCost(ctx, argExpr, paramType, typeParams, typeArgs);
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
                    argCost = xl6SemExprExplicitGenericParamMatchCost(ctx, argExpr, paramType, typeParams, typeArgs);
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg ) {
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

static XL6_AstNode* xl6SemResolveCallFunc(XL6_SemContext* ctx, XL6_AstNode* call, const char* name)
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
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( xl6SemIsFuncDecl(node) && xl6TextEquals(xl6SemFuncName(node), name) ) {
            int cost;
            candidateCount += 1;
            if ( xl6SemCallCandidateMatches(ctx, call, node, &cost) ) {
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
        xl6SemErrorNode(ctx, call, "函数调用存在重载歧义");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( varargBestCount > 1 ) {
        xl6SemErrorNode(ctx, call, "函数调用存在重载歧义");
    } else if ( candidateCount > 0 ) {
        xl6SemErrorNode(ctx, call, "没有匹配的函数重�?");
    }
    return NULL;
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
            xl6SemErrorNode(ctx, call, "没有匹配的显式泛型函数重�?");
        }
    }
    return NULL;
}

static int xl6SemCallArgCount(XL6_AstNode* call)
{
    XL6_AstNode* args;

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
    size_t i;

    body = NULL;
    if ( typeDecl != NULL && typeDecl->kind == XL6_AST_CLASS ) {
        body = typeDecl->childCount > 1 ? typeDecl->children[1] : NULL;
    } else if ( typeDecl != NULL && typeDecl->kind == XL6_AST_CUSTOM ) {
        body = typeDecl->childCount > 0 ? typeDecl->children[0] : NULL;
    }

    fixedSelected = NULL;
    varargSelected = NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    if ( body == NULL ) {
        return NULL;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];
        const XL6_AstNode* params;
        const char* name;

        if ( member == NULL || member->kind != memberKind ) {
            continue;
        }
        name = xl6SemMemberName(member);
        if ( !xl6TextEquals(name, "create") ) {
            continue;
        }
        {
            int cost;

            candidateCount += 1;
            if ( !xl6SemCallCandidateMatches(ctx, call, member, &cost) ) {
                continue;
            }
            params = xl6SemCallableParams(member);
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
        xl6SemErrorNode(ctx, call, "构造调用存在重载歧�?");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargSelected;
    }
    if ( varargBestCount > 1 ) {
        xl6SemErrorNode(ctx, call, "构造调用存在重载歧�?");
    } else if ( candidateCount > 0 ) {
        xl6SemErrorNode(ctx, call, "没有匹配�?create 重载");
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
        xl6SemErrorNode(ctx, call, "构造调用存在重载歧�?");
    } else if ( candidateCount > 0 ) {
        xl6SemErrorNode(ctx, call, "没有匹配�?create 重载");
    }
    return NULL;
}

static int xl6SemCheckClassCtorCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* classNode)
{
    const XL6_AstNode* typeArgs;

    if ( classNode == NULL ) {
        return 1;
    }

    if ( xl6SemClassHasTypeParams(classNode) ) {
        typeArgs = xl6SemCtorCallTypeArgs(call);
        if ( typeArgs == NULL ) {
            xl6SemErrorNode(ctx, call, "开放泛�?class 必须显式实例化后构�?");
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

    if ( xl6SemResolveCreateMember(ctx, call, classNode, XL6_AST_CREATE) != NULL ) {
        return 1;
    }
    if ( xl6SemCallArgCount(call) == 0 ) {
        return 1;
    }
    xl6SemErrorNode(ctx, call, "class 没有匹配�?create 构�?");
    return 0;
}

static int xl6SemCustomHasCreate(const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL &&
             member->childCount > 0 &&
             member->children[0] != NULL &&
             xl6TextEquals(member->children[0]->text, "create") ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckCustomPointBindingCtor(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* args;
    XL6_AstNode* arg;
    XL6_AstNode* argExpr;
    XL6_AstNode pointType;
    const char* argTypeName;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( args == NULL || args->childCount != 1 ) {
        xl6SemErrorNode(ctx, call, "custom 没有 create 时只能用一�?point 参数绑定已有句柄");
        return 0;
    }
    arg = args->children[0];
    if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG || arg->childCount == 0 ) {
        xl6SemErrorNode(ctx, arg != NULL ? arg : call, "custom 句柄绑定参数必须是位置参�?");
        return 0;
    }
    argExpr = arg->children[0];
    argTypeName = xl6SemExprStaticTypeName(ctx, argExpr);
    if ( argTypeName != NULL && !xl6TextEquals(argTypeName, "point") ) {
        xl6SemErrorNode(ctx, argExpr, "custom 句柄绑定参数必须�?point");
        return 0;
    }

    memset(&pointType, 0, sizeof(pointType));
    pointType.kind = XL6_AST_TYPE;
    pointType.text = "point";
    if ( !xl6SemExprCanMatchParamType(ctx, argExpr, &pointType) ) {
        xl6SemErrorNode(ctx, argExpr, "custom 句柄绑定参数必须�?point");
        return 0;
    }
    return 1;
}

static int xl6SemCheckCustomCtorCall(XL6_SemContext* ctx, XL6_AstNode* call, XL6_AstNode* customNode)
{
    if ( customNode == NULL ) {
        return 1;
    }
    if ( xl6SemCustomHasCreate(customNode) ) {
        return xl6SemResolveCreateMember(ctx, call, customNode, XL6_AST_CUSTOM_EXTERN_SPECIAL) != NULL;
    }
    return xl6SemCheckCustomPointBindingCtor(ctx, call);
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
    if ( typeInfo->kind == XL6_TYPE_CUSTOM ) {
        return xl6SemCheckCustomCtorCall(ctx, call, typeInfo->decl);
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
             member->kind == XL6_AST_CREATE &&
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
        xl6SemErrorNode(ctx, call, "base(...) 只能�?class �?create 体内使用");
        return 0;
    }

    baseDecl = xl6SemBaseClassDecl(ctx, ctx->currentClass);
    if ( baseDecl == NULL || baseDecl->kind != XL6_AST_CLASS ) {
        xl6SemErrorNode(ctx, call, "当前 class 没有父类，不能调�?base(...)");
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
    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* memberType = xl6SemCloneMemberFieldType(ctx, callee);
        if ( memberType != NULL && xl6TextEquals(memberType->text, "function") && memberType->childCount > 1 ) {
            if ( ownedType != NULL ) {
                *ownedType = memberType;
                return memberType->children[1];
            }
        }
        xl6AstFree(memberType);
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

    func = xl6SemResolveCallFunc(ctx, expr, callee->text);
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
        function 值本身只保存签名，不保存默认值�?        只有真实函数参数表（XL6_AST_PARAM）才能按可选参数展开匹配�?    */
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
            xl6SemErrorNode(ctx, atNode, "找不到可作为 function 值的具名函数");
        } else if ( functionType->childCount == 0 ) {
            xl6SemErrorNode(ctx, atNode, "�?function 目标无法从重载函数名选择唯一候�?");
        } else if ( matchCount == 0 ) {
            xl6SemErrorNode(ctx, atNode, "找不到和 function 目标签名匹配的具名函�?");
        } else {
            xl6SemErrorNode(ctx, atNode, "function 目标签名匹配到多个具名函数候�?");
        }
    }
    return NULL;
}

static int xl6SemExprCanMatchFunctionType(XL6_SemContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
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
            return xl6SemFindFunctionValueFunc(ctx, expr->text, typeNode, expr, 1) != NULL;
        }
    }
    if ( expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
         expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
        XL6_AstNode* func = xl6SemResolveCallFunc(ctx, (XL6_AstNode*)expr, expr->children[0]->text);
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

static int xl6SemParamPrefixEquals(XL6_SemContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b, int count)
{
    int i;

    for ( i = 0; i < count; ++i ) {
        XL6_AstNode* aType = (a->children[i]->childCount > 0) ? a->children[i]->children[0] : NULL;
        XL6_AstNode* bType = (b->children[i]->childCount > 0) ? b->children[i]->children[0] : NULL;
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
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;

    if ( fixedCount > 0 && provided == NULL ) {
        xl6SemErrorNode(ctx, call, "函数调用检查内存不�?");
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
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
                    ok = 0;
                }
                index = xl6SemFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    if ( provided[index] ) {
                        xl6SemErrorNode(ctx, arg, "重复传入同一个命名参�?");
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
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
                    ok = 0;
                }
                if ( seenNamed ) {
                    xl6SemErrorNode(ctx, arg, "位置参数不能出现在命名参数之�?");
                    ok = 0;
                }
                if ( positionalIndex < fixedCount ) {
                    if ( provided[positionalIndex] ) {
                        xl6SemErrorNode(ctx, arg, "参数已由命名参数传入");
                        ok = 0;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg ) {
                    xl6SemErrorNode(ctx, arg, "实参数量超过函数声明");
                    ok = 0;
                }
            }
        }
    }

    for ( i = 0; i < (size_t)fixedCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            xl6SemErrorNode(ctx, call, "缺少必选参�?");
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
    int ok;

    if ( functionType == NULL || !xl6TextEquals(functionType->text, "function") ) {
        return 1;
    }
    if ( functionType->childCount == 0 ) {
        ok = 1;
        args = call->childCount > 1 ? call->children[1] : NULL;
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                XL6_AstNode* arg = args->children[i];
                XL6_AstNode* argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
                if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                    xl6SemErrorNode(ctx, arg, "�?function 调用不支持命名参�?");
                    ok = 0;
                    continue;
                }
                if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                    xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
                    ok = 0;
                }
            }
        }
        return ok;
    }
    if ( functionType->childCount < 2 ) {
        xl6SemErrorNode(ctx, functionType, "function 类型签名不完�?");
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
    ok = 1;

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind != XL6_AST_NAMED_ARG ) {
                positionalArgCount += 1;
            }
        }
    }
    if ( positionalArgCount < fixedCount ) {
        xl6SemErrorNode(ctx, call, "function 调用实参数量和签名不匹配");
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
                        xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
                        ok = 0;
                    }
                    continue;
                }
                xl6SemErrorNode(ctx, arg, "function 值调用不支持命名参数");
                ok = 0;
                continue;
            }
            if ( seenNamed ) {
                xl6SemErrorNode(ctx, arg, "位置参数不能出现在命名参数之�?");
                ok = 0;
                continue;
            }
            if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
                xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
                ok = 0;
                continue;
            }
            paramIndex = positionalIndex;
            positionalIndex += 1;
            if ( paramIndex >= fixedCount && !hasArrayVararg ) {
                xl6SemErrorNode(ctx, argExpr, "function 调用实参数量和签名不匹配");
                ok = 0;
            }
            if ( paramIndex < fixedCount &&
                 !xl6SemExprCanMatchParamType(ctx, argExpr, params->children[paramIndex]) ) {
                xl6SemErrorNode(ctx, argExpr, "function 调用实参类型和签名不匹配");
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
            xl6SemErrorNode(ctx, arg, "位置参数不能出现在命名参数之�?");
            ok = 0;
        }
        if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
            xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为单个实�?");
            ok = 0;
            continue;
        }
        if ( !xl6SemExprCanBoxToVar(ctx, argExpr) ) {
            xl6SemErrorNode(ctx, argExpr, "var 函数调用实参必须能装箱为 var");
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
        xl6SemErrorNode(ctx, args->children[0], "type(...) 不支持命名参�?");
        return 0;
    }
    return 1;
}

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
        xl6SemErrorNode(ctx, arg, "类型转换不支持命名参�?");
        return 0;
    }
    argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
    if ( xl6SemExprIsMultiReturnCall(ctx, argExpr) ) {
        xl6SemErrorNode(ctx, argExpr, "多返回值函数调用不能作为类型转换参�?");
        return 0;
    }
    sourceType = xl6SemExprDeclaredTypeName(ctx, argExpr);
    if ( xl6TextEquals(callee->text, "var") && !xl6SemExprCanBoxToVar(ctx, argExpr) ) {
        xl6SemErrorNode(ctx, argExpr, "该表达式不能装入 var");
        return 0;
    }
    if ( !xl6SemCanConvertType(sourceType, callee->text) ) {
        xl6SemErrorNode(ctx, argExpr, "该表达式不能转换为目标类�?");
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
    return xl6SemTypeNameHasImplicitToString(typeName);
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
        xl6SemErrorNode(ctx, node, "thread constructor requires one function() int entry argument");
        return 0;
    }

    entryExpr = xl6SemCallArgExprAt(node, 0);
    entryType = xl6SemNewThreadEntryType(callee);
    if ( entryType == NULL ) {
        xl6SemErrorNode(ctx, node, "thread constructor entry type allocation failed");
        return 0;
    }
    if ( !xl6SemExprCanMatchFunctionType(ctx, entryExpr, entryType) ) {
        xl6SemErrorNode(ctx, entryExpr != NULL ? entryExpr : node, "thread entry must match function() int");
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

static int xl6SemCheckCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int ok;
    int channelOk;
    int syncOk;
    int threadOk;
    int memberCallOk;
    XL6_AstNode* func;
    XL6_Symbol* sym;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( xl6SemIsImplicitToStringCall(ctx, node) ) {
        return ok;
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
            if ( typeInfo != NULL && (typeInfo->kind == XL6_TYPE_CLASS || typeInfo->kind == XL6_TYPE_CUSTOM) ) {
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
    channelOk = xl6SemCheckChannelCall(ctx, node);
    if ( channelOk >= 0 ) {
        return channelOk && ok;
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

    memberCallOk = xl6SemCheckMemberFunctionValueCall(ctx, node);
    if ( memberCallOk >= 0 ) {
        return memberCallOk && ok;
    }

    if ( node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
        XL6_AstNode* instant = node->children[0];
        XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
        XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
        if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
            func = xl6SemResolveExplicitGenericCallFunc(ctx, node, nameNode->text, typeArgs, 1);
            return func != NULL && ok;
        }
    }

    if ( node->childCount > 0 && node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, node->children[0]->text);
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
        func = xl6SemResolveCallFunc(ctx, node, node->children[0]->text);
    } else {
        func = NULL;
    }
    if ( func != NULL && !xl6SemCheckKnownCall(ctx, node, func) ) {
        ok = 0;
    }
    return ok;
}

static XL6_AstNode* xl6SemFindExplicitGenericFunc(XL6_SemContext* ctx, const char* name, XL6_AstNode* typeArgs)
{
    XL6_AstNode* selected;
    size_t i;
    int matchCount;

    selected = NULL;
    matchCount = 0;
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
        if ( params != NULL && params->childCount == typeArgs->childCount ) {
            selected = node;
            matchCount += 1;
        }
    }
    if ( matchCount >= 1 ) {
        return selected;
    }
    if ( matchCount > 1 ) {
        xl6SemErrorNode(ctx, typeArgs, "显式泛型函数实例化存在歧�?");
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
            xl6SemErrorNode(ctx, node, "泛型 class 实参数量不匹�?");
            ok = 0;
        }
        return ok;
    }

    if ( typeInfo != NULL && typeInfo->kind == XL6_TYPE_BUILTIN && xl6TextEquals(nameNode->text, "channel") ) {
        if ( !typeInfo->allowGeneric || typeInfo->genericArity != (int)typeArgs->childCount ) {
            xl6SemErrorNode(ctx, node, "channel generic argument count mismatch");
            ok = 0;
        } else if ( !xl6SemCheckChannelTypeArg(ctx, node) ) {
            ok = 0;
        }
        return ok;
    }
    if ( xl6SemFindExplicitGenericFunc(ctx, nameNode->text, typeArgs) == NULL ) {
        xl6SemErrorNode(ctx, node, "找不到匹配的显式泛型函数实例");
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckTypedLiteral(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* literal;

    if ( node->childCount == 0 ) {
        return 1;
    }
    literal = node->children[0];

    if ( xl6TextEquals(node->text, "array") ) {
        if ( literal->kind != XL6_AST_ARRAY_LITERAL ) {
            xl6SemErrorNode(ctx, node, "array 字面量必须使用普�?[] 值列�?");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "list") ) {
        if ( literal->kind != XL6_AST_LIST_LITERAL ) {
            xl6SemErrorNode(ctx, node, "list 字面量必须使�?[key: value]");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "set") ) {
        /*
            普�?{} 默认解析�?dict；只有带 set 前缀的空 {} 才改作空集合�?            非空 set 仍必须使�?{value, value}，不能混�?{key: value}�?        */
        if ( literal->kind != XL6_AST_SET_LITERAL &&
             !(literal->kind == XL6_AST_DICT_LITERAL && literal->childCount == 0) ) {
            xl6SemErrorNode(ctx, node, "set 字面量必须使用普�?{} 值列�?");
            return 0;
        }
    } else if ( xl6TextEquals(node->text, "dict") ) {
        if ( literal->kind != XL6_AST_DICT_LITERAL ) {
            xl6SemErrorNode(ctx, node, "dict 字面量必须使�?{key: value}");
            return 0;
        }
    } else {
        xl6SemErrorNode(ctx, node, "未知的带类型容器字面�?");
        return 0;
    }

    return xl6SemCheckExpr(ctx, literal);
}

static const char* xl6SemExprStaticTypeName(XL6_SemContext* ctx, XL6_AstNode* expr)
{
    XL6_Symbol* sym;
    XL6_AstNode* func;
    XL6_AstNode* returns;
    const char* leftType;
    const char* rightType;
    const char* receiverType;

    if ( expr == NULL ) {
        return NULL;
    }

    switch ( expr->kind ) {
    case XL6_AST_INT:
        return "int";
    case XL6_AST_FLOAT:
        return "float";
    case XL6_AST_STRING:
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
    case XL6_AST_NAME:
        sym = xl6ScopeFind(ctx->scope, expr->text);
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
        if ( leftType != NULL && rightType != NULL && xl6TextEquals(leftType, rightType) ) {
            return leftType;
        }
        if ( (leftType != NULL && xl6TextEquals(leftType, "float")) ||
             (rightType != NULL && xl6TextEquals(rightType, "float")) ) {
            return "float";
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
    }    case XL6_AST_CALL:
        if ( xl6SemIsImplicitToStringCall(ctx, expr) ) {
            return "string";
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
            const char* channelResultType = xl6SemChannelCallResultTypeName(ctx, expr);
            if ( channelResultType != NULL ) {
                return channelResultType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_MEMBER ) {
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
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* instant = expr->children[0];
            XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
            XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
            if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                func = xl6SemResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
                returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
                if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC ) {
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
            func = xl6SemResolveCallFunc(ctx, expr, expr->children[0]->text);
            returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC ) {
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
           xl6TextEquals(name, "time") ||
           xl6TextEquals(name, "point");
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
    if ( itemType == NULL || !xl6SemChannelElementTypeIsFirstStageSafe(ctx, itemType) ) {
        xl6SemErrorNode(ctx, itemType != NULL ? itemType : callee, "channel<T> 第一版只支持 bool、整数、浮点、time、point 等可按值复制的元素类型");
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
    if ( node == NULL || node->childCount == 0 || !xl6SemCheckChannelTypeArg(ctx, node->children[0]) ) {
        ok = 0;
    }
    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    if ( args == NULL ) {
        return ok;
    }
    if ( args->childCount > 1 ) {
        xl6SemErrorNode(ctx, args, "channel 构造函数最多接受一个容量参�?");
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
            xl6SemErrorNode(ctx, expr, "channel 容量参数必须是整�?");
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
        xl6SemErrorNode(ctx, receiver != NULL ? receiver : member, "channel 第一版只支持对具名变量调用方�?");
        ok = 0;
    }
    args = node->childCount > 1 ? node->children[1] : NULL;
    argCount = args != NULL ? args->childCount : 0;

    if ( xl6TextEquals(member->text, "close") ||
         xl6TextEquals(member->text, "count") ||
         xl6TextEquals(member->text, "isClosed") ) {
        if ( argCount != 0 ) {
            xl6SemErrorNode(ctx, args, "�?channel 方法不接受参�?");
            ok = 0;
        }
        xl6AstFree(receiverType);
        return ok;
    }

    if ( xl6TextEquals(member->text, "send") ) {
        XL6_AstNode* arg = argCount == 1 ? args->children[0] : NULL;
        XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        itemType = xl6SemCloneSingleGenericArgType(ctx, receiverType, "channel");
        if ( argCount != 1 || arg == NULL || arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, node, "channel.send 必须传入一个位置参�?");
            ok = 0;
        } else if ( itemType == NULL || !xl6SemExprCanMatchParamType(ctx, expr, itemType) ) {
            xl6SemErrorNode(ctx, expr != NULL ? expr : arg, "channel.send 参数类型必须匹配 channel<T> 的元素类�?");
            ok = 0;
        }
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return ok;
    }

    if ( xl6TextEquals(member->text, "recvInto") ||
         xl6TextEquals(member->text, "tryRecvInto") ) {
        XL6_AstNode* arg = argCount == 1 ? args->children[0] : NULL;
        XL6_AstNode* expr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
        XL6_Symbol* targetSym = expr != NULL && expr->kind == XL6_AST_NAME ? xl6ScopeFind(ctx->scope, expr->text) : NULL;
        itemType = xl6SemCloneSingleGenericArgType(ctx, receiverType, "channel");
        if ( argCount != 1 || arg == NULL || arg->kind != XL6_AST_ARG || expr == NULL || expr->kind != XL6_AST_NAME ) {
            xl6SemErrorNode(ctx, node, "channel.recvInto 必须传入一个具名变�?");
            ok = 0;
        } else if ( targetSym == NULL || targetSym->type == NULL ) {
            xl6SemErrorNode(ctx, expr, "channel.recvInto 目标变量未声�?");
            ok = 0;
        } else if ( itemType == NULL || !xl6SemTypeEquals(ctx, targetSym->type, itemType) ) {
            xl6SemErrorNode(ctx, expr, "channel.recvInto 目标变量类型必须等于 channel<T> 的元素类�?");
            ok = 0;
        }
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return ok;
    }

    xl6SemErrorNode(ctx, member, "未知�?channel 方法");
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
         xl6TextEquals(member->text, "recvInto") ||
         xl6TextEquals(member->text, "tryRecvInto") ||
         xl6TextEquals(member->text, "isClosed") ) {
        result = "bool";
    } else if ( xl6TextEquals(member->text, "count") ) {
        result = "int";
    }
    xl6AstFree(receiverType);
    return result;
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
    return typeInfo != NULL ? typeInfo->name : NULL;
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
    receiverType = xl6SemInferAutoTypeNode(ctx, receiver);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteType != NULL && xl6TextEquals(concreteType->text, "set") ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    itemType = xl6SemCloneContainerItemType(ctx, receiverType);
    xl6AstFree(receiverType);
    return itemType;
}

static XL6_AstNode* xl6SemFunctionTypeFromAnon(const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* paramTypes;
    XL6_AstNode* returnTypes;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( expr == NULL || expr->kind != XL6_AST_ANON_FUNC ) {
        return NULL;
    }
    typeNode = xl6AstNewText(XL6_AST_TYPE, "function", 8, NULL);
    paramTypes = xl6AstNew(XL6_AST_LIST, NULL);
    returns = expr->childCount > 1 ? expr->children[1] : NULL;
    returnTypes = xl6SemCloneTypeNode(returns);
    if ( typeNode == NULL || paramTypes == NULL || returnTypes == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(paramTypes);
        xl6AstFree(returnTypes);
        return NULL;
    }
    typeNode->line = expr->line;
    typeNode->column = expr->column;
    params = expr->childCount > 0 ? expr->children[0] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            XL6_AstNode* paramType = (param != NULL && param->childCount > 0) ? xl6SemCloneTypeNode(param->children[0]) : NULL;
            if ( paramType == NULL ) {
                xl6AstFree(typeNode);
                xl6AstFree(paramTypes);
                xl6AstFree(returnTypes);
                return NULL;
            }
            xl6AstAddChild(paramTypes, paramType);
        }
    }
    xl6AstAddChild(typeNode, paramTypes);
    xl6AstAddChild(typeNode, returnTypes);
    return typeNode;
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
         typeInfo->kind != XL6_TYPE_CUSTOM &&
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

static XL6_AstNode* xl6SemInferAutoTypeNode(XL6_SemContext* ctx, XL6_AstNode* expr)
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
    if ( expr->kind == XL6_AST_INDEX || expr->kind == XL6_AST_SAFE_INDEX ) {
        XL6_AstNode* itemType = xl6SemCloneIndexedItemType(ctx, expr);
        if ( itemType != NULL ) {
            return itemType;
        }
    }
    if ( expr->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, expr->text);
        if ( sym != NULL && sym->type != NULL &&
             !xl6TextEquals(sym->type->text, "auto") && !xl6TextEquals(sym->type->text, "var") ) {
            return xl6SemCloneTypeNode(sym->type);
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
        if ( expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* instant = expr->children[0];
            XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
            XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
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
        if ( expr->children[0]->kind != XL6_AST_NAME ) {
            return NULL;
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
        func = xl6SemResolveCallFunc(ctx, expr, expr->children[0]->text);
        returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
        if ( func != NULL && func->kind == XL6_AST_ASYNC_FUNC ) {
            return xl6SemCloneAsyncCallResultType(ctx, func, NULL, expr);
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
static int xl6SemExprIsFreshTypeValue(XL6_SemContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* typeNode;
    XL6_AstNode* func;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
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
            return func != NULL && (func->kind == XL6_AST_ASYNC_FUNC || xl6SemFuncReturnsGenerator(ctx, func));
        }
        return 0;
    }
    if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
        func = xl6SemResolveCallFunc(ctx, (XL6_AstNode*)expr, callee->text);
        return func != NULL && (func->kind == XL6_AST_ASYNC_FUNC || xl6SemFuncReturnsGenerator(ctx, func));
    }
    return 0;
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
    if ( !xl6SemTypeCanCopy(ctx, typeNode) ) {
        xl6SemErrorNode(ctx, expr, message);
        ok = 0;
    }
    xl6AstFree(typeNode);
    return ok;
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

static int xl6SemCheckUnaryExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* operandType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
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

static int xl6SemCheckBinaryExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const char* leftType;
    const char* rightType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( node != NULL && (xl6TextEquals(node->text, "and") || xl6TextEquals(node->text, "or")) ) {
        if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "逻辑运算左操作数必须�?bool") ) {
            ok = 0;
        }
        if ( node->childCount > 1 && !xl6SemCheckBoolExpr(ctx, node->children[1], "逻辑运算右操作数必须�?bool") ) {
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
                xl6SemErrorNode(ctx, node, "字符串拼接两侧都必须�?string");
                ok = 0;
            }
        }
    }
    if ( node != NULL &&
         (xl6TextEquals(node->text, "+") || xl6TextEquals(node->text, "-") ||
          xl6TextEquals(node->text, "*") || xl6TextEquals(node->text, "/")) ) {
        leftType = node->childCount > 0 ? xl6SemExprStaticTypeName(ctx, node->children[0]) : NULL;
        rightType = node->childCount > 1 ? xl6SemExprStaticTypeName(ctx, node->children[1]) : NULL;
        if ( leftType != NULL && rightType != NULL ) {
            int bothStringConcat = xl6TextEquals(node->text, "+") &&
                                   xl6TextEquals(leftType, "string") &&
                                   xl6TextEquals(rightType, "string");
            if ( !bothStringConcat && !xl6TextEquals(leftType, "var") && !xl6TextEquals(rightType, "var") &&
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
        if ( leftType != NULL && rightType != NULL ) {
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

        /*
            比较表达式必须比较“值”�?            这条规则能防�?identity<int>(1) 被误解析�?(identity < int) > 1�?        */
        if ( xl6SemExprIsBareFunctionSymbol(ctx, left) ) {
            xl6SemErrorNode(ctx, left, "函数名不能作为普通比较操作数");
            ok = 0;
        }
        if ( xl6SemExprIsBareFunctionSymbol(ctx, right) ) {
            xl6SemErrorNode(ctx, right, "函数名不能作为普通比较操作数");
            ok = 0;
        }
        if ( !xl6SemCompareAllowsTypeLiteral(node->text) ) {
            if ( xl6SemExprIsTypeLiteralSymbol(ctx, left) ) {
                xl6SemErrorNode(ctx, left, "类型字面量只能用�?type 相等性比�?");
                ok = 0;
            }
            if ( xl6SemExprIsTypeLiteralSymbol(ctx, right) ) {
                xl6SemErrorNode(ctx, right, "类型字面量只能用�?type 相等性比�?");
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
                xl6SemErrorNode(ctx, node, "type 值只能用于相等性比�?");
                ok = 0;
            } else if ( !leftIsTypeOperand || !rightIsTypeOperand ) {
                xl6SemErrorNode(ctx, node, "type 值只能和 type 值或类型字面量比�?");
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
    size_t i;
    int ok;

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
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
    const char* receiverType;
    const char* indexType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    index = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    receiverType = receiver != NULL ? xl6SemExprStaticTypeName(ctx, receiver) : NULL;
    indexType = index != NULL ? xl6SemExprStaticTypeName(ctx, index) : NULL;

    if ( receiverType == NULL || indexType == NULL ) {
        return ok;
    }

    if ( xl6SemIsDictTypeName(receiverType) ) {
        if ( !xl6TextEquals(indexType, "string") && !xl6TextEquals(indexType, "var") ) {
            xl6SemErrorNode(ctx, index, "dict 索引键必须是 string");
            ok = 0;
        }
    } else if ( xl6SemIsArrayTypeName(receiverType) || xl6SemIsListTypeName(receiverType) ) {
        if ( !xl6SemIsIntegerTypeName(indexType) ) {
            xl6SemErrorNode(ctx, index, "array/list 索引必须�?int");
            ok = 0;
        }
    } else if ( xl6TextEquals(receiverType, "var") ) {
        if ( !xl6TextEquals(indexType, "var") && !xl6TextEquals(indexType, "string") && !xl6SemIsIntegerTypeName(indexType) ) {
            xl6SemErrorNode(ctx, index, "var container index key must be int, string or var");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemSafeMemberLengthReceiverIsDynamic(XL6_SemContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;
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

static int xl6SemCheckSafeMemberExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( node == NULL ) {
        return ok;
    }
    if ( !xl6TextEquals(node->text, "length") ) {
        xl6SemErrorNode(ctx, node, "safe member access only supports length");
        ok = 0;
    }

    receiver = node->childCount > 0 ? node->children[0] : NULL;
    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    if ( receiverType != NULL && !xl6SemSafeMemberLengthReceiverIsDynamic(ctx, receiverType) ) {
        xl6SemErrorNode(ctx, receiver, "safe member length receiver must be var or dynamic container");
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

    fieldType = xl6SemCloneMemberFieldType(ctx, node);
    if ( fieldType != NULL ) {
        xl6AstFree(fieldType);
        return ok;
    }

    receiverType = receiver != NULL ? xl6SemInferAutoTypeNode(ctx, receiver) : NULL;
    concreteReceiverType = xl6SemResolveAliasType(ctx, receiverType);
    receiverDecl = concreteReceiverType != NULL ? xl6SemFindTypeDecl(ctx, concreteReceiverType->text) : NULL;
    if ( receiverDecl != NULL && receiverDecl->kind == XL6_AST_CLASS && xl6SemClassHasCallableMember(ctx, receiverDecl, node->text) ) {
        xl6AstFree(receiverType);
        return ok;
    }
    if ( receiverDecl != NULL && (receiverDecl->kind == XL6_AST_CLASS || receiverDecl->kind == XL6_AST_UNION) ) {
        xl6SemErrorNode(ctx, node, "unknown class/union field");
        ok = 0;
    }
    xl6AstFree(receiverType);
    return ok;
}
static int xl6SemCheckNameExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Symbol* sym;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_NAME ) {
        return 1;
    }
    sym = xl6ScopeFind(ctx->scope, node->text);
    if ( sym == NULL || !xl6ScopeNameIsOutsideBoundary(ctx->scope, ctx->captureBoundary, node->text) ) {
        return 1;
    }
    if ( !xl6SemTypeCanCapture(ctx, sym->type) ) {
        xl6SemErrorNode(ctx, node, "该变量类型不能被匿名函数按值捕�?");
        return 0;
    }
    return 1;
}

static int xl6SemCheckExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 1;
    }

    switch ( node->kind ) {
    case XL6_AST_NAME:
        return xl6SemCheckNameExpr(ctx, node);
    case XL6_AST_TIME:
        return xl6SemCheckTimeLiteralExpr(ctx, node);
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
    case XL6_AST_UNARY:
        return xl6SemCheckUnaryExpr(ctx, node);
    case XL6_AST_AWAIT:
        return xl6SemCheckAwaitExpr(ctx, node);
    default:
        return xl6SemCheckChildrenExpr(ctx, node);
    }
}

static int xl6SemDeclareVar(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* declaredType;
    XL6_AstNode* symbolType;
    XL6_AstNode* initExpr;
    XL6_AstNode* inferredType;
    int ok;

    ok = 1;
    declaredType = node->childCount > 0 ? node->children[0] : NULL;
    symbolType = declaredType;
    initExpr = node->childCount > 1 ? node->children[1] : NULL;

    if ( declaredType != NULL ) {
        if ( !xl6SemCheckType(ctx, declaredType) ) {
            ok = 0;
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
    if ( !xl6ScopeAddTyped(ctx, node, node->kind == XL6_AST_CONST_DECL ? XL6_SYM_CONST : XL6_SYM_VALUE,
                           symbolType) ) {
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
        xl6SemErrorNode(ctx, initExpr, "多返回值函数调用必须使用多值接�?");
        ok = 0;
    }
    if ( initExpr != NULL && declaredType != NULL &&
         !xl6TextEquals(declaredType->text, "auto") &&
         !xl6SemExprCanMatchParamType(ctx, initExpr, declaredType) ) {
        xl6SemErrorNode(ctx, initExpr, "初始化表达式类型和声明类型不匹配");
        ok = 0;
    }
    return ok;
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
    if ( expr != NULL && !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    if ( returns == NULL || returns->childCount <= 1 ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : node, "多返回值声明右侧必须是多返回值函数调�?");
        ok = 0;
    } else if ( names == NULL || names->childCount != returns->childCount ) {
        xl6SemErrorNode(ctx, node, "多返回值声明数量和函数返回值数量不匹配");
        ok = 0;
    }

    if ( names != NULL ) {
        for ( i = 0; i < names->childCount; ++i ) {
            XL6_AstNode* name = names->children[i];
            XL6_AstNode* typeNode = (returns != NULL && i < returns->childCount) ? returns->children[i] : NULL;

            if ( name == NULL || name->kind != XL6_AST_NAME ) {
                xl6SemErrorNode(ctx, name != NULL ? name : node, "多返回值声明目标必须是变量�?");
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
    int resultIsFloat;

    if ( op == NULL || xl6TextEquals(op, "=") || xl6TextEquals(op, "?=") || right == NULL ) {
        return 1;
    }

    leftType = left != NULL ? xl6SemExprStaticTypeName(ctx, left) : NULL;
    rightType = right != NULL ? xl6SemExprStaticTypeName(ctx, right) : NULL;
    if ( leftType == NULL || rightType == NULL ) {
        return 1;
    }

    leftIsVar = xl6TextEquals(leftType, "var");
    rightIsVar = xl6TextEquals(rightType, "var");
    dynamicIndexTarget = left != NULL && left->kind == XL6_AST_INDEX && leftIsVar;

    if ( left != NULL && left->kind == XL6_AST_INDEX &&
         xl6SemCompoundAssignIsIndexOnlyUnsupportedOp(op) ) {
        xl6SemErrorNode(ctx, node, "index compound assignment operator is not supported");
        return 0;
    }

    if ( leftIsVar && !dynamicIndexTarget ) {
        xl6SemErrorNode(ctx, node, "var compound assignment target is not supported");
        return 0;
    }
    if ( rightIsVar && !dynamicIndexTarget ) {
        xl6SemErrorNode(ctx, node, "compound assignment var operand is not supported");
        return 0;
    }

    if ( dynamicIndexTarget && !xl6TextEquals(op, "**=") ) {
        return 1;
    }

    if ( xl6TextEquals(op, "+=") &&
         (xl6TextEquals(leftType, "string") || xl6TextEquals(rightType, "string")) ) {
        if ( !xl6TextEquals(leftType, "string") || !xl6TextEquals(rightType, "string") ) {
            xl6SemErrorNode(ctx, node, "string compound assignment operands must both be string");
            return 0;
        }
        return 1;
    }

    if ( xl6SemCompoundAssignIsSimpleNumericOp(op) ) {
        if ( !xl6SemIsNumericTypeName(leftType) || !xl6SemIsNumericTypeName(rightType) ) {
            xl6SemErrorNode(ctx, node, "numeric compound assignment operands must be numeric");
            return 0;
        }
        resultIsFloat = xl6SemIsFloatTypeName(leftType) || xl6SemIsFloatTypeName(rightType);
        if ( resultIsFloat && !xl6SemIsFloatTypeName(leftType) ) {
            xl6SemErrorNode(ctx, node, "compound assignment result type does not match target type");
            return 0;
        }
        if ( xl6TextEquals(op, "/=") && xl6SemExprIsZeroLiteral(right) ) {
            xl6SemErrorNode(ctx, node, "division by zero literal");
            return 0;
        }
        return 1;
    }

    if ( xl6TextEquals(op, "**=") ) {
        if ( !xl6SemIsIntegerTypeName(leftType) || !xl6SemIsIntegerTypeName(rightType) ) {
            xl6SemErrorNode(ctx, node, "power compound assignment operands must be integer");
            return 0;
        }
        return 1;
    }

    if ( xl6SemCompoundAssignIsIntegerOp(op) ) {
        if ( !xl6SemIsIntegerTypeName(leftType) || !xl6SemIsIntegerTypeName(rightType) ) {
            xl6SemErrorNode(ctx, node, "integer compound assignment operands must be integer");
            return 0;
        }
        if ( xl6TextEquals(op, "%=") && xl6SemExprIsZeroLiteral(right) ) {
            xl6SemErrorNode(ctx, node, "modulo by zero literal");
            return 0;
        }
        return 1;
    }

    xl6SemErrorNode(ctx, node, "unsupported compound assignment operator");
    return 0;
}
static int xl6SemCheckAssign(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* left;
    XL6_AstNode* right;
    XL6_Symbol* sym;
    const char* op;
    int ok;

    ok = 1;
    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "=";

    if ( left == NULL ||
         (left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER && left->kind != XL6_AST_INDEX) ) {
        xl6SemErrorNode(ctx, left != NULL ? left : node, "当前阶段赋值左侧必须是变量�?");
        ok = 0;
    } else if ( left->kind == XL6_AST_NAME ) {
        sym = xl6ScopeFind(ctx->scope, left->text);
        if ( sym == NULL ) {
            xl6SemErrorNode(ctx, left, "赋值目标未声明");
            ok = 0;
        } else if ( xl6SemIsImplicitSelfTarget(ctx, left) ) {
            xl6SemErrorNode(ctx, left, "self 不能被重新赋�?");
            ok = 0;
        } else if ( sym->kind == XL6_SYM_CONST ) {
            xl6SemErrorNode(ctx, left, "const 不能被重新赋�?");
            ok = 0;
        } else if ( sym->kind == XL6_SYM_FUNC ) {
            xl6SemErrorNode(ctx, left, "函数名不能作为赋值目�?");
            ok = 0;
        } else if ( xl6ScopeNameIsOutsideBoundary(ctx->scope, ctx->captureBoundary, left->text) ) {
            xl6SemErrorNode(ctx, left, "捕获变量在匿名函数体内只�?");
            ok = 0;
        } else if ( right != NULL &&
                    (xl6TextEquals(op, "=") || xl6TextEquals(op, "?=")) &&
                    !xl6SemExprCanMatchParamType(ctx, right, sym->type) ) {
            xl6SemErrorNode(ctx, right, "赋值表达式类型和目标变量类型不匹配");
            ok = 0;
        }
    } else if ( !xl6SemCheckExpr(ctx, left) ) {
        ok = 0;
    }

    if ( right != NULL && !xl6SemCheckExpr(ctx, right) ) {
        ok = 0;
    }
    if ( right != NULL && left != NULL && (left->kind == XL6_AST_MEMBER || left->kind == XL6_AST_INDEX) &&
         (xl6TextEquals(op, "=") || xl6TextEquals(op, "?=")) ) {
        XL6_AstNode* targetType = left->kind == XL6_AST_MEMBER ?
            xl6SemCloneMemberFieldType(ctx, left) :
            xl6SemCloneIndexedItemType(ctx, left);
        if ( targetType != NULL && !xl6SemExprCanMatchParamType(ctx, right, targetType) ) {
            xl6SemErrorNode(ctx, right, "assignment expression type does not match member/index target type");
            ok = 0;
        }
        xl6AstFree(targetType);
    }
    if ( right != NULL &&
         !xl6SemCheckCopyableExpr(ctx, right, 1, "不可复制类型不能通过赋值复�?") ) {
        ok = 0;
    }
    if ( right != NULL && xl6SemExprIsMultiReturnCall(ctx, right) ) {
        xl6SemErrorNode(ctx, right, "多返回值函数调用必须使用多值接�?");
        ok = 0;
    }
    if ( !xl6SemCheckCompoundAssignTypes(ctx, node, left, right, op) ) {
        ok = 0;
    }
    return ok;
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
    if ( expr != NULL && !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    if ( returns == NULL || returns->childCount <= 1 ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : node, "多返回值赋值右侧必须是多返回值函数调�?");
        ok = 0;
    } else if ( targets == NULL || targets->childCount != returns->childCount ) {
        xl6SemErrorNode(ctx, node, "多返回值赋值数量和函数返回值数量不匹配");
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
                    xl6SemErrorNode(ctx, target, "self 不能被重新赋�?");
                    ok = 0;
                } else if ( sym->kind == XL6_SYM_CONST ) {
                    xl6SemErrorNode(ctx, target, "const 不能被重新赋�?");
                    ok = 0;
                } else if ( sym->kind == XL6_SYM_FUNC ) {
                    xl6SemErrorNode(ctx, target, "函数名不能作为赋值目�?");
                    ok = 0;
                } else if ( xl6ScopeNameIsOutsideBoundary(ctx->scope, ctx->captureBoundary, target->text) ) {
                    xl6SemErrorNode(ctx, target, "捕获变量在匿名函数体内只�?");
                    ok = 0;
                } else if ( !xl6SemTypeCanAssignFromType(ctx, valueType, sym->type) ) {
                    xl6SemErrorNode(ctx, target, "多返回值赋值类型和目标变量类型不匹�?");
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
                    xl6SemErrorNode(ctx, target, "multi-return assignment type does not match member/index target type");
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
    if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "if 条件必须�?bool") ) {
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
            if ( item->childCount > 0 && !xl6SemCheckBoolExpr(ctx, item->children[0], "elseif 条件必须�?bool") ) {
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
    if ( node->childCount > 0 && !xl6SemCheckBoolExpr(ctx, node->children[0], "while 条件必须�?bool") ) {
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
    if ( node->childCount > 1 && !xl6SemCheckBoolExpr(ctx, node->children[1], "do while 条件必须�?bool") ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckThrowStmt(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* expr;
    const char* typeName;
    int ok;

    ok = 1;
    expr = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    if ( expr == NULL ) {
        if ( ctx->catchDepth <= 0 ) {
            xl6SemErrorNode(ctx, node, "�?throw 只能�?catch 块内重新抛出当前异常");
            ok = 0;
        }
        return ok;
    }

    if ( !xl6SemCheckExpr(ctx, expr) ) {
        ok = 0;
    }
    if ( xl6SemExprIsMultiReturnCall(ctx, expr) ) {
        xl6SemErrorNode(ctx, expr, "throw 表达式不能是多返回值函数调�?");
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
        xl6SemErrorNode(ctx, node, "assert 缺少条件表达�?");
        ok = 0;
    } else {
        if ( !xl6SemCheckExpr(ctx, cond) ) {
            ok = 0;
        }
        if ( !xl6SemCheckBoolExpr(ctx, cond, "assert 条件必须�?bool") ) {
            ok = 0;
        }
    }

    if ( message != NULL ) {
        if ( !xl6SemCheckExpr(ctx, message) ) {
            ok = 0;
        }
        if ( xl6SemExprIsMultiReturnCall(ctx, message) ) {
            xl6SemErrorNode(ctx, message, "assert 消息不能是多返回值函数调�?");
            ok = 0;
        }
        messageType = xl6SemExprDeclaredTypeName(ctx, message);
        if ( messageType != NULL && !xl6TextEquals(messageType, "string") ) {
            xl6SemErrorNode(ctx, message, "assert 消息必须�?string");
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
         node->kind == XL6_AST_CLASS ||
         node->kind == XL6_AST_CUSTOM ) {
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
    XL6_AstNode* catchNode;
    XL6_AstNode* finallyBlock;
    XL6_AstNode* catchName;
    XL6_AstNode* catchBlock;
    XL6_AstNode* errorType;
    XL6_Scope catchScope;
    int ok;

    if ( node == NULL ) {
        return 1;
    }

    ok = 1;
    tryBlock = node->childCount > 0 ? node->children[0] : NULL;
    catchNode = node->childCount > 1 ? node->children[1] : NULL;
    finallyBlock = node->childCount > 2 ? node->children[2] : NULL;

    if ( catchNode == NULL || catchNode->kind != XL6_AST_CATCH ) {
        catchNode = NULL;
    }
    if ( (catchNode == NULL || catchNode->childCount == 0) &&
         (finallyBlock == NULL || finallyBlock->childCount == 0) ) {
        xl6SemErrorNode(ctx, node, "try 必须包含 catch �?finally");
        ok = 0;
    }
    if ( xl6SemNodeContainsAbruptControl(tryBlock) ||
         xl6SemNodeContainsAbruptControl(catchNode) ||
         xl6SemNodeContainsAbruptControl(finallyBlock) ) {
        xl6SemErrorNode(ctx, node, "try/catch/finally 第一版暂不支�?return/break/continue 穿过异常边界");
        ok = 0;
    }

    if ( tryBlock != NULL && !xl6SemCheckBlock(ctx, tryBlock, 1) ) {
        ok = 0;
    }

    if ( catchNode != NULL ) {
        catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
        catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
        xl6SemPushScope(ctx, &catchScope);
        errorType = xl6SemOwnedSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
        if ( catchName == NULL || catchName->kind != XL6_AST_NAME ) {
            xl6SemErrorNode(ctx, catchNode, "catch 缺少异常变量�?");
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
        xl6SemErrorNode(ctx, at, "类型节点表内存不�?");
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
        if ( targetCount != 1 ) {
            xl6AstFree(iterableType);
            return NULL;
        }
        itemType = xl6SemCloneSingleGenericArgType(ctx, concreteType, "generator");
        xl6AstFree(iterableType);
        return itemType;
    }

    if ( xl6TextEquals(typeName, "var") ) {
        xl6AstFree(iterableType);
        return xl6SemNewForTargetSimpleType("var", iterable);
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

    if ( node->childCount > 0 && node->children[0] != NULL ) {
        targets = node->children[0];
        if ( targets->childCount == 0 || targets->childCount > 2 ) {
            xl6SemErrorNode(ctx, targets, "for in 只支持一个或两个迭代变量");
            ok = 0;
        }
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

static int xl6SemCheckFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_Scope scope;
    XL6_TypeParamScope typeParamScope;
    XL6_AstNode* typeParams;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* oldReturns;
    XL6_AstNode* oldFunction;
    size_t i;
    int ok;
    int pushedTypeParams;
    int oldGeneratorDepth;
    int isGeneratorFunc;
    const char* oldImplicitSelfName;

    ok = 1;
    pushedTypeParams = 0;
    xl6SemPushScope(ctx, &scope);
    if ( ctx->implicitSelfName != NULL ) {
        XL6_AstNode selfNode;
        XL6_AstNode* selfType;
        memset(&selfNode, 0, sizeof(selfNode));
        selfNode.kind = XL6_AST_NAME;
        selfNode.text = (char*)ctx->implicitSelfName;
        selfNode.line = node->line;
        selfNode.column = node->column;
        selfType = ctx->currentClass != NULL ? ctx->currentClass : ctx->currentCustom;
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

        seenOptional = 0;
        seenVararg = 0;
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* param = params->children[i];
            int isVararg = xl6SemParamIsVararg(param);
            if ( param->childCount > 0 ) {
                if ( xl6SemTypeIsAuto(param->children[0]) ) {
                    xl6SemErrorNode(ctx, param->children[0], "auto 不能作为函数参数类型");
                    ok = 0;
                } else {
                    if ( !xl6SemCheckType(ctx, param->children[0]) ) {
                        ok = 0;
                    }
                    if ( !isVararg && !xl6SemTypeCanCopy(ctx, param->children[0]) ) {
                        xl6SemErrorNode(ctx, param->children[0], "不可复制类型不能作为按值参�?");
                        ok = 0;
                    }
                }
            }
            if ( isVararg ) {
                if ( seenVararg ) {
                    xl6SemErrorNode(ctx, param, "一个函数最多只能声明一个可变参�?");
                    ok = 0;
                }
                if ( i + 1 < params->childCount ) {
                    xl6SemErrorNode(ctx, param, "可变参数必须位于参数表最�?");
                    ok = 0;
                }
                seenVararg = 1;
            } else if ( seenVararg ) {
                xl6SemErrorNode(ctx, param, "可变参数之后不能声明普通参�?");
                ok = 0;
            } else if ( param->childCount > 1 ) {
                seenOptional = 1;
                if ( !xl6SemCheckExpr(ctx, param->children[1]) ) {
                    ok = 0;
                }
                if ( !xl6SemExprCanMatchParamType(ctx, param->children[1], param->children[0]) ) {
                    xl6SemErrorNode(ctx, param->children[1], "默认值表达式类型和参数类型不匹配");
                    ok = 0;
                }
            } else if ( seenOptional ) {
                xl6SemErrorNode(ctx, param, "必选参数不能出现在可选参数之�?");
                ok = 0;
            }
            if ( !xl6ScopeAddTyped(ctx, param, XL6_SYM_VALUE, param->childCount > 0 ? param->children[0] : NULL) ) {
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    isGeneratorFunc = 0;
    if ( node->kind == XL6_AST_ASYNC_FUNC && returns != NULL && returns->childCount > 1 ) {
        xl6SemErrorNode(ctx, returns, "async 函数第一版只能省略返回值或返回一个�?");
        ok = 0;
    }
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            XL6_AstNode* returnType = (XL6_AstNode*)xl6SemResolveAliasType(ctx, returns->children[i]);
            if ( xl6SemTypeIsAuto(returns->children[i]) ) {
                xl6SemErrorNode(ctx, returns->children[i], "auto 不能作为函数返回类型");
                ok = 0;
            } else if ( !xl6SemCheckType(ctx, returns->children[i]) ) {
                ok = 0;
            }
            if ( node->kind == XL6_AST_ASYNC_FUNC && returnType != NULL &&
                 xl6SemIsFutureLikeTypeName(returnType->text) ) {
                xl6SemErrorNode(ctx, returns->children[i], "async 函数返回类型应声明为完成后的值类型，不应直接声明 future/task");
                ok = 0;
            }
            if ( returnType != NULL && xl6SemIsGeneratorTypeName(returnType->text) ) {
                isGeneratorFunc = 1;
            }
            if ( !xl6SemTypeIsAuto(returns->children[i]) &&
                 !(returnType != NULL && xl6SemIsGeneratorTypeName(returnType->text)) &&
                 !xl6SemTypeCanCopy(ctx, returns->children[i]) ) {
                xl6SemErrorNode(ctx, returns->children[i], "���ɸ������Ͳ�����Ϊ��ֵ��������");
                ok = 0;
            }
        }
    }

    if ( node->kind == XL6_AST_ASYNC_FUNC && isGeneratorFunc ) {
        xl6SemErrorNode(ctx, node, "第一版暂不支�?async generator");
        ok = 0;
    }

    oldReturns = ctx->currentReturns;
    oldFunction = ctx->currentFunction;
    oldGeneratorDepth = ctx->generatorDepth;
    ctx->currentReturns = returns;
    ctx->currentFunction = node;
    if ( isGeneratorFunc ) {
        ctx->generatorDepth += 1;
    }
    if ( node->childCount > 3 && !xl6SemCheckBlock(ctx, node->children[3], 0) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    ctx->currentFunction = oldFunction;
    ctx->generatorDepth = oldGeneratorDepth;

    if ( pushedTypeParams ) {
        xl6SemPopTypeParams(ctx, &typeParamScope);
    }
    xl6SemPopScope(ctx, &scope);
    return ok;
}

static int xl6SemCheckExportSymbolUnique(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;
    int ok;

    if ( ctx->root == NULL || node->text == NULL ) {
        return 1;
    }

    ok = 1;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* other = ctx->root->children[i];
        if ( other == node ) {
            continue;
        }
        if ( (other->kind == XL6_AST_EXPORT_FUNC ||
              other->kind == XL6_AST_EXPORT_VAR ||
              other->kind == XL6_AST_EXPORT_CONST) &&
             xl6TextEquals(other->text, node->text) ) {
            xl6SemErrorNode(ctx, node, "重复�?C export symbol");
            ok = 0;
            break;
        }
    }
    return ok;
}

static int xl6SemIsCAbiParamType(const char* name)
{
    return xl6SemIsBoolTypeName(name) || xl6SemIsIntegerTypeName(name) ||
           xl6SemIsFloatTypeName(name) ||
           xl6TextEquals(name, "point") || xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "var");
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

static int xl6SemIsCAbiUnionType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    return typeInfo != NULL && typeInfo->kind == XL6_TYPE_UNION;
}
static int xl6SemIsCAbiClassType(XL6_SemContext* ctx, XL6_AstNode* typeNode)
{
    const XL6_TypeInfo* typeInfo;

    typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return 0;
    }
    typeInfo = xl6TypeRegistryFind(&ctx->types, typeNode->text);
    if ( typeInfo == NULL || typeInfo->kind != XL6_TYPE_CLASS || typeInfo->allowGeneric ) {
        return 0;
    }
    return xl6SemTypeCanCopy(ctx, typeNode);
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
                xl6SemErrorNode(ctx, param, "C ABI export 不支持可选参�?");
                ok = 0;
            }
            if ( typeNode == NULL || xl6TextEquals(typeNode->text, "array<var>") || xl6TextEquals(typeNode->text, "dict<var>") ) {
                xl6SemErrorNode(ctx, param, "C ABI export 不支持可变参�?");
                ok = 0;
                continue;
            }
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( typeNode->childCount > 0 ||
                 (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiUnionType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode)) ) {
                xl6SemErrorNode(ctx, typeNode, "该参数类型不能直接进入第一阶段 C ABI export");
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            XL6_AstNode* typeNode = returns->children[i];
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( typeNode == NULL || typeNode->childCount > 0 ||
                 (!xl6SemIsCAbiReturnType(typeNode->text) && !xl6SemIsCAbiUnionType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode)) ) {
                xl6SemErrorNode(ctx, typeNode, "该返回类型不能直接进入第二阶�?C ABI export");
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
            if ( typeNode == NULL || xl6TextEquals(typeNode->text, "array<var>") || xl6TextEquals(typeNode->text, "dict<var>") ) {
                xl6SemErrorNode(ctx, param, "C ABI extern does not support varargs");
                ok = 0;
                continue;
            }
            typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
            if ( typeNode != NULL && xl6TextEquals(typeNode->text, "function") && xl6SemIsInternalStdRuntimeExtern(node) ) {
                continue;
            }
            if ( typeNode->childCount > 0 ||
                 (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiUnionType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode)) ) {
                xl6SemErrorNode(ctx, typeNode, "parameter type cannot enter first-stage C ABI extern directly");
                ok = 0;
            }
        }
    }

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6SemErrorNode(ctx, returns, "C ABI extern does not support multiple return values");
        ok = 0;
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        XL6_AstNode* typeNode = returns->children[0];
        typeNode = (XL6_AstNode*)xl6SemResolveAliasType(ctx, typeNode);
        if ( typeNode == NULL || typeNode->childCount > 0 ||
             (!xl6SemIsCAbiParamType(typeNode->text) && !xl6SemIsCAbiUnionType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode)) ) {
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
        if ( xl6SemResolveAliasType(ctx, typeNode)->childCount > 0 ||
             (!xl6SemIsCAbiVarType(xl6SemResolveAliasTypeName(ctx, typeNode)) && !xl6SemIsCAbiUnionType(ctx, typeNode) && !xl6SemIsCAbiClassType(ctx, typeNode)) ) {
            xl6SemErrorNode(ctx, typeNode, "variable type cannot enter first-stage C ABI directly");
            ok = 0;
        }
    }

    if ( nameNode != NULL ) {
        kind = node->kind == XL6_AST_EXPORT_CONST ? XL6_SYM_CONST : XL6_SYM_VALUE;
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
                xl6SemErrorNode(ctx, initExpr, "C ABI export variable initializer type mismatch");
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
    if ( !xl6SemCheckExportCAbi(ctx, node) ) {
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
           (node->kind == XL6_AST_CLASS || node->kind == XL6_AST_CUSTOM ||
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
    case XL6_AST_CUSTOM_EXTERN_SPECIAL:
    case XL6_AST_CUSTOM_EXTERN_FUNC:
        if ( node->childCount > 0 && node->children[0] != NULL ) {
            return node->children[0]->text;
        }
        return NULL;
    default:
        return NULL;
    }
}

static int xl6SemMembersShareOverloadGroup(const XL6_AstNode* a, const XL6_AstNode* b)
{
    const char* aName;
    const char* bName;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    if ( a->kind != XL6_AST_FUNC &&
         a->kind != XL6_AST_ASYNC_FUNC &&
         a->kind != XL6_AST_CREATE &&
         a->kind != XL6_AST_CUSTOM_EXTERN_FUNC &&
         a->kind != XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        return 0;
    }
    if ( b->kind != XL6_AST_FUNC &&
         b->kind != XL6_AST_ASYNC_FUNC &&
         b->kind != XL6_AST_CREATE &&
         b->kind != XL6_AST_CUSTOM_EXTERN_FUNC &&
         b->kind != XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        return 0;
    }

    aName = xl6SemMemberName(a);
    bName = xl6SemMemberName(b);
    return aName != NULL && bName != NULL && xl6TextEquals(aName, bName);
}

static int xl6SemCheckTypeDeclName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    size_t i;
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
    if ( ctx->root != NULL ) {
        for ( i = 0; i < ctx->root->childCount; ++i ) {
            XL6_AstNode* other = ctx->root->children[i];
            if ( other != node && xl6SemIsTypeDecl(other) && xl6TextEquals(other->text, node->text) ) {
                xl6SemErrorNode(ctx, node, "重复声明自定义类�?");
                ok = 0;
                break;
            }
        }
    }
    return ok;
}

static int xl6SemCheckParamListOnly(XL6_SemContext* ctx, XL6_AstNode* params, int addSymbols)
{
    size_t i;
    int ok;
    int seenOptional;
    int seenVararg;

    if ( params == NULL ) {
        return 1;
    }

    ok = 1;
    seenOptional = 0;
    seenVararg = 0;
    for ( i = 0; i < params->childCount; ++i ) {
        XL6_AstNode* param = params->children[i];
        int isVararg;

        if ( param == NULL ) {
            continue;
        }
        isVararg = xl6SemParamIsVararg(param);

        if ( param->childCount > 0 ) {
            if ( xl6SemTypeIsAuto(param->children[0]) ) {
                xl6SemErrorNode(ctx, param->children[0], "auto 不能作为函数参数类型");
                ok = 0;
            } else {
                if ( !xl6SemCheckType(ctx, param->children[0]) ) {
                    ok = 0;
                }
                if ( !isVararg && !xl6SemTypeCanCopy(ctx, param->children[0]) ) {
                    xl6SemErrorNode(ctx, param->children[0], "不可复制类型不能作为按值参�?");
                    ok = 0;
                }
            }
        }
        if ( isVararg ) {
            if ( seenVararg ) {
                xl6SemErrorNode(ctx, param, "一个函数最多只能声明一个可变参�?");
                ok = 0;
            }
            if ( i + 1 < params->childCount ) {
                xl6SemErrorNode(ctx, param, "可变参数必须位于参数表最�?");
                ok = 0;
            }
            seenVararg = 1;
        } else if ( seenVararg ) {
            xl6SemErrorNode(ctx, param, "可变参数之后不能声明普通参�?");
            ok = 0;
        } else if ( param->childCount > 1 ) {
            seenOptional = 1;
            if ( !xl6SemCheckExpr(ctx, param->children[1]) ) {
                ok = 0;
            }
            if ( !xl6SemExprCanMatchParamType(ctx, param->children[1], param->children[0]) ) {
                xl6SemErrorNode(ctx, param->children[1], "默认值表达式类型和参数类型不匹配");
                ok = 0;
            }
        } else if ( seenOptional ) {
            xl6SemErrorNode(ctx, param, "必选参数不能出现在可选参数之�?");
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
        if ( !xl6SemTypeIsAuto(returns->children[i]) && !xl6SemTypeCanCopy(ctx, returns->children[i]) ) {
            xl6SemErrorNode(ctx, returns->children[i], "不可复制类型不能作为按值返回类�?");
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
    XL6_Scope* oldCaptureBoundary;
    XL6_AstNode* oldCurrentClass;
    XL6_AstNode* oldCurrentCustom;
    int oldInClassCreate;
    int ok;

    params = node->childCount > 0 ? node->children[0] : NULL;
    returns = node->childCount > 1 ? node->children[1] : NULL;
    body = node->childCount > 2 ? node->children[2] : NULL;

    ok = 1;
    xl6SemPushScope(ctx, &scope);
    oldCaptureBoundary = ctx->captureBoundary;
    ctx->captureBoundary = &scope;
    if ( !xl6SemCheckParamListOnly(ctx, params, 1) ) {
        ok = 0;
    }
    if ( !xl6SemCheckReturnTypes(ctx, returns) ) {
        ok = 0;
    }
    oldReturns = ctx->currentReturns;
    oldCurrentClass = ctx->currentClass;
    oldCurrentCustom = ctx->currentCustom;
    oldInClassCreate = ctx->inClassCreate;
    ctx->currentReturns = returns;
    ctx->currentClass = NULL;
    ctx->currentCustom = NULL;
    ctx->inClassCreate = 0;
    if ( !xl6SemCheckBlock(ctx, body, 0) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    ctx->currentClass = oldCurrentClass;
    ctx->currentCustom = oldCurrentCustom;
    ctx->inClassCreate = oldInClassCreate;
    ctx->captureBoundary = oldCaptureBoundary;
    xl6SemPopScope(ctx, &scope);
    return ok;
}

static int xl6SemCheckField(XL6_SemContext* ctx, XL6_AstNode* field)
{
    int ok;

    ok = 1;
    if ( xl6SemIsSpecialName(field->text) ) {
        xl6SemErrorNode(ctx, field, "字段名不能使�?create �?destroy");
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
    if ( field->childCount > 1 && !xl6SemCheckExpr(ctx, field->children[1]) ) {
        ok = 0;
    }
    if ( field->childCount > 1 && field->childCount > 0 &&
         !xl6SemExprCanMatchParamType(ctx, field->children[1], field->children[0]) ) {
        xl6SemErrorNode(ctx, field->children[1], "字段默认值表达式类型和字段类型不匹配");
        ok = 0;
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
    if ( xl6TextEquals(typeNode->text, "function") ||
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
    if ( typeInfo->kind == XL6_TYPE_CUSTOM ) {
        return !xl6SemCustomDeclHasDestroy(typeInfo->decl);
    }

    decl = typeInfo->decl;
    if ( decl == NULL ) {
        return 1;
    }
    if ( typeInfo->kind == XL6_TYPE_UNION ) {
        body = xl6SemUnionBody((XL6_AstNode*)decl);
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
    XL6_AstNode* params;
    int ok;

    ok = 1;
    if ( !xl6SemIsSpecialName(node->text) ) {
        xl6SemErrorNode(ctx, node, "class 成员中只�?create/destroy 可以省略 func");
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
        selfType = ctx->currentClass != NULL ? ctx->currentClass : ctx->currentCustom;
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
    ctx->currentReturns = &emptyReturns;
    if ( node->childCount > 1 && !xl6SemCheckBlock(ctx, node->children[1], 0) ) {
        ok = 0;
    }
    ctx->currentReturns = oldReturns;
    xl6SemPopScope(ctx, &scope);
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

    concreteType = xl6SemResolveAliasType(ctx, receiverType);
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

    typeArgs = (ownerClass == decl) ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
    if ( typeArgs != NULL && xl6SemClassHasTypeParams(ownerClass) ) {
        return xl6SemSubstituteClassType(ownerClass, typeArgs, field->children[0]);
    }
    return xl6SemCloneTypeNode(field->children[0]);
}

static int xl6SemClassHasCallableMember(XL6_SemContext* ctx, const XL6_AstNode* classNode, const char* memberName)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL || memberName == NULL ) {
        return 0;
    }
    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member != NULL &&
                 (member->kind == XL6_AST_FUNC || member->kind == XL6_AST_ASYNC_FUNC || member->kind == XL6_AST_CREATE) &&
                 xl6TextEquals(xl6SemMemberName(member), memberName) ) {
                return 1;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6SemFindTypeDecl(ctx, base->text);
    return xl6SemClassHasCallableMember(ctx, baseClass, memberName);
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

        if ( member != NULL && member->kind == XL6_AST_CREATE && xl6TextEquals(name, "destroy") ) {
            destroyCount += 1;
            if ( destroyCount > 1 ) {
                xl6SemErrorNode(ctx, member, "destroy can only be declared once");
                ok = 0;
            }
        }
        if ( member != NULL &&
             (member->kind == XL6_AST_FUNC || member->kind == XL6_AST_ASYNC_FUNC) &&
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
             member->kind != XL6_AST_CREATE ||
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
        xl6SemErrorNode(ctx, classNode, "父类 create 需要参数，子类必须声明 create 并显式调�?base(...)");
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
    XL6_AstNode* oldCurrentCustom;
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
            xl6SemErrorNode(ctx, base, "第一阶段泛型 class 暂不支持继承");
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
        if ( !xl6SemCheckClassBaseCreateRule(ctx, node, baseDecl) ) {
            ok = 0;
        }
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_FIELD ) {
                if ( !xl6SemCheckField(ctx, member) ) {
                    ok = 0;
                }
            } else if ( member->kind == XL6_AST_UNION ) {
                oldCurrentClass = ctx->currentClass;
                ctx->currentClass = node;
                if ( !xl6SemCheckUnion(ctx, member) ) {
                    ok = 0;
                }
                ctx->currentClass = oldCurrentClass;
            } else if ( member->kind == XL6_AST_FUNC || member->kind == XL6_AST_ASYNC_FUNC ) {
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldCurrentCustom = ctx->currentCustom;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = "self";
                ctx->currentClass = node;
                ctx->currentCustom = NULL;
                ctx->inClassCreate = 0;
                if ( !xl6SemCheckFunc(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->currentCustom = oldCurrentCustom;
                ctx->inClassCreate = oldInClassCreate;
            } else if ( member->kind == XL6_AST_CREATE ) {
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldCurrentCustom = ctx->currentCustom;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = "self";
                ctx->currentClass = node;
                ctx->currentCustom = NULL;
                ctx->inClassCreate = xl6TextEquals(xl6SemMemberName(member), "create");
                if ( !xl6SemCheckClassSpecial(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->currentCustom = oldCurrentCustom;
                ctx->inClassCreate = oldInClassCreate;
            } else {
                xl6SemErrorNode(ctx, member, "class 中不支持该成�?");
                ok = 0;
            }
        }
    }
    if ( pushedTypeParams ) {
        xl6SemPopTypeParams(ctx, &typeParamScope);
    }
    return ok;
}

static int xl6SemCheckCustomExternSpecial(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    XL6_AstNode* params;
    int ok;

    ok = 1;
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;

    if ( nameNode == NULL || !xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode != NULL ? nameNode : node, "custom extern 省略 func 时只能声�?create/destroy");
        ok = 0;
    }
    if ( nameNode != NULL && xl6TextEquals(nameNode->text, "destroy") && params != NULL && params->childCount > 0 ) {
        xl6SemErrorNode(ctx, nameNode, "custom destroy 不能声明参数");
        ok = 0;
    }
    if ( !xl6SemCheckParamListOnly(ctx, params, 0) ) {
        ok = 0;
    }
    if ( !xl6SemCheckExternCAbi(ctx, node) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckCustomExternFunc(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* nameNode;
    int ok;

    ok = 1;
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( nameNode != NULL && xl6SemIsSpecialName(nameNode->text) ) {
        xl6SemErrorNode(ctx, nameNode, "custom create/destroy 必须使用专用声明形式");
        ok = 0;
    }
    if ( node->childCount > 1 && !xl6SemCheckParamListOnly(ctx, node->children[1], 0) ) {
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

static int xl6SemCheckCustomMemberConflicts(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* body;
    size_t i;
    size_t j;
    int ok;
    int destroyCount;

    if ( node->childCount < 1 ) {
        return 1;
    }

    body = node->children[0];
    ok = 1;
    destroyCount = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        XL6_AstNode* member = body->children[i];

        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL &&
             member->childCount > 0 &&
             member->children[0] != NULL &&
             xl6TextEquals(member->children[0]->text, "destroy") ) {
            destroyCount += 1;
            if ( destroyCount > 1 ) {
                xl6SemErrorNode(ctx, member->children[0], "custom destroy 只能声明一�?");
                ok = 0;
            }
        }
        if ( member->kind != XL6_AST_CUSTOM_EXTERN_SPECIAL &&
             member->kind != XL6_AST_CUSTOM_EXTERN_FUNC &&
             member->kind != XL6_AST_FUNC &&
             member->kind != XL6_AST_ASYNC_FUNC ) {
            continue;
        }
        for ( j = i + 1; j < body->childCount; ++j ) {
            XL6_AstNode* other = body->children[j];
            if ( other == NULL ) {
                continue;
            }
            if ( xl6SemMembersShareOverloadGroup(member, other) &&
                 xl6SemOverloadShapeConflicts(ctx, member, other) ) {
                xl6SemErrorNode(ctx, other, "重复或歧义的 custom 成员重载签名");
                ok = 0;
            }
            if ( (member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL || member->kind == XL6_AST_CUSTOM_EXTERN_FUNC) &&
                 (other->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL || other->kind == XL6_AST_CUSTOM_EXTERN_FUNC) &&
                 xl6TextEquals(member->text, other->text) ) {
                xl6SemErrorNode(ctx, other, "同一 custom 内不能重复绑定同一�?C 符号");
                ok = 0;
            }
        }
    }
    return ok;
}

static int xl6SemCheckCustom(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* body;
    size_t i;
    int ok;
    const char* oldImplicitSelfName;
    XL6_AstNode* oldCurrentClass;
    XL6_AstNode* oldCurrentCustom;
    int oldInClassCreate;

    ok = xl6SemCheckTypeDeclName(ctx, node);
    body = node->childCount > 0 ? node->children[0] : NULL;

    if ( body != NULL ) {
        if ( !xl6SemCheckCustomMemberConflicts(ctx, node) ) {
            ok = 0;
        }
        for ( i = 0; i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
                if ( !xl6SemCheckCustomExternSpecial(ctx, member) ) {
                    ok = 0;
                }
            } else if ( member->kind == XL6_AST_CUSTOM_EXTERN_FUNC ) {
                if ( !xl6SemCheckCustomExternFunc(ctx, member) ) {
                    ok = 0;
                }


            } else if ( member->kind == XL6_AST_FUNC || member->kind == XL6_AST_ASYNC_FUNC ) {
                if ( member->childCount > 0 && member->children[0] != NULL && xl6SemIsSpecialName(member->children[0]->text) ) {
                    xl6SemErrorNode(ctx, member->children[0], "custom create/destroy 必须使用专用声明形式");
                    ok = 0;
                }
                oldImplicitSelfName = ctx->implicitSelfName;
                oldCurrentClass = ctx->currentClass;
                oldCurrentCustom = ctx->currentCustom;
                oldInClassCreate = ctx->inClassCreate;
                ctx->implicitSelfName = "self";
                ctx->currentClass = NULL;
                ctx->currentCustom = node;
                ctx->inClassCreate = 0;
                if ( !xl6SemCheckFunc(ctx, member) ) {
                    ok = 0;
                }
                ctx->implicitSelfName = oldImplicitSelfName;
                ctx->currentClass = oldCurrentClass;
                ctx->currentCustom = oldCurrentCustom;
                ctx->inClassCreate = oldInClassCreate;
            } else {
                xl6SemErrorNode(ctx, member, "custom 中不支持该成�?");
                ok = 0;
            }
        }
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
        if ( !xl6SemIntLiteralToInt(node->children[0], &level) ) {
            xl6SemErrorNode(ctx, node->children[0], "break/continue 层数必须是有效整�?");
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

static int xl6SemCheckAwaitExpr(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* awaitType;
    int ok;

    ok = xl6SemCheckChildrenExpr(ctx, node);
    if ( ctx->currentFunction != NULL && ctx->currentFunction->kind != XL6_AST_ASYNC_FUNC ) {
        xl6SemErrorNode(ctx, node, "await can only be used at top level or inside async func");
        ok = 0;
    }
    awaitType = xl6SemCloneAwaitResultType(ctx, node);
    if ( awaitType == NULL ) {
        xl6SemErrorNode(ctx, node, "await 只能用于 future<T> �?task<T> 表达�?");
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
        xl6SemErrorNode(ctx, node, "yield 必须位于返回 generator<T> 的函数体�?");
        ok = 0;
    } else {
        yieldType = xl6SemCloneSingleGenericArgType(ctx, ctx->currentReturns->children[0], "generator");
        if ( yieldType == NULL ) {
            xl6SemErrorNode(ctx, ctx->currentReturns->children[0], "generator 必须写成 generator<T>");
            ok = 0;
        } else if ( node->childCount > 0 && !xl6SemExprCanMatchParamType(ctx, node->children[0], yieldType) ) {
            xl6SemErrorNode(ctx, node->children[0], "yield 表达式类型和 generator<T> 元素类型不匹�?");
            ok = 0;
        }
        xl6AstFree(yieldType);
    }
    if ( !xl6SemCheckChildrenExpr(ctx, node) ) {
        ok = 0;
    }
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
        xl6SemErrorNode(ctx, node, "generator 函数中的 return 不能携带返回�?");
        ok = 0;
    }
    if ( ctx->generatorDepth > 0 ) {
        if ( !xl6SemCheckChildrenExpr(ctx, node) ) {
            ok = 0;
        }
        return ok;
    }
    if ( ctx->currentReturns == NULL ) {
        xl6SemErrorNode(ctx, node, "return 必须位于函数或构�?析构体内");
        ok = 0;
    } else {
        expected = (int)ctx->currentReturns->childCount;
        actual = (int)node->childCount;
        if ( expected != actual ) {
            xl6SemErrorNode(ctx, node, "return 返回值数量和函数声明不匹�?");
            ok = 0;
        }
        for ( i = 0; i < expected && i < actual; ++i ) {
            if ( xl6SemExprIsMultiReturnCall(ctx, node->children[i]) ) {
                xl6SemErrorNode(ctx, node->children[i], "多返回值函数调用不能作为单�?return 表达�?");
                ok = 0;
            } else if ( !xl6SemExprCanMatchParamType(ctx, node->children[i], ctx->currentReturns->children[i]) ) {
                xl6SemErrorNode(ctx, node->children[i], "return 表达式类型和函数返回类型不匹�?");
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
    case XL6_AST_CUSTOM:
        return xl6SemCheckCustom(ctx, node) && ok;
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
            xl6SemErrorNode(ctx, child, "类型声明缺少名称");
            ok = 0;
            continue;
        }

        oldType = xl6TypeRegistryFind(&ctx->types, child->text);
        if ( oldType != NULL ) {
            if ( oldType->kind == XL6_TYPE_BUILTIN ) {
                xl6SemErrorNode(ctx, child, "自定义类型名不能和内置类型名重复");
            } else {
                xl6SemErrorNode(ctx, child, "重复声明自定义类�?");
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
            typeKind = child->kind == XL6_AST_CLASS ? XL6_TYPE_CLASS : XL6_TYPE_CUSTOM;
            typeParams = child->childCount > 2 ? child->children[2] : NULL;
            allowGeneric = child->kind == XL6_AST_CLASS && typeParams != NULL && typeParams->childCount > 0;
            genericArity = allowGeneric ? (int)typeParams->childCount : 0;
        }
        if ( !xl6TypeRegistryAdd(&ctx->types, child->text, typeKind, allowGeneric, genericArity, child) ) {
            xl6SemErrorNode(ctx, child, "注册自定义类型失�?");
            ok = 0;
        }
    }
    return ok;
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
           node->kind != XL6_AST_TYPE_ALIAS &&
           node->kind != XL6_AST_EXTERN_TYPE_ALIAS &&
           node->kind != XL6_AST_CUSTOM &&
           node->kind != XL6_AST_UNION &&
           node->kind != XL6_AST_CLASS;
}

static int xl6SemCheckMainEntry(XL6_SemContext* ctx)
{
    size_t i;
    int ok;
    int mainCount;
    int hasTopLevel;
    XL6_AstNode* mainNode;

    ok = 1;
    mainCount = 0;
    hasTopLevel = 0;
    mainNode = NULL;

    if ( ctx == NULL || ctx->root == NULL ) {
        return 1;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* child = ctx->root->children[i];
        XL6_AstNode* nameNode;

        if ( xl6SemIsTopLevelRuntimeStmt(child) ) {
            hasTopLevel = 1;
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
            xl6SemErrorNode(ctx, nameNode, "main 入口必须是普�?xlang 函数，不能是 extern �?export");
            ok = 0;
        }
    }

    if ( mainCount > 1 ) {
        xl6SemErrorNode(ctx, mainNode, "一个编译目标只能声明一�?xlang main 函数");
        ok = 0;
    }
    if ( mainNode != NULL && hasTopLevel ) {
        xl6SemErrorNode(ctx, mainNode, "显式 main 函数不能和顶层执行语句同时存�?");
        ok = 0;
    }
    if ( mainNode != NULL ) {
        XL6_AstNode* params = mainNode->childCount > 1 ? mainNode->children[1] : NULL;
        XL6_AstNode* returns = mainNode->childCount > 2 ? mainNode->children[2] : NULL;
        XL6_AstNode* typeParams = mainNode->childCount > 4 ? mainNode->children[4] : NULL;

        if ( typeParams != NULL && typeParams->childCount > 0 ) {
            xl6SemErrorNode(ctx, typeParams, "main 入口不能是泛型函�?");
            ok = 0;
        }
        if ( params == NULL || params->childCount != 1 || !xl6SemParamIsArrayVararg(params->children[0]) ) {
            xl6SemErrorNode(ctx, mainNode, "main 入口参数必须�?...，由 arg 接收 argc/argv");
            ok = 0;
        }
        if ( returns != NULL && returns->childCount > 1 ) {
            xl6SemErrorNode(ctx, returns, "main 入口不能返回多返回�?");
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
        } else if ( xl6SemIsExternalVarDecl(child) ) {
            if ( !xl6SemDeclareExternalVar(&ctx, child) ) {
                ok = 0;
            }
        }
    }

    if ( !xl6SemCheckDocsRecursive(&ctx, root) ) {
        ok = 0;
    }

    for ( i = 0; i < root->childCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child->kind == XL6_AST_IMPORT || xl6SemIsFuncDecl(child) || xl6SemIsTypeDecl(child) ) {
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
        } else if ( child->kind == XL6_AST_CUSTOM ) {
            if ( !xl6SemCheckCustom(&ctx, child) ) {
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
