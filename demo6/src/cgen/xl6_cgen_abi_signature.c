static void xl6CgenHashText(unsigned long long* hash, const char* text)
{
    const unsigned char* p;

    if ( hash == NULL || text == NULL ) {
        return;
    }
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        *hash ^= (unsigned long long)(*p);
        *hash *= 1099511628211ULL;
    }
}

static void xl6CgenHashTypeNode(unsigned long long* hash, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( typeNode == NULL ) {
        xl6CgenHashText(hash, "_");
        return;
    }
    xl6CgenHashText(
        hash,
        typeNode->text != NULL ? xl6TypeCanonicalBuiltinName(typeNode->text) : "_");
    args = xl6CgenTypeNodeGenericArgs(typeNode);
    if ( args == NULL && typeNode->childCount == 0 ) {
        return;
    }
    xl6CgenHashText(hash, "<");
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            if ( i > 0 ) {
                xl6CgenHashText(hash, ",");
            }
            xl6CgenHashTypeNode(hash, args->children[i]);
        }
    } else {
        for ( i = 0; i < typeNode->childCount; ++i ) {
            if ( i > 0 ) {
                xl6CgenHashText(hash, ",");
            }
            xl6CgenHashTypeNode(hash, typeNode->children[i]);
        }
    }
    xl6CgenHashText(hash, ">");
}

static void xl6CgenTypeText(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, char* buffer, size_t capacity)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    size_t i;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    /*
        __class_handle<T> 是 class 的内部存储包装，类型身份应归并到 T。
        nullable<__class_handle<T>> 则是独立的一等类型，不能在这里顺带解包，
        否则 T 与 T? 会得到相同的 TypeId 和描述符符号。
    */
    if ( concreteType != NULL &&
         xl6CTextEquals(concreteType->text, "__class_handle") &&
         xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    if ( concreteType == NULL ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }

    xl6CgenAppendText(buffer, capacity, concreteType->text != NULL ? concreteType->text : "_");
    args = xl6CgenTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    xl6CgenAppendText(buffer, capacity, "<");
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6CgenAppendText(buffer, capacity, ",");
        }
        xl6CgenTypeText(ctx, args->children[i], buffer + strlen(buffer), capacity - strlen(buffer));
    }
    xl6CgenAppendText(buffer, capacity, ">");
}

static void xl6CgenHashSignatureTypeNode(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode != NULL &&
         xl6CTextEquals(typeNode->text, "__class_handle") &&
         xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        typeNode = xl6CgenClassHandleTargetType(ctx, typeNode);
    }
    if ( typeNode == NULL ) {
        xl6CgenHashText(hash, "_");
        return;
    }
    xl6CgenHashText(
        hash,
        typeNode->text != NULL ? xl6TypeCanonicalBuiltinName(typeNode->text) : "_");
    args = xl6CgenTypeNodeGenericArgs(typeNode);
    if ( args == NULL && typeNode->childCount == 0 ) {
        return;
    }
    xl6CgenHashText(hash, "<");
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            if ( i > 0 ) {
                xl6CgenHashText(hash, ",");
            }
            xl6CgenHashSignatureTypeNode(ctx, hash, args->children[i]);
        }
    } else {
        for ( i = 0; i < typeNode->childCount; ++i ) {
            if ( i > 0 ) {
                xl6CgenHashText(hash, ",");
            }
            xl6CgenHashSignatureTypeNode(ctx, hash, typeNode->children[i]);
        }
    }
    xl6CgenHashText(hash, ">");
}

static void xl6CgenHashFunctionTypeList(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->childCount; ++i ) {
        const XL6_AstNode* item = list->children[i];
        const XL6_AstNode* typeNode = item;
        if ( item != NULL && item->kind == XL6_AST_PARAM && item->childCount > 0 ) {
            typeNode = item->children[0];
        }
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        if ( item != NULL && (item->flags & XL6_AST_FLAG_BYVAL) != 0 ) {
            xl6CgenHashText(hash, "byval:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_BYREF) != 0 ) {
            xl6CgenHashText(hash, "byref:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_OUT) != 0 ) {
            xl6CgenHashText(hash, "out:");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
}

static void xl6CgenHashFunctionTypePrefix(XL6_CgenContext* ctx, unsigned long long* hash, const XL6_AstNode* list, size_t count, int includeVararg)
{
    size_t i;
    int fixedCount;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < count && i < list->childCount; ++i ) {
        const XL6_AstNode* item = list->children[i];
        const XL6_AstNode* typeNode = xl6CgenCallableItemType(item);
        if ( i > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        if ( item != NULL && (item->flags & XL6_AST_FLAG_BYVAL) != 0 ) {
            xl6CgenHashText(hash, "byval:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_BYREF) != 0 ) {
            xl6CgenHashText(hash, "byref:");
        } else if ( item != NULL && (item->flags & XL6_AST_FLAG_OUT) != 0 ) {
            xl6CgenHashText(hash, "out:");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
    if ( includeVararg ) {
        const XL6_AstNode* item;
        const XL6_AstNode* typeNode;

        fixedCount = xl6CgenFixedParamCount(list);
        if ( list->childCount <= (size_t)fixedCount ) {
            return;
        }
        item = list->children[fixedCount];
        typeNode = xl6CgenCallableItemType(item);
        if ( count > 0 ) {
            xl6CgenHashText(hash, ",");
        }
        xl6CgenHashSignatureTypeNode(ctx, hash, typeNode);
    }
}

static unsigned long long xl6CgenFunctionSignatureId(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    unsigned long long hash;

    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "func(");
    xl6CgenHashFunctionTypeList(ctx, &hash, params);
    xl6CgenHashText(&hash, ") as (");
    xl6CgenHashFunctionTypeList(ctx, &hash, returns);
    xl6CgenHashText(&hash, ")");
    return hash;
}

static unsigned long long xl6CgenFunctionSignatureIdForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    unsigned long long hash;

    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "func(");
    xl6CgenHashFunctionTypePrefix(ctx, &hash, params, paramCount, includeVararg);
    xl6CgenHashText(&hash, ") as (");
    xl6CgenHashFunctionTypeList(ctx, &hash, returns);
    xl6CgenHashText(&hash, ")");
    return hash;
}

static void xl6CgenFunctionReturnStructNameForId(XL6_Output* out, unsigned long long signatureId)
{
    xl6OutputPrintf(out, "XL6_Function_Return_%016llX", signatureId);
}

static void xl6CgenFunctionReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    xl6CgenFunctionReturnStructNameForId(ctx->out, xl6CgenFunctionSignatureId(ctx, params, returns));
}

static void xl6CgenFunctionReturnStructNameForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    xl6CgenFunctionReturnStructNameForId(ctx->out, xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, paramCount, includeVararg));
}

static void xl6CgenFunctionReturnStructDeclForId(XL6_CgenContext* ctx, const XL6_AstNode* returns, unsigned long long signatureId)
{
    size_t i;

    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    if ( !xl6CgenRememberFunctionReturnStruct(ctx, signatureId) ) {
        return;
    }

    xl6OutputPrintf(ctx->out, "#ifndef XL6_FUNCTION_RETURN_%016llX\n", signatureId);
    xl6OutputPrintf(ctx->out, "#define XL6_FUNCTION_RETURN_%016llX\n", signatureId);
    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenFunctionReturnStructNameForId(ctx->out, signatureId);
    xl6OutputPuts(" {\n", ctx->out);
    for ( i = 0; i < returns->childCount; ++i ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[i]), ctx->out);
        xl6OutputPrintf(ctx->out, " value%u;\n", (unsigned)i);
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenFunctionReturnStructNameForId(ctx->out, signatureId);
    xl6OutputPuts(";\n#endif\n\n", ctx->out);
}

static void xl6CgenFunctionReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclForId(ctx, returns, xl6CgenFunctionSignatureId(ctx, params, returns));
}

static void xl6CgenFunctionReturnStructDeclForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    if ( returns == NULL || returns->childCount <= 1 ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclForId(ctx, returns, xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, paramCount, includeVararg));
}

static unsigned long long xl6CgenTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    unsigned long long hash;
    XL6_AstNode namedType;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode != NULL && typeNode->kind == XL6_AST_CLASS ) {
        memset(&namedType, 0, sizeof(namedType));
        namedType.kind = XL6_AST_TYPE;
        namedType.text = typeNode->text;
        namedType.line = typeNode->line;
        namedType.column = typeNode->column;
        typeNode = &namedType;
    }
    if ( typeNode != NULL &&
         xl6CTextEquals(typeNode->text, "function") &&
         typeNode->childCount >= 2 ) {
        return xl6CgenFunctionSignatureId(ctx, typeNode->children[0], typeNode->children[1]);
    }
    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "type:");
    xl6CgenHashSignatureTypeNode(ctx, &hash, typeNode);
    return hash;
}

static void xl6CgenEmitTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    xl6OutputPrintf(ctx->out, "0x%016llXULL", xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenEmitTypeIdForName(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    XL6_AstNode typeNode;

    memset(&typeNode, 0, sizeof(typeNode));
    typeNode.kind = XL6_AST_TYPE;
    typeNode.text = (char*)(name != NULL ? name : "_");
    if ( at != NULL ) {
        typeNode.line = at->line;
        typeNode.column = at->column;
    }
    xl6CgenEmitTypeIdForTypeNode(ctx, &typeNode);
}

static const char* xl6CgenRuntimeTypeDescKind(const XL6_AstNode* typeNode)
{
    const char* name = typeNode != NULL ? typeNode->text : NULL;

    if ( xl6CTextEquals(name, "array") ) {
        return "XL6_XRT_TYPE_KIND_ARRAY";
    }
    if ( xl6CTextEquals(name, "list") ) {
        return "XL6_XRT_TYPE_KIND_LIST";
    }
    if ( xl6CTextEquals(name, "set") ) {
        return "XL6_XRT_TYPE_KIND_SET";
    }
    if ( xl6CTextEquals(name, "dict") ) {
        return "XL6_XRT_TYPE_KIND_DICT";
    }
    if ( xl6CTextEquals(name, "__class_handle") ) {
        return "XL6_XRT_TYPE_KIND_CLASS";
    }
    if ( xl6CgenIsSpecialCollectionTypeName(name) ) {
        return "XL6_XRT_TYPE_KIND_HANDLE";
    }
    return "XL6_XRT_TYPE_KIND_INVALID";
}

static void xl6CgenRuntimeTypeDescSymbol(XL6_CgenContext* ctx, XL6_Output* out, const XL6_AstNode* typeNode)
{
    xl6OutputPrintf(out, "xl6_runtime_type_desc_%016llX", xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenEmitRuntimeTypeDescProto(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    xl6OutputPuts("static const xl6_type_desc* ", ctx->out);
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("(void);\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("()", ctx->out);
}

static void xl6CgenEmitXrtTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;
    char cTypeName[256];

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    name = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsNullableType(concreteType) ) {
        xl6CgenEmitNullableTypeDescRef(ctx, concreteType);
        return;
    }
    if ( concreteType != NULL && xl6CgenIsRuntimeContainerTypeDesc(ctx, concreteType) ) {
        xl6CgenEmitRuntimeTypeDescRef(ctx, concreteType);
        return;
    }
    if ( xl6CTextEquals(name, "var") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "null") ) {
        xl6OutputPuts("xl6TypeNull()", ctx->out);
    } else if ( xl6CTextEquals(name, "bool") ) {
        xl6OutputPuts("xl6TypeBool32()", ctx->out);
    } else if ( xl6CTextEquals(name, "int8") || xl6CTextEquals(name, "c_schar") ) {
        xl6OutputPuts("xl6TypeInt8()", ctx->out);
    } else if ( xl6CTextEquals(name, "uint8") || xl6CTextEquals(name, "c_uchar") ) {
        xl6OutputPuts("xl6TypeUInt8()", ctx->out);
    } else if ( xl6CTextEquals(name, "int16") || xl6CTextEquals(name, "c_short") ) {
        xl6OutputPuts("xl6TypeInt16()", ctx->out);
    } else if ( xl6CTextEquals(name, "uint16") || xl6CTextEquals(name, "c_ushort") ) {
        xl6OutputPuts("xl6TypeUInt16()", ctx->out);
    } else if ( xl6CTextEquals(name, "int32") || xl6CTextEquals(name, "c_int") ) {
        xl6OutputPuts("xl6TypeInt32()", ctx->out);
    } else if ( xl6CTextEquals(name, "uint32") || xl6CTextEquals(name, "c_uint") ) {
        xl6OutputPuts("xl6TypeUInt32()", ctx->out);
    } else if ( xl6CTextEquals(name, "int64") || xl6CTextEquals(name, "c_longlong") ) {
        xl6OutputPuts("xl6TypeInt64()", ctx->out);
    } else if ( xl6CTextEquals(name, "uint64") || xl6CTextEquals(name, "c_ulonglong") ) {
        xl6OutputPuts("xl6TypeUInt64()", ctx->out);
    } else if ( xl6CTextEquals(name, "c_char") ) {
        xl6OutputPuts("(((char)-1 < 0) ? xl6TypeInt8() : xl6TypeUInt8())", ctx->out);
    } else if ( xl6CTextEquals(name, "c_long") || xl6CTextEquals(name, "c_ssize") ||
                xl6CTextEquals(name, "c_ptrdiff") || xl6CTextEquals(name, "c_intptr") ) {
        xl6OutputPuts("((sizeof(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, concreteType), ctx->out);
        xl6OutputPuts(") == 8u) ? xl6TypeInt64() : xl6TypeInt32())", ctx->out);
    } else if ( xl6CTextEquals(name, "c_ulong") || xl6CTextEquals(name, "c_size") ||
                xl6CTextEquals(name, "c_uintptr") ) {
        xl6OutputPuts("((sizeof(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, concreteType), ctx->out);
        xl6OutputPuts(") == 8u) ? xl6TypeUInt64() : xl6TypeUInt32())", ctx->out);
    } else if ( xl6CTextEquals(name, "c_wchar") ) {
        xl6OutputPuts("((sizeof(wchar_t) == 2u) ? (((wchar_t)-1 < 0) ? xl6TypeInt16() : xl6TypeUInt16()) : (((wchar_t)-1 < 0) ? xl6TypeInt32() : xl6TypeUInt32()))", ctx->out);
    } else if ( xl6CTextEquals(name, "c_bool") ) {
        xl6OutputPuts("xl6TypeBool()", ctx->out);
    } else if ( xl6CTextEquals(name, "int") ) {
        xl6OutputPuts("xl6TypeInt64()", ctx->out);
    } else if ( xl6CTextEquals(name, "float32") || xl6CTextEquals(name, "c_float") ) {
        xl6OutputPuts("xl6TypeFloat32()", ctx->out);
    } else if ( xl6CTextEquals(name, "float64") || xl6CTextEquals(name, "c_double") ) {
        xl6OutputPuts("xl6TypeFloat64()", ctx->out);
    } else if ( xl6CTextEquals(name, "float") ) {
        xl6OutputPuts("xl6TypeFloat64()", ctx->out);
    } else if ( xl6CTextEquals(name, "string") ) {
        xl6OutputPuts("xl6TypeString()", ctx->out);
    } else if ( xl6CTextEquals(name, "time") ) {
        xl6OutputPuts("xl6TypeTime()", ctx->out);
    } else if ( xl6CTextEquals(name, "point") ) {
        xl6OutputPuts("xl6TypePointer()", ctx->out);
    } else if ( xl6CgenIsFutureLikeType(concreteType) ) {
        xl6OutputPuts("xl6TypeFuture()", ctx->out);
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPuts("xl6TypeFunction()", ctx->out);
    } else if ( xl6CTextEquals(name, "array") || xl6CTextEquals(name, "array<var>") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "list") || xl6CTextEquals(name, "list<var>") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "set") || xl6CTextEquals(name, "set<var>") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "dict") || xl6CTextEquals(name, "dict<var>") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "error") ) {
        xl6OutputPuts("xl6TypeError()", ctx->out);
    } else if ( xl6CTextEquals(name, "__class_handle") ) {
        xl6OutputPuts("xl6TypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "type") ) {
        xl6OutputPuts("xl6TypeType()", ctx->out);
    } else if ( xl6CgenIsNumericEnumType(ctx, concreteType) ||
                xl6CgenIsTaggedEnumType(ctx, concreteType) ) {
        xl6CgenEnumTypeDescRef(ctx, concreteType);
    } else if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else if ( xl6CgenIsProtocolType(ctx, concreteType) ) {
        if ( concreteType->childCount > 0 ) {
            xl6CgenGenericProtocolTypeDescSymbol(ctx, concreteType);
        } else {
            xl6CgenProtocolTypeDescSymbol(
                ctx->out,
                xl6CgenFindProtocol(ctx, concreteType->text));
        }
        xl6OutputPuts("()", ctx->out);
    } else if ( xl6CgenIsPlainRecordType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), concreteType->text);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), concreteType->text);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
}

static const XL6_AstNode* xl6CgenFunctionSigParamAt(const XL6_AstNode* params, size_t paramCount, size_t index)
{
    int fixedCount;

    if ( params == NULL ) {
        return NULL;
    }
    fixedCount = xl6CgenFixedParamCount(params);
    if ( index < paramCount && index < (size_t)fixedCount && index < params->childCount ) {
        return params->children[index];
    }
    return NULL;
}

static unsigned int xl6CgenFunctionSigParamCount(const XL6_AstNode* params, size_t paramCount)
{
    int fixedCount;

    if ( params == NULL ) {
        return 0;
    }
    fixedCount = xl6CgenFixedParamCount(params);
    if ( paramCount > (size_t)fixedCount ) {
        paramCount = (size_t)fixedCount;
    }
    if ( paramCount > params->childCount ) {
        paramCount = params->childCount;
    }
    return (unsigned int)paramCount;
}

static const XL6_AstNode* xl6CgenFunctionSigVarargParam(const XL6_AstNode* params, size_t paramCount, int includeVararg)
{
    int fixedCount;

    if ( params == NULL ) {
        return NULL;
    }
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params->childCount <= (size_t)fixedCount ) {
        return NULL;
    }
    if ( includeVararg || paramCount > (size_t)fixedCount ) {
        return params->children[fixedCount];
    }
    return NULL;
}

static unsigned int xl6CgenFunctionSigFlags(const XL6_AstNode* params, size_t paramCount, int includeVararg)
{
    const XL6_AstNode* vararg;

    vararg = xl6CgenFunctionSigVarargParam(params, paramCount, includeVararg);
    if ( xl6CgenParamIsDictVararg(vararg) ) {
        return 0x00000002u;
    }
    if ( xl6CgenParamIsArrayVararg(vararg) ) {
        return 0x00000001u;
    }
    return 0u;
}

static unsigned int xl6CgenFunctionSigParamFlags(const XL6_AstNode* param)
{
    unsigned int flags = 0u;

    if ( param == NULL ) {
        return 0u;
    }
    if ( param->kind == XL6_AST_PARAM && !xl6CgenParamIsVararg(param) && param->childCount > 1 ) {
        flags |= 0x00000001u;
    }
    if ( (param->flags & XL6_AST_FLAG_BYVAL) != 0 ) {
        flags |= 0x00000010u;
    } else if ( (param->flags & XL6_AST_FLAG_BYREF) != 0 ) {
        flags |= 0x00000020u;
    } else if ( (param->flags & XL6_AST_FLAG_OUT) != 0 ) {
        flags |= 0x00000040u;
    }
    return flags;
}

static const char* xl6CgenFunctionSigParamName(const XL6_AstNode* param)
{
    if ( param != NULL && param->kind == XL6_AST_PARAM && param->text != NULL ) {
        return param->text;
    }
    return "";
}

static void xl6CgenFunctionSigIndent(XL6_CgenContext* ctx)
{
    (void)ctx;
    xl6OutputPuts("    ", ctx->out);
}

static void xl6CgenEmitFunctionSigParamAssign(XL6_CgenContext* ctx, const char* localName, size_t index, const XL6_AstNode* param)
{
    const XL6_AstNode* typeNode;
    const char* paramName;

    typeNode = xl6CgenCallableItemType(param);
    paramName = xl6CgenFunctionSigParamName(param);

    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s_params[%u].Name = ", localName, (unsigned)index);
    xl6CgenCStringLiteral(ctx->out, paramName);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s_params[%u].NameSize = %uu;\n", localName, (unsigned)index, (unsigned)strlen(paramName));
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s_params[%u].Type = ", localName, (unsigned)index);
    xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s_params[%u].DefaultValue = 0;\n", localName, (unsigned)index);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s_params[%u].Flags = 0x%08XU;\n", localName, (unsigned)index, xl6CgenFunctionSigParamFlags(param));
}

static void xl6CgenEmitFunctionSigObjectForPrefix(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    unsigned int actualParamCount;
    unsigned int returnCount;
    unsigned int nameSize;
    unsigned int flags;
    size_t i;

    if ( localName == NULL || *localName == '\0' ) {
        localName = "__xl6_sig";
    }
    if ( name == NULL ) {
        name = "";
    }
    actualParamCount = xl6CgenFunctionSigParamCount(params, paramCount);
    returnCount = (unsigned int)(returns != NULL ? returns->childCount : 0);
    nameSize = (unsigned int)strlen(name);
    flags = xl6CgenFunctionSigFlags(params, paramCount, includeVararg);

    /* 签名元数据包含运行时类型指针，需要在 make 函数内按需填充。 */
    if ( actualParamCount > 0 ) {
        xl6CgenFunctionSigIndent(ctx);
        xl6OutputPrintf(ctx->out, "static xl6_func_param %s_params[%uu];\n", localName, actualParamCount);
    }
    if ( returnCount > 0 ) {
        xl6CgenFunctionSigIndent(ctx);
        xl6OutputPrintf(ctx->out, "static const struct xl6_type_desc* %s_returns[%uu];\n", localName, returnCount);
    }
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "static xl6_func_sig %s;\n", localName);

    for ( i = 0; i < actualParamCount; ++i ) {
        xl6CgenEmitFunctionSigParamAssign(ctx, localName, i, xl6CgenFunctionSigParamAt(params, paramCount, i));
    }
    for ( i = 0; returns != NULL && i < returns->childCount; ++i ) {
        xl6CgenFunctionSigIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s_returns[%u] = ", localName, (unsigned)i);
        xl6CgenEmitXrtTypeDescRef(ctx, returns->children[i]);
        xl6OutputPuts(";\n", ctx->out);
    }

    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.TypeId = 0x%016llXULL;\n", localName, signatureId);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.Name = ", localName);
    xl6CgenCStringLiteral(ctx->out, name);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.NameSize = %uu;\n", localName, nameSize);
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.ParamCount = %uu;\n", localName, actualParamCount);
    xl6CgenFunctionSigIndent(ctx);
    if ( actualParamCount > 0 ) {
        xl6OutputPrintf(ctx->out, "%s.Params = %s_params;\n", localName, localName);
    } else {
        xl6OutputPrintf(ctx->out, "%s.Params = 0;\n", localName);
    }
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.ReturnCount = %uu;\n", localName, returnCount);
    xl6CgenFunctionSigIndent(ctx);
    if ( returnCount > 0 ) {
        xl6OutputPrintf(ctx->out, "%s.ReturnTypes = %s_returns;\n", localName, localName);
    } else {
        xl6OutputPrintf(ctx->out, "%s.ReturnTypes = 0;\n", localName);
    }
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.Flags = 0x%08XU;\n", localName, flags);
}

static void xl6CgenEmitFunctionSigObject(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    xl6CgenEmitFunctionSigObjectForPrefix(ctx,
                                          localName,
                                          name,
                                          signatureId,
                                          params,
                                          returns,
                                          params != NULL ? params->childCount : 0,
                                          0);
}

