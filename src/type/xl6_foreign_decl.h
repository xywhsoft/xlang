#ifndef XL6_FOREIGN_DECL_H
#define XL6_FOREIGN_DECL_H

#include "src/ast/xl6_ast.h"
#include <string.h>

/* Declaration facts only. Backends consume the corresponding closed XLIR
 * contract, never these AST nodes. A caller lifetime is explicit: this view
 * does not retain, destroy, or extend the lifetime of its C object. */
typedef struct XL6_ForeignClassDecl {
    char cName[64];
    char header[256];
} XL6_ForeignClassDecl;

static inline int xl6ForeignIdentifier(const char* text)
{
    const unsigned char* p = (const unsigned char*)text;
    if (!p || !((*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') || *p == '_')) return 0;
    while (*++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}

static inline int xl6ForeignQuotedText(const XL6_AstNode* node,
    char* output, size_t capacity)
{
    size_t size;
    const char* text = node ? node->text : NULL;
    if (!text || !output || !capacity) return 0;
    size = strlen(text);
    if (size < 2 || text[0] != '"' || text[size-1] != '"' ||
        size - 2 >= capacity) return 0;
    /* Header/type metadata is deliberately a plain portable spelling, not
     * executable C or an escaped language string. */
    for (size_t i = 1; i + 1 < size; ++i)
        if ((unsigned char)text[i] < 32 || text[i] == '\\' || text[i] == '"') return 0;
    memcpy(output, text + 1, size - 2);
    output[size - 2] = 0;
    return 1;
}

static inline int xl6ForeignHeaderName(const char* text)
{
    if (!text || !*text || *text == '/' || strstr(text, "..")) return 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.' || *p == '/')) return 0;
    return 1;
}

/* 0: ordinary declaration; 1: valid external view; -1: malformed contract.
 * Keep parsing/semantic diagnostics and the AST->IR handoff on one schema. */
static inline int xl6ForeignClassRead(const XL6_AstNode* node,
    XL6_ForeignClassDecl* output, const char** error)
{
    XL6_ForeignClassDecl result = {0};
    const XL6_AstNode* declaration;
    unsigned seen = 0;
    if (!node || node->kind != XL6_AST_CLASS ||
        !(node->flags & XL6_AST_FLAG_EXTERN)) return 0;
    if (error) *error = "invalid external class declaration contract";
    if (node->childCount != 4 || !(declaration = node->children[3]) ||
        declaration->kind != XL6_AST_FIELD_ATTR || !declaration->text ||
        strcmp(declaration->text, "c_type") || declaration->childCount < 1 ||
        !xl6ForeignQuotedText(declaration->children[0], result.cName, sizeof(result.cName)) ||
        !xl6ForeignIdentifier(result.cName)) return -1;
    for (size_t i = 1; i < declaration->childCount; ++i) {
        const XL6_AstNode* attr = declaration->children[i];
        char value[256];
        if (!attr || attr->kind != XL6_AST_FIELD_ATTR || !attr->text ||
            attr->childCount != 1 ||
            !xl6ForeignQuotedText(attr->children[0], value, sizeof(value))) return -1;
        if (!strcmp(attr->text, "header")) {
            if ((seen & 1) || !xl6ForeignHeaderName(value)) return -1;
            memcpy(result.header, value, strlen(value) + 1); seen |= 1;
        } else if (!strcmp(attr->text, "lifetime")) {
            if ((seen & 2) || strcmp(value, "caller")) return -1;
            seen |= 2;
        } else return -1;
    }
    if (seen != 3) {
        if (error) *error = "external class requires @header(\"...\") and @lifetime(\"caller\")";
        return -1;
    }
    if (output) *output = result;
    return 1;
}

#endif
