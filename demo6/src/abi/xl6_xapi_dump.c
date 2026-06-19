#include "src/abi/xl6_xapi_dump.h"

#include "src/abi/xl6_callable.h"
#include "src/common/xl6_common.h"
#include "src/package/xl6_package.h"
#include "src/type/xl6_type_model.h"
#include "src/common/xl6_output.h"

static int xl6XapiTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void xl6XapiJsonEscapedRange(XL6_Output* out, const char* text, size_t len)
{
    size_t i;

    if ( out == NULL || text == NULL ) {
        return;
    }
    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '"' ) {
            xl6OutputPuts("\\\"", out);
        } else if ( ch == '\\' ) {
            xl6OutputPuts("\\\\", out);
        } else if ( ch == '\b' ) {
            xl6OutputPuts("\\b", out);
        } else if ( ch == '\f' ) {
            xl6OutputPuts("\\f", out);
        } else if ( ch == '\n' ) {
            xl6OutputPuts("\\n", out);
        } else if ( ch == '\r' ) {
            xl6OutputPuts("\\r", out);
        } else if ( ch == '\t' ) {
            xl6OutputPuts("\\t", out);
        } else if ( ch < 0x20 ) {
            xl6OutputPrintf(out, "\\u%04X", (unsigned)ch);
        } else {
            xl6OutputPutc((int)ch, out);
        }
    }
}

static void xl6XapiJsonEscaped(XL6_Output* out, const char* text)
{
    if ( text != NULL ) {
        xl6XapiJsonEscapedRange(out, text, strlen(text));
    }
}

static void xl6XapiJsonString(XL6_Output* out, const char* text)
{
    xl6OutputPutc('"', out);
    xl6XapiJsonEscaped(out, text);
    xl6OutputPutc('"', out);
}

static void xl6XapiJsonCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;

    xl6OutputPutc('"', out);
    if ( text != NULL ) {
        len = strlen(text);
        if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
            xl6XapiJsonEscapedRange(out, text + 1, len - 2);
        } else {
            xl6XapiJsonEscapedRange(out, text, len);
        }
    }
    xl6OutputPutc('"', out);
}

static int xl6XapiIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6XapiFindTypeAlias(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( xl6XapiIsTypeAliasDecl(node) && node->text != NULL && strcmp(node->text, name) == 0 ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6XapiResolveAliasType(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6XapiFindTypeAlias(root, typeNode->text);
        if ( aliasNode == NULL || aliasNode->childCount == 0 ) {
            return typeNode;
        }
        typeNode = aliasNode->children[0];
    }
    return typeNode;
}

static void xl6XapiDumpTypeJsonBody(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6XapiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        xl6OutputPuts("<missing-type>", out);
        return;
    }

    if ( xl6XapiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6OutputPuts("function(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6XapiDumpTypeJsonBody(out, root, args->children[i]);
            }
        }
        xl6OutputPutc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6OutputPuts("->", out);
            if ( returns->childCount > 1 ) {
                xl6OutputPutc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6XapiDumpTypeJsonBody(out, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6OutputPutc(')', out);
            }
        }
        return;
    }

    xl6XapiJsonEscaped(out, typeNode->text);
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST ) {
        return;
    }

    xl6OutputPutc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPutc(',', out);
        }
        xl6XapiDumpTypeJsonBody(out, root, args->children[i]);
    }
    xl6OutputPutc('>', out);
}

static void xl6XapiDumpTypeJsonString(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    xl6OutputPutc('"', out);
    xl6XapiDumpTypeJsonBody(out, root, typeNode);
    xl6OutputPutc('"', out);
}

static int xl6XapiIsTopLevelRuntimeStmt(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind != XL6_AST_IMPORT &&
           node->kind != XL6_AST_FUNC &&
           node->kind != XL6_AST_EXTERN_FUNC &&
           node->kind != XL6_AST_EXPORT_FUNC &&
           node->kind != XL6_AST_EXTERN_VAR &&
           node->kind != XL6_AST_EXPORT_VAR &&
           node->kind != XL6_AST_EXPORT_CONST &&
           node->kind != XL6_AST_TYPE_ALIAS &&
           node->kind != XL6_AST_EXTERN_TYPE_ALIAS &&
           node->kind != XL6_AST_CUSTOM &&
           node->kind != XL6_AST_CLASS;
}

static const XL6_AstNode* xl6XapiFindMainFunc(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* nameNode;
        if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6XapiTextEquals(nameNode->text, "main") ) {
            return node;
        }
    }
    return NULL;
}

static int xl6XapiHasTopLevelRuntimeStmt(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6XapiIsTopLevelRuntimeStmt(root->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6XapiDumpEntry(XL6_Output* out, const XL6_AstNode* root)
{
    const XL6_AstNode* mainFunc;
    int hasTopLevel;

    mainFunc = xl6XapiFindMainFunc(root);
    hasTopLevel = xl6XapiHasTopLevelRuntimeStmt(root);

    xl6OutputPuts("  \"entry\":{", out);
    if ( mainFunc != NULL ) {
        xl6OutputPuts("\"kind\":\"xlang-main\",\"function\":\"main\",\"argvMode\":\"array<var>\",\"returnType\":\"int\",", out);
        xl6OutputPuts("\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    } else if ( hasTopLevel ) {
        xl6OutputPuts("\"kind\":\"script-main\",\"argvMode\":\"array<var>\",\"returnType\":\"int\",", out);
        xl6OutputPuts("\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    } else {
        xl6OutputPuts("\"kind\":\"none\",\"requiresExplicitInit\":true,\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    }
}

static void xl6XapiDumpParams(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* params)
{
    size_t i;

    xl6OutputPutc('[', out);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6OutputPuts("{\"name\":", out);
            xl6XapiJsonString(out, param != NULL && param->text != NULL ? param->text : "");
            xl6OutputPuts(",\"type\":", out);
            if ( param != NULL && param->childCount > 0 ) {
                xl6XapiDumpTypeJsonString(out, root, param->children[0]);
            } else {
                xl6XapiJsonString(out, "<missing-type>");
            }
            xl6OutputPutc('}', out);
        }
    }
    xl6OutputPutc(']', out);
}

static void xl6XapiDumpReturns(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* returns)
{
    size_t i;

    xl6OutputPutc('[', out);
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6OutputPuts("{\"type\":", out);
            xl6XapiDumpTypeJsonString(out, root, returns->children[i]);
            xl6OutputPutc('}', out);
        }
    }
    xl6OutputPutc(']', out);
}

static void xl6XapiDumpExportFunc(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    returns = node->childCount > 2 ? node->children[2] : NULL;

    xl6OutputPuts("    {\"kind\":\"function\",\"name\":", out);
    xl6XapiJsonString(out, nameNode != NULL ? nameNode->text : "");
    xl6OutputPuts(",\"cSymbol\":", out);
    xl6XapiJsonCStringSymbol(out, node->text);
    xl6OutputPuts(",\"params\":", out);
    xl6XapiDumpParams(out, root, params);
    xl6OutputPuts(",\"returns\":", out);
    xl6XapiDumpReturns(out, root, returns);
    xl6OutputPutc('}', out);
}

static void xl6XapiDumpExportVar(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;

    xl6OutputPuts("    {\"kind\":", out);
    xl6XapiJsonString(out, node->kind == XL6_AST_EXPORT_CONST ? "const" : "var");
    xl6OutputPuts(",\"name\":", out);
    xl6XapiJsonString(out, nameNode != NULL ? nameNode->text : "");
    xl6OutputPuts(",\"cSymbol\":", out);
    xl6XapiJsonCStringSymbol(out, node->text);
    xl6OutputPuts(",\"type\":", out);
    xl6XapiDumpTypeJsonString(out, root, typeNode);
    xl6OutputPutc('}', out);
}

static void xl6XapiDumpTypeDecl(XL6_Output* out, const XL6_AstNode* node, int comma)
{
    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    xl6OutputPuts("    {\"kind\":", out);
    xl6XapiJsonString(out, node->kind == XL6_AST_CUSTOM ? "custom" : "class");
    xl6OutputPuts(",\"name\":", out);
    xl6XapiJsonString(out, node->text != NULL ? node->text : "");
    xl6OutputPutc('}', out);
}

static void xl6XapiDumpImport(XL6_Output* out, const XL6_AstNode* node, int comma)
{
    const char* target;
    size_t len;

    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    target = node != NULL && node->text != NULL ? node->text : "";
    len = strlen(target);
    xl6OutputPuts("    {\"target\":", out);
    if ( len >= 2 && ((target[0] == '"' && target[len - 1] == '"') ||
                      (target[0] == '\'' && target[len - 1] == '\'')) ) {
        xl6OutputPutc('"', out);
        xl6XapiJsonEscapedRange(out, target + 1, len - 2);
        xl6OutputPutc('"', out);
    } else {
        xl6XapiJsonString(out, target);
    }
    xl6OutputPuts(",\"mode\":\"package-or-path\"}", out);
}

typedef struct XL6_XapiStringBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_XapiStringBuilder;

static void xl6XapiStringBuilderInit(XL6_XapiStringBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6XapiStringBuilderFree(XL6_XapiStringBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6XapiStringBuilderGrow(XL6_XapiStringBuilder* builder, size_t addLen)
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
    newCapacity = builder->capacity == 0 ? 64 : builder->capacity;
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

static void xl6XapiStringBuilderAppendRange(XL6_XapiStringBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !xl6XapiStringBuilderGrow(builder, len) ) {
        return;
    }
    if ( len > 0 ) {
        memcpy(builder->data + builder->len, text, len);
    }
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6XapiStringBuilderAppend(XL6_XapiStringBuilder* builder, const char* text)
{
    if ( text != NULL ) {
        xl6XapiStringBuilderAppendRange(builder, text, strlen(text));
    }
}

static void xl6XapiStringBuilderAppendChar(XL6_XapiStringBuilder* builder, char ch)
{
    xl6XapiStringBuilderAppendRange(builder, &ch, 1);
}

static char* xl6XapiStringBuilderTake(XL6_XapiStringBuilder* builder)
{
    char* out;

    if ( builder == NULL || !builder->ok ) {
        return NULL;
    }
    if ( builder->data == NULL ) {
        builder->data = xrtCopyStr((char*)"", 0);
        builder->capacity = builder->data != NULL ? 1 : 0;
        builder->ok = builder->data != NULL;
    }
    out = builder->data;
    builder->data = NULL;
    builder->len = 0;
    builder->capacity = 0;
    return out;
}

static char* xl6XapiDupText(const char* text)
{
    return xrtCopyStr((char*)(text != NULL ? text : ""), text != NULL ? strlen(text) : 0);
}

static char* xl6XapiDupCStringSymbol(const char* text)
{
    size_t len;

    if ( text == NULL ) {
        return xl6XapiDupText("");
    }
    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        return xrtCopyStr((char*)text + 1, len - 2);
    }
    return xrtCopyStr((char*)text, len);
}

static char* xl6XapiDupImportTarget(const char* text)
{
    size_t len;

    if ( text == NULL ) {
        return xl6XapiDupText("");
    }
    len = strlen(text);
    if ( len >= 2 && ((text[0] == '"' && text[len - 1] == '"') ||
                      (text[0] == '\'' && text[len - 1] == '\'')) ) {
        return xrtCopyStr((char*)text + 1, len - 2);
    }
    return xrtCopyStr((char*)text, len);
}

static void xl6XapiAppendTypeBody(XL6_XapiStringBuilder* builder, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6XapiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        xl6XapiStringBuilderAppend(builder, "<missing-type>");
        return;
    }

    if ( xl6XapiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6XapiStringBuilderAppend(builder, "function(");
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6XapiStringBuilderAppendChar(builder, ',');
                }
                xl6XapiAppendTypeBody(builder, root, args->children[i]);
            }
        }
        xl6XapiStringBuilderAppendChar(builder, ')');
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6XapiStringBuilderAppend(builder, "->");
            if ( returns->childCount > 1 ) {
                xl6XapiStringBuilderAppendChar(builder, '(');
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6XapiStringBuilderAppendChar(builder, ',');
                }
                xl6XapiAppendTypeBody(builder, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6XapiStringBuilderAppendChar(builder, ')');
            }
        }
        return;
    }

    xl6XapiStringBuilderAppend(builder, typeNode->text);
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST ) {
        return;
    }

    xl6XapiStringBuilderAppendChar(builder, '<');
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6XapiStringBuilderAppendChar(builder, ',');
        }
        xl6XapiAppendTypeBody(builder, root, args->children[i]);
    }
    xl6XapiStringBuilderAppendChar(builder, '>');
}

static char* xl6XapiTypeToString(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    return xl6TypeToCanonicalString(root, typeNode);
}

void xl6XapiModuleInit(XL6_XapiModule* module)
{
    if ( module == NULL ) {
        return;
    }
    memset(module, 0, sizeof(*module));
}

void xl6XapiModuleFree(XL6_XapiModule* module)
{
    size_t i;
    size_t j;

    if ( module == NULL ) {
        return;
    }
    xrtFree(module->name);
    xrtFree(module->source);
    xrtFree(module->sourceHash);
    for ( i = 0; i < module->exportCount; ++i ) {
        XL6_XapiExport* item = &module->exports[i];
        xrtFree(item->kind);
        xrtFree(item->name);
        xrtFree(item->cSymbol);
        xrtFree(item->type);
        xrtFree(item->callingConvention);
        xrtFree(item->varargMode);
        for ( j = 0; j < item->paramCount; ++j ) {
            xrtFree(item->params[j].name);
            xrtFree(item->params[j].type);
        }
        xrtFree(item->params);
        for ( j = 0; j < item->returnCount; ++j ) {
            xrtFree(item->returns[j]);
        }
        xrtFree(item->returns);
    }
    xrtFree(module->exports);
    for ( i = 0; i < module->typeCount; ++i ) {
        xrtFree(module->types[i].kind);
        xrtFree(module->types[i].name);
    }
    xrtFree(module->types);
    for ( i = 0; i < module->importCount; ++i ) {
        xrtFree(module->imports[i].target);
        xrtFree(module->imports[i].mode);
        xrtFree(module->imports[i].resolvedKind);
        xrtFree(module->imports[i].manifestPath);
    }
    xrtFree(module->imports);
    memset(module, 0, sizeof(*module));
}

static int xl6XapiModuleAllocArrays(XL6_XapiModule* module, size_t exportCount, size_t typeCount, size_t importCount)
{
    if ( exportCount > 0 ) {
        module->exports = (XL6_XapiExport*)xrtCalloc(exportCount, sizeof(XL6_XapiExport));
        if ( module->exports == NULL ) {
            return 0;
        }
    }
    if ( typeCount > 0 ) {
        module->types = (XL6_XapiTypeDecl*)xrtCalloc(typeCount, sizeof(XL6_XapiTypeDecl));
        if ( module->types == NULL ) {
            return 0;
        }
    }
    if ( importCount > 0 ) {
        module->imports = (XL6_XapiImport*)xrtCalloc(importCount, sizeof(XL6_XapiImport));
        if ( module->imports == NULL ) {
            return 0;
        }
    }
    return 1;
}

static int xl6XapiExportBuildFunc(XL6_XapiExport* item, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    returns = node->childCount > 2 ? node->children[2] : NULL;

    item->kind = xl6XapiDupText("function");
    item->name = xl6XapiDupText(nameNode != NULL ? nameNode->text : "");
    item->cSymbol = xl6XapiDupCStringSymbol(node->text);
    item->callingConvention = xl6XapiDupText(xl6CallableConventionName(XL6_CALLABLE_CONV_CDECL));
    item->varargMode = xl6XapiDupText(xl6CallableVarargModeName(xl6CallableDetectVarargMode(params)));
    item->requiredParamCount = xl6CallableRequiredParamCount(params);
    item->fixedParamCount = xl6CallableFixedParamCount(params);
    if ( params != NULL && params->childCount > 0 ) {
        item->params = (XL6_XapiParam*)xrtCalloc(params->childCount, sizeof(XL6_XapiParam));
        if ( item->params == NULL ) {
            return 0;
        }
        item->paramCount = params->childCount;
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            item->params[i].name = xl6XapiDupText(param != NULL ? param->text : "");
            item->params[i].type = xl6XapiTypeToString(root, param != NULL && param->childCount > 0 ? param->children[0] : NULL);
            item->params[i].optional = param != NULL && param->childCount > 1;
            item->params[i].vararg = xl6CallableParamIsVararg(param);
        }
    }
    if ( returns != NULL && returns->childCount > 0 ) {
        item->returns = (char**)xrtCalloc(returns->childCount, sizeof(char*));
        if ( item->returns == NULL ) {
            return 0;
        }
        item->returnCount = returns->childCount;
        for ( i = 0; i < returns->childCount; ++i ) {
            item->returns[i] = xl6XapiTypeToString(root, returns->children[i]);
        }
    }
    return item->kind != NULL && item->name != NULL && item->cSymbol != NULL &&
           item->callingConvention != NULL && item->varargMode != NULL;
}

static int xl6XapiExportBuildVar(XL6_XapiExport* item, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;
    item->kind = xl6XapiDupText(node->kind == XL6_AST_EXPORT_CONST ? "const" : "var");
    item->name = xl6XapiDupText(nameNode != NULL ? nameNode->text : "");
    item->cSymbol = xl6XapiDupCStringSymbol(node->text);
    item->type = xl6XapiTypeToString(root, typeNode);
    return item->kind != NULL && item->name != NULL && item->cSymbol != NULL && item->type != NULL;
}

int xl6XapiModuleBuild(XL6_XapiModule* module, const XL6_AstNode* root, const char* sourcePath, const char* moduleName)
{
    size_t i;
    size_t exportCount;
    size_t typeCount;
    size_t importCount;
    size_t exportIndex;
    size_t typeIndex;
    size_t importIndex;

    if ( module == NULL ) {
        return 0;
    }
    xl6XapiModuleFree(module);
    xl6XapiModuleInit(module);

    module->name = xl6XapiDupText(moduleName != NULL ? moduleName : "app");
    module->source = xl6XapiDupText(sourcePath != NULL ? sourcePath : "");
    module->sourceHash = xl6XapiDupText("");
    if ( module->name == NULL || module->source == NULL || module->sourceHash == NULL ) {
        xl6XapiModuleFree(module);
        return 0;
    }

    if ( xl6XapiFindMainFunc(root) != NULL ) {
        module->entryKind = XL6_XAPI_ENTRY_XLANG_MAIN;
    } else if ( xl6XapiHasTopLevelRuntimeStmt(root) ) {
        module->entryKind = XL6_XAPI_ENTRY_SCRIPT_MAIN;
    } else {
        module->entryKind = XL6_XAPI_ENTRY_NONE;
    }

    exportCount = 0;
    typeCount = 0;
    importCount = 0;
    if ( root != NULL ) {
        for ( i = 0; i < root->childCount; ++i ) {
            const XL6_AstNode* child = root->children[i];
            if ( child == NULL ) {
                continue;
            }
            if ( child->kind == XL6_AST_EXPORT_FUNC || child->kind == XL6_AST_EXPORT_VAR || child->kind == XL6_AST_EXPORT_CONST ) {
                exportCount += 1;
            } else if ( child->kind == XL6_AST_CLASS || child->kind == XL6_AST_CUSTOM ) {
                typeCount += 1;
            } else if ( child->kind == XL6_AST_IMPORT ) {
                importCount += 1;
            }
        }
    }
    if ( !xl6XapiModuleAllocArrays(module, exportCount, typeCount, importCount) ) {
        xl6XapiModuleFree(module);
        return 0;
    }

    exportIndex = 0;
    typeIndex = 0;
    importIndex = 0;
    if ( root != NULL ) {
        for ( i = 0; i < root->childCount; ++i ) {
            const XL6_AstNode* child = root->children[i];
            if ( child == NULL ) {
                continue;
            }
            if ( child->kind == XL6_AST_EXPORT_FUNC ) {
                if ( !xl6XapiExportBuildFunc(&module->exports[exportIndex], root, child) ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                exportIndex += 1;
            } else if ( child->kind == XL6_AST_EXPORT_VAR || child->kind == XL6_AST_EXPORT_CONST ) {
                if ( !xl6XapiExportBuildVar(&module->exports[exportIndex], root, child) ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                exportIndex += 1;
            } else if ( child->kind == XL6_AST_CLASS || child->kind == XL6_AST_CUSTOM ) {
                module->types[typeIndex].kind = xl6XapiDupText(child->kind == XL6_AST_CUSTOM ? "custom" : "class");
                module->types[typeIndex].name = xl6XapiDupText(child->text != NULL ? child->text : "");
                if ( module->types[typeIndex].kind == NULL || module->types[typeIndex].name == NULL ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                typeIndex += 1;
            } else if ( child->kind == XL6_AST_IMPORT ) {
                XL6_PackageResolveResult resolved;
                char* target = xl6XapiDupImportTarget(child->text);

                module->imports[importIndex].target = target;
                module->imports[importIndex].mode = xl6XapiDupText("package-or-path");
                xl6PackageResolveImport(sourcePath, target, &resolved);
                module->imports[importIndex].resolved = resolved.resolved;
                module->imports[importIndex].resolvedKind = xl6XapiDupText(xl6PackageResolveKindName(resolved.kind));
                module->imports[importIndex].manifestPath = xl6XapiDupText(resolved.manifestPath);
                if ( module->imports[importIndex].target == NULL ||
                     module->imports[importIndex].mode == NULL ||
                     module->imports[importIndex].resolvedKind == NULL ||
                     module->imports[importIndex].manifestPath == NULL ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                importIndex += 1;
            }
        }
    }
    module->exportCount = exportIndex;
    module->typeCount = typeIndex;
    module->importCount = importIndex;
    return 1;
}

int xl6XapiModuleHasExecutableEntry(const XL6_XapiModule* module)
{
    return module != NULL &&
           (module->entryKind == XL6_XAPI_ENTRY_XLANG_MAIN ||
            module->entryKind == XL6_XAPI_ENTRY_SCRIPT_MAIN);
}

static void xl6XapiDumpEntryModule(XL6_Output* out, const XL6_XapiModule* module)
{
    xl6OutputPuts("  \"entry\":{", out);
    if ( module != NULL && module->entryKind == XL6_XAPI_ENTRY_XLANG_MAIN ) {
        xl6OutputPuts("\"kind\":\"xlang-main\",\"function\":\"main\",\"argvMode\":\"array<var>\",\"returnType\":\"int\",", out);
        xl6OutputPuts("\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    } else if ( module != NULL && module->entryKind == XL6_XAPI_ENTRY_SCRIPT_MAIN ) {
        xl6OutputPuts("\"kind\":\"script-main\",\"argvMode\":\"array<var>\",\"returnType\":\"int\",", out);
        xl6OutputPuts("\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    } else {
        xl6OutputPuts("\"kind\":\"none\",\"requiresExplicitInit\":true,\"initSymbol\":\"xlInit\",\"unitSymbol\":\"xlUnit\"}", out);
    }
}

void xl6XapiDumpJsonModule(XL6_Output* out, const XL6_XapiModule* module)
{
    size_t i;
    size_t j;

    if ( out == NULL ) {
        return;
    }
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("  \"format\":\"xlang-xapi-1\",\n", out);
    xl6OutputPuts("  \"compiler\":{\"name\":\"xl\",\"version\":\"demo6\",\"xlangAbi\":1,\"xrtAbi\":1},\n", out);
    xl6OutputPuts("  \"target\":{\"os\":\"windows\",\"arch\":\"x64\",\"callingConvention\":\"cdecl\"},\n", out);
    xl6OutputPuts("  \"module\":{\"name\":", out);
    xl6XapiJsonString(out, module != NULL ? module->name : "app");
    xl6OutputPuts(",\"source\":", out);
    xl6XapiJsonString(out, module != NULL ? module->source : "");
    xl6OutputPuts(",\"sourceHash\":", out);
    xl6XapiJsonString(out, module != NULL ? module->sourceHash : "");
    xl6OutputPuts("},\n", out);
    xl6XapiDumpEntryModule(out, module);
    xl6OutputPuts(",\n", out);

    xl6OutputPuts("  \"exports\":[\n", out);
    if ( module != NULL ) {
        for ( i = 0; i < module->exportCount; ++i ) {
            const XL6_XapiExport* item = &module->exports[i];
            if ( i > 0 ) {
                xl6OutputPuts(",\n", out);
            }
            xl6OutputPuts("    {\"kind\":", out);
            xl6XapiJsonString(out, item->kind);
            xl6OutputPuts(",\"name\":", out);
            xl6XapiJsonString(out, item->name);
            xl6OutputPuts(",\"cSymbol\":", out);
            xl6XapiJsonString(out, item->cSymbol);
            if ( item->type != NULL ) {
                xl6OutputPuts(",\"type\":", out);
                xl6XapiJsonString(out, item->type);
            }
            if ( item->paramCount > 0 || xl6XapiTextEquals(item->kind, "function") ) {
                xl6OutputPuts(",\"callingConvention\":", out);
                xl6XapiJsonString(out, item->callingConvention);
                xl6OutputPuts(",\"varargMode\":", out);
                xl6XapiJsonString(out, item->varargMode);
                xl6OutputPrintf(out, ",\"requiredParamCount\":%d,\"fixedParamCount\":%d",
                        item->requiredParamCount,
                        item->fixedParamCount);
                xl6OutputPuts(",\"params\":[", out);
                for ( j = 0; j < item->paramCount; ++j ) {
                    if ( j > 0 ) {
                        xl6OutputPutc(',', out);
                    }
                    xl6OutputPuts("{\"name\":", out);
                    xl6XapiJsonString(out, item->params[j].name);
                    xl6OutputPuts(",\"type\":", out);
                    xl6XapiJsonString(out, item->params[j].type);
                    xl6OutputPuts(",\"optional\":", out);
                    xl6OutputPuts(item->params[j].optional ? "true" : "false", out);
                    xl6OutputPuts(",\"vararg\":", out);
                    xl6OutputPuts(item->params[j].vararg ? "true" : "false", out);
                    xl6OutputPutc('}', out);
                }
                xl6OutputPuts("],\"returns\":[", out);
                for ( j = 0; j < item->returnCount; ++j ) {
                    if ( j > 0 ) {
                        xl6OutputPutc(',', out);
                    }
                    xl6OutputPuts("{\"type\":", out);
                    xl6XapiJsonString(out, item->returns[j]);
                    xl6OutputPutc('}', out);
                }
                xl6OutputPutc(']', out);
            }
            xl6OutputPutc('}', out);
        }
        if ( module->exportCount > 0 ) {
            xl6OutputPutc('\n', out);
        }
    }
    xl6OutputPuts("  ],\n", out);

    xl6OutputPuts("  \"types\":[\n", out);
    if ( module != NULL ) {
        for ( i = 0; i < module->typeCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(",\n", out);
            }
            xl6OutputPuts("    {\"kind\":", out);
            xl6XapiJsonString(out, module->types[i].kind);
            xl6OutputPuts(",\"name\":", out);
            xl6XapiJsonString(out, module->types[i].name);
            xl6OutputPutc('}', out);
        }
        if ( module->typeCount > 0 ) {
            xl6OutputPutc('\n', out);
        }
    }
    xl6OutputPuts("  ],\n", out);

    xl6OutputPuts("  \"imports\":[\n", out);
    if ( module != NULL ) {
        for ( i = 0; i < module->importCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(",\n", out);
            }
            xl6OutputPuts("    {\"target\":", out);
            xl6XapiJsonString(out, module->imports[i].target);
            xl6OutputPuts(",\"mode\":", out);
            xl6XapiJsonString(out, module->imports[i].mode);
            xl6OutputPuts(",\"resolved\":", out);
            xl6OutputPuts(module->imports[i].resolved ? "true" : "false", out);
            xl6OutputPuts(",\"resolvedKind\":", out);
            xl6XapiJsonString(out, module->imports[i].resolvedKind);
            xl6OutputPuts(",\"manifest\":", out);
            xl6XapiJsonString(out, module->imports[i].manifestPath);
            xl6OutputPutc('}', out);
        }
        if ( module->importCount > 0 ) {
            xl6OutputPutc('\n', out);
        }
    }
    xl6OutputPuts("  ],\n", out);
    xl6OutputPuts("  \"links\":[],\n", out);
    xl6OutputPuts("  \"loadlibs\":[],\n", out);
    xl6OutputPuts("  \"cSources\":[],\n", out);
    xl6OutputPuts("  \"docs\":[]\n", out);
    xl6OutputPuts("}\n", out);
}

void xl6XapiDumpJson(XL6_Output* out, const XL6_AstNode* root, const char* sourcePath, const char* moduleName)
{
    XL6_XapiModule module;

    if ( out == NULL ) {
        return;
    }
    xl6XapiModuleInit(&module);
    if ( xl6XapiModuleBuild(&module, root, sourcePath, moduleName) ) {
        xl6XapiDumpJsonModule(out, &module);
    }
    xl6XapiModuleFree(&module);
}
