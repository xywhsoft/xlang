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
    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
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
    return "void";
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
    xl6OutputPuts("#include <stdint.h>\n\n", out);
    xl6OutputPuts("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", out);
    xl6CgenHeaderLifecycleDecls(out);

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
