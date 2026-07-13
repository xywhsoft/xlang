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
    xl6CgenHashText(hash, typeNode->text != NULL ? typeNode->text : "_");
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
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
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
    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        typeNode = xl6CgenClassHandleTargetType(ctx, typeNode);
    }
    if ( typeNode == NULL ) {
        xl6CgenHashText(hash, "_");
        return;
    }
    xl6CgenHashText(hash, typeNode->text != NULL ? typeNode->text : "_");
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
    return "XL6_XRT_TYPE_KIND_INVALID";
}

static void xl6CgenRuntimeTypeDescSymbol(XL6_CgenContext* ctx, XL6_Output* out, const XL6_AstNode* typeNode)
{
    xl6OutputPrintf(out, "xl6_runtime_type_desc_%016llX", xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenEmitRuntimeTypeDescProto(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
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

    if ( concreteType != NULL && xl6CgenIsRuntimeContainerTypeDesc(ctx, concreteType) ) {
        xl6CgenEmitRuntimeTypeDescRef(ctx, concreteType);
        return;
    }
    if ( xl6CTextEquals(name, "var") ) {
        xl6OutputPuts("xrtTypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "null") ) {
        xl6OutputPuts("xrtTypeNull()", ctx->out);
    } else if ( xl6CTextEquals(name, "bool") ) {
        xl6OutputPuts("xrtTypeBool()", ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(name) ) {
        xl6OutputPuts("xrtTypeInt()", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(name) ) {
        xl6OutputPuts("xrtTypeFloat()", ctx->out);
    } else if ( xl6CTextEquals(name, "string") ) {
        xl6OutputPuts("xrtTypeString()", ctx->out);
    } else if ( xl6CTextEquals(name, "time") ) {
        xl6OutputPuts("xrtTypeTime()", ctx->out);
    } else if ( xl6CTextEquals(name, "point") ) {
        xl6OutputPuts("xrtTypePoint()", ctx->out);
    } else if ( xl6CgenIsFutureLikeType(concreteType) ) {
        xl6OutputPuts("xrtTypeFuture()", ctx->out);
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPuts("xrtTypeFunction()", ctx->out);
    } else if ( xl6CTextEquals(name, "array") || xl6CTextEquals(name, "array<var>") ) {
        xl6OutputPuts("xrtTypeArray()", ctx->out);
    } else if ( xl6CTextEquals(name, "list") || xl6CTextEquals(name, "list<var>") ) {
        xl6OutputPuts("xrtTypeList()", ctx->out);
    } else if ( xl6CTextEquals(name, "set") || xl6CTextEquals(name, "set<var>") ) {
        xl6OutputPuts("xrtTypeSet()", ctx->out);
    } else if ( xl6CTextEquals(name, "dict") || xl6CTextEquals(name, "dict<var>") ) {
        xl6OutputPuts("xrtTypeDict()", ctx->out);
    } else if ( xl6CTextEquals(name, "__class_handle") ) {
        xl6OutputPuts("xrtTypeValue()", ctx->out);
    } else if ( xl6CTextEquals(name, "type") ) {
        xl6OutputPuts("xrtTypeType()", ctx->out);
    } else if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
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

static const XL6_AstNode* xl6CgenFunctionSigParamAt(const XL6_AstNode* params, size_t paramCount, int includeVararg, size_t index)
{
    int fixedCount;

    if ( params == NULL ) {
        return NULL;
    }
    if ( index < paramCount && index < params->childCount ) {
        return params->children[index];
    }
    if ( includeVararg && index == paramCount ) {
        fixedCount = xl6CgenFixedParamCount(params);
        if ( params->childCount > (size_t)fixedCount ) {
            return params->children[fixedCount];
        }
    }
    return NULL;
}

static unsigned int xl6CgenFunctionSigParamCount(const XL6_AstNode* params, size_t paramCount, int includeVararg)
{
    unsigned int count;
    int fixedCount;

    if ( params == NULL ) {
        return 0;
    }
    count = (unsigned int)(paramCount < params->childCount ? paramCount : params->childCount);
    if ( includeVararg ) {
        fixedCount = xl6CgenFixedParamCount(params);
        if ( params->childCount > (size_t)fixedCount ) {
            count += 1u;
        }
    }
    return count;
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
    actualParamCount = xl6CgenFunctionSigParamCount(params, paramCount, includeVararg);
    returnCount = (unsigned int)(returns != NULL ? returns->childCount : 0);
    nameSize = (unsigned int)strlen(name);
    flags = xl6CgenFunctionSigFlags(params, paramCount, includeVararg);

    /* 签名元数据包含运行时类型指针，需要在 make 函数内按需填充。 */
    if ( actualParamCount > 0 ) {
        xl6CgenFunctionSigIndent(ctx);
        xl6OutputPrintf(ctx->out, "static xrt_func_param %s_params[%uu];\n", localName, actualParamCount);
    }
    if ( returnCount > 0 ) {
        xl6CgenFunctionSigIndent(ctx);
        xl6OutputPrintf(ctx->out, "static const struct xrt_type_desc* %s_returns[%uu];\n", localName, returnCount);
    }
    xl6CgenFunctionSigIndent(ctx);
    xl6OutputPrintf(ctx->out, "static xrt_func_sig %s;\n", localName);

    for ( i = 0; i < actualParamCount; ++i ) {
        xl6CgenEmitFunctionSigParamAssign(ctx, localName, i, xl6CgenFunctionSigParamAt(params, paramCount, includeVararg, i));
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

static void xl6CgenEmitRuntimeTypeDescDependencyProto(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const char* name;
    char cTypeName[256];
    size_t i;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    name = concreteType != NULL ? concreteType->text : NULL;

    if ( concreteType != NULL && xl6CgenIsRuntimeContainerTypeDesc(ctx, concreteType) ) {
        args = xl6CgenTypeNodeGenericArgs(concreteType);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                xl6CgenEmitRuntimeTypeDescDependencyProto(ctx, args->children[i]);
            }
        }
        return;
    }

    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
    } else if ( xl6CgenIsPlainRecordType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), name);
    } else if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), name);
    } else {
        return;
    }

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void);\n", ctx->out);
}

static void xl6CgenEmitRuntimeXValueTypeOps(XL6_CgenContext* ctx)
{
    XL6_Output* out;

    if ( ctx == NULL || ctx->out == NULL || ctx->runtimeTypeDescCount == 0 ) {
        return;
    }

    out = ctx->out;
    xl6OutputPuts("#ifndef XL6_XRT_TYPE_KIND_CLASS\n", out);
    xl6OutputPuts("#define XL6_XRT_TYPE_KIND_CLASS 17u\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XL6_RUNTIME_XVALUE_TYPE_OPS\n", out);
    xl6OutputPuts("#define XL6_RUNTIME_XVALUE_TYPE_OPS\n", out);
    xl6OutputPuts("static void xl6_runtime_xvalue_init(void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pObj != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pObj = xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_copy(void* pDst, const void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pSrc != 0 ? *(xvalue const*)pSrc : 0;\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pDst = value != 0 ? xvoCopy(value) : xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_move(void* pDst, void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pDst = pSrc != 0 ? *(xvalue*)pSrc : 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( pSrc != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pSrc = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_drop(void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value;\n", out);
    xl6OutputPuts("    if ( pObj == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    value = *(xvalue*)pObj;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xvoUnref(value);\n", out);
    xl6OutputPuts("        *(xvalue*)pObj = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_xvalue_compare_text(xvalue left, xvalue right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32 leftSize = 0;\n", out);
    xl6OutputPuts("    uint32 rightSize = 0;\n", out);
    xl6OutputPuts("    str leftText = xvoToString(left, &leftSize);\n", out);
    xl6OutputPuts("    str rightText = xvoToString(right, &rightSize);\n", out);
    xl6OutputPuts("    uint32 minSize = leftSize < rightSize ? leftSize : rightSize;\n", out);
    xl6OutputPuts("    int result = minSize > 0 ? memcmp(leftText, rightText, minSize) : 0;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        result = (leftSize > rightSize) - (leftSize < rightSize);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        result = result > 0 ? 1 : -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( leftText != 0 ) { xrtFree(leftText); }\n", out);
    xl6OutputPuts("    if ( rightText != 0 ) { xrtFree(rightText); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_xvalue_compare(const void* pA, const void* pB)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue left = pA != 0 ? *(xvalue const*)pA : 0;\n", out);
    xl6OutputPuts("    xvalue right = pB != 0 ? *(xvalue const*)pB : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* leftType;\n", out);
    xl6OutputPuts("    const xrt_type_desc* rightType;\n", out);
    xl6OutputPuts("    int cmp;\n", out);
    xl6OutputPuts("    if ( left == right ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    leftType = xvoTypeDesc(left);\n", out);
    xl6OutputPuts("    rightType = xvoTypeDesc(right);\n", out);
    xl6OutputPuts("    if ( !xrtTypeSame(leftType, rightType) ) {\n", out);
    xl6OutputPuts("        unsigned long long leftId = leftType != 0 ? (unsigned long long)leftType->TypeId : 0ULL;\n", out);
    xl6OutputPuts("        unsigned long long rightId = rightType != 0 ? (unsigned long long)rightType->TypeId : 0ULL;\n", out);
    xl6OutputPuts("        if ( leftId != rightId ) {\n", out);
    xl6OutputPuts("            return (leftId > rightId) - (leftId < rightId);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    cmp = xvoBasicCompare(left, right);\n", out);
    xl6OutputPuts("    if ( cmp != 2 ) {\n", out);
    xl6OutputPuts("        return cmp;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_runtime_xvalue_compare_text(left, right);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static uint64 xl6_runtime_xvalue_hash(const void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* type = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    uint64 seed = type != 0 ? (uint64)type->TypeId : 0ULL;\n", out);
    xl6OutputPuts("    uint32 textSize = 0;\n", out);
    xl6OutputPuts("    str text = xvoToString(value, &textSize);\n", out);
    xl6OutputPuts("    uint64 result = xrtHash64(&seed, sizeof(seed));\n", out);
    xl6OutputPuts("    if ( text != 0 && textSize > 0 ) {\n", out);
    xl6OutputPuts("        result ^= xrtHash64(text, textSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( text != 0 ) { xrtFree(text); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static str xl6_runtime_xvalue_to_string(const void* pObj, uint32* pSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    return xvoToString(value, pSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_runtime_xvalue_box(const void* pObj, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    xvalue boxed = value != 0 ? xvoCopy(value) : xvoCreateNull();\n", out);
    xl6OutputPuts("    if ( boxed != 0 && pType != 0 ) {\n", out);
    xl6OutputPuts("        (void)xvoSetTypeDesc(boxed, pType);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return boxed;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_runtime_xvalue_unbox(xvalue value, void* pOut, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual;\n", out);
    xl6OutputPuts("    xvalue copy;\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) {\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( value == 0 || xvoIsNull(value) ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pOut = xvoCreateNull();\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    actual = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    if ( pType != 0 && !xrtTypeSame(actual, pType) ) {\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = xvoCopy(value);\n", out);
    xl6OutputPuts("    if ( copy != 0 && pType != 0 ) {\n", out);
    xl6OutputPuts("        (void)xvoSetTypeDesc(copy, pType);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    *(xvalue*)pOut = copy != 0 ? copy : xvoCreateNull();\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const xrt_type_ops xl6_runtime_xvalue_ops = {\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_init,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_copy,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_move,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_drop,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_compare,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_hash,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_to_string,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_box,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_unbox\n", out);
    xl6OutputPuts("};\n", out);
    xl6OutputPuts("\nstatic void xl6_runtime_class_copy(void* pDst, const void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pSrc != 0 ? *(xvalue const*)pSrc : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) { *(xvalue*)pDst = value; }\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static int xl6_runtime_class_compare(const void* pA, const void* pB)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue left = pA != 0 ? *(xvalue const*)pA : 0;\n", out);
    xl6OutputPuts("    xvalue right = pB != 0 ? *(xvalue const*)pB : 0;\n", out);
    xl6OutputPuts("    bool leftNull = left == 0 || xvoIsNull(left);\n", out);
    xl6OutputPuts("    bool rightNull = right == 0 || xvoIsNull(right);\n", out);
    xl6OutputPuts("    if ( leftNull || rightNull ) { return leftNull == rightNull ? 0 : (leftNull ? -1 : 1); }\n", out);
    xl6OutputPuts("    return left == right ? 0 : ((uintptr_t)left < (uintptr_t)right ? -1 : 1);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static uint64 xl6_runtime_class_hash(const void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    return value == 0 || xvoIsNull(value) ? 0 : xrtHash64(&value, sizeof(value));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_class_box(const void* pObj, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    (void)pType;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); return value; }\n", out);
    xl6OutputPuts("    return xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static bool xl6_runtime_class_unbox(xvalue value, void* pOut, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual;\n", out);
    xl6OutputPuts("    const xrt_type_desc* target;\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( value == 0 || xvoIsNull(value) ) { *(xvalue*)pOut = xvoCreateNull(); return true; }\n", out);
    xl6OutputPuts("    actual = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    target = xrtTypeGenericArg(pType, 0u);\n", out);
    xl6OutputPuts("    if ( actual == 0 || target == 0 || !xrtTypeIsA(actual, target) ) { return false; }\n", out);
    xl6OutputPuts("    xvoAddRef(value);\n", out);
    xl6OutputPuts("    *(xvalue*)pOut = value;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_class_ops = {\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_init,\n", out);
    xl6OutputPuts("    xl6_runtime_class_copy,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_move,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_drop,\n", out);
    xl6OutputPuts("    xl6_runtime_class_compare,\n", out);
    xl6OutputPuts("    xl6_runtime_class_hash,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_to_string,\n", out);
    xl6OutputPuts("    xl6_runtime_class_box,\n", out);
    xl6OutputPuts("    xl6_runtime_class_unbox\n", out);
    xl6OutputPuts("};\n", out);

    xl6OutputPuts("\nxtarray xrtTypedArrayClone(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListClone(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetClone(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictClone(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayMoveToShared(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListMoveToShared(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetMoveToShared(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictMoveToShared(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayRetain(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListRetain(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetRetain(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictRetain(xtdict value);\n", out);
    xl6OutputPuts("xvalue xrtTypedArrayToValue(xtarray value);\n", out);
    xl6OutputPuts("xvalue xrtTypedListToValue(xtlist value);\n", out);
    xl6OutputPuts("xvalue xrtTypedSetToValue(xtset value);\n", out);
    xl6OutputPuts("xvalue xrtTypedDictToValue(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtlist xrtTypedListFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtset xrtTypedSetFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("void xrtTypedArrayDestroy(xtarray value);\n", out);
    xl6OutputPuts("void xrtTypedListDestroy(xtlist value);\n", out);
    xl6OutputPuts("void xrtTypedSetDestroy(xtset value);\n", out);
    xl6OutputPuts("void xrtTypedDictDestroy(xtdict value);\n", out);
    xl6OutputPuts("static void xl6_runtime_handle_init(void* pObj) { if (pObj != 0) { *(void**)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_handle_move(void* pDst, void* pSrc) { if (pDst != 0) { *(void**)pDst = pSrc != 0 ? *(void**)pSrc : 0; } if (pSrc != 0) { *(void**)pSrc = 0; } }\n", out);
    xl6OutputPuts("static int xl6_runtime_handle_compare(const void* pA, const void* pB) { void* a = pA != 0 ? *(void* const*)pA : 0; void* b = pB != 0 ? *(void* const*)pB : 0; return a == b ? 0 : ((uintptr_t)a < (uintptr_t)b ? -1 : 1); }\n", out);
    xl6OutputPuts("static uint64 xl6_runtime_handle_hash(const void* pObj) { void* value = pObj != 0 ? *(void* const*)pObj : 0; return value != 0 ? xrtHash64(&value, sizeof(value)) : 0; }\n", out);
    xl6OutputPuts("static void xl6_runtime_array_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtarray value = pSrc != 0 ? *(xtarray const*)pSrc : 0; *(xtarray*)pDst = value != 0 ? xrtTypedArrayRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_list_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtlist value = pSrc != 0 ? *(xtlist const*)pSrc : 0; *(xtlist*)pDst = value != 0 ? xrtTypedListRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_set_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtset value = pSrc != 0 ? *(xtset const*)pSrc : 0; *(xtset*)pDst = value != 0 ? xrtTypedSetRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_dict_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtdict value = pSrc != 0 ? *(xtdict const*)pSrc : 0; *(xtdict*)pDst = value != 0 ? xrtTypedDictRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_array_drop(void* pObj) { if (pObj != 0 && *(xtarray*)pObj != 0) { xrtTypedArrayDestroy(*(xtarray*)pObj); *(xtarray*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_list_drop(void* pObj) { if (pObj != 0 && *(xtlist*)pObj != 0) { xrtTypedListDestroy(*(xtlist*)pObj); *(xtlist*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_set_drop(void* pObj) { if (pObj != 0 && *(xtset*)pObj != 0) { xrtTypedSetDestroy(*(xtset*)pObj); *(xtset*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_dict_drop(void* pObj) { if (pObj != 0 && *(xtdict*)pObj != 0) { xrtTypedDictDestroy(*(xtdict*)pObj); *(xtdict*)pObj = 0; } }\n", out);
    xl6OutputPuts("static const xrt_type_desc* xl6_runtime_container_item_type(const xrt_type_desc* pType) { const xrt_generic_type* generic = pType != 0 ? (const xrt_generic_type*)pType->Extra : 0; return generic != 0 && generic->ArgCount == 1u ? generic->Args[0] : 0; }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_array_box(const void* pObj, const xrt_type_desc* pType) { xtarray value = pObj != 0 ? *(xtarray const*)pObj : 0; xvalue result = value != 0 ? xrtTypedArrayToValue(value) : xvoCreateNull(); if (result != 0 && pType != 0) { (void)xvoSetTypeDesc(result, pType); } return result; }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_list_box(const void* pObj, const xrt_type_desc* pType) { xtlist value = pObj != 0 ? *(xtlist const*)pObj : 0; xvalue result = value != 0 ? xrtTypedListToValue(value) : xvoCreateNull(); if (result != 0 && pType != 0) { (void)xvoSetTypeDesc(result, pType); } return result; }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_set_box(const void* pObj, const xrt_type_desc* pType) { xtset value = pObj != 0 ? *(xtset const*)pObj : 0; xvalue result = value != 0 ? xrtTypedSetToValue(value) : xvoCreateNull(); if (result != 0 && pType != 0) { (void)xvoSetTypeDesc(result, pType); } return result; }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_dict_box(const void* pObj, const xrt_type_desc* pType) { xtdict value = pObj != 0 ? *(xtdict const*)pObj : 0; xvalue result = value != 0 ? xrtTypedDictToValue(value) : xvoCreateNull(); if (result != 0 && pType != 0) { (void)xvoSetTypeDesc(result, pType); } return result; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_array_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtarray result; if (pOut == 0) { return false; } *(xtarray*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } result = itemType != 0 ? xrtTypedArrayFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtarray*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_list_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtlist result; if (pOut == 0) { return false; } *(xtlist*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } result = itemType != 0 ? xrtTypedListFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtlist*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_set_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtset result; if (pOut == 0) { return false; } *(xtset*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } result = itemType != 0 ? xrtTypedSetFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtset*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_dict_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtdict result; if (pOut == 0) { return false; } *(xtdict*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } result = itemType != 0 ? xrtTypedDictFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtdict*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_array_ops = { xl6_runtime_handle_init, xl6_runtime_array_copy, xl6_runtime_handle_move, xl6_runtime_array_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_array_box, xl6_runtime_array_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_list_ops = { xl6_runtime_handle_init, xl6_runtime_list_copy, xl6_runtime_handle_move, xl6_runtime_list_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_list_box, xl6_runtime_list_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_set_ops = { xl6_runtime_handle_init, xl6_runtime_set_copy, xl6_runtime_handle_move, xl6_runtime_set_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_set_box, xl6_runtime_set_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_dict_ops = { xl6_runtime_handle_init, xl6_runtime_dict_copy, xl6_runtime_handle_move, xl6_runtime_dict_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_dict_box, xl6_runtime_dict_unbox };\n", out);
    xl6OutputPuts("#endif\n\n", out);
}

static void xl6CgenEmitRuntimeTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    char typeText[512];
    unsigned long long typeId;
    const XL6_AstNode* args;
    size_t i;

    typeText[0] = '\0';
    xl6CgenTypeText(ctx, typeNode, typeText, sizeof(typeText));
    typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    args = xl6CgenTypeNodeGenericArgs(typeNode);

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPrintf(ctx->out, "    static const xrt_type_desc* args[%uu];\n", (unsigned)args->childCount);
        xl6OutputPrintf(ctx->out, "    static const xrt_generic_type generic = { %uu, args };\n", (unsigned)args->childCount);
    }
    xl6OutputPuts("    static const xrt_type_desc desc = {\n", ctx->out);
    xl6OutputPrintf(ctx->out, "        0x%08XU,\n", (unsigned int)(typeId & 0xFFFFFFFFULL));
    xl6OutputPrintf(ctx->out, "        %s,\n", xl6CgenRuntimeTypeDescKind(typeNode));
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, typeText);
    xl6OutputPuts(",\n", ctx->out);
    xl6OutputPrintf(ctx->out, "        %uu,\n", (unsigned)strlen(typeText));
    xl6OutputPuts("        \"xvalue\",\n", ctx->out);
    xl6OutputPuts("        6u,\n", ctx->out);
    xl6OutputPuts("        sizeof(void*),\n", ctx->out);
    xl6OutputPuts("        sizeof(void*),\n", ctx->out);
    if ( xl6CTextEquals(typeNode->text, "__class_handle") ) {
        xl6OutputPuts("        &xl6_runtime_class_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "array") ) {
        xl6OutputPuts("        &xl6_runtime_array_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "list") ) {
        xl6OutputPuts("        &xl6_runtime_list_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "set") ) {
        xl6OutputPuts("        &xl6_runtime_set_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "dict") ) {
        xl6OutputPuts("        &xl6_runtime_dict_ops,\n", ctx->out);
    } else {
        xl6OutputPuts("        &xl6_runtime_xvalue_ops,\n", ctx->out);
    }
    xl6OutputPuts("        0,\n", ctx->out);
    xl6OutputPuts((args != NULL && args->childCount > 0) ? "        &generic\n" : "        0\n", ctx->out);
    xl6OutputPuts("    };\n", ctx->out);
    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPuts("    if ( args[0] == 0 ) {\n", ctx->out);
        for ( i = 0; i < args->childCount; ++i ) {
            xl6OutputPrintf(ctx->out, "        args[%uu] = ", (unsigned)i);
            xl6CgenEmitXrtTypeDescRef(ctx, args->children[i]);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6OutputPuts("    return &desc;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeDescs(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDescDependencyProto(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDescProto(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
    if ( ctx->runtimeTypeDescCount > 0 ) {
        xl6OutputPutc('\n', ctx->out);
        xl6CgenEmitRuntimeXValueTypeOps(ctx);
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDesc(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
}

static int xl6CgenTypeValueUsesBaseType(const char* name)
{
    (void)name;
    return 0;
}

static XL6_AstNode* xl6CgenCloneExprTypeForTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* baseType;

    rawType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( rawType == NULL ) {
        return NULL;
    }

    concreteType = xl6CgenSubstituteGenericType(ctx, rawType);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL &&
         concreteType->childCount > 0 &&
         xl6CgenTypeValueUsesBaseType(concreteType->text) ) {
        baseType = xl6CgenNewSimpleType(concreteType->text, concreteType);
        xl6AstFree(rawType);
        return baseType;
    }
    return rawType;
}

static void xl6CgenEmitTypeOfExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;

    typeNode = xl6CgenCloneExprTypeForTypeValue(ctx, expr);
    if ( typeNode == NULL ) {
        xl6CgenError(ctx, expr, "cannot infer type for type(expr)");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "var") ) {
        xl6OutputPuts("xl6_xrt_type_value_id(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, expr) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(typeNode);
        return;
    }
    xl6CgenEmitTypeIdForTypeNode(ctx, typeNode);
    xl6AstFree(typeNode);
}
