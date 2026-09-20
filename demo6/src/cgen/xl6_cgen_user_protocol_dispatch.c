static void xl6CgenProtocolThunk(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* requirement,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* candidate)
{
    const XL6_AstNode* params = xl6CgenCallableParams(requirement);
    char basePath[256];
    int hasReturn = xl6CgenProtocolRequirementHasReturn(requirement);

    xl6OutputPuts("static ", ctx->out);
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenProtocolThunkSymbol(ctx, classNode, protocolNode, requirement);
    xl6OutputPutc('(', ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 0);
    xl6OutputPuts(")\n{\n    ", ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6CgenClassCallableSymbol(ctx, ownerClass, candidate);
    xl6OutputPutc('(', ctx->out);
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(
            ctx,
            classNode,
            ownerClass,
            basePath,
            sizeof(basePath)) ) {
        xl6CgenError(
            ctx,
            requirement,
            "protocol witness 无法生成继承 self 路径");
        basePath[0] = '\0';
    }
    if ( basePath[0] == '\0' ) {
        xl6OutputPuts("((", ctx->out);
        xl6CgenClassTypeName(ctx->out, classNode->text);
        xl6OutputPuts("*)xl6_class_data(self, \"\", 0, 0))", ctx->out);
    } else {
        xl6OutputPuts("&((", ctx->out);
        xl6CgenClassTypeName(ctx->out, classNode->text);
        xl6OutputPuts("*)xl6_class_data(self, \"\", 0, 0))->base", ctx->out);
        if ( strlen(basePath) > 5 ) {
            xl6OutputPuts(basePath + 5, ctx->out);
        }
    }
    xl6CgenProtocolParamNames(ctx, params, 1);
    xl6OutputPuts(");\n", ctx->out);
    if ( !hasReturn ) {
        xl6OutputPuts("    return;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenProtocolDispatchFunction(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* requirement)
{
    const XL6_AstNode* params = xl6CgenCallableParams(requirement);
    unsigned long long signatureId =
        xl6CgenProtocolRequirementSignatureId(ctx, requirement);
    unsigned long long protocolTypeId;
    XL6_AstNode protocolType;
    char runtimeName[384];
    int hasReturn = xl6CgenProtocolRequirementHasReturn(requirement);

    memset(&protocolType, 0, sizeof(protocolType));
    protocolType.kind = XL6_AST_TYPE;
    protocolType.text = protocolNode->text;
    protocolTypeId = xl6CgenTypeIdForTypeNode(ctx, &protocolType);

    xl6OutputPuts("static ", ctx->out);
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenProtocolCallSymbol(ctx, protocolNode, requirement);
    xl6OutputPutc('(', ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 1);
    xl6OutputPuts(")\n{\n", ctx->out);
    xl6OutputPuts(
        "    const xl6_type_desc* concrete = self != 0 ? xl6ValueType(self) : 0;\n"
        "    const xl6_protocol_witness* witness = concrete != 0\n"
        "        ? xl6ProtocolLookup(",
        ctx->out);
    xl6OutputPrintf(
        ctx->out,
        "0x%08XU, concrete->TypeId)\n"
        "        : 0;\n",
        (unsigned int)(protocolTypeId & 0xFFFFFFFFULL));
    xl6OutputPuts(
        "    const xl6_protocol_entry* entry = witness != 0\n"
        "        ? xl6ProtocolWitnessFind(witness, ",
        ctx->out);
    xl6CgenCStringLiteral(
        ctx->out,
        xl6CgenProtocolRequirementName(
            requirement,
            runtimeName,
            sizeof(runtimeName)));
    xl6OutputPrintf(
        ctx->out,
        ", %uu, 0x%016llXULL)\n"
        "        : 0;\n",
        (unsigned)strlen(runtimeName),
        signatureId);
    xl6OutputPuts("    if ( entry == 0 || entry->Entry == 0 ) {\n", ctx->out);
    xl6OutputPuts(
        "        xl6_error_raise(self == 0 || xl6_xrt_value_is_null(self) "
        "? \"NullReferenceError\" : \"TypeError\",\n"
        "                        self == 0 || xl6_xrt_value_is_null(self) "
        "? \"null protocol receiver\" : \"protocol witness not found\",\n"
        "                        file, line, column);\n"
        "        if ( releaseSelf && self != 0 ) { xrtValueRelease(self); }\n",
        ctx->out);
    xl6CgenProtocolDefaultReturn(ctx, requirement);
    xl6OutputPuts("    }\n", ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenProtocolReturnType(ctx, requirement);
        xl6OutputPuts(" result = ((", ctx->out);
    } else {
        xl6OutputPuts("    ((", ctx->out);
    }
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPuts(" (*)(", ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 0);
    xl6OutputPuts("))entry->Entry)(self", ctx->out);
    xl6CgenProtocolParamNames(ctx, params, 1);
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts(
        "    if ( releaseSelf && self != 0 ) { xrtValueRelease(self); }\n",
        ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("    return result;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenGenericProtocolThunk(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classType,
    const XL6_AstNode* protocolType,
    const XL6_AstNode* requirement,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* candidate)
{
    const XL6_AstNode* params = xl6CgenCallableParams(requirement);
    char basePath[256];
    char classTypeName[512];
    int hasReturn = xl6CgenProtocolRequirementHasReturn(requirement);

    xl6OutputPuts("static ", ctx->out);
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenGenericProtocolThunkSymbol(
        ctx,
        classType,
        protocolType,
        requirement);
    xl6OutputPutc('(', ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 0);
    xl6OutputPuts(")\n{\n    ", ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6CgenConcreteClassCallableSymbol(
        ctx,
        classNode,
        classType,
        ownerClass,
        candidate);
    xl6OutputPutc('(', ctx->out);
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(
            ctx,
            classNode,
            ownerClass,
            basePath,
            sizeof(basePath)) ) {
        xl6CgenError(
            ctx,
            requirement,
            "cannot generate inherited self path for generic protocol witness");
        basePath[0] = '\0';
    }
    xl6CgenClassTypeRefName(
        classTypeName,
        sizeof(classTypeName),
        classType);
    if ( basePath[0] == '\0' ) {
        xl6OutputPrintf(
            ctx->out,
            "((%s*)xl6_class_data(self, \"\", 0, 0))",
            classTypeName);
    } else {
        xl6OutputPrintf(
            ctx->out,
            "&((%s*)xl6_class_data(self, \"\", 0, 0))->base",
            classTypeName);
        if ( strlen(basePath) > 5 ) {
            xl6OutputPuts(basePath + 5, ctx->out);
        }
    }
    xl6CgenProtocolParamNames(ctx, params, 1);
    xl6OutputPuts(");\n", ctx->out);
    if ( !hasReturn ) {
        xl6OutputPuts("    return;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenGenericProtocolDispatchFunction(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolType,
    const XL6_AstNode* requirement)
{
    const XL6_AstNode* params = xl6CgenCallableParams(requirement);
    unsigned long long signatureId =
        xl6CgenProtocolRequirementSignatureId(ctx, requirement);
    unsigned long long protocolTypeId =
        xl6CgenTypeIdForTypeNode(ctx, protocolType);
    char runtimeName[384];
    int hasReturn = xl6CgenProtocolRequirementHasReturn(requirement);

    xl6OutputPuts("static ", ctx->out);
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenGenericProtocolCallSymbol(
        ctx,
        protocolType,
        requirement);
    xl6OutputPutc('(', ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 1);
    xl6OutputPuts(")\n{\n", ctx->out);
    xl6OutputPuts(
        "    const xl6_type_desc* concrete = self != 0 ? xl6ValueType(self) : 0;\n"
        "    const xl6_protocol_witness* witness = concrete != 0\n"
        "        ? xl6ProtocolLookup(",
        ctx->out);
    xl6OutputPrintf(
        ctx->out,
        "0x%08XU, concrete->TypeId)\n"
        "        : 0;\n",
        (unsigned int)(protocolTypeId & 0xFFFFFFFFULL));
    xl6OutputPuts(
        "    const xl6_protocol_entry* entry = witness != 0\n"
        "        ? xl6ProtocolWitnessFind(witness, ",
        ctx->out);
    xl6CgenCStringLiteral(
        ctx->out,
        xl6CgenProtocolRequirementName(
            requirement,
            runtimeName,
            sizeof(runtimeName)));
    xl6OutputPrintf(
        ctx->out,
        ", %uu, 0x%016llXULL)\n"
        "        : 0;\n",
        (unsigned)strlen(runtimeName),
        signatureId);
    xl6OutputPuts("    if (entry == 0 || entry->Entry == 0) {\n", ctx->out);
    xl6OutputPuts(
        "        xl6_error_raise(self == 0 || xl6_xrt_value_is_null(self) "
        "? \"NullReferenceError\" : \"TypeError\",\n"
        "                        self == 0 || xl6_xrt_value_is_null(self) "
        "? \"null protocol receiver\" : \"protocol witness not found\",\n"
        "                        file, line, column);\n"
        "        if (releaseSelf && self != 0) { xrtValueRelease(self); }\n",
        ctx->out);
    xl6CgenProtocolDefaultReturn(ctx, requirement);
    xl6OutputPuts("    }\n", ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenProtocolReturnType(ctx, requirement);
        xl6OutputPuts(" result = ((", ctx->out);
    } else {
        xl6OutputPuts("    ((", ctx->out);
    }
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPuts(" (*)(", ctx->out);
    xl6CgenProtocolParamDecls(ctx, params, 1, 0);
    xl6OutputPuts("))entry->Entry)(self", ctx->out);
    xl6CgenProtocolParamNames(ctx, params, 1);
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts(
        "    if (releaseSelf && self != 0) { xrtValueRelease(self); }\n",
        ctx->out);
    if ( hasReturn ) {
        xl6OutputPuts("    return result;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static int xl6CgenClassSatisfiesGenericProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* protocolType)
{
    XL6_AstNode classType;

    if ( classNode == NULL || protocolNode == NULL || protocolType == NULL ||
          xl6CgenClassHasTypeParams(classNode) ) {
        return 0;
    }
    memset(&classType, 0, sizeof(classType));
    classType.kind = XL6_AST_TYPE;
    classType.text = classNode->text;
    return xl6CgenConcreteClassSatisfiesProtocol(
        ctx,
        classNode,
        &classType,
        protocolNode,
        protocolType);
}

static void xl6CgenGenericProtocolDescriptor(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolType)
{
    char typeText[512];
    unsigned long long typeId;

    typeText[0] = '\0';
    xl6CgenTypeText(ctx, protocolType, typeText, sizeof(typeText));
    typeId = xl6CgenTypeIdForTypeNode(ctx, protocolType);
    xl6OutputPuts("static const xl6_type_desc* ", ctx->out);
    xl6CgenGenericProtocolTypeDescSymbol(ctx, protocolType);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    static xl6_type_desc desc = {\n", ctx->out);
    xl6OutputPrintf(
        ctx->out,
        "        0x%08XU, XL6_XRT_TYPE_KIND_PROTOCOL,\n        ",
        (unsigned int)(typeId & 0xFFFFFFFFULL));
    xl6CgenCStringLiteral(ctx->out, typeText);
    xl6OutputPrintf(
        ctx->out,
		", %uu,\n"
		"        \"xvalue*\", 6u, sizeof(void*), sizeof(void*),\n"
		"        &xl6_runtime_xvalue_ops, 0, 0, 0\n"
		"    };\n"
		"    return &desc;\n"
        "}\n\n",
        (unsigned)strlen(typeText));
}

