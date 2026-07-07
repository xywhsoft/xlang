#include "src/abi/xl6_abi_dump.h"
#include "src/abi/xl6_abi_json.h"
#include "src/common/xl6_output.h"

static void xl6AbiText(XL6_Output* out, const char* text)
{
    xl6OutputPuts(text != NULL ? text : "", out);
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

static void xl6AbiEncodedText(XL6_Output* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        xl6OutputPutc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        unsigned int cp = xl6AbiNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
            xl6OutputPutc((int)cp, out);
        } else if ( cp == '_' ) {
            xl6OutputPuts("__", out);
        } else if ( cp <= 0xFFFFU ) {
            xl6OutputPrintf(out, "_u%04X_", cp);
        } else {
            xl6OutputPrintf(out, "_u%06X_", cp);
        }
    }
}

static void xl6AbiCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
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

static void xl6AbiDumpType(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6AbiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        xl6OutputPuts("<missing-type>", out);
        return;
    }

    if ( xl6AbiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6OutputPuts("func(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6AbiDumpType(out, root, args->children[i]);
            }
        }
        xl6OutputPutc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6OutputPuts(" as ", out);
            if ( returns->childCount > 1 ) {
                xl6OutputPutc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6AbiDumpType(out, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6OutputPutc(')', out);
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

    xl6OutputPutc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPutc(',', out);
        }
        xl6AbiDumpType(out, root, args->children[i]);
    }
    xl6OutputPutc('>', out);
}

static void xl6AbiDumpTypeJsonBody(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6AbiResolveAliasType(root, typeNode);
    if ( typeNode == NULL ) {
        xl6OutputPuts("<missing-type>", out);
        return;
    }

    if ( xl6AbiTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6OutputPuts("func(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6AbiDumpTypeJsonBody(out, root, args->children[i]);
            }
        }
        xl6OutputPutc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6OutputPuts(" as ", out);
            if ( returns->childCount > 1 ) {
                xl6OutputPutc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPutc(',', out);
                }
                xl6AbiDumpTypeJsonBody(out, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6OutputPutc(')', out);
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

    xl6OutputPutc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPutc(',', out);
        }
        xl6AbiDumpTypeJsonBody(out, root, args->children[i]);
    }
    xl6OutputPutc('>', out);
}

static void xl6AbiDumpTypeJsonString(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    xl6OutputPutc('"', out);
    xl6AbiDumpTypeJsonBody(out, root, typeNode);
    xl6OutputPutc('"', out);
}

static void xl6AbiDumpSignature(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPrintf(out, "fn:app.%s(", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpType(out, root, params->children[i]->children[0]);
            } else {
                xl6OutputPuts("<missing-type>", out);
            }
        }
    }

    xl6OutputPuts(") as ", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        xl6OutputPuts("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpType(out, root, returns->children[0]);
    } else {
        xl6OutputPutc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6AbiDumpType(out, root, returns->children[i]);
        }
        xl6OutputPutc(')', out);
    }
}

static void xl6AbiDumpSignatureJsonString(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    xl6OutputPutc('"', out);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPuts("fn:app.", out);
    xl6AbiJsonEscaped(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpTypeJsonBody(out, root, params->children[i]->children[0]);
            } else {
                xl6OutputPuts("<missing-type>", out);
            }
        }
    }

    xl6OutputPuts(") as ", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        xl6OutputPuts("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpTypeJsonBody(out, root, returns->children[0]);
    } else {
        xl6OutputPutc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6AbiDumpTypeJsonBody(out, root, returns->children[i]);
        }
        xl6OutputPutc(')', out);
    }
    xl6OutputPutc('"', out);
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

static int xl6AbiExportFuncOverloadCount(const XL6_AstNode* root, const char* name)
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

        if ( node == NULL || node->kind != XL6_AST_EXPORT_FUNC || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6AbiTextEquals(nameNode->text, name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6AbiDumpParamSignatureSuffix(XL6_Output* out, const XL6_AstNode* node)
{
    const XL6_AstNode* params;
    size_t i;

    params = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            xl6OutputPutc('_', out);
        }
    }
}

static void xl6AbiDumpInternalSymbol(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6OutputPuts("xl6_export_", out);
        xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
        if ( nameNode != NULL && xl6AbiExportFuncOverloadCount(root, nameNode->text) > 1 ) {
            xl6AbiDumpParamSignatureSuffix(out, node);
        }
        return;
    }

    if ( nameNode != NULL && xl6AbiFuncOverloadCount(root, nameNode->text) <= 1 &&
         xl6AbiIsSimpleCIdent(nameNode->text) ) {
        xl6OutputPuts(nameNode->text, out);
        return;
    }

    xl6OutputPuts("xl_fn_", out);
    xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            xl6OutputPutc('_', out);
        }
    }
}

static void xl6AbiDumpInternalSymbolJsonString(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    xl6OutputPutc('"', out);
    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6OutputPuts("xl6_export_", out);
        xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
        if ( nameNode != NULL && xl6AbiExportFuncOverloadCount(root, nameNode->text) > 1 ) {
            xl6AbiDumpParamSignatureSuffix(out, node);
        }
        xl6OutputPutc('"', out);
        return;
    }

    if ( nameNode != NULL && xl6AbiFuncOverloadCount(root, nameNode->text) <= 1 &&
         xl6AbiIsSimpleCIdent(nameNode->text) ) {
        xl6AbiJsonEscaped(out, nameNode->text);
        xl6OutputPutc('"', out);
        return;
    }

    xl6OutputPuts("xl_fn_", out);
    xl6AbiEncodedText(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", out);
        xl6OutputPutc('"', out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            xl6OutputPutc('_', out);
        }
    }
    xl6OutputPutc('"', out);
}

static const char* xl6AbiCallableName(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_FUNC ||
         node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_EXPORT_FUNC ||
         node->kind == XL6_AST_GET ||
         node->kind == XL6_AST_LET ||
         node->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
         node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
        nameNode = node->childCount > 0 ? node->children[0] : NULL;
        return nameNode != NULL ? nameNode->text : NULL;
    }
    return node->text;
}

static int xl6AbiIsClassCallableOverloadMember(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_FUNC ||
            node->kind == XL6_AST_ASYNC_FUNC ||
            node->kind == XL6_AST_EXPORT_FUNC ||
            node->kind == XL6_AST_CREATE ||
            node->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
            node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL);
}

static int xl6AbiClassCallableOverloadCount(const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* body;
    const char* name;
    size_t i;
    int count;

    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    name = xl6AbiCallableName(memberNode);
    count = 0;
    if ( body == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* other = body->children[i];
        if ( xl6AbiIsClassCallableOverloadMember(memberNode) &&
             xl6AbiIsClassCallableOverloadMember(other) &&
             xl6AbiTextEquals(xl6AbiCallableName(other), name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6AbiDumpClassMemberSymbol(XL6_Output* out, const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6AbiCallableName(memberNode);
    xl6OutputPuts("xl_class_", out);
    xl6AbiEncodedText(out, classNode != NULL && classNode->text != NULL ? classNode->text : "_");
    xl6OutputPuts("__", out);
    xl6AbiEncodedText(out, name != NULL ? name : "_");

    if ( memberNode == NULL || xl6AbiClassCallableOverloadCount(classNode, memberNode) <= 1 ) {
        return;
    }
    params = memberNode->childCount > 1 ? memberNode->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6AbiEncodedText(out, param->children[0]->text);
        } else {
            xl6OutputPutc('_', out);
        }
    }
}

static void xl6AbiDumpClassMethodSignature(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    xl6OutputPuts("fn:app.", out);
    xl6AbiText(out, classNode != NULL && classNode->text != NULL ? classNode->text : "<anonymous-class>");
    xl6OutputPutc('.', out);
    xl6AbiText(out, xl6AbiCallableName(node) != NULL ? xl6AbiCallableName(node) : "<unnamed>");
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpType(out, root, params->children[i]->children[0]);
            } else {
                xl6OutputPuts("<missing-type>", out);
            }
        }
    }
    xl6OutputPuts(") as ", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        xl6OutputPuts("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpType(out, root, returns->children[0]);
    } else {
        xl6OutputPutc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6AbiDumpType(out, root, returns->children[i]);
        }
        xl6OutputPutc(')', out);
    }
}

static void xl6AbiDumpClassMethodSignatureJsonString(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    xl6OutputPutc('"', out);
    xl6OutputPuts("fn:app.", out);
    xl6AbiJsonEscaped(out, classNode != NULL && classNode->text != NULL ? classNode->text : "<anonymous-class>");
    xl6OutputPutc('.', out);
    xl6AbiJsonEscaped(out, xl6AbiCallableName(node) != NULL ? xl6AbiCallableName(node) : "<unnamed>");
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            if ( params->children[i]->childCount > 0 ) {
                xl6AbiDumpTypeJsonBody(out, root, params->children[i]->children[0]);
            } else {
                xl6OutputPuts("<missing-type>", out);
            }
        }
    }
    xl6OutputPuts(") as ", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns == NULL || returns->childCount == 0 ) {
        xl6OutputPuts("()", out);
    } else if ( returns->childCount == 1 ) {
        xl6AbiDumpTypeJsonBody(out, root, returns->children[0]);
    } else {
        xl6OutputPutc('(', out);
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPutc(',', out);
            }
            xl6AbiDumpTypeJsonBody(out, root, returns->children[i]);
        }
        xl6OutputPutc(')', out);
    }
    xl6OutputPutc('"', out);
}

static void xl6AbiDumpClassExportFunc(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode, const XL6_AstNode* node)
{
    xl6OutputPuts("export ", out);
    xl6AbiCStringSymbol(out, node->text);
    xl6OutputPuts("\n  owner: ", out);
    xl6AbiText(out, classNode != NULL && classNode->text != NULL ? classNode->text : "<anonymous-class>");
    xl6OutputPuts("\n  name: ", out);
    xl6AbiText(out, xl6AbiCallableName(node) != NULL ? xl6AbiCallableName(node) : "<unnamed>");
    xl6OutputPuts("\n  signature: ", out);
    xl6AbiDumpClassMethodSignature(out, root, classNode, node);
    xl6OutputPuts("\n  symbol: ", out);
    xl6AbiDumpClassMemberSymbol(out, classNode, node);
    xl6OutputPuts("\n  c_export: ", out);
    xl6AbiCStringSymbol(out, node->text);
    xl6OutputPutc('\n', out);
}

static void xl6AbiDumpClassExportFuncJson(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode, const XL6_AstNode* node, int comma)
{
    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    xl6OutputPuts("    {\"kind\":\"export_class_method\",\"owner\":", out);
    xl6AbiJsonString(out, classNode != NULL && classNode->text != NULL ? classNode->text : "");
    xl6OutputPuts(",\"name\":", out);
    xl6AbiJsonString(out, xl6AbiCallableName(node) != NULL ? xl6AbiCallableName(node) : "<unnamed>");
    xl6OutputPuts(",\"signature\":", out);
    xl6AbiDumpClassMethodSignatureJsonString(out, root, classNode, node);
    xl6OutputPuts(",\"symbol\":\"", out);
    xl6AbiDumpClassMemberSymbol(out, classNode, node);
    xl6OutputPuts("\",\"cExport\":", out);
    xl6AbiJsonCStringSymbol(out, node->text);
    xl6OutputPutc('}', out);
}

static void xl6AbiDumpFuncLike(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;

    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        xl6OutputPrintf(out, "extern ");
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPrintf(out, "\n  name: %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpSignature(out, root, node);
        xl6OutputPuts("\n  c_symbol: ", out);
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPutc('\n', out);
        return;
    }

    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6OutputPuts("export ", out);
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPrintf(out, "\n  name: %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpSignature(out, root, node);
        xl6OutputPuts("\n  symbol: ", out);
        xl6AbiDumpInternalSymbol(out, root, node);
        xl6OutputPuts("\n  c_export: ", out);
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPutc('\n', out);
        return;
    }

    xl6OutputPrintf(out, "func %s\n  signature: ", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6AbiDumpSignature(out, root, node);
    xl6OutputPuts("\n  symbol: ", out);
    xl6AbiDumpInternalSymbol(out, root, node);
    xl6OutputPutc('\n', out);
}

static void xl6AbiDumpVarLike(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;

    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        xl6OutputPuts("extern ", out);
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPrintf(out, "\n  name: %s\n  kind: var\n  type: ",
                (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
        xl6AbiDumpType(out, root, typeNode);
        xl6OutputPuts("\n  c_symbol: ", out);
        xl6AbiCStringSymbol(out, node->text);
        xl6OutputPutc('\n', out);
        return;
    }

    xl6OutputPuts("export ", out);
    xl6AbiCStringSymbol(out, node->text);
    xl6OutputPrintf(out, "\n  name: %s\n  kind: %s\n  type: ",
            (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>",
            node->kind == XL6_AST_EXPORT_CONST ? "const" : "var");
    xl6AbiDumpType(out, root, typeNode);
    xl6OutputPuts("\n  c_export: ", out);
    xl6AbiCStringSymbol(out, node->text);
    xl6OutputPutc('\n', out);
}

static void xl6AbiDumpFuncLikeJson(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }
    xl6OutputPuts("    {\"kind\":", out);
    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        xl6AbiJsonString(out, "extern_func");
    } else if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6AbiJsonString(out, "export_func");
    } else {
        xl6AbiJsonString(out, "func");
    }
    xl6OutputPuts(",\"name\":", out);
    xl6AbiJsonString(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6OutputPuts(",\"signature\":", out);
    xl6AbiDumpSignatureJsonString(out, root, node);

    if ( node->kind == XL6_AST_EXTERN_FUNC ) {
        xl6OutputPuts(",\"cSymbol\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    } else {
        xl6OutputPuts(",\"symbol\":", out);
        xl6AbiDumpInternalSymbolJsonString(out, root, node);
        if ( node->kind == XL6_AST_EXPORT_FUNC ) {
            xl6OutputPuts(",\"cExport\":", out);
            xl6AbiJsonCStringSymbol(out, node->text);
        }
    }
    xl6OutputPutc('}', out);
}

static void xl6AbiDumpVarLikeJson(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node, int comma)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;
    if ( comma ) {
        xl6OutputPuts(",\n", out);
    }

    xl6OutputPuts("    {\"kind\":", out);
    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        xl6AbiJsonString(out, "extern_var");
    } else if ( node->kind == XL6_AST_EXPORT_CONST ) {
        xl6AbiJsonString(out, "export_const");
    } else {
        xl6AbiJsonString(out, "export_var");
    }
    xl6OutputPuts(",\"name\":", out);
    xl6AbiJsonString(out, (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6OutputPuts(",\"type\":", out);
    xl6AbiDumpTypeJsonString(out, root, typeNode);
    if ( node->kind == XL6_AST_EXTERN_VAR ) {
        xl6OutputPuts(",\"cSymbol\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    } else {
        xl6OutputPuts(",\"cExport\":", out);
        xl6AbiJsonCStringSymbol(out, node->text);
    }
    xl6OutputPutc('}', out);
}

void xl6AbiDumpText(XL6_Output* out, const XL6_AstNode* root)
{
    size_t i;
    size_t j;

    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("xlang abi version: 6\n", out);
    xl6OutputPuts("module: app\n", out);

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
        } else if ( child->kind == XL6_AST_CLASS && child->childCount > 1 ) {
            const XL6_AstNode* body = child->children[1];
            for ( j = 0; body != NULL && j < body->childCount; ++j ) {
                const XL6_AstNode* member = body->children[j];
                if ( member != NULL && member->kind == XL6_AST_EXPORT_FUNC ) {
                    xl6AbiDumpClassExportFunc(out, root, child, member);
                }
            }
        }
    }
}

void xl6AbiDumpJson(XL6_Output* out, const XL6_AstNode* root)
{
    size_t i;
    size_t j;
    int emitted;

    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("{\n", out);
    xl6OutputPuts("  \"abiVersion\":6,\n", out);
    xl6OutputPuts("  \"module\":\"app\",\n", out);
    xl6OutputPuts("  \"symbols\":[\n", out);

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
            } else if ( child->kind == XL6_AST_CLASS && child->childCount > 1 ) {
                const XL6_AstNode* body = child->children[1];
                for ( j = 0; body != NULL && j < body->childCount; ++j ) {
                    const XL6_AstNode* member = body->children[j];
                    if ( member != NULL && member->kind == XL6_AST_EXPORT_FUNC ) {
                        xl6AbiDumpClassExportFuncJson(out, root, child, member, emitted);
                        emitted = 1;
                    }
                }
            }
        }
    }

    if ( emitted ) {
        xl6OutputPutc('\n', out);
    }
    xl6OutputPuts("  ]\n", out);
    xl6OutputPuts("}\n", out);
}
