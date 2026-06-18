#include "src/abi/xl6_abi_dump.h"

static void xl6AbiText(FILE* out, const char* text)
{
    fputs(text != NULL ? text : "", out);
}

static int xl6AbiTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static unsigned int xl6AbiNextUtf8Codepoint(const unsigned char** p)
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

static int xl6AbiIsSimpleCIdent(const char* text)
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

static void xl6AbiEncodedText(FILE* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        fputc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        unsigned int cp = xl6AbiNextUtf8Codepoint(&p);
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
}

static void xl6AbiCStringSymbol(FILE* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
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

static void xl6AbiJsonEscapedRange(FILE* out, const char* text, size_t len)
{
    size_t i;

    if ( text == NULL ) {
        return;
    }
    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '"' ) {
            fputs("\\\"", out);
        } else if ( ch == '\\' ) {
            fputs("\\\\", out);
        } else if ( ch == '\b' ) {
            fputs("\\b", out);
        } else if ( ch == '\f' ) {
            fputs("\\f", out);
        } else if ( ch == '\n' ) {
            fputs("\\n", out);
        } else if ( ch == '\r' ) {
            fputs("\\r", out);
        } else if ( ch == '\t' ) {
            fputs("\\t", out);
        } else if ( ch < 0x20 ) {
            fprintf(out, "\\u%04X", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
}

static void xl6AbiJsonEscaped(FILE* out, const char* text)
{
    if ( text == NULL ) {
        return;
    }
    xl6AbiJsonEscapedRange(out, text, strlen(text));
}

static void xl6AbiJsonString(FILE* out, const char* text)
{
    fputc('"', out);
    xl6AbiJsonEscaped(out, text);
    fputc('"', out);
}

static void xl6AbiJsonCStringSymbol(FILE* out, const char* text)
{
    size_t len;

    fputc('"', out);
    if ( text != NULL ) {
        len = strlen(text);
        if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
            xl6AbiJsonEscapedRange(out, text + 1, len - 2);
        } else {
            xl6AbiJsonEscapedRange(out, text, len);
        }
    }
    fputc('"', out);
}

static int xl6AbiIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6AbiFindTypeAlias(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( xl6AbiIsTypeAliasDecl(node) && node->text != NULL && strcmp(node->text, name) == 0 ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6AbiResolveAliasType(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6AbiFindTypeAlias(root, typeNode->text);
        if ( aliasNode == NULL || aliasNode->childCount == 0 ) {
            return typeNode;
        }
        typeNode = aliasNode->children[0];
    }
    return typeNode;
}

static void xl6AbiDumpType(FILE* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6AbiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        fputs("<missing-type>", out);
        return;
    }

    if ( xl6AbiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        fputs("function(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    fputc(',', out);
                }
                xl6AbiDumpType(out, root, args->children[i]);
            }
        }
        fputc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            fputs("->", out);
            if ( returns->childCount > 1 ) {
                fputc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    fputc(',', out);
                }
                xl6AbiDumpType(out, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                fputc(')', out);
            }
        }
        return;
    }

    xl6AbiText(out, typeNode->text);
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST ) {
        return;
    }

    fputc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            fputc(',', out);
        }
        xl6AbiDumpType(out, root, args->children[i]);
    }
    fputc('>', out);
}

static void xl6AbiDumpTypeJsonBody(FILE* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6AbiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        fputs("<missing-type>", out);
        return;
    }

    if ( xl6AbiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        fputs("function(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    fputc(',', out);
                }
                xl6AbiDumpTypeJsonBody(out, root, args->children[i]);
            }
        }
        fputc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            fputs("->", out);
            if ( returns->childCount > 1 ) {
                fputc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    fputc(',', out);
                }
                xl6AbiDumpTypeJsonBody(out, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                fputc(')', out);
            }
        }
        return;
    }

    xl6AbiJsonEscaped(out, typeNode->text);
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST ) {
        return;
    }

    fputc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            fputc(',', out);
        }
        xl6AbiDumpTypeJsonBody(out, root, args->children[i]);
    }
    fputc('>', out);
}

static void xl6AbiDumpTypeJsonString(FILE* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    fputc('"', out);
    xl6AbiDumpTypeJsonBody(out, root, typeNode);
    fputc('"', out);
}

static void xl6AbiDumpSignature(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    fprintf(out, "fn:app.%s(", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                fputc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpType(out, root, params->children[i]->children[0]);
            } else {
                fputs("<missing-type>", out);
            }
        }
    }

    fputs(")->", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        fputs("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpType(out, root, returns->children[0]);
    } else {
        fputc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                fputc(',', out);
            }
            xl6AbiDumpType(out, root, returns->children[i]);
        }
        fputc(')', out);
    }
}

static void xl6AbiDumpSignatureJsonString(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    fputc('"', out);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    fputs("fn:app.", out);
    xl6AbiJsonEscaped(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    fputc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                fputc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpTypeJsonBody(out, root, params->children[i]->children[0]);
            } else {
                fputs("<missing-type>", out);
            }
        }
    }

    fputs(")->", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        fputs("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpTypeJsonBody(out, root, returns->children[0]);
    } else {
        fputc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                fputc(',', out);
            }
            xl6AbiDumpTypeJsonBody(out, root, returns->children[i]);
        }
        fputc(')', out);
    }
    fputc('"', out);
}

static int xl6AbiFuncOverloadCount(const XL6_AstNode* root, const char* name)
{
    size_t i;
    int count;

    count = 0;
    if ( root == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* nameNode;
        if ( node == NULL || (node->kind != XL6_AST_FUNC && node->kind != XL6_AST_EXPORT_FUNC) ||
             node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6AbiTextEquals(nameNode->text, name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6AbiDumpInternalSymbol(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        fputs("xl6_export_", out);
        xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
        return;
    }

    if ( nameNode != NULL && xl6AbiFuncOverloadCount(root, nameNode->text) <= 1 &&
         xl6AbiIsSimpleCIdent(nameNode->text) ) {
        fputs(nameNode->text, out);
        return;
    }

    fputs("xl_fn_", out);
    xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            fputc('_', out);
        }
    }
}

static void xl6AbiDumpInternalSymbolJsonString(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    fputc('"', out);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        fputs("xl6_export_", out);
        xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
        fputc('"', out);
        return;
    }

    if ( nameNode != NULL && xl6AbiFuncOverloadCount(root, nameNode->text) <= 1 &&
         xl6AbiIsSimpleCIdent(nameNode->text) ) {
        xl6AbiJsonEscaped(out, nameNode->text);
        fputc('"', out);
        return;
    }

    fputs("xl_fn_", out);
    xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        fputs("__void", out);
        fputc('"', out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        fputs("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            fputc('_', out);
        }
    }
    fputc('"', out);
}

static void xl6AbiDumpFuncLike(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;

    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        fprintf(out, "extern ");
        xl6AbiCStringSymbol(out, node->text);
        fprintf(out, "\n  name: %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpSignature(out, root, node);
        fputs("\n  c_symbol: ", out);
        xl6AbiCStringSymbol(out, node->text);
        fputc('\n', out);
        return;
    }

    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        fputs("export ", out);
        xl6AbiCStringSymbol(out, node->text);
        fprintf(out, "\n  name: %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpSignature(out, root, node);
        fputs("\n  symbol: ", out);
        xl6AbiDumpInternalSymbol(out, root, node);
        fputs("\n  c_export: ", out);
        xl6AbiCStringSymbol(out, node->text);
        fputc('\n', out);
        return;
    }

    fprintf(out, "func %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6AbiDumpSignature(out, root, node);
    fputs("\n  symbol: ", out);
    xl6AbiDumpInternalSymbol(out, root, node);
    fputc('\n', out);
}

static void xl6AbiDumpVarLike(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;

    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        fputs("extern ", out);
        xl6AbiCStringSymbol(out, node->text);
        fprintf(out, "\n  name: %s\n  kind: var\n  type: ",
                (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpType(out, root, typeNode);
        fputs("\n  c_symbol: ", out);
        xl6AbiCStringSymbol(out, node->text);
        fputc('\n', out);
        return;
    }

    fputs("export ", out);
    xl6AbiCStringSymbol(out, node->text);
    fprintf(out, "\n  name: %s\n  kind: %s\n  type: ",
            (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>",
            node->kind == XL6_AST_EXPORT_CONST ? "const" : "var");
    xl6AbiDumpType(out, root, typeNode);
    fputs("\n  c_export: ", out);
    xl6AbiCStringSymbol(out, node->text);
    fputc('\n', out);
}

static void xl6AbiDumpFuncLikeJson(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( comma ) {
        fputs(",\n", out);
    }
    fputs("    {\"kind\":", out);
    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        xl6AbiJsonString(out, "extern_func");
    } else if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6AbiJsonString(out, "export_func");
    } else {
        xl6AbiJsonString(out, "func");
    }
    fputs(",\"name\":", out);
    xl6AbiJsonString(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    fputs(",\"signature\":", out);
    xl6AbiDumpSignatureJsonString(out, root, node);

    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        fputs(",\"cSymbol\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    } else {
        fputs(",\"symbol\":", out);
        xl6AbiDumpInternalSymbolJsonString(out, root, node);
        if ( node->kind == XL6_AST_EXPORT_FUNC ) {
            fputs(",\"cExport\":", out);
            xl6AbiJsonCStringSymbol(out, node->text);
        }
    }
    fputc('}', out);
}

static void xl6AbiDumpVarLikeJson(FILE* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;
    if ( comma ) {
        fputs(",\n", out);
    }

    fputs("    {\"kind\":", out);
    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        xl6AbiJsonString(out, "extern_var");
    } else if ( node->kind == XL6_AST_EXPORT_CONST ) {
        xl6AbiJsonString(out, "export_const");
    } else {
        xl6AbiJsonString(out, "export_var");
    }
    fputs(",\"name\":", out);
    xl6AbiJsonString(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    fputs(",\"type\":", out);
    xl6AbiDumpTypeJsonString(out, root, typeNode);
    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        fputs(",\"cSymbol\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    } else {
        fputs(",\"cExport\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    }
    fputc('}', out);
}

void xl6AbiDumpText(FILE* out, const XL6_AstNode* root)
{
    size_t i;

    if ( out == NULL ) {
        return;
    }

    fputs("xlang abi version: 6\n", out);
    fputs("module: app\n", out);

    if ( root == NULL ) {
        return;
    }

    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* child = root->children[i];
        if ( child->kind == XL6_AST_FUNC || child->kind == XL6_AST_EXTERN_FUNC || child->kind == XL6_AST_EXPORT_FUNC ) {
            xl6AbiDumpFuncLike(out, root, child);
        } else if ( child->kind == XL6_AST_EXTERN_VAR ||
                    child->kind == XL6_AST_EXPORT_VAR ||
                    child->kind == XL6_AST_EXPORT_CONST ) {
            xl6AbiDumpVarLike(out, root, child);
        }
    }
}

void xl6AbiDumpJson(FILE* out, const XL6_AstNode* root)
{
    size_t i;
    int emitted;

    if ( out == NULL ) {
        return;
    }

    fputs("{\n", out);
    fputs("  \"abiVersion\":6,\n", out);
    fputs("  \"module\":\"app\",\n", out);
    fputs("  \"symbols\":[\n", out);

    emitted = 0;
    if ( root != NULL ) {
        for ( i = 0; i < root->childCount; ++i ) {
            const XL6_AstNode* child = root->children[i];
            if ( child->kind == XL6_AST_FUNC || child->kind == XL6_AST_EXTERN_FUNC || child->kind == XL6_AST_EXPORT_FUNC ) {
                xl6AbiDumpFuncLikeJson(out, root, child, emitted);
                emitted = 1;
            } else if ( child->kind == XL6_AST_EXTERN_VAR ||
                        child->kind == XL6_AST_EXPORT_VAR ||
                        child->kind == XL6_AST_EXPORT_CONST ) {
                xl6AbiDumpVarLikeJson(out, root, child, emitted);
                emitted = 1;
            }
        }
    }

    if ( emitted ) {
        fputc('\n', out);
    }
    fputs("  ]\n", out);
    fputs("}\n", out);
}
