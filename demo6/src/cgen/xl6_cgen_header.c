#include "src/cgen/xl6_cgen.h"
#include "src/cgen/xl6_cgen_lifecycle.h"

static int xl6CgenHeaderTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static unsigned int xl6CgenHeaderNextUtf8Codepoint(const unsigned char** p)
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

static void xl6CgenHeaderFputsCIdentCodepoint(XL6_Output* out, unsigned int cp, int isFirst)
{
    if ( out == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
         (!isFirst && cp >= '0' && cp <= '9') ) {
        xl6OutputPutc((int)cp, out);
    } else if ( cp <= 0xFFFFU ) {
        xl6OutputPrintf(out, "_u%04X_", cp);
    } else {
        xl6OutputPrintf(out, "_u%06X_", cp);
    }
}

static void xl6CgenHeaderFputsCIdent(XL6_Output* out, const char* text)
{
    const unsigned char* p;
    int isFirst;

    if ( text == NULL || text[0] == '\0' ) {
        xl6OutputPutc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        xl6CgenHeaderFputsCIdentCodepoint(out, xl6CgenHeaderNextUtf8Codepoint(&p), isFirst);
        isFirst = 0;
    }
}

static void xl6CgenHeaderAppendText(char* buffer, size_t capacity, const char* text)
{
    size_t used;
    size_t left;
    size_t copyLen;

    if ( buffer == NULL || capacity == 0 || text == NULL ) {
        return;
    }
    used = strlen(buffer);
    if ( used + 1 >= capacity ) {
        return;
    }
    left = capacity - used - 1;
    copyLen = strlen(text);
    if ( copyLen > left ) {
        copyLen = left;
    }
    if ( copyLen > 0 ) {
        memcpy(buffer + used, text, copyLen);
    }
    buffer[used + copyLen] = '\0';
}

static void xl6CgenHeaderAppendEncodedText(char* buffer, size_t capacity, const char* text)
{
    const unsigned char* p;
    char tmp[32];

    if ( text == NULL || text[0] == '\0' ) {
        xl6CgenHeaderAppendText(buffer, capacity, "_");
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        unsigned int cp = xl6CgenHeaderNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
            tmp[0] = (char)cp;
            tmp[1] = '\0';
        } else if ( cp == '_' ) {
            tmp[0] = '_';
            tmp[1] = '_';
            tmp[2] = '\0';
        } else if ( cp <= 0xFFFFU ) {
            snprintf(tmp, sizeof(tmp), "_u%04X_", cp);
        } else {
            snprintf(tmp, sizeof(tmp), "_u%06X_", cp);
        }
        xl6CgenHeaderAppendText(buffer, capacity, tmp);
    }
}

static void xl6CgenHeaderUnionTypeTextName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenHeaderAppendText(buffer, capacity, "XL_Union_");
    xl6CgenHeaderAppendEncodedText(buffer, capacity, name);
}

static void xl6CgenHeaderClassTypeTextName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenHeaderAppendText(buffer, capacity, "XL_Class_");
    xl6CgenHeaderAppendEncodedText(buffer, capacity, name);
}

static void xl6CgenHeaderCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
        xl6OutputPuts("_", out);
        return;
    }

    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        for ( i = 1; i + 1 < len; ++i ) {
            xl6OutputPutc(text[i], out);
        }
        return;
    }
    xl6OutputPuts(text, out);
}

static int xl6CgenHeaderIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6CgenHeaderTypeAliasTargetType(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenHeaderExternalVarTypeNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenHeaderFindTypeAlias(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( xl6CgenHeaderIsTypeAliasDecl(node) && xl6CgenHeaderTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenHeaderFindUnion(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_UNION && xl6CgenHeaderTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenHeaderFindClass(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS && xl6CgenHeaderTextEquals(node->text, name) ) {
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
        typeNode = xl6CgenHeaderTypeAliasTargetType(aliasNode);
    }
    return typeNode;
}

static const char* xl6CgenHeaderTypeName(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    static char unionTypeName[256];
    static char classTypeName[256];

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_char") ) {
        return "char";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_schar") ) {
        return "signed char";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_uchar") ) {
        return "unsigned char";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_short") ) {
        return "short";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_ushort") ) {
        return "unsigned short";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_int") ) {
        return "int";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_uint") ) {
        return "unsigned int";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_long") ) {
        return "long";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_ulong") ) {
        return "unsigned long";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_longlong") ) {
        return "long long";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_ulonglong") ) {
        return "unsigned long long";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_size") ) {
        return "size_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_ssize") ) {
        return "xl6_ssize_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_ptrdiff") ) {
        return "ptrdiff_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_intptr") ) {
        return "intptr_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_uintptr") ) {
        return "uintptr_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_bool") ) {
        return "bool";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_float") ) {
        return "float";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_double") ) {
        return "double";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "c_wchar") ) {
        return "wchar_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "bool") ) {
        return "bool";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "int") || xl6CgenHeaderTextEquals(typeNode->text, "int64") ) {
        return "int64_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "int8") ) {
        return "int8_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "uint8") ) {
        return "uint8_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "int16") ) {
        return "int16_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "uint16") ) {
        return "uint16_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "int32") ) {
        return "int32_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "uint32") ) {
        return "uint32_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "uint64") ) {
        return "uint64_t";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "float") || xl6CgenHeaderTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    if ( xl6CgenHeaderFindUnion(root, typeNode->text) != NULL ) {
        xl6CgenHeaderUnionTypeTextName(unionTypeName, sizeof(unionTypeName), typeNode->text);
        return unionTypeName;
    }
    if ( xl6CgenHeaderFindClass(root, typeNode->text) != NULL ) {
        xl6CgenHeaderClassTypeTextName(classTypeName, sizeof(classTypeName), typeNode->text);
        return classTypeName;
    }
    return "void";
}

static void xl6CgenHeaderInlineUnionDecl(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* body;
    size_t i;

    if ( out == NULL || node == NULL ) {
        return;
    }
    xl6OutputPuts("    union {\n", out);
    body = node->childCount > 0 ? node->children[0] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("        ", out);
            xl6OutputPuts(xl6CgenHeaderTypeName(root, field->children[0]), out);
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, field->text);
            xl6OutputPuts(";\n", out);
        }
    }
    xl6OutputPuts("    }", out);
    if ( node->text != NULL ) {
        xl6OutputPutc(' ', out);
        xl6CgenHeaderFputsCIdent(out, node->text);
    }
    xl6OutputPuts(";\n", out);
}

static int xl6CgenHeaderNameListed(const char** names, size_t count, const char* name)
{
    size_t i;

    if ( names == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < count; ++i ) {
        if ( xl6CgenHeaderTextEquals(names[i], name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenHeaderUnionDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount);

static void xl6CgenHeaderClassDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount);

static void xl6CgenHeaderTypeDependencyDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount)
{
    const XL6_AstNode* decl;

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return;
    }

    decl = xl6CgenHeaderFindClass(root, typeNode->text);
    if ( decl != NULL ) {
        xl6CgenHeaderClassDecl(
            out,
            root,
            decl,
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount);
        return;
    }

    decl = xl6CgenHeaderFindUnion(root, typeNode->text);
    if ( decl != NULL ) {
        xl6CgenHeaderUnionDecl(
            out,
            root,
            decl,
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount);
    }
}

static void xl6CgenHeaderInlineUnionDependencyDecls(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount)
{
    const XL6_AstNode* body;
    size_t i;

    if ( out == NULL || root == NULL || node == NULL ) {
        return;
    }
    body = node->childCount > 0 ? node->children[0] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount);
        }
    }
}

static void xl6CgenHeaderUnionDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount)
{
    const XL6_AstNode* body;
    size_t i;
    char typeName[256];

    if ( out == NULL || root == NULL || node == NULL || node->text == NULL ||
         emittedUnions == NULL || emittedUnionCount == NULL ) {
        return;
    }
    if ( xl6CgenHeaderNameListed(emittedUnions, *emittedUnionCount, node->text) ) {
        return;
    }
    emittedUnions[*emittedUnionCount] = node->text;
    *emittedUnionCount += 1;

    body = node->childCount > 0 ? node->children[0] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount);
        }
    }

    xl6CgenHeaderUnionTypeTextName(typeName, sizeof(typeName), node->text);
    xl6OutputPrintf(out, "typedef union %s {\n", typeName);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", out);
            xl6OutputPuts(xl6CgenHeaderTypeName(root, field->children[0]), out);
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, field->text);
            xl6OutputPuts(";\n", out);
        }
    }
    xl6OutputPrintf(out, "} %s;\n", typeName);
}

static void xl6CgenHeaderClassDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    size_t i;
    char typeName[256];

    if ( out == NULL || root == NULL || node == NULL || node->text == NULL ||
         emittedClasses == NULL || emittedClassCount == NULL ) {
        return;
    }
    if ( xl6CgenHeaderNameListed(emittedClasses, *emittedClassCount, node->text) ) {
        return;
    }
    emittedClasses[*emittedClassCount] = node->text;
    *emittedClassCount += 1;

    base = node->childCount > 0 ? node->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        xl6CgenHeaderClassDecl(
            out,
            root,
            xl6CgenHeaderFindClass(root, base->text),
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount);
    }

    body = node->childCount > 1 ? node->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL ) {
                continue;
            }
            if ( field->kind == XL6_AST_UNION ) {
                xl6CgenHeaderInlineUnionDependencyDecls(
                    out,
                    root,
                    field,
                    emittedUnions,
                    emittedUnionCount,
                    emittedClasses,
                    emittedClassCount);
                continue;
            }
            if ( field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount);
        }
    }

    xl6CgenHeaderClassTypeTextName(typeName, sizeof(typeName), node->text);
    xl6OutputPrintf(out, "typedef struct %s {\n", typeName);
    if ( base != NULL && base->kind == XL6_AST_NAME && xl6CgenHeaderFindClass(root, base->text) != NULL ) {
        xl6OutputPuts("    ", out);
        xl6OutputPuts(xl6CgenHeaderTypeName(root, base), out);
        xl6OutputPuts(" base;\n", out);
    }
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL ) {
                continue;
            }
            if ( field->kind == XL6_AST_UNION ) {
                xl6CgenHeaderInlineUnionDecl(out, root, field);
                continue;
            }
            if ( field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", out);
            xl6OutputPuts(xl6CgenHeaderTypeName(root, field->children[0]), out);
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, field->text);
            xl6OutputPuts(";\n", out);
        }
    }
    xl6OutputPrintf(out, "} %s;\n", typeName);
}

static void xl6CgenHeaderExportFuncProto(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t i;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenHeaderTypeName(root, returns->children[0]), out);
    } else {
        xl6OutputPuts("void", out);
    }
    xl6OutputPutc(' ', out);
    xl6CgenHeaderCStringSymbol(out, node->text);
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( (params == NULL || params->childCount == 0) && (returns == NULL || returns->childCount <= 1) ) {
        xl6OutputPuts("void", out);
    } else {
        int wroteParam = 0;

        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                const XL6_AstNode* param = params->children[i];
                if ( wroteParam ) {
                    xl6OutputPuts(", ", out);
                }
                xl6OutputPuts(xl6CgenHeaderTypeName(root, param != NULL && param->childCount > 0 ? param->children[0] : NULL), out);
                xl6OutputPutc(' ', out);
                xl6CgenHeaderFputsCIdent(out, param != NULL ? param->text : NULL);
                wroteParam = 1;
            }
        }
        if ( returns != NULL && returns->childCount > 1 ) {
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( wroteParam ) {
                    xl6OutputPuts(", ", out);
                }
                xl6OutputPuts(xl6CgenHeaderTypeName(root, returns->children[i]), out);
                xl6OutputPrintf(out, "* out%u", (unsigned)i);
                wroteParam = 1;
            }
        }
    }
    xl6OutputPuts(");\n", out);
}

static void xl6CgenHeaderExportVarDecl(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenHeaderExternalVarTypeNode(node);
    xl6OutputPuts("extern ", out);
    if ( node != NULL && node->kind == XL6_AST_EXPORT_CONST ) {
        xl6OutputPuts("const ", out);
    }
    xl6OutputPuts(xl6CgenHeaderTypeName(root, typeNode), out);
    xl6OutputPutc(' ', out);
    xl6CgenHeaderCStringSymbol(out, node != NULL ? node->text : NULL);
    xl6OutputPuts(";\n", out);
}

int xl6CgenEmitHeader(XL6_Output* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags)
{
    size_t i;
    const char** emittedUnions;
    size_t emittedUnionCount;
    const char** emittedClasses;
    size_t emittedClassCount;

    (void)diags;
    if ( out == NULL || root == NULL ) {
        return 0;
    }
    if ( guardName == NULL || guardName[0] == '\0' ) {
        guardName = "XL6_GENERATED_HEADER_H";
    }

    xl6OutputPrintf(out, "#ifndef %s\n", guardName);
    xl6OutputPrintf(out, "#define %s\n\n", guardName);
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n\n", out);
    xl6OutputPuts("typedef intptr_t xl6_ssize_t;\n\n", out);
    xl6OutputPuts("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", out);
    xl6CgenHeaderLifecycleDecls(out);

    emittedUnions = NULL;
    emittedClasses = NULL;
    emittedUnionCount = 0;
    emittedClassCount = 0;
    if ( root->childCount > 0 ) {
        emittedUnions = (const char**)xrtCalloc(root->childCount, sizeof(char*));
        emittedClasses = (const char**)xrtCalloc(root->childCount, sizeof(char*));
    }
    if ( root->childCount > 0 && (emittedUnions == NULL || emittedClasses == NULL) ) {
        xrtFree(emittedUnions);
        xrtFree(emittedClasses);
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_UNION ) {
            xl6CgenHeaderUnionDecl(
                out,
                root,
                node,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount);
        } else if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenHeaderClassDecl(
                out,
                root,
                node,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount);
        }
    }
    xrtFree(emittedUnions);
    xrtFree(emittedClasses);

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

    xl6OutputPuts("\n#ifdef __cplusplus\n}\n#endif\n\n", out);
    xl6OutputPrintf(out, "#endif /* %s */\n", guardName);
    return 1;
}
