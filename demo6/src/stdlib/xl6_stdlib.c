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

#define XL6_STDLIB_FUNC_INDEX_CAPACITY 4096u

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
    return xrtStrDupN(src, strlen(src));
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
    /* Ordinary generic source helper, not a second native implementation. */
    const char* sourceName;
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
    {"string", "repeat", "repeat"},
    {"channel", "sendAsync", NULL, "__xl_channel_send_async"},
    {"channel", "sendAsyncCancel", NULL, "__xl_channel_send_async_cancel"},
    {"channel", "recvAsync", NULL, "__xl_channel_recv_async"},
    {"channel", "recvAsyncCancel", NULL, "__xl_channel_recv_async_cancel"},
    {"channel", "sendAsyncTimeout", NULL, "__xl_channel_send_async_timeout"},
    {"channel", "sendAsyncUntil", NULL, "__xl_channel_send_async_until"},
    {"channel", "sendAsyncTimeoutCancel", NULL, "__xl_channel_send_async_timeout_cancel"},
    {"channel", "sendAsyncUntilCancel", NULL, "__xl_channel_send_async_until_cancel"},
    {"channel", "recvAsyncTimeout", NULL, "__xl_channel_recv_async_timeout"},
    {"channel", "recvAsyncUntil", NULL, "__xl_channel_recv_async_until"},
    {"channel", "recvAsyncTimeoutCancel", NULL, "__xl_channel_recv_async_timeout_cancel"},
    {"channel", "recvAsyncUntilCancel", NULL, "__xl_channel_recv_async_until_cancel"}
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
    if(method->sourceName)return method->sourceName;
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
    /* Explicit generic construction (Type::<T>(...)) has a NAME inside a
     * GENERIC_INSTANT, not a TYPE or a direct CALL callee. It is nevertheless
     * a declaration dependency, including when the binding uses auto. */
    if ( node->kind == XL6_AST_GENERIC_INSTANT && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6StdlibTextEquals(node->children[0]->text, typeName) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6StdlibTextEquals(node->children[0]->text, typeName) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_MEMBER &&
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

static int xl6StdlibHasConcurrentImport(const XL6_AstNode* node)
{
    if (!node) return 0;
    if (node->kind == XL6_AST_IMPORT && node->text &&
        (!strcmp(node->text, "concurrent") || !strcmp(node->text, "\"concurrent\""))) return 1;
    for (size_t i = 0; i < node->childCount; ++i)
        if (xl6StdlibHasConcurrentImport(node->children[i])) return 1;
    return 0;
}

static int xl6StdlibNodeDefinesClass(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6StdlibNodeDefinesClass(node->children[i]) ) {
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

    if ( xl6StdlibFuncUsed(used, "hash", "u32Buffer") ||
         xl6StdlibFuncUsed(used, "hash", "u32BufferWithSeed") ||
         xl6StdlibFuncUsed(used, "hash", "u64Buffer") ||
         xl6StdlibFuncUsed(used, "hash", "u64BufferWithSeed") ) return 1;
    if ( xl6StdlibFuncUsed(used, "siphash", "hash64Buffer") ) return 1;
    if ( xl6StdlibFuncUsed(used, "random", "fillBytes") ||
         xl6StdlibFuncUsed(used, "random", "shuffleBuffer") ||
         xl6StdlibFuncUsed(used, "crypto", "fillRandomBytes") ) return 1;

    /* Buffer 返回值是标准库公开面的类型依赖，统一从注册表推导。 */
    if ( used != NULL ) {
        for ( index = 0; index < xl6StdlibFuncCount; ++index ) {
            if ( used[index] &&
                 xl6StdlibTextEquals(xl6StdlibFuncs[index].returnType, "Buffer") ) {
                return 1;
            }
        }

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
    static const char* names[] = {"make", "makeMany", "parse", "tryParse", "fromBytes"};
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

static int xl6StdlibNodeUsesMemberName(const XL6_AstNode* node, const char* memberName)
{
    size_t i;

    if ( node == NULL || memberName == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_MEMBER &&
         xl6StdlibTextEquals(node->text, memberName) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6StdlibNodeUsesMemberName(node->children[i], memberName) ) {
            return 1;
        }
    }
    return 0;
}

/* 网络类按公开能力分组，避免任一网络类型把完整 TCP、UDP 和 DNS API 注入模块。 */
static int xl6StdlibNeedsNetAddressClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetAddress") ||
           xl6StdlibNodeUsesTypeName(root, "NetAddressList") ||
           xl6StdlibNodeUsesTypeName(root, "NetInterface") ||
           xl6StdlibNodeUsesTypeName(root, "NetInterfaceList") ||
           xl6StdlibNodeUsesTypeName(root, "NetInterfaceAddress") ||
           xl6StdlibFuncUsed(used, "net", "interfaces") ||
           xl6StdlibFuncUsed(used, "net", "localAddress") ||
           xl6StdlibFuncUsed(used, "net", "resolve") ||
           xl6StdlibFuncUsed(used, "net", "resolveAll") ||
           xl6StdlibFuncUsed(used, "net", "resolveAsync");
}



/* 网络接口快照是独立基础能力，不应隐式引入 DNS、Engine 或 TCP。 */
static int xl6StdlibNeedsNetInterfaceClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetInterfaceList") ||
           xl6StdlibNodeUsesTypeName(root, "NetInterface") ||
           xl6StdlibNodeUsesTypeName(root, "NetInterfaceAddress") ||
           xl6StdlibFuncUsed(used, "net", "interfaces");
}



static int xl6StdlibNeedsNetEngineClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetEngine") ||
           xl6StdlibFuncUsed(used, "net", "stats") ||
           xl6StdlibFuncUsed(used, "net", "engine");
}



static int xl6StdlibNeedsNetEngineStatsClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetEngineStats") ||
           xl6StdlibFuncUsed(used, "net", "stats");
}



static int xl6StdlibNeedsNetResolverStatsClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetResolverStats") ||
           xl6StdlibFuncUsed(used, "net", "resolverStats");
}



static int xl6StdlibNeedsNetProxyClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "NetProxy") ||
           xl6StdlibFuncUsed(used, "net", "proxy");
}



static int xl6StdlibNeedsTcpStreamClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "TcpStream") ||
           xl6StdlibFuncUsed(used, "tcp", "connect") ||
           xl6StdlibFuncUsed(used, "tcp", "connectAsync");
}



/* 分隔帧与长度帧分别裁剪；共同结果类型不自行选择解析策略。 */
static int xl6StdlibNeedsLineFramerClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "LineFramer") ||
           xl6StdlibNodeUsesMemberName(root, "lineFramer");
}



static int xl6StdlibNeedsLengthFramerClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "LengthFramer") ||
           xl6StdlibNodeUsesMemberName(root, "lengthFramer");
}



static int xl6StdlibNeedsTcpServerClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "TcpServer") ||
           xl6StdlibFuncUsed(used, "tcp", "listen");
}



static int xl6StdlibNeedsTcpConnectConfigClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "TcpConnectConfig") ||
           xl6StdlibFuncUsed(used, "tcp", "connectConfig");
}



static int xl6StdlibNeedsTcpListenConfigClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "TcpListenConfig") ||
           xl6StdlibFuncUsed(used, "tcp", "listenConfig");
}



static int xl6StdlibNeedsUdpClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "UdpSocket") ||
           xl6StdlibNodeUsesTypeName(root, "UdpPacket") ||
           xl6StdlibNodeUsesTypeName(root, "UdpBatch") ||
           xl6StdlibNodeUsesTypeName(root, "UdpStats") ||
           xl6StdlibFuncUsed(used, "udp", "bind");
}



static int xl6StdlibNeedsUdpConfigClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root, "UdpConfig") ||
           xl6StdlibFuncUsed(used, "udp", "config");
}

static int xl6StdlibNeedsTlsSessionClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "TlsConfig") ||
           xl6StdlibNodeUsesTypeName(root, "TlsSession") ||
           xl6StdlibNodeUsesTypeName(root, "TlsResume") ||
           xl6StdlibNodeUsesTypeName(root, "TlsStream");
}

static int xl6StdlibNeedsTlsStreamClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TlsStream") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("tls", "connect", &index) != NULL && used[index] ) {
        return 1;
    }
    return xl6StdlibFindFunc("tls", "connectAsync", &index) != NULL && used[index];
}

static int xl6StdlibNeedsTlsServerClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TlsServer") ||
         xl6StdlibNodeUsesTypeName(root, "TlsServerStats") ) {
        return 1;
    }
    return used != NULL &&
           xl6StdlibFindFunc("tls", "listen", &index) != NULL &&
           used[index];
}



/* HTTP/1 核心类型独立于高级客户端、服务器和语义对象。 */
static int xl6StdlibNeedsHttp1Class(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "Http1") ||
           xl6StdlibNodeUsesTypeName(root, "Http1Head") ||
           xl6StdlibNodeUsesTypeName(root, "Http1Body");
}



/* Content-Encoding 解码器按需带入 gzip，而不污染纯报头路径。 */
static int xl6StdlibNeedsHttpDecoderClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "HttpDecoder");
}



/* WebSocket 帧和关闭载荷不隐式带入握手与网络传输。 */
static int xl6StdlibNeedsWsFrameClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "WsFrame") ||
           xl6StdlibNodeUsesTypeName(root, "WsClose");
}



/* 握手纯函数可以独立用于代理、测试和协议工具。 */
static int xl6StdlibNeedsWsHandshakeClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "WsHandshake");
}



/* Upgrade 只依赖严格 HTTP/1 报头，不依赖高级 HTTP 对象层。 */
static int xl6StdlibNeedsWsUpgradeClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "WsUpgrade");
}



/* WebSocket 流对象依赖 Upgrade，但配置对象可以独立构造和裁剪。 */
static int xl6StdlibNeedsWebSocketClass(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesTypeName(root, "WebSocket");
}



/* TLS 接管仅在源码显式选择 attachTls 时注入。 */
static int xl6StdlibNeedsWebSocketTls(const XL6_AstNode* root)
{
    return xl6StdlibNodeUsesMemberName(root, "attachTls") ||
           xl6StdlibNodeUsesMemberName(root, "attachTlsWithConfig");
}

static int xl6StdlibNeedsFileClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "File") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "open", &index) != NULL && used[index];
}

static int xl6StdlibNeedsProcessClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if (xl6StdlibNodeUsesTypeName(root,"ProcessCommand") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessRunOptions") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessResult") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessExitStatus") ||
        xl6StdlibNodeUsesTypeName(root,"Process") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessPipeline") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessStageResult") ||
        xl6StdlibNodeUsesTypeName(root,"PipelineResult")) return 1;
    if (xl6StdlibFuncUsed(used,"process","exec") ||
        xl6StdlibFuncUsed(used,"process","success") ||
        xl6StdlibFuncUsed(used,"process","exitCode")) return 1;
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
         xl6StdlibNodeUsesTypeName(root, "LogRecord") ||
         xl6StdlibNodeUsesTypeName(root, "LogFileMode") ||
         xl6StdlibNodeUsesTypeName(root, "LogFileSync") ||
         xl6StdlibNodeUsesTypeName(root, "LogQueueFull") ||
         xl6StdlibNodeUsesTypeName(root, "LogShutdown") ||
         xl6StdlibNodeUsesTypeName(root, "LogFileOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogFileStats") ||
         xl6StdlibNodeUsesTypeName(root, "LogAsyncOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogRingOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogQueueStats") ||
         xl6StdlibNodeUsesTypeName(root, "LogJsonOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogJsonFieldStyle") ||
         xl6StdlibNodeUsesTypeName(root, "LogNonFinite") ||
         xl6StdlibNodeUsesTypeName(root, "LogSink") ||
         xl6StdlibNodeUsesTypeName(root, "LogStats") ||
         xl6StdlibNodeUsesTypeName(root, "LogLevel") ||
         xl6StdlibNodeUsesTypeName(root, "LogResult") ||
         xl6StdlibNodeUsesTypeName(root, "LogConsoleOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogTextOptions") ||
         xl6StdlibNodeUsesTypeName(root, "LogTextStyle") ||
         xl6StdlibNodeUsesTypeName(root, "LogConsoleTarget") ||
         xl6StdlibNodeUsesTypeName(root, "LogColor") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    for (index=0;index<XL6_STDLIB_FUNC_COUNT;++index)
        if (used[index] && strcmp(xl6StdlibFuncs[index].ns,"log") == 0) return 1;
    return 0;
}

static int xl6StdlibNeedsSignalHandleClass(const XL6_AstNode* root, const int* used)
{
    if ( xl6StdlibNodeUsesTypeName(root, "SignalHandle") ) {
        return 1;
    }
    return xl6StdlibFuncUsed(used, "signal", "on") ||
           xl6StdlibFuncUsed(used, "signal", "once") ||
           xl6StdlibFuncUsed(used, "signal", "sendTo") ||
           xl6StdlibFuncUsed(used, "signal", "cancelOn");
}

static int xl6StdlibNeedsSignalClass(const XL6_AstNode* root, const int* used)
{
    return xl6StdlibNodeUsesTypeName(root,"SignalSubscription") ||
        xl6StdlibNodeUsesTypeName(root,"SignalEvent") || xl6StdlibNeedsSignalHandleClass(root,used);
}

static int xl6StdlibNeedsTemplateClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Template") ||
         xl6StdlibNodeUsesTypeName(root, "TemplateEngine") || xl6StdlibNodeUsesTypeName(root,"TemplateCall") ||
         xl6StdlibNodeUsesTypeName(root,"TemplateNode") || xl6StdlibNodeUsesTypeName(root,"TemplateArgument") ||
         xl6StdlibNodeUsesTypeName(root,"TemplateCompileOptions") || xl6StdlibNodeUsesTypeName(root,"TemplateRenderOptions") ||
         xl6StdlibNodeUsesTypeName(root,"TemplateLocation") || xl6StdlibFuncUsed(used,"template","errorLocation") ||
         xl6StdlibFuncUsed(used,"template","compileFileIn") || xl6StdlibFuncUsed(used,"template","renderFileIn") ) {
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
    if ( xl6StdlibFindFunc("template", "compileFile", &index) != NULL && used[index] ) {
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

    if ( xl6StdlibNodeUsesTypeName(root, "AesGcm") ||
         xl6StdlibNodeUsesTypeName(root, "ChaCha20Poly1305") ||
         xl6StdlibNodeUsesTypeName(root, "LegacyMd5") ||
         xl6StdlibNodeUsesTypeName(root, "LegacySha1") ||
         xl6StdlibNodeUsesTypeName(root, "Sha224") ||
         xl6StdlibNodeUsesTypeName(root, "Sha256") ||
         xl6StdlibNodeUsesTypeName(root, "Sha384") ||
         xl6StdlibNodeUsesTypeName(root, "Sha512") ||
         xl6StdlibNodeUsesTypeName(root, "Sha512_256") ||
         xl6StdlibNodeUsesTypeName(root, "HmacSha256") ||
         xl6StdlibNodeUsesTypeName(root, "HmacSha384") ||
         xl6StdlibNodeUsesTypeName(root, "HmacSha512") ||
         (xl6StdlibFindFunc("cryptoLegacy", "md5Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("cryptoLegacy", "sha1Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "sha224Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "sha256Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "sha384Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "sha512Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "sha512_256Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "hmacSha256Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "hmacSha384Stream", &index) != NULL && used[index]) ||
         (xl6StdlibFindFunc("crypto", "hmacSha512Stream", &index) != NULL && used[index]) ) return 1;
    return 0;
}

static int xl6StdlibNeedsTaskGroupClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TaskGroup") ||
         xl6StdlibNodeUsesTypeName(root, "TaskGroupSnapshot") ) {
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
         xl6StdlibNodeUsesTypeName(root, "XlonWriter") ||
         xl6StdlibNodeUsesTypeName(root, "XlonOptions") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFuncUsed(used, "xlon", "parse") ||
         xl6StdlibFuncUsed(used, "xlon", "valid") ||
         xl6StdlibFuncUsed(used, "xlon", "read") ||
         xl6StdlibFuncUsed(used, "xlon", "stringify") ||
         xl6StdlibFuncUsed(used, "xlon", "write") ||
         xl6StdlibFuncUsed(used, "xlon", "writer") ||
         xl6StdlibFuncUsed(used, "xlon", "visit") ) {
        /* All overloads are declared together, including XlonOptions forms.
         * Their type dependencies do not depend on the selected overload. */
        return 1;
    }
    if ( xl6StdlibFindFunc("json", "writer", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("xlon", "options", &index) != NULL && used[index] ) {
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
    return (xl6StdlibFindFunc("file", "openAsync", &index) != NULL && used[index]) ||
        (xl6StdlibFindFunc("file", "takeAsync", &index) != NULL && used[index]);
}

static unsigned int xl6StdlibForeignEffects(const XL6_StdlibFunc* func)
{
    size_t i;

    if ( func == NULL || func->internalName == NULL || func->cSymbol == NULL ) {
        return XL6_STDLIB_FOREIGN_EFFECT_NONE;
    }
    for ( i = 0; i < xl6StdlibForeignContractCount; ++i ) {
        const XL6_StdlibForeignContract* contract = &xl6StdlibForeignContracts[i];
        if ( strcmp(contract->internalName, func->internalName) == 0 &&
             strcmp(contract->cSymbol, func->cSymbol) == 0 ) {
            return contract->effects;
        }
    }
    return XL6_STDLIB_FOREIGN_EFFECT_NONE;
}

static void xl6StdlibAppendFuncDecl(XL6_StdlibTextBuilder* builder, const XL6_StdlibFunc* func)
{
    size_t i;
    unsigned int effects;

    if ( builder == NULL || func == NULL ) {
        return;
    }

    if ( func->sourceBody != NULL ) {
        xl6StdlibBuilderAppend(builder, func->sourceBody);
        return;
    }

    xl6StdlibBuilderAppend(builder, "extern ");
    xl6StdlibBuilderAppendQuoted(builder, func->cSymbol);
    effects = xl6StdlibForeignEffects(func);
    if ( (effects & XL6_STDLIB_FOREIGN_EFFECT_PURE) != 0 ) {
        xl6StdlibBuilderAppend(builder, " @pure");
    }
    if ( (effects & XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE) != 0 ) {
        xl6StdlibBuilderAppend(builder, " @no_raise");
    }
    if ( (effects & XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING) != 0 ) {
        xl6StdlibBuilderAppend(builder, " @nonblocking");
    }
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

/* These private CRT functions have a scalar C output pointer.  Keep their
 * physical ABI in the standard-library catalog; source-level parameter
 * modifiers are not part of xlang's public grammar. */
typedef struct XL6_StdlibCOutputPointer {
    const char* nativeName;
    const char* wrapperName;
    size_t parameterIndex;
} XL6_StdlibCOutputPointer;

static const XL6_StdlibCOutputPointer xl6StdlibCOutputPointers[] = {
    {"__xl_std_math_frexp_native", "__xl_std_math_frexp", 1},
    {"__xl_std_math_modf_native", "__xl_std_math_modf", 1},
    {"__xl_std_math_rem_quo_native", "__xl_std_math_rem_quo", 2}
};

static size_t xl6StdlibMarkCOutputCalls(XL6_AstNode* node,
                                         const XL6_StdlibCOutputPointer* contract)
{
    size_t i;
    size_t marked = 0;
    if ( node == NULL ) return 0;
    if ( node->kind == XL6_AST_CALL && node->childCount >= 2 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6StdlibTextEquals(node->children[0]->text, contract->nativeName) &&
         node->children[1] != NULL &&
         node->children[1]->childCount > contract->parameterIndex &&
         node->children[1]->children[contract->parameterIndex] != NULL &&
         node->children[1]->children[contract->parameterIndex]->kind == XL6_AST_ARG ) {
        node->children[1]->children[contract->parameterIndex]->flags |= XL6_AST_FLAG_OUT;
        ++marked;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        marked += xl6StdlibMarkCOutputCalls(node->children[i], contract);
    }
    return marked;
}

static int xl6StdlibApplyCOutputPointers(XL6_AstNode* generated,
                                          XL6_DiagList* diags)
{
    size_t c;
    for ( c = 0; c < sizeof(xl6StdlibCOutputPointers) /
                     sizeof(xl6StdlibCOutputPointers[0]); ++c ) {
        const XL6_StdlibCOutputPointer* contract = &xl6StdlibCOutputPointers[c];
        XL6_AstNode* native = NULL;
        XL6_AstNode* wrapper = NULL;
        XL6_AstNode* params;
        XL6_AstNode* param;
        size_t i;
        for ( i = 0; i < generated->childCount; ++i ) {
            XL6_AstNode* item = generated->children[i];
            if ( item == NULL || item->childCount == 0 || item->children[0] == NULL )
                continue;
            if ( item->kind == XL6_AST_EXTERN_FUNC &&
                 xl6StdlibTextEquals(item->children[0]->text, contract->nativeName) )
                native = item;
            if ( item->kind == XL6_AST_FUNC &&
                 xl6StdlibTextEquals(item->children[0]->text, contract->wrapperName) )
                wrapper = item;
        }
        if ( native == NULL && wrapper == NULL ) continue;
        params = native != NULL && native->childCount > 1 ? native->children[1] : NULL;
        param = params != NULL && params->childCount > contract->parameterIndex
            ? params->children[contract->parameterIndex] : NULL;
        if ( wrapper == NULL || param == NULL || param->kind != XL6_AST_PARAM ||
             param->childCount == 0 || param->children[0] == NULL ||
             xl6StdlibMarkCOutputCalls(wrapper, contract) != 1 ) {
            if ( diags != NULL )
                xl6DiagAdd(diags, 1, 1, "invalid private CRT output-pointer contract");
            return 0;
        }
        param->flags |= XL6_AST_FLAG_OUT;
        param->children[0]->flags |= XL6_AST_FLAG_OUT;
    }
    return 1;
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
    if ( result.root != NULL && !xl6StdlibApplyCOutputPointers(result.root, diags) ) {
        xl6ParseResultFree(&result);
        return 0;
    }
    if ( result.root != NULL ) {
        size_t originalCount=root->childCount, preludeCount=0;
        xl6AstMoveChildren(root, result.root);
        /* Library-owned state is a normal module global, initialized before
         * user statements and destroyed after them. Keep function/type
         * declarations in their established order; no backend special case. */
        for (size_t i=originalCount;i<root->childCount;++i) {
            if (root->children[i]->kind==XL6_AST_VAR_DECL) {
                XL6_AstNode* state=root->children[i];
                memmove(root->children+preludeCount+1,root->children+preludeCount,
                    (i-preludeCount)*sizeof(*root->children));
                root->children[preludeCount++]=state;
            }
        }
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
    int needsRandomBufferMethods;
    int needsSipHasher64Class;
    int needsSipHasher64BufferMethod;
    int needsXidClass;
    int needsBufferClass;
    int needsNetAddressClass;
    int needsNetAddressListClass;
    int needsNetResolverClass;
    int needsNetResolverOptionsClass;
    int needsNetDefaultResolver;
    int needsNetSocketClass;
    int needsNetInterfaceClass;
    int needsNetBytesClass;
    int needsNetEngineStatsClass;
    int needsNetResolverStatsClass;
    int needsNetProxyClass;
    int needsNetEngineClass;
    int needsTcpStreamStatsClass;
    int needsTcpServerStatsClass;
    int needsTcpStreamClass;
    int needsTcpStreamFile;
    int needsTcpServerClass;
    int needsTcpConnectConfigClass;
    int needsTcpListenConfigClass;
    int needsUdpStatsClass;
    int needsUdpClass;
    int needsUdpConfigClass;
    int needsNetRuntime;
    int needsTlsSessionClass;
    int needsTlsStreamClass;
    int needsTlsServerClass;
    int needsHttp1Class;
    int needsHttpDecoderClass;
    int needsWsFrameClass;
    int needsWsHandshakeClass;
    int needsWsUpgradeClass;
    int needsWebSocketConfigClass;
    int needsWebSocketClass;
    int needsWebSocketTls;
    int needsFileClass;
    int needsAsyncFileClass;
    int needsJsonClass;
    int needsProcessClass;
    int needsPipelineClass;
    int needsLiveProcessClass;
    int needsProcessStreams;
    int needsTaskGroupClass;
    int needsLoggerClass;
    int needsSignalClass;
    int needsSignalHandleClass;
    int needsTemplateClass;
    int needsTemplateEngine, needsTemplateFile, needsTemplateVfs, needsTemplateBuffer, needsTemplateWriter;
    int needsCompress, needsDeflater, needsInflater;
    int needsCryptoClass;
    int needsTccClass;
    int needsXlangClass;
    int needsCoreClass;
    int needsIoReader, needsIoWriter, needsIoLines, needsIoProtocols, needsIoBuffer;
    int needsStringIterator, needsUtf8Validator, needsRegex, needsRegexSet, needsRegexBuffer;
    int needsPattern;
    int needsDateTime;
    int needsVfs;
    int needsConsoleSession, needsConsoleOptions;
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
    needsCoreClass = xl6StdlibNodeDefinesClass(root) ||
        xl6StdlibNodeUsesTypeName(root, "object") ||
        xl6StdlibNodeUsesTypeName(root, "DynamicFields");
    needsIoLines = xl6StdlibFuncUsed(used, "io", "lines") || xl6StdlibNodeUsesTypeName(root, "IoLineReader");
    needsStringIterator = xl6StdlibFuncUsed(used, "string", "splitIter") ||
        xl6StdlibFuncUsed(used, "string", "linesIter") || xl6StdlibFuncUsed(used, "string", "fieldsIter") ||
        xl6StdlibNodeUsesTypeName(root, "StringIterator");
    needsUtf8Validator = xl6StdlibFuncUsed(used, "unicode", "validator") ||
        xl6StdlibNodeUsesTypeName(root, "Utf8Validator");
    needsRegex = xl6StdlibFuncUsed(used, "regex", "compile") ||
        xl6StdlibNodeUsesTypeName(root, "Regex") || xl6StdlibNodeUsesTypeName(root, "RegexMatch") ||
        xl6StdlibNodeUsesTypeName(root, "RegexIterator") || xl6StdlibNodeUsesTypeName(root, "RegexSplitIterator") ||
        xl6StdlibNodeUsesTypeName(root, "RegexSplitPart");
    needsRegexSet = xl6StdlibFuncUsed(used, "regex", "set") || xl6StdlibNodeUsesTypeName(root, "RegexSet");
    needsPattern = xl6StdlibFuncUsed(used, "pattern", "compile") ||
        xl6StdlibNodeUsesTypeName(root, "Pattern") || xl6StdlibNodeUsesTypeName(root, "PatternOptions") ||
        xl6StdlibNodeUsesTypeName(root, "PatternMatch") || xl6StdlibNodeUsesTypeName(root, "PatternBuilder") ||
        xl6StdlibNodeUsesTypeName(root, "PatternKey") || xl6StdlibNodeUsesTypeName(root, "PatternSpec");
    needsDateTime = xl6StdlibNodeUsesTypeName(root, "DateTime") ||
        xl6StdlibFuncUsed(used, "time", "split") || xl6StdlibFuncUsed(used, "time", "local") ||
        xl6StdlibFuncUsed(used, "time", "fromLocal") || xl6StdlibFuncUsed(used, "time", "utcToLocal") ||
        xl6StdlibFuncUsed(used, "time", "localToUTC") || xl6StdlibFuncUsed(used, "time", "parseFields");
    needsRegexBuffer = needsRegex && (xl6StdlibNodeUsesMemberName(root, "replaceTo") ||
        xl6StdlibNodeUsesMemberName(root, "replaceWithTo"));
    needsConsoleSession = xl6StdlibFuncUsed(used, "console", "openSession") ||
        xl6StdlibNodeUsesTypeName(root, "ConsoleSession") || xl6StdlibNodeUsesTypeName(root, "ConsoleEvent");
    needsConsoleOptions = needsConsoleSession || xl6StdlibNodeUsesTypeName(root, "ConsoleOptions") ||
        xl6StdlibNodeUsesTypeName(root, "ConsoleStyle") || xl6StdlibFuncUsed(used, "console", "writeStyled") ||
        xl6StdlibFuncUsed(used, "console", "errorStyled");
    needsIoProtocols = xl6StdlibFuncUsed(used, "io", "copy") || xl6StdlibFuncUsed(used, "io", "copyExact") ||
        xl6StdlibFuncUsed(used, "io", "copyUpTo") || xl6StdlibNodeUsesTypeName(root, "IoReadable") ||
        xl6StdlibNodeUsesTypeName(root, "IoWritable");
    needsIoReader = needsIoLines || needsIoProtocols || xl6StdlibNodeUsesTypeName(root, "IoReader") ||
        xl6StdlibFuncUsed(used, "io", "openReader") || xl6StdlibFuncUsed(used, "io", "fromBytes") ||
        xl6StdlibFuncUsed(used, "io", "fromBuffer") || xl6StdlibFuncUsed(used, "io", "stdin");
    needsIoWriter = needsIoProtocols || xl6StdlibNodeUsesTypeName(root, "IoWriter") ||
        xl6StdlibFuncUsed(used, "io", "openWriter") || xl6StdlibFuncUsed(used, "io", "bufferWriter") ||
        xl6StdlibFuncUsed(used, "io", "discardWriter") || xl6StdlibFuncUsed(used, "io", "stdout") ||
        xl6StdlibFuncUsed(used, "io", "stderr");
    needsIoBuffer = xl6StdlibNodeUsesTypeName(root, "Buffer") || xl6StdlibFuncUsed(used, "io", "fromBuffer") ||
        xl6StdlibNodeUsesMemberName(root, "readInto");
    needsRandomClass = xl6StdlibNeedsRandomClass(root, used);
    needsVfs = xl6StdlibFuncUsed(used, "vfs", "create") || xl6StdlibFuncUsed(used, "vfs", "memory") ||
        xl6StdlibFuncUsed(used, "vfs", "disk") || xl6StdlibFuncUsed(used, "vfs", "pack") ||
        xl6StdlibFuncUsed(used, "vfs", "packOptions") || xl6StdlibFuncUsed(used, "vfs", "takePack") ||
        xl6StdlibNodeUsesTypeName(root, "PackOptions") || xl6StdlibNodeUsesTypeName(root, "PackStats") ||
        xl6StdlibNodeUsesTypeName(root, "Vfs") || xl6StdlibNodeUsesTypeName(root, "MemoryFS") ||
        xl6StdlibNodeUsesTypeName(root, "DiskFS") || xl6StdlibNodeUsesTypeName(root, "PackFS") ||
        xl6StdlibNodeUsesTypeName(root, "VfsMount");
    needsRandomBufferMethods = needsRandomClass &&
        (xl6StdlibNodeUsesMemberName(root, "fillBytes") ||
         xl6StdlibNodeUsesMemberName(root, "shuffleBuffer"));
    needsSipHasher64Class =
        xl6StdlibFuncUsed(used, "siphash", "hasher") ||
        xl6StdlibNodeUsesTypeName(root, "SipHasher64");
    needsSipHasher64BufferMethod = needsSipHasher64Class &&
        xl6StdlibNodeUsesMemberName(root, "updateBuffer");
    needsXidClass = xl6StdlibNeedsXidClass(root, used);
    needsTlsStreamClass = xl6StdlibNeedsTlsStreamClass(root, used);
    needsTlsServerClass = xl6StdlibNeedsTlsServerClass(root, used);
    needsHttp1Class = xl6StdlibNeedsHttp1Class(root);
    needsHttpDecoderClass = xl6StdlibNeedsHttpDecoderClass(root);
    needsWsFrameClass = xl6StdlibNeedsWsFrameClass(root);
    needsWsHandshakeClass = xl6StdlibNeedsWsHandshakeClass(root);
    needsWsUpgradeClass = xl6StdlibNeedsWsUpgradeClass(root);
    needsWebSocketConfigClass = xl6StdlibNodeUsesTypeName(root, "WebSocketConfig");
    needsWebSocketClass = xl6StdlibNeedsWebSocketClass(root);
    needsWebSocketTls = needsWebSocketClass && xl6StdlibNeedsWebSocketTls(root);
    if ( needsWebSocketClass ) {
        needsWebSocketConfigClass = 1;
        needsWsUpgradeClass = 1;
    }
    if ( needsWsUpgradeClass ) {
        needsHttp1Class = 1;
    }
    if ( needsWebSocketTls ) {
        needsTlsStreamClass = 1;
    }
    if ( needsTlsServerClass ) {
        needsTlsStreamClass = 1;
    }
	needsTlsSessionClass = xl6StdlibNeedsTlsSessionClass(root) ||
		needsTlsStreamClass || needsTlsServerClass;
    needsNetAddressClass = xl6StdlibNeedsNetAddressClass(root, used);
    needsNetDefaultResolver=xl6StdlibFuncUsed(used,"net","resolveAsync")||xl6StdlibFuncUsed(used,"net","resolverStats");
    needsNetResolverClass=needsNetDefaultResolver||xl6StdlibNodeUsesTypeName(root,"NetResolver")||
        xl6StdlibNodeUsesTypeName(root,"NetResolveOperation");
    needsNetResolverOptionsClass=needsNetResolverClass||xl6StdlibNodeUsesTypeName(root,"NetResolverOptions");
    needsNetAddressListClass=needsNetResolverClass||xl6StdlibNodeUsesTypeName(root,"NetAddressList")||
        xl6StdlibFuncUsed(used,"net","resolveAll");
    if(needsNetAddressListClass)needsNetAddressClass=1;
    needsNetSocketClass=xl6StdlibNodeUsesTypeName(root,"NetSocket")||xl6StdlibNodeUsesTypeName(root,"NetStatus")||
        xl6StdlibNodeUsesTypeName(root,"NetSocketKind")||xl6StdlibNodeUsesTypeName(root,"NetSocketOption")||
        xl6StdlibNodeUsesTypeName(root,"NetShutdown")||xl6StdlibNodeUsesTypeName(root,"NetDatagramInfo")||
        xl6StdlibNodeUsesTypeName(root,"NetDatagramControl")||xl6StdlibNodeUsesTypeName(root,"NetDatagramMetadata")||
        xl6StdlibNodeUsesTypeName(root,"NetDatagramControlFlags")||xl6StdlibNodeUsesTypeName(root,"NetDatagramCapabilities")||
        xl6StdlibNodeUsesTypeName(root,"NetDatagramErrorInfo")||xl6StdlibNodeUsesTypeName(root,"NetDatagramErrorFlags")||
        xl6StdlibNodeUsesTypeName(root,"NetDatagramErrorOrigin")||xl6StdlibNodeUsesTypeName(root,"NetPathMtuMode")||
        xl6StdlibNodeUsesTypeName(root,"NetDatagramPacket")||xl6StdlibNodeUsesTypeName(root,"NetBatchProgress");
    if(needsNetSocketClass)needsNetAddressClass=1;
    needsNetInterfaceClass = xl6StdlibNeedsNetInterfaceClass(root, used);
    needsNetBytesClass = xl6StdlibNodeUsesTypeName(root, "NetBytes");
    needsNetEngineStatsClass = xl6StdlibNeedsNetEngineStatsClass(root, used);
    needsNetResolverStatsClass = xl6StdlibNeedsNetResolverStatsClass(root, used);
    needsNetResolverStatsClass=needsNetResolverStatsClass||needsNetResolverClass;
    needsNetProxyClass = xl6StdlibNeedsNetProxyClass(root, used);
    needsNetEngineClass = xl6StdlibNeedsNetEngineClass(root, used);
    needsTcpStreamStatsClass = xl6StdlibNodeUsesTypeName(root, "TcpStreamStats");
    needsTcpServerStatsClass = xl6StdlibNodeUsesTypeName(root, "TcpServerStats");
    needsTcpStreamClass = xl6StdlibNeedsTcpStreamClass(root, used);
    needsTcpStreamClass = needsTcpStreamClass || needsWebSocketClass;
    needsTcpServerClass = xl6StdlibNeedsTcpServerClass(root, used);
    needsTcpConnectConfigClass = xl6StdlibNeedsTcpConnectConfigClass(root, used);
    needsTcpListenConfigClass = xl6StdlibNeedsTcpListenConfigClass(root, used);
    needsUdpStatsClass = xl6StdlibNodeUsesTypeName(root, "UdpStats");
    needsUdpClass = xl6StdlibNeedsUdpClass(root, used);
    needsUdpConfigClass = xl6StdlibNeedsUdpConfigClass(root, used);

    /* 依赖由高层类型向基础类型单向闭包。 */
    needsNetProxyClass = needsNetProxyClass || needsTcpConnectConfigClass;
    needsTcpServerClass = needsTcpServerClass || needsTcpListenConfigClass;
    needsTcpStreamClass = needsTcpStreamClass || needsTcpServerClass ||
        needsTcpConnectConfigClass;
    needsTcpStreamFile = needsTcpStreamClass &&
        (xl6StdlibNodeUsesMemberName(root, "sendFile") ||
         xl6StdlibNodeUsesMemberName(root, "sendFileResult") ||
         xl6StdlibNodeUsesMemberName(root, "sendFileHandle") ||
         xl6StdlibNodeUsesMemberName(root, "sendFileHandleResult"));
    needsNetBytesClass = needsNetBytesClass || needsTcpStreamClass ||
        needsTlsStreamClass;
    needsNetEngineStatsClass = needsNetEngineStatsClass || needsNetEngineClass;
    needsTcpStreamStatsClass = needsTcpStreamStatsClass || needsTcpStreamClass;
    needsTcpServerStatsClass = needsTcpServerStatsClass || needsTcpServerClass;
    needsUdpClass = needsUdpClass || needsUdpConfigClass;
    needsUdpStatsClass = needsUdpStatsClass || needsUdpClass;
    needsNetRuntime = needsNetAddressClass || needsNetInterfaceClass ||
        needsNetBytesClass ||
        needsNetEngineStatsClass || needsNetResolverStatsClass ||
        needsNetProxyClass || needsNetEngineClass ||
        needsTcpStreamStatsClass || needsTcpServerStatsClass ||
        needsTcpStreamClass || needsTcpServerClass ||
        needsTcpConnectConfigClass || needsTcpListenConfigClass ||
        needsUdpStatsClass || needsUdpClass || needsUdpConfigClass;
    needsFileClass = xl6StdlibNeedsFileClass(root, used);
    needsFileClass = needsFileClass || xl6StdlibNeedsAsyncFileClass(root,used) || needsVfs || xl6StdlibFuncUsed(used, "file", "info") || xl6StdlibFuncUsed(used, "file", "temp") || xl6StdlibFuncUsed(used, "file", "stat") ||
        xl6StdlibFuncUsed(used, "file", "root") || xl6StdlibNodeUsesTypeName(root, "FileRoot") ||
        xl6StdlibNodeUsesTypeName(root, "MappedFile") || xl6StdlibNodeUsesTypeName(root, "FileLock") ||
        xl6StdlibFuncUsed(used, "dir", "open") || xl6StdlibNodeUsesTypeName(root, "FileStat") ||
        xl6StdlibNodeUsesTypeName(root, "DirectoryIterator");
    needsIoReader = needsIoReader || needsFileClass;
    needsIoWriter = needsIoWriter || needsFileClass;
    needsAsyncFileClass = xl6StdlibNeedsAsyncFileClass(root, used);
    needsJsonClass = xl6StdlibNeedsJsonClass(root, used);
    needsProcessClass = xl6StdlibNeedsProcessClass(root, used);
    needsPipelineClass = xl6StdlibFuncUsed(used,"process","pipeline") ||
        xl6StdlibNodeUsesTypeName(root,"PipelineResult") ||
        xl6StdlibNodeUsesTypeName(root,"ProcessPipeline") || xl6StdlibNodeUsesTypeName(root,"ProcessStageResult");
    needsLiveProcessClass = xl6StdlibFuncUsed(used,"process","start") ||
        xl6StdlibFuncUsed(used,"process","startFile") || xl6StdlibNodeUsesTypeName(root,"Process") ||
        (needsProcessClass && xl6StdlibNodeUsesMemberName(root,"start"));
    needsProcessClass = needsProcessClass || needsLiveProcessClass || needsPipelineClass;
    needsProcessStreams = needsLiveProcessClass &&
        (xl6StdlibNodeUsesMemberName(root,"stdinWriter") || xl6StdlibNodeUsesMemberName(root,"stdoutReader") ||
         xl6StdlibNodeUsesMemberName(root,"stderrReader") || xl6StdlibNodeUsesTypeName(root,"IoReader") ||
         xl6StdlibNodeUsesTypeName(root,"IoWriter"));
    needsIoReader = needsIoReader || needsProcessStreams;
    needsIoWriter = needsIoWriter || needsProcessStreams;
    needsTaskGroupClass = xl6StdlibNeedsTaskGroupClass(root, used) || xl6StdlibFuncUsed(used, "signal", "cancelOn");
    needsLoggerClass = xl6StdlibNeedsLoggerClass(root, used);
    needsIoWriter = needsIoWriter || needsLoggerClass;
    if (needsLoggerClass) xl6StdlibMarkFuncUsed(used,"__xl_runtime","logStats");
    needsSignalClass = xl6StdlibNeedsSignalClass(root, used);
    needsSignalHandleClass = xl6StdlibNeedsSignalHandleClass(root, used);
    needsTemplateClass = xl6StdlibNeedsTemplateClass(root, used);
    needsTemplateEngine = xl6StdlibFuncUsed(used,"template","engine") || xl6StdlibNodeUsesTypeName(root,"TemplateEngine");
    needsTemplateFile = xl6StdlibFuncUsed(used,"template","compileFile") || xl6StdlibFuncUsed(used,"template","renderFile") ||
        (needsTemplateEngine && xl6StdlibNodeUsesMemberName(root,"compileFile"));
    needsTemplateVfs = xl6StdlibFuncUsed(used,"template","compileFileIn") || xl6StdlibFuncUsed(used,"template","renderFileIn") ||
        (needsTemplateEngine && xl6StdlibNodeUsesMemberName(root,"compileFileIn"));
    needsVfs = needsVfs || needsTemplateVfs;
    needsTemplateBuffer = needsTemplateClass && xl6StdlibNodeUsesMemberName(root,"appendTo");
    needsTemplateWriter = needsTemplateClass && xl6StdlibNodeUsesMemberName(root,"writeTo");
    needsIoWriter = needsIoWriter || needsTemplateWriter;
    needsDeflater = xl6StdlibFuncUsed(used, "compress", "deflater") || xl6StdlibNodeUsesTypeName(root, "Deflater");
    needsInflater = xl6StdlibFuncUsed(used, "compress", "inflater") || xl6StdlibNodeUsesTypeName(root, "Inflater");
    needsCompress = needsDeflater || needsInflater || xl6StdlibNodeUsesTypeName(root, "DeflateOptions") ||
        xl6StdlibNodeUsesTypeName(root, "InflateOptions") || xl6StdlibFuncUsed(used, "compress", "encode") ||
        xl6StdlibFuncUsed(used, "compress", "encodeBuffer") || xl6StdlibFuncUsed(used, "compress", "decode") ||
        xl6StdlibFuncUsed(used, "compress", "decodeBuffer");
    needsCryptoClass = xl6StdlibNeedsCryptoClass(root, used);
    /*
        只有公开面真正交换字节容器时才注入 Buffer。同步地址解析等纯网络
        能力不依赖 Buffer，不能因为同属网络体系就拉入文件和二进制运行时。
    */
    needsBufferClass = needsNetSocketClass || needsTemplateBuffer || needsDeflater || needsInflater || xl6StdlibFuncUsed(used, "compress", "encodeBuffer") ||
        xl6StdlibFuncUsed(used, "compress", "decodeBuffer") || needsRegexBuffer || xl6StdlibFuncUsed(used, "io", "writeFile") || (needsIoBuffer && (needsIoReader || needsIoWriter)) || needsRandomBufferMethods || xl6StdlibNeedsBufferClass(root, used) ||
        needsNetBytesClass ||
        needsTcpStreamClass || needsUdpClass ||
        needsTlsSessionClass || needsHttp1Class || needsHttpDecoderClass ||
        needsWsFrameClass || needsWebSocketClass || needsFileClass ||
        needsAsyncFileClass || needsCryptoClass || needsLoggerClass;
    needsTccClass = xl6StdlibNeedsTccClass(root, used);
    needsXlangClass = xl6StdlibNeedsXlangClass(root, used);
    if ( needsCryptoClass || needsFileClass ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "byteLen");
    }
    if ( xl6StdlibFuncUsed(used, "signal", "cancelOn") ) {
        needsTaskGroupClass = 1;
    }
    needsCoreClass = needsCoreClass || needsNetResolverOptionsClass || needsCompress || needsDateTime || needsConsoleOptions || needsIoReader || needsIoWriter || needsIoLines || needsStringIterator || needsUtf8Validator || needsRegex || needsRandomClass || needsSipHasher64Class || needsXidClass ||
        needsBufferClass || needsNetAddressClass || needsNetInterfaceClass ||
        needsNetBytesClass ||
        needsNetEngineStatsClass || needsNetResolverStatsClass ||
        needsNetProxyClass || needsNetEngineClass || needsTcpStreamStatsClass ||
        needsTcpServerStatsClass || needsTcpStreamClass || needsTcpServerClass ||
        needsTcpConnectConfigClass || needsTcpListenConfigClass ||
        needsUdpStatsClass || needsUdpClass || needsUdpConfigClass ||
        needsTlsSessionClass || needsTlsStreamClass || needsTlsServerClass ||
        needsHttp1Class || needsHttpDecoderClass || needsWsFrameClass ||
        needsWsHandshakeClass || needsWsUpgradeClass ||
        needsWebSocketConfigClass || needsWebSocketClass ||
        needsFileClass || needsAsyncFileClass ||
        needsJsonClass || needsTaskGroupClass ||
        needsLoggerClass || needsTemplateClass ||
        needsCryptoClass || needsTccClass || needsXlangClass;
    if ( needsRandomClass || needsXidClass || needsBufferClass || needsNetRuntime ||
         needsTlsSessionClass || needsHttp1Class || needsWsFrameClass ||
         needsWsHandshakeClass || needsWsUpgradeClass || needsWebSocketClass ||
         needsWebSocketConfigClass || needsFileClass ||
         needsJsonClass ||
         needsCryptoClass || needsXlangClass ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
    }
    if ( needsRandomClass || needsSipHasher64Class )
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "byteLen");
    if ( needsXidClass ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "toString");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "toBytes");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "timestamp");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "compare");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "equal");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "isZero");
    }
    xl6StdlibApplyDeps(used);

    /* 只为实际使用的标准库函数注入 extern 声明。 */
    xl6StdlibBuilderInit(&builder);
    if (needsVfs) xl6StdlibBuilderAppend(&builder, xl6StdlibVfsSource);
    if ( needsCoreClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibCoreClassSource);
    }
    if (needsIoProtocols) xl6StdlibBuilderAppend(&builder, xl6StdlibIoProtocolsSource);
    if (needsConsoleOptions) xl6StdlibBuilderAppend(&builder, xl6StdlibConsoleOptionsSource);
    if (needsConsoleSession) xl6StdlibBuilderAppend(&builder, xl6StdlibConsoleSessionSource);
    if (needsIoReader) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibIoReaderSource);
        if (needsProcessStreams) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6ProcessReaderV1\" create(Process process, int stream)\n");
        if (needsFileClass) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6_xrt_fs_file_reader\" create(File file)\n");
        if (needsIoBuffer) xl6StdlibBuilderAppend(&builder, xl6StdlibIoReaderBufferSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
    }
    if (needsIoWriter) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibIoWriterSource);
        if (needsProcessStreams) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6ProcessWriterV1\" create(Process process)\n");
        if (needsFileClass) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6_xrt_fs_file_writer\" create(File file)\n");
        if (needsIoBuffer) xl6StdlibBuilderAppend(&builder, xl6StdlibIoWriterBufferSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
    }
    if (needsIoLines) xl6StdlibBuilderAppend(&builder, xl6StdlibIoLinesSource);
    if (needsStringIterator) xl6StdlibBuilderAppend(&builder, xl6StdlibStringIteratorSource);
    if (needsUtf8Validator) xl6StdlibBuilderAppend(&builder, xl6StdlibUtf8ValidatorSource);
    if (needsRegex) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexSource);
        if (needsRegexBuffer) xl6StdlibBuilderAppend(&builder, xl6StdlibRegexBufferSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexEndSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexReplacerSource);
        if (needsRegexBuffer) xl6StdlibBuilderAppend(&builder, xl6StdlibRegexReplacerBufferSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
    }
    if (needsRegexSet) xl6StdlibBuilderAppend(&builder, xl6StdlibRegexSetSource);
    if (needsPattern) xl6StdlibBuilderAppend(&builder, xl6StdlibPatternSource);
    if (needsDateTime) xl6StdlibBuilderAppend(&builder, xl6StdlibDateTimeSource);
    if (needsNetEngineClass || xl6StdlibNodeUsesTypeName(root,"NetEngineOptions") ||
        xl6StdlibNodeUsesTypeName(root,"NetBackend") ||
        xl6StdlibFuncUsed(used,"net","backendCompiled") ||
        xl6StdlibFuncUsed(used,"net","backendCapabilities") ||
        xl6StdlibFuncUsed(used,"net","backendName") ||
        xl6StdlibFuncUsed(used,"net","defaultBackend")) {
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetBackendSource);
    }
    if (needsDateTime || xl6StdlibNodeUsesTypeName(root, "TimeFold"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibTimeFoldSource);
    if (xl6StdlibNodeUsesTypeName(root, "RegexSplitFlags"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexSplitFlagsSource);
    if (xl6StdlibNodeUsesTypeName(root, "RegexFlags"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexFlagsSource);
    if (xl6StdlibNodeUsesTypeName(root, "NumberParseFlags"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibNumberParseFlagsSource);
    if (xl6StdlibNodeUsesTypeName(root, "IntegerTextFlags"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibIntegerTextFlagsSource);
    if (xl6StdlibNodeUsesTypeName(root, "FloatTextFlags"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibFloatTextFlagsSource);
    if ( xl6StdlibFuncUsed(used, "math", "classify") ||
         xl6StdlibNodeUsesTypeName(root, "FloatClass") ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFloatClassSource);
    }
    if ( needsRandomClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRandomClassSource);
        if ( needsRandomBufferMethods )
            xl6StdlibBuilderAppend(&builder, xl6StdlibRandomBufferMethodsSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibRandomClassEndSource);
    }
    if ( needsXidClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibXidClassSource);
    }
    if ( needsBufferClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibBufferClassSource);
    }
    if ( xl6StdlibNodeUsesTypeName(root, "FixedStack") ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFixedStackSource);
    }
    if ( needsSipHasher64Class ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibSipHasher64ClassSource);
        if ( needsSipHasher64BufferMethod )
            xl6StdlibBuilderAppend(&builder, xl6StdlibSipHasher64BufferMethodSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibSipHasher64ClassEndSource);
    }
    if ( needsNetAddressClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetAddressClassSource);
        if(needsNetAddressListClass)
            xl6StdlibBuilderAppend(&builder,xl6StdlibNetAddressListClassSource);
    }
    if(needsNetResolverOptionsClass)xl6StdlibBuilderAppend(&builder,xl6StdlibNetResolverOptionsSource);
    if(needsNetResolverClass){
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetResolveOperationSource);
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetResolverSource);
    }
    if(needsNetDefaultResolver)xl6StdlibBuilderAppend(&builder,xl6StdlibNetDefaultResolverSource);
    if ( needsNetInterfaceClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetInterfaceClassSource);
    }
    if(needsNetSocketClass)xl6StdlibBuilderAppend(&builder,xl6StdlibNetSocketSource);
    if ( needsNetBytesClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetBytesClassSource);
    }
    if ( needsNetEngineStatsClass ) {
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetEngineStateSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetEngineStatsClassSource);
    }
    else if(xl6StdlibNodeUsesTypeName(root,"NetEngineState"))
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetEngineStateSource);
    if ( needsNetResolverStatsClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetResolverStatsClassSource);
    }
    if ( needsNetProxyClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetProxyClassSource);
    }
    if ( needsNetEngineClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetEngineOptionsSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetEngineClassSource);
    }
    else if (xl6StdlibNodeUsesTypeName(root,"NetEngineOptions"))
        xl6StdlibBuilderAppend(&builder,xl6StdlibNetEngineOptionsSource);
    if ( needsTcpStreamStatsClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpStreamStatsClassSource);
    }
    if ( needsTcpServerStatsClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpServerStatsClassSource);
    }
    if ( needsTcpStreamClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpStreamClassSource);
        if ( needsTcpStreamFile ) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibTcpStreamFileMethodSource);
        }
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpStreamClassEndSource);
    }
    if ( needsTcpServerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpServerClassSource);
    }
    if ( needsUdpStatsClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibUdpStatsClassSource);
    }
    if ( needsUdpClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibUdpClassSource);
    }
    if ( needsTcpConnectConfigClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpConnectConfigClassSource);
    }
    if ( needsTcpListenConfigClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTcpListenConfigClassSource);
    }
    if ( needsUdpConfigClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibUdpConfigClassSource);
    }
    if ( needsTlsSessionClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTlsSessionClassSource);
    }
    if ( needsTlsStreamClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTlsStreamClassSource);
    }
    if ( needsTlsServerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTlsServerClassSource);
    }
    if ( needsHttp1Class ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttp1ClassSource);
    }
    if ( needsHttpDecoderClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpDecoderClassSource);
    }
    if ( needsWsFrameClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibWsFrameClassSource);
    }
    if ( needsWsHandshakeClass || needsWsUpgradeClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibWsHandshakeClassSource);
    }
    if ( needsWsUpgradeClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibWsUpgradeClassSource);
    }
    if ( needsWebSocketConfigClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketConfigClassSource);
    }
    if ( needsWebSocketClass ) {
        if ( needsWebSocketTls ) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketTlsExternSource);
        }
        xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketClassSource);
        if ( needsWebSocketTls ) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketTlsMethodSource);
        }
        xl6StdlibBuilderAppend(&builder, xl6StdlibWebSocketClassEndSource);
    }
    if ( needsLoggerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibLoggerClassSource);
        if (xl6StdlibFuncUsed(used,"log","setDefault"))
            xl6StdlibBuilderAppend(&builder, xl6StdlibLogDefaultScopeSource);
    }
    if ( needsSignalClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibSignalClassSource);
    }
    if ( needsSignalHandleClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibSignalHandleClassSource);
    }
    if ( needsTaskGroupClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTaskGroupClassSource);
    }
    if ( needsTemplateClass ) {
        if (needsTemplateFile) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateReadFileSource);
        if (needsTemplateVfs) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateReadVfsSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibTemplateClassSource);
        if (needsTemplateBuffer) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateBufferSource);
        if (needsTemplateWriter) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateWriterSource);
        if (needsTemplateFile) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateFileSource);
        if (needsTemplateVfs) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateVfsSource);
        xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateEndSource);
        if (needsTemplateEngine) {
            xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateEngineSource);
            if (needsTemplateFile) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateEngineFileSource);
            if (needsTemplateVfs) xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateEngineVfsSource);
            xl6StdlibBuilderAppend(&builder,xl6StdlibTemplateEngineEndSource);
        }
    }
    if ( needsCompress ) xl6StdlibBuilderAppend(&builder, xl6StdlibCompressOptionsSource);
    if ( needsDeflater ) xl6StdlibBuilderAppend(&builder, xl6StdlibDeflaterSource);
    if ( needsInflater ) xl6StdlibBuilderAppend(&builder, xl6StdlibInflaterSource);
    if (xl6StdlibNodeUsesTypeName(root, "DerValue") ||
        xl6StdlibNodeUsesTypeName(root, "DerReader") ||
        xl6StdlibNodeUsesTypeName(root, "DerBuilder") ||
        xl6StdlibNodeUsesTypeName(root, "PemBlock") ||
        xl6StdlibNodeUsesTypeName(root, "PemReader") ||
        xl6StdlibFuncUsed(used, "asn1", "parse") || xl6StdlibFuncUsed(used, "pem", "parse") || xl6StdlibFuncUsed(used, "pem", "decode")) xl6StdlibBuilderAppend(&builder, xl6StdlibAsn1ClassSource);
    if (needsNetResolverClass || needsTaskGroupClass || xl6StdlibHasConcurrentImport(root)) {
        bool needsChannelCases=xl6StdlibNodeUsesTypeName(root,"ChannelCase") ||
            xl6StdlibNodeUsesTypeName(root,"ChannelSelection");
        bool needsChannelDirect=xl6StdlibNodeUsesTypeName(root,"ChannelReceiveResult") ||
            xl6StdlibNodeUsesMemberName(root,"sendAsync") || xl6StdlibNodeUsesMemberName(root,"sendAsyncCancel") ||
            xl6StdlibNodeUsesMemberName(root,"recvAsync") || xl6StdlibNodeUsesMemberName(root,"recvAsyncCancel") ||
            xl6StdlibNodeUsesMemberName(root,"sendAsyncTimeout") || xl6StdlibNodeUsesMemberName(root,"sendAsyncUntil") ||
            xl6StdlibNodeUsesMemberName(root,"sendAsyncTimeoutCancel") || xl6StdlibNodeUsesMemberName(root,"sendAsyncUntilCancel") ||
            xl6StdlibNodeUsesMemberName(root,"recvAsyncTimeout") || xl6StdlibNodeUsesMemberName(root,"recvAsyncUntil") ||
            xl6StdlibNodeUsesMemberName(root,"recvAsyncTimeoutCancel") || xl6StdlibNodeUsesMemberName(root,"recvAsyncUntilCancel");
        if(needsChannelCases)xl6StdlibBuilderAppend(&builder,xl6StdlibChannelCaseSource);
        if(needsChannelDirect)xl6StdlibBuilderAppend(&builder,xl6StdlibChannelDirectFutureSource);
        if (xl6StdlibNodeUsesTypeName(root, "Once")) xl6StdlibBuilderAppend(&builder, xl6StdlibOnceSource);
        if (xl6StdlibNodeUsesTypeName(root,"Executor") || xl6StdlibNodeUsesTypeName(root,"ExecutorSnapshot"))
            xl6StdlibBuilderAppend(&builder,xl6StdlibExecutorSource);
        int needsTaskPoolClass=xl6StdlibNodeUsesTypeName(root,"TaskPool")||xl6StdlibNodeUsesTypeName(root,"TaskPoolSnapshot");
        if(needsTaskPoolClass)xl6StdlibBuilderAppend(&builder,xl6StdlibTaskPoolSource);
        if (xl6StdlibNodeUsesTypeName(root, "Event")) xl6StdlibBuilderAppend(&builder, xl6StdlibEventSource);
        if (xl6StdlibNodeUsesTypeName(root, "ThreadLocal")) xl6StdlibBuilderAppend(&builder, xl6StdlibThreadLocalSource);
        if (xl6StdlibNodeUsesTypeName(root, "CancellationSource") ||
            xl6StdlibNodeUsesTypeName(root, "CancellationToken") ||
            xl6StdlibNodeUsesTypeName(root, "CancellationRegistration") ||
            xl6StdlibNodeUsesTypeName(root, "Promise") || needsNetResolverClass || needsTaskGroupClass || needsChannelCases || needsChannelDirect || needsTaskPoolClass)
            xl6StdlibBuilderAppend(&builder, xl6StdlibCancellationSource);
        if(xl6StdlibNodeUsesTypeName(root,"Promise"))xl6StdlibBuilderAppend(&builder,xl6StdlibPromiseSource);
        if (xl6StdlibNodeUsesTypeName(root, "AtomicInt32")) xl6StdlibBuilderAppend(&builder, xl6StdlibAtomicInt32Source);
        if (xl6StdlibNodeUsesTypeName(root, "AtomicUInt32")) xl6StdlibBuilderAppend(&builder, xl6StdlibAtomicUInt32Source);
        if (xl6StdlibNodeUsesTypeName(root, "AtomicInt64")) xl6StdlibBuilderAppend(&builder, xl6StdlibAtomicInt64Source);
        if (xl6StdlibNodeUsesTypeName(root, "AtomicUInt64")) xl6StdlibBuilderAppend(&builder, xl6StdlibAtomicUInt64Source);
        if (xl6StdlibNodeUsesTypeName(root, "AtomicBool")) xl6StdlibBuilderAppend(&builder, xl6StdlibAtomicBoolSource);
    }
    if ( needsCryptoClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibCryptoClassSource);
    }
    if (xl6StdlibFuncUsed(used, "path", "parse") || xl6StdlibFuncUsed(used, "path", "components") ||
        xl6StdlibNodeUsesTypeName(root, "PathParts") || xl6StdlibNodeUsesTypeName(root, "PathIterator") ||
        xl6StdlibNodeUsesTypeName(root, "PathComponent"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsPathSource);
    if (xl6StdlibFuncUsed(used, "file", "walk") || xl6StdlibFuncUsed(used, "dir", "entries") ||
        xl6StdlibFuncUsed(used, "dir", "matchEntries") || xl6StdlibFuncUsed(used, "dir", "walk") ||
        xl6StdlibNodeUsesTypeName(root, "WalkEntry") || xl6StdlibNodeUsesTypeName(root, "WalkError"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsWalkSource);
    if (xl6StdlibFuncUsed(used,"file","copyTreeAsync") || xl6StdlibFuncUsed(used,"file","removeTreeAsync") ||
        xl6StdlibFuncUsed(used,"dir","copyAsync") || xl6StdlibFuncUsed(used,"dir","deleteAsync") ||
        xl6StdlibFuncUsed(used,"dir","cleanAsync") || xl6StdlibFuncUsed(used,"dir","statsAsync"))
        xl6StdlibBuilderAppend(&builder,xl6StdlibFsAsyncStatsSource);
    if (xl6StdlibFuncUsed(used,"file","copyTreeAsync") || xl6StdlibFuncUsed(used,"file","removeTreeAsync") ||
        xl6StdlibFuncUsed(used,"dir","copyAsync") || xl6StdlibFuncUsed(used,"dir","deleteAsync") ||
        xl6StdlibFuncUsed(used,"dir","cleanAsync") || xl6StdlibFuncUsed(used,"dir","statsAsync") ||
        xl6StdlibFuncUsed(used, "file", "walk") || xl6StdlibNodeUsesTypeName(root, "WalkStats") || xl6StdlibNodeUsesTypeName(root, "TreeStats") || xl6StdlibFuncUsed(used, "file", "walkStats") ||
        xl6StdlibFuncUsed(used, "file", "copyTree") || xl6StdlibFuncUsed(used, "file", "removeTree") ||
        xl6StdlibFuncUsed(used, "dir", "stats") || xl6StdlibFuncUsed(used, "dir", "count") ||
        xl6StdlibFuncUsed(used, "dir", "clean") || xl6StdlibFuncUsed(used, "dir", "delete"))
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsTreeSource);
    if ( needsFileClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsFileSource);
        if (needsVfs) xl6StdlibBuilderAppend(&builder, xl6StdlibFsFileVfsSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsStatSource);
        if (needsVfs) xl6StdlibBuilderAppend(&builder, xl6StdlibFsStatVfsSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsDirectorySource);
        if (needsVfs) xl6StdlibBuilderAppend(&builder, xl6StdlibFsDirectoryVfsSource);
        xl6StdlibBuilderAppend(&builder, "end\n");
        xl6StdlibBuilderAppend(&builder, xl6StdlibFsResourceSource);
    }
    if ( needsAsyncFileClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibAsyncFileClassSource);
    }
    if ( needsJsonClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibJsonClassSource);
    }
    if ( needsProcessClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibProcessClassSource);
        xl6StdlibBuilderAppend(&builder, xl6StdlibProcessStatusSource);
        if (needsLiveProcessClass) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6ProcessLiveStatusV1\" create(Process process)\n");
        if (needsPipelineClass) xl6StdlibBuilderAppend(&builder,
            "    extern \"xl6ProcessStageStatusV1\" create(ProcessStageResult stage)\n");
        xl6StdlibBuilderAppend(&builder, "end\n");
        xl6StdlibBuilderAppend(&builder, xl6StdlibProcessCommandSource);
        if (needsFileClass) xl6StdlibBuilderAppend(&builder, xl6StdlibProcessFileSource);
        if (needsLiveProcessClass) xl6StdlibBuilderAppend(&builder,
            "    func start() as Process\n        return Process(self)\n    end\n");
        xl6StdlibBuilderAppend(&builder, "end\n");
        if (needsLiveProcessClass) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibProcessLiveSource);
            if (needsProcessStreams) xl6StdlibBuilderAppend(&builder, xl6StdlibProcessStreamSource);
            xl6StdlibBuilderAppend(&builder, "end\n");
        }
        if (needsPipelineClass) {
            xl6StdlibBuilderAppend(&builder, xl6StdlibProcessPipelineSource);
        }
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
