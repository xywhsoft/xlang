#include "src/sem/xl6_sem_docs.h"

#include "src/common/xl6_xrt.h"

#include <string.h>

static void xl6SemDocsErrorNode(XL6_DiagList* diags, const XL6_AstNode* node, const char* message)
{
    if ( diags == NULL ) {
        return;
    }
    xl6DiagAdd(diags, node != NULL ? node->line : 1, node != NULL ? node->column : 1, message);
}

static int xl6SemDocsTextEquals(const char* left, const char* right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
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
            node->kind == XL6_AST_UNION ||
            node->kind == XL6_AST_FIELD ||
            node->kind == XL6_AST_CREATE ||
            node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ||
            node->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
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

static int xl6SemDocLineLooksLikeHeader(const char* text, size_t len)
{
    if ( text == NULL ) {
        return 0;
    }
    len = xl6SemDocTrimEndLen(text, len);
    if ( len == 0 || text[0] == ' ' || text[0] == '\t' ) {
        return 0;
    }
    return memchr(text, ':', len) != NULL;
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
    XL6_DiagList* diags,
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
            xl6SemDocsErrorNode(diags, node, "#doc 字段检查内存不足");
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
    case XL6_AST_NATIVE_EXTERN_FUNC:
        return node->childCount > 1 ? node->children[1] : NULL;
    case XL6_AST_CREATE:
        return node->childCount > 0 ? node->children[0] : NULL;
    case XL6_AST_NATIVE_EXTERN_SPECIAL:
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
    case XL6_AST_NATIVE_EXTERN_FUNC:
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
             typeNode != NULL && xl6SemDocsTextEquals(typeNode->text, "array<var>") ) {
            return 1;
        }
        if ( xl6SemDocRangeEquals(name, nameLen, "***") &&
             typeNode != NULL && xl6SemDocsTextEquals(typeNode->text, "dict<var>") ) {
            return 1;
        }
        if ( param->text != NULL && xl6SemDocRangeEquals(name, nameLen, param->text) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6SemCheckDocField(
    XL6_DiagList* diags,
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
    if ( xl6SemDocsTextEquals(kind, "desc") && xl6SemDocSeenContains(*fields, *fieldCount, kind, "", 0) ) {
        xl6SemDocsErrorNode(diags, node, "#doc desc 字段不能重复");
        ok = 0;
    } else if ( (xl6SemDocsTextEquals(kind, "param") || xl6SemDocsTextEquals(kind, "return")) &&
                xl6SemDocSeenContains(*fields, *fieldCount, kind, name, nameLen) ) {
        xl6SemDocsErrorNode(diags, node, "#doc param/return 字段不能重复描述同一个名称");
        ok = 0;
    }

    if ( xl6SemDocsTextEquals(kind, "param") ) {
        params = xl6SemDocParamsForNode(node);
        if ( nameLen == 0 ) {
            xl6SemDocsErrorNode(diags, node, "#doc param 字段缺少参数名");
            ok = 0;
        } else if ( !xl6SemDocParamExists(params, name, nameLen) ) {
            xl6SemDocsErrorNode(diags, node, "#doc param 字段必须对应目标声明中的参数");
            ok = 0;
        }
    } else if ( xl6SemDocsTextEquals(kind, "return") ) {
        returns = xl6SemDocReturnsForNode(node);
        if ( nameLen == 0 ) {
            xl6SemDocsErrorNode(diags, node, "#doc return 字段缺少返回值名");
            ok = 0;
        } else if ( returns == NULL || returns->childCount == 0 ) {
            xl6SemDocsErrorNode(diags, node, "#doc return 字段只能用于有返回值的函数声明");
            ok = 0;
        }
    }

    if ( !xl6SemDocAddSeen(diags, node, fields, fieldCount, fieldCapacity, kind, name, nameLen) ) {
        ok = 0;
    }
    return ok;
}

static int xl6SemCheckDocTarget(XL6_DiagList* diags, XL6_AstNode* node)
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
    int hasStructureError;
    size_t baseIndent;

    if ( node == NULL || node->doc == NULL || node->doc[0] == '\0' ) {
        return 1;
    }
    ok = 1;
    if ( !xl6SemIsDocumentableNode(node) ) {
        xl6SemDocsErrorNode(diags, node, "#doc 只能绑定到函数、类型、变量、常量、字段、extern/export 声明");
        ok = 0;
    }

    fields = NULL;
    fieldCount = 0;
    fieldCapacity = 0;
    seenField = 0;
    hasStructureError = 0;
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
            if ( !xl6SemCheckDocField(diags, node, &fields, &fieldCount, &fieldCapacity, kind, name, nameLen) ) {
                ok = 0;
            }
            continue;
        }

        if ( parseLineLen > 0 && (parseLine[0] == ' ' || parseLine[0] == '\t') ) {
            if ( !seenField && !hasStructureError ) {
                xl6SemDocsErrorNode(diags, node, "#doc 正文必须位于字段头之后");
                ok = 0;
                hasStructureError = 1;
            }
            continue;
        }

        if ( xl6SemDocLineLooksLikeHeader(parseLine, parseLineLen) ) {
            xl6SemDocsErrorNode(diags, node, "#doc 未知字段");
        } else {
            xl6SemDocsErrorNode(diags, node, "#doc 存在未知字段或未归属字段的裸文本");
        }
        ok = 0;
        hasStructureError = 1;
    }

    if ( fieldCount == 0 && !hasStructureError ) {
        xl6SemDocsErrorNode(diags, node, "#doc 至少需要一个字段");
        ok = 0;
    }
    xrtFree(fields);
    return ok;
}

static int xl6SemCheckDocsRecursive(XL6_DiagList* diags, XL6_AstNode* node)
{
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }
    ok = xl6SemCheckDocTarget(diags, node);
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6SemCheckDocsRecursive(diags, node->children[i]) ) {
            ok = 0;
        }
    }
    return ok;
}


int xl6SemCheckDocs(XL6_AstNode* root, XL6_DiagList* diags)
{
    return xl6SemCheckDocsRecursive(diags, root);
}
