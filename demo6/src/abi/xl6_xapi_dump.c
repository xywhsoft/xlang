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
           node->kind != XL6_AST_UNION &&
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
    xl6XapiJsonString(out, node->kind == XL6_AST_CUSTOM ? "custom" : (node->kind == XL6_AST_UNION ? "union" : "class"));
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

static int xl6XapiDocLineIsEnd(const char* line, size_t len)
{
    while ( len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n') ) {
        len -= 1;
    }
    while ( len > 0 && (line[0] == ' ' || line[0] == '\t') ) {
        line += 1;
        len -= 1;
    }
    return len >= 4 && strncmp(line, "#end", 4) == 0;
}

static int xl6XapiDocLineIsStart(const char* line, size_t len)
{
    while ( len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n') ) {
        len -= 1;
    }
    while ( len > 0 && (line[0] == ' ' || line[0] == '\t') ) {
        line += 1;
        len -= 1;
    }
    return len >= 4 && strncmp(line, "#doc", 4) == 0;
}

static size_t xl6XapiDocLeadingIndent(const char* line, size_t len)
{
    size_t indent;

    indent = 0;
    while ( indent < len && (line[indent] == ' ' || line[indent] == '\t') ) {
        indent += 1;
    }
    return indent;
}

static void xl6XapiDocStripBaseIndent(const char** line, size_t* len, size_t baseIndent)
{
    size_t removed;

    if ( line == NULL || *line == NULL || len == NULL || baseIndent == 0 ) {
        return;
    }
    removed = 0;
    while ( removed < baseIndent && *len > 0 && ((*line)[0] == ' ' || (*line)[0] == '\t') ) {
        *line += 1;
        *len -= 1;
        removed += 1;
    }
}

static size_t xl6XapiDocBaseIndent(const char* raw)
{
    const char* p;

    if ( raw == NULL ) {
        return 0;
    }
    p = raw;
    while ( *p != '\0' ) {
        const char* line = p;
        size_t len;

        while ( *p != '\0' && *p != '\n' ) {
            p += 1;
        }
        len = (size_t)(p - line);
        if ( *p == '\n' ) {
            p += 1;
            len += 1;
        }
        if ( xl6XapiDocLineIsStart(line, len) ) {
            return xl6XapiDocLeadingIndent(line, len);
        }
    }
    return 0;
}

static int xl6XapiDocHeaderText(
    const char* line,
    size_t len,
    const char** outKind,
    const char** outName,
    size_t* outNameLen,
    const char** outBody,
    size_t* outBodyLen)
{
    const char* colon;
    size_t headLen;
    const char* body;
    size_t bodyLen;

    while ( len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n') ) {
        len -= 1;
    }
    if ( len == 0 || line[0] == ' ' || line[0] == '\t' ) {
        return 0;
    }
    colon = (const char*)memchr(line, ':', len);
    if ( colon == NULL ) {
        return 0;
    }
    headLen = (size_t)(colon - line);
    body = colon + 1;
    bodyLen = len - headLen - 1;
    if ( bodyLen > 0 && (*body == ' ' || *body == '\t') ) {
        body += 1;
        bodyLen -= 1;
    }

    *outName = NULL;
    *outNameLen = 0;
    if ( headLen == 4 && strncmp(line, "desc", 4) == 0 ) {
        *outKind = "desc";
    } else if ( headLen == 4 && strncmp(line, "note", 4) == 0 ) {
        *outKind = "note";
    } else if ( headLen == 7 && strncmp(line, "example", 7) == 0 ) {
        *outKind = "example";
    } else if ( headLen > 6 && strncmp(line, "param ", 6) == 0 ) {
        *outKind = "param";
        *outName = line + 6;
        *outNameLen = headLen - 6;
    } else if ( headLen > 7 && strncmp(line, "return ", 7) == 0 ) {
        *outKind = "return";
        *outName = line + 7;
        *outNameLen = headLen - 7;
    } else {
        return 0;
    }

    *outBody = body;
    *outBodyLen = bodyLen;
    return 1;
}

static void xl6XapiDocDumpDedentedString(XL6_Output* out, const char* text)
{
    const char* p;
    const char* line;
    size_t minIndent;
    int hasContent;

    if ( text == NULL ) {
        xl6XapiJsonString(out, "");
        return;
    }

    minIndent = (size_t)-1;
    hasContent = 0;
    p = text;
    while ( *p != '\0' ) {
        size_t indent = 0;
        const char* scan;

        line = p;
        while ( *p != '\0' && *p != '\n' ) {
            p += 1;
        }
        scan = line;
        while ( scan < p && (*scan == ' ' || *scan == '\t') ) {
            indent += 1;
            scan += 1;
        }
        if ( scan < p && *scan != '\r' ) {
            hasContent = 1;
            if ( indent < minIndent ) {
                minIndent = indent;
            }
        }
        if ( *p == '\n' ) {
            p += 1;
        }
    }
    if ( !hasContent ) {
        minIndent = 0;
    }

    xl6OutputPutc('"', out);
    p = text;
    while ( *p != '\0' ) {
        size_t skipped = 0;
        const char* lineEnd;

        line = p;
        while ( *p != '\0' && *p != '\n' ) {
            p += 1;
        }
        lineEnd = p;
        while ( line < lineEnd && skipped < minIndent && (*line == ' ' || *line == '\t') ) {
            line += 1;
            skipped += 1;
        }
        if ( lineEnd > line && lineEnd[-1] == '\r' ) {
            lineEnd -= 1;
        }
        xl6XapiJsonEscapedRange(out, line, (size_t)(lineEnd - line));
        if ( *p == '\n' ) {
            p += 1;
            if ( *p != '\0' ) {
                xl6OutputPuts("\\n", out);
            }
        }
    }
    xl6OutputPutc('"', out);
}

static void xl6XapiDocDumpField(
    XL6_Output* out,
    const char* kind,
    const char* name,
    size_t nameLen,
    XL6_XapiStringBuilder* body,
    int* first)
{
    if ( kind == NULL || body == NULL ) {
        return;
    }
    if ( !*first ) {
        xl6OutputPutc(',', out);
    }
    *first = 0;
    xl6OutputPuts("{\"kind\":", out);
    xl6XapiJsonString(out, kind);
    if ( name != NULL ) {
        xl6OutputPuts(",\"name\":\"", out);
        xl6XapiJsonEscapedRange(out, name, nameLen);
        xl6OutputPutc('"', out);
    }
    xl6OutputPuts(",\"text\":", out);
    xl6XapiDocDumpDedentedString(out, body->data != NULL ? body->data : "");
    xl6OutputPutc('}', out);
}

static void xl6XapiDumpDocJson(XL6_Output* out, const char* raw)
{
    XL6_XapiStringBuilder body;
    const char* p;
    const char* currentKind;
    const char* currentName;
    size_t baseIndent;
    size_t currentNameLen;
    int first;

    xl6OutputPuts("{\"raw\":", out);
    xl6XapiJsonString(out, raw != NULL ? raw : "");
    xl6OutputPuts(",\"fields\":[", out);
    if ( raw == NULL ) {
        xl6OutputPuts("]}", out);
        return;
    }

    xl6XapiStringBuilderInit(&body);
    p = raw;
    currentKind = NULL;
    currentName = NULL;
    currentNameLen = 0;
    baseIndent = xl6XapiDocBaseIndent(raw);
    first = 1;
    while ( *p != '\0' ) {
        const char* line = p;
        const char* parseLine;
        size_t len;
        size_t parseLen;
        const char* kind;
        const char* name;
        const char* fieldBody;
        size_t nameLen;
        size_t fieldBodyLen;

        while ( *p != '\0' && *p != '\n' ) {
            p += 1;
        }
        len = (size_t)(p - line);
        if ( *p == '\n' ) {
            p += 1;
            len += 1;
        }
        parseLine = line;
        parseLen = len;
        xl6XapiDocStripBaseIndent(&parseLine, &parseLen, baseIndent);

        if ( xl6XapiDocLineIsStart(parseLine, parseLen) || xl6XapiDocLineIsEnd(parseLine, parseLen) ) {
            continue;
        }
        if ( xl6XapiDocHeaderText(parseLine, parseLen, &kind, &name, &nameLen, &fieldBody, &fieldBodyLen) ) {
            xl6XapiDocDumpField(out, currentKind, currentName, currentNameLen, &body, &first);
            xl6XapiStringBuilderFree(&body);
            xl6XapiStringBuilderInit(&body);
            currentKind = kind;
            currentName = name;
            currentNameLen = nameLen;
            xl6XapiStringBuilderAppendRange(&body, fieldBody, fieldBodyLen);
        } else if ( currentKind != NULL ) {
            if ( body.len > 0 ) {
                xl6XapiStringBuilderAppendChar(&body, '\n');
            }
            while ( parseLen > 0 && (parseLine[parseLen - 1] == '\n' || parseLine[parseLen - 1] == '\r') ) {
                parseLen -= 1;
            }
            xl6XapiStringBuilderAppendRange(&body, parseLine, parseLen);
        }
    }
    xl6XapiDocDumpField(out, currentKind, currentName, currentNameLen, &body, &first);
    xl6XapiStringBuilderFree(&body);
    xl6OutputPuts("]}", out);
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

static char* xl6XapiBuildPackageId(const char* name, const char* version, const char* abiVersion)
{
    char buffer[512];
    int written;

    written = snprintf(buffer, sizeof(buffer), "%s@%s#abi%s",
                       name != NULL && name[0] != '\0' ? name : "app",
                       version != NULL && version[0] != '\0' ? version : "0.0.0",
                       abiVersion != NULL && abiVersion[0] != '\0' ? abiVersion : "1");
    if ( written <= 0 || (size_t)written >= sizeof(buffer) ) {
        return NULL;
    }
    return xl6XapiDupText(buffer);
}

static char* xl6XapiBuildModuleId(const char* packageId, const char* moduleName)
{
    XL6_XapiStringBuilder builder;
    char* out;

    xl6XapiStringBuilderInit(&builder);
    xl6XapiStringBuilderAppend(&builder, packageId != NULL && packageId[0] != '\0' ? packageId : "app@0.0.0#abi1");
    xl6XapiStringBuilderAppend(&builder, "::");
    xl6XapiStringBuilderAppend(&builder, moduleName != NULL && moduleName[0] != '\0' ? moduleName : "app");
    out = xl6XapiStringBuilderTake(&builder);
    xl6XapiStringBuilderFree(&builder);
    return out;
}

static char* xl6XapiBuildSignature(const XL6_XapiExport* item)
{
    XL6_XapiStringBuilder builder;
    size_t i;
    char* out;

    if ( item == NULL || item->kind == NULL ) {
        return xl6XapiDupText("");
    }

    xl6XapiStringBuilderInit(&builder);
    if ( xl6XapiTextEquals(item->kind, "function") ) {
        xl6XapiStringBuilderAppend(&builder, "func(");
        for ( i = 0; i < item->paramCount; ++i ) {
            if ( i > 0 ) {
                xl6XapiStringBuilderAppendChar(&builder, ',');
            }
            xl6XapiStringBuilderAppend(&builder, item->params[i].type != NULL ? item->params[i].type : "<missing-type>");
        }
        xl6XapiStringBuilderAppendChar(&builder, ')');
        if ( item->returnCount > 0 ) {
            xl6XapiStringBuilderAppend(&builder, "->");
            if ( item->returnCount > 1 ) {
                xl6XapiStringBuilderAppendChar(&builder, '(');
            }
            for ( i = 0; i < item->returnCount; ++i ) {
                if ( i > 0 ) {
                    xl6XapiStringBuilderAppendChar(&builder, ',');
                }
                xl6XapiStringBuilderAppend(&builder, item->returns[i] != NULL ? item->returns[i] : "<missing-type>");
            }
            if ( item->returnCount > 1 ) {
                xl6XapiStringBuilderAppendChar(&builder, ')');
            }
        }
    } else {
        xl6XapiStringBuilderAppend(&builder, item->kind);
        xl6XapiStringBuilderAppendChar(&builder, ':');
        xl6XapiStringBuilderAppend(&builder, item->type != NULL ? item->type : "<missing-type>");
    }
    out = xl6XapiStringBuilderTake(&builder);
    xl6XapiStringBuilderFree(&builder);
    return out;
}

static char* xl6XapiBuildExportId(const XL6_XapiModule* module, const XL6_XapiExport* item)
{
    XL6_XapiStringBuilder builder;
    char* out;

    xl6XapiStringBuilderInit(&builder);
    xl6XapiStringBuilderAppend(&builder, module != NULL && module->moduleId != NULL ? module->moduleId : "app@0.0.0#abi1::app");
    xl6XapiStringBuilderAppend(&builder, "::");
    xl6XapiStringBuilderAppend(&builder, item != NULL && item->name != NULL ? item->name : "");
    xl6XapiStringBuilderAppendChar(&builder, '(');
    xl6XapiStringBuilderAppend(&builder, item != NULL && item->signature != NULL ? item->signature : "");
    xl6XapiStringBuilderAppendChar(&builder, ')');
    out = xl6XapiStringBuilderTake(&builder);
    xl6XapiStringBuilderFree(&builder);
    return out;
}

static int xl6XapiFinalizeExportIdentity(XL6_XapiModule* module, XL6_XapiExport* item)
{
    if ( item == NULL ) {
        return 0;
    }
    if ( item->signature == NULL ) {
        item->signature = xl6XapiBuildSignature(item);
    }
    if ( item->exportId == NULL ) {
        item->exportId = xl6XapiBuildExportId(module, item);
    }
    return item->signature != NULL && item->exportId != NULL;
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

static const XL6_AstNode* xl6XapiClassBody(const XL6_AstNode* classNode)
{
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || classNode->childCount <= 1 ) {
        return NULL;
    }
    return classNode->children[1];
}

static const XL6_AstNode* xl6XapiClassBase(const XL6_AstNode* classNode)
{
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || classNode->childCount == 0 ) {
        return NULL;
    }
    return classNode->children[0];
}

static const XL6_AstNode* xl6XapiClassTypeParams(const XL6_AstNode* classNode)
{
    if ( classNode == NULL || classNode->kind != XL6_AST_CLASS || classNode->childCount <= 2 ) {
        return NULL;
    }
    return classNode->children[2];
}

static const XL6_AstNode* xl6XapiUnionBody(const XL6_AstNode* unionNode)
{
    if ( unionNode == NULL || unionNode->kind != XL6_AST_UNION || unionNode->childCount == 0 ) {
        return NULL;
    }
    return unionNode->children[0];
}

static size_t xl6XapiCountUnionFields(const XL6_AstNode* unionNode)
{
    const XL6_AstNode* body;
    size_t i;
    size_t count;

    body = xl6XapiUnionBody(unionNode);
    count = 0;
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD ) {
            count += 1;
        }
    }
    return count;
}

static size_t xl6XapiCountClassFields(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    size_t count;

    body = xl6XapiClassBody(classNode);
    count = 0;
    if ( body == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FIELD ) {
            count += 1;
        } else if ( member->kind == XL6_AST_UNION ) {
            count += xl6XapiCountUnionFields(member);
        }
    }
    return count;
}

static int xl6XapiSetField(
    XL6_XapiField* out,
    const XL6_AstNode* root,
    const XL6_AstNode* field,
    const char* storage)
{
    if ( out == NULL || field == NULL || field->kind != XL6_AST_FIELD ) {
        return 0;
    }
    out->name = xl6XapiDupText(field->text != NULL ? field->text : "");
    out->type = xl6XapiTypeToString(root, field->childCount > 0 ? field->children[0] : NULL);
    out->doc = field->doc != NULL ? xl6XapiDupText(field->doc) : NULL;
    out->storage = xl6XapiDupText(storage != NULL ? storage : "field");
    return out->name != NULL && out->type != NULL && out->storage != NULL;
}

static int xl6XapiFillUnionFields(
    XL6_XapiField* fields,
    size_t fieldCount,
    size_t* index,
    const XL6_AstNode* root,
    const XL6_AstNode* unionNode,
    const char* storage)
{
    const XL6_AstNode* body;
    size_t i;

    if ( fields == NULL || index == NULL ) {
        return 0;
    }
    body = xl6XapiUnionBody(unionNode);
    if ( body == NULL ) {
        return 1;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field == NULL || field->kind != XL6_AST_FIELD ) {
            continue;
        }
        if ( *index >= fieldCount || !xl6XapiSetField(&fields[*index], root, field, storage) ) {
            return 0;
        }
        *index += 1;
    }
    return 1;
}

static int xl6XapiFillClassFields(
    XL6_XapiField* fields,
    size_t fieldCount,
    const XL6_AstNode* root,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    size_t index;

    body = xl6XapiClassBody(classNode);
    index = 0;
    if ( body == NULL ) {
        return 1;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FIELD ) {
            if ( index >= fieldCount || !xl6XapiSetField(&fields[index], root, member, "field") ) {
                return 0;
            }
            index += 1;
        } else if ( member->kind == XL6_AST_UNION ) {
            if ( !xl6XapiFillUnionFields(fields, fieldCount, &index, root, member, "inline-union") ) {
                return 0;
            }
        }
    }
    return index == fieldCount;
}

static int xl6XapiTypeBuildFields(XL6_XapiTypeDecl* item, const XL6_AstNode* root, const XL6_AstNode* node)
{
    size_t fieldCount;
    size_t index;

    if ( item == NULL || node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS ) {
        fieldCount = xl6XapiCountClassFields(node);
    } else if ( node->kind == XL6_AST_UNION ) {
        fieldCount = xl6XapiCountUnionFields(node);
    } else {
        return 1;
    }
    if ( fieldCount == 0 ) {
        return 1;
    }
    item->fields = (XL6_XapiField*)xrtCalloc(fieldCount, sizeof(XL6_XapiField));
    if ( item->fields == NULL ) {
        return 0;
    }
    item->fieldCount = fieldCount;
    if ( node->kind == XL6_AST_CLASS ) {
        return xl6XapiFillClassFields(item->fields, item->fieldCount, root, node);
    }
    index = 0;
    return xl6XapiFillUnionFields(item->fields, item->fieldCount, &index, root, node, "union") &&
           index == item->fieldCount;
}

static int xl6XapiTypeBuildClassShape(XL6_XapiTypeDecl* item, const XL6_AstNode* node)
{
    const XL6_AstNode* base;
    const XL6_AstNode* params;
    size_t i;

    if ( item == NULL || node == NULL || node->kind != XL6_AST_CLASS ) {
        return 1;
    }
    base = xl6XapiClassBase(node);
    if ( base != NULL && base->text != NULL && base->text[0] != '\0' ) {
        item->base = xl6XapiDupText(base->text);
        if ( item->base == NULL ) {
            return 0;
        }
    }

    params = xl6XapiClassTypeParams(node);
    if ( params == NULL || params->childCount == 0 ) {
        return 1;
    }
    item->typeParams = (char**)xrtCalloc(params->childCount, sizeof(char*));
    if ( item->typeParams == NULL ) {
        return 0;
    }
    item->typeParamCount = params->childCount;
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        item->typeParams[i] = xl6XapiDupText(param != NULL && param->text != NULL ? param->text : "");
        if ( item->typeParams[i] == NULL ) {
            return 0;
        }
    }
    return 1;
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
    xrtFree(module->packageName);
    xrtFree(module->packageVersion);
    xrtFree(module->packageAbiVersion);
    xrtFree(module->packageId);
    xrtFree(module->name);
    xrtFree(module->moduleId);
    xrtFree(module->source);
    xrtFree(module->sourceHash);
    for ( i = 0; i < module->exportCount; ++i ) {
        XL6_XapiExport* item = &module->exports[i];
        xrtFree(item->kind);
        xrtFree(item->name);
        xrtFree(item->cSymbol);
        xrtFree(item->type);
        xrtFree(item->signature);
        xrtFree(item->exportId);
        xrtFree(item->callingConvention);
        xrtFree(item->varargMode);
        xrtFree(item->doc);
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
        xrtFree(module->types[i].doc);
        xrtFree(module->types[i].base);
        for ( j = 0; j < module->types[i].typeParamCount; ++j ) {
            xrtFree(module->types[i].typeParams[j]);
        }
        xrtFree(module->types[i].typeParams);
        for ( j = 0; j < module->types[i].fieldCount; ++j ) {
            xrtFree(module->types[i].fields[j].name);
            xrtFree(module->types[i].fields[j].type);
            xrtFree(module->types[i].fields[j].doc);
            xrtFree(module->types[i].fields[j].storage);
        }
        xrtFree(module->types[i].fields);
    }
    xrtFree(module->types);
    for ( i = 0; i < module->importCount; ++i ) {
        xrtFree(module->imports[i].target);
        xrtFree(module->imports[i].mode);
        xrtFree(module->imports[i].resolvedKind);
        xrtFree(module->imports[i].manifestPath);
        xrtFree(module->imports[i].interfacePath);
        xrtFree(module->imports[i].packageId);
    }
    xrtFree(module->imports);
    for ( i = 0; i < module->linkCount; ++i ) {
        xrtFree(module->links[i]);
    }
    xrtFree(module->links);
    for ( i = 0; i < module->loadlibCount; ++i ) {
        xrtFree(module->loadlibs[i]);
    }
    xrtFree(module->loadlibs);
    for ( i = 0; i < module->cSourceCount; ++i ) {
        xrtFree(module->cSources[i]);
    }
    xrtFree(module->cSources);
    xrtFree(module->nativeLoadName);
    xrtFree(module->nativeVersion);
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
    item->doc = node != NULL && node->doc != NULL ? xl6XapiDupText(node->doc) : NULL;
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
    item->doc = node != NULL && node->doc != NULL ? xl6XapiDupText(node->doc) : NULL;
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
    module->packageName = xl6XapiDupText(module->name != NULL ? module->name : "app");
    module->packageVersion = xl6XapiDupText("0.0.0");
    module->packageAbiVersion = xl6XapiDupText("1");
    module->packageId = xl6XapiBuildPackageId(module->packageName, module->packageVersion, module->packageAbiVersion);
    module->moduleId = xl6XapiBuildModuleId(module->packageId, module->name);
    module->source = xl6XapiDupText(sourcePath != NULL ? sourcePath : "");
    module->sourceHash = xl6XapiDupText("");
    if ( module->name == NULL || module->packageName == NULL || module->packageVersion == NULL ||
         module->packageAbiVersion == NULL || module->packageId == NULL || module->moduleId == NULL ||
         module->source == NULL || module->sourceHash == NULL ) {
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
            } else if ( child->kind == XL6_AST_CLASS || child->kind == XL6_AST_CUSTOM || child->kind == XL6_AST_UNION ) {
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
                if ( !xl6XapiFinalizeExportIdentity(module, &module->exports[exportIndex]) ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                exportIndex += 1;
            } else if ( child->kind == XL6_AST_EXPORT_VAR || child->kind == XL6_AST_EXPORT_CONST ) {
                if ( !xl6XapiExportBuildVar(&module->exports[exportIndex], root, child) ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                if ( !xl6XapiFinalizeExportIdentity(module, &module->exports[exportIndex]) ) {
                    xl6XapiModuleFree(module);
                    return 0;
                }
                exportIndex += 1;
            } else if ( child->kind == XL6_AST_CLASS || child->kind == XL6_AST_CUSTOM || child->kind == XL6_AST_UNION ) {
                module->types[typeIndex].kind = xl6XapiDupText(child->kind == XL6_AST_CUSTOM ? "custom" : (child->kind == XL6_AST_UNION ? "union" : "class"));
                module->types[typeIndex].name = xl6XapiDupText(child->text != NULL ? child->text : "");
                module->types[typeIndex].doc = child->doc != NULL ? xl6XapiDupText(child->doc) : NULL;
                if ( module->types[typeIndex].kind == NULL ||
                     module->types[typeIndex].name == NULL ||
                     !xl6XapiTypeBuildClassShape(&module->types[typeIndex], child) ||
                     !xl6XapiTypeBuildFields(&module->types[typeIndex], root, child) ) {
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
                module->imports[importIndex].interfacePath = xl6XapiDupText(resolved.interfacePath);
                module->imports[importIndex].packageId = xl6XapiDupText(resolved.packageId);
                if ( module->imports[importIndex].target == NULL ||
                     module->imports[importIndex].mode == NULL ||
                     module->imports[importIndex].resolvedKind == NULL ||
                     module->imports[importIndex].manifestPath == NULL ||
                     module->imports[importIndex].interfacePath == NULL ||
                     module->imports[importIndex].packageId == NULL ) {
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

static void xl6XapiDumpStringArray(XL6_Output* out, char** items, size_t count)
{
    size_t i;

    xl6OutputPutc('[', out);
    for ( i = 0; i < count; ++i ) {
        if ( i > 0 ) {
            xl6OutputPutc(',', out);
        }
        xl6XapiJsonString(out, items[i]);
    }
    xl6OutputPutc(']', out);
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
    xl6OutputPuts("  \"package\":{\"name\":", out);
    xl6XapiJsonString(out, module != NULL ? module->packageName : "app");
    xl6OutputPuts(",\"version\":", out);
    xl6XapiJsonString(out, module != NULL ? module->packageVersion : "0.0.0");
    xl6OutputPuts(",\"abiVersion\":", out);
    xl6XapiJsonString(out, module != NULL ? module->packageAbiVersion : "1");
    xl6OutputPuts(",\"packageId\":", out);
    xl6XapiJsonString(out, module != NULL ? module->packageId : "app@0.0.0#abi1");
    xl6OutputPuts("},\n", out);
    xl6OutputPuts("  \"module\":{\"name\":", out);
    xl6XapiJsonString(out, module != NULL ? module->name : "app");
    xl6OutputPuts(",\"moduleId\":", out);
    xl6XapiJsonString(out, module != NULL ? module->moduleId : "app@0.0.0#abi1::app");
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
            if ( item->signature != NULL ) {
                xl6OutputPuts(",\"signature\":", out);
                xl6XapiJsonString(out, item->signature);
            }
            if ( item->exportId != NULL ) {
                xl6OutputPuts(",\"exportId\":", out);
                xl6XapiJsonString(out, item->exportId);
            }
            if ( item->type != NULL ) {
                xl6OutputPuts(",\"type\":", out);
                xl6XapiJsonString(out, item->type);
            }
            if ( item->doc != NULL ) {
                xl6OutputPuts(",\"doc\":", out);
                xl6XapiDumpDocJson(out, item->doc);
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
            if ( module->types[i].base != NULL && module->types[i].base[0] != '\0' ) {
                xl6OutputPuts(",\"base\":", out);
                xl6XapiJsonString(out, module->types[i].base);
            }
            if ( module->types[i].typeParamCount > 0 ) {
                xl6OutputPuts(",\"typeParams\":[", out);
                for ( j = 0; j < module->types[i].typeParamCount; ++j ) {
                    if ( j > 0 ) {
                        xl6OutputPutc(',', out);
                    }
                    xl6XapiJsonString(out, module->types[i].typeParams[j]);
                }
                xl6OutputPutc(']', out);
            }
            if ( module->types[i].doc != NULL ) {
                xl6OutputPuts(",\"doc\":", out);
                xl6XapiDumpDocJson(out, module->types[i].doc);
            }
            if ( module->types[i].fieldCount > 0 ) {
                xl6OutputPuts(",\"fields\":[", out);
                for ( j = 0; j < module->types[i].fieldCount; ++j ) {
                    const XL6_XapiField* field = &module->types[i].fields[j];
                    if ( j > 0 ) {
                        xl6OutputPutc(',', out);
                    }
                    xl6OutputPuts("{\"name\":", out);
                    xl6XapiJsonString(out, field->name);
                    xl6OutputPuts(",\"type\":", out);
                    xl6XapiJsonString(out, field->type);
                    xl6OutputPuts(",\"storage\":", out);
                    xl6XapiJsonString(out, field->storage);
                    if ( field->doc != NULL ) {
                        xl6OutputPuts(",\"doc\":", out);
                        xl6XapiDumpDocJson(out, field->doc);
                    }
                    xl6OutputPutc('}', out);
                }
                xl6OutputPutc(']', out);
            }
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
            xl6OutputPuts(",\"interface\":", out);
            xl6XapiJsonString(out, module->imports[i].interfacePath);
            xl6OutputPuts(",\"packageId\":", out);
            xl6XapiJsonString(out, module->imports[i].packageId);
            xl6OutputPutc('}', out);
        }
        if ( module->importCount > 0 ) {
            xl6OutputPutc('\n', out);
        }
    }
    xl6OutputPuts("  ],\n", out);
    xl6OutputPuts("  \"links\":", out);
    xl6XapiDumpStringArray(out, module != NULL ? module->links : NULL, module != NULL ? module->linkCount : 0);
    xl6OutputPuts(",\n", out);
    xl6OutputPuts("  \"loadlibs\":", out);
    xl6XapiDumpStringArray(out, module != NULL ? module->loadlibs : NULL, module != NULL ? module->loadlibCount : 0);
    xl6OutputPuts(",\n", out);
    xl6OutputPuts("  \"cSources\":", out);
    xl6XapiDumpStringArray(out, module != NULL ? module->cSources : NULL, module != NULL ? module->cSourceCount : 0);
    xl6OutputPuts(",\n", out);
    xl6OutputPuts("  \"native\":{\"singleton\":", out);
    xl6OutputPuts(module != NULL && module->nativeSingleton ? "true" : "false", out);
    xl6OutputPuts(",\"loadName\":", out);
    xl6XapiJsonString(out, module != NULL ? module->nativeLoadName : "");
    xl6OutputPuts(",\"version\":", out);
    xl6XapiJsonString(out, module != NULL ? module->nativeVersion : "");
    xl6OutputPuts("},\n", out);
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
