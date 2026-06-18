#include "src/cgen/xl6_cgen.h"
#include "src/common/xl6_time_literal.h"

#include <limits.h>

typedef struct XL6_CgenVar {
    char* name;
    const XL6_AstNode* type;
} XL6_CgenVar;

typedef enum XL6_CgenCleanupKind {
    XL6_CGEN_CLEANUP_CUSTOM,
    XL6_CGEN_CLEANUP_CLASS,
    XL6_CGEN_CLEANUP_FUNCTION,
    XL6_CGEN_CLEANUP_XVALUE,
    XL6_CGEN_CLEANUP_AVLTB_ITER
} XL6_CgenCleanupKind;

typedef struct XL6_CgenCleanup {
    char* name;
    XL6_CgenCleanupKind kind;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* classTypeNode;
} XL6_CgenCleanup;

typedef struct XL6_CgenGenericInstance {
    const XL6_AstNode* func;
    const XL6_AstNode* typeArgs;
    int ownsTypeArgs;
} XL6_CgenGenericInstance;

typedef struct XL6_CgenGenericClassInstance {
    const XL6_AstNode* classNode;
    const XL6_AstNode* typeNode;
} XL6_CgenGenericClassInstance;

typedef struct XL6_CgenAnonCapture {
    char* name;
    const XL6_AstNode* type;
} XL6_CgenAnonCapture;

typedef struct XL6_CgenAnonFunc {
    const XL6_AstNode* node;
    int id;
    XL6_CgenAnonCapture* captures;
    size_t captureCount;
    size_t captureCapacity;
} XL6_CgenAnonFunc;

typedef struct XL6_CgenLoopTarget {
    size_t breakCleanupMark;
    size_t continueCleanupMark;
    int breakId;
    int continueId;
} XL6_CgenLoopTarget;

typedef struct XL6_CgenContext {
    FILE* out;
    XL6_AstNode* root;
    XL6_DiagList* diags;
    XL6_CgenVar* vars;
    size_t varCount;
    size_t varCapacity;
    char** emittedClasses;
    size_t emittedClassCount;
    size_t emittedClassCapacity;
    XL6_CgenCleanup* cleanups;
    size_t cleanupCount;
    size_t cleanupCapacity;
    XL6_CgenLoopTarget* loopTargets;
    size_t loopTargetCount;
    size_t loopTargetCapacity;
    XL6_CgenGenericInstance* genericInstances;
    size_t genericInstanceCount;
    size_t genericInstanceCapacity;
    XL6_CgenGenericClassInstance* genericClassInstances;
    size_t genericClassInstanceCount;
    size_t genericClassInstanceCapacity;
    XL6_CgenAnonFunc* anonFuncs;
    size_t anonFuncCount;
    size_t anonFuncCapacity;
    XL6_AstNode** ownedTypeNodes;
    size_t ownedTypeNodeCount;
    size_t ownedTypeNodeCapacity;
    unsigned long long* emittedFunctionReturns;
    size_t emittedFunctionReturnCount;
    size_t emittedFunctionReturnCapacity;
    const XL6_AstNode* currentGenericParams;
    const XL6_AstNode* currentGenericArgs;
    const XL6_AstNode* currentReturnType;
    const XL6_AstNode* currentReturnParams;
    const XL6_AstNode* currentReturns;
    const XL6_AstNode* currentReturnFunc;
    const XL6_AstNode* currentClass;
    const XL6_AstNode* currentCustom;
    const XL6_CgenAnonFunc* currentAnonFunc;
    int needsFunctionVarRuntime;
    int ok;
    int indent;
    int tempId;
} XL6_CgenContext;

static int xl6CgenParamRequiredCount(const XL6_AstNode* params);

static int xl6CTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static void xl6CgenError(XL6_CgenContext* ctx, const XL6_AstNode* node, const char* message)
{
    ctx->ok = 0;
    if ( ctx->diags != NULL ) {
        xl6DiagAdd(ctx->diags, node != NULL ? node->line : 1, node != NULL ? node->column : 1, message);
    }
}

static void xl6CgenIndent(XL6_CgenContext* ctx)
{
    int i;

    for ( i = 0; i < ctx->indent; ++i ) {
        fputs("    ", ctx->out);
    }
}

static unsigned int xl6CgenNextUtf8Codepoint(const unsigned char** p)
{
    const unsigned char* s;
    unsigned int cp;

    if ( p == NULL || *p == NULL || **p == '\0' ) {
        return 0;
    }

    s = *p;
    if ( s[0] < 0x80 ) {
        *p = s + 1;
        return s[0];
    }
    if ( (s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x1F) << 6) | (unsigned int)(s[1] & 0x3F);
        *p = s + 2;
        return cp;
    }
    if ( (s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x0F) << 12) |
             ((unsigned int)(s[1] & 0x3F) << 6) |
             (unsigned int)(s[2] & 0x3F);
        *p = s + 3;
        return cp;
    }
    if ( (s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 &&
         (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x07) << 18) |
             ((unsigned int)(s[1] & 0x3F) << 12) |
             ((unsigned int)(s[2] & 0x3F) << 6) |
             (unsigned int)(s[3] & 0x3F);
        *p = s + 4;
        return cp;
    }

    *p = s + 1;
    return s[0];
}

static int xl6CgenIsSimpleCIdent(const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }

    p = (const unsigned char*)text;
    if ( !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') ) {
        return 0;
    }
    for ( ++p; *p != '\0'; ++p ) {
        if ( !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_') ) {
            return 0;
        }
    }
    return 1;
}

static void xl6CgenFputsEncodedCodepoint(FILE* out, unsigned int cp)
{
    if ( out == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
        fputc((int)cp, out);
    } else if ( cp == '_' ) {
        fputs("__", out);
    } else if ( cp <= 0xFFFFU ) {
        fprintf(out, "_u%04X_", cp);
    } else {
        fprintf(out, "_u%06X_", cp);
    }
}

static void xl6CgenFputsEncodedText(FILE* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        fputc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        xl6CgenFputsEncodedCodepoint(out, xl6CgenNextUtf8Codepoint(&p));
    }
}

static void xl6CgenAppendText(char* buffer, size_t capacity, const char* text);

static void xl6CgenFputsCIdentCodepoint(FILE* out, unsigned int cp, int isFirst)
{
    if ( out == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
         (!isFirst && cp >= '0' && cp <= '9') ) {
        fputc((int)cp, out);
    } else if ( cp <= 0xFFFFU ) {
        fprintf(out, "_u%04X_", cp);
    } else {
        fprintf(out, "_u%06X_", cp);
    }
}

static void xl6CgenFputsCIdent(FILE* out, const char* text)
{
    const unsigned char* p;
    int isFirst;

    if ( text == NULL || text[0] == '\0' ) {
        fputc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        xl6CgenFputsCIdentCodepoint(out, xl6CgenNextUtf8Codepoint(&p), isFirst);
        isFirst = 0;
    }
}

static void xl6CgenAppendCIdentText(char* buffer, size_t capacity, const char* text)
{
    const unsigned char* p;
    char tmp[32];
    int isFirst;

    if ( text == NULL || text[0] == '\0' ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        unsigned int cp = xl6CgenNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
             (!isFirst && cp >= '0' && cp <= '9') ) {
            tmp[0] = (char)cp;
            tmp[1] = '\0';
        } else if ( cp <= 0xFFFFU ) {
            snprintf(tmp, sizeof(tmp), "_u%04X_", cp);
        } else {
            snprintf(tmp, sizeof(tmp), "_u%06X_", cp);
        }
        xl6CgenAppendText(buffer, capacity, tmp);
        isFirst = 0;
    }
}

static void xl6CgenCIdentText(char* buffer, size_t capacity, const char* text)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendCIdentText(buffer, capacity, text);
}

static void xl6CgenCustomTypeName(FILE* out, const char* name)
{
    fputs("XL_Custom_", out);
    xl6CgenFputsEncodedText(out, name);
}

static void xl6CgenClassTypeName(FILE* out, const char* name)
{
    fputs("XL_Class_", out);
    xl6CgenFputsEncodedText(out, name);
}

static void xl6CgenAppendText(char* buffer, size_t capacity, const char* text)
{
    size_t used;
    size_t left;

    if ( buffer == NULL || capacity == 0 || text == NULL ) {
        return;
    }
    used = strlen(buffer);
    if ( used + 1 >= capacity ) {
        return;
    }
    left = capacity - used - 1;
    strncat(buffer, text, left);
}

static void xl6CgenAppendMangledText(char* buffer, size_t capacity, const char* text)
{
    const unsigned char* p;
    char tmp[32];

    if ( text == NULL || text[0] == '\0' ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        unsigned int cp = xl6CgenNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
            tmp[0] = (char)cp;
            tmp[1] = '\0';
        } else if ( cp == '_' ) {
            strcpy(tmp, "__");
        } else if ( cp <= 0xFFFFU ) {
            snprintf(tmp, sizeof(tmp), "_u%04X_", cp);
        } else {
            snprintf(tmp, sizeof(tmp), "_u%06X_", cp);
        }
        xl6CgenAppendText(buffer, capacity, tmp);
    }
}

static void xl6CgenAppendTypeId(char* buffer, size_t capacity, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( typeNode == NULL ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    xl6CgenAppendMangledText(buffer, capacity, typeNode->text);
    args = typeNode->childCount > 0 && typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    xl6CgenAppendText(buffer, capacity, "_of");
    for ( i = 0; i < args->childCount; ++i ) {
        xl6CgenAppendText(buffer, capacity, "_");
        xl6CgenAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenCustomTypeRefName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Custom_");
    xl6CgenAppendMangledText(buffer, capacity, name);
}

static void xl6CgenClassTypeTextName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Class_");
    xl6CgenAppendMangledText(buffer, capacity, name);
}

static void xl6CgenClassTypeRefName(char* buffer, size_t capacity, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    xl6CgenClassTypeTextName(buffer, capacity, typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL && typeNode->childCount > 0 && typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        xl6CgenAppendText(buffer, capacity, "__");
        xl6CgenAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenClassMemberSymbol(FILE* out, const char* className, const char* memberName)
{
    fputs("xl_class_", out);
    xl6CgenFputsEncodedText(out, className);
    fputs("__", out);
    xl6CgenFputsEncodedText(out, memberName);
}

static XL6_AstNode* xl6CgenFindCustom(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_CUSTOM && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenFindClass(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_CLASS && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenIsCustomType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindCustom(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsClassType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindClass(ctx, typeNode->text) != NULL;
}

static int xl6CgenClassHasTypeParams(const XL6_AstNode* classNode)
{
    const XL6_AstNode* typeParams;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static const XL6_AstNode* xl6CgenClassTypeParams(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
}

static const XL6_AstNode* xl6CgenFuncTypeParams(const XL6_AstNode* funcNode)
{
    return funcNode != NULL && funcNode->childCount > 4 ? funcNode->children[4] : NULL;
}

static int xl6CgenFuncIsGeneric(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* typeParams;

    typeParams = xl6CgenFuncTypeParams(funcNode);
    return typeParams != NULL && typeParams->childCount > 0;
}

static XL6_AstNode* xl6CgenCloneCanonicalTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* node);

static const XL6_AstNode* xl6CgenSubstituteGenericType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || typeNode == NULL || typeNode->text == NULL ||
         ctx->currentGenericParams == NULL || ctx->currentGenericArgs == NULL ) {
        return typeNode;
    }
    for ( i = 0; i < ctx->currentGenericParams->childCount && i < ctx->currentGenericArgs->childCount; ++i ) {
        const XL6_AstNode* param = ctx->currentGenericParams->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, typeNode->text) ) {
            return ctx->currentGenericArgs->children[i];
        }
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenClassBody(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenFindClassFunc(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != XL6_AST_FUNC || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassSpecial(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_CREATE && xl6CTextEquals(member->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static int xl6CgenClassHasCreate(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "create") != NULL;
}

static const XL6_AstNode* xl6CgenCallableParams(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }

    /*
        不同声明节点的参数表位置不同，这里统一成“可调用参数表”。
        普通 func / custom extern func: child[1]
        class create: child[0]
        custom extern create/destroy: child[1]
    */
    if ( node->kind == XL6_AST_CREATE ) {
        return node->childCount > 0 ? node->children[0] : NULL;
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenFindClassCreateCallableWithoutArgs(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CREATE &&
             xl6CTextEquals(member->text, "create") &&
             xl6CgenParamRequiredCount(xl6CgenCallableParams(member)) == 0 ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindCustomMember(const XL6_AstNode* customNode, XL6_AstKind kind, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( customNode == NULL || customNode->childCount == 0 || name == NULL ) {
        return NULL;
    }
    body = customNode->children[0];
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != kind || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindCustomSpecial(const XL6_AstNode* customNode, const char* name)
{
    return xl6CgenFindCustomMember(customNode, XL6_AST_CUSTOM_EXTERN_SPECIAL, name);
}

static const XL6_AstNode* xl6CgenFindCustomExternFunc(const XL6_AstNode* customNode, const char* name)
{
    return xl6CgenFindCustomMember(customNode, XL6_AST_CUSTOM_EXTERN_FUNC, name);
}

static int xl6CgenIsExternalVarDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXTERN_VAR || node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST);
}

static const XL6_AstNode* xl6CgenExternalVarNameNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarTypeNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarInitExpr(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6CgenIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6CgenTypeAliasTargetType(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindTypeAlias(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( xl6CgenIsTypeAliasDecl(node) && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveAliasType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6CgenFindTypeAlias(ctx, typeNode->text);
        if ( aliasNode == NULL ) {
            return typeNode;
        }
        typeNode = xl6CgenTypeAliasTargetType(aliasNode);
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenFindExternalVar(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        const XL6_AstNode* nameNode;
        if ( !xl6CgenIsExternalVarDecl(node) ) {
            continue;
        }
        nameNode = xl6CgenExternalVarNameNode(node);
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindTrackedVarType(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = ctx->varCount; i > 0; --i ) {
        if ( xl6CTextEquals(ctx->vars[i - 1].name, name) ) {
            return ctx->vars[i - 1].type;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindVarType(XL6_CgenContext* ctx, const char* name)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* externalVar;

    typeNode = xl6CgenFindTrackedVarType(ctx, name);
    if ( typeNode != NULL ) {
        return typeNode;
    }
    externalVar = xl6CgenFindExternalVar(ctx, name);
    return xl6CgenExternalVarTypeNode(externalVar);
}

static void xl6CgenAddVarType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* type)
{
    XL6_CgenVar* newItems;
    size_t newCapacity;

    if ( ctx == NULL || name == NULL || type == NULL ) {
        return;
    }
    if ( ctx->varCount == ctx->varCapacity ) {
        newCapacity = ctx->varCapacity == 0 ? 16 : ctx->varCapacity * 2;
        newItems = (XL6_CgenVar*)realloc(ctx->vars, newCapacity * sizeof(XL6_CgenVar));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, type, "C 生成变量类型表内存不足");
            return;
        }
        ctx->vars = newItems;
        ctx->varCapacity = newCapacity;
    }
    ctx->vars[ctx->varCount].name = xl6StrDupRange(name, strlen(name));
    ctx->vars[ctx->varCount].type = type;
    ctx->varCount += 1;
}

static void xl6CgenFreeVars(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->varCount; ++i ) {
        xl6Free(ctx->vars[i].name);
    }
    xl6Free(ctx->vars);
    ctx->vars = NULL;
    ctx->varCount = 0;
    ctx->varCapacity = 0;
}

static void xl6CgenFreeVarsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->varCount ) {
        return;
    }
    for ( i = mark; i < ctx->varCount; ++i ) {
        xl6Free(ctx->vars[i].name);
    }
    ctx->varCount = mark;
}

static int xl6CgenClassWasEmitted(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->emittedClassCount; ++i ) {
        if ( xl6CTextEquals(ctx->emittedClasses[i], name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenMarkClassEmitted(XL6_CgenContext* ctx, const char* name)
{
    char** newItems;
    size_t newCapacity;

    if ( ctx == NULL || name == NULL || xl6CgenClassWasEmitted(ctx, name) ) {
        return;
    }
    if ( ctx->emittedClassCount == ctx->emittedClassCapacity ) {
        newCapacity = ctx->emittedClassCapacity == 0 ? 16 : ctx->emittedClassCapacity * 2;
        newItems = (char**)realloc(ctx->emittedClasses, newCapacity * sizeof(char*));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成 class 表内存不足");
            return;
        }
        ctx->emittedClasses = newItems;
        ctx->emittedClassCapacity = newCapacity;
    }
    ctx->emittedClasses[ctx->emittedClassCount] = xl6StrDupRange(name, strlen(name));
    ctx->emittedClassCount += 1;
}

static void xl6CgenFreeEmittedClasses(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->emittedClassCount; ++i ) {
        xl6Free(ctx->emittedClasses[i]);
    }
    xl6Free(ctx->emittedClasses);
    ctx->emittedClasses = NULL;
    ctx->emittedClassCount = 0;
    ctx->emittedClassCapacity = 0;
}

static int xl6CgenHasFunctionReturnStruct(const XL6_CgenContext* ctx, unsigned long long signatureId)
{
    size_t i;

    if ( ctx == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->emittedFunctionReturnCount; ++i ) {
        if ( ctx->emittedFunctionReturns[i] == signatureId ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenRememberFunctionReturnStruct(XL6_CgenContext* ctx, unsigned long long signatureId)
{
    unsigned long long* newItems;
    size_t newCapacity;

    if ( ctx == NULL || xl6CgenHasFunctionReturnStruct(ctx, signatureId) ) {
        return 0;
    }
    if ( ctx->emittedFunctionReturnCount == ctx->emittedFunctionReturnCapacity ) {
        newCapacity = ctx->emittedFunctionReturnCapacity == 0 ? 16 : ctx->emittedFunctionReturnCapacity * 2;
        newItems = (unsigned long long*)realloc(ctx->emittedFunctionReturns, newCapacity * sizeof(unsigned long long));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成 function 返回结构体表内存不足");
            return 0;
        }
        ctx->emittedFunctionReturns = newItems;
        ctx->emittedFunctionReturnCapacity = newCapacity;
    }
    ctx->emittedFunctionReturns[ctx->emittedFunctionReturnCount] = signatureId;
    ctx->emittedFunctionReturnCount += 1;
    return 1;
}

static void xl6CgenFreeFunctionReturnStructs(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xl6Free(ctx->emittedFunctionReturns);
    ctx->emittedFunctionReturns = NULL;
    ctx->emittedFunctionReturnCount = 0;
    ctx->emittedFunctionReturnCapacity = 0;
}

static int xl6CgenIsBuiltinTypeName(const char* name)
{
    static const char* names[] = {
        "null", "bool",
        "int", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
        "float", "float32", "float64",
        "string", "time", "point", "function", "var", "type",
        "array", "list", "set", "dict"
    };
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6CTextEquals(name, names[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsIntegerTypeName(const char* name)
{
    return xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int8") ||
           xl6CTextEquals(name, "uint8") || xl6CTextEquals(name, "int16") ||
           xl6CTextEquals(name, "uint16") || xl6CTextEquals(name, "int32") ||
           xl6CTextEquals(name, "uint32") || xl6CTextEquals(name, "int64") ||
           xl6CTextEquals(name, "uint64");
}

static int xl6CgenIsFloatTypeName(const char* name)
{
    return xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float32") ||
           xl6CTextEquals(name, "float64");
}

static int xl6CgenIsBuiltinConvertTypeName(const char* name)
{
    return xl6CgenIsIntegerTypeName(name) || xl6CgenIsFloatTypeName(name) ||
           xl6CTextEquals(name, "bool") || xl6CTextEquals(name, "string") ||
           xl6CTextEquals(name, "time") || xl6CTextEquals(name, "point") ||
           xl6CTextEquals(name, "var");
}

static const char* xl6CgenConvertCTypeName(const char* name)
{
    if ( xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int64") ||
         xl6CTextEquals(name, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(name, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(name, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(name, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(name, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(name, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(name, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(name, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(name, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(name, "point") ) {
        return "void*";
    }
    if ( xl6CTextEquals(name, "var") ) {
        return "xvalue";
    }
    return "int";
}

static int xl6CgenIsKnownTypeName(XL6_CgenContext* ctx, const char* name)
{
    return xl6CgenIsBuiltinTypeName(name) ||
           xl6CgenFindTypeAlias(ctx, name) != NULL ||
           xl6CgenFindClass(ctx, name) != NULL ||
           xl6CgenFindCustom(ctx, name) != NULL;
}

static const char* xl6CgenTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    static char customTypeName[256];

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6CgenError(ctx, typeNode, "缺少类型");
        return "int";
    }

    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    if ( xl6CTextEquals(typeNode->text, "type") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "var") ||
         xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>") ||
         xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>") ||
         xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>") ||
         xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>") ) {
        return "xvalue";
    }
    if ( xl6CTextEquals(typeNode->text, "function") ) {
        return "XL6_Function";
    }
    if ( xl6CgenIsCustomType(ctx, typeNode) ) {
        xl6CgenCustomTypeRefName(customTypeName, sizeof(customTypeName), typeNode->text);
        return customTypeName;
    }
    if ( xl6CgenIsClassType(ctx, typeNode) ) {
        xl6CgenClassTypeRefName(customTypeName, sizeof(customTypeName), typeNode);
        return customTypeName;
    }

    xl6CgenError(ctx, typeNode, "当前 C 生成暂不支持该类型");
    return "int";
}

static const char* xl6CgenCAbiTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "int64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "int8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "uint8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "int16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "uint16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "uint32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "uint64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    return xl6CgenTypeName(ctx, typeNode);
}

static XL6_AstNode* xl6CgenFindFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC) &&
             (node->kind != XL6_AST_FUNC || !xl6CgenFuncIsGeneric(node)) &&
             node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenTypeNodeEqual(const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->kind != b->kind || a->childCount != b->childCount ) {
        return 0;
    }
    if ( (a->text == NULL) != (b->text == NULL) ) {
        return 0;
    }
    if ( a->text != NULL && !xl6CTextEquals(a->text, b->text) ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        if ( !xl6CgenTypeNodeEqual(a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenTypeArgsEqual(const XL6_AstNode* a, const XL6_AstNode* b)
{
    return xl6CgenTypeNodeEqual(a, b);
}

static int xl6CgenHasGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs)
{
    size_t i;

    if ( ctx == NULL || func == NULL || typeArgs == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        if ( ctx->genericInstances[i].func == func && xl6CgenTypeArgsEqual(ctx->genericInstances[i].typeArgs, typeArgs) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenAddGenericInstanceEx(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs, int ownsTypeArgs)
{
    XL6_CgenGenericInstance* newItems;
    size_t newCapacity;
    XL6_AstNode* canonicalArgs;

    if ( ctx == NULL || func == NULL || typeArgs == NULL ) {
        if ( ownsTypeArgs ) {
            xl6AstFree((XL6_AstNode*)typeArgs);
        }
        return 0;
    }
    canonicalArgs = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( ownsTypeArgs ) {
        xl6AstFree((XL6_AstNode*)typeArgs);
    }
    if ( canonicalArgs == NULL ) {
        xl6CgenError(ctx, func, "泛型实例类型实参规范化失败");
        return 0;
    }
    if ( xl6CgenHasGenericInstance(ctx, func, canonicalArgs) ) {
        xl6AstFree(canonicalArgs);
        return 0;
    }
    if ( ctx->genericInstanceCount == ctx->genericInstanceCapacity ) {
        newCapacity = ctx->genericInstanceCapacity == 0 ? 16 : ctx->genericInstanceCapacity * 2;
        newItems = (XL6_CgenGenericInstance*)realloc(ctx->genericInstances, newCapacity * sizeof(XL6_CgenGenericInstance));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, func, "C 生成泛型实例表内存不足");
            xl6AstFree(canonicalArgs);
            return 0;
        }
        ctx->genericInstances = newItems;
        ctx->genericInstanceCapacity = newCapacity;
    }
    ctx->genericInstances[ctx->genericInstanceCount].func = func;
    ctx->genericInstances[ctx->genericInstanceCount].typeArgs = canonicalArgs;
    ctx->genericInstances[ctx->genericInstanceCount].ownsTypeArgs = 1;
    ctx->genericInstanceCount += 1;
    return 1;
}

static void xl6CgenAddGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs)
{
    (void)xl6CgenAddGenericInstanceEx(ctx, func, typeArgs, 0);
}

static void xl6CgenAddOwnedGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, XL6_AstNode* typeArgs)
{
    (void)xl6CgenAddGenericInstanceEx(ctx, func, typeArgs, 1);
}

static void xl6CgenFreeGenericInstances(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        if ( ctx->genericInstances[i].ownsTypeArgs ) {
            xl6AstFree((XL6_AstNode*)ctx->genericInstances[i].typeArgs);
        }
    }
    xl6Free(ctx->genericInstances);
    ctx->genericInstances = NULL;
    ctx->genericInstanceCount = 0;
    ctx->genericInstanceCapacity = 0;
}

static int xl6CgenHasGenericClassInstance(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        if ( ctx->genericClassInstances[i].classNode == classNode &&
             xl6CgenTypeNodeEqual(ctx->genericClassInstances[i].typeNode, typeNode) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAddGenericClassInstance(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    XL6_CgenGenericClassInstance* newItems;
    size_t newCapacity;
    XL6_AstNode* canonicalType;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL ) {
        return;
    }
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    if ( canonicalType == NULL ) {
        xl6CgenError(ctx, classNode, "泛型 class 实例类型规范化失败");
        return;
    }
    if ( xl6CgenHasGenericClassInstance(ctx, classNode, canonicalType) ) {
        xl6AstFree(canonicalType);
        return;
    }
    if ( ctx->genericClassInstanceCount == ctx->genericClassInstanceCapacity ) {
        newCapacity = ctx->genericClassInstanceCapacity == 0 ? 8 : ctx->genericClassInstanceCapacity * 2;
        newItems = (XL6_CgenGenericClassInstance*)realloc(ctx->genericClassInstances, newCapacity * sizeof(XL6_CgenGenericClassInstance));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, classNode, "泛型 class 实例表内存不足");
            xl6AstFree(canonicalType);
            return;
        }
        ctx->genericClassInstances = newItems;
        ctx->genericClassInstanceCapacity = newCapacity;
    }
    ctx->genericClassInstances[ctx->genericClassInstanceCount].classNode = classNode;
    ctx->genericClassInstances[ctx->genericClassInstanceCount].typeNode = canonicalType;
    ctx->genericClassInstanceCount += 1;
}

static void xl6CgenFreeGenericClassInstances(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        xl6AstFree((XL6_AstNode*)ctx->genericClassInstances[i].typeNode);
    }
    xl6Free(ctx->genericClassInstances);
    ctx->genericClassInstances = NULL;
    ctx->genericClassInstanceCount = 0;
    ctx->genericClassInstanceCapacity = 0;
}

static XL6_CgenAnonFunc* xl6CgenFindAnonFuncInfo(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        if ( ctx->anonFuncs[i].node == node ) {
            return &ctx->anonFuncs[i];
        }
    }
    return NULL;
}

static XL6_CgenAnonFunc* xl6CgenAddAnonFuncInfo(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenAnonFunc* info;
    XL6_CgenAnonFunc* newItems;
    size_t newCapacity;

    info = xl6CgenFindAnonFuncInfo(ctx, node);
    if ( info != NULL ) {
        return info;
    }
    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    if ( ctx->anonFuncCount == ctx->anonFuncCapacity ) {
        newCapacity = ctx->anonFuncCapacity == 0 ? 8 : ctx->anonFuncCapacity * 2;
        newItems = (XL6_CgenAnonFunc*)realloc(ctx->anonFuncs, newCapacity * sizeof(XL6_CgenAnonFunc));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, node, "匿名函数表内存不足");
            return NULL;
        }
        ctx->anonFuncs = newItems;
        memset(ctx->anonFuncs + ctx->anonFuncCapacity, 0, (newCapacity - ctx->anonFuncCapacity) * sizeof(XL6_CgenAnonFunc));
        ctx->anonFuncCapacity = newCapacity;
    }
    info = &ctx->anonFuncs[ctx->anonFuncCount];
    memset(info, 0, sizeof(*info));
    info->node = node;
    info->id = (int)ctx->anonFuncCount;
    ctx->anonFuncCount += 1;
    return info;
}

static int xl6CgenAnonHasCapture(const XL6_CgenAnonFunc* info, const char* name)
{
    size_t i;

    if ( info == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( xl6CTextEquals(info->captures[i].name, name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAnonAddCapture(XL6_CgenContext* ctx, XL6_CgenAnonFunc* info, const char* name, const XL6_AstNode* type)
{
    XL6_CgenAnonCapture* newItems;
    size_t newCapacity;

    if ( ctx == NULL || info == NULL || name == NULL || type == NULL || xl6CgenAnonHasCapture(info, name) ) {
        return;
    }
    if ( info->captureCount == info->captureCapacity ) {
        newCapacity = info->captureCapacity == 0 ? 4 : info->captureCapacity * 2;
        newItems = (XL6_CgenAnonCapture*)realloc(info->captures, newCapacity * sizeof(XL6_CgenAnonCapture));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, info->node, "匿名函数捕获表内存不足");
            return;
        }
        info->captures = newItems;
        info->captureCapacity = newCapacity;
    }
    info->captures[info->captureCount].name = xl6StrDupRange(name, strlen(name));
    info->captures[info->captureCount].type = type;
    info->captureCount += 1;
}

static void xl6CgenFreeAnonFuncs(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        for ( j = 0; j < ctx->anonFuncs[i].captureCount; ++j ) {
            xl6Free(ctx->anonFuncs[i].captures[j].name);
        }
        xl6Free(ctx->anonFuncs[i].captures);
    }
    xl6Free(ctx->anonFuncs);
    ctx->anonFuncs = NULL;
    ctx->anonFuncCount = 0;
    ctx->anonFuncCapacity = 0;
}

static void xl6CgenOwnTypeNode(XL6_CgenContext* ctx, XL6_AstNode* typeNode)
{
    XL6_AstNode** newItems;
    size_t newCapacity;

    if ( ctx == NULL || typeNode == NULL ) {
        return;
    }
    if ( ctx->ownedTypeNodeCount == ctx->ownedTypeNodeCapacity ) {
        newCapacity = ctx->ownedTypeNodeCapacity == 0 ? 8 : ctx->ownedTypeNodeCapacity * 2;
        newItems = (XL6_AstNode**)realloc(ctx->ownedTypeNodes, newCapacity * sizeof(XL6_AstNode*));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "C 生成类型节点表内存不足");
            return;
        }
        ctx->ownedTypeNodes = newItems;
        ctx->ownedTypeNodeCapacity = newCapacity;
    }
    ctx->ownedTypeNodes[ctx->ownedTypeNodeCount] = typeNode;
    ctx->ownedTypeNodeCount += 1;
}

static void xl6CgenFreeOwnedTypeNodes(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->ownedTypeNodeCount; ++i ) {
        xl6AstFree(ctx->ownedTypeNodes[i]);
    }
    xl6Free(ctx->ownedTypeNodes);
    ctx->ownedTypeNodes = NULL;
    ctx->ownedTypeNodeCount = 0;
    ctx->ownedTypeNodeCapacity = 0;
}

static int xl6CgenCallIsCustomCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outCustom);
static int xl6CgenCallIsClassCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outClass);
static XL6_AstNode* xl6CgenResolveCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name);
static const XL6_AstNode* xl6CgenResolveClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call, const char* name, const XL6_AstNode** outOwnerClass);
static const XL6_AstNode* xl6CgenResolveClassCreate(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call);
static const XL6_AstNode* xl6CgenResolveCustomFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name, int reportNoMatch);
static const XL6_AstNode* xl6CgenResolveCustomExternFuncWithReport(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name, int reportNoMatch);
static const XL6_AstNode* xl6CgenResolveCustomExternFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name);
static const XL6_AstNode* xl6CgenResolveCustomCreate(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call);
static XL6_AstNode* xl6CgenBaseClass(XL6_CgenContext* ctx, const XL6_AstNode* classNode);
static XL6_AstNode* xl6CgenFindExternFunc(XL6_CgenContext* ctx, const char* name);
static void xl6CgenFuncSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode);
static void xl6CgenClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* memberNode);
static void xl6CgenClassDefaultInitFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath);
static void xl6CgenCollectGenericInstances(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenGenericClassMemberSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const char* memberName);
static void xl6CgenGenericClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* memberNode);
static int xl6CgenFixedParamCount(const XL6_AstNode* params);
static int xl6CgenParamRequiredCount(const XL6_AstNode* params);
static const XL6_AstNode* xl6CgenCallableParams(const XL6_AstNode* node);
static int xl6CgenParamIsArrayVararg(const XL6_AstNode* param);
static int xl6CgenParamIsDictVararg(const XL6_AstNode* param);
static int xl6CgenParamIsVararg(const XL6_AstNode* param);
static const XL6_AstNode* xl6CgenFindArgForParam(const XL6_AstNode* args, const char* name, int index);
static const XL6_AstNode* xl6CgenMemberFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member);
static XL6_AstNode* xl6CgenCloneMemberFieldTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* member);
static int xl6CgenIsFunctionType(const XL6_AstNode* typeNode);
static int xl6CgenIsBareFunctionType(const XL6_AstNode* typeNode);
static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsXValueType(const XL6_AstNode* typeNode);
static const XL6_AstNode* xl6CgenFunctionReturnTypes(const XL6_AstNode* functionType);
static void xl6CgenRegisterParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params);
static unsigned long long xl6CgenFunctionSignatureId(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static unsigned long long xl6CgenFunctionSignatureIdForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static void xl6CgenFunctionReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static void xl6CgenFunctionReturnStructNameForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static void xl6CgenFunctionReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static void xl6CgenFunctionReturnStructDeclForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static XL6_AstNode* xl6CgenFindFunctionValueFunc(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode);
static void xl6CgenFunctionValueMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* targetType);
static void xl6CgenFunctionValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType);
static int xl6CgenAnonCaptureIndex(const XL6_CgenAnonFunc* info, const char* name);
static int xl6CgenIsRangeCall(const XL6_AstNode* node);
static void xl6CgenRangeContainsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* rangeCall);
static void xl6CgenInExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenExprIntPowKind(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static void xl6CgenPowExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenApproxExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenStringConcatExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenStringCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenTimeLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static const XL6_AstNode* xl6CgenArgExpr(const XL6_AstNode* arg);
static void xl6CgenCStringLiteral(FILE* out, const char* text);
static void xl6CgenArrayVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args, int fixedCount);
static void xl6CgenEmitAutoBaseCreateForValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* atNode);
static void xl6CgenHashText(unsigned long long* hash, const char* text);
static void xl6CgenHashTypeNode(unsigned long long* hash, const XL6_AstNode* typeNode);
static int xl6CgenIsKnownTypeName(XL6_CgenContext* ctx, const char* name);
static void xl6CgenEmitTypeIdForName(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at);
static void xl6CgenEmitTypeOfExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static XL6_AstNode* xl6CgenCloneExprTypeForTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static XL6_AstNode* xl6CgenResolveCallFuncInternal(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs, int report);
static XL6_AstNode* xl6CgenResolveCallFuncWithTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs);
static XL6_AstNode* xl6CgenResolveExplicitGenericCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, const XL6_AstNode* typeArgs, int report);

static XL6_AstNode* xl6CgenCloneTypeNode(const XL6_AstNode* node)
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
        XL6_AstNode* childCopy = xl6CgenCloneTypeNode(node->children[i]);
        if ( childCopy == NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, childCopy);
    }
    return copy;
}

static XL6_AstNode* xl6CgenNewSimpleType(const char* name, const XL6_AstNode* at)
{
    XL6_AstNode* node;

    if ( name == NULL ) {
        return NULL;
    }
    node = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    if ( node != NULL && at != NULL ) {
        node->line = at->line;
        node->column = at->column;
    }
    return node;
}

static XL6_AstNode* xl6CgenCloneCanonicalTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* concrete;
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }

    concrete = xl6CgenSubstituteGenericType(ctx, node);
    concrete = xl6CgenResolveAliasType(ctx, concrete);
    if ( concrete == NULL ) {
        return NULL;
    }

    if ( concrete->text != NULL ) {
        copy = xl6AstNewText(concrete->kind, concrete->text, strlen(concrete->text), NULL);
    } else {
        copy = xl6AstNew(concrete->kind, NULL);
    }
    if ( copy == NULL ) {
        return NULL;
    }
    copy->line = concrete->line;
    copy->column = concrete->column;

    for ( i = 0; i < concrete->childCount; ++i ) {
        XL6_AstNode* childCopy = xl6CgenCloneCanonicalTypeNode(ctx, concrete->children[i]);
        if ( childCopy == NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, childCopy);
    }
    return copy;
}

static XL6_AstNode* xl6CgenNewGenericTypeNode(const char* name, XL6_AstNode* typeArg, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* args;

    if ( typeArg == NULL ) {
        return xl6CgenNewSimpleType(name, at);
    }

    typeNode = xl6CgenNewSimpleType(name, at);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( typeNode == NULL || args == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(args);
        xl6AstFree(typeArg);
        return NULL;
    }

    xl6AstAddChild(args, typeArg);
    xl6AstAddChild(typeNode, args);
    return typeNode;
}

static XL6_AstNode* xl6CgenCloneCommonLiteralElementType(XL6_CgenContext* ctx, const XL6_AstNode* literal, int usePairValue)
{
    XL6_AstNode* common;
    size_t i;

    if ( literal == NULL || literal->childCount == 0 ) {
        return NULL;
    }

    common = NULL;
    for ( i = 0; i < literal->childCount; ++i ) {
        const XL6_AstNode* item = literal->children[i];
        const XL6_AstNode* value = usePairValue ?
            (item != NULL && item->childCount > 1 ? item->children[1] : NULL) :
            item;
        XL6_AstNode* valueType = xl6CgenCloneExprTypeForInference(ctx, value);
        XL6_AstNode* canonicalType;

        if ( valueType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }
        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, valueType);
        xl6AstFree(valueType);
        if ( canonicalType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }

        if ( common == NULL ) {
            common = canonicalType;
        } else if ( xl6CgenTypeNodeEqual(common, canonicalType) ) {
            xl6AstFree(canonicalType);
        } else {
            xl6AstFree(common);
            xl6AstFree(canonicalType);
            return NULL;
        }
    }
    return common;
}

static const char* xl6CgenContainerLiteralName(const XL6_AstNode* literal)
{
    if ( literal == NULL ) {
        return NULL;
    }
    switch ( literal->kind ) {
    case XL6_AST_ARRAY_LITERAL:
        return "array";
    case XL6_AST_LIST_LITERAL:
        return "list";
    case XL6_AST_SET_LITERAL:
        return "set";
    case XL6_AST_DICT_LITERAL:
        return "dict";
    default:
        return NULL;
    }
}

static XL6_AstNode* xl6CgenCloneContainerLiteralType(
    XL6_CgenContext* ctx,
    const char* forcedName,
    const XL6_AstNode* literal,
    const XL6_AstNode* at)
{
    const char* name;
    int usePairValue;
    XL6_AstNode* elementType;

    name = forcedName != NULL ? forcedName : xl6CgenContainerLiteralName(literal);
    if ( name == NULL ) {
        return NULL;
    }

    if ( literal == NULL || literal->childCount == 0 ) {
        return xl6CgenNewSimpleType(name, at);
    }

    usePairValue = xl6CTextEquals(name, "list") || xl6CTextEquals(name, "dict");
    elementType = xl6CgenCloneCommonLiteralElementType(ctx, literal, usePairValue);
    if ( elementType == NULL ) {
        return xl6CgenNewSimpleType(name, at);
    }
    return xl6CgenNewGenericTypeNode(name, elementType, at);
}

static XL6_AstNode* xl6CgenNewFunctionTypeFromParts(const XL6_AstNode* params, const XL6_AstNode* returns, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* paramTypes;
    XL6_AstNode* returnTypes;
    size_t i;

    typeNode = xl6CgenNewSimpleType("function", at);
    paramTypes = xl6AstNew(XL6_AST_LIST, NULL);
    returnTypes = returns != NULL ? xl6CgenCloneTypeNode(returns) : xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( typeNode == NULL || paramTypes == NULL || returnTypes == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(paramTypes);
        xl6AstFree(returnTypes);
        return NULL;
    }

    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* typeCopy;
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ) {
                continue;
            }
            typeCopy = xl6CgenCloneTypeNode(param->children[0]);
            if ( typeCopy == NULL ) {
                xl6AstFree(typeNode);
                xl6AstFree(paramTypes);
                xl6AstFree(returnTypes);
                return NULL;
            }
            xl6AstAddChild(paramTypes, typeCopy);
        }
    }

    xl6AstAddChild(typeNode, paramTypes);
    xl6AstAddChild(typeNode, returnTypes);
    return typeNode;
}

static XL6_AstNode* xl6CgenAnonFunctionType(const XL6_AstNode* anon)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( anon == NULL || anon->kind != XL6_AST_ANON_FUNC ) {
        return NULL;
    }
    params = anon->childCount > 0 ? anon->children[0] : NULL;
    returns = anon->childCount > 1 ? anon->children[1] : NULL;
    return xl6CgenNewFunctionTypeFromParts(params, returns, anon);
}

static XL6_AstNode* xl6CgenGenericClassCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call, XL6_AstNode** outClass)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* typeNode;
    XL6_AstNode* argsCopy;
    const XL6_AstNode* classParams;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    instant = call->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || typeArgs == NULL ) {
        return NULL;
    }

    classNode = xl6CgenFindClass(ctx, nameNode->text);
    if ( classNode == NULL || !xl6CgenClassHasTypeParams(classNode) ) {
        return NULL;
    }
    classParams = xl6CgenClassTypeParams(classNode);
    if ( classParams == NULL || classParams->childCount != typeArgs->childCount ) {
        xl6CgenError(ctx, call, "泛型 class 构造类型参数数量不匹配");
        return NULL;
    }

    typeNode = xl6CgenNewSimpleType(nameNode->text, nameNode);
    argsCopy = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( typeNode == NULL || argsCopy == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argsCopy);
        xl6CgenError(ctx, call, "泛型 class 构造类型节点内存不足");
        return NULL;
    }
    xl6AstAddChild(typeNode, argsCopy);
    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    return typeNode;
}

static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* varType;
    XL6_AstNode* receiverType;

    if ( expr == NULL ) {
        return NULL;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return xl6CgenNewSimpleType("int", expr);
    case XL6_AST_FLOAT:
        return xl6CgenNewSimpleType("float", expr);
    case XL6_AST_STRING:
        return xl6CgenNewSimpleType("string", expr);
    case XL6_AST_TIME:
        return xl6CgenNewSimpleType("time", expr);
    case XL6_AST_BOOL:
        return xl6CgenNewSimpleType("bool", expr);
    case XL6_AST_NULL:
        return xl6CgenNewSimpleType("null", expr);
    case XL6_AST_ARRAY_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "array", expr, expr);
    case XL6_AST_LIST_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "list", expr, expr);
    case XL6_AST_SET_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "set", expr, expr);
    case XL6_AST_DICT_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "dict", expr, expr);
    case XL6_AST_TYPED_LITERAL:
        if ( expr->text != NULL &&
             (xl6CTextEquals(expr->text, "array") || xl6CTextEquals(expr->text, "list") ||
              xl6CTextEquals(expr->text, "set") || xl6CTextEquals(expr->text, "dict")) ) {
            return xl6CgenCloneContainerLiteralType(
                ctx,
                expr->text,
                expr->childCount > 0 ? expr->children[0] : NULL,
                expr);
        }
        return xl6CgenNewSimpleType(expr->text != NULL ? expr->text : "var", expr);
    case XL6_AST_ANON_FUNC:
        return xl6CgenAnonFunctionType(expr);
    case XL6_AST_NAME:
        varType = xl6CgenFindVarType(ctx, expr->text);
        if ( varType != NULL && !xl6CTextEquals(varType->text, "auto") ) {
            return xl6CgenCloneTypeNode(varType);
        }
        if ( xl6CgenIsKnownTypeName(ctx, expr->text) ) {
            return xl6CgenNewSimpleType("type", expr);
        }
        return NULL;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "in") ) {
            return xl6CgenNewSimpleType("bool", expr);
        }
        if ( xl6CTextEquals(expr->text, "??") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            XL6_AstNode* result = NULL;

            if ( leftType != NULL && xl6CTextEquals(leftType->text, "null") && rightType != NULL ) {
                result = xl6CgenCloneTypeNode(rightType);
            } else if ( rightType != NULL && xl6CTextEquals(rightType->text, "null") && leftType != NULL ) {
                result = xl6CgenCloneTypeNode(leftType);
            } else if ( leftType != NULL && rightType != NULL && xl6CgenTypeNodeEqual(leftType, rightType) ) {
                result = xl6CgenCloneTypeNode(leftType);
            } else if ( (leftType != NULL && xl6CgenIsXValueType(leftType)) ||
                        (rightType != NULL && xl6CgenIsXValueType(rightType)) ) {
                result = xl6CgenNewSimpleType("var", expr);
            }
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return result;
        }
        if ( xl6CTextEquals(expr->text, "+") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            int isString = leftType != NULL && rightType != NULL &&
                           xl6CTextEquals(leftType->text, "string") &&
                           xl6CTextEquals(rightType->text, "string");
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            if ( isString ) {
                return xl6CgenNewSimpleType("string", expr);
            }
        }
        if ( xl6CTextEquals(expr->text, "**") && xl6CgenExprIntPowKind(ctx, expr) ) {
            return xl6CgenNewSimpleType("int", expr);
        }
        return NULL;
    case XL6_AST_MEMBER:
        {
            XL6_AstNode* fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
            if ( fieldType != NULL ) {
                return fieldType;
            }
        }
        return NULL;
    case XL6_AST_SAFE_MEMBER:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( expr->text != NULL && xl6CTextEquals(expr->text, "length") && xl6CgenIsXValueType(receiverType) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) ||
             xl6CgenIsDictXValueType(receiverType) ||
             (expr->kind == XL6_AST_SAFE_INDEX && receiverType != NULL && xl6CTextEquals(receiverType->text, "var")) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_CALL:
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* genericClassType = xl6CgenGenericClassCtorType(ctx, expr, NULL);
            if ( genericClassType != NULL ) {
                return genericClassType;
            }
            {
                XL6_AstNode* instant = expr->children[0];
                XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
                XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
                XL6_AstNode* func = NULL;

                if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                    func = xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
                }
                if ( func != NULL && func->childCount > 2 &&
                     func->children[2] != NULL && func->children[2]->childCount == 1 ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
                    XL6_AstNode* result;

                    /*
                        显式泛型调用已经给出完整类型实参，auto 推断时要用它们替换返回类型里的 T。
                        这里和普通泛型推断调用共用规范化类型克隆逻辑，确保嵌套类型也能继续访问成员。
                    */
                    ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                    ctx->currentGenericArgs = typeArgs;
                    result = xl6CgenCloneCanonicalTypeNode(ctx, func->children[2]->children[0]);
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                    return result;
                }
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
            if ( xl6CTextEquals(expr->children[0]->text, "type") ) {
                return xl6CgenNewSimpleType("type", expr);
            }
            if ( xl6CgenIsBuiltinConvertTypeName(expr->children[0]->text) &&
                 xl6CgenIsKnownTypeName(ctx, expr->children[0]->text) ) {
                return xl6CgenNewSimpleType(expr->children[0]->text, expr);
            }
            const XL6_AstNode* functionVarType = xl6CgenFindVarType(ctx, expr->children[0]->text);
            const XL6_AstNode* functionReturns = xl6CgenFunctionReturnTypes(functionVarType);
            if ( xl6CgenIsFunctionType(functionVarType) ) {
                if ( xl6CgenIsBareFunctionType(functionVarType) ) {
                    return xl6CgenNewSimpleType("var", expr);
                }
                if ( functionReturns != NULL && functionReturns->childCount == 1 ) {
                    return xl6CgenCloneTypeNode(functionReturns->children[0]);
                }
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
            XL6_AstNode* func = xl6CgenFindExternFunc(ctx, expr->children[0]->text);
            XL6_AstNode* inferredArgs = NULL;
            const XL6_AstNode* returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            if ( returns == NULL ) {
                func = xl6CgenResolveCallFuncInternal(ctx, expr, expr->children[0]->text, &inferredArgs, 0);
                returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            }
            if ( returns != NULL && returns->childCount == 1 ) {
                XL6_AstNode* result;

                if ( inferredArgs != NULL ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                    ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                    ctx->currentGenericArgs = inferredArgs;
                    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                    xl6AstFree(inferredArgs);
                    return result;
                }
                result = xl6CgenCloneTypeNode(returns->children[0]);
                xl6AstFree(inferredArgs);
                return result;
            }
            xl6AstFree(inferredArgs);
        }
        return NULL;
    default:
        return NULL;
    }
}

static int xl6CgenExprStaticTypeIs(XL6_CgenContext* ctx, const XL6_AstNode* expr, const char* typeName)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    result = concreteType != NULL && concreteType->text != NULL && xl6CTextEquals(concreteType->text, typeName);
    xl6AstFree(typeNode);
    return result;
}

static int xl6CgenBinaryPlusIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || !xl6CTextEquals(expr->text, "+") || expr->childCount < 2 ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryOpIsOrderedCompare(const char* op)
{
    return xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=") ||
           xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
           xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=");
}

static int xl6CgenBinaryCompareIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CgenBinaryOpIsOrderedCompare(expr->text) ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryCompareIsTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         (!xl6CTextEquals(expr->text, "==") && !xl6CTextEquals(expr->text, "!=")) ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "type") ||
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "type");
}

static void xl6CgenTypeCompareOperand(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_NULL ) {
        xl6CgenEmitTypeIdForName(ctx, "null", expr);
        return;
    }
    xl6CgenExpr(ctx, expr);
}

static void xl6CgenTypeCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    fputc('(', ctx->out);
    xl6CgenTypeCompareOperand(ctx, expr->childCount > 0 ? expr->children[0] : NULL);
    fprintf(ctx->out, " %s ", expr->text != NULL ? expr->text : "==");
    xl6CgenTypeCompareOperand(ctx, expr->childCount > 1 ? expr->children[1] : NULL);
    fputc(')', ctx->out);
}

static int xl6CgenBinaryApproxIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CTextEquals(expr->text, "~=") ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryInIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CTextEquals(expr->text, "in") ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenTypeParamIndex(const XL6_AstNode* typeParams, const char* name)
{
    size_t i;

    if ( typeParams == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        const XL6_AstNode* param = typeParams->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return (int)i;
        }
    }
    return -1;
}

static int xl6CgenTypeContainsTypeParam(const XL6_AstNode* typeParams, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( xl6CgenTypeParamIndex(typeParams, typeNode->text) >= 0 && typeNode->childCount == 0 ) {
        return 1;
    }
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( xl6CgenTypeContainsTypeParam(typeParams, typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenInferenceTypeEqual(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int equal;

    leftType = xl6CgenCloneCanonicalTypeNode(ctx, left);
    rightType = xl6CgenCloneCanonicalTypeNode(ctx, right);
    equal = leftType != NULL && rightType != NULL && xl6CgenTypeNodeEqual(leftType, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return equal;
}

static int xl6CgenRememberInferredType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferred,
    int index,
    const XL6_AstNode* argType,
    int report,
    const XL6_AstNode* at)
{
    XL6_AstNode* canonicalType;

    (void)typeParams;
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, argType);
    if ( canonicalType == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "无法从实参推断泛型类型");
        }
        return 0;
    }

    if ( inferred[index] == NULL ) {
        inferred[index] = canonicalType;
        return 1;
    }

    if ( xl6CgenTypeNodeEqual(inferred[index], canonicalType) ) {
        xl6AstFree(canonicalType);
        return 1;
    }

    xl6AstFree(canonicalType);
    if ( report ) {
        xl6CgenError(ctx, at, "同一泛型参数推断出不同类型");
    }
    return 0;
}

static int xl6CgenInferGenericTypeFromTypes(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferred,
    const XL6_AstNode* paramType,
    const XL6_AstNode* argType,
    int report,
    const XL6_AstNode* at)
{
    const XL6_AstNode* paramResolved;
    const XL6_AstNode* argResolved;
    int typeParamIndex;
    size_t i;

    if ( paramType == NULL || !xl6CgenTypeContainsTypeParam(typeParams, paramType) ) {
        return 1;
    }
    if ( argType == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "无法从实参推断泛型类型");
        }
        return 0;
    }

    paramResolved = xl6CgenResolveAliasType(ctx, paramType);
    argResolved = xl6CgenResolveAliasType(ctx, argType);
    if ( paramResolved == NULL || argResolved == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "无法从实参推断泛型类型");
        }
        return 0;
    }
    typeParamIndex = xl6CgenTypeParamIndex(typeParams, paramResolved != NULL ? paramResolved->text : NULL);

    if ( typeParamIndex >= 0 && paramResolved->childCount == 0 ) {
        return xl6CgenRememberInferredType(ctx, typeParams, inferred, typeParamIndex, argResolved, report, at);
    }

    if ( paramResolved->kind != argResolved->kind ||
         paramResolved->childCount != argResolved->childCount ||
         ((paramResolved->text == NULL) != (argResolved->text == NULL)) ||
         (paramResolved->text != NULL && !xl6CTextEquals(paramResolved->text, argResolved->text)) ) {
        if ( report ) {
            xl6CgenError(ctx, at, "无法从实参推断泛型类型");
        }
        return 0;
    }

    for ( i = 0; i < paramResolved->childCount; ++i ) {
        const XL6_AstNode* paramChild = paramResolved->children[i];
        const XL6_AstNode* argChild = argResolved->children[i];

        if ( xl6CgenTypeContainsTypeParam(typeParams, paramChild) ) {
            if ( !xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferred, paramChild, argChild, report, at) ) {
                return 0;
            }
        } else if ( !xl6CgenInferenceTypeEqual(ctx, paramChild, argChild) ) {
            if ( report ) {
                xl6CgenError(ctx, at, "泛型实参类型结构不匹配");
            }
            return 0;
        }
    }

    return 1;
}

static XL6_AstNode* xl6CgenInferGenericTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* func, int report)
{
    const XL6_AstNode* typeParams;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    XL6_AstNode** inferred;
    XL6_AstNode* result;
    size_t i;
    int ok;

    typeParams = xl6CgenFuncTypeParams(func);
    if ( typeParams == NULL || typeParams->childCount == 0 ) {
        return NULL;
    }

    inferred = (XL6_AstNode**)calloc(typeParams->childCount, sizeof(XL6_AstNode*));
    if ( inferred == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, call, "泛型类型推断内存不足");
        }
        return NULL;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    ok = 1;

    if ( params != NULL ) {
        int fixedCount = xl6CgenFixedParamCount(params);
        for ( i = 0; i < (size_t)fixedCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
            const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param != NULL ? param->text : NULL, (int)i);

            if ( expr == NULL || paramType == NULL || !xl6CgenTypeContainsTypeParam(typeParams, paramType) ) {
                continue;
            }

            {
                XL6_AstNode* exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
                if ( exprType == NULL ) {
                    ok = 0;
                    if ( report ) {
                        xl6CgenError(ctx, expr, "无法从实参推断泛型类型");
                    }
                    break;
                }
                if ( !xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferred, paramType, exprType, report, expr) ) {
                    xl6AstFree(exprType);
                    ok = 0;
                    break;
                }
                xl6AstFree(exprType);
            }
        }
    }

    for ( i = 0; ok && i < typeParams->childCount; ++i ) {
        if ( inferred[i] == NULL ) {
            ok = 0;
            if ( report ) {
                xl6CgenError(ctx, call, "泛型类型参数无法全部推断");
            }
        }
    }

    if ( !ok ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferred[i]);
        }
        free(inferred);
        return NULL;
    }

    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferred[i]);
        }
        free(inferred);
        if ( report ) {
            xl6CgenError(ctx, call, "泛型类型推断内存不足");
        }
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        xl6AstAddChild(result, inferred[i]);
        inferred[i] = NULL;
    }
    free(inferred);
    return result;
}

static XL6_AstNode* xl6CgenFindInferredGenericFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs, int report)
{
    XL6_AstNode* selected;
    XL6_AstNode* selectedArgs;
    size_t i;
    int candidateCount;
    int matchCount;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    selected = NULL;
    selectedArgs = NULL;
    candidateCount = 0;
    matchCount = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* typeArgs;
        if ( node == NULL || node->kind != XL6_AST_FUNC || !xl6CgenFuncIsGeneric(node) ||
             node->childCount == 0 || node->children[0] == NULL ||
             !xl6CTextEquals(node->children[0]->text, name) ) {
            continue;
        }
        candidateCount += 1;
        typeArgs = xl6CgenInferGenericTypeArgs(ctx, call, node, 0);
        if ( typeArgs != NULL ) {
            matchCount += 1;
            if ( selectedArgs != NULL ) {
                xl6AstFree(selectedArgs);
            }
            selected = node;
            selectedArgs = typeArgs;
        }
    }

    if ( matchCount == 1 ) {
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = selectedArgs;
        } else {
            xl6AstFree(selectedArgs);
        }
        return selected;
    }

    if ( selectedArgs != NULL ) {
        xl6AstFree(selectedArgs);
    }
    if ( matchCount > 1 && report ) {
        xl6CgenError(ctx, call, "泛型函数推断存在重载歧义");
    } else if ( candidateCount > 0 && report ) {
        for ( i = 0; i < ctx->root->childCount; ++i ) {
            XL6_AstNode* node = ctx->root->children[i];
            if ( node != NULL && node->kind == XL6_AST_FUNC && xl6CgenFuncIsGeneric(node) &&
                 node->childCount > 0 && node->children[0] != NULL &&
                 xl6CTextEquals(node->children[0]->text, name) ) {
                (void)xl6CgenInferGenericTypeArgs(ctx, call, node, 1);
                break;
            }
        }
    }
    return NULL;
}

static void xl6CgenCollectGenericCall(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* func;

    if ( call == NULL || call->childCount == 0 ) {
        return;
    }
    callee = call->children[0];
    if ( callee == NULL ) {
        return;
    }
    if ( callee->kind == XL6_AST_NAME ) {
        XL6_AstNode* inferredArgs = NULL;
        func = xl6CgenResolveCallFuncInternal(ctx, call, callee->text, &inferredArgs, 0);
        if ( func != NULL && inferredArgs != NULL ) {
            xl6CgenAddOwnedGenericInstance(ctx, func, inferredArgs);
        } else {
            xl6AstFree(inferredArgs);
        }
        return;
    }
    if ( callee->kind != XL6_AST_GENERIC_INSTANT || callee->childCount < 2 ) {
        return;
    }

    nameNode = callee->children[0];
    typeArgs = callee->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ) {
        xl6CgenError(ctx, callee, "当前 C 生成只支持函数名形式的泛型实例化");
        return;
    }

    if ( xl6CgenFindClass(ctx, nameNode->text) != NULL ) {
        return;
    }
    func = xl6CgenResolveExplicitGenericCallFunc(ctx, call, nameNode->text, typeArgs, 0);
    if ( func == NULL ) {
        xl6CgenError(ctx, callee, "找不到匹配的泛型函数声明");
        return;
    }
    xl6CgenAddGenericInstance(ctx, func, typeArgs);
}

static void xl6CgenCollectGenericInstances(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }

    /* 泛型模板本身不直接扫描，只有被实例化后才进入代码生成。 */
    if ( node->kind == XL6_AST_FUNC && xl6CgenFuncIsGeneric(node) ) {
        return;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;
        XL6_AstNode* inferredType;

        if ( initExpr != NULL ) {
            xl6CgenCollectGenericInstances(ctx, initExpr);
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenAddVarType(ctx, node->text, inferredType);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenAddVarType(ctx, node->text, typeNode);
        }
        return;
    }
    if ( node->kind == XL6_AST_CALL ) {
        xl6CgenCollectGenericCall(ctx, node);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectGenericInstances(ctx, node->children[i]);
    }
}

static void xl6CgenCollectGenericInstancesToFixedPoint(XL6_CgenContext* ctx)
{
    size_t scanned;

    if ( ctx == NULL ) {
        return;
    }
    xl6CgenCollectGenericInstances(ctx, ctx->root);
    scanned = 0;
    while ( scanned < ctx->genericInstanceCount ) {
        const XL6_CgenGenericInstance* instance = &ctx->genericInstances[scanned];
        const XL6_AstNode* body = instance->func != NULL && instance->func->childCount > 3 ? instance->func->children[3] : NULL;
        xl6CgenCollectGenericInstances(ctx, body);
        scanned += 1;
    }
}

static const char* xl6CgenInferExprType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    static char customTypeName[256];
    XL6_AstNode* customNode;
    XL6_AstNode* classNode;
    const XL6_AstNode* varType;

    if ( expr == NULL ) {
        return "int";
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return "long long";
    case XL6_AST_FLOAT:
        return "double";
    case XL6_AST_STRING:
        return "const char*";
    case XL6_AST_TIME:
        return "long long";
    case XL6_AST_BOOL:
        return "int";
    case XL6_AST_ARRAY_LITERAL:
    case XL6_AST_LIST_LITERAL:
    case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:
    case XL6_AST_TYPED_LITERAL: {
        XL6_AstNode* typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
        const char* cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "xvalue";
        xl6AstFree(typeNode);
        return cType;
    }
    case XL6_AST_ANON_FUNC:
        return "XL6_Function";
    case XL6_AST_NAME:
        varType = xl6CgenFindVarType(ctx, expr->text);
        if ( varType != NULL && !xl6CTextEquals(varType->text, "auto") ) {
            return xl6CgenTypeName(ctx, varType);
        }
        if ( xl6CgenIsKnownTypeName(ctx, expr->text) ) {
            return "unsigned long long";
        }
        break;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "in") ) {
            return "int";
        }
        if ( xl6CTextEquals(expr->text, "??") ) {
            XL6_AstNode* typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
            const char* cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "int";
            xl6AstFree(typeNode);
            return cType;
        }
        if ( xl6CgenBinaryPlusIsString(ctx, expr) ) {
            return "const char*";
        }
        if ( xl6CTextEquals(expr->text, "**") && xl6CgenExprIntPowKind(ctx, expr) ) {
            return "long long";
        }
        break;
    case XL6_AST_MEMBER:
        if ( expr->text != NULL && xl6CTextEquals(expr->text, "length") &&
             expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_NAME &&
             xl6CgenIsXValueType(xl6CgenFindVarType(ctx, expr->children[0]->text)) ) {
            return "long long";
        }
        {
            XL6_AstNode* typeNode = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
            const char* cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : NULL;
            xl6AstFree(typeNode);
            if ( cType != NULL ) {
                return cType;
            }
        }
        break;
    case XL6_AST_SAFE_MEMBER: {
        XL6_AstNode* typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
        const char* cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "xvalue";
        xl6AstFree(typeNode);
        return cType;
    }
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX: {
        XL6_AstNode* typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
        const char* cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "xvalue";
        xl6AstFree(typeNode);
        return cType;
    }
    case XL6_AST_CALL:
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, "type") ) {
            return "unsigned long long";
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_NAME &&
             xl6CgenIsBuiltinConvertTypeName(expr->children[0]->text) &&
             xl6CgenIsKnownTypeName(ctx, expr->children[0]->text) ) {
            return xl6CgenConvertCTypeName(expr->children[0]->text);
        }
        if ( xl6CgenCallIsCustomCtor(ctx, expr, &customNode) ) {
            xl6CgenCustomTypeRefName(customTypeName, sizeof(customTypeName), customNode->text);
            return customTypeName;
        }
        if ( xl6CgenCallIsClassCtor(ctx, expr, &classNode) ) {
            xl6CgenClassTypeTextName(customTypeName, sizeof(customTypeName), classNode->text);
            return customTypeName;
        }
        {
            XL6_AstNode* genericClassType = xl6CgenGenericClassCtorType(ctx, expr, &classNode);
            if ( genericClassType != NULL ) {
                const char* cType = xl6CgenTypeName(ctx, genericClassType);
                xl6AstFree(genericClassType);
                return cType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            const XL6_AstNode* instant = expr->children[0];
            const XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
            const XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
            XL6_AstNode* func = (nameNode != NULL && nameNode->kind == XL6_AST_NAME) ?
                                xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0) : NULL;
            if ( func != NULL && func->childCount > 2 && func->children[2]->childCount == 1 ) {
                const XL6_AstNode* prevParams = ctx->currentGenericParams;
                const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
                const char* cType;

                ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                ctx->currentGenericArgs = typeArgs;
                cType = xl6CgenTypeName(ctx, func->children[2]->children[0]);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
                return cType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0]->kind == XL6_AST_NAME ) {
            const XL6_AstNode* functionVarType = xl6CgenFindVarType(ctx, expr->children[0]->text);
            const XL6_AstNode* functionReturns = xl6CgenFunctionReturnTypes(functionVarType);
            if ( xl6CgenIsFunctionType(functionVarType) ) {
                if ( xl6CgenIsBareFunctionType(functionVarType) ) {
                    return "xvalue";
                }
                if ( functionReturns != NULL && functionReturns->childCount == 1 ) {
                    return xl6CgenTypeName(ctx, functionReturns->children[0]);
                }
            }
        }
        if ( expr->childCount > 0 && expr->children[0]->kind == XL6_AST_NAME ) {
            XL6_AstNode* func = xl6CgenFindExternFunc(ctx, expr->children[0]->text);
            XL6_AstNode* inferredArgs = NULL;
            if ( func == NULL ) {
                func = xl6CgenResolveCallFuncInternal(ctx, expr, expr->children[0]->text, &inferredArgs, 0);
            }
            if ( func != NULL && func->childCount > 2 && func->children[2]->childCount == 1 ) {
                if ( inferredArgs != NULL ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
                    const char* cType;

                    ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                    ctx->currentGenericArgs = inferredArgs;
                    cType = xl6CgenTypeName(ctx, func->children[2]->children[0]);
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                    xl6CgenAddOwnedGenericInstance(ctx, func, inferredArgs);
                    return cType;
                }
                xl6AstFree(inferredArgs);
                return xl6CgenTypeName(ctx, func->children[2]->children[0]);
            }
            xl6AstFree(inferredArgs);
        } else if ( expr->childCount > 0 && expr->children[0]->kind == XL6_AST_MEMBER ) {
            const XL6_AstNode* member = expr->children[0];
            const XL6_AstNode* receiver = member->childCount > 0 ? member->children[0] : NULL;
            const XL6_AstNode* receiverType = (receiver != NULL && receiver->kind == XL6_AST_NAME) ? xl6CgenFindVarType(ctx, receiver->text) : NULL;
            const XL6_AstNode* customReceiverType = receiver != NULL && receiver->kind == XL6_AST_MEMBER ? xl6CgenMemberFieldType(ctx, receiver) : receiverType;
            const XL6_AstNode* customDecl = customReceiverType != NULL ? xl6CgenFindCustom(ctx, customReceiverType->text) : NULL;
            const XL6_AstNode* method = NULL;
            const XL6_AstNode* returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
            if ( customDecl != NULL ) {
                const XL6_AstNode* helperMethod = xl6CgenResolveCustomFunc(ctx, customDecl, expr, member->text, 0);
                const XL6_AstNode* externMethod = xl6CgenResolveCustomExternFuncWithReport(ctx, customDecl, expr, member->text, 0);
                if ( helperMethod != NULL && externMethod != NULL ) {
                    xl6CgenError(ctx, expr, "custom 方法调用存在 helper / extern 歧义");
                    return "int";
                }
                method = helperMethod != NULL ? helperMethod : externMethod;
                returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
            }
            if ( returns != NULL && returns->childCount == 1 ) {
                return xl6CgenTypeName(ctx, returns->children[0]);
            }
            if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
                const XL6_AstNode* classDecl;
                if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
                    classDecl = ctx->currentClass;
                } else {
                    classDecl = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
                }
                if ( classDecl != NULL ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classDecl) ) {
                        ctx->currentGenericParams = xl6CgenClassTypeParams(classDecl);
                        ctx->currentGenericArgs = receiverType->children[0];
                    }
                    method = xl6CgenResolveClassFunc(ctx, classDecl, expr, member->text, NULL);
                    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
                    if ( returns != NULL && returns->childCount == 1 ) {
                        const char* cType = xl6CgenTypeName(ctx, returns->children[0]);
                        ctx->currentGenericParams = prevParams;
                        ctx->currentGenericArgs = prevArgs;
                        return cType;
                    }
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                }
            }
        }
        break;
    default:
        break;
    }

    xl6CgenError(ctx, expr, "无法为 auto 推断 C 类型");
    return "int";
}

static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static void xl6CgenCStringSymbol(FILE* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
        fputs("_", out);
        return;
    }

    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        for ( i = 1; i + 1 < len; ++i ) {
            fputc(text[i], out);
        }
        return;
    }
    fputs(text, out);
}

static int xl6CgenCustomHasDestroy(const XL6_AstNode* customNode)
{
    return xl6CgenFindCustomSpecial(customNode, "destroy") != NULL;
}

static int xl6CgenClassHasDestroy(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "destroy") != NULL;
}

static int xl6CgenIsFunctionType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CTextEquals(typeNode->text, "function");
}

static int xl6CgenIsBareFunctionType(const XL6_AstNode* typeNode)
{
    return xl6CgenIsFunctionType(typeNode) && typeNode->childCount == 0;
}

static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>"));
}

static int xl6CgenIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CgenIsArrayXValueType(typeNode) ||
            xl6CgenIsListXValueType(typeNode) ||
            xl6CgenIsSetXValueType(typeNode) ||
            xl6CgenIsDictXValueType(typeNode));
}

static const char* xl6CgenXValueRuntimeTypeConst(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "bool") ) {
        return "XL6_XVO_DT_BOOL";
    }
    if ( xl6CgenIsIntegerTypeName(typeName) ) {
        return "XL6_XVO_DT_INT";
    }
    if ( xl6CgenIsFloatTypeName(typeName) ) {
        return "XL6_XVO_DT_FLOAT";
    }
    if ( xl6CTextEquals(typeName, "string") ) {
        return "XL6_XVO_DT_TEXT";
    }
    if ( xl6CTextEquals(typeName, "time") ) {
        return "XL6_XVO_DT_TIME";
    }
    if ( xl6CTextEquals(typeName, "point") ) {
        return "XL6_XVO_DT_POINT";
    }
    if ( xl6CgenIsArrayXValueType(concreteType) ) {
        return "XL6_XVO_DT_ARRAY";
    }
    if ( xl6CgenIsListXValueType(concreteType) ) {
        return "XL6_XVO_DT_LIST";
    }
    if ( xl6CgenIsSetXValueType(concreteType) ) {
        return "XL6_XVO_DT_COLL";
    }
    if ( xl6CgenIsDictXValueType(concreteType) ) {
        return "XL6_XVO_DT_TABLE";
    }
    return NULL;
}

static int xl6CgenTypeSupportsVarInvokeValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(concreteType->text, "var") ) {
        return 1;
    }
    return xl6CgenXValueRuntimeTypeConst(ctx, concreteType) != NULL;
}

static int xl6CgenAnonSupportsVarInvoke(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( ctx == NULL || info == NULL || info->node == NULL ) {
        return 0;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ||
                 !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static const XL6_AstNode* xl6CgenFindDirectClassFieldNode(const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || fieldName == NULL ) {
        return NULL;
    }
    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && xl6CTextEquals(member->text, fieldName) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassFieldNode(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* field;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    field = xl6CgenFindDirectClassFieldNode(classNode, fieldName);
    if ( field != NULL ) {
        return field;
    }

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    return xl6CgenFindClassFieldNode(ctx, baseClass, fieldName);
}

static XL6_AstNode* xl6CgenCloneMemberFieldTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* field;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* result;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return NULL;
    }

    receiverType = NULL;
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    if ( field == NULL || field->childCount == 0 ) {
        return NULL;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    result = xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static int xl6CgenClassNeedsDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDestroy(classNode) ) {
        return 1;
    }

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            const XL6_AstNode* typeNode;
            XL6_AstNode* customNode;
            XL6_AstNode* nestedClass;

            if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
                continue;
            }
            typeNode = member->children[0];
            customNode = xl6CgenFindCustom(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( customNode != NULL && xl6CgenCustomHasDestroy(customNode) ) {
                return 1;
            }
            nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
                return 1;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAddCleanupEntry(XL6_CgenContext* ctx, const char* name, XL6_CgenCleanupKind kind, const XL6_AstNode* typeNode, const XL6_AstNode* classTypeNode)
{
    XL6_CgenCleanup* newItems;
    size_t newCapacity;
    char cName[512];

    if ( ctx == NULL || name == NULL || typeNode == NULL ) {
        return;
    }
    if ( ctx->cleanupCount == ctx->cleanupCapacity ) {
        newCapacity = ctx->cleanupCapacity == 0 ? 16 : ctx->cleanupCapacity * 2;
        newItems = (XL6_CgenCleanup*)realloc(ctx->cleanups, newCapacity * sizeof(XL6_CgenCleanup));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "C 生成清理表内存不足");
            return;
        }
        ctx->cleanups = newItems;
        ctx->cleanupCapacity = newCapacity;
    }
    xl6CgenCIdentText(cName, sizeof(cName), name);
    ctx->cleanups[ctx->cleanupCount].name = xl6StrDupRange(cName, strlen(cName));
    ctx->cleanups[ctx->cleanupCount].kind = kind;
    ctx->cleanups[ctx->cleanupCount].typeNode = typeNode;
    ctx->cleanups[ctx->cleanupCount].classTypeNode = classTypeNode;
    ctx->cleanupCount += 1;
}

static void xl6CgenAddCustomCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* customNode)
{
    if ( customNode == NULL || !xl6CgenCustomHasDestroy(customNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CUSTOM, customNode, NULL);
}

static void xl6CgenAddClassCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* classNode, const XL6_AstNode* classTypeNode)
{
    if ( classNode == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CLASS, classNode, classTypeNode);
}

static void xl6CgenAddFunctionCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsFunctionType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_FUNCTION, typeNode, NULL);
}

static void xl6CgenAddXValueCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsXValueType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_XVALUE, typeNode, NULL);
}

static void xl6CgenFreeCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = mark; i < ctx->cleanupCount; ++i ) {
        xl6Free(ctx->cleanups[i].name);
    }
    ctx->cleanupCount = mark;
}

static void xl6CgenFreeCleanups(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6Free(ctx->cleanups);
    ctx->cleanups = NULL;
    ctx->cleanupCapacity = 0;
}

static void xl6CgenEmitOneCleanup(XL6_CgenContext* ctx, const XL6_CgenCleanup* cleanup)
{
    const XL6_AstNode* destroyNode;

    if ( ctx == NULL || cleanup == NULL || cleanup->name == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_AVLTB_ITER ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xrtAVLTB_IterEnd(%s);\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        return;
    }
    if ( cleanup->typeNode == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_CUSTOM ) {
        destroyNode = xl6CgenFindCustomSpecial(cleanup->typeNode, "destroy");
        if ( destroyNode == NULL ) {
            return;
        }

        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (%s.handle != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenCStringSymbol(ctx->out, destroyNode->text);
        fprintf(ctx->out, "(%s.handle);\n", cleanup->name);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s.handle = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_FUNCTION ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cleanup->name, cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s.release(%s.env);\n", cleanup->name, cleanup->name);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s.env = 0;\n", cleanup->name);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s.release = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_XVALUE ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvoUnref(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        return;
    }

    if ( !xl6CgenClassNeedsDestroy(ctx, cleanup->typeNode) ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( cleanup->classTypeNode != NULL && cleanup->classTypeNode->childCount > 0 ) {
        xl6CgenGenericClassMemberSymbol(ctx, cleanup->typeNode, cleanup->classTypeNode, "destroy");
    } else {
    xl6CgenClassMemberSymbol(ctx->out, cleanup->typeNode->text, "destroy");
    }
    fprintf(ctx->out, "(&%s);\n", cleanup->name);
}

static void xl6CgenRetainFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenCIdentText(cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s.retain != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "%s.retain(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
}

static void xl6CgenReleaseFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenCIdentText(cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "%s.release(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
}

static int xl6CgenExprIsFunctionBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsFunctionType(typeNode);
}

static int xl6CgenExprIsXValueBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_INDEX || expr->kind == XL6_AST_SAFE_INDEX ) {
        return 1;
    }
    if ( expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsXValueType(typeNode);
}

static void xl6CgenEmitCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = ctx->cleanupCount; i > mark; --i ) {
        xl6CgenEmitOneCleanup(ctx, &ctx->cleanups[i - 1]);
    }
}

static void xl6CgenCloseScope(XL6_CgenContext* ctx, size_t mark)
{
    xl6CgenEmitCleanupsSince(ctx, mark);
    xl6CgenFreeCleanupsSince(ctx, mark);
}

static void xl6CgenPushLoopTarget(XL6_CgenContext* ctx, size_t breakMark, size_t continueMark, int breakId, int continueId)
{
    XL6_CgenLoopTarget* newItems;
    size_t newCapacity;

    if ( ctx == NULL ) {
        return;
    }
    if ( ctx->loopTargetCount == ctx->loopTargetCapacity ) {
        newCapacity = ctx->loopTargetCapacity == 0 ? 8 : ctx->loopTargetCapacity * 2;
        newItems = (XL6_CgenLoopTarget*)realloc(ctx->loopTargets, newCapacity * sizeof(XL6_CgenLoopTarget));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成循环清理表内存不足");
            return;
        }
        ctx->loopTargets = newItems;
        ctx->loopTargetCapacity = newCapacity;
    }
    ctx->loopTargets[ctx->loopTargetCount].breakCleanupMark = breakMark;
    ctx->loopTargets[ctx->loopTargetCount].continueCleanupMark = continueMark;
    ctx->loopTargets[ctx->loopTargetCount].breakId = breakId;
    ctx->loopTargets[ctx->loopTargetCount].continueId = continueId;
    ctx->loopTargetCount += 1;
}

static void xl6CgenPopLoopTarget(XL6_CgenContext* ctx)
{
    if ( ctx != NULL && ctx->loopTargetCount > 0 ) {
        ctx->loopTargetCount -= 1;
    }
}

static XL6_CgenLoopTarget* xl6CgenLoopTargetAtLevel(XL6_CgenContext* ctx, int level)
{
    size_t index;

    if ( ctx == NULL || level <= 0 || (size_t)level > ctx->loopTargetCount ) {
        return NULL;
    }
    index = ctx->loopTargetCount - (size_t)level;
    return &ctx->loopTargets[index];
}

static void xl6CgenFreeLoopTargets(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xl6Free(ctx->loopTargets);
    ctx->loopTargets = NULL;
    ctx->loopTargetCount = 0;
    ctx->loopTargetCapacity = 0;
}

static int xl6CgenDigitValue(char ch)
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

static void xl6CgenIntLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* p;
    unsigned long long value;
    int base;

    if ( node == NULL || node->text == NULL ) {
        fputs("0", ctx->out);
        return;
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
        digit = xl6CgenDigitValue(*p);
        if ( digit < 0 || digit >= base ||
             value > (ULLONG_MAX - (unsigned long long)digit) / (unsigned long long)base ) {
            xl6CgenError(ctx, node, "整数字面量超出当前 C 生成可表示范围");
            fputs("0", ctx->out);
            return;
        }
        value = value * (unsigned long long)base + (unsigned long long)digit;
        ++p;
    }

    fprintf(ctx->out, "%lluLL", value);
}

static void xl6CgenFloatLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* p;

    (void)ctx;
    if ( node == NULL || node->text == NULL ) {
        fputs("0.0", ctx->out);
        return;
    }
    for ( p = node->text; *p != '\0'; ++p ) {
        if ( *p != '_' ) {
            fputc((int)*p, ctx->out);
        }
    }
}

static int xl6CgenIntLiteralLevel(const XL6_AstNode* node)
{
    const char* p;
    int base;
    int value;

    if ( node == NULL || node->kind != XL6_AST_INT || node->text == NULL ) {
        return 1;
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
        digit = xl6CgenDigitValue(*p);
        if ( digit < 0 || digit >= base || value > (INT_MAX - digit) / base ) {
            return INT_MAX;
        }
        value = value * base + digit;
        ++p;
    }
    return value;
}

static void xl6CgenLoopJump(XL6_CgenContext* ctx, const XL6_AstNode* node, int isContinue)
{
    int level;
    XL6_CgenLoopTarget* target;

    level = node != NULL && node->childCount > 0 ? xl6CgenIntLiteralLevel(node->children[0]) : 1;
    target = xl6CgenLoopTargetAtLevel(ctx, level);
    if ( target == NULL ) {
        xl6CgenError(ctx, node, isContinue ? "continue 层数超过当前循环深度" : "break 层数超过当前循环深度");
        return;
    }

    xl6CgenEmitCleanupsSince(ctx, isContinue ? target->continueCleanupMark : target->breakCleanupMark);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "goto __xl6_%s_%d;\n", isContinue ? "continue" : "break", isContinue ? target->continueId : target->breakId);
}

static void xl6CgenExportInternalSymbol(FILE* out, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    fputs("xl6_export_", out);
    xl6CgenFputsEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
}

static XL6_AstNode* xl6CgenFindExternFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_EXTERN_FUNC && node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenFindExportFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_EXPORT_FUNC && node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static void xl6CgenCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* args)
{
    size_t i;

    if ( args == NULL ) {
        return;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( i > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            xl6CgenError(ctx, arg, "当前 C 生成暂不支持命名参数调用");
            fputs("0", ctx->out);
        } else if ( arg->childCount > 0 ) {
            xl6CgenExpr(ctx, arg->children[0]);
        }
    }
}

static const XL6_AstNode* xl6CgenFunctionParamTypes(const XL6_AstNode* functionType)
{
    return functionType != NULL && functionType->childCount > 0 ? functionType->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFunctionReturnTypes(const XL6_AstNode* functionType)
{
    return functionType != NULL && functionType->childCount > 1 ? functionType->children[1] : NULL;
}

static int xl6CgenFunctionTypeParamIsArrayVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "array<var>");
}

static int xl6CgenFunctionTypeParamIsDictVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "dict<var>");
}

static size_t xl6CgenFunctionTypeFixedParamCount(const XL6_AstNode* paramTypes)
{
    size_t count;
    const XL6_AstNode* last;

    count = paramTypes != NULL ? paramTypes->childCount : 0;
    if ( count == 0 ) {
        return 0;
    }
    last = paramTypes->children[count - 1];
    if ( xl6CgenFunctionTypeParamIsArrayVararg(last) ||
         xl6CgenFunctionTypeParamIsDictVararg(last) ) {
        return count - 1;
    }
    return count;
}

static size_t xl6CgenFunctionValuePositionalArgCount(const XL6_AstNode* args)
{
    size_t i;
    size_t count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind != XL6_AST_NAMED_ARG ) {
            count += 1;
        }
    }
    return count;
}

static const XL6_AstNode* xl6CgenFunctionValuePositionalArg(const XL6_AstNode* args, size_t index)
{
    size_t i;
    size_t seen;

    seen = 0;
    if ( args == NULL ) {
        return NULL;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            continue;
        }
        if ( seen == index ) {
            return arg;
        }
        seen += 1;
    }
    return NULL;
}

static int xl6CgenFunctionValueDictVarargCount(const XL6_AstNode* args)
{
    size_t i;
    int count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenFunctionValueDictVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args)
{
    size_t i;
    int count;

    count = xl6CgenFunctionValueDictVarargCount(args);
    fprintf(ctx->out, "xl6_xrt_dict_varargs(%d", count);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind != XL6_AST_NAMED_ARG ) {
                continue;
            }
            fputs(", ", ctx->out);
            xl6CgenCStringLiteral(ctx->out, arg->text);
            fputs(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
        }
    }
    fputc(')', ctx->out);
}

static const XL6_AstNode* xl6CgenCallableItemType(const XL6_AstNode* item)
{
    if ( item != NULL && item->kind == XL6_AST_PARAM && item->childCount > 0 ) {
        return item->children[0];
    }
    return item;
}

static int xl6CgenCallableTypeListEqual(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    size_t i;

    if ( left == NULL || right == NULL ) {
        return left == right;
    }
    if ( left->childCount != right->childCount ) {
        return 0;
    }
    for ( i = 0; i < left->childCount; ++i ) {
        const XL6_AstNode* leftType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(left->children[i]));
        const XL6_AstNode* rightType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(right->children[i]));
        if ( !xl6CgenTypeNodeEqual(leftType, rightType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenCallableListLooksLikeParams(const XL6_AstNode* list)
{
    return list != NULL && list->childCount > 0 &&
           list->children[0] != NULL && list->children[0]->kind == XL6_AST_PARAM;
}

static int xl6CgenCallableParamPrefixMatches(XL6_CgenContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params, size_t count)
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
        const XL6_AstNode* expectedType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(expected->children[i]));
        const XL6_AstNode* actualType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(params->children[i]));
        if ( !xl6CgenTypeNodeEqual(expectedType, actualType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenCallableParamListMatchesFunctionParams(XL6_CgenContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params)
{
    size_t expectedCount;
    size_t expectedFixed;
    int expectedHasArrayVararg;
    int expectedHasDictVararg;
    int actualHasVararg;
    int actualFixed;
    int actualRequired;

    if ( !xl6CgenCallableListLooksLikeParams(params) ) {
        return xl6CgenCallableTypeListEqual(ctx, expected, params);
    }

    expectedCount = expected != NULL ? expected->childCount : 0;
    expectedHasArrayVararg = expectedCount > 0 && xl6CgenFunctionTypeParamIsArrayVararg(expected->children[expectedCount - 1]);
    expectedHasDictVararg = expectedCount > 0 && xl6CgenFunctionTypeParamIsDictVararg(expected->children[expectedCount - 1]);
    expectedFixed = (expectedHasArrayVararg || expectedHasDictVararg) ? expectedCount - 1 : expectedCount;
    actualFixed = xl6CgenFixedParamCount(params);
    actualRequired = xl6CgenParamRequiredCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)actualFixed;

    if ( expectedHasArrayVararg || expectedHasDictVararg ) {
        const XL6_AstNode* actualVararg;

        if ( !actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
            return 0;
        }
        actualVararg = params->children[actualFixed];
        if ( expectedHasArrayVararg && !xl6CgenParamIsArrayVararg(actualVararg) ) {
            return 0;
        }
        if ( expectedHasDictVararg && !xl6CgenParamIsDictVararg(actualVararg) ) {
            return 0;
        }
        return xl6CgenCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
    }

    if ( actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
        return 0;
    }
    return xl6CgenCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
}

static int xl6CgenFunctionTypeMatchesFunc(XL6_CgenContext* ctx, const XL6_AstNode* functionType, const XL6_AstNode* func)
{
    const XL6_AstNode* expectedParams;
    const XL6_AstNode* expectedReturns;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( functionType == NULL || !xl6CTextEquals(functionType->text, "function") || func == NULL ) {
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
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    return xl6CgenCallableParamListMatchesFunctionParams(ctx, expectedParams, params) &&
           xl6CgenCallableTypeListEqual(ctx, expectedReturns, returns);
}

static XL6_AstNode* xl6CgenFindFunctionValueFunc(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode)
{
    XL6_AstNode* selected;
    int candidateCount;
    int matchCount;
    size_t i;

    selected = NULL;
    candidateCount = 0;
    matchCount = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL || functionType == NULL ||
         !xl6CTextEquals(functionType->text, "function") ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* func = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( func == NULL || func->kind != XL6_AST_FUNC || xl6CgenFuncIsGeneric(func) || func->childCount == 0 ) {
            continue;
        }
        nameNode = func->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }
        candidateCount += 1;
        if ( xl6CgenFunctionTypeMatchesFunc(ctx, functionType, func) ) {
            selected = func;
            matchCount += 1;
        }
    }

    if ( matchCount == 1 ) {
        return selected;
    }
    if ( candidateCount == 0 ) {
        xl6CgenError(ctx, atNode, "找不到可作为 function 值的具名函数");
    } else if ( functionType->childCount == 0 ) {
        xl6CgenError(ctx, atNode, "裸 function 目标无法从重载函数名选择唯一候选");
    } else if ( matchCount == 0 ) {
        xl6CgenError(ctx, atNode, "找不到和 function 目标签名匹配的具名函数");
    } else {
        xl6CgenError(ctx, atNode, "function 目标签名匹配到多个具名函数候选");
    }
    return NULL;
}

static void xl6CgenFunctionValueCName(XL6_CgenContext* ctx, const char* name, char* cName, size_t cNameSize)
{
    char baseName[512];

    if ( cName == NULL || cNameSize == 0 ) {
        return;
    }
    xl6CgenCIdentText(baseName, sizeof(baseName), name);
    if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, name) >= 0 ) {
        snprintf(cName, cNameSize, "__env->%s", baseName);
    } else {
        snprintf(cName, cNameSize, "%s", baseName);
    }
}

static void xl6CgenBareFunctionInvoke(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* args)
{
    size_t i;
    char cName[512];

    xl6CgenFunctionValueCName(ctx, name, cName, sizeof(cName));
    fprintf(ctx->out, "xl6_function_call_var(%s, %u", cName, args != NULL ? (unsigned)args->childCount : 0);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            fputs(", ", ctx->out);
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                xl6CgenError(ctx, arg, "裸 function 调用不支持命名参数");
                fputs("xvoCreateNull()", ctx->out);
            } else if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                fputs("xvoCreateNull()", ctx->out);
            }
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenFunctionInvoke(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* args)
{
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* returns;
    unsigned long long signatureId;
    size_t i;
    size_t fixedCount;
    size_t positionalCount;
    int hasArrayVararg;
    int hasDictVararg;
    int returnCount;
    char cName[512];

    paramTypes = xl6CgenFunctionParamTypes(functionType);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( paramTypes == NULL || returns == NULL ) {
        xl6CgenBareFunctionInvoke(ctx, name, args);
        return;
    }
    hasArrayVararg = paramTypes->childCount > 0 &&
                     xl6CgenFunctionTypeParamIsArrayVararg(paramTypes->children[paramTypes->childCount - 1]);
    hasDictVararg = paramTypes->childCount > 0 &&
                    xl6CgenFunctionTypeParamIsDictVararg(paramTypes->children[paramTypes->childCount - 1]);
    fixedCount = xl6CgenFunctionTypeFixedParamCount(paramTypes);
    positionalCount = xl6CgenFunctionValuePositionalArgCount(args);
    if ( positionalCount < fixedCount ||
         (!hasArrayVararg && positionalCount > fixedCount) ||
         (!hasDictVararg && xl6CgenFunctionValueDictVarargCount(args) > 0) ) {
        xl6CgenError(ctx, functionType, "function 调用实参数量和签名不匹配");
        fputs("0", ctx->out);
        return;
    }

    returnCount = (int)returns->childCount;
    signatureId = xl6CgenFunctionSignatureId(ctx, paramTypes, returns);
    xl6CgenFunctionValueCName(ctx, name, cName, sizeof(cName));

    fprintf(ctx->out, "((%s.signature == 0x%016llXULL) ? ", cName, signatureId);
    fputs("((", ctx->out);
    if ( returnCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
    } else if ( returnCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fputs(" (*)(void*", ctx->out);
    for ( i = 0; i < paramTypes->childCount; ++i ) {
        fputs(", ", ctx->out);
        fputs(xl6CgenTypeName(ctx, paramTypes->children[i]), ctx->out);
    }
    fprintf(ctx->out, "))%s.invoke)(%s.env", cName, cName);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* arg = xl6CgenFunctionValuePositionalArg(args, i);
        fputs(", ", ctx->out);
        if ( arg != NULL && arg->childCount > 0 ) {
            xl6CgenExpr(ctx, arg->children[0]);
        } else {
            fputs("0", ctx->out);
        }
    }
    if ( hasArrayVararg ) {
        fputs(", ", ctx->out);
        xl6CgenArrayVarargExpr(ctx, args, (int)fixedCount);
    } else if ( hasDictVararg ) {
        fputs(", ", ctx->out);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
    }
    if ( returnCount > 1 ) {
        fputs(") : (exit(3), (", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
        fputs("){0}))", ctx->out);
    } else if ( returnCount == 1 ) {
        fprintf(ctx->out, ") : (exit(3), (%s)0))", xl6CgenTypeName(ctx, returns->children[0]));
    } else {
        fputs(") : exit(3))", ctx->out);
    }
}

static void xl6CgenAnonMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenAnonFunc* info;
    size_t i;

    info = xl6CgenFindAnonFuncInfo(ctx, node);
    if ( info == NULL ) {
        xl6CgenError(ctx, node, "找不到匿名函数生成信息");
        fputs("(XL6_Function){0}", ctx->out);
        return;
    }

    fprintf(ctx->out, "xl6_anon_%d_make(", info->id);
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( i > 0 ) {
            fputs(", ", ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
    }
    fputc(')', ctx->out);
}

static void xl6CgenFunctionValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType)
{
    if ( expr != NULL && expr->kind == XL6_AST_NAME &&
         xl6CgenFindVarType(ctx, expr->text) == NULL &&
         xl6CgenIsFunctionType(targetType) ) {
        XL6_AstNode* func = xl6CgenFindFunctionValueFunc(ctx, expr->text, targetType, expr);
        if ( func != NULL ) {
            xl6CgenFunctionValueMakeCall(ctx, func, targetType);
            return;
        }
    }
    xl6CgenExpr(ctx, expr);
}

static int xl6CgenParamIsVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           (xl6CTextEquals(param->children[0]->text, "array<var>") ||
            xl6CTextEquals(param->children[0]->text, "dict<var>"));
}

static int xl6CgenParamIsArrayVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6CTextEquals(param->children[0]->text, "array<var>");
}

static int xl6CgenParamIsDictVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6CTextEquals(param->children[0]->text, "dict<var>");
}

static int xl6CgenFixedParamCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CgenParamIsVararg(params->children[count]) ) {
        ++count;
    }
    return count;
}

static int xl6CgenParamRequiredCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CgenParamIsVararg(params->children[count]) ) {
        if ( params->children[count]->childCount > 1 ) {
            break;
        }
        ++count;
    }
    return count;
}

static int xl6CgenFuncHasVararg(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    int fixedCount;

    if ( funcNode == NULL || funcNode->childCount <= 1 ) {
        return 0;
    }
    params = funcNode->children[1];
    fixedCount = xl6CgenFixedParamCount(params);
    return params != NULL && params->childCount > (size_t)fixedCount;
}

static const XL6_AstNode* xl6CgenArgExpr(const XL6_AstNode* arg)
{
    if ( arg != NULL && arg->childCount > 0 ) {
        return arg->children[0];
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindArgForParam(const XL6_AstNode* args, const char* name, int index)
{
    size_t i;
    int positionalIndex;

    if ( args == NULL ) {
        return NULL;
    }

    positionalIndex = 0;
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            if ( xl6CTextEquals(arg->text, name) ) {
                return xl6CgenArgExpr(arg);
            }
        } else {
            if ( positionalIndex == index ) {
                return xl6CgenArgExpr(arg);
            }
            positionalIndex += 1;
        }
    }
    return NULL;
}

static int xl6CgenFindParamIndex(const XL6_AstNode* params, int fixedCount, const char* name)
{
    int i;

    if ( params == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < fixedCount && (size_t)i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return i;
        }
    }
    return -1;
}

static void xl6CgenWriteCStringChar(FILE* out, unsigned char ch)
{
    switch ( ch ) {
    case '\n':
        fputs("\\n", out);
        break;
    case '\r':
        fputs("\\r", out);
        break;
    case '\t':
        fputs("\\t", out);
        break;
    case '"':
    case '\\':
        fputc('\\', out);
        fputc((int)ch, out);
        break;
    default:
        fputc((int)ch, out);
        break;
    }
}

static void xl6CgenCStringLiteral(FILE* out, const char* text)
{
    const unsigned char* p;

    fputc('"', out);
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
            xl6CgenWriteCStringChar(out, *p);
        }
    }
    fputc('"', out);
}

static int xl6CgenStringLiteralParts(const char* text, size_t* outStart, size_t* outEnd, int* outRaw)
{
    size_t len;
    size_t pos;
    int raw;

    if ( text == NULL || outStart == NULL || outEnd == NULL || outRaw == NULL ) {
        return 0;
    }
    len = strlen(text);
    pos = 0;
    raw = 0;
    if ( len > 0 && text[0] == 'r' ) {
        raw = 1;
        pos = 1;
    }

    if ( pos + 6 <= len && strncmp(text + pos, "\"\"\"", 3) == 0 &&
         strncmp(text + len - 3, "\"\"\"", 3) == 0 ) {
        *outStart = pos + 3;
        *outEnd = len - 3;
        *outRaw = raw;
        return 1;
    }
    if ( pos + 6 <= len && strncmp(text + pos, "'''", 3) == 0 &&
         strncmp(text + len - 3, "'''", 3) == 0 ) {
        *outStart = pos + 3;
        *outEnd = len - 3;
        *outRaw = raw;
        return 1;
    }
    if ( pos + 2 <= len && (text[pos] == '"' || text[pos] == '\'') && text[len - 1] == text[pos] ) {
        *outStart = pos + 1;
        *outEnd = len - 1;
        *outRaw = raw;
        return 1;
    }
    return 0;
}

static void xl6CgenStringLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* text;
    size_t start;
    size_t end;
    size_t i;
    int raw;

    text = node != NULL ? node->text : NULL;
    if ( !xl6CgenStringLiteralParts(text, &start, &end, &raw) ) {
        xl6CgenError(ctx, node, "字符串字面量格式无效");
        fputs("\"\"", ctx->out);
        return;
    }

    fputc('"', ctx->out);
    for ( i = start; i < end; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( !raw && ch == '\\' && i + 1 < end ) {
            fputc('\\', ctx->out);
            fputc(text[++i], ctx->out);
            continue;
        }
        xl6CgenWriteCStringChar(ctx->out, ch);
    }
    fputc('"', ctx->out);
}

static void xl6CgenXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    if ( expr == NULL ) {
        fputs("xvoCreateNull()", ctx->out);
        return;
    }
    if ( expr->kind == XL6_AST_NULL ) {
        fputs("xvoCreateNull()", ctx->out);
        return;
    }

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( typeNode != NULL && xl6CgenIsXValueType(typeNode) ) {
        if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
            fputs("xl6_xrt_retain(", ctx->out);
            xl6CgenExpr(ctx, expr);
            fputc(')', ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
        }
    } else if ( typeNode != NULL &&
                (xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int8") ||
                 xl6CTextEquals(typeNode->text, "uint8") || xl6CTextEquals(typeNode->text, "int16") ||
                 xl6CTextEquals(typeNode->text, "uint16") || xl6CTextEquals(typeNode->text, "int32") ||
                 xl6CTextEquals(typeNode->text, "uint32") || xl6CTextEquals(typeNode->text, "int64") ||
                 xl6CTextEquals(typeNode->text, "uint64")) ) {
        fputs("xvoCreateInt((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "point") ) {
        fputs("xvoCreatePoint((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("))", ctx->out);
    } else if ( typeNode != NULL &&
                (xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
                 xl6CTextEquals(typeNode->text, "float64")) ) {
        fputs("xvoCreateFloat((double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "string") ) {
        fputs("xvoCreateText((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("), 0, false)", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "time") ) {
        fputs("xvoCreateTime((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "bool") ) {
        fputs("xvoCreateBool(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "当前 C 生成暂不支持将该表达式打包为 var");
        fputs("xvoCreateNull()", ctx->out);
    }
    xl6AstFree(typeNode);
}

static const XL6_AstNode* xl6CgenTypeConvertArg(const XL6_AstNode* node)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static void xl6CgenCastExpr(XL6_CgenContext* ctx, const char* cType, const XL6_AstNode* expr)
{
    fprintf(ctx->out, "((%s)(", cType != NULL ? cType : "int");
    xl6CgenExpr(ctx, expr);
    fputs("))", ctx->out);
}

static void xl6CgenTypeConvertExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* arg;
    XL6_AstNode* sourceType;
    const char* target;
    const char* source;
    int sourceIsXValue;

    callee = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    arg = xl6CgenTypeConvertArg(node);
    target = callee != NULL ? callee->text : NULL;
    sourceType = xl6CgenCloneExprTypeForInference(ctx, arg);
    source = sourceType != NULL ? sourceType->text : NULL;
    sourceIsXValue = xl6CgenIsXValueType(sourceType);

    if ( xl6CTextEquals(target, "var") ) {
        xl6CgenXValueFromExpr(ctx, arg);
    } else if ( sourceIsXValue ) {
        if ( xl6CgenIsIntegerTypeName(target) ) {
            fprintf(ctx->out, "((%s)xvoGetInt(", xl6CgenConvertCTypeName(target));
            xl6CgenExpr(ctx, arg);
            fputs("))", ctx->out);
        } else if ( xl6CgenIsFloatTypeName(target) ) {
            fprintf(ctx->out, "((%s)xvoGetFloat(", xl6CgenConvertCTypeName(target));
            xl6CgenExpr(ctx, arg);
            fputs("))", ctx->out);
        } else if ( xl6CTextEquals(target, "bool") ) {
            fputs("xvoGetBool(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "string") ) {
            fputs("xl6_xrt_value_to_string(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "time") ) {
            fputs("xvoGetTime(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "point") ) {
            fputs("xvoGetPoint(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持该类型转换");
            fputs("0", ctx->out);
        }
    } else if ( xl6CgenIsIntegerTypeName(target) ) {
        fprintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            fputs("xrtStrToI64(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else if ( xl6CTextEquals(source, "null") ) {
            fputs("0", ctx->out);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        fputs("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(target) ) {
        fprintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            fputs("xrtStrToNum(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else if ( xl6CTextEquals(source, "null") ) {
            fputs("0", ctx->out);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        fputs("))", ctx->out);
    } else if ( xl6CTextEquals(target, "bool") ) {
        if ( xl6CTextEquals(source, "null") ) {
            fputs("0", ctx->out);
        } else {
            fputc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs(" != 0)", ctx->out);
        }
    } else if ( xl6CTextEquals(target, "string") ) {
        if ( xl6CTextEquals(source, "string") ) {
            xl6CgenExpr(ctx, arg);
        } else if ( xl6CTextEquals(source, "null") ) {
            fputs("\"\"", ctx->out);
        } else if ( xl6CgenIsIntegerTypeName(source) ) {
            fputs("xl6_xrt_i64_to_string((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs("))", ctx->out);
        } else if ( xl6CgenIsFloatTypeName(source) ) {
            fputs("xl6_xrt_f64_to_string((double)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs("))", ctx->out);
        } else if ( xl6CTextEquals(source, "bool") ) {
            fputc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs(" ? \"true\" : \"false\")", ctx->out);
        } else if ( xl6CTextEquals(source, "time") ) {
            fputs("xrtTimeToStr((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs("), 0)", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            fputs("xrtFormat(\"[point:%p]\", ", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputc(')', ctx->out);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持转换为 string");
            fputs("\"\"", ctx->out);
        }
    } else if ( xl6CTextEquals(target, "time") ) {
        if ( xl6CTextEquals(source, "string") ) {
            fputs("xrtStrToTime(", ctx->out);
            xl6CgenExpr(ctx, arg);
            fputs(", 0)", ctx->out);
        } else if ( xl6CTextEquals(source, "null") ) {
            fputs("0", ctx->out);
        } else {
            xl6CgenCastExpr(ctx, "long long", arg);
        }
    } else if ( xl6CTextEquals(target, "point") ) {
        if ( xl6CTextEquals(source, "null") ) {
            fputs("(void*)0", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            xl6CgenExpr(ctx, arg);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持转换为 point");
            fputs("(void*)0", ctx->out);
        }
    } else {
        xl6CgenError(ctx, node, "当前 C 生成不支持该类型转换");
        fputs("0", ctx->out);
    }

    xl6AstFree(sourceType);
}

static void xl6CgenNullCoalesceExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int isXValue;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( left != NULL && left->kind == XL6_AST_NULL ) {
        xl6CgenExpr(ctx, right);
        return;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    isXValue = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);

    if ( isXValue ) {
        if ( left == NULL || !xl6CgenIsXValueType(leftType) ) {
            xl6CgenError(ctx, node, "当前 C 生成中 ?? 左侧必须是 var 或 XRT 容器值");
            fputs("xvoCreateNull()", ctx->out);
        } else {
            fputs("(xvoIsNull(", ctx->out);
            xl6CgenExpr(ctx, left);
            fputs(") ? ", ctx->out);
            xl6CgenXValueFromExpr(ctx, right);
            fputs(" : ", ctx->out);
            xl6CgenXValueFromExpr(ctx, left);
            fputc(')', ctx->out);
        }
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6CgenError(ctx, node, "当前 C 生成中 ?? 两侧必须是同一静态类型，或左侧为 null/var");
        fputs("0", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    fputc('(', ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(" != 0 ? ", ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(" : ", ctx->out);
    xl6CgenExpr(ctx, right);
    fputc(')', ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
}

static void xl6CgenInExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* rightType;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( right != NULL && xl6CgenIsRangeCall(right) ) {
        xl6CgenRangeContainsExpr(ctx, left, right);
        return;
    }

    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    if ( rightType == NULL || rightType->text == NULL ) {
        xl6CgenError(ctx, node, "当前 C 生成 in 右侧类型必须可静态确定，或为 var");
        fputs("0", ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "string") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "字符串 in 判断左侧必须是 string");
            fputs("0", ctx->out);
        } else {
            fputs("(xrtFindStr(", ctx->out);
            xl6CgenExpr(ctx, right);
            fputs(", 0, ", ctx->out);
            xl6CgenExpr(ctx, left);
            fputs(", 0, false) != 0)", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsArrayXValueType(rightType) ) {
        fputs("xl6_xrt_array_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        fputc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "list") || xl6CTextEquals(rightType->text, "list<var>") ) {
        fputs("xl6_xrt_list_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        fputc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "set") || xl6CTextEquals(rightType->text, "set<var>") ) {
        fputs("xl6_xrt_set_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        fputc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsDictXValueType(rightType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "dict in 判断左侧必须是 string 键");
            fputs("0", ctx->out);
        } else {
            fputs("xvoTableExists(", ctx->out);
            xl6CgenExpr(ctx, right);
            fputs(", ", ctx->out);
            xl6CgenExpr(ctx, left);
            fputs(", 0)", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "var") ) {
        fputs("xl6_xrt_value_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        fputc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenError(ctx, node, "当前 C 生成 in 右侧只支持 string、array、list、set、dict、range 或 var");
    fputs("0", ctx->out);
    xl6AstFree(rightType);
}

static void xl6CgenArrayLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    fprintf(ctx->out, "xl6_xrt_array_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    fputc(')', ctx->out);
}

static void xl6CgenListLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    fprintf(ctx->out, "xl6_xrt_list_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        fputs(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, key);
        fputs("), ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    fputc(')', ctx->out);
}

static void xl6CgenSetLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    fprintf(ctx->out, "xl6_xrt_set_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    fputc(')', ctx->out);
}

static void xl6CgenDictLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    fprintf(ctx->out, "xl6_xrt_dict_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        fputs(", ", ctx->out);
        if ( !xl6CgenExprStaticTypeIs(ctx, key, "string") ) {
            xl6CgenError(ctx, key, "当前 C 生成 dict 字面量键只支持 string");
            fputs("\"\"", ctx->out);
        } else {
            xl6CgenExpr(ctx, key);
        }
        fputs(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    fputc(')', ctx->out);
}

static void xl6CgenTypedLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* literal = node != NULL && node->childCount > 0 ? node->children[0] : NULL;

    if ( xl6CTextEquals(node != NULL ? node->text : NULL, "array") ) {
        xl6CgenArrayLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "list") ) {
        xl6CgenListLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "set") ) {
        xl6CgenSetLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "dict") ) {
        xl6CgenDictLiteralExpr(ctx, literal);
    } else {
        xl6CgenError(ctx, node, "未知的带类型容器字面量");
        fputs("xvoCreateNull()", ctx->out);
    }
}

static int xl6CgenArrayVarargCount(const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;

    positionalIndex = 0;
    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            continue;
        }
        if ( positionalIndex >= fixedCount ) {
            count += 1;
        }
        positionalIndex += 1;
    }
    return count;
}

static void xl6CgenArrayVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;
    int emitted;

    count = xl6CgenArrayVarargCount(args, fixedCount);
    fprintf(ctx->out, "xl6_xrt_array_varargs(%d", count);
    positionalIndex = 0;
    emitted = 0;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            if ( positionalIndex >= fixedCount ) {
                fputs(", ", ctx->out);
                xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
                emitted += 1;
            }
            positionalIndex += 1;
        }
    }
    (void)emitted;
    fputc(')', ctx->out);
}

static int xl6CgenDictVarargCount(const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG &&
             xl6CgenFindParamIndex(params, fixedCount, arg->text) < 0 ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenDictVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = xl6CgenDictVarargCount(params, args, fixedCount);
    fprintf(ctx->out, "xl6_xrt_dict_varargs(%d", count);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind != XL6_AST_NAMED_ARG ||
                 xl6CgenFindParamIndex(params, fixedCount, arg->text) >= 0 ) {
                continue;
            }
            fputs(", ", ctx->out);
            xl6CgenCStringLiteral(ctx->out, arg->text);
            fputs(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenKnownCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args)
{
    const XL6_AstNode* params;
    int fixedCount;
    int hasVararg;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasVararg = params != NULL && params->childCount > (size_t)fixedCount;

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);

        if ( i > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenExpr(ctx, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "缺少必选参数");
            fputs("0", ctx->out);
        }
    }
    if ( hasVararg ) {
        const XL6_AstNode* vararg = params->children[fixedCount];
        if ( fixedCount > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( xl6CgenIsArrayXValueType(vararg->children[0]) ) {
            xl6CgenArrayVarargExpr(ctx, args, fixedCount);
        } else if ( xl6CgenIsDictXValueType(vararg->children[0]) ) {
            xl6CgenDictVarargExpr(ctx, params, args, fixedCount);
        } else {
            xl6CgenError(ctx, vararg, "未知可变参数类型");
            fputs("xvoCreateNull()", ctx->out);
        }
    }
}

static void xl6CgenParamNodeArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args)
{
    int fixedCount;
    int i;

    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        xl6CgenError(ctx, params, "当前 C 生成暂不支持可变参数调用");
    }
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);
        if ( i > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenExpr(ctx, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "缺少必选参数");
            fputs("0", ctx->out);
        }
    }
}

static int xl6CgenFuncOverloadCount(XL6_CgenContext* ctx, const char* name)
{
    size_t i;
    int count;

    count = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( node == NULL || node->kind != XL6_AST_FUNC || xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenMangledPart(FILE* out, const char* text)
{
    xl6CgenFputsEncodedText(out, text);
}

static void xl6CgenMangledType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        fputc('_', ctx->out);
        return;
    }

    xl6CgenMangledPart(ctx->out, typeNode->text);
    args = (typeNode->childCount > 0 && typeNode->children[0] != NULL &&
            typeNode->children[0]->kind == XL6_AST_LIST) ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }

    fputs("_of", ctx->out);
    for ( i = 0; i < args->childCount; ++i ) {
        fputs("_", ctx->out);
        xl6CgenMangledType(ctx, args->children[i]);
    }
}

static int xl6CgenGenericFuncNeedsSignatureSuffix(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeParams;
    size_t i;
    int count;

    if ( ctx == NULL || ctx->root == NULL || funcNode == NULL || !xl6CgenFuncIsGeneric(funcNode) ) {
        return 0;
    }
    nameNode = funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    typeParams = xl6CgenFuncTypeParams(funcNode);
    if ( nameNode == NULL || typeParams == NULL ) {
        return 0;
    }

    count = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        const XL6_AstNode* otherName;
        const XL6_AstNode* otherParams;

        if ( node == NULL || node->kind != XL6_AST_FUNC || !xl6CgenFuncIsGeneric(node) ) {
            continue;
        }
        otherName = node->childCount > 0 ? node->children[0] : NULL;
        otherParams = xl6CgenFuncTypeParams(node);
        if ( otherName != NULL && otherParams != NULL &&
             xl6CTextEquals(otherName->text, nameNode->text) &&
             otherParams->childCount == typeParams->childCount ) {
            count += 1;
        }
    }
    return count > 1;
}

static void xl6CgenGenericFuncSignatureSuffix(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    size_t i;

    params = funcNode != NULL && funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    fputs("__sig", ctx->out);
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", ctx->out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            fputc('_', ctx->out);
        }
    }
}

static const char* xl6CgenCallableName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_FUNC ||
         node->kind == XL6_AST_CUSTOM_EXTERN_FUNC ||
         node->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        const XL6_AstNode* nameNode = node->childCount > 0 ? node->children[0] : NULL;
        return nameNode != NULL ? nameNode->text : NULL;
    }
    return node->text;
}

static int xl6CgenClassCallableOverloadCount(const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* body;
    const char* name;
    size_t i;
    int count;

    body = xl6CgenClassBody(classNode);
    name = xl6CgenCallableName(memberNode);
    count = 0;
    if ( body == NULL || memberNode == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* other = body->children[i];
        if ( other != NULL &&
             other->kind == memberNode->kind &&
             xl6CTextEquals(xl6CgenCallableName(other), name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    xl6CgenClassMemberSymbol(ctx->out, classNode != NULL ? classNode->text : "_", name != NULL ? name : "_");

    if ( memberNode == NULL ||
         (memberNode->kind != XL6_AST_FUNC && memberNode->kind != XL6_AST_CREATE) ||
         xl6CgenClassCallableOverloadCount(classNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            fputc('_', ctx->out);
        }
    }
}

static void xl6CgenCustomMemberSymbol(FILE* out, const char* customName, const char* memberName)
{
    fputs("xl_custom_", out);
    xl6CgenFputsEncodedText(out, customName);
    fputs("__", out);
    xl6CgenFputsEncodedText(out, memberName);
}

static int xl6CgenCustomCallableOverloadCount(const XL6_AstNode* customNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* body;
    const char* name;
    size_t i;
    int count;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    name = xl6CgenCallableName(memberNode);
    count = 0;
    if ( body == NULL || memberNode == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* other = body->children[i];
        if ( other != NULL &&
             other->kind == memberNode->kind &&
             xl6CTextEquals(xl6CgenCallableName(other), name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenCustomCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    xl6CgenCustomMemberSymbol(ctx->out, customNode != NULL ? customNode->text : "_", name != NULL ? name : "_");

    if ( memberNode == NULL ||
         memberNode->kind != XL6_AST_FUNC ||
         xl6CgenCustomCallableOverloadCount(customNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            fputc('_', ctx->out);
        }
    }
}

static void xl6CgenFuncSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    nameNode = funcNode != NULL && funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    if ( xl6CgenFuncIsGeneric(funcNode) ) {
        if ( ctx->currentGenericArgs == NULL ) {
            xl6CgenError(ctx, funcNode, "泛型函数必须先实例化再生成 C 符号");
            fputs("xl_gen_uninstantiated", ctx->out);
            return;
        }
        fputs("xl_gen_", ctx->out);
        xl6CgenMangledPart(ctx->out, nameNode != NULL ? nameNode->text : "_");
        for ( i = 0; i < ctx->currentGenericArgs->childCount; ++i ) {
            fputs("__", ctx->out);
            xl6CgenMangledType(ctx, ctx->currentGenericArgs->children[i]);
        }
        if ( xl6CgenGenericFuncNeedsSignatureSuffix(ctx, funcNode) ) {
            xl6CgenGenericFuncSignatureSuffix(ctx, funcNode);
        }
        return;
    }
    if ( nameNode == NULL || (xl6CgenFuncOverloadCount(ctx, nameNode->text) <= 1 &&
         xl6CgenIsSimpleCIdent(nameNode->text)) ) {
        fputs((nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_", ctx->out);
        return;
    }

    fputs("xl_fn_", ctx->out);
    xl6CgenMangledPart(ctx->out, nameNode->text);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", ctx->out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6CgenMangledPart(ctx->out, param->children[0]->text);
        } else {
            fputc('_', ctx->out);
        }
    }
}

static void xl6CgenFunctionValueSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const char* suffix)
{
    fputs("xl6_fnvalue_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    fputs(suffix != NULL ? suffix : "", ctx->out);
}

static void xl6CgenFunctionValueAdapterSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, int paramCount, int includeVararg, const char* suffix)
{
    fputs("xl6_fnvalue_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    fprintf(ctx->out, "_arity%d", paramCount);
    if ( includeVararg ) {
        fputs("_vararg", ctx->out);
    }
    fputs(suffix != NULL ? suffix : "", ctx->out);
}

static int xl6CgenFunctionValueAdapterInfo(const XL6_AstNode* funcNode, const XL6_AstNode* targetType, int* outParamCount, int* outIncludeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* targetParams;
    size_t targetCount;
    size_t targetFixed;
    int targetHasArrayVararg;
    int targetHasDictVararg;
    int actualFixed;
    int actualRequired;
    int actualHasVararg;
    int includeVararg;

    if ( outParamCount != NULL ) {
        *outParamCount = 0;
    }
    if ( outIncludeVararg != NULL ) {
        *outIncludeVararg = 0;
    }
    if ( funcNode == NULL || targetType == NULL || !xl6CTextEquals(targetType->text, "function") ||
         targetType->childCount < 2 ) {
        return 0;
    }

    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    targetParams = targetType->children[0];
    targetCount = targetParams != NULL ? targetParams->childCount : 0;
    targetHasArrayVararg = targetCount > 0 && xl6CgenFunctionTypeParamIsArrayVararg(targetParams->children[targetCount - 1]);
    targetHasDictVararg = targetCount > 0 && xl6CgenFunctionTypeParamIsDictVararg(targetParams->children[targetCount - 1]);
    targetFixed = (targetHasArrayVararg || targetHasDictVararg) ? targetCount - 1 : targetCount;
    actualFixed = xl6CgenFixedParamCount(params);
    actualRequired = xl6CgenParamRequiredCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)actualFixed;
    includeVararg = targetHasArrayVararg || targetHasDictVararg;

    if ( targetFixed >= (size_t)actualFixed || targetFixed < (size_t)actualRequired ) {
        return 0;
    }
    if ( includeVararg ) {
        const XL6_AstNode* actualVararg;

        if ( !actualHasVararg ) {
            return 0;
        }
        actualVararg = params->children[actualFixed];
        if ( targetHasArrayVararg && !xl6CgenParamIsArrayVararg(actualVararg) ) {
            return 0;
        }
        if ( targetHasDictVararg && !xl6CgenParamIsDictVararg(actualVararg) ) {
            return 0;
        }
    } else if ( actualHasVararg ) {
        return 0;
    }

    if ( outParamCount != NULL ) {
        *outParamCount = (int)targetFixed;
    }
    if ( outIncludeVararg != NULL ) {
        *outIncludeVararg = includeVararg;
    }
    return 1;
}

static void xl6CgenFunctionValueMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* targetType)
{
    int adapterParamCount;
    int adapterIncludeVararg;

    if ( funcNode == NULL ) {
        fputs("(XL6_Function){0}", ctx->out);
        return;
    }
    if ( xl6CgenFunctionValueAdapterInfo(funcNode, targetType, &adapterParamCount, &adapterIncludeVararg) ) {
        xl6CgenFunctionValueAdapterSymbol(ctx, funcNode, adapterParamCount, adapterIncludeVararg, "_make()");
        return;
    }
    xl6CgenFunctionValueSymbol(ctx, funcNode, "_make()");
}

static int xl6CgenFuncHasMultiReturn(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;

    returns = funcNode != NULL && funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    return returns != NULL && returns->childCount > 1;
}

static void xl6CgenFuncReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    fputs("XL6_Return_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
}

static void xl6CgenFuncReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;
    size_t i;

    if ( !xl6CgenFuncHasMultiReturn(funcNode) ) {
        return;
    }

    returns = funcNode->children[2];
    fputs("typedef struct ", ctx->out);
    xl6CgenFuncReturnStructName(ctx, funcNode);
    fputs(" {\n", ctx->out);
    for ( i = 0; i < returns->childCount; ++i ) {
        fputs("    ", ctx->out);
        fputs(xl6CgenTypeName(ctx, returns->children[i]), ctx->out);
        fprintf(ctx->out, " value%u;\n", (unsigned)i);
    }
    fputs("} ", ctx->out);
    xl6CgenFuncReturnStructName(ctx, funcNode);
    fputs(";\n\n", ctx->out);
}

static void xl6CgenFunctionReturnStructDeclsInNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }

    if ( node->kind == XL6_AST_TYPE && xl6CTextEquals(node->text, "function") && node->childCount > 1 ) {
        xl6CgenFunctionReturnStructDecl(ctx, node->children[0], node->children[1]);
    } else if ( node->kind == XL6_AST_ANON_FUNC && node->childCount > 1 ) {
        xl6CgenFunctionReturnStructDecl(ctx, node->childCount > 0 ? node->children[0] : NULL, node->children[1]);
    }

    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenFunctionReturnStructDeclsInNode(ctx, node->children[i]);
    }
}

static void xl6CgenFunctionReturnStructDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    if ( ctx == NULL || root == NULL ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclsInNode(ctx, root);
}

static int xl6CgenLiteralMatchesType(const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    if ( expr == NULL || typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ||
               xl6CTextEquals(typeNode->text, "int32");
    case XL6_AST_FLOAT:
        return xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ||
               xl6CTextEquals(typeNode->text, "float32");
    case XL6_AST_STRING:
        return xl6CTextEquals(typeNode->text, "string");
    case XL6_AST_TIME:
        return xl6CTextEquals(typeNode->text, "time");
    case XL6_AST_BOOL:
        return xl6CTextEquals(typeNode->text, "bool");
    default:
        return 1;
    }
}

static int xl6CgenExprParamMatchCost(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode);

static int xl6CgenExprMatchesParamType(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    return xl6CgenExprParamMatchCost(ctx, expr, typeNode) >= 0;
}

static int xl6CgenExprParamMatchCost(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_AstNode* exprType;
    const XL6_AstNode* paramType;
    const char* exprTypeName;
    const char* targetType;
    int result;

    if ( expr == NULL || typeNode == NULL ) {
        return 0;
    }

    paramType = xl6CgenSubstituteGenericType(ctx, typeNode);
    targetType = paramType != NULL ? paramType->text : NULL;
    if ( targetType == NULL ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( exprType != NULL ) {
        exprTypeName = exprType->text;
        if ( xl6CgenTypeNodeEqual(exprType, paramType) ) {
            xl6AstFree(exprType);
            return 0;
        }
        if ( xl6CTextEquals(targetType, "var") ) {
            xl6AstFree(exprType);
            return 80;
        }
        if ( xl6CgenIsIntegerTypeName(targetType) &&
             (xl6CgenIsIntegerTypeName(exprTypeName) || xl6CTextEquals(exprTypeName, "bool")) ) {
            xl6AstFree(exprType);
            return 10;
        }
        if ( xl6CgenIsFloatTypeName(targetType) &&
             (xl6CgenIsIntegerTypeName(exprTypeName) || xl6CgenIsFloatTypeName(exprTypeName) ||
              xl6CTextEquals(exprTypeName, "bool")) ) {
            result = (xl6CgenIsIntegerTypeName(exprTypeName) || xl6CTextEquals(exprTypeName, "bool")) ? 20 : 10;
            xl6AstFree(exprType);
            return result;
        }
        xl6AstFree(exprType);
        return -1;
    }

    if ( xl6CTextEquals(targetType, "var") ) {
        return 80;
    }
    if ( expr->kind == XL6_AST_INT && xl6CgenIsIntegerTypeName(targetType) ) {
        return xl6CTextEquals(targetType, "int") ? 0 : 1;
    }
    if ( expr->kind == XL6_AST_FLOAT && xl6CgenIsFloatTypeName(targetType) ) {
        return xl6CTextEquals(targetType, "float") ? 0 : 2;
    }
    return xl6CgenLiteralMatchesType(expr, paramType) ? 0 : -1;
}

static void xl6CgenFreeInferredTypes(XL6_AstNode** inferredTypes, size_t count)
{
    size_t i;

    if ( inferredTypes == NULL ) {
        return;
    }
    for ( i = 0; i < count; ++i ) {
        xl6AstFree(inferredTypes[i]);
    }
    free(inferredTypes);
}

static int xl6CgenExprGenericParamMatchCost(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes)
{
    XL6_AstNode* exprType;
    int index;
    int ok;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( typeParams != NULL &&
         typeParams->childCount > 0 &&
         xl6CgenTypeContainsTypeParam(typeParams, typeNode) ) {
        exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
        if ( exprType == NULL ) {
            return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
        }
        ok = xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, typeNode, exprType, 0, expr);
        xl6AstFree(exprType);
        if ( ok ) {
            return 40;
        }

        index = xl6CgenTypeParamIndex(typeParams, typeNode->text);
        if ( index < 0 && !xl6CTextEquals(typeNode->text, "function") ) {
            return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
        }
        return -1;
    }
    return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
}

static XL6_AstNode* xl6CgenTakeInferredTypeArgs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    const XL6_AstNode* at)
{
    XL6_AstNode* result;
    size_t i;

    if ( typeParams == NULL || typeParams->childCount == 0 || inferredTypes == NULL ) {
        return NULL;
    }
    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        xl6CgenError(ctx, at, "泛型类型推断内存不足");
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        if ( inferredTypes[i] == NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        xl6AstAddChild(result, inferredTypes[i]);
        inferredTypes[i] = NULL;
    }
    return result;
}

static int xl6CgenFuncMatchesCallEx(
    XL6_CgenContext* ctx,
    const XL6_AstNode* funcNode,
    const XL6_AstNode* args,
    int* outCost,
    XL6_AstNode** outTypeArgs)
{
    const XL6_AstNode* params;
    const XL6_AstNode* typeParams;
    XL6_AstNode** inferredTypes;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;
    int cost;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    params = xl6CgenCallableParams(funcNode);
    typeParams = xl6CgenFuncTypeParams(funcNode);
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)calloc((size_t)fixedCount, sizeof(int)) : NULL;
    inferredTypes = typeParams != NULL && typeParams->childCount > 0 ?
                    (XL6_AstNode**)calloc(typeParams->childCount, sizeof(XL6_AstNode*)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 0;

    if ( fixedCount > 0 && provided == NULL ) {
        xl6CgenFreeInferredTypes(inferredTypes, typeParams != NULL ? typeParams->childCount : 0);
        return 0;
    }
    if ( typeParams != NULL && typeParams->childCount > 0 && inferredTypes == NULL ) {
        free(provided);
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL ) {
                continue;
            }
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6CgenFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    const XL6_AstNode* param = params->children[index];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprGenericParamMatchCost(ctx, expr, param->children[0], typeParams, inferredTypes) : 0;
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
                    const XL6_AstNode* param = params->children[positionalIndex];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprGenericParamMatchCost(ctx, expr, param->children[0], typeParams, inferredTypes) : 0;
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
        const XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    if ( ok && typeParams != NULL && typeParams->childCount > 0 ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            if ( inferredTypes[i] == NULL ) {
                ok = 0;
                break;
            }
        }
    }

    if ( ok && outTypeArgs != NULL && typeParams != NULL && typeParams->childCount > 0 ) {
        *outTypeArgs = xl6CgenTakeInferredTypeArgs(ctx, typeParams, inferredTypes, funcNode);
        if ( *outTypeArgs == NULL ) {
            ok = 0;
        }
    }

    free(provided);
    xl6CgenFreeInferredTypes(inferredTypes, typeParams != NULL ? typeParams->childCount : 0);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static int xl6CgenFuncMatchesCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* args, int* outCost)
{
    return xl6CgenFuncMatchesCallEx(ctx, funcNode, args, outCost, NULL);
}

static int xl6CgenExprExplicitGenericParamMatchCost(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode)
{
    XL6_AstNode* concreteType;
    int cost;

    concreteType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    cost = xl6CgenExprParamMatchCost(ctx, expr, concreteType != NULL ? concreteType : typeNode);
    xl6AstFree(concreteType);
    return cost;
}

static int xl6CgenFuncMatchesExplicitGenericCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* funcNode,
    const XL6_AstNode* args,
    const XL6_AstNode* typeArgs,
    int* outCost)
{
    const XL6_AstNode* params;
    const XL6_AstNode* typeParams;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;
    int cost;

    typeParams = xl6CgenFuncTypeParams(funcNode);
    if ( typeParams == NULL || typeParams->childCount == 0 ||
         typeArgs == NULL || typeParams->childCount != typeArgs->childCount ) {
        return 0;
    }

    params = xl6CgenCallableParams(funcNode);
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)calloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 40;

    if ( fixedCount > 0 && provided == NULL ) {
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = typeParams;
    ctx->currentGenericArgs = typeArgs;

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL ) {
                continue;
            }
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6CgenFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    const XL6_AstNode* param = params->children[index];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprExplicitGenericParamMatchCost(ctx, expr, param->children[0]) : 0;
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
                    const XL6_AstNode* param = params->children[positionalIndex];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprExplicitGenericParamMatchCost(ctx, expr, param->children[0]) : 0;
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
        const XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    free(provided);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static void xl6CgenSelectCallCandidate(
    XL6_AstNode** selected,
    XL6_AstNode** selectedArgs,
    int* bestCount,
    int* bestCost,
    int* bestIsGeneric,
    XL6_AstNode* candidate,
    XL6_AstNode* candidateArgs,
    int candidateCost)
{
    int candidateIsGeneric;

    candidateIsGeneric = xl6CgenFuncIsGeneric(candidate);
    if ( *bestCount == 0 ||
         candidateCost < *bestCost ||
         (candidateCost == *bestCost && *bestIsGeneric && !candidateIsGeneric) ) {
        xl6AstFree(*selectedArgs);
        *selected = candidate;
        *selectedArgs = candidateArgs;
        *bestCost = candidateCost;
        *bestCount = 1;
        *bestIsGeneric = candidateIsGeneric;
        return;
    }

    if ( candidateCost == *bestCost ) {
        if ( !*bestIsGeneric && candidateIsGeneric ) {
            xl6AstFree(candidateArgs);
            return;
        }
        *bestCount += 1;
    }
    xl6AstFree(candidateArgs);
}

static XL6_AstNode* xl6CgenResolveCallFuncInternal(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const char* name,
    XL6_AstNode** outTypeArgs,
    int report)
{
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    XL6_AstNode* fixedArgs;
    XL6_AstNode* varargArgs;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;
    int fixedBestIsGeneric;
    int varargBestIsGeneric;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedMatch = NULL;
    varargMatch = NULL;
    fixedArgs = NULL;
    varargArgs = NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    fixedBestIsGeneric = 0;
    varargBestIsGeneric = 0;

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        int cost;
        XL6_AstNode* candidateArgs;

        if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }

        candidateArgs = NULL;
        if ( !xl6CgenFuncMatchesCallEx(ctx, node, args, &cost, &candidateArgs) ) {
            xl6AstFree(candidateArgs);
            continue;
        }

        if ( xl6CgenFuncHasVararg(node) ) {
            xl6CgenSelectCallCandidate(&varargMatch, &varargArgs,
                                       &varargBestCount, &varargBestCost, &varargBestIsGeneric,
                                       node, candidateArgs, cost);
        } else {
            xl6CgenSelectCallCandidate(&fixedMatch, &fixedArgs,
                                       &fixedBestCount, &fixedBestCost, &fixedBestIsGeneric,
                                       node, candidateArgs, cost);
        }
    }

    if ( fixedBestCount == 1 ) {
        xl6AstFree(varargArgs);
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = fixedArgs;
        } else {
            xl6AstFree(fixedArgs);
        }
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6AstFree(fixedArgs);
        xl6AstFree(varargArgs);
        if ( report ) {
            xl6CgenError(ctx, call, "重载调用存在歧义，当前 C 生成无法选择唯一函数");
        }
        return NULL;
    }

    if ( varargBestCount == 1 ) {
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = varargArgs;
        } else {
            xl6AstFree(varargArgs);
        }
        return varargMatch;
    }
    xl6AstFree(fixedArgs);
    xl6AstFree(varargArgs);
    if ( varargBestCount > 1 ) {
        if ( report ) {
            xl6CgenError(ctx, call, "重载调用存在歧义，当前 C 生成无法选择唯一函数");
        }
    } else if ( report ) {
        xl6CgenError(ctx, call, "找不到匹配的重载函数");
    }
    return NULL;
}

static XL6_AstNode* xl6CgenResolveCallFuncWithTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs)
{
    return xl6CgenResolveCallFuncInternal(ctx, call, name, outTypeArgs, 1);
}

static XL6_AstNode* xl6CgenResolveExplicitGenericCallFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const char* name,
    const XL6_AstNode* typeArgs,
    int report)
{
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    if ( ctx == NULL || ctx->root == NULL || name == NULL || typeArgs == NULL ) {
        return NULL;
    }

    fixedMatch = NULL;
    varargMatch = NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        int cost;

        if ( node == NULL || node->kind != XL6_AST_FUNC || !xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }
        if ( !xl6CgenFuncMatchesExplicitGenericCall(ctx, node, args, typeArgs, &cost) ) {
            continue;
        }

        if ( xl6CgenFuncHasVararg(node) ) {
            if ( varargBestCount == 0 || cost < varargBestCost ) {
                varargMatch = node;
                varargBestCost = cost;
                varargBestCount = 1;
            } else if ( cost == varargBestCost ) {
                varargBestCount += 1;
            }
        } else {
            if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                fixedMatch = node;
                fixedBestCost = cost;
                fixedBestCount = 1;
            } else if ( cost == fixedBestCost ) {
                fixedBestCount += 1;
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6CgenError(ctx, call, "显式泛型函数调用存在重载歧义");
        }
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( report ) {
        if ( varargBestCount > 1 ) {
            xl6CgenError(ctx, call, "显式泛型函数调用存在重载歧义");
        } else {
            xl6CgenError(ctx, call, "找不到匹配的显式泛型函数重载");
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenResolveCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name)
{
    XL6_AstNode* typeArgs;
    XL6_AstNode* func;
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    typeArgs = NULL;
    func = xl6CgenResolveCallFuncInternal(ctx, call, name, &typeArgs, 1);
    if ( typeArgs != NULL ) {
        xl6AstFree(typeArgs);
        return NULL;
    }
    return func;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    if ( xl6CgenFuncOverloadCount(ctx, name) <= 1 ) {
        return xl6CgenFindFunc(ctx, name);
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedMatch = NULL;
    varargMatch = NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( node == NULL || node->kind != XL6_AST_FUNC || xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            int cost;

            if ( !xl6CgenFuncMatchesCall(ctx, node, args, &cost) ) {
                continue;
            }
            if ( xl6CgenFuncHasVararg(node) ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargMatch = node;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedMatch = node;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }
    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6CgenError(ctx, call, "重载调用存在歧义，当前 C 生成无法选择唯一函数");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( varargBestCount > 1 ) {
        xl6CgenError(ctx, call, "重载调用存在歧义，当前 C 生成无法选择唯一函数");
    } else {
        xl6CgenError(ctx, call, "找不到匹配的重载函数");
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveCallableInBody(
    XL6_CgenContext* ctx,
    const XL6_AstNode* body,
    const XL6_AstNode* call,
    XL6_AstKind kind,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* fixedMatch;
    const XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int candidateCount;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    fixedMatch = NULL;
    varargMatch = NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;

    if ( body == NULL || name == NULL ) {
        return NULL;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* params;
        int fixedCount;
        int hasVararg;

        if ( member == NULL || member->kind != kind || !xl6CTextEquals(xl6CgenCallableName(member), name) ) {
            continue;
        }
        candidateCount += 1;
        {
            int cost;

            if ( !xl6CgenFuncMatchesCall(ctx, member, args, &cost) ) {
                continue;
            }

            params = xl6CgenCallableParams(member);
            fixedCount = xl6CgenFixedParamCount(params);
            hasVararg = params != NULL && params->childCount > (size_t)fixedCount;
            if ( hasVararg ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargMatch = member;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedMatch = member;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6CgenError(ctx, call, "成员调用存在重载歧义，当前 C 生成无法选择唯一成员");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( varargBestCount > 1 ) {
        xl6CgenError(ctx, call, "成员调用存在重载歧义，当前 C 生成无法选择唯一成员");
    } else if ( candidateCount > 0 && reportNoMatch ) {
        xl6CgenError(ctx, call, "找不到匹配的成员重载");
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveClassCreate(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call)
{
    return xl6CgenResolveCallableInBody(ctx, xl6CgenClassBody(classNode), call, XL6_AST_CREATE, "create", 1);
}

static const XL6_AstNode* xl6CgenResolveClassFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* call,
    const char* name,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* method;
    XL6_AstNode* baseClass;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( classNode == NULL || name == NULL ) {
        return NULL;
    }

    method = xl6CgenResolveCallableInBody(ctx, xl6CgenClassBody(classNode), call, XL6_AST_FUNC, name, 0);
    if ( method != NULL ) {
        if ( outOwnerClass != NULL ) {
            *outOwnerClass = classNode;
        }
        return method;
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return NULL;
    }
    return xl6CgenResolveClassFunc(ctx, baseClass, call, name, outOwnerClass);
}

static const XL6_AstNode* xl6CgenResolveCustomFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_FUNC, name, reportNoMatch);
}

static const XL6_AstNode* xl6CgenResolveCustomExternFuncWithReport(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_CUSTOM_EXTERN_FUNC, name, reportNoMatch);
}

static const XL6_AstNode* xl6CgenResolveCustomExternFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name)
{
    return xl6CgenResolveCustomExternFuncWithReport(ctx, customNode, call, name, 1);
}

static const XL6_AstNode* xl6CgenResolveCustomCreate(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_CUSTOM_EXTERN_SPECIAL, "create", 1);
}

static int xl6CgenCallIsCustomCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outCustom)
{
    const XL6_AstNode* callee;
    XL6_AstNode* customNode;

    if ( outCustom != NULL ) {
        *outCustom = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    customNode = xl6CgenFindCustom(ctx, callee->text);
    if ( customNode == NULL ) {
        return 0;
    }
    if ( outCustom != NULL ) {
        *outCustom = customNode;
    }
    return 1;
}

static int xl6CgenCallIsClassCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outClass)
{
    const XL6_AstNode* callee;
    XL6_AstNode* classNode;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    classNode = xl6CgenFindClass(ctx, callee->text);
    if ( classNode == NULL ) {
        return 0;
    }
    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    return 1;
}

static int xl6CgenClassBasePath(XL6_CgenContext* ctx, const XL6_AstNode* fromClass, const XL6_AstNode* targetClass, char* buffer, size_t capacity)
{
    XL6_AstNode* baseClass;

    if ( buffer == NULL || capacity == 0 || fromClass == NULL || targetClass == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(fromClass->text, targetClass->text) ) {
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, fromClass);
    if ( baseClass == NULL ) {
        return 0;
    }
    xl6CgenAppendText(buffer, capacity, ".base");
    if ( xl6CTextEquals(baseClass->text, targetClass->text) ) {
        return 1;
    }
    return xl6CgenClassBasePath(ctx, baseClass, targetClass, buffer, capacity);
}

static void xl6CgenCustomCtorValue(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call)
{
    const XL6_AstNode* createNode;
    const XL6_AstNode* args;

    args = call->childCount > 1 ? call->children[1] : NULL;
    createNode = xl6CgenResolveCustomCreate(ctx, customNode, call);

    if ( createNode != NULL ) {
        xl6CgenCStringSymbol(ctx->out, createNode->text);
        fputc('(', ctx->out);
        xl6CgenKnownCallArgs(ctx, createNode, args);
        fputc(')', ctx->out);
        return;
    }

    if ( args != NULL && args->childCount == 1 && args->children[0] != NULL && args->children[0]->childCount > 0 ) {
        xl6CgenExpr(ctx, args->children[0]->children[0]);
        return;
    }

    xl6CgenError(ctx, call, "custom 没有 create 时只能用一个 point 参数绑定已有句柄");
    fputs("0", ctx->out);
}

static void xl6CgenCustomCtorAssign(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const char* varName, const XL6_AstNode* call)
{
    xl6CgenIndent(ctx);
    xl6CgenFputsCIdent(ctx->out, varName);
    fputs(".handle = ", ctx->out);
    xl6CgenCustomCtorValue(ctx, customNode, call);
    fputs(";\n", ctx->out);
}

static void xl6CgenDefaultValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        fputs("0", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
         xl6CTextEquals(typeNode->text, "float64") ) {
        fputs("0.0", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        fputs("\"\"", ctx->out);
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        fputs("{0}", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "array") ) {
        fputs("xvoCreateArray()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "list") ) {
        fputs("xvoCreateList()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "set") ) {
        fputs("xvoCreateColl()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "dict") ) {
        fputs("xvoCreateTable()", ctx->out);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        fputs("xvoCreateNull()", ctx->out);
        return;
    }
    fputs("0", ctx->out);
}

static int xl6CgenIsCurrentCustomSelfReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    return ctx != NULL &&
           ctx->currentCustom != NULL &&
           receiver != NULL &&
           receiver->kind == XL6_AST_NAME &&
           xl6CTextEquals(receiver->text, "self");
}

static void xl6CgenCustomReceiverHandleExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentCustomSelfReceiver(ctx, receiver) ) {
        fputs("self->handle", ctx->out);
        return;
    }
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        fputs(".handle", ctx->out);
        return;
    }

    xl6CgenExpr(ctx, receiver);
    fputs(".handle", ctx->out);
}

static void xl6CgenCustomReceiverPtrExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentCustomSelfReceiver(ctx, receiver) ) {
        fputs("self", ctx->out);
        return;
    }

    fputc('&', ctx->out);
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenFputsCIdent(ctx->out, receiver->text);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
}

static void xl6CgenCustomMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* customNode;
    const XL6_AstNode* helperMethod;
    const XL6_AstNode* externMethod;
    const XL6_AstNode* method;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    const char* receiverName;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER) ) {
        xl6CgenError(ctx, member, "custom 方法调用接收者必须是变量名或 class 字段");
        fputs("0", ctx->out);
        return;
    }

    receiverName = receiver->kind == XL6_AST_NAME ? receiver->text : NULL;
    receiverType = receiver->kind == XL6_AST_NAME ? xl6CgenFindVarType(ctx, receiverName) : xl6CgenMemberFieldType(ctx, receiver);
    customNode = receiverType != NULL ? xl6CgenFindCustom(ctx, receiverType->text) : NULL;
    if ( customNode == NULL ) {
        xl6CgenError(ctx, member, "找不到 custom 方法接收者类型");
        fputs("0", ctx->out);
        return;
    }

    helperMethod = xl6CgenResolveCustomFunc(ctx, customNode, call, member->text, 0);
    externMethod = xl6CgenResolveCustomExternFuncWithReport(ctx, customNode, call, member->text, 0);
    if ( helperMethod != NULL && externMethod != NULL ) {
        xl6CgenError(ctx, member, "custom 方法调用存在 helper / extern 歧义");
        fputs("0", ctx->out);
        return;
    }

    method = helperMethod != NULL ? helperMethod : externMethod;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "找不到 custom extern 方法");
        fputs("0", ctx->out);
        return;
    }

    if ( helperMethod != NULL ) {
        xl6CgenCustomCallableSymbol(ctx, customNode, method);
    } else {
        xl6CgenCStringSymbol(ctx->out, method->text);
    }
    fputc('(', ctx->out);
    if ( helperMethod != NULL ) {
        xl6CgenCustomReceiverPtrExpr(ctx, receiver);
    } else {
        xl6CgenCustomReceiverHandleExpr(ctx, receiver);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    params = xl6CgenCallableParams(method);
    if ( params != NULL && params->childCount > 0 ) {
        fputs(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    fputc(')', ctx->out);
}

static int xl6CgenClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* args;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char basePath[256];
    int fixedCount;

    receiverType = NULL;
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }

    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    ownerClass = NULL;
    method = xl6CgenResolveClassFunc(ctx, classNode, call, member->text, &ownerClass);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "找不到 class 方法");
        fputs("0", ctx->out);
        return 1;
    }
    if ( ownerClass == NULL ) {
        ownerClass = classNode;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = receiverType->children[0];
        xl6CgenGenericClassCallableSymbol(ctx, ownerClass, receiverType, method);
    } else {
        xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    }
    fputc('(', ctx->out);
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, member, "无法生成继承方法调用的 base 路径");
        basePath[0] = '\0';
    }
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( basePath[0] == '\0' ) {
            fputs("self", ctx->out);
        } else {
            fputs("&self->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                fputs(basePath + 5, ctx->out);
            }
        }
    } else {
        fputc('&', ctx->out);
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        fputs(basePath, ctx->out);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(method->childCount > 1 ? method->children[1] : NULL);
    if ( fixedCount > 0 ) {
        fputs(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    fputc(')', ctx->out);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return 1;
}

static XL6_AstNode* xl6CgenBaseClass(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;

    if ( classNode == NULL ) {
        return NULL;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, base->text);
}

static void xl6CgenEmitClassCreateCall(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* createNode, const char* selfExpr, const XL6_AstNode* args)
{
    int fixedCount;

    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
    fputc('(', ctx->out);
    fputs(selfExpr != NULL ? selfExpr : "self", ctx->out);
    fixedCount = xl6CgenFixedParamCount(createNode != NULL && createNode->childCount > 0 ? createNode->children[0] : NULL);
    if ( fixedCount > 0 ) {
        fputs(", ", ctx->out);
        xl6CgenParamNodeArgs(ctx, createNode->childCount > 0 ? createNode->children[0] : NULL, args);
    }
    fputc(')', ctx->out);
}

static int xl6CgenBaseCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "base") ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    if ( ctx->currentClass == NULL ) {
        xl6CgenError(ctx, call, "base(...) 只能在 class 方法中使用");
        fputs("0", ctx->out);
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, ctx->currentClass);
    if ( baseClass == NULL ) {
        xl6CgenError(ctx, call, "当前 class 没有父类，不能调用 base(...)");
        fputs("0", ctx->out);
        return 1;
    }

    createNode = xl6CgenResolveClassCreate(ctx, baseClass, call);
    if ( createNode == NULL ) {
        if ( args != NULL && args->childCount > 0 ) {
            xl6CgenError(ctx, call, "父类没有 create，base(...) 不能传参");
        }
        fputs("((void)0)", ctx->out);
        return 1;
    }

    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, "&self->base", args);
    return 1;
}

static int xl6CgenClassHasDirectField(const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || fieldName == NULL || classNode->childCount < 2 ) {
        return 0;
    }
    body = classNode->children[1];
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* item = body->children[i];
        if ( item != NULL && item->kind == XL6_AST_FIELD && xl6CTextEquals(item->text, fieldName) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenEmitClassFieldSuffixWithSep(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const char* sep)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL || fieldName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDirectField(classNode, fieldName) ) {
        fputs(sep != NULL ? sep : ".", ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        return 1;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL ) {
        return 0;
    }

    fputs(sep != NULL ? sep : ".", ctx->out);
    fputs("base", ctx->out);
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, baseClass, fieldName, ".");
}

static int xl6CgenEmitClassFieldSuffix(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, classNode, fieldName, ".");
}

static const XL6_AstNode* xl6CgenMemberFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* field;
    XL6_AstNode* classNode;

    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return NULL;
    }

    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
}

static void xl6CgenClassFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    XL6_AstNode* classNode;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        xl6CgenError(ctx, member, "class 字段访问接收者必须是变量名");
        fputs("0", ctx->out);
        return;
    }

    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        fputs("self", ctx->out);
        if ( !xl6CgenEmitClassFieldSuffixWithSep(ctx, ctx->currentClass, member->text, "->") ) {
            xl6CgenError(ctx, member, "找不到 class 字段");
        }
        return;
    }

    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    if ( classNode == NULL ) {
        xl6CgenError(ctx, member, "找不到 class 字段接收者类型");
        fputs("0", ctx->out);
        return;
    }

    xl6CgenFputsCIdent(ctx->out, receiver->text);
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
        xl6CgenError(ctx, member, "找不到 class 字段");
    }
}

static int xl6CgenXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsXValueType(receiverType) || !xl6CTextEquals(member->text, "length") ) {
        return 0;
    }
    if ( xl6CgenIsDictXValueType(receiverType) ) {
        fputs("xvoTableItemCount(", ctx->out);
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        fputc(')', ctx->out);
    } else {
        fputs("xvoArrayItemCount(", ctx->out);
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        fputc(')', ctx->out);
    }
    return 1;
}

static int xl6CgenSafeXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || !xl6CTextEquals(member->text, "length") ||
         member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    fputs("xl6_xrt_member_safe_length(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    fputc(')', ctx->out);
    xl6AstFree(receiverType);
    return 1;
}

static void xl6CgenIndexAssignTemp(XL6_CgenContext* ctx, const XL6_AstNode* left, const char* tempName)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    index = left != NULL && left->childCount > 1 ? left->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    xl6CgenIndent(ctx);
    fputs("if ( !", ctx->out);
    if ( xl6CgenIsArrayXValueType(receiverType) ) {
        fputs("xvoArraySetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        fputs(", (unsigned int)(", ctx->out);
        xl6CgenExpr(ctx, index);
        fprintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        fputs("xvoListSetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        fputs(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        fprintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenError(ctx, index, "dict 索引键必须是 string");
            fprintf(ctx->out, "false");
        } else {
            fputs("xvoTableSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            fputs(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            fprintf(ctx->out, ", 0, %s, true)", tempName != NULL ? tempName : "0");
        }
    } else {
        xl6CgenError(ctx, left, "当前 C 生成只支持 array/list/dict 索引赋值");
        fputs("false", ctx->out);
    }
    fprintf(ctx->out, " && %s != 0 ) {\n", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);

    xl6AstFree(receiverType);
}

static void xl6CgenIndexAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    int id;
    char tempName[64];

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_index_assign_%d", id);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xvalue %s = ", tempName);
    xl6CgenXValueFromExpr(ctx, right);
    fputs(";\n", ctx->out);
    xl6CgenIndexAssignTemp(ctx, left, tempName);
}

static const char* xl6CgenCompoundBinaryOp(const char* op)
{
    if ( xl6CTextEquals(op, "+=") ) {
        return "+";
    }
    if ( xl6CTextEquals(op, "-=") ) {
        return "-";
    }
    if ( xl6CTextEquals(op, "*=") ) {
        return "*";
    }
    if ( xl6CTextEquals(op, "/=") ) {
        return "/";
    }
    if ( xl6CTextEquals(op, "%=") ) {
        return "%";
    }
    return NULL;
}

static int xl6CgenExprTypeIsFloat(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text);
    xl6AstFree(typeNode);
    return result;
}

static void xl6CgenXValueAsI64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        fputs("xvoGetInt(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text) ) {
        fputs("(long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputc(')', ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
    xl6AstFree(typeNode);
}

static void xl6CgenXValueAsF64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        fputs("xl6_xrt_value_to_double(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs(", xvoType(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs("))", ctx->out);
    } else {
        fputs("(double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputc(')', ctx->out);
    }
    xl6AstFree(typeNode);
}

static void xl6CgenIndexCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    int id;
    int rightIsFloat;
    const char* binaryOp;
    char currentName[64];
    char rightName[64];
    char typeName[64];
    char tempName[64];

    if ( xl6CTextEquals(op, "=") ) {
        xl6CgenIndexAssignStmt(ctx, left, right);
        return;
    }

    id = ctx->tempId++;
    snprintf(currentName, sizeof(currentName), "__xl6_index_current_%d", id);
    snprintf(rightName, sizeof(rightName), "__xl6_index_right_%d", id);
    snprintf(typeName, sizeof(typeName), "__xl6_index_type_%d", id);
    snprintf(tempName, sizeof(tempName), "__xl6_index_assign_%d", id);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xvalue %s = ", currentName);
    xl6CgenIndexExpr(ctx, left);
    fputs(";\n", ctx->out);

    if ( xl6CTextEquals(op, "?=") ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (xvoIsNull(%s)) {\n", currentName);
        ctx->indent += 1;
        xl6CgenIndexAssignStmt(ctx, left, right);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        return;
    }

    if ( xl6CTextEquals(op, "**=") ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenXValueAsI64Expr(ctx, right);
        fputs(");\n", ctx->out);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xvoCreateInt(xl6_int_pow_i64(xvoGetInt(%s), %s));\n",
                tempName, currentName, rightName);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        return;
    }

    binaryOp = xl6CgenCompoundBinaryOp(op);
    if ( binaryOp == NULL ) {
        xl6CgenError(ctx, left, "当前 C 生成不支持该索引复合赋值运算符");
        return;
    }

    rightIsFloat = xl6CgenExprTypeIsFloat(ctx, right);
    if ( rightIsFloat ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "double %s = ", rightName);
        xl6CgenXValueAsF64Expr(ctx, right);
        fputs(";\n", ctx->out);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "int %s = xvoType(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        fprintf(ctx->out,
                "xvalue %s = xvoCreateFloat(xl6_xrt_value_to_double(%s, %s) %s %s);\n",
                tempName, currentName, typeName, binaryOp, rightName);
    } else {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenXValueAsI64Expr(ctx, right);
        fputs(");\n", ctx->out);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "int %s = xvoType(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = 0;\n", tempName);
        if ( xl6CTextEquals(op, "%=") ) {
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "%s = xvoCreateInt(xvoGetInt(%s) %% %s);\n",
                    tempName, currentName, rightName);
        } else {
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "if (%s == XL6_XVO_DT_FLOAT) {\n", typeName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            fprintf(ctx->out,
                    "%s = xvoCreateFloat(xl6_xrt_value_to_double(%s, %s) %s (double)%s);\n",
                    tempName, currentName, typeName, binaryOp, rightName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputs("} else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "%s = xvoCreateInt(xvoGetInt(%s) %s %s);\n",
                    tempName, currentName, binaryOp, rightName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputs("}\n", ctx->out);
        }
    }

    xl6CgenIndexAssignTemp(ctx, left, tempName);
}

static void xl6CgenAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* op;

    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "=";

    if ( left == NULL || (left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER && left->kind != XL6_AST_INDEX) ) {
        xl6CgenError(ctx, node, "当前 C 生成暂只支持变量名、class 字段或容器索引赋值");
        return;
    }
    if ( left->kind == XL6_AST_INDEX ) {
        xl6CgenIndexCompoundAssignStmt(ctx, left, op, right);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* ctorCustom;
        const XL6_AstNode* fieldType;
        XL6_AstNode* fieldCustom;

        ctorCustom = NULL;
        if ( xl6CgenCallIsCustomCtor(ctx, right, &ctorCustom) ) {
            fieldType = xl6CgenMemberFieldType(ctx, left);
            fieldCustom = fieldType != NULL ? xl6CgenFindCustom(ctx, fieldType->text) : NULL;
            if ( fieldCustom == NULL ) {
                xl6CgenError(ctx, left, "custom 构造只能赋给同类型 custom 字段");
                return;
            }
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, right, "custom 字段只能使用同类型构造表达式赋值");
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            fputs(".handle = ", ctx->out);
            xl6CgenCustomCtorValue(ctx, ctorCustom, right);
            fputs(";\n", ctx->out);
            return;
        }
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsFunctionType(xl6CgenFindVarType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, left->text);

        xl6CgenCIdentText(cName, sizeof(cName), left->text);
        snprintf(tempName, sizeof(tempName), "__xl6_function_assign_%d", id);

        xl6CgenIndent(ctx);
        fprintf(ctx->out, "XL6_Function %s = ", tempName);
        xl6CgenFunctionValueExpr(ctx, right, targetType);
        fputs(";\n", ctx->out);
        if ( xl6CgenExprIsFunctionBorrow(ctx, right) ) {
            xl6CgenRetainFunctionValue(ctx, tempName);
        }
        xl6CgenReleaseFunctionValue(ctx, left->text);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s = %s;\n", cName, tempName);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsXValueType(xl6CgenFindVarType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];

        xl6CgenCIdentText(cName, sizeof(cName), left->text);

        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
        xl6CgenXValueFromExpr(ctx, right);
        fputs(";\n", ctx->out);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (%s != 0) {\n", cName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvoUnref(%s);\n", cName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
        return;
    }

    xl6CgenIndent(ctx);
    if ( xl6CTextEquals(op, "?=") ) {
        if ( left->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenFindVarType(ctx, left->text)) ) {
            int id = ctx->tempId++;
            char cName[512];

            xl6CgenCIdentText(cName, sizeof(cName), left->text);
            fprintf(ctx->out, "if (xvoIsNull(%s)) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
            xl6CgenXValueFromExpr(ctx, right);
            fputs(";\n", ctx->out);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "if (%s != 0) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "xvoUnref(%s);\n", cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputs("}\n", ctx->out);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputs("}\n", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, left);
        fputs(" = (", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(" == 0 ? ", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(" : ", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(");\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(op, "+=") &&
         xl6CgenExprStaticTypeIs(ctx, left, "string") &&
         xl6CgenExprStaticTypeIs(ctx, right, "string") ) {
        xl6CgenExpr(ctx, left);
        fputs(" = xl6_xrt_str_concat(", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(");\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(op, "**=") ) {
        if ( !xl6CgenExprIntPowKind(ctx, left) || !xl6CgenExprIntPowKind(ctx, right) ) {
            xl6CgenError(ctx, node, "当前 C 生成 **= 只支持整数表达式");
            return;
        }
        xl6CgenExpr(ctx, left);
        fputs(" = xl6_int_pow_i64(", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(");\n", ctx->out);
        return;
    }

    xl6CgenExpr(ctx, left);
    fprintf(ctx->out, " %s ", op);
    xl6CgenExpr(ctx, right);
    fputs(";\n", ctx->out);
}

static void xl6CgenTimeLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_TimeLiteralParts parts;

    if ( !xl6TimeLiteralParse(node != NULL ? node->text : NULL, &parts) ) {
        xl6CgenError(ctx, node, "time 字面量格式必须是 #yyyy-mm-dd#、#hh:mm:ss# 或 #yyyy-mm-dd hh:mm:ss#");
        fputs("0", ctx->out);
        return;
    }

    if ( parts.kind == XL6_TIME_LITERAL_DATE ) {
        fprintf(ctx->out, "xrtDateSerial(%d, %d, %d)", parts.year, parts.month, parts.day);
        return;
    }
    if ( parts.kind == XL6_TIME_LITERAL_TIME ) {
        fprintf(ctx->out, "xrtTimeSerial(%d, %d, %d)", parts.hour, parts.minute, parts.second);
        return;
    }
    fprintf(ctx->out, "xrtDateTimeSerial(%d, %d, %d, %d, %d, %d)",
            parts.year, parts.month, parts.day, parts.hour, parts.minute, parts.second);
}

static void xl6CgenIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    index = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            fputs("xl6_xrt_index_safe_get_key(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            fputs(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            fputc(')', ctx->out);
        } else if ( receiverType != NULL &&
                    (xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) ||
                     xl6CTextEquals(receiverType->text, "var")) ) {
            fputs("xl6_xrt_index_safe_get_i64(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            fputs(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            fputs("))", ctx->out);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成安全索引访问只支持 array/list/dict/var");
            fputs("xvoCreateNull()", ctx->out);
        }
    } else if ( xl6CgenIsArrayXValueType(receiverType) ) {
        fputs("xvoArrayGetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        fputs(", (unsigned int)(", ctx->out);
        xl6CgenExpr(ctx, index);
        fputs("))", ctx->out);
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        fputs("xvoListGetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        fputs(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        fputs("))", ctx->out);
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenError(ctx, index, "dict 索引键必须是 string");
            fputs("xvoCreateNull()", ctx->out);
        } else {
            fputs("xvoTableGetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            fputs(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            fputs(", 0)", ctx->out);
        }
    } else {
        xl6CgenError(ctx, node, "当前 C 生成索引访问只支持 array、list 和 dict");
        fputs("xvoCreateNull()", ctx->out);
    }

    xl6AstFree(receiverType);
}

static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL ) {
        fputs("0", ctx->out);
        return;
    }

    switch ( node->kind ) {
    case XL6_AST_NAME:
        if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, node->text) >= 0 ) {
            fputs("__env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, node->text);
            break;
        }
        if ( xl6CgenFindTrackedVarType(ctx, node->text) == NULL ) {
            const XL6_AstNode* externalVar = xl6CgenFindExternalVar(ctx, node->text);
            if ( externalVar != NULL ) {
                xl6CgenCStringSymbol(ctx->out, externalVar->text);
                break;
            }
        }
        if ( xl6CgenFindVarType(ctx, node->text) == NULL && xl6CgenIsKnownTypeName(ctx, node->text) ) {
            xl6CgenEmitTypeIdForName(ctx, node->text, node);
            break;
        }
        xl6CgenFputsCIdent(ctx->out, node->text);
        break;
    case XL6_AST_INT:
        xl6CgenIntLiteralExpr(ctx, node);
        break;
    case XL6_AST_FLOAT:
        xl6CgenFloatLiteralExpr(ctx, node);
        break;
    case XL6_AST_STRING:
        xl6CgenStringLiteralExpr(ctx, node);
        break;
    case XL6_AST_TIME:
        xl6CgenTimeLiteralExpr(ctx, node);
        break;
    case XL6_AST_BOOL:
        fputs(xl6CTextEquals(node->text, "true") ? "1" : "0", ctx->out);
        break;
    case XL6_AST_NULL:
        fputs("0", ctx->out);
        break;
    case XL6_AST_ARRAY_LITERAL:
        xl6CgenArrayLiteralExpr(ctx, node);
        break;
    case XL6_AST_LIST_LITERAL:
        xl6CgenListLiteralExpr(ctx, node);
        break;
    case XL6_AST_SET_LITERAL:
        xl6CgenSetLiteralExpr(ctx, node);
        break;
    case XL6_AST_DICT_LITERAL:
        xl6CgenDictLiteralExpr(ctx, node);
        break;
    case XL6_AST_TYPED_LITERAL:
        xl6CgenTypedLiteralExpr(ctx, node);
        break;
    case XL6_AST_ANON_FUNC:
        xl6CgenAnonMakeCall(ctx, node);
        break;
    case XL6_AST_MEMBER:
        if ( !xl6CgenXValueMemberAccess(ctx, node) ) {
            xl6CgenClassFieldAccess(ctx, node);
        }
        break;
    case XL6_AST_SAFE_MEMBER:
        if ( !xl6CgenSafeXValueMemberAccess(ctx, node) ) {
            xl6CgenError(ctx, node, "当前 C 生成安全成员访问只支持容器 length");
            fputs("0", ctx->out);
        }
        break;
    case XL6_AST_INDEX:
        xl6CgenIndexExpr(ctx, node);
        break;
    case XL6_AST_SAFE_INDEX:
        xl6CgenIndexExpr(ctx, node);
        break;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(node->text, "~=") ) {
            xl6CgenApproxExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryCompareIsTypeValue(ctx, node) ) {
            xl6CgenTypeCompareExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryCompareIsString(ctx, node) ) {
            xl6CgenStringCompareExpr(ctx, node);
            break;
        }
        if ( xl6CTextEquals(node->text, "**") ) {
            xl6CgenPowExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryPlusIsString(ctx, node) ) {
            xl6CgenStringConcatExpr(ctx, node);
            break;
        }
        if ( xl6CTextEquals(node->text, "in") ) {
            xl6CgenInExpr(ctx, node);
            break;
        }
        if ( xl6CTextEquals(node->text, "??") ) {
            xl6CgenNullCoalesceExpr(ctx, node);
            break;
        }
        fputc('(', ctx->out);
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        if ( xl6CTextEquals(node->text, "and") ) {
            fputs(" && ", ctx->out);
        } else if ( xl6CTextEquals(node->text, "or") ) {
            fputs(" || ", ctx->out);
        } else {
            fprintf(ctx->out, " %s ", node->text != NULL ? node->text : "?");
        }
        if ( node->childCount > 1 ) {
            xl6CgenExpr(ctx, node->children[1]);
        }
        fputc(')', ctx->out);
        break;
    case XL6_AST_UNARY:
        if ( xl6CTextEquals(node->text, "not") ) {
            fputs("(!", ctx->out);
        } else {
            fprintf(ctx->out, "(%s", node->text != NULL ? node->text : "!");
        }
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        fputc(')', ctx->out);
        break;
    case XL6_AST_CONDITIONAL:
        fputc('(', ctx->out);
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        fputs(" ? ", ctx->out);
        if ( node->childCount > 1 ) {
            xl6CgenExpr(ctx, node->children[1]);
        }
        fputs(" : ", ctx->out);
        if ( node->childCount > 2 ) {
            xl6CgenExpr(ctx, node->children[2]);
        }
        fputc(')', ctx->out);
        break;
    case XL6_AST_CALL:
        if ( node->childCount > 0 ) {
            const XL6_AstNode* callee = node->children[0];
            XL6_AstNode* knownFunc = NULL;
            if ( callee->kind == XL6_AST_MEMBER ) {
                if ( !xl6CgenClassMethodCall(ctx, node, callee) ) {
                    xl6CgenCustomMethodCall(ctx, node, callee);
                }
                break;
            }
            if ( xl6CgenBaseCtorCall(ctx, node) ) {
                break;
            }
            if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
                const XL6_AstNode* instantName = callee->childCount > 0 ? callee->children[0] : NULL;
                const XL6_AstNode* typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
                const XL6_AstNode* prevParams = ctx->currentGenericParams;
                const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                if ( instantName == NULL || instantName->kind != XL6_AST_NAME ) {
                    xl6CgenError(ctx, callee, "当前 C 生成只支持函数名形式的泛型实例化");
                    fputs("0", ctx->out);
                    break;
                }

                knownFunc = xl6CgenResolveExplicitGenericCallFunc(ctx, node, instantName->text, typeArgs, 1);
                if ( knownFunc == NULL ) {
                    xl6CgenError(ctx, callee, "找不到匹配的泛型函数声明");
                    fputs("0", ctx->out);
                    break;
                }

                xl6CgenAddGenericInstance(ctx, knownFunc, typeArgs);
                ctx->currentGenericParams = xl6CgenFuncTypeParams(knownFunc);
                ctx->currentGenericArgs = typeArgs;
                xl6CgenFuncSymbol(ctx, knownFunc);
                fputc('(', ctx->out);
                if ( node->childCount > 1 ) {
                    xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                }
                fputc(')', ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
                break;
            }
            if ( callee->kind == XL6_AST_NAME ) {
                XL6_AstNode* externFunc = xl6CgenFindExternFunc(ctx, callee->text);
                XL6_AstNode* exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
                const XL6_AstNode* functionVarType = xl6CgenFindVarType(ctx, callee->text);
                if ( xl6CTextEquals(callee->text, "type") ) {
                    const XL6_AstNode* args = node->childCount > 1 ? node->children[1] : NULL;
                    const XL6_AstNode* arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
                    xl6CgenEmitTypeOfExpr(ctx, arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL);
                    break;
                }
                if ( xl6CgenIsBuiltinConvertTypeName(callee->text) && xl6CgenIsKnownTypeName(ctx, callee->text) ) {
                    xl6CgenTypeConvertExpr(ctx, node);
                    break;
                }
                if ( xl6CgenIsFunctionType(functionVarType) ) {
                    xl6CgenFunctionInvoke(ctx, callee->text, functionVarType, node->childCount > 1 ? node->children[1] : NULL);
                    break;
                }
                if ( externFunc != NULL ) {
                    knownFunc = externFunc;
                    xl6CgenCStringSymbol(ctx->out, externFunc->text);
                } else if ( exportFunc != NULL ) {
                    knownFunc = exportFunc;
                    xl6CgenExportInternalSymbol(ctx->out, exportFunc);
                } else {
                    XL6_AstNode* inferredArgs = NULL;
                    knownFunc = xl6CgenResolveCallFuncWithTypeArgs(ctx, node, callee->text, &inferredArgs);
                    if ( knownFunc != NULL && inferredArgs != NULL ) {
                        const XL6_AstNode* prevParams = ctx->currentGenericParams;
                        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                        ctx->currentGenericParams = xl6CgenFuncTypeParams(knownFunc);
                        ctx->currentGenericArgs = inferredArgs;
                        xl6CgenFuncSymbol(ctx, knownFunc);
                        fputc('(', ctx->out);
                        if ( node->childCount > 1 ) {
                            xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                        }
                        fputc(')', ctx->out);
                        ctx->currentGenericParams = prevParams;
                        ctx->currentGenericArgs = prevArgs;
                        xl6CgenAddOwnedGenericInstance(ctx, knownFunc, inferredArgs);
                        break;
                    }
                    xl6AstFree(inferredArgs);
                    if ( knownFunc != NULL ) {
                        xl6CgenFuncSymbol(ctx, knownFunc);
                    } else {
                        xl6CgenExpr(ctx, callee);
                    }
                }
            } else {
                xl6CgenExpr(ctx, callee);
            }
            fputc('(', ctx->out);
            if ( node->childCount > 1 ) {
                if ( knownFunc != NULL ) {
                    xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                } else {
                    xl6CgenCallArgs(ctx, node->children[1]);
                }
            }
            fputc(')', ctx->out);
            break;
        }
        fputs("()", ctx->out);
        break;
    default:
        xl6CgenError(ctx, node, "当前 C 生成暂不支持该表达式");
        fputs("0", ctx->out);
        break;
    }
}

static int xl6CgenTypeNodeApproxKind(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int8") ||
         xl6CTextEquals(typeNode->text, "uint8") || xl6CTextEquals(typeNode->text, "int16") ||
         xl6CTextEquals(typeNode->text, "uint16") || xl6CTextEquals(typeNode->text, "int32") ||
         xl6CTextEquals(typeNode->text, "uint32") || xl6CTextEquals(typeNode->text, "int64") ||
         xl6CTextEquals(typeNode->text, "uint64") ) {
        return 1;
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
         xl6CTextEquals(typeNode->text, "float64") ) {
        return 2;
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return 3;
    }
    if ( xl6CTextEquals(typeNode->text, "time") ) {
        return 4;
    }
    return 0;
}

static int xl6CgenCTypeApproxKind(const char* cType)
{
    if ( cType == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(cType, "long long") || xl6CTextEquals(cType, "int") ||
         xl6CTextEquals(cType, "unsigned long long") || xl6CTextEquals(cType, "unsigned int") ) {
        return 1;
    }
    if ( xl6CTextEquals(cType, "double") || xl6CTextEquals(cType, "float") ) {
        return 2;
    }
    if ( xl6CTextEquals(cType, "const char*") ) {
        return 3;
    }
    return 0;
}

static int xl6CgenExprApproxKind(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return 1;
    case XL6_AST_FLOAT:
        return 2;
    case XL6_AST_STRING:
        return 3;
    case XL6_AST_TIME:
        return 4;
    case XL6_AST_NAME:
        return xl6CgenTypeNodeApproxKind(ctx, xl6CgenFindVarType(ctx, expr->text));
    case XL6_AST_MEMBER: {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        int kind = xl6CgenTypeNodeApproxKind(ctx, memberType);
        xl6AstFree(memberType);
        return kind;
    }
    case XL6_AST_CALL: {
        XL6_AstNode* callType = xl6CgenCloneExprTypeForInference(ctx, expr);
        int kind = xl6CgenTypeNodeApproxKind(ctx, callType);
        xl6AstFree(callType);
        if ( kind != 0 ) {
            return kind;
        }
        return xl6CgenCTypeApproxKind(xl6CgenInferExprType(ctx, expr));
    }
    default:
        return 0;
    }
}

static int xl6CgenExprIntPowKind(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return 1;
    case XL6_AST_NAME:
        return xl6CgenTypeNodeApproxKind(ctx, xl6CgenFindVarType(ctx, expr->text)) == 1;
    case XL6_AST_MEMBER: {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        int isInt = xl6CgenTypeNodeApproxKind(ctx, memberType) == 1;
        xl6AstFree(memberType);
        return isInt;
    }
    case XL6_AST_CALL:
        return xl6CgenCTypeApproxKind(xl6CgenInferExprType(ctx, expr)) == 1;
    case XL6_AST_UNARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
             xl6CTextEquals(expr->text, "~") ) {
            return expr->childCount > 0 && xl6CgenExprIntPowKind(ctx, expr->children[0]);
        }
        return 0;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
             xl6CTextEquals(expr->text, "*") || xl6CTextEquals(expr->text, "/") ||
             xl6CTextEquals(expr->text, "%") || xl6CTextEquals(expr->text, "**") ||
             xl6CTextEquals(expr->text, "<<") || xl6CTextEquals(expr->text, ">>") ||
             xl6CTextEquals(expr->text, "&") || xl6CTextEquals(expr->text, "|") ||
             xl6CTextEquals(expr->text, "^") ) {
            return expr->childCount > 1 &&
                   xl6CgenExprIntPowKind(ctx, expr->children[0]) &&
                   xl6CgenExprIntPowKind(ctx, expr->children[1]);
        }
        return 0;
    default:
        return 0;
    }
}

static int xl6CgenAnonCaptureIndex(const XL6_CgenAnonFunc* info, const char* name)
{
    size_t i;

    if ( info == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( xl6CTextEquals(info->captures[i].name, name) ) {
            return (int)i;
        }
    }
    return -1;
}

static void xl6CgenRegisterAnonCaptureTypes(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    size_t i;

    if ( ctx == NULL || info == NULL ) {
        return;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        xl6CgenAddVarType(ctx, info->captures[i].name, info->captures[i].type);
    }
}

static void xl6CgenPowExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( !xl6CgenExprIntPowKind(ctx, left) || !xl6CgenExprIntPowKind(ctx, right) ) {
        xl6CgenError(ctx, node, "当前 C 生成 ** 只支持整数表达式");
        fputs("0", ctx->out);
        return;
    }

    fputs("xl6_int_pow_i64(", ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    fputc(')', ctx->out);
}

static void xl6CgenStringConcatExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    fputs("xl6_xrt_str_concat(", ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    fputc(')', ctx->out);
}

static void xl6CgenStringCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* op;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    op = node != NULL && node->text != NULL ? node->text : "==";

    fputs("(xrtStrComp(", ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    fputs(", 0, false) ", ctx->out);
    fputs(op, ctx->out);
    fputs(" 0)", ctx->out);
}

static void xl6CgenApproxExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int leftKind;
    int rightKind;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    leftKind = xl6CgenExprApproxKind(ctx, left);
    rightKind = xl6CgenExprApproxKind(ctx, right);

    if ( leftKind == 3 && rightKind == 3 ) {
        fputs("xrtStrApprox(", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(", 0, ", ctx->out);
        xl6CgenExpr(ctx, right);
        fputs(", 0)", ctx->out);
        return;
    }

    if ( leftKind == 4 && rightKind == 4 ) {
        fputs("xrtTimeApprox(", ctx->out);
        xl6CgenExpr(ctx, left);
        fputs(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        fputc(')', ctx->out);
        return;
    }

    if ( leftKind == 0 || rightKind == 0 || leftKind == 3 || rightKind == 3 ||
         leftKind == 4 || rightKind == 4 ) {
        xl6CgenError(ctx, node, "当前 C 生成 ~= 只支持整数、浮点、字符串和时间表达式");
        fputs("0", ctx->out);
        return;
    }

    fputs((leftKind == 2 || rightKind == 2) ? "xrtNumApprox(" : "xrtIntApprox(", ctx->out);
    xl6CgenExpr(ctx, left);
    fputs(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    fputc(')', ctx->out);
}

static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);

static void xl6CgenBlock(XL6_CgenContext* ctx, const XL6_AstNode* block)
{
    size_t i;
    size_t cleanupMark;

    if ( block == NULL ) {
        return;
    }
    cleanupMark = ctx->cleanupCount;
    for ( i = 0; i < block->childCount; ++i ) {
        xl6CgenStmt(ctx, block->children[i]);
    }
    xl6CgenCloseScope(ctx, cleanupMark);
}

static void xl6CgenDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* cType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;
    XL6_AstNode* ctorCustom;
    XL6_AstNode* declCustom;
    XL6_AstNode* ctorClass;
    XL6_AstNode* declClass;
    XL6_AstNode* ctorClassType;
    XL6_AstNode* declClassType;
    char cName[512];

    xl6CgenIndent(ctx);
    typeNode = node->childCount > 0 ? node->children[0] : NULL;
    initExpr = node->childCount > 1 ? node->children[1] : NULL;
    xl6CgenCIdentText(cName, sizeof(cName), node->text);
    ctorCustom = NULL;
    declCustom = NULL;
    ctorClass = NULL;
    declClass = NULL;
    ctorClassType = NULL;
    declClassType = NULL;

    if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && initExpr != NULL ) {
        xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom);
        if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
            ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
        }
    } else if ( xl6CgenIsCustomType(ctx, typeNode) ) {
        declCustom = xl6CgenFindCustom(ctx, typeNode->text);
    } else if ( xl6CgenIsClassType(ctx, typeNode) ) {
        declClass = xl6CgenFindClass(ctx, typeNode->text);
        if ( initExpr != NULL && typeNode != NULL && typeNode->childCount > 0 ) {
            declClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
            if ( declClassType == NULL ) {
                xl6CgenError(ctx, typeNode, "泛型 class 声明类型规范化失败");
            } else if ( ctorClassType != NULL && !xl6CgenTypeNodeEqual(declClassType, ctorClassType) ) {
                xl6CgenError(ctx, initExpr, "泛型 class 变量只能使用同类型构造表达式初始化");
            }
        } else if ( typeNode != NULL && typeNode->childCount > 0 ) {
            declClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            if ( declClassType == NULL ) {
                xl6CgenError(ctx, typeNode, "泛型 class 声明类型规范化失败");
            }
        }
    }

    if ( ctorCustom != NULL || declCustom != NULL ) {
        const XL6_AstNode* customNode = ctorCustom != NULL ? ctorCustom : declCustom;
        xl6CgenCustomTypeName(ctx->out, customNode->text);
        fprintf(ctx->out, " %s = {0};\n", cName);
        xl6CgenAddVarType(ctx, node->text, customNode);
        xl6CgenAddCustomCleanup(ctx, node->text, customNode);
        if ( initExpr != NULL ) {
            if ( xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom) && xl6CTextEquals(ctorCustom->text, customNode->text) ) {
                xl6CgenCustomCtorAssign(ctx, customNode, node->text, initExpr);
            } else {
                XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
                const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);

                /*
                    无 destroy 的 custom 本质只是句柄值，可以按结构体值复制。
                    有 destroy 的 custom 已经由语义层判为不可复制，这里仍保持构造表达式专用路径。
                */
                if ( resolvedInitType != NULL &&
                     xl6CTextEquals(resolvedInitType->text, customNode->text) &&
                     !xl6CgenCustomHasDestroy(customNode) ) {
                    xl6CgenIndent(ctx);
                    fprintf(ctx->out, "%s = ", cName);
                    xl6CgenExpr(ctx, initExpr);
                    fputs(";\n", ctx->out);
                } else {
                    xl6CgenError(ctx, initExpr, "custom 变量只能使用同类型构造表达式或可复制同类型值初始化");
                }
                xl6AstFree(initType);
            }
        }
        return;
    }

    if ( ctorClass != NULL || declClass != NULL ) {
        const XL6_AstNode* classNode = declClass != NULL ? declClass : ctorClass;
        const XL6_AstNode* classTypeNode = (declClass != NULL && declClassType != NULL) ? declClassType : (ctorClassType != NULL ? ctorClassType : typeNode);
        int isGenericClass = (ctorClassType != NULL) || (declClassType != NULL);
        const XL6_AstNode* args = initExpr != NULL && initExpr->childCount > 1 ? initExpr->children[1] : NULL;
        const XL6_AstNode* createNode = NULL;
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        if ( ctorClassType != NULL ) {
            xl6CgenOwnTypeNode(ctx, ctorClassType);
        }
        if ( declClassType != NULL ) {
            xl6CgenOwnTypeNode(ctx, declClassType);
        }
        if ( isGenericClass ) {
            ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
            ctx->currentGenericArgs = classTypeNode->children[0];
            fputs(xl6CgenTypeName(ctx, classTypeNode), ctx->out);
        } else {
            xl6CgenClassTypeName(ctx->out, classNode->text);
        }
        createNode = (initExpr != NULL && ctorClass != NULL) ?
                     xl6CgenResolveClassCreate(ctx, classNode, initExpr) :
                     xl6CgenFindClassSpecial(classNode, "create");
        fprintf(ctx->out, " %s = {0};\n", cName);
        xl6CgenAddVarType(ctx, node->text, isGenericClass ? classTypeNode : classNode);
        xl6CgenAddClassCleanup(ctx, node->text, classNode, isGenericClass ? classTypeNode : NULL);
        if ( initExpr != NULL && ctorClassType == NULL ) {
            if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) || !xl6CTextEquals(ctorClass->text, classNode->text) ) {
                xl6CgenError(ctx, initExpr, "class 变量只能使用同类型构造表达式初始化");
            }
        }
        xl6CgenClassDefaultInitFields(ctx, classNode, cName);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        if ( initExpr != NULL && ctorClass != NULL && xl6CTextEquals(ctorClass->text, classNode->text) ) {
            if ( createNode != NULL ) {
                xl6CgenIndent(ctx);
                if ( isGenericClass ) {
                    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                    ctx->currentGenericArgs = classTypeNode->children[0];
                    xl6CgenGenericClassCallableSymbol(ctx, classNode, classTypeNode, createNode);
                } else {
                    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
                }
                fprintf(ctx->out, "(&%s", cName);
                if ( args != NULL && xl6CgenFixedParamCount(createNode->childCount > 0 ? createNode->children[0] : NULL) > 0 ) {
                    fputs(", ", ctx->out);
                    xl6CgenParamNodeArgs(ctx, createNode->childCount > 0 ? createNode->children[0] : NULL, args);
                }
                fputs(");\n", ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
            } else if ( args != NULL && args->childCount > 0 ) {
                xl6CgenError(ctx, initExpr, "class 没有 create 时只能使用无参默认构造");
            }
        }
        if ( createNode == NULL ) {
            xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, cName, node);
        }
        return;
    }

    if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
        XL6_AstNode* inferredTypeNode;
        cType = initExpr != NULL ? xl6CgenInferExprType(ctx, initExpr) : "int";
        inferredTypeNode = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        if ( inferredTypeNode != NULL ) {
            xl6CgenOwnTypeNode(ctx, inferredTypeNode);
            typeNode = inferredTypeNode;
        }
    } else {
        cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "int";
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenAddVarType(ctx, node->text, typeNode);
        xl6CgenAddXValueCleanup(ctx, node->text, typeNode);
        fprintf(ctx->out, "xvalue %s = ", cName);
        if ( initExpr != NULL ) {
            xl6CgenXValueFromExpr(ctx, initExpr);
        } else {
            xl6CgenDefaultValue(ctx, typeNode);
        }
        fputs(";\n", ctx->out);
        return;
    }
    xl6CgenAddVarType(ctx, node->text, typeNode);
    xl6CgenAddFunctionCleanup(ctx, node->text, typeNode);
    fprintf(ctx->out, "%s %s", cType, cName);
    if ( initExpr != NULL ) {
        fputs(" = ", ctx->out);
        if ( xl6CgenIsFunctionType(typeNode) ) {
            xl6CgenFunctionValueExpr(ctx, initExpr, typeNode);
        } else {
            xl6CgenExpr(ctx, initExpr);
        }
    } else {
        fputs(" = ", ctx->out);
        xl6CgenDefaultValue(ctx, typeNode);
    }
    fputs(";\n", ctx->out);
    if ( xl6CgenIsFunctionType(typeNode) && xl6CgenExprIsFunctionBorrow(ctx, initExpr) ) {
        xl6CgenRetainFunctionValue(ctx, node->text);
    }
}

static const XL6_AstNode* xl6CgenResolveMultiReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;
    const XL6_AstNode* returns;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return NULL;
    }
    func = xl6CgenResolveCallFunc(ctx, expr, callee->text);
    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    return (returns != NULL && returns->childCount > 1) ? func : NULL;
}

static const XL6_AstNode* xl6CgenResolveMultiReturnFunctionType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* functionType;
    const XL6_AstNode* returns;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return NULL;
    }

    functionType = xl6CgenFindVarType(ctx, callee->text);
    returns = xl6CgenFunctionReturnTypes(functionType);
    return (xl6CgenIsFunctionType(functionType) && returns != NULL && returns->childCount > 1) ? functionType : NULL;
}

static void xl6CgenEmitKnownFuncCall(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* call)
{
    xl6CgenFuncSymbol(ctx, func);
    fputc('(', ctx->out);
    if ( call != NULL && call->childCount > 1 ) {
        xl6CgenKnownCallArgs(ctx, func, call->children[1]);
    }
    fputc(')', ctx->out);
}

static void xl6CgenEmitMultiReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* func, const XL6_AstNode* functionType)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( functionType != NULL ) {
        callee = expr != NULL && expr->childCount > 0 ? expr->children[0] : NULL;
        args = expr != NULL && expr->childCount > 1 ? expr->children[1] : NULL;
        if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
            xl6CgenFunctionInvoke(ctx, callee->text, functionType, args);
            return;
        }
    }

    xl6CgenEmitKnownFuncCall(ctx, func, expr);
}

static void xl6CgenMultiReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* functionType)
{
    if ( functionType != NULL ) {
        xl6CgenFunctionReturnStructName(ctx,
                                        xl6CgenFunctionParamTypes(functionType),
                                        xl6CgenFunctionReturnTypes(functionType));
        return;
    }
    xl6CgenFuncReturnStructName(ctx, func);
}

static void xl6CgenAddManagedCleanupForType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;
    XL6_AstNode* customNode;

    if ( name == NULL || typeNode == NULL ) {
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenAddFunctionCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenAddXValueCleanup(ctx, name, typeNode);
        return;
    }
    customNode = typeNode->text != NULL ? xl6CgenFindCustom(ctx, typeNode->text) : NULL;
    if ( customNode != NULL ) {
        xl6CgenAddCustomCleanup(ctx, name, customNode);
        return;
    }
    classNode = typeNode->text != NULL ? xl6CgenFindClass(ctx, typeNode->text) : NULL;
    if ( classNode != NULL ) {
        xl6CgenAddClassCleanup(ctx, name, classNode, typeNode->childCount > 0 ? typeNode : NULL);
    }
}

static void xl6CgenMultiDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* names;
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    names = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr) : NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) : (func != NULL ? func->children[2] : NULL);
    if ( returns == NULL || names == NULL ) {
        xl6CgenError(ctx, node, "多返回值声明右侧必须是多返回值 xlang 函数调用");
        return;
    }

    if ( names->childCount != returns->childCount ) {
        xl6CgenError(ctx, node, "多返回值声明数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6CgenMultiReturnStructName(ctx, func, functionType);
    fprintf(ctx->out, " __xl6_multi_%d = ", id);
    xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType);
    fputs(";\n", ctx->out);

    for ( i = 0; i < names->childCount; ++i ) {
        const XL6_AstNode* name = names->children[i];
        const XL6_AstNode* typeNode = returns->children[i];
        char cName[512];

        if ( name == NULL || xl6CTextEquals(name->text, "_") ) {
            if ( xl6CgenIsXValueType(typeNode) ) {
                xl6CgenIndent(ctx);
                fprintf(ctx->out, "if (__xl6_multi_%d.value%u != 0) {\n", id, (unsigned)i);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                fprintf(ctx->out, "xvoUnref(__xl6_multi_%d.value%u);\n", id, (unsigned)i);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                fputs("}\n", ctx->out);
            }
            continue;
        }
        xl6CgenCIdentText(cName, sizeof(cName), name->text);
        xl6CgenIndent(ctx);
        fputs(xl6CgenTypeName(ctx, typeNode), ctx->out);
        fprintf(ctx->out, " %s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
        xl6CgenAddVarType(ctx, name->text, typeNode);
        xl6CgenAddManagedCleanupForType(ctx, name->text, typeNode);
    }
}

static void xl6CgenXValueFromTypedCExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* exprText)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsXValueType(concreteType) ) {
        fputs(exprText != NULL ? exprText : "xvoCreateNull()", ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        fprintf(ctx->out, "xvoCreateInt((long long)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        fprintf(ctx->out, "xvoCreateFloat((double)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "bool") ) {
        fprintf(ctx->out, "xvoCreateBool(%s)", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "string") ) {
        fprintf(ctx->out, "xvoCreateText((void*)(%s), 0, false)", exprText != NULL ? exprText : "\"\"");
    } else if ( xl6CTextEquals(typeName, "time") ) {
        fprintf(ctx->out, "xvoCreateTime((long long)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "point") ) {
        fprintf(ctx->out, "xvoCreatePoint((void*)(%s))", exprText != NULL ? exprText : "0");
    } else {
        xl6CgenError(ctx, typeNode, "当前 C 生成暂不支持将该多返回值写入容器索引");
        fputs("xvoCreateNull()", ctx->out);
    }
}

static void xl6CgenMultiAssign(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    targets = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr) : NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) : (func != NULL ? func->children[2] : NULL);
    if ( returns == NULL || targets == NULL ) {
        xl6CgenError(ctx, node, "多返回值赋值右侧必须是多返回值 xlang 函数调用");
        return;
    }

    if ( targets->childCount != returns->childCount ) {
        xl6CgenError(ctx, node, "多返回值赋值数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6CgenMultiReturnStructName(ctx, func, functionType);
    fprintf(ctx->out, " __xl6_multi_%d = ", id);
    xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType);
    fputs(";\n", ctx->out);

    for ( i = 0; i < targets->childCount; ++i ) {
        const XL6_AstNode* target = targets->children[i];
        const XL6_AstNode* typeNode = returns->children[i];
        char cName[512];

        if ( target == NULL || (target->kind == XL6_AST_NAME && xl6CTextEquals(target->text, "_")) ) {
            if ( xl6CgenIsXValueType(typeNode) ) {
                xl6CgenIndent(ctx);
                fprintf(ctx->out, "if (__xl6_multi_%d.value%u != 0) {\n", id, (unsigned)i);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                fprintf(ctx->out, "xvoUnref(__xl6_multi_%d.value%u);\n", id, (unsigned)i);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                fputs("}\n", ctx->out);
            }
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenCIdentText(cName, sizeof(cName), target->text);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "if (%s != 0) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "xvoUnref(%s);\n", cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputs("}\n", ctx->out);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_INDEX ) {
            int valueId = ctx->tempId++;
            char tempName[64];
            char valueExpr[96];

            snprintf(tempName, sizeof(tempName), "__xl6_multi_index_%d", valueId);
            snprintf(valueExpr, sizeof(valueExpr), "__xl6_multi_%d.value%u", id, (unsigned)i);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "xvalue %s = ", tempName);
            xl6CgenXValueFromTypedCExpr(ctx, typeNode, valueExpr);
            fputs(";\n", ctx->out);
            xl6CgenIndexAssignTemp(ctx, target, tempName);
            continue;
        }
        xl6CgenIndent(ctx);
        xl6CgenExpr(ctx, target);
        fprintf(ctx->out, " = __xl6_multi_%d.value%u;\n", id, (unsigned)i);
    }
}

static void xl6CgenIf(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* elseifList;

    xl6CgenIndent(ctx);
    fputs("if (", ctx->out);
    if ( node->childCount > 0 ) {
        xl6CgenExpr(ctx, node->children[0]);
    }
    fputs(") {\n", ctx->out);
    ctx->indent += 1;
    if ( node->childCount > 1 ) {
        xl6CgenBlock(ctx, node->children[1]);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputc('}', ctx->out);

    if ( node->childCount > 2 ) {
        elseifList = node->children[2];
        for ( i = 0; i < elseifList->childCount; ++i ) {
            const XL6_AstNode* item = elseifList->children[i];
            fputs(" else if (", ctx->out);
            if ( item->childCount > 0 ) {
                xl6CgenExpr(ctx, item->children[0]);
            }
            fputs(") {\n", ctx->out);
            ctx->indent += 1;
            if ( item->childCount > 1 ) {
                xl6CgenBlock(ctx, item->children[1]);
            }
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            fputc('}', ctx->out);
        }
    }

    if ( node->childCount > 3 && node->children[3]->childCount > 0 ) {
        fputs(" else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenBlock(ctx, node->children[3]);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputc('}', ctx->out);
    }
    fputc('\n', ctx->out);
}

static void xl6CgenWhile(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    fputs("while (", ctx->out);
    if ( node->childCount > 0 ) {
        xl6CgenExpr(ctx, node->children[0]);
    }
    fputs(") {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
    if ( node->childCount > 1 ) {
        xl6CgenBlock(ctx, node->children[1]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_continue_%d:\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    fputs(";\n", ctx->out);
}

static void xl6CgenDoWhile(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    fputs("do {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
    if ( node->childCount > 0 ) {
        xl6CgenBlock(ctx, node->children[0]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_continue_%d:\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("} while (", ctx->out);
    if ( node->childCount > 1 ) {
        xl6CgenExpr(ctx, node->children[1]);
    }
    fputs(");\n", ctx->out);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    fputs(";\n", ctx->out);
}

static int xl6CgenIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CTextEquals(node->children[0]->text, "range");
}

static void xl6CgenRangePart(XL6_CgenContext* ctx, const XL6_AstNode* args, int index, const char* defaultText)
{
    if ( args != NULL && args->childCount > (size_t)index ) {
        const XL6_AstNode* arg = args->children[index];
        if ( arg != NULL && arg->childCount > 0 ) {
            xl6CgenExpr(ctx, arg->children[0]);
            return;
        }
    }
    fputs(defaultText, ctx->out);
}

static void xl6CgenRangeContainsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* rangeCall)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsRangeCall(rangeCall) || rangeCall->childCount < 2 ) {
        xl6CgenError(ctx, rangeCall, "in 右侧必须是 range(...)");
        fputs("0", ctx->out);
        return;
    }

    args = rangeCall->children[1];
    if ( args == NULL || args->childCount == 0 || args->childCount > 3 ) {
        xl6CgenError(ctx, rangeCall, "range 参数数量必须是 1、2 或 3");
        fputs("0", ctx->out);
        return;
    }

    fputs("xl6_range_contains_i64(", ctx->out);
    xl6CgenExpr(ctx, value);
    fputs(", ", ctx->out);
    if ( args->childCount == 1 ) {
        fputs("0", ctx->out);
    } else {
        xl6CgenRangePart(ctx, args, 0, "0");
    }
    fputs(", ", ctx->out);
    if ( args->childCount == 1 ) {
        xl6CgenRangePart(ctx, args, 0, "0");
    } else {
        xl6CgenRangePart(ctx, args, 1, "0");
    }
    fputs(", ", ctx->out);
    if ( args->childCount >= 3 ) {
        xl6CgenRangePart(ctx, args, 2, "1");
    } else {
        fputs("1", ctx->out);
    }
    fputc(')', ctx->out);
}

static const XL6_AstNode* xl6CgenOwnSimpleType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenNewSimpleType(name, at);
    if ( typeNode != NULL ) {
        xl6CgenOwnTypeNode(ctx, typeNode);
    }
    return typeNode;
}

static int xl6CgenIsForIterableType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CgenIsArrayXValueType(typeNode) ||
            xl6CgenIsListXValueType(typeNode) ||
            xl6CgenIsSetXValueType(typeNode) ||
            xl6CgenIsDictXValueType(typeNode));
}

static const char* xl6CgenForTargetName(const XL6_AstNode* targets, size_t index)
{
    if ( targets == NULL || index >= targets->childCount || targets->children[index] == NULL ||
         targets->children[index]->text == NULL ) {
        return "_";
    }
    return targets->children[index]->text;
}

static void xl6CgenForTargetCName(char* buffer, size_t capacity, const XL6_AstNode* targets, size_t index)
{
    xl6CgenCIdentText(buffer, capacity, xl6CgenForTargetName(targets, index));
}

static void xl6CgenRegisterLoopTargetType(XL6_CgenContext* ctx, const XL6_AstNode* target, const char* typeName)
{
    const XL6_AstNode* typeNode;

    if ( target == NULL || target->text == NULL || typeName == NULL ) {
        return;
    }
    typeNode = xl6CgenOwnSimpleType(ctx, typeName, target);
    xl6CgenAddVarType(ctx, target->text, typeNode);
    xl6CgenAddXValueCleanup(ctx, target->text, typeNode);
}

static const char* xl6CgenForTargetTypeName(const XL6_AstNode* iterableType, size_t targetIndex, size_t targetCount)
{
    if ( iterableType == NULL || iterableType->text == NULL ) {
        return NULL;
    }
    if ( xl6CTextEquals(iterableType->text, "range") ) {
        return "int";
    }
    if ( xl6CTextEquals(iterableType->text, "var") ) {
        return "var";
    }
    if ( xl6CgenIsDictXValueType(iterableType) ) {
        if ( targetCount == 1 || targetIndex == 0 ) {
            return "string";
        }
        return "var";
    }
    if ( xl6CgenIsArrayXValueType(iterableType) || xl6CgenIsListXValueType(iterableType) ||
         xl6CgenIsSetXValueType(iterableType) ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            return "int";
        }
        return "var";
    }
    return NULL;
}

static void xl6CgenAddAvltbIterCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenOwnSimpleType(ctx, "var", at);
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_AVLTB_ITER, typeNode, NULL);
}

static void xl6CgenFinishLoopIteration(XL6_CgenContext* ctx, size_t cleanupMark, size_t varMark, int continueId)
{
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeVarsSince(ctx, varMark);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_continue_%d:\n", continueId);
    xl6CgenIndent(ctx);
    fputs(";\n", ctx->out);
}

static void xl6CgenForLoopBody(XL6_CgenContext* ctx, const XL6_AstNode* node, size_t breakCleanupMark, size_t continueCleanupMark, size_t varMark, int breakId, int continueId)
{
    xl6CgenPushLoopTarget(ctx, breakCleanupMark, continueCleanupMark, breakId, continueId);
    if ( node->childCount > 2 ) {
        xl6CgenBlock(ctx, node->children[2]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenFinishLoopIteration(ctx, continueCleanupMark, varMark, continueId);
}

static void xl6CgenForArrayLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t varMark;
    size_t loopMark;

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "unsigned int __xl6_count_%d = xvoArrayItemCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "for (unsigned int __xl6_index_%d = 0; __xl6_index_%d < __xl6_count_%d; ++__xl6_index_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoArrayGetValue(%s, __xl6_index_%d));\n",
                valueName, iterName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            fprintf(ctx->out, "xvalue %s = xvoCreateInt((long long)__xl6_index_%d);\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            fprintf(ctx->out, "long long %s = (long long)__xl6_index_%d;\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoArrayGetValue(%s, __xl6_index_%d));\n",
                valueName, iterName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
}

static void xl6CgenForListLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char avlName[64];

    snprintf(avlName, sizeof(avlName), "__xl6_avl_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xavltbase %s = (xavltbase)xvoGetList(%s);\n", avlName, iterName);
    xl6CgenAddAvltbIterCleanup(ctx, avlName, node);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s != 0) {\n", avlName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xrtAVLTB_IterBegin(%s);\n", avlName);
    xl6CgenIndent(ctx);
    fprintf(ctx->out,
            "for (long long* __xl6_list_data_%d = (long long*)xrtAVLTB_IterNext(__xl6_avl_%d); "
            "__xl6_list_data_%d != 0; "
            "__xl6_list_data_%d = (long long*)xrtAVLTB_IterNext(__xl6_avl_%d)) {\n",
            continueId, continueId, continueId, continueId, continueId);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(((xvalue*)(&__xl6_list_data_%d[1]))[0]);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            fprintf(ctx->out, "xvalue %s = xvoCreateInt(__xl6_list_data_%d[0]);\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            fprintf(ctx->out, "long long %s = __xl6_list_data_%d[0];\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(((xvalue*)(&__xl6_list_data_%d[1]))[0]);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForSetLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char avlName[64];

    snprintf(avlName, sizeof(avlName), "__xl6_avl_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xavltbase %s = (xavltbase)xvoGetColl(%s);\n", avlName, iterName);
    xl6CgenAddAvltbIterCleanup(ctx, avlName, node);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s != 0) {\n", avlName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xrtAVLTB_IterBegin(%s);\n", avlName);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "long long __xl6_set_index_%d = 0;\n", continueId);
    xl6CgenIndent(ctx);
    fprintf(ctx->out,
            "for (Coll_Key* __xl6_set_data_%d = (Coll_Key*)xrtAVLTB_IterNext(__xl6_avl_%d); "
            "__xl6_set_data_%d != 0; "
            "__xl6_set_data_%d = (Coll_Key*)xrtAVLTB_IterNext(__xl6_avl_%d), ++__xl6_set_index_%d) {\n",
            continueId, continueId, continueId, continueId, continueId, continueId);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(__xl6_set_data_%d->Value);\n", valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            fprintf(ctx->out, "xvalue %s = xvoCreateInt(__xl6_set_index_%d);\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            fprintf(ctx->out, "long long %s = __xl6_set_index_%d;\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(__xl6_set_data_%d->Value);\n", valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForDictLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char avlName[64];

    snprintf(avlName, sizeof(avlName), "__xl6_avl_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xavltbase %s = (xavltbase)xvoGetTable(%s);\n", avlName, iterName);
    xl6CgenAddAvltbIterCleanup(ctx, avlName, node);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s != 0) {\n", avlName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xrtAVLTB_IterBegin(%s);\n", avlName);
    xl6CgenIndent(ctx);
    fprintf(ctx->out,
            "for (Dict_Key* __xl6_dict_data_%d = (Dict_Key*)xrtAVLTB_IterNext(__xl6_avl_%d); "
            "__xl6_dict_data_%d != 0; "
            "__xl6_dict_data_%d = (Dict_Key*)xrtAVLTB_IterNext(__xl6_avl_%d)) {\n",
            continueId, continueId, continueId, continueId, continueId);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* keyTarget = targets->children[0];
        char keyName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            fprintf(ctx->out, "xvalue %s = xvoCreateText(__xl6_dict_data_%d->Key, __xl6_dict_data_%d->KeyLen, false);\n",
                    keyName, continueId, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "var");
        } else {
            fprintf(ctx->out, "const char* %s = (const char*)__xl6_dict_data_%d->Key;\n", keyName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "string");
        }
    } else {
        const XL6_AstNode* keyTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char keyName[512];
        char valueName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            fprintf(ctx->out, "xvalue %s = xvoCreateText(__xl6_dict_data_%d->Key, __xl6_dict_data_%d->KeyLen, false);\n",
                    keyName, continueId, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "var");
        } else {
            fprintf(ctx->out, "const char* %s = (const char*)__xl6_dict_data_%d->Key;\n", keyName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "string");
        }
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "xvalue %s = xl6_xrt_retain(((xvalue*)(&__xl6_dict_data_%d[1]))[0]);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForRange(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* rangeCall;
    const XL6_AstNode* args;
    size_t varMark;
    size_t loopMark;
    int id;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "for 语句结构不完整");
        return;
    }

    targets = node->children[0];
    rangeCall = node->children[1];
    if ( !xl6CgenIsRangeCall(rangeCall) ) {
        xl6CgenError(ctx, rangeCall, "当前 C 生成只支持 for ... in range(...)");
        return;
    }
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for range 只支持一个或两个迭代变量");
        return;
    }

    args = rangeCall->children[1];
    if ( args->childCount == 0 || args->childCount > 3 ) {
        xl6CgenError(ctx, rangeCall, "range 参数数量必须是 1、2 或 3");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "long long __xl6_range_start_%d = ", id);
    if ( args->childCount == 1 ) {
        fputs("0", ctx->out);
    } else {
        xl6CgenRangePart(ctx, args, 0, "0");
    }
    fputs(";\n", ctx->out);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "long long __xl6_range_stop_%d = ", id);
    if ( args->childCount == 1 ) {
        xl6CgenRangePart(ctx, args, 0, "0");
    } else {
        xl6CgenRangePart(ctx, args, 1, "0");
    }
    fputs(";\n", ctx->out);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "long long __xl6_range_step_%d = ", id);
    if ( args->childCount >= 3 ) {
        xl6CgenRangePart(ctx, args, 2, "1");
    } else {
        fputs("1", ctx->out);
    }
    fputs(";\n", ctx->out);

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "long long __xl6_range_index_%d = 0;\n", id);

    xl6CgenIndent(ctx);
    fprintf(ctx->out,
            "for (long long __xl6_range_value_%d = __xl6_range_start_%d; "
            "(__xl6_range_step_%d > 0) ? (__xl6_range_value_%d < __xl6_range_stop_%d) : (__xl6_range_value_%d > __xl6_range_stop_%d); "
            "__xl6_range_value_%d += __xl6_range_step_%d, ++__xl6_range_index_%d) {\n",
            id, id, id, id, id, id, id, id, id, id);

    ctx->indent += 1;
    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "long long %s = __xl6_range_value_%d;\n", valueName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
    } else {
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "long long %s = __xl6_range_index_%d;\n", indexName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "long long %s = __xl6_range_value_%d;\n", valueName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[1], "int");
    }

    xl6CgenForLoopBody(ctx, node, loopMark, loopMark, varMark, id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    fputs(";\n", ctx->out);
}

static void xl6CgenForXValueIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    const XL6_AstNode* tempType;
    size_t iterMark;
    int id;
    char iterName[64];

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "for 语句结构不完整");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for in 只支持一个或两个迭代变量");
        return;
    }

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_iter_%d", id);

    xl6CgenIndent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent += 1;

    iterMark = ctx->cleanupCount;
    tempType = xl6CgenOwnSimpleType(ctx, "var", iterable);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "xvalue %s = ", iterName);
    xl6CgenXValueFromExpr(ctx, iterable);
    fputs(";\n", ctx->out);
    xl6CgenAddXValueCleanup(ctx, iterName, tempType);

    if ( xl6CgenIsArrayXValueType(iterableType) ) {
        xl6CgenForArrayLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsListXValueType(iterableType) ) {
        xl6CgenForListLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsSetXValueType(iterableType) ) {
        xl6CgenForSetLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsDictXValueType(iterableType) ) {
        xl6CgenForDictLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else {
        int arrayId = ctx->tempId++;
        int listId = ctx->tempId++;
        int setId = ctx->tempId++;
        int dictId = ctx->tempId++;

        xl6CgenIndent(ctx);
        fprintf(ctx->out, "int __xl6_iter_type_%d = xvoType(%s);\n", id, iterName);

        xl6CgenIndent(ctx);
        fprintf(ctx->out, "if (__xl6_iter_type_%d == XL6_XVO_DT_ARRAY) {\n", id);
        ctx->indent += 1;
        xl6CgenForArrayLoop(ctx, node, targets, iterName, iterMark, id, arrayId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_LIST) {\n", id);
        ctx->indent += 1;
        xl6CgenForListLoop(ctx, node, targets, iterName, iterMark, id, listId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_COLL) {\n", id);
        ctx->indent += 1;
        xl6CgenForSetLoop(ctx, node, targets, iterName, iterMark, id, setId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_TABLE) {\n", id);
        ctx->indent += 1;
        xl6CgenForDictLoop(ctx, node, targets, iterName, iterMark, id, dictId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        fputs("}\n", ctx->out);
    }

    xl6CgenCloseScope(ctx, iterMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    fputs(";\n", ctx->out);
}

static void xl6CgenForIn(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* iterableType;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "for 语句结构不完整");
        return;
    }
    if ( xl6CgenIsRangeCall(node->children[1]) ) {
        xl6CgenForRange(ctx, node);
        return;
    }

    iterableType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    if ( !xl6CgenIsForIterableType(iterableType) ) {
        xl6CgenError(ctx, node->children[1], "for in 右侧必须是 range、array、list、set、dict 或 var");
        xl6AstFree(iterableType);
        return;
    }

    xl6CgenForXValueIterable(ctx, node, iterableType);
    xl6AstFree(iterableType);
}

static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return;
    }

    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        xl6CgenDecl(ctx, node);
        break;
    case XL6_AST_MULTI_VAR_DECL:
        xl6CgenMultiDecl(ctx, node);
        break;
    case XL6_AST_ASSIGN:
        xl6CgenAssignStmt(ctx, node);
        break;
    case XL6_AST_MULTI_ASSIGN:
        xl6CgenMultiAssign(ctx, node);
        break;
    case XL6_AST_RETURN:
        if ( node->childCount > 0 ) {
            int id = ctx->tempId++;
            char returnName[64];

            if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
                size_t i;

                xl6CgenIndent(ctx);
                if ( ctx->currentReturnFunc != NULL ) {
                    xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
                } else {
                    xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
                }
                fprintf(ctx->out, " __xl6_return_%d;\n", id);
                for ( i = 0; i < node->childCount; ++i ) {
                    const XL6_AstNode* retType = ctx->currentReturns->children[i];

                    xl6CgenIndent(ctx);
                    fprintf(ctx->out, "__xl6_return_%d.value%u = ", id, (unsigned)i);
                    if ( xl6CgenIsXValueType(retType) ) {
                        xl6CgenXValueFromExpr(ctx, node->children[i]);
                    } else {
                        xl6CgenExpr(ctx, node->children[i]);
                    }
                    fputs(";\n", ctx->out);
                }
            } else {
                xl6CgenIndent(ctx);
                fputs(ctx->currentReturnType != NULL ? xl6CgenTypeName(ctx, ctx->currentReturnType) : xl6CgenInferExprType(ctx, node->children[0]), ctx->out);
                fprintf(ctx->out, " __xl6_return_%d = ", id);
                if ( xl6CgenIsXValueType(ctx->currentReturnType) ) {
                    xl6CgenXValueFromExpr(ctx, node->children[0]);
                } else if ( xl6CgenIsFunctionType(ctx->currentReturnType) ) {
                    xl6CgenFunctionValueExpr(ctx, node->children[0], ctx->currentReturnType);
                } else {
                    xl6CgenExpr(ctx, node->children[0]);
                }
                fputs(";\n", ctx->out);
                if ( xl6CgenIsFunctionType(ctx->currentReturnType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[0]) ) {
                    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                    xl6CgenRetainFunctionValue(ctx, returnName);
                }
            }
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            fprintf(ctx->out, "return __xl6_return_%d;\n", id);
        } else {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            fputs("return;\n", ctx->out);
        }
        break;
    case XL6_AST_EXPR_STMT:
        xl6CgenIndent(ctx);
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        fputs(";\n", ctx->out);
        break;
    case XL6_AST_IF:
        xl6CgenIf(ctx, node);
        break;
    case XL6_AST_WHILE:
        xl6CgenWhile(ctx, node);
        break;
    case XL6_AST_DO_WHILE:
        xl6CgenDoWhile(ctx, node);
        break;
    case XL6_AST_FOR:
        xl6CgenForIn(ctx, node);
        break;
    case XL6_AST_BREAK:
        xl6CgenLoopJump(ctx, node, 0);
        break;
    case XL6_AST_CONTINUE:
        xl6CgenLoopJump(ctx, node, 1);
        break;
    default:
        xl6CgenError(ctx, node, "当前 C 生成暂不支持该语句");
        break;
    }
}

static void xl6CgenFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* nameNode;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, node);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    (void)nameNode;
    fputc(' ', ctx->out);
    xl6CgenFuncSymbol(ctx, node);
    fputc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            if ( param->childCount == 0 ) {
                xl6CgenError(ctx, param, "函数参数缺少类型");
                fputs("int ", ctx->out);
            } else {
                fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
                fputc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenExportInternalProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "当前 C 生成暂不支持多返回值 export 函数");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    fputc(' ', ctx->out);
    xl6CgenExportInternalSymbol(ctx->out, node);
    fputc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 export 函数");
                fputs("int ", ctx->out);
            } else {
                fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
                fputc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenExportWrapperProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "当前 C 生成暂不支持多返回值 export wrapper");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    fputc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node->text);
    fputc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 export wrapper");
                fputs("int ", ctx->out);
            } else {
                fputs(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                fputc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenExternFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "当前 C 生成暂不支持多返回值 extern 函数");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    fputc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node->text);
    fputc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 extern 函数");
                fputs("int ", ctx->out);
            } else {
                fputs(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                fputc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenExternVarDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenExternalVarTypeNode(node);
    fputs("extern ", ctx->out);
    fputs(xl6CgenCAbiTypeName(ctx, typeNode), ctx->out);
    fputc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node != NULL ? node->text : NULL);
    fputs(";\n", ctx->out);
}

static void xl6CgenExportVarDef(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;

    typeNode = xl6CgenExternalVarTypeNode(node);
    initExpr = xl6CgenExternalVarInitExpr(node);
    if ( node != NULL && node->kind == XL6_AST_EXPORT_CONST ) {
        fputs("const ", ctx->out);
    }
    fputs(xl6CgenCAbiTypeName(ctx, typeNode), ctx->out);
    fputc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node != NULL ? node->text : NULL);
    fputs(" = ", ctx->out);
    xl6CgenExpr(ctx, initExpr);
    fputs(";\n", ctx->out);
}

static void xl6CgenClassParamList(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* params)
{
    size_t i;

    xl6CgenClassTypeName(ctx->out, classNode->text);
    fputs("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "class C 生成暂不支持可变参数方法");
            fputs("int ", ctx->out);
        } else {
            fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            fputc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenClassFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "class C 生成暂不支持多返回值方法");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fputc(' ', ctx->out);
    xl6CgenClassCallableSymbol(ctx, classNode, funcNode);
    fputc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenClassParamList(ctx, classNode, params);
    fputc(')', ctx->out);
}

static void xl6CgenClassSpecialProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* params;

    fputs("void ", ctx->out);
    if ( xl6CTextEquals(specialNode->text, "create") ) {
        xl6CgenClassCallableSymbol(ctx, classNode, specialNode);
    } else {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, specialNode->text);
    }
    fputc('(', ctx->out);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenClassParamList(ctx, classNode, params);
    fputc(')', ctx->out);
}

static void xl6CgenClassDestroyProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    fputs("void ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
    fputc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    fputs("* self)", ctx->out);
}

static void xl6CgenGenericClassDecl(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* classNode;
    const XL6_AstNode* body;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char classTypeName[256];
    size_t i;

    if ( ctx == NULL || typeNode == NULL || typeNode->childCount == 0 ) {
        return;
    }
    classNode = xl6CgenFindClass(ctx, typeNode->text);
    if ( classNode == NULL || !xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }

    xl6CgenClassTypeRefName(classTypeName, sizeof(classTypeName), typeNode);
    if ( xl6CgenClassWasEmitted(ctx, classTypeName) ) {
        return;
    }
    xl6CgenMarkClassEmitted(ctx, classTypeName);
    xl6CgenAddGenericClassInstance(ctx, classNode, typeNode);

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    fprintf(ctx->out, "typedef struct %s {\n", classTypeName);
    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
                continue;
            }
            fputs("    ", ctx->out);
            fputs(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            fputs(";\n", ctx->out);
        }
    }
    fprintf(ctx->out, "} %s;\n", classTypeName);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenCollectGenericClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE && node->childCount > 0 && xl6CgenIsClassType(ctx, node) ) {
        XL6_AstNode* canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, node);
        if ( canonicalType != NULL ) {
            xl6CgenGenericClassDecl(ctx, canonicalType);
            xl6AstFree(canonicalType);
        }
    } else if ( node->kind == XL6_AST_CALL ) {
        XL6_AstNode* classNode = NULL;
        XL6_AstNode* ctorType = xl6CgenGenericClassCtorType(ctx, node, &classNode);
        if ( ctorType != NULL ) {
            char classTypeName[256];
            xl6CgenClassTypeRefName(classTypeName, sizeof(classTypeName), ctorType);
            if ( xl6CgenClassWasEmitted(ctx, classTypeName) ) {
                xl6AstFree(ctorType);
            } else {
                xl6CgenOwnTypeNode(ctx, ctorType);
                xl6CgenGenericClassDecl(ctx, ctorType);
            }
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectGenericClassDecls(ctx, node->children[i]);
    }
}

static void xl6CgenGenericClassMemberSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const char* memberName)
{
    const XL6_AstNode* args;
    size_t i;

    (void)classNode;
    fputs("xl_class_", ctx->out);
    xl6CgenMangledPart(ctx->out, typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL && typeNode->childCount > 0 ? typeNode->children[0] : NULL;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            fputs("__", ctx->out);
            xl6CgenMangledType(ctx, args->children[i]);
        }
    }
    fputs("__", ctx->out);
    xl6CgenMangledPart(ctx->out, memberName != NULL ? memberName : "_");
}

static void xl6CgenGenericClassCallableSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, name != NULL ? name : "_");

    if ( memberNode == NULL ||
         (memberNode->kind != XL6_AST_FUNC && memberNode->kind != XL6_AST_CREATE) ||
         xl6CgenClassCallableOverloadCount(classNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            fputc('_', ctx->out);
        }
    }
}

static void xl6CgenGenericClassParamList(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* params)
{
    size_t i;

    fputs(xl6CgenTypeName(ctx, typeNode), ctx->out);
    fputs("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "泛型 class 方法暂不支持可变参数");
            fputs("int ", ctx->out);
        } else {
            fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            fputc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenGenericClassFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "泛型 class 方法暂不支持多返回值");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fputc(' ', ctx->out);
    xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, funcNode);
    fputc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenGenericClassParamList(ctx, typeNode, params);
    fputc(')', ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassSpecialProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    fputs("void ", ctx->out);
    xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, specialNode);
    fputc('(', ctx->out);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenGenericClassParamList(ctx, typeNode, params);
    fputc(')', ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassDestroyProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    fputs("void ", ctx->out);
    xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, "destroy");
    fputc('(', ctx->out);
    fputs(xl6CgenTypeName(ctx, typeNode), ctx->out);
    fputs("* self)", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassMethodProtos(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_AstNode* classNode = ctx->genericClassInstances[i].classNode;
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;
        const XL6_AstNode* body = xl6CgenClassBody(classNode);
        int hasDestroy;

        if ( body == NULL ) {
            continue;
        }
        hasDestroy = 0;
        for ( j = 0; j < body->childCount; ++j ) {
            const XL6_AstNode* member = body->children[j];
            if ( member != NULL && member->kind == XL6_AST_FUNC ) {
                xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, member);
                fputs(";\n", ctx->out);
            } else if ( member != NULL && member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenGenericClassSpecialProto(ctx, classNode, typeNode, member);
                fputs(";\n", ctx->out);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenGenericClassDestroyProto(ctx, classNode, typeNode);
            fputs(";\n", ctx->out);
        }
    }
}

static void xl6CgenClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL || classNode->text == NULL || xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        return;
        xl6CgenError(ctx, classNode, "当前 C 生成暂不支持泛型 class");
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            xl6CgenClassDecls(ctx, baseClass);
        }
    }

    xl6CgenMarkClassEmitted(ctx, classNode->text);
    fputs("typedef struct ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    fputs(" {\n", ctx->out);
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        fputs("    ", ctx->out);
        xl6CgenClassTypeName(ctx->out, base->text);
        fputs(" base;\n", ctx->out);
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
                continue;
            }
            fputs("    ", ctx->out);
            fputs(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            fputs(";\n", ctx->out);
        }
    }
    fputs("} ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    fputs(";\n", ctx->out);

    if ( body != NULL ) {
        int hasDestroy = 0;
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_FUNC ) {
                xl6CgenClassFuncProto(ctx, classNode, member);
                fputs(";\n", ctx->out);
            } else if ( member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenClassSpecialProto(ctx, classNode, member);
                fputs(";\n", ctx->out);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenClassDestroyProto(ctx, classNode);
            fputs(";\n", ctx->out);
        }
    }
}

static void xl6CgenClassDefaultInitFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    char basePath[512];
    size_t i;

    if ( classNode == NULL || varPath == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            snprintf(basePath, sizeof(basePath), "%s.base", varPath);
            xl6CgenClassDefaultInitFields(ctx, baseClass, basePath);
        }
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* initExpr;
        XL6_AstNode* fieldCustom;
        XL6_AstNode* ctorCustom;

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount < 2 ) {
            continue;
        }
        fieldType = member->children[0];
        initExpr = member->children[1];
        fieldCustom = fieldType != NULL ? xl6CgenFindCustom(ctx, fieldType->text) : NULL;
        ctorCustom = NULL;
        if ( fieldCustom != NULL && xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom) ) {
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, initExpr, "custom 字段只能使用同类型构造表达式初始化");
                continue;
            }
            xl6CgenIndent(ctx);
            fputs(varPath, ctx->out);
            fputc('.', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            fputs(".handle = ", ctx->out);
            xl6CgenCustomCtorValue(ctx, ctorCustom, initExpr);
            fputs(";\n", ctx->out);
            continue;
        }
        xl6CgenIndent(ctx);
        fputs(varPath, ctx->out);
        fputc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, member->text);
        fputs(" = ", ctx->out);
        xl6CgenExpr(ctx, initExpr);
        fputs(";\n", ctx->out);
    }
}

static void xl6CgenCustomParamList(XL6_CgenContext* ctx, const XL6_AstNode* params, int includeSelf)
{
    size_t i;
    int wrote;

    wrote = 0;
    if ( includeSelf ) {
        fputs("void* self", ctx->out);
        wrote = 1;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( wrote ) {
                fputs(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "custom C API 不支持可变参数");
                fputs("int ", ctx->out);
            } else {
                fputs(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                fputc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
            wrote = 1;
        }
    }
    if ( !wrote ) {
        fputs("void", ctx->out);
    }
}

static void xl6CgenCustomExternProto(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* member)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    (void)customNode;
    if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        nameNode = member->childCount > 0 ? member->children[0] : NULL;
        params = member->childCount > 1 ? member->children[1] : NULL;
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "create") ) {
            fputs("void* ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            fputc('(', ctx->out);
            xl6CgenCustomParamList(ctx, params, 0);
            fputc(')', ctx->out);
        } else if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "destroy") ) {
            fputs("long long ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            fputs("(void* self)", ctx->out);
        }
        return;
    }

    nameNode = member->childCount > 0 ? member->children[0] : NULL;
    (void)nameNode;
    params = member->childCount > 1 ? member->children[1] : NULL;
    returns = member->childCount > 2 ? member->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "custom C API 不支持多返回值");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fputc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, member->text);
    fputc('(', ctx->out);
    xl6CgenCustomParamList(ctx, params, 1);
    fputc(')', ctx->out);
}

static void xl6CgenCustomHelperParamList(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* params)
{
    size_t i;

    xl6CgenCustomTypeName(ctx->out, customNode->text);
    fputs("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        fputs(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "custom helper C 生成暂不支持可变参数方法");
            fputs("int ", ctx->out);
        } else {
            fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            fputc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenCustomFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "custom helper C 生成暂不支持多返回值方法");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fputc(' ', ctx->out);
    xl6CgenCustomCallableSymbol(ctx, customNode, funcNode);
    fputc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenCustomHelperParamList(ctx, customNode, params);
    fputc(')', ctx->out);
}

static void xl6CgenCustomDecls(XL6_CgenContext* ctx, const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    size_t i;

    fputs("typedef struct ", ctx->out);
    xl6CgenCustomTypeName(ctx->out, customNode->text);
    fputs(" { void* handle; } ", ctx->out);
    xl6CgenCustomTypeName(ctx->out, customNode->text);
    fputs(";\n", ctx->out);

    if ( customNode->childCount == 0 ) {
        return;
    }
    body = customNode->children[0];
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL || member->kind == XL6_AST_CUSTOM_EXTERN_FUNC ) {
            xl6CgenCustomExternProto(ctx, customNode, member);
            fputs(";\n", ctx->out);
        } else if ( member->kind == XL6_AST_FUNC ) {
            xl6CgenCustomFuncProto(ctx, customNode, member);
            fputs(";\n", ctx->out);
        }
    }
}

static void xl6CgenRegisterParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenAddVarType(ctx, param->text, param->children[0]);
            if ( xl6CgenParamIsVararg(param) ) {
                xl6CgenAddXValueCleanup(ctx, param->text, param->children[0]);
            }
        }
    }
}

static int xl6CgenNodeContainsBaseCtorCall(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "base") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeContainsBaseCtorCall(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitAutoBaseCreateWithSelfExpr(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* selfExpr, const XL6_AstNode* atNode)
{
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return;
    }

    createNode = xl6CgenFindClassCreateCallableWithoutArgs(baseClass);
    if ( createNode == NULL ) {
        if ( xl6CgenClassHasCreate(baseClass) ) {
            xl6CgenError(ctx, atNode, "父类 create 需要参数，子类 create 必须显式调用 base(...)");
        }
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, selfExpr, NULL);
    fputs(";\n", ctx->out);
}

static void xl6CgenEmitAutoBaseCreateForSelf(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* atNode)
{
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, "&self->base", atNode);
}

static void xl6CgenEmitAutoBaseCreateForValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* atNode)
{
    char selfExpr[512];

    if ( varPath == NULL ) {
        return;
    }
    snprintf(selfExpr, sizeof(selfExpr), "&%s.base", varPath);
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, selfExpr, atNode);
}

static void xl6CgenEmitCustomHandleCleanupPath(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const char* handlePath)
{
    const XL6_AstNode* destroyNode;

    if ( customNode == NULL || handlePath == NULL ) {
        return;
    }
    destroyNode = xl6CgenFindCustomSpecial(customNode, "destroy");
    if ( destroyNode == NULL ) {
        return;
    }

    xl6CgenIndent(ctx);
    fprintf(ctx->out, "if (%s != 0) {\n", handlePath);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6CgenCStringSymbol(ctx->out, destroyNode->text);
    fprintf(ctx->out, "(%s);\n", handlePath);
    xl6CgenIndent(ctx);
    fprintf(ctx->out, "%s = 0;\n", handlePath);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    fputs("}\n", ctx->out);
}

static void xl6CgenEmitClassDirectFieldCleanups(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return;
    }

    for ( i = body->childCount; i > 0; --i ) {
        const XL6_AstNode* member = body->children[i - 1];
        const XL6_AstNode* typeNode;
        XL6_AstNode* customNode;
        XL6_AstNode* nestedClass;
        char path[512];
        char cFieldName[512];

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 || member->text == NULL ) {
            continue;
        }
        xl6CgenCIdentText(cFieldName, sizeof(cFieldName), member->text);
        typeNode = member->children[0];
        customNode = xl6CgenFindCustom(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( customNode != NULL && xl6CgenCustomHasDestroy(customNode) ) {
            snprintf(path, sizeof(path), "self->%s.handle", cFieldName);
            xl6CgenEmitCustomHandleCleanupPath(ctx, customNode, path);
            continue;
        }

        nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
            xl6CgenIndent(ctx);
            xl6CgenClassMemberSymbol(ctx->out, nestedClass->text, "destroy");
            fprintf(ctx->out, "(&self->%s);\n", cFieldName);
        }
    }
}

static void xl6CgenEmitClassBaseCleanup(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL ) {
        return;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return;
    }

    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL || !xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenClassMemberSymbol(ctx->out, baseClass->text, "destroy");
    fputs("(&self->base);\n", ctx->out);
}

static void xl6CgenEmitClassDestroyTail(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenEmitClassDirectFieldCleanups(ctx, classNode);
    xl6CgenEmitClassBaseCleanup(ctx, classNode);
}

static void xl6CgenClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;

    xl6CgenClassFuncProto(ctx, classNode, funcNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    xl6CgenAddVarType(ctx, "self", classNode);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenGenericClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, funcNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    xl6CgenAddVarType(ctx, "self", typeNode);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassSpecialProto(ctx, classNode, typeNode, specialNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", typeNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassDestroyProto(ctx, classNode, typeNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", typeNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* params;
    size_t varMark;

    xl6CgenClassSpecialProto(ctx, classNode, specialNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", classNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( xl6CTextEquals(specialNode->text, "create") &&
         !xl6CgenNodeContainsBaseCtorCall(specialNode->childCount > 1 ? specialNode->children[1] : NULL) ) {
        xl6CgenEmitAutoBaseCreateForSelf(ctx, classNode, specialNode);
    }
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* prevClass;
    size_t varMark;

    xl6CgenClassDestroyProto(ctx, classNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", classNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenClassMethods(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasDestroy;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    hasDestroy = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FUNC ) {
            xl6CgenClassFunc(ctx, classNode, member);
        } else if ( member->kind == XL6_AST_CREATE ) {
            if ( xl6CTextEquals(member->text, "destroy") ) {
                hasDestroy = 1;
            }
            xl6CgenClassSpecial(ctx, classNode, member);
        }
    }
    if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenClassAutoDestroy(ctx, classNode);
    }
}

static void xl6CgenGenericClassMethods(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_AstNode* classNode = ctx->genericClassInstances[i].classNode;
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;
        const XL6_AstNode* body = xl6CgenClassBody(classNode);
        int hasDestroy;

        if ( body == NULL ) {
            continue;
        }
        hasDestroy = 0;
        for ( j = 0; j < body->childCount; ++j ) {
            const XL6_AstNode* member = body->children[j];
            if ( member != NULL && member->kind == XL6_AST_FUNC ) {
                xl6CgenGenericClassFunc(ctx, classNode, typeNode, member);
            } else if ( member != NULL && member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenGenericClassSpecial(ctx, classNode, typeNode, member);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenGenericClassAutoDestroy(ctx, classNode, typeNode);
        }
    }
}

static int xl6CgenBlockEndsWithReturn(const XL6_AstNode* block)
{
    const XL6_AstNode* last;

    if ( block == NULL || block->childCount == 0 ) {
        return 0;
    }
    last = block->children[block->childCount - 1];
    return last != NULL && last->kind == XL6_AST_RETURN;
}

static void xl6CgenCustomFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevCustom;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenCustomFuncProto(ctx, customNode, funcNode);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;

    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    prevClass = ctx->currentClass;
    prevCustom = ctx->currentCustom;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    body = funcNode->childCount > 3 ? funcNode->children[3] : NULL;

    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = funcNode;
    ctx->currentClass = NULL;
    ctx->currentCustom = customNode;

    xl6CgenAddVarType(ctx, "self", customNode);
    xl6CgenRegisterParamTypes(ctx, params);
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);

    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentClass = prevClass;
    ctx->currentCustom = prevCustom;
    xl6CgenFreeVarsSince(ctx, varMark);

    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenCustomMethods(XL6_CgenContext* ctx, const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    if ( body == NULL ) {
        return;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FUNC ) {
            xl6CgenCustomFunc(ctx, customNode, member);
        }
    }
}

static int xl6CgenScopeVarFind(XL6_CgenVar* vars, size_t count, const char* name)
{
    size_t i;

    if ( vars == NULL || name == NULL ) {
        return -1;
    }
    for ( i = count; i > 0; --i ) {
        if ( xl6CTextEquals(vars[i - 1].name, name) ) {
            return (int)(i - 1);
        }
    }
    return -1;
}

static int xl6CgenScopeVarAdd(XL6_CgenContext* ctx, XL6_CgenVar** vars, size_t* count, size_t* capacity, const char* name, const XL6_AstNode* type)
{
    XL6_CgenVar* newItems;
    size_t newCapacity;

    if ( vars == NULL || count == NULL || capacity == NULL || name == NULL || type == NULL ) {
        return 1;
    }
    if ( *count == *capacity ) {
        newCapacity = *capacity == 0 ? 8 : *capacity * 2;
        newItems = (XL6_CgenVar*)realloc(*vars, newCapacity * sizeof(XL6_CgenVar));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, type, "匿名函数作用域表内存不足");
            return 0;
        }
        *vars = newItems;
        *capacity = newCapacity;
    }
    (*vars)[*count].name = (char*)name;
    (*vars)[*count].type = type;
    *count += 1;
    return 1;
}

static int xl6CgenNameInParamList(const XL6_AstNode* params, const char* name)
{
    size_t i;

    if ( params == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenCollectAnonCaptures(XL6_CgenContext* ctx, XL6_CgenAnonFunc* info, const XL6_AstNode* node, const XL6_AstNode* params, XL6_CgenVar* outerVars, size_t outerCount)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node != info->node && node->kind == XL6_AST_ANON_FUNC ) {
        return;
    }
    if ( node->kind == XL6_AST_NAME && !xl6CgenNameInParamList(params, node->text) ) {
        int index = xl6CgenScopeVarFind(outerVars, outerCount, node->text);
        if ( index >= 0 ) {
            xl6CgenAnonAddCapture(ctx, info, node->text, outerVars[index].type);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectAnonCaptures(ctx, info, node->children[i], params, outerVars, outerCount);
    }
}

static void xl6CgenCollectAnonFuncsInNode(XL6_CgenContext* ctx, const XL6_AstNode* node, XL6_CgenVar** scopeVars, size_t* scopeCount, size_t* scopeCapacity)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        XL6_CgenAnonFunc* info = xl6CgenAddAnonFuncInfo(ctx, node);
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        if ( info != NULL ) {
            xl6CgenCollectAnonCaptures(ctx, info, node, params, *scopeVars, *scopeCount);
        }
        return;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        if ( node->childCount > 1 ) {
            xl6CgenCollectAnonFuncsInNode(ctx, node->children[1], scopeVars, scopeCount, scopeCapacity);
        }
        if ( node->childCount > 0 ) {
            const XL6_AstNode* typeNode = node->children[0];
            if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && node->childCount > 1 ) {
                XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
                if ( inferredType != NULL ) {
                    xl6CgenOwnTypeNode(ctx, inferredType);
                    typeNode = inferredType;
                }
            }
            (void)xl6CgenScopeVarAdd(ctx, scopeVars, scopeCount, scopeCapacity, node->text, typeNode);
        }
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectAnonFuncsInNode(ctx, node->children[i], scopeVars, scopeCount, scopeCapacity);
    }
}

static void xl6CgenCollectAnonFuncsInFunc(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    XL6_CgenVar* scopeVars;
    size_t scopeCount;
    size_t scopeCapacity;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t i;

    scopeVars = NULL;
    scopeCount = 0;
    scopeCapacity = 0;
    params = funcNode != NULL && funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    body = funcNode != NULL && funcNode->childCount > 3 ? funcNode->children[3] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param != NULL && param->childCount > 0 ) {
                (void)xl6CgenScopeVarAdd(ctx, &scopeVars, &scopeCount, &scopeCapacity, param->text, param->children[0]);
            }
        }
    }
    xl6CgenCollectAnonFuncsInNode(ctx, body, &scopeVars, &scopeCount, &scopeCapacity);
    xl6Free(scopeVars);
}

static void xl6CgenCollectAnonFuncs(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    XL6_CgenVar* scopeVars;
    size_t scopeCount;
    size_t scopeCapacity;
    size_t i;

    scopeVars = NULL;
    scopeCount = 0;
    scopeCapacity = 0;
    if ( root == NULL ) {
        return;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node == NULL ) {
            continue;
        }
        if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenCollectAnonFuncsInFunc(ctx, node);
        } else {
            xl6CgenCollectAnonFuncsInNode(ctx, node, &scopeVars, &scopeCount, &scopeCapacity);
        }
    }
    xl6Free(scopeVars);
}

static void xl6CgenHashText(unsigned long long* hash, const char* text)
{
    const unsigned char* p;

    if ( hash == NULL || text == NULL ) {
        return;
    }
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        *hash ^= (unsigned long long)(*p);
        *hash *= 1099511628211ULL;
    }
}

static void xl6CgenHashTypeNode(unsigned long long* hash, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( typeNode == NULL ) {
        xl6CgenHashText(hash, "_");
        return;
    }
    xl6CgenHashText(hash, typeNode->text != NULL ? typeNode->text : "_");
    if ( typeNode->childCount == 0 ) {
        return;
    }
    xl6CgenHashText(hash, "<");
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashTypeNode(hash, typeNode->children[i]);
    }
    xl6CgenHashText(hash, ">");
}

static void xl6CgenHashSignatureTypeNode(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* typeNode)
{
    size_t i;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL ) {
        xl6CgenHashText(hash, "_");
        return;
    }
    xl6CgenHashText(hash, typeNode->text != NULL ? typeNode->text : "_");
    if ( typeNode->childCount == 0 ) {
        return;
    }
    xl6CgenHashText(hash, "<");
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode->children[i]);
    }
    xl6CgenHashText(hash, ">");
}

static void xl6CgenHashFunctionTypeList(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->childCount; ++i ) {
        const XL6_AstNode* item = list->children[i];
        const XL6_AstNode* typeNode = item;
        if ( item != NULL && item->kind == XL6_AST_PARAM && item->childCount > 0 ) {
            typeNode = item->children[0];
        }
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
}

static void xl6CgenHashFunctionTypePrefix(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* list, size_t count, int includeVararg)
{
    size_t i;
    int fixedCount;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < count && i < list->childCount; ++i ) {
        const XL6_AstNode* item = list->children[i];
        const XL6_AstNode* typeNode = xl6CgenCallableItemType(item);
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
    if ( includeVararg ) {
        const XL6_AstNode* item;
        const XL6_AstNode* typeNode;

        fixedCount = xl6CgenFixedParamCount(list);
        if ( list->childCount <= (size_t)fixedCount ) {
            return;
        }
        item = list->children[fixedCount];
        typeNode = xl6CgenCallableItemType(item);
        if ( count > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
}

static unsigned long long xl6CgenFunctionSignatureId(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    unsigned long long hash;

    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "function(");
    xl6CgenHashFunctionTypeList(ctx, &hash, params);
    xl6CgenHashText(&hash, ")->(");
    xl6CgenHashFunctionTypeList(ctx, &hash, returns);
    xl6CgenHashText(&hash, ")");
    return hash;
}

static unsigned long long xl6CgenFunctionSignatureIdForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    unsigned long long hash;

    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "function(");
    xl6CgenHashFunctionTypePrefix(ctx, &hash, params, paramCount, includeVararg);
    xl6CgenHashText(&hash, ")->(");
    xl6CgenHashFunctionTypeList(ctx, &hash, returns);
    xl6CgenHashText(&hash, ")");
    return hash;
}

static void xl6CgenFunctionReturnStructNameForId(FILE* out, unsigned long long signatureId)
{
    fprintf(out, "XL6_Function_Return_%016llX", signatureId);
}

static void xl6CgenFunctionReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    xl6CgenFunctionReturnStructNameForId(ctx->out, xl6CgenFunctionSignatureId(ctx, params, returns));
}

static void xl6CgenFunctionReturnStructNameForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    xl6CgenFunctionReturnStructNameForId(ctx->out, xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, paramCount, includeVararg));
}

static void xl6CgenFunctionReturnStructDeclForId(XL6_CgenContext* ctx, const XL6_AstNode* returns, unsigned long long signatureId)
{
    size_t i;

    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    if ( !xl6CgenRememberFunctionReturnStruct(ctx, signatureId) ) {
        return;
    }

    fprintf(ctx->out, "#ifndef XL6_FUNCTION_RETURN_%016llX\n", signatureId);
    fprintf(ctx->out, "#define XL6_FUNCTION_RETURN_%016llX\n", signatureId);
    fputs("typedef struct ", ctx->out);
    xl6CgenFunctionReturnStructNameForId(ctx->out, signatureId);
    fputs(" {\n", ctx->out);
    for ( i = 0; i < returns->childCount; ++i ) {
        fputs("    ", ctx->out);
        fputs(xl6CgenTypeName(ctx, returns->children[i]), ctx->out);
        fprintf(ctx->out, " value%u;\n", (unsigned)i);
    }
    fputs("} ", ctx->out);
    xl6CgenFunctionReturnStructNameForId(ctx->out, signatureId);
    fputs(";\n#endif\n\n", ctx->out);
}

static void xl6CgenFunctionReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclForId(ctx, returns, xl6CgenFunctionSignatureId(ctx, params, returns));
}

static void xl6CgenFunctionReturnStructDeclForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclForId(ctx, returns, xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, paramCount, includeVararg));
}

static unsigned long long xl6CgenTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    unsigned long long hash;
    XL6_AstNode namedType;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode != NULL && (typeNode->kind == XL6_AST_CLASS || typeNode->kind == XL6_AST_CUSTOM) ) {
        memset(&namedType, 0, sizeof(namedType));
        namedType.kind = XL6_AST_TYPE;
        namedType.text = typeNode->text;
        namedType.line = typeNode->line;
        namedType.column = typeNode->column;
        typeNode = &namedType;
    }
    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "type:");
    xl6CgenHashTypeNode(&hash, typeNode);
    return hash;
}

static void xl6CgenEmitTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    fprintf(ctx->out, "0x%016llXULL", xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenEmitTypeIdForName(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    XL6_AstNode typeNode;

    memset(&typeNode, 0, sizeof(typeNode));
    typeNode.kind = XL6_AST_TYPE;
    typeNode.text = (char*)(name != NULL ? name : "_");
    if ( at != NULL ) {
        typeNode.line = at->line;
        typeNode.column = at->column;
    }
    xl6CgenEmitTypeIdForTypeNode(ctx, &typeNode);
}

static int xl6CgenTypeValueUsesBaseType(const char* name)
{
    return name != NULL &&
           (xl6CTextEquals(name, "array") ||
            xl6CTextEquals(name, "list") ||
            xl6CTextEquals(name, "set") ||
            xl6CTextEquals(name, "dict") ||
            xl6CTextEquals(name, "function"));
}

static XL6_AstNode* xl6CgenCloneExprTypeForTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* baseType;

    rawType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( rawType == NULL ) {
        return NULL;
    }

    concreteType = xl6CgenSubstituteGenericType(ctx, rawType);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL &&
         concreteType->childCount > 0 &&
         xl6CgenTypeValueUsesBaseType(concreteType->text) ) {
        baseType = xl6CgenNewSimpleType(concreteType->text, concreteType);
        xl6AstFree(rawType);
        return baseType;
    }
    return rawType;
}

static void xl6CgenEmitTypeOfExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;

    typeNode = xl6CgenCloneExprTypeForTypeValue(ctx, expr);
    if ( typeNode == NULL ) {
        xl6CgenError(ctx, expr, "无法确定 type(...) 参数的静态类型");
        fputs("0", ctx->out);
        return;
    }
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "var") ) {
        fputs("xl6_xrt_type_value_id(", ctx->out);
        xl6CgenExpr(ctx, expr);
        fputs(xl6CgenExprIsXValueBorrow(ctx, expr) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(typeNode);
        return;
    }
    xl6CgenEmitTypeIdForTypeNode(ctx, typeNode);
    xl6AstFree(typeNode);
}

static void xl6CgenEmitFunctionRuntime(FILE* out)
{
    fputs("#include <stdlib.h>\n", out);
    fputs("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    fputs("#define XL6_XVALUE_TYPEDEF\n", out);
    fputs("typedef struct xvalue_struct* xvalue;\n", out);
    fputs("#endif\n", out);
    fputs("typedef struct XL6_Function {\n", out);
    fputs("    unsigned long long signature;\n", out);
    fputs("    void* env;\n", out);
    fputs("    void* invoke;\n", out);
    fputs("    xvalue (*invoke_var)(void* env, int argc, xvalue* argv);\n", out);
    fputs("    void (*retain)(void* env);\n", out);
    fputs("    void (*release)(void* env);\n", out);
    fputs("} XL6_Function;\n\n", out);
}

static void xl6CgenEmitFunctionVarRuntime(FILE* out)
{
    fputs("static xvalue xl6_function_call_var(XL6_Function fn, int argc, ...)\n", out);
    fputs("{\n", out);
    fputs("    xvalue* argv = 0;\n", out);
    fputs("    xvalue result;\n", out);
    fputs("    va_list ap;\n", out);
    fputs("    int i;\n\n", out);
    fputs("    if ( argc < 0 || fn.invoke_var == 0 ) {\n", out);
    fputs("        exit(3);\n", out);
    fputs("    }\n", out);
    fputs("    if ( argc > 0 ) {\n", out);
    fputs("        argv = (xvalue*)malloc(sizeof(xvalue) * (size_t)argc);\n", out);
    fputs("        if ( argv == 0 ) {\n", out);
    fputs("            exit(2);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    va_start(ap, argc);\n", out);
    fputs("    for ( i = 0; i < argc; ++i ) {\n", out);
    fputs("        argv[i] = va_arg(ap, xvalue);\n", out);
    fputs("    }\n", out);
    fputs("    va_end(ap);\n", out);
    fputs("    result = fn.invoke_var(fn.env, argc, argv);\n", out);
    fputs("    for ( i = 0; i < argc; ++i ) {\n", out);
    fputs("        if ( argv[i] != 0 ) {\n", out);
    fputs("            xvoUnref(argv[i]);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    free(argv);\n", out);
    fputs("    return result != 0 ? result : xvoCreateNull();\n", out);
    fputs("}\n\n", out);
}

static void xl6CgenEmitAnonInvokeProto(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, params, returns);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }
    fprintf(ctx->out, " xl6_anon_%d_invoke(void* __xl6_env", info->id);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            fputs(", ", ctx->out);
            fputs(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenEmitVarInvokeArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, unsigned index)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "var") || xl6CgenIsArrayXValueType(concreteType) ||
         xl6CgenIsListXValueType(concreteType) || xl6CgenIsSetXValueType(concreteType) ||
         xl6CgenIsDictXValueType(concreteType) ) {
        fprintf(ctx->out, "__xl6_argv[%u]", index);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        fprintf(ctx->out, "((%s)xvoGetInt(__xl6_argv[%u]))", xl6CgenTypeName(ctx, concreteType), index);
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        fprintf(ctx->out, "((%s)xvoGetFloat(__xl6_argv[%u]))", xl6CgenTypeName(ctx, concreteType), index);
    } else if ( xl6CTextEquals(typeName, "bool") ) {
        fprintf(ctx->out, "xvoGetBool(__xl6_argv[%u])", index);
    } else if ( xl6CTextEquals(typeName, "string") ) {
        fprintf(ctx->out, "xvoGetText(__xl6_argv[%u])", index);
    } else if ( xl6CTextEquals(typeName, "time") ) {
        fprintf(ctx->out, "xvoGetTime(__xl6_argv[%u])", index);
    } else if ( xl6CTextEquals(typeName, "point") ) {
        fprintf(ctx->out, "xvoGetPoint(__xl6_argv[%u])", index);
    } else {
        fputs("0", ctx->out);
    }
}

static int xl6CgenParamListHasArrayVararg(const XL6_AstNode* params, int fixedCount)
{
    return params != NULL && params->childCount > (size_t)fixedCount &&
           xl6CgenParamIsArrayVararg(params->children[fixedCount]);
}

static void xl6CgenEmitVarInvokeLocals(XL6_CgenContext* ctx, int hasArrayVararg)
{
    if ( hasArrayVararg ) {
        fputs("    xvalue __xl6_vararg;\n", ctx->out);
        fputs("    int __xl6_i;\n", ctx->out);
    }
}

static void xl6CgenEmitVarInvokeArityCheck(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    int requiredCount;

    requiredCount = xl6CgenParamRequiredCount(params);
    if ( hasArrayVararg ) {
        fprintf(ctx->out, "    if (__xl6_argc < %u) {\n", (unsigned)requiredCount);
    } else {
        fprintf(ctx->out, "    if (__xl6_argc < %u || __xl6_argc > %u) {\n", (unsigned)requiredCount, (unsigned)fixedCount);
    }
    fputs("        exit(3);\n", ctx->out);
    fputs("    }\n", ctx->out);
}

static void xl6CgenEmitVarInvokeTypeChecks(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    size_t i;
    size_t checkCount;
    int requiredCount;

    (void)hasArrayVararg;
    checkCount = params != NULL ? (size_t)fixedCount : 0;
    requiredCount = xl6CgenParamRequiredCount(params);
    for ( i = 0; params != NULL && i < checkCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        const char* runtimeType = xl6CgenXValueRuntimeTypeConst(ctx, paramType);
        if ( runtimeType != NULL ) {
            if ( (int)i >= requiredCount ) {
                fprintf(ctx->out, "    if (__xl6_argc > %u && xvoType(__xl6_argv[%u]) != %s) {\n", (unsigned)i, (unsigned)i, runtimeType);
            } else {
                fprintf(ctx->out, "    if (xvoType(__xl6_argv[%u]) != %s) {\n", (unsigned)i, runtimeType);
            }
            fputs("        exit(3);\n", ctx->out);
            fputs("    }\n", ctx->out);
        }
    }
}

static void xl6CgenEmitVarInvokeArrayPack(XL6_CgenContext* ctx, int fixedCount, int hasArrayVararg)
{
    if ( !hasArrayVararg ) {
        return;
    }
    fputs("    __xl6_vararg = xvoCreateArray();\n", ctx->out);
    fputs("    if (__xl6_vararg == 0) {\n", ctx->out);
    fputs("        exit(2);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fprintf(ctx->out, "    for (__xl6_i = %u; __xl6_i < __xl6_argc; ++__xl6_i) {\n", (unsigned)fixedCount);
    /* __xl6_argv 的释放仍由 xl6_function_call_var 负责，这里必须追加引用。 */
    fputs("        if (!xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)) {\n", ctx->out);
    fputs("            exit(2);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("    }\n", ctx->out);
}

static void xl6CgenEmitVarInvokeArgOrDefault(XL6_CgenContext* ctx, const XL6_AstNode* param, unsigned index)
{
    const XL6_AstNode* paramType;

    paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    if ( param != NULL && param->childCount > 1 ) {
        fprintf(ctx->out, "((__xl6_argc > %u) ? (", index);
        xl6CgenEmitVarInvokeArgExpr(ctx, paramType, index);
        fputs(") : (", ctx->out);
        xl6CgenExpr(ctx, param->children[1]);
        fputs("))", ctx->out);
    } else {
        xl6CgenEmitVarInvokeArgExpr(ctx, paramType, index);
    }
}

static void xl6CgenEmitVarInvokeCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs(", ", ctx->out);
        if ( hasArrayVararg && i == (size_t)fixedCount ) {
            fputs("__xl6_vararg", ctx->out);
        } else {
            xl6CgenEmitVarInvokeArgOrDefault(ctx, param, (unsigned)i);
        }
    }
}

static void xl6CgenEmitAnonVarInvoke(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenAnonSupportsVarInvoke(ctx, info) ) {
        return;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);

    fprintf(ctx->out, "static xvalue xl6_anon_%d_invoke_var(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)\n{\n", info->id);
    xl6CgenEmitVarInvokeLocals(ctx, hasArrayVararg);
    xl6CgenEmitVarInvokeArityCheck(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeTypeChecks(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeArrayPack(ctx, fixedCount, hasArrayVararg);

    if ( returns != NULL && returns->childCount == 1 ) {
        fputs("    ", ctx->out);
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
        fputs(" __xl6_result = ", ctx->out);
    } else {
        fputs("    ", ctx->out);
    }
    fprintf(ctx->out, "xl6_anon_%d_invoke(__xl6_env", info->id);
    xl6CgenEmitVarInvokeCallArgs(ctx, params, fixedCount, hasArrayVararg);
    fputs(");\n", ctx->out);

    if ( returns != NULL && returns->childCount == 1 ) {
        fputs("    return ", ctx->out);
        xl6CgenXValueFromTypedCExpr(ctx, returns->children[0], "__xl6_result");
        fputs(";\n", ctx->out);
    } else {
        fputs("    return xvoCreateNull();\n", ctx->out);
    }
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitAnonFunction(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* body;
    const XL6_CgenAnonFunc* prevAnon;
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    size_t varMark;
    size_t cleanupMark;
    size_t i;

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    body = info->node->childCount > 2 ? info->node->children[2] : NULL;

    xl6CgenFunctionReturnStructDecl(ctx, params, returns);
    if ( info->captureCount > 0 ) {
        fprintf(ctx->out, "typedef struct XL6_Closure_%d {\n", info->id);
        fputs("    int ref_count;\n", ctx->out);
        for ( i = 0; i < info->captureCount; ++i ) {
            fputs("    ", ctx->out);
            fputs(xl6CgenTypeName(ctx, info->captures[i].type), ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
            fputs(";\n", ctx->out);
        }
        fprintf(ctx->out, "} XL6_Closure_%d;\n\n", info->id);
    }

    fputs("static ", ctx->out);
    xl6CgenEmitAnonInvokeProto(ctx, info);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevAnon = ctx->currentAnonFunc;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    ctx->currentAnonFunc = info;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenRegisterAnonCaptureTypes(ctx, info);
    if ( info->captureCount > 0 ) {
        xl6CgenIndent(ctx);
        fprintf(ctx->out, "XL6_Closure_%d* __env = (XL6_Closure_%d*)__xl6_env;\n", info->id, info->id);
    } else {
        xl6CgenIndent(ctx);
        fputs("(void)__xl6_env;\n", ctx->out);
    }
    xl6CgenBlock(ctx, body);
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentAnonFunc = prevAnon;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);

    if ( info->captureCount > 0 ) {
        fprintf(ctx->out, "static void xl6_anon_%d_retain(void* env)\n{\n", info->id);
        fprintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)env;\n", info->id, info->id);
        fputs("    if (__env != 0) {\n", ctx->out);
        fputs("        __env->ref_count += 1;\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("}\n\n", ctx->out);

        fprintf(ctx->out, "static void xl6_anon_%d_release(void* env)\n{\n", info->id);
        fprintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)env;\n", info->id, info->id);
        fputs("    if (__env == 0) {\n", ctx->out);
        fputs("        return;\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("    __env->ref_count -= 1;\n", ctx->out);
        fputs("    if (__env->ref_count == 0) {\n", ctx->out);
        for ( i = 0; i < info->captureCount; ++i ) {
            if ( xl6CgenIsXValueType(info->captures[i].type) ) {
                fputs("        if (__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(" != 0) {\n", ctx->out);
                fputs("            xvoUnref(__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(");\n", ctx->out);
                fputs("            __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(" = 0;\n", ctx->out);
                fputs("        }\n", ctx->out);
            } else if ( xl6CgenIsFunctionType(info->captures[i].type) ) {
                fputs("        if (__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".release != 0 && __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".env != 0) {\n", ctx->out);
                fputs("            __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".release(__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".env);\n", ctx->out);
                fputs("            __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".env = 0;\n", ctx->out);
                fputs("            __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".release = 0;\n", ctx->out);
                fputs("        }\n", ctx->out);
            }
        }
        fputs("        free(__env);\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }

    xl6CgenEmitAnonVarInvoke(ctx, info);

    fprintf(ctx->out, "static XL6_Function xl6_anon_%d_make(", info->id);
    if ( info->captureCount == 0 ) {
        fputs("void", ctx->out);
    } else {
        for ( i = 0; i < info->captureCount; ++i ) {
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            fputs(xl6CgenTypeName(ctx, info->captures[i].type), ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
        }
    }
    fputs(")\n{\n", ctx->out);
    if ( info->captureCount > 0 ) {
        fprintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)malloc(sizeof(XL6_Closure_%d));\n", info->id, info->id, info->id);
        fputs("    if (__env == 0) {\n", ctx->out);
        fputs("        exit(2);\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("    __env->ref_count = 1;\n", ctx->out);
        for ( i = 0; i < info->captureCount; ++i ) {
            fputs("    __env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
            if ( xl6CgenIsXValueType(info->captures[i].type) ) {
                fputs(" = xl6_xrt_retain(", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputc(')', ctx->out);
            } else {
                fputs(" = ", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
            }
            fputs(";\n", ctx->out);
            if ( xl6CgenIsFunctionType(info->captures[i].type) ) {
                fputs("    if (__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".retain != 0 && __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".env != 0) {\n", ctx->out);
                fputs("        __env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".retain(__env->", ctx->out);
                xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
                fputs(".env);\n", ctx->out);
                fputs("    }\n", ctx->out);
            }
        }
    }
    fputs("    XL6_Function __fn;\n", ctx->out);
    fprintf(ctx->out, "    __fn.signature = 0x%016llXULL;\n", xl6CgenFunctionSignatureId(ctx, params, returns));
    fprintf(ctx->out, "    __fn.env = %s;\n", info->captureCount > 0 ? "__env" : "0");
    fprintf(ctx->out, "    __fn.invoke = (void*)xl6_anon_%d_invoke;\n", info->id);
    if ( ctx->needsFunctionVarRuntime && xl6CgenAnonSupportsVarInvoke(ctx, info) ) {
        fprintf(ctx->out, "    __fn.invoke_var = xl6_anon_%d_invoke_var;\n", info->id);
    } else {
        fputs("    __fn.invoke_var = 0;\n", ctx->out);
    }
    if ( info->captureCount > 0 ) {
        fprintf(ctx->out, "    __fn.retain = xl6_anon_%d_retain;\n", info->id);
        fprintf(ctx->out, "    __fn.release = xl6_anon_%d_release;\n", info->id);
    } else {
        fputs("    __fn.retain = 0;\n", ctx->out);
        fputs("    __fn.release = 0;\n", ctx->out);
    }
    fputs("    return __fn;\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

static int xl6CgenFuncSupportsVarInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( ctx == NULL || func == NULL ) {
        return 0;
    }
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ||
                 !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static void xl6CgenEmitFunctionValueInvokeProto(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, params, returns);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    fputc(' ', ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    fputs("(void* __xl6_env", ctx->out);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            fputs(", ", ctx->out);
            fputs(param != NULL && param->childCount > 0 ? xl6CgenTypeName(ctx, param->children[0]) : "int", ctx->out);
            fputc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
        }
    }
    fputc(')', ctx->out);
}

static void xl6CgenEmitFunctionValueInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;

    xl6CgenFunctionReturnStructDecl(ctx, params, returns);
    fputs("static ", ctx->out);
    xl6CgenEmitFunctionValueInvokeProto(ctx, func);
    fputs("\n{\n", ctx->out);
    fputs("    (void)__xl6_env;\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        fputs("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, func);
        fputs(" __xl6_src = ", ctx->out);
        xl6CgenFuncSymbol(ctx, func);
        fputc('(', ctx->out);
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                if ( i > 0 ) {
                    fputs(", ", ctx->out);
                }
                xl6CgenFputsCIdent(ctx->out, params->children[i] != NULL ? params->children[i]->text : "_");
            }
        }
        fputs(");\n", ctx->out);
        fputs("    ", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, params, returns);
        fputs(" __xl6_out;\n", ctx->out);
        for ( i = 0; i < returns->childCount; ++i ) {
            fprintf(ctx->out, "    __xl6_out.value%u = __xl6_src.value%u;\n", (unsigned)i, (unsigned)i);
        }
        fputs("    return __xl6_out;\n", ctx->out);
        fputs("}\n\n", ctx->out);
        return;
    }
    fputs("    ", ctx->out);
    if ( returns != NULL && returns->childCount > 0 ) {
        fputs("return ", ctx->out);
    }
    xl6CgenFuncSymbol(ctx, func);
    fputc('(', ctx->out);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, params->children[i] != NULL ? params->children[i]->text : "_");
        }
    }
    fputs(");\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueVarInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenFuncSupportsVarInvoke(ctx, func) ) {
        return;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);

    fputs("static xvalue ", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke_var");
    fputs("(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)\n{\n", ctx->out);
    xl6CgenEmitVarInvokeLocals(ctx, hasArrayVararg);
    xl6CgenEmitVarInvokeArityCheck(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeTypeChecks(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeArrayPack(ctx, fixedCount, hasArrayVararg);

    if ( returns != NULL && returns->childCount == 1 ) {
        fputs("    ", ctx->out);
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
        fputs(" __xl6_result = ", ctx->out);
    } else {
        fputs("    ", ctx->out);
    }
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    fputs("(__xl6_env", ctx->out);
    xl6CgenEmitVarInvokeCallArgs(ctx, params, fixedCount, hasArrayVararg);
    fputs(");\n", ctx->out);

    if ( returns != NULL && returns->childCount == 1 ) {
        fputs("    return ", ctx->out);
        xl6CgenXValueFromTypedCExpr(ctx, returns->children[0], "__xl6_result");
        fputs(";\n", ctx->out);
    } else {
        fputs("    return xvoCreateNull();\n", ctx->out);
    }
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueMake(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;

    fputs("static XL6_Function ", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_make");
    fputs("(void)\n{\n", ctx->out);
    fputs("    XL6_Function __fn;\n", ctx->out);
    fprintf(ctx->out, "    __fn.signature = 0x%016llXULL;\n", xl6CgenFunctionSignatureId(ctx, params, returns));
    fputs("    __fn.env = 0;\n", ctx->out);
    fputs("    __fn.invoke = (void*)", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    fputs(";\n", ctx->out);
    if ( ctx->needsFunctionVarRuntime && xl6CgenFuncSupportsVarInvoke(ctx, func) ) {
        fputs("    __fn.invoke_var = ", ctx->out);
        xl6CgenFunctionValueSymbol(ctx, func, "_invoke_var");
        fputs(";\n", ctx->out);
    } else {
        fputs("    __fn.invoke_var = 0;\n", ctx->out);
    }
    fputs("    __fn.retain = 0;\n", ctx->out);
    fputs("    __fn.release = 0;\n", ctx->out);
    fputs("    return __fn;\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterInvokeProto(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* vararg;
    int fixedCount;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructNameForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        fputs("void", ctx->out);
    }

    fputc(' ', ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
    fputs("(void* __xl6_env", ctx->out);
    for ( i = 0; params != NULL && i < paramCount && (size_t)i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs(", ", ctx->out);
        fputs(param != NULL && param->childCount > 0 ? xl6CgenTypeName(ctx, param->children[0]) : "int", ctx->out);
        fputc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
    }
    if ( includeVararg && params != NULL && params->childCount > (size_t)fixedCount ) {
        vararg = params->children[fixedCount];
        fputs(", ", ctx->out);
        fputs(vararg != NULL && vararg->childCount > 0 ? xl6CgenTypeName(ctx, vararg->children[0]) : "xvalue", ctx->out);
        fputs(" __xl6_vararg", ctx->out);
    }
    fputc(')', ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int actualHasVararg;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)fixedCount;

    xl6CgenFunctionReturnStructDeclForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
    fputs("static ", ctx->out);
    xl6CgenEmitFunctionValueAdapterInvokeProto(ctx, func, paramCount, includeVararg);
    fputs("\n{\n", ctx->out);
    fputs("    (void)__xl6_env;\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        fputs("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, func);
        fputs(" __xl6_src = ", ctx->out);
    } else {
        fputs("    ", ctx->out);
        if ( returns != NULL && returns->childCount > 0 ) {
            fputs("return ", ctx->out);
        }
    }
    xl6CgenFuncSymbol(ctx, func);
    fputc('(', ctx->out);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        if ( i > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( i < paramCount ) {
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
        } else if ( param != NULL && param->childCount > 1 ) {
            /* function 适配器在这里补齐省略的可选参数默认值。 */
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "function 适配器缺少可选参数默认值");
            fputs("0", ctx->out);
        }
    }
    if ( actualHasVararg ) {
        if ( fixedCount > 0 ) {
            fputs(", ", ctx->out);
        }
        if ( includeVararg ) {
            fputs("__xl6_vararg", ctx->out);
        } else {
            xl6CgenError(ctx, func, "function 适配器暂不支持省略可变参数");
            fputs("xvoCreateNull()", ctx->out);
        }
    }
    fputs(");\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        fputs("    ", ctx->out);
        xl6CgenFunctionReturnStructNameForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
        fputs(" __xl6_out;\n", ctx->out);
        for ( i = 0; i < (int)returns->childCount; ++i ) {
            fprintf(ctx->out, "    __xl6_out.value%d = __xl6_src.value%d;\n", i, i);
        }
        fputs("    return __xl6_out;\n", ctx->out);
    }
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterMake(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;

    fputs("static XL6_Function ", ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_make");
    fputs("(void)\n{\n", ctx->out);
    fputs("    XL6_Function __fn;\n", ctx->out);
    fprintf(ctx->out,
            "    __fn.signature = 0x%016llXULL;\n",
            xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg));
    fputs("    __fn.env = 0;\n", ctx->out);
    fputs("    __fn.invoke = (void*)", ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
    fputs(";\n", ctx->out);
    fputs("    __fn.invoke_var = 0;\n", ctx->out);
    fputs("    __fn.retain = 0;\n", ctx->out);
    fputs("    __fn.release = 0;\n", ctx->out);
    fputs("    return __fn;\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueOptionalAdapters(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int requiredCount;
    int fixedCount;
    int hasVararg;
    int count;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    requiredCount = xl6CgenParamRequiredCount(params);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( requiredCount >= fixedCount ) {
        return;
    }
    hasVararg = params != NULL && params->childCount > (size_t)fixedCount;
    for ( count = requiredCount; count < fixedCount; ++count ) {
        int includeVararg = hasVararg ? 1 : 0;
        xl6CgenEmitFunctionValueAdapterInvoke(ctx, func, count, includeVararg);
        xl6CgenEmitFunctionValueAdapterMake(ctx, func, count, includeVararg);
    }
}

static void xl6CgenEmitFunctionValueWrappers(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* func = ctx->root->children[i];
        if ( func == NULL || func->kind != XL6_AST_FUNC || xl6CgenFuncIsGeneric(func) ) {
            continue;
        }
        xl6CgenEmitFunctionValueInvoke(ctx, func);
        xl6CgenEmitFunctionValueVarInvoke(ctx, func);
        xl6CgenEmitFunctionValueMake(ctx, func);
        xl6CgenEmitFunctionValueOptionalAdapters(ctx, func);
    }
}

static void xl6CgenEmitAnonFunctions(XL6_CgenContext* ctx)
{
    size_t i;

    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        xl6CgenEmitAnonFunction(ctx, &ctx->anonFuncs[i]);
    }
}

static void xl6CgenFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenFuncProto(ctx, node);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = node;
    xl6CgenRegisterParamTypes(ctx, params);
    body = node->childCount > 3 ? node->children[3] : NULL;
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenExportFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;

    xl6CgenExportInternalProto(ctx, node);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevReturnType = ctx->currentReturnType;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( node->childCount > 3 ) {
        xl6CgenBlock(ctx, node->children[3]);
    }
    ctx->currentReturnType = prevReturnType;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static void xl6CgenExportWrapper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    xl6CgenExportWrapperProto(ctx, node);
    fputs("\n{\n", ctx->out);
    ctx->indent += 1;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    xl6CgenIndent(ctx);
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs("return ", ctx->out);
    }
    xl6CgenExportInternalSymbol(ctx->out, node);
    fputc('(', ctx->out);
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                fputs(", ", ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, params->children[i] != NULL ? params->children[i]->text : NULL);
        }
    }
    fputs(");\n", ctx->out);
    ctx->indent -= 1;
    fputs("}\n\n", ctx->out);
}

static int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CTextEquals(node->text, "in") &&
         node->childCount > 1 && xl6CgenIsRangeCall(node->children[1]) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRangeContains(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CTextEquals(node->text, "**") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CTextEquals(node->text, "**=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsIntPow(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CTextEquals(node->text, "function") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsApprox(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CTextEquals(node->text, "~=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsApprox(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TIME ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTimeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsStringRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenBinaryPlusIsString(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenBinaryCompareIsString(ctx, node) || xl6CgenBinaryApproxIsString(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenBinaryInIsString(ctx, node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CTextEquals(node->text, "+=") &&
         node->childCount > 1 &&
         xl6CgenExprStaticTypeIs(ctx, node->children[0], "string") &&
         xl6CgenExprStaticTypeIs(ctx, node->children[1], "string") ) {
        return 1;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsStringRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenAddVarType(ctx, node->text, inferredType);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenAddVarType(ctx, node->text, typeNode);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 3]
                                      : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( node->kind == XL6_AST_FUNC && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsStringRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsStringRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsStringRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsStringRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsStringRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenTypeConvertCallNeedsRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* arg;
    XL6_AstNode* sourceType;
    const char* target;
    const char* source;
    int result;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL || node->children[0]->kind != XL6_AST_NAME ) {
        return 0;
    }
    callee = node->children[0];
    target = callee->text;
    if ( !xl6CgenIsBuiltinConvertTypeName(target) || !xl6CgenIsKnownTypeName(ctx, target) ) {
        return 0;
    }

    arg = xl6CgenTypeConvertArg(node);
    sourceType = xl6CgenCloneExprTypeForInference(ctx, arg);
    source = sourceType != NULL ? sourceType->text : NULL;
    result = 0;

    if ( (xl6CgenIsIntegerTypeName(target) || xl6CgenIsFloatTypeName(target) || xl6CTextEquals(target, "time")) &&
         xl6CTextEquals(source, "string") ) {
        result = 1;
    } else if ( xl6CTextEquals(target, "string") &&
                (xl6CgenIsIntegerTypeName(source) || xl6CgenIsFloatTypeName(source) ||
                 xl6CTextEquals(source, "time") || xl6CTextEquals(source, "point") ||
                 xl6CTextEquals(source, "var")) ) {
        result = 1;
    }

    xl6AstFree(sourceType);
    return result;
}

static int xl6CgenNodeNeedsTypeConvertRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenTypeConvertCallNeedsRuntime(ctx, node) ) {
        return 1;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsTypeConvertRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenAddVarType(ctx, node->text, inferredType);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenAddVarType(ctx, node->text, typeNode);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 3]
                                      : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( node->kind == XL6_AST_FUNC && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_AstNode* iterableType;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        iterableType = xl6CgenCloneExprTypeForInference(ctx, iterable);
        if ( targets != NULL ) {
            for ( i = 0; i < targets->childCount; ++i ) {
                const char* typeName = xl6CgenForTargetTypeName(iterableType, i, targets->childCount);
                xl6CgenRegisterLoopTargetType(ctx, targets->children[i], typeName);
            }
        }
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        xl6AstFree(iterableType);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsTypeConvertRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsFunctionVarRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }

    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        const XL6_AstNode* functionType = xl6CgenFindVarType(ctx, node->children[0]->text);
        if ( xl6CgenIsBareFunctionType(functionType) ) {
            return 1;
        }
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsFunctionVarRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenAddVarType(ctx, node->text, inferredType);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenAddVarType(ctx, node->text, typeNode);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 2 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 2 : 3]
                                      : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( node->kind == XL6_AST_FUNC && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_AstNode* iterableType;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        iterableType = xl6CgenCloneExprTypeForInference(ctx, iterable);
        if ( targets != NULL ) {
            for ( i = 0; i < targets->childCount; ++i ) {
                const char* typeName = xl6CgenForTargetTypeName(iterableType, i, targets->childCount);
                xl6CgenRegisterLoopTargetType(ctx, targets->children[i], typeName);
            }
        }
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        xl6AstFree(iterableType);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsFunctionVarRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsXValueType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "var") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ARRAY_LITERAL || node->kind == XL6_AST_LIST_LITERAL ||
         node->kind == XL6_AST_SET_LITERAL || node->kind == XL6_AST_DICT_LITERAL ||
         node->kind == XL6_AST_TYPED_LITERAL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXValueRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitApproxProtos(FILE* out)
{
    fputs("#include <stdbool.h>\n", out);
    fputs("bool xrtIntApprox(long long a, long long b);\n", out);
    fputs("bool xrtNumApprox(double a, double b);\n\n", out);
    fputs("bool xrtTimeApprox(long long a, long long b);\n\n", out);
}

static void xl6CgenEmitTimeRuntime(FILE* out)
{
    fputs("long long xrtTimeSerial(int iHour, int iMinute, int iSecond);\n", out);
    fputs("long long xrtDateSerial(long long iYear, int iMonth, int iDay);\n", out);
    fputs("long long xrtDateTimeSerial(long long iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);\n\n", out);
}

static void xl6CgenEmitStringRuntime(FILE* out)
{
    fputs("#include <stdbool.h>\n", out);
    fputs("#include <stddef.h>\n", out);
    fputs("#include <string.h>\n", out);
    fputs("void* xrtMalloc(size_t iSize);\n\n", out);
    fputs("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n", out);
    fputs("const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);\n", out);
    fputs("bool xrtStrApprox(const char* s1, size_t len1, const char* s2, size_t len2);\n\n", out);

    fputs("static const char* xl6_xrt_str_concat(const char* left, const char* right)\n", out);
    fputs("{\n", out);
    fputs("    size_t leftLen = left != 0 ? strlen(left) : 0;\n", out);
    fputs("    size_t rightLen = right != 0 ? strlen(right) : 0;\n", out);
    fputs("    size_t totalLen = leftLen + rightLen;\n", out);
    fputs("    char* text;\n\n", out);
    fputs("    if ( totalLen == 0 ) {\n", out);
    fputs("        return \"\";\n", out);
    fputs("    }\n", out);
    fputs("    text = (char*)xrtMalloc(totalLen + 1);\n", out);
    fputs("    if ( text == 0 ) {\n", out);
    fputs("        return \"\";\n", out);
    fputs("    }\n", out);
    fputs("    if ( leftLen > 0 ) {\n", out);
    fputs("        memcpy(text, left, leftLen);\n", out);
    fputs("    }\n", out);
    fputs("    if ( rightLen > 0 ) {\n", out);
    fputs("        memcpy(text + leftLen, right, rightLen);\n", out);
    fputs("    }\n", out);
    fputs("    text[totalLen] = 0;\n", out);
    fputs("    return text;\n", out);
    fputs("}\n\n", out);
}

static void xl6CgenEmitTypeConvertRuntime(FILE* out)
{
    fputs("#include <stdbool.h>\n", out);
    fputs("#include <stddef.h>\n", out);
    fputs("#include <string.h>\n", out);
    fputs("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    fputs("#define XL6_XVALUE_TYPEDEF\n", out);
    fputs("typedef struct xvalue_struct* xvalue;\n", out);
    fputs("#endif\n", out);
    fputs("void* xrtMalloc(size_t iSize);\n", out);
    fputs("long long xrtStrToI64(const void* pStr);\n", out);
    fputs("double xrtStrToNum(const void* pStr);\n", out);
    fputs("int xrtI64ToStr(long long num, char* buffer);\n", out);
    fputs("int xrtNumToStr(double num, char* buffer);\n", out);
    fputs("long long xrtStrToTime(const char* sTime, size_t iSize);\n", out);
    fputs("const char* xrtTimeToStr(long long iTime, int iFormat);\n", out);
    fputs("const char* xrtFormat(const void* sFormat, ...);\n\n", out);
    fputs("int xvoType(xvalue pVal);\n", out);
    fputs("bool xvoGetBool(xvalue pVal);\n", out);
    fputs("long long xvoGetInt(xvalue pVal);\n", out);
    fputs("double xvoGetFloat(xvalue pVal);\n", out);
    fputs("long long xvoGetTime(xvalue pVal);\n", out);
    fputs("void* xvoGetPoint(xvalue pVal);\n", out);
    fputs("const char* xvoGetText(xvalue pVal);\n\n", out);

    fputs("static const char* xl6_xrt_i64_to_string(long long value)\n", out);
    fputs("{\n", out);
    fputs("    char buffer[64];\n", out);
    fputs("    int len = xrtI64ToStr(value, buffer);\n", out);
    fputs("    char* text = (char*)xrtMalloc((size_t)len + 1);\n", out);
    fputs("    if ( text == 0 ) {\n", out);
    fputs("        return \"\";\n", out);
    fputs("    }\n", out);
    fputs("    memcpy(text, buffer, (size_t)len + 1);\n", out);
    fputs("    return text;\n", out);
    fputs("}\n\n", out);

    fputs("static const char* xl6_xrt_f64_to_string(double value)\n", out);
    fputs("{\n", out);
    fputs("    char buffer[128];\n", out);
    fputs("    int len = xrtNumToStr(value, buffer);\n", out);
    fputs("    char* text = (char*)xrtMalloc((size_t)len + 1);\n", out);
    fputs("    if ( text == 0 ) {\n", out);
    fputs("        return \"\";\n", out);
    fputs("    }\n", out);
    fputs("    memcpy(text, buffer, (size_t)len + 1);\n", out);
    fputs("    return text;\n", out);
    fputs("}\n\n", out);

    fputs("static const char* xl6_xrt_value_to_string(xvalue value)\n", out);
    fputs("{\n", out);
    fputs("    enum {\n", out);
    fputs("        XL6_CONV_XVO_DT_NULL = 0,\n", out);
    fputs("        XL6_CONV_XVO_DT_BOOL = 1,\n", out);
    fputs("        XL6_CONV_XVO_DT_INT = 2,\n", out);
    fputs("        XL6_CONV_XVO_DT_FLOAT = 3,\n", out);
    fputs("        XL6_CONV_XVO_DT_TEXT = 4,\n", out);
    fputs("        XL6_CONV_XVO_DT_TIME = 5,\n", out);
    fputs("        XL6_CONV_XVO_DT_POINT = 6\n", out);
    fputs("    };\n", out);
    fputs("    int type = xvoType(value);\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_NULL ) {\n", out);
    fputs("        return \"\";\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_BOOL ) {\n", out);
    fputs("        return xvoGetBool(value) ? \"true\" : \"false\";\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_INT ) {\n", out);
    fputs("        return xl6_xrt_i64_to_string(xvoGetInt(value));\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_FLOAT ) {\n", out);
    fputs("        return xl6_xrt_f64_to_string(xvoGetFloat(value));\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_TEXT ) {\n", out);
    fputs("        return xvoGetText(value);\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_TIME ) {\n", out);
    fputs("        return xrtTimeToStr(xvoGetTime(value), 0);\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_CONV_XVO_DT_POINT ) {\n", out);
    fputs("        return xrtFormat(\"[point:%p]\", xvoGetPoint(value));\n", out);
    fputs("    }\n", out);
    fputs("    return xvoGetText(value);\n", out);
    fputs("}\n\n", out);
}

static void xl6CgenEmitXValueRuntime(XL6_CgenContext* ctx)
{
    FILE* out = ctx->out;

    fputs("#include <stdarg.h>\n", out);
    fputs("#include <stdbool.h>\n", out);
    fputs("#include <stddef.h>\n", out);
    fputs("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    fputs("#define XL6_XVALUE_TYPEDEF\n", out);
    fputs("typedef struct xvalue_struct* xvalue;\n", out);
    fputs("#endif\n", out);
    fputs("typedef void* xlist;\n", out);
    fputs("typedef void* xavltbase;\n", out);
    fputs("typedef struct Dict_Key {\n", out);
    fputs("    void* Key;\n", out);
    fputs("    unsigned int KeyLen;\n", out);
    fputs("    unsigned int Hash;\n", out);
    fputs("} Dict_Key;\n", out);
    fputs("typedef struct Coll_Key {\n", out);
    fputs("    unsigned long long Hash;\n", out);
    fputs("    xvalue Value;\n", out);
    fputs("} Coll_Key;\n", out);
    fputs("typedef bool (*List_EachProc)(long long pKey, void* pVal, void* pArg);\n", out);
    fputs("const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);\n", out);
    fputs("xvalue xvoCreateNull(void);\n", out);
    fputs("xvalue xvoCreateBool(bool bVal);\n", out);
    fputs("xvalue xvoCreateInt(long long iVal);\n", out);
    fputs("xvalue xvoCreateFloat(double fVal);\n", out);
    fputs("xvalue xvoCreateTime(long long tVal);\n", out);
    fputs("xvalue xvoCreatePoint(void* point);\n", out);
    fputs("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    fputs("xvalue xvoCreateArray(void);\n", out);
    fputs("xvalue xvoCreateList(void);\n", out);
    fputs("xvalue xvoCreateColl(void);\n", out);
    fputs("xvalue xvoCreateTable(void);\n", out);
    fputs("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    fputs("bool xvoArraySetValue(xvalue pArr, unsigned int index, xvalue pVal, bool bColloc);\n", out);
    fputs("bool xvoListSetValue(xvalue pList, long long index, xvalue pVal, bool bColloc);\n", out);
    fputs("bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);\n", out);
    fputs("bool xvoTableSetValue(xvalue pTbl, const void* key, unsigned int kl, xvalue pVal, bool bColloc);\n", out);
    fputs("xvalue xvoArrayGetValue(xvalue pArr, unsigned int index);\n", out);
    fputs("xvalue xvoListGetValue(xvalue pList, long long index);\n", out);
    fputs("xvalue xvoTableGetValue(xvalue pTbl, const void* key, unsigned int kl);\n", out);
    fputs("unsigned int xvoArrayItemCount(xvalue pArr);\n", out);
    fputs("unsigned int xvoListItemCount(xvalue pList);\n", out);
    fputs("unsigned int xvoCollItemCount(xvalue pColl);\n", out);
    fputs("unsigned int xvoTableItemCount(xvalue pTbl);\n", out);
    fputs("xlist xvoGetList(xvalue pVal);\n", out);
    fputs("xavltbase xvoGetColl(xvalue pVal);\n", out);
    fputs("xavltbase xvoGetTable(xvalue pVal);\n", out);
    fputs("void xrtAVLTB_IterBegin(xavltbase objAVLT);\n", out);
    fputs("void* xrtAVLTB_IterNext(xavltbase objAVLT);\n", out);
    fputs("void xrtAVLTB_IterEnd(xavltbase objAVLT);\n", out);
    fputs("void xrtListWalk(xlist objList, List_EachProc procEach, void* pArg);\n", out);
    fputs("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n", out);
    fputs("bool xvoCollExists(xvalue pColl, xvalue pVal);\n", out);
    fputs("bool xvoTableExists(xvalue pTbl, const char* key, unsigned int kl);\n", out);
    fputs("int xvoType(xvalue pVal);\n", out);
    fputs("bool xvoGetBool(xvalue pVal);\n", out);
    fputs("long long xvoGetInt(xvalue pVal);\n", out);
    fputs("double xvoGetFloat(xvalue pVal);\n", out);
    fputs("long long xvoGetTime(xvalue pVal);\n", out);
    fputs("void* xvoGetPoint(xvalue pVal);\n", out);
    fputs("const char* xvoGetText(xvalue pVal);\n", out);
    fputs("bool xvoIsNull(xvalue pVal);\n", out);
    fputs("void xvoAddRef(xvalue pVal);\n", out);
    fputs("void xvoUnref(xvalue pVal);\n\n", out);

    fputs("#define XL6_XVO_DT_NULL 0\n", out);
    fputs("#define XL6_XVO_DT_BOOL 1\n", out);
    fputs("#define XL6_XVO_DT_INT 2\n", out);
    fputs("#define XL6_XVO_DT_FLOAT 3\n", out);
    fputs("#define XL6_XVO_DT_TEXT 4\n", out);
    fputs("#define XL6_XVO_DT_TIME 5\n", out);
    fputs("#define XL6_XVO_DT_POINT 6\n", out);
    fputs("#define XL6_XVO_DT_ARRAY 8\n", out);
    fputs("#define XL6_XVO_DT_LIST 9\n", out);
    fputs("#define XL6_XVO_DT_COLL 10\n", out);
    fputs("#define XL6_XVO_DT_TABLE 11\n\n", out);

    fputs("static unsigned long long xl6_xrt_type_value_id(xvalue value, bool release)\n", out);
    fputs("{\n", out);
    fputs("    unsigned long long result;\n", out);
    fputs("    int type;\n\n", out);
    fputs("    if ( value == 0 ) {\n", out);
    fputs("        return ", out);
    xl6CgenEmitTypeIdForName(ctx, "null", NULL);
    fputs(";\n", out);
    fputs("    }\n", out);
    fputs("    type = xvoType(value);\n", out);
    fputs("    switch ( type ) {\n", out);
    fputs("    case XL6_XVO_DT_NULL:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "null", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_BOOL:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "bool", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_INT:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "int", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_FLOAT:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "float", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_TEXT:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "string", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_TIME:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "time", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_POINT:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "point", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_ARRAY:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "array", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_LIST:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "list", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_COLL:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "set", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    case XL6_XVO_DT_TABLE:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "dict", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    default:\n", out);
    fputs("        result = ", out);
    xl6CgenEmitTypeIdForName(ctx, "var", NULL);
    fputs(";\n", out);
    fputs("        break;\n", out);
    fputs("    }\n", out);
    fputs("    if ( release ) {\n", out);
    fputs("        xvoUnref(value);\n", out);
    fputs("    }\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_retain(xvalue value)\n", out);
    fputs("{\n", out);
    fputs("    if ( value != 0 ) {\n", out);
    fputs("        xvoAddRef(value);\n", out);
    fputs("    }\n", out);
    fputs("    return value;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_index_safe_get_i64(xvalue values, long long index)\n", out);
    fputs("{\n", out);
    fputs("    int type;\n", out);
    fputs("    if ( values == 0 || xvoIsNull(values) ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    type = xvoType(values);\n", out);
    fputs("    if ( type == XL6_XVO_DT_ARRAY ) {\n", out);
    fputs("        return xvoArrayGetValue(values, (unsigned int)index);\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_XVO_DT_LIST ) {\n", out);
    fputs("        return xvoListGetValue(values, index);\n", out);
    fputs("    }\n", out);
    fputs("    return xvoCreateNull();\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_index_safe_get_key(xvalue values, const char* key)\n", out);
    fputs("{\n", out);
    fputs("    if ( values == 0 || xvoIsNull(values) || xvoType(values) != XL6_XVO_DT_TABLE ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    return xvoTableGetValue(values, key, 0);\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_member_safe_length(xvalue values)\n", out);
    fputs("{\n", out);
    fputs("    int type;\n", out);
    fputs("    if ( values == 0 || xvoIsNull(values) ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    type = xvoType(values);\n", out);
    fputs("    if ( type == XL6_XVO_DT_ARRAY ) {\n", out);
    fputs("        return xvoCreateInt((long long)xvoArrayItemCount(values));\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_XVO_DT_LIST ) {\n", out);
    fputs("        return xvoCreateInt((long long)xvoListItemCount(values));\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_XVO_DT_COLL ) {\n", out);
    fputs("        return xvoCreateInt((long long)xvoCollItemCount(values));\n", out);
    fputs("    }\n", out);
    fputs("    if ( type == XL6_XVO_DT_TABLE ) {\n", out);
    fputs("        return xvoCreateInt((long long)xvoTableItemCount(values));\n", out);
    fputs("    }\n", out);
    fputs("    return xvoCreateNull();\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_type_is_numeric(int type)\n", out);
    fputs("{\n", out);
    fputs("    return type == XL6_XVO_DT_BOOL || type == XL6_XVO_DT_INT || type == XL6_XVO_DT_FLOAT;\n", out);
    fputs("}\n\n", out);

    fputs("static double xl6_xrt_value_to_double(xvalue value, int type)\n", out);
    fputs("{\n", out);
    fputs("    if ( type == XL6_XVO_DT_FLOAT ) {\n", out);
    fputs("        return xvoGetFloat(value);\n", out);
    fputs("    }\n", out);
    fputs("    return (double)xvoGetInt(value);\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_basic_equal(xvalue left, xvalue right)\n", out);
    fputs("{\n", out);
    fputs("    int leftType = xvoType(left);\n", out);
    fputs("    int rightType = xvoType(right);\n\n", out);
    fputs("    if ( leftType == XL6_XVO_DT_NULL || rightType == XL6_XVO_DT_NULL ) {\n", out);
    fputs("        return leftType == rightType;\n", out);
    fputs("    }\n", out);
    fputs("    if ( xl6_xrt_type_is_numeric(leftType) && xl6_xrt_type_is_numeric(rightType) ) {\n", out);
    fputs("        if ( leftType == XL6_XVO_DT_FLOAT || rightType == XL6_XVO_DT_FLOAT ) {\n", out);
    fputs("            return xl6_xrt_value_to_double(left, leftType) == xl6_xrt_value_to_double(right, rightType);\n", out);
    fputs("        }\n", out);
    fputs("        return xvoGetInt(left) == xvoGetInt(right);\n", out);
    fputs("    }\n", out);
    fputs("    if ( leftType == XL6_XVO_DT_TEXT && rightType == XL6_XVO_DT_TEXT ) {\n", out);
    fputs("        return xrtStrComp(xvoGetText(left), xvoGetText(right), 0, false) == 0;\n", out);
    fputs("    }\n", out);
    fputs("    if ( leftType == XL6_XVO_DT_TIME && rightType == XL6_XVO_DT_TIME ) {\n", out);
    fputs("        return xvoGetTime(left) == xvoGetTime(right);\n", out);
    fputs("    }\n", out);
    fputs("    return 0;\n", out);
    fputs("}\n\n", out);

    fputs("typedef struct XL6_XrtListContainsCtx {\n", out);
    fputs("    xvalue item;\n", out);
    fputs("    int found;\n", out);
    fputs("} XL6_XrtListContainsCtx;\n\n", out);

    fputs("static bool xl6_xrt_list_contains_each(long long key, void* value, void* arg)\n", out);
    fputs("{\n", out);
    fputs("    XL6_XrtListContainsCtx* ctx = (XL6_XrtListContainsCtx*)arg;\n", out);
    fputs("    xvalue* slot = (xvalue*)value;\n", out);
    fputs("    (void)key;\n\n", out);
    fputs("    if ( ctx != 0 && slot != 0 && xl6_xrt_basic_equal(*slot, ctx->item) ) {\n", out);
    fputs("        ctx->found = 1;\n", out);
    fputs("        return true;\n", out);
    fputs("    }\n", out);
    fputs("    return false;\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_array_contains(xvalue values, xvalue item)\n", out);
    fputs("{\n", out);
    fputs("    unsigned int count = xvoArrayItemCount(values);\n", out);
    fputs("    unsigned int i;\n", out);
    fputs("    int found = 0;\n\n", out);
    fputs("    for ( i = 0; i < count; ++i ) {\n", out);
    fputs("        if ( xl6_xrt_basic_equal(xvoArrayGetValue(values, i), item) ) {\n", out);
    fputs("            found = 1;\n", out);
    fputs("            break;\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    if ( item != 0 ) {\n", out);
    fputs("        xvoUnref(item);\n", out);
    fputs("    }\n", out);
    fputs("    return found;\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_list_contains(xvalue values, xvalue item)\n", out);
    fputs("{\n", out);
    fputs("    XL6_XrtListContainsCtx ctx;\n", out);
    fputs("    xlist list = xvoGetList(values);\n\n", out);
    fputs("    ctx.item = item;\n", out);
    fputs("    ctx.found = 0;\n", out);
    fputs("    if ( list != 0 ) {\n", out);
    fputs("        xrtListWalk(list, xl6_xrt_list_contains_each, &ctx);\n", out);
    fputs("    }\n", out);
    fputs("    if ( item != 0 ) {\n", out);
    fputs("        xvoUnref(item);\n", out);
    fputs("    }\n", out);
    fputs("    return ctx.found;\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_set_contains(xvalue values, xvalue item)\n", out);
    fputs("{\n", out);
    fputs("    int found = xvoCollExists(values, item) ? 1 : 0;\n", out);
    fputs("    if ( item != 0 ) {\n", out);
    fputs("        xvoUnref(item);\n", out);
    fputs("    }\n", out);
    fputs("    return found;\n", out);
    fputs("}\n\n", out);

    fputs("static int xl6_xrt_value_contains(xvalue values, xvalue item)\n", out);
    fputs("{\n", out);
    fputs("    int valuesType = xvoType(values);\n", out);
    fputs("    int itemType = xvoType(item);\n", out);
    fputs("    int found = 0;\n\n", out);
    fputs("    if ( valuesType == XL6_XVO_DT_ARRAY ) {\n", out);
    fputs("        return xl6_xrt_array_contains(values, item);\n", out);
    fputs("    }\n", out);
    fputs("    if ( valuesType == XL6_XVO_DT_LIST ) {\n", out);
    fputs("        return xl6_xrt_list_contains(values, item);\n", out);
    fputs("    }\n", out);
    fputs("    if ( valuesType == XL6_XVO_DT_COLL ) {\n", out);
    fputs("        return xl6_xrt_set_contains(values, item);\n", out);
    fputs("    }\n", out);
    fputs("    if ( valuesType == XL6_XVO_DT_TABLE ) {\n", out);
    fputs("        if ( itemType == XL6_XVO_DT_TEXT ) {\n", out);
    fputs("            found = xvoTableExists(values, xvoGetText(item), 0) ? 1 : 0;\n", out);
    fputs("        }\n", out);
    fputs("    } else if ( valuesType == XL6_XVO_DT_TEXT && itemType == XL6_XVO_DT_TEXT ) {\n", out);
    fputs("        found = xrtFindStr(xvoGetText(values), 0, xvoGetText(item), 0, false) != 0;\n", out);
    fputs("    }\n", out);
    fputs("    if ( item != 0 ) {\n", out);
    fputs("        xvoUnref(item);\n", out);
    fputs("    }\n", out);
    fputs("    return found;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_array_varargs(int count, ...)\n", out);
    fputs("{\n", out);
    fputs("    xvalue arg = xvoCreateArray();\n", out);
    fputs("    va_list ap;\n", out);
    fputs("    int i;\n\n", out);
    fputs("    if ( arg == 0 ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    va_start(ap, count);\n", out);
    fputs("    for ( i = 0; i < count; ++i ) {\n", out);
    fputs("        xvalue item = va_arg(ap, xvalue);\n", out);
    fputs("        if ( !xvoArrayAppendValue(arg, item, true) && item != 0 ) {\n", out);
    fputs("            xvoUnref(item);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    va_end(ap);\n", out);
    fputs("    return arg;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_list_literal(int count, ...)\n", out);
    fputs("{\n", out);
    fputs("    xvalue list = xvoCreateList();\n", out);
    fputs("    va_list ap;\n", out);
    fputs("    int i;\n\n", out);
    fputs("    if ( list == 0 ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    va_start(ap, count);\n", out);
    fputs("    for ( i = 0; i < count; ++i ) {\n", out);
    fputs("        long long index = va_arg(ap, long long);\n", out);
    fputs("        xvalue item = va_arg(ap, xvalue);\n", out);
    fputs("        if ( !xvoListSetValue(list, index, item, true) && item != 0 ) {\n", out);
    fputs("            xvoUnref(item);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    va_end(ap);\n", out);
    fputs("    return list;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_set_literal(int count, ...)\n", out);
    fputs("{\n", out);
    fputs("    xvalue set = xvoCreateColl();\n", out);
    fputs("    va_list ap;\n", out);
    fputs("    int i;\n\n", out);
    fputs("    if ( set == 0 ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    va_start(ap, count);\n", out);
    fputs("    for ( i = 0; i < count; ++i ) {\n", out);
    fputs("        xvalue item = va_arg(ap, xvalue);\n", out);
    fputs("        if ( !xvoCollSetValue(set, item, true) && item != 0 ) {\n", out);
    fputs("            xvoUnref(item);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    va_end(ap);\n", out);
    fputs("    return set;\n", out);
    fputs("}\n\n", out);

    fputs("static xvalue xl6_xrt_dict_varargs(int count, ...)\n", out);
    fputs("{\n", out);
    fputs("    xvalue arg = xvoCreateTable();\n", out);
    fputs("    va_list ap;\n", out);
    fputs("    int i;\n\n", out);
    fputs("    if ( arg == 0 ) {\n", out);
    fputs("        return xvoCreateNull();\n", out);
    fputs("    }\n", out);
    fputs("    va_start(ap, count);\n", out);
    fputs("    for ( i = 0; i < count; ++i ) {\n", out);
    fputs("        const char* key = va_arg(ap, const char*);\n", out);
    fputs("        xvalue item = va_arg(ap, xvalue);\n", out);
    fputs("        if ( !xvoTableSetValue(arg, key, 0, item, true) && item != 0 ) {\n", out);
    fputs("            xvoUnref(item);\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    va_end(ap);\n", out);
    fputs("    return arg;\n", out);
    fputs("}\n\n", out);
}

static void xl6CgenEmitIntPowHelper(FILE* out)
{
    fputs("static long long xl6_int_pow_i64(long long base, long long exp)\n", out);
    fputs("{\n", out);
    fputs("    long long result = 1;\n\n", out);
    fputs("    if ( exp < 0 ) {\n", out);
    fputs("        return 0;\n", out);
    fputs("    }\n", out);
    fputs("    while ( exp > 0 ) {\n", out);
    fputs("        if ( (exp & 1) != 0 ) {\n", out);
    fputs("            result *= base;\n", out);
    fputs("        }\n", out);
    fputs("        exp >>= 1;\n", out);
    fputs("        if ( exp > 0 ) {\n", out);
    fputs("            base *= base;\n", out);
    fputs("        }\n", out);
    fputs("    }\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
}

static void xl6CgenEmitRangeContainsHelper(FILE* out)
{
    fputs("static int xl6_range_contains_i64(long long value, long long start, long long stop, long long step)\n", out);
    fputs("{\n", out);
    fputs("    if ( step == 0 ) {\n", out);
    fputs("        return 0;\n", out);
    fputs("    }\n", out);
    fputs("    if ( step > 0 ) {\n", out);
    fputs("        if ( value < start || value >= stop ) {\n", out);
    fputs("            return 0;\n", out);
    fputs("        }\n", out);
    fputs("        return ((value - start) % step) == 0;\n", out);
    fputs("    }\n", out);
    fputs("    if ( value > start || value <= stop ) {\n", out);
    fputs("        return 0;\n", out);
    fputs("    }\n", out);
    fputs("    return ((start - value) % -step) == 0;\n", out);
    fputs("}\n\n", out);
}

static const XL6_AstNode* xl6CgenHeaderFindTypeAlias(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( xl6CgenIsTypeAliasDecl(node) && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenHeaderResolveAliasType(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6CgenHeaderFindTypeAlias(root, typeNode->text);
        if ( aliasNode == NULL ) {
            return typeNode;
        }
        typeNode = xl6CgenTypeAliasTargetType(aliasNode);
    }
    return typeNode;
}

static const char* xl6CgenHeaderTypeName(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "int64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "int8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "uint8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "int16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "uint16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "uint32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "uint64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    return "void";
}

static void xl6CgenHeaderExportFuncProto(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t i;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        fputs(xl6CgenHeaderTypeName(root, returns->children[0]), out);
    } else {
        fputs("void", out);
    }
    fputc(' ', out);
    xl6CgenCStringSymbol(out, node->text);
    fputc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("void", out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                fputs(", ", out);
            }
            fputs(xl6CgenHeaderTypeName(root, param != NULL && param->childCount > 0 ? param->children[0] : NULL), out);
            fputc(' ', out);
            xl6CgenFputsCIdent(out, param != NULL ? param->text : NULL);
        }
    }
    fputs(");\n", out);
}

static void xl6CgenHeaderExportVarDecl(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenExternalVarTypeNode(node);
    fputs("extern ", out);
    if ( node != NULL && node->kind == XL6_AST_EXPORT_CONST ) {
        fputs("const ", out);
    }
    fputs(xl6CgenHeaderTypeName(root, typeNode), out);
    fputc(' ', out);
    xl6CgenCStringSymbol(out, node != NULL ? node->text : NULL);
    fputs(";\n", out);
}

int xl6CgenEmitHeader(FILE* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags)
{
    size_t i;

    (void)diags;
    if ( out == NULL || root == NULL ) {
        return 0;
    }
    if ( guardName == NULL || guardName[0] == '\0' ) {
        guardName = "XL6_GENERATED_HEADER_H";
    }

    fprintf(out, "#ifndef %s\n", guardName);
    fprintf(out, "#define %s\n\n", guardName);
    fputs("#include <stdbool.h>\n", out);
    fputs("#include <stdint.h>\n\n", out);
    fputs("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", out);

    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenHeaderExportFuncProto(out, root, node);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && (node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST) ) {
            xl6CgenHeaderExportVarDecl(out, root, node);
        }
    }

    fputs("\n#ifdef __cplusplus\n}\n#endif\n\n", out);
    fprintf(out, "#endif /* %s */\n", guardName);
    return 1;
}

int xl6CgenEmitC(FILE* out, XL6_AstNode* root, XL6_DiagList* diags)
{
    XL6_CgenContext ctx;
    size_t i;
    int hasTopLevel;
    int hasStringRuntime;
    int hasTimeRuntime;
    int hasTypeConvertRuntime;
    int hasFunctionVarRuntime;
    int hasXValueRuntime;

    if ( out == NULL || root == NULL ) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.root = root;
    ctx.diags = diags;
    ctx.ok = 1;

    xl6CgenCollectAnonFuncs(&ctx, root);
    xl6CgenCollectGenericInstancesToFixedPoint(&ctx);
    xl6CgenFreeVarsSince(&ctx, 0);
    hasStringRuntime = xl6CgenProgramNeedsStringRuntime(&ctx, root);
    hasTypeConvertRuntime = xl6CgenProgramNeedsTypeConvertRuntime(&ctx, root);
    hasFunctionVarRuntime = xl6CgenProgramNeedsFunctionVarRuntime(&ctx, root);
    hasXValueRuntime = xl6CgenNodeNeedsXValueRuntime(root) || hasFunctionVarRuntime;
    hasTimeRuntime = xl6CgenNodeNeedsTimeRuntime(root);
    ctx.needsFunctionVarRuntime = hasFunctionVarRuntime;

    fputs("/* Generated by xlang DEMO6 cgen. */\n\n", out);
    fputs("#include <stdbool.h>\n", out);
    fputs("#include <stdint.h>\n\n", out);
    if ( xl6CgenNodeNeedsFunctionRuntime(root) ) {
        xl6CgenEmitFunctionRuntime(out);
    }
    if ( xl6CgenNodeNeedsApprox(root) ) {
        xl6CgenEmitApproxProtos(out);
    }
    if ( hasStringRuntime ) {
        xl6CgenEmitStringRuntime(out);
    }
    if ( hasTypeConvertRuntime ) {
        xl6CgenEmitTypeConvertRuntime(out);
    }
    if ( hasTimeRuntime ) {
        xl6CgenEmitTimeRuntime(out);
    }
    if ( hasXValueRuntime ) {
        xl6CgenEmitXValueRuntime(&ctx);
    }
    if ( hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionVarRuntime(out);
    }
    if ( xl6CgenNodeNeedsIntPow(root) ) {
        xl6CgenEmitIntPowHelper(out);
    }
    if ( xl6CgenNodeNeedsRangeContains(root) ) {
        xl6CgenEmitRangeContainsHelper(out);
    }
    xl6CgenCollectGenericClassDecls(&ctx, root);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS ) {
            xl6CgenClassDecls(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_CUSTOM ) {
            xl6CgenCustomDecls(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            xl6CgenFuncReturnStructDecl(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        xl6CgenFuncReturnStructDecl(&ctx, ctx.genericInstances[i].func);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenFunctionReturnStructDecls(&ctx, root);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_EXTERN_FUNC ) {
            xl6CgenExternFuncProto(&ctx, root->children[i]);
            fputs(";\n", out);
        } else if ( root->children[i]->kind == XL6_AST_EXTERN_VAR ) {
            xl6CgenExternVarDecl(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportInternalProto(&ctx, root->children[i]);
            fputs(";\n", out);
            xl6CgenExportWrapperProto(&ctx, root->children[i]);
            fputs(";\n", out);
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_VAR ||
                    root->children[i]->kind == XL6_AST_EXPORT_CONST ) {
            xl6CgenExportVarDef(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            xl6CgenFuncProto(&ctx, root->children[i]);
            fputs(";\n", out);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        xl6CgenFuncProto(&ctx, ctx.genericInstances[i].func);
        fputs(";\n", out);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenGenericClassMethodProtos(&ctx);
    if ( root->childCount > 0 ) {
        fputc('\n', out);
    }

    if ( xl6CgenNodeNeedsFunctionRuntime(root) ) {
        xl6CgenEmitFunctionValueWrappers(&ctx);
    }
    xl6CgenEmitAnonFunctions(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS ) {
            xl6CgenClassMethods(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CUSTOM ) {
            xl6CgenCustomMethods(&ctx, root->children[i]);
        }
    }
    xl6CgenGenericClassMethods(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            xl6CgenFunc(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportFunc(&ctx, root->children[i]);
            xl6CgenExportWrapper(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        xl6CgenFunc(&ctx, ctx.genericInstances[i].func);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }

    hasTopLevel = 0;
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind != XL6_AST_FUNC &&
             root->children[i]->kind != XL6_AST_EXTERN_FUNC &&
             root->children[i]->kind != XL6_AST_EXPORT_FUNC &&
             !xl6CgenIsExternalVarDecl(root->children[i]) &&
             !xl6CgenIsTypeAliasDecl(root->children[i]) &&
             root->children[i]->kind != XL6_AST_CUSTOM &&
             root->children[i]->kind != XL6_AST_CLASS ) {
            hasTopLevel = 1;
            break;
        }
    }

    if ( hasTopLevel ) {
        fputs("int main(void)\n{\n", out);
        ctx.indent = 1;
        for ( i = 0; i < root->childCount; ++i ) {
            if ( root->children[i]->kind != XL6_AST_FUNC &&
                 root->children[i]->kind != XL6_AST_EXTERN_FUNC &&
                 root->children[i]->kind != XL6_AST_EXPORT_FUNC &&
                 !xl6CgenIsExternalVarDecl(root->children[i]) &&
                 !xl6CgenIsTypeAliasDecl(root->children[i]) &&
                 root->children[i]->kind != XL6_AST_CUSTOM &&
                 root->children[i]->kind != XL6_AST_CLASS ) {
                xl6CgenStmt(&ctx, root->children[i]);
            }
        }
        xl6CgenCloseScope(&ctx, 0);
        fputs("    return 0;\n", out);
        fputs("}\n", out);
    }

    hasTopLevel = ctx.ok && (diags == NULL || diags->count == 0);
    xl6CgenFreeVars(&ctx);
    xl6CgenFreeEmittedClasses(&ctx);
    xl6CgenFreeFunctionReturnStructs(&ctx);
    xl6CgenFreeCleanups(&ctx);
    xl6CgenFreeLoopTargets(&ctx);
    xl6CgenFreeGenericInstances(&ctx);
    xl6CgenFreeGenericClassInstances(&ctx);
    xl6CgenFreeAnonFuncs(&ctx);
    xl6CgenFreeOwnedTypeNodes(&ctx);
    return hasTopLevel;
}
