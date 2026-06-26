static int xl6CgenIsBuiltinTypeName(const char* name)
{
    static const char* names[] = {
        "null", "bool",
        "int", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
        "float", "float32", "float64",
        "c_char", "c_schar", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
        "c_long", "c_ulong", "c_longlong", "c_ulonglong",
        "c_size", "c_ssize", "c_ptrdiff", "c_intptr", "c_uintptr", "c_bool",
        "c_float", "c_double", "c_wchar",
        "string", "time", "point", "error", "function", "var", "type",
        "array", "list", "set", "dict",
        "future", "task", "channel", "generator",
        "thread", "mutex", "cond", "rwlock", "sem"
    };
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6CTextEquals(name, names[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsIntegerTypeName(const char* name)
{
    return xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int8") ||
           xl6CTextEquals(name, "uint8") || xl6CTextEquals(name, "int16") ||
           xl6CTextEquals(name, "uint16") || xl6CTextEquals(name, "int32") ||
           xl6CTextEquals(name, "uint32") || xl6CTextEquals(name, "int64") ||
           xl6CTextEquals(name, "uint64") ||
           xl6CTextEquals(name, "c_char") || xl6CTextEquals(name, "c_schar") ||
           xl6CTextEquals(name, "c_uchar") || xl6CTextEquals(name, "c_short") ||
           xl6CTextEquals(name, "c_ushort") || xl6CTextEquals(name, "c_int") ||
           xl6CTextEquals(name, "c_uint") || xl6CTextEquals(name, "c_long") ||
           xl6CTextEquals(name, "c_ulong") || xl6CTextEquals(name, "c_longlong") ||
           xl6CTextEquals(name, "c_ulonglong") || xl6CTextEquals(name, "c_size") ||
           xl6CTextEquals(name, "c_ssize") || xl6CTextEquals(name, "c_ptrdiff") ||
           xl6CTextEquals(name, "c_intptr") || xl6CTextEquals(name, "c_uintptr") ||
           xl6CTextEquals(name, "c_wchar");
}

static int xl6CgenIsFloatTypeName(const char* name)
{
    return xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float32") ||
           xl6CTextEquals(name, "float64") ||
           xl6CTextEquals(name, "c_float") || xl6CTextEquals(name, "c_double");
}

static int xl6CgenIsBoolTypeName(const char* name)
{
    return xl6CTextEquals(name, "bool") || xl6CTextEquals(name, "c_bool");
}

static int xl6CgenIsXValueTypeName(const char* name)
{
    return xl6CTextEquals(name, "var") ||
           xl6CTextEquals(name, "array") ||
           xl6CTextEquals(name, "list") ||
           xl6CTextEquals(name, "set") ||
           xl6CTextEquals(name, "dict");
}

static int xl6CgenIsBuiltinConvertTypeName(const char* name)
{
    return xl6CgenIsIntegerTypeName(name) || xl6CgenIsFloatTypeName(name) ||
           xl6CgenIsBoolTypeName(name) || xl6CTextEquals(name, "string") ||
           xl6CTextEquals(name, "time") || xl6CTextEquals(name, "point") ||
           xl6CgenIsXValueTypeName(name);
}

static const char* xl6CgenConvertCTypeName(const char* name)
{
    if ( xl6CTextEquals(name, "c_char") ) {
        return "char";
    }
    if ( xl6CTextEquals(name, "c_schar") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(name, "c_uchar") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(name, "c_short") ) {
        return "short";
    }
    if ( xl6CTextEquals(name, "c_ushort") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(name, "c_int") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "c_uint") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(name, "c_long") ) {
        return "long";
    }
    if ( xl6CTextEquals(name, "c_ulong") ) {
        return "unsigned long";
    }
    if ( xl6CTextEquals(name, "c_longlong") ) {
        return "long long";
    }
    if ( xl6CTextEquals(name, "c_ulonglong") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(name, "c_size") ) {
        return "size_t";
    }
    if ( xl6CTextEquals(name, "c_ssize") ) {
        return "xl6_ssize_t";
    }
    if ( xl6CTextEquals(name, "c_ptrdiff") ) {
        return "ptrdiff_t";
    }
    if ( xl6CTextEquals(name, "c_intptr") ) {
        return "intptr_t";
    }
    if ( xl6CTextEquals(name, "c_uintptr") ) {
        return "uintptr_t";
    }
    if ( xl6CTextEquals(name, "c_bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(name, "c_float") ) {
        return "float";
    }
    if ( xl6CTextEquals(name, "c_double") ) {
        return "double";
    }
    if ( xl6CTextEquals(name, "c_wchar") ) {
        return "wchar_t";
    }
    if ( xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int64") ||
         xl6CTextEquals(name, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(name, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(name, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(name, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(name, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(name, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(name, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(name, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(name, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(name, "point") ) {
        return "void*";
    }
    if ( xl6CgenIsXValueTypeName(name) ) {
        return "xvalue";
    }
    return "int";
}

static int xl6CgenIsKnownTypeName(XL6_CgenContext* ctx, const char* name)
{
    return xl6CgenIsBuiltinTypeName(name) ||
           xl6CgenFindTypeAlias(ctx, name) != NULL ||
           xl6CgenFindClass(ctx, name) != NULL ||
           xl6CgenFindCustom(ctx, name) != NULL ||
           xl6CgenFindUnion(ctx, name) != NULL;
}

static const char* xl6CgenTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    static char customTypeName[256];

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6CgenError(ctx, typeNode, "缺少类型");
        return "int";
    }

    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(typeNode->text, "c_char") || xl6CTextEquals(typeNode->text, "c_schar") ||
         xl6CTextEquals(typeNode->text, "c_uchar") || xl6CTextEquals(typeNode->text, "c_short") ||
         xl6CTextEquals(typeNode->text, "c_ushort") || xl6CTextEquals(typeNode->text, "c_int") ||
         xl6CTextEquals(typeNode->text, "c_uint") || xl6CTextEquals(typeNode->text, "c_long") ||
         xl6CTextEquals(typeNode->text, "c_ulong") || xl6CTextEquals(typeNode->text, "c_longlong") ||
         xl6CTextEquals(typeNode->text, "c_ulonglong") || xl6CTextEquals(typeNode->text, "c_size") ||
         xl6CTextEquals(typeNode->text, "c_ssize") || xl6CTextEquals(typeNode->text, "c_ptrdiff") ||
         xl6CTextEquals(typeNode->text, "c_intptr") || xl6CTextEquals(typeNode->text, "c_uintptr") ||
         xl6CTextEquals(typeNode->text, "c_bool") || xl6CTextEquals(typeNode->text, "c_float") ||
         xl6CTextEquals(typeNode->text, "c_double") || xl6CTextEquals(typeNode->text, "c_wchar") ) {
        return xl6CgenConvertCTypeName(typeNode->text);
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    if ( xl6CTextEquals(typeNode->text, "future") ||
         xl6CTextEquals(typeNode->text, "task") ) {
        return "xfuture*";
    }
    if ( xl6CTextEquals(typeNode->text, "channel") ) {
        return "xchannel";
    }
    if ( xl6CTextEquals(typeNode->text, "thread") ||
         xl6CTextEquals(typeNode->text, "mutex") ||
         xl6CTextEquals(typeNode->text, "cond") ||
         xl6CTextEquals(typeNode->text, "rwlock") ||
         xl6CTextEquals(typeNode->text, "sem") ) {
        return "void*";
    }
    if ( xl6CTextEquals(typeNode->text, "generator") ) {
        return "XL6_Generator*";
    }
    if ( xl6CTextEquals(typeNode->text, "error") ) {
        return "XL6_Error*";
    }
    if ( xl6CTextEquals(typeNode->text, "type") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "var") ||
         xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>") ||
         xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>") ||
         xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>") ||
         xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>") ) {
        return "xvalue";
    }
    if ( xl6CTextEquals(typeNode->text, "function") ) {
        return "XL6_Function";
    }
    if ( xl6CgenIsCustomType(ctx, typeNode) ) {
        xl6CgenCustomTypeRefName(customTypeName, sizeof(customTypeName), typeNode->text);
        return customTypeName;
    }
    if ( xl6CgenIsClassType(ctx, typeNode) ) {
        XL6_AstNode* canonicalType = NULL;
        const XL6_AstNode* refType = typeNode;

        if ( typeNode->childCount > 0 ) {
            canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            if ( canonicalType != NULL ) {
                refType = canonicalType;
            }
        }
        xl6CgenClassTypeRefName(customTypeName, sizeof(customTypeName), refType);
        xl6AstFree(canonicalType);
        return customTypeName;
    }
    if ( xl6CgenIsUnionType(ctx, typeNode) ) {
        xl6CgenUnionTypeTextName(customTypeName, sizeof(customTypeName), typeNode->text);
        return customTypeName;
    }

    snprintf(customTypeName, sizeof(customTypeName), "C generator does not support type node: %s (%s)",
             typeNode->text != NULL ? typeNode->text : "<missing>", xl6AstKindName(typeNode->kind));
    xl6CgenError(ctx, typeNode, customTypeName);
    return "int";
}

static const char* xl6CgenCAbiTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(typeNode->text, "c_char") || xl6CTextEquals(typeNode->text, "c_schar") ||
         xl6CTextEquals(typeNode->text, "c_uchar") || xl6CTextEquals(typeNode->text, "c_short") ||
         xl6CTextEquals(typeNode->text, "c_ushort") || xl6CTextEquals(typeNode->text, "c_int") ||
         xl6CTextEquals(typeNode->text, "c_uint") || xl6CTextEquals(typeNode->text, "c_long") ||
         xl6CTextEquals(typeNode->text, "c_ulong") || xl6CTextEquals(typeNode->text, "c_longlong") ||
         xl6CTextEquals(typeNode->text, "c_ulonglong") || xl6CTextEquals(typeNode->text, "c_size") ||
         xl6CTextEquals(typeNode->text, "c_ssize") || xl6CTextEquals(typeNode->text, "c_ptrdiff") ||
         xl6CTextEquals(typeNode->text, "c_intptr") || xl6CTextEquals(typeNode->text, "c_uintptr") ||
         xl6CTextEquals(typeNode->text, "c_bool") || xl6CTextEquals(typeNode->text, "c_float") ||
         xl6CTextEquals(typeNode->text, "c_double") || xl6CTextEquals(typeNode->text, "c_wchar") ) {
        return xl6CgenConvertCTypeName(typeNode->text);
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "int64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "int8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "uint8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "int16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "uint16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "uint32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "uint64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    return xl6CgenTypeName(ctx, typeNode);
}

static XL6_AstNode* xl6CgenFindFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( xl6CgenIsFuncDeclNode(node) &&
             !xl6CgenFuncIsGeneric(node) &&
             node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenTypeNodeEqual(const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->kind != b->kind || a->childCount != b->childCount ) {
        return 0;
    }
    if ( (a->text == NULL) != (b->text == NULL) ) {
        return 0;
    }
    if ( a->text != NULL && !xl6CTextEquals(a->text, b->text) ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        if ( !xl6CgenTypeNodeEqual(a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}
