static void xl6CgenEmitRuntimeTypeCastHelpers(XL6_CgenContext* ctx);
static void xl6CgenEmitRuntimeTypeClassRecordCastHelper(XL6_CgenContext* ctx, const XL6_AstNode* targetType);
static unsigned long long xl6CgenTypeIdForTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);

/*
    xlang 的 bool 在普通 C 表达式中使用 int 承载，因此静态 ABI 描述符是 bool32。
    xvalue 则保存 XRT 原生 bool；运行时 is/as 必须使用原生描述符进行比较。
*/
static void xl6CgenEmitRuntimeValueTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "bool") ) {
        xl6OutputPuts("xrtTypeBool()", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
}

static void xl6CgenEmitProgramTypeNameCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* typeName)
{
    if ( ctx == NULL || typeNode == NULL || typeName == NULL || typeName[0] == '\0' ) {
        return;
    }
    xl6OutputPrintf(ctx->out, "    if (typeId == 0x%016llXULL) { return ",
                    xl6CgenTypeIdForTypeNode(ctx, typeNode));
    xl6CgenCStringLiteral(ctx->out, typeName);
    xl6OutputPuts("; }\n", ctx->out);
}

/* type 本质上保存稳定 ID；程序自定义类型的反向名称由当前模块生成。 */
static void xl6CgenEmitProgramTypeNameHelper(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->out == NULL || ctx->root == NULL ) {
        return;
    }
    xl6OutputPuts("static const char* xl6_program_type_name_from_id(unsigned long long typeId)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode typeNode;

        if ( node == NULL || node->text == NULL ||
             (node->kind != XL6_AST_CLASS && node->kind != XL6_AST_UNION) ||
             (node->kind == XL6_AST_CLASS && xl6CgenClassHasTypeParams(node)) ) {
            continue;
        }
        memset(&typeNode, 0, sizeof(typeNode));
        typeNode.kind = XL6_AST_TYPE;
        typeNode.text = node->text;
        typeNode.line = node->line;
        typeNode.column = node->column;
        xl6CgenEmitProgramTypeNameCase(ctx, &typeNode, node->text);
    }

    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        char typeName[512];
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;

        typeName[0] = '\0';
        xl6CgenTypeText(ctx, typeNode, typeName, sizeof(typeName));
        xl6CgenEmitProgramTypeNameCase(ctx, typeNode, typeName);
    }

    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        char typeName[512];
        const XL6_AstNode* typeNode = ctx->runtimeTypeDescs[i].typeNode;

        typeName[0] = '\0';
        xl6CgenTypeText(ctx, typeNode, typeName, sizeof(typeName));
        xl6CgenEmitProgramTypeNameCase(ctx, typeNode, typeName);
    }

    xl6OutputPuts("    return xl6_xrt_type_name_from_id(typeId);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

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
    xl6OutputPuts(
        "    if (target->Kind == XL6_XRT_TYPE_KIND_PROTOCOL) {\n"
        "        return xrtProtocolLookup(target->TypeId, actual->TypeId) != 0;\n"
        "    }\n",
        out);
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
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_runtime_value_as_class(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if (!xl6_runtime_type_is(xl6_runtime_value_type_desc(value), target)) {\n", out);
    xl6OutputPuts("        if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("        xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if (release && value != 0) { (void)xl6_temp_managed_disown((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!release && value != 0) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_runtime_value_try_as_class(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if (!xl6_runtime_type_is(xl6_runtime_value_type_desc(value), target)) {\n", out);
    xl6OutputPuts("        if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if (release && value != 0) { (void)xl6_temp_managed_disown((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!release && value != 0) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("    return value;\n", out);
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
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
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
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static long long xl6_runtime_value_as_i64(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetInt(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static double xl6_runtime_value_as_f64(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double result = 0.0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetFloat(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_value_as_bool(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetBool(value) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", out);
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
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_runtime_value_as_point(xvalue value, const xrt_type_desc* target, bool release)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* result = 0;\n", out);
    xl6OutputPuts("    bool ok = xl6_runtime_value_is(value, target, false);\n", out);
    xl6OutputPuts("    if (ok) { result = xvoGetPoint(value); }\n", out);
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", out);
    xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", out);
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
           xl6CgenIsPlainRecordType(ctx, concreteTarget) ||
           xl6CgenIsNumericEnumType(ctx, concreteTarget) ||
           xl6CgenIsTaggedEnumType(ctx, concreteTarget) ||
           xl6CgenIsNativeBackedClassType(ctx, concreteTarget)) ) {
        return;
    }

    if ( xl6CgenIsNumericEnumType(ctx, concreteTarget) ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "#ifndef XL6_RUNTIME_AS_HELPER_%s\n", cType);
        xl6OutputPrintf(ctx->out, "#define XL6_RUNTIME_AS_HELPER_%s\n", cType);
        xl6OutputPrintf(ctx->out, "static %s xl6_runtime_value_as_%s(xvalue value, bool release)\n", cType, cType);
        xl6OutputPuts("{\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    %s result = (%s)0;\n", cType, cType);
        xl6OutputPuts("    bool ok = xrtTypeUnboxValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, concreteTarget);
        xl6OutputPuts(", value, &result);\n", ctx->out);
        xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", ctx->out);
        xl6OutputPuts("    if (!ok) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", ctx->out);
        xl6OutputPuts("    return result;\n}\n", ctx->out);
        xl6OutputPuts("#endif\n\n", ctx->out);
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
    xl6OutputPuts(", &result, release)) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", ctx->out);
    xl6OutputPuts("    return result;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
    xl6OutputPuts("#endif\n\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeNullableCastHelper(
    XL6_CgenContext* ctx,
    const XL6_AstNode* target)
{
    const XL6_AstNode* concreteTarget;
    const XL6_AstNode* inner;
    const XL6_AstNode* concreteInner;
    const char* name;
    const char* getter;
    const char* resultType;
    char helperName[96];
    char guardName[128];
    unsigned long long typeId;
    size_t i;

    concreteTarget = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, target));
    inner = xl6CgenNullableInnerType(concreteTarget);
    concreteInner = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, inner));
    if ( concreteInner == NULL ||
         xl6CgenNullableUsesXValue(concreteTarget) ) {
        return;
    }
    typeId = xl6CgenNullableTypeId(concreteTarget);
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        if ( ctx->nullableTypes[i].typeId != typeId ) {
            continue;
        }
        if ( ctx->nullableTypes[i].runtimeCastHelperEmitted ) {
            return;
        }
        ctx->nullableTypes[i].runtimeCastHelperEmitted = 1;
        break;
    }

    snprintf(
        helperName,
        sizeof(helperName),
        "xl6_runtime_value_try_as_nullable_%016llx",
        typeId);
    snprintf(
        guardName,
        sizeof(guardName),
        "XL6_RUNTIME_TRY_AS_NULLABLE_%016llX",
        typeId);
    resultType = xl6CgenTypeName(ctx, concreteTarget);
    name = concreteInner->text;

    xl6OutputPrintf(ctx->out, "#ifndef %s\n", guardName);
    xl6OutputPrintf(ctx->out, "#define %s\n", guardName);
    xl6OutputPrintf(
        ctx->out,
        "static %s %s(xvalue value, bool release)\n",
        resultType,
        helperName);
    xl6OutputPuts("{\n", ctx->out);

    if ( xl6CgenNullableUsesPointer(concreteTarget) ) {
        xl6OutputPrintf(ctx->out, "    %s result = 0;\n", resultType);
        xl6OutputPuts(
            "    bool ok = xl6_runtime_value_is(value, ",
            ctx->out);
        xl6CgenEmitRuntimeValueTypeDescRef(ctx, concreteInner);
        xl6OutputPuts(", false);\n", ctx->out);
        if ( xl6CTextEquals(name, "string") ) {
            xl6OutputPuts(
                "    if (ok) { result = xl6_runtime_temp_cstr(xvoGetText(value)); }\n",
                ctx->out);
        } else if ( xl6CTextEquals(name, "point") ) {
            xl6OutputPuts(
                "    if (ok) { result = xvoGetPoint(value); }\n",
                ctx->out);
        }
        xl6OutputPuts(
            "    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n",
            ctx->out);
        xl6OutputPuts("    return result;\n", ctx->out);
        xl6OutputPuts("}\n", ctx->out);
        xl6OutputPuts("#endif\n\n", ctx->out);
        return;
    }

    xl6OutputPrintf(ctx->out, "    %s result = {0};\n", resultType);
    if ( xl6CgenIsNumericEnumType(ctx, concreteInner) ) {
        xl6OutputPuts("    result.hasValue = xrtTypeUnboxValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, concreteInner);
        xl6OutputPuts(", value, &result.value);\n", ctx->out);
        xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", ctx->out);
        xl6OutputPuts("    return result;\n", ctx->out);
        xl6OutputPuts("}\n", ctx->out);
        xl6OutputPuts("#endif\n\n", ctx->out);
        return;
    }

    if ( xl6CgenIsPlainRecordType(ctx, concreteInner) ||
         xl6CgenIsTaggedEnumType(ctx, concreteInner) ||
         xl6CgenIsNativeBackedClassType(ctx, concreteInner) ) {
        xl6OutputPuts("    result.hasValue = ", ctx->out);
        xl6OutputPuts(
            xl6CgenIsNativeBackedClassType(ctx, concreteInner) ?
                "xl6_runtime_value_as_handle(value, " :
                "xl6_runtime_value_as_record(value, ",
            ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, concreteInner);
        xl6OutputPuts(", &result.value, release);\n", ctx->out);
        xl6OutputPuts("    return result;\n", ctx->out);
        xl6OutputPuts("}\n", ctx->out);
        xl6OutputPuts("#endif\n\n", ctx->out);
        return;
    }

    getter = NULL;
    if ( xl6CgenIsIntegerTypeName(name) ||
         xl6CTextEquals(name, "type") ||
         xl6CTextEquals(name, "time") ) {
        getter = "xvoGetInt";
    } else if ( xl6CgenIsFloatTypeName(name) ) {
        getter = "xvoGetFloat";
    } else if ( xl6CgenIsBoolTypeName(name) ) {
        getter = "xvoGetBool";
    }

    xl6OutputPuts(
        "    result.hasValue = xl6_runtime_value_is(value, ",
        ctx->out);
    xl6CgenEmitRuntimeValueTypeDescRef(ctx, concreteInner);
    xl6OutputPuts(", false);\n", ctx->out);
    if ( getter != NULL ) {
        xl6OutputPuts("    if (result.hasValue) { result.value = (", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, concreteInner), ctx->out);
        xl6OutputPuts(")", ctx->out);
        xl6OutputPuts(getter, ctx->out);
        xl6OutputPuts("(value); }\n", ctx->out);
    }
    xl6OutputPuts(
        "    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n",
        ctx->out);
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
    if ( (node->kind == XL6_AST_TYPE_AS || node->kind == XL6_AST_TYPE_IS) &&
         node->childCount > 1 ) {
        const XL6_AstNode* concreteTarget = xl6CgenResolveAliasType(
            ctx,
            xl6CgenSubstituteGenericType(ctx, node->children[1]));
        const XL6_AstNode* nullableInner =
            xl6CgenNullableInnerType(concreteTarget);

        if ( nullableInner != NULL ) {
            xl6CgenEmitRuntimeTypeNullableCastHelper(ctx, concreteTarget);
            xl6CgenEmitRuntimeTypeRecordCastHelper(ctx, nullableInner);
        } else {
            xl6CgenEmitRuntimeTypeRecordCastHelper(ctx, concreteTarget);
        }
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
    xl6OutputPuts("    if (release && value != 0) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref); }\n", ctx->out);
    xl6OutputPuts("    return ok;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPrintf(ctx->out, "static %s xl6_runtime_value_as_%s(xvalue value, bool release)\n", targetCType, targetCType);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    %s result = {0};\n", targetCType);
    xl6OutputPrintf(ctx->out, "    if (!xl6_runtime_value_as_class_%s(value, &result, release)) { xl6_error_raise(\"TypeError\", \"runtime type cast failed\", \"\", 0, 0); }\n", targetCType);
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
    if ( target != NULL && target->text != NULL &&
         xl6CgenFindProtocol(ctx, target->text) != NULL ) {
        xl6OutputPuts("xl6_runtime_value_is(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, target);
        xl6OutputPuts(
            xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)",
            ctx->out);
        xl6AstFree(valueType);
        return;
    }
    if ( xl6CgenClassHandleTargetType(ctx, target) != NULL ) {
        xl6OutputPuts("xl6_runtime_value_is(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, xl6CgenClassHandleTargetType(ctx, target));
        xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(valueType);
        return;
    }
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
    xl6CgenEmitRuntimeValueTypeDescRef(ctx, target);
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
    xl6CgenEmitRuntimeValueTypeDescRef(ctx, concreteTarget);
    xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false))" : ", true))", ctx->out);
    return 1;
}

/*
 * 流类型收窄只改变表达式类型，不改变变量的真实 C 存储。
 * var 仍然是 xvalue；当 HIR 把当前名称收窄为基础值类型时，读取点必须解箱。
 * class、protocol 和容器本身也由 xvalue 承载，不需要进入这里。
 */
static int xl6CgenRuntimeNarrowedNameExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    const XL6_AstNode* declaredType;
    const XL6_HirNode* hirNode;
    XL6_AstNode* targetType;
    const XL6_AstNode* concreteTarget;
    const char* helper;
    const char* cType;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_NAME ||
         node->text == NULL || ctx->hir == NULL ) {
        return 0;
    }
    declaredType = xl6CgenFindVarType(ctx, node->text);
    if ( declaredType == NULL || declaredType->text == NULL ||
         !xl6CTextEquals(declaredType->text, "var") ) {
        return 0;
    }
    hirNode = xl6CgenHirNode(ctx, node);
    targetType = hirNode != NULL && hirNode->type != XL6_HIR_TYPE_INVALID
        ? xl6HirTypeCloneAst(&ctx->hir->types, hirNode->type)
        : NULL;
    concreteTarget = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, targetType));
    if ( concreteTarget == NULL || concreteTarget->text == NULL ||
         xl6CTextEquals(concreteTarget->text, "var") ||
         xl6CgenIsXValueType(concreteTarget) ) {
        xl6AstFree(targetType);
        return 0;
    }

    helper = NULL;
    if ( xl6CgenIsIntegerTypeName(concreteTarget->text) ||
         xl6CTextEquals(concreteTarget->text, "type") ||
         xl6CTextEquals(concreteTarget->text, "time") ) {
        helper = "xl6_runtime_value_as_i64";
    } else if ( xl6CgenIsFloatTypeName(concreteTarget->text) ) {
        helper = "xl6_runtime_value_as_f64";
    } else if ( xl6CgenIsBoolTypeName(concreteTarget->text) ) {
        helper = "xl6_runtime_value_as_bool";
    } else if ( xl6CTextEquals(concreteTarget->text, "point") ) {
        helper = "xl6_runtime_value_as_point";
    } else if ( xl6CTextEquals(concreteTarget->text, "string") ) {
        helper = "xl6_runtime_value_as_string";
    }
    if ( helper != NULL ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "((%s)%s(", cType, helper);
        xl6CgenNamedValueReference(ctx, node->text);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeValueTypeDescRef(ctx, concreteTarget);
        xl6OutputPuts(", false))", ctx->out);
        xl6AstFree(targetType);
        return 1;
    }

    if ( xl6CgenIsPlainRecordType(ctx, concreteTarget) ||
         xl6CgenIsNumericEnumType(ctx, concreteTarget) ||
         xl6CgenIsTaggedEnumType(ctx, concreteTarget) ||
         xl6CgenIsNativeBackedClassType(ctx, concreteTarget) ) {
        cType = xl6CgenTypeName(ctx, concreteTarget);
        xl6OutputPrintf(ctx->out, "xl6_runtime_value_as_%s(", cType);
        xl6CgenNamedValueReference(ctx, node->text);
        xl6OutputPuts(", false)", ctx->out);
        xl6AstFree(targetType);
        return 1;
    }

    xl6AstFree(targetType);
    return 0;
}

static void xl6CgenRuntimeTypeAsNullableExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* value,
    const XL6_AstNode* target,
    const XL6_AstNode* valueType)
{
    const XL6_AstNode* concreteTarget;
    const XL6_AstNode* inner;
    const XL6_AstNode* concreteInner;
    const XL6_AstNode* concreteValueType;
    char helperName[96];

    concreteTarget = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, target));
    inner = xl6CgenNullableInnerType(concreteTarget);
    concreteInner = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, inner));
    concreteValueType = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, valueType));

    if ( value == NULL || value->kind == XL6_AST_NULL ) {
        if ( xl6CgenNullableUsesTaggedValue(concreteTarget) ) {
            char cTypeName[64];
            xl6CgenNullableCTypeName(
                cTypeName,
                sizeof(cTypeName),
                concreteTarget);
            xl6OutputPrintf(ctx->out, "(%s){0}", cTypeName);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
        return;
    }

    if ( concreteValueType != NULL &&
         xl6CgenTypeNodeEqual(concreteValueType, concreteTarget) ) {
        xl6CgenExpr(ctx, value);
        return;
    }

    if ( concreteValueType != NULL &&
         xl6CgenTypeNodeEqual(concreteValueType, concreteInner) &&
         !xl6CgenNullableUsesXValue(concreteTarget) ) {
        if ( xl6CgenNullableUsesTaggedValue(concreteTarget) ) {
            xl6CgenNullableTaggedValueExpr(ctx, concreteTarget, value);
        } else {
            xl6CgenExpr(ctx, value);
        }
        return;
    }

    if ( xl6CgenNullableUsesXValue(concreteTarget) ) {
        const XL6_AstNode* classTarget =
            xl6CgenClassHandleTargetType(ctx, concreteInner);

        xl6OutputPuts("xl6_runtime_value_try_as_class(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(
            ctx,
            classTarget != NULL ? classTarget : concreteInner);
        xl6OutputPuts(
            xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)",
            ctx->out);
        return;
    }

    snprintf(
        helperName,
        sizeof(helperName),
        "xl6_runtime_value_try_as_nullable_%016llx",
        xl6CgenNullableTypeId(concreteTarget));
    xl6OutputPrintf(ctx->out, "%s(", helperName);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(
        xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)",
        ctx->out);
}

static void xl6CgenRuntimeTypeAsExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    const XL6_AstNode* target;
    const XL6_AstNode* concreteTarget;
    XL6_AstNode* valueType;
    const XL6_AstNode* concreteValueType;
    const XL6_AstNode* nullableValueInner;
    const char* cType;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    target = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    valueType = xl6CgenCloneExprTypeForInference(ctx, value);
    concreteValueType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, valueType));
    concreteTarget = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, target));
    nullableValueInner = xl6CgenNullableInnerType(concreteValueType);

    if ( xl6CgenNullableInnerType(concreteTarget) != NULL ) {
        xl6CgenRuntimeTypeAsNullableExpr(
            ctx,
            value,
            concreteTarget,
            concreteValueType);
        xl6AstFree(valueType);
        return;
    }

    if ( nullableValueInner != NULL && concreteTarget != NULL &&
         xl6CgenTypeNodeEqual(
             xl6CgenResolveAliasType(
                 ctx,
                 xl6CgenSubstituteGenericType(ctx, nullableValueInner)),
             concreteTarget) ) {
        if ( xl6CgenNullableUsesTaggedValue(concreteValueType) ) {
            char helperName[64];

            xl6CgenNullableUnwrapHelperName(
                helperName,
                sizeof(helperName),
                concreteValueType);
            xl6OutputPrintf(ctx->out, "%s(", helperName);
            xl6CgenExpr(ctx, value);
            xl6OutputPrintf(
                ctx->out,
                ", %s, \"\", %d, %d)",
                xl6CgenHirOwnership(ctx, value) == XL6_HIR_OWNERSHIP_MOVE
                    ? "false"
                    : "true",
                node != NULL ? node->line : 0,
                node != NULL ? node->column : 0);
            xl6AstFree(valueType);
            return;
        }
        if ( xl6CgenNullableUsesPointer(concreteValueType) ) {
            cType = xl6CgenTypeName(ctx, concreteTarget);
            xl6OutputPrintf(ctx->out, "((%s)xl6_nullable_require_pointer((void*)(", cType);
            xl6CgenExpr(ctx, value);
            xl6OutputPrintf(
                ctx->out,
                "), \"\", %d, %d))",
                node != NULL ? node->line : 0,
                node != NULL ? node->column : 0);
            xl6AstFree(valueType);
            return;
        }
    }

    if ( xl6CgenClassHandleTargetType(ctx, concreteTarget) != NULL ) {
        xl6OutputPuts("xl6_runtime_value_as_class(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, xl6CgenClassHandleTargetType(ctx, concreteTarget));
        xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)", ctx->out);
        xl6AstFree(valueType);
        return;
    }

    if ( concreteTarget != NULL && concreteTarget->text != NULL &&
         xl6CgenFindProtocol(ctx, concreteTarget->text) != NULL ) {
        xl6OutputPuts("xl6_runtime_value_as_class(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, concreteTarget);
        xl6OutputPuts(
            xl6CgenExprIsXValueBorrow(ctx, value) ? ", false)" : ", true)",
            ctx->out);
        xl6AstFree(valueType);
        return;
    }

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

    if ( concreteTarget != NULL &&
         (xl6CgenIsClassType(ctx, concreteTarget) ||
          xl6CgenIsPlainRecordType(ctx, concreteTarget) ||
          xl6CgenIsNumericEnumType(ctx, concreteTarget) ||
          xl6CgenIsTaggedEnumType(ctx, concreteTarget)) ) {
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
