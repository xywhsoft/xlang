#include "src/stdlib/xl6_stdlib.h"
#include "src/stdlib/xl6_stdlib_data.h"

#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"

#include <stdio.h>

typedef struct XL6_StdlibTextBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_StdlibTextBuilder;

#define XL6_STDLIB_FUNC_INDEX_CAPACITY 2048u

#include "src/stdlib/xl6_stdlib_deps.inc"
#include "src/stdlib/xl6_stdlib_func_index.inc"

typedef char XL6_StdlibFuncIndexCapacityCheck[
    XL6_STDLIB_FUNC_COUNT * 2u <= XL6_STDLIB_FUNC_INDEX_CAPACITY ? 1 : -1];
typedef char XL6_StdlibFuncIndexWidthCheck[
    XL6_STDLIB_FUNC_COUNT < 65535u ? 1 : -1];

static int xl6StdlibTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static char* xl6StdlibDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

static void xl6StdlibBuilderInit(XL6_StdlibTextBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6StdlibBuilderFree(XL6_StdlibTextBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6StdlibBuilderGrow(XL6_StdlibTextBuilder* builder, size_t addLen)
{
    char* newData;
    size_t required;
    size_t newCapacity;

    if ( builder == NULL || !builder->ok ) {
        return 0;
    }
    required = builder->len + addLen + 1;
    if ( required <= builder->capacity ) {
        return 1;
    }
    newCapacity = builder->capacity == 0 ? 512 : builder->capacity;
    while ( newCapacity < required ) {
        newCapacity *= 2;
    }
    newData = (char*)xrtRealloc(builder->data, newCapacity);
    if ( newData == NULL ) {
        builder->ok = 0;
        return 0;
    }
    builder->data = newData;
    builder->capacity = newCapacity;
    return 1;
}

static void xl6StdlibBuilderAppendN(XL6_StdlibTextBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !builder->ok ) {
        return;
    }
    if ( !xl6StdlibBuilderGrow(builder, len) ) {
        return;
    }
    memcpy(builder->data + builder->len, text, len);
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6StdlibBuilderAppend(XL6_StdlibTextBuilder* builder, const char* text)
{
    xl6StdlibBuilderAppendN(builder, text, text != NULL ? strlen(text) : 0);
}

static void xl6StdlibBuilderAppendQuoted(XL6_StdlibTextBuilder* builder, const char* text)
{
    const unsigned char* p;

    xl6StdlibBuilderAppend(builder, "\"");
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
            char tmp[7];
            if ( *p == '"' || *p == '\\' ) {
                tmp[0] = '\\';
                tmp[1] = (char)*p;
                tmp[2] = '\0';
                xl6StdlibBuilderAppend(builder, tmp);
            } else if ( *p < 0x20 ) {
                snprintf(tmp, sizeof(tmp), "\\x%02X", (unsigned)*p);
                xl6StdlibBuilderAppend(builder, tmp);
            } else {
                xl6StdlibBuilderAppendN(builder, (const char*)p, 1);
            }
        }
    }
    xl6StdlibBuilderAppend(builder, "\"");
}

static unsigned long long xl6StdlibFuncKeyHash(const char* ns, const char* name)
{
    const unsigned char* p;
    unsigned long long hash = 1469598103934665603ULL;

    for ( p = (const unsigned char*)(ns != NULL ? ns : ""); *p != '\0'; ++p ) {
        hash ^= *p;
        hash *= 1099511628211ULL;
    }
    hash ^= 0xFFu;
    hash *= 1099511628211ULL;
    for ( p = (const unsigned char*)(name != NULL ? name : ""); *p != '\0'; ++p ) {
        hash ^= *p;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static const XL6_StdlibFunc* xl6StdlibFindFunc(const char* ns, const char* name, size_t* outIndex)
{
    size_t slot;
    size_t start;

    if ( name == NULL ) {
        return NULL;
    }
    slot = (size_t)(xl6StdlibFuncKeyHash(ns, name) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u));
    start = slot;
    while ( xl6StdlibFuncIndex[slot] != 0 ) {
        size_t index = (size_t)xl6StdlibFuncIndex[slot] - 1u;
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[index].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[index].name, name) ) {
            if ( outIndex != NULL ) {
                *outIndex = index;
            }
            return &xl6StdlibFuncs[index];
        }
        slot = (slot + 1u) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u);
        if ( slot == start ) {
            break;
        }
    }
    return NULL;
}

typedef struct XL6_StdlibImplicitMethod {
    const char* typeName;
    const char* methodName;
    const char* functionName;
} XL6_StdlibImplicitMethod;

/*
    隐性基础类只复用标准库函数，不另造一套实现。
    表中仅放适合作为实例方法、且第一个参数就是接收者的函数。
*/
static const XL6_StdlibImplicitMethod xl6StdlibImplicitMethods[] = {
    {"string", "byteLen", "byteLen"},
    {"string", "compare", "compare"},
    {"string", "equals", "equals"},
    {"string", "contains", "contains"},
    {"string", "indexOf", "indexOf"},
    {"string", "lastIndexOf", "lastIndexOf"},
    {"string", "count", "count"},
    {"string", "startsWith", "startsWith"},
    {"string", "endsWith", "endsWith"},
    {"string", "hasAny", "hasAny"},
    {"string", "like", "like"},
    {"string", "approx", "approx"},
    {"string", "sim", "sim"},
    {"string", "isUtf8", "isUtf8"},
    {"string", "lower", "lower"},
    {"string", "toLower", "lower"},
    {"string", "upper", "upper"},
    {"string", "toUpper", "upper"},
    {"string", "trim", "trim"},
    {"string", "ltrim", "ltrim"},
    {"string", "rtrim", "rtrim"},
    {"string", "filter", "filter"},
    {"string", "replace", "replace"},
    {"string", "split", "split"},
    {"string", "slice", "slice"},
    {"string", "left", "left"},
    {"string", "right", "right"},
    {"string", "mid", "mid"},
    {"string", "charAt", "charAt"},
    {"string", "byteAt", "byteAt"},
    {"string", "repeat", "repeat"}
};

static const XL6_StdlibImplicitMethod* xl6StdlibFindImplicitMethod(
    const char* typeName,
    const char* methodName)
{
    size_t i;

    for ( i = 0; i < sizeof(xl6StdlibImplicitMethods) / sizeof(xl6StdlibImplicitMethods[0]); ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibImplicitMethods[i].typeName, typeName) &&
             xl6StdlibTextEquals(xl6StdlibImplicitMethods[i].methodName, methodName) ) {
            return &xl6StdlibImplicitMethods[i];
        }
    }
    return NULL;
}

const char* xl6StdlibImplicitMethodInternalName(const char* typeName, const char* methodName)
{
    const XL6_StdlibImplicitMethod* method = xl6StdlibFindImplicitMethod(typeName, methodName);
    const XL6_StdlibFunc* func;

    if ( method == NULL ) {
        return NULL;
    }
    func = xl6StdlibFindFunc(method->typeName, method->functionName, NULL);
    return func != NULL ? func->internalName : NULL;
}

static const XL6_StdlibConst* xl6StdlibFindConst(const char* ns, const char* name)
{
    size_t i;

    for ( i = 0; i < XL6_STDLIB_CONST_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibConsts[i].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibConsts[i].name, name) ) {
            return &xl6StdlibConsts[i];
        }
    }
    return NULL;
}

static int xl6StdlibNamespaceExists(const char* ns)
{
    size_t i;

    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) ) {
            return 1;
        }
    }
    for ( i = 0; i < XL6_STDLIB_CONST_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibConsts[i].ns, ns) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibMarkFuncUsed(int* used, const char* ns, const char* name)
{
    size_t slot;
    size_t start;
    int changed;

    if ( used == NULL || name == NULL ) {
        return 0;
    }
    changed = 0;
    slot = (size_t)(xl6StdlibFuncKeyHash(ns, name) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u));
    start = slot;
    while ( xl6StdlibFuncIndex[slot] != 0 ) {
        size_t index = (size_t)xl6StdlibFuncIndex[slot] - 1u;
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[index].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[index].name, name) ) {
            /* 同名函数可能有多个重载，名称被使用时必须全部注入。 */
            if ( !used[index] ) {
                used[index] = 1;
                changed = 1;
            }
        }
        slot = (slot + 1u) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u);
        if ( slot == start ) {
            break;
        }
    }
    return changed;
}

static int xl6StdlibFuncUsed(const int* used, const char* ns, const char* name)
{
    size_t slot;
    size_t start;

    if ( used == NULL || name == NULL ) {
        return 0;
    }
    slot = (size_t)(xl6StdlibFuncKeyHash(ns, name) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u));
    start = slot;
    while ( xl6StdlibFuncIndex[slot] != 0 ) {
        size_t index = (size_t)xl6StdlibFuncIndex[slot] - 1u;
        if ( used[index] &&
             xl6StdlibTextEquals(xl6StdlibFuncs[index].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[index].name, name) ) {
            return 1;
        }
        slot = (slot + 1u) & (XL6_STDLIB_FUNC_INDEX_CAPACITY - 1u);
        if ( slot == start ) {
            break;
        }
    }
    return 0;
}

static int xl6StdlibIsCompilerIntrinsicMember(const char* ns, const char* name)
{
    if ( !xl6StdlibTextEquals(ns, "task") ) {
        return 0;
    }
    return xl6StdlibTextEquals(name, "spawn") ||
           xl6StdlibTextEquals(name, "cancel") ||
           xl6StdlibTextEquals(name, "whenAny") ||
           xl6StdlibTextEquals(name, "all") ||
           xl6StdlibTextEquals(name, "race");
}

static void xl6StdlibApplyDeps(int* used)
{
    size_t i;
    int changed;

    if ( used == NULL ) {
        return;
    }

    /*
        依赖关系是有向图，不依赖表项顺序。
        反复传播到固定点后，间接依赖和循环依赖都会得到完整且稳定的结果。
    */
    do {
        changed = 0;
        for ( i = 0; i < XL6_STDLIB_DEPENDENCY_COUNT; ++i ) {
            const XL6_StdlibDependency* dependency = &xl6StdlibDependencies[i];

            if ( xl6StdlibFuncUsed(
                    used,
                    dependency->sourceNamespace,
                    dependency->sourceName) ) {
                changed |= xl6StdlibMarkFuncUsed(
                    used,
                    dependency->targetNamespace,
                    dependency->targetName);
            }
        }
    } while ( changed );
}
static int xl6StdlibAppendPath(char* out, size_t outSize, const char* text)
{
    size_t oldLen;
    size_t addLen;

    if ( out == NULL || outSize == 0 || text == NULL ) {
        return 0;
    }
    oldLen = strlen(out);
    addLen = strlen(text);
    if ( oldLen + addLen + 1 > outSize ) {
        return 0;
    }
    memcpy(out + oldLen, text, addLen + 1);
    return 1;
}

static int xl6StdlibReceiverPath(const XL6_AstNode* node, char* out, size_t outSize)
{
    if ( out == NULL || outSize == 0 || node == NULL ) {
        return 0;
    }
    out[0] = '\0';
    if ( node->kind == XL6_AST_NAME ) {
        return xl6StdlibAppendPath(out, outSize, node->text);
    }
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        if ( !xl6StdlibReceiverPath(node->children[0], out, outSize) ) {
            return 0;
        }
        if ( !xl6StdlibAppendPath(out, outSize, ".") ) {
            return 0;
        }
        return xl6StdlibAppendPath(out, outSize, node->text);
    }
    return 0;
}

static void xl6StdlibRewriteMemberToName(XL6_AstNode* node, const char* name)
{
    size_t i;

    if ( node == NULL || name == NULL ) {
        return;
    }

    /* 释放原成员访问结果，保留节点位置用于诊断。 */
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);

    node->kind = XL6_AST_NAME;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->text = xl6StdlibDupText(name);
}

static void xl6StdlibRewriteMemberToLiteral(XL6_AstNode* node, XL6_AstKind kind, const char* value)
{
    size_t i;
    int negativeInt;

    if ( node == NULL || value == NULL ) {
        return;
    }
    negativeInt = kind == XL6_AST_INT && value[0] == '-' && value[1] != '\0';
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);

    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    if ( negativeInt ) {
        XL6_AstNode* child = xl6AstNewText(XL6_AST_INT, value + 1, strlen(value + 1), NULL);
        if ( child == NULL ) {
            node->kind = kind;
            node->text = NULL;
            return;
        }
        child->line = node->line;
        child->column = node->column;
        node->kind = XL6_AST_UNARY;
        node->text = xl6StdlibDupText("-");
        xl6AstAddChild(node, child);
        return;
    }
    node->kind = kind;
    node->text = xl6StdlibDupText(value);
}

typedef struct XL6_StdlibNameScope {
    const struct XL6_StdlibNameScope* parent;
    const char** names;
    size_t count;
    size_t capacity;
} XL6_StdlibNameScope;

static void xl6StdlibScopeInit(XL6_StdlibNameScope* scope, const XL6_StdlibNameScope* parent)
{
    if ( scope == NULL ) {
        return;
    }
    memset(scope, 0, sizeof(*scope));
    scope->parent = parent;
}

static void xl6StdlibScopeFree(XL6_StdlibNameScope* scope)
{
    if ( scope == NULL ) {
        return;
    }
    xrtFree(scope->names);
    memset(scope, 0, sizeof(*scope));
}

static int xl6StdlibScopeContains(const XL6_StdlibNameScope* scope, const char* name)
{
    size_t i;

    for ( ; scope != NULL; scope = scope->parent ) {
        for ( i = 0; i < scope->count; ++i ) {
            if ( xl6StdlibTextEquals(scope->names[i], name) ) {
                return 1;
            }
        }
    }
    return 0;
}

static int xl6StdlibScopeAdd(XL6_StdlibNameScope* scope, const char* name)
{
    const char** names;
    size_t capacity;

    if ( scope == NULL || name == NULL || name[0] == '\0' || xl6StdlibScopeContains(scope, name) ) {
        return 1;
    }
    if ( scope->count == scope->capacity ) {
        capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        names = (const char**)xrtRealloc(scope->names, capacity * sizeof(const char*));
        if ( names == NULL ) {
            return 0;
        }
        scope->names = names;
        scope->capacity = capacity;
    }
    scope->names[scope->count++] = name;
    return 1;
}

static int xl6StdlibScopeAddParams(XL6_StdlibNameScope* scope, const XL6_AstNode* params)
{
    size_t i;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && !xl6StdlibScopeAdd(scope, param->text) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6StdlibScopeAddDecl(XL6_StdlibNameScope* scope, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        return xl6StdlibScopeAdd(scope, node->text);
    }
    if ( node->kind == XL6_AST_MULTI_VAR_DECL && node->childCount > 0 ) {
        const XL6_AstNode* names = node->children[0];
        for ( i = 0; names != NULL && i < names->childCount; ++i ) {
            if ( names->children[i] != NULL && !xl6StdlibScopeAdd(scope, names->children[i]->text) ) {
                return 0;
            }
        }
    }
    return 1;
}

static const char* xl6StdlibReceiverRootName(const XL6_AstNode* node)
{
    while ( node != NULL && (node->kind == XL6_AST_MEMBER || node->kind == XL6_AST_SAFE_MEMBER) && node->childCount > 0 ) {
        node = node->children[0];
    }
    return node != NULL && node->kind == XL6_AST_NAME ? node->text : NULL;
}

static int xl6StdlibRewriteRefsScoped(
    XL6_AstNode* node,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope);

static int xl6StdlibRewriteCallable(
    XL6_AstNode* node,
    const XL6_AstNode* params,
    XL6_AstNode* body,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope)
{
    XL6_StdlibNameScope functionScope;
    size_t i;
    int ok;

    xl6StdlibScopeInit(&functionScope, scope);
    ok = xl6StdlibScopeAddParams(&functionScope, params);
    for ( i = 0; ok && i < node->childCount; ++i ) {
        if ( node->children[i] != body &&
             !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &functionScope) ) {
            ok = 0;
        }
    }
    if ( ok && body != NULL ) {
        ok = xl6StdlibRewriteRefsScoped(body, used, diags, &functionScope);
    }
    xl6StdlibScopeFree(&functionScope);
    return ok;
}

static int xl6StdlibRewriteRefsScoped(
    XL6_AstNode* node,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope)
{
    char ns[128];
    const char* rootName;
    size_t index;
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }

    if ( node->kind == XL6_AST_MODULE || node->kind == XL6_AST_BLOCK ) {
        XL6_StdlibNameScope blockScope;

        xl6StdlibScopeInit(&blockScope, scope);
        ok = 1;
        if ( node->kind == XL6_AST_MODULE ) {
            for ( i = 0; i < node->childCount; ++i ) {
                const XL6_AstNode* item = node->children[i];
                if ( !xl6StdlibScopeAddDecl(&blockScope, item) ) {
                    ok = 0;
                }
                if ( item != NULL &&
                     (item->kind == XL6_AST_FUNC || item->kind == XL6_AST_ASYNC_FUNC ||
                      item->kind == XL6_AST_EXTERN_FUNC || item->kind == XL6_AST_EXPORT_FUNC) &&
                     item->childCount > 0 && item->children[0] != NULL &&
                     !xl6StdlibScopeAdd(&blockScope, item->children[0]->text) ) {
                    ok = 0;
                }
            }
        }
        for ( i = 0; i < node->childCount; ++i ) {
            if ( !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &blockScope) ||
                 (node->kind == XL6_AST_BLOCK && !xl6StdlibScopeAddDecl(&blockScope, node->children[i])) ) {
                ok = 0;
            }
        }
        xl6StdlibScopeFree(&blockScope);
        return ok;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_GET || node->kind == XL6_AST_LET ) {
        const XL6_AstNode* params = node->childCount > 1 ? node->children[1] : NULL;
        XL6_AstNode* body = node->childCount > 3 ? node->children[3] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_CLASS && node->childCount > 1 ) {
        const XL6_AstNode* body = node->children[1];
        XL6_StdlibNameScope classScope;

        xl6StdlibScopeInit(&classScope, scope);
        ok = 1;
        for ( i = 0; body != NULL && i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member != NULL && member->kind == XL6_AST_FIELD &&
                 !xl6StdlibScopeAdd(&classScope, member->text) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && i < node->childCount; ++i ) {
            if ( node->children[i] != body &&
                 !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &classScope) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && body != NULL && i < body->childCount; ++i ) {
            if ( !xl6StdlibRewriteRefsScoped(body->children[i], used, diags, &classScope) ) {
                ok = 0;
            }
        }
        xl6StdlibScopeFree(&classScope);
        return ok;
    }
    if ( node->kind == XL6_AST_FOR && node->childCount > 2 ) {
        const XL6_AstNode* targets = node->children[0];
        XL6_StdlibNameScope loopScope;

        ok = xl6StdlibRewriteRefsScoped(node->children[1], used, diags, scope);
        xl6StdlibScopeInit(&loopScope, scope);
        for ( i = 0; ok && targets != NULL && i < targets->childCount; ++i ) {
            if ( targets->children[i] != NULL && !xl6StdlibScopeAdd(&loopScope, targets->children[i]->text) ) {
                ok = 0;
            }
        }
        if ( ok ) {
            ok = xl6StdlibRewriteRefsScoped(node->children[2], used, diags, &loopScope);
        }
        xl6StdlibScopeFree(&loopScope);
        return ok;
    }
    if ( node->kind == XL6_AST_CATCH && node->childCount > 1 ) {
        XL6_StdlibNameScope catchScope;
        const XL6_AstNode* catchName;
        XL6_AstNode* catchBlock;

        xl6StdlibScopeInit(&catchScope, scope);
        catchName = node->childCount == 3 ? node->children[1] : node->children[0];
        catchBlock = node->childCount == 3 ? node->children[2] : node->children[1];
        ok = xl6StdlibScopeAdd(&catchScope, catchName != NULL ? catchName->text : NULL) &&
             xl6StdlibRewriteRefsScoped(catchBlock, used, diags, &catchScope);
        xl6StdlibScopeFree(&catchScope);
        return ok;
    }

    /* 全局标准库函数：print(...) -> __xl_std_print(...)。 */
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        XL6_AstNode* callee = node->children[0];
        const XL6_StdlibFunc* func = xl6StdlibScopeContains(scope, callee->text) ?
                                    NULL : xl6StdlibFindFunc("", callee->text, &index);
        if ( func != NULL ) {
            xl6StdlibMarkFuncUsed(used, "", callee->text);
            xrtFree(callee->text);
            callee->text = xl6StdlibDupText(func->internalName);
            if ( callee->text == NULL ) {
                return 0;
            }
        }
    }

    /* 标准库命名空间：math.log -> __xl_std_math_log。 */
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        rootName = xl6StdlibReceiverRootName(node->children[0]);
        if ( !xl6StdlibScopeContains(scope, rootName) &&
             xl6StdlibReceiverPath(node->children[0], ns, sizeof(ns)) ) {
            const XL6_StdlibConst* constant = xl6StdlibFindConst(ns, node->text);
            if ( constant != NULL ) {
                xl6StdlibRewriteMemberToLiteral(node, constant->kind, constant->value);
                return node->text != NULL;
            }
            const XL6_StdlibFunc* func = xl6StdlibFindFunc(ns, node->text, &index);
            if ( func != NULL ) {
                xl6StdlibMarkFuncUsed(used, ns, node->text);
                xl6StdlibRewriteMemberToName(node, func->internalName);
                return node->text != NULL;
            }
            if ( xl6StdlibNamespaceExists(ns) &&
                 !xl6StdlibIsCompilerIntrinsicMember(ns, node->text) ) {
                if ( diags != NULL ) {
                    xl6DiagAdd(diags, node->line, node->column, "standard library namespace has no such symbol");
                }
                return 0;
            }
        }

        /* 接收者类型尚未确定，此处只登记隐性方法所需的标准库实现。 */
        {
            const XL6_StdlibImplicitMethod* method = xl6StdlibFindImplicitMethod("string", node->text);
            if ( method != NULL ) {
                xl6StdlibMarkFuncUsed(used, method->typeName, method->functionName);
            }
        }
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, scope) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6StdlibNodeUsesTypeName(const XL6_AstNode* node, const char* typeName)
{
    size_t i;

    if ( node == NULL || typeName == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6StdlibTextEquals(node->text, typeName) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6StdlibTextEquals(node->children[0]->text, typeName) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6StdlibNodeUsesTypeName(node->children[i], typeName) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsRandomClass(const XL6_AstNode* root, const int* used)
{
    (void)used;
    return xl6StdlibNodeUsesTypeName(root, "Random");
}

static int xl6StdlibNeedsBufferClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    /* Buffer 返回值是标准库公开面的类型依赖，统一从注册表推导。 */
    if ( used != NULL ) {
        for ( index = 0; index < xl6StdlibFuncCount; ++index ) {
            if ( used[index] &&
                 xl6StdlibTextEquals(xl6StdlibFuncs[index].returnType, "Buffer") ) {
                return 1;
            }
        }

    }
    if ( xl6StdlibFindFunc("encoding", "toBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("encoding", "convertBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "readBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "writeBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "appendBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    return xl6StdlibNodeUsesTypeName(root, "Buffer");
}

static int xl6StdlibNeedsXidClass(const XL6_AstNode* root, const int* used)
{
    static const char* names[] = {"make", "decode", "encode", "compare"};
    size_t i;
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Xid") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    for ( i = 0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        if ( xl6StdlibFindFunc("id", names[i], &index) != NULL && used[index] ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsNetClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TcpStream") ||
         xl6StdlibNodeUsesTypeName(root, "TcpServer") ||
         xl6StdlibNodeUsesTypeName(root, "UdpSocket") ||
         xl6StdlibNodeUsesTypeName(root, "UdpPacket") ||
         xl6StdlibNodeUsesTypeName(root, "NetAddress") ||
         xl6StdlibNodeUsesTypeName(root, "NetProxy") ||
         xl6StdlibNodeUsesTypeName(root, "NetEngine") ||
         xl6StdlibNodeUsesTypeName(root, "TcpConnectConfig") ||
         xl6StdlibNodeUsesTypeName(root, "TcpListenConfig") ||
         xl6StdlibNodeUsesTypeName(root, "UdpConfig") ||
         xl6StdlibNodeUsesTypeName(root, "TlsConfig") ||
         xl6StdlibNodeUsesTypeName(root, "TlsSession") ||
         xl6StdlibNodeUsesTypeName(root, "TlsResume") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("tcp", "connect", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "connectAsync", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "listen", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("udp", "bind", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("net", "resolve", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("net", "proxy", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("net", "engine", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "connectConfig", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "listenConfig", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("udp", "config", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsNetTlsClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "TlsConfig") ||
           xl6StdlibNodeUsesTypeName(root, "TlsSession") ||
           xl6StdlibNodeUsesTypeName(root, "TlsResume");
}

static int xl6StdlibNeedsFileInfoClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "FileInfo") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "info", &index) != NULL && used[index];
}

static int xl6StdlibNeedsFileClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "File") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "open", &index) != NULL && used[index];
}

static int xl6StdlibNeedsRegexClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Regex") ||
         xl6StdlibNodeUsesTypeName(root, "RegexMatch") ||
         xl6StdlibNodeUsesTypeName(root, "RegexSet") ||
         xl6StdlibNodeUsesTypeName(root, "RegexBuilder") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("regex", "compile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("regex", "builder", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("regex", "match", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("regex", "set", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsProcessClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    (void)root;
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("process", "run", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "runFile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "cmd", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "shell", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "pipeline", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "start", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "startFile", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsTccClass(const XL6_AstNode* root, const int* used)
{
    static const char* names[] = {
        "Context",
        "run",
        "runFile",
        "compile",
        "compileFile",
        "vfsMount",
        "vfsClear"
    };
    size_t i;
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TccContext") ||
         xl6StdlibNodeUsesTypeName(root, "TccHandler") ) {
        return 1;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6StdlibFindFunc("tcc", names[i], &index) != NULL && used[index] ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsXlangClass(const XL6_AstNode* root, const int* used)
{
    static const char* names[] = {
        "Context",
        "run",
        "runFile"
    };
    size_t i;
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "XLangContext") ||
         xl6StdlibNodeUsesTypeName(root, "XLangHandler") ) {
        return 1;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6StdlibFindFunc("xlang", names[i], &index) != NULL && used[index] ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsLoggerClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Logger") ||
         xl6StdlibNodeUsesTypeName(root, "LogAppender") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("log", "create", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("log", "default", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsSignalClass(const XL6_AstNode* root, const int* used)
{
    if ( xl6StdlibNodeUsesTypeName(root, "SignalHandle") ) {
        return 1;
    }
    return xl6StdlibFuncUsed(used, "signal", "on") ||
           xl6StdlibFuncUsed(used, "signal", "once") ||
           xl6StdlibFuncUsed(used, "signal", "sendTo") ||
           xl6StdlibFuncUsed(used, "signal", "cancelOn");
}

static int xl6StdlibNeedsTemplateClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Template") ||
         xl6StdlibNodeUsesTypeName(root, "TemplateEngine") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("template", "compile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "engine", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "load", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "render", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "renderFile", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsCryptoClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Sha1") ||
         xl6StdlibNodeUsesTypeName(root, "Sha256") ||
         xl6StdlibNodeUsesTypeName(root, "Sha384") ||
         xl6StdlibNodeUsesTypeName(root, "Sha512") ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha1Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha256Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha384Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha512Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsHttpServerClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "HttpRequest") ||
         xl6StdlibNodeUsesTypeName(root, "HttpResponse") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("http.server", "getRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "postRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "anyRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "routeStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "postStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "anyStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appRouteStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appPostStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appAnyStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "error", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appError", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsTaskGroupClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TaskGroup") ) {
        return 1;
    }
    return used != NULL &&
           xl6StdlibFindFunc("task", "group", &index) != NULL &&
           used[index];
}

static int xl6StdlibNeedsJsonClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "JsonWriter") ||
         xl6StdlibNodeUsesTypeName(root, "XsonOptions") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFuncUsed(used, "xson", "parse") ||
         xl6StdlibFuncUsed(used, "xson", "valid") ||
         xl6StdlibFuncUsed(used, "xson", "read") ||
         xl6StdlibFuncUsed(used, "xson", "stringify") ||
         xl6StdlibFuncUsed(used, "xson", "write") ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("json", "writer", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("xson", "options", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsAsyncFileClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "AsyncFile") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "openAsync", &index) != NULL && used[index];
}

static int xl6StdlibNeedsHttpClientClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "HttpClient") ||
         xl6StdlibNodeUsesTypeName(root, "HttpClientRequest") ||
         xl6StdlibNodeUsesTypeName(root, "HttpClientResponse") ||
         xl6StdlibNodeUsesTypeName(root, "HttpDiagnostics") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFuncUsed(used, "http", "client") ||
         xl6StdlibFuncUsed(used, "http", "copyChunk") ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "request", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "get", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "post", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsHttpSemanticsClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "HttpHeaders") ||
           xl6StdlibNodeUsesTypeName(root, "SearchParams") ||
           xl6StdlibNodeUsesTypeName(root, "HttpContext") ||
           xl6StdlibNodeUsesTypeName(root, "HttpBody") ||
           xl6StdlibNodeUsesTypeName(root, "FormData") ||
           xl6StdlibNodeUsesTypeName(root, "FormDataEncoded") ||
           xl6StdlibNodeUsesTypeName(root, "CookieJar");
}

static int xl6StdlibNeedsWebSocketClass(const XL6_AstNode* root)
{
    static const char* names[] = {
        "WebSocketCloseInfo", "WebSocketError", "WebSocketHandshakeMessage",
        "WebSocketHandshakeResponse", "WebSocketWriter", "WebSocketClientOptions",
        "WebSocketConnection", "WebSocketClient", "WebSocketServerOptions",
        "WebSocketServer"
    };
    size_t i;
    for ( i = 0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        if ( xl6StdlibNodeUsesTypeName(root, names[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6StdlibAppendFuncDecl(XL6_StdlibTextBuilder* builder, const XL6_StdlibFunc* func)
{
    size_t i;

    if ( builder == NULL || func == NULL ) {
        return;
    }

    if ( func->sourceBody != NULL ) {
        xl6StdlibBuilderAppend(builder, func->sourceBody);
        return;
    }

    xl6StdlibBuilderAppend(builder, "extern ");
    xl6StdlibBuilderAppendQuoted(builder, func->cSymbol);
    xl6StdlibBuilderAppend(builder, " func ");
    xl6StdlibBuilderAppend(builder, func->internalName);
    xl6StdlibBuilderAppend(builder, "(");
    for ( i = 0; i < func->paramCount; ++i ) {
        if ( i > 0 ) {
            xl6StdlibBuilderAppend(builder, ", ");
        }
        xl6StdlibBuilderAppend(builder, func->params[i].type);
        xl6StdlibBuilderAppend(builder, " ");
        xl6StdlibBuilderAppend(builder, func->params[i].name);
    }
    xl6StdlibBuilderAppend(builder, ")");
    if ( func->returnType != NULL && func->returnType[0] != '\0' ) {
        xl6StdlibBuilderAppend(builder, " as ");
        xl6StdlibBuilderAppend(builder, func->returnType);
    }
    xl6StdlibBuilderAppend(builder, "\n");
}

static int xl6StdlibMergeGeneratedDecls(XL6_AstNode* root, const char* generated, XL6_DiagList* diags)
{
    XL6_ParseResult result;
    int ok;

    if ( generated == NULL || generated[0] == '\0' ) {
        return 1;
    }

    xl6ParseResultInit(&result);
    ok = xl6ParseSource(generated, "<stdlib>", &result);
    if ( !ok ) {
        if ( diags != NULL && result.diags.count > 0 ) {
            size_t i;
            for ( i = 0; i < result.diags.count; ++i ) {
                xl6DiagAdd(diags, result.diags.items[i].line, result.diags.items[i].column, result.diags.items[i].message);
            }
        } else if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "standard library generated invalid extern declarations");
        }
        xl6ParseResultFree(&result);
        return 0;
    }
    if ( result.root != NULL ) {
        xl6AstMoveChildren(root, result.root);
    }
    xl6ParseResultFree(&result);
    return 1;
}

int xl6StdlibExpandModule(XL6_AstNode* root, XL6_DiagList* diags)
{
    int used[XL6_STDLIB_FUNC_COUNT];
    XL6_StdlibTextBuilder builder;
    size_t i;
    int needsRandomClass;
    int needsXidClass;
    int needsBufferClass;
    int needsNetClass;
    int needsNetTlsClass;
    int needsFileInfoClass;
    int needsFileClass;
    int needsAsyncFileClass;
    int needsRegexClass;
    int needsJsonClass;
    int needsProcessClass;
    int needsTaskGroupClass;
    int needsLoggerClass;
    int needsSignalClass;
    int needsTemplateClass;
    int needsCryptoClass;
    int needsHttpServerClass;
    int needsHttpClientClass;
    int needsHttpSemanticsClass;
    int needsWebSocketClass;
    int needsTccClass;
    int needsXlangClass;
    int ok;

    if ( root == NULL ) {
        return 1;
    }

    memset(used, 0, sizeof(used));
    ok = xl6StdlibRewriteRefsScoped(root, used, diags, NULL);
    if ( !ok ) {
        return 0;
    }
    xl6StdlibApplyDeps(used);
    needsRandomClass = xl6StdlibNeedsRandomClass(root, used);
    needsXidClass = xl6StdlibNeedsXidClass(root, used);
    needsHttpServerClass = xl6StdlibNeedsHttpServerClass(root, used);
    needsHttpClientClass = xl6StdlibNeedsHttpClientClass(root, used);
    needsHttpSemanticsClass = xl6StdlibNeedsHttpSemanticsClass(root) || needsHttpClientClass;
    needsWebSocketClass = xl6StdlibNeedsWebSocketClass(root);
    needsNetTlsClass = xl6StdlibNeedsNetTlsClass(root) || needsWebSocketClass;
    needsNetClass = xl6StdlibNeedsNetClass(root, used) || needsHttpServerClass || needsHttpClientClass || needsWebSocketClass;
    needsFileInfoClass = xl6StdlibNeedsFileInfoClass(root, used);
    needsFileClass = xl6StdlibNeedsFileClass(root, used);
    needsAsyncFileClass = xl6StdlibNeedsAsyncFileClass(root, used);
    needsRegexClass = xl6StdlibNeedsRegexClass(root, used);
    needsJsonClass = xl6StdlibNeedsJsonClass(root, used);
    needsProcessClass = xl6StdlibNeedsProcessClass(root, used);
    needsTaskGroupClass = xl6StdlibNeedsTaskGroupClass(root, used);
    needsLoggerClass = xl6StdlibNeedsLoggerClass(root, used);
    needsSignalClass = xl6StdlibNeedsSignalClass(root, used);
    needsTemplateClass = xl6StdlibNeedsTemplateClass(root, used);
    needsCryptoClass = xl6StdlibNeedsCryptoClass(root, used);
    needsBufferClass = xl6StdlibNeedsBufferClass(root, used) || needsNetClass || needsHttpServerClass || needsHttpClientClass || needsHttpSemanticsClass || needsWebSocketClass || needsFileClass || needsAsyncFileClass || needsCryptoClass || needsProcessClass;
    needsTccClass = xl6StdlibNeedsTccClass(root, used);
    needsXlangClass = xl6StdlibNeedsXlangClass(root, used);
    if ( needsRegexClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsCryptoClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsFileClass || needsProcessClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsHttpClientClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsNetClass ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "netEnginePost");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "netEnginePostDelayed");
    }
    if ( xl6StdlibFuncUsed(used, "signal", "cancelOn") ) {
        needsTaskGroupClass = 1;
    }
    if ( needsRandomClass || needsXidClass || needsBufferClass || needsNetClass || needsFileClass || needsRegexClass || needsJsonClass || needsProcessClass || needsLoggerClass || needsHttpServerClass || needsHttpClientClass || needsHttpSemanticsClass || needsWebSocketClass || needsTemplateClass || needsCryptoClass || needsXlangClass ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
    }
    xl6StdlibApplyDeps(used);

    /* 只为实际使用的标准库函数注入 extern 声明。 */
    xl6StdlibBuilderInit(&builder);
    xl6StdlibBuilderAppend(&builder, xl6StdlibCoreClassSource);
    if ( needsRandomClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRandomClassSource);
    }
    if ( needsXidClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibXidClassSource);
    }
    if ( needsBufferClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibBufferClassSource);
    }
    if ( needsLoggerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibLoggerClassSource);
    }
    if ( needsSignalClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibSignalClassSource);
    }
    if ( needsTaskGroupClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTaskGroupClassSource);
    }
    if ( needsTemplateClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTemplateClassSource);
    }
    if ( needsCryptoClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibCryptoClassSource);
    }
    if ( needsHttpSemanticsClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpSemanticsClassSource);
    }
    if ( needsWebSocketClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketClassSource);
    }
    if ( needsHttpServerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpServerClassSource);
        if ( needsWebSocketClass ) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibHttpServerWebSocketMethodSource);
        }
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpServerClassEndSource);
    }
    if ( needsHttpClientClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpClientClassSource);
    }
    if ( needsNetClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetBaseClassSource);
        if ( needsNetTlsClass ) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibNetTlsClassSource);
        }
        else {
            xl6StdlibBuilderAppend(&builder, xl6StdlibNetConfigClassSource);
        }
    }
    if ( needsFileInfoClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFileInfoClassSource);
    }
    if ( needsFileClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFileClassSource);
    }
    if ( needsAsyncFileClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibAsyncFileClassSource);
    }
    if ( needsRegexClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexClassSource);
    }
    if ( needsJsonClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibJsonClassSource);
    }
    if ( needsProcessClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibProcessClassSource);
    }
    if ( needsTccClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTccClassSource);
    }
    if ( needsXlangClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibXlangClassSource);
    }
    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( used[i] ) {
            xl6StdlibAppendFuncDecl(&builder, &xl6StdlibFuncs[i]);
        }
    }
    ok = builder.ok && xl6StdlibMergeGeneratedDecls(root, builder.data, diags);
    xl6StdlibBuilderFree(&builder);
    return ok;
}
