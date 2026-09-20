#include "src/cgen/xl6_cgen.h"
#include "src/cgen/xl6_cgen_lifecycle.h"
#include "src/type/xl6_type_model.h"

static int xl6CgenHeaderCAbiCallbackCanBridge(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode);
static unsigned long long xl6CgenHeaderFunctionSignatureId(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode);
static void xl6CgenHeaderPutTypeName(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode);

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
    if ( xl6CgenHeaderTextEquals(typeNode->text, "time") ) {
        return "int64_t";
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
        return "xvalue*";
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
            return "xvalue*";
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
        /*
            func 在 xlang 数据布局中始终是带签名和生命周期的 XL6_Function。
            只有明确位于 C ABI 参数位置时，才由 ParamDecl 改写为裸函数指针。
        */
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

static const XL6_AstNode* xl6CgenHeaderCallableItemType(const XL6_AstNode* item)
{
    if ( item != NULL && item->kind == XL6_AST_PARAM ) {
        return item->childCount > 0 ? item->children[0] : NULL;
    }
    return item;
}

static int xl6CgenHeaderCAbiCallbackValueType(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode,
    int isReturn)
{
    const char* name;

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount != 0 ) {
        return 0;
    }
    name = typeNode->text;
    if ( !isReturn && xl6CgenHeaderTextEquals(name, "string") ) {
        return 1;
    }
    return xl6CgenHeaderTextEquals(name, "bool") ||
           xl6CgenHeaderTextEquals(name, "int") ||
           xl6CgenHeaderTextEquals(name, "uint") ||
           xl6CgenHeaderTextEquals(name, "int8") ||
           xl6CgenHeaderTextEquals(name, "uint8") ||
           xl6CgenHeaderTextEquals(name, "int16") ||
           xl6CgenHeaderTextEquals(name, "uint16") ||
           xl6CgenHeaderTextEquals(name, "int32") ||
           xl6CgenHeaderTextEquals(name, "uint32") ||
           xl6CgenHeaderTextEquals(name, "int64") ||
           xl6CgenHeaderTextEquals(name, "uint64") ||
           xl6CgenHeaderTextEquals(name, "float") ||
           xl6CgenHeaderTextEquals(name, "float32") ||
           xl6CgenHeaderTextEquals(name, "float64") ||
           xl6CgenHeaderTextEquals(name, "point") ||
           xl6CgenHeaderTextEquals(name, "c_char") ||
           xl6CgenHeaderTextEquals(name, "c_schar") ||
           xl6CgenHeaderTextEquals(name, "c_uchar") ||
           xl6CgenHeaderTextEquals(name, "c_short") ||
           xl6CgenHeaderTextEquals(name, "c_ushort") ||
           xl6CgenHeaderTextEquals(name, "c_int") ||
           xl6CgenHeaderTextEquals(name, "c_uint") ||
           xl6CgenHeaderTextEquals(name, "c_long") ||
           xl6CgenHeaderTextEquals(name, "c_ulong") ||
           xl6CgenHeaderTextEquals(name, "c_longlong") ||
           xl6CgenHeaderTextEquals(name, "c_ulonglong") ||
           xl6CgenHeaderTextEquals(name, "c_size") ||
           xl6CgenHeaderTextEquals(name, "c_ssize") ||
           xl6CgenHeaderTextEquals(name, "c_ptrdiff") ||
           xl6CgenHeaderTextEquals(name, "c_intptr") ||
           xl6CgenHeaderTextEquals(name, "c_uintptr") ||
           xl6CgenHeaderTextEquals(name, "c_bool") ||
           xl6CgenHeaderTextEquals(name, "c_float") ||
           xl6CgenHeaderTextEquals(name, "c_double") ||
           xl6CgenHeaderTextEquals(name, "c_wchar");
}

static int xl6CgenHeaderCAbiCallbackCanBridge(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    if ( typeNode == NULL || !xl6CgenHeaderTextEquals(typeNode->text, "function") ||
         typeNode->childCount != 2 ) {
        return 0;
    }
    params = typeNode->children[0];
    returns = typeNode->children[1];
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* item = params->children[i];
        if ( item == NULL ||
             (item->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ||
             (item->kind == XL6_AST_PARAM && item->childCount > 1) ||
             !xl6CgenHeaderCAbiCallbackValueType(
                 root,
                 xl6CgenHeaderCallableItemType(item),
                 0) ) {
            return 0;
        }
    }
    return returns == NULL || returns->childCount == 0 ||
           xl6CgenHeaderCAbiCallbackValueType(root, returns->children[0], 1);
}

static void xl6CgenHeaderHashText(unsigned long long* hash, const char* text)
{
    const unsigned char* p;

    if ( hash == NULL || text == NULL ) {
        return;
    }
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        *hash ^= (unsigned long long)*p;
        *hash *= 1099511628211ULL;
    }
}

static void xl6CgenHeaderHashCallbackType(
    const XL6_AstNode* root,
    unsigned long long* hash,
    const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    xl6CgenHeaderHashText(
        hash,
        typeNode != NULL && typeNode->text != NULL ?
            xl6TypeCanonicalBuiltinName(typeNode->text) : "_");
}

static void xl6CgenHeaderHashCallbackList(
    const XL6_AstNode* root,
    unsigned long long* hash,
    const XL6_AstNode* list)
{
    size_t i;

    for ( i = 0; list != NULL && i < list->childCount; ++i ) {
        const XL6_AstNode* item = list->children[i];
        if ( i > 0 ) {
            xl6CgenHeaderHashText(hash, ",");
        }
        if ( item != NULL && (item->flags & XL6_AST_FLAG_BYVAL) != 0 ) {
            xl6CgenHeaderHashText(hash, "byval:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_BYREF) != 0 ) {
            xl6CgenHeaderHashText(hash, "byref:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_OUT) != 0 ) {
            xl6CgenHeaderHashText(hash, "out:");
        }
        xl6CgenHeaderHashCallbackType(
            root,
            hash,
            xl6CgenHeaderCallableItemType(item));
    }
}

static unsigned long long xl6CgenHeaderFunctionSignatureId(
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode)
{
    unsigned long long hash = 1469598103934665603ULL;

    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    xl6CgenHeaderHashText(&hash, "func(");
    xl6CgenHeaderHashCallbackList(
        root,
        &hash,
        typeNode != NULL && typeNode->childCount > 0 ? typeNode->children[0] : NULL);
    xl6CgenHeaderHashText(&hash, ") as (");
    xl6CgenHeaderHashCallbackList(
        root,
        &hash,
        typeNode != NULL && typeNode->childCount > 1 ? typeNode->children[1] : NULL);
    xl6CgenHeaderHashText(&hash, ")");
    return hash;
}

static void xl6CgenHeaderEmitCallbackTypedef(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    unsigned long long typeId;
    size_t i;

    if ( !xl6CgenHeaderCAbiCallbackCanBridge(root, typeNode) ) {
        return;
    }
    typeNode = xl6CgenHeaderResolveAliasType(root, typeNode);
    params = typeNode->children[0];
    returns = typeNode->children[1];
    typeId = xl6CgenHeaderFunctionSignatureId(root, typeNode);
    xl6OutputPrintf(out, "#ifndef XL6_C_CALLBACK_%016llX\n", typeId);
    xl6OutputPrintf(out, "#define XL6_C_CALLBACK_%016llX\n", typeId);
    xl6OutputPuts("typedef ", out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6CgenHeaderPutTypeName(out, root, returns->children[0]);
    } else {
        xl6OutputPuts("void", out);
    }
    xl6OutputPrintf(out, " (*XL6_C_Callback_%016llX)(", typeId);
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6CgenHeaderPutTypeName(
                out,
                root,
                xl6CgenHeaderCallableItemType(params->children[i]));
        }
    }
    xl6OutputPuts(");\n#endif\n", out);
}

typedef struct XL6_CgenHeaderCallbackSet {
    unsigned long long* values;
    size_t count;
    size_t capacity;
} XL6_CgenHeaderCallbackSet;

static int xl6CgenHeaderRememberCallback(
    XL6_CgenHeaderCallbackSet* set,
    unsigned long long typeId)
{
    unsigned long long* values;
    size_t capacity;
    size_t i;

    if ( set == NULL ) {
        return -1;
    }
    for ( i = 0; i < set->count; ++i ) {
        if ( set->values[i] == typeId ) {
            return 0;
        }
    }
    if ( set->count == set->capacity ) {
        if ( set->capacity > SIZE_MAX / 2u ||
             (set->capacity != 0 &&
              set->capacity > SIZE_MAX / sizeof(unsigned long long)) ) {
            return -1;
        }
        capacity = set->capacity == 0 ? 8 : set->capacity * 2;
        if ( capacity > SIZE_MAX / sizeof(unsigned long long) ) {
            return -1;
        }
        values = (unsigned long long*)xrtRealloc(
            set->values,
            capacity * sizeof(unsigned long long));
        if ( values == NULL ) {
            return -1;
        }
        set->values = values;
        set->capacity = capacity;
    }
    set->values[set->count] = typeId;
    set->count += 1;
    return 1;
}

static int xl6CgenHeaderEmitCallbackTypedefsRec(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    XL6_CgenHeaderCallbackSet* emitted)
{
    size_t i;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE &&
         xl6CgenHeaderTextEquals(node->text, "function") ) {
        unsigned long long typeId = xl6CgenHeaderFunctionSignatureId(root, node);
        int remember = xl6CgenHeaderRememberCallback(emitted, typeId);

        if ( remember < 0 ) {
            return 0;
        }
        if ( remember > 0 ) {
            xl6CgenHeaderEmitCallbackTypedef(out, root, node);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6CgenHeaderEmitCallbackTypedefsRec(
                 out,
                 root,
                 node->children[i],
                 emitted) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenHeaderEmitCallbackTypedefs(
    XL6_Output* out,
    const XL6_AstNode* root)
{
    XL6_CgenHeaderCallbackSet emitted;
    int ok;

    memset(&emitted, 0, sizeof(emitted));
    ok = xl6CgenHeaderEmitCallbackTypedefsRec(out, root, root, &emitted);
    xrtFree(emitted.values);
    return ok;
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
    char callbackTypeName[64];

    if ( typeNode != NULL && xl6CgenHeaderTextEquals(typeNode->text, "c_vararg") ) {
        xl6OutputPuts("...", out);
        return;
    }
    if ( xl6CgenHeaderCAbiCallbackCanBridge(root, typeNode) ) {
        snprintf(
            callbackTypeName,
            sizeof(callbackTypeName),
            "XL6_C_Callback_%016llX",
            xl6CgenHeaderFunctionSignatureId(root, typeNode));
        xl6OutputPuts(callbackTypeName, out);
    } else {
        xl6CgenHeaderPutTypeName(out, root, typeNode);
    }
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

