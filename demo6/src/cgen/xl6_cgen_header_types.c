#include "src/cgen/xl6_cgen.h"
#include "src/cgen/xl6_cgen_lifecycle.h"

static int xl6CgenHeaderTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static void xl6CgenHeaderFputsCIdent(XL6_Output* out, const char* text)
{
    char encoded[49];

    if ( out == NULL ) {
        return;
    }
    xl6CgenEncodeCIdentifier(encoded, sizeof(encoded), text);
    xl6OutputPuts(encoded, out);
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
    char encoded[49];

    xl6CgenEncodeSymbolComponent(encoded, sizeof(encoded), text);
    xl6CgenHeaderAppendText(buffer, capacity, encoded);
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

static void xl6CgenHeaderAppendTypeId(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( typeNode == NULL ) {
        xl6CgenHeaderAppendText(buffer, capacity, "_");
        return;
    }
    xl6CgenHeaderAppendEncodedText(buffer, capacity, typeNode->text);
    args = typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ?
        typeNode->children[0] :
        NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    xl6CgenHeaderAppendText(buffer, capacity, "_of");
    for ( i = 0; i < args->childCount; ++i ) {
        xl6CgenHeaderAppendText(buffer, capacity, "_");
        xl6CgenHeaderAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenHeaderEnumTypeRefName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenHeaderAppendText(buffer, capacity, "XL_Enum_");
    xl6CgenHeaderAppendEncodedText(
        buffer,
        capacity,
        typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL &&
           typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ?
        typeNode->children[0] :
        NULL;
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        xl6CgenHeaderAppendText(buffer, capacity, "__");
        xl6CgenHeaderAppendTypeId(buffer, capacity, args->children[i]);
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

static void xl6CgenHeaderExportFuncSymbol(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node)
{
    (void)root;
    if ( node != NULL && node->text != NULL && node->text[0] != '\0' ) {
        xl6CgenHeaderCStringSymbol(out, node->text);
        return;
    }
    xl6OutputPuts("_", out);
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

static const XL6_AstNode* xl6CgenHeaderFindEnum(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_ENUM &&
             xl6CgenHeaderTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenHeaderClassHasTypeParams(const XL6_AstNode* classNode)
{
    const XL6_AstNode* typeParams;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6CgenHeaderEnumHasTypeParams(const XL6_AstNode* enumNode)
{
    const XL6_AstNode* typeParams;

    typeParams = enumNode != NULL && enumNode->childCount > 2 ?
        enumNode->children[2] :
        NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static XL6_AstNode* xl6CgenHeaderCloneTypeTree(const XL6_AstNode* node)
{
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }
    copy = node->text != NULL ?
        xl6AstNewText(node->kind, node->text, strlen(node->text), NULL) :
        xl6AstNew(node->kind, NULL);
    if ( copy == NULL ) {
        return NULL;
    }
    copy->flags = node->flags;
    copy->semanticTypeId = node->semanticTypeId;
    copy->semanticTypeAbiId = node->semanticTypeAbiId;
    copy->line = node->line;
    copy->column = node->column;
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* child = xl6CgenHeaderCloneTypeTree(node->children[i]);

        if ( child == NULL && node->children[i] != NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, child);
    }
    return copy;
}

static XL6_AstNode* xl6CgenHeaderSubstituteType(
    const XL6_AstNode* typeParams,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* typeNode)
{
    XL6_AstNode* result;
    size_t i;

    if ( typeNode == NULL ) {
        return NULL;
    }
    if ( typeNode->kind == XL6_AST_TYPE &&
         typeNode->text != NULL &&
         typeParams != NULL &&
         typeArgs != NULL &&
         typeParams->childCount == typeArgs->childCount ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            const XL6_AstNode* param = typeParams->children[i];

            if ( param != NULL &&
                 xl6CgenHeaderTextEquals(param->text, typeNode->text) ) {
                return xl6CgenHeaderCloneTypeTree(typeArgs->children[i]);
            }
        }
    }

    result = typeNode->text != NULL ?
        xl6AstNewText(
            typeNode->kind,
            typeNode->text,
            strlen(typeNode->text),
            NULL) :
        xl6AstNew(typeNode->kind, NULL);
    if ( result == NULL ) {
        return NULL;
    }
    result->flags = typeNode->flags;
    result->semanticTypeId = typeNode->semanticTypeId;
    result->semanticTypeAbiId = typeNode->semanticTypeAbiId;
    for ( i = 0; i < typeNode->childCount; ++i ) {
        XL6_AstNode* child = xl6CgenHeaderSubstituteType(
            typeParams,
            typeArgs,
            typeNode->children[i]);

        if ( child == NULL && typeNode->children[i] != NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        xl6AstAddChild(result, child);
    }
    return result;
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

static const char* xl6CgenHeaderTypeName(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode,
    char* typeName,
    size_t typeNameCapacity)
{
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
    if ( xl6CgenHeaderTextEquals(typeNode->text, "var") ||
         xl6CgenHeaderTextEquals(typeNode->text, "__class_handle") ) {
        return "xvalue";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "array") ||
         xl6CgenHeaderTextEquals(typeNode->text, "list") ||
         xl6CgenHeaderTextEquals(typeNode->text, "set") ||
         xl6CgenHeaderTextEquals(typeNode->text, "dict") ) {
        const XL6_AstNode* args =
            typeNode->childCount > 0 &&
            typeNode->children[0] != NULL &&
            typeNode->children[0]->kind == XL6_AST_LIST ?
                typeNode->children[0] :
                NULL;
        const XL6_AstNode* itemType =
            args != NULL && args->childCount == 1 ?
                args->children[0] :
                NULL;

        if ( itemType == NULL ||
             xl6CgenHeaderTextEquals(itemType->text, "var") ) {
            return "xvalue";
        }
        if ( xl6CgenHeaderTextEquals(typeNode->text, "array") ) {
            return "xtarray";
        }
        if ( xl6CgenHeaderTextEquals(typeNode->text, "list") ) {
            return "xtlist";
        }
        if ( xl6CgenHeaderTextEquals(typeNode->text, "set") ) {
            return "xtset";
        }
        return "xtdict";
    }
    if ( xl6CgenHeaderTextEquals(typeNode->text, "function") ) {
        return "XL6_Function";
    }
    if ( xl6CgenHeaderFindUnion(root, typeNode->text) != NULL ) {
        xl6CgenHeaderUnionTypeTextName(typeName, typeNameCapacity, typeNode->text);
        return typeName;
    }
    if ( xl6CgenHeaderFindClass(root, typeNode->text) != NULL ) {
        xl6CgenHeaderClassTypeTextName(typeName, typeNameCapacity, typeNode->text);
        return typeName;
    }
    if ( xl6CgenHeaderFindEnum(root, typeNode->text) != NULL ) {
        xl6CgenHeaderEnumTypeRefName(typeName, typeNameCapacity, typeNode);
        return typeName;
    }
    return "void";
}

static void xl6CgenHeaderPutTypeName(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode)
{
    char typeName[1024];

    xl6OutputPuts(
        xl6CgenHeaderTypeName(root, typeNode, typeName, sizeof(typeName)),
        out);
}

static void xl6CgenHeaderParamDecl(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* param)
{
    const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;

    if ( typeNode != NULL && xl6CgenHeaderTextEquals(typeNode->text, "c_vararg") ) {
        xl6OutputPuts("...", out);
        return;
    }
    xl6CgenHeaderPutTypeName(out, root, typeNode);
    if ( param != NULL && (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
        xl6OutputPutc('*', out);
    }
    xl6OutputPutc(' ', out);
    xl6CgenHeaderFputsCIdent(out, param != NULL ? param->text : NULL);
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
            xl6CgenHeaderPutTypeName(out, root, field->children[0]);
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
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount);

static void xl6CgenHeaderClassDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount);

static void xl6CgenHeaderEnumDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const XL6_AstNode* enumType,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount);

static void xl6CgenHeaderTypeDependencyDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
{
    const XL6_AstNode* decl;

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL ) {
        return;
    }

    decl = typeNode->childCount == 0 ?
        xl6CgenHeaderFindClass(root, typeNode->text) :
        NULL;
    if ( decl != NULL ) {
        xl6CgenHeaderClassDecl(
            out,
            root,
            decl,
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount,
            emittedEnums,
            emittedEnumCount);
        return;
    }

    decl = typeNode->childCount == 0 ?
        xl6CgenHeaderFindUnion(root, typeNode->text) :
        NULL;
    if ( decl != NULL ) {
        xl6CgenHeaderUnionDecl(
            out,
            root,
            decl,
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount,
            emittedEnums,
            emittedEnumCount);
        return;
    }

    decl = xl6CgenHeaderFindEnum(root, typeNode->text);
    if ( decl != NULL ) {
        xl6CgenHeaderEnumDecl(
            out,
            root,
            decl,
            typeNode,
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount,
            emittedEnums,
            emittedEnumCount);
    }
}

static void xl6CgenHeaderInlineUnionDependencyDecls(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
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
            if ( field->kind != XL6_AST_FIELD || (field->flags & XL6_AST_FLAG_STATIC) != 0 || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount,
                emittedEnums,
                emittedEnumCount);
        }
    }
}

