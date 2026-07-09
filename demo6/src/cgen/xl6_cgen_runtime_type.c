static void xl6CgenEmitRuntimeTypeCastHelpers(XL6_CgenContext* ctx);
static void xl6CgenEmitRuntimeTypeClassRecordCastHelper(XL6_CgenContext* ctx, const XL6_AstNode* targetType);

static int xl6CgenNodeNeedsRuntimeTypeOps(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE_IS || node->kind == XL6_AST_TYPE_AS ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRuntimeTypeOps(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitRuntimeTypeClassDescRef(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    char cTypeName[256];

    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), classNode);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
}

static void xl6CgenEmitRuntimeTypeClassBaseChecks(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    XL6_AstNode* baseClass;

    if ( classNode == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    while ( baseClass != NULL && !xl6CgenClassHasTypeParams(baseClass) ) {
        xl6OutputPuts("        if (xrtTypeSame(target, ", ctx->out);
        xl6CgenEmitRuntimeTypeClassDescRef(ctx, baseClass);
        xl6OutputPuts(")) { return true; }\n", ctx->out);
        baseClass = xl6CgenBaseClass(ctx, baseClass);
    }
}

static void xl6CgenEmitRuntimeTypeClassRelations(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* baseClass;

        if ( node == NULL || node->kind != XL6_AST_CLASS || xl6CgenClassHasTypeParams(node) ) {
            continue;
        }
        baseClass = xl6CgenBaseClass(ctx, node);
        if ( baseClass == NULL || xl6CgenClassHasTypeParams(baseClass) ) {
            continue;
        }

        xl6OutputPuts("    if (xrtTypeSame(actual, ", ctx->out);
        xl6CgenEmitRuntimeTypeClassDescRef(ctx, node);
        xl6OutputPuts(")) {\n", ctx->out);
        xl6CgenEmitRuntimeTypeClassBaseChecks(ctx, node);
        xl6OutputPuts("    }\n", ctx->out);
    }
}

static void xl6CgenEmitRuntimeTypeHelpers(XL6_CgenContext* ctx)
{
    XL6_Output* out;

    if ( ctx == NULL || ctx->out == NULL ) {
        return;
    }
    out = ctx->out;

    xl6OutputPuts("static bool xl6_runtime_type_is(const xrt_type_desc* actual, const xrt_type_desc* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if (xrtTypeSame(actual, target)) { return true; }\n", out);
    xl6OutputPuts("    if (actual == 0 || target == 0) { return false; }\n", out);
    xl6CgenEmitRuntimeTypeClassRelations(ctx);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const xrt_type_desc* xl6_runtime_value_type_desc(xvalue value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* desc;\n", out);
    xl6OutputPuts("    if (value == 0) { return xrtTypeNull(); }\n", out);
    xl6OutputPuts("    desc = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    if (desc != 0) { return desc; }\n", out);
    xl6OutputPuts("    switch (xvoType(value)) {\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_NULL: return xrtTypeNull();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_BOOL: return xrtTypeBool();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_INT: return xrtTypeInt();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_FLOAT: return xrtTypeFloat();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_TEXT: return xrtTypeString();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_TIME: return xrtTypeTime();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_POINT: return xrtTypePoint();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_FUNC: return xrtTypeFunction();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_ARRAY: return xrtTypeArray();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_LIST: return xrtTypeList();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_COLL: return xrtTypeSet();\n", out);
    xl6OutputPuts("    case XL6_XVO_DT_TABLE: return xrtTypeDict();\n", out);
    xl6OutputPuts("    default: return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_runtime_value_is(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual = xl6_runtime_value_type_desc(value);\n", out);
    xl6OutputPuts("    bool result = xl6_runtime_type_is(actual, target);\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_runtime_value_as_record(xvalue value, const xrt_type_desc* target, void* outValue, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual = xl6_runtime_value_type_desc(value);\n", out);
    xl6OutputPuts("    const void* data = 0;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    if (outValue != 0 && target != 0 && xl6_runtime_type_is(actual, target)) {\n", out);
    xl6OutputPuts("        data = xvoGetRecordData(value);\n", out);
    xl6OutputPuts("        if (data != 0) {\n", out);
    xl6OutputPuts("            if (target->Ops != 0 && target->Ops->copy != 0) { target->Ops->copy(outValue, data); }\n", out);
    xl6OutputPuts("            else { memcpy(outValue, data, target->Size); }\n", out);
    xl6OutputPuts("            ok = true;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_runtime_value_as_handle(xvalue value, const xrt_type_desc* target, void* outValue, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual = xl6_runtime_value_type_desc(value);\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    if (outValue != 0 && target != 0 && xrtTypeSame(actual, target)) {\n", out);
    xl6OutputPuts("        memset(outValue, 0, target->Size);\n", out);
    xl6OutputPuts("        *((void**)outValue) = xvoGetHandleData(value);\n", out);
    xl6OutputPuts("        ok = true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static long long xl6_runtime_value_as_i64(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetInt(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_runtime_fatal(4); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static double xl6_runtime_value_as_f64(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double result = 0.0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetFloat(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_runtime_fatal(4); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_value_as_bool(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetBool(value) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_runtime_fatal(4); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_runtime_temp_cstr(const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if (value == 0) { return \"\"; }\n", out);
    xl6OutputPuts("    len = strlen(value);\n", out);
    xl6OutputPuts("    text = (char*)xrtTempMemory(len + 1u);\n", out);
    xl6OutputPuts("    if (text == 0) { return \"\"; }\n", out);
    xl6OutputPuts("    memcpy(text, value, len + 1u);\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_runtime_value_as_string(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* result = \"\";\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xl6_runtime_temp_cstr(xvoGetText(value)); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_runtime_fatal(4); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_runtime_value_as_point(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetPoint(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_runtime_fatal(4); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6CgenEmitRuntimeTypeCastHelpers(ctx);
}

static void xl6CgenEmitRuntimeTypeRecordCastHelper(XL6_CgenContext* ctx, const XL6_AstNode* target)
{
    const XL6_AstNode* concreteTarget;
    const char* cType;

    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, target));
    if ( concreteTarget == NULL ||
         !(xl6CgenIsClassType(ctx, concreteTarget) ||
           xl6CgenIsUnionType(ctx, concreteTarget) ||
           xl6CgenIsNativeBackedClassType(ctx, concreteTarget)) ) {
        return;
    }

    if ( xl6CgenIsClassType(ctx, concreteTarget) &&
         !xl6CgenIsNativeBackedClassType(ctx, concreteTarget) ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "#ifndef XL6_RUNTIME_AS_HELPER_%s\n", cType);
        xl6OutputPrintf(ctx->out, "#define XL6_RUNTIME_AS_HELPER_%s\n", cType);
        xl6CgenEmitRuntimeTypeClassRecordCastHelper(ctx, concreteTarget);
        xl6OutputPuts("#endif\n\n", ctx->out);
        return;
    }

    cType = xl6CgenTypeName(ctx, concreteTarget);
    xl6OutputPrintf(ctx->out, "#ifndef XL6_RUNTIME_AS_HELPER_%s\n", cType);
    xl6OutputPrintf(ctx->out, "#define XL6_RUNTIME_AS_HELPER_%s\n", cType);
    xl6OutputPrintf(ctx->out, "static %s xl6_runtime_value_as_%s(xvalue value, bool release)\n", cType, cType);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    %s result = {0};\n", cType);
    if ( xl6CgenIsNativeBackedClassType(ctx, concreteTarget) ) {
        xl6OutputPuts("    if (!xl6_runtime_value_as_handle(value, ", ctx->out);
    } else {
        xl6OutputPuts("    if (!xl6_runtime_value_as_record(value, ", ctx->out);
    }
    xl6CgenEmitXrtTypeDescRef(ctx, concreteTarget);
    xl6OutputPuts(", &result, release)) { xl6_runtime_fatal(4); }\n", ctx->out);
    xl6OutputPuts("    return result;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
    xl6OutputPuts("#endif\n\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeCastHelpersForNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE_AS && node->childCount > 1 ) {
        xl6CgenEmitRuntimeTypeRecordCastHelper(ctx, node->children[1]);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenEmitRuntimeTypeCastHelpersForNode(ctx, node->children[i]);
    }
}

static void xl6CgenEmitRuntimeTypeCastHelpers(XL6_CgenContext* ctx)
{
    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    xl6CgenEmitRuntimeTypeCastHelpersForNode(ctx, ctx->root);
}

static void xl6CgenEmitRuntimeTypeClassUpcastCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetClass,
    const XL6_AstNode* actualClass,
    const char* targetCType,
    const char* basePath)
{
    char actualCType[512];
    int prevIndent;

    if ( ctx == NULL || targetClass == NULL || actualClass == NULL ||
         targetCType == NULL || basePath == NULL ) {
        return;
    }

    xl6CgenClassTypeRefName(actualCType, sizeof(actualCType), actualClass);
    xl6OutputPuts("    if (xrtTypeSame(actual, ", ctx->out);
    xl6CgenEmitRuntimeTypeClassDescRef(ctx, actualClass);
    xl6OutputPuts(")) {\n", ctx->out);
    xl6OutputPuts("        const ", ctx->out);
    xl6OutputPuts(actualCType, ctx->out);
    xl6OutputPuts("* source = (const ", ctx->out);
    xl6OutputPuts(actualCType, ctx->out);
    xl6OutputPuts("*)data;\n", ctx->out);
    xl6OutputPuts("        *outValue = source", ctx->out);
    if ( basePath[0] == '.' ) {
        xl6OutputPuts("->", ctx->out);
        xl6OutputPuts(basePath + 1, ctx->out);
    } else {
        xl6OutputPuts(basePath, ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    prevIndent = ctx->indent;
    ctx->indent = 2;
    xl6CgenClassClearVirtualSelfRefs(ctx, targetClass, "(*outValue)");
    xl6CgenRetainClassFunctionFields(ctx, targetClass, "(*outValue)");
    if ( xl6CgenClassHasVirtualSlotsInChain(ctx, targetClass) ) {
        xl6CgenClassAssignVirtualSelfRefs(ctx, targetClass, "(*outValue)", "value");
    }
    ctx->indent = prevIndent;

    xl6OutputPuts("        ok = true;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeClassRecordCastHelper(XL6_CgenContext* ctx, const XL6_AstNode* targetType)
{
    XL6_AstNode* targetClass;
    char targetCType[512];
    size_t i;

    targetClass = xl6CgenClassNodeForType(ctx, targetType);
    if ( ctx == NULL || ctx->root == NULL || targetClass == NULL || xl6CgenClassHasTypeParams(targetClass) ) {
        return;
    }

    xl6CgenClassTypeRefName(targetCType, sizeof(targetCType), targetType);
    xl6OutputPrintf(ctx->out, "static bool xl6_runtime_value_as_class_%s(xvalue value, %s* outValue, bool release)\n", targetCType, targetCType);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    const xrt_type_desc* actual = xl6_runtime_value_type_desc(value);\n", ctx->out);
    xl6OutputPuts("    const void* data = xvoGetRecordData(value);\n", ctx->out);
    xl6OutputPuts("    bool ok = false;\n", ctx->out);
    xl6OutputPuts("    if (outValue != 0 && data != 0) {\n", ctx->out);
    xl6OutputPuts("        if (xrtTypeSame(actual, ", ctx->out);
    xl6CgenEmitRuntimeTypeClassDescRef(ctx, targetClass);
    xl6OutputPuts(")) {\n", ctx->out);
    xl6OutputPrintf(ctx->out, "            xl6_type_copy_%s(outValue, data);\n", targetCType);
    xl6OutputPuts("            ok = true;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    if (!ok && outValue != 0 && data != 0) {\n", ctx->out);
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        char basePath[256];

        if ( node == NULL || node->kind != XL6_AST_CLASS ||
             node == targetClass ||
             xl6CgenClassHasTypeParams(node) ) {
            continue;
        }
        basePath[0] = '\0';
        if ( !xl6CgenClassBasePath(ctx, node, targetClass, basePath, sizeof(basePath)) ||
             basePath[0] == '\0' ) {
            continue;
        }
        xl6CgenEmitRuntimeTypeClassUpcastCase(ctx, targetClass, node, targetCType, basePath);
    }
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    if (release && value != 0) { xvoUnref(value); }\n", ctx->out);
    xl6OutputPuts("    return ok;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPrintf(ctx->out, "static %s xl6_runtime_value_as_%s(xvalue value, bool release)\n", targetCType, targetCType);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    %s result = {0};\n", targetCType);
    xl6OutputPrintf(ctx->out, "    if (!xl6_runtime_value_as_class_%s(value, &result, release)) { xl6_runtime_fatal(4); }\n", targetCType);
    xl6OutputPuts("    return result;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenRuntimeTypeStaticIs(XL6_CgenContext* ctx, const XL6_AstNode* valueType, const XL6_AstNode* targetType)
{
    const XL6_AstNode* concreteValue;
    const XL6_AstNode* concreteTarget;
    XL6_AstNode* valueClass;
    XL6_AstNode* targetClass;

    concreteValue = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, valueType));
    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, targetType));
    if ( concreteValue == NULL || concreteTarget == NULL ) {
        return 0;
    }
    valueClass = xl6CgenClassNodeForType(ctx, concreteValue);
    targetClass = xl6CgenClassNodeForType(ctx, concreteTarget);
    if ( valueClass != NULL && targetClass != NULL ) {
        if ( concreteValue->kind == XL6_AST_TYPE && concreteValue->childCount > 0 ) {
            return xl6CgenTypeNodeEqual(concreteValue, concreteTarget);
        }
        if ( concreteTarget->kind == XL6_AST_TYPE && concreteTarget->childCount > 0 ) {
            return xl6CgenTypeNodeEqual(concreteValue, concreteTarget);
        }
        if ( xl6CTextEquals(valueClass->text, targetClass->text) ) {
            return 1;
        }
        while ( valueClass != NULL ) {
            valueClass = xl6CgenBaseClass(ctx, valueClass);
            if ( valueClass != NULL && xl6CTextEquals(valueClass->text, targetClass->text) ) {
                return 1;
            }
        }
        return 0;
    }
    if ( xl6CgenTypeNodeEqual(concreteValue, concreteTarget) ) {
        return 1;
    }
    if ( concreteValue->childCount > 0 || concreteTarget->childCount > 0 ) {
        return 0;
    }
    return 0;
}

static const XL6_AstNode* xl6CgenRuntimeTypeFunctionTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType)
{
    const XL6_AstNode* concreteTarget;

    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, targetType));
    if ( concreteTarget != NULL && xl6CgenIsFunctionType(concreteTarget) && concreteTarget->childCount >= 2 ) {
        return concreteTarget;
    }
    return NULL;
}

static void xl6CgenRuntimeTypeFunctionIsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* target)
{
    unsigned long long typeId;

    typeId = xl6CgenFunctionSignatureId(ctx, target->children[0], target->children[1]);
    xl6OutputPuts("(xl6_xrt_type_value_id(", ctx->out);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false) == " : ", true) == ", ctx->out);
    xl6OutputPrintf(ctx->out, "0x%016llXULL)", typeId);
}

static void xl6CgenRuntimeTypeIsExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    const XL6_AstNode* target;
    const XL6_AstNode* functionTarget;
    XL6_AstNode* valueType;
    const XL6_AstNode* concreteValueType;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    target = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    valueType = xl6CgenCloneExprTypeForInference(ctx, value);
    concreteValueType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, valueType));
    if ( concreteValueType != NULL && !xl6CgenIsXValueType(concreteValueType) ) {
        xl6OutputPuts(xl6CgenRuntimeTypeStaticIs(ctx, concreteValueType, target) ? "1" : "0", ctx->out);
        xl6AstFree(valueType);
        return;
    }

    functionTarget = xl6CgenRuntimeTypeFunctionTarget(ctx, target);
    if ( functionTarget != NULL ) {
        xl6CgenRuntimeTypeFunctionIsExpr(ctx, value, functionTarget);
        xl6AstFree(valueType);
        return;
    }

    xl6OutputPuts("xl6_runtime_value_is(", ctx->out);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, target);
    xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)", ctx->out);
    xl6AstFree(valueType);
}

static int xl6CgenRuntimeTypeAsScalarExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* target)
{
    const XL6_AstNode* concreteTarget;
    const char* name;
    const char* helper;
    const char* cType;

    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, target));
    name = concreteTarget != NULL ? concreteTarget->text : NULL;
    if ( name == NULL ) {
        return 0;
    }

    helper = NULL;
    if ( xl6CgenIsIntegerTypeName(name) || xl6CTextEquals(name, "type") ) {
        helper = "xl6_runtime_value_as_i64";
    } else if ( xl6CgenIsFloatTypeName(name) ) {
        helper = "xl6_runtime_value_as_f64";
    } else if ( xl6CgenIsBoolTypeName(name) ) {
        helper = "xl6_runtime_value_as_bool";
    } else if ( xl6CTextEquals(name, "time") ) {
        helper = "xl6_runtime_value_as_i64";
    } else if ( xl6CTextEquals(name, "point") ) {
        helper = "xl6_runtime_value_as_point";
    } else if ( xl6CTextEquals(name, "string") ) {
        helper = "xl6_runtime_value_as_string";
    }

    if ( helper == NULL ) {
        return 0;
    }

    cType = xl6CgenTypeName(ctx, concreteTarget);
    xl6OutputPrintf(ctx->out, "((%s)%s(", cType, helper);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, concreteTarget);
    xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false))" : ", true))", ctx->out);
    return 1;
}

static void xl6CgenRuntimeTypeAsExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    const XL6_AstNode* target;
    const XL6_AstNode* concreteTarget;
    XL6_AstNode* valueType;
    const XL6_AstNode* concreteValueType;
    const char* cType;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    target = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    valueType = xl6CgenCloneExprTypeForInference(ctx, value);
    concreteValueType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, valueType));
    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, target));

    if ( concreteValueType != NULL && concreteTarget != NULL &&
         !xl6CgenIsXValueType(concreteValueType) &&
         xl6CgenTypeNodeEqual(concreteValueType, concreteTarget) ) {
        xl6CgenExpr(ctx, value);
        xl6AstFree(valueType);
        return;
    }

    if ( concreteValueType != NULL && concreteTarget != NULL &&
         !xl6CgenIsXValueType(concreteValueType) &&
         xl6CgenClassUpcastExpr(ctx, concreteTarget, value) ) {
        xl6AstFree(valueType);
        return;
    }

    if ( xl6CgenRuntimeTypeAsScalarExpr(ctx, value, concreteTarget) ) {
        xl6AstFree(valueType);
        return;
    }

    if ( concreteTarget != NULL && (xl6CgenIsClassType(ctx, concreteTarget) || xl6CgenIsUnionType(ctx, concreteTarget)) ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "xl6_runtime_value_as_%s(", cType);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(valueType);
        return;
    }

    if ( concreteTarget != NULL && xl6CgenIsNativeBackedClassType(ctx, concreteTarget) ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "xl6_runtime_value_as_%s(", cType);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(valueType);
        return;
    }

    xl6CgenError(ctx, node, "当前 C 生成不支持该 as 目标类型");
    xl6OutputPuts("0", ctx->out);
    xl6AstFree(valueType);
}
