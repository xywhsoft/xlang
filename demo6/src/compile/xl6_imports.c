#include "src/compile/xl6_imports.h"

#include "src/abi/xl6_xapi_load.h"
#include "src/common/xl6_common.h"
#include "src/package/xl6_package.h"
#include "src/parser/xl6_parser_api.h"

typedef struct XL6_ImportTextBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_ImportTextBuilder;

static int xl6ImportsTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void xl6ImportsBuilderInit(XL6_ImportTextBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6ImportsBuilderFree(XL6_ImportTextBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6ImportsBuilderGrow(XL6_ImportTextBuilder* builder, size_t addLen)
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
    newCapacity = builder->capacity == 0 ? 256 : builder->capacity;
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

static void xl6ImportsBuilderAppendRange(XL6_ImportTextBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !xl6ImportsBuilderGrow(builder, len) ) {
        return;
    }
    if ( len > 0 ) {
        memcpy(builder->data + builder->len, text, len);
    }
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6ImportsBuilderAppend(XL6_ImportTextBuilder* builder, const char* text)
{
    if ( text != NULL ) {
        xl6ImportsBuilderAppendRange(builder, text, strlen(text));
    }
}

static void xl6ImportsBuilderAppendQuoted(XL6_ImportTextBuilder* builder, const char* text)
{
    const char* p;

    xl6ImportsBuilderAppend(builder, "\"");
    for ( p = text != NULL ? text : ""; *p != '\0'; ++p ) {
        if ( *p == '"' || *p == '\\' ) {
            xl6ImportsBuilderAppend(builder, "\\");
        }
        xl6ImportsBuilderAppendRange(builder, p, 1);
    }
    xl6ImportsBuilderAppend(builder, "\"");
}

static char* xl6ImportsBuilderTake(XL6_ImportTextBuilder* builder)
{
    char* out;

    if ( builder == NULL || !builder->ok ) {
        return NULL;
    }
    if ( builder->data == NULL ) {
        builder->data = xrtCopyStr((char*)"", 0);
    }
    out = builder->data;
    builder->data = NULL;
    builder->len = 0;
    builder->capacity = 0;
    return out;
}

static char* xl6ImportsDupImportTarget(const char* text)
{
    size_t len;

    if ( text == NULL ) {
        return xrtCopyStr((char*)"", 0);
    }
    len = strlen(text);
    if ( len >= 2 && ((text[0] == '"' && text[len - 1] == '"') ||
                      (text[0] == '\'' && text[len - 1] == '\'')) ) {
        return xrtCopyStr((char*)text + 1, len - 2);
    }
    return xrtCopyStr((char*)text, len);
}

static int xl6ImportsFunctionCanEnterExtern(const XL6_XapiExport* item)
{
    size_t i;

    if ( item == NULL || !xl6ImportsTextEquals(item->kind, "function") ) {
        return 0;
    }
    if ( item->callingConvention != NULL && !xl6ImportsTextEquals(item->callingConvention, "cdecl") ) {
        return 0;
    }
    if ( item->varargMode != NULL && !xl6ImportsTextEquals(item->varargMode, "none") ) {
        return 0;
    }
    if ( item->returnCount > 1 ) {
        return 0;
    }
    for ( i = 0; i < item->paramCount; ++i ) {
        if ( item->params[i].optional || item->params[i].vararg ) {
            return 0;
        }
    }
    return 1;
}

static void xl6ImportsAppendFunctionDecl(XL6_ImportTextBuilder* builder, const XL6_XapiExport* item)
{
    size_t i;

    xl6ImportsBuilderAppend(builder, "extern ");
    xl6ImportsBuilderAppendQuoted(builder, item->cSymbol);
    xl6ImportsBuilderAppend(builder, " func ");
    xl6ImportsBuilderAppend(builder, item->name);
    xl6ImportsBuilderAppend(builder, "(");
    for ( i = 0; i < item->paramCount; ++i ) {
        if ( i > 0 ) {
            xl6ImportsBuilderAppend(builder, ", ");
        }
        xl6ImportsBuilderAppend(builder, item->params[i].type);
        xl6ImportsBuilderAppend(builder, " ");
        xl6ImportsBuilderAppend(builder, item->params[i].name);
    }
    xl6ImportsBuilderAppend(builder, ")");
    if ( item->returnCount == 1 ) {
        xl6ImportsBuilderAppend(builder, " -> ");
        xl6ImportsBuilderAppend(builder, item->returns[0]);
    }
    xl6ImportsBuilderAppend(builder, "\n");
}

static void xl6ImportsAppendVarDecl(XL6_ImportTextBuilder* builder, const XL6_XapiExport* item)
{
    if ( item == NULL || !xl6ImportsTextEquals(item->kind, "var") || item->type == NULL ) {
        return;
    }
    xl6ImportsBuilderAppend(builder, "extern ");
    xl6ImportsBuilderAppendQuoted(builder, item->cSymbol);
    xl6ImportsBuilderAppend(builder, " var ");
    xl6ImportsBuilderAppend(builder, item->type);
    xl6ImportsBuilderAppend(builder, " ");
    xl6ImportsBuilderAppend(builder, item->name);
    xl6ImportsBuilderAppend(builder, "\n");
}

static int xl6ImportsAppendXapiDecls(XL6_ImportTextBuilder* builder, const XL6_XapiModule* module)
{
    size_t i;

    if ( builder == NULL || module == NULL ) {
        return 0;
    }
    for ( i = 0; i < module->exportCount; ++i ) {
        const XL6_XapiExport* item = &module->exports[i];
        if ( xl6ImportsFunctionCanEnterExtern(item) ) {
            xl6ImportsAppendFunctionDecl(builder, item);
        } else if ( xl6ImportsTextEquals(item->kind, "var") ) {
            xl6ImportsAppendVarDecl(builder, item);
        }
    }
    return builder->ok;
}

static int xl6ImportsMergeGeneratedDecls(XL6_AstNode* root, const char* generated, XL6_DiagList* diags)
{
    XL6_ParseResult result;
    int ok;

    if ( generated == NULL || generated[0] == '\0' ) {
        return 1;
    }
    xl6ParseResultInit(&result);
    ok = xl6ParseSource(generated, "<xapi-import>", &result);
    if ( !ok ) {
        if ( diags != NULL && result.diags.count > 0 ) {
            size_t i;
            for ( i = 0; i < result.diags.count; ++i ) {
                xl6DiagAdd(diags, result.diags.items[i].line, result.diags.items[i].column, result.diags.items[i].message);
            }
        } else if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "XAPI import 生成 extern 声明失败");
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

static int xl6ImportsLoadOne(XL6_AstNode* root, const char* sourcePath, const XL6_AstNode* importNode, XL6_DiagList* diags)
{
    XL6_PackageResolveResult resolved;
    XL6_XapiModule module;
    XL6_ImportTextBuilder builder;
    char xapiPath[512];
    char* target;
    char* generated;
    int ok;

    target = xl6ImportsDupImportTarget(importNode != NULL ? importNode->text : NULL);
    if ( target == NULL ) {
        return 0;
    }

    xl6PackageResolveImport(sourcePath, target, &resolved);
    if ( !resolved.resolved ) {
        xrtFree(target);
        return 1;
    }
    if ( !xl6PackageResolveInterfacePath(&resolved, xapiPath, sizeof(xapiPath)) ) {
        xrtFree(target);
        if ( diags != NULL ) {
            xl6DiagAdd(diags, importNode != NULL ? importNode->line : 1, importNode != NULL ? importNode->column : 1, "import package 缺少 package.xapi.json 接口文件");
        }
        return 0;
    }

    xl6XapiModuleInit(&module);
    ok = xl6XapiLoadJsonFile(&module, xapiPath, diags);
    if ( ok ) {
        xl6ImportsBuilderInit(&builder);
        ok = xl6ImportsAppendXapiDecls(&builder, &module);
        generated = xl6ImportsBuilderTake(&builder);
        xl6ImportsBuilderFree(&builder);
        if ( ok ) {
            ok = xl6ImportsMergeGeneratedDecls(root, generated, diags);
        }
        xrtFree(generated);
    }
    xl6XapiModuleFree(&module);
    xrtFree(target);
    return ok;
}

int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags)
{
    size_t i;
    size_t initialCount;
    int ok;

    if ( root == NULL ) {
        return 1;
    }
    initialCount = root->childCount;
    ok = 1;
    for ( i = 0; i < initialCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child != NULL && child->kind == XL6_AST_IMPORT ) {
            if ( !xl6ImportsLoadOne(root, sourcePath, child, diags) ) {
                ok = 0;
            }
        }
    }
    return ok;
}
