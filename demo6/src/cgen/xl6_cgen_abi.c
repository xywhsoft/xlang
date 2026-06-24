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
    xl6CgenHashText(&hash, "function(");
    xl6CgenHashFunctionTypeList(ctx, &hash, params);
    xl6CgenHashText(&hash, ")->(");
    xl6CgenHashFunctionTypeList(ctx, &hash, returns);
    xl6CgenHashText(&hash, ")");
    return hash;
}

static unsigned long long xl6CgenFunctionSignatureIdForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg)
{
    unsigned long long hash;

    hash = 1469598103934665603ULL;
    xl6CgenHashText(&hash, "function(");
    xl6CgenHashFunctionTypePrefix(ctx, &hash, params, paramCount, includeVararg);
    xl6CgenHashText(&hash, ")->(");
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
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[i]), ctx->out);
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

static void xl6CgenEmitFunctionSigObject(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    xl6CgenEmitFunctionSigObjectForCounts(ctx,
                                          localName,
                                          name,
                                          signatureId,
                                          (unsigned int)(params != NULL ? params->childCount : 0),
                                          (unsigned int)(returns != NULL ? returns->childCount : 0));
}

static void xl6CgenEmitFunctionSigObjectForCounts(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, unsigned int paramCount, unsigned int returnCount)
{
    unsigned int nameSize;

    if ( localName == NULL || *localName == '\0' ) {
        localName = "__xl6_sig";
    }
    if ( name == NULL ) {
        name = "";
    }
    nameSize = (unsigned int)strlen(name);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "static const xrt_func_sig %s = { 0x%016llXULL, ", localName, signatureId);
    xl6CgenCStringLiteral(ctx->out, name);
    xl6OutputPrintf(ctx->out,
            ", %uu, %uu, 0, %uu, 0, 0u };\n",
            nameSize,
            paramCount,
            returnCount);
}

static unsigned long long xl6CgenTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    unsigned long long hash;
    XL6_AstNode namedType;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode != NULL && (typeNode->kind == XL6_AST_CLASS || typeNode->kind == XL6_AST_CUSTOM) ) {
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
    xl6CgenHashTypeNode(&hash, typeNode);
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
    if ( xl6CTextEquals(name, "null") ) {
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
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPuts("xrtTypeFunction()", ctx->out);
    } else if ( xl6CTextEquals(name, "type") ) {
        xl6OutputPuts("xrtTypeType()", ctx->out);
    } else if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else if ( xl6CgenIsUnionType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), concreteType->text);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else if ( xl6CgenIsCustomType(ctx, concreteType) ) {
        xl6CgenCustomTypeRefName(cTypeName, sizeof(cTypeName), concreteType->text);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPuts("()", ctx->out);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
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
    } else if ( xl6CgenIsUnionType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), name);
    } else if ( xl6CgenIsCustomType(ctx, concreteType) ) {
        xl6CgenCustomTypeRefName(cTypeName, sizeof(cTypeName), name);
    } else {
        return;
    }

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void);\n", ctx->out);
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
    xl6OutputPuts("        0,\n", ctx->out);
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
        xl6CgenError(ctx, expr, "code generation error");
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
