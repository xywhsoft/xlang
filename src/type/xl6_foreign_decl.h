#ifndef XL6_FOREIGN_DECL_H
#define XL6_FOREIGN_DECL_H

#include "src/ast/xl6_ast.h"
#include "src/type/xl6_foreign_call_contract.h"
#include <string.h>

/* Declaration facts only. Backends consume the corresponding closed XLIR
 * contract, never these AST nodes. A caller lifetime is explicit: this view
 * does not retain, destroy, or extend the lifetime of its C object. */
typedef struct XL6_ForeignClassDecl {
    char cName[64];
    char header[256];
} XL6_ForeignClassDecl;

static inline const XL6_AstNode* xl6ForeignFunctionAttributes(const XL6_AstNode* node)
{
    if (!node || !node->childCount) return NULL;
    const XL6_AstNode* last = node->children[node->childCount - 1];
    return last && last->kind == XL6_AST_FIELD_ATTR && last->text &&
        !strcmp(last->text, "c_call") ? last : NULL;
}

static inline int xl6ForeignDecimalInt(const XL6_AstNode* node, int32* output)
{
    int negative = node && node->kind == XL6_AST_UNARY && node->text &&
        !strcmp(node->text, "-") && node->childCount == 1;
    if (negative) node = node->children[0];
    if (!node || node->kind != XL6_AST_INT || !node->text || !*node->text) return 0;
    uint64 value = 0, limit = negative ? UINT64_C(2147483648) : INT32_MAX;
    for (const unsigned char* p = (const unsigned char*)node->text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (limit - (*p - '0')) / 10u) return 0;
        value = value * 10u + (*p - '0');
    }
    *output = negative ? (int32)(-(int64)value) : (int32)value;
    return 1;
}

static inline int xl6ForeignFunctionRead(const XL6_AstNode* node,
    XL6_ForeignCallFacts* output)
{
    XL6_ForeignCallFacts facts = {0};
    const XL6_AstNode* attributes = xl6ForeignFunctionAttributes(node);
    for (size_t i = 0; attributes && i < attributes->childCount; ++i) {
        const XL6_AstNode* attr = attributes->children[i];
        int32 index;
        if (attr && attr->text && !strcmp(attr->text, "c_result")) {
            const XL6_AstNode* value = attr->childCount == 1 ? attr->children[0] : NULL;
            if (facts.borrowedViewResult || !value || value->kind != XL6_AST_STRING || !value->text) return 0;
            if (!strcmp(value->text, "\"text_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_TEXT;
            else if (!strcmp(value->text, "\"bytes_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_BYTES;
            else return 0;
            continue;
        }
        if (!attr || !attr->text || !attr->childCount ||
            !xl6ForeignDecimalInt(attr->children[0], &index) || index < 0 || index >= 64) return 0;
        if (!strcmp(attr->text, "c_text_view")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.textViews & bit)) return 0;
            facts.textViews |= bit;
        } else if (!strcmp(attr->text, "c_bytes_view")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.bytesViews & bit)) return 0;
            facts.bytesViews |= bit;
        } else if (!strcmp(attr->text, "c_ref")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.constRecordAddresses & bit)) return 0;
            facts.constRecordAddresses |= bit;
        } else if (!strcmp(attr->text, "c_static_callback")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.staticCallbackAddresses & bit)) return 0;
            facts.staticCallbackAddresses |= bit;
        } else if (!strcmp(attr->text, "factory")) {
            if (facts.outputIndexPlusOne || attr->childCount != 3 ||
                !xl6ForeignDecimalInt(attr->children[1], &facts.successCode) ||
                !attr->children[2] || attr->children[2]->kind != XL6_AST_NAME ||
                !attr->children[2]->text) return 0;
            facts.outputIndexPlusOne = (uint32)index + 1;
            facts.rollbackSymbol = attr->children[2]->semanticSymbolId;
        } else return 0;
    }
    if ((facts.textViews & (facts.constRecordAddresses | facts.staticCallbackAddresses | facts.bytesViews)) ||
        (facts.bytesViews & (facts.constRecordAddresses | facts.staticCallbackAddresses)) ||
        (facts.borrowedViewResult && facts.outputIndexPlusOne)) return 0;
    if (output) *output = facts;
    return 1;
}

/* Parser-owned fixed C array vocabulary. '$' is not a source identifier, so
 * the family is introduced only by a checked field declarator, not by an
 * arbitrary user spelling. Extents remain ordinary exact type identities in
 * the frontend; XLIR stores the decoded extent as a closed numeric fact. */
static inline int xl6ForeignArrayExtent(const char* name, uint64* extent)
{
    static const char prefix[] = "c_array$";
    uint64 value = 0;
    if (!name || strncmp(name, prefix, sizeof(prefix)-1)) return 0;
    const unsigned char* p = (const unsigned char*)name + sizeof(prefix)-1;
    if (!*p || *p == '0') return 0;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (UINT32_MAX - (*p - '0')) / 10u) return 0;
        value = value * 10u + (*p - '0');
    }
    if (!value) return 0;
    if (extent) *extent = value;
    return 1;
}

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

/* A typedef naming a pointer, or a bounded ordinary C pointer declarator.
 * No parentheses, arrays, preprocessor tokens or arbitrary C expressions.
 * Pointee qualifications remain part of the exact physical call signature. */
static inline int xl6ForeignPointerSpelling(const char* text)
{
    char identifier[64];
    const char* p = text;
    size_t size = 0, pointers = 0;
    if (xl6ForeignIdentifier(text)) return 1;
    if (!p) return 0;
    if (!strncmp(p, "const ", 6)) p += 6;
    if (!strncmp(p, "struct ", 7)) p += 7;
    else if (!strncmp(p, "union ", 6)) p += 6;
    while (*p && *p != ' ' && *p != '*') {
        if (size + 1 >= sizeof(identifier)) return 0;
        identifier[size++] = *p++;
    }
    identifier[size] = 0;
    if (!xl6ForeignIdentifier(identifier)) return 0;
    while (*p == ' ') ++p;
    while (*p == '*') {
        if (++pointers > 8) return 0;
        ++p;
        while (*p == ' ') ++p;
    }
    return pointers && !*p;
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
        !xl6ForeignPointerSpelling(result.cName)) return -1;
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

/* C records are trivial values. Their field layout belongs to the named C
 * declaration, and is checked against the ordered language field schema. */
static inline int xl6ForeignRecordRead(const XL6_AstNode* node,
    XL6_ForeignClassDecl* output, const char** error)
{
    XL6_ForeignClassDecl result = {0};
    const XL6_AstNode* declaration;
    if (!node || node->kind != XL6_AST_C_RECORD) return 0;
    if (error) *error = "C record requires a C type identifier and one @header(\"...\")";
    if (!(node->flags & XL6_AST_FLAG_EXTERN) || node->childCount != 2 ||
        !(declaration = node->children[1]) ||
        declaration->kind != XL6_AST_FIELD_ATTR || !declaration->text ||
        strcmp(declaration->text, "c_type") || declaration->childCount != 2 ||
        !xl6ForeignQuotedText(declaration->children[0], result.cName, sizeof(result.cName)) ||
        !xl6ForeignIdentifier(result.cName)) return -1;
    const XL6_AstNode* header = declaration->children[1];
    if (!header || header->kind != XL6_AST_FIELD_ATTR || !header->text ||
        strcmp(header->text, "header") || header->childCount != 1 ||
        !xl6ForeignQuotedText(header->children[0], result.header, sizeof(result.header)) ||
        !xl6ForeignHeaderName(result.header)) return -1;
    if (output) *output = result;
    return 1;
}

#endif
