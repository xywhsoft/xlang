static void xl6CgenRecordFieldSelfBaseExpr(XL6_CgenContext* ctx, const char* basePath)
{
    const char* p;

    xl6OutputPuts("self", ctx->out);
    if ( basePath == NULL || basePath[0] == '\0' ) {
        return;
    }
    p = basePath;
    if ( *p == '.' ) {
        xl6OutputPuts("->", ctx->out);
        ++p;
    }
    while ( *p != '\0' ) {
        if ( *p == '.' ) {
            xl6OutputPutc('.', ctx->out);
        } else {
            xl6OutputPutc(*p, ctx->out);
        }
        ++p;
    }
}

static void xl6CgenRecordFieldSelfBaseAddressExpr(XL6_CgenContext* ctx, const char* basePath)
{
    if ( basePath == NULL || basePath[0] == '\0' ) {
        xl6OutputPuts("self", ctx->out);
        return;
    }
    xl6OutputPutc('&', ctx->out);
    xl6CgenRecordFieldSelfBaseExpr(ctx, basePath);
}

/* 动态字段桥接保留 class 引用；JSON/XSON 转换只由显式持久化入口负责。 */
static void xl6CgenRecordFieldValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        xl6OutputPuts("xl6_xrt_retain(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenClassValueExpr(ctx, typeNode, valuePath);
}

static void xl6CgenRecordFieldAssignFromValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* targetPath,
    const char* valuePath,
    const char* okName)
{
    xl6CgenClassAssignFromValue(ctx, typeNode, targetPath, valuePath, okName, NULL);
}

static int xl6CgenRecordFieldStaticGetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* field)
{
    const XL6_AstNode* fieldType;
    char fieldPath[512];

    if ( ctx == NULL || classNode == NULL || field == NULL ||
         field->text == NULL || field->childCount == 0 ) {
        return 0;
    }

    fieldType = field->children[0];
    xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "(*self)", 0, field->text);

    xl6OutputPuts("        if ( strcmp(key, ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, field->text);
    xl6OutputPuts(") == 0 ) {\n", ctx->out);
    xl6OutputPuts("            result = ", ctx->out);
    xl6CgenRecordFieldValueExpr(ctx, fieldType, fieldPath);
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("            goto done;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    return 1;
}

static int xl6CgenRecordFieldPropertyGetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* runtimeClass,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* getter)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    const XL6_AstNode* nameNode;
    char basePath[256];
    char valueName[64];
    int id;

    if ( ctx == NULL || runtimeClass == NULL || ownerClass == NULL ||
         getter == NULL || getter->kind != XL6_AST_GET ) {
        return 0;
    }
    nameNode = getter->childCount > 0 ? getter->children[0] : NULL;
    if ( nameNode == NULL || nameNode->text == NULL ) {
        return 0;
    }
    params = xl6CgenCallableParams(getter);
    returns = getter->childCount > 2 ? getter->children[2] : NULL;
    if ( xl6CgenFixedParamCount(params) != 0 ||
         returns == NULL || returns->childCount != 1 ) {
        return 0;
    }
    returnType = returns->children[0];
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(
            ctx,
            runtimeClass,
            ownerClass,
            basePath,
            sizeof(basePath)) ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_record_property_%d", id);
    xl6OutputPuts("        if ( strcmp(key, ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, nameNode->text);
    xl6OutputPuts(") == 0 ) {\n", ctx->out);
    xl6OutputPuts("            ", ctx->out);
    /* getter 先保留真实 C 存储类型，再统一装箱为动态字段结果。 */
    xl6OutputPuts(xl6CgenValueStorageCType(ctx, returnType), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6OutputPuts(valueName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, getter);
    xl6OutputPutc('(', ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts("            result = ", ctx->out);
    if ( xl6CgenIsXValueType(returnType) ||
         xl6CgenClassHandleTargetType(ctx, returnType) != NULL ) {
        /* getter 的托管返回值已经转移给调用方，动态桥直接接管该引用。 */
        xl6OutputPuts(valueName, ctx->out);
    } else {
        xl6CgenRecordFieldValueExpr(ctx, returnType, valueName);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("            goto done;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    return 1;
}

static int xl6CgenRecordFieldStaticSetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* field)
{
    const XL6_AstNode* fieldType;
    char fieldPath[512];
    int prevIndent;

    if ( ctx == NULL || classNode == NULL || field == NULL ||
         field->text == NULL || field->childCount == 0 ) {
        return 0;
    }

    fieldType = field->children[0];
    xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "(*self)", 0, field->text);

    xl6OutputPuts("        if ( strcmp(key, ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, field->text);
    xl6OutputPuts(") == 0 ) {\n", ctx->out);
    xl6OutputPuts("            int ok = 1;\n", ctx->out);
    prevIndent = ctx->indent;
    ctx->indent = 3;
    xl6CgenRecordFieldAssignFromValue(ctx, fieldType, fieldPath, "value", "ok");
    ctx->indent = prevIndent;
    xl6OutputPuts("            return ok != 0;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    return 1;
}

static void xl6CgenRecordFieldProtocolGetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    char basePath[256];
    char valueName[64];
    int id;

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, "__field__", &ownerClass);
    if ( method == NULL || ownerClass == NULL ) {
        return;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        return;
    }

    returns = method->childCount > 2 ? method->children[2] : NULL;
    returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( returnType == NULL ) {
        return;
    }
    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_record_field_result_%d", id);

    /* __field__ 可以返回任意可装箱类型，先保留真实 C 类型，再统一转换为 xvalue。 */
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenValueStorageCType(ctx, returnType), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6OutputPuts(valueName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(", ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    xl6OutputPuts(", key);\n", ctx->out);
    xl6OutputPuts("        result = ", ctx->out);
    if ( xl6CgenIsXValueType(returnType) || xl6CgenClassHandleTargetType(ctx, returnType) != NULL ) {
        /* 协议函数返回的 xvalue 已转移所有权，桥接层直接接管，不能再次 retain。 */
        xl6OutputPuts(valueName, ctx->out);
    } else {
        xl6CgenRecordFieldValueExpr(ctx, returnType, valueName);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("        goto done;\n", ctx->out);
}

static void xl6CgenRecordFieldProtocolSetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    char basePath[256];

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, "__setField__", &ownerClass);
    if ( method == NULL || ownerClass == NULL ) {
        return;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        return;
    }

    xl6OutputPuts("        ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(", ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    /* __setField__ 的 var 参数按值接管一个引用，借用桥接必须先 retain。 */
    xl6OutputPuts(", key, xl6_xrt_retain(value));\n", ctx->out);
    xl6OutputPuts("        return true;\n", ctx->out);
}

static void xl6CgenRecordFieldGetClassCase(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    const XL6_AstNode* ownerClass;
    char cTypeName[256];
    size_t i;
    size_t depth;

    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ||
         !xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }

    xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
    xl6OutputPuts("    if ( xrtTypeSame(desc, ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
    xl6OutputPuts(") ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("* self = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("*)data;\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        if ( xl6CgenIsInstanceField(body->children[i]) ) {
            xl6CgenRecordFieldStaticGetCase(ctx, classNode, body->children[i]);
        }
    }
    /* 派生类 getter 优先；同名基类 getter 会因前面的 goto done 自然失效。 */
    ownerClass = classNode;
    depth = 0;
    while ( ownerClass != NULL && depth++ < 256 ) {
        body = xl6CgenClassBody(ownerClass);
        for ( i = 0; body != NULL && i < body->childCount; ++i ) {
            xl6CgenRecordFieldPropertyGetCase(
                ctx,
                classNode,
                ownerClass,
                body->children[i]);
        }
        ownerClass = xl6CgenBaseClass(ctx, ownerClass);
    }
    xl6OutputPuts("        result = xl6_bound_method_create(obj, key);\n", ctx->out);
    xl6OutputPuts("        if ( result != 0 ) { goto done; }\n", ctx->out);
    xl6CgenRecordFieldProtocolGetCase(ctx, classNode);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenRecordFieldSetClassCase(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    char cTypeName[256];
    size_t i;

    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ||
         !xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }

    xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
    xl6OutputPuts("    if ( xrtTypeSame(desc, ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
    xl6OutputPuts(") ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("* self = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("*)data;\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        if ( xl6CgenIsInstanceField(body->children[i]) ) {
            xl6CgenRecordFieldStaticSetCase(ctx, classNode, body->children[i]);
        }
    }
    xl6CgenRecordFieldProtocolSetCase(ctx, classNode);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitRecordFieldHelpers(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->out == NULL || ctx->root == NULL ) {
        return;
    }

    xl6OutputPuts(
        "typedef struct XL6_Bound_Method_Env { xvalue self; const char* name; unsigned int nameSize; } XL6_Bound_Method_Env;\n"
        "bool xrtTypeSame(const xrt_type_desc* a, const xrt_type_desc* b);\n"
        "bool xrtTypeIsA(const xrt_type_desc* type, const xrt_type_desc* target);\n"
        "bool xrtTypeCanWiden(const xrt_type_desc* source, const xrt_type_desc* target);\n\n"
        "static int xl6_bound_method_value_match_cost(xvalue value, const xrt_type_desc* target)\n"
        "{\n"
        "    const xrt_type_desc* actual;\n"
        "    if ( value == 0 || target == 0 ) { return -1; }\n"
        "    actual = xvoTypeDesc(value);\n"
        "    if ( actual == 0 ) { return -1; }\n"
        "    if ( xrtTypeSame(actual, target) ) { return 0; }\n"
        "    if ( xrtTypeIsA(actual, target) ) { return 1; }\n"
        "    if ( xrtTypeCanWiden(actual, target) ) { return 2; }\n"
        "    if ( xrtTypeSame(target, xrtTypeValue()) ) { return 3; }\n"
        "    return -1;\n"
        "}\n\n"
        "static int xl6_bound_method_signature_cost(const xrt_func_sig* sig, const xrt_call_frame* frame)\n"
        "{\n"
        "    unsigned int fixedCount, requiredCount, index, namedCount;\n"
        "    int score = 0;\n"
        "    int hasVarargs, hasKwargs;\n"
        "    if ( sig == 0 || frame == 0 ) { return -1; }\n"
        "    hasVarargs = (sig->Flags & XL6_XRT_FUNC_FLAG_VARARGS) != 0;\n"
        "    hasKwargs = (sig->Flags & XL6_XRT_FUNC_FLAG_KWARGS) != 0;\n"
        "    fixedCount = sig->ParamCount - ((hasVarargs || hasKwargs) && sig->ParamCount > 0 ? 1u : 0u);\n"
        "    if ( frame->Argc > fixedCount && !hasVarargs ) { return -1; }\n"
        "    requiredCount = 0u;\n"
        "    while ( requiredCount < fixedCount && (sig->Params[requiredCount].Flags & XL6_XRT_FUNC_PARAM_OPTIONAL) == 0 ) { requiredCount++; }\n"
        "    namedCount = 0u;\n"
        "    for ( index = 0u; index < fixedCount; ++index ) {\n"
        "        xvalue argument = 0;\n"
        "        int cost;\n"
        "        int hasNamed = frame->Kwargs != 0 && xvoTableExists(frame->Kwargs, (str)sig->Params[index].Name, sig->Params[index].NameSize);\n"
        "        if ( index < frame->Argc ) {\n"
        "            if ( hasNamed ) { return -1; }\n"
        "            argument = frame->Argv[index];\n"
        "        } else if ( hasNamed ) {\n"
        "            argument = xvoTableGetValue(frame->Kwargs, (str)sig->Params[index].Name, sig->Params[index].NameSize);\n"
        "            namedCount++;\n"
        "        } else if ( index < requiredCount ) {\n"
        "            return -1;\n"
        "        } else {\n"
        "            score += 1;\n"
        "        }\n"
        "        if ( argument != 0 ) {\n"
        "            cost = xl6_bound_method_value_match_cost(argument, sig->Params[index].Type);\n"
        "            if ( cost < 0 ) { return -1; }\n"
        "            score += cost;\n"
        "        }\n"
        "    }\n"
        "    if ( frame->Kwargs != 0 && !hasKwargs && xvoTableItemCount(frame->Kwargs) != namedCount ) { return -1; }\n"
        "    if ( hasVarargs && frame->Argc > fixedCount ) { score += 8; }\n"
        "    return score;\n"
        "}\n\n"
        "static void xl6_bound_method_drop(void* value)\n"
        "{\n"
        "    XL6_Bound_Method_Env* env = (XL6_Bound_Method_Env*)value;\n"
        "    if ( env == 0 ) { return; }\n"
        "    if ( env->self != 0 ) { xvoUnref(env->self); }\n"
        "    xrtFree(env);\n"
        "}\n\n"
        "#define XL6_BOUND_METHOD_NAME(env, literal) ((env)->nameSize == (unsigned int)(sizeof(literal) - 1u) && memcmp((env)->name, literal, sizeof(literal) - 1u) == 0)\n"
        "static int xl6_bound_container_method_supported(xvalue self, const char* name, unsigned int nameSize)\n"
        "{\n"
        "    unsigned int kind;\n"
        "    int type;\n"
        "    if ( self == 0 || name == 0 ) { return 0; }\n"
        "    kind = xl6_xrt_typed_container_kind(self);\n"
        "    type = xvoType(self);\n"
        "#define XL6_BOUND_SUPPORTS(literal) (nameSize == (unsigned int)(sizeof(literal) - 1u) && memcmp(name, literal, sizeof(literal) - 1u) == 0)\n"
        "    if ( kind == XL6_XRT_TYPE_KIND_ARRAY || type == XL6_XVO_DT_ARRAY ) {\n"
        "        return XL6_BOUND_SUPPORTS(\"push\") || XL6_BOUND_SUPPORTS(\"append\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"insert\") || XL6_BOUND_SUPPORTS(\"remove\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"removeValue\") || XL6_BOUND_SUPPORTS(\"pop\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"take\") || XL6_BOUND_SUPPORTS(\"clear\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"reserve\") || XL6_BOUND_SUPPORTS(\"swap\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"contains\") || XL6_BOUND_SUPPORTS(\"indexOf\");\n"
        "    }\n"
        "    if ( kind == XL6_XRT_TYPE_KIND_LIST || type == XL6_XVO_DT_LIST ) {\n"
        "        return XL6_BOUND_SUPPORTS(\"remove\") || XL6_BOUND_SUPPORTS(\"take\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"clear\") || XL6_BOUND_SUPPORTS(\"contains\");\n"
        "    }\n"
        "    if ( kind == XL6_XRT_TYPE_KIND_SET || type == XL6_XVO_DT_COLL ) {\n"
        "        return XL6_BOUND_SUPPORTS(\"add\") || XL6_BOUND_SUPPORTS(\"remove\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"clear\") || XL6_BOUND_SUPPORTS(\"contains\");\n"
        "    }\n"
        "    if ( kind == XL6_XRT_TYPE_KIND_DICT || type == XL6_XVO_DT_TABLE ) {\n"
        "        return XL6_BOUND_SUPPORTS(\"remove\") || XL6_BOUND_SUPPORTS(\"take\") ||\n"
        "               XL6_BOUND_SUPPORTS(\"clear\") || XL6_BOUND_SUPPORTS(\"contains\");\n"
        "    }\n"
        "    return 0;\n"
        "#undef XL6_BOUND_SUPPORTS\n"
        "}\n\n"
        "static bool xl6_bound_container_result(xrt_call_result* result, xvalue value)\n"
        "{\n"
        "    if ( value == 0 ) { return false; }\n"
        "    if ( xrtCallResultSetValue(result, 0u, value, true) ) { return true; }\n"
        "    xvoUnref(value);\n"
        "    return false;\n"
        "}\n\n"
        "static bool xl6_bound_container_method_xcall(const XL6_Bound_Method_Env* env, const xrt_call_frame* frame, xrt_call_result* result)\n"
        "{\n"
        "    xvalue value;\n"
        "    int ok;\n"
        "    if ( env == 0 || frame == 0 || result == 0 || env->self == 0 ) { return false; }\n"
        "    if ( frame->Kwargs != 0 && xvoTableItemCount(frame->Kwargs) != 0u ) { return false; }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"push\") || XL6_BOUND_METHOD_NAME(env, \"append\") ) {\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        ok = xl6_xrt_container_push_value(env->self, xl6_xrt_retain(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"insert\") ) {\n"
        "        if ( frame->Argc != 2u ) { return false; }\n"
        "        ok = xl6_xrt_container_insert_value(env->self, xl6_xrt_retain(frame->Argv[0]), xl6_xrt_retain(frame->Argv[1]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"remove\") ) {\n"
        "        unsigned int count = 1u;\n"
        "        if ( frame->Argc < 1u || frame->Argc > 2u ) { return false; }\n"
        "        if ( frame->Argc == 2u ) {\n"
        "            if ( frame->Argv[1] == 0 || xvoType(frame->Argv[1]) != XL6_XVO_DT_INT || xvoGetInt(frame->Argv[1]) < 0 ) { return false; }\n"
        "            count = (unsigned int)xvoGetInt(frame->Argv[1]);\n"
        "        }\n"
        "        ok = xl6_xrt_container_remove_value(env->self, xl6_xrt_retain(frame->Argv[0]), count, frame->Argc == 2u);\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"removeValue\") ) {\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        ok = xl6_xrt_value_remove_value_release(env->self, xl6_xrt_retain(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"pop\") ) {\n"
        "        if ( frame->Argc != 0u ) { return false; }\n"
        "        return xl6_bound_container_result(result, xl6_xrt_container_pop_value(env->self));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"take\") ) {\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        return xl6_bound_container_result(result, xl6_xrt_container_take_value(env->self, xl6_xrt_retain(frame->Argv[0])));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"clear\") ) {\n"
        "        if ( frame->Argc != 0u ) { return false; }\n"
        "        ok = xl6_xrt_container_clear(env->self);\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"reserve\") ) {\n"
        "        if ( frame->Argc != 1u || frame->Argv[0] == 0 || xvoType(frame->Argv[0]) != XL6_XVO_DT_INT || xvoGetInt(frame->Argv[0]) < 0 ) { return false; }\n"
        "        ok = xl6_xrt_container_reserve(env->self, (unsigned int)xvoGetInt(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"swap\") ) {\n"
        "        if ( frame->Argc != 2u || frame->Argv[0] == 0 || frame->Argv[1] == 0 ||\n"
        "             xvoType(frame->Argv[0]) != XL6_XVO_DT_INT || xvoType(frame->Argv[1]) != XL6_XVO_DT_INT ||\n"
        "             xvoGetInt(frame->Argv[0]) < 0 || xvoGetInt(frame->Argv[1]) < 0 ) { return false; }\n"
        "        ok = xl6_xrt_container_swap(env->self, (unsigned int)xvoGetInt(frame->Argv[0]), (unsigned int)xvoGetInt(frame->Argv[1]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"add\") ) {\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        ok = xl6_xrt_container_add_value(env->self, xl6_xrt_retain(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"contains\") ) {\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        ok = xl6_xrt_value_contains(env->self, xl6_xrt_retain(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateBool(ok != 0));\n"
        "    }\n"
        "    if ( XL6_BOUND_METHOD_NAME(env, \"indexOf\") ) {\n"
        "        long long index;\n"
        "        if ( frame->Argc != 1u ) { return false; }\n"
        "        index = xl6_xrt_array_index_of(env->self, xl6_xrt_retain(frame->Argv[0]));\n"
        "        return xl6_bound_container_result(result, xvoCreateInt(index));\n"
        "    }\n"
        "    return false;\n"
        "}\n\n"
        "static bool xl6_bound_method_xcall(xrt_call_frame* frame, xrt_call_result* result)\n"
        "{\n"
        "    XL6_Bound_Method_Env* env;\n"
        "    const xrt_type_desc* type;\n"
        "    const xrt_method_desc* selected = 0;\n"
        "    int selectedScore = 0x7FFFFFFF;\n"
        "    unsigned int selectedDepth = 0u;\n"
        "    unsigned int depth = 0u;\n"
        "    xrt_call_frame localFrame;\n"
        "    if ( frame == 0 || result == 0 || frame->Env == 0 ) { return false; }\n"
        "    env = (XL6_Bound_Method_Env*)frame->Env;\n"
        "    type = env->self != 0 ? xvoTypeDesc(env->self) : 0;\n"
        "    while ( type != 0 && depth++ < 256u ) {\n"
        "        const xrt_method_table* table = type->Methods;\n"
        "        unsigned int index;\n"
        "        for ( index = 0u; table != 0 && table->Methods != 0 && index < table->Count; ++index ) {\n"
        "            const xrt_method_desc* method = &table->Methods[index];\n"
        "            int score;\n"
        "            if ( method->Name == 0 || method->NameSize != env->nameSize ||\n"
        "                 memcmp(method->Name, env->name, env->nameSize) != 0 || method->XCallEntry == 0 ) { continue; }\n"
        "            score = xl6_bound_method_signature_cost(method->Sig, frame);\n"
        "            if ( score < 0 ) { continue; }\n"
        "            if ( selected == 0 || score < selectedScore ) {\n"
        "                selected = method; selectedScore = score; selectedDepth = depth - 1u; continue;\n"
        "            }\n"
        "            if ( score == selectedScore && selectedDepth < depth - 1u &&\n"
        "                 selected->Sig != 0 && method->Sig != 0 &&\n"
        "                 selected->Sig->TypeId == method->Sig->TypeId ) { continue; }\n"
        "            if ( score == selectedScore ) { return false; }\n"
        "        }\n"
        "        type = type->BaseType;\n"
        "    }\n"
        "    if ( selected == 0 ) { return xl6_bound_container_method_xcall(env, frame, result); }\n"
        "    localFrame = *frame;\n"
        "    localFrame.Self = env->self;\n"
        "    localFrame.Sig = selected->Sig;\n"
        "    return selected->XCallEntry(&localFrame, result);\n"
        "}\n\n"
        "static xvalue xl6_bound_method_create(xvalue self, const char* name)\n"
        "{\n"
        "    XL6_Bound_Method_Env* env;\n"
        "    xrt_callable* callable;\n"
        "    xvalue value;\n"
        "    const xrt_type_desc* type;\n"
        "    unsigned int nameSize;\n"
        "    unsigned int depth = 0u;\n"
        "    int found = 0;\n"
        "    if ( self == 0 || name == 0 ) { return 0; }\n"
        "    nameSize = (unsigned int)strlen(name);\n"
        "    type = xvoTypeDesc(self);\n"
        "    while ( type != 0 && depth++ < 256u && !found ) {\n"
        "        const xrt_method_table* table = type->Methods;\n"
        "        unsigned int index;\n"
        "        for ( index = 0u; table != 0 && table->Methods != 0 && index < table->Count; ++index ) {\n"
        "            const xrt_method_desc* method = &table->Methods[index];\n"
        "            if ( method->XCallEntry != 0 && method->Name != 0 && method->NameSize == nameSize &&\n"
        "                 memcmp(method->Name, name, nameSize) == 0 ) { found = 1; break; }\n"
        "        }\n"
        "        type = type->BaseType;\n"
        "    }\n"
        "    if ( !found ) { found = xl6_bound_container_method_supported(self, name, nameSize); }\n"
        "    if ( !found ) { return 0; }\n"
        "    env = (XL6_Bound_Method_Env*)xrtMalloc(sizeof(*env));\n"
        "    if ( env == 0 ) { return 0; }\n"
        "    env->self = xl6_xrt_retain(self);\n"
        "    env->name = name;\n"
        "    env->nameSize = nameSize;\n"
        "    callable = xrtCallableCreate(0, 0, 0u, xl6_bound_method_xcall, env, xl6_bound_method_drop);\n"
        "    if ( callable == 0 ) { xl6_bound_method_drop(env); return 0; }\n"
        "    value = xvoCreateCallable(callable, true);\n"
        "    if ( value == 0 ) { xrtCallableUnref(callable); return 0; }\n"
        "    return value;\n"
        "}\n\n",
        ctx->out);

    xl6OutputPuts("static xvalue xl6_record_field_get_owned(xvalue obj, const char* key, int release)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    xvalue result = 0;\n", ctx->out);
    xl6OutputPuts("    const xrt_type_desc* desc = obj != 0 ? xvoTypeDesc(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    void* data = obj != 0 ? xvoGetRecordData(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    if ( desc != 0 && data != 0 && key != 0 ) {\n", ctx->out);
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenRecordFieldGetClassCase(ctx, node);
        }
    }
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    result = xl6_bound_method_create(obj, key);\n", ctx->out);
    xl6OutputPuts("    if ( result != 0 ) { goto done; }\n", ctx->out);
    xl6OutputPuts("    result = xl6_xrt_index_safe_get_key(obj, key);\n", ctx->out);
    xl6OutputPuts("done:\n", ctx->out);
    xl6OutputPuts("    if ( release && obj != 0 ) { xvoUnref(obj); }\n", ctx->out);
    xl6OutputPuts("    return result != 0 ? result : xvoCreateNull();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static xvalue xl6_record_field_get_mutable(xvalue obj, const char* key)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    return xvoIndexGetMutableKey(obj, (str)key, 0);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static bool xl6_record_field_set_borrow(xvalue obj, const char* key, xvalue value)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    if ( xl6_xrt_typed_container_kind(obj) == XL6_XRT_TYPE_KIND_DICT ) {\n", ctx->out);
    xl6OutputPuts("        return xl6_xrt_typed_container_set_key_borrow(obj, key, value);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    const xrt_type_desc* desc = obj != 0 ? xvoTypeDesc(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    void* data = obj != 0 ? xvoGetRecordData(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    if ( desc != 0 && data != 0 && key != 0 ) {\n", ctx->out);
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenRecordFieldSetClassCase(ctx, node);
        }
    }
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    return xvoTableSetValue(obj, key, 0, value, false);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}
